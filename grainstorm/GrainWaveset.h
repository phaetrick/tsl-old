#pragma once
//
// WAVESET - Wishart-style waveset distortion, per grain.
//
// A waveset is the stretch of signal between two upward zero crossings. The
// grain buffer is exactly the right unit to do this in: it arrives raw and
// un-windowed, the whole segment is in memory at once, and whatever is left
// over at the end is faded out by the grain envelope anyway. GROUP joins N
// wavesets into one operated-on unit (Wishart's group), which is also the
// escape hatch for noisy material that crosses zero constantly.
//
// The operation runs the input wavesets through once and fills the output
// buffer as it goes: modes that emit more than they consume (REPEAT) therefore
// truncate the grain, and modes that emit less (OMIT) leave a zeroed tail.
// That asymmetry is the effect, not a defect.
//

#ifndef GRAINSTORM_GRAINWAVESET_H
#define GRAINSTORM_GRAINWAVESET_H

#include <atomic>
#include <vector>
#include "base.h"

class GrainWaveset : public Effect {
public:
    GrainWaveset(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

    enum {
        MODE_REPEAT = 0,
        MODE_OMIT,
        MODE_REVERSE,
        MODE_NORMALIZE,
        MODE_SHUFFLE,
        MODE_SINE
    };

private:
    // Boost ceiling for NORMALIZE. Without it a waveset sitting in the noise
    // floor is lifted to the grain's peak and the silence between events turns
    // into a roar.
    static constexpr MYFLOAT NORM_MAX_BOOST = 32.;
    // Bound on the refill passes (see compute): one pass always emits at least
    // one waveset, so this only caps the pathological "grain made of 1-sample
    // wavesets" case.
    static constexpr int MAX_PASSES = 64;

    std::vector<MYFLOAT> _scratch;
    std::vector<int32_t> _starts;   // waveset boundaries, _starts[0] == 0

    std::atomic<MYFLOAT> *_mode{}, *_amt{}, *_group{};
    // replaces AMOUNT outright, per grain (Effect::grainLfoIndex)
    std::atomic<LFO *> *_lfo_amt{};
};

#endif //GRAINSTORM_GRAINWAVESET_H
