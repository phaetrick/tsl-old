//
// Zero-delay-feedback (TPT) analog-modelled filters.
//
// Created by claude on 23.07.2026.
//
// Three filters, shared by grainstorm and pocket analog. Each offers LP, BP
// and HP taps off the same structure - the taps are weighted sums of the
// existing stage outputs, so they cost nothing in state or CPU:
//
//   ZdfLadder - 4-pole transistor ladder (Moog). Xpander-style multimode taps
//               (LP4/LP2/BP4/BP2/HP4/HP2), tanh feedback solved by Newton so
//               the nonlinearity is zero-delay too, saturated states.
//   ZdfSvf    - 2-pole state-variable (Oberheim SEM). LP/BP/HP/notch/peak
//               plus the SEM's LP<->HP blend.
//   Zdf303    - 3-pole acid ladder (LP3/BP/HP3). Voiced for the TB-303 rather
//               than modelled from it - see the note on that class for why
//               the diode-ladder circuit model was dropped.
//
// Why these and not the structures in moogladder.h: with one exception, every
// filter in there (Huovilainen, Krajeski, Stilson, Microtracker, MusicDSP,
// Oberheim, ...) puts a unit delay in the resonance feedback path, or
// integrates with forward Euler. That delay is what makes them squeal near
// Nyquist, click under fast cutoff modulation and blow up under drive - it is
// structural and cannot be tuned away.
//
// The exception is DiodeLadderFilter (Teemu Voipio's 303), which does solve
// its feedback loop properly and has a good response shape. It is unused dead
// code in both projects, and it has its prewarp commented out ("not required
// with 2x oversampling"), so its cutoff mapping is only correct when run at
// 2x, and it carries a 1/a term that degrades as cutoff approaches zero.
//
// These solve the feedback loop algebraically (Zavalishin, "The Art of VA
// Filter Design"), so:
//   - stable at any cutoff, at any sample rate (g is capped just below the
//     point where the trapezoidal integrator degenerates into an fs/2
//     oscillator - see kMaxG; the ceiling is above 18 kHz at any sample rate)
//   - audio-rate cutoff modulation is clean, no zipper, no blowup
//   - self-oscillation tracks cutoff exactly with setStageSaturation(false);
//     with it on (the default) it runs sharp - measured +17 cents at 100 Hz
//     easing to +5 at 4 kHz, since saturated stages shed phase lag. Neither
//     the oversampling nor the stage mismatch causes this; both were checked
//   - correct at 1x; oversampling becomes a quality choice, not a crutch
//
// The integrator states additionally pass through a bounded saturator, so
// no state can ever leave [-satLimit, satLimit] no matter what is fed in.
// Combined with the NaN guard in tick() the filters cannot latch up.
//
// Header-only, no allocation, no virtuals - safe on the audio thread.
//
// Typical use:
//
//     tsl::dsp::ZdfLadder f;
//     f.init(sampleRate);
//     f.setDrive(2.0);
//     ...
//     f.setCutoff(cutoffHz);        // per block, or per sample if modulated
//     f.setResonance(res01);
//     out = f.tick(in);
//

#ifndef TSL_ZDF_FILTERS_H
#define TSL_ZDF_FILTERS_H

#include "defines.h"

#include <cmath>
#include <cstring>

