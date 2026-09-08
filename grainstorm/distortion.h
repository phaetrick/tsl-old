#pragma once
//
// Created by pr on 03.11.21.
//

#ifndef GRAINSTORM_DISTORTION_H
#define GRAINSTORM_DISTORTION_H

#include <tools.h>
#include "types.h"
#include "base.h"
#include "resample.h"


#define OVERSAMPLERATIO 2

class BitCrusher : public Effect, private PolyPhaseResampler2x<MYFLOAT>, private tsl::Balance<MYFLOAT>  {
public:
    BitCrusher(TRACK *_track, int32_t _channel);

    struct SRContext {
        MYFLOAT phase{};
        MYFLOAT last{};
    };


    // Sample and hold with a fractional period: count one per sub-sample and
    // latch whenever a full period has elapsed, carrying the remainder so the
    // hold length averages out to exactly `samples`. The old target/real pair
    // reset on every latch, so the fraction never accumulated and the period
    // was always round(samples) - asking for 2.5 gave exactly 3.
    template<typename T, typename T2>
    T samplereduction(T in, T2 samples) {
        // Reject anything that is not a usable period. A reversed LFO range can
        // take the rate through or below zero, and sr/rate is then negative,
        // NaN or infinite - the first two stall the accumulator, the last
        // freezes the hold on one sample. Written so NaN fails the test too.
        if (!(samples > 1. && samples < 1e6))
            return sr.last = in;
        if ((sr.phase += 1.) >= samples) {
            sr.phase -= samples;
            sr.last = in;
        }
        return sr.last;
    }

    // Truncating quantizer on a grid anchored at zero. For a whole-numbered
    // fact this is algebraically identical to the old floor((in+1)*fact)/fact-1
    // - measured bit-for-bit equal at every integer depth - but that form put
    // the levels at k/fact - 1, a grid that contains 0 only when fact is a whole
    // number. Once BITS went continuous, silence quantized to a DC offset of up
    // to a full step, and the offset jumped by half of full scale every time
    // fact crossed an integer: 0.5 at BITS 2, which is what crackled on a glide
    // or a modulated BITS at low depths.
    // Both ends of the clamp are load-bearing. It hard-clips a hot track (plus
    // the 2x upsampler's overshoot) to the quantizer's own range, and it bounds
    // the top level - in = 1 exactly would land one level above the intended
    // top. Working in double throughout also retires the old uint16_t cast,
    // which was undefined for negative input and gave -1, 0 and +3.3e7 across
    // three builds of this function.
    // The bottom level needs clamping too. floor(-fact)/fact is -ceil(fact)/fact,
    // which is exactly -1 for a whole-numbered fact but overshoots for anything
    // between: BITS 1.5 put the negative rail at -1.414, 3 dB past full scale.
    template<typename T, typename T2>
    static inline T bitreduction(T in, T2 fact) {
        in = MAX(-1., MIN(1. - 3.0517578125e-05, in));   // 1 - 2^-15
        return MAX(-1., floor(in * fact) / fact);
    }

    void compute(MYFLOAT *in, int32_t size);

private:
    std::atomic<MYFLOAT> *bits, *samples, *_tone;
    MYFLOAT _oldtone{};
    MYFLOAT xdc, ydc;
    MYFLOAT _dcb{};     // DC blocker pole, runs at sr * OVERSAMPLERATIO
    MYFLOAT _bitssm{};  // smoothed BITS, in the bit domain
    SRContext sr;
};

class DISTORT : public Effect, private PolyPhaseResampler2x<MYFLOAT>, private tsl::Balance<MYFLOAT>{
public:
    DISTORT(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t s);
private:
    std::atomic<MYFLOAT> *_drive{};     // 0..1 knob, see kDistDriveMin/Max
    std::atomic<MYFLOAT> *_postgain{};
    std::atomic<MYFLOAT> *_cut_off{};
    MYFLOAT _cut_off_prev{};
    MYFLOAT _xt{}, _yt{};
    MYFLOAT _dcb{};     // DC blocker pole, runs at the base rate
    MYFLOAT _ksm{};     // smoothed shaper gain
};


#endif //GRAINSTORM_DISTORTION_H
