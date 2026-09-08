#pragma once
//
// Created by pr on 08.12.18.
//

#ifndef GRAINSTORM_VOCODER_H
#define GRAINSTORM_VOCODER_H

#include <thread>
#include "tools.h"
#include "tools/aligned_memalloc.h"
#include "grainstorm.h"
#include "lpc.h"
#include "track.h"
#include "ffttools.h"

#define VOCODER_CHANNELS 28


class Vocoder : public Effect, private tsl::random::RandBase {
public:
    Vocoder(TRACK *_track, int32_t channel);

    void compute(MYFLOAT *in, int32_t size)override ;

private:
    // whitening: rms_env / (rms_sou + kEnvEps), boost capped so near-empty
    // carrier bands can't amplify the noise floor without bound
    static constexpr MYFLOAT kEnvEps = 1.0e-4f;
    static constexpr MYFLOAT kMaxBoost = 8.f; // ~ +18 dB
    // noise floor mixed into the sibilance band so consonants survive a
    // carrier without HF content
    static constexpr MYFLOAT kSibNoise = 0.02f;

    // synthesis-bank filter. Analysis is a complex analytic filter either way
    // (see setupAnalytic) -- this only picks what shapes the carrier. Safe to
    // mismatch the two banks because the whitening divides by the synthesis
    // band's own envelope, so its passband gain cancels; only shape, phase and
    // ring differ. No param wired yet; change the initialiser to A/B.
    enum SynthFilter { SYNTH_BUTTERBP = 0, SYNTH_RESONZ = 1 };
    // false spaces band centres geometrically again, to A/B against Bark
    static constexpr bool kBarkSpacing = true;
    // -3 dB width of two cascaded identical sections relative to one section,
    // sqrt(sqrt(2) - 1); narrows the resonz bandwidth to what the cascade
    // actually spans so both banks read the same width at a given Q
    static constexpr MYFLOAT kCascadeBW = 0.6436f;
    // resonz lands ~7.6 dB hotter than the cascade at matched width, measured
    // across channel counts / Q / bounds (spread ran +1 to +10 dB). It is not
    // the filter's own gain -- that cancels, since the whitening divides by the
    // synthesis band's own envelope -- it is the 6 dB/oct skirts overlapping
    // adjacent bands so they sum coherently. So the trim belongs on the summed
    // result in _norm, not on the coefficients.
    static constexpr MYFLOAT kResonZTrim = 0.42f;
    // A rectified waveform reads below the true magnitude the analytic filters
    // hand over, and the floors used to slow the low bands down on top of that,
    // so swapping the detector cost 5.9 dB mean across the same 27 configs
    // (Bark spacing accounted for only 0.45 dB of it). Unconditional, unlike
    // kResonZTrim, because both filter modes share the envelope path.
    static constexpr MYFLOAT kEnvTrim = 1.98f;

    // VOCATT/VOCREL sit on a Log10 curve: the stored value is 20*log10(ms).
    // _attackOld caches that stored value (it is what the change guard in
    // compute() compares against); _attackMs is the decoded time in ms.
    void setAttackTime(MYFLOAT attack_log) {
        _attackOld = attack_log;
        _attackMs = LOG2NORMAL(attack_log);
        _attack = std::exp(ANALOG_TC / ((_attackMs + 0.25) * _STATE->sr * 0.001));
    }

    void setReleaseTime(MYFLOAT release_log) {
        _releaseOld = release_log;
        _releaseMs = LOG2NORMAL(release_log);
        _release = std::exp(ANALOG_TC / ((_releaseMs + .25) * _STATE->sr * 0.001));
    }

    inline MYFLOAT tickEnvF(MYFLOAT in, MYFLOAT &state, const MYFLOAT att, const MYFLOAT rel) {
        in = std::abs(in);
        state = (in > state ? att : rel) * (state - in) + in;
        if (state < 1.0e-15f) state = 0.f; // flush denormals on decay
        return state;
    }

    inline void setupBP(MYFLOAT center, MYFLOAT bw, MYFLOAT coeffs[3]) {
        double pfreq = center * _pidsr;
        double pbw = bw * pfreq * 0.5f;

        double C = 1. / tan(pbw);
        double D = 2. * cos(pfreq);

        coeffs[0] = 1. / (1. + C);
        coeffs[1] = C * D * coeffs[0];
        coeffs[2] = (1. - C) * coeffs[0];
    }

