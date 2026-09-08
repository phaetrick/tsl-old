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


#include <cstring>
#include <cmath>
#include <cassert>
#include <defines.h>
#include "dplimit1.h"


void Histmin::init(int hlen) {
    int i;

    assert (hlen <= SIZE);
    _hlen = hlen;
    _hold = hlen;
    _wind = 0;
    _vmin = 1;
    for (i = 0; i < SIZE; i++) _hist[i] = _vmin;
}


MYFLOAT Histmin::write(MYFLOAT v) {
    int i, j;

    i = _wind;
    _hist[i] = v;
    if (v <= _vmin) {
        _vmin = v;
        _hold = _hlen;
    } else if (--_hold == 0) {
        _vmin = v;
        _hold = _hlen;
        for (j = 1 - _hlen; j < 0; j++) {
            v = _hist[(i + j) & MASK];
            if (v < _vmin) {
                _vmin = v;
                _hold = _hlen + j;
            }
        }
    }
    _wind = ++i & MASK;
    return _vmin;
}


Dplimit1::Dplimit1(MYFLOAT sr) {
    _oldrelease = _rel = 50;
    _oldtresh = _tresh = -1;
    init(sr, 2, _oldtresh, _oldrelease);
}


Dplimit1::~Dplimit1(void) {
    fini();
}


void Dplimit1::set_threshd(MYFLOAT v) {
    _gt =  pow(10.0, -0.05 * v);
}


void Dplimit1::set_reltime(MYFLOAT v) {
    _w3 = 1.0f / (v * _fsamp);
}


void Dplimit1::init(MYFLOAT fsamp, int nchan, MYFLOAT threshd, MYFLOAT reltime) {
    int i;

    fini();
    if (nchan > 2) nchan = 2;
    _fsamp = fsamp;
    _nchan = nchan;
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
    for (i = 0; i < _nchan; i++) {
        _dbuff[i] = new MYFLOAT[_dsize];
        memset(_dbuff[i], 0, _dsize * sizeof(MYFLOAT));
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
    for (i = 0; i < _nchan; i++) _zlf[i] = 0.0;
    _z1 = 1.0;
    _z2 = 1.0;
    _z3 = 1.0;
    _gmax = 1.0;
    _gmin = 1.0;
}


void Dplimit1::fini(void) {
    int i;

    for (i = 0; i < 2; i++) {
        delete[] _dbuff[i];
        _dbuff[i] = 0;
    }
    _nchan = 0;
}

void Dplimit1::compute(MYFLOAT *inl, MYFLOAT *inr, int size) {
    MYFLOAT rel = _rel;
    if((rel != _oldrelease)){
        _oldrelease = rel;
        set_reltime(LOG2NORMAL2F(rel));
    }
    MYFLOAT tresh = _tresh;
    if(tresh != _oldtresh){
        _oldtresh = tresh;
        set_threshd(tresh);
    }
    MYFLOAT *dp[2] = {inl, inr};
    int i, j, k, n, c1, c2, ri, wi;
    MYFLOAT g1, g2, m1, m2, x, z, z1, z2, z3, pk, t0, t1, *p;

    ri = _delri;
    wi = (ri + _delay) & _dmask;
    g1 = _hist1.vmin();
    c1 = _c1;
    m1 = _m1;
    g2 = _hist2.vmin();
    c2 = _c2;
    m2 = _m2;
    z1 = _z1;
    z2 = _z2;
    z3 = _z3;
    n = _nchan;

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

    int nsamp = size;
    while (nsamp) {
        k = (c1 < nsamp) ? c1 : nsamp;

        for (j = 0; j < n; j++) {
            p = dp[j];
            z = _zlf[j];
            for (i = 0; i < k; i++) {
                x = *p++;
                z += _wlf * (x - z) + 1e-20;
                _dbuff[j][wi + i] = x;
                x = fabs(x);
                if (x > m1) m1 = x;
                x = fabs(z);
                if (x > m2) m2 = x;
            }
            _zlf[j] = z;
        }

        c1 -= k;
        if (c1 == 0) {
            m1 *= _gt;
            if (m1 > pk) pk = m1;
            g1 = (m1 > 1.0) ? 1.0 / m1 : 1.0;
            g1 = _hist1.write(g1);
            c1 = _div1;
            m1 = 0;
            if (--c2 == 0) {
                m2 *= _gt;
                g2 = (m2 > 1.0) ? 1.0 / m2 : 1.0;
                g2 = _hist2.write(g2);
                c2 = _div2;
                m2 = 0;
            }
        }

        for (i = 0; i < k; i++) {
            z1 += _w1 * (g1 - z1);
            z2 += _w2 * (g2 - z2);
            z = (z2 < z1) ? z2 : z1;
            if (z < z3) z3 += _w1 * (z - z3);
            else z3 += _w3 * (z - z3);
            if (z3 > t1) t1 = z3;
            if (z3 < t0) t0 = z3;
            for (j = 0; j < n; j++) {
                dp[j][i] = z3 * _dbuff[j][ri + i];
            }
        }

        for (j = 0; j < n; j++) dp[j] += k;
        wi = (wi + k) & _dmask;
        ri = (ri + k) & _dmask;
        nsamp -= k;
    }

    _delri = ri;
    _c1 = c1;
    _m1 = m1;
    _c2 = c2;
    _m2 = m2;
    _z1 = z1;
    _z2 = z2;
    _z3 = z3;
    _peak = pk;
    _gmin = t0;
    _gmax = t1;
}
