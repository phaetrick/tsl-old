#pragma once
//
// VOX — mono-FX FOF vowel synthesis (Csound fof-style granular formants).
// Continuous burst scheduler at a fundamental (fixed or pitch-followed).
// Each burst is a formant snapshot with linen-style rise/duration/decay
// (kris/kdur/kdec), bandwidth-derived exponential decay, per-burst
// glissando (kgliss, semitones) and octaviation (koct). Vowel morphs
// between the classic Csound appendix D vowel tables burst-to-burst.
// VOWEL and CPS are LFO destinations; VOWEL and WET are envelope-follower
// destinations.
//

#ifndef GRAINSTORM_VOX_H
#define GRAINSTORM_VOX_H

#include <atomic>
#include "base.h"

class Vox : public Effect {
public:
    Vox(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t size) override;

private:
    static constexpr int MAXEXC = 24;     // overlapping FOF bursts (xfund * kdur)
    static constexpr int NFORM = 5;

    struct Excitation {
        float phs[NFORM]{};
        float phinc[NFORM]{};    // per-burst formant snapshot
        float glissinc[NFORM]{}; // per-sample phase-inc ramp for kgliss
        float expamp[NFORM]{};   // bandwidth exponential decay state
        float decmult[NFORM]{};  // per-burst decay multipliers
        float amp{1.f};          // octaviation attenuation for this burst
        int32_t age{};
        int32_t attLen{};
        int32_t glissLen{};      // gliss ramp span; holds target after
        bool active{};
    };

    void updateFormants(int vc, float morph);

    Excitation _exc[MAXEXC];
    float _phinc[NFORM]{};     // current formant phase increments (spawn source)
    float _decmult[NFORM]{};   // current per-formant decay per sample
    float _spawnGain[NFORM]{}; // current per-formant linear gain
    int32_t _spawnCounter{};
    uint32_t _burstIndex{};    // for octaviation odd/even attenuation
    float _lastCps{220.f};
    float _lastMorph{-1.f};
    int _lastVc{-1};
    float _wetDbLast{-1000.f};
    float _wetLin{};

    const MYFLOAT *_sinewave{};
    float _mpidsr{};

    std::atomic<MYFLOAT> *_freq{}, *_vowel{}, *_voice{}, *_att{},
        *_wet{}, *_dry{}, *_follow{}, *_hold{}, *_pitchOut{},
        *_gliss{}, *_oct{};
    std::atomic<LFO *> *_lfo_vowel{}, *_lfo_freq{};
};

#endif //GRAINSTORM_VOX_H
