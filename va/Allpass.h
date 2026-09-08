//
// Created by pr on 01.08.20.
//

#ifndef GRAINSTORM_ALLPASS_H
#define GRAINSTORM_ALLPASS_H

#include <cmath>
#include <cstring>
#include <vector>
#include "tools.h"
#include "random.h"


#define TBLMASK4 4095
#define RNDTABLMAX 4096

template<typename T>
class Ap {
public:
    void init(int size, T c) {
        _size = size;
        _line.resize(size, 0);
        _i = 0;
        _c = c;
    }


    T process(T x) {
        T z = _line[_i];
        x -= _c * z;
        _line[_i] = x;
        if (++_i == _size) _i = 0;
        return z + _c * x;
    }

    T tick(T x) {
        T out = _line[_i] - _c * x;
        _line[_i] = x + out * _c;
        if (++_i == _size) _i = 0;
        return out;
    }

    T lastout() {
        return _lastout;
    }

    T tap(int pos) {
        int offset = _i - pos;
        if (offset < 0)
            offset += _size;
        return _line[offset];
    }

    void reset() {
        _i = 0;
        std::fill(_line.begin(), _line.end(), 0);
    }

private:
    int _i{};
    T _c{}, _lastout{};
    int _size{};
    std::vector<T> _line;
};


template<typename T>
class ApNested {
public:
    void
    init1(T sr, T size1, T c1, T size2, T c2) {
        _line1.resize(sr * size1, 0);
        _off1 = _line1.size();
        _c1 = c1;
        _line2.resize(sr * size2, 0);
        _off2 = _line2.size();
        _c2 = c2;
    }

    void
    init2(T sr, T size1, T c1, T size2, T c2, T size3, T c3) {
        _line1.resize(sr * size1, 0);
        _off1 = _line1.size();
        _c1 = c1;
        _line2.resize(sr * size2, 0);
        _off2 = _line2.size();
        _c2 = c2;
        _line3.resize(sr * size3, 0);
        _off3 = _line3.size();
        _c3 = c3;
    }


    T tick1(T x) {
        T sum = _line2[_i2] - _c2 * _line1[_i1];
        T out = sum - _c1 * x;
        _line2[_i2] = _line1[_i1] + _c2 * sum;
        _line1[_i1] = x + _c1 * out;
        _i1++;
        if (_i1 >= _off1) _i1 = 0;
        _i2++;
        if (_i2 >= _off2) _i2 = 0;
        return out;
    }

    T tick2(T x) {
        T sum1 = _line2[_i2] - _c2 * _line1[_i1];
        T sum2 = _line3[_i3] - _c3 * sum1;
        T out = sum2 - _c1 * x;
        _line3[_i3] = sum1 + _c3 * sum2;
        _line2[_i2] = _line1[_i1] + _c2 * sum1;
        _line1[_i1] = x + _c1 * out;
        _i1++;
        if (_i1 >= _off1) _i1 = 0;
        _i2++;
        if (_i2 >= _off2) _i2 = 0;
        _i3++;
        if (_i3 >= _off3) _i3 = 0;
        return out;
    }


    T tap1(int pos) {
        int offset = _i1 - pos;
        if (offset < 0)
            offset += _off1;
        return _line1[offset];
    }

    T tap2(int pos) {
        int offset = _i2 - pos;
        if (offset < 0)
            offset += _off2;
        return _line2[offset];
    }

    void setT60(T t60, T sr) {
        setDec1(pow(0.001, (_off1) / (t60 * sr)));
        setDec2(pow(0.001, (_off2) / (t60 * sr)));
    }

    void setDec1(T dec) {
        _decay1 = dec;
    }

    void setDec2(T dec) {
        _decay2 = dec;
    }