    // two cascaded sections; each keeps its own delay pair (3,4 then 5,6) --
    // sharing one pair makes the second section read what the first just wrote,
    // which is not a series cascade and not the response below
    inline MYFLOAT tickBP12(const MYFLOAT in, MYFLOAT coeffs[7]) {
        MYFLOAT y0 = in + coeffs[1] * coeffs[3] + coeffs[2] * coeffs[4];
        MYFLOAT tmp = coeffs[0] * (y0 - coeffs[4]);
        coeffs[4] = coeffs[3];
        coeffs[3] = y0;
        y0 = tmp + coeffs[1] * coeffs[5] + coeffs[2] * coeffs[6];
        tmp = coeffs[0] * (y0 - coeffs[6]);
        coeffs[6] = coeffs[5];
        coeffs[5] = y0;
        return tmp;
        /*
        y0 = *(in++) + b1_bp * y1_bp1 + b2_bp * y2_bp1;
        tmpbuf[0] = a0_bp * (y0 - y2_bp1);
        y2_bp1 = y1_bp1;
        y1_bp1 = y0;
        y0 = tmpbuf[0] + b1_bp * y1_bp2 + b2_bp * y2_bp2;
        *(out++) = (MYFLOAT) (a0_bp * (y0 - y2_bp2));
        y2_bp2 = y1_bp2;
        y1_bp2 = y0;
         */
    }

    // Csound resonz: one pole pair behind the same 1 - z^-2 numerator butterbp
    // has, so DC and Nyquist stay nulled and carrier rumble can't leak into the
    // low bands. 6 dB/oct skirts instead of the cascade's 12, so bands bleed
    // more -- that wider overlap plus the longer ring is the point. Unlike
    // butterbp it stays stable for any bw, r < 1 always.
    // scale is Csound scaletype 2 (RMS for noise input); the whitening divides
    // it back out, it only keeps the magnitudes sane.
    // layout: scale,c1,c2 then y1,y2,x1,x2
    inline void setupResonZ(MYFLOAT center, MYFLOAT bw, MYFLOAT c[3]) {
        const double r = exp(-bw * _pidsr);
        const double c2 = r * r;
        c[0] = (MYFLOAT) sqrt((1. - c2) * .5);
        c[1] = (MYFLOAT) (2. * r * cos(2. * center * _pidsr));
        c[2] = (MYFLOAT) c2;
    }

    inline MYFLOAT tickResonZ(const MYFLOAT x, MYFLOAT c[7]) {
        MYFLOAT y = c[0] * (x - c[6]) + c[1] * c[3] - c[2] * c[4];
        UDD(y) // a narrow band rings a long way down into denormals
        c[6] = c[5];
        c[5] = x;
        c[4] = c[3];
        c[3] = y;
        return y;
    }

    // Peak gain |H(e^jw0)| of the resonz section at its centre. The denominator
    // factors as (1-r)(1 - r e^-2jw0), which is what keeps this closed form
    // short. Needed so the analytic envelope filter can be scaled to read the
    // same amplitude the synthesis filter actually puts out -- otherwise the
    // rms_env/rms_sou ratio loses the calibration kResonZTrim was measured for.
    inline MYFLOAT resonZPeakGain(MYFLOAT center, double r) const {
        const double w0 = 2. * center * _pidsr;
        const double s = sqrt((1. - r * r) * .5);
        const double den = (1. - r) * sqrt(1. - 2. * r * cos(2. * w0) + r * r);
        return (MYFLOAT) (2. * s * fabs(sin(w0)) / std::max(den, 1.0e-20));
    }