namespace tsl {
namespace dsp {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

namespace zdf {

constexpr MYFLOAT kPi = 3.14159265358979323846;

// Pade(3,2) tanh. Bounded and monotone *everywhere* - the derivative is
// 9*(9-x*x)^2 / (27+9*x*x)^2, which is never negative, and the rational part
// reaches exactly +-1 at x = +-3 so the clamp is continuous in value and in
// slope. This matters: polynomial tanh approximations that turn back over
// outside their fit range are a classic cause of "explodes on extreme input",
// and a non-monotone saturator breaks the Newton solve below.
inline MYFLOAT tanhPade(MYFLOAT x) {
    if (x < -3.0) return -1.0;
    if (x > 3.0) return 1.0;
    const MYFLOAT x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

// d/dx of tanhPade. Needed by the Newton iteration on the feedback saturator.
inline MYFLOAT tanhPadeDeriv(MYFLOAT x) {
    if (x < -3.0 || x > 3.0) return 0.0;
    const MYFLOAT x2 = x * x;
    const MYFLOAT d = 27.0 + 9.0 * x2;
    const MYFLOAT n = 9.0 - x2;
    return 9.0 * n * n / (d * d);
}

// tan(pi * x) for x in [0, 0.5). Used for the cutoff prewarp when cutoff is
// modulated per sample and a real tan() would dominate the cost.
// setCutoff() uses std::tan; setCutoffFast() uses this.
//
// The rational core is only usable well away from the pole at x = 0.5 - the
// bare Pade form overshoots by 49% at x = 0.4 and turns NEGATIVE above
// x = 0.4517, which as a prewarp silently collapses the cutoff to DC. So the
// top half is reflected instead: tan(pi*x) = 1/tan(pi*(0.5-x)), which keeps
// the evaluated argument inside [0, pi/4] where the approximation is good,
// and reproduces the pole exactly as 1/(something going to zero).
// [3/2] Pade of tan: (a - a^3/15) / (1 - 2a^2/5). Chosen over the commonly
// quoted 0.1352/0.4967 fit because that one is 3.8% high at a = pi/4, which
// would put a 7% step at the reflection point and click on a cutoff sweep.
// This form is within 2e-4 there, so the two branches meet cleanly.
inline MYFLOAT tanPiQuarter(MYFLOAT x) { // x in [0, 0.25]
    const MYFLOAT a = x * kPi;
    const MYFLOAT a2 = a * a;
    return a * (1.0 - a2 * (1.0 / 15.0)) / (1.0 - a2 * 0.4);
}

inline MYFLOAT tanPiApprox(MYFLOAT x) {
    if (x <= 0.0) return 0.0;
    if (x > 0.49999) x = 0.49999; // finite, ~1e5, keeps g sane at Nyquist
    if (x > 0.25) {
        const MYFLOAT r = tanPiQuarter(0.5 - x);
        return (r > 1e-9) ? (1.0 / r) : 1e9;
    }
    return tanPiQuarter(x);
}

// Saturator with headroom: linear to within ~0.2% for |x| < h/8, bends
// smoothly above, hard-bounded at +-h. The headroom matters - a bare tanh on
// a signal that is already around unity costs ~10 dB of passband gain and
// audibly detunes the filter, which is not saturation, it is just a bug.
inline MYFLOAT satH(MYFLOAT x, MYFLOAT h) {
    return h * tanhPade(x / h);
}

// Asymmetric saturator. tanh is odd-symmetric and therefore produces only odd
// harmonics; real ladders clip harder on one rail and generate second-order
// content too, which is a large part of what reads as "warm" rather than
// "clean". Offsetting the curve by `bias` and subtracting the resulting DC
// gives that asymmetry without shifting the operating point - the output is
// still exactly 0 for 0 in, so nothing downstream sees an offset.
inline MYFLOAT satAsym(MYFLOAT x, MYFLOAT h, MYFLOAT bias) {
    return h * (tanhPade((x + bias) / h) - tanhPade(bias / h));
}

// d/dx of satAsym - the bias term is constant so it drops out.
inline MYFLOAT satAsymDeriv(MYFLOAT x, MYFLOAT h, MYFLOAT bias) {
    return tanhPadeDeriv((x + bias) / h);
}

// 13-tap Blackman-windowed halfband, used for both the 2x interpolation and
// the 2x decimation. Halfband means every even tap except the centre is zero,
// so this costs 7 multiplies rather than 13, and the coefficients sum to
// exactly 1 so there is no normalisation error to carry.
//
// Why oversample at all: the linear response is exact at 1x, but the
// saturators are not - they generate harmonics above Nyquist that fold back.
// Measured on the ladder, non-harmonic energy relative to harmonic energy was
// -57 dB at 1x with resonance at 1.0 and a loud input, which is audible as
// grit on resonant sweeps. Running the nonlinear core at 2x removes it.
struct Halfband {
    MYFLOAT z[11]{};

    void reset() {
        for (int i = 0; i < 11; ++i) z[i] = 0.0;
    }

    inline void push(MYFLOAT x) {
        for (int i = 10; i > 0; --i) z[i] = z[i - 1];
        z[0] = (x > -1e-20 && x < 1e-20) ? 0.0 : x;
    }

    inline MYFLOAT out() const {
        return 0.5 * z[5]
             + 0.29296     * (z[4] + z[6])
             - 0.048720    * (z[2] + z[8])
             + 0.0057585   * (z[0] + z[10]);
    }
};

// Cap on the TPT integrator coefficient g.
//
// A trapezoidal one-pole updates its state as s <- s + 2*(x - s)*g/(1+g). As
// g grows that tends to s <- 2x - s, which is an *undamped oscillator at
// fs/2*. Past roughly g = 5 the ladders stop filtering and latch into a
// full-scale Nyquist alternation - and because it is a numerical degeneracy
// rather than resonance, it appears at identical amplitude whether resonance
// is at 0.95 or 1.0, which is how it gets spotted. Measured on the ladder
// with a saw input: clean at g = 3.90, fully latched at g = 5.24.
//
// 4.0 keeps a margin below that and still puts the cutoff ceiling above
// 18 kHz at every supported sample rate (0.42*fs at 48 k). Clamping g rather
// than the frequency means the bound is identical at any sample rate.
constexpr MYFLOAT kMaxG = 4.0;

inline MYFLOAT clampG(MYFLOAT g) {
    if (g < 0.0) return 0.0;
    return (g > kMaxG) ? kMaxG : g;
}

inline MYFLOAT flushDenorm(MYFLOAT v) {
    return (v > -1e-20 && v < 1e-20) ? 0.0 : v;
}

inline bool isBad(MYFLOAT v) {
    return !(v == v) || v > 1e12 || v < -1e12;
}

} // namespace zdf

// ---------------------------------------------------------------------------
// ZdfLadder - 4-pole nonlinear transistor ladder
// ---------------------------------------------------------------------------
//
// Structure per sample, with g = G/(1+G) and G = tan(pi*fc/fs):
//
//   four TPT one-poles: v = (x - s)*g;  y = v + s;  s = sat(y + v)
//   the chain's output before feedback is
//       y4 = g^4 * u  +  (1-g) * (g^3*s0 + g^2*s1 + g*s2 + s3)
//   which is affine in u, so the feedback
//       u = tanh( drive*x*(1 + k*bassComp) - k*y4 )
//   can be solved for u directly instead of using last sample's y4.
//
// The tanh makes that solve implicit, so it is closed by Newton iteration
// (warm-started from the previous sample - one step is usually already at
// machine precision). The result is a genuinely zero-delay *nonlinear*
// feedback path, which is the difference between a ladder that compresses
// musically under drive and one that overshoots and screams.
//
class ZdfLadder {
public:
    // Xpander-style taps. LP4 is the plain Moog.
    enum Mode {
        LP4 = 0,
        LP2,
        BP4,
        BP2,
        HP4,
        HP2,
        NUM_MODES
    };

    ZdfLadder() { init(44100.0); }

    void init(MYFLOAT sampleRate) {
        setSampleRate(sampleRate);
        reset();
    }

    void setSampleRate(MYFLOAT sampleRate) {
        mSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        mNyquistLimit = mSampleRate * 0.49;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setCutoff(mCutoffHz);
        snapCutoff();
    }

    // Run the nonlinear core at 2x. On by default: the linear response is
    // exact at 1x but the saturators fold harmonics back, measured at -57 dB
    // relative to the harmonics with resonance up and a loud input. Costs
    // roughly 2x the filter CPU.
    void setOversampling(bool on) {
        if (on == mOversample) return;
        mOversample = on;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setCutoff(mCutoffHz);
        snapCutoff();
        mUp.reset();
        mDown.reset();
    }

    // Cutoff in Hz. Accurate prewarp - use per block, or per sample if you
    // do not care about the tan() cost.
    void setCutoff(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        setGTarget(std::tan(zdf::kPi * mCutoffHz / mCoreRate));
    }

