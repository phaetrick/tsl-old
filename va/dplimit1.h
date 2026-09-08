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




class Histmin
{
public:

    Histmin (void) {}
    ~Histmin (void) {}

    void  init (int hlen);
    MYFLOAT write (MYFLOAT v);
    MYFLOAT vmin (void) { return _vmin; }

private:

    enum { SIZE = 32, MASK = SIZE - 1 };

    int    _hlen;
    int    _hold;
    int    _wind;
    MYFLOAT  _vmin;
    MYFLOAT  _hist [SIZE];
};


class Dplimit1
{
public:
	Dplimit1(MYFLOAT sr);
	~Dplimit1();
    void init (MYFLOAT fsamp, int nchan, MYFLOAT threshd, MYFLOAT reltime);
    void fini (void);

    void set_threshd (MYFLOAT v);
    void set_reltime (MYFLOAT v);

    void get_stats (MYFLOAT *peak, MYFLOAT *gmax, MYFLOAT *gmin)
    {
	*peak = _peak;
	*gmax = _gmax;
	*gmin = _gmin;
	_rstat = true;
    }

    void prepare (int nsamp);

    void compute(MYFLOAT *inl, MYFLOAT *inr, int size);

private:

    void process1 (int nsamp, MYFLOAT *data[]);

    int               _state{};
    MYFLOAT             _fsamp{};
    int               _nchan{2};
    int               _div1{};
    int               _div2{};
    int               _len1{};
    int               _len2{};
    int               _delay{};
    int               _dsize{};
    int               _dmask{};
    int               _delri{};
    MYFLOAT            *_dbuff [2]{};
    int               _c1{};
    int               _c2{};
    MYFLOAT             _gt{};
    MYFLOAT             _m1{};
    MYFLOAT             _m2{};
    MYFLOAT             _wlf{};
    MYFLOAT             _w1{};
    MYFLOAT             _w2{};
    MYFLOAT             _w3{};
    MYFLOAT             _z1{};
    MYFLOAT             _z2{};
    MYFLOAT             _z3{};
    MYFLOAT             _zlf [2]{};
    volatile bool     _rstat{};
    volatile MYFLOAT    _peak{};
    volatile MYFLOAT    _gmax{1};
    volatile MYFLOAT    _gmin{1};
    Histmin           _hist1;
    Histmin           _hist2;
	MYFLOAT _oldtresh{}, _oldrelease{};
	MYFLOAT  _tresh, _rel;
};

#endif
