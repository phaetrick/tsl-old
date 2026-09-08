#pragma once
//
// Created by pr on 15.08.20.
//

#ifndef GRAINSTORM_REVERB_H
#define GRAINSTORM_REVERB_H

#include "DelayBase.h"
#include "base.h"
#include <cmath>
#include <cstring>
#include "Allpass.h"
#include <app.h>

template<typename T>
class DCBlocker {
public:
    void clear() { _z = _output = 0; }

    inline T process(T input) {
        _output = input - _z + _b * _output;
        _z = input;
        return _output;
    }
private:
    T _output{};
    T _b{.9999};
    T _z{};
};

template<typename T>
class OnePole2 {
public:
    void setB0(T b0) { _b0 = b0; };

    //! Set the a[1] coefficient value.
    void setA1(T a1) { _a1 = a1; };

    void setCoefficients(T b0, T a1, bool clearState = false) {
        if (std::abs(a1) >= 1.0) {
            return;
        }

        _b0 = b0;
        _a1 = a1;

        if (clearState) _state = 0;
    }

    void setPole(T thePole) {
        if (std::abs(thePole) >= 1.0) {
            return;
        }

        // Normalize coefficients for peak unity gain.
        if (thePole > 0.0)
            _b0 = (T) (1.0 - thePole);
        else
            _b0 = (T) (1.0 + thePole);

        _a1 = -thePole;
    }

    T tick(T input) {
        return _state = (_b0 * input - _a1 * _state);
    }

    void undenormalize() {
        if(isDouble) {
            UDD(_state);
        }
        else {
            UDF(_state);
        }
    }

private:
    const  bool isDouble = sizeof(T) == sizeof(MYFLOAT);
    T _a1{}, _b0{}, _state{};
};

template<typename T>
class ReverbButter1 {
public:
    ReverbButter1() = default;
    ReverbButter1(T sr, T hpcut = 18, T lpcut = 20000) {
        init(sr, hpcut, lpcut);
    }

    void init(T sr, T hpcut = 18, T lpcut = 20000) {
        pidsr = PI_P / (T) sr;
        currenthp = nexthp = hpcut;
        currentlp = nextlp = lpcut;
        updateLp();
        updateHP();
        reset();
    }

    void reset() {
        y1_h  = 0.0;
        y1_l = 0.0;
    }

    inline T tick(T in) {
        if (nextlp != currentlp) {
            if (DISTANCE(nextlp, currentlp) < 3) {
                currentlp = nextlp;
            } else if (currentlp < nextlp)
                currentlp += 3;
            else
                currentlp -= 3;
            updateLp();
        }

        if (nexthp != currenthp) {
            if (DISTANCE(currenthp, nexthp) < 3) {
                currenthp = nexthp;
            } else if (currenthp < nexthp)
                currenthp += 3;
            else
                currenthp -= 3;
            updateHP();
        }
        T tmp = in * b1_h + y1_h;
        y1_h = tmp * a2_h + in * b2_h;
        T output = tmp * b1_l + y1_l;
        y1_l = output * a2_l + tmp * b2_l;
        return output;
    }

    inline T ticklphp(T in) {
        T tmp = in * b1_h + y1_h;
        y1_h = tmp * a2_h + in * b2_h;
        T output = tmp * b1_l + y1_l;
        y1_l = output * a2_l + tmp * b2_l;
        return output;
    }

    inline T ticklp(T in) {
        T output = in * b1_l + y1_l;
        y1_l = output * a2_l + in * b2_l;
        return output;
    }

    void setNextLp(T lp) {
        nextlp = lp;
    };

    void setNextHp(T hp) {
        nexthp = hp;
    }

    void setLp(T lp) {
        currentlp = nextlp = lp;
        updateLp();
    };

    void setHp(T hp) {
        currenthp = nexthp = hp;
        updateHP();
    }

    void undenormalize() {
        if(isDouble) {
            UDD(y1_h);
            UDD(y1_l);
        } else {
            UDF(y1_h);
            UDF(y1_l);
        }
    }

private:
    const  bool isDouble = sizeof(T) == sizeof(MYFLOAT);
    inline void updateHP() {
        T omega_2 = currenthp * pidsr;
        T tan_omega_2 = std::tan(omega_2);
        b1_h = 1 / (1 + tan_omega_2);
        b2_h = -1 * b1_h;
        a2_h = (1 - tan_omega_2) / (1 + tan_omega_2);
    }

    inline void updateLp() {
        T omega_2 = currentlp * pidsr;
        T tan_omega_2 = std::tan(omega_2);
        b1_l = b2_l = tan_omega_2 / (1 + tan_omega_2);
        a2_l = (1 - tan_omega_2) / (1 + tan_omega_2);
    }

    T nexthp{}, currenthp{}, a2_h{}, b1_h{}, b2_h{}, y1_h{};
    T currentlp{}, nextlp{}, a2_l{}, b1_l{}, b2_l{}, y1_l{};
    T pidsr;
};


template<typename T>
class ReverbButter {
public:
    ReverbButter(T sr, T hpcut = 18, T lpcut = 20000) {
        pidsr = PI_P / (T) sr;
        currenthp = nexthp = hpcut;
        currentlp = nextlp = lpcut;
        updateLp();
        updateHP();
        reset();
    }

    void reset() {
        y1_h = y2_h = 0.0;
        y1_l = y2_l = 0.0;
    }

