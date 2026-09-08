//
// Created by pr on 20.12.17.
//

#include "logger.h"


#include "lfo.h"
#include "delay.h"
#include "track.h"
#include "filter.h"
#include "grainstorm.h"
#include "envelope.h"
#include "tools.h"
#include "app.h"
#include "button.h"
#include "infopanel.h"
#include "view.h"
#include "ControlItem.h"


#define MAXDELAY 1

// The FB knobs run to +-1, where the loop is only marginally stable: the
// Butterworth passband gain sits a hair under 1, so a sustained input keeps
// accumulating instead of settling, and the pitch shifter in the loop can push
// the round trip over unity outright. HOLD is the control for an endless loop,
// so feedback only ever has to stay strictly contracting. .99 is the figure
// phaser.cpp already uses. Clamped rather than scaled (the phaser/flanger house
// style) so that every setting below the top of the knob - and so every saved
// preset - keeps the tail length it was dialled in with.
#define MAXFEEDBACK .99

void MDELAY::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(_gain->load());
    MYFLOAT mix;
    const bool bypass = (*_bypass || destroyRequested);
    if (bypass) {
        mix = 0;
    } else {
        mix = *_mix;
    }
    for (int32_t i = 0; i < size; i++) {
        _buffer[i] = _buffer2[i] = in[i];
    }
    //memcpy(_buffer, in, sizeof * size);
    //memcpy(_buffer2, in, sizeof * size));
    memset(in, 0, sizeof(MYFLOAT) * size);

    if ((int) _mode->load() == DELAY_PARALLEL) {
        for (int32_t i = 0; i < MAX_DELAYS; i++) {
            if (_STATE->params[_track->index][MDELAY1_POW + i].load()) {
                if (!_active_prev[i]) {
                    _delays[i]->clear();
                    //std::fill(_delays[i]->_delayline.begin(), _delays[i]->_delayline.end(), 0);
                    _active_prev[i] = true;
                }
                _delays[i]->computeMdelay(_buffer.data(), in, nullptr, size);
            } else
                _active_prev[i] = false;
        }
    } else {
        for (int32_t i = 0; i < MAX_DELAYS; i++) {
            if (_STATE->params[_track->index][MDELAY1_POW + i].load()) {
                if (!_active_prev[i]) {
                    _delays[i]->clear();
                    _active_prev[i] = true;
                }

                _delays[i]->computeMdelay(_buffer.data(), _buffer.data(), in, size);
            } else
                _active_prev[i] = false;
        }
    }

    for (int32_t i = 0; i < size; i++) {
        in[i] = in[i] * _smooth1 * _smooth2 + _buffer2[i] * (1. - _smooth1);
        smmixgain(mix, gain);
    }

}

MDELAY::MDELAY(TRACK *t, int32_t channel) : Effect(t, channel, SPACE_MDELAY, MONOEFFECT) {
    _bypass = &t->bypass[SPACE_MDELAY];
    _mode = &_STATE->params[t->index][MDELAY_MODE];
    _mix = &_STATE->params[t->index][MDELAYMIX];
    _gain = &_STATE->params[t->index][MDELAYGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _buffer.resize(_STATE->maxBufSize);
    _buffer2.resize(_STATE->maxBufSize);
    for (int32_t i = 0; i < MAX_DELAYS; i++) {
        // auto offset = (uint32_t) (1 + i * NUM_CONTROLS_MDELAY);
        _delays[i] = std::make_unique<TapDelay>(t, channel, i);
    }
};

TapDelay::TapDelay(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_DELAY, MONOEFFECT),
                                             inc(20. * track->_STATE->onedsr), nextBut(track->_STATE), prevBut(track->_STATE) {
    _delayLineRev.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY * 2)), 0);
    _delayLine.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY)), 0);
    mask = _delayLine.size() - 1;
    mask2 = _delayLineRev.size() - 1;
    _bypass = &track->bypass[SPACE_DELAY];
    _feedback = &track->_STATE->params[track->index][DELAYFB];
    _dry = &track->_STATE->params[track->index][DELAYDRY];
    _wet = &track->_STATE->params[track->index][DELAYWET];
    _lp_cut = &track->_STATE->params[track->index][DELAYLP];
    // Seeded in Hz, not in the param's own 20*log10 domain: compute() compares
    // these against LOG2NORMAL(*_lp_cut), so a raw seed never matches and makes
    // the first block re-set a filter that is already set.
    _prev_lp_cut = LOG2NORMAL(*_lp_cut);
    _hp_cut = &track->_STATE->params[track->index][DELAYHP];
    _prev_hp_cut = LOG2NORMAL(*_hp_cut);
    _delay = &track->_STATE->params[track->index][DELAYDEL];
    _lfo = &track->lfo[DELAYDEL];
     fol = &track->_STATE->followerMap[track->index].at(DELAYDEL);
    _hold = &track->_STATE->params[track->index][DELAYHOLD];
    _backw = &track->_STATE->params[track->index][DELAYBACKW];
    _shift = &track->_STATE->params[track->index][DELAYSHIFT2];
    _shiftmix = &track->_STATE->params[track->index][DELAYSHIFTMIX];
    nextMode = track->_STATE->params[_track->index][DELAYBACKW].load() == 1.0;
    prevDelay = nextDelay = static_cast<int>(pow(10, *_delay * .05) * track->_STATE->sr * .001);
    minDelay = (int) (5. * track->_STATE->sr * .001);

    nextBut.setHP(pow(10, *_hp_cut * .05) * track->_STATE->onedsr);
    nextBut.setLP(pow(10, *_lp_cut * .05) * track->_STATE->onedsr);
    _shiftold = *_shift;
    _shiftmixold = *_shiftmix;
    pitchShiftNext.setShift(_shiftold);
}

