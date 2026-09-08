#pragma once
#ifndef FILTER_H
#define FILTER_H

#include <cstdint>
#include <atomic>
#include <cstring>
#include <cmath>
#include <vector>
#include "types.h"
#include "defines.h"
#include "base.h"
class BiquadBandpassFilter {
public:
    BiquadBandpassFilter(double sr)
            : z1(0.0), z2(0.0) {
        twopidsr = TWOPI_P / sr;
    }

    inline double tick(double input) {
        double output = b0 * input + z1;
        z1 = z2 - a1 * output;
        z2 = b2 * input - a2 * output;
        return output;
    }

    void calculateCoefficients(double centerFreq, double bandwidth) {
        double omega0 = centerFreq * twopidsr;
        double alpha = std::sin(omega0) / (2.0 * bandwidth);

        b0 = alpha;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * std::cos(omega0);
        a2 = 1.0 - alpha;

        // Normalize the coefficients
        b0 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }
private:
    double twopidsr;
    double b0, b2, a0, a1, a2;
    double z1, z2;
};


class RESONGRAIN;

class RESON : public Effect{
	friend RESONGRAIN;
public:
	RESON(TRACK *t, int32_t chan, bool isGrain = false);
	virtual void compute(MYFLOAT *in, int32_t size)override ;
    void reset(){
		MYFLOAT b = 2. - cos(10 * _twopidsr);
		_c2 = b - sqrt(b * b - 1.);
		_c1 = 1. - _c2;
		//_prvq = _prvr = _prva = 0.0;
		_offset = 0;
    };
private:
    std::vector<MYFLOAT> _temp_buffer;
    long _offset{};
	std::atomic<LFO*> *_lfo_freq{};
    std::atomic<LFO*> *_lfo_bw{};
    MYFLOAT _minuspidsr{};
    MYFLOAT _twopidsr{};
	std::atomic<MYFLOAT> *_freq{}, *_bw{};
   std::atomic<MYFLOAT> *_freq_min{}, *_freq_max{}, *_bw_min{}, *_bw_max{};
	MYFLOAT _c1{}, _c2{};
    MYFLOAT _x1{},_x2{},_y1{},_y2{};
};

class RESONGRAIN : public RESON {
public:
	RESONGRAIN(TRACK *t, int32_t chan);
	void compute(MYFLOAT *in, int32_t size)override ;
};



struct RINGM{
    void *p;
    void *track;
    bool active;
    uint32_t sr;
    MYFLOAT freq;
    MYFLOAT attack;
    MYFLOAT decay;
    MYFLOAT gain;
    MYFLOAT mix;
    MYFLOAT minuspidsr;
    MYFLOAT twopidsr;
    MYFLOAT saved_freq;
    MYFLOAT saved_attack;
    MYFLOAT saved_decay;
    MYFLOAT b01;
	MYFLOAT b02;
	MYFLOAT y01;
	MYFLOAT y02;
	MYFLOAT b11;
	MYFLOAT b12;
	MYFLOAT y11;
	MYFLOAT y12;
	void (*init)(void *filter);
	void (*sleep)(void *filter);
    void (*destroy)(void *filter);
    void (*reset)(void *filter);
    void (*compute)(void *filter, MYFLOAT *in, uint32_t size);
    void (*filter)(void *filter, MYFLOAT *in, uint32_t size);
    RINGM(){memset(this, 0, sizeof(RINGM));}
};

class DCBLOCKER : public Effect{
public:
	DCBLOCKER(TRACK *track);
	void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;
	void compute(MYFLOAT *in, int32_t size)override ;

private:
    MYFLOAT _xtl{}, _xtr{}, _ytl{}, _ytr{};
};

class MOOGLADDER : public Effect{
public:
    MOOGLADDER(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t s) override ;
	MYFLOAT *_out{};
	MYFLOAT *_in{};
	std::atomic<MYFLOAT> *_freq;
	std::atomic<MYFLOAT> *_res;
	MYFLOAT _oldfreq, _oldres;
	MYFLOAT _delay[6] {};
	MYFLOAT _tanhstg[3] {};
	MYFLOAT _tune{}, _res4{};
};





#endif