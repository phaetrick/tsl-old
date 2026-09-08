#pragma once
//
// Created by pr on 15.08.20.
//

#ifndef GRAINSTORM_SHIMMER_H
#define GRAINSTORM_SHIMMER_H

#include "base.h"
#include "defines.h"
#include "track.h"
#include "Biquad.h"
#include "Allpass.h"
#include "Reverb.h"
#include "app.h"
#include "resample.h"

#define NUMAPCHANNEL 8

// -----------------------------------------------------------------------

#define OSFACTORREVERB 2


#define GRAINSIZEPITCH 8192

#define NUMGRAINS 4
#define RNDOFFSETPITCH 240

template<typename T>
class PitchShiftRnd : private tsl::random::RandBase {
public:
    PitchShiftRnd() = default;

    void init(int32_t grainsize = GRAINSIZEPITCH, int32_t rndoffset = RNDOFFSETPITCH) {
        _grainsize = grainsize;
        _delayandmask = 4 * _grainsize - 1;
        _rndoffset = rndoffset;
        _sizeoutbuf = _grainsize * 2;
        _outbufandmask = _sizeoutbuf - 1;
        setShift(0);
        _window.resize(_grainsize, 0);
        _inbuf.resize(4 * _grainsize, 0);
        _outbuf.resize(_sizeoutbuf, 0);
        for (int32_t i = 0; i < _grainsize; i++)
            //_window[i] = (T) (1. / m) * (m - std::abs(i % (2 * m) - m));
            _window[i] = (T) (0.5 - 0.5 * cos((double) i * TWOPI_P / (double) _grainsize)) *
                         (2. / (float) NUMGRAINS);
    }

    void clear() {
        std::fill(_inbuf.begin(), _inbuf.end(), 0);
        std::fill(_outbuf.begin(), _outbuf.end(), 0);
    }

    void setShift(T shift) {
        _shift = shift;
        for (int32_t i = 0; i < 2; i++) {
            if (_mode == 0 || _mode == 1 || _mode == 3) {
                _rates[i] = pow(2, _shift / 12.);
            } else {
                _rates[i] = i & 1 ? pow(2, _shift / 12.) : pow(2, -_shift / 12.);
            }
        }
    }

    void setMode(int32_t mode) {
        _mode = mode;
        setShift(_shift);
    }

    inline T tick(T input) {
        const MYFLOAT pos = (MYFLOAT) _count / (MYFLOAT) (_grainsize / (NUMGRAINS));
        int32_t posint = (int) pos;
        if (pos - posint == 0) {
            posint &= 1;
            auto &rss = rs[posint];
            if (_oldrates[posint] != _rates[posint]) {
                _oldrates[posint] = _rates[posint];
                rss.init(_oldrates[posint]);
            }
            rss.clear();
            int32_t readpos =
                    (_inpointer - (int32_t) rs->samplesToWrite(_grainsize) - 1) & _delayandmask;
            int32_t outpointer = _outpointer + (int) (randGab * _rndoffset);
            int32_t dir = 1;
            if (_mode >= 3) {
                dir = -1;
                outpointer += _grainsize;
            }
            int32_t frameswritten = 0;
            while (frameswritten < _grainsize) {
                while (rss.isWriteNeeded()) {
                    rss.writeNextFrame(_inbuf[(readpos++) & _delayandmask]);
                }
                outpointer &= _outbufandmask;
                _outbuf[outpointer] += rs->readNextFrame() * _window[frameswritten++];
                outpointer += dir;
            }
        }
        (++_count) &= _delayandmask;

        _inbuf[_inpointer++] = input;
        _inpointer &= _delayandmask;

        T out = _outbuf[_outpointer];
        _outbuf[_outpointer++] = 0;
        _outpointer &= _outbufandmask;

        return out;
    }

protected:
    int32_t _grainsize;
    int32_t _delayandmask;
    int32_t _rndoffset;
    int32_t _sizeoutbuf;
    int32_t _outbufandmask;
    std::vector<T> _inbuf;
    std::vector<T> _outbuf;
    std::vector<T> _window;
    int32_t _inpointer{}, _outpointer{};
    uint32_t holdrand{};
    int32_t _count{};
    T _rates[2]{}, _oldrates[2]{};
    T _shift{};
    int32_t _mode{};
    ResamplerRounded<MYFLOAT> rs[2];
};