TapDelay::TapDelay(TRACK *track, int32_t chan, int offset) : Effect(track, chan, SPACE_DELAY,
                                                                MONOEFFECT),
                                                         inc(20. * track->_STATE->onedsr), nextBut(track->_STATE), prevBut(track->_STATE) {
    _delayLineRev.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY * 2)), 0);
    _delayLine.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY)), 0);
    _lfo = &track->lfo[MDELAY1DEL + offset * NUM_CONTROLS_MDELAY];
    fol = &track->_STATE->followerMap[track->index].at(MDELAY1DEL + offset * NUM_CONTROLS_MDELAY);
    lfoDataOffset = offset;
/*
    _delays[i]->lfoDataIndex = offset;
    _delays[i]->_feedback = &_STATE->params[t->index][MDELAY1FB + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_delay = &_STATE->params[t->index][MDELAY1DEL + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_lfo = &t->lfo[MDELAY1DEL + i * NUM_CONTROLS_MDELAY];
    // _delays[i]->_smooth = &_STATE->params[t->index][MDELAY1SM + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_hold = &_STATE->params[t->index][MDELAY1HOLD + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_hp_cut = &_STATE->params[t->index][MDELAY1HP + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_lp_cut = &_STATE->params[t->index][MDELAY1LP + i * NUM_CONTROLS_MDELAY];
    _delays[i]->_gain = &_STATE->params[t->index][MDELAY1GAIN + i * NUM_CONTROLS_MDELAY];
    //_delays[i]->_flush = &t->mdelay_flush[i][channel];
*/
    mask = _delayLine.size() - 1;
    mask2 = _delayLineRev.size() - 1;
    _feedback = &track->_STATE->params[track->index][MDELAY1FB + offset * NUM_CONTROLS_MDELAY];
    _gain = &track->_STATE->params[track->index][MDELAY1GAIN + offset * NUM_CONTROLS_MDELAY];
    _lp_cut = &track->_STATE->params[track->index][MDELAY1LP + offset * NUM_CONTROLS_MDELAY];
    _prev_lp_cut = LOG2NORMAL(*_lp_cut);
    _hp_cut = &track->_STATE->params[track->index][MDELAY1HP + offset * NUM_CONTROLS_MDELAY];
    _prev_hp_cut = LOG2NORMAL(*_hp_cut);
    _delay = &track->_STATE->params[track->index][MDELAY1DEL + offset * NUM_CONTROLS_MDELAY];
    _hold = &track->_STATE->params[track->index][MDELAY1HOLD + offset * NUM_CONTROLS_MDELAY];
    _backw = &track->_STATE->params[track->index][MDELAY1BACKW + offset];
    _shift = &track->_STATE->params[track->index][MDELAY1SHIFT + offset];
    _shiftmix = &_STATE->params[track->index][MDELAY1SHIFTMIX + offset];
    nextMode = _backw->load() == 1.0;
    prevDelay = nextDelay = static_cast<int>(pow(10, *_delay * .05) * track->_STATE->sr * .001);
    minDelay = (int) (5. * track->_STATE->sr * .001);

    nextBut.setHP(pow(10, *_hp_cut * .05) * track->_STATE->onedsr);
    nextBut.setLP(pow(10, *_lp_cut * .05) * track->_STATE->onedsr);
    _shiftold = *_shift;
    _shiftmixold = *_shiftmix;
    pitchShiftNext.setShift(_shiftold);
}

void TapDelay::clear() {
    std::fill(_delayLineRev.begin(), _delayLineRev.end(), 0);
    std::fill(_delayLine.begin(), _delayLine.end(), 0);
    crossFade = 0.;
}

