#pragma once
//
// VOWEL - the VOX2 formant filter bank, per grain.
//
// Five parallel unity-peak-gain reson filters (Csound reson, iscl=1) on the
// Csound appendix D vowel tables, exactly as in Vox2 - the difference is
// where it sits. As a grain effect the vowel is re-rolled between VOWEL MIN
// and VOWEL MAX once per grain, so a dense cloud is a room full of voices on
// different vowels rather than one voice; set MIN == MAX for a single vowel.
//
// The filter state carries across grains, which is what keeps the formants
// continuous instead of restarting on every grain edge.
//

#ifndef GRAINSTORM_GRAINVOWEL_H
#define GRAINSTORM_GRAINVOWEL_H

#include <atomic>
#include "base.h"

class GrainVowel : public Effect {
public:
    GrainVowel(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

private:
    static constexpr int NFORM = 5;

    void updateFormants(int vc, MYFLOAT morph, MYFLOAT bwScale);

    MYFLOAT _c1[NFORM]{};    // input scaling (unity gain at the formant centre)
    MYFLOAT _c2[NFORM]{};    // pole coefficient 2r cos(theta)
    MYFLOAT _c3[NFORM]{};    // pole coefficient r^2
    MYFLOAT _fgain[NFORM]{}; // per-formant linear gain (amp table)
    MYFLOAT _y1[NFORM]{}, _y2[NFORM]{};

    int _lastVc{-1};
    MYFLOAT _lastMorph{-1.}, _lastBwScale{-1.};
    MYFLOAT _mtpdsr{}, _tpidsr{};

    std::atomic<MYFLOAT> *_voice{}, *_vmin{}, *_vmax{}, *_bw{};
    // VOWEL moves the centre of the MIN/MAX window (the spread survives), BW is
    // a plain sample-and-hold. Both are per grain - see Effect::grainLfoIndex.
    std::atomic<LFO *> *_lfo_vowel{}, *_lfo_bw{};
};

#endif //GRAINSTORM_GRAINVOWEL_H