    // Complex (analytic) one-pole: y[n] = g*x[n] + r*e^(jw0)*y[n-1]. One pole
    // with no conjugate passes only positive frequencies, so |y| is the true
    // instantaneous magnitude -- none of the ripple at 2*fc you get from
    // rectifying a real bandpass. That ripple was the only reason the per-band
    // envelope floors existed, so they are gone and the follower runs at
    // whatever the user asks for.
    // peak scales it to match the signal-path filter's own peak gain. The 2 is
    // the analytic-signal convention: a real sine of amplitude A carries A/2 in
    // its positive-frequency half, which is all a one-sided filter sees, so
    // without it |y| reads half the band's true amplitude.
    // layout: g, r*cos(w0), r*sin(w0) then yre,yim
    inline void setupAnalytic(MYFLOAT center, double r, MYFLOAT peak, MYFLOAT c[3]) {
        const double w0 = 2. * center * _pidsr;
        c[0] = (MYFLOAT) (2. * peak * (1. - r));
        c[1] = (MYFLOAT) (r * cos(w0));
        c[2] = (MYFLOAT) (r * sin(w0));
    }

    inline MYFLOAT tickAnalytic(const MYFLOAT x, MYFLOAT c[5]) {
        MYFLOAT re = c[0] * x + c[1] * c[3] - c[2] * c[4];
        MYFLOAT im = c[2] * c[3] + c[1] * c[4];
        UDD(re)
        UDD(im)
        c[3] = re;
        c[4] = im;
        return std::sqrt(re * re + im * im);
    }

    // Traunmueller Bark scale. Geometric spacing spreads bands evenly in
    // octaves; Bark concentrates them in the 500-3000 Hz formant region where
    // intelligibility actually lives, so the same channel count reads clearer.
    static MYFLOAT hzToBark(MYFLOAT f) { return 26.81f * f / (1960.f + f) - 0.53f; }

    static MYFLOAT barkToHz(MYFLOAT z) { return 1960.f * (z + .53f) / (26.28f - z); }

    // RBJ highpass, Q = 1/sqrt(2); coeffs layout b0,b1,b2,a1,a2
    inline void setupHP(MYFLOAT cutoff, MYFLOAT c[5]) {
        const double w0 = 2. * cutoff * _pidsr;
        const double cw = cos(w0);
        const double alpha = sin(w0) * 0.70710678118654752;
        const double a0 = 1. / (1. + alpha);
        c[0] = (MYFLOAT) ((1. + cw) * .5 * a0);
        c[1] = (MYFLOAT) (-(1. + cw) * a0);
        c[2] = c[0];
        c[3] = (MYFLOAT) (-2. * cw * a0);
        c[4] = (MYFLOAT) ((1. - alpha) * a0);
    }

    inline MYFLOAT tickHP(const MYFLOAT x, MYFLOAT z[2]) {
        const MYFLOAT y = _hp[0] * x + z[0];
        z[0] = _hp[1] * x - _hp[3] * y + z[1];
        z[1] = _hp[2] * x - _hp[4] * y;
        return y;
    }

    MYFLOAT *modbuf;

    void reset();

    void clear();

    void retune(bool clearStates);

    void updateEnvCoeffs();

    MYFLOAT _qold{}, _band_low_old{}, _band_high_old{}, _channelsold{-1};
    std::atomic<MYFLOAT> *_att, *_rel, *_q, *_band_low, *_band_high, *_channels, *_gain, *_mix;
    MYFLOAT *insrc;
    MYFLOAT _attack{}, _release{}, _attackOld{}, _releaseOld{}, _attackMs{}, _releaseMs{};
    MYFLOAT _envstateSRC[VOCODER_CHANNELS]{}, _envfstateMOD[VOCODER_CHANNELS]{};
    // synthesis signal path: 3 coeffs then 4 states -- two delay pairs for the
    // butterbp cascade, or y1,y2,x1,x2 for resonz
    MYFLOAT _filtstateSrc[VOCODER_CHANNELS][7]{};
    // analysis: complex one-pole per band, 3 coeffs then re,im. The modulator
    // band signal was only ever used to derive an envelope, so nothing on the
    // signal path changed by swapping its bandpass out for this.
    MYFLOAT _analyticMod[VOCODER_CHANNELS][5]{}, _analyticSrc[VOCODER_CHANNELS][5]{};
    MYFLOAT _centers[VOCODER_CHANNELS]{};
    MYFLOAT _attackB[VOCODER_CHANNELS]{}, _releaseB[VOCODER_CHANNELS]{};
    int32_t _nchan{};
    int32_t _synthFilter{SYNTH_RESONZ}; // retune() picks this up, don't flip it mid-block
    MYFLOAT _norm{1.f};
    MYFLOAT _hp[5]{};
    MYFLOAT _hpModState[2]{}, _hpSrcState[2]{};
    MYFLOAT _envSibMod{}, _envSibSrc{};
    const MYFLOAT _pidsr;
};