    inline T tick(T in) {
        if (nextlp != currentlp) {
            if (DISTANCE(nextlp, currentlp) < 3) {
                currentlp = nextlp;
            } else if (currentlp < nextlp)
                currentlp += 3;
            else
                currentlp -= 3;
            updateLp();
        }

        if (nexthp != currenthp) {
            if (DISTANCE(currenthp, nexthp) < 3) {
                currenthp = nexthp;
            } else if (currenthp < nexthp)
                currenthp += 3;
            else
                currenthp -= 3;
            updateHP();
        }

        T y0 = in + b1_h * y1_h + b2_h * y2_h;
        T tmp = a0_h * (y0 - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0;
        y0 = tmp + b1_l * y1_l + b2_l * y2_l;
        tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    inline T ticklphp(T in) {
        T y0 = in + b1_h * y1_h + b2_h * y2_h;
        T tmp = a0_h * (y0 - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0;
        y0 = tmp + b1_l * y1_l + b2_l * y2_l;
        tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    inline T ticklp(T in) {
        T y0 = in + b1_l * y1_l + b2_l * y2_l;
        T tmp = a0_l * (y0 + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0;
        return tmp;
    }

    void setNextLp(T lp) {
        nextlp = lp;
    };

    void setNextHp(T hp) {
        nexthp = hp;
    }

    void setLp(T lp) {
        currentlp = nextlp = lp;
        updateLp();
    };

    void setHp(T hp) {
        currenthp = nexthp = hp;
        updateHP();
    }

    void undenormalize() {
        if(isDouble) {
            UDD(y1_h);
            UDD(y2_h);
            UDD(y1_l);
            UDD(y2_l);        }
        else {
            UDF(y1_h);
            UDF(y2_h);
            UDF(y1_l);
            UDF(y2_l);        }
    }

private:
    const  bool isDouble = sizeof(T) == sizeof(MYFLOAT);
    inline void updateHP() {
        T pfreq = currenthp * pidsr;
        T C = tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0_h = 1.f / (1.f + sqrt2C + C2);
        b1_h = 2.f * (1.f - C2) * a0_h;
        b2_h = -(1.f - sqrt2C + C2) * a0_h;
    }

    inline void updateLp() {
        T pfreq = currentlp * pidsr;
        T C = 1.f / tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0_l = 1.f / (1.f + sqrt2C + C2);
        b1_l = -2.f * (1.f - C2) * a0_l;
        b2_l = -(1.f - sqrt2C + C2) * a0_l;
    }

    T nexthp{}, currenthp{}, a0_h{}, b1_h{}, b2_h{}, y1_h{}, y2_h{};
    T currentlp{}, nextlp{}, a0_l{}, b1_l{}, b2_l{}, y1_l{}, y2_l{};
    T pidsr;
};

template<typename T>
class Filt1 {
public:
    Filt1(void) : _slo(0), _shi(0) {}

    ~Filt1(void) {}


    void set_params(T del, T tmf, T tlo, T wlo, T thi, T chi) {
        T g, t;
        _gmf = pow(0.001, del / tmf);
        _glo = pow(0.001, del / tlo) / _gmf - 1.0;
        _wlo = wlo;
        g = pow(0.001, del / thi) / _gmf;
        t = (1 - g * g) / (2 * g * g * chi);
        _whi = (sqrt(1 + 4 * t) - 1) / (2 * t);
    }

    T process(T x) {
        _slo += _wlo * (x - _slo) + 1e-10;
        x += _glo * _slo;
        _shi += _whi * (x - _shi);
        return _gmf * _shi;
    }

    void reset() {
        _slo = _shi = 0;
    }

private:
    T _gmf;
    T _glo;
    T _wlo;
    T _whi;
    T _slo;
    T _shi;
};

class Pareq {
public:

    Pareq(void);

    ~Pareq(void);

    void setfsamp(MYFLOAT fsamp);

    void setparam(MYFLOAT f, MYFLOAT g) {
        _f0 = f;
        _g0 = powf(10.0f, 0.05f * g);
        _touch0 = _touch0+1;
    }

    void reset(void);

    void prepare(int32_t nsamp);

    void process(int32_t nsamp, int nchan, MYFLOAT *data[]) {
        if (_state != 0) process1(nsamp, nchan, data);
    }

private:

    enum {
        BYPASS, STATIC, SMOOTH, MAXCH = 4
    };

    void calcpar1(int32_t nsamp, MYFLOAT g, MYFLOAT f);

    void process1(int32_t nsamp, int nchan, MYFLOAT *data[]);

    volatile int16_t _touch0;
    volatile int16_t _touch1;
    bool _bypass;
    int32_t _state;
    MYFLOAT _fsamp;

    MYFLOAT _g0, _g1;
    MYFLOAT _f0, _f1;
    MYFLOAT _c1, _dc1;
    MYFLOAT _c2, _dc2;
    MYFLOAT _gg, _dgg;

    MYFLOAT _z1[MAXCH];
    MYFLOAT _z2[MAXCH];
};

template<typename T>
class Delay {
public:
    void init(int32_t size) {
        _line.resize(size, 0);
        _size = _line.size();
    }

    int32_t getDelay() { return _size; };

    T read(void) {
        return _line[_i];
    }

    void write(MYFLOAT x) {
        _line[_i++] = x;
        if (_i == _size) _i = 0;
    }

    T readwrite(T x) {
        T z = _line[_i];
        _line[_i] = x;
        if (++_i == _size) _i = 0;
        return z;
    }

    T tap(int32_t pos) {
        int32_t offset = _i - pos;
        if (offset < 0)
            offset += _size;
        return _line[offset];
    }

    void reset() {
        std::fill(_line.begin(), _line.end(), 0);
        _i = 0;
    }

private:
    int32_t _i{};
    int32_t _size{};
    std::vector<T> _line;
};


template<typename T>
class OnePoleLp {
public:
    void setCoeff(T c) {
        _c = c;
    }

    T tickbw(T in) {
        return _state = (in * _c + (1 - _c) * _state);
    }

    T tickdamping(T in) {
        return _state = (in * (1 - _c) + _c * _state);
    }

    void undenormalize() {
        UNDENORMAL(_state);
    }

private:
    T _c{};
    T _state{};
};

template<typename T>
class Vdelay {
public:

    ~Vdelay(void) {
        fini();
    }


    void init(int32_t size) {
        _size = size;
        _line = new T[size];
        memset(_line, 0, size * sizeof(T));
        _ir = 0;
        _iw = 0;
    }


    void fini(void) {
        delete[] _line;
        _size = 0;
        _line = 0;
    }


    void set_delay(int32_t del) {
        reset();
        _ir = _iw - del;
        if (_ir < 0) _ir += _size;
    }

    inline T read(void) {
        T x = _line[_ir++];
        if (_ir == _size) _ir = 0;
        return x;
    }

    inline void write(T x) {
        _line[_iw++] = x;
        if (_iw == _size) _iw = 0;
    }

    inline T readwrite(T in) {
        write(in);
        return read();
    }

    inline T tap(int32_t delay) {
        int32_t offset = _iw - delay;
        if (offset < 0)
            offset += _size;
        return _line[offset];
    }


    void reset() {
        memset(_line, 0, sizeof(T) * _size);
    }

private:
    int32_t _ir;
    int32_t _iw;
    int32_t _size;
    T *_line;
};


class Reverb {
public:

    Reverb(void) {
    }

    void init(MYFLOAT fsamp) {
        int32_t i, k1, k2;

        _fsamp = fsamp;
        _cntA1 = 1;
        _cntA2 = 0;
        _cntB1 = 1;
        _cntB2 = 0;
        _cntC1 = 1;
        _cntC2 = 0;

        _ipdel = 0.04f;
        _xover = 200.0f;
        _rtlow = 3.0f;
        _rtmid = 2.0f;
        _fdamp = 3e3f;
        _opmix = 0.5f;

        _g0 = 0;
        _g1 = 0;

        _vdelay0.init((int) (0.1f * _fsamp));
        _vdelay1.init((int) (0.1f * _fsamp));
        for (i = 0; i < 8; i++) {
            k1 = (int) (floorf(_tdiff1[i] * _fsamp + 0.5f));
            k2 = (int) (floorf(_tdelay[i] * _fsamp + 0.5f));
            _diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
            _delay[i].init(k2 - k1);
        }

        prepare();
    }


    void prepare() {
        if (_cntA1 != _cntA2) {
            int32_t k = (int) (floorf((_ipdel - 0.020f) * _fsamp + 0.5f));
            _vdelay0.set_delay(k);
            _vdelay1.set_delay(k);
            _cntA2 = _cntA1;
        }

        if (_cntB1 != _cntB2) {
            MYFLOAT chi;
            MYFLOAT wlo = 6.2832f * _xover / _fsamp;
            if (_fdamp > 0.49f * _fsamp) chi = 2;
            else chi = 1 - cosf(6.2832f * _fdamp / _fsamp);
            for (int32_t i = 0; i < 8; i++) {
                _filt1[i].set_params(_tdelay[i], _rtmid, _rtlow, wlo, 0.5f * _rtmid, chi);
            }
            _cntB2 = _cntB1;
        }

        if (_cntC1 != _cntC2) {
            {
                _g0 = (1 - _opmix) * (1 + _opmix);
                _g1 = 0.7f * _opmix * (2 - _opmix) / sqrtf(_rtmid);
            }
            _cntC2 = _cntC1;
        }

        // _pareq1.prepare (nfram);
        // _pareq2.prepare (nfram);
    }

    void process(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t size) {
        MYFLOAT *p0, *p1;
        MYFLOAT *q0, *q1, *q2, *q3;

        MYFLOAT g = sqrtf(0.125f);

        for (int32_t i = 0; i < size; i++) {
            _vdelay0.write(inl[i]);
            _vdelay1.write(inr[i]);

            MYFLOAT t = 0.3f * _vdelay0.read();
            MYFLOAT x0 = _diff1[0].process(_delay[0].read() + t);
            MYFLOAT x1 = _diff1[1].process(_delay[1].read() + t);
            MYFLOAT x2 = _diff1[2].process(_delay[2].read() - t);
            MYFLOAT x3 = _diff1[3].process(_delay[3].read() - t);
            t = 0.3f * _vdelay1.read();
            MYFLOAT x4 = _diff1[4].process(_delay[4].read() + t);
            MYFLOAT x5 = _diff1[5].process(_delay[5].read() + t);
            MYFLOAT x6 = _diff1[6].process(_delay[6].read() - t);
            MYFLOAT x7 = _diff1[7].process(_delay[7].read() - t);

            t = x0 - x1;
            x0 += x1;
            x1 = t;
            t = x2 - x3;
            x2 += x3;
            x3 = t;
            t = x4 - x5;
            x4 += x5;
            x5 = t;
            t = x6 - x7;
            x6 += x7;
            x7 = t;

            t = x0 - x2;
            x0 += x2;
            x2 = t;
            t = x1 - x3;
            x1 += x3;
            x3 = t;
            t = x4 - x6;
            x4 += x6;
            x6 = t;
            t = x5 - x7;
            x5 += x7;
            x7 = t;

            t = x0 - x4;
            x0 += x4;
            x4 = t;
            t = x1 - x5;
            x1 += x5;
            x5 = t;
            t = x2 - x6;
            x2 += x6;
            x6 = t;
            t = x3 - x7;
            x3 += x7;
            x7 = t;

            {
                outl[i] = _g1 * (x1 + x2) + _g0 * inl[i];
                outr[i] = _g1 * (x1 - x2) + _g0 * inr[i];
            }

            _delay[0].write(_filt1[0].process(g * x0));
            _delay[1].write(_filt1[1].process(g * x1));
            _delay[2].write(_filt1[2].process(g * x2));
            _delay[3].write(_filt1[3].process(g * x3));
            _delay[4].write(_filt1[4].process(g * x4));
            _delay[5].write(_filt1[5].process(g * x5));
            _delay[6].write(_filt1[6].process(g * x6));
            _delay[7].write(_filt1[7].process(g * x7));
        }
/*
    n = _ambis ? 4 : 2;
    _pareq1.process (nfram, n, out);
    _pareq2.process (nfram, n, out);
    if (!_ambis)
    {
        for (i = 0; i < nfram; i++)
        {
            _g0 += _d0;
            q0 [i] += _g0 * p0 [i];
            q1 [i] += _g0 * p1 [i];
        }
    }
    */
    }


    void set_delay(MYFLOAT v) {
        _ipdel = v;
        _cntA1++;
    }

    void set_xover(MYFLOAT v) {
        _xover = v;
        _cntB1++;
    }

    void set_rtlow(MYFLOAT v) {
        _rtlow = v;
        _cntB1++;
    }

    void set_rtmid(MYFLOAT v) {
        _rtmid = v;
        _cntB1++;
        _cntC1++;
    }

    void set_fdamp(MYFLOAT v) {
        _fdamp = v;
        _cntB1++;
    }

    void set_opmix(MYFLOAT v) {
        _opmix = v;
        _cntC1++;
    }

    void reset() {
        _g0 = 0;
        _g1 = 0;
        _vdelay0.reset();
        _vdelay1.reset();
        for (int32_t i = 0; i < 8; i++) {
            _diff1[i].reset();
            _delay[i].reset();
            _filt1[i].reset();
        }
    }

private:
    MYFLOAT _fsamp;
    Vdelay<MYFLOAT> _vdelay0;
    Vdelay<MYFLOAT> _vdelay1;
    Ap<MYFLOAT> _diff1[8];
    Filt1<MYFLOAT> _filt1[8];
    Delay<MYFLOAT> _delay[8];

    int32_t _cntA1;
    int32_t _cntB1;
    int32_t _cntC1;
    int32_t _cntA2;
    int32_t _cntB2;
    int32_t _cntC2;

    MYFLOAT _ipdel;
    MYFLOAT _xover;
    MYFLOAT _rtlow;
    MYFLOAT _rtmid;
    MYFLOAT _fdamp;
    MYFLOAT _opmix;

    MYFLOAT _g0;
    MYFLOAT _g1;

    MYFLOAT _tdiff1[8]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
            22904e-6f,
            29291e-6f,
            13458e-6f,
            19123e-6f
    };
    MYFLOAT _tdelay[8]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
            174713e-6f,
            192303e-6f,
            125000e-6f,
            219991e-6f
    };
};

class ReverbFDN {
public:

    ReverbFDN(void) {
    }

    void init(MYFLOAT fsamp) {
        _sr = fsamp;
        MYFLOAT sum = 0;
        _scale = _sr / 48000.f;
        for (int32_t i = 0; i < 8; i++) {
            int32_t k1 = (int) (floorf(_tdiff1[i] * _scale * _sr + 0.5f));
            int32_t k2 = (int) (floorf(_tdelay[i] * _scale * _sr + 0.5f));
            //_diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
            _diff1[i].init(_sr, k1 / _sr, 64 * _scale / _sr);
            _diff1[i].setDiff((i & 1) ? -0.6f : 0.6f);
            _delay[i].init(k2 - k1);
            sum += k2;
        }
        _averagedelay = sum / 8.f;
    }

    void process(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            //_vdelay0.write(inl[i]);
            //_vdelay1.write(inr[i]);

            MYFLOAT t = inl[i];//_vdelay0.read();
            MYFLOAT x0 = _diff1[0].tickok(_delay[0].read() + t);
            MYFLOAT x1 = _diff1[1].tickok(_delay[1].read() + t);
            MYFLOAT x2 = _diff1[2].tickok(_delay[2].read() - t);
            MYFLOAT x3 = _diff1[3].tickok(_delay[3].read() - t);
            t = inr[i];//_vdelay1.read();
            MYFLOAT x4 = _diff1[4].tickok(_delay[4].read() + t);
            MYFLOAT x5 = _diff1[5].tickok(_delay[5].read() + t);
            MYFLOAT x6 = _diff1[6].tickok(_delay[6].read() - t);
            MYFLOAT x7 = _diff1[7].tickok(_delay[7].read() - t);

            t = x0 - x1;
            x0 += x1;
            x1 = t;
            t = x2 - x3;
            x2 += x3;
            x3 = t;
            t = x4 - x5;
            x4 += x5;
            x5 = t;
            t = x6 - x7;
            x6 += x7;
            x7 = t;

            t = x0 - x2;
            x0 += x2;
            x2 = t;
            t = x1 - x3;
            x1 += x3;
            x3 = t;
            t = x4 - x6;
            x4 += x6;
            x6 = t;
            t = x5 - x7;
            x5 += x7;
            x7 = t;

            t = x0 - x4;
            x0 += x4;
            x4 = t;
            t = x1 - x5;
            x1 += x5;
            x5 = t;
            t = x2 - x6;
            x2 += x6;
            x6 = t;
            t = x3 - x7;
            x3 += x7;
            x7 = t;

            {
                outl[i] = _mix * (x1 + x2) + _mixsrc * inl[i];
                outr[i] = _mix * (x1 - x2) + _mixsrc * inr[i];
            }

            _filtstate[0] = (x0 * (1 - _c) + _c * _filtstate[0]);
            _filtstate[1] = (x1 * (1 - _c) + _c * _filtstate[1]);
            _filtstate[2] = (x2 * (1 - _c) + _c * _filtstate[2]);
            _filtstate[3] = (x3 * (1 - _c) + _c * _filtstate[3]);
            _filtstate[4] = (x4 * (1 - _c) + _c * _filtstate[4]);
            _filtstate[5] = (x5 * (1 - _c) + _c * _filtstate[5]);
            _filtstate[6] = (x6 * (1 - _c) + _c * _filtstate[6]);
            _filtstate[7] = (x7 * (1 - _c) + _c * _filtstate[7]);


            _delay[0].write(_feedback[0] * _filtstate[0]);
            _delay[1].write(_feedback[1] * _filtstate[1]);
            _delay[2].write(_feedback[2] * _filtstate[2]);
            _delay[3].write(_feedback[3] * _filtstate[3]);
            _delay[4].write(_feedback[4] * _filtstate[4]);
            _delay[5].write(_feedback[5] * _filtstate[5]);
            _delay[6].write(_feedback[6] * _filtstate[6]);
            _delay[7].write(_feedback[7] * _filtstate[7]);
        }
/*
    n = _ambis ? 4 : 2;
    _pareq1.process (nfram, n, out);
    _pareq2.process (nfram, n, out);
    if (!_ambis)
    {
        for (i = 0; i < nfram; i++)
        {
            _g0 += _d0;
            q0 [i] += _g0 * p0 [i];
            q1 [i] += _g0 * p1 [i];
        }
    }
    */
    }

    void reset() {
        for (int32_t i = 0; i < 4; i++) {
            _diff1[i].reset();
            _delay[i].reset();
            _filtstate[i] = 0;
        }
    }

    void setDamp(MYFLOAT damp) {
        _c = damp;
    }

    void setT60(MYFLOAT T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < 8; i++) {
                _feedback[i] = 0;
            }
            _gainfact = 1.f;
        } else {
            for (int32_t i = 0; i < 8; i++) {
                _feedback[i] =
                        powf(10.0, (-3.0f * (int) (floorf(_tdelay[i] * _scale * _sr + 0.5f)) /
                                    (T60 * _sr))) * .25f;
            }
            _gainfact = 1 / (powf(0.001f, (_averagedelay) / (T60 * _sr)) * 3.f);
        }
        if (_gainfact > 1)
            _gainfact = 1;
        _rt = T60;
        set_mix(_opmix);
    }


