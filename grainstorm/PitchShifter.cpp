//
// Created by pr on 06.08.20.
//

#include "PitchShifter.h"
#include "base.h"
#include "resample.h"
#include "track.h"
#include "app.h"
#include "grainstorm.h"

PITCHSHIFT::PITCHSHIFT(TRACK *t, int32_t channel) : Effect(t, channel, SPACE_SHIFTER, MONOEFFECT) {
    _bypass = &t->bypass[SPACE_SHIFTER];
    _mix = &_STATE->params[t->index][PITCHSHIFERMIX];
    _shift = &_STATE->params[t->index][PITCHSHIFTSHIFT];
    _mode = &_STATE->params[t->index][PITCHSHIFTMODE];
    _lfo = &t->lfo[PITCHSHIFTSHIFT];
}

void PITCHSHIFT::compute(MYFLOAT *in, int32_t size) {
    /*
    rs.downSample(in, outbuf.data(), size);
    rs.upSample(outbuf.data(), in, size/2);
    return;
*/
    MYFLOAT shift_const = *_shift;
    MYFLOAT shift = shift_const;
    auto &fol = _STATE->followerMap[_track->index].at(PITCHSHIFTSHIFT);
    auto src = fol.source.load();
    MYFLOAT *envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    auto env_on = fol.prepare(_chan);

    MYFLOAT range = 0.;
    LFO *lfo = *_lfo;
    bool lfo_on = lfo && lfo->power() && !env_on;
    if (lfo_on) {
        auto a = _STATE->controls[_track->index][PITCHSHIFTSHIFT].lfo_min.load();
        auto b = _STATE->controls[_track->index][PITCHSHIFTSHIFT].lfo_max.load();
        range = b-a;
        shift_const = a;
    }

    const auto mix = (*_bypass || destroyRequested) ? 0. : _mix->load();
    MYFLOAT shift1gain = *_mode == 1 ? .5 : 1.;
    MYFLOAT shift2gain = 1. - shift1gain;

    for (int32_t i = 0; i < size; i++) {
        if (env_on) {
            shift = _chan == 0 ? fol.detectL(envbuf[i]) : fol.detectR(envbuf[i]);
        }
        if (lfo_on) {
            shift = shift_const +
                    lfo->buf[i] * range;
        }
            _pitchUp.setShift(shift);
            _pitchDown.setShift(-shift);
            auto tmp = in[i];
            in[i] = in[i] * (1. - _smooth1) +
                    (shift1gain * _pitchUp.tick(tmp) + shift2gain * _pitchDown.tick(tmp)) *
                    _smooth1;
        sm1(mix);
    }
}
