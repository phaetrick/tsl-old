#pragma once
//
// Created by pr on 25.12.17.
//

#ifndef GRAINSTORM_EQ_H
#define GRAINSTORM_EQ_H

#include "types.h"
#include "defines.h"
#include "base.h"
#include "app.h"
#include "grainstorm.h"
#include <cstdint>
#include <sys/types.h>
#include <atomic>
#include <cmath>
#include <algorithm> // For std::clamp, std::max
#include "SpectrumAnalyzer.h"

// ---------------------------------------------------------------------------
// BIQUAD CONVENTION - read this before touching any Set*() below.
//
//   w[n] = x[n] + b1*w[n-1] + b2*w[n-2]
//   y[n] = a0*w[n] + a1*w[n-1] + a2*w[n-2]
//
// so a0..a2 are the NUMERATOR (feed-forward) coefficients and b1, b2 are the
// NEGATED denominator, i.e. they are ADDED, not subtracted. That is the inverse
// of the RBJ cookbook naming, where b is the numerator and a the denominator.
// computeresponse() in frequencyresponce.cpp evaluates 1 - b1*z - b2*z^2 to
// match, so the drawn curve only stays honest as long as everything feeding it
// uses this convention. Never mix a coefficient set from another convention
// into this pipeline: the feedback sign flips and the filter blows up.
//
// Gain convention: 'g' below is RBJ's amplitude A = 10^(dB/40) (the LOG2NORMAL2
// macro). A section's actual gain is A*A = 10^(dB/20), for both the shelves and
// the peaking filter.
//
// Q convention: 'q' below is RECIPROCAL Q - alpha = sin(w0) * 0.5 * q, so
// q = 1/Q. The Q parameters are stored pre-converted as 20*log10(q), so the
// inverse is LOG2NORMAL (/20), NOT LOG2NORMAL2 (/40).
// ---------------------------------------------------------------------------

// The *W variants take cached sin(w0)/cos(w0) instead of a frequency. Those two
// are the expensive part and depend only on the frequency, so a filter whose
// gain moves per sample (DynEq) can hoist them out of its inner loop and still
// produce coefficients identical to the plain versions - which is what keeps
// DynEq's audio matching the curve drawn from Eq5::getCoeffs.

template<typename T>
static inline void SetLowShelfW(T sinw0, T cosw0, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    T alpha = sinw0 * 0.5 * sqrt((g + (1. / g)) * (q) + 2.);
    T i = (g + 1.) * cosw0;
    T j = (g - 1.) * cosw0;
    T k = 2. * sqrt(g) * alpha;
    T b0rz = 1. / ((g + 1.) + j + k);
    a0 = g * ((g + 1.) - j + k) * b0rz;
    a1 = 2. * g * ((g - 1.) - i) * b0rz;
    a2 = g * ((g + 1.) - j - k) * b0rz;
    b1 = 2. * ((g - 1.) + i) * b0rz;
    b2 = ((g + 1.) + j - k) * -b0rz;
}

template<typename T>
static inline void SetHighShelfW(T sinw0, T cosw0, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    T alpha = sinw0 * 0.5 * sqrt((g + (1. / g)) * (q) + 2.);
    T i = (g + 1.) * cosw0;
    T j = (g - 1.) * cosw0;
    T k = 2. * sqrt(g) * alpha;
    T b0rz = 1. / ((g + 1.) - j + k);
    a0 = g * ((g + 1.) + j + k) * b0rz;
    a1 = -2. * g * ((g - 1.) + i) * b0rz;
    a2 = g * ((g + 1.) + j - k) * b0rz;
    b1 = -2. * ((g - 1.) - i) * b0rz;
    b2 = ((g + 1.) - j - k) * -b0rz;
}

template<typename T>
static inline void SetPeakingW(T sinw0, T cosw0, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    T alpha = sinw0 * 0.5 * q;
    T b0rz = 1. / (1. + (alpha / g));
    b1 = 2. * b0rz * cosw0;
    a0 = (1. + (alpha * g)) * b0rz;
    a1 = -b1;
    a2 = (1. - (alpha * g)) * b0rz;
    b2 = (1. - (alpha / g)) * -b0rz;
}

