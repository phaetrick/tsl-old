//
// Created by pr on 14.08.20.
//

#include "chorus.h"
#include "track.h"
#include "app.h"
#include "grainstorm.h"

Chorus::Chorus(TRACK *t) : Effect(t, SPACE_CHORUS, STEREOEFFECT) {
    _bypass = &t->bypass[SPACE_CHORUS];
    _gain = &_STATE->params[t->index][CHORUSGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _mix = &_STATE->params[t->index][CHORUSMIX];
    fol = &t->_STATE->followerMap[t->index].at(CHORUSMIX);
    depth = &_STATE->params[t->index][CHORUSINT];
    depth_prev = *depth;
    freq = &_STATE->params[t->index][CHORUS2RATE];
    freq_prev = *freq;
    ntaps = &_STATE->params[t->index][CHORUS2TAPS];
    ntaps_prev = *ntaps;
    _spread = &_STATE->params[t->index][CHORUSPHASE];
    spread_prev = *_spread;
    width = &_STATE->params[t->index][CHORUS2WIDTH];
    width_prev = *width;
    _delay = &_STATE->params[t->index][CHORUSDELAY];
    _delay_prev = *_delay;
    _mod = &_STATE->params[t->index][CHORUSMOD];
    _mod_prev = *_mod;
    _delayL.init(_STATE->sr + 10, _STATE->sr * _delay_prev * 0.001);
    _delayR.init(_STATE->sr + 10, _STATE->sr * _delay_prev * 0.001);
    chorus.initstereo(_STATE->sr);
    chorus.set_type((int) _mod_prev);
    chorus.set_speed_Hz(LOG2NORMALF(freq_prev));
    chorus.set_depth_ms(10 * depth_prev);
    chorus.set_nr(ntaps_prev * 2);
    chorus.set_width(width_prev);
    chorus.set_phase_diff(spread_prev);
    chorus.updatestereo();
}

void Chorus::check() {
    if (*width != width_prev) {
        width_prev = *width;
        chorus.set_width(width_prev);
    }

    if (*depth != depth_prev || *freq != freq_prev || *ntaps != ntaps_prev ||
                              *_spread != spread_prev || *_mod != _mod_prev) {
        _fadeinc = - _fadeconst;
    }
    MYFLOAT delay = *_delay;
    if(delay != _delay_prev) {
        _delay_prev = delay;
        _delayL.setDelayMS(_delay_prev, _STATE->sr);
        _delayR.setDelayMS(_delay_prev, _STATE->sr);
    }
}

void Chorus::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    MYFLOAT mix, gain = dbToLinear60(*_gain);
    const bool bypass = (_bypass->load() || destroyRequested);
    if (bypass) {
        mix = 0.f;

    } else {
        mix = _mix->load();
    }


    check();


    const bool env_on = fol->prepare();
    MYFLOAT *envbuf[2];
    if (env_on) {
        auto src = fol->source.load();
        envbuf[0] = src == _track->index ? inl : _DATA->tracks[src]->envf_buffer[0];
        envbuf[1] = src == _track->index ? inr : _DATA->tracks[src]->envf_buffer[1];
    }

    for (int32_t i = 0; i < s; i++) {

        MYFLOAT mixl = (env_on && !bypass) ? fol->detectL(envbuf[0][i]) : _smooth1;
        MYFLOAT mixr = (env_on && !bypass) ? fol->detectR(envbuf[1][i]) : _smooth1;
        MYFLOAT mixsrcl = 1. - mixl;
        MYFLOAT mixsrcr = 1. - mixr;
        if (_fade < 0) {
            _fadeinc = _fadeconst;
            _fade = 0;
            ntaps_prev = *ntaps;
            freq_prev = *freq;
            depth_prev = *depth;
            spread_prev = *_spread;
            _mod_prev = *_mod;
            chorus.set_phase_diff(spread_prev);
            chorus.set_depth_ms(depth_prev * 10);
            chorus.set_speed_Hz(LOG2NORMALF(freq_prev));
            chorus.set_nr(ntaps_prev * 2);
            chorus.set_width(width_prev);
            chorus.set_type((int) _mod_prev);
            chorus.updatestereo();
        } else if (_fade > 1) {
            _fadeinc = 0;
            _fade = 1;
        } else {
            _fade += _fadeinc;
        }
        MYFLOAT l = 0, r = 0;
        chorus.tickstereo(inl[i], inr[i], &l, &r);
        outl[i] = mixl * _fade * _smooth2 * _delayL.tick(l) + mixsrcl * inl[i] ;
        outr[i] = mixr * _fade * _smooth2 * _delayR.tick(r) + mixsrcr * inr[i];
        smmixgain(mix, gain);
    }
}