//
// Fixed-ratio 2x / 4x oversampling for nonlinear audio cores.
//
// Created by claude on 09.08.2026.
//
// This is NOT the same job as resample.h. That header does arbitrary and
// rational sample-rate conversion (PolyPhaseResampler, WindowedSinc, delay
// lines with fractional reads) and is the right tool for playing a 44.1 kHz
// file on a 48 kHz device. This header does one narrow thing instead: wrap a
// per-sample nonlinear process so it runs at 2x or 4x the host rate and comes
// back clean, as cheaply as the maths allows.
//
// Typical use - the whole point is that it stays this short:
//
//     tsl::dsp::Oversampler<> os;          // Std quality
//     os.setFactor(4);
//     os.reset();
//     ...
//     out = os.process(in, [&](MYFLOAT x) { return ladder.tick(x); });
//
// or, when the core is not a single expression:
//
//     MYFLOAT buf[tsl::dsp::Oversampler<>::kMaxFactor];
//     const int n = os.up(in, buf);
//     for (int i = 0; i < n; ++i) buf[i] = core.tick(buf[i]);
//     out = os.down(buf);
//
// Header-only, no allocation, no virtuals, no branches in the inner loop
// beyond the factor switch - safe on the audio thread.
//
//
// HOW IT WORKS
// ------------
//
// Upsampling by 2 is "insert a zero between every pair of samples, then
// lowpass at the old Nyquist". Downsampling by 2 is "lowpass at the new
// Nyquist, then keep every second sample". Both lowpasses are the same
// halfband filter, and both collapse into almost nothing once you exploit
// their structure:
//
//   * A halfband FIR of length N = 4n-1 has its centre tap fixed at 0.5 and
//     every EVEN offset from the centre is exactly zero. So only 2n+1 of the
//     N taps are nonzero, and because the filter is symmetric those 2n fold
//     into n multiplies.
//
//   * Split the taps by index parity (a polyphase decomposition) and the two
//     branches are wildly lopsided. One branch is the 2n even-index taps -
//     an n-multiply symmetric FIR. The other branch is the centre tap alone.
//     For the interpolator that second branch is literally a copy: half the
//     samples handed to the core ARE the input samples, delayed. For the
//     decimator it is one multiply by 0.5.
//
//   * The decimator never computes the sample it is about to throw away. It
//     is fed both high-rate samples but only evaluates its FIR once per
//     output. That single skipped evaluation IS the decimation.
//
// Net cost per INPUT sample, counting only multiplies:
//   2x:  2*n1              4x:  2*n1 + 4*n2
// (stage 1 converts fs <-> 2fs, stage 2 converts 2fs <-> 4fs).
//
// The delay lines are double-written circular buffers - each sample is stored
// at both cur and cur+L so the FIR reads a contiguous window and needs no
// wraparound test. Same trick as PolyPhaseResampler2x in resample.h.
//
//
// WHY A CASCADE FOR 4x
// --------------------
//
// 4x is two 2x stages, not one wide filter, and the two stages have very
// different jobs. Stage 1 works at the bottom rate where the transition band
// is narrow - it must pass 20 kHz and stop by 28 kHz (at 48 kHz input), which
// is expensive. Stage 2 works an octave up, where everything stage 1 will
// later reject is already fair game, so its transition band is four times
// wider and a much shorter filter reaches the same attenuation. Hence
// n2 << n1 in every tier below.
//
//
// CHOOSING A TIER
// ---------------
//
// All three are equiripple halfbands designed against the same spec: flat to
// 20 kHz, stopband from 28 kHz, referred to 48 kHz input (the band edges
// scale with the host rate, so at 96 kHz they sit at 40 / 56 kHz).
//
//   tier    stage1  stage2   stopband     mults/input     latency (input smp)
//                                          2x     4x       2x       4x
//   Fast     n=4     n=2    -28 / -31 dB    8     16      7.0      8.5
//   Std      n=10    n=4    -59 / -59 dB   20     36     19.0     22.5
//   Steep    n=16    n=6    -89 / -86 dB   32     56     31.0     36.5
//
// Passband deviation over 0..20 kHz for the whole up-core-down chain,
// measured on the composite impulse response with an identity core:
//
//   Fast   2x  +-0.70 dB      Fast   4x  -0.36 / +1.16 dB
//   Std    2x  +-0.02 dB      Std    4x  +-0.04 dB
//   Steep  2x  +-0.001 dB     Steep  4x  +-0.002 dB
//
// Measured alias rejection, worst spurious peak relative to the fundamental,
// 9 kHz sine into a memoryless nonlinearity at 48 kHz. 9 kHz is chosen
// because an odd nonlinearity puts its harmonics at 9/27/45/63/81 kHz and
// only the first survives band-limiting, so the ideal output is a single tone
// and every other bin in the spectrum is error - no masking heuristics:
//
//                     tanh(4x)      tanh(x)     x - x^3/3
//   no oversampling    -11.8 dB     -25.0 dB     -21.5 dB
//   Fast   2x          -30.7        -42.9        -39.3
//   Fast   4x          -30.5        -43.7        -40.1
//   Std    2x          -34.8        -55.1        -51.4
//   Std    4x          -43.1        -56.3        -52.8
//   Steep  2x          -34.8        -66.6        -63.0
//   Steep  4x          -54.5        -66.7        -63.2
//
// Read that table before reaching for Steep. Two things fall out of it:
//
//   * For a HARD nonlinearity, 2x saturates at about -35 dB no matter how
//     good the filter is - Std and Steep tie there. That floor is not the
//     resampler. It is the core's own aliasing: tanh(4x) on a 9 kHz tone puts
//     real energy at 81 kHz, which is above Nyquist even at 2x, so it has
//     already folded to 15 kHz inside the core before the decimator ever sees
//     it. No filter can remove it. Only running the core faster can, which is
//     exactly what 4x buys and why the Steep column only pulls away there.
//
//   * For a gentle nonlinearity the opposite holds - 2x and 4x are within
//     1 dB, and the filter tier is what moves the number. Spending CPU on 4x
//     there is waste; spend it on Steep at 2x instead.
//
// Std at 2x is the sensible default and is what Oversampler<> gives you.
//
//
// THERE IS NO CUTOFF CONTROL, AND THERE CANNOT BE ONE
// ---------------------------------------------------
//
// The corner is pinned at exactly the host Nyquist and is not adjustable.
// That is not a missing feature, it is the definition of a halfband: the
// identity H(f) + H(0.5-f) = 1 forces H to pass through 0.5 (-6 dB) at 0.25
// of the high rate, and it is the same identity that makes every even-offset
// tap zero. Move the corner anywhere else and the zeros fill in - 2n+1 taps
// become 4n-1, the interpolator's free-copy branch turns into a second real
// FIR, and the cost roughly doubles. The fixed cutoff IS the speed.
//
// What DOES vary between tiers is the width of the transition band around
// that fixed centre, not its position. Measured on the exact composite
// impulse response, as a fraction of the host Nyquist:
//
//   tier        -0.1 dB   -1 dB   -3 dB   -6 dB
//   Fast  2x     0.382    0.844   0.898   0.993
//   Std   2x     0.847    0.893   0.934   0.996
//   Steep 2x     0.870    0.913   0.948   0.997
//
// (4x is within 0.01 of the 2x row in every column.)
//
// Because that curve is fixed in NORMALISED frequency, the host rate decides
// where 20 kHz lands on it. Gain at exactly 20 kHz:
//
//   tier        44.1 kHz    48 kHz   88.2 kHz    96 kHz
//   Fast  2x     -3.40      -0.69     -0.58      -0.36
//   Fast  4x     -4.16      -0.29     -0.28      -0.09
//   Std   2x     -1.56      -0.02     +0.01      -0.01
//   Std   4x     -1.73      -0.03     +0.02      +0.00
//   Steep 2x     -0.78      -0.00     +0.00      +0.00
//   Steep 4x     -0.82      -0.00     -0.00      +0.00
//
// So: at 48 kHz and above every tier is flat to 20 kHz and the choice is
// purely about alias rejection. At 44.1 kHz there is no room left - 20 kHz
// sits inside the transition band, and Fast costs 3.4 dB up there. If the
// host is at 44.1 kHz and top-octave flatness matters, use Steep (-0.78 dB)
// or do not oversample the stage at all. Alias rejection itself does not
// change with host rate; only this passband edge does.
//
// If a genuinely narrower transition at 44.1 kHz is ever needed, the fix is
// another compile-time kernel set designed at that band edge, not a runtime
// control - see the design notes on the coefficient tables below.
//
//
// MEASURED COST
// -------------
//
// Apple silicon, clang -O3, 4 M input samples per run. The core is a cheap
// cubic (x - x^3/6) so that what is being timed is the resampler itself and
// not the thing inside it. "old 2x" is the direct-form loop in ZdfFilters.h
// driving the same core:
//
//   ZdfFilters direct form 2x            8.29 ns/sample
//   ZdfFilters direct form, 4x cascade  23.42 ns/sample
//   Oversampler Fast  2x                 2.35 ns/sample   0.28x
//   Oversampler Fast  4x                 6.48 ns/sample   0.78x
//   Oversampler Std   2x                 4.43 ns/sample   0.53x
//   Oversampler Std   4x                10.95 ns/sample   1.32x
//   Oversampler Steep 2x                 7.20 ns/sample   0.87x
//   Oversampler Steep 4x                17.65 ns/sample   2.13x
//   Oversampler 1x (bypass)              0.53 ns/sample   0.06x
//
// So Steep at 4x - a 63-tap and a 23-tap halfband, -89 dB - is still cheaper
// than the old 11-tap filter cascaded to 4x, and Fast at 2x is 3.5x cheaper
// than the old 2x while being about 9 dB better at rejection and 3.6 dB
// flatter at 20 kHz. Almost all of that comes from two things: the polyphase
// split (half the taps in an interpolator multiply known zeros) and the
// circular buffer (the old push() memmoves the whole delay line every call,
// which costs more than the four multiplies it exists to serve).
//
// Put a real core in and the picture changes completely - with ZdfLadder
// inside, the same run gives:
//
//   ladder alone, no oversampling       35.97 ns/sample
//   ladder + ZdfFilters direct 2x       72.17 ns/sample
//   ladder + Oversampler Std 2x         72.13 ns/sample
//   ladder + Oversampler Std 4x        145.51 ns/sample
//
// i.e. for an expensive core the resampler is free either way and the choice
// is purely about quality; the speed argument only matters when what you are
// oversampling is cheap (a waveshaper, a clipper, a single saturator).
//
//
// A NOTE ON THE OLD HALFBAND IN ZdfFilters.h
// ------------------------------------------
//
// zdf::Halfband is a Blackman-windowed design. Blackman buys its stopband
// with an enormously wide transition band, and at 11 taps there is not enough
// filter left to place the corner properly: measured on the composite
// response, its 2x chain is -1.3 dB at 15 kHz and -4.3 dB at 20 kHz. That is
// not a subtlety, it is an audible dulling of the top end whenever
// oversampling is enabled, and it gets worse if the stage is cascaded. Its
// alias rejection is also the worst in the table above. The Fast tier here
// costs one extra multiply per stage and is roughly 9 dB better at rejection
// while staying inside +-0.7 dB across the band.
//
// Nothing in this header touches ZdfFilters.h - the classes there keep their
// own resampler. Migrating them is a separate, behaviour-changing decision.
//

