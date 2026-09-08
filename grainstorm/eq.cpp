//
// Created by pr on 25.12.17.
//

#include "eq.h"
#include "grainstorm.h"
#include "defines.h"
#include "track.h"
#include "resample.h"
#include <cstdint>
#include <cmath>
#include "logger.h"

void Eq5::compute(MYFLOAT *in, int32_t size) {
    // helpbuf is sized to maxBufSize at construction. A host handing us a
    // larger block used to walk straight off the end of it; grow once instead.
    if (size > (int32_t) helpbuf.size()) helpbuf.resize(size);

    MYFLOAT *out = in;
    MYFLOAT *inbuf = in;
    std::memcpy(helpbuf.data(), in, sizeof(MYFLOAT) * size);

    MYFLOAT y0, tmpbuf[3];

    const uint32_t size_main_loop = size / 3;
    const uint32_t size_rest = size % 3;

    MYFLOAT vars[5][3];
    bool changed[5];

    for (int32_t b = 0; b < 5; b++) {
        vars[b][0] = LOG2NORMAL(_cf[b]->load());
        vars[b][1] = LOG2NORMAL2(_gn[b]->load());
        // Q is stored as 20*log10(q), so the inverse is LOG2NORMAL. This used
        // to be LOG2NORMAL2, which applied sqrt(q) and left the audio path
        // disagreeing with the curve drawn from Eq5::getCoeffs.
        vars[b][2] = LOG2NORMAL(_q[b]->load());
        changed[b] = DISTANCE(vars[b][0], _smooth[b][0]) > 1. ||
                     DISTANCE(vars[b][1], _smooth[b][1]) > 0.01 ||
                     DISTANCE(vars[b][2], _smooth[b][2]) > 0.01;
        // A band under the gate threshold is skipped in the inner loop - that
        // is the point of the gate - but its state still has to be snapped to
        // the target and its coefficients recomputed once. Without this
        // _smooth never converges and the band keeps a permanent sub-threshold
        // offset (up to 1 Hz and ~0.17 dB) that no later parameter change can
        // clear, and small parameter nudges do nothing at all.
        if (!changed[b] && (_smooth[b][0] != vars[b][0] || _smooth[b][1] != vars[b][1] ||
                            _smooth[b][2] != vars[b][2])) {
            _smooth[b][0] = vars[b][0];
            _smooth[b][1] = vars[b][1];
            _smooth[b][2] = vars[b][2];
            updateBand(b);
        }
    }

    for (uint32_t i = 0; i < size_main_loop; i++) {
        for (int32_t b = 0; b < 5; b++) if (changed[b]) smoothBand(b, vars);
/*
        MYFLOAT y0 = in0 + b1 * y1 + b2 * y2;
        ZXP(out) = a0 * y0 + a1 * y1 + a2 * y2;

        y2 = in1 + b1 * y0 + b2 * y1;
        ZXP(out) = a0 * y2 + a1 * y0 + a2 * y1;

        y1 = in2 + b1 * y2 + b2 * y0;
        ZXP(out) = a0 * y1 + a1 * y2 + a2 * y0;
        */
/* Low Shelf*/
        y0 = *(in++) + b1_l * y1_l + b2_l * y2_l;
        UDD(y0)
        tmpbuf[0] = a0_l * y0 + a1_l * y1_l + a2_l * y2_l;

        y2_l = *(in++) + b1_l * y0 + b2_l * y1_l;
        tmpbuf[1] = a0_l * y2_l + a1_l * y0 + a2_l * y1_l;

        y1_l = *(in++) + b1_l * y2_l + b2_l * y0;
        tmpbuf[2] = a0_l * y1_l + a1_l * y2_l + a2_l * y0;
/* Peak 0*/

        y0 = tmpbuf[0] + b1_0 * y1_0 + b2_0 * y2_0;
        UDD(y0)
        tmpbuf[0] = a0_0 * y0 + a1_0 * y1_0 + a2_0 * y2_0;

        y2_0 = tmpbuf[1] + b1_0 * y0 + b2_0 * y1_0;
        tmpbuf[1] = a0_0 * y2_0 + a1_0 * y0 + a2_0 * y1_0;

        y1_0 = tmpbuf[2] + b1_0 * y2_0 + b2_0 * y0;
        tmpbuf[2] = a0_0 * y1_0 + a1_0 * y2_0 + a2_0 * y0;
        /* Peak 1*/

        y0 = tmpbuf[0] + b1_1 * y1_1 + b2_1 * y2_1;
        UDD(y0)
        tmpbuf[0] = a0_1 * y0 + a1_1 * y1_1 + a2_1 * y2_1;

        y2_1 = tmpbuf[1] + b1_1 * y0 + b2_1 * y1_1;
        tmpbuf[1] = a0_1 * y2_1 + a1_1 * y0 + a2_1 * y1_1;

        y1_1 = tmpbuf[2] + b1_1 * y2_1 + b2_1 * y0;
        tmpbuf[2] = a0_1 * y1_1 + a1_1 * y2_1 + a2_1 * y0;
        /* Peak 2*/

        y0 = tmpbuf[0] + b1_2 * y1_2 + b2_2 * y2_2;
        UDD(y0)
        tmpbuf[0] = a0_2 * y0 + a1_2 * y1_2 + a2_2 * y2_2;

        y2_2 = tmpbuf[1] + b1_2 * y0 + b2_2 * y1_2;
        tmpbuf[1] = a0_2 * y2_2 + a1_2 * y0 + a2_2 * y1_2;

        y1_2 = tmpbuf[2] + b1_2 * y2_2 + b2_2 * y0;
        tmpbuf[2] = a0_2 * y1_2 + a1_2 * y2_2 + a2_2 * y0;
        /* High Shelf*/

        y0 = tmpbuf[0] + b1_h * y1_h + b2_h * y2_h;
        UDD(y0)
        *(out++) = (MYFLOAT) ((a0_h * y0 + a1_h * y1_h + a2_h * y2_h));

        y2_h = tmpbuf[1] + b1_h * y0 + b2_h * y1_h;
        *(out++) = (MYFLOAT) ((a0_h * y2_h + a1_h * y0 + a2_h * y1_h));

        y1_h = tmpbuf[2] + b1_h * y2_h + b2_h * y0;
        *(out++) = (MYFLOAT) ((a0_h * y1_h + a1_h * y2_h + a2_h * y0));
    }
    for (uint32_t i = 0; i < size_rest; i++) {
        for (int32_t b = 0; b < 5; b++) if (changed[b]) smoothBand(b, vars);

        y0 = *(in++) + b1_l * y1_l + b2_l * y2_l;
        UDD(y0)
        tmpbuf[0] = a0_l * y0 + a1_l * y1_l + a2_l * y2_l;
        y2_l = y1_l;
        y1_l = y0;

        y0 = tmpbuf[0] + b1_0 * y1_0 + b2_0 * y2_0;
        UDD(y0)
        tmpbuf[0] = a0_0 * y0 + a1_0 * y1_0 + a2_0 * y2_0;
        y2_0 = y1_0;
        y1_0 = y0;

        y0 = tmpbuf[0] + b1_1 * y1_1 + b2_1 * y2_1;
        UDD(y0)
        tmpbuf[0] = a0_1 * y0 + a1_1 * y1_1 + a2_1 * y2_1;
        y2_1 = y1_1;
        y1_1 = y0;

        y0 = tmpbuf[0] + b1_2 * y1_2 + b2_2 * y2_2;
        UDD(y0)
        tmpbuf[0] = a0_2 * y0 + a1_2 * y1_2 + a2_2 * y2_2;
        y2_2 = y1_2;
        y1_2 = y0;

        y0 = tmpbuf[0] + b1_h * y1_h + b2_h * y2_h;
        UDD(y0)
        *(out++) = (MYFLOAT) ((a0_h * y0 + a1_h * y1_h + a2_h * y2_h));
        y2_h = y1_h;
        y1_h = y0;
    }

    const MYFLOAT gg = dbToLinear60(*_gain);
    MYFLOAT mix;
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = 1.f;
    }
    const bool doSpectrum = _DATA->updateRenderThreadeq5[_track->index].load() == true && _chan == 0;
    if (!doSpectrum && _chan == 0)analyzer.reset();

    for (int32_t i = 0; i < size; i++) {
        inbuf[i] = (helpbuf[i] * (1.f - _smooth1) + inbuf[i] * _smooth2 * _smooth1);
        if(doSpectrum)analyzer.tick(inbuf[i]);
        sm1(mix);
        sm2(gg);
    }


    UDD(y1_l);
    UDD(y2_l);
    UDD(y1_h);
    UDD(y2_h);
    UDD(y1_0);
    UDD(y2_0);
    UDD(y1_1);
    UDD(y2_1);
    UDD(y1_2);
    UDD(y2_2);
}