class ShimmerDark : public Effect {
    typedef void (ShimmerDark::*ShimmerProcess)(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl,
                                                MYFLOAT *outr, int);

public:
    ShimmerDark(TRACK *t) : Effect(t, SPACE_REVERB6, STEREOEFFECT),
                            _filtl(_STATE->sr, LOG2NORMALF(_STATE->params[t->index][REV7HPCUT]),
                                   LOG2NORMALF(_STATE->params[t->index][REV7LPCUT])),
                            _filtr(_STATE->sr, LOG2NORMAL(_STATE->params[t->index][REV7HPCUT]),
                                   LOG2NORMAL(_STATE->params[t->index][REV7LPCUT])),
                            _filtln(_STATE->sr,
                                    LOG2NORMALF(_STATE->params[t->index][REV7HPCUT]),
                                    LOG2NORMALF(_STATE->params[t->index][REV7LPCUT])),
                            _filtrn(_STATE->sr,
                                    LOG2NORMALF(_STATE->params[t->index][REV7HPCUT]),
                                    LOG2NORMALF(_STATE->params[t->index][REV7LPCUT]))
    {
        _pitchshiftL.init(GRAINSIZEPITCH / OSFACTORREVERB, RNDOFFSETPITCH / OSFACTORREVERB);
        _pitchshiftR.init(GRAINSIZEPITCH / OSFACTORREVERB, RNDOFFSETPITCH / OSFACTORREVERB);
        _pitchshiftLn.init();
        _pitchshiftRn.init();
        rsUp.init(0.5);
        rsDown.init(2);
        _depth = &_STATE->params[t->index][REV7DEPTH];
        _rate = &_STATE->params[t->index][REV7RATE];
        _fb = &_STATE->params[t->index][REV7FB];
        _revsize = &_STATE->params[t->index][REV7SIZE];
        _revsizeold = *_revsize;
        _diff = &_STATE->params[t->index][REV7DIFF];
        _mix = &_STATE->params[t->index][REV7MIX];
        _gain = &_STATE->params[t->index][REV7GAIN];
        _smooth2 = dbToLinear60(*_gain);
        _shift = &_STATE->params[t->index][REV7SHIFT];
        _shiftmode = &_STATE->params[t->index][REV7SHIFTMODE];
        _bypass = &t->bypass[SPACE_REVERB6];
        _darkmode = &_STATE->params[t->index][SHIMMERDARKMODE];
        _darkmodeold = *_darkmode;
        _lpcut = &_STATE->params[t->index][REV7LPCUT];
        _lpcutold = *_lpcut;
        _hpcut = &_STATE->params[t->index][REV7HPCUT];
        _hpcutold = *_hpcut;

        for (int32_t i = 0; i < NUMAPCHANNEL; i++) {
            _apln[i]._appState = _aprn[i]._appState = _appState;
            _apl[i].init(_STATE->sr * .5,
                         reverbParams2[i][0] / 44100 * 8,
                         reverbParams2[i][1] * .5);
            _apr[i].init(_STATE->sr * .5,
                         reverbParams2[i][0] / 44100. * 8 + 10. / _STATE->sr,
                         reverbParams2[i][1] * .5);
            _apln[i].init(_STATE->sr,
                          reverbParams2[i][0] / 44100. * 8,
                          reverbParams2[i][1] * 2);
            _aprn[i].init(_STATE->sr,
                          reverbParams2[i][0] / 44100. * 8 +
                          10. / _STATE->sr,
                          reverbParams2[i][1] * 2);
        }
        flush();
    }