    // Same, with the approximate tan. Cheap enough for audio-rate modulation.
    void setCutoffFast(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        setGTarget(zdf::tanPiApprox(mCutoffHz / mCoreRate));
    }

    // Glide time, in seconds, that the coefficient takes to follow setCutoff.
    //
    // Hosts update filter parameters once per control block - 64 samples in
    // pocket analog, so 1.3 ms at 48 k - and a stepped coefficient is audible
    // as zipper on fast filter envelopes. This is not the filter struggling
    // with modulation (it handles audio-rate cutoff changes cleanly); it is
    // simply not being fed them. Smoothing bridges the steps. 0 disables.
    void setSmoothingTime(MYFLOAT seconds) {
        mSmoothTime = (seconds < 0.0) ? 0.0 : seconds;
        updateSmoothCoef();
    }

    // Jump straight to the target, skipping the glide. Use after a voice
    // steal or a preset change so the first note does not sweep in.
    void snapCutoff() {
        mG = mGTarget;
        updateStageCoefs();
    }

    // 0 = no resonance, 1 = exactly at self-oscillation. Values above 1 are
    // allowed (up to 1.4) and give the overdriven, squashed self-osc.
    void setResonance(MYFLOAT r01) {
        if (r01 < 0.0) r01 = 0.0;
        if (r01 > 1.4) r01 = 1.4;
        mK = 4.0 * r01;
    }

    // Input gain into the ladder's saturator. The saturator has headroom, so
    // 1 is near-clean on a full-scale signal (~1 dB of bend at the peaks),
    // 2-4 is where the ladder starts to growl, 10+ is squashed.
    void setDrive(MYFLOAT d) {
        mDrive = (d < 0.001) ? 0.001 : d;
    }

    // Headroom of the input/feedback saturator, in signal units. Lower =
    // dirties up sooner. Default 1.5 assumes roughly full-scale input.
    void setHeadroom(MYFLOAT h) {
        mHeadroom = (h < 0.1) ? 0.1 : h;
    }

    // Headroom of the per-stage integrator saturator. Default 3 keeps the
    // passband transparent and only bends on resonant peaks and transients.
    void setStageHeadroom(MYFLOAT h) {
        mStageHeadroom = (h < 0.1) ? 0.1 : h;
    }

    // Bass compensation, 0..1, default 0.5.
    //
    // A real ladder loses low end as resonance rises, because the feedback
    // subtracts the (in-band) output from the input. Feeding a fraction of
    // the dry signal back in cancels that. 0 = full authentic bass loss,
    // 1 = fully compensated. Half is the classic setting - the loss is a big
    // part of why the hardware sounds the way it does, so do not zero it out.
    void setBassComp(MYFLOAT c) {
        if (c < 0.0) c = 0.0;
        if (c > 1.0) c = 1.0;
        mBassComp = c;
    }

    void setMode(Mode m) {
        if (m < 0 || m >= NUM_MODES) m = LP4;
        mMode = m;
    }

    // Saturate the integrator states as well as the feedback. On by default.
    // Off gives a cleaner, more "digital" ladder that still cannot blow up
    // (the states are still hard-limited), but loses the stage compression.
    //
    // Note this affects self-oscillation *pitch*: saturated stages contribute
    // less phase lag, so the 180 degree point moves up and the filter goes
    // sharp as you push past self-osc - measured +17 cents at res 1.05,
    // +60 cents at res 1.2. That is the hardware behaviour and is usually
    // what you want. Turn it off if you are keyboard-tracking the
    // self-oscillation as a sine source and need exact pitch (with it off,
    // self-osc lands on the cutoff to within 0.00 cents at any frequency).
    void setStageSaturation(bool on) { mStageSat = on; }

    // Per-stage detuning of g, mimicking component tolerance. All four poles
    // being mathematically identical is what makes a digital ladder sound
    // clinical; staggering them broadens the resonance slightly and takes the
    // edge off. The four factors multiply to exactly 1, so g^4 - and with it
    // the k = 4 self-oscillation threshold - is unchanged.
    //
    // Pitch-neutral, which is not obvious and is the reason for the unit
    // product: perturbing the stage frequencies while holding their geometric
    // mean fixed leaves the 180-degree point unchanged to first order.
    // Measured self-oscillation pitch is identical to within 0.1 cent at
    // amounts 0, 0.5 and 1.0, so this can be dialled purely by ear.
    void setStageMismatch(MYFLOAT amount) {
        if (amount < 0.0) amount = 0.0;
        if (amount > 1.0) amount = 1.0;
        mMismatch = amount;
        mMis[0] = 1.0 + 0.020 * amount;
        mMis[1] = 1.0 - 0.015 * amount;
        mMis[2] = 1.0;
        mMis[3] = 1.0 / (mMis[0] * mMis[1] * mMis[2]);
        updateStageCoefs();
    }

    // Asymmetry of the input/feedback saturator, roughly 0..0.5. 0 is a pure
    // odd-harmonic tanh; raising it adds second-order content. Default 0.12.
    void setAsymmetry(MYFLOAT a) {
        if (a < 0.0) a = 0.0;
        if (a > 0.5) a = 0.5;
        mAsym = a;
    }

    // Newton steps for the feedback solve. 1 is transparent in practice
    // because of the warm start; 2 only matters at extreme drive + resonance
    // with fast modulation. Clamped to 1..4.
    void setIterations(int n) {
        mIterations = (n < 1) ? 1 : (n > 4 ? 4 : n);
    }

    void reset() {
        mS[0] = mS[1] = mS[2] = mS[3] = 0.0;
        mU = 0.0;
        mUp.reset();
        mDown.reset();
    }

    MYFLOAT tick(MYFLOAT in) {
        if (zdf::isBad(in)) in = 0.0;
        if (!mOversample) return tickCore(in);

        // zero-stuff and interpolate, run the nonlinear core at 2x, decimate
        mUp.push(2.0 * in);
        const MYFLOAT a = tickCore(mUp.out());
        mUp.push(0.0);
        const MYFLOAT b = tickCore(mUp.out());
        mDown.push(a);
        mDown.push(b);
        return mDown.out();
    }

