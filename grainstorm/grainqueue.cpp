//
// Created by pr on 21.10.20.
//

#include "grainqueue.h"
#include "grainstorm.h"
#include "vco.h"
#include "moogladder.h"
#include "app.h"

void ResonGrain::prepare(TRACK *_track) {
    auto _appState = _track->_appState;

    if (_track->bypass[SPACE_GRAIN_BP].load()) {
        gain = 1.f;
        mix = 0.f;
    } else {
        gain = dbToLinear60(_STATE->params[_track->index][GRAINBPGAIN]);
        mix = _STATE->params[_track->index][GRAINBPMIX];
    }

    MYFLOAT cf =
            pow(10., .05 * tsl::random::randomfloat(_STATE->params[_track->index][GRAINBPCENTERMIN].load(),
                                        _STATE->params[_track->index][GRAINBPCENTERMAX].load()));

    MYFLOAT bw_pre = pow(10, .05 * tsl::random::randomfloat(_STATE->params[_track->index][GRAINBPBWMIN].load(),
                                               _STATE->params[_track->index][GRAINBPBWMAX].load()));

    MYFLOAT bw = cf * bw_pre;

    r = exp(bw * -_STATE->pidsr);
    c1 = 2.f * r * cos(cf * _STATE->twopidsr);
    c2 = r * r;

    x1 = x2 = y1 = y2 = 0;
}

MYFLOAT ResonGrain::tick(MYFLOAT in, int, int) {
    auto y = (1. - r) * (in - r * x2) + c1 * y1 - c2 * y2;
    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = y;
    return (in * (1.f - mix) + y * mix) * gain;
}

MYFLOAT GrainReverb::_tdiff1[4]{
        20346e-6f,
        24421e-6f,
        31604e-6f,
        27333e-6f,
};
MYFLOAT GrainReverb::_tdelay[4]{
        153129e-6f,
        210389e-6f,
        127837e-6f,
        256891e-6f,
};

void GrainReverb::prepare(TRACK *track, int32_t chan, int size) {
    auto _appState = track->_appState;
    bool integerenvcycles = _STATE->params[track->index][INTEGERENVCYCLES].load() == 1.0;

    MYFLOAT window_freq_anal_const = integerenvcycles ? (int) _STATE->params[track->index][AWINCYLCES].load()
                                                    : _STATE->params[track->index][AWINCYLCES].load();
    sl = (double) (((WINDOW_SIZE - 1) * (long double) window_freq_anal_const +
                    window_freq_anal_const - 0.94) /
                   ((long double) size - 1));
    sp = 0.;
    auto tindex = track->index;
    //window = track->grainenv[GASENV];

    gain = dbToLinear60(_STATE->params[track->index][GRAINREVERBGAIN]);
    mix = _STATE->params[track->index][GRAINREVERBMIX];
    for (int32_t i = 0; i < 4; i++) {
        {
            _diff1[i].reset();
            _delay[i].reset();
            _filtstate[i] = 0;
        }
    }
    init(_STATE->sr, chan);
    setT60(_STATE->sr, _STATE->params[track->index][GRAINREVERBDECAY] * 20);
}


void GrainReverb::setT60(MYFLOAT sr, MYFLOAT T60) {
    if (T60 == 0) {
        for (int32_t i = 0; i < 4; i++) {
            _feedback[i] = 0;
        }
        _gainfact = 1.f;
    } else {
        for (int32_t i = 0; i < 4; i++) {
            _feedback[i] = powf(10.0, (-3.0f * (int) (floorf(_tdelay[i] * sr + 0.5f)) /
                                       (T60 * sr))) * .5f;
        }
        _gainfact = 1 / (powf(0.001f, (_averagedelay) / (T60 * sr)) * 2.f);
    }
    if (_gainfact > 1)
        _gainfact = 1;
}

void GrainReverb::init(MYFLOAT sr, int32_t chan) {
    MYFLOAT sum = 0;
    for (int32_t i = 0; i < 4; i++) {
        int32_t k1 = (int) ((floorf(_tdiff1[i] * sr + 0.5f)) * (chan == 1 ? 1. : 1.01f));
        int32_t k2 = (int) ((floorf(_tdelay[i] * sr + 0.5f)) * (chan == 1 ? 1. : 1.01f));
        _diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
        _delay[i].init(k2 - k1);
        sum += k2;
    }
    _averagedelay = sum / 4.f;
}

