#pragma once
// ----------------------------------------------------------------------------
//
//  Copyright (C) 2008-2017 Fons Adriaensen <fons@linuxaudio.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------


#ifndef __DPLIMIT1_H
#define __DPLIMIT1_H


#include <stdint.h>
#include <defines.h>
#include "base.h"

template<typename T>
class Histmin {
public:

    Histmin(void) {}

    ~Histmin(void) {}

    void init(int32_t hlen) {
        int32_t i;

        assert (hlen <= SIZE);
        _hlen = hlen;
        _hold = hlen;
        _wind = 0;
        _vmin = 1;
        for (i = 0; i < SIZE; i++) _hist[i] = _vmin;
    }


    T write(T v) {
        _hist[_wind] = v;
        if (v <= _vmin) {
            _vmin = v;
            _hold = _hlen;
        } else if (--_hold == 0) {
            _vmin = v;
            _hold = _hlen;
            for (int32_t j = 1 - _hlen; j < 0; j++) {
                v = _hist[(_wind + j) & MASK];
                if (v < _vmin) {
                    _vmin = v;
                    _hold = _hlen + j;
                }
            }
        }
        (++_wind) &= MASK;
        return _vmin;
    }

    T vmin(void) { return _vmin; }

private:

    enum {
        SIZE = 32, MASK = SIZE - 1
    };

    int32_t _hlen;
    int32_t _hold;
    int32_t _wind;
    T _vmin;
    T _hist[SIZE];
};

template<typename T>
class Limiter {
public:
    Limiter()= default;

    void set_threshd(T v) {
        _gt = pow(10., -0.05 * v);
    }


    void set_reltime(T v) {
        _w3 = 1.0 / (v * _fsamp * 0.001);
    }

    void init(T fsamp,T threshd, T reltime) {
        _fsamp = fsamp;
        if (fsamp > 130000) _div1 = 32;
        else if (fsamp > 65000) _div1 = 16;
        else _div1 = 8;
        _div2 = 8;
        _len1 = (int) (ceil(1.2e-3 * _fsamp / _div1));
        _len2 = 12;
        _delay = _len1 * _div1;
        for (_dsize = 64; _dsize < _delay + _div1; _dsize *= 2);
        _dmask = _dsize - 1;
        _delri = 0;
        for (auto &db : _dbuff) {
            db.resize(_dsize, 0);
        }
        set_threshd(threshd);
        set_reltime(reltime);
        _hist1.init(_len1 + 1);
        _hist2.init(_len2);
        _c1 = _div1;
        _c2 = _div2;
        _m1 = 0.0;
        _m2 = 0.0;
        _wlf = 6.28 * 500.0 / fsamp;
        _w1 = 10.0 / _delay;
        _w2 = _w1 / _div2;
        for (int32_t i = 0; i < 2; i++) _zlf[i] = 0.0;
        _z1 = 1.0;
        _z2 = 1.0;
        _z3 = 1.0;
        _gmax = 1.0;
        _gmin = 1.0;
    }
    void reset(){
            for (auto &i :_zlf)i = 0.0;
            for (auto &i : _dbuff)
                for(auto &z:i)z=0.0;
    };