    void set_mix(MYFLOAT mix) {
        _opmix = mix;
        _mixsrc = (1 - mix) * (1 + mix);
        _mix = 0.7f * mix * (2 - mix) / sqrtf(_rt);
    }

    void setGain(MYFLOAT g) {
        _gain = g;
    };


private:
    MYFLOAT _sr, _scale, _rt{1}, _opmix{1};
    Ap1<MYFLOAT> _diff1[8];
    Delay<MYFLOAT> _delay[8];
    MYFLOAT _filtstate[8]{};
    MYFLOAT _feedback[8]{};
    MYFLOAT _c{}, _gain{1};
    MYFLOAT _mix{1}, _mixsrc{0};
    MYFLOAT _averagedelay, _gainfact{.5};
    MYFLOAT _tdiff1[8]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
            22904e-6f,
            29291e-6f,
            13458e-6f,
            19123e-6f
    };
    MYFLOAT _tdelay[8]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
            174713e-6f,
            192303e-6f,
            125000e-6f,
            219991e-6f
    };
};

#define GRAINSIZEDAREV 8192

class DaRev : public Effect {
public:
    DaRev(TRACK *t);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override {
        check();
        reverbFdn.process(inl, inr, inl, inr, s);
    }

private:
    void check() {
        if (_olddamp != *_damp) {
            _olddamp = *_damp;
            reverbFdn.setDamp(.5f + _olddamp * .45f);
        }
        if (_oldt60 != *_t60) {
            _oldt60 = *_t60;
            reverbFdn.setT60(LOG2NORMALF(_oldt60));
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverbFdn.set_mix(_oldmix);
        }
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            reverbFdn.setGain(dbToLinear60(_oldgain));
        }
    }

    std::atomic<MYFLOAT> *_damp{}, *_t60{};
    MYFLOAT _olddamp{-100000000}, _oldt60{-1000000000}, _oldmix{-1}, _oldgain{-1000};
    ReverbFDN reverbFdn;
    MYFLOAT _window[GRAINSIZEDAREV]{};

};