#include "ffttools.h"
//#include "Convolver.h"

#define LPC_BUFLEN 1024
#define LPC_MAX_ORDER 150
#define LPCHOP 128


class LPC2 : private tsl::random::RandBase {
public:
    LPC2() {
        hamm();
    };

    void hamm(){
        for (size_t i = 0; i < LPC_BUFLEN; ++i) {
            env[i] = 0.54 - 0.46 * std::cos(2 * PI_P * i / (LPC_BUFLEN - 1));
        }
    }

    void computeLPC(const MYFLOAT x[], MYFLOAT coeffs[],
                    int32_t order);

    static void computeLPC(const MYFLOAT x[], MYFLOAT coeffs[], MYFLOAT r[],
                           int32_t order, int size, FFT &fft);

    // Variable-length reflection-coefficient analysis, for the cross-synthesis
    // LPC in pv.cpp. Same FFT autocorrelation as computeLPC above, but the
    // recursion returns k[1..order] rather than direct-form a[k], because the
    // synthesis lattice interpolates its coefficients across a grain and only
    // |k| < 1 gives a stability guarantee that survives a linear blend.
    //
    // r[] is scratch and must hold 2 * size doubles. k[] must hold order + 1.
    // Returns the residual-to-signal amplitude ratio, i.e. how much of the
    // frame the fit failed to predict, which is a usable excitation gain.
    static MYFLOAT computeReflection(const MYFLOAT x[], MYFLOAT k[], MYFLOAT r[],
                                     int32_t order, int size, FFT &fft, MYFLOAT sr);

    std::vector<double> levinsonDurbin(const MYFLOAT in[], size_t order) {
        std::vector<double> a(order + 1, 0.0);
        std::vector<double> e(order + 1, 0.0);
        std::vector<double> k(order + 1, 0.0);
        std::vector<double> temp_a(order + 1, 0.0);
        for (int32_t i = 0; i < LPC_BUFLEN; i++) {
            r[i] = in[i] * env[i] + 0.001 * BiRandGab;
        }

        autoCorr.compute(r);


        e[0] = r[0];

        for (size_t i = 1; i <= order; ++i) {
            double sum = 0.0;
            for (size_t j = 1; j < i; ++j) {
                sum += a[j] * r[i - j];
            }
            k[i] = (r[i] - sum) / e[i - 1];
            a[i] = k[i];

            for (size_t j = 1; j < i; ++j) {
                temp_a[j] = a[j] - k[i] * a[i - j];
            }

            for (size_t j = 1; j < i; ++j) {
                a[j] = temp_a[j];
            }

            e[i] = (1 - k[i] * k[i]) * e[i - 1];
        }

        return std::vector<double>(a.begin() + 1, a.end());
    }
private:
    MYFLOAT r[LPC_BUFLEN], env[LPC_BUFLEN+2];
    AutoCorrelation autoCorr{LPC_BUFLEN};
};

typedef std::complex<double> dcomp;

class FIR {
public:
    template<typename T>
    static void convolve(const T Signal[/* SignalLen */], size_t SignalLen,
                         const T Kernel[/* KernelLen */], size_t KernelLen,
                         T Result[/* SignalLen + KernelLen - 1 */]) {

        for (int32_t n = 0; n < SignalLen + KernelLen - 1; n++) {

            Result[n] = 0;

            size_t kmin = (n >= KernelLen - 1) ? n - (KernelLen - 1) : 0;
            size_t kmax = (n < SignalLen - 1) ? n : SignalLen - 1;

            for (size_t k = kmin; k <= kmax; k++) {
                Result[n] += Signal[k] * Kernel[n - k];
            }
        }
    }

    template<typename T>
    static T frequencyResponse(T coeffs[], int32_t taps, T fr, T sr) {

        const T w = fr / sr * TWOPI_P;
        T res = coeffs[0];
        for (int32_t i = 1; i < taps; i++)
            res += coeffs[i] * cos(w * i);

//        std::complex<double> res(0, 0);
        //      for (int32_t i = 0; i < taps; i++)
        //        res += coeffs[i] * exp(w * i * -J);
        //  LOGE("%g %g", res1, std::abs(res));
        return res;
    }
};