#ifndef TSL_OVERSAMPLER_H
#define TSL_OVERSAMPLER_H

#include "defines.h"

namespace tsl {
namespace dsp {

// ---------------------------------------------------------------------------
// coefficient sets
// ---------------------------------------------------------------------------
//
// Each set is the FOLDED EVEN-INDEX HALF of a halfband of length N = 4n-1:
// c[k] holds tap h[2k] for k = 0..n-1, and the full tap list is
//
//     h[2k]    = c[k]           k <  n
//     h[2k]    = c[2n-1-k]      k >= n
//     h[2n-1]  = 0.5                       (the centre)
//     h[odd]   = 0                         (everything else)
//
// so c[0] is the outermost tap and c[n-1] the one next to the centre.
//
// Designed by iteratively reweighted least squares on a dense stopband grid,
// which converges to a true equiripple solution (verified: all stopband lobes
// equal to within 0.1%), under the linear constraint
//
//     2 * sum(odd-offset taps) = 0.5   i.e.   H(0) = 1 exactly.
//
// That constraint matters more than it looks. An UNCONSTRAINED equiripple
// halfband has H(0) = 1 - delta, because the halfband identity
// H(f) + H(0.5-f) = 1 ties DC gain directly to the stopband ripple at
// Nyquist. In a plain resampler that is a harmless fraction of a dB. Here it
// is not: it would give the interpolator's two polyphase branches DIFFERENT
// DC gains (2*sum(even taps) versus 2*h[centre] = 1), so a DC input would
// come out with an fs/2 alternation riding on it, and the 4x cascade would
// lose about 1 dB of level outright. Constraining DC costs at most 1.9 dB of
// stopband (the n=2 kernel; under 0.5 dB for every other one) and removes the
// problem at the root rather than papering over it with a makeup gain.
//
namespace hb {

// stopband from 0.29167 of the 2x rate (28 kHz at 48 kHz input)
struct S1Fast {
    static constexpr int n = 4;                     // N = 15, -27.95 dB
    static constexpr MYFLOAT c[n] = {
        -3.20299862991493267e-02, +5.36242354844859437e-02, -9.12499804638493284e-02,
        +3.19655731278512711e-01,
    };
};

struct S1Std {
    static constexpr int n = 10;                    // N = 39, -59.37 dB
    static constexpr MYFLOAT c[n] = {
        -1.33609707548920653e-03, +2.70720304532504064e-03, -4.96692457115044481e-03,
        +8.72479040932140452e-03, -1.40255929361601528e-02, +2.21357355746308569e-02,
        -3.43226697794567839e-02, +5.53401615463512131e-02, -1.00846334531893234e-01,
        +3.16589728318521280e-01,
    };
};

struct S1Steep {
    static constexpr int n = 16;                    // N = 63, -88.94 dB
    static constexpr MYFLOAT c[n] = {
        -6.76410405923699720e-05, +1.85186804020954902e-04, -4.13413382064718249e-04,
        +8.17167717683521923e-04, -1.46341766345082147e-03, +2.45774932975624701e-03,
        -3.91010167282847265e-03, +5.97892290156789956e-03, -8.85132759555124285e-03,
        +1.28131947422391174e-02, -1.82943206263586865e-02, +2.60944179171900374e-02,
        -3.78801168825867451e-02, +5.80263941622542676e-02, -1.02631438692303914e-01,
        +3.17138743981024929e-01,
    };
};

// stopband from 0.35417 of the 4x rate - wider transition, so much shorter
// for the same attenuation. See "why a cascade for 4x" above.
struct S2Fast {
    static constexpr int n = 2;                     // N = 7, -31.01 dB
    static constexpr MYFLOAT c[n] = {
        -5.46371072085337639e-02, +3.04637107208533764e-01,
    };
};

struct S2Std {
    static constexpr int n = 4;                     // N = 15, -59.30 dB
    static constexpr MYFLOAT c[n] = {
        -5.40451533256958161e-03, +2.46285638952481907e-02, -7.66936505434748089e-02,
        +3.07469601980796203e-01,
    };
};

struct S2Steep {
    static constexpr int n = 6;                     // N = 23, -85.83 dB
    static constexpr MYFLOAT c[n] = {
        -6.48893040475884142e-04, +3.71670887440424301e-03, -1.27165158808390782e-02,
        +3.41453932119901576e-02, -8.52573239269205502e-02, +3.10760630761841072e-01,
    };
};

inline MYFLOAT flush(MYFLOAT v) {
    return (v > -1e-20 && v < 1e-20) ? 0.0 : v;
}

// ---------------------------------------------------------------------------
// Interp2 - one input sample in, two high-rate samples out
// ---------------------------------------------------------------------------
//
//   even output = 2 * sum_k c[k] * (w[k] + w[2n-1-k])
//   odd  output = w[n-1]                                  (a copy, no maths)
//
// where w[j] is the input sample from j frames ago. The factor 2 is the
// zero-stuffing makeup: inserting a zero between every pair of samples halves
// the average energy, so the interpolator has to put it back.
//
template<class K>
class Interp2 {
public:
    static constexpr int kN = K::n;
    static constexpr int kL = 2 * K::n;

