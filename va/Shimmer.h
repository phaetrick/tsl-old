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

#define NUMAPCHANNEL 8

// -----------------------------------------------------------------------
template<typename T>
class FastDownSampler {
public:
    void down(T *inl, T *inr, T *outl, T *outr, int size) {
        for (int i = 0, j = 0; i < size; i += 2, j++) {
            auto out = _xl + (inl[i] * .5);
            _xl = inl[i + 1] * .25;
            outl[j] = out + _xl;
            out = _xr + (inr[i] * .5);
            _xr = inr[i + 1] * .25;
            outr[j] = out + _xr;
        }
    }

private:
    T _xl{}, _xr{};
};

#define OSFACTORREVERB 2

class ShimmerDark : public Effect {
    typedef void (ShimmerDark::*ShimmerProcess)(float *inl, float *inr, float *outl, float *outr);

public:
    ShimmerDark(TRACK *t) : Effect(t),
                            _filtl(_sr, LOG2NORMALF(t->params[REV7HPCUT]),
                                   LOG2NORMALF(t->params[REV7LPCUT])),
                            _filtr(_sr, LOG2NORMAL(t->params[REV7HPCUT]),
                                   LOG2NORMAL(t->params[REV7LPCUT])),
                            _filtln(_sr, LOG2NORMALF(t->params[REV7HPCUT]),
                                    LOG2NORMALF(t->params[REV7LPCUT])),
                            _filtrn(_sr, LOG2NORMALF(t->params[REV7HPCUT]),
                                    LOG2NORMALF(t->params[REV7LPCUT])) {
        _pitchshiftL.init(GRAINSIZEPITCH / OSFACTORREVERB, RNDOFFSETPITCH / OSFACTORREVERB);
        _pitchshiftR.init(GRAINSIZEPITCH / OSFACTORREVERB, RNDOFFSETPITCH / OSFACTORREVERB);
        _pitchshiftLn.init();
        _pitchshiftRn.init();
        _depth = &t->params[REV7DEPTH];
        _rate = &t->params[REV7RATE];
        _fb = &t->params[REV7FB];
        _revsize = &t->params[REV7SIZE];
        _revsizeold = *_revsize;
        _diff = &t->params[REV7DIFF];
        _mix = &t->params[REV7MIX];
        _gain = &t->params[REV7GAIN];
        _shift = &t->params[REV7SHIFT];
        _shiftmode = &t->params[REV7SHIFTMODE];
        _bypass = &t->bypass[SPACE_REVERB6];
        _darkmode = &t->params[SHIMMERDARKMODE];
        _darkmodeold = *_darkmode;
        _lpcut = &t->params[REV7LPCUT];
        _lpcutold = *_lpcut;
        _hpcut = &t->params[REV7HPCUT];
        _hpcutold = *_hpcut;

        for (int i = 0; i < NUMAPCHANNEL; i++) {
            _apl[i].init(_sr / OSFACTORREVERB,
                         reverbParams2[i][0] * _sr / 44100. / _sr * (8 / OSFACTORREVERB),
                         reverbParams2[i][1] * 2);
            _apr[i].init(_sr / OSFACTORREVERB,
                         reverbParams2[i][0] * _sr / 44100. / _sr * (8 / OSFACTORREVERB) + 10 / _sr,
                         reverbParams2[i][1] * 2);
            _apln[i].init(_sr,
                          reverbParams2[i][0] * _sr / 44100. / _sr * (8),
                          reverbParams2[i][1] * 2);
            _aprn[i].init(_sr,
                          reverbParams2[i][0] * _sr / 44100. / _sr * (8) + 10 / _sr,
                          reverbParams2[i][1] * 2);
        }


/*

        double _internalsr = 34125.0;

        float _scale{};
        _scale = (_sr / 2.) / _internalsr;
        _internalsr = _sr / 2.;

        allpass3L_34_37.init(1264 * _scale, .25, 816 * _scale, .25,
                             1212 * _scale, .406, .781, .219, _internalsr, 0.0009, 3.4,
                             0.0012,
                             2.4, 0.001, 0.8);
        allpass3R_52_55.init(1340 * _scale, .25, 688 * _scale, .25,
                             1452 * _scale, .406, .188, .812, _internalsr, 0.001, 1.8,
                             0.0011,
                             2.1, 0.0013, 3.2);
*/

        flush();
    }

    void compute(float *inl, float *inr, float *outl, float *outr) {
        (this->*funcs[(int) _darkmodeold])(inl, inr, outl, outr);
    }