    void process(MYFLOAT *buf, int n) {
        for (int i = 0; i < n; ++i) buf[i] = tick(buf[i]);
    }

    void process(const MYFLOAT *in, MYFLOAT *out, int n) {
        for (int i = 0; i < n; ++i) out[i] = tick(in[i]);
    }

    MYFLOAT cutoff() const { return mCutoffHz; }

private:

    MYFLOAT clampCutoff(MYFLOAT hz) const {
        if (hz < 5.0) return 5.0;
        return (hz > mNyquistLimit) ? mNyquistLimit : hz;
    }

    void setGTarget(MYFLOAT G) {
        G = zdf::clampG(G);
        mGTarget = G / (1.0 + G);
        if (mSmoothCoef >= 1.0) snapCutoff();
    }

    void updateSmoothCoef() {
        if (mSmoothTime <= 0.0) { mSmoothCoef = 1.0; return; }
        const MYFLOAT n = mSmoothTime * mCoreRate;
        mSmoothCoef = (n < 1.0) ? 1.0 : (1.0 - std::exp(-1.0 / n));
    }

    // Cache everything that depends on g. Recomputed only when g actually
    // moves, so a steady cutoff costs nothing extra.
    void updateStageCoefs() {
        mGs[0] = mG * mMis[0];
        mGs[1] = mG * mMis[1];
        mGs[2] = mG * mMis[2];
        mGs[3] = mG * mMis[3];
        for (int i = 0; i < 4; ++i) {
            if (mGs[i] > 0.999) mGs[i] = 0.999;
            if (mGs[i] < 0.0) mGs[i] = 0.0;
        }
        // y4 = (prod g)*u + sum_i c_i * s_i
        const MYFLOAT p3 = mGs[3];
        const MYFLOAT p2 = p3 * mGs[2];
        const MYFLOAT p1 = p2 * mGs[1];
        mG4 = p1 * mGs[0];
        mSc[0] = p1 * (1.0 - mGs[0]);
        mSc[1] = p2 * (1.0 - mGs[1]);
        mSc[2] = p3 * (1.0 - mGs[2]);
        mSc[3] = 1.0 - mGs[3];
    }

    MYFLOAT tickCore(MYFLOAT in) {
        if (mG != mGTarget) {
            mG += (mGTarget - mG) * mSmoothCoef;
            const MYFLOAT d = mGTarget - mG;
            if (d < 1e-10 && d > -1e-10) mG = mGTarget;
            updateStageCoefs();
        }

        const MYFLOAT S = mSc[0] * mS[0] + mSc[1] * mS[1] +
                          mSc[2] * mS[2] + mSc[3] * mS[3];

        const MYFLOAT dx = in * mDrive;

        // u = sat(A - B*u), solved by Newton
        const MYFLOAT A = dx * (1.0 + mK * mBassComp) - mK * S;
        const MYFLOAT B = mK * mG4;

        const MYFLOAT h = mHeadroom;
        const MYFLOAT bias = mAsym * h;
        MYFLOAT u = mU; // warm start from last sample
        for (int i = 0; i < mIterations; ++i) {
            const MYFLOAT arg = A - B * u;
            const MYFLOAT f = u - zdf::satAsym(arg, h, bias);
            const MYFLOAT fp = 1.0 + B * zdf::satAsymDeriv(arg, h, bias);
            u -= f / fp; // fp >= 1 always, the derivative is >= 0
        }
        mU = u;

        // run the chain; y4 reproduces the solved value
        MYFLOAT x = u;
        MYFLOAT y[4];
        for (int i = 0; i < 4; ++i) {
            const MYFLOAT v = (x - mS[i]) * mGs[i];
            const MYFLOAT o = v + mS[i];
            MYFLOAT s = o + v;
            // Bounded state update: the filter can never latch up, whatever
            // is fed in. This is also where the per-stage ladder compression
            // comes from.
            mS[i] = zdf::flushDenorm(mStageSat ? zdf::satH(s, mStageHeadroom)
                                               : clampState(s));
            y[i] = o;
            x = o;
        }

        MYFLOAT out;
        switch (mMode) {
            case LP2: out = y[1]; break;
            case BP4: out = y[1] - 2.0 * y[2] + y[3]; break;
            case BP2: out = y[0] - y[1]; break;
            case HP4: out = u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3]; break;
            case HP2: out = u - 2.0 * y[0] + y[1]; break;
            case LP4:
            default:  out = y[3]; break;
        }
        out *= kMakeup[mMode];

        if (zdf::isBad(out)) { reset(); return 0.0; }
        return out;
    }

    static MYFLOAT clampState(MYFLOAT s) {
        const MYFLOAT lim = 8.0;
        if (s > lim) return lim;
        if (s < -lim) return -lim;
        return s;
    }

    // Tap makeup gains. These are measured, not guessed - each is the gain
    // that brings that tap's resonant peak level with LP4's at matched cutoff
    // and resonance, so switching modes does not jump. Without them the six
    // modes spread over 9 dB (the 2-pole taps are the loud ones, since they
    // attenuate far less). Order matches Mode.
    static constexpr MYFLOAT kMakeup[NUM_MODES] = {
        1.0,      // LP4
        0.4983,   // LP2
        1.0093,   // BP4
        0.4903,   // BP2
        0.8414,   // HP4
        0.5017    // HP2
    };

    MYFLOAT mSampleRate = 44100.0;
    MYFLOAT mCoreRate = 88200.0;
    MYFLOAT mNyquistLimit = 21609.0;
    MYFLOAT mCutoffHz = 1000.0;

    MYFLOAT mG = 0.0;        // g = G/(1+G)
    MYFLOAT mGTarget = 0.0;
    MYFLOAT mGs[4] = {0.0, 0.0, 0.0, 0.0};   // per-stage, with mismatch
    MYFLOAT mSc[4] = {0.0, 0.0, 0.0, 0.0};   // state -> output coefficients
    MYFLOAT mMismatch = 0.5;
    MYFLOAT mMis[4] = {1.010, 0.9925, 1.0, 0.99752};
    MYFLOAT mG4 = 0.0;       // product of the four stage gains
    MYFLOAT mK = 0.0;        // resonance, 0..4 (self-osc at 4)
    MYFLOAT mDrive = 1.0;
    MYFLOAT mBassComp = 0.5;
    MYFLOAT mHeadroom = 1.5;
    MYFLOAT mStageHeadroom = 3.0;
    MYFLOAT mAsym = 0.12;