    inline T tick(T in) {
        int32_t ri = _delri;
        int32_t wi = (ri + _delay) & _dmask;
        auto g1 = _hist1.vmin();
        auto g2 = _hist2.vmin();

        T pk, t0, t1;
        if (_rstat) {
            _rstat = false;
            pk = 0;
            t0 = _gmax;
            t1 = _gmin;
        } else {
            pk = _peak;
            t0 = _gmin;
            t1 = _gmax;
        }
        auto z = _zlf[0];
        UDD(in)
        z += _wlf * (in - z) + 1e-20;
        _dbuff[0][wi] = in;
        in = fabs(in);
        if (in > _m1) _m1 = in;
        in = fabs(z);
        if (in > _m2) _m2 = in;

        UDD(z)
        _zlf[0] = z;


        if (--_c1 == 0) {
            _m1 *= _gt;
            if (_m1 > pk) pk = _m1;
            g1 = (_m1 > 1.0) ? 1.0 / _m1 : 1.0;
            g1 = _hist1.write(g1);
            _c1 = _div1;
            _m1 = 0;
            if (--_c2 == 0) {
                _m2 *= _gt;
                g2 = (_m2 > 1.0) ? 1.0 / _m2 : 1.0;
                g2 = _hist2.write(g2);
                _c2 = _div2;
                _m2 = 0;
            }
        }

        _z1 += _w1 * (g1 - _z1);
        _z2 += _w2 * (g2 - _z2);
        auto z1 = (_z2 < _z1) ? _z2 : _z1;
        if (z1 < _z3) _z3 += _w1 * (z1 - _z3);
        else _z3 += _w3 * (z1 - _z3);
        if (_z3 > t1) t1 = _z3;
        if (_z3 < t0) t0 = _z3;
        auto out = _z3 * _dbuff[0][ri];

        (++ri) &= _dmask;

        _delri = ri;
        _peak = pk;
        _gmin = t0;
        _gmax = t1;
        return out;
    }


    inline void tick(T &inl, T &inr) {
        int32_t ri = _delri;
        int32_t wi = (ri + _delay) & _dmask;
        auto g1 = _hist1.vmin();
        auto g2 = _hist2.vmin();

        T pk, t0, t1;
        if (_rstat) {
            _rstat = false;
            pk = 0;
            t0 = _gmax;
            t1 = _gmin;
        } else {
            pk = _peak;
            t0 = _gmin;
            t1 = _gmax;
        }
        auto z = _zlf[0];
        auto x = inl;
        UDD(x)
        z += _wlf * (x - z) + 1e-20;
        _dbuff[0][wi] = x;
        x = std::abs(x);
        if (x > _m1) _m1 = x;
        x = std::abs(z);
        if (x > _m2) _m2 = x;

        UDD(z)
        _zlf[0] = z;

        z = _zlf[1];
        x = inr;
        UDD(x)
        z += _wlf * (x - z) + 1e-20;
        _dbuff[1][wi] = x;
        x = std::abs(x);
        if (x > _m1) _m1 = x;
        x = std::abs(z);
        if (x > _m2) _m2 = x;

        UDD(z)
        _zlf[1] = z;


        if (--_c1 == 0) {
            _m1 *= _gt;
            if (_m1 > pk) pk = _m1;
            g1 = (_m1 > 1.0) ? 1.0 / _m1 : 1.0;
            g1 = _hist1.write(g1);
            _c1 = _div1;
            _m1 = 0;
            if (--_c2 == 0) {
                _m2 *= _gt;
                g2 = (_m2 > 1.0) ? 1.0 / _m2 : 1.0;
                g2 = _hist2.write(g2);
                _c2 = _div2;
                _m2 = 0;
            }
        }

        _z1 += _w1 * (g1 - _z1);
        _z2 += _w2 * (g2 - _z2);
        auto z1 = (_z2 < _z1) ? _z2 : _z1;
        if (z1 < _z3) _z3 += _w1 * (z1 - _z3);
        else _z3 += _w3 * (z1 - _z3);
        if (_z3 > t1) t1 = _z3;
        if (_z3 < t0) t0 = _z3;

        inl = _z3 * _dbuff[0][ri];
        inr = _z3 * _dbuff[1][ri];

        (++ri) &= _dmask;

        _delri = ri;
        _peak = pk;
        _gmin = t0;
        _gmax = t1;
    }


private:
    T _fsamp{};
    int32_t _div1{};
    int32_t _div2{};
    int32_t _len1{};
    int32_t _len2{};
    int32_t _delay{};
    int32_t _dsize{};
    int32_t _dmask{};
    int32_t _delri{};
    tsl::AlignedVector<T> _dbuff[2]{};
    int32_t _c1{};
    int32_t _c2{};
    T _gt{};
    T _m1{};
    T _m2{};
    T _wlf{};
    T _w1{};
    T _w2{};
    T _w3{};
    T _z1{};
    T _z2{};
    T _z3{};
    T _zlf[2]{};
    bool _rstat{};
    T _peak{};
    T _gmax{1};
    T _gmin{1};
    Histmin<T> _hist1;
    Histmin<T> _hist2;
};