    void computedark(float *inl, float *inr, float *outl, float *outr) {
        check();
        float fb = *_fb;
        float gain = LOG2NORMALF(*_gain);

        float mix = *_mix;
        float mixsrc = 1.0f - mix;
        mix *= gain;
        mixsrc *= gain;
        if (_bypass->load()) {
            mix = 0;
            mixsrc = 1.0;
        }
        float shiftwet = _shiftmodeold != 0 ? 1.0f : 0;
        float shiftdry = 1.f - shiftwet;
        shiftwet *= fb;
        shiftdry *= fb;

        UNDENORMAL(_state);

        _filtl.undenormalize();
        _filtr.undenormalize();

        for (int i = 0; i < _size; i += 2) {
            //inl[i] = inl[i+1] = allpass3L_34_37.tick3(inl[i]) * .5f - allpass3R_52_55.tick3(inl[i]) * .5f;
            //continue;
            // outr[i] = outl[i] = _pitchshiftL.ticklin(inl[i]);
            // outr[i+1] = outl[i+1] = outr[i];
            // continue;
            _state = shiftdry * _state + shiftwet * _pitchshiftL.tick(_state);

            _state = _filtl.tick(_state + /*allpass3L_34_37.tick3(*/inl[i] * .5f/*)*/);

            for (int n = 0; n < NUMAPCHANNEL; n++) {
                _state = _apl[n].tickok(_state);
            }

            //_rsbufL[i] *= mixsrc;
            float left = _state * mix * _fadesampls * _fadefac;
            outl[i] *= mixsrc;
            outl[i] += left;
            outl[i + 1] *= mixsrc;
            outl[i + 1] += left;

            _state = shiftdry * _state + shiftwet * _pitchshiftR.tick(_state);

            _state = _filtr.tick(_state + /*allpass3R_52_55.tick3(*/inr[i] * .5f/*)*/);

            for (int n = 0; n < NUMAPCHANNEL; n++) {
                _state = _apr[n].tickok(_state);
            }
            //_rsbufR[i] *= mixsrc;
            float right = _state * mix * _fadesampls * _fadefac;
            outr[i] *= mixsrc;
            outr[i] += right;
            outr[i + 1] *= mixsrc;
            outr[i + 1] += right;

            _fadesampls -= _fadeinc;
            if (_fadesampls == 0) {
                flush();
                _fadeinc = 0;
                _fadesampls = _fadeconst;
            }
        }
    }

    void computenormal(float *inl, float *inr, float *outl, float *outr) {
        check();
        float fb = *_fb;
        float gain = LOG2NORMALF(*_gain);

        float mix = *_mix;
        float mixsrc = 1.0f - mix;
        mix *= gain;
        mixsrc *= gain;
        if (_bypass->load()) {
            mix = 0;
            mixsrc = 1.0;
        }
        float shiftwet = _shiftmodeold != 0 ? 1.f : 0;
        float shiftdry = 1.f - shiftwet;
        shiftwet *= fb;
        shiftdry *= fb;

        UNDENORMAL(_state);

        _filtl.undenormalize();
        _filtr.undenormalize();

        for (int i = 0; i < _size; i++) {
            //inl[i] = inl[i+1] = allpass3L_34_37.tick3(inl[i]) * .5f - allpass3R_52_55.tick3(inl[i]) * .5f;
            //continue;
            // outr[i] = outl[i] = _pitchshiftL.ticklin(inl[i]);
            // continue;
            _staten = shiftdry * _staten + shiftwet * _pitchshiftLn.tick(_staten);

            _staten = _filtln.tick(_staten + /*allpass3L_34_37.tick3(*/inl[i] * .5f/*)*/);

            for (int n = 0; n < NUMAPCHANNEL; n++) {
                _staten = _apln[n].tickok(_staten);
            }

            //_rsbufL[i] *= mixsrc;
            float left = _staten * mix * _fadesampls * _fadefac;
            outl[i] *= mixsrc;
            outl[i] += left;

            _staten = shiftdry * _staten + shiftwet * _pitchshiftRn.tick(_staten);

            _staten = _filtrn.tick(_staten + /*allpass3R_52_55.tick3(*/inr[i] * .5f/*)*/);

            for (int n = 0; n < NUMAPCHANNEL; n++) {
                _staten = _aprn[n].tickok(_staten);
            }
            //_rsbufR[i] *= mixsrc;
            float right = _staten * mix * _fadesampls * _fadefac;
            outr[i] *= mixsrc;
            outr[i] += right;

            _fadesampls -= _fadeinc;
            if (_fadesampls == 0) {
                flush();
                _fadeinc = 0;
                _fadesampls = _fadeconst;
                mix = 0;
            }
        }
    }