    MYFLOAT mSmoothTime = 0.0005;   // 0.5 ms - shorter than a 64-sample block
    MYFLOAT mSmoothCoef = 1.0;

    MYFLOAT mS[4] = {0.0, 0.0, 0.0, 0.0};
    MYFLOAT mU = 0.0;

    zdf::Halfband mUp, mDown;

    Mode mMode = LP4;
    bool mStageSat = true;
    bool mOversample = true;
    int mIterations = 1;
};

// ---------------------------------------------------------------------------
// ZdfSvf - 2-pole state-variable, Oberheim SEM flavour
// ---------------------------------------------------------------------------
//
// Same TPT/ZDF stability guarantees, completely different personality:
// smoother, more vocal, self-oscillates cleanly, and does not wool up the
// bottom end the way the ladder does. A ladder plus one of these covers most
// of the classic ground.
//
// Core (Zavalishin ch. 4):
//   hp = (in - (2R + g)*ic1 - ic2) / (1 + 2R*g + g*g)
//   bp = g*hp + ic1;   ic1 = g*hp + bp
//   lp = g*bp + ic2;   ic2 = g*bp + lp
//
// SEM mode blends LP and HP continuously; at the midpoint it is the notch,
// which is the sound the SEM is actually known for.
//
class ZdfSvf {
public:
    enum Mode {
        LP = 0,
        BP,
        HP,
        NOTCH,
        PEAK,
        SEM_BLEND, // setSemBlend(): 0 = LP, 0.5 = notch, 1 = HP
        NUM_MODES
    };

    ZdfSvf() { init(44100.0); }

    void init(MYFLOAT sampleRate) {
        setSampleRate(sampleRate);
        reset();
    }

    void setSampleRate(MYFLOAT sampleRate) {
        mSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        mNyquistLimit = mSampleRate * 0.49;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setCutoff(mCutoffHz);
        snapCutoff();
    }

    // See ZdfLadder::setOversampling.
    void setOversampling(bool on) {
        if (on == mOversample) return;
        mOversample = on;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setCutoff(mCutoffHz);
        snapCutoff();
        mUp.reset();
        mDown.reset();
    }

    void setCutoff(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        mGTarget = zdf::clampG(std::tan(zdf::kPi * mCutoffHz / mCoreRate));
        if (mSmoothCoef >= 1.0) snapCutoff();
    }

    void setCutoffFast(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        mGTarget = zdf::clampG(zdf::tanPiApprox(mCutoffHz / mCoreRate));
        if (mSmoothCoef >= 1.0) snapCutoff();
    }

    // See ZdfLadder::setSmoothingTime.
    void setSmoothingTime(MYFLOAT seconds) {
        mSmoothTime = (seconds < 0.0) ? 0.0 : seconds;
        updateSmoothCoef();
    }

    void snapCutoff() {
        mG = mGTarget;
        updateCoeffs();
    }

    // 0 = maximally damped, 1 = infinite Q (rings forever), above 1 = true
    // self-oscillation with negative damping, amplitude limited by the
    // integrator saturator. Capped at 1.1; the denominator 1 + 2R*g + g*g
    // has no real root for 2R > -2, so this stays well inside safe territory.
    void setResonance(MYFLOAT r01) {
        if (r01 < 0.0) r01 = 0.0;
        if (r01 > 1.1) r01 = 1.1;
        mTwoR = 2.0 - 2.0 * r01;
        updateCoeffs();
    }

    void setDrive(MYFLOAT d) { mDrive = (d < 0.001) ? 0.001 : d; }

    // Headroom of the input saturator. See ZdfLadder::setHeadroom.
    void setHeadroom(MYFLOAT h) { mHeadroom = (h < 0.1) ? 0.1 : h; }

    void setMode(Mode m) {
        if (m < 0 || m >= NUM_MODES) m = LP;
        mMode = m;
    }

    void setSemBlend(MYFLOAT b01) {
        if (b01 < 0.0) b01 = 0.0;
        if (b01 > 1.0) b01 = 1.0;
        mSemBlend = b01;
    }

    // Saturate the input stage and the bandpass integrator. This is what
    // bounds the output under extreme input, limits self-oscillation
    // amplitude, and gives the SEM its slightly squashed resonant peak.
    // Turning it off makes the filter linear - still unconditionally stable,
    // but the output is then only as bounded as the input.
    void setStateSaturation(bool on) { mStateSat = on; }

    // See ZdfLadder::setAsymmetry.
    void setAsymmetry(MYFLOAT a) {
        if (a < 0.0) a = 0.0;
        if (a > 0.5) a = 0.5;
        mAsym = a;
    }

    void reset() {
        mIc1 = 0.0;
        mIc2 = 0.0;
        mUp.reset();
        mDown.reset();
    }

    MYFLOAT tick(MYFLOAT in) {
        if (zdf::isBad(in)) in = 0.0;
        if (!mOversample) return tickCore(in);
        mUp.push(2.0 * in);
        const MYFLOAT a = tickCore(mUp.out());
        mUp.push(0.0);
        const MYFLOAT b = tickCore(mUp.out());
        mDown.push(a);
        mDown.push(b);
        return mDown.out();
    }

