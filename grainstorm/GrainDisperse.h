#pragma once
//
// DISPERSE - cascaded 2nd-order allpasses, per grain.
//
// An allpass chain leaves the magnitude spectrum alone and rearranges the
// signal in time: each stage delays a band around its own pole frequency more
// than the rest, so a grain comes out as a chirp - the "boing" of a spring, a
// drum shell, a tunnel. STAGES x DEPTH is the amount of dispersion; the stage
// frequencies are spread log-uniformly between FREQ MIN and FREQ MAX, so a
// wide span smears the whole band and a narrow one rings at a pitch.
//
// The chain state deliberately carries across grains - that is what turns a
// dense cloud into one continuous smear rather than a row of separate chirps.
// The input is windowed on the way in (the dry path is not), because the grain
// buffer is raw: feeding its discontinuous edges to a resonant chain would ring
// a click through every stage. Same trick as SimpleReverb.
//

#ifndef GRAINSTORM_GRAINDISPERSE_H
#define GRAINSTORM_GRAINDISPERSE_H

#include <atomic>
#include "base.h"

class GrainDisperse : public Effect {
public:
    GrainDisperse(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

    static constexpr int MAXSTAGES = 48;

private:
    void updateStages(int stages, MYFLOAT fmin, MYFLOAT fmax, MYFLOAT depth);

    MYFLOAT _a1[MAXSTAGES]{}, _a2[MAXSTAGES]{};
    MYFLOAT _x1[MAXSTAGES]{}, _x2[MAXSTAGES]{}, _y1[MAXSTAGES]{}, _y2[MAXSTAGES]{};

    int _stages{0};
    MYFLOAT _lastFmin{-1.}, _lastFmax{-1.}, _lastDepth{-1.};
    MYFLOAT _tpidsr{}, _sr{};

    std::atomic<MYFLOAT> *_nstages{}, *_fmin{}, *_fmax{}, *_depth{};
    // FREQ slides the whole MIN..MAX band and keeps its span; DEPTH is a plain
    // sample-and-hold. Both per grain - see Effect::grainLfoIndex.
    std::atomic<LFO *> *_lfo_freq{}, *_lfo_depth{};
};

#endif //GRAINSTORM_GRAINDISPERSE_H