    void computedark(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
        check();

        MYFLOAT fb = *_fb;

        MYFLOAT gain = dbToLinear60(*_gain), mix;
        if (*_bypass || destroyRequested) {
            mix = 0;
        } else {
            mix = *_mix;
        }

        MYFLOAT shiftwet = _shiftmodeold != 0 ? 1.0 : 0;
        MYFLOAT shiftdry = 1.f - shiftwet;
        shiftwet *= fb;
        shiftdry *= fb;

        UDD(_state);

        _filtl.undenormalize();
        _filtr.undenormalize();

        for (int32_t i = 0; i < s; i += 2) {
            //inl[i] = inl[i+1] = allpass3L_34_37.tick3(inl[i]) * .5f - allpass3R_52_55.tick3(inl[i]) * .5f;
            //continue;
            // outr[i] = outl[i] = _pitchshiftL.ticklin(inl[i]);
            // outr[i+1] = outl[i+1] = outr[i];
            // continue;
            MYFLOAT tmp[2]{inl[i], inr[i]};
            rsDown.writeNextFrame(tmp);
            tmp[0] = inl[i + 1];
            tmp[1] = inr[i + 1];
            rsDown.writeNextFrame(tmp);
            rsDown.readNextFrame(tmp);

            _state = shiftdry * _state + shiftwet * _pitchshiftL.tick(_state);

            _state = _filtl.tick(_state + /*allpass3L_34_37.tick3(*/tmp[0] * .5/*)*/);

            for (int32_t n = 0; n < NUMAPCHANNEL; n++) {
                _state = _apl[n].tickok(_state);
            }

            //_rsbufL[i] *= mixsrc;

            tmp[0] = _state * _smooth1 * _fade * _smooth2;

            // _state = shiftdry * _state + shiftwet * _pitchshiftR.tick(_state);

            _state = _filtr.tick(_state + /*allpass3R_52_55.tick3(*/tmp[1] * .5/*)*/);

            for (int32_t n = 0; n < NUMAPCHANNEL; n++) {
                _state = _apr[n].tickok(_state);
            }
            //_rsbufR[i] *= mixsrc;

            tmp[1] = _state * _smooth1 * _fade * _smooth2;

            rsUp.writeNextFrame(tmp);
            const MYFLOAT mixsrc = 1. - _smooth1;

            rsUp.readNextFrame(tmp);
            outl[i] = outl[i] * mixsrc + tmp[0];
            outr[i] = outr[i] * mixsrc + tmp[1];

            smmixgain(mix, gain);
            rsUp.readNextFrame(tmp);

            outl[i + 1] = outl[i + 1] * mixsrc + tmp[0];
            outr[i + 1] = outr[i + 1] * mixsrc + tmp[1];

            smmixgain(mix, gain);

            if (_fade < 0) {
                _fadeinc = _fadeconst;
                _fade = 0;
                flush();
            } else if (_fade > 1) {
                _fadeinc = 0;
                _fade = 1;
            } else {
                _fade += 2 * _fadeinc;
            }
        }
    }


    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override {
        (this->*funcs[(int) _darkmodeold])(inl, inr, outl, outr, s);
    }

    void computedark2(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
        check();

        MYFLOAT fb = *_fb;

        MYFLOAT gain = dbToLinear60(*_gain), mix;
        if (*_bypass || destroyRequested) {
            mix = 0;
        } else {
            mix = *_mix;
        }

        MYFLOAT shiftwet = _shiftmodeold != 0 ? 1.0 : 0;
        MYFLOAT shiftdry = 1.f - shiftwet;

        UDD(_state);

        _filtl.undenormalize();
        _filtr.undenormalize();

        for (int32_t i = 0; i < s; i += 2) {
            //inl[i] = inl[i+1] = allpass3L_34_37.tick3(inl[i]) * .5f - allpass3R_52_55.tick3(inl[i]) * .5f;
            //continue;
            // outr[i] = outl[i] = _pitchshiftL.ticklin(inl[i]);
            // outr[i+1] = outl[i+1] = outr[i];
            // continue;
            MYFLOAT tmp[2]{inl[i], inr[i]};
            rsDown.writeNextFrame(tmp);
            tmp[0] = inl[i + 1];
            tmp[1] = inr[i + 1];
            rsDown.writeNextFrame(tmp);
            rsDown.readNextFrame(tmp);
            MYFLOAT ainL = _filtl.tick(shiftdry * _state + shiftwet * _pitchshiftL.tick(_state) +
                                       /*allpass3L_34_37.tick3(*/tmp[0] * .5/*)*/);
            MYFLOAT ainR = _filtr.tick(shiftdry * _state + shiftwet * _pitchshiftR.tick(_state) +
                                       /*allpass3L_34_37.tick3(*/tmp[1] * .5/*)*/);

            MYFLOAT aoutL = 0.0;
            MYFLOAT aoutR = 0.0;

            for (auto &ap : _apl)
                aoutL += (ainL = ap.tickok(ainL));

            for (auto &ap : _apr)
                aoutR += (ainR = ap.tickok(ainR));

            aoutL *= .5;
            aoutR *= .5;

            _state = _filtl.tick((ainL + ainR) * .5) * fb;

            tmp[0] = aoutL * _smooth1 * _fade * _smooth2;
            tmp[1] = aoutR * _smooth1 * _fade * _smooth2;

            rsUp.writeNextFrame(tmp);
            const MYFLOAT mixsrc = 1. - _smooth1;

            rsUp.readNextFrame(tmp);
            outl[i] = outl[i] * mixsrc + tmp[0];
            outr[i] = outr[i] * mixsrc + tmp[1];

            smmixgain(mix, gain);
            rsUp.readNextFrame(tmp);

            outl[i + 1] = outl[i + 1] * mixsrc + tmp[0];
            outr[i + 1] = outr[i + 1] * mixsrc + tmp[1];

            smmixgain(mix, gain);

            if (_fade < 0) {
                _fadeinc = _fadeconst;
                _fade = 0;
                flush();
            } else if (_fade > 1) {
                _fadeinc = 0;
                _fade = 1;
            } else {
                _fade += 2 * _fadeinc;
            }
        }
    }

