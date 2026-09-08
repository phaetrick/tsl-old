#include <sys/types.h>
#include <cstdlib> // pulls in declaration of malloc, free
#include <cstring>
#include "app.h"
//
// Created by pr on 30.12.17.
//


#define OS 8u


#include "saturator.h"

static void compute(SATURATOR *p, MYFLOAT *in, uint32_t size);

static void destroy(SATURATOR *p);


template<typename T>
static void bilinear_transform(const T acoefs[], T dcoefs[], T fs) {
    T b0 = acoefs[0];
    T b1 = acoefs[1];
    T b2 = acoefs[2];
    T a0 = acoefs[3];
    T a1 = acoefs[4];
    T a2 = acoefs[5];

    T az0 = a2 * 4 * fs * fs + a1 * 2 * fs + a0;
    T bz2 = (b2 * 4 * fs * fs - b1 * 2 * fs + b0) / az0;
    T bz1 = (-b2 * 8 * fs * fs + 2 * b0) / az0;
    T bz0 = (b2 * 4 * fs * fs + b1 * 2 * fs + b0) / az0;
    T az2 = (a2 * 4 * fs * fs - a1 * 2 * fs + a0) / az0;
    T az1 = (-a2 * 8 * fs * fs + 2 * a0) / az0;

    dcoefs[0] = bz0;
    dcoefs[1] = bz1;
    dcoefs[2] = bz2;
    dcoefs[3] = az1;
    dcoefs[4] = az2;
}

static const MYFLOAT aacoefs[6][7] =
        {
                {2.60687e-05, 2.98697e-05, 2.60687e-05, -1.31885, 0.437162, 0.0, 0.0},
                {1,           -0.800256,   1,           -1.38301, 0.496576, 0.0, 0.0},
                {1,           -1.42083,    1,           -1.48787, 0.594413, 0.0, 0.0},
                {1,           -1.6374,     1,           -1.60688, 0.707142, 0.0, 0.0},
                {1,           -1.7261,     1,           -1.7253,  0.822156, 0.0, 0.0},
                {1,           -1.75999,    1,           -1.84111, 0.938811, 0.0, 0.0}
        };

static MYFLOAT scoeffs[6] = {0, 1, 0, 5 * TWOPI_P, 1, 0};

#include "track.h"

SATURATOR::SATURATOR(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_SATURATOR, MONOEFFECT), Balance(_STATE->sr) {
    _bypass = &_track->bypass[SPACE_SATURATOR];
    _drive = &_STATE->params[_track->index][SATDRIVE];
    _wet = &_STATE->params[_track->index][SATWET];
    _dry = &_STATE->params[_track->index][SATDRY];
    _drivesm = LOG2NORMALF(_drive->load());


    for (int32_t i = 0; i < 6; i++) {
        for (int32_t j = 0; j < 7; j++) {
            _aa[i][j] = aacoefs[i][j];
            _ai[i][j] = aacoefs[i][j];
        }
    }
    MYFLOAT zcoeffs[5];
    bilinear_transform(scoeffs, zcoeffs, _STATE->sr * OS);

    for (auto &i : _dcblocker) {
        for (int32_t j = 0; j < 5; j++)
            i[j] = zcoeffs[j];
        i[5] = 0.0;
        i[6] = 0.0;
    }
}

template<typename T>
static inline void quad_compute(T p[7], T input, T *output) {
    *output = p[5] + input * p[0];
    p[5] = p[6] + input * p[1] - *output * p[3];
    p[6] = input * p[2] - *output * p[4];
}


void SATURATOR::compute(MYFLOAT *in, int32_t size) {
    const bool bypass = (_bypass->load() || destroyRequested);

    MYFLOAT drive = LOG2NORMALF(*_drive);


    MYFLOAT wet, dry;
    if (bypass) {
        wet = 0.f;
        dry = 1.f;
    } else {
        wet = LOG2NORMALF(*_wet);
        dry = LOG2NORMALF(*_dry);
    }


//    MYFLOAT dcoffset = *_dcoffset;

    //MYFLOAT sig;
    MYFLOAT output = 0.0;

    for (int32_t n = 0; n < size; n++) {
        // Smoothed as a linear gain, matching how every effect here smooths
        // LOG2NORMALF(gain): a block-rate step on DRIVE is a step in level
        // straight into the saturator, i.e. a click.
        MYFLOAT fsignal = paramSmooth(drive, _drivesm) * in[n];
        UDD(fsignal)
        for (int32_t i = 0; i < OS; i++) {
            MYFLOAT usignal = (i == 0) ? OS * fsignal : 0.;
            for (auto &j : _ai)
                quad_compute(j, usignal, &usignal);
            MYFLOAT dsignal = (usignal) / (1. + fabs(usignal));

            quad_compute(_dcblocker[0], dsignal, &dsignal);
            quad_compute(_dcblocker[1], dsignal, &dsignal);

            for (auto &j : _aa)
                quad_compute(j, dsignal, &output);
        }
        UDD(output)
        // No third DC blocker here: _dcblocker[0..1] above are exact zeros at
        // z=1 running at sr*OS, x/(1+|x|) is odd so it generates no offset, and
        // decimation cannot create one. The old base-rate .99 stage was pure
        // bass loss - 76 Hz at 48 kHz, 153 Hz at 96 kHz.
        in[n] = in[n] * _smooth2 + tickBalance(output, in[n]) * _smooth1;
        UDD(in[n])
        smwetdry(wet, dry);
    }
}