#define NUMCOMBS 4
#define NUMALLPASS 2

class SimpleReverb : public Effect {
public:
    SimpleReverb(TRACK *track, int32_t channel);

    void compute(MYFLOAT *in, int32_t size) override;

    void check() {
        MYFLOAT t60 = *_t60;
        if (_prvt60 != t60) {
            _prvt60 = t60;
            setT60(_prvt60 * 20);
        }
    }

private:
    MYFLOAT _prvt60{}, _prdmp{};
    std::atomic<MYFLOAT> *_t60, *_damp{};
    Ap<MYFLOAT> _diff1[4];
    Delay<MYFLOAT> _delay[4];
    MYFLOAT _filtstate[4]{};
    MYFLOAT _feedback[4]{};
    MYFLOAT _c{};
    MYFLOAT _averagedelay{}, _gainfact{.5};
    MYFLOAT _tdiff1[4]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
    };
    MYFLOAT _tdelay[4]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
    };

    void setDamp(MYFLOAT damp) {
        _c = damp * .75f;
    }

    void setT60(MYFLOAT T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < 4; i++) {
                _feedback[i] = 0;
            }
            _gainfact = 1.f;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                _feedback[i] = pow(10.0, (-3.0 * (int) (floorf(_tdelay[i] * _STATE->sr + 0.5)) /
                                           (T60 * _STATE->sr))) * .5;
            }
            _gainfact = 1. / (pow(0.001, (_averagedelay) / (T60 * _STATE->sr)) * 2.);
        }
        if (_gainfact > 1)
            _gainfact = 1;
    }

    void init() {
        MYFLOAT sum = 0;
        for (int32_t i = 0; i < 4; i++) {
            int32_t k1 = (int) ((floor(_tdiff1[i] * _STATE->sr + 0.5)) * (_chan == 1 ? 1. : 1.01));
            int32_t k2 = (int) ((floor(_tdelay[i] * _STATE->sr + 0.5)) * (_chan == 1 ? 1. : 1.01));
            _diff1[i].init(k1, (i & 1) ? -0.6 : 0.6);
            _delay[i].init(k2 - k1);
            sum += k2;
        }
        _averagedelay = sum / 4.;
    }
};


class SimpleReverb2 : public Effect {
public:
    SimpleReverb2(TRACK *track, int32_t channel);

    int32_t compsize() {
        int32_t s = 0;
        for (int32_t i = 0; i < NUMCOMBS + NUMALLPASS; i++)
            s += MYFLT2LRND(_lpt[i] * _STATE->sr);
        return s;
    }

    void compute(MYFLOAT *in, int32_t size)override ;

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override {
        MYFLOAT mix = (MYFLOAT) *_mix;
        MYFLOAT mixsrc = 1.0f - mix;
        if (*_rvt != _prvt) {
            _prvt = *_rvt;
            reset();
        }
        for (int32_t i = 0; i < s; i++) {
            MYFLOAT inn = inl[i];
            MYFLOAT tmp = *_xp[0];
            *_xp[0] *= _coef[0];
            *_xp[0] += inn;
            MYFLOAT out0 = tmp;
            if (++_xp[0] >= _end[0])
                _xp[0] = _start[0];

            tmp = *_xp[1];
            *_xp[1] *= _coef[1];
            *_xp[1] += inn;
            MYFLOAT out1 = tmp;
            if (++_xp[1] >= _end[1])
                _xp[1] = _start[1];

            tmp = *_xp[2];
            *_xp[2] *= _coef[2];
            *_xp[2] += inn;
            MYFLOAT out2 = tmp;
            if (++_xp[2] >= _end[2])
                _xp[2] = _start[2];

            tmp = *_xp[3];
            *_xp[3] *= _coef[3];
            *_xp[3] += inn;
            MYFLOAT out3 = tmp;
            if (++_xp[3] >= _end[3])
                _xp[3] = _start[3];

            MYFLOAT sum = out0 + out1 + out2 + out3;

            MYFLOAT y = *_xp[4], z;
            *_xp[4] = z = _coef[4] * y + sum;
            MYFLOAT out4 = y - _coef[4] * z;
            if (++_xp[4] >= _end[4])
                _xp[4] = _start[4];

            y = *_xp[5];
            *_xp[5] = z = _coef[5] * y + out4;
            MYFLOAT res = y - _coef[5] * z;
            if (++_xp[5] >= _end[5])
                _xp[5] = _start[5];

            outl[i] = outr[i] = inn * mixsrc + res * mix;
        }
    }