void TapDelay::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT dry, wet;

    if (_bypass->load() || destroyRequested) {
        dry = 1.;
        wet = 0.;
    } else {
        dry = LOG2NORMALF(*_dry);
        wet = LOG2NORMALF(*_wet);
    };

    int32_t channel = _chan;

    auto hpcut = LOG2NORMAL(*_hp_cut), lpcut = LOG2NORMAL(*_lp_cut);
    if (hpcut != _prev_hp_cut) {
        _prev_hp_cut = hpcut;
        nextBut.setHP(hpcut * _STATE->onedsr);
    }
    if (lpcut != _prev_lp_cut) {
        _prev_lp_cut = lpcut;
        nextBut.setLP(lpcut * _STATE->onedsr);
    }

    MYFLOAT shiftmix = *_shiftmix;
    if (_shiftmixold != shiftmix) {
        _shiftmixold = shiftmix;
        if (_shiftmixold == 0) {
            pitchShiftNext.clear();
        }
    }
    if (*_shift != _shiftold) {
        pitchShiftNext.setShift(*_shift);
        _shiftold = *_shift;
    }

    const MYFLOAT shiftmixsrc = 1.0 - shiftmix;
    MYFLOAT feedback = *_feedback;
    CLAMP(feedback, -MAXFEEDBACK, MAXFEEDBACK);
    LFO *lfo = _lfo->load();
    const bool lfo_on = lfo && lfo->power();
    MYFLOAT lfostart{};
    MYFLOAT lforange{};
    if (lfo_on) {
        auto a = LOG2NORMAL(
                _STATE->controls[_track->index][DELAYDEL].lfo_min.load()) * _STATE->sr *
                 .001, b = LOG2NORMAL(
                _STATE->controls[_track->index][DELAYDEL].lfo_max.load()) * _STATE->sr * .001;
        lfostart = a;
        lforange = b - a;
    }
    const bool env_on = fol->prepare(_chan);
    MYFLOAT *envbuf = nullptr;
    if (env_on) {
        auto src = fol->source.load();
        envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    }

    // HOLD and BACKW have to come straight off the params for the modulated
    // path: next*/prev* below are only ever updated by the static branch, so
    // they go stale the moment an LFO or a follower takes over the delay time.
    const bool modulated = lfo_on || env_on;
    const bool modHold = modulated && *_hold == 1.0;
    const bool modReverse = modulated && *_backw == 1.0;

    if (crossFade == 1. && !lfo_on && !env_on) {
        bool hold = *_hold == 1.0;
        auto next = static_cast<int>(pow(10, *_delay * .05) * _STATE->sr * .001);
        int mode = *_backw == 1.0;
        if (nextDelay != next || nextMode != mode || nextHold != hold) {
            crossFade = 0;
            if (nextDelay != next) {
                //prevHold = nextHold;
                //nextHold = false;
            }
            prevDelay = nextDelay;
            nextDelay = next;
            prevMode = nextMode;
            nextMode = mode;
            prevHold = nextHold;
            nextHold = hold;
            if (shiftmix != 0) {
                pitchShiftPrev = pitchShiftNext;
                pitchShiftNext.clear();
            }
            prevBut = nextBut;
            nextBut.reset();
            if (prevMode == DELAYMODEREVERSE) {
                prevRevPos = nextRevPos;
            }
            if (nextMode == DELAYMODEREVERSE) {
                nextRevPos = 0;
            }
        }
    }


    for (int32_t i = 0; i < size; i++) {
        if (modulated) {
            MYFLOAT dellen = lfo_on ? lfostart + lfo->buf[i] * lforange : 0;
            if(env_on){
                dellen = (_chan == 0 ? fol->detectL(envbuf[i]) : fol->detectR(envbuf[i])) * _STATE->sr * 0.001;
            }
            if(dellen < minDelay)dellen = minDelay;
            else if(dellen>_STATE->sr)dellen = _STATE->sr;
            // The reverse grain is as long as the delay time currently is, so it
            // tracks the modulation. The minDelay clamp above keeps it off zero,
            // which is what stops the window below dividing by it.
            const int32_t grain = static_cast<int32_t>(dellen);
            MYFLOAT del = _writepos - dellen;
            auto floordel = static_cast<int>(floor(del));
            MYFLOAT frac = del - floordel;
            MYFLOAT tmp[16];
            for (MYFLOAT &t : tmp)
                t = _delayLine[(floordel--) & mask];
            auto res = _STATE->windowedSinc.tick(tmp, frac);
            if (modReverse) {
                _delayLineRev[writePosRev] = modHold ? 0 : in[i];
                if (nextRevPos >= grain) nextRevPos = 0;
                const MYFLOAT win = 4. * nextRevPos / (MYFLOAT) grain *
                                    (1. - nextRevPos / (MYFLOAT) grain);
                res += _delayLineRev[(writePosRev - 2 * nextRevPos) & mask2] * win;
                ++nextRevPos;
            }
            res = nextBut.tickLpHp6(res);
            if (shiftmix > 0.0)
                res = shiftmix * pitchShiftNext.tick(res) + shiftmixsrc * res;
            // Reverse takes its input from _delayLineRev only, and HOLD
            // recirculates at unity instead of taking new input - both exactly
            // as the static branch below does it.
            if (modReverse)
                _delayLine[_writepos] = modHold ? res : res * feedback;
            else
                _delayLine[_writepos] = modHold ? res : in[i] + res * feedback;
            (++writePosRev) &= mask2;

            in[i] = in[i] * _smooth2 + res * _smooth1;
            // Run any fade that was in flight when the modulation came on out to
            // completion, so turning it off again does not resume a stale
            // crossfade against line content this branch has overwritten.
            crossFade += inc;
            if (crossFade >= 1.0) {
                crossFade = 1.0;
            }
            smwetdry(wet, dry);
            (++_writepos) &= mask;
            continue;
        }
        MYFLOAT tapPrev{}, tapNext{};
        if (crossFade < 1) {
            if (prevMode == DELAYMODEREVERSE) {
                auto ret = _delayLine[static_cast<int>(_writepos - prevDelay) &
                                      mask];
                if (prevRevPos >= prevDelay) prevRevPos = 0;
                auto win = 4. * prevRevPos / (MYFLOAT) prevDelay *
                           (1. - prevRevPos / (MYFLOAT) prevDelay);
                auto ret2 = _delayLineRev[(writePosRev - 2 * prevRevPos) & mask2] * win;
                tapPrev = prevBut.tickLpHp6(ret + ret2) *
                          (1. - crossFade);
                if (shiftmix > 0.0)
                    tapPrev = shiftmix * pitchShiftPrev.tick(tapPrev) + shiftmixsrc * tapPrev;
                _delayLine[_writepos] = prevHold ? tapPrev : tapPrev * feedback;
                ++prevRevPos;
            } else {
                tapPrev = prevBut.tickLpHp6(
                        _delayLine[static_cast<int>(_writepos - prevDelay) & mask]) *
                          (1 - crossFade);
                if (shiftmix > 0.0)
                    tapPrev = shiftmix * pitchShiftPrev.tick(tapPrev) + shiftmixsrc * tapPrev;
                _delayLine[_writepos] = prevHold ? tapPrev : in[i] * (1. - crossFade) +
                                                             tapPrev * feedback;
            }
        }
        if (nextMode == DELAYMODEREVERSE) {
            _delayLineRev[writePosRev] = nextHold ? (prevHold ? 0 : in[i] * (1 - crossFade))
                                                  : in[i];
            auto ret = _delayLine[static_cast<int>(_writepos - nextDelay) &
                                  mask];
            if (nextRevPos >= nextDelay) nextRevPos = 0;
            auto win = 4. * nextRevPos / (MYFLOAT) nextDelay *
                       (1. - nextRevPos / (MYFLOAT) nextDelay);
            auto ret2 = _delayLineRev[(writePosRev - 2 * nextRevPos) & mask2] * win;

            tapNext = nextBut.tickLpHp6(ret + ret2) * crossFade;
            if (shiftmix > 0.0)
                tapNext = shiftmix * pitchShiftNext.tick(tapNext) + shiftmixsrc * tapNext;
            if (crossFade == 1.0)
                _delayLine[_writepos] = nextHold ? tapNext : tapNext * feedback;
            else
                _delayLine[_writepos] += nextHold ? tapNext : tapNext * feedback;
            ++nextRevPos;
        } else {
            tapNext =
                    nextBut.tickLpHp6(_delayLine[static_cast<int>(_writepos - nextDelay) & mask]) *
                    crossFade;
            if (shiftmix > 0.0)
                tapNext = shiftmix * pitchShiftNext.tick(tapNext) + shiftmixsrc * tapNext;
            if (crossFade == 1.0)
                _delayLine[_writepos] = nextHold ? tapNext : in[i] + tapNext * feedback;
            else
                _delayLine[_writepos] += nextHold ? tapNext : in[i] * crossFade +
                                                              tapNext * feedback;
        }
        (++writePosRev) &= mask2;

        in[i] = in[i] * _smooth2 + (tapPrev + tapNext) * _smooth1;
        crossFade += inc;
        if (crossFade >= 1.0) {
            crossFade = 1.0;
        }
        smwetdry(wet, dry);
        (++_writepos) &= mask;
    }
}