MYFLOAT GrainReverb::tick(MYFLOAT in, int, int) {
    MYFLOAT t = in * window[(int) sp];
    sp += sl;
    if (sp >= WINDOW_SIZE)
        sp -= WINDOW_SIZE;
    MYFLOAT x0 = _diff1[0].process(_delay[0].read() + t);
    MYFLOAT x1 = _diff1[1].process(_delay[1].read() + t);
    MYFLOAT x2 = _diff1[2].process(_delay[2].read() - t);
    MYFLOAT x3 = _diff1[3].process(_delay[3].read() - t);

    t = x0 - x1;
    x0 += x1;
    x1 = t;
    t = x2 - x3;
    x2 += x3;
    x3 = t;

    t = x0 - x2;
    x0 += x2;
    x2 = t;
    t = x1 - x3;
    x1 += x3;
    x3 = t;

    _filtstate[0] = (x0 * (1 - _c) + _c * _filtstate[0]);
    _filtstate[1] = (x1 * (1 - _c) + _c * _filtstate[1]);
    _filtstate[2] = (x2 * (1 - _c) + _c * _filtstate[2]);
    _filtstate[3] = (x3 * (1 - _c) + _c * _filtstate[3]);

    _delay[0].write(_feedback[0] * _filtstate[0]);
    _delay[1].write(_feedback[1] * _filtstate[1]);
    _delay[2].write(_feedback[2] * _filtstate[2]);
    _delay[3].write(_feedback[3] * _filtstate[3]);

    return (in * (1.f - mix) + mix * _gainfact * (x1 + x2)) * gain;
}


void grain_t::init(std::shared_ptr<std::vector<short>> &filepointer, MYFLOAT gain, int32_t writeoffset,
                   double readoffset, MYFLOAT pitch,
                   int32_t readdirection, int grainsize,
                   bool env_only, MYFLOAT *table, uint32_t frq, uint32_t lobits, uint32_t mask,
                   MYFLOAT pfrac) {
    _gain = gain;
    _envonly = env_only;
    _filepointer = filepointer;
    _readoffset = readoffset;
    _writeoffset = writeoffset;
    if (readdirection != 1)
        _readoffset += _grainsize * pitch;
    _interpol = pitch != 1.;
    _pitch = pitch * readdirection;
    _grainsize = grainsize;
    _count = 0;
    _phs = 0;
    _env = table;
    _frq = frq;
    _lobits = lobits;
    _mask = mask;
    _pfrac = pfrac;

    for (auto it = track->fx_queue_grain[0].begin(); it  !=track->fx_queue_grain[0].end(); it++) {
        auto fx = it.operator*()->_id;
        if (fx == SPACE_GRAINFILTER) {
            grainFilter.prepare(track, chan);
            activeFX.push_back(&grainFilter);
            continue;
        } else if (fx == SPACE_GRAINPART) {
            grainBuzz.prepare(track, chan, grainsize);
            activeFX.push_back(&grainBuzz);
            continue;
        } else if (fx == SPACE_RM_GRAIN) {
            ringMod.prepare(track, chan);
            activeFX.push_back(&ringMod);
            continue;
        } else if (fx == SPACE_GRAIN_BP) {
            resonGrain.prepare(track);
            activeFX.push_back(&resonGrain);
            continue;
        }
        else if (fx == SPACE_GRAINREVERB) {
            grainReverb.prepare(track, chan, grainsize);
            activeFX.push_back(&grainReverb);
            continue;
        }
        else if (fx == SPACE_GRAINMODAL) {
            grainModal.prepare(track, chan);
            activeFX.push_back(&grainModal);
            continue;
        }
    }
}


MYFLOAT grain_t::tick() {
    if (_writeoffset-- >= 0)
        return 0.0f;
    MYFLOAT grainsmpl{};
    if (_filepointer == nullptr || _envonly) {
        grainsmpl = 0;
    } else {
        if (_readoffset >= _filepointer->size() - 1 || _readoffset < 0) {
            _readoffset -= _pitch;
            _pitch *= -1;
        }
        if (_interpol) {
            int32_t v1 = (int) _readoffset;
            int32_t v2 = v1 + 1;

            grainsmpl = (MYFLOAT) (
                    (*_filepointer)[v1] +
                    (_readoffset - v1) *
                    ((*_filepointer)[v2] - (*_filepointer)[v1])) *
                        CONVMYFLT;
        } else {
            grainsmpl = ((MYFLOAT) (*_filepointer)[(int) _readoffset] *
                         CONVMYFLT);
        }
    }
    _readoffset += _pitch;
    for (auto fx : activeFX)
        grainsmpl = fx->tick(grainsmpl, _count, _grainsize);
    if (!_envonly) {
        grainsmpl *= _env[(_phs >> _lobits)];
    } else {
        int32_t lookup = _phs >> _lobits;
        MYFLOAT v = _env[lookup++];
        v += (_env[lookup] - v) * (MYFLOAT) ((int32_t) (_phs & _mask)) * _pfrac;
        grainsmpl = v;
    }
    _phs = (_phs + _frq) & OSCBNK_PHSMSK_64;
    _count++;
    return grainsmpl * _gain;
}