template<typename T>
class Emphasis : public FIR {
public:
    Emphasis(T sr, T alpha) {
        T cp[2] = {1, -alpha}, cd[2] = {-alpha, 1};
        convolve(cp, 2, cd, 2, tapspre.data());
        prefact = 1. / frequencyResponse<T>(tapspre.data(), 3, 1000., sr);
        for (auto &t :tapspre)
            t *= prefact;

        T cp2[2]{alpha, 1 - alpha}, cd2[2] = {1 - alpha, alpha};
        convolve(cp2, 2, cd2, 2, tapsde.data());
    }

    void reset() {
        std::fill(premem.begin(), premem.end(), 0);
        std::fill(demem.begin(), demem.end(), 0);
    }

    inline T tickPre(T in) {
        T tmp = in * tapspre[0] + premem[0] * tapspre[1] + premem[1] * tapspre[2];
        premem[1] = premem[0];
        premem[0] = in;
        return tmp;
    }

    inline T tickDe(T in) {
        T tmp = in * tapsde[0] + demem[0] * tapsde[1] + demem[1] * tapsde[2];
        demem[1] = demem[0];
        demem[0] = in;
        return tmp;
    }

private:
    T prefact{};
    std::array<T, 3> tapspre, tapsde;
    std::array<T, 2> premem{}, demem{};
};

class LPCVocoder3 : public Effect {
public:
    LPCVocoder3(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_LPCVOCODER, MONOEFFECT) {
        ctrlbuf = track->destinationz->envf_buffer[channel];
        _order = &_STATE->params[track->index][LPCVOCORDER];
        oldorder = *_order;
        _gain = &_STATE->params[track->index][LPCVOCGAIN];
        _smooth2 = dbToLinear60(*_gain);
        _mix = &_STATE->params[track->index][LPCVOCMIX];
        _bypass = &track->bypass[SPACE_LPCVOCODER];
        white = &_STATE->params[track->index][LPCVOCWHITE];
        oldwhite = *white == 1.0f;
        emphasis = &_STATE->params[track->index][LPCVOCEMPH];
        oldemph = *emphasis == 1.0f;
    }


    void compute(MYFLOAT *in, int32_t s)override ;

    void reset() {
        memset(iirmem, 0, sizeof(double) * LPC_MAX_ORDER);
    }

    void coef2Pole(const double *c);

    void coef2Parm();

    void resonBnk(MYFLOAT *in, int32_t size);

private:
    MYFLOAT modbuf[LPC_BUFLEN]{};
    MYFLOAT carrierbuf[LPC_BUFLEN]{};
    MYFLOAT firmem[LPC_MAX_ORDER]{};
    MYFLOAT iirmem[LPC_MAX_ORDER]{};
    MYFLOAT computebufmod[LPC_BUFLEN]{};
    MYFLOAT computebufcarrier[LPC_BUFLEN]{};
    std::array<MYFLOAT, LPC_MAX_ORDER + 1> coeffsmod{}, coeffscarr{}, nextcoeffsmod{}, nextcoeffscarr{}, coeffsmodInc{}, coeffscarrInc{};
    MYFLOAT *cfsmod{&coeffsmod[1]}, *cfscar{&coeffscarr[1]}, *cfsmodInc{&coeffsmodInc[1]}, *cfscarInc{&coeffscarrInc[1]};

    int32_t tmpoff{};
    MYFLOAT *ctrlbuf;
    std::atomic<MYFLOAT> *_order, *white, *emphasis;
    bool oldwhite, oldemph;
    LPC2 lpc{};
    int32_t oldorder{}, nextorder{};
    int32_t rp{};
    int32_t hopcount{1};

    std::array<tsl::complex<MYFLOAT>, LPC_MAX_ORDER + 1> pl{0, 0};
    std::array<MYFLOAT, LPC_MAX_ORDER + 1> cf{0};
    std::array<MYFLOAT, LPC_MAX_ORDER * 2> pp{0};
    int32_t resonord{};
    MYFLOAT sum{};
    MYFLOAT yt1[100]{}, yt2[100]{}, c2o[100]{}, c3o[100]{}, c2[100]{}, c3[100]{};
    MYFLOAT pree{}, dee{};
};

