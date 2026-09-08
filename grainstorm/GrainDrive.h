#pragma once
//
// DRIVE - per-grain waveshaper.
//
// The grain chain had no nonlinearity at all: saturation only existed after
// the overlap-add, where it acts on the finished cloud. Distorting each grain
// on its own is a different sound (every grain clips against its own peak, and
// the overlap sums the results), and it is the only place where the drive can
// be re-rolled per grain - DRIVE MIN/MAX is that range, the RINGMOD/RESON
// grain convention.
//
// Every shaper is normalised so that a full-scale input still maps to full
// scale, i.e. the character changes with DRIVE but the ceiling does not.
// ASYM and CHEBY produce DC, which the grain envelope would turn into a
// thump, so the wet path always ends in a DC blocker.
//

#ifndef GRAINSTORM_GRAINDRIVE_H
#define GRAINSTORM_GRAINDRIVE_H

#include <atomic>
#include "base.h"

class GrainDrive : public Effect {
public:
    GrainDrive(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

    enum {
        DRIVE_SOFT = 0,
        DRIVE_HARD,
        DRIVE_FOLD,
        DRIVE_ASYM,
        DRIVE_CHEBY
    };

private:
    static constexpr MYFLOAT ASYM_BIAS = .4;    // even-harmonic offset for ASYM

    MYFLOAT _dcx{}, _dcy{};                     // DC blocker state, kept across grains

    std::atomic<MYFLOAT> *_type{}, *_dmin{}, *_dmax{};
    // moves the centre of the MIN/MAX window, per grain (Effect::grainLfoIndex)
    std::atomic<LFO *> *_lfo_drive{};
};

#endif //GRAINSTORM_GRAINDRIVE_H
