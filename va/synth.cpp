
#include "synth.h"

#include <dlfcn.h>
#include "defines.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "button.h"
#include "ffttools.h"
#include <queue.h>
#include <file.h>
#include "tools.h"
#include "player.h"
#include "view.h"
#include "moogladder.h"
#include <audio/ZdfFilters.h>
#include "reverbprogenitor.h"
#include "sequencer.h"
#include <envelope.h>
#include <tools.h>
#include <tools/AdpfHint.h>
#include "vco.h"
#include "random.h"
#ifdef __ANDROID__
#include <jni.h>
#endif
#include <app.h>
#include <resample.h>

#define SR 44100
#define MAX_OFFSET 4000
#define M_2PI 2 * M_PI


static constexpr int numtaps = 16;

static inline int64_t getNanoseconds(clockid_t clockId = CLOCK_MONOTONIC) {
    struct timespec time;
    int result = clock_gettime(clockId, &time);
    if (result < 0) {
        return result;
    }
    return (time.tv_sec * tsl::time::nanosPerSecond) + time.tv_nsec;
}


#if defined(__i386__) || defined(__x86_64__)
#define cpu_relax() asm volatile("rep; nop" ::: "memory");

#elif defined(__arm__) || defined(__mips__)
#define cpu_relax() asm volatile("":::"memory")

#elif defined(__aarch64__)
#define cpu_relax() asm volatile("yield" ::: "memory")
#else
#error "cpu_relax is not defined for this architecture"
#endif

constexpr int32_t kLoadGenerationStepSizeNanos = 20000;
constexpr MYFLOAT kPercentageOfCallbackToUse = 0.8;

static inline void generateLoad(int64_t durationNanos, MYFLOAT &mOpsPerNano) {
    int64_t currentTimeNanos = getNanoseconds();
    int64_t deadlineTimeNanos = currentTimeNanos + durationNanos;

    // opsPerStep gives us an estimated number of operations which need to be run to fully utilize
    // the CPU for a fixed amount of time (specified by kLoadGenerationStepSizeNanos).
    // After each step the opsPerStep value is re-calculated based on the actual time taken to
    // execute those operations.
    auto opsPerStep = (int) (mOpsPerNano * kLoadGenerationStepSizeNanos);
    int64_t stepDurationNanos = 0;
    int64_t previousTimeNanos = 0;

    while (currentTimeNanos <= deadlineTimeNanos) {

        for (int i = 0; i < opsPerStep; i++) cpu_relax();

        previousTimeNanos = currentTimeNanos;
        currentTimeNanos = getNanoseconds();
        stepDurationNanos = currentTimeNanos - previousTimeNanos;

        // Calculate exponential moving average to smooth out values, this acts as a low pass filter.
        // @see https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
        static const MYFLOAT kFilterCoefficient = 0.1;
        auto measuredOpsPerNano = (MYFLOAT) opsPerStep / stepDurationNanos;
        mOpsPerNano =
                kFilterCoefficient * measuredOpsPerNano + (1.0 - kFilterCoefficient) * mOpsPerNano;
        opsPerStep = (int) (mOpsPerNano * kLoadGenerationStepSizeNanos);
    }
}

template<typename T>
class Jitter {
public:
    Jitter() = default;

    void update(T amp, T cpsA, T cpsB) {
        if (_prevA != cpsA || _amp != amp || _prevB != cpsB) {
            _prevA = cpsA;
            _prevB = cpsB;
            _amp = amp;
            _cpsMin = std::min(cpsA, cpsB);
            _cpsAmount = DISTANCE(cpsA, cpsB);
            nextSeg();
        }
    }

    T inline tick() {
        T ret = _currentAmp;
        _currentAmp += _ampinc;
        if (--_count <= 0)
            nextSeg();
        return ret;
    }

    void nextSeg() {
        _count = _ksr / (_cpsMin + randGab * _cpsAmount);
        T nextamp = randGab * _amp;
        _ampinc = (nextamp - _currentAmp) / (T) _count;
    }

    T _ksr{};

private:
    int32_t _count{};
    T _amp{}, _ampinc{};
    T _currentAmp{};
    T _cpsMin{};
    T _cpsAmount{};
    T _prevA{}, _prevB{};
    uint32_t holdrand{(uint32_t) NoiseBase::randomMYFLOAT(10, 1000000)};
};


struct ADSR {
    static enum {
        ATTACK = 0,
        DECAY = 1,
        SUSTAIN = 2,
        RELEASE = 3,
        DEAD = 4
    };
    static constexpr MYFLOAT base = 20000.0;
    static constexpr MYFLOAT maxTime = 1. / 10.0;

    // THIS ENVELOPE RUNS AT THE CONTROL RATE, ksr = sr/64, i.e. 750 Hz at 48 kHz, and
    // every segment below is the discrete one-pole env += k*(target-env)/ksr. That is
    // env *= (1 - k/ksr) for the release, so it is only monotonic while k < ksr:
    // between ksr and 2*ksr the multiplier is NEGATIVE and the envelope alternates
    // sign every control tick, and past 2*ksr it diverges.
    //
    // setRelease's floor of 0.15 is what keeps that safe — it gives k = 20000^0.85*0.1
    // = 453/s, k/ksr = 0.60. DO NOT WRITE A SMALLER NUMBER INTO `release` TO GET A
    // FASTER FADE. Voice stealing did exactly that (0.07, k = 1000, k/ksr = 1.33), so
    // every stolen voice inverted its own polarity two or three times on the way down
    // and every steal was ear-reported as a click. A steal now fades at the SAMPLE
    // rate outside this class entirely — see VcoNote::beginSteal.

    ADSR() = default;

    ADSR(MYFLOAT attRate, MYFLOAT decRate,
         MYFLOAT susLevel, MYFLOAT relRate) {
        setAttack(attRate);
        setSustain(susLevel);
        setDecay(decRate);
        setRelease(relRate);
    }

    void keyOff() {
        if (state != DEAD) {
            state = RELEASE;
        }
    }

    // Retriggers into ATTACK without touching env. A hard reset (env = 0)
    // would click, since env directly drives audible parameters (amp, filter,
    // pitch...) simultaneously — leaving env wherever it currently is means
    // the attack curve just continues rising smoothly from that point,
    // same principle as keyOff() not resetting env when moving to RELEASE.
    void retrigger() {
        state = ATTACK;
    }

    void setAll(MYFLOAT attRate, MYFLOAT decRate,
                MYFLOAT susLevel, MYFLOAT relRate) {
        env = 0;
        state = ATTACK;
        setAttack(attRate);
        setDecay(decRate);
        setSustain(susLevel);
        setRelease(relRate);
    }

    void setAttack(MYFLOAT att) {
        attack = 0.15 + att * 0.7;
    }

    void setDecay(MYFLOAT dec) {
        decay = 0.15 + dec * 0.7;
        // if (decay < 1e-3)decay = 1e-3;
    }

    void setSustain(MYFLOAT sus) {
        sustain = sus;
        if (sustain < 1e-3)sustain = 1e-3;
    }

    void setRelease(MYFLOAT rel) {
        release = 0.15 + rel * 0.7;
        //  release = rel;
        //if (release < 1e-3)release = 1e-3;
    }

    MYFLOAT tick() {
        if (state == ATTACK) {
            env += pow(base, 1 - attack) * maxTime * (1.01 - env) * _onedksr;
            if (env >= 1.0) {
                env = 1.0;
                state = DECAY;
            }
        } else if (state == DECAY) {
            env += pow(base, 1 - decay) * maxTime * (sustain - env) * _onedksr;
            if (DISTANCE(env, sustain) <= 1e-3) {
                env = sustain;
                state = SUSTAIN;
            }
        } else if (state == RELEASE) {
            env += pow(base, 1. - release) * maxTime * (0.0 - env) * _onedksr;
            if (DISTANCE(env, 0.0) <= 1e-2) {
                env = 0.0;
                state = DEAD;
            }
        }
        return env;
    }

    MYFLOAT _onedksr{};
    MYFLOAT env{};
    MYFLOAT attack{};
    MYFLOAT decay{};
    MYFLOAT sustain{};
    MYFLOAT release{};
    int state{};
};

struct ADSR2 {
    static enum {
        ATTACK = 0,
        DECAY = 1,
        SUSTAIN = 2,
        RELEASE = 3,
        DEAD = 4
    };

    ADSR2() = default;

    ADSR2(MYFLOAT attRate, MYFLOAT decRate,
          MYFLOAT susLevel, MYFLOAT relRate) {
        setAll(attRate, decRate, susLevel, relRate);
    }

    void keyOff() {
        if (state != DEAD) {
            state = RELEASE;
        }
    }

    void setAll(MYFLOAT attRate, MYFLOAT decRate,
                MYFLOAT susLevel, MYFLOAT relRate) {
        setAttack(attRate);
        setSustain(susLevel);
        setDecay(decRate);
        setRelease(relRate);
    }

    void setAttack(MYFLOAT att) {
        if (att == 0) {
            state = DECAY;
            env = 1.0;
        } else {
            state = ATTACK;
            attack = _onedksr / (10. * pow(2, -10 + att * 10));
            env = 0;
        }
    }

    void setDecay(MYFLOAT dec) {
        if (sustain == 1.0 || dec == 0) decay = 0;
        else
            decay = _onedksr / (10. * (1. - sustain) * pow(2, -10 + dec * 10));
    }

    void setSustain(MYFLOAT sus) {
        sustain = sus;
    }

    void setRelease(MYFLOAT rel) {
        if (sustain == 0)
            release = 0;
        else
            release = _onedksr / (sustain * 10. * pow(2, -10 + rel * 10));
    }

    MYFLOAT tick() {
        if (state == ATTACK) {
            env += attack;
            if (env >= 1.0) {
                env = 1.0;
                state = DECAY;
            }
        } else if (state == DECAY) {
            env -= decay;
            if (env <= sustain) {
                env = sustain;
                state = SUSTAIN;
            }

        } else if (state == RELEASE) {
            env -= release;
            if (env <= 0) {
                env = 0;
                state = DEAD;
            }

        }
        return env;
    }

    MYFLOAT _onedksr{};
    MYFLOAT env = 0.0;
    MYFLOAT attack{};
    MYFLOAT decay{};
    MYFLOAT sustain{};
    MYFLOAT release{};
    int state{};
};


template<typename T>
class RingModFast {
public:
    void init(int type) {
        vco._appState = _appState;
        vco.check(type);
        vco.phs = 0;
        _isModal = type == 97;
        mCursorDown = mCursorUp = 0;
        for (int i = 0; i < numtaps * 2; i++)mXDown[i] = mXUp[i] = 0;
    }

    // The modulator is a full Vco, so a WT/PAD/MODAL modulator needs the same note-on
    // setup the audible oscillators get — check() alone leaves a modal bank that was
    // never laid out or struck (its tick returns 0 forever, and the ring multiply then
    // silences the whole voice) and leaves WT/PAD on their default sets. VcoNote::
    // initRingModulator does that setup through this accessor.
    Vco& modVco() { return vco; }

    // Oversampled multiply of the main signal by the SUB oscillator. Ring mod
    // (bipolar, carrier suppressed) and AM (unipolar, carrier retained) can be
    // enabled together or separately; the modulator is ticked once per
    // subsample and reused so its phase stays correct either way.
    T tick(const T in, T tune, T pw, T ringGain, bool ringOn, bool amOn, T amDepth) {
        // MODAL (mode 5) is not oversampled: the bank has no phase accumulator and is
        // band-limited by construction, and its tick converts cps straight to real Hz
        // — the halved tune below would retune it an octave low, and ticking it twice
        // per sample would halve its decay times in real time. So it ticks ONCE at the
        // true tune and the value feeds both subsamples.
        const T mModal = _isModal ? vco.tick(tune, 1.0, pw) : (T)0.0;
        // Move cursor before write so that cursor points to last written frame in read.
        if (--mCursorUp < 0) {
            mCursorUp = numtaps - 1;
        }
        mXUp[mCursorUp] = mXUp[mCursorUp + numtaps] = in;

        T sum = 0.0;

        // Multiply input times precomputed windowed sinc function.
        auto coefficients = _appState->data->coeffsUp.data();
        auto xFrame = &mXUp[mCursorUp];
        const int numLoops = numtaps >> 2; // n/4
        for (int i = 0; i < numLoops; i++) {
            // Manual loop unrolling, might get converted to SIMD.
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
        }
        T m1 = _isModal ? mModal : vco.tick(tune * .5, 1.0, pw);
        T f1 = 1.0;
        if (ringOn) f1 = m1 * ringGain;
        if (amOn)   f1 *= (1.0 + amDepth * m1);
        auto out1 = sum * f1;
        sum = 0.0;

        // Multiply input times precomputed windowed sinc function.
        xFrame = &mXUp[mCursorUp];
        for (int i = 0; i < numLoops; i++) {
            // Manual loop unrolling, might get converted to SIMD.
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
        }
        T m2 = _isModal ? mModal : vco.tick(tune * .5, 1.0, pw);
        T f2 = 1.0;
        if (ringOn) f2 = m2 * ringGain;
        if (amOn)   f2 *= (1.0 + amDepth * m2);
        auto out2 = sum * f2;
        // Move cursor before write so that cursor points to last written frame in read.
        if (--mCursorDown < 0) {
            mCursorDown = numtaps - 1;
        }
        mXDown[mCursorDown] = mXDown[mCursorDown + numtaps] = out1;

        if (--mCursorDown < 0) {
            mCursorDown = numtaps - 1;
        }
        mXDown[mCursorDown] = mXDown[mCursorDown + numtaps] = out2;

        sum = 0.0;

        // Multiply input times precomputed windowed sinc function.
        coefficients = _appState->data->coeffsDown.data();
        xFrame = &mXDown[mCursorDown];
        for (int i = 0; i < numLoops; i++) {
            // Manual loop unrolling, might get converted to SIMD.
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
            sum += *xFrame++ * *coefficients++;
        }
        return sum;
    }

    tsl::AppState* _appState{};
private:
    MYFLOAT mXUp[numtaps * 2]{}, mXDown[numtaps * 2]{};
    int mCursorDown{}, mCursorUp{};
    Vco vco;
    bool _isModal{};
};

class Phaser {
public:
    void init(MYFLOAT base, MYFLOAT range, MYFLOAT feedb, MYFLOAT speed) {
        clear();
        _base = base;
        _inc = speed * _appState->onedksr;
        fb = .7 + feedb * .29;
        // PHASERRANGE is labeled "SEMITONES" in the UI, so the sweep peak
        // should land exactly `range` semitones above _base (_base*2^(range/12)).
        // _range is the additional Hz needed to get there from _base, since the
        // sweep formula below is _base + sweepShape*_range (0 at trough, all of
        // _range added at the peak). The old `pow(2,range/12)*_base` treated
        // that whole product as the additional Hz on top of _base, so the actual
        // peak was _base*(1+2^(range/12)) - e.g. range=12 landed ~19 semitones
        // above base (3x the frequency) instead of the labeled 12 (2x).
        _range = (pow(2., range/12.) - 1.) * _base;
        active = true;
    }

    void clear(){
        for (int i = 0; i < 4; i++) {
            _x[i] = _y[i] = 0;
        }
        phase = 0;
    }

    inline void update() {
        if (!active)
            return;
        // A raised cosine computed directly, not a lookup into synth_hann: that
        // table is a one-shot analysis window, tapered to exactly 0 at both
        // ends but with mismatched slope there (decreasing into the end,
        // increasing out of the start) - fine for windowing a buffer once, but
        // tiling it as a repeating LFO puts a real kink at the seam, audible as
        // a jump once per cycle. cos() is exactly periodic, so this has matching
        // value AND slope (both 0) at the wrap - no kink.
        const MYFLOAT sweepShape = 0.5 * (1.0 - cos(phase * TWOPI_P));
        MYFLOAT omegapi = _appState->pidsr * (_base + sweepShape * _range);
        if (omegapi > PI_P * .5) omegapi = PI_P * .5;
        _beta = (1. - omegapi) / (1. + omegapi);
        phase += _inc;
        while (phase >= 1.0)
            phase -= 1.0;
    }

    inline MYFLOAT tick(const MYFLOAT in) {
        if (!active)return in;
        // Filter::fast_tanh is a Pade approximant that only saturates for small
        // inputs - for |x| beyond ~10-15 it diverges roughly linearly instead of
        // clamping like real tanh, unlike every moogladder.h call site (which
        // pre-scale into that safe range). Here the raw signal + near-unity
        // feedback (fb up to .99) can spike past that range at resonant peaks
        // in the sweep, so the "saturator" fails right when it's needed and
        // the loop briefly blows up - audible as clicks. std::tanh actually clamps.
        const MYFLOAT x = std::tanh(in + _y[3] * fb);
        _y[0] = _beta * (x + _y[0]) - _x[0];
        _x[0] = x;
        _y[1] = _beta * (_y[0] + _y[1]) - _x[1];
        _x[1] = _y[0];
        _y[2] = _beta * (_y[1] + _y[2]) - _x[2];
        _x[2] = _y[1];
        _y[3] = _beta * (_y[2] + _y[3]) - _x[3];
        _x[3] = _y[2];
        return (_y[3] + in) * 0.5;
    }

    tsl::AppState* _appState{};
protected:
    bool active{};
private:
    MYFLOAT _base{}, _range{}, fb{}, _inc{}, _beta{};
    MYFLOAT phase{};
    MYFLOAT _x[4]{};
    MYFLOAT _y[4]{};
};

struct Lfo {
    MYFLOAT phase{};
    MYFLOAT value{};

    void reset() { phase = 0.; value = 0.; }

    // `off` is the PHASE param, 0..1 of a cycle. Applied to a COPY when the shape is
    // read, never to the accumulator: two LFOs given the same rate then stay bit-identical
    // no matter what their phases are, and moving PHASE takes effect on a sounding note
    // rather than at the next note-on. The cost is that turning it steps the LFO value --
    // absorbed by the per-sample ramp on the gain destinations, and no worse than any
    // other param change on the bipolar ones.
    MYFLOAT tick(MYFLOAT rate, int wave, MYFLOAT sr, MYFLOAT off) {
        phase += rate / sr;
        if (phase >= 1.) phase -= 1.;
        MYFLOAT ph = phase + off;
        if (ph >= 1.) ph -= 1.;        // off is 0..1 and phase is 0..1, so one wrap does it
        switch (wave) {
            // OUTPUT IS BIPOLAR, -1..1, and every shape is centred on 0. It used to be
            // 0..1 because every route was a unipolar attenuator; now that only AMP is,
            // 0..1 meant each of the other fifteen destinations had to undo it with
            // (val-0.5)*2 before it could be used. -1..1 IS the modulation offset, so
            // phase 0 reads 0.0 — no modulation — instead of a 0.5 that has to be
            // mentally converted. AMP converts back, once, at its own site.
            //
            // Triangle is quarter-cycle advanced so it STARTS AT THE CENTRE and rises,
            // exactly as the sine does. The LFO is per-voice and reset to phase 0 at
            // every note-on, so an un-shifted triangle began each note pinned at its
            // negative extreme, jumping the target to the bottom of its range before the
            // sweep even started. Cycle is now centre -> peak -> centre -> trough.
            //
            // Saw and square are deliberately NOT shifted. A saw's identity is the ramp
            // from one end to the other, and a square has only two states with no centre
            // to start from; for both, beginning at an extreme is the shape, not a defect.
            case 1: { const MYFLOAT p = ph >= 0.75 ? ph - 0.75 : ph + 0.25;
                      value = p < 0.5 ? 4.*p - 1. : 3. - 4.*p; } break;       // triangle -1..1
            case 2: value = 2.*ph - 1.; break;                                // saw -1..1
            case 3: value = ph < 0.5 ? 1. : -1.; break;                       // square -1..1
            default: value = sin(ph * 6.28318530718); break;                  // sine -1..1
        }
        return value;
    }
};

// Per-voice noise generator. mode: -1 off, 0 white, 1 pink, 2 brown — matches
// the NOISEMODE param values {-1,0,1,2}. The three coloured generators keep
// their own filter state; only the selected one is ticked. Summed into the
// voice pre-filter so the Moog ladder shapes it (wind/breath) rather than
// sitting on top as broadband hiss.
struct NoiseSource {
    int mode{-1};
    WhiteNoise<MYFLOAT>    white;
    PinkingFilter<MYFLOAT> pink;
    BrowningFilter<MYFLOAT> brown;
    inline MYFLOAT tick() {
        switch (mode) {
            case 0:  return white.tick();
            case 1:  return pink.tick();
            case 2:  return brown.tick();
            default: return 0.0;
        }
    }
};