template<typename T>
class CircularBufferCross2 {
protected:
    CircularBufferCross2(int size, float overlap):_buffersize(size), _hopsize((int) (size / overlap)) {
        inbuf1.resize(_buffersize, 0);
        inbuf2.resize(_buffersize, 0);
        outbuf.resize(_buffersize, 0);
        computeBuf1.resize(_buffersize, 0);
        computeBuf2.resize(_buffersize, 0);
    }
    virtual void onBufferReady(T*, T*, int s) = 0;

    inline T _tick(T in1, T in2) {
        auto out = outbuf[count];
        inbuf1[_buffersize - _hopsize + count] = in1;
        inbuf2[_buffersize - _hopsize + count] = in2;
        if(++count==_hopsize){
            count = 0;
            computeBuf1 = inbuf1;
            computeBuf2 = inbuf2;
            onBufferReady(computeBuf1.data(),computeBuf2.data(), _buffersize);
            for(int i = 0;i<_buffersize - _hopsize;i++){
                inbuf1[i] = inbuf1[i+_hopsize];
                inbuf2[i] = inbuf2[i+_hopsize];
                outbuf[i] = outbuf[i+_hopsize];
                outbuf[i] += computeBuf1[i];
            }
            for(int i=_buffersize-_hopsize;i<_buffersize;i++){
                outbuf[i] = computeBuf1[i];
            }
        }
        return out;
    }

    int getDelay() {
        return _buffersize;
    }

private:
    tsl::AlignedVector<T> inbuf1, inbuf2 ,outbuf, computeBuf1, computeBuf2;
    int count{};
    const int _buffersize;
    const int _hopsize;
};

class LPCVocoder4 : public Effect, private CircularBufferCross2<MYFLOAT> {
public:
    LPCVocoder4(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_LPCVOCODER, MONOEFFECT), CircularBufferCross2<MYFLOAT>(LPC_BUFLEN, 2) {
        ctrlbuf = track->destinationz->envf_buffer[channel];
        _order = &_STATE->params[track->index][LPCVOCORDER];
        _gain = &_STATE->params[track->index][LPCVOCGAIN];
        _smooth2 = dbToLinear60(*_gain);
        _mix = &_STATE->params[track->index][LPCVOCMIX];
        _bypass = &track->bypass[SPACE_LPCVOCODER];
        white = &_STATE->params[track->index][LPCVOCWHITE];
        emphasis = &_STATE->params[track->index][LPCVOCEMPH];
        for (size_t i = 0; i < LPC_BUFLEN; ++i) {
            env[i] = .5 - .5 * cos(TWOPI_P * i / (MYFLOAT) LPC_BUFLEN);
        }
    }

    void onBufferReady(MYFLOAT*, MYFLOAT*, int size) override;

    void compute(MYFLOAT *in, int32_t s)override ;

private:
    static constexpr int N = LPC_BUFLEN*2;
    MYFLOAT env[LPC_BUFLEN]{};
    MYFLOAT carrierbuf[LPC_BUFLEN]{};
    MYFLOAT r[N]{};
    MYFLOAT e[LPC_MAX_ORDER+1]{};
    MYFLOAT k[LPC_MAX_ORDER+1]{};
    MYFLOAT temp_a[LPC_MAX_ORDER+1]{};
    MYFLOAT coeffsmod[LPC_MAX_ORDER + 1]{};
    MYFLOAT coeffscarr[LPC_MAX_ORDER + 1]{};
    MYFLOAT iirmem[LPC_MAX_ORDER+1]{}, firmem[LPC_MAX_ORDER+1]{};

