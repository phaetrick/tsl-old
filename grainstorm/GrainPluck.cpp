//
// PLUCK - see GrainPluck.h.
//

#include <cmath>
#include <algorithm>
#include "GrainPluck.h"
#include "track.h"
#include "grainstorm.h"
#include "random.h"
#include "lfo.h"

GrainPluck::GrainPluck(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_GRAINPLUCK, GRAINEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_GRAINPLUCK];
    _fmin = &_STATE->params[t->index][GRAINPLKMIN];
    _fmax = &_STATE->params[t->index][GRAINPLKMAX];
    _decay = &_STATE->params[t->index][GRAINPLKDECAY];
    _damp = &_STATE->params[t->index][GRAINPLKDAMP];
    _mix = &_STATE->params[t->index][GRAINPLKMIX];
    _gain = &_STATE->params[t->index][GRAINPLKGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _lfo_tune = &t->lfo[GRAINPLKMIN];

    // longest line = the lowest tuning; power of two so the read/write wrap is
    // a mask. Allocated here, off the audio thread.
    int32_t need = (int32_t) ((MYFLOAT) _STATE->sr / MIN_FREQ) + 4;
    int32_t sz = 2;
    while (sz < need) sz <<= 1;
    _buf.assign(sz, 0.);
    _mask = sz - 1;
    _delay = (MYFLOAT) _STATE->sr / 220.;
}

void GrainPluck::compute(MYFLOAT *in, int32_t size) {
    auto _appState = this->_appState;
    const MYFLOAT gain = dbToLinear60(*_gain);
    const MYFLOAT mixTarget = (*_bypass || destroyRequested) ? 0. : (MYFLOAT) *_mix;
    const MYFLOAT sr = (MYFLOAT) _STATE->sr;

    // One tuning per grain. The params are dB-log Hz, so centre and spread are
    // computed in that domain and the window is a fixed number of semitones
    // wherever the LFO puts it. An LFO here moves the centre and keeps the
    // spread: a stepped LFO plays a line, the spread keeps each note a small
    // cloud around it rather than a single string.
    const MYFLOAT dbmin = _fmin->load(), dbmax = _fmax->load();
    MYFLOAT centre = (dbmin + dbmax) * .5;
    const MYFLOAT spread = std::fabs(dbmax - dbmin) * .5;
    const LFO *lfoT = _lfo_tune->load();
    if (lfoT && lfoT->power()) {
        const MYFLOAT a = lfoT->min(GRAINPLKMIN), b = lfoT->max(GRAINPLKMIN);
        centre = a + lfoT->buf[grainLfoIndex()] * (b - a);
    }
    const MYFLOAT freq = Effect::limit(
            (MYFLOAT) LOG2NORMALF(tsl::random::randomfloat(centre - spread, centre + spread)),
            MIN_FREQ, MAX_FREQ);
    // the loop filter contributes about half a sample of delay
    const MYFLOAT target = Effect::limit(sr / freq - .5, 2., (MYFLOAT) (_mask - 2));
    const MYFLOAT dInc = size > 0 ? (target - _delay) / (MYFLOAT) size : 0.;

    // DECAY is a T60 in seconds; per-loop gain follows from the loop length, so
    // the decay time means the same thing at every tuning
    const MYFLOAT t60 = .05 * std::pow(200., Effect::limit((MYFLOAT) *_decay, 0., 1.));
    // DAMP 0 = no loss filter at all, 1 = only the fundamental survives
    const MYFLOAT damp = Effect::limit((MYFLOAT) *_damp, 0., 1.);
    const MYFLOAT lpC = 1. - .97 * damp;

    // g^(t60*sr/delay) = 10^-3. Computed once per grain from the mean of the
    // glide rather than per sample - a pow() in the inner loop would cost more
    // than the whole rest of the effect.
    const MYFLOAT gLoop = std::pow(10., -3. * ((_delay + target) * .5) / (t60 * sr));

    const MYFLOAT *window = _DATA->hanningwin;
    const MYFLOAT sl = 1. / (MYFLOAT) size;
    MYFLOAT sp = 0.;

    for (int32_t i = 0; i < size; i++) {
        const MYFLOAT exc = in[i] * window[PHS2INT(sp)];
        sp += sl;

        // fractional read, linear interpolation
        const MYFLOAT rp = (MYFLOAT) _wpos - _delay;
        const int32_t r0 = ((int32_t) std::floor(rp)) & _mask;
        const int32_t r1 = (r0 + 1) & _mask;
        const MYFLOAT frac = rp - std::floor(rp);
        const MYFLOAT s = _buf[r0] + frac * (_buf[r1] - _buf[r0]);

        _lp += lpC * (s - _lp);

        MYFLOAT fb = _lp * gLoop;
        if (std::isnan(fb)) fb = 0.;
        _buf[_wpos] = exc + fb;
        _wpos = (_wpos + 1) & _mask;
        _delay += dInc;

        in[i] = in[i] * (1. - smmix()) + s * smmix() * smgain();
        smmixgain(mixTarget, gain);
    }

    _delay = target;
    UDD(_lp);
}