    void reset() {
        for (int32_t i = 0; i < NUMCOMBS; i++) {
            MYFLOAT exp_arg = (MYFLOAT) (log001 * _lpt[i] / _prvt);
            if (exp_arg < -36.8413615)    /* ln(1.0e-16) */
                _coef[i] = 0.0;
            else
                _coef[i] = (MYFLOAT) exp(exp_arg);
        }
    }

private:
    const MYFLOAT _lpt[
            NUMCOMBS + NUMALLPASS] = {0.0297, 0.0371, 0.0411, 0.0437, 0.005, 0.02291};
    MYFLOAT _prvt;
    std::atomic<MYFLOAT> *_rvt;
    MYFLOAT _filtstate{};
    MYFLOAT _coef[NUMCOMBS + NUMALLPASS]{};
    MYFLOAT *_xp[NUMCOMBS + NUMALLPASS]{};
    MYFLOAT *_start[NUMCOMBS + NUMALLPASS]{};
    MYFLOAT *_end[NUMCOMBS + NUMALLPASS]{};
    std::vector<MYFLOAT> buf;
};


template<typename T>
class allpass {
public:

    void setbuffer(T *buf, int32_t size) {
        buffer = buf;
        bufsize = size;
    }

    inline T tick(T x) {
        T z = buffer[bufidx];
        x += feedback * z;
        buffer[bufidx] = x;
        if (++bufidx == bufsize) bufidx = 0;
        return z - feedback * x;
    }

    inline T tickfreeverb(T x) {
        T z = buffer[bufidx];

        buffer[bufidx] = x + (z * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return -x + z;
    }

    void mute() {
        for (int32_t i = 0; i < bufsize; i++)
            buffer[i] = 0;
    }

    void setfeedback(T val) {
        feedback = val;
    }

    T getfeedback() {
        return feedback;
    }

private:
    T feedback{};
    T *buffer{};
    int32_t bufsize{};
    int32_t bufidx{};
};

template<typename T>
class comb {
public:
    comb() {
        filterstore = 0;
        bufidx = 0;
    }

    void setbuffer(T *buf, int32_t size) {
        buffer = buf;
        bufsize = size;
    }