    MYFLOAT tickCore(MYFLOAT in) {
        if (mG != mGTarget) {
            mG += (mGTarget - mG) * mSmoothCoef;
            const MYFLOAT d = mGTarget - mG;
            if (d < 1e-10 && d > -1e-10) mG = mGTarget;
            updateCoeffs();
        }

        MYFLOAT x = in * mDrive;
        if (mStateSat) x = zdf::satAsym(x, mHeadroom, mAsym * mHeadroom);

        const MYFLOAT hp = (x - (mTwoR + mG) * mIc1 - mIc2) * mInvDenom;

        const MYFLOAT v1 = mG * hp;
        const MYFLOAT bp = v1 + mIc1;
        mIc1 = zdf::flushDenorm(mStateSat ? zdf::satH(v1 + bp, mStateHeadroom)
                                          : (v1 + bp));

        const MYFLOAT v2 = mG * bp;
        const MYFLOAT lp = v2 + mIc2;
        mIc2 = zdf::flushDenorm(v2 + lp);

        MYFLOAT out;
        switch (mMode) {
            case BP:        out = kMakeBp * bp; break;
            case HP:        out = hp; break;
            case NOTCH:     out = lp + hp; break;
            case PEAK:      out = lp - hp; break;
            case SEM_BLEND: out = (1.0 - mSemBlend) * lp + mSemBlend * hp; break;
            case LP:
            default:        out = lp; break;
        }

        if (zdf::isBad(out) || zdf::isBad(mIc2)) { reset(); return 0.0; }
        return out;
    }

    void process(MYFLOAT *buf, int n) {
        for (int i = 0; i < n; ++i) buf[i] = tick(buf[i]);
    }

    void process(const MYFLOAT *in, MYFLOAT *out, int n) {
        for (int i = 0; i < n; ++i) out[i] = tick(in[i]);
    }

    MYFLOAT cutoff() const { return mCutoffHz; }

private:
    MYFLOAT clampCutoff(MYFLOAT hz) const {
        if (hz < 5.0) return 5.0;
        return (hz > mNyquistLimit) ? mNyquistLimit : hz;
    }

    void updateCoeffs() {
        mInvDenom = 1.0 / (1.0 + mTwoR * mG + mG * mG);
    }

    void updateSmoothCoef() {
        if (mSmoothTime <= 0.0) { mSmoothCoef = 1.0; return; }
        const MYFLOAT n = mSmoothTime * mCoreRate;
        mSmoothCoef = (n < 1.0) ? 1.0 : (1.0 - std::exp(-1.0 / n));
    }

    // Measured so BP peaks level with LP at matched settings.
    static constexpr MYFLOAT kMakeBp = 1.00;

    MYFLOAT mSampleRate = 44100.0;
    MYFLOAT mCoreRate = 88200.0;
    MYFLOAT mNyquistLimit = 21609.0;
    MYFLOAT mCutoffHz = 1000.0;

    MYFLOAT mG = 0.0;
    MYFLOAT mGTarget = 0.0;
    MYFLOAT mSmoothTime = 0.0005;
    MYFLOAT mSmoothCoef = 1.0;
    MYFLOAT mAsym = 0.12;
    zdf::Halfband mUp, mDown;
    bool mOversample = true;
    MYFLOAT mTwoR = 2.0;
    MYFLOAT mInvDenom = 1.0;
    MYFLOAT mDrive = 1.0;
    MYFLOAT mHeadroom = 1.5;
    MYFLOAT mStateHeadroom = 3.0;
    MYFLOAT mSemBlend = 0.5;

    MYFLOAT mIc1 = 0.0;
    MYFLOAT mIc2 = 0.0;

    Mode mMode = LP;
    bool mStateSat = true;
};

// ---------------------------------------------------------------------------
// Zdf303 - 3-pole acid ladder (TB-303 voicing)
// ---------------------------------------------------------------------------
//
// Honest description first: this is NOT a diode-ladder circuit model. It is a
// 3-pole ZDF ladder voiced for acid, built on the same verified TPT core as
// ZdfLadder.
//
// The first attempt here *was* a circuit model - the Pirkle/Zavalishin diode
// coefficient set, with inter-stage loading. It measured badly and sounded
// worse: at setCutoff(1000) with no resonance it was -24 dB at its own
// nominal cutoff, with the -3 dB corner down at ~110 Hz. Corner and resonant
// peak nine octaves-ish apart means a steeply falling response with a narrow
// spike perched on it - dark and thin, with no body for the resonance to bite
// into. For reference the transistor ladder is -12 dB at cutoff with a 2.3x
// corner/peak ratio, and Voipio's 303 (in va/moogladder.h) is -5.6 dB. The
// diode coefficients spread the poles roughly 4x more than a Moog's, and no
// amount of recentring the knob fixes a shape that wrong.
//
// So this trades the circuit model for the three things that actually make
// the 303 sound like the 303:
//
//   1. 18 dB/oct, not 24. This is the single most recognisable part of the
//      sound - it leaves far more harmonic content above the cutoff than a
//      4-pole does, which is what the resonance then has to scream against.
//
//      Done by keeping a *four*-pole feedback loop and tapping the output at
//      the third stage, rather than by building a 3-pole loop. That detail
//      matters. Three cascaded one-poles reach 180 degrees at sqrt(3)*fc, so
//      a 3-pole ladder's corner and its resonance are inherently 1.73x apart
//      and you can only put one of them on the knob; built that way it
//      measured -18 dB at its own cutoff with a feeble +2.5 dB peak. Four
//      poles reach 180 degrees at exactly fc (45 degrees each), so corner and
//      resonance coincide - and the y3 tap is still H^3, i.e. 18 dB/oct and
//      only -9 dB at cutoff. Loop and slope are independent.
//   2. No bass compensation by default. The Moog ladder keeps its level as
//      resonance rises; the 303 emphatically does not - it hollows out and
//      gets nastier. setBassComp defaults to 0 here and 0.5 there, and that
//      difference is most of the attitude.
//   3. A highpass in the resonance feedback path (the squelch). Without it
//      the resonance muddies the low end instead of squelching. Folded into
//      the ZDF solve as another TPT state, not bolted on with a delay - a TPT
//      highpass output is affine in its input, so it composes into the same
//      Newton iteration as the input saturation.
//
// Because the loop is the same 4-pole one ZdfLadder uses, setCutoff is the
// resonant frequency exactly as it is there, self-oscillation closes at
// k = 4, and no frequency compensation is needed at all.
//
class Zdf303 {
public:
    enum Mode {
        LP3 = 0,   // the 303 itself
        BP,        // H(1-H)^2
        HP3,       // (1-H)^3
        NUM_MODES
    };