// Clamps live here, in the one place every caller goes through. Without them a
// frequency at or past Nyquist (44.1 kHz with a 20 kHz shelf, or any modulated
// centre) sends cos(w0) the wrong side of the unit circle and the section is
// unstable; g == 0 divides by zero in the shelf alpha term.
template<typename T>
static inline void ClampEqParams(T sr, T &fr, T &q, T &g) {
    fr = std::clamp(fr, T(20.), sr * T(0.45));
    q = std::max(q, T(1e-4));
    g = std::max(g, T(1e-4));
}

template<typename T>
static void SetLowShelf(T sr, T fr, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    ClampEqParams(sr, fr, q, g);
    T w0 = TWOPI_P / (T) sr * fr;
    SetLowShelfW(sin(w0), cos(w0), q, g, a0, a1, a2, b1, b2);
}

template<typename T>
static void SetHighShelf(T sr, T fr, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    ClampEqParams(sr, fr, q, g);
    T w0 = TWOPI_P / (T) sr * fr;
    SetHighShelfW(sin(w0), cos(w0), q, g, a0, a1, a2, b1, b2);
}

template<typename T>
static void SetPeaking(T sr, T fr, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) {
    ClampEqParams(sr, fr, q, g);
    T w0 = TWOPI_P / (T) sr * fr;
    SetPeakingW(sin(w0), cos(w0), q, g, a0, a1, a2, b1, b2);
}

// Layout of one EQ5/DYNEQ5 band block, and of the vars[] array the frequency
// response window hands to Eq5::getCoeffs.
enum _eq5vars {
    LOWCENTER,
    LOWGAIN,
    QLOW,
    PEAK0CENTER,
    PEAK0GAIN,
    Q0,
    PEAK1CENTER,
    PEAK1GAIN,
    Q1,
    PEAK2CENTER,
    PEAK2GAIN,
    Q2,
    HIGHCENTER,
    HIGHGAIN,
    QHIGH,
    NUM_ElementS_EQVARS
};

// Both effects index their parameters as base + band*3 + {CF, GAIN, Q} rather
// than naming each one, so a band can never again be wired to the wrong id (the
// high shelf used to read its centre frequency from EQ5QHIGH). The response
// window and Eq5::getCoeffs assume the same layout.
static_assert(EQ5QHIGH - EQ5LOWCF == NUM_ElementS_EQVARS - 1,
              "EQ5 params must stay laid out as 5 x {CF, GAIN, Q}");
static_assert(DYNEQ5QHIGH - DYNEQ5LOWCF == NUM_ElementS_EQVARS - 1,
              "DYNEQ5 params must stay laid out as 5 x {CF, GAIN, Q}");

class Eq5 : public Effect {
public:
    Eq5(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_EQ5, MONOEFFECT), analyzer(_STATE, _DATA->inputeq5[track->index]) {

        Reset();
        helpbuf.resize(_STATE->maxBufSize);
        twopidsr = TWOPI_P / (MYFLOAT) _STATE->sr;
        for (int32_t b = 0; b < 5; b++) {
            _cf[b] = &_STATE->params[track->index][EQ5LOWCF + b * 3 + LOWCENTER];
            _gn[b] = &_STATE->params[track->index][EQ5LOWCF + b * 3 + LOWGAIN];
            _q[b] = &_STATE->params[track->index][EQ5LOWCF + b * 3 + QLOW];
            _smooth[b][0] = LOG2NORMAL(_cf[b]->load());
            _smooth[b][1] = LOG2NORMAL2(_gn[b]->load());
            _smooth[b][2] = LOG2NORMAL(_q[b]->load());
        }
        _gain = &_STATE->params[track->index][EQ5GAIN];
        _bypass = &track->bypass[SPACE_EQ5];

        for (int32_t b = 0; b < 5; b++) updateBand(b);
    };