    inline T ticklp(T input) {
        T output = buffer[bufidx];

        filterstore = output * damp2 + filterstore * damp1;

        buffer[bufidx] = input + (filterstore * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return output;
    }

    inline T tick(T input) {
        T output = buffer[bufidx];

        buffer[bufidx] = input + (output * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return output;
    }

    inline T tickdelay(T input) {
        T output = buffer[bufidx];

        buffer[bufidx++] = input;

        if (bufidx >= bufsize) bufidx = 0;

        return output;
    }


    void mute() {
        for (int32_t i = 0; i < bufsize; i++)
            buffer[i] = 0;
    }

    void setdamp(T val) {
        damp1 = val;
        damp2 = 1 - val;
    }

    T getdamp() {
        return damp1;
    }

    void setfeedback(T val) {
        feedback = val;
    }

    T getfeedback() {
        return feedback;
    }

    void undenormalize() {
        if(sizeof(T) == sizeof(MYFLOAT)) {
            UDF(filterstore);
        }
        else{
            UDD(filterstore);}
    }

private:
    T feedback;
    T filterstore;
    T damp1;
    T damp2;
    T *buffer;
    int32_t bufsize;
    int32_t bufidx;
};


const int32_t numcombs = 8;
const int32_t numallpasses = 4;
const MYFLOAT muted = 0;
const MYFLOAT fixedgain = 0.015f;
const MYFLOAT scalewet = 3;
const MYFLOAT scaledry = 2;
const MYFLOAT scaledamp = 0.4f;
const MYFLOAT scaleroom = 0.28f;
const MYFLOAT offsetroom = 0.7f;
const MYFLOAT initialroom = 0.5f;
const MYFLOAT initialdamp = 0.5f;
const MYFLOAT initialwet = 1 / scalewet;
const MYFLOAT initialdry = 0;
const MYFLOAT initialwidth = 1;
const int32_t stereospread = 23;


// These values assume 44.1KHz sample rate
// they will probably be OK for 48KHz sample rate
// but would need scaling for 96KHz (or other) sample rates.
// The values were obtained by listening tests.
const MYFLOAT combtuningL1 = (MYFLOAT) (1116);
const MYFLOAT combtuningR1 = (MYFLOAT) ((1116 + stereospread));
const MYFLOAT combtuningL2 = (MYFLOAT) (1188);
const MYFLOAT combtuningR2 = (MYFLOAT) ((1188 + stereospread));
const MYFLOAT combtuningL3 = (MYFLOAT) (1277);
const MYFLOAT combtuningR3 = (MYFLOAT) ((1277 + stereospread));
const MYFLOAT combtuningL4 = (MYFLOAT) (1356);
const MYFLOAT combtuningR4 = (MYFLOAT) ((1356 + stereospread));
const MYFLOAT combtuningL5 = (MYFLOAT) (1422);
const MYFLOAT combtuningR5 = (MYFLOAT) ((1422 + stereospread));
const MYFLOAT combtuningL6 = (MYFLOAT) (1491);
const MYFLOAT combtuningR6 = (MYFLOAT) ((1491 + stereospread));
const MYFLOAT combtuningL7 = (MYFLOAT) (1557);
const MYFLOAT combtuningR7 = (MYFLOAT) ((1557 + stereospread));
const MYFLOAT combtuningL8 = (MYFLOAT) (1617);
const MYFLOAT combtuningR8 = (MYFLOAT) ((1617 + stereospread));
const MYFLOAT allpasstuningL1 = (MYFLOAT) (556);
const MYFLOAT allpasstuningR1 = (MYFLOAT) ((556 + stereospread));
const MYFLOAT allpasstuningL2 = (MYFLOAT) (441);
const MYFLOAT allpasstuningR2 = (MYFLOAT) ((441 + stereospread));
const MYFLOAT allpasstuningL3 = (MYFLOAT) (341);
const MYFLOAT allpasstuningR3 = (MYFLOAT) ((341 + stereospread));
const MYFLOAT allpasstuningL4 = (MYFLOAT) (225);
const MYFLOAT allpasstuningR4 = (MYFLOAT) ((225 + stereospread));


template<typename T>
class revmodel {
public:
    revmodel(T sr) {
        _sr = sr;
        MYFLOAT scaler = sr / 44100.;
        int32_t total = 0;
        for (int32_t i = 0; i < 24; i++) {
            int32_t delay = (int) floor(scaler * tunings[i]);
            //if ((delay & 1) == 0) delay++;
            //while (!ispprime(delay)) delay += 2;
            tunings[i] = delay;
            total += delay;
        }
        mem.resize(total);
        int32_t offset = 0;
        for (int32_t i = 0; i < 8; i++) {
            combL[i].setbuffer(mem.data() + offset, (int) tunings[i]);
            offset += (int) tunings[i];
        }
        for (int32_t i = 0; i < 8; i++) {
            combR[i].setbuffer(mem.data() + offset, (int) tunings[i + 8]);
            offset += (int) tunings[i + 8];
        }
        for (int32_t i = 0; i < 4; i++) {
            allpassL[i].setbuffer(mem.data() + offset, (int) tunings[i + 16]);
            offset += (int) tunings[i + 16];
        }
        for (int32_t i = 0; i < 4; i++) {
            allpassR[i].setbuffer(mem.data() + offset, (int) tunings[i + 20]);
            offset += (int) tunings[i + 20];
        }

        // Set default values
        allpassL[0].setfeedback(0.5f);
        allpassR[0].setfeedback(0.5f);
        allpassL[1].setfeedback(0.5f);
        allpassR[1].setfeedback(0.5f);
        allpassL[2].setfeedback(0.5f);
        allpassR[2].setfeedback(0.5f);
        allpassL[3].setfeedback(0.5f);
        allpassR[3].setfeedback(0.5f);
        setwet(initialwet);
        setdry(initialdry);
        setdamp(initialdamp);
        setwidth(initialwidth);

        // Buffer will be full of rubbish - so we MUST mute them
        mute();
    }

    void mute() {

        for (int32_t i = 0; i < numcombs; i++) {
            combL[i].mute();
            combR[i].mute();
        }
        for (int32_t i = 0; i < numallpasses; i++) {
            allpassL[i].mute();
            allpassR[i].mute();
        }
    }

    void
    processreplace(MYFLOAT *inputL, MYFLOAT *inputR, MYFLOAT *outputL, MYFLOAT *outputR, long numsamples) {
        while (numsamples-- > 0) {
            T outL = 0, outR = 0;
            T input = (*inputL + *inputR);

            // Accumulate comb filters in parallel
            for (int32_t i = 0; i < numcombs; i++) {
                outL += combL[i].ticklp(input);
                outR += combR[i].ticklp(input);
            }

            // Feed through allpasses in series
            for (int32_t i = 0; i < numallpasses; i++) {
                outL = allpassL[i].tick(outL);
                outR = allpassR[i].tick(outR);
            }

            // Calculate output REPLACING anything already there
            //*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
            //*outputR = outR*wet1 + outL*wet2 + *inputR*dry;
            *outputL = outL * wet + *inputL * dry;
            *outputR = outR * wet + *inputR * dry;
            // Increment sample pointers, allowing for interleave (if any)
            inputL++;
            inputR++;
            outputL++;
            outputR++;
        }

        for (int32_t i = 0; i < numcombs; i++) {
            combL[i].undenormalize();
            combR[i].undenormalize();
        }
    }

    inline void
    tick(MYFLOAT inputL, MYFLOAT inputR, MYFLOAT *outputL, MYFLOAT *outputR) {
            T outL = 0, outR = 0;
            T input = inputL + inputR;

            // Accumulate comb filters in parallel
            for (int32_t i = 0; i < numcombs; i++) {
                outL += combL[i].ticklp(input);
                outR += combR[i].ticklp(input);
            }

            // Feed through allpasses in series
            for (int32_t i = 0; i < numallpasses; i++) {
                outL = allpassL[i].tick(outL);
                outR = allpassR[i].tick(outR);
            }

            // Calculate output REPLACING anything already there
            //*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
            //*outputR = outR*wet1 + outL*wet2 + *inputR*dry;
            *outputL = outL;
            *outputR = outR;
    }

    void undenormalize(){
        for (int32_t i = 0; i < numcombs; i++) {
            combL[i].undenormalize();
            combR[i].undenormalize();
        }
    }

    void setdamp(T value) {
        for (int32_t i = 0; i < numcombs; i++) {
            combL[i].setdamp(value * scaledamp);
            combR[i].setdamp(value * scaledamp);
        }
    }

    void setwet(T value) {
        wet = value * .125 * .5;
        // wet1 = wet * (width / 2 + 0.5);
        // wet2 = wet * ((1 - width) / 2);

    }

    void setdry(T value) {
        dry = value;
    }

    void setwidth(T value) {
        width = value;
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < numcombs; i++) {
                combL[i].setfeedback(0);
                combR[i].setfeedback(0);
            }
        } else
            for (int32_t i = 0; i < numcombs; i++) {
                combL[i].setfeedback(pow(10.0, (-3.0 * tunings[i] / (T60 * _sr))));
                combR[i].setfeedback(pow(10.0, (-3.0 * tunings[i + 8] / (T60 * _sr))));
            }
    }

private:
    T _sr;
    T tunings[24] = {combtuningL1, combtuningL2, combtuningL3, combtuningL4, combtuningL5,
                     combtuningL6, combtuningL7, combtuningL8, combtuningR1, combtuningR2,
                     combtuningR3, combtuningR4, combtuningR5, combtuningR6, combtuningR7,
                     combtuningR8, allpasstuningL1, allpasstuningL2, allpasstuningL3,
                     allpasstuningL4, allpasstuningR1, allpasstuningR2, allpasstuningR3,
                     allpasstuningR4};
    T wet, wet1, wet2;
    T dry;
    T width;
    // The following are all declared inline
    // to remove the need for dynamic allocation
    // with its subsequent error-checking messiness

    // Comb filters
    comb<T> combL[8];
    comb<T> combR[8];

    // Allpass filters
    allpass<T> allpassL[4];
    allpass<T> allpassR[4];
    std::vector<T> mem;
};


class Freeverb : public Effect {
public:
    Freeverb(TRACK *t);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s )override {
        MYFLOAT             gain = dbToLinear60(*_gain), mix;
        if (*_bypass || destroyRequested){
            mix = 0;
        }
        else{
            mix = *_mix;
        }

        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }

        if (_olddamp != *_damp) {
            _olddamp = *_damp;
            reverb.setdamp(_olddamp);
        }

        for(int32_t i=0;i<s;i++){
            MYFLOAT l, r;
            reverb.tick(inl[i], inr[i], &l, &r);
            const MYFLOAT mixsrc = 1. - _smooth1;
            const MYFLOAT mixx = _smooth1 * (.125 * .5);
            inl[i] = (inl[i] * mixsrc + l * mixx) * _smooth2;
            inr[i] = (inr[i] * mixsrc + r * mixx) * _smooth2;
            smmixgain(mix, gain);
        }

        reverb.undenormalize();
    }

    void computeold(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
        if (*_bypass || destroyRequested)
            return;
        update();
        reverb.processreplace(inl, inr, outl, outr, s);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = dbToLinear60(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }
        if (_olddamp != *_damp) {
            _olddamp = *_damp;
            reverb.setdamp(_olddamp);
        }
    }

private:
    std::atomic<MYFLOAT> *_mix, *_gain, *_room, *_damp;
    MYFLOAT _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    revmodel<MYFLOAT> reverb;
};


template<typename T>
class nrev {
public:
    nrev(T sr) {
        _sr = sr;
        T scaler = sr / 25641.0;

        int32_t delaysum = 0;
        for (int32_t i = 0; i < 15; i++) {
            int32_t delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);
            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int32_t sumtmp = 0;
        for (int32_t i = 0; i < 6; i++) {
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            sumtmp += _lengths[i];
        }

        for (int32_t i = 0; i < 8; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 6]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i + 6];
        }
    }

    void process(T *inl, T *inr, T *outl, T *outr, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T in = inl[i] + inr[i];
            T temp0 = 0.0;
            for (int32_t n = 0; n < 6; n++) {
                temp0 += _combs[n].tick(in);
            }

            temp0 /= 12.;

            for (int32_t n = 0; n < 3; n++) {
                temp0 = _aps[n].tick(temp0);
            }

            // One-pole lowpass filter.
            _lowpassState = 0.9 * _lowpassState + 0.1 * temp0;

            temp0 = _aps[3].tick(_lowpassState);

            outl[i] = _wet * _aps[4].tick(temp0) + _dry * inl[i];
            outr[i] = _wet * _aps[5].tick(temp0) + _dry * inr[i];
        }
    }

    inline void tick(T inl, T inr, T *outl, T *outr) {
            T in = inl + inr;
            T temp0 = 0.0;
            for (int32_t n = 0; n < 6; n++) {
                temp0 += _combs[n].tick(in);
            }

            temp0 /= 12.;

            for (int32_t n = 0; n < 3; n++) {
                temp0 = _aps[n].tick(temp0);
            }

            // One-pole lowpass filter.
            _lowpassState = 0.9 * _lowpassState + 0.1 * temp0;

            temp0 = _aps[3].tick(_lowpassState);

            *outl = _aps[4].tick(temp0);
            *outr = _aps[5].tick(temp0);
    }


    void setT60(T T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < 6; i++)
                _combs[i].setfeedback(0);
        } else
            for (int32_t i = 0; i < 6; i++)
                _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i] / (T60 * _sr))));
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }

private:
    int32_t _lengths[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    T _sr;
    allpass<T> _aps[8];
    comb<T> _combs[6];
    T _lowpassState{};
    std::vector<T> _buf;
    T _wet{.5}, _dry{.5};
};


#define NREVB_NUM_COMB    6
#define NREVB_NUM_COMB_2    12
#define NREVB_NUM_ALLPASS 5
#define NREVB_NUM_ALLPASS_2 3
#define NREVB_SCALE_WET    0.4
#define NREV_STEREO_SPREAD 13
#define NREV_DEFAULT_FS 25641.
#define NREVB_DEFAULT_FEEDBACK .7
#define NREVOS 1

template<typename T>
class nrev2 {
public:
    nrev2(T sr) {
        _sr = sr / (T) NREVOS;
        T scaler = (_sr / NREV_DEFAULT_FS);
        const int32_t stereospread = scaler * NREV_STEREO_SPREAD;

        int32_t delaysum = 0;
        for (int32_t i = 0; i < NREVB_NUM_COMB + NREVB_NUM_ALLPASS; i++) {
            int32_t delay = (int) floor(scaler * _lengthsL[i]);
            delay = findNextPrime(delay);
            _lengthsL[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthsR[i] = delay;
            delaysum += delay;
        }

        for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) {
            int32_t delay = (int) floor(scaler * _lengthscomb2L[i]);
            delay = findNextPrime(delay);
            _lengthscomb2L[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthscomb2R[i] = delay;
            delaysum += delay;
        }

        for (int32_t i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            int32_t delay = (int) floor(scaler * _lengthsaps2L[i]);
            delay = findNextPrime(delay);
            _lengthsaps2L[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthsaps2R[i] = delay;
            delaysum += delay;
        }


        _buf.resize(delaysum, 0);
        int32_t sumtmp = 0;

        for (int32_t i = 0; i < NREVB_NUM_COMB; i++) {
            _combsL[i].setbuffer(_buf.data() + sumtmp, _lengthsL[i]);
            sumtmp += _lengthsL[i];
        }

        for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) {
            _combs2L[i].setbuffer(_buf.data() + sumtmp, _lengthscomb2L[i]);
            sumtmp += _lengthscomb2L[i];
        }

        for (int32_t i = 0; i < NREVB_NUM_ALLPASS; i++) {
            _apsL[i].setbuffer(_buf.data() + sumtmp, _lengthsL[i + 6]);
            _apsL[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsL[i + 6];
        }

        for (int32_t i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            _aps2L[i].setbuffer(_buf.data() + sumtmp, _lengthsaps2L[i]);
            _aps2L[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsaps2L[i];
        }

        for (int32_t i = 0; i < NREVB_NUM_COMB; i++) {
            _combsR[i].setbuffer(_buf.data() + sumtmp, _lengthsR[i]);
            sumtmp += _lengthsR[i];
        }

        for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) {
            _combs2R[i].setbuffer(_buf.data() + sumtmp, _lengthscomb2R[i]);
            sumtmp += _lengthscomb2R[i];
        }

        for (int32_t i = 0; i < NREVB_NUM_ALLPASS; i++) {
            _apsR[i].setbuffer(_buf.data() + sumtmp, _lengthsR[i + 6]);
            _apsR[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsR[i + 6];
        }

        for (int32_t i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            _aps2R[i].setbuffer(_buf.data() + sumtmp, _lengthsaps2R[i]);
            _aps2R[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsaps2R[i];
        }
        if (sumtmp != delaysum)
            LOGE("111111 -------------------------------------   FATAAAAAAALLLLLLLLLLLLLLLLLLLLLL");

    }

    /*
    T z = _line[_writepos];
    x += _c * z;
    _line[_writepos] = x;
    if (++_writepos >= _maxdelaysmpls) _writepos = 0;
    return z - _c * x;
     */
    void process(T *inl, T *inr, T *outl, T *outr, int32_t size) {
        T outL, outR, tmpL, tmpR, apfeedback = .5;

        for (int32_t n = 0; n < size; n += NREVOS) {
            outL = outR = tmpL = tmpR =/* _lowpassStatein = .6 * _lowpassStatein + .4 * */(inl[n] +
                                                                                           inr[n]);
            outL += .5 * lastL;
            lastL -= .5 * outL;
            for (int32_t i = 0; i < NREVB_NUM_COMB; i++) outL += _combsL[i].tick(tmpL);
            for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) outL += _combs2L[i].tick(tmpL);
            for (int32_t i = 0; i < 3; i++) outL = _apsL[i].tick(outL);
            for (int32_t i = 0; i < NREVB_NUM_ALLPASS_2; i++) outL = _aps2L[i].tick(outL);
            _lowpassStateL = .7 * _lowpassStateL + .3 * outL;
            outL = _apsL[3].tick(_lowpassStateL);
            outL = _dcL.process(_apsL[4].tick(outL));
            outl[n] *= _dry;
            outl[n] += _wet * lastL;
            //outl[n+1] *= _dry;
            //outl[n+1] += _wet * lastL;

            outR += apfeedback * lastR;
            lastR -= apfeedback * outR;
            for (int32_t i = 0; i < NREVB_NUM_COMB; i++) outR += _combsR[i].tick(tmpR);
            for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) outR += _combs2R[i].tick(tmpR);
            for (int32_t i = 0; i < 3; i++) outR = _apsR[i].tick(outR);
            for (int32_t i = 0; i < NREVB_NUM_ALLPASS_2; i++) outR = _aps2R[i].tick(outR);
            _lowpassStateR = .7 * _lowpassStateR + .3 * outR;
            outR = _apsR[3].tick(_lowpassStateR);
            outR = _dcR.process(_apsR[4].tick(outR));
            outr[n] *= _dry;
            outr[n] += _wet * lastR;
            //outr[n+1] *= _dry;
            //outr[n+1] += _wet * lastR;
            lastL = outL;
            lastR = outR;
        }
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) {
                _combs2L[i].setfeedback(0);
                _combs2R[i].setfeedback(0);
            }
            for (int32_t i = 0; i < NREVB_NUM_COMB; i++) {
                _combsL[i].setfeedback(0);
                _combsR[i].setfeedback(0);
            }
        } else {
            for (int32_t i = 0; i < NREVB_NUM_COMB_2; i++) {
                _combs2L[i].setfeedback(pow(10.0, (-3.0 * _lengthscomb2L[i] / (T60 * _sr))));
                _combs2R[i].setfeedback(pow(10.0, (-3.0 * _lengthscomb2R[i] / (T60 * _sr))));
            }
            for (int32_t i = 0; i < NREVB_NUM_COMB; i++) {
                _combsL[i].setfeedback(pow(10.0, (-3.0 * _lengthsL[i] / (T60 * _sr))));
                _combsR[i].setfeedback(pow(10.0, (-3.0 * _lengthsR[i] / (T60 * _sr))));
            }
        }
    }