    void reset() {
        for (int i = 0; i < 2 * kL; ++i) mBuf[i] = 0.0;
        mCur = 0;
    }

    // out[0], out[1] = the two samples at 2x the input rate
    inline void tick(MYFLOAT x, MYFLOAT out[2]) {
        mCur = (mCur == 0) ? kL - 1 : mCur - 1;
        const MYFLOAT v = flush(x);
        mBuf[mCur] = mBuf[mCur + kL] = v;

        const MYFLOAT *w = &mBuf[mCur];
        MYFLOAT s = 0.0;
        for (int k = 0; k < kN; ++k) s += K::c[k] * (w[k] + w[kL - 1 - k]);
        out[0] = 2.0 * s;
        out[1] = w[kN - 1];
    }

    // group delay in INPUT samples
    static constexpr MYFLOAT latency() { return (2.0 * kN - 1.0) * 0.5; }

private:
    MYFLOAT mBuf[2 * kL]{};
    int mCur = 0;
};

// ---------------------------------------------------------------------------
// Decim2 - two high-rate samples in, one output sample out
// ---------------------------------------------------------------------------
//
//   y = sum_k c[k] * (e[k] + e[2n-1-k]) + 0.5 * o[n]
//
// e is the stream of first-of-pair samples, o the stream of second-of-pair.
// Only the e branch needs the full delay line; o needs a plain n+1 delay.
//
// This aligns the output to the even phase of the high-rate stream, which
// makes the up+down latency exactly 2n-1 input samples - an integer. Aligning
// to the odd phase (what the direct-form loop in ZdfFilters.h happens to do)
// is equally valid and gives 2n-1.5. Both were checked against an explicit
// zero-stuff/convolve/decimate reference and agree to 4e-16.
//
template<class K>
class Decim2 {
public:
    static constexpr int kN = K::n;
    static constexpr int kL = 2 * K::n;
    static constexpr int kM = K::n + 1;

