#pragma once
//
// PLUCK - Karplus-Strong string excited by the grain.
//
// MODAL is a bank you tune; this is one tuned delay, an order of magnitude
// cheaper, so it is affordable at the densities where the grain chain actually
// lives. The grain lights the string and the string keeps ringing into the
// grains that follow, which is how a noisy source turns into a pitched cloud.
//
// Two things the grain context forces:
//  - the excitation is windowed on the way in (the dry path is not), because
//    the raw grain edges would inject a step into the loop and ring as a click;
//  - the tuning is re-rolled per grain between TUNE MIN and TUNE MAX, and the
//    delay length glides to the new value across the grain rather than jumping
//    - a step change in a ringing delay line is a click, a glide is a slide.
//
// Note that the tail is amplitude-modulated by the grain envelope of every
// grain it survives into: at high density that reads as sustain, at low density
// as a tremolo at the grain rate. That is inherent to where the effect sits.
//

#ifndef GRAINSTORM_GRAINPLUCK_H
#define GRAINSTORM_GRAINPLUCK_H

#include <atomic>
#include <vector>
#include "base.h"

class GrainPluck : public Effect {
public:
    GrainPluck(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

    static constexpr MYFLOAT MIN_FREQ = 20.;    // sets the delay line length
    static constexpr MYFLOAT MAX_FREQ = 4000.;

private:
    std::vector<MYFLOAT> _buf;
    int32_t _mask{0};       // _buf.size() is a power of two
    int32_t _wpos{0};
    MYFLOAT _delay{0.};     // current delay length in samples
    MYFLOAT _lp{0.};        // one-pole loss filter in the loop

    std::atomic<MYFLOAT> *_fmin{}, *_fmax{}, *_decay{}, *_damp{};
    // moves the centre of the TUNE window, per grain (Effect::grainLfoIndex)
    std::atomic<LFO *> *_lfo_tune{};
};

#endif //GRAINSTORM_GRAINPLUCK_H