    void reset() {
        _i1 = _i2 = _i3 = 0;
        std::fill(_line1.begin(), _line1.end(), 0);
        std::fill(_line2.begin(), _line2.end(), 0);
        std::fill(_line3.begin(), _line3.end(), 0);
    }

private:
    std::vector<T> _line1, _line2, _line3;
    int _i1{}, _i2{}, _i3{};
    T _c1{}, _c2{}, _c3, _decay1{1}, _decay2{1}, _decay3{1};
    int _off1{}, _off2{}, _off3{};
};


#define DELAYPOS_SHIFT1  28
#define DELAYPOS_SCALE1  0x10000000
#define DELAYPOS_MASK1   0x0FFFFFFF


template<typename T>
class ApModRnd {
public:
    static int maxdel(int ndel, int drnd) {
        double maxDel = ndel;
        maxDel += drnd * 1.125;
        return (int32_t) (maxDel + 16.5);
    }

    void init(int size1, T c0, int size2, T c1, int size3, T c2,
              T forw, T backw, float sr, float rndlenth1, float rnd1, float rndlenth2,
              float rnd2, float rndlenth3, float rnd3) {
        _sr = sr;
        _rnd[0] = rndlenth1;
        _rnd[1] = rndlenth2;
        _rnd[2] = rndlenth3;

        _rndlinecnt[0] = sr / rnd1;
        _rndlinecnt[1] = sr / rnd2;
        _rndlinecnt[2] = sr / rnd3;

        _delay[0] = size1;
        _line[0].resize(next_pow_2(maxdel(_delay[0], sr * rndlenth1)), 0);
        _mask[0] = _line[0].size() - 1;
        _c0 = c0;

        _delay[1] = size2;
        _line[1].resize(next_pow_2(maxdel(_delay[1], sr * rndlenth2)), 0);
        _mask[1] = _line[1].size() - 1;
        _c1 = c1;

        _delay[2] = size3;
        _line[2].resize(next_pow_2(maxdel(_delay[2], sr * rndlenth3)), 0);
        _mask[2] = _line[2].size() - 1;
        _c2 = c2;

        _forw = forw;
        _backw = backw;
        initdelay(0);
        initdelay(1);
        initdelay(2);
    }

    void setDiff(T diff){
        _c0 = _c1 = _c2 = diff;
    }

    void initdelay(int n) {
        _writepos[n] = 0;
        _seedval[n] = (int) randomfloat(29000, 30000);

        /* set initial delay time */
        double readPos = (double) _seedval[n] / 32768 * _rnd[n];

        readPos = _delay[n] / _sr + (readPos);

        readPos = (double) (_mask[n] + 1) - (readPos * _sr);

        _readpos[n] = (int32_t) readPos;
        readPos = (readPos - (double) _readpos[n]) * (double) DELAYPOS_SCALE1;
        _readposfrac[n] = (int32_t) (readPos + 0.5);
        /* initialise first random line segment */
        next_random_lineseg(n);
    }

    void next_random_lineseg(int n) {
        /* update random seed */
        _rndlinecnttmp[n] = _rndlinecnt[n];

        if (_seedval[n] < 0)
            _seedval[n] += 0x10000;
        _seedval[n] = (_seedval[n] * 15625 + 1) & 0xFFFF;
        if (_seedval[n] >= 0x8000)
            _seedval[n] -= 0x10000;
        /* length of next segment in samples */
        double prvDel = (double) _writepos[n];
        prvDel -= ((double) _readpos[n]
                   + ((double) _readposfrac[n] / (double) DELAYPOS_SCALE1));
        while (prvDel < 0.0)
            prvDel += (double) (_mask[n] + 1);
        prvDel = prvDel / _sr;    /* previous delay time in seconds */
        double nxtDel = (double) _seedval[n] * _rnd[n] / 32768.0;
        /* next delay time in seconds */
        nxtDel = _delay[n] / _sr + nxtDel;

        /* calculate phase increment per sample */
        double phs_incVal = (prvDel - nxtDel) / (double) _rndlinecnt[n];
        phs_incVal = phs_incVal * _sr + 1.0;

        _readposfracinc[n] = (int32_t) (phs_incVal * DELAYPOS_SCALE1 + 0.5);
    }


