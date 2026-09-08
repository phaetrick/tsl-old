#pragma once
//
// Created by pr on 30.12.17.
//

#ifndef GRAINSTORM_SATURATOR_H
#define GRAINSTORM_SATURATOR_H


#include <atomic>
#include "types.h"
#include "base.h"
#include <tools.h>

class SATURATOR : public Effect , private tsl::Balance<MYFLOAT>
{
public:
SATURATOR(TRACK *t, int32_t chan);
    void compute(MYFLOAT *in, int32_t s)override ;
private:
    std::atomic<MYFLOAT> *_drive, *_wet, *_dry;
    MYFLOAT _dcblocker[2][7]{};
    MYFLOAT _ai[6][7]{};
    MYFLOAT _aa[6][7]{};
    MYFLOAT _drivesm{};     // smoothed DRIVE, as a linear gain
};

#endif //GRAINSTORM_SATURATOR_H