// Modal bank params for oscillator o (0/1 = OSC1/2, 2 = SUB), read from the
// per-block synth_params snapshot. No warm-up sibling: unlike PADsynth there is
// nothing to build, so note-on can lay the bank out in full with no first-note
// penalty. DECAY is a Log10 param, so refreshSynthParams has already decoded it
// into seconds — do NOT decode it again here.
static ModalOscParams modalParamsFromSnapshot(tsl::AppState* _appState, int o) {
    static const int chP[3]  = {VCO1MODALCH,  VCO2MODALCH,  VCO3MODALCH};
    static const int decP[3] = {VCO1MODALDEC, VCO2MODALDEC, VCO3MODALDEC};
    static const int brtP[3] = {VCO1MODALBRT, VCO2MODALBRT, VCO3MODALBRT};
    static const int hrdP[3] = {VCO1MODALHRD, VCO2MODALHRD, VCO3MODALHRD};
    static const int posP[3] = {VCO1MODALPOS, VCO2MODALPOS, VCO3MODALPOS};
    const auto& sp = _appState->data->synth_params;
    ModalOscParams p;
    p.character = (int)std::lround(sp[chP[o]]);
    p.decaySec  = sp[decP[o]];
    p.bright    = sp[brtP[o]];
    p.hardness  = sp[hrdP[o]];
    p.position  = sp[posP[o]];
    return p;
}

// Just enough attack to turn the initial-condition step into a strike instead of a
// click. The musical attack is the amp EG's job — this one is de-clicking, which is
// why it is a constant and not a param.
static constexpr double MODAL_STRIKE_ATTACK = 0.0015;

// Vibrato range at LFO DEPTH 1, in semitones either side of the note. DEPTH is the
// only control the LFO has, so this sets what the top of that fader means; at 1.0 a
// 0.01 depth step is 1 cent, which is a usable resolution for subtle vibrato.
static constexpr double LFO_PITCH_SEMITONES = 1.0;

// Per-voice seed for the bank's per-partial amplitude scatter. Same idiom as
// padNoteOn's: without a fresh seed every voice gets an identical timbre and the
// scatter stops doing the one job it exists for.
static inline uint32_t nextModalSeed() {
    static std::atomic<uint32_t> ctr{0x9E3779B9u};
    return ctr.fetch_add(0x9E3779B9u, std::memory_order_relaxed) | 1u;
}

// Which parameter ids carry oscillator o's MORPH A / B / EG source. PAD has its own
// set; every other type uses the wavetable oscillator's. They used to be shared, so an
// osc switched between WT and PAD dragged its morph range across and a patch could not
// hold a sensible morph for each. Centralised here because FOUR places read them: the
// per-block morph ramp, the two note-on _livemorph seeds, and padParamsFromSnapshot.
struct MorphIds { int a, b, eg; };
static inline MorphIds morphIdsFor(int o, int waveform) {
    static const MorphIds wt[3] = {{VCO1WTPOS, VCO1MORPHTO, VCO1PWMODSRC},
                                   {VCO2WTPOS, VCO2MORPHTO, VCO2PWMODSRC},
                                   {VCO3WTPOS, VCO3MORPHTO, VCO3PWMODSRC}};
#if PA_ENABLE_PAD
    static const MorphIds pad[3] = {{VCO1PADPOS, VCO1PADMTO, VCO1PADMEG},
                                    {VCO2PADPOS, VCO2PADMTO, VCO2PADMEG},
                                    {VCO3PADPOS, VCO3PADMTO, VCO3PADMEG}};
    if (waveform == 98) return pad[o];
#else
    (void)waveform;
#endif
    return wt[o];
}

#if PA_ENABLE_PAD
// PADsynth build-time params for oscillator o (0/1 = OSC1/2, 2 = SUB), read from
// the per-block synth_params snapshot rather than params[0] directly. Free function
// because both the voice (at note-on) and the block loop (to warm the build before
// the first note) need it.
static PadParams padParamsFromSnapshot(tsl::AppState* _appState, int o) {
    static const int selP[3]  = {VCO1PADSEL,   VCO2PADSEL,   VCO3PADSEL};
    // VCOxPADBW / PADBWSC / PADSTR / PADSEED are NO LONGER READ — see the note on
    // PAD_FIXED_BW in vco.h. The parameters still exist and must stay where they are
    // (ids are positions); this is the one place that used to consult them.
    // MORPH A and B. PAD shares these two params with the WT oscillator, and as there
    // they are BUILD inputs, not runtime ones — the baked levels span A→B. Read the
    // KNOBS here; the modulated position between them rides pw into padRead instead.
    const MorphIds mid = morphIdsFor(o, 98);   // PAD's own MORPH A/B, not the WT pair
    const auto& sp = _appState->data->synth_params;
    PadParams p;
    p.table     = (int)std::lround(sp[selP[o]]);
    // BANDW is a 0..1 knob; the cents it means depend on the table AND on BW SCL (see
    // PAD_BW_NID). Still resolved HERE rather than in buildPadRegion so the cache key,
    // the build and the whole read path keep working in cents and know nothing about
    // the mapping — the mapping is per-table even though the knob is now a constant.
    p.bwScale   = PAD_FIXED_BWSCALE;
    p.bandwidth = padBandwidthCents(p.table, PAD_FIXED_BW, p.bwScale);
    p.stretch   = PAD_FIXED_STRETCH;
    p.seed      = PAD_FIXED_SEED;
    p.mA        = padQuantMorph((float)sp[mid.a]);
    p.mB        = padQuantMorph((float)sp[mid.b]);
    // The single construction site, so this is the one place the snap has to happen.
    // Without it a sub-quantum drift in BANDW / BW SCL / STRETCH builds a duplicate set
    // that the lookup cannot see — see padSnapParams.
    padSnapParams(p);
    return p;
}

// A PADsynth set takes ~150 ms to build, so waiting for note-on to ask would make
// the first note silent. Called every block, whether or not a voice is running;
// padWarmRequest is a no-op once the set is cached or already building.
static void warmPadSets(tsl::AppState* _appState) {
    static const int typeP[3]  = {VCO1TYPE,  VCO2TYPE,  VCO3TYPE};
    // The PAD page's subsection index per oscillator: VCO3 has the extra MOD tab, so
    // its PAD entry sits one later (vco3subnames in gui.cpp).
    static const int spaceP[3] = {VCO1SPACE, VCO2SPACE, VCO3SPACE};
    static const int padSpc[3] = {5, 5, 6};
    const auto& sp = _appState->data->synth_params;
    for (int o = 0; o < 3; o++) {
        // Warm + publish when the oscillator is ON the PAD engine, and also while its
        // PAD page is the visible subsection: the scope on that page has to show the
        // selected table no matter what the oscillator is currently playing —
        // otherwise the page reads as having no display at all.
        // The page-open case costs at most one set build, on demand, and the LRU's
        // touch-on-hit keeps sounding sets ahead of a browsed one in eviction order.
        if ((int)sp[typeP[o]] != 98 && (int)sp[spaceP[o]] != padSpc[o]) continue;
        const PadParams p = padParamsFromSnapshot(_appState, o);
        padWarmRequest(_appState, p, (double)_appState->sr);
        // Tell the PAD display which set this oscillator is actually on — here
        // for the same reason warmWtSets publishes: the picture has to be right
        // on a SILENT instrument too, and this key is byte-identical to the one
        // the warm above requests, so the two cannot disagree.
        padPublishDisplay(o, p, (double)_appState->sr);
    }
}
#else
// PADsynth parked (see PA_ENABLE_PAD in types_pocketanalog.h): its params live past
// NUM_PARAMS, i.e. outside synth_params, so nothing may read them.
static inline void warmPadSets(tsl::AppState*) {}
#endif

// The same warm-up for wavetables, and for the same reason — a set that isn't cached
// can only be built off-thread, so whoever asks first pays a note for it.
//
// Without this the first ask happened inside refreshWt(), which only runs from a live
// oscillator, i.e. at note-on. The miss does not fall silent: the voice keeps the set
// it is already holding, so the first note after a preset change played the OUTGOING
// preset's table (or the default one a recycled voice had just picked up) and only
// crossed over when the build landed, mid-note. Every later note was correct, because
// by then the set was in the warm LRU. That asymmetry is what made it read as "the
// first note is still the old preset".
//
// Keyed off params[0] rather than synth_params because that is what tick()'s rangeOf
// uses to build _range. The warm only helps if the key is byte-identical to the one
// refreshWt() will look up, and a value that has been through a decode is not.
static void warmWtSets(tsl::AppState* _appState) {
    static const int typeP[3]    = {VCO1TYPE,    VCO2TYPE,    VCO3TYPE};
    static const int selP[3]     = {VCO1WTSEL,   VCO2WTSEL,   VCO3WTSEL};
    static const int warpTypeP[3]= {VCO1WARPTYPE,VCO2WARPTYPE,VCO3WARPTYPE};
    static const int wtposP[3]   = {VCO1WTPOS,   VCO2WTPOS,   VCO3WTPOS};
    static const int morphToP[3] = {VCO1MORPHTO, VCO2MORPHTO, VCO3MORPHTO};
    static const int warpAmtP[3] = {VCO1WARPAMT, VCO2WARPAMT, VCO3WARPAMT};
    static const int warpToP[3]  = {VCO1WARPTO,  VCO2WARPTO,  VCO3WARPTO};
    auto& p = _appState->params[0];
    for (int o = 0; o < 3; o++) {
        if ((int)p[typeP[o]].load() != 99) continue;   // WT oscillators only
        const WtRange r{ (float)p[wtposP[o]].load(),   (float)p[morphToP[o]].load(),
                         (float)p[warpAmtP[o]].load(), (float)p[warpToP[o]].load() };
        const int sel = (int)p[selP[o]].load(), wt = (int)p[warpTypeP[o]].load();
        wtWarmRequest(_appState, sel, wt, r);
        // Tell the display which set this oscillator is actually on. Here rather than
        // at note-on because the picture has to be right on a SILENT instrument too --
        // nothing adopts a set while no voice is running, so a publish hung off
        // adoption would leave the display showing the last note played. Keyed off the
        // same params[0] read the warm above uses, so the two cannot disagree.
        wtPublishDisplay(o, sel, wt, r);
    }
}

struct VcoNote : private Phaser,HuovilainenMoog,RingModFast<MYFLOAT> {
    VcoNote() = default;

    VcoNote(MYFLOAT freq, int64_t time, MYFLOAT vel) {
        init(freq, time, vel);
    }