void Eq10::compute(MYFLOAT *in, int32_t size) {
    if (size > (int32_t) helpbuf.size()) helpbuf.resize(size);

    MYFLOAT *inbuf = in;
    memcpy(helpbuf.data(), in, sizeof(MYFLOAT) * size);
    MYFLOAT y0, tmpbuf[3];

    uint32_t size_main_loop = size / 3;
    uint32_t size_rest = size % 3;


    MYFLOAT gains[10];
    bool changed[10];
    for (int32_t stage = 0; stage < 10; stage++) {
        // LOG2NORMAL2 (/40) = RBJ's A; the section gain is A*A, so the slider
        // dB is now what the band actually delivers. See the Eq10 constructor.
        gains[stage] = LOG2NORMAL2(gain[stage]->load());
        changed[stage] = DISTANCE(gains[stage], _smooth[stage]) >= 0.01;
    }


    MYFLOAT *out = in;
    const MYFLOAT qold = LOG2NORMAL(q->load());
    const bool qchanged = DISTANCE(qold, _smooth[10]) >= 0.01;
    for (uint32_t i = 0; i < size_main_loop; i++) {
        if (qchanged || changed[0])
            SetLowShelf(_STATE->sr, freqs[0], paramSmooth(qold, _smooth[10]),
                        paramSmooth(gains[0], _smooth[0]), a0[0],
                        a1[0], a2[0], b1[0], b2[0]);
        y0 = *(in++) + b1[0] * y1[0] + b2[0] * y2[0];
        UDD(y0)
        tmpbuf[0] = a0[0] * y0 + a1[0] * y1[0] + a2[0] * y2[0];

        y2[0] = *(in++) + b1[0] * y0 + b2[0] * y1[0];
        tmpbuf[1] = a0[0] * y2[0] + a1[0] * y0 + a2[0] * y1[0];

        y1[0] = *(in++) + b1[0] * y2[0] + b2[0] * y0;
        tmpbuf[2] = a0[0] * y1[0] + a1[0] * y2[0] + a2[0] * y0;

        for (int32_t stage = 1; stage < 9; stage++) {
            if (qchanged || changed[stage])
                SetPeaking(_STATE->sr, freqs[stage], _smooth[10],
                           paramSmooth(gains[stage], _smooth[stage]),
                           a0[stage],
                           a1[stage], a2[stage], b1[stage], b2[stage]);
            y0 = tmpbuf[0] + b1[stage] * y1[stage] + b2[stage] * y2[stage];
            UDD(y0)
            tmpbuf[0] = a0[stage] * y0 + a1[stage] * y1[stage] + a2[stage] * y2[stage];

            y2[stage] = tmpbuf[1] + b1[stage] * y0 + b2[stage] * y1[stage];
            tmpbuf[1] = a0[stage] * y2[stage] + a1[stage] * y0 + a2[stage] * y1[stage];

            y1[stage] = tmpbuf[2] + b1[stage] * y2[stage] + b2[stage] * y0;
            tmpbuf[2] = a0[stage] * y1[stage] + a1[stage] * y2[stage] + a2[stage] * y0;
        }
        if (qchanged || changed[9])
            SetHighShelf(_STATE->sr, freqs[9], _smooth[10], paramSmooth(gains[9], _smooth[9]),
                         a0[9],
                         a1[9], a2[9], b1[9], b2[9]);
        y0 = tmpbuf[0] + b1[9] * y1[9] + b2[9] * y2[9];
        UDD(y0)
        *(out++) = (MYFLOAT) (a0[9] * y0 + a1[9] * y1[9] + a2[9] * y2[9]);

        y2[9] = tmpbuf[1] + b1[9] * y0 + b2[9] * y1[9];
        *(out++) = (MYFLOAT) (a0[9] * y2[9] + a1[9] * y0 + a2[9] * y1[9]);

        y1[9] = tmpbuf[2] + b1[9] * y2[9] + b2[9] * y0;
        *(out++) = (MYFLOAT) (a0[9] * y1[9] + a1[9] * y2[9] + a2[9] * y0);
    }

    for (uint32_t i = 0; i < size_rest; i++) {
        if (qchanged || changed[0])
            SetLowShelf(_STATE->sr, freqs[0], paramSmooth(qold, _smooth[10]), paramSmooth(gains[0], _smooth[0]),
                        a0[0],
                        a1[0], a2[0], b1[0], b2[0]);
        y0 = *(in++) + b1[0] * y1[0] + b2[0] * y2[0];
        UDD(y0)
        tmpbuf[0] = a0[0] * y0 + a1[0] * y1[0] + a2[0] * y2[0];
        y2[0] = y1[0];
        y1[0] = y0;

        for (int32_t stage = 1; stage < 9; stage++) {
            if (qchanged || changed[stage])
                SetPeaking(_STATE->sr, freqs[stage], _smooth[10],
                           paramSmooth(gains[stage], _smooth[stage]),
                           a0[stage],
                           a1[stage], a2[stage], b1[stage], b2[stage]);
            y0 = tmpbuf[0] + b1[stage] * y1[stage] + b2[stage] * y2[stage];
            UDD(y0)
            tmpbuf[0] = a0[stage] * y0 + a1[stage] * y1[stage] + a2[stage] * y2[stage];
            y2[stage] = y1[stage];
            y1[stage] = y0;
        }
        if (qchanged || changed[9])
            SetHighShelf(_STATE->sr, freqs[9], _smooth[10], paramSmooth(gains[9], _smooth[9]), a0[9],
                         a1[9], a2[9], b1[9], b2[9]);
        y0 = tmpbuf[0] + b1[9] * y1[9] + b2[9] * y2[9];
        UDD(y0)
        *(out++) = (MYFLOAT) (a0[9] * y0 + a1[9] * y1[9] + a2[9] * y2[9]);
        y2[9] = y1[9];
        y1[9] = y0;
    }

    for (int32_t stage = 0; stage < 10; stage++) {
        UDD(y1[stage]);
        UDD(y2[stage]);
    }

    const MYFLOAT gg = dbToLinear60(*_gain);
    MYFLOAT mix;
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = 1.f;
    }

    for (int32_t i = 0; i < size; i++) {
        inbuf[i] = (helpbuf[i] * (1.f - _smooth1) + inbuf[i] * _smooth2 * _smooth1);
        sm1(mix);
        sm2(gg);
    }

}