    template<typename T>
    static void getCoeffs(T *a, T *b, T sr, const T *params) {
        T low = LOG2NORMAL(params[LOWCENTER]), peak0 = LOG2NORMAL(
                params[PEAK0CENTER]), peak1 = LOG2NORMAL(params[PEAK1CENTER]), peak2 = LOG2NORMAL(
                params[PEAK2CENTER]), high = LOG2NORMAL(params[HIGHCENTER]), lowgain = LOG2NORMAL2(
                params[LOWGAIN]), peak0gain = LOG2NORMAL2(
                params[PEAK0GAIN]), peak1gain = LOG2NORMAL2(
                params[PEAK1GAIN]), peak2gain = LOG2NORMAL2(
                params[PEAK2GAIN]), highgain = LOG2NORMAL2(
                params[HIGHGAIN]), qlow = LOG2NORMAL(params[QLOW]), q0 = LOG2NORMAL(
                params[Q0]), q1 = LOG2NORMAL(params[Q1]), q2 = LOG2NORMAL(
                params[Q2]), qhigh = LOG2NORMAL(params[QHIGH]);
        T a0_l, a1_l, a2_l, b1_l, b2_l;
        T a0_h, a1_h, a2_h, b1_h, b2_h;
        T a0_0, a1_0, a2_0, b1_0, b2_0;
        T a0_1, a1_1, a2_1, b1_1, b2_1;
        T a0_2, a1_2, a2_2, b1_2, b2_2;
        SetLowShelf(sr, low, qlow, lowgain, a0_l, a1_l, a2_l, b1_l, b2_l);

        a[0] = a0_l;
        a[1] = a1_l;
        a[2] = a2_l;
        b[0] = 1;
        b[1] = b1_l;
        b[2] = b2_l;
        SetHighShelf(sr, high, qhigh, highgain, a0_h, a1_h, a2_h, b1_h, b2_h);

        a[12] = a0_h;
        a[13] = a1_h;
        a[14] = a2_h;
        b[12] = 1;
        b[13] = b1_h;
        b[14] = b2_h;
        SetPeaking(sr, peak0, q0, peak0gain, a0_0, a1_0, a2_0, b1_0, b2_0);
        a[3] = a0_0;
        a[4] = a1_0;
        a[5] = a2_0;
        b[3] = 1;
        b[4] = b1_0;
        b[5] = b2_0;

        SetPeaking(sr, peak1, q1, peak1gain, a0_1, a1_1, a2_1, b1_1, b2_1);

        a[6] = a0_1;
        a[7] = a1_1;
        a[8] = a2_1;
        b[6] = 1;
        b[7] = b1_1;
        b[8] = b2_1;

        SetPeaking(sr, peak2, q2, peak2gain, a0_2, a1_2, a2_2, b1_2, b2_2);

        a[9] = a0_2;
        a[10] = a1_2;
        a[11] = a2_2;
        b[9] = 1;
        b[10] = b1_2;
        b[11] = b2_2;
    };


    void compute(MYFLOAT *in, int32_t size) override;

    void Reset() {
        y1_l = y2_l = y1_0 = y2_0 = y1_1 = y2_1 = y1_2 = y2_2 = y1_h = y2_h = 0.0;
    };
private:
    // Recompute one band's coefficients from its current smoothed state.
    inline void updateBand(int32_t b) {
        const MYFLOAT sr = _STATE->sr;
        switch (b) {
            case 0:
                SetLowShelf(sr, _smooth[0][0], _smooth[0][2], _smooth[0][1], a0_l, a1_l, a2_l, b1_l, b2_l);
                break;
            case 1:
                SetPeaking(sr, _smooth[1][0], _smooth[1][2], _smooth[1][1], a0_0, a1_0, a2_0, b1_0, b2_0);
                break;
            case 2:
                SetPeaking(sr, _smooth[2][0], _smooth[2][2], _smooth[2][1], a0_1, a1_1, a2_1, b1_1, b2_1);
                break;
            case 3:
                SetPeaking(sr, _smooth[3][0], _smooth[3][2], _smooth[3][1], a0_2, a1_2, a2_2, b1_2, b2_2);
                break;
            default:
                SetHighShelf(sr, _smooth[4][0], _smooth[4][2], _smooth[4][1], a0_h, a1_h, a2_h, b1_h, b2_h);
                break;
        }
    }