    VcoNote &operator=(VcoPreNote &o) {
        _freq = o._finalfreq;
        _time = o._finaltime;
        _gain = o._gain;
        _pws[0] = o._pws[0]; _livepws[0] = o._pws[0]; _pwinc[0] = 0.;
        _pws[1] = o._pws[1]; _livepws[1] = o._pws[1]; _pwinc[1] = 0.;
        _pws[2] = o._pws[2]; _livepws[2] = o._pws[2]; _pwinc[2] = 0.;
        for (int i = 0; i < 4; i++) { _liveGainFact[i] = 1.; _gainFactInc[i] = 0.; }
        _seedLive = true;   // first block snaps these to the real duck targets
        // WTPOS isn't stored in the frozen VcoPreNote layout — read live. Seeded
        // from o.waveform, NOT this->waveform: the node was just wiped by _alloc,
        // so the member is still 0 here and a PAD oscillator would seed its morph
        // from the WT knob — the exact cross-type leak morphIdsFor exists to stop.
        for (int o2 = 0; o2 < 3; o2++) {
            _livemorph[o2] = _appState->data->synth_params[morphIdsFor(o2, o.waveform[o2]).a];
            _morphinc[o2] = 0.;
        }
        gains[0] = o.gains[0];
        gains[1] = o.gains[1];
        gains[2] = o.gains[2];
        gains[3] = o.gains[3];
        gains[4] = o.gains[4];
        _velnorm = o._velnorm;
        _atSnapshot = o._atSnapshot;
        _noteNum = o._noteNum;
        egs[0] = o.egs[0];
        egs[1] = o.egs[1];
        egs[2] = o.egs[2];
        egs[3] = o.egs[3];
        // NOISEMODE is a single global timbre param (not stored per-note in
        // VcoPreNote, whose layout is frozen by the preset file format), so
        // read it live from the current synth params.
        _noise.mode = (int) _appState->data->synth_params[NOISEMODE];
        egs[4] = o.egs[4];
        egs[5] = o.egs[5];
        egs[6] = o.egs[6];
        egs[7] = o.egs[7];
        // egs[8] is the RESONANCE EG source, and VcoPreNote::egs only has EIGHT slots
        // -- its layout is frozen by the preset file format (notes are fwrite'd raw),
        // so RESEG never had anywhere to live there. Copying o.egs[8] read one past
        // the end of that array. Read live instead, like WTPOS and NOISEMODE above.
        egs[8] = (int) _appState->data->synth_params[RESEG];
        deriveEgRouting();
        // Belt and braces: FastQueue::_alloc assigns a default-constructed VcoNote over
        // the recycled node, so the gate is already down. A stuck note is expensive
        // enough to have shipped twice that it is worth not depending on that.
        _keyUp = false;
        _tune1 = o._tune1;
        _tune2 = o._tune2;
        _tunenormal = o._tunenormal;
        waveform[0] = o.waveform[0];
        waveform[1] = o.waveform[1];
        waveform[2] = o.waveform[2];
        _doringmod = o._doringmod;
        _donormal = o._donormal;
        // AM/PM (and their depths) are global SUB routing params not carried in
        // the frozen VcoPreNote layout — read live like NOISEMODE.
        _doam = _appState->data->synth_params[VCO3AM] == 1.;
        _dopm = _appState->data->synth_params[VCO3PM] == 1.;
        _amdepth = _appState->data->synth_params[VCO3AMDEPTH];
        _pmdepth = _appState->data->synth_params[VCO3PMDEPTH];
        _sync[0] = o._sync[0];
        _sync[1] = o._sync[1];
        _sync[2] = o._sync[2];

        for (auto& a : adsr) a._onedksr = _appState->onedksr;
        jitter0._ksr = jitter1._ksr = jitternormal._ksr = _appState->ksr;
        RingModFast<MYFLOAT>::_appState = _appState;
        Phaser::_appState = _appState;
        vcos[0]._appState = vcos[1]._appState = vconormal._appState = _pmSine._appState = _appState;
        adsr[0].setAll(o.egvals[0], o.egvals[1],
                       o.egvals[2], o.egvals[3]);
        adsr[1].setAll(o.egvals[4], o.egvals[5],
                       o.egvals[6], o.egvals[7]);
        adsr[2].setAll(o.egvals[8], o.egvals[9],
                       o.egvals[10], o.egvals[11]);
        adsr[3].setAll(o.egvals[12], o.egvals[13],
                       o.egvals[14], o.egvals[15]);
        jitter0.update(o._jittercents, o._jittera,
                       o._jitterb);
        jitter1.update(o._jittercents, o._jittera,
                       o._jitterb);

        count = 0;
        egtmp[0] = egtmp[1] = egtmp[2] = egtmp[3] = 0.;
        // The unenveloped slot ramps 0 -> 1 rather than starting there, so a source
        // with no envelope gets the same short fade at the START of the note that
        // isDead() gives it at the end. Living in egtmp means only the unenveloped
        // sources are affected -- a zero-attack EG beside one keeps its hard edge --
        // and it is free, since the multiply by egtmp is already in the sample loop.
        egtmp[EG_NONE] = 0.;
        _noneFadeInc = (MYFLOAT)((double)_appState->onedsr / STEAL_FADE_S);
        _cps = _freq * _appState->onedsr;
        _phase = 0;
        // ~3 ms one-pole for the amp-gain follower, expressed against the real sample
        // rate so it stays 3 ms at 44.1k and 96k. The seed below is a stale no-op kept
        // harmlessly: _atAmpGain is still at its wiped default here, and the real seed
        // is the first-block _seedLive snap in tick(), which runs before any sample.
        _ampSlew = (MYFLOAT)(1.0 - std::exp(-(double)_appState->onedsr / 0.003));
        _atAmpSmooth = _atAmpGain;
        resetFilters();

        aimWavetables();   // must precede check()/selectWavetable() -- see the note there
        vcos[0].check(waveform[0]);
        vcos[1].check(waveform[1]);
        vcos[0].phs = vcos[1].phs = 0;
        if (waveform[0] == 99) vcos[0].selectWavetable((int)_appState->data->synth_params[VCO1WTSEL]);
        if (waveform[1] == 99) vcos[1].selectWavetable((int)_appState->data->synth_params[VCO2WTSEL]);
        // PADsynth reads a seconds-long sample, so note-on must re-roll its read
        // offset (otherwise every voice enters at sample 0 and they correlate) and
        // unlatch the key region so this note picks the one for its own pitch.
#if PA_ENABLE_PAD
        if (waveform[0] == 98) { vcos[0].setPad(padParamsOf(0)); vcos[0].padNoteOn(); }
        if (waveform[1] == 98) { vcos[1].setPad(padParamsOf(1)); vcos[1].padNoteOn(); }
#endif
        // Modal: note-on lays the bank out (partial count and ratios depend on pitch,
        // via the Nyquist cull) and strikes it. _freq is the note's base Hz, not this
        // oscillator's tuned frequency — COARSE/FINE/jitter arrive within one
        // MODAL_RETUNE_INTERVAL through tick()'s block-rate retune, and the only
        // thing the base is used for here is how many partials fit under Nyquist.
        if (waveform[0] == 97) {
            vcos[0].setModal(modalParamsOf(0));
            vcos[0].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        if (waveform[1] == 97) {
            vcos[1].setModal(modalParamsOf(1));
            vcos[1].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        {   // spread unison phases so the detuned copies don't start aligned (per osc)
            auto& sp = _appState->data->synth_params;
            vcos[0].spreadUnison((int)std::lround(sp[VCO1UNIVOICES]));
            vcos[1].spreadUnison((int)std::lround(sp[VCO2UNIVOICES]));
            vconormal.spreadUnison((int)std::lround(sp[VCO3UNIVOICES]));
        }
        // vconormal drives both the NORMAL (added) output and the PM modulator;
        // RingModFast drives RINGMOD and AM. jitternormal feeds the SUB pitch
        // whenever any SUB routing is active.
        if (_donormal || _doringmod || _doam || _dopm) {
            jitternormal.update(o._jittercents, o._jittera, o._jitterb);
        }
        if (_donormal || _dopm) {
            vconormal.check(waveform[2]);
            vconormal.phs = 0;
            if (waveform[2] == 99) vconormal.selectWavetable((int)_appState->data->synth_params[VCO3WTSEL]);
#if PA_ENABLE_PAD
            if (waveform[2] == 98) { vconormal.setPad(padParamsOf(2)); vconormal.padNoteOn(); }
#endif
            if (waveform[2] == 97) {
                vconormal.setModal(modalParamsOf(2));
                vconormal.modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
            }
        }
        if (_dopm) {
            _pmSine.check(-1);   // sine modulator for clean FM sidebands
            _pmSine.phs = 0;
            _pmCursor = 0;
            for (int i = 0; i < numtaps * 2; i++) _pmDown[i] = 0;
        }
        if (_doringmod || _doam) {
            initRingModulator();
        }

        if(o._phaseractive){
            Phaser::init(_freq, o.phaserrange, o.phaserfb, o.phaserrate);
        }
        else Phaser::active = false;

        return *this;
    }

    void applyAT(float pressure) {
        _atSnapshot = pressure;
        const MYFLOAT at = pressure / 127.;
        auto& p = _appState->params[0];
        const MYFLOAT cut     = p[FILT_CUT].load();
        const MYFLOAT baseHz  = filtBaseHz();
        const MYFLOAT base    = baseHz * _appState->onedsr;
        const MYFLOAT rest    = (0.5 - base) * cut;
        const MYFLOAT velFact = 1. - p[VEL_TO_FILT].load() * (1. - _velnorm);
        const MYFLOAT atFact  = 1. - p[AT_TO_FILT].load()  * (1. - at);
        const MYFLOAT mwFact  = 1. - p[MW_TO_FILT].load()  * (1. - _appState->data->modwheel.load(std::memory_order_relaxed) / 127.);
        const MYFLOAT filtEgVal = egs[4] == -1 ? 1. : egtmp[egs[4]];
        // No LFO term here: aftertouch recomputes between blocks and the block loop
        // puts the LFO back on the next pass. Clamped to FILTCENTER's own range, as
        // tick() does, so the two paths cannot land on different limits.
        gains[4] = base + rest * filtEgVal * velFact * atFact * mwFact;
        const MYFLOAT cutMinAT = 20. * _appState->onedsr;
        if (gains[4] > .5) gains[4] = .5;
        if (gains[4] < cutMinAT) gains[4] = cutMinAT;
        const MYFLOAT velResFact = 1. - p[VEL_TO_RES].load() * (1. - _velnorm);
        const MYFLOAT atResFact  = 1. - p[AT_TO_RES].load()  * (1. - at);
        const MYFLOAT mwResFact  = 1. - p[MW_TO_RES].load()  * (1. - _appState->data->modwheel.load(std::memory_order_relaxed) / 127.);
        const MYFLOAT resEgVal   = egs[8] == -1 ? 1. : egtmp[egs[8]];
        MYFLOAT liveRes = p[FILTRES].load() * velResFact * atResFact * mwResFact * resEgVal;
        if (liveRes < 0.) liveRes = 0.; else if (liveRes > 1.) liveRes = 1.;
        selectFiltMode((int)p[FILT_MODE].load());
        // Dispatch on the family, not on `>= FILTMODE_ZDF_FIRST`: the modal bank sits
        // above the ZDF block numerically but is not a ZDF filter, so the old
        // comparison would have handed mode 17 to setZdfParams.
        if (filtFamily(_filtMode) == 4) {
            setModalFilterParams(gains[4], liveRes, baseHz);
        } else if (_filtMode >= FILTMODE_ZDF_FIRST) {
            setZdfParams(_filtMode, gains[4], liveRes);
        } else {
            HuovilainenMoog::setParams(gains[4], liveRes);
            HuovilainenMoog::setMode(_filtMode);
        }
        // _atAmpGain is deliberately NOT recomputed here, unlike the filter above:
        // the block loop rebuilds it (with the AMP-LFO duck) within <=63 samples and
        // _atAmpSmooth hides that latency anyway. Writing it here without lfoAmpFact
        // made every pressure event blip a tremolo/gate toward un-ducked — a flutter
        // locked to the AT message rate, audible even with AT_TO_AMP at 0.
    }

    // Retriggers this already-live voice for a new note-on on the same pitch,
    // instead of allocating a second voice. Refreshes the new press's
    // velocity-derived fields, then soft-retriggers every envelope (no env
    // reset, see ADSR::retrigger) so there's no click. Oscillator phase and
    // tuning are left running, only the envelopes restart.
    //
    // EXCEPT for a MODAL oscillator, which has to be struck again. Leaving the
    // oscillator running is the right call for every type that free-runs — a saw, a
    // wavetable, a PAD read — because the amp EG restarting is what makes the new
    // note. A modal bank is not like that: its entire sound is the initial condition
    // that excite() writes, and by the second press the bank has already decayed. Only
    // restarting the envelope re-opened a VCA over a ring that was no longer there, so
    // repeating a pitch produced near-silence. Mirrors the note-on path exactly,
    // including the fresh seed — without one, every repeat of a note would get an
    // identical partial scatter, which is the thing the seed exists to prevent.
    void retrigger(MYFLOAT vel) {
        _velnorm = vel / 127.;
        const MYFLOAT velAmt = _appState->params[0][VEL_TO_AMP].load();
        _gain = velAmt > 0. ? pow(_velnorm, velAmt * 3.) : 1.;
        // The key is down again. Without this an unenveloped source would be reaped on
        // the next sample, because the gate is what ends it and it is still raised.
        _keyUp = false;
        for (auto& a : adsr) a.retrigger();
        if (waveform[0] == 97) {
            vcos[0].setModal(modalParamsOf(0));
            vcos[0].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        if (waveform[1] == 97) {
            vcos[1].setModal(modalParamsOf(1));
            vcos[1].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        if ((_donormal || _dopm) && waveform[2] == 97) {
            vconormal.setModal(modalParamsOf(2));
            vconormal.modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        // Same for the RINGMOD/AM modulator's bank — without a fresh strike a
        // retriggered ring-modal voice rides a ring that has already decayed away.
        if ((_doringmod || _doam) && waveform[2] == 97) {
            auto& rv = RingModFast<MYFLOAT>::modVco();
            rv.setModal(modalParamsOf(2));
            rv.modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
    }

    // ── amp EG routing, including NONE ───────────────────────────────────────────
    // egs[0..3] are the amp EG sources for VCO1, VCO2, SUB and NOISE, and each may be
    // -1 = NONE: that source is not shaped by an envelope and sits at its GAIN for as
    // long as the voice lives. -1 is not an adsr[] index and testing for it four times
    // per sample per voice would be pure waste, so the three things that care about it
    // are precomputed here, once per note.
    //
    //   _ampIdx  what the sample loop multiplies by. NONE routes to egtmp[EG_NONE],
    //            which ramps 0 -> 1 over ~3 ms at note-on and then holds 1.0 — the
    //            same "no envelope means factor 1" convention FILTEG and the TUNE
    //            EGs use, reached without a branch.
    //
    //   _lifeEg  the DISTINCT EGs, at most two, that decide when the voice ends — VCO1's
    //   _lifeN   and VCO2's. Only those two ever did; the sub and the noise never got a
    //            vote. An unenveloped one is simply left out, so with both NONE the list
    //            is EMPTY and no envelope has any say in this voice's lifetime.
    //
    //   _ampNone whether ANY of the four is unenveloped, i.e. whether this voice is still
    //            making sound once the envelopes are finished. That is the one case where
    //            the envelopes have NOT already faded the voice out, so isDead() has to
    //            wait for the key and then ramp before it frees the slot.
    void deriveEgRouting() {
        for (int i = 0; i < 4; i++) _ampIdx[i] = egs[i] < 0 ? EG_NONE : egs[i];
        _lifeN = 0;
        if (egs[0] >= 0)                     _lifeEg[_lifeN++] = egs[0];
        if (egs[1] >= 0 && egs[1] != egs[0]) _lifeEg[_lifeN++] = egs[1];
        _ampNone = egs[0] < 0 || egs[1] < 0 || egs[2] < 0 || egs[3] < 0;
    }

    // Vacuously true when nothing is routed, which is what leaves an all-NONE voice
    // depending on the key alone.
    bool envDone() const {
        for (int i = 0; i < _lifeN; i++)
            if (adsr[_lifeEg[i]].state != ADSR::DEAD) return false;
        return true;
    }

    // Every keyOff in the synth goes through here rather than looping over adsr at the
    // call site, because an unenveloped amp source has no envelope to run out — the KEY
    // is what ends it, and isDead() has no other way to know the key came up.
    void gateOff() {
        _keyUp = true;
        for (auto& a : adsr) a.keyOff();
    }

    // A fade-out is armed: this voice is milliseconds from being recycled, whether it was
    // stolen or is an unenveloped one whose key came up. Anything that would prolong the
    // voice has to check this — see the retrigger search in play2.
    bool fading() const { return _stealFadeInc > 0.; }

    bool isDead() {
        // A ramp is running — a steal, or the end-of-life ramp below. Either way the
        // voice is done when the ramp reaches true zero and not before: reaching zero
        // is the entire reason the ramp exists.
        if (fading()) return _stealFade <= 0.;
        if (!envDone()) return false;
        // With every source enveloped, the envelopes have already taken the voice to
        // silence and the slot can go back now. Otherwise:
        if (_ampNone) {
            // An unenveloped source has no release to run out, so the key is what ends
            // it. Testing the envelopes instead would cut a held drone short the moment
            // a percussive EG on the OTHER oscillator hit its floor — a sustain of 0
            // reaches DEAD during DECAY, with the key still down.
            if (!_keyUp) return false;
            // Still at full level, so ramp before handing the slot back. The unenveloped
            // source therefore outlives the enveloped one rather than being gated off
            // underneath it: a voice ends when its longest part does, and the part with
            // no envelope has no length of its own to contribute.
            armFade();
            return false;
        }
        return true;
    }

    // ── voice stealing ───────────────────────────────────────────────────────────
    // _lifeEg is exactly what isDead() waits on, so it is what a steal has to move.
    bool ampReleasing() const {
        if (_lifeN == 0) return _keyUp;   // nothing enveloped: the key is all there is
        for (int i = 0; i < _lifeN; i++)
            if (adsr[_lifeEg[i]].state < ADSR::RELEASE) return false;
        return true;
    }
    // How loud this voice still is, for choosing the least audible victim. The loudest of
    // the routed EGs, because either oscillator on its own is enough to be heard.
    MYFLOAT ampEnv() const {
        // An unenveloped source holds full level for the whole life of the voice, so
        // there is no envelope to read: such a voice is always the loudest candidate,
        // which makes it the last one taken. Right answer for the wrong-sounding reason
        // — it is not that it is loud, it is that nothing about it is decaying.
        if (_ampNone) return 1.;
        MYFLOAT e = 0.;
        for (int i = 0; i < _lifeN; i++)
            if (adsr[_lifeEg[i]].env > e) e = adsr[_lifeEg[i]].env;
        return e;
    }
    // Fade this voice out over STEAL_FADE_S so its slot comes free. It is NOT torn down
    // here: the reap in play2 then finds it dead and runs releaseTables() and del() the
    // way it does for any finished voice, which is the whole reason the steal is
    // deferred rather than done in place — see the note on SynthQueue::stealVoice.
    //
    // A DEDICATED SAMPLE-RATE RAMP, NOT THE AMP ENVELOPE. Two reasons, both learned the
    // hard way:
    //   - The envelopes run at ksr = sr/64, so a release fast enough to be useful for a
    //     steal is past the point where that discrete one-pole stays monotonic. Forcing
    //     one made every stolen voice flip polarity two or three times on the way down.
    //     See the note at the top of ADSR.
    //   - ADSR::tick calls a voice DEAD at env <= 0.01, so an envelope-driven steal
    //     hands back the slot while the voice is still at 1% — a -40 dB step, from full
    //     sustain, on every steal. This ramp reaches exactly zero before isDead() does.
    void beginSteal() { armFade(); _stealing = true; }
    // The ramp on its own, without claiming the voice was stolen. isDead() uses it for
    // an unenveloped source, which needs the same fade for the same reason but must
    // stay a legal steal victim — _stealing is what excludes a voice from that.
    void armFade() {
        const double sr = _appState ? (double)_appState->sr : 48000.0;
        _stealFadeInc = (MYFLOAT)(1.0 / (STEAL_FADE_S * sr));
    }

    // Hand the shared wavetable / PADsynth sets back before this voice returns to the
    // pool. FastQueue recycles VcoNote objects and only wipes them in _alloc() on the
    // NEXT note-on, so a finished voice would otherwise keep its sets resident until
    // its slot happens to be reused (ten voices x three oscillators can pin ten
    // different ~12 MB sets), and that wipe would then run the whole deallocation
    // inside the note-on callback. Vco::releaseTables defers the destructor to the
    // UiTasks worker.
    void releaseTables() {
        vcos[0].releaseTables();
        vcos[1].releaseTables();
        vconormal.releaseTables();
        // The RINGMOD/AM modulator is a fourth Vco and can hold WT/PAD sets too.
        RingModFast<MYFLOAT>::modVco().releaseTables();
    }

    // Note-on setup for the RINGMOD/AM modulator, mirroring the vconormal block: a
    // MODAL modulator must be laid out and struck (check() alone leaves the bank
    // silent, and a silent modulator multiplies the entire voice to nothing), and a
    // WT/PAD modulator should play the SUB's selected set, not the default. The
    // modulator has no aim/setRange pass, so a WT set is read over its full unwarped
    // range — the live morph position (passed into tick per sample) still applies.
    void initRingModulator() {
        RingModFast<MYFLOAT>::init(waveform[2]);
        auto& rv = RingModFast<MYFLOAT>::modVco();
        if (waveform[2] == 99) rv.selectWavetable((int)_appState->data->synth_params[VCO3WTSEL]);
#if PA_ENABLE_PAD
        if (waveform[2] == 98) { rv.setPad(padParamsOf(2)); rv.padNoteOn(); }
#endif
        if (waveform[2] == 97) {
            rv.setModal(modalParamsOf(2));
            rv.modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
    }

    void init(MYFLOAT freq, int64_t time, MYFLOAT vel, uint8_t noteNum = 0) {
        auto& params = _appState->data->synth_params;
        vcos[0]._appState = vcos[1]._appState = vconormal._appState = _pmSine._appState = _appState;
        RingModFast<MYFLOAT>::_appState = _appState;
        Phaser::_appState = _appState;
        for (auto& a : adsr) a._onedksr = _appState->onedksr;
        jitter0._ksr = jitter1._ksr = jitternormal._ksr = _appState->ksr;
        _freq = freq;
        _time = time;
        _noteNum = noteNum;
        _fromSequencer = false;
        _atSnapshot = 0.f;
        _lfo1.reset(); _lfo2.reset(); _lfo3.reset(); _lfo4.reset();
        _lfo1val = 0.; _lfo2val = 0.; _lfo3val = 0.; _lfo4val = 0.; _lfoStrikeOff = 0.;
        _lfo1RateHz = params[LFO1RATE]; _lfo2RateHz = params[LFO2RATE]; // already decoded (paramCurve==Log10)
        _lfo3RateHz = params[LFO3RATE]; _lfo4RateHz = params[LFO4RATE];
        _velnorm = vel / 127.;
        const MYFLOAT velAmt = _appState->params[0][VEL_TO_AMP].load();
        _gain = velAmt > 0. ? pow(_velnorm, velAmt * 3.) : 1.;
        _pws[0] = 0.5 - 0.45 * params[VCO1PW];
        _pws[1] = 0.5 - 0.45 * params[VCO2PW];
        _pws[2] = 0.5 - 0.45 * params[VCO3PW];
        _livepws[0] = _pws[0]; _livepws[1] = _pws[1]; _livepws[2] = _pws[2];
        _pwinc[0] = _pwinc[1] = _pwinc[2] = 0.;
        for (int i = 0; i < 4; i++) { _liveGainFact[i] = 1.; _gainFactInc[i] = 0.; }
        _seedLive = true;   // first block snaps these to the real duck targets
        // _livemorph is seeded AFTER the waveform[] assignments below — seeding from
        // the stale (wiped) waveform made a PAD oscillator take its morph from the
        // WT knob for its first block.
        gains[0] = LOG2NORMALF(params[VCO1GAIN]);
        gains[1] = LOG2NORMALF(params[VCO2GAIN]);
        gains[2] = LOG2NORMALF(params[VCO3GAIN]);
        gains[3] = LOG2NORMALF(params[NOISEGAIN]);
        gains[4] = 0.;
        _noise.mode = (int) params[NOISEMODE];
        egs[0] = (int) params[VCO1EG];
        egs[1] = (int) params[VCO2EG];
        egs[2] = (int) params[VCO3EG];
        egs[3] = (int) params[NOISEEG];
        egs[4] = (int) params[FILTEG];
        egs[5] = (int) params[VCO1TUNEEG];
        egs[6] = (int) params[VCO2TUNEEG];
        egs[7] = (int) params[VCO3TUNEEG];
        egs[8] = (int) params[RESEG];
        deriveEgRouting();
        _keyUp = false;   // see the note in the VcoPreNote path

        _tune1 = params[VCO1COARSE] / 12. + params[VCO1FINE] / 1200.;
        _tune2 = params[VCO2COARSE] / 12. + params[VCO2FINE] / 1200.;

        waveform[0] = (int) params[VCO1TYPE];
        waveform[1] = (int) params[VCO2TYPE];
        waveform[2] = (int) params[VCO3TYPE];
        for (int o = 0; o < 3; o++) _livemorph[o] = params[morphIdsFor(o, waveform[o]).a];
        _morphinc[0] = _morphinc[1] = _morphinc[2] = 0.;
        _doringmod = params[VCO3RINGMOD] == 1.;
        _donormal = params[VCO3NORMAL] == 1.;
        _doam = params[VCO3AM] == 1.;
        _dopm = params[VCO3PM] == 1.;
        _amdepth = params[VCO3AMDEPTH];
        _pmdepth = params[VCO3PMDEPTH];
        _sync[0] = params[VCO1SYNC] == 1.;
        _sync[1] = params[VCO2SYNC] == 1.;
        _sync[2] = params[VCO3SYNC] == 1.;

        adsr[0].setAll(params[EG1ATTACK], params[EG1DECAY],
                       params[EG1SUSTAIN], params[EG1RELEASE]);
        adsr[1].setAll(params[EG2ATTACK], params[EG2DECAY],
                       params[EG2SUSTAIN], params[EG2RELEASE]);
        adsr[2].setAll(params[EG3ATTACK], params[EG3DECAY],
                       params[EG3SUSTAIN], params[EG3RELEASE]);
        adsr[3].setAll(params[EG4ATTACK], params[EG4DECAY],
                       params[EG4SUSTAIN], params[EG4RELEASE]);
        jitter0.update(params[JITTERCENTS] / 1200., params[JITTERA],
                       params[JITTERB]);
        jitter1.update(params[JITTERCENTS] / 1200., params[JITTERA], params[JITTERB]);

        count = 0;
        egtmp[0] = egtmp[1] = egtmp[2] = egtmp[3] = 0.;
        // The unenveloped slot ramps 0 -> 1 rather than starting there, so a source
        // with no envelope gets the same short fade at the START of the note that
        // isDead() gives it at the end. Living in egtmp means only the unenveloped
        // sources are affected -- a zero-attack EG beside one keeps its hard edge --
        // and it is free, since the multiply by egtmp is already in the sample loop.
        egtmp[EG_NONE] = 0.;
        _noneFadeInc = (MYFLOAT)((double)_appState->onedsr / STEAL_FADE_S);
        _cps = _freq * _appState->onedsr;
        _phase = 0;
        // ~3 ms one-pole for the amp-gain follower, expressed against the real sample
        // rate so it stays 3 ms at 44.1k and 96k. The seed below is a stale no-op kept
        // harmlessly: _atAmpGain is still at its wiped default here, and the real seed
        // is the first-block _seedLive snap in tick(), which runs before any sample.
        _ampSlew = (MYFLOAT)(1.0 - std::exp(-(double)_appState->onedsr / 0.003));
        _atAmpSmooth = _atAmpGain;
        resetFilters();
        aimWavetables();   // must precede check()/selectWavetable() -- see the note there
        vcos[0].check(waveform[0]);
        vcos[1].check(waveform[1]);
        vcos[0].phs = vcos[1].phs = 0;
        if (waveform[0] == 99) vcos[0].selectWavetable((int)_appState->data->synth_params[VCO1WTSEL]);
        if (waveform[1] == 99) vcos[1].selectWavetable((int)_appState->data->synth_params[VCO2WTSEL]);
        // PADsynth reads a seconds-long sample, so note-on must re-roll its read
        // offset (otherwise every voice enters at sample 0 and they correlate) and
        // unlatch the key region so this note picks the one for its own pitch.
#if PA_ENABLE_PAD
        if (waveform[0] == 98) { vcos[0].setPad(padParamsOf(0)); vcos[0].padNoteOn(); }
        if (waveform[1] == 98) { vcos[1].setPad(padParamsOf(1)); vcos[1].padNoteOn(); }
#endif
        // Modal: note-on lays the bank out (partial count and ratios depend on pitch,
        // via the Nyquist cull) and strikes it. _freq is the note's base Hz, not this
        // oscillator's tuned frequency — COARSE/FINE/jitter arrive within one
        // MODAL_RETUNE_INTERVAL through tick()'s block-rate retune, and the only
        // thing the base is used for here is how many partials fit under Nyquist.
        if (waveform[0] == 97) {
            vcos[0].setModal(modalParamsOf(0));
            vcos[0].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        if (waveform[1] == 97) {
            vcos[1].setModal(modalParamsOf(1));
            vcos[1].modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
        }
        {   // spread unison phases so the detuned copies don't start aligned (per osc)
            auto& sp = _appState->data->synth_params;
            vcos[0].spreadUnison((int)std::lround(sp[VCO1UNIVOICES]));
            vcos[1].spreadUnison((int)std::lround(sp[VCO2UNIVOICES]));
            vconormal.spreadUnison((int)std::lround(sp[VCO3UNIVOICES]));
        }
        if (_donormal || _doringmod || _doam || _dopm) {
            _tunenormal = _appState->params[0][VCO3COARSEST].load() / 12. + params[VCO3FINE] / 1200.;
            jitternormal.update(params[JITTERCENTS] / 1200., params[JITTERA], params[JITTERB]);
        }
        if (_donormal || _dopm) {
            vconormal.check(waveform[2]);
            vconormal.phs = 0;
            if (waveform[2] == 99) vconormal.selectWavetable((int)_appState->data->synth_params[VCO3WTSEL]);
#if PA_ENABLE_PAD
            if (waveform[2] == 98) { vconormal.setPad(padParamsOf(2)); vconormal.padNoteOn(); }
#endif
            if (waveform[2] == 97) {
                vconormal.setModal(modalParamsOf(2));
                vconormal.modalNoteOn(_freq, MODAL_STRIKE_ATTACK, _velnorm, nextModalSeed());
            }
        }
        if (_dopm) {
            _pmSine.check(-1);   // sine modulator for clean FM sidebands
            _pmSine.phs = 0;
            _pmCursor = 0;
            for (int i = 0; i < numtaps * 2; i++) _pmDown[i] = 0;
        }
        if (_doringmod || _doam) {
            initRingModulator();
        }
        if(params[PHASERPOW] == 1.0){
            Phaser::init(_freq, params[PHASERRANGE], params[PHASERFB], params[PHASERRATE]);
        } else
            Phaser::active = false;
      //  Phaser::init(_freq, 3, 1, 1);
    }

    // 2:1 decimator for the oversampled PM carrier sum (windowed-sinc, same
    // coefficients as the ring-mod downsampler). s0/s1 are the two subsamples.
    MYFLOAT pmDecimate(MYFLOAT s0, MYFLOAT s1) {
        auto coeffs = _appState->data->coeffsDown.data();
        if (--_pmCursor < 0) _pmCursor = numtaps - 1;
        _pmDown[_pmCursor] = _pmDown[_pmCursor + numtaps] = s0;
        if (--_pmCursor < 0) _pmCursor = numtaps - 1;
        _pmDown[_pmCursor] = _pmDown[_pmCursor + numtaps] = s1;
        MYFLOAT sum = 0.0;
        auto x = &_pmDown[_pmCursor];
        for (int i = 0; i < (numtaps >> 2); i++) {
            sum += *x++ * *coeffs++;
            sum += *x++ * *coeffs++;
            sum += *x++ * *coeffs++;
            sum += *x++ * *coeffs++;
        }
        return sum;
    }

    // Aim the oscillators at the wavetable set this patch wants BEFORE note-on fires
    // its first lookup.
    //
    // A set is keyed by (table, warp type, A->B range). Until tick() runs setRange,
    // `_range` holds whatever the PREVIOUS note left, so check() and selectWavetable()
    // look up a key nothing has warmed, miss, and leave the voice sounding on the set
    // it is still holding. warmWtSets can't help there — it warms the key the patch
    // actually asks for, which is not the key note-on was requesting.
    //
    // Read from params[0], the same source tick()'s rangeOf and warmWtSets use, so all
    // three ask for a byte-identical key. aimWt sets table, warp type and range as one
    // step and looks up ONCE, so no half-updated intermediate key is ever requested;
    // the lookup is a small map probe, and a no-op while the osc is off mode 3.
    void aimWavetables() {
        auto& lp = _appState->params[0];
        static const int wtypeP[3] = {VCO1WARPTYPE, VCO2WARPTYPE, VCO3WARPTYPE};
        static const int wamtP[3]  = {VCO1WARPAMT,  VCO2WARPAMT,  VCO3WARPAMT};
        static const int posP[3]   = {VCO1WTPOS,    VCO2WTPOS,    VCO3WTPOS};
        static const int mtoP[3]   = {VCO1MORPHTO,  VCO2MORPHTO,  VCO3MORPHTO};
        static const int wtoP[3]   = {VCO1WARPTO,   VCO2WARPTO,   VCO3WARPTO};
        static const int selP[3]   = {VCO1WTSEL,    VCO2WTSEL,    VCO3WTSEL};
        Vco* v[3] = {&vcos[0], &vcos[1], &vconormal};
        for (int o = 0; o < 3; o++) {
            if (waveform[o] != 99) continue;   // WT oscillators only
            v[o]->aimWt((int)lp[selP[o]].load(), (int)lp[wtypeP[o]].load(),
                        (float)lp[wamtP[o]].load(),
                        WtRange{ (float)lp[posP[o]].load(),  (float)lp[mtoP[o]].load(),
                                 (float)lp[wamtP[o]].load(), (float)lp[wtoP[o]].load() });
        }
    }

    MYFLOAT tick() {
        auto& tukey = _appState->data->synth_tukey;
        auto& lparams = _appState->params[0];
        _lfo1val = _lfo1.tick(_lfo1RateHz, (int)lparams[LFO1WAVE].load(), _appState->sr,
                              lparams[LFO1PHASE].load() * (MYFLOAT)(1. / 360.));
        _lfo2val = _lfo2.tick(_lfo2RateHz, (int)lparams[LFO2WAVE].load(), _appState->sr,
                              lparams[LFO2PHASE].load() * (MYFLOAT)(1. / 360.));
        _lfo3val = _lfo3.tick(_lfo3RateHz, (int)lparams[LFO3WAVE].load(), _appState->sr,
                              lparams[LFO3PHASE].load() * (MYFLOAT)(1. / 360.));
        _lfo4val = _lfo4.tick(_lfo4RateHz, (int)lparams[LFO4WAVE].load(), _appState->sr,
                              lparams[LFO4PHASE].load() * (MYFLOAT)(1. / 360.));
        if (--count <= 0) {
            egtmp[0] = adsr[0].tick();
            egtmp[1] = adsr[1].tick();
            egtmp[2] = adsr[2].tick();
            egtmp[3] = adsr[3].tick();
            count = 64;
            Phaser::update();
            auto& params = _appState->data->synth_params;
            const MYFLOAT at = _atSnapshot / 127.;
            const MYFLOAT mw = _appState->data->modwheel.load(std::memory_order_relaxed) / 127.;

            // LFO modulation comes from the MULTI-DEST MATRIX (lfoMdId): one bipolar
            // depth per (LFO, dest), every slot live at once. The legacy
            // LFOnDEST/LFOnDEPTH pair is a UI window onto the selected slot and is
            // folded into the matrix at load time (lfoWindowTick / foldLegacyLfoRoutes)
            // — the engine reads ONLY the matrix, or the window would double-apply.
            // MW/AT -> LFO depth: same (1-route)+(route*val) factor as MW_TO_VIBRATO/AT_TO_VIBRATO
            const MYFLOAT mwLfoDepthFact = 1. - lparams[MW_TO_LFODEPTH].load() * (1. - mw);
            const MYFLOAT atLfoDepthFact = 1. - lparams[AT_TO_LFODEPTH].load() * (1. - at);
            const MYFLOAT mdScale = mwLfoDepthFact * atLfoDepthFact;
            const MYFLOAT lfoVals[4] = {_lfo1val, _lfo2val, _lfo3val, _lfo4val};
            // scaled slot depth — the routed excursion; raw (unscaled) reads stay in
            // lfoFeeds below, where "is anything routed here" must not depend on MW/AT.
            auto md = [&](int n, int dest) -> MYFLOAT {
                return lparams[lfoMdId(n, dest)].load() * mdScale;
            };
            // MW/AT -> LFO rate: additive shift in the same log domain the rate
            // param is stored in (not a linear Hz-domain scale), so the response
            // stays evenly spaced across the range like the rate knob itself.
            // Shifts from both sources add before decoding; clamped so a
            // fully-pressed dual route can't overshoot the parameter's own max.
            const MYFLOAT lfoRateRouteAmt = lparams[MW_TO_LFORATE].load() * mw + lparams[AT_TO_LFORATE].load() * at;
            const MYFLOAT lfoRateRouteAmtClamped = lfoRateRouteAmt > 1. ? 1. : lfoRateRouteAmt;
            const MYFLOAT rate1InLog = lparams[LFO1RATE].load();
            const MYFLOAT rate2InLog = lparams[LFO2RATE].load();
            const MYFLOAT rate3InLog = lparams[LFO3RATE].load();
            const MYFLOAT rate4InLog = lparams[LFO4RATE].load();
            _lfo1RateHz = LOG2NORMALF(rate1InLog + (_appState->parameters[LFO1RATE].max - rate1InLog) * lfoRateRouteAmtClamped);
            _lfo2RateHz = LOG2NORMALF(rate2InLog + (_appState->parameters[LFO2RATE].max - rate2InLog) * lfoRateRouteAmtClamped);
            _lfo3RateHz = LOG2NORMALF(rate3InLog + (_appState->parameters[LFO3RATE].max - rate3InLog) * lfoRateRouteAmtClamped);
            _lfo4RateHz = LOG2NORMALF(rate4InLog + (_appState->parameters[LFO4RATE].max - rate4InLog) * lfoRateRouteAmtClamped);
            // LFO ROUTES ARE BIPOLAR. Lfo::tick returns -1..1 with every shape centred
            // on zero, so val*depth is already a signed excursion centred on zero: the
            // knob is the CENTRE of the sweep and the LFO swings symmetrically either
            // side of it.
            //
            // This is deliberately NOT the shape the VEL/AT/MW routes use, and they are
            // left alone. For those, "the knob is the maximum and a source at rest pulls
            // down" is correct — a key you have not pressed must not open the filter past
            // where the knob sits. For an LFO the same formula was wrong: it put the whole
            // excursion BELOW the knob, so a filter LFO could only ever close, and vibrato
            // could only ever go flat of the note (see the pitch note further down).
            //
            // AMP (dest 3) is the one LFO destination that stays unipolar — see lfoAmpFact
            // below. Bipolar amplitude modulation is not tremolo: it would make the LFO
            // peak LOUDER than the amp EG's output and turn the note's nominal level into
            // the midpoint of the sweep.
            //
            // Each destination adds the offset in its OWN domain and then clamps to that
            // parameter's real min/max, so a deep LFO saturates against the end of the
            // range rather than running past it. modrange.cpp mirrors every one of these
            // so the knob and slider arcs cannot disagree with what is actually heard.
            auto lfoBi = [&](int dest) -> MYFLOAT {
                MYFLOAT o = 0.;
                for (int n = 0; n < 4; n++) o += lfoVals[n] * md(n, dest);
                return o;
            };
            const MYFLOAT lfoFiltOff     = lfoBi(2);
            const MYFLOAT lfoPitchOff    = lfoBi(1);
            const MYFLOAT lfoPwOff[3]    = {lfoBi(4),  lfoBi(5),  lfoBi(6)};
            const MYFLOAT lfoResOff      = lfoBi(7);
            const MYFLOAT lfoMorphOff[3] = {lfoBi(8),  lfoBi(9),  lfoBi(10)};
            const MYFLOAT lfoWarpOff[3]  = {lfoBi(11), lfoBi(12), lfoBi(13)};
            const MYFLOAT lfoUniOff[3]   = {lfoBi(14), lfoBi(15), lfoBi(16)};
            // MODAL filter strike (17). Scaled by the parameter's own 0..0.5 range so
            // depth 1 is a full-range swing; clamped where it is applied.
            _lfoStrikeOff = lfoBi(17) * 0.5;

            // filter
            // The floor the cutoff is built up from — the played note at full key
            // tracking, middle C at none. Hoisted out of the block below because the
            // modal bank needs it as well: it has to derive its own pitch from the
            // same floor, or KEYTRACK would move the cutoff and leave the body behind.
            const MYFLOAT baseHz   = filtBaseHz();
            const MYFLOAT filtBase = baseHz * _appState->onedsr;
            {
                const MYFLOAT cut     = lparams[FILT_CUT].load();
                const MYFLOAT rest    = (0.5 - filtBase) * cut;
                const MYFLOAT velFact = 1. - lparams[VEL_TO_FILT].load() * (1. - _velnorm);
                const MYFLOAT atFact  = 1. - lparams[AT_TO_FILT].load()  * (1. - at);
                const MYFLOAT mwFact  = 1. - lparams[MW_TO_FILT].load()  * (1. - mw);
                const MYFLOAT filtEgVal = egs[4] == -1 ? 1. : egtmp[egs[4]];
                // The LFO offsets the normalised cut AMOUNT, so it swings either side of
                // where CUT sits instead of only closing below it.
                gains[4] = filtBase + rest * (filtEgVal * velFact * atFact * mwFact + lfoFiltOff);
                // FILTCENTER's own min is 20 Hz and its max is Nyquist.
                const MYFLOAT cutMin = 20. * _appState->onedsr;
                if (gains[4] > .5) gains[4] = .5;
                if (gains[4] < cutMin) gains[4] = cutMin;
            }
            // resonance
            const MYFLOAT velResFact = 1. - lparams[VEL_TO_RES].load() * (1. - _velnorm);
            const MYFLOAT atResFact  = 1. - lparams[AT_TO_RES].load()  * (1. - at);
            const MYFLOAT mwResFact  = 1. - lparams[MW_TO_RES].load()  * (1. - mw);
            const MYFLOAT resEgVal   = egs[8] == -1 ? 1. : egtmp[egs[8]];
            MYFLOAT liveRes = lparams[FILTRES].load() * velResFact * atResFact * mwResFact * resEgVal + lfoResOff;
            if (liveRes < 0.) liveRes = 0.; else if (liveRes > 1.) liveRes = 1.;   // FILTRES min/max
            selectFiltMode((int)lparams[FILT_MODE].load());
            // Family dispatch — see the note at the other call site.
            if (filtFamily(_filtMode) == 4) {
                setModalFilterParams(gains[4], liveRes, baseHz);
            } else if (_filtMode >= FILTMODE_ZDF_FIRST) {
                setZdfParams(_filtMode, gains[4], liveRes);
            } else {
                HuovilainenMoog::setParams(gains[4], liveRes);
                HuovilainenMoog::setMode(_filtMode);
            }

            // amp
            // aftertouch is squared for the AMP route only (gentler onset); every
            // other AT route uses raw pressure.
            const MYFLOAT atCurved = at * at;
            // AMP stays unipolar (a tremolo only ducks), so it maps the bipolar value
            // back to 0..1 — the 0.5 is that conversion, not a change of depth: at
            // val -1 the factor is still 1-depth and at +1 it is still 1.
            // One LFO's contribution to a unipolar duck. A NEGATIVE depth inverts the
            // LFO, NOT the duck: writing 1 - depth*0.5*(1-val) with depth < 0 takes the
            // factor ABOVE 1, which would boost past the knob and make it the middle of
            // the excursion instead of its ceiling. |depth| keeps the ceiling, -val does
            // the inverting, and a pair with opposite signs is then exactly
            // complementary -- one source opens as the other closes.
            auto duck1 = [](MYFLOAT depth, MYFLOAT val) -> MYFLOAT {
                return depth < 0. ? 1. + depth * 0.5 * (1. + val)
                                  : 1. - depth * 0.5 * (1. - val);
            };
            // duck1(0, v) is exactly 1, so unrouted slots cost a multiply and change
            // nothing — no dest test needed with the matrix.
            const MYFLOAT lfoAmpFact = duck1(md(0, 3), _lfo1val) * duck1(md(1, 3), _lfo2val)
                                     * duck1(md(2, 3), _lfo3val) * duck1(md(3, 3), _lfo4val);
            // PER-SOURCE GAIN (18 GAIN1 .. 21 NOISE), the same unipolar duck as AMP and
            // for the same reason -- see the note above lfoBi. Bipolar here would put
            // the GAIN knob in the middle of the excursion, so the number written on the
            // panel would no longer be the loudest the source gets. At depth 1 this
            // reaches silence, which is what makes it usable as a gate.
            // A NEGATIVE depth here is the crossfade: point both LFOs at two gains with
            // opposite signs and one opens as the other closes. The two LFOs stay
            // exactly in step for it -- equal RATE params give bit-identical phase,
            // because both accumulators are reset together at note-on and the MW/AT
            // rate shift below is the same formula toward the same max for both.
            auto lfoDuck = [&](int dest) -> MYFLOAT {
                MYFLOAT f = 1.;
                for (int n = 0; n < 4; n++) f *= duck1(md(n, dest), lfoVals[n]);
                return f;
            };
            for (int i = 0; i < 4; i++)
                _gainFactInc[i] = (lfoDuck(18 + i) - _liveGainFact[i]) * (1. / 64.);
            _atAmpGain = (1. - lparams[AT_TO_AMP].load() * (1. - atCurved)) * lfoAmpFact;
            _atAmpGain = std::max((MYFLOAT)0, _atAmpGain);
            // First block of a fresh note: open AT the duck instead of ramping toward
            // it. Note-on wiped these to 1.0, so a note starting under a closed duck
            // (an AMP/GAIN LFO sitting low at its start phase, or a heavy AT_TO_AMP
            // route) played ~1.3 ms at full gain before the ramp caught up — and
            // _atAmpSmooth's init-time seed predates lfoAmpFact, so it, too, was
            // always 1.0 (audit findings M14/M15). This runs before the first sample
            // of the note is rendered: count starts at 0, so the block section
            // executes on the very first tick().
            if (_seedLive) {
                _seedLive = false;
                for (int i = 0; i < 4; i++) { _liveGainFact[i] = lfoDuck(18 + i); _gainFactInc[i] = 0.; }
                _atAmpSmooth = _atAmpGain;
            }
            // NB: this is a BLOCK-rate value (every 64 samples) that used to be applied
            // straight onto the per-sample output. A square LFO on AMP therefore stepped
            // the gain by the full tremolo depth between one sample and the next — 4.4 dB
            // at depth .4 — which is a click on every edge, 11 times a second on Tremolo
            // Rhodes. _atAmpSmooth below is what actually multiplies the output.

            // vibrato (pitch via jitter)
            const MYFLOAT mwVib = lparams[MW_TO_VIBRATO].load();
            const MYFLOAT atVib = lparams[AT_TO_VIBRATO].load();
            const MYFLOAT jitterBase = params[JITTERCENTS] / 1200.;
            const MYFLOAT mwVibFact = 1. - mwVib + mwVib * mw;
            const MYFLOAT atVibFact = 1. - atVib + atVib * at;
            const MYFLOAT jitterAmp = jitterBase * mwVibFact * atVibFact;
            jitter0.update(jitterAmp, params[JITTERA], params[JITTERB]);
            jitter1.update(jitterAmp, params[JITTERA], params[JITTERB]);
            const MYFLOAT tuneEgVal0 = egs[5] == -1 ? 1. : egtmp[egs[5]];
            const MYFLOAT tuneEgVal1 = egs[6] == -1 ? 1. : egtmp[egs[6]];
            const MYFLOAT tuneEgVal2 = egs[7] == -1 ? 1. : egtmp[egs[7]];
            // PITCH (dest 1) is the one destination that needed more than a sign change.
            // The LFO used to SCALE the oscillator's static detune (_tune * lfoFact), so
            // the pitch could only travel between the dialled-in detune and the true note:
            // vibrato needed a nonzero FINE/COARSE to exist at all, its rest position was
            // the sharp extreme, and it could never go sharp of the written pitch. It is
            // now an additive offset in the same octave domain as the exponent, so the
            // static detune is left exactly where the knob puts it and the LFO swings
            // symmetrically around it — and vibrato works at zero detune.
            const MYFLOAT lfoPitchOct = lfoPitchOff * (LFO_PITCH_SEMITONES / 12.);
            _jit[0] = _cps * pow(2, jitter0.tick() + _tune1 * tuneEgVal0 + lfoPitchOct);
            _jit[1] = _cps * pow(2, jitter1.tick() + _tune2 * tuneEgVal1 + lfoPitchOct);
            _jit[2] = _cps * pow(2, jitternormal.tick() + _tunenormal * tuneEgVal2 + lfoPitchOct);

            // PW mod (EG + LFO, pow(x,0.85) curve on the EG depth).
            // The EG (if assigned) walks basePw toward `target` along the span the
            // PWM DEPTH knob declares. The LFO is added in the PW KNOB'S OWN DOMAIN
            // (0.45 = the knob's full scale), per the rule at lfoBi — it used to be
            // added to `t` and so was scaled by the span 0.45*(PW − depth^0.85):
            // exactly zero on a default patch (both knobs at 0), so DEST=PW did
            // nothing at full depth, and its sign flipped once the depth curve
            // crossed PW (audit finding M5). Clamped to the duty limits the
            // oscillator accepts.
            auto applyPwMod = [&](MYFLOAT basePw, int srcParam, int depthParam, MYFLOAT lfoOff) -> MYFLOAT {
                const int src = (int)lparams[srcParam].load();
                const MYFLOAT depth  = pow(lparams[depthParam].load(), 0.85);
                const MYFLOAT target = 0.5 - depth * 0.45;
                const MYFLOAT span   = target - basePw;
                const MYFLOAT t = (src != 0) ? egtmp[src - 1] : 0.;
                const MYFLOAT pw = basePw + t * span + lfoOff * 0.45;
                return pw < 0.01 ? 0.01 : (pw > 0.99 ? 0.99 : pw);
            };
            const MYFLOAT pw1 = 0.5 - 0.45 * lparams[VCO1PW].load();
            const MYFLOAT pw2 = 0.5 - 0.45 * lparams[VCO2PW].load();
            const MYFLOAT pw3 = 0.5 - 0.45 * lparams[VCO3PW].load();
            // ramp toward the new block target instead of jumping, otherwise
            // the pw step every 64 samples puts audible sidebands on the pulse
            _pwinc[0] = (applyPwMod(pw1, VCO1PWMODSRC, VCO1PWMODDEPTH, lfoPwOff[0]) - _livepws[0]) * (1. / 64.);
            _pwinc[1] = (applyPwMod(pw2, VCO2PWMODSRC, VCO2PWMODDEPTH, lfoPwOff[1]) - _livepws[1]) * (1. / 64.);
            _pwinc[2] = (applyPwMod(pw3, VCO3PWMODSRC, VCO3PWMODDEPTH, lfoPwOff[2]) - _livepws[2]) * (1. / 64.);

            // Wavetable morph 0..1 = base (VCOxWTPOS) + the osc's PW-EG source
            // (VCOxPWMODSRC, repurposed for morph, since a WT osc has no pulse width)
            // + the LFO's own MORPH route (dests 8..10 — the LFO-PW dests are NOT
            // reused), ramped per block like pw so the position sweep doesn't zipper.
            // aftertouch + mod wheel push morph toward B (global, all oscs)
            // MORPH (= A = WTPOS) is the rest position; the osc's EG (VCOxPWMODSRC,
            // repurposed), LFO, AT and MW sweep it toward MORPH TO (= B). B<A inverts.
            // At B=1 this reduces to the old "push toward full morph" behaviour.
            const MYFLOAT morphPush = lparams[AT_TO_MORPH].load() * at + lparams[MW_TO_MORPH].load() * mw;
            // `t` is the position along the A->B span, 0..1. With an EG assigned the EG
            // drives it and the LFO offsets around wherever the EG has got to. With no
            // EG, the rest position depends on whether an LFO actually feeds this dest:
            // if one does, t centres at the MIDDLE of the span so A and B become the
            // LFO's two extremes; if none does, t rests at 0 — the fader's own value
            // sounds, exactly as the thumb shows. (The unconditional 0.5 rest was a
            // regression from the bipolar-LFO rework: an unmodulated patch audibly sat
            // at (A+B)/2 while the fader displayed A — audit finding M8. The routed
            // test uses the raw DEPTH knob, not the MW/AT-attenuated depth, so the
            // rest position is a property of the patch, not of the performance.)
            //
            // t is clamped to 0..1 FIRST, i.e. to the span the patch declared: modulation
            // moves within A..B and cannot carry the value somewhere the slider's arc does
            // not show. (AT/MW used to be able to push past B. They no longer can.)
            auto lfoFeeds = [&](int dest) -> bool {
                // RAW slot reads on purpose: whether a patch routes an LFO here is a
                // property of the patch, not of the MW/AT performance (see applyMorph).
                for (int n = 0; n < 4; n++)
                    if (lparams[lfoMdId(n, dest)].load() != 0.) return true;
                return false;
            };
            auto applyMorph = [&](MYFLOAT A, MYFLOAT B, int srcParam, MYFLOAT lfoOff, bool lfoRouted) -> MYFLOAT {
                const MYFLOAT span = B - A;
                const int src = (int)lparams[srcParam].load();
                MYFLOAT t = (src != 0) ? egtmp[src - 1] : (lfoRouted ? 0.5 : 0.);
                t += lfoOff + morphPush;
                if (t < 0.) t = 0.; else if (t > 1.) t = 1.;
                const MYFLOAT m = A + t * span;
                return m < 0. ? 0. : (m > 1. ? 1. : m);   // VCOxWTPOS min/max
            };
            // Published to the WT display as a POSITION ALONG THE SPAN, not an absolute
            // morph: the stack's slice 0 is A and its last slice is B, so this is
            // already the t the display draws at. A collapsed span (B == A) has nowhere
            // to be but the front. Doing the normalisation here also means the display
            // never has to read A and B, and so cannot disagree with this about where
            // the ends are.
            for (int o = 0; o < 3; o++) {
                const MorphIds mid = morphIdsFor(o, waveform[o]);
                const MYFLOAT A = lparams[mid.a].load(), B = lparams[mid.b].load();
                const MYFLOAT m = applyMorph(A, B, mid.eg, lfoMorphOff[o], lfoFeeds(8 + o));
                _morphinc[o] = (m - _livemorph[o]) * (1. / 64.);
                const MYFLOAT span = B - A;
                // Slot o feeds the WAVETABLE display and slot WT_DISPLAY_OSCS + o the
                // PAD display — each page keeps its own picture, so each type
                // publishes only to its own slot: a PAD osc publishing into the WT
                // slot would drive the picture of a set it is not playing.
                if (waveform[o] == 99)
                    wtPublishMorph(o, std::fabs(span) < 1e-6 ? 0.f : (float)((m - A) / span));
                else if (waveform[o] == 98)
                    wtPublishMorph(WT_DISPLAY_OSCS + o,
                                   std::fabs(span) < 1e-6 ? 0.f : (float)((m - A) / span));
            }

            // Per-osc unison config (applied in the non-PM path). ratio = symmetric
            // detune in cents; gain = centre-weighted blend, then RMS-normalised so
            // overall loudness ~matches a single voice.
            static const int uniVoicesP[3] = {VCO1UNIVOICES, VCO2UNIVOICES, VCO3UNIVOICES};
            static const int uniDetuneP[3] = {VCO1UNIDETUNE, VCO2UNIDETUNE, VCO3UNIDETUNE};
            static const int uniBlendP[3]  = {VCO1UNIBLEND,  VCO2UNIBLEND,  VCO3UNIBLEND};
            // DETUNE is modulatable by the LFO (dests UNI1/2/3) and by AT/MW. The LFO
            // route is bipolar like the rest: it swings DETUNE either side of the knob,
            // so it can narrow below the dialled-in spread as well as widen past it.
            // AT/MW still only WIDEN: they sweep DETUNE up toward full spread, rather
            // than attenuating it downward the way the filter/amp routes do. That
            // convention is wrong for this parameter — it can only ever subtract from
            // the knob, so at the 0.2 default a full-depth route buys 10 cents of
            // movement, and at DETUNE 0 (where a stack that is meant to open up starts)
            // it does exactly nothing. Pushing upward keeps the knob as the floor and
            // gives every depth setting an audible range. With nothing assigned the
            // value is still the raw knob.
            // VOICES is deliberately NOT modulatable — spreadUnison only seeds the copy
            // phases at note-on, so growing U.n mid-note would start the new copies
            // from a stale phase and jump the RMS normalisation: an audible click.
            const MYFLOAT uniPush = lparams[AT_TO_UNI].load() * at + lparams[MW_TO_UNI].load() * mw;
            for (int o = 0; o < 3; o++) {
                int uv = (int)std::lround(lparams[uniVoicesP[o]].load());
                if (uv < 1) uv = 1; else if (uv > UNISON_MAX) uv = UNISON_MAX;
                // PADsynth used to be forced to 1 here. It stacks like everything else
                // now — see the note by UNISON_MAX for the measurement that removed the
                // restriction. Note that BANDWIDTH is already a continuum of detuned
                // partials, so what unison adds on top is COHERENT periodic beating
                // between copies rather than more of the same wash.
                VcoUnison& U = _uni[o];
                U.n = uv;
                MYFLOAT det = lparams[uniDetuneP[o]].load();
                det += lfoUniOff[o];                    // LFO swings either side of the knob
                det += uniPush * (1. - det);            // aftertouch / wheel still only widen
                if (det < 0.) det = 0.; else if (det > 1.) det = 1.;   // VCOxUNIDETUNE min/max
                const MYFLOAT detCents = det * 50.0;                             // 0..50 cents
                const MYFLOAT blend    = lparams[uniBlendP[o]].load();          // side level 0..1
                MYFLOAT wsum2 = 0.0;
                for (int k = 0; k < uv; k++) {
                    const MYFLOAT pos = (uv == 1) ? 0.0 : ((MYFLOAT)k / (uv - 1)) * 2.0 - 1.0;
                    U.ratio[k] = (float)std::pow(2.0, (pos * detCents) / 1200.0);
                    const MYFLOAT w = 1.0 - (1.0 - blend) * std::fabs(pos);
                    U.gain[k] = (float)w;
                    wsum2 += w * w;
                }
                const MYFLOAT norm = wsum2 > 0.0 ? 1.0 / std::sqrt(wsum2) : 1.0;
                for (int k = 0; k < uv; k++) U.gain[k] *= (float)norm;
            }

            // Per-osc wavetable warp (WT mode only). WARP AMT (= A) is the rest value;
            // the EG source (VCOxWARPEG), LFO route (WARP1/2/3), AT and MW sweep it
            // toward WARP TO (= B). B<A inverts the sweep (warp falls as the mod rises).
            // At B=1 this reduces to the old "push toward full warp" behaviour.
            const MYFLOAT atWarp = lparams[AT_TO_WARP].load();
            const MYFLOAT mwWarp = lparams[MW_TO_WARP].load();
            // Same shape as applyMorph: position along A->B, clamped to the declared
            // span, and the same rest rule — 0.5 only when an LFO actually feeds the
            // dest, else the WARP AMT fader's own value sounds (see applyMorph).
            auto applyWarpMod = [&](MYFLOAT A, MYFLOAT B, int egSrc, MYFLOAT lfoOff, bool lfoRouted) -> float {
                const MYFLOAT span = B - A;
                MYFLOAT t = (egSrc != 0) ? egtmp[egSrc - 1] : (lfoRouted ? 0.5 : 0.);
                t += lfoOff + atWarp * at + mwWarp * mw;
                if (t < 0.) t = 0.; else if (t > 1.) t = 1.;
                const MYFLOAT m = A + t * span;
                return (float)(m < 0. ? 0. : (m > 1. ? 1. : m));   // VCOxWARPAMT min/max
            };
            // PADsynth build params. setPad() compares against what the oscillator
            // already holds and only fires an off-thread rebuild on a real change,
            // so calling it every block is free while a knob is still.
#if PA_ENABLE_PAD
            if (waveform[0] == 98) vcos[0].setPad(padParamsOf(0));
            if (waveform[1] == 98) vcos[1].setPad(padParamsOf(1));
            if (waveform[2] == 98) vconormal.setPad(padParamsOf(2));
#endif
            // Modal params are cheap and stateless, so refresh them every block. Only
            // DECAY / BRIGHT / MALLET reach the currently ringing note (through the
            // coefficient refresh); CHARACTER and STRIKE restructure the bank and so
            // land on the next note-on, which is why they are not smoothed.
            if (waveform[0] == 97) vcos[0].setModal(modalParamsOf(0));
            if (waveform[1] == 97) vcos[1].setModal(modalParamsOf(1));
            if (waveform[2] == 97) vconormal.setModal(modalParamsOf(2));

            // WARP moves at block rate with no per-sample ramp (unlike MORPH and PW)
            // — a known, accepted zipper for fast LFOs.
            vcos[0].setWarp((int)lparams[VCO1WARPTYPE].load(),
                            applyWarpMod(lparams[VCO1WARPAMT].load(), lparams[VCO1WARPTO].load(), (int)lparams[VCO1WARPEG].load(), lfoWarpOff[0], lfoFeeds(11 + 0)));
            vcos[1].setWarp((int)lparams[VCO2WARPTYPE].load(),
                            applyWarpMod(lparams[VCO2WARPAMT].load(), lparams[VCO2WARPTO].load(), (int)lparams[VCO2WARPEG].load(), lfoWarpOff[1], lfoFeeds(11 + 1)));
            vconormal.setWarp((int)lparams[VCO3WARPTYPE].load(),
                            applyWarpMod(lparams[VCO3WARPAMT].load(), lparams[VCO3WARPTO].load(), (int)lparams[VCO3WARPEG].load(), lfoWarpOff[2], lfoFeeds(11 + 2)));

            // The A→B spans the wavetables are BAKED over, so the frames land inside
            // the range modulation actually uses. Only a change in the quantised range
            // rebuilds, so calling this every block is free while the faders are still.
            auto rangeOf = [&](int wtpos, int morphTo, int warpAmt, int warpTo) {
                return WtRange{ (float)lparams[wtpos].load(),   (float)lparams[morphTo].load(),
                                (float)lparams[warpAmt].load(), (float)lparams[warpTo].load() };
            };
            vcos[0].setRange(rangeOf(VCO1WTPOS, VCO1MORPHTO, VCO1WARPAMT, VCO1WARPTO));
            vcos[1].setRange(rangeOf(VCO2WTPOS, VCO2MORPHTO, VCO2WARPAMT, VCO2WARPTO));
            vconormal.setRange(rangeOf(VCO3WTPOS, VCO3MORPHTO, VCO3WARPAMT, VCO3WARPTO));
            // TABLE, per block like WARP TYPE beside it. It was previously written only
            // by aimWt()/selectWavetable(), both of which run at note-on, so turning the
            // selector did nothing to a note that was already sounding — while WARP TYPE,
            // which keys the same cache entry, changed immediately. The swap fade covers
            // the discontinuity that made carrying it live unattractive before.
            vcos[0].setTable((int)std::lround(lparams[VCO1WTSEL].load()));
            vcos[1].setTable((int)std::lround(lparams[VCO2WTSEL].load()));
            vconormal.setTable((int)std::lround(lparams[VCO3WTSEL].load()));
        }
        for (int i = 0; i < 4; i++) _liveGainFact[i] += _gainFactInc[i];
        _livepws[0] += _pwinc[0];
        _livepws[1] += _pwinc[1];
        _livepws[2] += _pwinc[2];
        _livemorph[0] += _morphinc[0];
        _livemorph[1] += _morphinc[1];
        _livemorph[2] += _morphinc[2];

        // Per-osc control value: morph position for wavetable (99) and PADsynth (98)
        // oscs, pulse width otherwise. PADsynth interpolates its baked morph levels
        // from this, so the position has to arrive per-sample-ramped like WT's — a
        // block-rate step would zipper straight through the crossfade. Modal (97)
        // ignores the argument entirely, so it can take either.
        const bool m0m = (waveform[0] == 99 || waveform[0] == 98);
        const bool m1m = (waveform[1] == 99 || waveform[1] == 98);
        const bool m2m = (waveform[2] == 99 || waveform[2] == 98);
        const MYFLOAT c0 = m0m ? _livemorph[0] : _livepws[0];
        const MYFLOAT c1 = m1m ? _livemorph[1] : _livepws[1];
        const MYFLOAT c2 = m2m ? _livemorph[2] : _livepws[2];

        // SUB NORMAL output (added, pre-filter). PM has its own sine modulator,
        // so vconormal is only ticked for the NORMAL routing here.
        MYFLOAT subRaw = _donormal
            ? (_uni[2].n > 1 ? vconormal.tickUnison(_jit[2], 1.0, c2, _uni[2])
                             : vconormal.tick(_jit[2], 1.0, c2))
            : 0.0;

        const MYFLOAT g0 = egtmp[_ampIdx[0]] * (_sync[0] ? tukey[_phase] * gains[0] : gains[0]) * _liveGainFact[0];
        const MYFLOAT g1 = egtmp[_ampIdx[1]] * (_sync[1] ? tukey[_phase] * gains[1] : gains[1]) * _liveGainFact[1];
        MYFLOAT carriers;
        // Oversampled PM only for the plain-oscillator carriers; a wavetable carrier
        // falls back to the 1x path (PM not applied to WT in v1). Modal has to be in
        // this guard too because PM is meaningless on a bank with no phase accumulator.
        // tickOS2 does carry a mode-5 branch, but only as a backstop: it drops to the
        // plain tick() and duplicates the sample, so a modal carrier reaching it would
        // sound wrong (un-oversampled, PM ignored) rather than crash.
        const bool wtCarrier = m0m || m1m || waveform[0] == 97 || waveform[1] == 97;
        if (_dopm && !wtCarrier) {
            // sine modulator at 2x -> phase-mod offsets -> carriers generated 2x
            // and decimated (anti-aliased FM).
            MYFLOAT m0, m1;
            _pmSine.tickOS2(_jit[2], 1.0, 0.5, 0, 0, m0, m1);
            const MYFLOAT amt = _pmdepth * _pmdepth;   // squared: gentle low end, strong top
            const uint32_t pm0 = (uint32_t)(int64_t)(m0 * amt * 0.5 * (double)OSCBNK_PHSMAX_32);
            const uint32_t pm1 = (uint32_t)(int64_t)(m1 * amt * 0.5 * (double)OSCBNK_PHSMAX_32);
            MYFLOAT a0, a1, b0, b1;
            vcos[0].tickOS2(_jit[0], g0, _livepws[0], pm0, pm1, a0, a1);
            vcos[1].tickOS2(_jit[1], g1, _livepws[1], pm0, pm1, b0, b1);
            carriers = pmDecimate(a0 + b0, a1 + b1);
        } else {
            carriers = (_uni[0].n > 1 ? vcos[0].tickUnison(_jit[0], g0, c0, _uni[0])
                                      : vcos[0].tick(_jit[0], g0, c0)) +
                       (_uni[1].n > 1 ? vcos[1].tickUnison(_jit[1], g1, c1, _uni[1])
                                      : vcos[1].tick(_jit[1], g1, c1));
        }

        auto smpl = carriers +
                    (_donormal ? subRaw * egtmp[_ampIdx[2]] * gains[2] * _liveGainFact[2] : 0.0) +
                    _noise.tick() * egtmp[_ampIdx[3]] * gains[3] * _liveGainFact[3];

        _phase += _cps;
        if (_phase >= 1.0) {
            _phase -= 1.0;
            if (_sync[0])
                vcos[0].reset();
            if (_sync[1])
                vcos[1].reset();
        }

        // c2 gives the modulator the same live control value the NORMAL path feeds
        // vconormal: the ramped morph position for WT/PAD, the ramped pulse width
        // otherwise. It used to get a pulse width frozen at note-on, so PW3 and PWM
        // modulation reached NORMAL but silently never RINGMOD/AM.
        MYFLOAT modulated = (_doringmod || _doam)
            ? RingModFast<MYFLOAT>::tick(smpl, _jit[2], c2, egtmp[_ampIdx[2]] * gains[2] * _liveGainFact[2],
                                         _doringmod, _doam, _amdepth)
            : smpl;
        // Chase the block-rate target per sample so a gain change becomes a short ramp
        // rather than a step. ~3 ms: fast enough that a gated tremolo still reads as
        // gated (a 5.5 Hz square has a 91 ms half-period), slow enough that the edge
        // carries no click. Costs one multiply-add per sample.
        _atAmpSmooth += (_atAmpGain - _atAmpSmooth) * _ampSlew;
        // Note-START ramp for unenveloped sources, the counterpart to the end-of-life
        // ramp in isDead(). Ticked here and not with the envelopes because those run at
        // ksr = sr/64, where 3 ms is two steps of a staircase — the zipper this exists to
        // avoid. Advanced after use, like _stealFade below, so the first sample of a note
        // is at zero. Costs one compare per sample once it has arrived, and nothing reads
        // the slot on a fully enveloped voice, so there is no need to test _ampNone.
        if (egtmp[EG_NONE] < 1.) {
            egtmp[EG_NONE] += _noneFadeInc;
            if (egtmp[EG_NONE] > 1.) egtmp[EG_NONE] = 1.;
        }
        // _stealFadeInc, not _stealing: the same ramp also runs for a voice ending
        // naturally with an unenveloped source (see isDead), which is not a steal.
        if (_stealFadeInc > 0. && _stealFade > 0.) {
            _stealFade -= _stealFadeInc;
            if (_stealFade < 0.) _stealFade = 0.;
        }
        return Phaser::tick(tickFilter(modulated)) * _gain * _atAmpSmooth * _stealFade;

    }


#if PA_ENABLE_PAD
    PadParams padParamsOf(int o) const { return padParamsFromSnapshot(_appState, o); }
#endif
    // The cutoff FLOOR, with KEYTRACK_TO_FILT applied.
    //
    // The cutoff has always been built upward from the played note — gains[4] = _cps
    // + (0.5 - _cps) * cut — which is 100% key tracking with no way to turn it down.
    // That is why KEYTRACK_TO_FILT sat unread for so long: there was nothing for it
    // to scale, because the tracking was baked into the shape of the expression.
    //
    // Blend the floor toward a fixed reference instead. The blend is geometric, i.e.
    // linear in octaves, which is the only thing "half key tracking" can mean for a
    // pitch:  base = ref * (cps/ref)^kt.
    //
    // kt = 1 is the played note and reproduces the old behaviour. kt = 0 pins the
    // floor at middle C whatever is played — a formant rather than a filter, and for
    // MODAL a resonating body that stays put while the notes move under it, which is
    // how every real acoustic instrument works and was previously unreachable.
    //
    // The kt >= 1 branch is a correctness guarantee, not an optimisation: in floating
    // point ref * (f/ref) is NOT bit-identical to f, and since 1.0 is now this
    // parameter's initvalue, the default path has to reproduce the old output exactly
    // rather than merely closely. (Every preset on disk gets 1.0 for free — presets
    // are stored sparsely against initvalue and none of them contains this id, since
    // it never had a control.)
    //
    // The floor is derived in HERTZ and converted once, rather than blended in the
    // normalised domain. That is what makes the identity hold at BOTH ends: the modal
    // bank wants Hz and everything else wants cycles/sample, and `hz * onedsr * sr`
    // does not give back `hz`. Going Hz -> cps exactly once, through the same
    // expression the voice used to build _cps in the first place, means kt = 1
    // reproduces _cps and _freq exactly rather than to within a rounding step.

    // Where the floor sits at kt = 0. Middle C, the same pivot KEYTRACK_TO_DECAY
    // uses, so the two keytrack controls agree on the middle of the keyboard.
    static constexpr double FILT_KEYTRACK_REF_HZ = 261.6255;

    MYFLOAT filtBaseHz() const {
        const MYFLOAT kt = _appState->params[0][KEYTRACK_TO_FILT].load();
        if (kt >= 1.f) return _freq;
        const MYFLOAT ref = (MYFLOAT)FILT_KEYTRACK_REF_HZ;
        if (kt <= 0.f) return ref;
        if (_freq <= 0.f) return _freq;
        return ref * (MYFLOAT)std::pow((double)_freq / (double)ref, (double)kt);
    }
    // Callers convert with `baseHz * _appState->onedsr` — exactly the expression the
    // voice uses at note-on (_cps = _freq * onedsr), so at kt = 1 the result is
    // bit-identical to _cps. They keep the Hz value too, because the modal bank needs
    // it; there is deliberately no cps-returning helper, since calling one would
    // evaluate filtBaseHz() a second time.

    // T60 with its three routes folded in. Lives here rather than in the block loop
    // so that note-on and the per-block refresh cannot disagree: a note struck while
    // aftertouch is already held must start damped, not start long and jump when the
    // next block runs.
    double routedModalDecay(double t60) const {
        auto& lparams = _appState->params[0];

        // KEYTRACK. On any real struck instrument the tail shortens as the pitch
        // rises; a bank with one T60 for the whole keyboard sounds synthetic at the
        // top and stunted at the bottom. At amount 1 this halves T60 per octave above
        // C4 and doubles it per octave below, which is roughly the piano's own law.
        const double kt = lparams[KEYTRACK_TO_DECAY].load();
        if (kt > 0.) {
            const double f = (double)_freq > 1. ? (double)_freq : 1.;
            t60 *= std::pow(2.0, -std::log2(f / 261.6255) * kt);   // C4 is the pivot
        }

        // Aftertouch / mod wheel DAMP. Deliberately the opposite direction to the
        // FILT and AMP routes: those use 1 - route*(1 - src), so the source has to be
        // pushed to reach the knob's value. Damping has to work the other way round —
        // the knob is the open string and the hand only ever shortens it — which is
        // the same argument the UNI routes make for pushing upward instead.
        const double at = (double)_atSnapshot / 127.;
        const double mw = (double)_appState->data->modwheel.load(std::memory_order_relaxed) / 127.;
        t60 *= (1. - lparams[AT_TO_DECAY].load() * at)
             * (1. - lparams[MW_TO_DECAY].load() * mw);

        // Floor rather than let it reach zero: a fully damped bank should be a thud,
        // and a T60 of 0 would put r at 0 and silence the resonator outright.
        return t60 < 0.02 ? 0.02 : t60;
    }

    ModalOscParams modalParamsOf(int o) const {
        ModalOscParams p = modalParamsFromSnapshot(_appState, o);
        p.decaySec = routedModalDecay(p.decaySec);
        return p;
    }

    tsl::AppState* _appState{};
    MYFLOAT _freq{}, _tune1{1}, _tune2{1}, _tunenormal{};
    int64_t _time{};
    MYFLOAT _pws[3]{};
    MYFLOAT _livepws[3]{0.5, 0.5, 0.5};
    MYFLOAT _pwinc[3]{};
    MYFLOAT _livemorph[3]{};
    MYFLOAT _morphinc[3]{};
    MYFLOAT gains[5]{};
    int egs[9]{};
    int waveform[3]{};
    bool _doringmod{};
    bool _donormal{};
    bool _doam{};
    bool _dopm{};
    MYFLOAT _amdepth{};
    MYFLOAT _pmdepth{};
    bool _sync[3]{};
    Lfo _lfo1{}, _lfo2{}, _lfo3{}, _lfo4{};
    MYFLOAT _lfo1val{}, _lfo2val{}, _lfo3val{}, _lfo4val{};
    // MODAL filter STRIKE excursion (LFO dest 17). An additive bipolar offset whose
    // neutral is 0, which is also what applyAT sees before tick() has ever run.
    MYFLOAT _lfoStrikeOff{0};
    // MW/AT->rate shift computed once per 64-sample block (see tick()), then
    // read every sample by the _lfo1/_lfo2 .tick() calls - same split as
    // gains[4]/resonance (block-rate compute, per-sample use).
    MYFLOAT _lfo1RateHz{}, _lfo2RateHz{}, _lfo3RateHz{}, _lfo4RateHz{};
    MYFLOAT _phase{}, _gain{1.}, _velnorm{1.}, _atAmpGain{1.};
    // Per-sample follower for _atAmpGain, and its one-pole coefficient. Seeded by the
    // first-block _seedLive snap in tick() (not at init time — that assignment is a
    // stale no-op) so a note starts at its real gain instead of ramping up into it.
    MYFLOAT _atAmpSmooth{1.}, _ampSlew{1.};
    // LFO -> per-source GAIN, dests 18..21, index i is gains[i] (VCO1/VCO2/SUB/NOISE).
    // Ramped across the block exactly as _livepws is: the block section sets the
    // target increment, the sample loop walks to it. A block-rate step straight onto a
    // gain is a zipper at 64 samples -- at 20 Hz and full depth the factor moves ~0.17
    // per block, which is a step, not a modulation.
    MYFLOAT _liveGainFact[4]{1., 1., 1., 1.}, _gainFactInc[4]{};
    // True from note-on until the first block section runs; makes the ramps and the
    // amp smoother START at their targets rather than at 1.0. Deliberately NOT set
    // by retrigger(): a retriggered voice's ramps are already live and converged.
    bool _seedLive{true};
    bool _fromSequencer{false};
    // Set by beginSteal(): this voice is fading out to free its slot. It must not be
    // picked as a victim twice, and must not be retriggered by a note-on on the same
    // key — that would resurrect a voice that is about to lose its slot.
    // Cleared for free on reuse: FastQueue::_alloc assigns a default-constructed
    // VcoNote over the recycled node.
    bool _stealing{false};
    // 3 ms, linear, at the SAMPLE rate. Long enough that the ramp is a fade rather than
    // a step, short enough that the note taking the slot is not audibly late.
    static constexpr double STEAL_FADE_S = 0.003;
    MYFLOAT _stealFade{1.}, _stealFadeInc{0.};
    uint8_t _noteNum{};
    float _atSnapshot{};
    int count{};
    // FIVE, not four: egtmp[0..3] are EG1..EG4, and egtmp[EG_NONE] is the factor for
    // an amp source with no envelope assigned, multiplied in without a branch in the
    // sample loop. The ADSR tick only touches 0..3; the init paths clear this slot to
    // 0 and the note-start ramp in tick() writes it every sample until it reaches 1.0
    // (~3 ms), where it holds for the rest of the voice's life.
    static constexpr int EG_NONE = 4;
    MYFLOAT egtmp[5]{0., 0., 0., 0., 1.};
    // Per-sample increment for the note-start ramp on egtmp[EG_NONE]. Defaulted for 48 k
    // rather than left at 0, so that if a future init path ever forgets to set it the
    // ramp is merely at the wrong rate instead of stuck at silence.
    MYFLOAT _noneFadeInc{(MYFLOAT)(1.0 / (0.003 * 48000.0))};
    MYFLOAT _jit[3]{}, _cps{};
    // All three derived from egs[0..3] and nothing else; deriveEgRouting() recomputes
    // them wherever those are set, and documents what each one means.
    int  _ampIdx[4]{EG_NONE, EG_NONE, EG_NONE, EG_NONE};
    int  _lifeEg[2]{};
    int  _lifeN{0};
    bool _ampNone{false};
    // The gate, set by gateOff(). Only an unenveloped amp source needs it — everything
    // else reads the key off the envelope states — but it must be cleared on retrigger,
    // which revives a voice whose key has already come up.
    bool _keyUp{false};
    ADSR adsr[4]{};
    Jitter<MYFLOAT> jitter0{}, jitter1{}, jitternormal{};
    VcoUnison _uni[3]{};   // per-osc unison config (VCO1/VCO2/SUB), recomputed per block
    Vco vcos[2], vconormal;
    NoiseSource _noise;
    // PM: a dedicated sine modulator (cleanest FM sidebands) + a 2:1 decimator
    // for the oversampled carrier sum. Only used when _dopm.
    Vco _pmSine;
    MYFLOAT _pmDown[numtaps * 2]{};
    int _pmCursor{};

    // --- ZDF filter modes (FILT_MODE 7..16) ------------------------------
    // Added alongside HuovilainenMoog rather than replacing it: modes 0..6
    // still run the original filter and stay bit-identical. Those seven are the
    // only ones that ever shipped, so they are FROZEN — every released preset
    // stores a mode index and 0..6 must keep meaning what it always meant.
    //
    // 7..16 have never been in a release, so they are grouped by filter family
    // here (all three ladder taps, then all four state-variable taps, then the
    // three acid taps) instead of being ordered low-pass-first. That puts each
    // family together in the mode menu.
    enum {
        FILTMODE_ZDF_FIRST = 7,
        FILTMODE_LAD_LP    = 7,    // "LAD"  ladder LP4
        FILTMODE_LAD_BP    = 8,    // "LADDER BP"
        FILTMODE_LAD_HP    = 9,    // "LADDER HP"
        FILTMODE_SEM_LP    = 10,   // "SEM"  state variable LP
        FILTMODE_SEM_BP    = 11,   // "SEM BP" state variable
        FILTMODE_SEM_HP    = 12,   // "SEM HP" state variable
        FILTMODE_SEM_N     = 13,   // "SEM NOTCH" the SEM's signature
        FILTMODE_303_LP    = 14,   // "303"  acid ladder LP3
        FILTMODE_303_BP    = 15,   // "303B" acid ladder BP
        FILTMODE_303_HP    = 16,   // "303H" acid ladder HP3
        // A modal resonator bank driven by the mix — the same 14 complex rotations
        // the MODAL oscillator strikes, but excited continuously instead. CUT is the
        // bank's fundamental and RESO is its decay; see setModalFilterParams.
        FILTMODE_MODAL     = 17    // "MODAL"
    };

    // Which filter object a mode runs on: 0 Huovilainen, 1 ladder, 2 SVF, 3 303,
    // 4 modal bank.
    static inline int filtFamily(int mode) {
        switch (mode) {
            case FILTMODE_LAD_LP: case FILTMODE_LAD_BP: case FILTMODE_LAD_HP: return 1;
            case FILTMODE_SEM_LP: case FILTMODE_SEM_BP: case FILTMODE_SEM_HP:
            case FILTMODE_SEM_N:                                              return 2;
            case FILTMODE_303_LP: case FILTMODE_303_BP: case FILTMODE_303_HP: return 3;
            case FILTMODE_MODAL:                                              return 4;
            default:                                                          return 0;
        }
    }

    // Only the selected filter ticks, so an unselected one holds whatever ring
    // was in it when the mode last moved away — switching back mid-note fired
    // that stale energy into the output (measured 7.6x the running level).
    // Clear the incoming filter's state when the mode crosses families; within
    // a family (LADDER LP -> LADDER BP etc.) the state is live and must carry over.
    inline void selectFiltMode(int mode) {
        if (mode == _filtMode) return;
        const int fam = filtFamily(mode);
        if (fam != filtFamily(_filtMode)) {
            switch (fam) {
                case 1: _zdfLadder.reset(); break;
                case 2: _zdfSvf.reset(); break;
                case 3: _zdf303.reset(); break;
                // A modal bank holds its ring far longer than any of the others, so
                // this matters more here than anywhere: switching away and back
                // without it would fire seconds-old energy into the output.
                case 4: _modalFilt.reset(); _modalFiltBody = -1; break;
                default: HuovilainenMoog::reset(); break;
            }
        }
        _filtMode = mode;
    }

    // CUT -> bank fundamental, RESO -> decay. Called once per block from the same
    // place the other filters get their params.
    //
    // baseHz is the cutoff floor from filtBaseHz(), i.e. the played note scaled by
    // KEYTRACK_TO_FILT. It is passed in rather than re-read from _freq because the
    // bank's pitch and the cutoff have to come from the SAME floor — otherwise
    // turning keytrack down would move the cutoff and leave the body sitting on the
    // note, which is the one configuration this is meant to make reachable. Passing
    // Hz rather than cps also keeps the caller's `baseHz * onedsr` the only place the
    // conversion happens, so the two agree bit for bit.
    inline void setModalFilterParams(MYFLOAT cutNorm, MYFLOAT res, MYFLOAT baseHz) {
        const double sr = (double)_appState->sr;
        if (sr != _modalFiltSr) { _modalFilt.init(sr); _modalFiltSr = sr; _modalFiltBody = -1; }

        // Do NOT read gains[4] as the bank's frequency. It runs LINEARLY from _cps
        // (the played note) at CUT 0 to 0.5 (Nyquist) at CUT 1 — see applyAT, where
        // rest = (0.5 - _cps) * cut. That suits a lowpass corner, for which "wide
        // open" means pass everything, and is precisely wrong for a resonator
        // fundamental, for which wide open means putting every mode above hearing.
        // FILT_CUT defaults to 1.0, so taken literally the bank sat at 24 kHz: the
        // Nyquist cull then deleted every partial (STRING and DEEP ended up with
        // NONE) and what was left was one shrill mode sweeping with the EG.
        //
        // Recover the normalised cut POSITION instead and spend it as an octave
        // offset on the note. u = 0 puts the bank exactly on the played pitch, which
        // is both the body-at-note-pitch sound and the only place the oscillators'
        // own harmonics drive it hard; u = 1 is MODAL_FILT_OCTAVES above. Everything
        // that moves gains[4] — FILTEG, VEL, AT, MW, both LFOs — still moves u, so no
        // routing is lost. (KEYTRACK_TO_FILT is not in that list because it acts a
        // level earlier: filtBaseHz() reads it and scales the cutoff floor itself,
        // which arrives here as baseHz — see the note above filtBaseHz.)
        // u = 1 is the TOP of the cut range and is where FILT_CUT defaults (initvalue
        // 1.0), so u = 1 must be the good-sounding position: the bank sitting exactly
        // on the played note. Turning CUT down walks it DOWN, up to
        // MODAL_FILT_OCTAVES below — a bigger, darker body, which is the same
        // direction of travel a lowpass has and reads the same way by ear.
        //
        // Mapping u = 1 to the top of an upward range instead (the obvious reading)
        // put the default 3 octaves above the note, where it is both quiet and, since
        // T60 fixes bandwidth in HZ and Q therefore rises with frequency, effectively
        // high-Q: at 3520 Hz with a 73 Hz skirt that is Q 48, a whistle, even at
        // RESO 0. The same setting on the note is Q 3, which is a body.
        //
        // Both the span and the pitch come from the keytracked floor, not from _cps /
        // _freq. At full keytrack the floor IS the note, so this is the old expression
        // unchanged; at keytrack 0 the body stops following the keyboard and stays
        // where CUT puts it — a fixed resonating body with the notes moving through
        // it, which is how an actual instrument is built and was not reachable before.
        constexpr double MODAL_FILT_OCTAVES = 3.0;
        const double base = (double)(baseHz * _appState->onedsr);   // matches the caller
        const double span = 0.5 - base;
        double u = span > 1e-9 ? ((double)cutNorm - base) / span : 1.0;
        if (u < 0.) u = 0.; else if (u > 1.) u = 1.;
        double f0 = (double)baseHz * std::pow(2.0, (u - 1.0) * MODAL_FILT_OCTAVES);
        const double f0max = sr / 12.0;
        if (f0 > f0max) f0 = f0max;
        if (f0 < 20.0)  f0 = 20.0;

        // RESO spans the DRIVEN-useful window only: T60 0.03 s (bandwidth ~73 Hz,
        // Q~3, a formant) to 0.5 s (~4.4 Hz, Q~50, a plate). The struck oscillator's
        // multi-second decays are deliberately out of reach here — at T60 8 s the
        // skirt is +/-1 cent wide, so a driven bank simply stops tracking its input.
        double rn = (double)res;
        if (rn < 0.) rn = 0.; else if (rn > 1.) rn = 1.;
        const double t60 = 0.03 * std::pow(0.5 / 0.03, rn);

        const auto& sp = _appState->data->synth_params;
        const int  body = (int)std::lround(sp[FILTMODALBODY]);

        // STRIKE, swung either side of the knob by the LFO's STRIKE destination (the
        // offset is bipolar). Clamped to the parameter's own 0..0.5 range — past 0.5
        // the comb mirrors, so a route that ran further would fold back on itself and
        // reverse direction mid-sweep.
        double pos = (double)sp[FILTMODALPOS];
        pos += (double)_lfoStrikeOff;
        if (pos < 0.) pos = 0.; else if (pos > 0.5) pos = 0.5;

        // Three tiers, cheapest first. Only BODY needs the full relayout: it changes
        // the ratios, hence the partial count, the Nyquist cull and every cached
        // falloff. setDecay rewrites r (14 pow + 14 sqrt), setPosition rewrites the
        // strike comb and the input gains (14 sin), and retune rewrites the rotation
        // coefficients from f0 (14 sin/cos). All three are exact against a full
        // configure() — see the equivalence harness — and none of them touch the
        // ringing state, which is what makes them safe to drive from an LFO.
        //
        // f0 moves every block with the filter EG, so retune is the unconditional
        // common path; the other two only fire on a real change.
        if (body != _modalFiltBody) {
            _modalFilt.configure(body, f0, t60, 0.5, 0.55, pos, 0x9E3779B9u);
            _modalFiltBody = body; _modalFiltT60 = t60; _modalFiltPos = pos;
        } else {
            if (t60 != _modalFiltT60) { _modalFilt.setDecay(t60);    _modalFiltT60 = t60; }
            if (pos != _modalFiltPos) { _modalFilt.setPosition(pos); _modalFiltPos = pos; }
            _modalFilt.retune(f0);
        }
    }

    tsl::dsp::ZdfLadder _zdfLadder{};
    tsl::dsp::ZdfSvf    _zdfSvf{};
    tsl::dsp::Zdf303    _zdf303{};
    pa::ModalBank       _modalFilt{};
    double _modalFiltSr{0}, _modalFiltT60{-1}, _modalFiltPos{-1};
    int    _modalFiltBody{-1};
    int _filtMode{0};
    MYFLOAT _zdfSr{0};

    // gains[4] arrives as a normalized frequency (cycles/sample, 0..0.5) - it
    // is fed to HuovilainenMoog, which is constructed with sampleRate 1 so its
    // internal fc = cutoff/sampleRate is just that value. The ZDF filters take
    // Hz, hence the * sr. No extra curve is applied: FILT_CUT's shaping has
    // already happened upstream in gains[4], and both filter families take a
    // real frequency, so the knob feel carries over unchanged.
    //
    // liveRes is 0..1 linear, and 1 is HuovilainenMoog's self-oscillation
    // point (it applies resonance * 0.99 internally). That is exactly what
    // setResonance(1) means for the ladder and the SVF, so those pass through
    // untouched. The 303 is the exception and does need scaling: its feedback
    // highpass and stage saturation eat loop gain, so its measured critical
    // resonance is 1.02 at 4 kHz rising to 1.20 by 440 Hz. The 1.25 gets a
    // full RES knob self-oscillating from ~440 Hz up; below that it stops
    // screaming, which is authentic - the real thing loses resonance as the
    // filter closes.
    inline void setZdfParams(int mode, MYFLOAT cpsCutoff, MYFLOAT res) {
        const MYFLOAT sr = _appState->sr;
        if (sr != _zdfSr) {
            _zdfSr = sr;
            _zdfLadder.setSampleRate(sr);
            _zdfSvf.setSampleRate(sr);
            _zdf303.setSampleRate(sr);
        }
        const MYFLOAT hz = cpsCutoff * sr;
        if (res < 0.) res = 0.;
        if (res > 1.) res = 1.;
        switch (mode) {
            case FILTMODE_LAD_LP:
            case FILTMODE_LAD_BP:
            case FILTMODE_LAD_HP:
                _zdfLadder.setCutoffFast(hz);
                // Top-stretch so the RES knob's max actually self-oscillates:
                // stage saturation puts the ladder's measured onset at 1.014
                // (8 kHz) rising to 1.058 (55 Hz), so a straight pass-through
                // sits just under the scream everywhere — while legacy LP4,
                // SEM and the 303 all reach theirs. Cubic keeps the lower
                // knob range untouched (+1% at 0.5) and lands 1.08 at 1.0.
                _zdfLadder.setResonance(res * (1. + .08 * res * res * res));
                _zdfLadder.setMode(mode == FILTMODE_LAD_LP ? tsl::dsp::ZdfLadder::LP4
                                 : mode == FILTMODE_LAD_BP ? tsl::dsp::ZdfLadder::BP4
                                                           : tsl::dsp::ZdfLadder::HP4);
                break;
            case FILTMODE_SEM_LP:
            case FILTMODE_SEM_BP:
            case FILTMODE_SEM_HP:
            case FILTMODE_SEM_N:
                _zdfSvf.setCutoffFast(hz);
                _zdfSvf.setResonance(res);
                _zdfSvf.setMode(mode == FILTMODE_SEM_LP ? tsl::dsp::ZdfSvf::LP
                              : mode == FILTMODE_SEM_BP ? tsl::dsp::ZdfSvf::BP
                              : mode == FILTMODE_SEM_N  ? tsl::dsp::ZdfSvf::NOTCH
                                                        : tsl::dsp::ZdfSvf::HP);
                break;
            default:
                _zdf303.setCutoffFast(hz);
                _zdf303.setResonance(res * 1.25);
                _zdf303.setMode(mode == FILTMODE_303_LP ? tsl::dsp::Zdf303::LP3
                              : mode == FILTMODE_303_BP ? tsl::dsp::Zdf303::BP
                                                        : tsl::dsp::Zdf303::HP3);
                break;
        }
    }

    inline MYFLOAT tickFilter(MYFLOAT in) {
        switch (filtFamily(_filtMode)) {
            case 1:  return _zdfLadder.tick(in);
            case 2:  return _zdfSvf.tick(in);
            case 3:  return _zdf303.tick(in);
            case 4:  return _modalFilt.process(in);
            default: return HuovilainenMoog::tick(in);
        }
    }

    inline void resetFilters() {
        HuovilainenMoog::reset();
        _zdfLadder.reset();
        _zdfSvf.reset();
        _zdf303.reset();
        _modalFilt.reset();
        _modalFiltBody = -1;   // force a relayout on the next param update
    }
};

void VcoPreNote::init(tsl::AppState* _appState, double freq, int64_t time, MYFLOAT vel, uint8_t noteNum) {
    auto& params = _appState->data->synth_params;
    _finalfreq = _freq = freq;
    _finaltime = _time = time;
    _noteNum = noteNum;
    _velnorm = vel / 127.;
    const MYFLOAT velAmt = _appState->params[0][VEL_TO_AMP].load();
    _gain = velAmt > 0. ? pow(_velnorm, velAmt * 3.) : 1.;
    _pws[0] = 0.5 - 0.45 * params[VCO1PW];
    _pws[1] = 0.5 - 0.45 * params[VCO2PW];
    _pws[2] = 0.5 - 0.45 * params[VCO3PW];
    gains[0] = LOG2NORMALF(params[VCO1GAIN]);
    gains[1] = LOG2NORMALF(params[VCO2GAIN]);
    gains[2] = LOG2NORMALF(params[VCO3GAIN]);
    gains[3] = LOG2NORMALF(params[NOISEGAIN]);
    egs[0] = (int) params[VCO1EG];
    egs[1] = (int) params[VCO2EG];
    egs[2] = (int) params[VCO3EG];
    egs[3] = (int) params[NOISEEG];
    egs[4] = (int) params[FILTEG];
    egs[5] = (int) params[VCO1TUNEEG];
    egs[6] = (int) params[VCO2TUNEEG];
    egs[7] = (int) params[VCO3TUNEEG];
    // NO egs[8] HERE: this array is int[8]. RESEG is read live when the note is
    // adopted (VcoNote::operator=), because the frozen layout has no slot for it.
    _tune1 = params[VCO1COARSE] / 12. + params[VCO1FINE] / 1200.;
    _tune2 = params[VCO2COARSE] / 12. + params[VCO2FINE] / 1200.;
    _donormal = params[VCO3NORMAL] == 1.;
    _tunenormal = _appState->params[0][VCO3COARSEST].load() / 12. + params[VCO3FINE] / 1200.;
    waveform[0] = (int) params[VCO1TYPE];
    waveform[1] = (int) params[VCO2TYPE];
    waveform[2] = (int) params[VCO3TYPE];
    _doringmod = params[VCO3RINGMOD] == 1.;
    _sync[0] = params[VCO1SYNC] == 1.;
    _sync[1] = params[VCO2SYNC] == 1.;
    _sync[2] = params[VCO3SYNC] == 1.;
    egvals[0] = params[EG1ATTACK];
    egvals[1] = params[EG1DECAY];
    egvals[2] = params[EG1SUSTAIN];
    egvals[3] = params[EG1RELEASE];
    egvals[4] = params[EG2ATTACK];
    egvals[5] = params[EG2DECAY];
    egvals[6] = params[EG2SUSTAIN];
    egvals[7] = params[EG2RELEASE];
    egvals[8] = params[EG3ATTACK];
    egvals[9] = params[EG3DECAY];
    egvals[10] = params[EG3SUSTAIN];
    egvals[11] = params[EG3RELEASE];
    egvals[12] = params[EG4ATTACK];
    egvals[13] = params[EG4DECAY];
    egvals[14] = params[EG4SUSTAIN];
    egvals[15] = params[EG4RELEASE];
    _jittercents = params[JITTERCENTS] / 1200.;
    _jittera = params[JITTERA];
    _jitterb = params[JITTERB];
    _phaseractive = params[PHASERPOW] == 1.0;
    if(_phaseractive){
        phaserrange = params[PHASERRANGE];
        phaserfb = params[PHASERFB];
        phaserrate = params[PHASERRATE];
    }
}


struct SynthQueue : tsl::FastQueue<VcoNote> {
public:
    tsl::AppState* _appState{};
    explicit SynthQueue(int s) : FastQueue<VcoNote>(s) {};

    void release() {
        _pendCount = 0;   // a note-on that has not sounded yet is cancelled, not queued
        auto temp = _first;
        while (temp) {
            temp->data.gateOff();
            temp = temp->next;
        }
    }

    // Shadows FastQueue::flush so a pending steal cannot outlive an All Sound Off or a
    // sample-rate rebuild and then fire a note into what should be silence.
    //
    // The live voices must hand their wavetable/PAD sets back BEFORE the pool reset:
    // FastQueue::flush does no per-node teardown, so a discarded voice would keep its
    // sets resident in the pooled node (ten voices can pin ten different ~12 MB sets)
    // until the slot is reused — at which point _alloc's wipe would drop the last
    // reference and run the whole deallocation inside the note-on callback. Same
    // reasoning as the reap path in play2 and as releaseTables' own comment.
    void flush() {
        _pendCount = 0;
        for (auto* temp = _first; temp; temp = temp->next)
            temp->data.releaseTables();
        FastQueue<VcoNote>::flush();
    }

    // ── voice stealing ───────────────────────────────────────────────────────────
    //
    // The pool is fixed at 10 and FastQueue::push returns nullptr once it is empty, so
    // a note arriving with every slot busy used to be DROPPED SILENTLY. That is what
    // made amp-EG RELEASE a polyphony budget rather than a taste control: a 3.7 s
    // release holds all ten slots, and playing anything at speed simply stopped making
    // sound. Reported as "no sound" / "drops randomly" / "issue in whole synth", and
    // worked around for a long time by keeping RELEASE under ~0.7 in every preset.
    //
    // STEALING IS DEFERRED, AND HAS TO BE. Re-initialising a sounding voice in place
    // steps its output — env drives amplitude directly, the same click ADSR::retrigger
    // exists to avoid — and, worse, it walks past releaseTables(), so the victim's
    // wavetable and PADsynth sets are never handed back. One PAD voice pins ~16.75 MB,
    // and the eventual free would then run on the audio thread. So instead: fade the
    // victim over ~3 ms (STEAL_FADE_S), let the reap loop below do the teardown it
    // already does correctly for any finished voice, and hold the new note until a
    // slot is actually free. The cost is ~3 ms of latency on the stolen note only —
    // drainPending is checked every sample, so the wait is the fade itself.
    struct Pending {
        bool       fromSequencer{false};
        // Set when the key came up before the voice was free — see noteOff. Without
        // it the note-off is lost and the note rings forever.
        bool       released{false};
        MYFLOAT    cps{}, vel{};
        int64_t    time{};
        uint8_t    noteNum{};
        VcoPreNote note{};   // sequencer path only; POD, so a copy is safe and cheap
    };
    // Four deep so a chord landing on a full pool does not come out as its first note
    // alone. Past that the player is outrunning the fade and dropping is the honest
    // answer — the same answer as before, just four notes later.
    static constexpr int MAX_PENDING = 4;
    Pending _pending[MAX_PENDING]{};
    int     _pendCount{0};

    // What push() did before stealing existed. nullptr means the pool is empty.
    VcoNote* pushNow(MYFLOAT cps, int64_t time, MYFLOAT vel, uint8_t noteNum) {
        auto* p = FastQueue<VcoNote>::push();
        if (!p) return nullptr;
        p->_appState = _appState;
        p->init(cps, time, vel, noteNum);
        return p;
    }
    VcoNote* pushNow(VcoPreNote* note) {
        auto* p = FastQueue<VcoNote>::push();
        if (!p) return nullptr;
        p->_appState = _appState;
        *p = *note;
        p->_atSnapshot = note->_atSnapshot;
        p->_fromSequencer = true;
        p->_gain *= LOG2NORMALF(_appState->params[0][SEQ_GAIN].load());
        return p;
    }

    // Victim priority: a voice whose key is already up beats one still held — its
    // disappearance is the thing the player is least likely to be listening to — and
    // among those the quietest goes first. Only when every voice is still held do we
    // take the oldest, which is _first because the list is in push order. Voices
    // already fading out for an earlier steal are skipped, so each pending note is
    // owed its own slot rather than several notes waiting on one.
    bool stealVoice() {
        VcoNote* victim = nullptr;
        int      bestRank = 2;
        MYFLOAT  bestEnv = 0.;
        for (auto* n = _first; n; n = n->next) {
            auto& v = n->data;
            if (v._stealing) continue;
            const int     rank = v.ampReleasing() ? 0 : 1;
            const MYFLOAT e    = v.ampEnv();
            if (!victim || rank < bestRank || (rank == bestRank && e < bestEnv)) {
                bestRank = rank; bestEnv = e; victim = &v;
            }
        }
        if (!victim) return false;
        victim->beginSteal();
        return true;
    }

    bool queuePending(const Pending& p) {
        if (_pendCount >= MAX_PENDING) return false;
        if (!stealVoice()) return false;   // nothing left to take: drop, as before
        _pending[_pendCount++] = p;
        return true;
    }

    // MUST be called from outside the voice traversal. push() relinks the list, and
    // the traversal reads temp->next after del(), so the two cannot interleave.
    void drainPending() {
        while (_pendCount > 0 && !full()) {
            const Pending p = _pending[0];
            for (int i = 1; i < _pendCount; i++) _pending[i - 1] = _pending[i];
            _pendCount--;
            VcoNote* v = nullptr;
            if (p.fromSequencer) { VcoPreNote n = p.note; v = pushNow(&n); }
            else                 v = pushNow(p.cps, p.time, p.vel, p.noteNum);
            // The key came up while this note was waiting. Release it now rather than
            // dropping it, so a note played shorter than the steal takes still sounds
            // — as the blip it was — instead of either vanishing or ringing forever.
            if (v && p.released) v->gateOff();
        }
    }

    void push(MYFLOAT cps, int64_t time, MYFLOAT vel = 127., uint8_t noteNum = 0) {
        if (pushNow(cps, time, vel, noteNum)) return;
        Pending p;
        p.fromSequencer = false;
        p.cps = cps; p.time = time; p.vel = vel; p.noteNum = noteNum;
        queuePending(p);
    }

    void push(VcoPreNote *note) {
        if (pushNow(note)) return;
        Pending p;
        p.fromSequencer = true;
        p.note = *note;
        queuePending(p);
    }


    void noteOff(MYFLOAT freq) {
        // A NOTE WAITING FOR A STOLEN VOICE IS NOT IN THIS LIST, and its key-up is
        // still its only way to ever stop. play2 drains the whole preQueue before any
        // audio, so a note-on and its note-off in the same block are both handled up
        // front — and at a 1024-sample buffer that window is 21 ms, which any run,
        // arpeggio or sequencer line clears easily. Walking past the pending slot left
        // the note to be pushed ~3 ms later (after the steal fade) with _time ==
        // INT64_MAX and nothing to release it: it rang until the synth was reset.
        // Ear-reported as "left a note ringing forever that doesn't go away".
        //
        // Marked rather than cancelled, so the note still sounds as the short blip it
        // was played as; drainPending releases it the moment it is pushed.
        for (int i = 0; i < _pendCount; i++)
            if (!_pending[i].fromSequencer && _pending[i].cps == freq)
                _pending[i].released = true;

        auto temp = _first;
        while (temp) {
            if (temp->data._time == INT64_MAX && temp->data._freq == freq)
                temp->data.gateOff();
            temp = temp->next;
        }
    }

    // play() was deleted: play2 below is the only live path, and the old play()
    // lacked both the event drain and the sequencer deadline gate — reviving it
    // would ship stuck sequencer notes.

    void play2(MYFLOAT *buf, int s) {
        const auto learn = _appState->params[0][SEQLEARNING].load() == 1.;
        VCOPreEvent event{};

        while (_appState->data->preQueue.pop(event)) {
            if (event.type == tsl::midi::STATUS_NOTE_ON) {
                // Retrigger an already-live voice on the same pitch instead of
                // stacking a second one on top of it: with only a handful of
                // voice slots total, unbounded stacking from repeated notes on
                // one key can starve genuinely different notes of polyphony.
                // A voice with a fade armed is excluded: it is milliseconds from being
                // recycled, so retriggering it resurrects a note that is about to have
                // its slot taken away. That covers both reasons a fade runs — a steal,
                // and an unenveloped source ending on key-up. The second one matters
                // here because _time stays INT64_MAX through a note-off (only gateOff
                // runs), so such a voice is otherwise a candidate, and the ramp would
                // carry on to zero and kill the note that had just been retriggered.
                VcoNote* existing = nullptr;
                for (auto* node = _first; node; node = node->next) {
                    if (node->data._noteNum == event.noteNum && !node->data._fromSequencer &&
                        !node->data.fading() && node->data._time == INT64_MAX) {
                        existing = &node->data;
                        break;
                    }
                }
                if (existing)
                    existing->retrigger(event.keyvel);
                else
                    push(event.cps, INT64_MAX, event.keyvel, event.noteNum);
            }
            else if (event.type == tsl::midi::STATUS_NOTE_OFF)
                noteOff(event.cps);
            else if (event.type == 0xA0) {
                _appState->data->lastPolyAT.store(event.keyvel, std::memory_order_relaxed);
                auto* node = _first;
                while (node) {
                    if (node->data._noteNum == event.noteNum && !node->data._fromSequencer)
                        node->data.applyAT(event.keyvel);
                    node = node->next;
                }
            }
            // Channel pressure: one message for the whole keyboard, so it lands on
            // every live voice. A later poly-AT message for one note overwrites that
            // voice's snapshot — poly wins per note when a controller sends both.
            else if (event.type == tsl::midi::STATUS_CHANNEL_PRESSURE) {
                _appState->data->lastPolyAT.store(event.keyvel, std::memory_order_relaxed);
                for (auto* node = _first; node; node = node->next)
                    if (!node->data._fromSequencer)
                        node->data.applyAT(event.keyvel);
            }
            else if (event.type == tsl::midi::STATUS_CONTROL_CHANGE) {
                if (event.noteNum == 120)
                    flush();     // All Sound Off: immediate hard silence, no release
                else if (event.noteNum == 123)
                    release();   // All Notes Off: graceful release, as if note-off on everything
            }
            if (learn) {
                sequencer::preQueue.push(event);
            }
        }
        // Anything a producer could not fit in preQueue is latched instead of lost —
        // see DATA::lostNoteOffs. Applied after the drain so a note-on and its latched
        // note-off from the same burst still land in order. A lost All Sound Off
        // degrades to release() — graceful, but guaranteed to arrive.
        if (_appState->data->lostAllOff.exchange(false, std::memory_order_acquire))
            release();
        for (int w = 0; w < 2; w++) {
            uint64_t bits = _appState->data->lostNoteOffs[w].exchange(0, std::memory_order_acquire);
            while (bits) {
                const int n = w * 64 + __builtin_ctzll(bits);
                bits &= bits - 1;
                noteOff(tsl::midi::MidiNotes::midiToFreq(n));
            }
        }
        const auto cur = tsl::time::nanosecondsSinceEpoch();

        for (int i = 0; i < s; i++) {
            const auto cc = cur + _appState->nanospersample * i;
            if (++count == 64) {
                count = 0;

                VcoPreNote *note{};

                sequencer::tick(_appState, &note);

                if (note != nullptr) {
                    note->_finaltime = note->_time + cc;
                    push(note);
                }
            }

            // Before the traversal, never inside it — see drainPending. Checked every
            // sample so a stolen note starts the instant its slot frees rather than at
            // the next block boundary; it is one int test when nothing is pending.
            if (_pendCount) drainPending();

            auto temp = _first;
            while (temp) {
                if (temp->data._time < cc) temp->data.gateOff();
                if (temp->data.isDead()) {
                    temp->data.releaseTables();
                    del(temp);
                } else
                    buf[i] += temp->data.tick();
                temp = temp->next;
            }
        }
    }

    int count{};
};


#define MAX_DELAY_S 3

template<typename T>
class DelayEffect {
public:
    explicit DelayEffect(tsl::AppState* appState) : _appState(appState) {
        int s = next_pow_2(3 * _appState->sr + 2);
        _andmask = s - 1;
        int delaysize = 2 * s;
        _delayline.resize(delaysize, 0);
        _linel = &_delayline[0];
        _liner = &_delayline[s];
        _fb = &_appState->params[0][CDELFB];
        _delay = &_appState->params[0][CDELDEL];
        _mix = &_appState->params[0][CDELMIX];
        _mod = &_appState->params[0][CDDELMODRATE];
        _depth = &_appState->params[0][CDDELMODDEPTH];
        int n = 0;
        for (; n < TBLSIZE2 >> 1; n++)
            tri[n] = 2.0 * n / ((T) TBLSIZE2);
        for (; n < TBLSIZE2; n++)
            tri[n] = (T) (2.0 * (TBLSIZE2 - n + 1)) / ((T) TBLSIZE2);
        //  tri[i] = (T) (0.5 - 0.5 * cos((MYFLOAT) i * TWOPI_P / (MYFLOAT) TBLSIZE2));
    }


    inline T tickLine(T lag, T *line) {
        T delay = _writeoff - lag;

        auto readPos = (uint32_t) delay;
        T frac = delay - readPos;

        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        readPos &= _andmask;
        T v0 = line[readPos];
        --readPos;
        readPos &= _andmask;
        T vm1 = line[readPos];
        readPos += 2;
        readPos &= _andmask;
        T v1 = line[readPos];
        ++readPos;
        readPos &= _andmask;
        T v2 = line[readPos];

        return (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;
    }

    inline void tickLines(T lag, T &outl, T &outr) {
        T delay = _writeoff - lag;

        auto readPos = (int32_t) delay;
        T frac = delay - readPos;

        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        readPos &= _andmask;
        T v0l = _linel[readPos];
        T v0r = _liner[readPos];

        --readPos;
        readPos &= _andmask;
        T vm1l = _linel[readPos];
        T vm1r = _liner[readPos];

        readPos += 2;
        readPos &= _andmask;
        T v1l = _linel[readPos];
        T v1r = _liner[readPos];

        ++readPos;
        readPos &= _andmask;
        T v2l = _linel[readPos];
        T v2r = _liner[readPos];

        outl = (am1 * vm1l + a0 * v0l + a1 * v1l + a2 * v2l) * frac + v0l;
        outr = (am1 * vm1r + a0 * v0r + a1 * v1r + a2 * v2r) * frac + v0r;
    }

    void compute(const T *in, T *outl, T *outr, int size) {
        const T feedback = *_fb;
        const T lag = LOG2NORMALF(*_delay) * _appState->sr;
        const int mode = 1;//*_mode;
        const MYFLOAT mix = *_mix;
        const MYFLOAT mixsrc = 1. - mix;
        const MYFLOAT lfocps = LOG2NORMALF(*_mod) * _appState->onedsr;
        const MYFLOAT multi = (lag - 0.001 * _appState->sr) * *_depth;
        for (int i = 0; i < size; i++) {
            if (mode == 0) {
                T tap = tickLine(lag, _linel);
                _linel[_writeoff] = tap * feedback + in[i];
                _liner[_writeoff] = 0;
                outl[i] = outr[i] = in[i] * mixsrc + mix * tap;
            } else if (mode == 1) {
                const T lagtmp = 2 + lag - multi * tri[PHS2INT2(lfophase)];
                lfophase += lfocps;
                if (lfophase >= 1.)
                    lfophase -= 1.;
                T tapL, tapR;
                tickLines(lagtmp, tapL, tapR);
                _linel[_writeoff] = tapR + in[i];
                _liner[_writeoff] = tapL * feedback;
                const MYFLOAT o = in[i] * mixsrc;
                outl[i] = o + tapL * mix;
                outr[i] = o + tapR * mix;
            }
            (++_writeoff) &= _andmask;
        }
    }

    // Clears the delay line so re-enabling after being switched off doesn't
    // dump stale, frozen echoes back in.
    void reset() {
        std::fill(_delayline.begin(), _delayline.end(), 0);
        _writeoff = 0;
        lfophase = 0;
    }

private:
    tsl::AppState* _appState;
    std::atomic<T> *_fb, *_delay, *_mix, *_mod, *_depth;
    uint32_t _writeoff{}, _andmask;
    T lfophase{};
    T *_linel, *_liner;
    tsl::AlignedVector<T> _delayline;
    T tri[TBLSIZE2];
};

// Every voice is built from this snapshot, not from params[0] itself: VcoNote::init
// and VcoPreNote::init read synth_params. It is taken once per audio block, so
// anything that rewrites params[0] AFTER the snapshot and expects to be heard on the
// very next note - a preset load, which arrives on the audio thread through
// toAudioThreadQueue in the middle of the block - has to retake it.
// (The LFO DEST-cursor/DEPTH-window coherence hook lives in EventHandler.cpp's
// lfoWindowHook, on the Event::apply funnel — NOT here. A first version ran per
// audio block in synthFunc and the window went dead whenever audio wasn't
// processing.)

void refreshSynthParams(tsl::AppState* _appState) {
    auto& synth_params = _DATA->synth_params;
    for (int i = 1; i < NUM_PARAMS; i++) {
        synth_params[i] = (_STATE->parameters[i].paramCurve == Param::ParamCurve::Log10)
                          ? LOG2NORMALF(_STATE->params[0][i].load())
                          : _STATE->params[0][i].load();
    }
}

#ifndef AUDIO_NO_THREADS
void synth_main_thread() {
    tsl::AppState* _appState = __STATE;
    LOGI("Synththread: %ld", gettid());
    __STATE->data->onedtwosr = _STATE->onedsr * 0.5;

    const int channels = _STATE->channels;
    tsl::Player &player = _STATE->player;

    const int bufsize_init = _STATE->currentBufSize;

    const int64_t numFramesAsNanos =
            (bufsize_init * kPercentageOfCallbackToUse / _STATE->sr) * kNanosPerSecond;
    // The hint wants the whole deadline, not the 0.8 of it the spin aims at:
    // telling the governor the deadline is tighter than it is would be asking
    // for a boost against the wrong target.
    const int64_t blockNanos = int64_t((double(bufsize_init) / _STATE->sr) * kNanosPerSecond);

    //SYNTH_THREAD_DATA *thread_datas[8];
    tprio(-19);
    Player::currentoff = 0;
    __STATE->data->currentoff = 0;
    int buf = 0;
    int64_t diffprint = 0;
    const int checkload = __STATE->sr / 4;

    MYFLOAT mOpsPerNano = 1;

    // Ask the governor to keep this thread fast rather than buying it with
    // generateLoad below — same flow as grainstorm's worker threads. See
    // tools/AdpfHint.h: the spin is what this replaces, and it stays as the
    // fallback for devices that do not implement the hint.
    //
    // Opened here rather than lazily because this thread already knows its
    // deadline: bufsize_init is captured above and never changes for the life
    // of the thread, unlike grainstorm's, which has to wait for the first block.
    //
    // NOT COMPILED as things stand. va/CMakeLists.txt defines AUDIO_NO_THREADS
    // unconditionally, so this whole function is behind a dead #ifndef and
    // PocketAnalog renders on the Oboe callback thread instead — the live hint
    // is the one in PlayerBase::synthFunc below. Kept in step with that one so
    // the threaded path is not broken if it is ever switched back on.
#ifdef __ANDROID__
    tsl::AdpfHint hint;
    LOGI("Synththread ADPF %s (target %lld ns)",
         hint.startForCurrentThread(blockNanos) ? "session created" : "unavailable",
         (long long) blockNanos);
#endif

    // DATA::thread_datas = thread_datas;

    MYFLOAT buffer[bufsize_init];
    MYFLOAT tmp[bufsize_init];
    _appState->data->tmpbuf = &tmp[0];

    SynthQueue synthQueue(10);
    synthQueue._appState = _appState;
    Resampler5::setup();

    Progenitor1Dark<MYFLOAT> progenitor1Dark(_appState);

    DelayEffect<MYFLOAT> delayEffect(_appState);

    do {
        Player::open.store(false, std::memory_order_release);
        Player::playsem.acquire();

        // if (tsl::app::destroyRequested.load())
        //   break;
        if (__STATE->params[0][SYNTHRESET].exchange(0.0) == 1.0) {
            synthQueue.flush();
        }
        int64_t start = getNanoseconds();
        refreshSynthParams(_appState);
        warmPadSets(_appState);
        warmWtSets(_appState);

        auto* tmpbuf = __STATE->data->tmpbuf;
        memset(buffer, 0, sizeof(MYFLOAT) * bufsize_init);
        sequencer::check(_appState);
        synthQueue.play2(&buffer[0], bufsize_init);

        int offset = 0;


        if (__STATE->params[0][CDELPOW].load()) {
            delayEffect.compute(buffer, buffer, tmpbuf, bufsize_init);
        } else {
            memcpy(tmpbuf, buffer, sizeof(MYFLOAT) * bufsize_init);
        }

        if (__STATE->params[0][REVPOW].load()) {
            progenitor1Dark.compute(buffer, tmpbuf, buffer, tmpbuf);
        }
        /* Android threaded path - unused when AUDIO_NO_THREADS is defined */
        (void)buffer; (void)tmpbuf;

        __STATE->data->offset += bufsize_init;


        int64_t stop = getNanoseconds();
        int64_t executionDurationNanos = stop - start;

        diffprint += executionDurationNanos;

        buf += bufsize_init;

        if (buf >= checkload) {
            buf -= checkload;
            __STATE->buffer_fill = diffprint;
            //LOGE("%d", (int) (diffprint / 1000000.));
            diffprint = 0;
        }

#ifdef __ANDROID__
        if (hint.active()) {
            // Reporting is the half that does the work: the governor adjusts by
            // comparing the actual duration against the target. A session that
            // is never reported to does nothing at all.
            hint.report(executionDurationNanos);
        } else
#endif
        {
            int64_t nanosleep = numFramesAsNanos - executionDurationNanos;

            if (nanosleep > 0)
                generateLoad(nanosleep, mOpsPerNano);
        }

    } while (true);
#ifdef __ANDROID__
    hint.stop();
#endif
    LOGI("Synththread exit.");
}
#else

void onGotSampleRate(tsl::AppState* _appState, int sampleRate) {
    PolyPhaseResampler2x<MYFLOAT>::generateCoefficients(_DATA->coeffsUp, numtaps, 1, 2, WindowedSinc<>::getUpSampleBandWidth(numtaps));
    PolyPhaseResampler2x<MYFLOAT>::generateCoefficients(_DATA->coeffsDown, numtaps, 2, 1, WindowedSinc<>::getDownSampleBandWidth(numtaps));
    _DATA->onedtwosr = _STATE->onedsr * 0.5;
    _DATA->synth_bufl.clear();
    _DATA->synth_bufr.clear();
    if (!_DATA->synthQueue_obj) {
        _DATA->synthQueue_obj = {new SynthQueue(10), [](void* p){ delete static_cast<SynthQueue*>(p); }};
    } else {
        static_cast<SynthQueue*>(_DATA->synthQueue_obj.get())->flush();
    }
    _DATA->delayEffect_obj = {new DelayEffect<MYFLOAT>(_appState), [](void* p){ delete static_cast<DelayEffect<MYFLOAT>*>(p); }};
    _DATA->progenitor1Dark_obj = {new Progenitor1Dark<MYFLOAT>(_appState), [](void* p){ delete static_cast<Progenitor1Dark<MYFLOAT>*>(p); }};
}
#ifdef __ANDROID__
void PlayerBase::synthFunc(tsl::AppState *_appState, float *out, int samples) {
#else
void
PlayerBase::synthFunc(tsl::AppState* _appState, sampleTSL** tin, sampleTSL** out, int samples, int channelMask) {
#endif
    auto& bb = _DATA->synth_bb;
    auto& diffprint = _DATA->synth_diffprint;

    const int checkload = _STATE->sr / 4;
    auto& synthQueue = *static_cast<SynthQueue*>(_DATA->synthQueue_obj.get());
    synthQueue._appState = _appState;

    auto start = tsl::time::nanosecondsSinceEpoch();

#ifdef __ANDROID__
    // Ask the governor to keep this thread fast. Same purpose as grainstorm's
    // worker-thread hint, but attached here because this is where PocketAnalog
    // renders: AUDIO_NO_THREADS is defined unconditionally in va/CMakeLists.txt,
    // so synth_main_thread() is not compiled and there is no worker to hint.
    //
    // tsl::Player::BuildStream already enables Oboe's own equivalent
    // (setPerformanceHintEnabled) for this same thread, but that is documented
    // as device-specific and was measured doing nothing on the OpenSLES path
    // this app defaults to. An explicit session works regardless of which audio
    // API is in use.
    //
    // Function-local statics: this app is IS_SINGLETON, only the audio thread
    // reaches this line, and the session must be created from the thread it
    // describes — which is this one.
    //
    // No spin fallback, deliberately. There has never been one on this path, and
    // adding one would be re-implementing oboe::StabilizedCallback inside the
    // callback it wraps — the ~90%-of-a-core cost this work exists to remove.
    static tsl::AdpfHint hint;
    static bool hintTried = false;
    if (!hintTried) {
        hintTried = true;
        const int64_t blockNanos =
                int64_t((double(samples) / _STATE->sr) * tsl::time::nanosPerSecond);
        LOGI("PA audio thread ADPF %s (target %lld ns)",
             hint.startForCurrentThread(blockNanos) ? "session created" : "unavailable",
             (long long) blockNanos);
    }
#endif


    if (_STATE->params[0][SYNTHRESET].exchange(0.0) == 1.0) {
        synthQueue.flush();
    }
    refreshSynthParams(_appState);
    warmPadSets(_appState);
    warmWtSets(_appState);

    auto& bufl = _DATA->synth_bufl;
    auto& bufr = _DATA->synth_bufr;
    auto& dryl = _DATA->synth_dryl;
    auto& dryr = _DATA->synth_dryr;
    if (bufl.size() < samples) {
        bufl.resize(samples);
        bufr.resize(samples);
        dryl.resize(samples);
        dryr.resize(samples);
    }
    std::fill(bufl.begin(), bufl.begin() + samples, 0);
    std::fill(bufr.begin(), bufr.begin() + samples, 0);
    sequencer::check(_appState);
    synthQueue.play2(bufl.data(), samples);

    // Both effects crossfade between their own wet/dry-mixed output and the
    // untouched dry signal on power on/off (0.001/sample one-pole, same
    // coefficient as the postgain smoothing below - ~20ms), instead of a hard
    // cut, and reset their internal buffers once fully faded out so turning
    // back on doesn't dump a stale, frozen tail back in.
    {
        const bool cdelOn = _STATE->params[0][CDELPOW].load() == 1.0;
        auto& fade = _DATA->synth_delayFade;
        auto& needsReset = _DATA->synth_delayNeedsReset;
        if (cdelOn || fade > 0.0001) {
            std::copy(bufl.begin(), bufl.begin() + samples, dryl.begin());
            static_cast<DelayEffect<MYFLOAT>*>(_DATA->delayEffect_obj.get())->compute(bufl.data(), bufl.data(), bufr.data(), samples);
            for (int i = 0; i < samples; i++) {
                fade += 0.001 * ((cdelOn ? 1.0 : 0.0) - fade);
                bufl[i] = dryl[i] + fade * (bufl[i] - dryl[i]);
                bufr[i] = dryl[i] + fade * (bufr[i] - dryl[i]);
            }
            needsReset = true;
        } else {
            bufr = bufl;
            if (needsReset) {
                static_cast<DelayEffect<MYFLOAT>*>(_DATA->delayEffect_obj.get())->reset();
                needsReset = false;
            }
        }
    }

    {
        const bool revOn = _STATE->params[0][REVPOW].load() == 1.0;
        auto& fade = _DATA->synth_reverbFade;
        auto& needsReset = _DATA->synth_reverbNeedsReset;
        if (revOn || fade > 0.0001) {
            std::copy(bufl.begin(), bufl.begin() + samples, dryl.begin());
            std::copy(bufr.begin(), bufr.begin() + samples, dryr.begin());
            static_cast<Progenitor1Dark<MYFLOAT>*>(_DATA->progenitor1Dark_obj.get())->compute(bufl.data(), bufr.data(), bufl.data(), bufr.data(), samples);
            for (int i = 0; i < samples; i++) {
                fade += 0.001 * ((revOn ? 1.0 : 0.0) - fade);
                bufl[i] = dryl[i] + fade * (bufl[i] - dryl[i]);
                bufr[i] = dryr[i] + fade * (bufr[i] - dryr[i]);
            }
            needsReset = true;
        } else if (needsReset) {
            static_cast<Progenitor1Dark<MYFLOAT>*>(_DATA->progenitor1Dark_obj.get())->reset();
            needsReset = false;
        }
    }

    // Session wall: glide to silence through the existing smoothing instead of
    // letting stream->stop() cut mid-waveform.
    auto postgain = _DATA->sessionMute.load() ? 0.f
        : LOG2NORMALF(_STATE->params[0][POSTGAIN].load());

    // The ensemble, on the MIXED voices — where a string machine's is. See
    // Chorus.h for why this is not per-note. Crossfaded on power on/off and reset
    // once fully faded, exactly like the delay and reverb above, so switching it
    // back on cannot dump a stale tail back in.
    {
        const bool chOn = _STATE->params[0][CHORUSPOW].load() == 1.0;
        auto& fade = _DATA->synth_chorusFade;
        auto& needsReset = _DATA->synth_chorusNeedsReset;
        if (chOn || fade > 0.0001) {
            std::copy(bufl.begin(), bufl.begin() + samples, dryl.begin());
            std::copy(bufr.begin(), bufr.begin() + samples, dryr.begin());
            _DATA->synth_chorus.setDepth((double)_STATE->params[0][CHORUSDEPTH].load());
            _DATA->synth_chorus.setRate((double)_STATE->params[0][CHORUSRATE].load());
            _DATA->synth_chorus.process(bufl.data(), bufr.data(), samples,
                                        (double)_STATE->params[0][CHORUSMIX].load());
            for (int i = 0; i < samples; i++) {
                fade += 0.001 * ((chOn ? 1.0 : 0.0) - fade);
                bufl[i] = dryl[i] + fade * (bufl[i] - dryl[i]);
                bufr[i] = dryr[i] + fade * (bufr[i] - dryr[i]);
            }
            needsReset = true;
        } else if (needsReset) {
            _DATA->synth_chorus.clear();
            needsReset = false;
        }
    }

    float peakright = 0.00001;
    float peakleft = 0.00001;
    auto& smoothed = _DATA->synth_smoothed;
    const bool degrade = _DATA->integrity_failed.load();
    static constexpr float deg_step = 0.5f;
    static constexpr float deg_dc   = 0.05f;
#ifdef __ANDROID__
    for (int i = 0; i < samples; i++) {
        auto vl = static_cast<sampleTSL>(bufl[i] * smoothed);
        auto vr = static_cast<sampleTSL>(bufr[i] * smoothed);
        if (degrade) {
            vl = roundf(vl / deg_step) * deg_step + deg_dc;
            vr = roundf(vr / deg_step) * deg_step + deg_dc;
        }
        if (vl >  0.99f) vl =  0.99f; else if (vl < -0.99f) vl = -0.99f;
        if (vr >  0.99f) vr =  0.99f; else if (vr < -0.99f) vr = -0.99f;
        out[i * 2]     = vl;
        out[i * 2 + 1] = vr;
        smoothed += 0.001f * (postgain - smoothed);
        peakleft  = std::abs(vl) > peakleft  ? std::abs(vl)  : peakleft;
        peakright = std::abs(vr) > peakright ? std::abs(vr) : peakright;
    }
#else
    auto* outL = out[0];
    auto* outR = out[1];
    for (int i = 0; i < samples; i++) {
        auto vl = static_cast<sampleTSL>(bufl[i] * smoothed);
        auto vr = static_cast<sampleTSL>(bufr[i] * smoothed);
        if (degrade) {
            vl = roundf(vl / deg_step) * deg_step + deg_dc;
            vr = roundf(vr / deg_step) * deg_step + deg_dc;
        }
        if (vl >  0.99) vl =  0.99; else if (vl < -0.99) vl = -0.99;
        if (vr >  0.99) vr =  0.99; else if (vr < -0.99) vr = -0.99;
        outL[i] = vl;
        outR[i] = vr;
        smoothed += 0.001f * (postgain - smoothed);
        peakleft  = std::abs(vl) > peakleft  ? std::abs(vl)  : peakleft;
        peakright = std::abs(vr) > peakright ? std::abs(vr) : peakright;
    }
#endif

    _STATE->peak[0] = peakleft;
    _STATE->peak[1] = peakright;

    if (_STATE->player.isrecording.load(std::memory_order_acquire)) {
        int remaining = samples;
        int srcIdx = 0;
        while (remaining > 0) {
            auto* recItem = static_cast<tsl::AudioBuffer<sampleTSL>*>(
                _STATE->pool.acquire(sizeof(tsl::AudioBuffer<sampleTSL>)));
            if (!recItem) break;
            int chunk = std::min(remaining, (int)(tsl::audioBufferDefaultSize / 2));
            recItem->frames   = chunk;
            recItem->channels = 2;
            for (int i = 0; i < chunk; i++) {
#ifdef __ANDROID__
                recItem->data[i * 2]     = out[srcIdx * 2];
                recItem->data[i * 2 + 1] = out[srcIdx * 2 + 1];
#else
                recItem->data[i * 2]     = out[0][srcIdx];
                recItem->data[i * 2 + 1] = out[1][srcIdx];
#endif
                srcIdx++;
            }
            if (!_STATE->player.recQueue.try_push(recItem)) {
                _STATE->pool.release(recItem);
            } else {
                _STATE->waitNotify.wake_thread(_STATE->player.slot);
            }
            remaining -= chunk;
        }
    }

    _DATA->offset += samples;

    const auto executionDurationNanos = tsl::time::nanosecondsSinceEpoch() - start;
    diffprint += executionDurationNanos;

#ifdef __ANDROID__
    // Reporting is the half that does the work: the governor adjusts by
    // comparing actual durations against the target. A session that is never
    // reported to does nothing at all.
    hint.report(executionDurationNanos);
#endif

    bb += samples;

    if (bb >= checkload) {
        bb -= checkload;
        _STATE->buffer_fill = diffprint;
        diffprint = 0;
    }

};
#endif