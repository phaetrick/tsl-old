#pragma once
//
// VOX2 — mono-FX formant filter bank (vowel filter).
// The input signal runs through five parallel unity-peak-gain reson
// filters (Csound reson, iscl=1) tuned to the classic Csound appendix D
// vowel tables and summed with the per-formant amp weights, so the
// track material itself "speaks" — the complement to VOX, which
// resynthesizes a voice via FOF bursts. Vowel morphs between the vowel
// tables like VOX; BW scales all formant bandwidths.
// VOWEL and BW are LFO destinations; VOWEL and WET are
// envelope-follower destinations.
//

#ifndef GRAINSTORM_VOX2_H
#define GRAINSTORM_VOX2_H

#include <atomic>
#include "base.h"

class Vox2 : public Effect {
public:
    Vox2(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

private:
    static constexpr int NFORM = 5;

    void updateFormants(int vc, float morph, float bwScale);

    MYFLOAT _c1[NFORM]{};    // input scaling (unity gain at the formant centre)
    MYFLOAT _c2[NFORM]{};    // pole coefficient 2r cos(theta)
    MYFLOAT _c3[NFORM]{};    // pole coefficient r^2
    MYFLOAT _fgain[NFORM]{}; // per-formant linear gain (amp table)
    MYFLOAT _y1[NFORM]{}, _y2[NFORM]{};

    float _lastMorph{-1.f};
    float _lastBwScale{-1.f};
    int _lastVc{-1};
    float _wetDbLast{-1000.f};
    float _wetLin{};
    MYFLOAT _mtpdsr{}, _tpidsr{};

    std::atomic<MYFLOAT> *_vowel{}, *_voice{}, *_bw{}, *_wet{}, *_dry{};
    std::atomic<LFO *> *_lfo_vowel{}, *_lfo_bw{};
};

#endif //GRAINSTORM_VOX2_H
