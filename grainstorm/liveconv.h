#pragma once
//
// Created by pr on 15.05.23.
//

#ifndef GRAINSTORM_LIVECONV_H
#define GRAINSTORM_LIVECONV_H

#include <iostream>
#include <cmath>
#include "base.h"
#include "Convolver.h"

class LiveConvolver : public Effect{
public:
    LiveConvolver(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_LIVECONV, MONOEFFECT), convolverNonUniform(_STATE->sr){
        _modbuf.resize(_STATE->sr, 0);
        _tmpbuf.resize(_STATE->sr, 0);
        _smooth1 = 0.;
        _smooth2 = 1.;
        _oldsize = _STATE->params[_track->index][LIVECONVSIZE].load();
    }

    void compute(MYFLOAT *in, int32_t size) override;

private:
    int32_t _cnt{}, _cnt2{};
    ConvolverNonUniform convolverNonUniform;
    std::vector<MYFLOAT> _modbuf, _tmpbuf;
    MYFLOAT _oldsize;
};
#endif //GRAINSTORM_LIVECONV_H