    // Advance one band's smoothers a step, then recompute its coefficients.
    inline void smoothBand(int32_t b, const MYFLOAT (&vars)[5][3]) {
        sm(vars[b][0], _smooth[b][0]);
        sm(vars[b][1], _smooth[b][1]);
        sm(vars[b][2], _smooth[b][2]);
        updateBand(b);
    }


    // [band] -> centre frequency / gain / Q parameter
    std::atomic<MYFLOAT> *_cf[5]{}, *_gn[5]{}, *_q[5]{};
    MYFLOAT a0_l, a1_l, a2_l, b1_l, b2_l, y1_l, y2_l;
    MYFLOAT a0_h, a1_h, a2_h, b1_h, b2_h, y1_h, y2_h;
    MYFLOAT a0_0, a1_0, a2_0, b1_0, b2_0, y1_0, y2_0;
    MYFLOAT a0_1, a1_1, a2_1, b1_1, b2_1, y1_1, y2_1;
    MYFLOAT a0_2, a1_2, a2_2, b1_2, b2_2, y1_2, y2_2;
    MYFLOAT twopidsr;
    MYFLOAT _smooth[5][3];
    std::vector<MYFLOAT> helpbuf;
    SpectrumAnalyzer analyzer;

    inline MYFLOAT sm(const MYFLOAT in, MYFLOAT &smooth) {
        return smooth = smoothCoeff * (smooth - in) + in;
    }
};


class Eq10 : public Effect {
public:
    Eq10(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_EQ, MONOEFFECT) {
        _bypass = &track->bypass[SPACE_EQ];
        q = &_STATE->params[track->index][EQ10_0 + 10];
        _smooth[10] = LOG2NORMAL(q->load());
        _gain = &_STATE->params[track->index][EQ10_0 + 11];
        for (int32_t i = 0; i < 10; i++) {
            gain[i] = &_STATE->params[track->index][EQ10_0 + i];
            // Band gains are RBJ's A (LOG2NORMAL2, /40) - the section gain is
            // A*A. This used to be LOG2NORMAL (/20), so every band delivered
            // twice the dB its slider claimed. Note the Q below is a different
            // convention and correctly uses LOG2NORMAL; see the header block.
            _smooth[i] = LOG2NORMAL2(gain[i]->load());
            ComputeCoeffs<MYFLOAT>[i]((MYFLOAT) _STATE->sr, freqs[i], LOG2NORMAL(q->load()),
                                      LOG2NORMAL2(gain[i]->load()), a0[i], a1[i], a2[i], b1[i],
                                      b2[i]);
            y1[i] = y2[i] = 0.0;
        }
        helpbuf.resize(_STATE->maxBufSize);
    }

    template<typename T>
    static constexpr void
    (*ComputeCoeffs[10])(T sr, T fr, T q, T g, T &a0, T &a1, T &a2, T &b1, T &b2) = {SetLowShelf,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetPeaking,
                                                                                     SetHighShelf};

    void compute(MYFLOAT *in, int32_t size) override;


private:
    MYFLOAT a0[10], a1[10], a2[10], b1[10], b2[10], y1[10], y2[10];
    static constexpr MYFLOAT freqs[10] = {31.25, 62.5, 125., 250., 500., 1000., 2000., 4000., 8000.,
                                          16000.};
    std::atomic<MYFLOAT> *gain[10];
    std::atomic<MYFLOAT> *q;
    std::vector<MYFLOAT> helpbuf;
    MYFLOAT _smooth[11]{};
};

// Sidechain filter for DynEq: a plain Butterworth LP/BP/HP, second order. The
// coefficients only move when the band's frequency or Q does, so this one is
// set per block and ticked per sample.
template<typename T>
class EqFilter {
public:
    inline T tickLP(T in) {
        auto y0 = in + b1 * y1 + b2 * y2;
        UDD(y0);
        auto ret = a0 * (y0 + 2. * y1 + y2);
        y2 = y1;
        y1 = y0;
        return ret;
    }

