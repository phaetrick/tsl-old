#pragma once
//
// Created by pr on 15.08.20.
//

#ifndef GRAINSTORM_MODAL_H
#define GRAINSTORM_MODAL_H


#include <atomic>
#include <vector>
#include "base.h"
#include "types.h"

class StReson {
public:
    void prepare(MYFLOAT sr);
    void setup(MYFLOAT sr, MYFLOAT freq);
    void reset() {
        writepos = lpmem = apmem = 0;
    }


    inline MYFLOAT tick(MYFLOAT in) {
        int32_t rp = (vdt + writepos);
        if (rp >= delaybuf.size()) rp -= delaybuf.size();
        auto tmpo = delaybuf[rp];
        auto w = in + tmpo;
        auto s = (lpmem + w) * 0.5f;
        lpmem = w;
        auto ret = apmem + s * a;
        apmem = s - (ret * a);
        delaybuf[writepos++] = ret * .9f;
        if (writepos == delaybuf.size()) writepos = 0;
        return ret;
    }

private:
    std::vector<MYFLOAT> delaybuf;
    int32_t writepos{}, vdt;
    MYFLOAT lpmem{}, apmem{}, a;
};


struct Mode{
    void reset() {
        xnm1 = ynm1 = ynm2 = 0;
    };
    void setup(MYFLOAT sr, MYFLOAT freq, MYFLOAT q);
    inline MYFLOAT tick(MYFLOAT in){
        MYFLOAT xn =  in;

        MYFLOAT yn = a0 * xnm1 - a1 * ynm1 - a2 * ynm2;

        xnm1 = xn;
        ynm2 = ynm1;
        ynm1 = yn;

        yn = yn * d;

        return  yn;
    }

    MYFLOAT a0{}, a1{}, a2{}, d{}, xnm1{}, ynm1{}, ynm2{};
};

#define MODAL_MAX_CHANNELS 10

class GrainModal: public GrainEffect{
public:
    void prepare(TRACK *_track, int32_t chan);
    MYFLOAT tick(MYFLOAT in, int32_t offset, int) override ;
protected:
    MYFLOAT ingain, pregain, dry, wet;
    int32_t numChannels;
    Mode filters[MODAL_MAX_CHANNELS];
};

class Modal : public Effect, private GrainModal {
public:
    Modal(TRACK *track, int32_t chan): Effect(track, chan, SPACE_GRAINMODAL, GRAINEFFECT) {};
    void compute(MYFLOAT *in, int32_t size) override ;
};

class ModalEffect : public Effect{
public:
    ModalEffect(TRACK *_track, int32_t _chan);
    void compute(MYFLOAT *in, int32_t size) override ;
private:
    void check(MYFLOAT cps, MYFLOAT q, MYFLOAT mode);
    std::atomic<MYFLOAT> *q, *dry, *wet, *hold, *_follow, *_cpsold, *mode, *center;
    MYFLOAT oldmode{-1.f}, oldfreq{-1.f}, oldq{-1}, scale{0.1};
    int32_t numChannels{};
    Mode filters[MODAL_MAX_CHANNELS];
    int32_t pdetectindex{};
    MYFLOAT *_pdetectout;
};
#endif //GRAINSTORM_MODAL_H