    void computenormal(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
        check();

        MYFLOAT fb = *_fb;

        MYFLOAT gain = dbToLinear60(*_gain), mix;
        if (*_bypass || destroyRequested) {
            mix = 0;
        } else {
            mix = *_mix;
        }

        MYFLOAT shiftwet = _shiftmodeold != 0 ? 1. : 0;
        MYFLOAT shiftdry = 1. - shiftwet;
        shiftwet *= fb;
        shiftdry *= fb;

        UDD(_staten);

        _filtln.undenormalize();
        _filtrn.undenormalize();

        for (int32_t i = 0; i < s; i++) {
            //inl[i] = inl[i+1] = allpass3L_34_37.tick3(inl[i]) * .5f - allpass3R_52_55.tick3(inl[i]) * .5f;
            //continue;
            //outr[i] = outl[i] = _apln[0].tickok(inl[i]);
            //smmixgain(mix, gain);
            //continue;
            _staten = shiftdry * _staten + shiftwet * _pitchshiftLn.tick(_staten);

            _staten = _filtln.tick(_staten + /*allpass3L_34_37.tick3(*/inl[i] * .5/*)*/);

            for (int32_t n = 0; n < NUMAPCHANNEL; n++) {
                _staten = _apln[n].tick(_staten);
            }

            //_rsbufL[i] *= mixsrc;

            const MYFLOAT left = _staten * _smooth1 * _fade;

            // _staten = shiftdry * _staten + shiftwet * _pitchshiftRn.tick(_staten);

            _staten = _filtrn.tick(_staten + /*allpass3R_52_55.tick3(*/inr[i] * .5/*)*/);

            for (int32_t n = 0; n < NUMAPCHANNEL; n++) {
                _staten = _aprn[n].tick(_staten);
            }
            //_rsbufR[i] *= mixsrc;
            const MYFLOAT right = _staten * _smooth1 * _fade;

            const MYFLOAT mixsrc = 1.f - _smooth1;
            outl[i] = outl[i] * mixsrc + left * _smooth2;
            outr[i] = outr[i] * mixsrc + right * _smooth2;

            smmixgain(mix, gain);

            if (_fade < 0) {
                _fadeinc = _fadeconst;
                _fade = 0;
                flush();
            } else if (_fade > 1) {
                _fadeinc = 0;
                _fade = 1;
            } else {
                _fade += _fadeinc;
            }
        }
    }


    void check() {
        if (_diffold != *_diff) {
            _diffold = *_diff;
            for (int32_t i = 0; i < NUMAPCHANNEL; i++) {
                _apl[i].setDiff((i & 1 ? -1. : 1.) *  (.1 + 0.836 * _diffold));
                _apr[i].setDiff((i & 1 ? -1. : 1.) *  (.1 + 0.836 * _diffold));
                _apln[i].setDiff((i & 1 ? -1. : 1.) *  (.1 + 0.836 * _diffold));
                _aprn[i].setDiff((i & 1 ? -1. : 1.) *  (.1 + 0.836 * _diffold));
            }
            //        allpass3R_52_55.setDiff(_diffold);
            //      allpass3L_34_37.setDiff(_diffold);
        }
        if (_fbold != *_fb) {
            _fbold = *_fb;
        }
        if (_revsizeold != *_revsize || _darkmodeold != *_darkmode) {
            _fadeinc = -_fadeconst;
        }

        if (_depthold != *_depth) {
            _depthold = *_depth;
            for (int32_t i = 0; i < NUMAPCHANNEL; i++) {
                _apl[i].setRndDepth(_depthold);
                _apr[i].setRndDepth(_depthold);
                _apln[i].setRndDepth(_depthold);
                _aprn[i].setRndDepth(_depthold);
            }
        }
        if (_rateold != *_rate) {
            _rateold = *_rate;
            for (int32_t i = 0; i < NUMAPCHANNEL; i++) {
                _apl[i].setRndRate(_modratesL[i] * (.01 + .99 * _rateold));
                _apr[i].setRndRate(_modratesR[i] * (.01 + .99 * _rateold));
                _apln[i].setRndRate(_modratesL[i] * (.01 + .99 * _rateold));
                _aprn[i].setRndRate(_modratesR[i] * (.01 + .99 * _rateold));
            }
        }

        if (_shiftold != *_shift) {
            _shiftold = *_shift;
            _pitchshiftL.setShift(_shiftold);
            _pitchshiftR.setShift(_shiftold);
            _pitchshiftLn.setShift(_shiftold);
            _pitchshiftRn.setShift(_shiftold);
        }

        if (_shiftmodeold != *_shiftmode) {
            _shiftmodeold = *_shiftmode;
            _pitchshiftL.setMode((int) _shiftmodeold);
            _pitchshiftR.setMode((int) _shiftmodeold);
            _pitchshiftLn.setMode((int) _shiftmodeold);
            _pitchshiftRn.setMode((int) _shiftmodeold);
        }

        if (_hpcutold != *_hpcut) {
            _hpcutold = *_hpcut;
            _filtl.setNextHp(LOG2NORMALF(_hpcutold));
            _filtr.setNextHp(LOG2NORMALF(_hpcutold));
            _filtln.setNextHp(LOG2NORMALF(_hpcutold));
            _filtrn.setNextHp(LOG2NORMALF(_hpcutold));
        }

        if (_lpcutold != *_lpcut) {
            _lpcutold = *_lpcut;
            _filtl.setNextLp(LOG2NORMALF(_lpcutold));
            _filtr.setNextLp(LOG2NORMALF(_lpcutold));
            _filtln.setNextLp(LOG2NORMALF(_lpcutold));
            _filtrn.setNextLp(LOG2NORMALF(_lpcutold));
        }
    }