    inline T tickBP(T in) {
        auto y0 = in + b1 * y1 + b2 * y2;
        UDD(y0);
        auto ret = a0 * (y0 - y2);
        y2 = y1;
        y1 = y0;
        return ret;
    }

    inline T tickHP(T in) {
        auto y0 = in + b1 * y1 + b2 * y2;
        UDD(y0);
        auto ret = a0 * (y0 - 2. * y1 + y2);
        y2 = y1;
        y1 = y0;
        return ret;
    }

    void SetHp(T onedsr, T f) {
        T pfreq = clampPfreq(onedsr, f);
        T C = tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = 2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    void SetLp(T onedsr, T f) {
        T pfreq = clampPfreq(onedsr, f);
        T C = 1. / tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = -2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    void SetBp(T onedsr, T f, T bw) {
        T pfreq = clampPfreq(onedsr, f);
        // bw is a fraction of the centre frequency; a vanishing bandwidth sends
        // 1/tan() to infinity.
        T pbw = std::clamp(bw * pfreq * T(0.5), T(1e-4), T(PI_P * 0.49));

        T C = 1. / tan(pbw);
        T D = 2. * cos(pfreq);

        a0 = 1. / (1. + C);
        b1 = C * D * a0;
        b2 = (1. - C) * a0;
    }

    void reset() { y1 = y2 = 0; }

private:
    // onedsr is 1/sr, so pfreq = f*pi/sr; keep it away from pi/2 (Nyquist).
    static inline T clampPfreq(T onedsr, T f) {
        return std::clamp(f * T(PI_P) * onedsr, T(1e-5), T(PI_P * 0.45));
    }

    T a0{}, b1{}, b2{}, y1{}, y2{};
};

// One DynEq band. Same coefficients as calling SetLowShelf/SetPeaking/
// SetHighShelf per sample, but sin(w0) and cos(w0) - which depend only on the
// frequency, not on the dynamic gain - are hoisted into setFreq(). That turns
// the inner loop from "two transcendentals plus two square roots plus two
// divides per sample" into a couple of divides, which is what made the old
// version the most expensive effect in the chain.
template<typename T>
class DynEqSection {
public:
    enum Type { LOWSHELF = 0, PEAKING = 1, HIGHSHELF = 2 };

    void setFreq(T sr, T fr, T q) {
        fr = std::clamp(fr, T(20.), sr * T(0.45));
        const T w0 = TWOPI_P / sr * fr;
        _sinw0 = std::sin(w0);
        _cosw0 = std::cos(w0);
        _q = std::max(q, T(1e-4));
    }

    inline T tick(T x, int type, T g) {
        g = std::max(g, T(1e-4));
        if (type == LOWSHELF) SetLowShelfW(_sinw0, _cosw0, _q, g, a0, a1, a2, b1, b2);
        else if (type == PEAKING) SetPeakingW(_sinw0, _cosw0, _q, g, a0, a1, a2, b1, b2);
        else SetHighShelfW(_sinw0, _cosw0, _q, g, a0, a1, a2, b1, b2);
        T y0 = x + b1 * y1 + b2 * y2;
        UDD(y0)
        const T ret = a0 * y0 + a1 * y1 + a2 * y2;
        y2 = y1;
        y1 = y0;
        return ret;
    }

    void reset() { y1 = y2 = 0; }

private:
    T _sinw0{}, _cosw0{1}, _q{1};
    T a0{}, a1{}, a2{}, b1{}, b2{}, y1{}, y2{};
};

// Sidechain level detector plus gain computer for one DynEq band.
//
// The old version compared the raw sidechain sample against the threshold every
// sample, so for any periodic signal the decision flipped twice per cycle and
// the envelope settled on a duty-cycle average of the two targets instead of
// tracking level. Detection and gain computation are separate now:
//
//   rectify -> peak detector (instant attack, program release)
//           -> gain computer (soft knee, ratio, clamped to the band's GAIN)
//           -> attack/release smoother
//
// The gain computer is what turns THRES from a switch into a threshold: the
// band moves in proportion to how far past it the sidechain is.
template<typename T>
class DynEQFollower {
public:
    // Knee width in dB, centred on the threshold. Fixed rather than exposed:
    // a KNEE control on top of THRES/RATIO/ATT/REL is more surface than this
    // effect needs, and 6 dB is soft enough to hide the corner.
    static constexpr T kKneeDb = T(6);

    void setAttack(T sr, T ms) {
        if (ms == _attMs && sr == _attSr) return;   // pow() is not free and this
        _attMs = ms;                                // runs once per band per block
        _attSr = sr;
        const T n = MS2SMPL(ms, sr);
        _attackDelta = n > 0 ? std::pow(T(0.01), T(1) / n) : T(0);
    }

    void setRelease(T sr, T ms) {
        if (ms == _relMs && sr == _relSr) return;
        _relMs = ms;
        _relSr = sr;
        const T n = MS2SMPL(ms, sr);
        _releaseDelta = n > 0 ? std::pow(T(0.01), T(1) / n) : T(0);
        // The detector falls on kDetectorFallMs, except that it must never be
        // SLOWER than the program release: a fast REL would then be capped by
        // the detector rather than by the knob (at a flat 30 ms, REL 1 ms still
        // took 17.6 ms to let go). Taking the min keeps the decoupling exactly
        // where the pinning problem was - the long-REL end - and leaves the
        // short end under the knob's control.
        const T dn = MS2SMPL(std::min(kDetectorFallMs, ms), sr);
        _detDelta = dn > 0 ? std::pow(T(0.01), T(1) / dn) : T(0);
    }

    void setMode(int mode) { _mode = mode; }

    // Detector fall time. Fixed, and deliberately NOT tied to REL: borrowing the
    // program release meant a long REL kept the detector pinned between hits on
    // dense material, so the target never moved and BOTH knobs went dead (a
    // 10 ms tone every 100 ms at REL 200 ms left the gain only 1.57 dB of travel
    // out of 12). Fast enough to reopen that travel, slow enough that the
    // detector still does not follow the waveform at low frequencies.
    // Derived in setRelease(), which DynEq calls for every band before any
    // detect() in the block - _detDelta must never still be zero at that point
    // or the detector degenerates into a plain rectifier and follows the
    // waveform. reset() clears _relMs so the next setRelease always recomputes.
    static constexpr T kDetectorFallMs = T(30);

    // Peak detector: instant attack so no transient is missed, own fall.
    inline void detect(T x) {
        const T r = std::abs(x);
        _det = r > _det ? r : _detDelta * (_det - r) + r;
        UDD(_det)
    }

    // Gain computer. The overshoot past the threshold maps 1:1 into applied
    // gain and is clamped to the band's GAIN, so the band reaches full GAIN
    // once the sidechain is GAIN dB past THRES. There is deliberately no ratio
    // control: measured, anything above ~4:1 is within ~1.5 dB of this curve
    // because the clamp dominates, and 1:1 only reproduces GAIN = 0. What makes
    // THRES a threshold rather than a switch is the detector, knee and clamp
    // below - not a ratio.
    //   thrDb   threshold, dB
    //   bandDb  the band's GAIN in dB, signed (a cut moves the other way)
    // The detector moves slowly by construction, so DynEq calls this at control
    // rate rather than per sample - the log10/exp2 are the only costly part.
    void updateTarget(T thrDb, T bandDb) {
        const T levelDb = LOG10D20(std::max(_det, T(1e-7)));
        // BELOW acts as the level falls under the threshold, ABOVE as it rises
        // over it. kneed is the softened overshoot and is never negative.
        const T over = _mode == 0 ? (thrDb - levelDb) : (levelDb - thrDb);

        T kneed;
        if (over <= -kKneeDb * T(0.5)) kneed = 0;
        else if (over >= kKneeDb * T(0.5)) kneed = over;
        else {
            const T t = over + kKneeDb * T(0.5);
            kneed = t * t / (T(2) * kKneeDb);
        }

        // Straight into dB and clamped to the band's own GAIN. Interpolating in
        // the amplitude domain instead would overshoot mid-range (half the
        // travel would be 7.0 dB of a 12 dB band, not 6.0), and with no ratio
        // control this curve is the only shaping there is, so it has to mean
        // what the comment above says. A band at 0 dB falls out at zero with no
        // special case. A = 10^(dB/40) = exp2(dB * log2(10)/40).
        const T appliedDb = std::copysign(std::min(kneed, std::abs(bandDb)), bandDb);
        _target = std::exp2(appliedDb * T(0.0830482023721841));
    }

    // Attack while moving away from unity, release while returning to it.
    inline T tick() {
        const T d = std::abs(_target - T(1)) > std::abs(_env - T(1)) ? _attackDelta
                                                                     : _releaseDelta;
        _env = d * (_env - _target) + _target;
        return _env;
    }

    T env() const { return _env; }

    void reset() {
        _det = 0;
        _env = _target = 1;
        _relMs = -1;   // force setRelease to recompute _detDelta
    }

private:
    T _det{}, _env{1}, _target{1};
    T _attackDelta{}, _releaseDelta{}, _detDelta{};
    T _attMs{-1}, _relMs{-1}, _attSr{-1}, _relSr{-1};
    int _mode{};
};


template<typename T>
class DynEq : public Effect {
public:
    // How often the band centre frequencies and the gain targets are refreshed.
    // The frequency smoother still advances every sample; only the coefficient
    // solve and the log10 in the gain computer are decimated.
    static constexpr int32_t kCoeffInterval = 16;
    static constexpr int32_t kGainInterval = 8;

    DynEq(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_DYNEQ5, MONOEFFECT), analyzer(_STATE, _DATA->inputeq5dyn[t->index]) {
        for (int32_t b = 0; b < 5; b++) {
            _cf[b] = &t->_STATE->params[t->index][DYNEQ5LOWCF + b * 3 + LOWCENTER];
            _gn[b] = &t->_STATE->params[t->index][DYNEQ5LOWCF + b * 3 + LOWGAIN];
            _q[b] = &t->_STATE->params[t->index][DYNEQ5LOWCF + b * 3 + QLOW];
            // Seed the frequency smoothers from the parameters. They used to
            // start at 0.01 (Hz), which swept every band up from DC on the
            // first block after the effect was created.
            smooth[b] = LOG2NORMAL(_cf[b]->load());
        }
        _gain = &t->_STATE->params[t->index][DYNEQ5GAIN];
        _bypass = &t->bypass[SPACE_DYNEQ5];
        Reset();
    }

    ~DynEq() override {
        for (int32_t i = 0; i < 5; i++) {
            _STATE->params[_track->index][DYNEQ5ENV10 + i + _chan * 5] = _STATE->parameters[
                    DYNEQ5ENV10 + i +
                    _chan * 5].initvalue;
        }
    }

    void Reset() {
        for (int32_t b = 0; b < 5; b++) {
            sect[b].reset();
            butter[b].reset();
            followers[b].reset();
        }
    }

    void compute(MYFLOAT *in, int32_t size) override {
        const MYFLOAT sr = _STATE->sr;
        auto &params = _STATE->params[_track->index];

        // ---- per-block band setup
        T freq[5], q[5], bandDb[5], thrDb[5], scMix[5], scDry[5];
        for (int32_t b = 0; b < 5; b++) {
            freq[b] = LOG2NORMAL(_cf[b]->load());
            q[b] = LOG2NORMAL(_q[b]->load());
            bandDb[b] = _gn[b]->load();
            thrDb[b] = params[DYNEQ5THR0 + b].load();

            scMix[b] = params[DYNEQFILT0 + b].load();
            scDry[b] = T(1) - scMix[b];

            // ATT/REL are Log10 params: stored as 20*log10(ms), so LOG2NORMAL
            // recovers the milliseconds. The curve lives on these original ids;
            // DYNEQ5ATTLOG0/DYNEQ5RELLOG0 are retired - do not read those.
            followers[b].setAttack(sr, LOG2NORMAL(params[DYNEQ5ATT0 + b].load()));
            followers[b].setRelease(sr, LOG2NORMAL(params[DYNEQ5REL0 + b].load()));
            followers[b].setMode(static_cast<int>(params[DYNEQBELOW0 + b].load()));
        }

        MYFLOAT mix;
        if (*_bypass || destroyRequested) {
            mix = 0.;
        } else {
            mix = 1.;
        }
        auto gain = dbToLinear60(*_gain);

        const bool doSpectrum = _DATA->updateRenderThreadeq5dyn[_track->index].load() == true && _chan == 0;
        if (!doSpectrum)analyzer.reset();

        for (int32_t i = 0; i < size; i++) {
            // The frequency smoothers advance every sample so their time
            // constant stays independent of the decimation below; only the
            // coefficient solve is decimated.
            for (int32_t b = 0; b < 5; b++) paramSmooth(freq[b], smooth[b]);
            if ((i % kCoeffInterval) == 0) {
                for (int32_t b = 0; b < 5; b++) {
                    sect[b].setFreq(sr, smooth[b], q[b]);
                    // Sidechain filters track the same smoothed frequency, so
                    // the detector's band no longer lags the band it controls.
                    if (b == 0) butter[b].SetLp(_appState->onedsr, smooth[b]);
                    else if (b == 4) butter[b].SetHp(_appState->onedsr, smooth[b]);
                    else butter[b].SetBp(_appState->onedsr, smooth[b], q[b]);
                }
            }

            const T sig = in[i];
            const bool doGain = (i % kGainInterval) == 0;

            // Every band listens to the effect's input, not to the output of
            // the band before it.
            followers[0].detect(scMix[0] * butter[0].tickLP(sig) + scDry[0] * sig);
            followers[1].detect(scMix[1] * butter[1].tickBP(sig) + scDry[1] * sig);
            followers[2].detect(scMix[2] * butter[2].tickBP(sig) + scDry[2] * sig);
            followers[3].detect(scMix[3] * butter[3].tickBP(sig) + scDry[3] * sig);
            followers[4].detect(scMix[4] * butter[4].tickHP(sig) + scDry[4] * sig);

            if (doGain)
                for (int32_t b = 0; b < 5; b++)
                    followers[b].updateTarget(thrDb[b], bandDb[b]);

            T out = sect[0].tick(sig, DynEqSection<T>::LOWSHELF, followers[0].tick());
            out = sect[1].tick(out, DynEqSection<T>::PEAKING, followers[1].tick());
            out = sect[2].tick(out, DynEqSection<T>::PEAKING, followers[2].tick());
            out = sect[3].tick(out, DynEqSection<T>::PEAKING, followers[3].tick());
            out = sect[4].tick(out, DynEqSection<T>::HIGHSHELF, followers[4].tick());

            in[i] = _smooth2 * _smooth1 * out + (1. - _smooth1) * in[i];
            if (doSpectrum)analyzer.tick(in[i]);
            sm1(mix);
            sm2(gain);
        }

        // Publish the envelopes for DynEqView. Write only - the follower owns
        // its state, and reading this back made the audio path depend on a
        // parameter the GUI, the host and preset loading can all write.
        for (int32_t b = 0; b < 5; b++)
            params[DYNEQ5ENV10 + b + _chan * 5].store(followers[b].env());
    }

private:
    T smooth[5]{};
    EqFilter<T> butter[5];
    DynEQFollower<T> followers[5];
    DynEqSection<T> sect[5];
    // [band] -> centre frequency / gain / Q parameter
    std::atomic<MYFLOAT> *_cf[5]{}, *_gn[5]{}, *_q[5]{};
    SpectrumAnalyzer analyzer;
};


#endif //GRAINSTORM_EQ_H
