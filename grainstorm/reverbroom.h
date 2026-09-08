#pragma once
//
// Created by pr on 31.08.20.
#ifndef GRAINSTORM_REVERBROOM_H
#define GRAINSTORM_REVERBROOM_H


#include "Allpass.h"
#include "Reverb.h"
#include "reverbearlyrefl.h"
#include <atomic>

template<typename T>
class reverbroombase {
public:
    reverbroombase() = default;

    virtual ~reverbroombase() = default;

    virtual inline T tick(T in) = 0;

    virtual void process(float *, float *, float *, float *, int) {};

    virtual void reset() = 0;

protected:
};



template<typename T>
class smallroom : public reverbroombase<T> {
public:
    smallroom(T sr, T offset = 1.0) : _lpf(sr), _bpf(sr) {
        _lpf.setLp(6000.);
        _bpf.setLp(2000);
        _bpf.setHp(1200.);
        _apn1.init1(sr, (0.066 - 0.030) * offset, .08, 0.030 * offset, .3);
        _apn2.init2(sr, (0.035 - 0.0022 - 0.00083) * offset, .15, 0.0022 * offset, .25,
                    0.00083 * offset, .3);
        _del.init(0.024 * sr * offset);
    }

    void process(T *in, T *out, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T tap1 = _apn2.tick2(
                    _del.readwrite(_lpf.ticklp(in[i]) + .5 * _bpf.ticklphp(.5 * _lastout)));
            _lastout = _apn1.tick1(tap1);
            out[i] = .5 * tap1 + .5 * _lastout;
        }
    }

    inline T tick(T in) {
        T tap1 = _apn2.tick2(_del.readwrite(_lpf.ticklp(in) + .5 * _bpf.ticklphp(.5 * _lastout)));
        _lastout = _apn1.tick1(tap1);
        return .5 * tap1 + .5 * _lastout;
    }


    void reset() {
        _apn1.reset();
        _apn2.reset();
        _del.reset();
        _lpf.reset();
        _bpf.reset();
        _lastout = 0;
    }

private:
    ApNested<T> _apn1, _apn2;
    Delay<T> _del;
    ReverbButter<T> _lpf, _bpf;
    T _lastout{};
};


template<typename T>
class mediumroom : public reverbroombase<T> {
public:
    mediumroom(T sr, T offset = 1.0) : _lpf(sr), _bpf(sr) {
        _lpf.setLp(6000.);
        _bpf.setLp(1250);
        _bpf.setHp(750.);
        _ap1.init(0.030 * sr * offset, .45);
        _apn1.init1(sr, (0.039 - 0.0098) * offset, .25, 0.0098 * offset, .35);
        _apn2.init2(sr, (0.035 - 0.0083 - 0.022) * offset, .25, 0.0083 * offset, .35,
                    0.022 * offset, .45);
        _del1.init(0.005 * sr * offset);
        _del2.init(0.067 * sr * offset);
        _del3.init(0.015 * sr * offset);
        _del4.init(0.108 * sr * offset);
    }

    void process(T *in, T *out, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T tap1 = _apn2.tick2(
                    _lpf.ticklp(in[i]) + _bpf.ticklphp(.4 * _del4.readwrite(_lastout)));
            T tap2 = _del2.readwrite(_ap1.tick(_del1.readwrite(tap1)));
            _lastout = _apn1.tick1(0.4 * _del3.readwrite(tap2) + .5 * in[i]);
            out[i] = .5 * tap1 + .5 * tap2 + .5 * _lastout;
        }
    }

    inline T tick(T in) {
        T tap1 = _apn2.tick2(_lpf.ticklp(in) + _bpf.ticklphp(.4 * _del4.readwrite(_lastout)));
        T tap2 = _del2.readwrite(_ap1.tick(_del1.readwrite(tap1)));
        _lastout = _apn1.tick1(0.4 * _del3.readwrite(tap2) + .5 * in);
        return .5 * tap1 + .5 * tap2 + .5 * _lastout;
    }


    void reset() {
        _ap1.reset();
        _apn1.reset();
        _apn2.reset();
        _del1.reset();
        _del2.reset();
        _del3.reset();
        _del4.reset();
        _lpf.reset();
        _bpf.reset();
        _lastout = 0;
    }

private:
    Ap<T> _ap1;
    ApNested<T> _apn1, _apn2;
    Delay<T> _del1, _del2, _del3, _del4;
    ReverbButter<T> _lpf, _bpf;
    T _lastout{};
};