    void check() {
        if (_diffold != *_diff) {
            _diffold = *_diff;
            for (int i = 0; i < NUMAPCHANNEL; i++) {
                _apl[i].setDiff(((i & 1) ? -_diffold : _diffold) * .99f);
                _apr[i].setDiff(((i & 1) ?-_diffold : _diffold) * .99f);
                _apln[i].setDiff(((i & 1) ? -_diffold : _diffold) * .99f);
                _aprn[i].setDiff(((i & 1) ? -_diffold : _diffold) * .99f);
            }
            //        allpass3R_52_55.setDiff(_diffold);
            //      allpass3L_34_37.setDiff(_diffold);
        }
        if (_fbold != *_fb) {
            _fbold = *_fb;
        }
        if (_revsizeold != *_revsize && _fadeinc == 0) {
            _revsizeold = *_revsize;
            _fadeinc = _darkmodeold == 1 ? 2 : 1;
        }

        if (_darkmodeold != *_darkmode && _fadeinc == 0) {
            _fadeinc = _darkmodeold == 1 ? 2 : 1;
        }

        if (_depthold != *_depth) {
            _depthold = *_depth;
            for (int i = 0; i < NUMAPCHANNEL; i++) {
                _apl[i].setRndDepth(_depthold);
                _apr[i].setRndDepth(_depthold);
                _apln[i].setRndDepth(_depthold);
                _aprn[i].setRndDepth(_depthold);

            }
        }
        if (_rateold != *_rate) {
            _rateold = *_rate;
            for (int i = 0; i < NUMAPCHANNEL; i++) {
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
        float size = .1 + .9 * _revsizeold;

        for (int i = 0; i < NUMAPCHANNEL; i++) {
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
    };

private:
    static constexpr double reverbParams2[8][4] = {
            {2473.0, 0.0010, 3.100, 1966.0},
            {2767.0, 0.0011, 3.500, 29491.0},
            {3217.0, 0.0017, 1.110, 22937.0},
            {3557.0, 0.0006, 3.973, 9830.0},
            {3907.0, 0.0010, 2.341, 20643.0},
            {4127.0, 0.0011, 1.897, 22937.0},
            {2143.0, 0.0017, 0.891, 29491.0},
            {1933.0, 0.0006, 3.221, 14417.0}
    };

    static constexpr double _modratesL[8] = {3.100, 3.500, 1.110, 3.973, 2.341, 1.897, 0.891,
                                             3.221};
    static constexpr double _modratesR[8] = {3.029, 3.412, 1.003, 3.723, 2.254, 1.702, 0.793,
                                             3.122};
    static constexpr int _fadeconst = 10000;
    static constexpr float _fadefac = 1.f / (float) _fadeconst;
    int _fadeinc{};
    int _fadesampls{_fadeconst};
    ShimmerProcess funcs[2] = {&ShimmerDark::computedark, &ShimmerDark::computenormal};
    std::atomic<float> *_fb, *_revsize, *_diff, *_mix, *_gain, *_depth, *_rate, *_shift, *_shiftmode, *_darkmode, *_lpcut, *_hpcut;
    float _fbold{}, _revsizeold{}, _diffold{}, _depthold{-1}, _rateold{
            -1}, _shiftold{}, _shiftmodeold{}, _darkmodeold{}, _hpcutold{}, _lpcutold{};
    Ap1<float> _apl[NUMAPCHANNEL], _apr[NUMAPCHANNEL];
    Ap1<float> _apln[NUMAPCHANNEL], _aprn[NUMAPCHANNEL];

    float _state{}, _staten{};

    PitchShiftRnd<float> _pitchshiftL, _pitchshiftR, _pitchshiftLn, _pitchshiftRn;
    ReverbButter1<float> _filtl, _filtr, _filtln, _filtrn;


#ifdef HQRS
    std::vector<float> _rsbufL, _rsbufR;
    SwrContext *swrCtxUpL = nullptr, *swrCtxDownL = nullptr, *swrCtxUpR = nullptr, *swrCtxDownR = nullptr;;
#endif
};


#endif //GRAINSTORM_SHIMMER_H