void TapDelay::computeMdelay(MYFLOAT *in, MYFLOAT *out, MYFLOAT *out2, int32_t size) {
    auto gain = dbToLinear60(*_gain);

    int32_t channel = _chan;

    auto hpcut = LOG2NORMAL(*_hp_cut), lpcut = LOG2NORMAL(*_lp_cut);
    if (hpcut != _prev_hp_cut) {
        _prev_hp_cut = hpcut;
        nextBut.setHP(hpcut * _STATE->onedsr);
    }
    if (lpcut != _prev_lp_cut) {
        _prev_lp_cut = lpcut;
        nextBut.setLP(lpcut * _STATE->onedsr);
    }

    MYFLOAT shiftmix = *_shiftmix;
    if (_shiftmixold != shiftmix) {
        _shiftmixold = shiftmix;
        if (_shiftmixold == 0) {
            pitchShiftNext.clear();
        }
    }
    if (*_shift != _shiftold) {
        pitchShiftNext.setShift(*_shift);
        _shiftold = *_shift;
    }

    const MYFLOAT shiftmixsrc = 1.0 - shiftmix;
    MYFLOAT feedback = *_feedback;
    CLAMP(feedback, -MAXFEEDBACK, MAXFEEDBACK);

    LFO *lfo = _lfo->load();
    const bool lfo_on = lfo && lfo->power();
    MYFLOAT lfostart{};
    MYFLOAT lforange{};
    if (lfo_on) {
        auto a = LOG2NORMAL(
                         _STATE->controls[_track->index][MDELAY1DEL + lfoDataOffset * NUM_CONTROLS_MDELAY].lfo_min.load()) * _STATE->sr *
                 .001, b = LOG2NORMAL(
                                   _STATE->controls[_track->index][MDELAY1DEL + lfoDataOffset * NUM_CONTROLS_MDELAY].lfo_max.load()) * _STATE->sr * .001;
        lfostart = a;
        lforange = b - a;
    }

    const bool env_on = fol->prepare(_chan);
    MYFLOAT *envbuf = nullptr;
    if (env_on) {
        auto src = fol->source.load();
        envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    }

    // HOLD and BACKW have to come straight off the params for the modulated
    // path: next*/prev* below are only ever updated by the static branch, so
    // they go stale the moment an LFO or a follower takes over the delay time.
    const bool modulated = lfo_on || env_on;
    const bool modHold = modulated && *_hold == 1.0;
    const bool modReverse = modulated && *_backw == 1.0;

    if (crossFade == 1. && !lfo_on && !env_on) {
        bool hold = *_hold == 1.0;
        auto next = static_cast<int>(pow(10, *_delay * .05) * _STATE->sr * .001);
        int mode = *_backw == 1.0;
        if (nextDelay != next || nextMode != mode || nextHold != hold) {
            crossFade = 0;
            if (nextDelay != next) {
                //prevHold = nextHold;
                //nextHold = false;
            }
            prevDelay = nextDelay;
            nextDelay = next;
            prevMode = nextMode;
            nextMode = mode;
            prevHold = nextHold;
            nextHold = hold;
            if (shiftmix != 0) {
                pitchShiftPrev = pitchShiftNext;
                pitchShiftNext.clear();
            }
            prevBut = nextBut;
            nextBut.reset();
            if (prevMode == DELAYMODEREVERSE) {
                prevRevPos = nextRevPos;
            }
            if (nextMode == DELAYMODEREVERSE) {
                nextRevPos = 0;
            }
        }
    }

    for (int32_t i = 0; i < size; i++) {
        if (modulated) {
            MYFLOAT dellen = lfo_on ? lfostart + lfo->buf[i] * lforange : 0;
            if(env_on){
                dellen = (_chan == 0 ? fol->detectL(envbuf[i]) : fol->detectR(envbuf[i])) * _STATE->sr * 0.001;
            }
            if(dellen < minDelay)dellen = minDelay;
            else if(dellen>_STATE->sr)dellen = _STATE->sr;
            // The reverse grain is as long as the delay time currently is, so it
            // tracks the modulation. The minDelay clamp above keeps it off zero,
            // which is what stops the window below dividing by it.
            const int32_t grain = static_cast<int32_t>(dellen);
            MYFLOAT del = _writepos - dellen;
            auto floordel = static_cast<int>(floor(del));
            MYFLOAT frac = del - floordel;
            MYFLOAT tmp[16];
            for (MYFLOAT &t : tmp)
                t = _delayLine[(floordel--) & mask];
            auto res = _STATE->windowedSinc.tick(tmp, frac);
            if (modReverse) {
                _delayLineRev[writePosRev] = modHold ? 0 : in[i];
                if (nextRevPos >= grain) nextRevPos = 0;
                const MYFLOAT win = 4. * nextRevPos / (MYFLOAT) grain *
                                    (1. - nextRevPos / (MYFLOAT) grain);
                res += _delayLineRev[(writePosRev - 2 * nextRevPos) & mask2] * win;
                ++nextRevPos;
            }
            res = nextBut.tickLpHp6(res);
            if (shiftmix > 0.0)
                res = shiftmix * pitchShiftNext.tick(res) + shiftmixsrc * res;
            // Reverse takes its input from _delayLineRev only, and HOLD
            // recirculates at unity instead of taking new input - both exactly
            // as the static branch below does it.
            if (modReverse)
                _delayLine[_writepos] = modHold ? res : res * feedback;
            else
                _delayLine[_writepos] = modHold ? res : in[i] + res * feedback;
            (++writePosRev) &= mask2;
            if (out2) {
                out[i] = res;
                out2[i] +=res * gain;
            } else
                out[i] += res * gain;
            // Run any fade that was in flight when the modulation came on out to
            // completion, so turning it off again does not resume a stale
            // crossfade against line content this branch has overwritten.
            crossFade += inc;
            if (crossFade >= 1.0) {
                crossFade = 1.0;
            }
            (++_writepos) &= mask;
            continue;
        }
        MYFLOAT tapPrev{}, tapNext{};
        if (crossFade < 1) {
            if (prevMode == DELAYMODEREVERSE) {
                auto ret = _delayLine[static_cast<int>(_writepos - prevDelay) &
                                      mask];
                if (prevRevPos >= prevDelay) prevRevPos = 0;
                auto win = 4. * prevRevPos / (MYFLOAT) prevDelay *
                           (1. - prevRevPos / (MYFLOAT) prevDelay);
                auto ret2 = _delayLineRev[(writePosRev - 2 * prevRevPos) & mask2] * win;
                tapPrev = prevBut.tickLpHp6(ret + ret2) *
                          (1. - crossFade);
                if (shiftmix > 0.0)
                    tapPrev = shiftmix * pitchShiftPrev.tick(tapPrev) + shiftmixsrc * tapPrev;
                _delayLine[_writepos] = prevHold ? tapPrev : tapPrev * feedback;
                ++prevRevPos;
            } else {
                tapPrev = prevBut.tickLpHp6(
                        _delayLine[static_cast<int>(_writepos - prevDelay) & mask]) *
                          (1 - crossFade);
                if (shiftmix > 0.0)
                    tapPrev = shiftmix * pitchShiftPrev.tick(tapPrev) + shiftmixsrc * tapPrev;
                _delayLine[_writepos] = prevHold ? tapPrev : in[i] * (1. - crossFade) +
                                                             tapPrev * feedback;
            }
        }
        if (nextMode == DELAYMODEREVERSE) {
            _delayLineRev[writePosRev] = nextHold ? (prevHold ? 0 : in[i] * (1 - crossFade))
                                                  : in[i];
            auto ret = _delayLine[static_cast<int>(_writepos - nextDelay) &
                                  mask];
            if (nextRevPos >= nextDelay) nextRevPos = 0;
            auto win = 4. * nextRevPos / (MYFLOAT) nextDelay *
                       (1. - nextRevPos / (MYFLOAT) nextDelay);
            auto ret2 = _delayLineRev[(writePosRev - 2 * nextRevPos) & mask2] * win;

            tapNext = nextBut.tickLpHp6(ret + ret2) * crossFade;
            if (shiftmix > 0.0)
                tapNext = shiftmix * pitchShiftNext.tick(tapNext) + shiftmixsrc * tapNext;
            if (crossFade == 1.0)
                _delayLine[_writepos] = nextHold ? tapNext : tapNext * feedback;
            else
                _delayLine[_writepos] += nextHold ? tapNext : tapNext * feedback;
            ++nextRevPos;
        } else {
            tapNext =
                    nextBut.tickLpHp6(_delayLine[static_cast<int>(_writepos - nextDelay) & mask]) *
                    crossFade;
            if (shiftmix > 0.0)
                tapNext = shiftmix * pitchShiftNext.tick(tapNext) + shiftmixsrc * tapNext;
            if (crossFade == 1.0)
                _delayLine[_writepos] = nextHold ? tapNext : in[i] + tapNext * feedback;
            else
                _delayLine[_writepos] += nextHold ? tapNext : in[i] * crossFade +
                                                              tapNext * feedback;
        }
        (++writePosRev) &= mask2;
        const auto wetsig = tapPrev + tapNext;
        if (out2) {
            out[i] = wetsig;
            out2[i] += wetsig * gain;
        } else
            out[i] += wetsig * gain;
        crossFade += inc;
        if (crossFade >= 1.0) {
            crossFade = 1.0;
        }
        (++_writepos) &= mask;
    }
}