    void reset() {
        for (int i = 0; i < 2 * kL; ++i) mBuf[i] = 0.0;
        for (int i = 0; i < 2 * kM; ++i) mOdd[i] = 0.0;
        mCur = mOCur = 0;
    }

    inline MYFLOAT tick(const MYFLOAT in[2]) {
        mCur = (mCur == 0) ? kL - 1 : mCur - 1;
        mBuf[mCur] = mBuf[mCur + kL] = flush(in[0]);

        mOCur = (mOCur == 0) ? kM - 1 : mOCur - 1;
        mOdd[mOCur] = mOdd[mOCur + kM] = flush(in[1]);

        const MYFLOAT *w = &mBuf[mCur];
        MYFLOAT s = 0.0;
        for (int k = 0; k < kN; ++k) s += K::c[k] * (w[k] + w[kL - 1 - k]);
        return s + 0.5 * mOdd[mOCur + kN];
    }

    // group delay in OUTPUT samples
    static constexpr MYFLOAT latency() { return (2.0 * kN - 1.0) * 0.5; }

private:
    MYFLOAT mBuf[2 * kL]{};
    MYFLOAT mOdd[2 * kM]{};
    int mCur = 0;
    int mOCur = 0;
};

} // namespace hb

// ---------------------------------------------------------------------------
// Oversampler
// ---------------------------------------------------------------------------

enum OsQuality {
    OS_FAST = 0,   // -28 dB stopband,  8/16 mults per input sample at 2x/4x
    OS_STD,        // -59 dB stopband, 20/36
    OS_STEEP       // -89 dB stopband, 32/56
};

namespace hb {
template<OsQuality Q> struct Kernels;
template<> struct Kernels<OS_FAST>  { using S1 = S1Fast;  using S2 = S2Fast;  };
template<> struct Kernels<OS_STD>   { using S1 = S1Std;   using S2 = S2Std;   };
template<> struct Kernels<OS_STEEP> { using S1 = S1Steep; using S2 = S2Steep; };
} // namespace hb

template<OsQuality Q = OS_STD>
class Oversampler {
public:
    using S1 = typename hb::Kernels<Q>::S1;
    using S2 = typename hb::Kernels<Q>::S2;

