//
// Created by pr on 29.01.18.
//

#include <cstdint>
#include "logger.h"
#include <cmath>
#include <SkPath.h>
#include "Biquad.h"
#include "grainstorm.h"
#include "tools.h"
#include "random.h"
#include "synth.h"
#include "envelope_window.h"


const MYFLOAT  Q_INC =  (1.f/(20000.f / 3.f));
const MYFLOAT GAIN_INC =  (DISTANCE(LOG2NORMAL(-60.), LOG2NORMAL(60.))/(20000. / 3.));

void Reson::compute(MYFLOAT *in, MYFLOAT *out, uint32_t size) {
    if (out == nullptr)
        out = in;

    MYFLOAT *insrc = in;
    MYFLOAT _gain = 1.0, _mixsrc = 0;

    if (gain != nullptr) {
        _gain = dbToLinear60(gain->load());
    }

    if (mix != nullptr) {
        _gain *= mix->load();
        _mixsrc = 1.f - mix->load();
    }

    MYFLOAT y0;

    uint32_t size_main_loop = size / 3;
    uint32_t size_rest = size % 3;

    if ((center != nullptr && center->load() != center_saved) ||
        (q != nullptr && q->load() != q_saved)) {
        MYFLOAT fr = logarithmic ? powf(10, center->load() * .05f) : center->load();
        MYFLOAT bw = 0.01f + 0.99 * q->load();
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (DISTANCE(fr, centerold) < 3) {
                centerold = fr;
                center_saved = center->load();
            } else if (centerold < fr)
                centerold += 3;
            else
                centerold -= 3;

            if (DISTANCE(bw, qold) < Q_INC) {
                qold = bw;
                q_saved = q->load();
            } else if (qold < bw)
                qold += Q_INC;
            else
                qold -= Q_INC;
            Setup();

            y0 = *(in++) + b1 * y11 + b2 * y21;
            *(out++) = (MYFLOAT) ((a0 * (y0 - y21)) * _gain + *(insrc++) * _mixsrc);

            y21 = *(in++) + b1 * y0 + b2 * y11;
            *(out++) = (MYFLOAT) ((a0 * (y21 - y11)) * _gain + *(insrc++) * _mixsrc);

            y11 = *(in++) + b1 * y21 + b2 * y0;
            *(out++) = (MYFLOAT) ((a0 * (y11 - y0)) * _gain + *(insrc++) * _mixsrc);
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (DISTANCE(fr, centerold) < 3) {
                centerold = fr;
                center_saved = center->load();
            } else if (centerold < fr)
                centerold += 3;
            else
                centerold -= 3;

            if (DISTANCE(bw, qold) < Q_INC) {
                qold = bw;
                q_saved = q->load();
            } else if (qold < bw)
                qold += Q_INC;
            else
                qold -= Q_INC;
            Setup();

            y0 = *(in++) + b1 * y11 + b2 * y21;
            *(out++) = (MYFLOAT) ((a0 * (y0 - y21)) * _gain + *(insrc++) * _mixsrc);
            y21 = y11;
            y11 = y0;

        }
    } else {
        for (uint32_t i = 0; i < size_main_loop; i++) {
            y0 = *(in++) + b1 * y11 + b2 * y21;
            *(out++) = (MYFLOAT) ((a0 * (y0 - y21)) * _gain + *(insrc++) * _mixsrc);

            y21 = *(in++) + b1 * y0 + b2 * y11;
            *(out++) = (MYFLOAT) ((a0 * (y21 - y11)) * _gain + *(insrc++) * _mixsrc);

            y11 = *(in++) + b1 * y21 + b2 * y0;
            *(out++) = (MYFLOAT) ((a0 * (y11 - y0)) * _gain + *(insrc++) * _mixsrc);
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            y0 = *(in++) + b1 * y11 + b2 * y21;
            *(out++) = (MYFLOAT) ((a0 * (y0 - y21)) * _gain + *(insrc++) * _mixsrc);
            y21 = y11;
            y11 = y0;

        }
    }
    UDD(y11);
    UDD(y21);
    // LOGE("%g %g %g", pidsr, y11, y21);

}