PingPongDelay::PingPongDelay(TRACK *track) : Effect(track, SPACE_PINGPONG, STEREOEFFECT),
                                             inc(20. * track->_STATE->onedsr), nextBut(track->_STATE), prevBut(track->_STATE),
                                             nextButR(track->_STATE), prevButR(track->_STATE) {
    _delayLineRev.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY * 2)), 0);
    _delayLineL.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY)), 0);
    _delayLineR.resize(next_pow_2(static_cast<int>(track->_STATE->sr * MAXDELAY)), 0);
    mask = _delayLineL.size() - 1;
    mask2 = _delayLineRev.size() - 1;
    _bypass = &track->bypass[SPACE_PINGPONG];
    _feedback = &track->_STATE->params[track->index][PPFB];
    _dry = &track->_STATE->params[track->index][PPDRY];
    _wet = &track->_STATE->params[track->index][PPWET];
    _lp_cut = &track->_STATE->params[track->index][PPLP];
    _prev_lp_cut = LOG2NORMAL(*_lp_cut);
    _hp_cut = &track->_STATE->params[track->index][PPHP];
    _prev_hp_cut = LOG2NORMAL(*_hp_cut);
    _delay = &track->_STATE->params[track->index][PPDELAY];
    _hold = &track->_STATE->params[track->index][PPHOLD];
    _backw = &track->_STATE->params[track->index][PPREVERSE];
    _shift = &track->_STATE->params[track->index][PPSHIFT];
    _shiftmix = &track->_STATE->params[track->index][PPSHIFTMIX];
    nextMode = track->_STATE->params[_track->index][PPREVERSE].load() == 1.0;
    prevDelay = nextDelay = static_cast<int>(pow(10, *_delay * .05) * track->_STATE->sr * .001);
    minDelay = (int) (5. * (MYFLOAT) track->_STATE->sr * .001);

    nextBut.setHP(pow(10, *_hp_cut * .05) * track->_STATE->onedsr);
    nextBut.setLP(pow(10, *_lp_cut * .05) * track->_STATE->onedsr);
    nextButR.setHP(pow(10, *_hp_cut * .05) * track->_STATE->onedsr);
    nextButR.setLP(pow(10, *_lp_cut * .05) * track->_STATE->onedsr);
    _shiftold = *_shift;
    _shiftmixold = *_shiftmix;
    pitchShiftNext.setShift(_shiftold);
    pitchShiftNextR.setShift(_shiftold);
}

