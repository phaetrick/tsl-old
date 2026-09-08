//
// DISPERSE - see GrainDisperse.h.
//

#include <cmath>
#include <algorithm>
#include "GrainDisperse.h"
#include "track.h"
#include "grainstorm.h"
#include "lfo.h"

GrainDisperse::GrainDisperse(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_GRAINDISPERSE, GRAINEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_GRAINDISPERSE];
    _nstages = &_STATE->params[t->index][GRAINDISPSTAGES];
    _fmin = &_STATE->params[t->index][GRAINDISPFMIN];
    _fmax = &_STATE->params[t->index][GRAINDISPFMAX];
    _depth = &_STATE->params[t->index][GRAINDISPDEPTH];
    _mix = &_STATE->params[t->index][GRAINDISPMIX];
    _gain = &_STATE->params[t->index][GRAINDISPGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _lfo_freq = &t->lfo[GRAINDISPFMIN];
    _lfo_depth = &t->lfo[GRAINDISPDEPTH];
    _sr = (MYFLOAT) _STATE->sr;
    _tpidsr = TWOPI_P / _sr;
}

void GrainDisperse::updateStages(int stages, MYFLOAT fmin, MYFLOAT fmax, MYFLOAT depth) {
    _stages = stages;
    _lastFmin = fmin;
    _lastFmax = fmax;
    _lastDepth = depth;

    const MYFLOAT nyq = _sr * .49;
    const MYFLOAT lo = Effect::limit(fmin, 10., nyq);
    const MYFLOAT hi = Effect::limit(fmax, lo, nyq);
    // pole radius: how long each stage holds on to its own band
    const MYFLOAT r = Effect::limit(.5 + depth * .49, .5, .995);

    for (int k = 0; k < stages; k++) {
        const MYFLOAT frac = stages > 1 ? (MYFLOAT) k / (MYFLOAT) (stages - 1) : 0.;
        const MYFLOAT f = lo * std::pow(hi / lo, frac);
        const MYFLOAT w = f * _tpidsr;
        // H(z) = (a2 + a1 z^-1 + z^-2) / (1 + a1 z^-1 + a2 z^-2): poles at r e^(+-jw)
        _a1[k] = -2. * r * std::cos(w);
        _a2[k] = r * r;
    }
    for (int k = stages; k < MAXSTAGES; k++)
        _x1[k] = _x2[k] = _y1[k] = _y2[k] = 0.;
}

void GrainDisperse::compute(MYFLOAT *in, int32_t size) {
    auto _appState = this->_appState;
    const MYFLOAT gain = dbToLinear60(*_gain);
    const MYFLOAT mixTarget = (*_bypass || destroyRequested) ? 0. : (MYFLOAT) *_mix;

    const int stages = Effect::limit((int) (_nstages->load() + .5), 1, MAXSTAGES);

    // FREQ MIN/MAX is a span across the stages here, not a random range, so an
    // LFO on it slides the whole band and keeps the span: the stages stay the
    // same distance apart in pitch and the smear sweeps, phaser-fashion. Done
    // in the stored dB-log domain so the span is a fixed number of octaves.
    const MYFLOAT dbmin = _fmin->load(), dbmax = _fmax->load();
    MYFLOAT dbcentre = (dbmin + dbmax) * .5;
    const MYFLOAT dbhalf = std::fabs(dbmax - dbmin) * .5;
    const LFO *lfoF = _lfo_freq->load();
    if (lfoF && lfoF->power()) {
        const MYFLOAT a = lfoF->min(GRAINDISPFMIN), b = lfoF->max(GRAINDISPFMIN);
        dbcentre = a + lfoF->buf[grainLfoIndex()] * (b - a);
    }
    const MYFLOAT fmin = LOG2NORMALF(dbcentre - dbhalf);
    const MYFLOAT fmax = LOG2NORMALF(dbcentre + dbhalf);

    MYFLOAT depth = (MYFLOAT) *_depth;
    const LFO *lfoD = _lfo_depth->load();
    if (lfoD && lfoD->power())
        depth = lfoD->min(GRAINDISPDEPTH) +
                lfoD->buf[grainLfoIndex()] * (lfoD->max(GRAINDISPDEPTH) - lfoD->min(GRAINDISPDEPTH));
    depth = Effect::limit(depth, 0., 1.);

    if (stages != _stages || std::fabs(fmin - _lastFmin) > .5
        || std::fabs(fmax - _lastFmax) > .5 || std::fabs(depth - _lastDepth) > .002)
        updateStages(stages, fmin, fmax, depth);

    const MYFLOAT *window = _DATA->hanningwin;
    const MYFLOAT sl = 1. / (MYFLOAT) size;
    MYFLOAT sp = 0.;

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT v = in[i] * window[PHS2INT(sp)];
        sp += sl;

        for (int k = 0; k < stages; k++) {
            const MYFLOAT a1 = _a1[k], a2 = _a2[k];
            const MYFLOAT y = a2 * v + a1 * _x1[k] + _x2[k] - a1 * _y1[k] - a2 * _y2[k];
            _x2[k] = _x1[k];
            _x1[k] = v;
            _y2[k] = _y1[k];
            _y1[k] = y;
            v = y;
        }

        in[i] = in[i] * (1. - smmix()) + v * smmix() * smgain();
        smmixgain(mixTarget, gain);
    }

    for (int k = 0; k < stages; k++) {
        UDD(_x1[k]);
        UDD(_x2[k]);
        UDD(_y1[k]);
        UDD(_y2[k]);
    }
}