#include "track.h"

template<typename T>
class Dplimit1 : public Effect, private Limiter<T> {
public:
    Dplimit1(): Effect(_DATA->tracks[0], SPACE_LIMITER, STEREOEFFECT) {};

    Dplimit1(TRACK *track) : Effect(track, SPACE_LIMITER, STEREOEFFECT) {
        _oldrelease = *(_rel = &_STATE->params[track->index][LIMREL]);
        _oldtresh = *(_tresh = &_STATE->params[track->index][LIMTHRES]);
        _bypass = &track->bypass[SPACE_LIMITER];
        Limiter<T>::init(_STATE->sr, _oldtresh, LOG2NORMAL(_oldrelease));
    }


    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override {
        const MYFLOAT mix = (*_bypass || destroyRequested) ? 0.f : 1.f;
        T rel = *_rel;
        if ((rel != _oldrelease)) {
            _oldrelease = rel;
            Limiter<T>::set_reltime(LOG2NORMAL(rel));
        }
        T tresh = *_tresh;
        if (tresh != _oldtresh) {
            _oldtresh = tresh;
            Limiter<T>::set_threshd(tresh);
        }

        for(int32_t i=0;i<s;i++){
            const MYFLOAT mm = _smooth1 >= 0.999 ? 1. : _smooth1;
            const MYFLOAT mixsrc = 1. - mm;
            auto l = inl[i], r = inr[i];
            Limiter<T>::tick(l,r);
            outl[i] = outl[i] * mixsrc + l * mm;
            outr[i] = outr[i] * mixsrc + r * mm;
            sm1(mix);

        }
    }

private:
    T _oldtresh{}, _oldrelease{};
    std::atomic<MYFLOAT> *_tresh, *_rel;
};

class Clipper : public Effect {
public:
    Clipper(TRACK *t) : Effect(t, SPACE_CLIPPER, STEREOEFFECT) {
        _bypass = &t->bypass[SPACE_CLIPPER];
    }

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override {
        const auto thrs = LOG2NORMALF(_STATE->params[_track->index][CLIPPERTHRS].load());
        const auto arg = thrs * (0.99 - .95 * _STATE->params[_track->index][CLIPPERSTART].load());
        auto k1 = FL(1.0) / (thrs - arg);
        k1 = k1 * k1;
        const auto k2 = (thrs + arg) * FL(0.5);
        const MYFLOAT mix = *_bypass || destroyRequested ? 0.0 : 1.0;
        for (int32_t i = 0; i < s; i++) {
            MYFLOAT x = inl[i];
            const MYFLOAT mm = _smooth1 >= 0.999 ? 1. : _smooth1;
            const MYFLOAT mixsrc = 1.f - mm;
            if (x >= FL(0.0)) {
                if (UNLIKELY(x > thrs)) x = k2;
                else if (x > arg)
                    x = arg + (x - arg) / (FL(1.0) + (x - arg) * (x - arg) * k1);
            } else {
                if (UNLIKELY(x < -thrs))
                    x = -k2;
                else if (-x > arg)
                    x = -arg + (x + arg) / (FL(1.0) + (x + arg) * (x + arg) * k1);
            }

            outl[i] = x * mm + inl[i] * mixsrc;
            x = inr[i];
            if (x >= FL(0.0)) {
                if (UNLIKELY(x > thrs)) x = k2;
                else if (x > arg)
                    x = arg + (x - arg) / (FL(1.0) + (x - arg) * (x - arg) * k1);
            } else {
                if (UNLIKELY(x < -thrs))
                    x = -k2;
                else if (-x > arg)
                    x = -arg + (x + arg) / (FL(1.0) + (x + arg) * (x + arg) * k1);
            }
            outr[i] = x * mm + inr[i] * mixsrc;
            sm1(mix);
        }

    }
};

#endif