void PingPongDelay::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    MYFLOAT dry, wet;

    if (_bypass->load() || destroyRequested) {
        dry = 1.;
        wet = 0.;
    } else {
        dry = LOG2NORMALF(*_dry);
        wet = LOG2NORMALF(*_wet);
    };
    auto hpcut = LOG2NORMAL(*_hp_cut), lpcut = LOG2NORMAL(*_lp_cut);
    if (hpcut != _prev_hp_cut) {
        _prev_hp_cut = hpcut;
        nextBut.setHP(hpcut * _STATE->onedsr);
        nextButR.setHP(hpcut * _STATE->onedsr);
    }
    if (lpcut != _prev_lp_cut) {
        _prev_lp_cut = lpcut;
        nextBut.setLP(lpcut * _STATE->onedsr);
        nextButR.setLP(lpcut * _STATE->onedsr);
    }

    MYFLOAT shiftmix = *_shiftmix;
    if (_shiftmixold != shiftmix) {
        _shiftmixold = shiftmix;
        if (_shiftmixold == 0) {
            pitchShiftNext.clear();
            pitchShiftNextR.clear();
        }
    }
    if (*_shift != _shiftold) {
        pitchShiftNext.setShift(*_shift);
        pitchShiftNextR.setShift(*_shift);
        _shiftold = *_shift;
    }

    const MYFLOAT shiftmixsrc = 1.0 - shiftmix;
    MYFLOAT feedback = *_feedback;
    CLAMP(feedback, -MAXFEEDBACK, MAXFEEDBACK);

    if (crossFade == 1.) {
        bool hold = *_hold == 1.0;
        auto next = static_cast<int>(pow(10, *_delay * .05) * _STATE->sr * .001);
        int mode = *_backw == 1.0;
        if (nextDelay != next || nextMode != mode || nextHold != hold) {
            crossFade = 0;
            if (nextDelay != next) {
                //prevHold = nextHold;
                //nextHold = false;
            }
            prevDelay = nextDelay;
            nextDelay = next;
            prevMode = nextMode;
            nextMode = mode;
            prevHold = nextHold;
            nextHold = hold;
            if (shiftmix != 0) {
                pitchShiftPrev = pitchShiftNext;
                pitchShiftNext.clear();
                pitchShiftPrevR = pitchShiftNextR;
                pitchShiftNextR.clear();
            }
            prevBut = nextBut;
            nextBut.reset();
            prevButR = nextButR;
            nextButR.reset();
            if (prevMode == TapDelay::DELAYMODEREVERSE) {
                prevRevPos = nextRevPos;
            }
            if (nextMode == TapDelay::DELAYMODEREVERSE) {
                nextRevPos = 0;
            }
        }
    }

    for (int32_t i = 0; i < s; i++) {
        MYFLOAT tapPrevL{}, tapPrevR{}, tapNextL{}, tapNextR{};
        const auto in = (inl[i] + inr[i]) * .5;
        if (crossFade < 1) {
            if (prevMode == TapDelay::DELAYMODEREVERSE) {
                // The tap is stored raw and scaled by (1-crossFade) once, below,
                // with the other three taps - only the write into the line takes
                // the fade gain here.
                tapPrevR = prevButR.tickLpHp6(_delayLineR[(_writepos - prevDelay) & mask]);
                if (shiftmix > 0.0)
                    tapPrevR = shiftmix * pitchShiftPrevR.tick(tapPrevR) + shiftmixsrc * tapPrevR;
                _delayLineL[_writepos] = tapPrevR * (1. - crossFade);
                auto ret = _delayLineL[(_writepos - prevDelay) & mask];
                if (prevRevPos >= prevDelay) prevRevPos = 0;
                auto win = 4. * prevRevPos / (MYFLOAT) prevDelay *
                           (1. - prevRevPos / (MYFLOAT) prevDelay);
                auto ret2 = _delayLineRev[(writePosRev - 2 * prevRevPos) & mask2] * win;
                tapPrevL = prevBut.tickLpHp6(ret + ret2);
                if (shiftmix > 0.0)
                    tapPrevL = shiftmix * pitchShiftPrev.tick(tapPrevL) + shiftmixsrc * tapPrevL;
                _delayLineR[_writepos] =
                        (prevHold ? tapPrevL : tapPrevL * feedback) * (1. - crossFade);
                ++prevRevPos;
            } else {
                tapPrevL = prevBut.tickLpHp6(
                        _delayLineL[(_writepos - prevDelay) & mask]);
                tapPrevR = prevButR.tickLpHp6(_delayLineR[(_writepos - prevDelay) & mask]);
                if (shiftmix > 0.0) {
                    tapPrevL = shiftmix * pitchShiftPrev.tick(tapPrevL) + shiftmixsrc * tapPrevL;
                    tapPrevR = shiftmix * pitchShiftPrevR.tick(tapPrevR) + shiftmixsrc * tapPrevR;
                }
                _delayLineL[_writepos] = tapPrevR * (1. - crossFade);
                _delayLineR[_writepos] =
                        (prevHold ? tapPrevL : in + tapPrevL * feedback) * (1. - crossFade);
            }
        }


        if (nextMode == TapDelay::DELAYMODEREVERSE) {
            _delayLineRev[writePosRev] = nextHold ? in * (1. - crossFade) : in;
            tapNextR = nextButR.tickLpHp6(_delayLineR[(_writepos - nextDelay) & mask]);
            if (shiftmix > 0.0)
                tapNextR = shiftmix * pitchShiftNextR.tick(tapNextR) + shiftmixsrc * tapNextR;
            if (crossFade == 1.0)
                _delayLineL[_writepos] = tapNextR;
            else
                _delayLineL[_writepos] += tapNextR * crossFade;
            auto ret = _delayLineL[(_writepos - nextDelay) & mask];
            if (nextRevPos >= nextDelay) nextRevPos = 0;
            //if (writePosRevNext >= nextDelay * 2)writePosRevNext = 0;
            //if (delay < 0)
            //  delay += 2 * nextDelay;
            auto win = 4. * nextRevPos / (MYFLOAT) nextDelay *
                       (1. - nextRevPos / (MYFLOAT) nextDelay);
            auto ret2 =
                    _delayLineRev[(writePosRev - 2 * nextRevPos) & mask2] * (nextHold ? 0 : win);

            tapNextL = nextBut.tickLpHp6(ret + ret2);
            if (shiftmix > 0.0)
                tapNextL = shiftmix * pitchShiftNext.tick(tapNextL) + shiftmixsrc * tapNextL;
            if (crossFade == 1.0)
                _delayLineR[_writepos] = nextHold ? tapNextL : tapNextL * feedback;
            else
                _delayLineR[_writepos] +=
                        (nextHold ? tapNextL : tapNextL * feedback) * crossFade;
            ++nextRevPos;
        } else {
            tapNextL = nextBut.tickLpHp6(
                    _delayLineL[(_writepos - nextDelay) & mask]);
            tapNextR = nextButR.tickLpHp6(_delayLineR[(_writepos - nextDelay) & mask]);
            if (shiftmix > 0.0) {
                tapNextL = shiftmix * pitchShiftNext.tick(tapNextL) + shiftmixsrc * tapNextL;
                tapNextR = shiftmix * pitchShiftNextR.tick(tapNextR) + shiftmixsrc * tapNextR;
            }
            if (crossFade == 1.0) {
                _delayLineL[_writepos] = tapNextR;
                _delayLineR[_writepos] = nextHold ? tapNextL : in + tapNextL * feedback;
            } else {
                _delayLineL[_writepos] += tapNextR * crossFade;
                _delayLineR[_writepos] +=
                        (nextHold ? tapNextL : in + tapNextL * feedback) * crossFade;
            }

        }
        tapPrevL *= (1 - crossFade);
        tapPrevR *= (1 - crossFade);
        tapNextL *= crossFade;
        tapNextR *= crossFade;
        (++writePosRev) &= mask2;

        inl[i] = inl[i] * _smooth2 + (tapPrevL + tapNextL) * _smooth1;
        inr[i] = inr[i] * _smooth2 + (tapPrevR + tapNextR) * _smooth1;
        crossFade += inc;
        if (crossFade >= 1.0) {
            crossFade = 1.0;
        }
        smwetdry(wet, dry);
        (++_writepos) &= mask;
    }

}