    void setwet(T value) {
        _wet = value * (1. / (NREVB_NUM_COMB + NREVB_NUM_COMB_2));
    }

    void setdry(T value) {
        _dry = value;
    }

private:
    int32_t _lengthsL[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    int32_t _lengthsR[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    int32_t _lengthscomb2L[NREVB_NUM_COMB_2]{1257, 1333, 1499, 1501, 1535, 1637, 1738, 1904, 1999, 2020,
                                         2120, 2509};
    int32_t _lengthscomb2R[NREVB_NUM_COMB_2]{1257, 1333, 1499, 1501, 1535, 1637, 1738, 1904, 1999, 2020,
                                         2120, 2509};
    int32_t _lengthsaps2L[NREVB_NUM_ALLPASS_2]{57, 87, 103,};
    int32_t _lengthsaps2R[NREVB_NUM_ALLPASS_2]{57, 87, 103,};
    T _sr;

    allpass<T> _apsL[NREVB_NUM_ALLPASS], _aps2L[NREVB_NUM_ALLPASS_2];
    allpass<T> _apsR[NREVB_NUM_ALLPASS], _aps2R[NREVB_NUM_ALLPASS_2];
    comb<T> _combsL[NREVB_NUM_COMB], _combs2L[NREVB_NUM_COMB_2];
    comb<T> _combsR[NREVB_NUM_COMB], _combs2R[NREVB_NUM_COMB_2];
    DCBlocker<T> _dcL, _dcR;
    T lastL{}, lastR{}, _lowpassStateL{}, _lowpassStateR{}, _lowpassStatein{};
    std::vector<T> _buf;
    T _wet{.5}, _dry{.5};
};

class NRev : public Effect {
public:
    NRev(TRACK *t);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override {
        MYFLOAT             gain = dbToLinear60(*_gain), mix;
        if (*_bypass || destroyRequested){
            mix = 0;
        }
        else{
            mix = *_mix;
        }

        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }

        for(int32_t i=0;i<s;i++){
            MYFLOAT l, r;
            reverb.tick(inl[i], inr[i], &l, &r);
            const MYFLOAT mixsrc = 1.f - _smooth1;
            inl[i] = inl[i] * mixsrc + l * _smooth1 * _smooth2;
            inr[i] = inr[i] * mixsrc + r * _smooth1 * _smooth2;
            smmixgain(mix, gain);
        }
    }

    void computeold(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
        if (*_bypass)
            return;
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }
        reverb.process(inl, inr, outl, outr, s);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = dbToLinear60(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }
    }

private:
    std::atomic<MYFLOAT> *_mix, *_gain, *_room;
    MYFLOAT _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    nrev<MYFLOAT> reverb;
};

template<typename T>
class jcrev {
public:
    jcrev(T sr) {
        _sr = sr;
        MYFLOAT scaler = sr / 44100.0;
        int32_t delaysum = 0;

        for (int32_t i = 0; i < 9; i++) {
            int32_t delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);
            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int32_t sumtmp = 0;


        for (int32_t i = 0; i < 3; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 4]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i + 4];
        }

        for (int32_t i = 0; i < 4; i++) {
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            sumtmp += _lengths[i];
            _combs[i].setdamp(0.2);
        }

        _leftdel.setbuffer(_buf.data() + sumtmp, _lengths[7]);
        sumtmp += _lengths[7];
        _rightdel.setbuffer(_buf.data() + sumtmp, _lengths[8]);
        sumtmp += _lengths[8];
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }


    void setT60(T T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < 4; i++)
                _combs[i].setfeedback(0);
        }
        for (int32_t i = 0; i < 4; i++)
            _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i] / (T60 * _sr))));
    }

    void process(T *inl, T *inr, T *outl, T *outr, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T in = (inl[i] + inr[i]);
            for (int32_t n = 0; n < 3; n++)
                in = _aps[n].tick(in);

            T sum = 0.0;

            for (int32_t n = 0; n < 4; n++)
                sum += _combs[n].ticklp(in);

            sum *= .125;

            outl[i] = _wet * _leftdel.tickdelay(sum) + _dry * inl[i];
            outr[i] = _wet * _rightdel.tickdelay(sum) + _dry * inr[i];
        }

    }

private:
    int32_t _lengths[9]{1116, 1356, 1422, 1617, 225, 341, 441, 211, 179};
    T _sr;
    allpass<T> _aps[3];
    comb<T> _combs[4];
    // OnePole2 _lps[4];
    comb<T> _leftdel;
    comb<T> _rightdel;
    T _wet, _dry;
    std::vector<T> _buf;
};

class JCRev : public Effect {
public:
    JCRev(TRACK *t);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override {
        if (*_bypass)
            return;
        update();
        reverb.process(inl, inr, outl, outr, s);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = dbToLinear60(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(_oldroom * 10);
        }
    }

private:
    std::atomic<MYFLOAT> *_mix, *_gain, *_room;
    MYFLOAT _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    jcrev<MYFLOAT> reverb;
};


template<typename T>
class prcrev {
public:
    prcrev(T sr) {
        _sr = sr;
        MYFLOAT scaler = sr / 44100.0;
        int32_t delaysum = 0;

        for (int32_t i = 0; i < 4; i++) {
            int32_t delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);

            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int32_t sumtmp = 0;


        for (int32_t i = 0; i < 2; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i];
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 2]);
            sumtmp += _lengths[i + 2];
        }
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int32_t i = 0; i < 2; i++)
                _combs[i].setfeedback(0);
        }
        for (int32_t i = 0; i < 2; i++)
            _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i + 2] / (T60 * _sr))));
    }

    void process(T *inl, T *inr, T *outl, T *outr, int32_t size) {
        for (int32_t i = 0; i < size; i++) {
            T in = _aps[1].tick(_aps[0].tick((inl[i] + inr[i]) * .5));
            outl[i] = _wet * _combs[0].tick(in) + _dry * inl[i];
            outr[i] = _wet * _combs[1].tick(in) + _dry * inr[i];
        }

    }

private:
    int32_t _lengths[4]{341, 613, 1557, 2137};
    T _sr;
    allpass<T> _aps[2];
    comb<T> _combs[2];
    T _wet, _dry;
    std::vector<T> _buf;
};


class PRCRev : public Effect {
public:
    PRCRev(TRACK *t);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override {
        if (*_bypass)
            return;
        update();
        reverb.process(inl, inr, outl, outr, s);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = dbToLinear60(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(_oldroom * 10);
        }
    }

private:
    std::atomic<MYFLOAT> *_mix, *_gain, *_room;
    MYFLOAT _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    prcrev<MYFLOAT> reverb;
};
//#include "Reverb8.h"

class REVERB5 : public Effect {
public:
    REVERB5(TRACK *track);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;

private:
    void updateFilters();
    MYFLOAT _tdelay[8]{};
    MYFLOAT _averagedelay{};
    std::atomic<MYFLOAT> *kFeedBack{}, *kLPFreq{}, *_xover, *_t60low, *_t60mid, *_damp, *_predelay;
    MYFLOAT _oldxover{}, _oldt60low{}, _oldt60mid{}, _olddamp{}, _predelayprev{};
    MYFLOAT _dampFact{};
    MYFLOAT prv_LPFreq{};
    MYFLOAT _xt{}, _yt{};
    Filt1<MYFLOAT> _filt[8];
    MYFLOAT _filterstate[8]{};
    MYFLOAT _gainfact{};
    SimpleDelay2<MYFLOAT> _predelayL, _predelayR;
    Ap1ModRndSpline<MYFLOAT> rndLine[8];
 //   StereoHybridReverb str;
};

#endif //GRAINSTORM_REVERB_H