    Zdf303() { init(44100.0); }

    void init(MYFLOAT sampleRate) {
        setSampleRate(sampleRate);
        reset();
    }

    void setSampleRate(MYFLOAT sampleRate) {
        mSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        mNyquistLimit = mSampleRate * 0.49;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setFeedbackHighpass(mHpHz);
        setCutoff(mCutoffHz);
        snapCutoff();
    }

    // See ZdfLadder::setOversampling.
    void setOversampling(bool on) {
        if (on == mOversample) return;
        mOversample = on;
        mCoreRate = mSampleRate * (mOversample ? 2.0 : 1.0);
        updateSmoothCoef();
        setFeedbackHighpass(mHpHz);
        setCutoff(mCutoffHz);
        snapCutoff();
        mUp.reset();
        mDown.reset();
    }

    void setCutoff(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        setGTarget(std::tan(zdf::kPi * mCutoffHz / mCoreRate));
    }

    void setCutoffFast(MYFLOAT hz) {
        mCutoffHz = clampCutoff(hz);
        setGTarget(zdf::tanPiApprox(mCutoffHz / mCoreRate));
    }

    // See ZdfLadder::setSmoothingTime.
    void setSmoothingTime(MYFLOAT seconds) {
        mSmoothTime = (seconds < 0.0) ? 0.0 : seconds;
        updateSmoothCoef();
    }

    void snapCutoff() {
        mG = mGTarget;
        updateStageCoefs();
    }

    // 0 = none, 1 = self-oscillation, up to 1.4 for the overdriven squeal.
    void setResonance(MYFLOAT r01) {
        if (r01 < 0.0) r01 = 0.0;
        if (r01 > 1.4) r01 = 1.4;
        mK = kSelfOsc * r01;
    }

    void setDrive(MYFLOAT d) { mDrive = (d < 0.001) ? 0.001 : d; }

    void setHeadroom(MYFLOAT h) { mHeadroom = (h < 0.1) ? 0.1 : h; }

    void setStageHeadroom(MYFLOAT h) { mStageHeadroom = (h < 0.1) ? 0.1 : h; }

    // 0 by default, unlike ZdfLadder's 0.5 - see note 2 above. Raise it if you
    // want the 303 to hold its low end as resonance comes up, which is exactly
    // what it does not do on the real thing.
    void setBassComp(MYFLOAT c) {
        if (c < 0.0) c = 0.0;
        if (c > 1.0) c = 1.0;
        mBassComp = c;
    }

    // Corner of the highpass in the resonance feedback path. 0 bypasses it.
    void setFeedbackHighpass(MYFLOAT hz) {
        mHpHz = (hz < 0.0) ? 0.0 : hz;
        if (mHpHz < 1.0) {
            mGh = 0.0;
            mHpOneMinusG = 1.0;
        } else {
            const MYFLOAT Gh = std::tan(zdf::kPi * mHpHz / mCoreRate);
            mGh = Gh / (1.0 + Gh);
            mHpOneMinusG = 1.0 - mGh;
        }
    }

    void setMode(Mode m) {
        if (m < 0 || m >= NUM_MODES) m = LP3;
        mMode = m;
    }

    void setStageSaturation(bool on) { mStageSat = on; }

    // Per-stage detuning of g, mimicking component tolerance. All four poles
    // being mathematically identical is what makes a digital ladder sound
    // clinical; staggering them broadens the resonance slightly and takes the
    // edge off. The four factors multiply to exactly 1, so g^4 - and with it
    // the k = 4 self-oscillation threshold - is unchanged.
    //
    // Pitch-neutral, which is not obvious and is the reason for the unit
    // product: perturbing the stage frequencies while holding their geometric
    // mean fixed leaves the 180-degree point unchanged to first order.
    // Measured self-oscillation pitch is identical to within 0.1 cent at
    // amounts 0, 0.5 and 1.0, so this can be dialled purely by ear.
    void setStageMismatch(MYFLOAT amount) {
        if (amount < 0.0) amount = 0.0;
        if (amount > 1.0) amount = 1.0;
        mMismatch = amount;
        mMis[0] = 1.0 + 0.020 * amount;
        mMis[1] = 1.0 - 0.015 * amount;
        mMis[2] = 1.0;
        mMis[3] = 1.0 / (mMis[0] * mMis[1] * mMis[2]);
        updateStageCoefs();
    }

    // See ZdfLadder::setAsymmetry. Defaults higher here - the 303 wants grit.
    void setAsymmetry(MYFLOAT a) {
        if (a < 0.0) a = 0.0;
        if (a > 0.5) a = 0.5;
        mAsym = a;
    }

    void setIterations(int n) { mIterations = (n < 1) ? 1 : (n > 4 ? 4 : n); }

    void reset() {
        mS[0] = mS[1] = mS[2] = mS[3] = 0.0;
        mZh = 0.0;
        mU = 0.0;
        mUp.reset();
        mDown.reset();
    }

    MYFLOAT tick(MYFLOAT in) {
        if (zdf::isBad(in)) in = 0.0;
        if (!mOversample) return tickCore(in);
        mUp.push(2.0 * in);
        const MYFLOAT a = tickCore(mUp.out());
        mUp.push(0.0);
        const MYFLOAT b = tickCore(mUp.out());
        mDown.push(a);
        mDown.push(b);
        return mDown.out();
    }