    MYFLOAT *cfsmod{&coeffsmod[1]}, *cfscar{&coeffscarr[1]};
    MYFLOAT *ctrlbuf;
    std::atomic<MYFLOAT> *_order, *white, *emphasis;
    LPC2 lpc;
    MYFLOAT pree{}, dee{};
    FFT fft{N};
    //p->rms = SQRT(ro/N)
    void levinsonDurbin(const MYFLOAT in[], MYFLOAT coeffs[], size_t order) {
        for(int i=0;i<order+1;i++) coeffs[i] = e[i] = k[i] = temp_a[i] = 0.0;
        for (int32_t i = 0; i < LPC_BUFLEN; i++) {
            r[i] = in[i] * env[i];
        }
        for (int32_t i = LPC_BUFLEN; i < N; i++) {
            r[i] = 0.0;
        }

        fft.forward(r);

        r[0] = r[0] * r[0];
        r[1] = r[1] * r[1];

        for (int32_t i = 2; i < N; i += 2) {
            r[i] = r[i] * r[i] + r[i + 1] * r[i + 1];
            r[i + 1] = 0;
        }

        fft.backward(r);


        e[0] = r[0];

        for (size_t i = 1; i <= order; ++i) {
            double sum = 0.0;
            for (size_t j = 1; j < i; ++j) {
                sum += coeffs[j] * r[i - j];
            }
            k[i] = (r[i] - sum) / e[i - 1];
            coeffs[i] = k[i];

            for (size_t j = 1; j < i; ++j) {
                temp_a[j] = coeffs[j] - k[i] * coeffs[i - j];
            }

            for (size_t j = 1; j < i; ++j) {
                coeffs[j] = temp_a[j];
            }

            e[i] = (1 - k[i] * k[i]) * e[i - 1];
        }
    }

};

// ===========================================================================
// LPCVocoder5 -- continuous-state lattice vocoder.
//
// Replaces the overlap-add synthesis of LPCVocoder4. Overlap-add is fine for
// LPC *analysis* (framing + windowing) but wrong for LPC *synthesis*: the old
// code zeroed the all-pole filter state every hop, which amputated the formant
// ringing (longer than the hop at any useful Q) and then summed two
// correlated-but-phase-incoherent copies of the same carrier, giving comb
// artefacts at the frame rate. Here one filter runs continuously and the
// coefficients are interpolated across the hop instead.
//
// That interpolation is why the analysis returns reflection coefficients
// rather than direct-form a[k]: |k|<1 is the stability condition for a
// lattice, and a linear blend of two stable sets stays stable. Blending
// direct-form coefficients has no such guarantee.
// ===========================================================================

#define LPC_HOP 256

// Deliberately separate from LPC2, which pv.cpp and granulate_fft.cpp still use.
class LpcReflection {
public:
    explicit LpcReflection(MYFLOAT sr) {
        for (int32_t i = 0; i < LPC_BUFLEN; ++i)
            win[i] = 0.54 - 0.46 * std::cos(TWOPI_P * i / (MYFLOAT) (LPC_BUFLEN - 1));
        setSampleRate(sr);
    }

    void setSampleRate(MYFLOAT sr) {
        // Conditioning applied to the autocorrelation before Levinson:
        //   Gaussian lag window -- tames the noisy high-lag estimates and keeps
        //                          the recursion well conditioned on near-silent
        //                          or very tonal input.
        //   exponential g^i     -- exactly equivalent to bandwidth expansion
        //                          a[k] *= g^k (it maps every pole p -> p*g), so
        //                          Levinson hands back reflection coefficients
        //                          that are already expanded and no step-down
        //                          conversion is needed.
        const double gauss = TWOPI_P * LAG_WINDOW_HZ / sr;
        const double g = std::exp(-PI_P * BW_EXPAND_HZ / sr);
        double gpow = 1.;
        for (int32_t i = 0; i <= LPC_MAX_ORDER; ++i) {
            const double lag = gauss * i;
            rWin[i] = std::exp(-0.5 * lag * lag) * gpow;
            gpow *= g;
        }
    }

    // x holds LPC_BUFLEN samples, oldest first. Fills k[1..order] with the
    // reflection coefficients. Level is not returned: the vocoder sets its gain
    // from the lattice's measured gain instead, which holds for any carrier.
    void analyse(const MYFLOAT x[], MYFLOAT k[], int32_t order);

private:
    static constexpr double LAG_WINDOW_HZ = 60.;
    static constexpr double BW_EXPAND_HZ = 40.;
    static constexpr double MAX_REFL = 0.999;

    MYFLOAT win[LPC_BUFLEN]{};
    double rWin[LPC_MAX_ORDER + 1]{};
    double r[LPC_BUFLEN]{};            // scratch: windowed frame in, lags out
    double a[LPC_MAX_ORDER + 1]{};
    double tmp[LPC_MAX_ORDER + 1]{};
    AutoCorrelation autoCorr{LPC_BUFLEN};
};