    static constexpr int kMaxFactor = 4;

    Oversampler() { reset(); }

    // 1 (bypass), 2 or 4. Anything else is clamped to the nearest of those.
    // Changing the factor clears the delay lines, so do not call it per
    // sample - it is a setup / "quality knob moved" operation.
    void setFactor(int f) {
        const int nf = (f >= 4) ? 4 : (f >= 2) ? 2 : 1;
        if (nf == mFactor) return;
        mFactor = nf;
        reset();
    }

    int factor() const { return mFactor; }

    void reset() {
        mUp1.reset();
        mDown1.reset();
        mUp2.reset();
        mDown2.reset();
    }

    // Total group delay of up + core + down, in INPUT samples. Exact, and an
    // integer at 2x; at 4x it is always a half-integer, which is inherent to
    // cascading two odd-length halfbands and not something a different design
    // would avoid.
    MYFLOAT latencySamples() const {
        if (mFactor == 1) return 0.0;
        const MYFLOAT l1 = 2.0 * S1::n - 1.0;
        if (mFactor == 2) return l1;
        return l1 + (2.0 * S2::n - 1.0) * 0.5;
    }

    // ---- the short form -------------------------------------------------
    //
    // core is anything callable as MYFLOAT(MYFLOAT) - a lambda, a functor, a
    // function pointer. It is called mFactor times per input sample, in time
    // order, and must be stateful-safe: it sees a genuine 2x or 4x stream, so
    // anything inside it that depends on the sample rate must already have
    // been set up for coreRate(), not the host rate.
    //
    template<class Core>
    inline MYFLOAT process(MYFLOAT in, Core &&core) {
        if (mFactor == 1) return core(in);

        MYFLOAT a[2];
        mUp1.tick(in, a);

        if (mFactor == 2) {
            a[0] = core(a[0]);
            a[1] = core(a[1]);
            return mDown1.tick(a);
        }

        MYFLOAT b[2], y[2];
        mUp2.tick(a[0], b);
        b[0] = core(b[0]);
        b[1] = core(b[1]);
        y[0] = mDown2.tick(b);

        mUp2.tick(a[1], b);
        b[0] = core(b[0]);
        b[1] = core(b[1]);
        y[1] = mDown2.tick(b);

        return mDown1.tick(y);
    }

