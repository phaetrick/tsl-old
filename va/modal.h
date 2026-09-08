#pragma once
//
// Modal resonator bank — ported from CPP-New/generative/Voice.h.
//
// A bank of exponentially decaying sinusoids, which is what a bell, a bar, a
// plate or a piano string actually is. Each partial is one complex rotation per
// sample:
//
//     z <- z * r * e^{jw}
//
// two state variables, four multiplies and two adds. Unlike a biquad ringing at
// its resonance it cannot drift in frequency or blow up: r < 1 makes the
// magnitude monotonically decreasing by construction.
//
// ── Two entry points, one bank ───────────────────────────────────────────────
//
// The self-excited and driven versions differ ONLY in where the per-partial
// amplitude a_i lands:
//
//   excite()          a_i is an initial condition   -> osc type 97
//   process(x)        a_i is an input gain          -> filter mode 17
//
// Everything expensive (ratio walk, Nyquist cull, tilt/damping, the strike comb,
// t60 -> r) is common to both.
//
// ── Deliberately dependency-free ─────────────────────────────────────────────
//
// Plain standard C++ double: no MYFLOAT, no defines.h, no AppState. That is what
// makes this compilable standalone in one clang command, which is what makes the
// measure-and-fix loop possible. Keep it that way — glue belongs in vco.cpp /
// synth.cpp, not here. double (not float) is not a style choice either: these are
// high-Q recursions where r is within 1e-4 of 1.
//

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pa {

inline constexpr int MODAL_MAX_PARTIALS = 14;

// Output trim for the self-excited path. This is the one constant to change if the
// level is wrong by ear — PAD_TARGET_RMS's opposite number.
//
// It exists because energy normalisation equalises RMS, not PEAK. Measured across
// all six characters at 220 Hz / 2 s: RMS is uniform (0.139..0.153) but peak runs
// 0.85 (DEEP) to 1.53 (BELL), because a handful of partials align far more often
// than fourteen do. Crest is therefore ~10.6 (20.5 dB) against a saw's 1.73
// (4.8 dB), and no single number can match a saw on both — matching RMS puts BELL's
// peaks near 6. Trimmed for peak instead, which lands the bank ~16 dB under SAW at
// the same GAIN. That is the honest cost of a struck resonator and not a bug.
inline constexpr double MODAL_OUTPUT_TRIM = 0.65;

inline double modalFlushDenorm(double x) {
    return (x > -1e-30 && x < 1e-30) ? 0.0 : x;
}

// Output safety for the driven path, with a soft knee. Below the threshold this is
// exactly the identity — no colour, no gain change — and above it the tanh maps
// [t, inf) into [t, 1) so full scale can never be exceeded. Slope is 1 at the
// threshold, so the transition is smooth in value and in derivative.
//
// The driven path needs this and the struck path does not, because a resonator fed a
// TONAL source is a coincidence detector with enormous gain when it coincides: a saw
// whose harmonics land on modes measured +15 dBFS at Q~50. That is resonance working
// as intended, not a bug, so it wants a ceiling rather than a smaller input gain.
inline double modalSoftClip(double x) {
    constexpr double t = 0.75;
    if (x > -t && x < t) return x;
    const double s = x < 0.0 ? -1.0 : 1.0;
    return s * (t + (1.0 - t) * std::tanh((std::fabs(x) - t) / (1.0 - t)));
}

// Output trim for the driven path, applied before the soft clip.
// Output trim for the driven path, applied before the soft clip. Set by measurement,
// not taste: at trim 1.0 the worst tonal case (saw at f0, RESO max, bank on the same
// pitch — which is the DEFAULT case, since gains[4] starts from _cps) peaks at
// +27.9 dBFS. This lands typical RESO near unity and lets the clip only just catch
// the top of the range.
//
// Broadband input sits ~33 dB below tonal input through the same bank, because a
// resonator only captures the fraction of a source that falls inside its skirts.
// That is not correctable with one constant — the noise generator's own -60..+60 dB
// GAIN is the makeup, and a noise-excited modal patch wants it turned well up.
inline constexpr double MODAL_DRIVEN_TRIM = 0.05;

// ── Characters ───────────────────────────────────────────────────────────────
//
// `ratios` is the important field. Randomising tilt and decay around a single
// harmonic series only ever produces variations on one instrument, because the
// *partial ratios* are what the ear identifies. A bar, a bell and a string differ
// in where their modes sit, not in how loud they are.
//
// Trimmed against gen's VoiceCharacter: regCentre/regSpread were the generative
// engine's note-assignment weights (PA picks with a knob), and stretchLo/stretchHi
// collapse to the midpoint constant since the per-note roll is gone — see the
// velocity note in ModalBank::configure.
struct ModalCharacter {
    const char* name;

    /** Explicit partial ratios relative to the fundamental. If nRatios is 0 the
     * partials are harmonic (k) with the stretch law below applied instead. */
    double ratios[MODAL_MAX_PARTIALS];
    int    nRatios;

    /** Inharmonicity for the harmonic case: f_k = k*f0*sqrt(1 + stretch*k^2).
     * This is the piano/string law; it cannot produce a bell. */
    double stretch;

    /** Partial amplitude falloff: tilt = tiltBase - tiltBright * brightness. */
    double tiltBase, tiltBright;

    /** How much faster partial k decays than the fundamental. */
    double dampBase, dampBright;

    double decayScale;   ///< multiplies the patch decay for this character
    double attackScale;  ///< multiplies the patch attack
    double ampTrim;      ///< measured against the others, not guessed
};

inline constexpr int MODAL_NUM_CHARACTERS = 6;

// Ordered dark/low to bright/high. Keep the ordering — a selector sweep should
// change the instrument gradually rather than jump.
inline constexpr ModalCharacter kModalCharacters[MODAL_NUM_CHARACTERS] = {
    // Warm, few partials, very long — the closest thing to a sustained tone the
    // struck voices have.
    {"DEEP",   {}, 0,  0.00008,   2.35, 0.55,  0.34, 0.16,  1.30, 1.45, 0.84},

    // Piano-ish: harmonic with genuine string inharmonicity, many partials.
    {"STRING", {}, 0,  0.000315,  1.62, 0.72,  0.30, 0.18,  1.05, 1.00, 0.92},

    // Rhodes-ish. The 4th partial sits slightly sharp of harmonic, which is most
    // of what makes a tine sound like a tine rather than a piano.
    {"TINE",   {1.0, 2.0, 3.01, 4.07, 6.15, 9.42}, 6,
                   0.0,       1.48, 0.60,  0.26, 0.14,  0.95, 0.85, 0.95},

    // Tuned bar: the classic 1 : 4 : 10 marimba tuning, only a few modes, and the
    // highs die fast. Short by the standards of the other characters.
    {"WOOD",   {1.0, 3.93, 9.55, 16.72}, 4,
                   0.0,       1.15, 0.45,  0.62, 0.22,  0.55, 0.55, 1.22},

    // Church-bell partials: hum, prime, tierce (a minor third — this is why bells
    // always sound faintly minor), quint, nominal, and upper modes.
    {"BELL",   {0.5, 1.0, 1.19, 1.51, 2.0, 2.62, 3.44, 4.42}, 8,
                   0.0,       1.05, 0.40,  0.22, 0.12,  1.35, 1.20, 0.83},

    // Sparse and strongly inharmonic, very long. Sits above everything else.
    {"GLASS",  {1.0, 2.32, 4.25, 6.63, 9.38}, 5,
                   0.0,       0.95, 0.35,  0.20, 0.10,  1.45, 1.60, 0.83},
};

// ── Bank ─────────────────────────────────────────────────────────────────────

class ModalBank {
public:
    void init(double sampleRate) {
        mSampleRate = sampleRate;
        mNyquist    = sampleRate * 0.5;
        reset();
    }

    void reset() {
        mNumPartials = 0;
        mAttackPos   = 1.0;
        for (int i = 0; i < MODAL_MAX_PARTIALS; ++i) {
            mRe[i] = mIm[i] = 0.0;
            mA[i]  = mB[i]  = 0.0;
            mR[i]  = mAmp[i] = mG[i] = 0.0;
            mRR[i] = mJitter[i] = mFall[i] = mGBase[i] = 0.0;
        }
    }

    /** Lay out the mode structure. Call at note-on, for BOTH paths.
     *
     * decaySec is the fundamental's T60; higher partials die faster, which is what
     * makes a long tail darken as it fades instead of just getting quieter.
     * brightness 0..1 tilts the initial partial amplitudes: around 1.7 is dark and
     * flute-like, 0.9 bright and bell-like (measured — 2.1 put partial 8 at -50 dB).
     *
     * hardness is contact time as a spectral tilt: a soft mallet never delivers
     * energy to the high modes in the first place. Drive it from velocity — that
     * is what replaced gen's random +-7% gesture wobble, and it is both cheaper
     * and physically right.
     *
     * position is where it is struck. A mode with a node at the striking point
     * cannot be excited, giving the comb |sin(pi * ratio * position)|. Striking
     * dead centre (0.5) nulls every even partial, which is most of why a
     * centre-struck bar sounds hollow.
     */
    void configure(int characterIdx, double f0Hz, double decaySec, double brightness,
                   double hardness, double position, uint32_t seed) {
        const int ci = std::clamp(characterIdx, 0, MODAL_NUM_CHARACTERS - 1);
        const ModalCharacter& ch = kModalCharacters[ci];
        mCharacter = &ch;

        mF0        = f0Hz;
        mDecaySec  = decaySec * ch.decayScale;
        mAmpTrim   = ch.ampTrim;

        hardness   = std::clamp(hardness, 0.0, 1.0);
        mPosition  = std::clamp(position, 0.0, 0.5);

        // Both of these fold BRIGHTNESS in, which is why brightness is a structural
        // parameter and position is not: it moves the damping as well as the tilt,
        // so it cannot be changed by the cheap path below.
        const double tilt     = ch.tiltBase - ch.tiltBright * brightness;
        mHiDamp               = ch.dampBase - ch.dampBright * brightness;
        const double softTilt = (1.0 - hardness) * 1.35;
        const double tiltExp  = tilt + softTilt;

        const int maxK = ch.nRatios > 0 ? ch.nRatios : MODAL_MAX_PARTIALS;

        uint32_t rng = seed ? seed : 0x9E3779B9u;

        mNumPartials = 0;
        for (int k = 1; k <= maxK; ++k) {
            // An explicit ratio table is what separates a bell from a string; the
            // stretch law can only ever bend a harmonic series.
            const double ratio = ch.nRatios > 0
                                     ? ch.ratios[k - 1]
                                     : double(k) * std::sqrt(1.0 + ch.stretch * double(k * k));
            const double f = f0Hz * ratio;
            if (f >= mNyquist * 0.92) break;

            const int i = mNumPartials++;
            mRatio[i] = ratio;

            // Falloff and damping follow the partial's *frequency*, not its index.
            // For a harmonic series the two are the same, but a bell's 8th mode
            // sits at 4.4x rather than 8x and should be damped like a mode at 4.4x.
            // Clamped at 1 so a sub-fundamental (a bell's hum note) is not boosted
            // above the partial it belongs to.
            mRR[i] = ratio < 1.0 ? 1.0 : ratio;

            // Per-strike amplitude scatter. This is the one randomisation that has
            // to survive the port to PA: without it every note is the same timbre
            // and the ear stops listening after a dozen of them. Free — no param,
            // one rng call per partial at note-on.
            //
            // CACHED rather than re-rolled, because setPosition() rebuilds the
            // amplitudes at block rate: re-running the rng there would re-scatter
            // the whole timbre 750 times a second, which is noise, not character.
            mJitter[i] = 0.8 + 0.4 * nextUniform(rng);

            // The spectral falloff, likewise cached — it depends only on the ratio
            // and on BRIGHT/HARD, none of which the cheap paths touch. This is the
            // pow() that made a per-block rebuild look expensive.
            mFall[i] = 1.0 / std::pow(mRR[i], tiltExp);
        }

        // Any partials the previous layout was using but this one is not keep their
        // state and would resume ringing if a later configure() grows the count
        // again — stale energy from an old body fired into a new one. The struck
        // path never sees this (excite() rewrites every slot), but the filter
        // re-configures on the fly whenever RESO or BODY moves.
        for (int i = mNumPartials; i < MODAL_MAX_PARTIALS; ++i) mRe[i] = mIm[i] = 0.0;

        rebuildDecay();
        rebuildExcitation();
        retune(f0Hz);
    }

    /** Change the fundamental's T60 without disturbing anything that is ringing.
     *
     * Safe on BOTH paths, and the only structural parameter of which that is true.
     * r appears solely in the *next* rotation step, so rewriting it lengthens or
     * shortens the tail from this sample forward and leaves the state vector's
     * magnitude alone — exactly the argument that makes retune() click-free. The
     * amplitudes are untouched, so mNorm does not move and the struck path's
     * output-side normalisation cannot step.
     *
     * Cost is 14 pow + 14 sqrt, plus the retune() that follows because mA/mB carry
     * r. Call it at block rate, not per sample.
     */
    void setDecay(double decaySec) {
        const double d = decaySec * (mCharacter ? mCharacter->decayScale : 1.0);
        if (d == mDecaySec) return;
        mDecaySec = d;
        rebuildDecay();
        refreshInputGains();   // mG carries sqrt(1-r^2), which just moved
        retune(mF0);
    }

    /** Change the strike position. DRIVEN PATH ONLY (filter mode 17).
     *
     * On the driven path the amplitudes are input gains: rewriting them changes how
     * much *new* input each mode receives and does nothing to the energy already
     * circulating, which is why dragging STRIKE is silent (measured 1.9x the
     * sample-to-sample delta of a bank held still, against 7.9x before mNorm was
     * moved off the output).
     *
     * Do NOT call this on a struck bank. There the amplitudes were consumed by
     * excite() and mNorm is applied at the output, so moving it would rescale
     * already-ringing energy — the step discontinuity described in
     * refreshInputGains(). The struck oscillator takes a new position at the next
     * note-on instead, which is also the only place a real strike position can move.
     *
     * Cost is 14 sin and a handful of multiplies: no pow, no sqrt, and no retune,
     * because neither the ratios nor r are involved.
     */
    void setPosition(double position) {
        const double p = std::clamp(position, 0.0, 0.5);
        if (p == mPosition) return;
        mPosition = p;
        rebuildExcitation();
    }

    /** Recompute the rotation coefficients for a new fundamental.
     *
     * Safe to call while the bank is ringing: the complex-rotation form is
     * magnitude-preserving, so changing w rewrites the rotation rate without
     * touching the state vector's amplitude — no click, no discontinuity. This is
     * what makes glide, bend, LFO->pitch and Jitter work on a modal bank at all.
     * Call it at block rate; 14 sin/cos per 64 samples is nothing.
     */
    void retune(double f0Hz) {
        mF0 = f0Hz;
        const double k = 6.283185307179586 / mSampleRate;
        for (int i = 0; i < mNumPartials; ++i) {
            const double f = f0Hz * mRatio[i];
            double r = mR[i];
            // A partial modulated up past Nyquist would alias. The bank structure is
            // fixed at note-on, so instead of restructuring mid-note damp it out over
            // a few ms and let it come back when the pitch falls again. Only reachable
            // with large upward pitch modulation, where these partials are quiet.
            if (f >= mNyquist * 0.98) r *= 0.9;
            const double w = k * f;
            mA[i] = r * std::cos(w);
            mB[i] = r * std::sin(w);
        }
    }

    /** Strike it. Osc 97 only — sets initial conditions, takes no input.
     *
     * Partials start on the imaginary axis so the output (the real part) begins at
     * zero and the attack envelope has something to shape. They start at full
     * amplitude, so the raw sum begins with a click; the attack is what turns that
     * into a strike, and lengthening it is the whole difference between a mallet
     * and a bowed swell.
     */
    void excite(double attackSec) {
        for (int i = 0; i < mNumPartials; ++i) {
            mRe[i] = 0.0;
            mIm[i] = mAmp[i];
        }
        const double atk = attackSec > 0.0005 ? attackSec : 0.0005;
        mAttackInc = 1.0 / (atk * mSampleRate);
        mAttackPos = 0.0;
    }

    /** Self-excited output. Sums the PRE-update real parts, exactly as the
     * generative engine does, so a correct port is sample-identical to the
     * ear-verified reference and can be A/B'd against it. */
    inline double tick() {
        double sum = 0.0;
        for (int i = 0; i < mNumPartials; ++i) {
            const double re = mRe[i], im = mIm[i];
            mRe[i] = modalFlushDenorm(re * mA[i] - im * mB[i]);
            mIm[i] = modalFlushDenorm(re * mB[i] + im * mA[i]);
            sum += re;
        }
        // Raised cosine attack: no corner at either end, so no click and no audible
        // "arrival" at full level.
        double atk = 1.0;
        if (mAttackPos < 1.0) {
            atk = 0.5 - 0.5 * std::cos(3.141592653589793 * mAttackPos);
            mAttackPos += mAttackInc;
        }
        return sum * atk * mNorm * mAmpTrim * MODAL_OUTPUT_TRIM;
    }

    /** Driven output. Filter 17 only — never call excite() on a bank used this
     * way, and never reset its state at note-on, or it stops being a resonator.
     *
     * Sums the POST-update real parts so injected energy reaches the output in the
     * same sample rather than one late.
     *
     * Note the bandwidth this implies: BW ~= 2.2/t60 Hz, so a 2 s decay is a 1.1 Hz
     * skirt. A tonal source at the same pitch will excite almost nothing except
     * where a mode happens to land on a harmonic — feed this noise, transients, or
     * a gated osc burst, or shorten the decay into the 30-80 ms range where the
     * skirts are wide enough to catch. */
    inline double process(double x) {
        double sum = 0.0;
        for (int i = 0; i < mNumPartials; ++i) {
            const double re = mRe[i], im = mIm[i];
            const double nr = modalFlushDenorm(re * mA[i] - im * mB[i] + x * mG[i]);
            mRe[i] = nr;
            mIm[i] = modalFlushDenorm(re * mB[i] + im * mA[i]);
            sum += nr;
        }
        // No mNorm here — it lives in mG and strike() for this path, so that a live
        // re-configure never changes the gain applied to already-ringing state.
        return modalSoftClip(sum * mAmpTrim * MODAL_DRIVEN_TRIM);
    }

    int         numPartials() const { return mNumPartials; }
    const char* characterName() const { return mCharacter ? mCharacter->name : ""; }
    double      attackScale() const { return mCharacter ? mCharacter->attackScale : 1.0; }

private:
    // Small inline PRNG so the header stays dependency-free (gen uses Rng.h). Only
    // called MODAL_MAX_PARTIALS times per note-on, so quality matters more than
    // speed and xorshift32 is already past what this needs.
    static inline double nextUniform(uint32_t& s) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return double(s) * (1.0 / 4294967296.0);
    }

    /** r and its companion sqrt(1-r^2), from mDecaySec / mHiDamp / mRR. */
    void rebuildDecay() {
        for (int i = 0; i < mNumPartials; ++i) {
            // r depends on the RATIO only, never on f0 — which is what makes
            // retune() cheap and what lets pitch be modulated at block rate.
            const double t60 = mDecaySec / (1.0 + mHiDamp * (mRR[i] - 1.0));
            const double r   = std::pow(10.0, -3.0 / (t60 * mSampleRate)); // r^(t60*fs)=1e-3
            mR[i] = r;

            // Input gain for the driven path. sqrt(1-r^2), NOT (1-r).
            //
            // (1-r) is unity gain for a sine sitting exactly on the mode, which
            // sounds like the principled choice and is not: a resonator's bandwidth
            // is ~2.2/T60 Hz, so as DECAY rises the bank captures less and less of a
            // broadband source and the output collapses. Measured on BELL with white
            // noise, (1-r) swings 27 dB across the DECAY range (-35 dB at 0.05 s to
            // -62 dB at 8 s) — DECAY would secretly be a volume control.
            //
            // For a one-pole resonator driven by white noise the output variance is
            // g^2/(1-r^2), so g = sqrt(1-r^2) holds output POWER constant regardless
            // of r. Same measurement with this law: 5 dB of swing instead of 27.
            const double g2 = 1.0 - r * r;
            mGBase[i] = g2 > 0.0 ? std::sqrt(g2) : 0.0;
        }
    }

    /** Amplitudes, energy normalisation and input gains, from the cached jitter and
     * falloff plus the strike comb at mPosition. The only thing setPosition() runs. */
    void rebuildExcitation() {
        double energy = 0.0;
        for (int i = 0; i < mNumPartials; ++i) {
            double comb = 1.0;
            if (mPosition > 0.001) {
                // A mode with a node at the striking point cannot be excited.
                comb = std::fabs(std::sin(3.141592653589793 * mRatio[i] * mPosition));
                // Floored rather than allowed to reach zero: an exact null on the
                // fundamental would drop the note's pitch out entirely, and with
                // inharmonic ratios the nulls do not land where a real strike would
                // put them anyway.
                if (comb < 0.12) comb = 0.12;
            }
            const double a = mJitter[i] * comb * mFall[i];
            mAmp[i] = a;
            energy += a * a;
        }

        // Normalise on energy, not on the sum of amplitudes. Summing amplitudes
        // bounds the theoretical peak at 1, but partials only reach that bound when
        // they align — which happens far more often with four partials than with
        // fourteen, so the sparse characters run hot. Energy normalisation makes
        // loudness independent of how many partials a character has.
        mNorm = energy > 0.0 ? 1.0 / std::sqrt(energy) : 1.0;
        refreshInputGains();
    }

    /** mG = sqrt(1-r^2) * a * mNorm. Split out because setDecay() moves the first
     * factor while leaving the other two alone.
     *
     * Note where mNorm lands: on the INPUT gains, not the output. The struck path
     * can normalise at the output safely because configure() is only ever followed
     * by excite(), which rewrites every state slot. The driven path cannot — the
     * filter rebuilds live whenever STRIKE, RESO or BODY moves, and mNorm depends on
     * the strike comb, so an output-side mNorm would rescale energy that is ALREADY
     * RINGING: a step discontinuity on every knob update. Measured while dragging
     * STRIKE, that was 7.9x the sample-to-sample delta of the same bank held still.
     * Applied here it only ever affects future input, so the drag is silent. */
    void refreshInputGains() {
        for (int i = 0; i < mNumPartials; ++i) mG[i] = mGBase[i] * mAmp[i] * mNorm;
    }

    double mSampleRate{48000.0};
    double mNyquist{24000.0};

    double mRe[MODAL_MAX_PARTIALS]{}, mIm[MODAL_MAX_PARTIALS]{};
    double mA[MODAL_MAX_PARTIALS]{},  mB[MODAL_MAX_PARTIALS]{};
    double mR[MODAL_MAX_PARTIALS]{};      // decay per partial, f0-independent
    double mRatio[MODAL_MAX_PARTIALS]{};  // kept so retune() needs no character lookup
    double mAmp[MODAL_MAX_PARTIALS]{};    // a_i — initial condition (excite path)
    double mG[MODAL_MAX_PARTIALS]{};      // sqrt(1-r^2)*a_i*norm — input gain (driven)
    int    mNumPartials{0};

    // Caches that let setDecay()/setPosition() skip the expensive parts of the
    // layout. All four are written only by configure(), and depend only on the
    // character, f0 and BRIGHT/HARD — never on decay or position.
    double mRR[MODAL_MAX_PARTIALS]{};     // ratio clamped at 1 (falloff/damping index)
    double mJitter[MODAL_MAX_PARTIALS]{}; // per-strike scatter, rolled once per note
    double mFall[MODAL_MAX_PARTIALS]{};   // 1/rr^(tilt+softTilt)
    double mGBase[MODAL_MAX_PARTIALS]{};  // sqrt(1-r^2), rewritten by setDecay()

    double mNorm{1.0}, mAmpTrim{1.0};
    double mF0{440.0}, mDecaySec{1.0};
    double mHiDamp{0.0}, mPosition{0.0};
    double mAttackPos{1.0}, mAttackInc{1.0};
    const ModalCharacter* mCharacter{nullptr};
};

}  // namespace pa