class LPCVocoder5 : public Effect {
public:
    LPCVocoder5(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_LPCVOCODER, MONOEFFECT),
                                                 lpc(track->_appState->sr) {
        ctrlbuf = track->destinationz->envf_buffer[channel];
        _modOrder = &_STATE->params[track->index][LPCVOCORDER];
        _gain = &_STATE->params[track->index][LPCVOCGAIN];
        _smooth2 = dbToLinear60(*_gain);
        _mix = &_STATE->params[track->index][LPCVOCMIX];
        _bypass = &track->bypass[SPACE_LPCVOCODER];
        white = &_STATE->params[track->index][LPCVOCWHITE];
        emphasis = &_STATE->params[track->index][LPCVOCEMPH];
        // modulator loud-level reference releases over ~1 s so it holds across
        // speech pauses; the envelope itself releases over ~25 ms
        modRefRelease = 1. - std::exp(-(double) LPC_HOP / (1.0 * _STATE->sr));
        duckRelease = 1. - std::exp(-(double) LPC_HOP / (0.025 * _STATE->sr));
        // The order params are calibrated at 48 kHz. How many poles it takes to
        // resolve formants scales with the sample rate, so scale with it and a
        // preset keeps its sound at 44.1 / 96 / 192 kHz.
        orderScale = _STATE->sr / 48000.f;
    }

    void compute(MYFLOAT *in, int32_t s) override;

private:
    void analyse();

    // --- analysis side ---
    MYFLOAT modRing[LPC_BUFLEN]{}, carRing[LPC_BUFLEN]{};
    MYFLOAT modFrame[LPC_BUFLEN]{}, carFrame[LPC_BUFLEN]{};
    int32_t ringPos{};
    int32_t hopCount{1};
    MYFLOAT preState{};                 // pre-emphasis delay, analysis path only
    MYFLOAT preAlpha{};

    // --- reflection coefficients: current value, per-sample increment, target ---
    MYFLOAT kMod[LPC_MAX_ORDER + 1]{}, kModInc[LPC_MAX_ORDER + 1]{}, kModTgt[LPC_MAX_ORDER + 1]{};
    MYFLOAT kCar[LPC_MAX_ORDER + 1]{}, kCarInc[LPC_MAX_ORDER + 1]{}, kCarTgt[LPC_MAX_ORDER + 1]{};
    MYFLOAT exciteGain{}, exciteGainInc{};

    // runOrder is what the sample loop traverses; tgtOrder is where it settles
    // once retired stages have finished ramping their k down to zero.
    int32_t runModOrder{}, tgtModOrder{};
    int32_t runCarOrder{}, tgtCarOrder{};

    // --- lattice state (b = backward prediction errors, one sample delayed) ---
    MYFLOAT bMod[LPC_MAX_ORDER + 1]{};  // all-pole, synthesis
    MYFLOAT bCar[LPC_MAX_ORDER + 1]{};  // FIR, carrier whitening

    // --- level control: output = carrier level x modulator envelope. The
    // --- carrier is followed live through the signal path, the modulator ducks
    // --- it, and chainGain measures what the filters do so the two land where
    // --- they are meant to whatever the material is ---
    double modMsAccum{}, dryMsAccum{}, outMsAccum{}, gainAccum{};
    double chainGain{1.}, modRefMs{}, duckSm{};
    double modRefRelease{}, duckRelease{};
    int32_t hopSamples{};
    MYFLOAT orderScale{1.f};

    bool chainGainStale{true};

    static constexpr double CHAIN_GAIN_SMOOTH = 0.5;    // ~2 hops, about 10 ms
    static constexpr MYFLOAT MAX_EXCITE_GAIN = 1e4f;
    static constexpr MYFLOAT MAX_GAIN_RISE_PER_HOP = 2.f;
    static constexpr MYFLOAT CAR_ORDER_AT_48K = 8.f;

    MYFLOAT denormFlip{1e-20f};

    MYFLOAT *ctrlbuf;
    std::atomic<MYFLOAT> *_modOrder, *white, *emphasis;
    LpcReflection lpc;
};


#endif //GRAINSTORM_VOCODER_H
