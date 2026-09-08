#pragma once
//
// Created by pr on 10.09.23.
//

#ifndef GRAINSTORM_RESONBANK_H
#define GRAINSTORM_RESONBANK_H

#include "base.h"
#include "types.h"

class resonbank :public Effect{
public:
     resonbank(TRACK *_t) :Effect(_t, STEREOEFFECT, SPACE_RESONBANK){}
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;

private:
};


#endif //GRAINSTORM_RESONBANK_H