    void flush() {
        _darkmodeold = *_darkmode;
        _revsizeold = *_revsize;
        MYFLOAT size = .1 + .9 * _revsizeold;

        for (int32_t i = 0; i < NUMAPCHANNEL; i++) {
            _apl[i].setScale(size);
            _apr[i].setScale(size);
            _apln[i].setScale(size);
            _aprn[i].setScale(size);
        }

        _pitchshiftL.clear();
        _pitchshiftR.clear();
        _pitchshiftLn.clear();
        _pitchshiftRn.clear();


        _filtl.reset();
        _filtr.reset();
        _filtln.reset();
        _filtrn.reset();
        _staten = _state = 0;
        rsUp.clear();
        rsDown.clear();
    };

private:
    static constexpr MYFLOAT reverbParams2[8][4] = {
            {2473.0, 0.0010, 3.100, 1966.0},
            {2767.0, 0.0011, 3.500, 29491.0},
            {3217.0, 0.0017, 1.110, 22937.0},
            {3557.0, 0.0006, 3.973, 9830.0},
            {3907.0, 0.0010, 2.341, 20643.0},
            {4127.0, 0.0011, 1.897, 22937.0},
            {2143.0, 0.0017, 0.891, 29491.0},
            {1933.0, 0.0006, 3.221, 14417.0}
    };

    static constexpr MYFLOAT _modratesL[8] = {3.100, 3.500, 1.110, 3.973, 2.341, 1.897, 0.891,
                                              3.221};
    static constexpr MYFLOAT _modratesR[8] = {3.029, 3.412, 1.003, 3.723, 2.254, 1.702, 0.793,
                                              3.122};
    ShimmerProcess funcs[2] = {&ShimmerDark::computedark, &ShimmerDark::computenormal};
    std::atomic<MYFLOAT> *_fb, *_revsize, *_diff, *_mix, *_gain, *_depth, *_rate, *_shift, *_shiftmode, *_darkmode, *_lpcut, *_hpcut;
    MYFLOAT _fbold{}, _revsizeold{}, _diffold{}, _depthold{-1}, _rateold{
            -1}, _shiftold{}, _shiftmodeold{}, _darkmodeold{}, _hpcutold{}, _lpcutold{};
    Ap1<MYFLOAT> _apl[NUMAPCHANNEL], _apr[NUMAPCHANNEL];
    ApSincSpline<MYFLOAT> _apln[NUMAPCHANNEL], _aprn[NUMAPCHANNEL];

    MYFLOAT _state{}, _staten{};

    PitchShiftRnd<MYFLOAT> _pitchshiftL, _pitchshiftR, _pitchshiftLn, _pitchshiftRn;
    ReverbButter1<MYFLOAT> _filtl, _filtr, _filtln, _filtrn;
    MultiChanPolyPhaseResampler<MYFLOAT, 2, 32, 2> rsUp, rsDown;
#ifdef HQRS
    std::vector<MYFLOAT> _rsbufL, _rsbufR;
    SwrContext *swrCtxUpL = nullptr, *swrCtxDownL = nullptr, *swrCtxUpR = nullptr, *swrCtxDownR = nullptr;;
#endif
};


#endif //GRAINSTORM_SHIMMER_H