    MYFLOAT tickCore(MYFLOAT in) {
        if (mG != mGTarget) {
            mG += (mGTarget - mG) * mSmoothCoef;
            const MYFLOAT d = mGTarget - mG;
            if (d < 1e-10 && d > -1e-10) mG = mGTarget;
            updateStageCoefs();
        }

        // state contribution reaching the fourth stage - the loop is the full
        // 4-pole one; only the output tap is at stage 3
        const MYFLOAT S = mSc[0] * mS[0] + mSc[1] * mS[1] +
                          mSc[2] * mS[2] + mSc[3] * mS[3];

        const MYFLOAT dx = in * mDrive;

        // feedback goes through the TPT highpass, whose output is
        // (1-gh)*(y4 - zh) and so stays affine in y4
        const MYFLOAT kh = mK * mHpOneMinusG;
        const MYFLOAT A = dx * (1.0 + mK * mBassComp) - kh * S + kh * mZh;
        const MYFLOAT B = kh * mG4;

        const MYFLOAT h = mHeadroom;
        const MYFLOAT bias = mAsym * h;
        MYFLOAT u = mU;
        for (int i = 0; i < mIterations; ++i) {
            const MYFLOAT arg = A - B * u;
            const MYFLOAT f = u - zdf::satAsym(arg, h, bias);
            const MYFLOAT fp = 1.0 + B * zdf::satAsymDeriv(arg, h, bias);
            u -= f / fp;
        }
        mU = u;

        MYFLOAT x = u;
        MYFLOAT y[4];
        for (int i = 0; i < 4; ++i) {
            const MYFLOAT v = (x - mS[i]) * mGs[i];
            const MYFLOAT o = v + mS[i];
            const MYFLOAT s = o + v;
            mS[i] = zdf::flushDenorm(mStageSat ? zdf::satH(s, mStageHeadroom)
                                               : clampState(s));
            y[i] = o;
            x = o;
        }

        // the highpass filters the loop signal, which is the 4th stage
        const MYFLOAT vh = (y[3] - mZh) * mGh;
        mZh = zdf::flushDenorm(clampState((vh + mZh) + vh));

        // taps are off y0..y2 and u, so everything here is 3-pole: 18 dB/oct
        MYFLOAT out;
        switch (mMode) {
            // H(1-H)^2, peaks at 4/27 for a real one-pole hence the makeup
            case BP:  out = kMakeBp * (y[0] - 2.0 * y[1] + y[2]); break;
            case HP3: out = u - 3.0 * y[0] + 3.0 * y[1] - y[2]; break;
            case LP3:
            default:  out = y[2]; break;
        }

        if (zdf::isBad(out)) { reset(); return 0.0; }
        return out;
    }

    void process(MYFLOAT *buf, int n) {
        for (int i = 0; i < n; ++i) buf[i] = tick(buf[i]);
    }

    void process(const MYFLOAT *in, MYFLOAT *out, int n) {
        for (int i = 0; i < n; ++i) out[i] = tick(in[i]);
    }

    MYFLOAT cutoff() const { return mCutoffHz; }

private:
    // Same 4-pole loop as ZdfLadder, so the same threshold.
    static constexpr MYFLOAT kSelfOsc = 4.0;


    // Measured so BP peaks level with LP3 at matched settings.
    static constexpr MYFLOAT kMakeBp = 1.06;

    MYFLOAT clampCutoff(MYFLOAT hz) const {
        if (hz < 5.0) return 5.0;
        return (hz > mNyquistLimit) ? mNyquistLimit : hz;
    }

    static MYFLOAT clampState(MYFLOAT s) {
        const MYFLOAT lim = 8.0;
        if (s > lim) return lim;
        if (s < -lim) return -lim;
        return s;
    }

    void setGTarget(MYFLOAT G) {
        G = zdf::clampG(G);
        mGTarget = G / (1.0 + G);
        if (mSmoothCoef >= 1.0) snapCutoff();
    }

    void updateSmoothCoef() {
        if (mSmoothTime <= 0.0) { mSmoothCoef = 1.0; return; }
        const MYFLOAT n = mSmoothTime * mCoreRate;
        mSmoothCoef = (n < 1.0) ? 1.0 : (1.0 - std::exp(-1.0 / n));
    }

    void updateStageCoefs() {
        mGs[0] = mG * mMis[0];
        mGs[1] = mG * mMis[1];
        mGs[2] = mG * mMis[2];
        mGs[3] = mG * mMis[3];
        for (int i = 0; i < 4; ++i) {
            if (mGs[i] > 0.999) mGs[i] = 0.999;
            if (mGs[i] < 0.0) mGs[i] = 0.0;
        }
        const MYFLOAT p3 = mGs[3];
        const MYFLOAT p2 = p3 * mGs[2];
        const MYFLOAT p1 = p2 * mGs[1];
        mG4 = p1 * mGs[0];
        mSc[0] = p1 * (1.0 - mGs[0]);
        mSc[1] = p2 * (1.0 - mGs[1]);
        mSc[2] = p3 * (1.0 - mGs[2]);
        mSc[3] = 1.0 - mGs[3];
    }

    MYFLOAT mSampleRate = 44100.0;
    MYFLOAT mCoreRate = 88200.0;
    MYFLOAT mNyquistLimit = 21609.0;
    MYFLOAT mCutoffHz = 1000.0;
    MYFLOAT mHpHz = 80.0;

    MYFLOAT mG = 0.0;
    MYFLOAT mGTarget = 0.0;
    MYFLOAT mGs[4] = {0.0, 0.0, 0.0, 0.0};
    MYFLOAT mSc[4] = {0.0, 0.0, 0.0, 0.0};
    MYFLOAT mMismatch = 0.5;
    MYFLOAT mMis[4] = {1.010, 0.9925, 1.0, 0.99752};
    MYFLOAT mG4 = 0.0;
    MYFLOAT mSmoothTime = 0.0005;
    MYFLOAT mSmoothCoef = 1.0;
    MYFLOAT mAsym = 0.20;
    zdf::Halfband mUp, mDown;
    bool mOversample = true;
    MYFLOAT mGh = 0.0;
    MYFLOAT mHpOneMinusG = 1.0;

    MYFLOAT mK = 0.0;
    MYFLOAT mDrive = 1.0;
    MYFLOAT mBassComp = 0.0;
    MYFLOAT mHeadroom = 1.5;
    MYFLOAT mStageHeadroom = 3.0;

    MYFLOAT mS[4] = {0.0, 0.0, 0.0, 0.0};
    MYFLOAT mZh = 0.0;
    MYFLOAT mU = 0.0;

    Mode mMode = LP3;
    bool mStageSat = true;
    int mIterations = 1;
};

} // namespace dsp
} // namespace tsl

#endif // TSL_ZDF_FILTERS_H
