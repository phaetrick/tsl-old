#pragma once
#ifndef PHASER_H
#define PHASER_H

#include <cstdint>
#include <atomic>
#include <cstring>
#include "types.h"
#include "base.h"
#include "defines.h"


#define MAX_NUM_NOTCHES 64

class PHASER : public Effect{
public:
    PHASER(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t s)override ;
private:
    void reset(){
        _fb = 0.;
        std::memset(_xarray, 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
        std::memset(_yarray, 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
    }
    std::atomic<MYFLOAT> *_freq{};
    MYFLOAT _fb{};
    std::atomic<MYFLOAT> *_feedback{};
    std::atomic<MYFLOAT> *_notches{};
    MYFLOAT _prevnotches{};
    MYFLOAT _xarray[MAX_NUM_NOTCHES]{};
    MYFLOAT _yarray[MAX_NUM_NOTCHES]{};
};

class PHASER4 : public Effect {
public:
    PHASER4(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t s)override ;
private:
    std::atomic<LFO*> *_lfofreq{};
    std::atomic<LFO*> *_lfospacing{};
    std::atomic<MYFLOAT> *_spacing{};
    std::atomic<MYFLOAT> *_radius{};
    std::atomic<MYFLOAT> *_freq{};
    MYFLOAT _fb{};
    std::atomic<MYFLOAT> *_feedback{};
    std::atomic<MYFLOAT> *_stages{};
    MYFLOAT _oldstages;
    MYFLOAT _xarray[MAX_NUM_NOTCHES]{};
    MYFLOAT _yarray[MAX_NUM_NOTCHES]{};
    void reset(){
        _fb = 0.;
        std::memset(_xarray, 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
        std::memset(_yarray, 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
    }
};


template<class T>
struct StereoSample {
public:
    StereoSample(const T &left, const T &right) :l(left), r(right)
    {};

    /**Initializes Stereo with left and right set to val
     * @param val the value for both channels*/
    StereoSample(const T &val):l(val), r(val)
    {};
    ~StereoSample() {}

    StereoSample<T> &operator=(const StereoSample<T> &smp){
        l = smp.l;
        r = smp.r;
        return *this;
    };

    //data
    T l, r;
};

#define MAX_PHASER_STAGES 12

class Phaser:public Effect
{
public:
    Phaser(TRACK *);
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;
private:
    void reset(){
        _fb[0] = _fb[1] = 0.;
        std::memset(_xarray[0], 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
        std::memset(_yarray[0], 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
        std::memset(_xarray[1], 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
        std::memset(_yarray[1], 0, sizeof(MYFLOAT) * MAX_NUM_NOTCHES);
    }
    MYFLOAT _fb[2]{};
    MYFLOAT _xarray[2][MAX_NUM_NOTCHES]{};
    MYFLOAT _yarray[2][MAX_NUM_NOTCHES]{};
    const int32_t stages = 6;
    std::atomic<MYFLOAT> *_phase_offset{};
    std::atomic<MYFLOAT> *_feedback{}, *_mix;
    MYFLOAT *_wave;
};

class ROTARY_SSB : public Effect{
public:
    ROTARY_SSB(TRACK *track);
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;

private:
    int32_t channel{};
    std::atomic<MYFLOAT> *_pos{};
    MYFLOAT _twopidsr{};
    MYFLOAT _coef[12]{};
    MYFLOAT _xarray[MAX_CHANNELS][12]{};
    MYFLOAT _yarray[MAX_CHANNELS][12]{};
    MYFLOAT _phase{};
    MYFLOAT _xt[MAX_CHANNELS][2][4]{}, _ytupperl{}, _ytlowerl{}, _ytupperr{}, _ytlowerr{};
};

class PHASER_SSB : public Effect{
public:
    PHASER_SSB(TRACK *track);
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;
private:
    int32_t channel{};
    std::atomic<MYFLOAT> *_lp_cut{}, *_hp_cut{};
    MYFLOAT _prev_lp_cut{}, _prev_hp_cut{};
    MYFLOAT _coef[12]{};
    MYFLOAT _xarray[12]{};
    MYFLOAT _yarray[12]{};
    MYFLOAT _y1_h_left{}, _y2_h_left{}, _y1_h_right{}, _y2_h_right{}, _a0_h{}, _b1_h{}, _b2_h{};
    MYFLOAT _y1_l_left{}, _y2_l_left{}, _y1_l_right{}, _y2_l_right{}, _a0_l{}, _b1_l{}, _b2_l{};
};
#define UPPER_SIDEBAND 0
#define LOWER_SIDEBAND 1
#define DOUBLE_SIDEBAND 2

class FreqShift : public Effect{
public:
    FreqShift(TRACK *_track, int32_t _channel);
    void compute(MYFLOAT *in, int32_t s)override ;
private:
    MYFLOAT _coef[12]{};
    MYFLOAT _xarray[12]{};
    MYFLOAT _yarray[12]{};
    std::atomic<MYFLOAT>*_type{};
    std::atomic<MYFLOAT> *_freq{};
    MYFLOAT _oscphase{};
    MYFLOAT *_sinewave{};
};

#endif