    T tick3(T x) {
        /* read from delay line with cubic interpolation */
        if (_readposfrac[0] >= DELAYPOS_SCALE1) {
            _readpos[0] += (_readposfrac[0] >> DELAYPOS_SHIFT1);
            _readposfrac[0] &= DELAYPOS_MASK1;
        }


        T frac = (T) _readposfrac[0] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[0] &= _mask[0];

        int32_t readPos = _readpos[0];
        T v0 = _line[0][readPos];
        --readPos;
        readPos &= _mask[0];
        T vm1 = _line[0][readPos];
        readPos += 2;
        readPos &= _mask[0];
        T v1 = _line[0][readPos];
        ++readPos;
        readPos &= _mask[0];
        T v2 = _line[0][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;

        x += _c0 * v0;
        T output = v0 - x * _c0;

        if (_readposfrac[1] >= DELAYPOS_SCALE1) {
            _readpos[1] += (_readposfrac[1] >> DELAYPOS_SHIFT1);
            _readposfrac[1] &= DELAYPOS_MASK1;
        }

        frac = (T) _readposfrac[1] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        am1 = a1 - 1.0;
        a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[1] &= _mask[1];

        readPos = _readpos[1];
        v0 = _line[1][readPos];
        --readPos;
        readPos &= _mask[1];
        vm1 = _line[1][readPos];
        readPos += 2;
        readPos &= _mask[1];
        v1 = _line[1][readPos];
        ++readPos;
        readPos &= _mask[1];
        v2 = _line[1][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;
        x += _c1 * v0;

        _line[0][_writepos[0]] = v0 - x * _c1;


        if (_readposfrac[2] >= DELAYPOS_SCALE1) {
            _readpos[2] += (_readposfrac[2] >> DELAYPOS_SHIFT1);
            _readposfrac[2] &= DELAYPOS_MASK1;
        }

        frac = (T) _readposfrac[2] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        am1 = a1 - 1.0;
        a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[2] &= _mask[2];

        readPos = _readpos[2];
        v0 = _line[2][readPos];
        --readPos;
        readPos &= _mask[2];
        vm1 = _line[2][readPos];
        readPos += 2;
        readPos &= _mask[2];
        v1 = _line[2][readPos];
        ++readPos;
        readPos &= _mask[2];
        v2 = _line[2][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;


        x += _c2 * v0;
        _line[1][_writepos[1]] = v0 - _c2 * x;

       // _line[0][_writepos[0]] = x;//v0 - x * _c0;;

        _filtstate = _line[2][_writepos[2]] = _filtstate * _backw + x * _forw;

        for (int n = 0; n < 3; n++) {
            if (--(_rndlinecnttmp[n]) <= 0)
                next_random_lineseg(n);
        }

        ++_writepos[0] &= _mask[0];
        ++_writepos[1] &= _mask[1];
        ++_writepos[2] &= _mask[2];

        _readposfrac[0] += _readposfracinc[0];
        _readposfrac[1] += _readposfracinc[1];
        _readposfrac[2] += _readposfracinc[2];


        return output;
    }

    T tick3n(T x) {
        x += _c0 * _line[0][_writepos[0]];
        T output = _line[0][_writepos[0]] - _c0 * x;

        x += _c1 * _line[1][_writepos[1]];
        _line[0][_writepos[0]] = _line[1][_writepos[1]] - _c1 * x;

        x += _c2 * _line[2][_writepos[2]];
        _line[1][_writepos[1]] = _line[2][_writepos[2]] - _c2 * x;

        _filtstate = _line[2][_writepos[2]] = _filtstate * _backw + x * _forw;

        _writepos[0]++;
        if (_writepos[0] >= _delay[0]) _writepos[0] = 0;
        _writepos[1]++;
        if (_writepos[1] >= _delay[1]) _writepos[1] = 0;
        _writepos[2]++;
        if (_writepos[2] >= _delay[2]) _writepos[2] = 0;
        return output;
    }

    T tick2(T x) {

        if (_readposfrac[0] >= DELAYPOS_SCALE1) {
            _readpos[0] += (_readposfrac[1] >> DELAYPOS_SHIFT1);
            _readposfrac[0] &= DELAYPOS_MASK1;
        }

        T frac = (T) _readposfrac[0] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[0] &= _mask[0];

        int32_t readPos = _readpos[0];
        T v0 = _line[0][readPos];
        --readPos;
        readPos &= _mask[0];
        T vm1 = _line[0][readPos];
        readPos += 2;
        readPos &= _mask[0];
        T v1 = _line[0][readPos];
        ++readPos;
        readPos &= _mask[0];
        T v2 = _line[0][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;
        x += _c0 * v0;

        T output = v0 - x * _c0;

        if (_readposfrac[1] >= DELAYPOS_SCALE1) {
            _readpos[1] += (_readposfrac[1] >> DELAYPOS_SHIFT1);
            _readposfrac[1] &= DELAYPOS_MASK1;
        }

        frac = (T) _readposfrac[1] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        am1 = a1 - 1.0;
        a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[1] &= _mask[1];

        readPos = _readpos[1];
        v0 = _line[1][readPos];
        --readPos;
        readPos &= _mask[1];
        vm1 = _line[1][readPos];
        readPos += 2;
        readPos &= _mask[1];
        v1 = _line[1][readPos];
        ++readPos;
        readPos &= _mask[1];
        v2 = _line[1][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;

        x += _c1 * v0;

        _line[0][_writepos[0]] = v0 - x * _c1;

        _line[1][_writepos[1]] = x;

        for (int n = 0; n < 2; n++) {
            if (--(_rndlinecnttmp[n]) <= 0)
                next_random_lineseg(n);
        }

        ++_writepos[0] &= _mask[0];
        ++_writepos[1] &= _mask[1];

        _readposfrac[0] += _readposfracinc[0];
        _readposfrac[1] += _readposfracinc[1];
        return output;
    }

    T tick2n(T x) {
        x += _c0 * _line[0][_writepos[0]];
        T output = _line[0][_writepos[0]] - x * _c0;

        x += _c1 * _line[1][_writepos[1]];
        _line[0][_writepos[0]] = _line[1][_writepos[1]] - x * _c1;

        _line[1][_writepos[1]] = x;

        _writepos[0]++;
        if (_writepos[0] >= _delay[0]) _writepos[0] = 0;
        _writepos[1]++;
        if (_writepos[1] >= _delay[1]) _writepos[1] = 0;
        return output;
    }


    T tick1(T x) {
        if (_readposfrac[0] >= DELAYPOS_SCALE1) {
            _readpos[0] += (_readposfrac[0] >> DELAYPOS_SHIFT1);
            _readposfrac[0] &= DELAYPOS_MASK1;
        }
        T frac = (T) _readposfrac[0] * (1.0 / (double) DELAYPOS_SCALE1);
        /* calculate interpolation coefficients */
        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;

        _readpos[0] &= _mask[0];

        int32_t readPos = _readpos[0];
        T v0 = _line[0][readPos];
        --readPos;
        readPos &= _mask[0];
        T vm1 = _line[0][readPos];
        readPos += 2;
        readPos &= _mask[0];
        T v1 = _line[0][readPos];
        ++readPos;
        readPos &= _mask[0];
        T v2 = _line[0][readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;

        x += _c0 * v0;

        _line[0][_writepos[0]] = x;

        for (int n = 0; n < 1; n++) {
            if (--(_rndlinecnttmp[n]) <= 0)
                next_random_lineseg(n);
        }

        ++_writepos[0] &= _mask[0];

        _readposfrac[0] += _readposfracinc[0];
        return v0 - x * _c0;
    }

    T tick1lin(T x) {
        if (_readposfrac[0] >= DELAYPOS_SCALE1) {
            _readpos[0] += (_readposfrac[0] >> DELAYPOS_SHIFT1);
            _readposfrac[0] &= DELAYPOS_MASK1;
        }

        T frac = (T) _readposfrac[0] * (1.0 / (double) DELAYPOS_SCALE1);
        _readpos[0] &= _mask[0];
        T v0 =
                _line[0][_readpos[0]] +
                frac *
                (_line[0][((_readpos[0] + 1) & _mask[0])] - _line[0][_readpos[0]]);


        x += _c0 * v0;

        _line[0][_writepos[0]] = x;

        for (int n = 0; n < 1; n++) {
            if (--(_rndlinecnttmp[n]) <= 0)
                next_random_lineseg(n);
        }

        ++_writepos[0] &= _mask[0];

        _readposfrac[0] += _readposfracinc[0];
        return v0 - x * _c0;
    }


    T tick1n(T x) {
        T z = _line[0][_writepos[0]];
        x -= _c0 * z;
        _line[0][_writepos[0]] = x;
        if (++_writepos[0] >= _delay[0]) _writepos[0] = 0;
        return z + _c0 * x;
    }

    void reset() {
        std::fill(_line[0].begin(), _line[0].end(), 0);
        std::fill(_line[1].begin(), _line[1].end(), 0);
        std::fill(_line[2].begin(), _line[2].end(), 0);
    }

    int getTotalDelay(){
        return _delay[0] + _delay[1] + _delay[2];
    }

private:
    double _sr;
    T _c0{}, _c1{}, _c2{}, _forw{}, _backw{}, _filtstate{};
    std::vector<T> _line[3];
    float _rnd[3]{};
    int32_t _mask[3], _readposfrac[3]{}, _readposfracinc[3]{}, _readpos[3]{}, _writepos[3]{}, _seedval[3]{}, _rndlinecnt[3]{}, _rndlinecnttmp[3]{};
    int _delay[3]{};
};


template<typename T>
class Ap1 {
public:
    static int maxdel(int ndel, int drnd) {
        double maxDel = ndel;
        maxDel += drnd * 1.125;
        return (int32_t) (maxDel + 16.5);
    }

    void init(T sr, T lengthseconds, T rndlength) {
        _sr = sr;
        _rnd = rndlength;
        _maxdelaysmpls = sr * lengthseconds;
        _line.resize(next_pow_2(maxdel(_maxdelaysmpls, sr * rndlength)), 0);
        _off = _line.size();
        _offandmask = _off - 1;
        initdelay();

        /*
        for (int i = 0; i < INTERPOLATION_SAMPLES; i++) {
         for (int ii = 0; ii < INTERPOLATION_SUBSAMPLES; ii++) {
                double i_shifted = ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
                                    + (double) ii / (double) INTERPOLATION_SUBSAMPLES);
                if (fabs(i_shifted) < 0.000001) {
                    _sinc_table[i][ii] = (T) 1.;

                } else {
                    _sinc_table[i][ii] = (T) sin(i_shifted * PI_P) / (PI_P * i_shifted);
                    _sinc_table[i][ii] *= (T) 0.5 * (1.0 + cos(2.0 * PI_P * i_shifted /
                                                               (T) INTERPOLATION_SAMPLES));
                };
            };
        };
    */
    }

    void initdelay() {
        _seedval = (int) randomfloat(29000, 30000);
        /* set initial delay time */
        double readPos = (double) _seedval / 32768 * _rnd;

        readPos = _maxdelaysmpls * _scale / _sr + (readPos);

        readPos = (double) _off - (readPos * _sr);

        _readpos = (int32_t) readPos;
        readPos = (readPos - (double) _readpos) * (double) DELAYPOS_SCALE1;
        _readposfrac = (int32_t) (readPos + 0.5);
        /* initialise first random line segment */
        next_random_lineseg();
    }

    void next_random_lineseg() {
        _rndlinecnt = _sr / _rndrate ;
        /* update random seed */

        if (_seedval < 0)
            _seedval += 0x10000;
        _seedval = (_seedval * 15625 + 1) & 0xFFFF;
        if (_seedval >= 0x8000)
            _seedval -= 0x10000;
        /* length of next segment in samples */
        double prvDel = (double) _writepos;
        prvDel -= ((double) _readpos
                   + ((double) _readposfrac / (double) DELAYPOS_SCALE1));
        while (prvDel < 0.0)
            prvDel += (double) _off;
        prvDel = prvDel / _sr;    /* previous delay time in seconds */
        double nxtDel = (double) _seedval * _rnd / 32768.0 * _rnddepth;
        /* next delay time in seconds */
        nxtDel = (_maxdelaysmpls * _scale) / _sr + nxtDel;

        /* calculate phase increment per sample */
        double phs_incVal = (prvDel - nxtDel) / (double) _rndlinecnt;
        phs_incVal = phs_incVal * _sr + 1.0;
        _readposfracinc = (int32_t) (phs_incVal * DELAYPOS_SCALE1 + 0.5);
    }


    T tickok(T x) {
        if (_readposfrac >= DELAYPOS_SCALE1) {
            _readpos += (_readposfrac >> DELAYPOS_SHIFT1);
            _readposfrac &= DELAYPOS_MASK1;
        }


        T frac = (T) _readposfrac * (1.0 / (T) DELAYPOS_SCALE1);

        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;


        _readpos &= _offandmask;

        int32_t readPos = _readpos;
        T v0 = _line[readPos];
        --readPos;
        readPos &= _offandmask;
        T vm1 = _line[readPos];
        readPos += 2;
        readPos &= _offandmask;
        T v1 = _line[readPos];
        ++readPos;
        readPos &= _offandmask;
        T v2 = _line[readPos];

        v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;


        if (DISTANCE(_c, _nextc >= 0.00011)) {
            if (_c < _nextc)
                _c += 0.00005;
            else _c -= 0.00005;
        } else
            _c = _nextc;

        x += _c * v0;

        _line[_writepos] = x;

        if (--_rndlinecnt <= 0)
            next_random_lineseg();


        _writepos++;
        _writepos &= _offandmask;
        _readposfrac += _readposfracinc;

        return v0 - x * _c;
    }


    T ticklin(T x) {
        if (_readposfrac >= DELAYPOS_SCALE1) {
            _readpos += (_readposfrac >> DELAYPOS_SHIFT1);
            _readposfrac &= DELAYPOS_MASK1;
            _readpos &= _offandmask;
        }

        T frac = _readposfrac * (1.0 / (T) DELAYPOS_SCALE1);
        T v0 =
                _line[_readpos] +
                frac *
                (_line[((_readpos + 1) & _offandmask)] - _line[_readpos]);


        if (DISTANCE(_c, _nextc >= 0.00011)) {
            if (_c < _nextc)
                _c += 0.00005;
            else _c -= 0.00005;
        } else
            _c = _nextc;

        x += _c * v0;

        _line[_writepos] = x;

        if (--_rndlinecnt <= 0)
            next_random_lineseg();


        _writepos++;
        _writepos &= _offandmask;
        _readposfrac += _readposfracinc;

        return v0 - x * _c;
    }


    T tickdelay(T x) {
        if (_readposfrac >= DELAYPOS_SCALE1) {
            _readpos += (_readposfrac >> DELAYPOS_SHIFT1);
            _readposfrac &= DELAYPOS_MASK1;
        }


        T frac = (T) _readposfrac * (1.0 / (T) DELAYPOS_SCALE1);

        T a2 = frac * frac;
        a2 -= 1.0;
        a2 *= (1.0 / 6.0);
        T a1 = frac;
        a1 += 1.0;
        a1 *= 0.5;
        T am1 = a1 - 1.0;
        T a0 = 3.0 * a2;
        a1 -= a0;
        am1 -= a2;
        a0 -= frac;


        _readpos &= _offandmask;

        int32_t readPos = _readpos;
        T v0 = _line[readPos];
        --readPos;
        readPos &= _offandmask;
        T vm1 = _line[readPos];
        readPos += 2;
        readPos &= _offandmask;
        T v1 = _line[readPos];
        ++readPos;
        readPos &= _offandmask;
        T v2 = _line[readPos];

        _line[_writepos] = x;

        if (_rnddepth > 0 && --_rndlinecnt <= 0)
            next_random_lineseg();


        _writepos++;
        _writepos &= _offandmask;
        _readposfrac += _readposfracinc;

        return (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;
    }

    T tick1n(T x) {
        T z = _line[_writepos];
        x += _c * z;
        _line[_writepos] = x;
        if (++_writepos >= _maxdelaysmpls) _writepos = 0;
        return z - _c * x;
    }


    T tap(int pos) {
        return _line[(_writepos - pos) & _offandmask];
    }

    void reset() {
        std::fill(_line.begin(), _line.end(), 0);
        _writepos = 0;
    }

    void setScale(float scale) {
        reset();
        _scale = scale;
        initdelay();
    }

    void setDiff(T diff) {
        _nextc = diff;
    }

    void setRndDepth(float depth) {
        _rnddepth =  depth;
        _rndlinecnt = 0;
    }

    void setRndRate(float rate) {
        _rndrate = rate;
        _rndlinecnt = 0;
    }

    /*
    T tickdelaysinc(T x) {
        if (_readposfrac >= DELAYPOS_SCALE1) {
            _readpos += (_readposfrac >> DELAYPOS_SHIFT1);
            _readposfrac &= DELAYPOS_MASK1;
            _readpos &= _offandmask;
        }

        int32_t frac =
                _readposfrac * (1.0 / (T) DELAYPOS_SCALE1 * INTERPOLATION_SUBSAMPLES_ANDMASK);


        T v0 = 0.0;

        int32_t readpos = _readpos;

        for (int i = 0; i < INTERPOLATION_SAMPLES; i++) {
            v0 += _line[readpos & _offandmask]
                  * _sinc_table[i][frac];
            readpos--;
        }


        _line[_writepos] = x;

        if (_rnddepth > 0 && --_rndlinecnt <= 0)
            next_random_lineseg();


        _writepos++;
        _writepos &= _offandmask;
        _readposfrac += _readposfracinc;

        return v0;
    }

         T ticksinc(T x) {
        if (_readposfrac >= DELAYPOS_SCALE1) {
            _readpos += (_readposfrac >> DELAYPOS_SHIFT1);
            _readposfrac &= DELAYPOS_MASK1;
            _readpos &= _offandmask;
        }

        int32_t frac =
                _readposfrac * (1.0 / (T) DELAYPOS_SCALE1 * INTERPOLATION_SUBSAMPLES_ANDMASK);


        T v0 = 0.0;

        int32_t readpos = _readpos;

        for (int i = 0; i < INTERPOLATION_SAMPLES; i++) {
            v0 += _line[readpos & _offandmask]
                  * _sinc_table[i][frac];
            readpos--;
        }


        if (DISTANCE(_c, _nextc >= 0.00011)) {
            if (_c < _nextc)
                _c += 0.00005;
            else _c -= 0.00005;
        } else
            _c = _nextc;

        x += _c * v0;

        _line[_writepos] = x;

        if (_rnddepth > 0 && --_rndlinecnt <= 0)
            next_random_lineseg();


        _writepos++;
        _writepos &= _offandmask;
        _readposfrac += _readposfracinc;

        return v0 - x * _c;
    }

*/

private:
    T _sr;
    T _c{}, _nextc{};
    std::vector<T> _line;
    float _rnd{}, _scale{1}, _rnddepth{1.f}, _rndrate{3.f};
    int32_t _maxdelaysmpls, _off, _offandmask, _readposfrac{}, _readposfracinc{}, _readpos{}, _writepos{}, _seedval{}, _rndlinecnt{};
    //T _sinc_table[INTERPOLATION_SAMPLES][INTERPOLATION_SUBSAMPLES];
};

#endif //GRAINSTORM_ALLPASS_H