void Reson::ComputeGrain(Reson *xx, MYFLOAT *in, uint32_t size) {
    if (xx->bypass->load())
        return;
    MYFLOAT centermin = powf(10, xx->centermin->load() * .05);
    MYFLOAT centermax = powf(10, xx->centermax->load() * .05);
    MYFLOAT qmin = pow(10, xx->qmin->load() * .05);
    MYFLOAT qmax = pow(10, xx->qmax->load() * .05);

    MYFLOAT center = tsl::random::randomfloat(centermin, centermax);
    MYFLOAT q = tsl::random::randomfloat(qmin, qmax);
    //xx->Reset();
    xx->Setup(center, q);

    MYFLOAT mix = xx->mixgrain->load();
    MYFLOAT mixsrc = 1.0f - mix;

    MYFLOAT gain = dbToLinear60(xx->gaingrain->load());

    MYFLOAT maxsrc = *std::max_element(in, in + size);
    xx->compute(in, in, size);
    MYFLOAT maxout = *std::max_element(in, in + size);
    MYFLOAT adjust = maxout > 0.f ? maxsrc / maxout * gain : 0.f;
    //LOGE("%g %g %f", center, q, adjust);

    for (int32_t i = 0; i < size; i++) {
        in[i] *= adjust;
    }
}

Reson *
Reson::initGrain(int32_t sr, std::atomic<MYFLOAT> *centermin, std::atomic<MYFLOAT> *centermax,
                 std::atomic<MYFLOAT> *qmin, std::atomic<MYFLOAT> *qmax,
                 std::atomic<MYFLOAT> *mix,
                 std::atomic<MYFLOAT> *gain, std::atomic<bool> *bypass) {
    Reson *b = new Reson(sr, nullptr, nullptr, nullptr, nullptr, false);
    b->bypass = bypass;
    b->mixgrain = mix;
    b->gaingrain = gain;
    b->centermin = centermin;
    b->centermax = centermax;
    b->qmin = qmin;
    b->qmax = qmax;
    return b;
}


ButterLPHP::ButterLPHP(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_HPLP, MONOEFFECT), filter(t->_appState, &_STATE->params[t->index][LPHPHPCUT],
                                                                     &_STATE->params[t->index][LPHPLPCUT],
                                                                     nullptr, nullptr, nullptr,
                                                                     nullptr,
                                                                     true) {
    _bypass = &t->bypass[SPACE_HPLP];
    _mix = &_STATE->params[t->index][LPHPMIX];
    _gain = &_STATE->params[t->index][LPHPGAIN];
    _smooth2 = dbToLinear60(*_gain);
    helpbuf.resize(_STATE->maxBufSize);
};

void ButterLPHP::compute(MYFLOAT *in, int32_t size) {
    memcpy(helpbuf.data(), in, sizeof(MYFLOAT) * size);
    MYFLOAT mix, gain = dbToLinear60(*_gain);
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = *_mix;
    }
    filter.HpLp24(in, in, size);
    for (int32_t i = 0; i < size; i++) {
        in[i] = helpbuf[i] * (1.f - _smooth1) + in[i] * _smooth1 * _smooth2;
        smmixgain(mix, gain);
    }
}


ButterBR::ButterBR(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_BANDREJECT, MONOEFFECT),
                                         filter(t->_appState, nullptr, nullptr, &_STATE->params[t->index][BRCENTER],
                                                &_STATE->params[t->index][BRBW],
                                                &_STATE->params[t->index][BRCENTER], &_STATE->params[t->index][BRBW],
                                                true) {
    _bypass = &t->bypass[SPACE_BANDREJECT];
    _gain = &_STATE->params[t->index][BRGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _mix = &_STATE->params[t->index][BRMIX];
    helpbuf.resize(_STATE->maxBufSize);
};

void ButterBR::compute(MYFLOAT *in, int32_t size) {
    std::memcpy(helpbuf.data(), in, sizeof(MYFLOAT) * size);
    MYFLOAT mix, gain = dbToLinear60(*_gain);
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = *_mix;
    }

    filter.Br24(in, in, size);
    for (int32_t i = 0; i < size; i++) {
        in[i] = helpbuf[i] * (1.f - _smooth1) + in[i] * _smooth1 * _smooth2;
        smmixgain(mix, gain);
    }
}