FLANGER::FLANGER(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_FLANGER, MONOEFFECT) {
    _bypass = &_track->bypass[SPACE_FLANGER];
    _delay = &t->_STATE->params[_track->index][FLANGERDELAY];
    _max_delay = (uint32_t) (t->_STATE->sr * 0.001 * 5);
    _lfo = &_track->lfo[FLANGERDELAY];
    _mix = &t->_STATE->params[_track->index][FLANGERMIX];
    _gain = &t->_STATE->params[_track->index][FLANGERGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _feedback = &t->_STATE->params[_track->index][FLANGERFB];
    WindowedSincDelay<>::init(_max_delay + 1);
}

void FLANGER::compute(MYFLOAT *in, int32_t size) {

    MYFLOAT gain = dbToLinear60(*_gain), mix;

    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;
    }

    LFO *lfo = *_lfo;

    MYFLOAT feedback = _feedback->load() * .95;

    MYFLOAT delay_smpls;
    MYFLOAT min_delay{};

    bool lfo_on = lfo && lfo->power();
    if (lfo_on) {
        MYFLOAT a = _STATE->controls[_track->index][FLANGERDELAY].lfo_min.load();
        MYFLOAT b = _STATE->controls[_track->index][FLANGERDELAY].lfo_max.load();
        delay_smpls = _STATE->sr * 0.001 * (b-a);
        min_delay = _STATE->sr * 0.001 * a;
    } else {
        delay_smpls = *_delay * _STATE->sr * 0.001;
    }
    const MYFLOAT inc = .1 / _STATE->sr;

    MYFLOAT *sine = getsinewave();

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT delay = (lfo_on ? min_delay + lfo->buf[i] * delay_smpls : delay_smpls *
                                                                          (.5 + .5 *
                                                                                sine[PHS2INT(
                                                                                        _phase)]));
        if (delay > _max_delay)
            delay = _max_delay;
        in[i] = in[i] * (1. - _smooth1) +
                WindowedSincDelay<>::tick(in[i], delay, feedback) * _smooth1 * _smooth2;
        smmixgain(mix, gain);
        _phase += inc;
        if (_phase > 1.0)
            _phase -= 1.0;
    }
}

