#pragma once
//
// Created by pr on 20.12.17.
//

#ifndef GRAINSTORM_DELAY_H
#define GRAINSTORM_DELAY_H


#include "types.h"
#include <atomic>
#include "base.h"
#include "ControlItem.h"
#include "DelayBase.h"
#include "PitchShifter.h"
#include "Butterworth.h"


class TapDelay : public Effect{

public:
    TapDelay(TRACK *track, int32_t chan);
    TapDelay(TRACK *track, int32_t chan, int offset);

    void compute(MYFLOAT *in, int32_t size)override ;
    void computeMdelay(MYFLOAT *in, MYFLOAT *out, MYFLOAT *out2, int32_t size);
    void clear();
    enum DelayMode{
        DELAYMODENORMAL = 0,
        DELAYMODEREVERSE
    };
private:
    Butter6<MYFLOAT> prevBut, nextBut;
    std::vector<MYFLOAT> _delayLine{}, _delayLineRev{};
    int32_t _writepos{}, mask{}, mask2{}, writePosRev{}, prevRevPos{}, nextRevPos{};
    PitchShiftCorr<MYFLOAT> pitchShiftPrev, pitchShiftNext;
    std::atomic<MYFLOAT> *_feedback{};
    std::atomic<MYFLOAT> *_dry{};
    std::atomic<MYFLOAT> *_wet{};
    std::atomic<MYFLOAT> *_delay{};
    int32_t prevDelay{}, nextDelay{};
    int32_t prevMode{}, nextMode{};
    const MYFLOAT inc;
    MYFLOAT crossFade{1.0};
    int32_t minDelay{};
    std::atomic<MYFLOAT> *_lp_cut{}, *_hp_cut{}, *_shift, *_shiftmix;
    MYFLOAT _shiftold, _shiftmixold;
    MYFLOAT _prev_lp_cut = -1, _prev_hp_cut = -1;
    std::atomic<MYFLOAT> *_hold{};
    std::atomic<MYFLOAT> *_backw{};
    bool prevHold{}, nextHold{};
    MYFLOAT buf[16]{};
    int32_t lfoDataOffset{};
    Follower *fol{};
};



class MDELAY : public Effect {
public:
    MDELAY(TRACK *track, int32_t chan);

    void compute(MYFLOAT *in, int32_t size)override ;

    std::vector<MYFLOAT> _buffer, _buffer2;
    std::unique_ptr<TapDelay> _delays[MAX_DELAYS];
    std::atomic<MYFLOAT> *_mode{};
    bool _active_prev[MAX_DELAYS]{};
};

class PingPongDelay : public Effect{

public:
    PingPongDelay(TRACK *track);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;
    void clear();
private:
    int32_t lfoDataIndex{};
    Butter6<MYFLOAT> prevBut, nextBut;
    // Second damping/transposing stage, for the R hop. One in each of the two
    // lines means every repeat is filtered and shifted exactly once, the way the
    // mono delay does it - with a single stage the L->R->L round trip left every
    // other repeat (starting with the first) untouched.
    Butter6<MYFLOAT> prevButR, nextButR;
    LFO *prevLFO{}, *nextLFO{};
    MYFLOAT prevLFORange{}, nextLFORange{};
    MYFLOAT prevLFOStart{}, nextLFOStart{};
    std::vector<MYFLOAT> _delayLineL{},_delayLineR{}, _delayLineRev{};
    int32_t _writepos{}, mask{}, mask2{}, writePosRev{}, prevRevPos{}, nextRevPos{};
    PitchShiftCorr<MYFLOAT> pitchShiftPrev, pitchShiftNext;
    PitchShiftCorr<MYFLOAT> pitchShiftPrevR, pitchShiftNextR;
    std::atomic<MYFLOAT> *_feedback{};
    std::atomic<MYFLOAT> *_dry{};
    std::atomic<MYFLOAT> *_wet{};
    std::atomic<MYFLOAT> *_delay{};
    int32_t prevDelay{}, nextDelay{};
    int32_t prevMode{}, nextMode{};
    const MYFLOAT inc;
    MYFLOAT crossFade{1.0};
    int32_t minDelay{};
    std::atomic<MYFLOAT> *_lp_cut{}, *_hp_cut{}, *_shift, *_shiftmix;
    MYFLOAT _shiftold, _shiftmixold;
    MYFLOAT _prev_lp_cut = -1, _prev_hp_cut = -1;
    std::atomic<MYFLOAT> *_hold{};
    std::atomic<MYFLOAT> *_backw{};
    bool prevHold{}, nextHold{};


    MYFLOAT buf[16]{};
};

class FLANGER : public Effect, private WindowedSincDelay<> {
public:
    FLANGER(TRACK *t, int32_t chan);

    void compute(MYFLOAT *in, int32_t size)override ;

private:
    int32_t _index{};
    std::atomic<MYFLOAT> *_feedback{};
    MYFLOAT _yt1{};
    uint32_t  _max_delay{};
    std::atomic<MYFLOAT> *_delay{};
    MYFLOAT _phase{};
};

#endif //GRAINSTORM_DELAY_H