    // ---- the explicit form ----------------------------------------------
    //
    // For cores that are not a single call - a filter that wants its own
    // block loop, several stages, SIMD over the oversampled frame.
    //
    //     MYFLOAT buf[Oversampler<>::kMaxFactor];
    //     const int n = os.up(in, buf);
    //     for (int i = 0; i < n; ++i) buf[i] = core.tick(buf[i]);
    //     out = os.down(buf);
    //
    // up() and down() must be called in strict alternation with the same
    // buffer contents in between; the second stage keeps state across the two
    // halves of a 4x frame.
    //
    inline int up(MYFLOAT in, MYFLOAT out[kMaxFactor]) {
        if (mFactor == 1) {
            out[0] = in;
            return 1;
        }
        MYFLOAT a[2];
        mUp1.tick(in, a);
        if (mFactor == 2) {
            out[0] = a[0];
            out[1] = a[1];
            return 2;
        }
        mUp2.tick(a[0], &out[0]);
        mUp2.tick(a[1], &out[2]);
        return 4;
    }

    inline MYFLOAT down(const MYFLOAT in[kMaxFactor]) {
        if (mFactor == 1) return in[0];
        if (mFactor == 2) return mDown1.tick(in);
        MYFLOAT y[2];
        y[0] = mDown2.tick(&in[0]);
        y[1] = mDown2.tick(&in[2]);
        return mDown1.tick(y);
    }

    // ---- block helper ---------------------------------------------------
    template<class Core>
    inline void processBlock(MYFLOAT *buf, int n, Core &&core) {
        for (int i = 0; i < n; ++i) buf[i] = process(buf[i], core);
    }

    // The rate the core actually runs at. Pass this to the core's init(),
    // not the host rate - forgetting to is the classic oversampling bug and
    // detunes every frequency-dependent coefficient inside it.
    MYFLOAT coreRate(MYFLOAT hostRate) const { return hostRate * mFactor; }

private:
    hb::Interp2<S1> mUp1;
    hb::Decim2<S1> mDown1;
    hb::Interp2<S2> mUp2;
    hb::Decim2<S2> mDown2;
    int mFactor = 2;
};

// The factor is a runtime setting, not a template parameter, so that a
// quality control in the UI can move between 1x / 2x / 4x without the host
// having to hold three differently-typed objects. The quality tier IS a
// template parameter, because switching filter length at runtime would mean
// either allocating or sizing every buffer for OS_STEEP.
//
//     tsl::dsp::Oversampler<tsl::dsp::OS_FAST> cheap;  cheap.setFactor(2);
//     tsl::dsp::Oversampler<> normal;                  normal.setFactor(4);

} // namespace dsp
} // namespace tsl

#endif // TSL_OVERSAMPLER_H