template<typename T>
class largeroom : public reverbroombase<T> {
public:
    largeroom(T sr, T offset = 1.0) : _lpf(sr), _bpf(sr) {
        _lpf.setLp(4000.);
        _bpf.setLp(1250);
        _bpf.setHp(750.);
        _ap1.init(0.008 * sr * offset, .3);
        _ap2.init(0.012 * sr * offset, .3);
        _apn1.init1(sr, (0.087 - 0.062) * offset, .5, 0.062 * offset, .25);
        _apn2.init2(sr, (0.120 - 0.076 - 0.030) * offset, .5, 0.076 * offset, .25, 0.030 * offset,
                    .25);
        _del1.init(0.004 * sr * offset);
        _del2.init(0.017 * sr * offset);
        _del3.init(0.031 * sr * offset);
        _del4.init(0.003 * sr * offset);
    }

    void process(T *in, T *out, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T tap1 = _del1.readwrite(
                    _ap2.tick(_ap1.tick(_lpf.ticklp(in[i]) + .5 * _bpf.ticklphp(.5 * _lastout))));
            T tap2 = _del3.readwrite(_apn1.tick1(_del2.readwrite(tap1)));
            _lastout = _apn2.tick2(_del4.readwrite(tap2));
            out[i] = 1.5 * tap1 + 0.8 * tap2 + 0.8 * _lastout;
        }
    }

    inline T tick(T in) {
        T tap1 = _del1.readwrite(
                _ap2.tick(_ap1.tick(_lpf.ticklp(in) + .5 * _bpf.ticklphp(.5 * _lastout))));
        T tap2 = _del3.readwrite(_apn1.tick1(_del2.readwrite(tap1)));
        _lastout = _apn2.tick2(_del4.readwrite(tap2));
        return .7 * tap1 + .4 * tap2 + .4 * _lastout;
    }

    void reset() {
        _ap1.reset();
        _ap2.reset();
        _apn1.reset();
        _apn2.reset();
        _del1.reset();
        _del2.reset();
        _del3.reset();
        _del4.reset();
        _lpf.reset();
        _bpf.reset();
        _lastout = 0;
    }

private:
    Ap<T> _ap1, _ap2;
    ApNested<T> _apn1, _apn2;
    Delay<T> _del1, _del2, _del3, _del4;
    ReverbButter<T> _lpf, _bpf;
    T _lastout{};
};

class reverbbase : public Effect{
public:
    reverbbase(TRACK *t) : Effect(t, SPACE_REVERB, STEREOEFFECT), _earlyrefl(_STATE->sr){};
protected:
    earlyrefl<float> _earlyrefl;
    std::atomic<float> *_mode, *_early;
    float _oldmode{}, _oldearly{};
    const int32_t _fadeconst = 5000;
    const float _fadefac = 1.f / (float) _fadeconst;
    int32_t _fadeinc{};
    int32_t _fadesampls{_fadeconst};
};

class roomreverb : public reverbbase {
public:
    roomreverb(TRACK *t);

    void compute(float *inl, float *inr, float *outl, float *outr) {
        float mix = *_mix;
        float mixsrc = 1 - mix;
        float g = dbToLinear60(*_gain);
        mix *= g;
        mixsrc *= g;
        reverbroombase<float> *revl = rev[(int) _oldmode][0];
        reverbroombase<float> *revr = rev[(int) _oldmode][1];

        check();

        for (int32_t i = 0; i < _STATE->currentBufSize; i++) {
            float l, r;
            _earlyrefl.tick(inl[i], inr[i], &l, &r);
            outl[i] *= mixsrc;
            outl[i] += revl->tick(l) * mix;
            outr[i] *= mixsrc;
            outr[i] += revr->tick(r) * mix;
            _fadesampls -= _fadeinc;
            if (_fadesampls == 0) {
                _oldmode = *_mode;
                _oldearly = *_early;
                _earlyrefl.loadPresetReflection(_oldearly);
                rev[(int) _oldmode][0]->reset();
                rev[(int) _oldmode][1]->reset();
                _fadeinc = 0;
                _fadesampls = _fadeconst;
            }
        }

    }

    void check() {
        if ((*_mode != _oldmode || *_early != _oldearly) && _fadeinc == 0) {
            _fadeinc = 1;
        }
    }

private:
    largeroom<float> _largel, _larger;
    mediumroom<float> _mediuml, _mediumr;
    smallroom<float> _smalll, _smallr;
    reverbroombase<float> *rev[3][2];
};

#endif //GRAINSTORM_REVERBROOM_H
