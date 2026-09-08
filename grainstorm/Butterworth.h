#pragma once
//
// Created by pr on 06.11.22.
//

#ifndef GRAINSTORM_BUTTERWORTH_H
#define GRAINSTORM_BUTTERWORTH_H


#include <defines.h>
#include <cmath>
#include <atomic>
#include <complex>
#include <vector>
#include "base.h"
#include <app.h>


template<typename T>
class Butter6 {
public:
    Butter6(tsl::AppState  *appState) : _appState(appState){}
    Butter6<T> &operator=(Butter6<T> &o) {
        // Guard self assignment
        if (this == &o)
            return *this;
        prevHP = o.prevHP;
        prevLP = o.prevLP;

        b1_h = o.b1_h;
        b2_h = o.b2_h;
        b1_h_n = o.b1_h_n;
        b2_h_n = o.b2_h_n;
        b1_h_sl = o.b1_h_sl;
        b2_h_sl = o.b2_h_sl;
        y1_h = o.y1_h;
        y2_h = o.y2_h;
        a0_h = o.a0_h;
        a0_h_n = o.a0_h_n;
        a0_h_sl = o.a0_h_sl;
        b1_l = o.b1_l;
        b2_l = o.b2_l;
        b1_l_n = o.b1_l_n;
        b2_l_n = o.b2_l_n;
        b1_l_sl = o.b1_l_sl;
        b2_l_sl = o.b2_l_sl;
        y1_l = o.y1_l;
        y2_l = o.y2_l;
        a0_l = o.a0_l;
        a0_l_n = o.a0_l_n;
        a0_l_sl = o.a0_l_sl;
        countLP = o.countLP;
        countHP = o.countHP;
        _appState = o._appState;
        return *this;
    }

    void reset() {
        y1_l = y2_l = y1_h = y2_h = 0;
    }

    inline T tickLpHp6(T in) {
        auto y0h = in + b1_h * y1_h + b2_h * y2_h;
        auto tmp = a0_h * (y0h - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0h;
        auto y0l = tmp + b1_l * y1_l + b2_l * y2_l;
        auto out = a0_l * (y0l + 2. * y1_l + y2_l);
        y2_l = y1_l;
        y1_l = y0l;
        if (countHP < countMax) {
            ++countHP;
            a0_h += a0_h_sl;
            b1_h += b1_h_sl;
            b2_h += b2_h_sl;
        }
        if (countLP < countMax) {
            ++countLP;
            a0_l += a0_l_sl;
            b1_l += b1_l_sl;
            b2_l += b2_l_sl;
        }
        return out;
    }

    void setHP(const T f) {
        if (f == prevHP)return;
        prevHP = f;
        countHP = 0;
        SetHp(f, a0_h_n, b1_h_n, b2_h_n);
        a0_h_sl = (a0_h_n - a0_h) / (T) countMax;
        b1_h_sl = (b1_h_n - b1_h) / (T) countMax;
        b2_h_sl = (b2_h_n - b2_h) / (T) countMax;
    }

    void setLP(const T f) {
        if (f == prevLP)return;
        prevLP = f;
        countLP = 0;
        SetLp(f, a0_l_n, b1_l_n, b2_l_n);
        a0_l_sl = (a0_l_n - a0_l) / (T) countMax;
        b1_l_sl = (b1_l_n - b1_l) / (T) countMax;
        b2_l_sl = (b2_l_n - b2_l) / (T) countMax;
    }

protected:
    tsl::AppState *_appState{};
private:
    const int32_t countMax{static_cast<int>(_STATE->sr * .05)};

    static void SetHp(const T f, T &a0, T &b1, T &b2) {
        T C = tan(f * PI_P);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = 2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    static void SetLp(const T f, T &a0, T &b1, T &b2) {
        T C = 1. / tan(f * PI_P);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = -2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    T prevHP{}, prevLP{};

    T b1_h{}, b2_h{}, b1_h_n{}, b2_h_n{}, b1_h_sl{}, b2_h_sl{}, y1_h{}, y2_h{}, a0_h{}, a0_h_n{}, a0_h_sl{};
    T b1_l{}, b2_l{}, b1_l_n{}, b2_l_n{}, b1_l_sl{}, b2_l_sl{}, y1_l{}, y2_l{}, a0_l{}, a0_l_n{}, a0_l_sl{};
    int32_t countLP{}, countHP{};
};

template<typename T>
class ThreeBandSplitter {
public:
    ThreeBandSplitter(tsl::AppState *appState) : _appState{appState} {
        reset();
    }

    tsl::AppState *_appState;

    static void SetHp(tsl::AppState *_appState, T f, T &a0, T &b1, T &b2) {
        T pfreq = f * _STATE->pidsr;
        T C = tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = 2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    static void SetLp(tsl::AppState *_appState, T f, T &a0, T &b1, T &b2) {
        T pfreq = f * _STATE->pidsr;
        T C = 1. / tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0 = 1. / (1. + sqrt2C + C2);
        b1 = -2. * (1. - C2) * a0;
        b2 = -(1. - sqrt2C + C2) * a0;
    }

    inline void smooth(T in, T &sm) {
        sm = _STATE->smoothCoeff2 * (sm - in) + in;
    }


    inline void tick(T in, T out[]) {
        // LP
        T y0l = in + b1_lps * y1_lp1 + b2_lps * y2_lp1;
        UDD(y0l);
        T tmp = a0_lps * (y0l + 2. * y1_lp1 + y2_lp1);
        y2_lp1 = y1_lp1;
        y1_lp1 = y0l;
        y0l = tmp + b1_lps * y1_lp2 + b2_lps * y2_lp2;
        UDD(y0l);
        T lp = a0_lps * (y0l + 2. * y1_lp2 + y2_lp2);
        y2_lp2 = y1_lp2;
        y1_lp2 = y0l;

        y0l = in + b1_hps * y1_hp1 + b2_hps * y2_hp1;
        UDD(y0l);
        tmp = a0_hps * (y0l - 2. * y1_hp1 + y2_hp1);
        y2_hp1 = y1_hp1;
        y1_hp1 = y0l;
        y0l = tmp + b1_hps * y1_hp2 + b2_hps * y2_hp2;
        UDD(y0l);
        T hp = a0_hps * (y0l - 2. * y1_hp2 + y2_hp2);
        y2_hp2 = y1_hp2;
        y1_hp2 = y0l;


        y0l = lp + b1_apls * y1_apl11 + b2_apls * y2_apl11;
        UDD(y0l);
        tmp = a0_apls * (y0l + 2. * y1_apl11 + y2_apl11);
        y2_apl11 = y1_apl11;
        y1_apl11 = y0l;
        y0l = tmp + b1_apls * y1_apl12 + b2_apls * y2_apl12;
        UDD(y0l);
        T lp11 = a0_apls * (y0l + 2. * y1_apl12 + y2_apl12);
        y2_apl12 = y1_apl12;
        y1_apl12 = y0l;

        y0l = lp + b1_aphs * y1_aph11 + b2_aphs * y2_aph11;
        UDD(y0l);
        tmp = a0_aphs * (y0l - 2. * y1_aph11 + y2_aph11);
        y2_aph11 = y1_aph11;
        y1_aph11 = y0l;
        y0l = tmp + b1_aphs * y1_aph12 + b2_aphs * y2_aph12;
        UDD(y0l);
        T hp11 = a0_aphs * (y0l - 2. * y1_aph12 + y2_aph12);
        y2_aph12 = y1_aph12;
        y1_aph12 = y0l;


        out[0] = lp11 + hp11;

        y0l = hp + b1_apls * y1_apl21 + b2_apls * y2_apl21;
        UDD(y0l);
        tmp = a0_apls * (y0l + 2. * y1_apl21 + y2_apl21);
        y2_apl21 = y1_apl21;
        y1_apl21 = y0l;
        y0l = tmp + b1_apls * y1_apl22 + b2_apls * y2_apl22;
        UDD(y0l);
        out[1] = a0_apls * (y0l + 2. * y1_apl22 + y2_apl22);
        y2_apl22 = y1_apl22;
        y1_apl22 = y0l;

        y0l = hp + b1_aphs * y1_aph21 + b2_aphs * y2_aph21;
        UDD(y0l);
        tmp = a0_aphs * (y0l - 2. * y1_aph21 + y2_aph21);
        y2_aph21 = y1_aph21;
        y1_aph21 = y0l;
        y0l = tmp + b1_aphs * y1_aph22 + b2_aphs * y2_aph22;
        UDD(y0l);
        out[2] = a0_aphs * (y0l - 2. * y1_aph22 + y2_aph22);
        y2_aph22 = y1_aph22;
        y1_aph22 = y0l;

        smooth(b1_lp, b1_lps);
        smooth(b2_lp, b2_lps);
        smooth(a0_lp, a0_lps);
        smooth(b1_hp, b1_hps);
        smooth(b2_hp, b2_hps);
        smooth(a0_hp, a0_hps);

        smooth(b1_apl, b1_apls);
        smooth(b2_apl, b2_apls);
        smooth(a0_apl, a0_apls);
        smooth(b1_aph, b1_aphs);
        smooth(b2_aph, b2_aphs);
        smooth(a0_aph, a0_aphs);
    }

    void check(T lowco, T hico) {
        if (oldlowco != lowco) {
            oldlowco = lowco;
            SetLp(_STATE, oldlowco, a0_lp, b1_lp, b2_lp);
            SetHp(_STATE, oldlowco, a0_hp, b1_hp, b2_hp);
        }
        if (oldhico != hico) {
            oldhico = hico;
            SetLp(_STATE, oldhico, a0_apl, b1_apl, b2_apl);
            SetHp(_STATE, oldhico, a0_aph, b1_aph, b2_aph);
        }
    }

private:
    T oldlowco{}, oldhico{};
    T y1_lp1, y2_lp1, y1_hp1, y2_hp1;
    T y1_lp2, y2_lp2, y1_hp2, y2_hp2;
    T y1_apl11, y2_apl11, y1_apl12, y2_apl12, y1_aph11, y2_aph11, y1_aph12, y2_aph12, y1_apl21, y2_apl21, y1_apl22, y2_apl22, y1_aph21, y2_aph21, y1_aph22, y2_aph22;
    T a0_apl, b1_apl, b2_apl, a0_aph, b1_aph, b2_aph, a0_lp, b1_lp, b2_lp, a0_hp, b1_hp, b2_hp;
    T a0_lps, b1_lps, b2_lps, a0_hps, b1_hps, b2_hps, a0_apls, b1_apls, b2_apls, a0_aphs, b1_aphs, b2_aphs;

    void reset() {
        y1_lp1 = y2_lp1 = y1_hp1 = y2_hp1 = 0.0;
        y1_lp2 = y2_lp2 = y1_hp2 = y2_hp2 = 0.0;
        y1_apl11 = y2_apl11 = y1_apl12 = y2_apl12 = y1_aph11 = y2_aph11 = y1_aph12 = y2_aph12 = y1_apl21 = y2_apl21 = y1_apl22 = y2_apl22 = y1_aph21 = y2_aph21 = y1_aph22 = y2_aph22 = 0.0;
        a0_apl = b1_apl = b2_apl = a0_aph = b1_aph = b2_aph = a0_lp = b1_lp = b2_lp = a0_hp = b1_hp = b2_hp = 0.0;
        a0_lps = b1_lps = b2_lps = a0_hps = b1_hps = b2_hps = a0_apls = b1_apls = b2_apls = a0_aphs = b1_aphs = b2_aphs = 0.0;
        oldlowco = oldhico = 0.0;
    }

/*
    LR2 with DFII:

//------------------------------
// LR2
// fc -> cutoff frequency
// pi -> 3.14285714285714
// srate -> sample rate
//------------------------------
            fpi = pi*fc;
    wc = 2*fpi;
    wc2 = wc*wc;
    wc22 = 2*wc2;
    k = wc/tan(fpi/srate);
    k2 = k*k;
    k22 = 2*k2;
    wck2 = 2*wc*k;
    tmpk = (k2+wc2+wck2);
//b shared
    b1 = (-k22+wc22)/tmpk;
    b2 = (-wck2+k2+wc2)/tmpk;
//---------------
// low-pass
//---------------
    a0_lp = (wc2)/tmpk;
    a1_lp = (wc22)/tmpk;
    a2_lp = (wc2)/tmpk;
//----------------
// high-pass
//----------------
    a0_hp = (k2)/tmpk;
    a1_hp = (-k22)/tmpk;
    a2_hp = (k2)/tmpk;

//=========================
// sample loop, in -> input
//=========================
//---lp
    lp_out = a0_lp*in + lp_xm0;
    lp_xm0 = a1_lp*in - b1*lp_out + lp_xm1;
    lp_xm1 = a2_lp*in - b2*lp_out;
//---hp
    hp_out = a0_hp*in + hp_xm0;
    hp_xm0 = a1_hp*in - b1*hp_out + hp_xm1;
    hp_xm1 = a2_hp*in - b2*hp_out;

// the two are with 180 degrees phase shift,
// so you need to invert the phase of one.
    out = lp_out + hp_out*(-1);

//result is allpass at Fc
 */

};


template<typename T>
class Butterworth {
public:
    Butterworth(tsl::AppState *appState) : _appState(appState), smoothCoeff{exp(
            -2. / (FXRELEASE * appState->sr * 0.001))} {};

    Butterworth(tsl::AppState *appState, std::atomic<MYFLOAT> *_hpcut, std::atomic<MYFLOAT> *_lpcut,
                std::atomic<MYFLOAT> *_bpcenter, std::atomic<MYFLOAT> *_bpbw,
                std::atomic<MYFLOAT> *_brcenter, std::atomic<MYFLOAT> *_brbw,
                bool _logarithmic) : _appState(appState), smoothCoeff{exp(
            -2. / (FXRELEASE * appState->sr * 0.001))} {
        init(_hpcut, _lpcut, _bpcenter, _bpbw, _brcenter, _brbw, _logarithmic);
    }

    Butterworth(tsl::AppState *appState, std::atomic<MYFLOAT> *_hpcut, std::atomic<MYFLOAT> *_lpcut,
                bool log) : _appState(appState), smoothCoeff{exp(
            -2. / (FXRELEASE * appState->sr * 0.001))} {
        init(_hpcut, _lpcut, nullptr, nullptr, nullptr, nullptr, log);
    }

    void init(std::atomic<MYFLOAT> *_hpcut, std::atomic<MYFLOAT> *_lpcut,
              std::atomic<MYFLOAT> *_bpcenter, std::atomic<MYFLOAT> *_bpbw,
              std::atomic<MYFLOAT> *_brcenter, std::atomic<MYFLOAT> *_brbw,
              bool _logarithmic) {
        logarithmic = _logarithmic;
        hp_cut = _hpcut;
        lp_cut = _lpcut;
        bp_center = _bpcenter;
        bp_bw = _bpbw;
        br_center = _brcenter;
        br_bw = _brbw;

        Reset();

        if (hp_cut != nullptr) {
            _smooth[0] = logarithmic ? LOG2NORMAL(hp_cut->load()) : hp_cut->load();
            SetHp(_smooth[0]);
        }

        if (lp_cut != nullptr) {
            _smooth[1] = logarithmic ? LOG2NORMAL(lp_cut->load()) : lp_cut->load();
            SetLp(_smooth[1]);
        }

        if (bp_center != nullptr && bp_bw != nullptr) {
            _smooth[0] = logarithmic ? LOG2NORMAL(bp_center->load()) : bp_center->load();
            _smooth[1] = logarithmic ? LOG2NORMAL(bp_bw->load()) : bp_bw->load();
            SetBp(_smooth[0], _smooth[1]);
        }

        if (br_center != nullptr && br_bw != nullptr) {
            _smooth[0] = logarithmic ? LOG2NORMAL(br_center->load()) : br_center->load();
            _smooth[1] = 0.01 + 0.99 * br_bw->load();
            SetBr(_smooth[0], _smooth[1]);
        }
    }

    void Reset() {
        a0_h = b1_h = b2_h = y1_h = y2_h = y1_h2 = y2_h2 = y1_h3 = y2_h3 = y1_h4 = y2_h4 = 0.0;
        a0_l = b1_l = b2_l = y1_l = y2_l = y1_l2 = y2_l2 = y1_l3 = y2_l3 = y1_l4 = y2_l4 = 0.0;
        a0_br = a1_br = b2_br = y1_br1 = y2_br1 = y1_br2 = y2_br2 = y1_br3 = y2_br3 = y1_br4 = y2_br4 = 0.0;
        a0_bp = b1_bp = b2_bp = y1_bp1 = y2_bp1 = y1_bp2 = y2_bp2 = 0.0;
    }


    template<typename T2>
    T2 TickLp(T2 in) {
        T frlp = logarithmic ? LOG2NORMAL(lp_cut->load()) : lp_cut->load();
        if (DISTANCE(frlp, _smooth[1]) > 1.)
            SetLp(sm(frlp, _smooth[1], smoothCoeff));
        T y0 = in + b1_l * y1_l + b2_l * y2_l;
        UDD(y0)
        auto ret = (T2) (a0_l * (y0 + 2. * y1_l + y2_l));
        y2_l = y1_l;
        y1_l = y0;
        return ret;
    }

    template<typename T2>
    void HpLp(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;

        T y0h, y0l, tmp;

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;
        T frhp = logarithmic ? LOG2NORMAL(hp_cut->load()) : hp_cut->load();
        T frlp = logarithmic ? LOG2NORMAL(lp_cut->load()) : lp_cut->load();
        const bool hpcut_changed = DISTANCE(frhp, _smooth[0]) > 1.;
        const bool lpcut_changed = DISTANCE(frlp, _smooth[1]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }

            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l + y2_l));

            y2_h = *(in++) + b1_h * y0h + b2_h * y1_h;
            tmp = a0_h * (y2_h - 2. * y0h + y1_h);
            y2_l = tmp + b1_l * y0l + b2_l * y1_l;
            *(out++) = (T2) (a0_l * (y2_l + 2. * y0l + y1_l));

            y1_h = *(in++) + b1_h * y2_h + b2_h * y0h;
            tmp = a0_h * (y1_h - 2. * y2_h + y0h);
            y1_l = tmp + b1_l * y2_l + b2_l * y0l;
            *(out++) = (T2) (a0_l * (y1_l + 2. * y2_l + y0l));
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }
            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y2_h = y1_h;
            y1_h = y0h;
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l + y2_l));
            y2_l = y1_l;
            y1_l = y0l;
        }
        UDD(y1_h);
        UDD(y2_h);
        UDD(y1_l);
        UDD(y2_l);
    }

    template<typename T2>
    T2 tickLpHp6(T2 in) {
        auto y0h = in + b1_h * y1_h + b2_h * y2_h;
        auto tmp = a0_h * (y0h - 2. * y1_h + y2_h);
        y2_h = y1_h;
        y1_h = y0h;
        auto y0l = tmp + b1_l * y1_l + b2_l * y2_l;
        auto out = (T2) (a0_l * (y0l + 2. * y1_l + y2_l));
        y2_l = y1_l;
        y1_l = y0l;
        return out;
    }

    template<typename T2>
    void HpLp12(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;

        T y0h, y0l, tmp, tmpbuf[3];
        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;
        T frhp = logarithmic ? LOG2NORMAL(hp_cut->load()) : hp_cut->load();
        T frlp = logarithmic ? LOG2NORMAL(lp_cut->load()) : lp_cut->load();
        const bool hpcut_changed = DISTANCE(frhp, _smooth[0]) > 1.;
        const bool lpcut_changed = DISTANCE(frlp, _smooth[1]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }
            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l + y2_l));

            y2_h = *(in++) + b1_h * y0h + b2_h * y1_h;
            tmp = a0_h * (y2_h - 2. * y0h + y1_h);
            y2_l = tmp + b1_l * y0l + b2_l * y1_l;
            tmpbuf[1] = (a0_l * (y2_l + 2. * y0l + y1_l));

            y1_h = *(in++) + b1_h * y2_h + b2_h * y0h;
            tmp = a0_h * (y1_h - 2. * y2_h + y0h);
            y1_l = tmp + b1_l * y2_l + b2_l * y0l;
            tmpbuf[2] = (a0_l * (y1_l + 2. * y2_l + y0l));

            y0h = tmpbuf[0] + b1_h * y1_h2 + b2_h * y2_h2;
            tmp = a0_h * (y0h - 2. * y1_h2 + y2_h2);
            y0l = tmp + b1_l * y1_l2 + b2_l * y2_l2;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l2 + y2_l2));

            y2_h2 = tmpbuf[1] + b1_h * y0h + b2_h * y1_h2;
            tmp = a0_h * (y2_h2 - 2. * y0h + y1_h2);
            y2_l2 = tmp + b1_l * y0l + b2_l * y1_l2;
            *(out++) = (T2) (a0_l * (y2_l2 + 2. * y0l + y1_l2));

            y1_h2 = tmpbuf[2] + b1_h * y2_h2 + b2_h * y0h;
            tmp = a0_h * (y1_h2 - 2. * y2_h2 + y0h);
            y1_l2 = tmp + b1_l * y2_l2 + b2_l * y0l;
            *(out++) = (T2) (a0_l * (y1_l2 + 2. * y2_l2 + y0l));
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }
            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y2_h = y1_h;
            y1_h = y0h;
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l + y2_l));
            y2_l = y1_l;
            y1_l = y0l;

            y0h = tmpbuf[0] + b1_h * y1_h2 + b2_h * y2_h2;
            tmp = a0_h * (y0h - 2. * y1_h2 + y2_h2);
            y2_h2 = y1_h2;
            y1_h2 = y0h;
            y0l = tmp + b1_l * y1_l2 + b2_l * y2_l2;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l2 + y2_l2));
            y2_l2 = y1_l2;
            y1_l2 = y0l;
        }
        UDD(y1_h);
        UDD(y2_h);
        UDD(y1_l);
        UDD(y2_l);
        UDD(y1_h2);
        UDD(y2_h2);
        UDD(y1_l2);
        UDD(y2_l2);
    }

    template<typename T2>
    T2 tickLp12(const T2 in) {
        auto y0l = in + b1_l * y1_l + b2_l * y2_l;
        auto tmp = (a0_l * (y0l + 2. * y1_l + y2_l));
        y2_l = y1_l;
        y1_l = y0l;
        y0l = tmp + b1_l * y1_l2 + b2_l * y2_l2;
        tmp = (T2) (a0_l * (y0l + 2. * y1_l2 + y2_l2));
        y2_l2 = y1_l2;
        y1_l2 = y0l;
        return tmp;
    }


    template<typename T2>
    void HpLp24(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;

        T y0h, y0l, tmp, tmpbuf[3];


        uint32_t size_main_loop = size / 3;
        uint32_t size_rest = size % 3;

        T frhp = logarithmic ? LOG2NORMAL(hp_cut->load()) : hp_cut->load();
        T frlp = logarithmic ? LOG2NORMAL(lp_cut->load()) : lp_cut->load();
        const bool hpcut_changed = DISTANCE(frhp, _smooth[0]) > 1.;
        const bool lpcut_changed = DISTANCE(frlp, _smooth[1]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }
            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l + y2_l));

            y2_h = *(in++) + b1_h * y0h + b2_h * y1_h;
            tmp = a0_h * (y2_h - 2. * y0h + y1_h);
            y2_l = tmp + b1_l * y0l + b2_l * y1_l;
            tmpbuf[1] = (a0_l * (y2_l + 2. * y0l + y1_l));

            y1_h = *(in++) + b1_h * y2_h + b2_h * y0h;
            tmp = a0_h * (y1_h - 2. * y2_h + y0h);
            y1_l = tmp + b1_l * y2_l + b2_l * y0l;
            tmpbuf[2] = (a0_l * (y1_l + 2. * y2_l + y0l));

            y0h = tmpbuf[0] + b1_h * y1_h2 + b2_h * y2_h2;
            tmp = a0_h * (y0h - 2. * y1_h2 + y2_h2);
            y0l = tmp + b1_l * y1_l2 + b2_l * y2_l2;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l2 + y2_l2));

            y2_h2 = tmpbuf[1] + b1_h * y0h + b2_h * y1_h2;
            tmp = a0_h * (y2_h2 - 2. * y0h + y1_h2);
            y2_l2 = tmp + b1_l * y0l + b2_l * y1_l2;
            tmpbuf[1] = (a0_l * (y2_l2 + 2. * y0l + y1_l2));

            y1_h2 = tmpbuf[2] + b1_h * y2_h2 + b2_h * y0h;
            tmp = a0_h * (y1_h2 - 2. * y2_h2 + y0h);
            y1_l2 = tmp + b1_l * y2_l2 + b2_l * y0l;
            tmpbuf[2] = (a0_l * (y1_l2 + 2. * y2_l2 + y0l));

            y0h = tmpbuf[0] + b1_h * y1_h3 + b2_h * y2_h3;
            tmp = a0_h * (y0h - 2. * y1_h3 + y2_h3);
            y0l = tmp + b1_l * y1_l3 + b2_l * y2_l3;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l3 + y2_l3));

            y2_h3 = tmpbuf[1] + b1_h * y0h + b2_h * y1_h3;
            tmp = a0_h * (y2_h3 - 2. * y0h + y1_h3);
            y2_l3 = tmp + b1_l * y0l + b2_l * y1_l3;
            tmpbuf[1] = (a0_l * (y2_l3 + 2. * y0l + y1_l3));

            y1_h3 = tmpbuf[2] + b1_h * y2_h3 + b2_h * y0h;
            tmp = a0_h * (y1_h3 - 2. * y2_h3 + y0h);
            y1_l3 = tmp + b1_l * y2_l3 + b2_l * y0l;
            tmpbuf[2] = (a0_l * (y1_l3 + 2. * y2_l3 + y0l));

            y0h = tmpbuf[0] + b1_h * y1_h4 + b2_h * y2_h4;
            tmp = a0_h * (y0h - 2. * y1_h4 + y2_h4);
            y0l = tmp + b1_l * y1_l4 + b2_l * y2_l4;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l4 + y2_l4));

            y2_h4 = tmpbuf[1] + b1_h * y0h + b2_h * y1_h4;
            tmp = a0_h * (y2_h4 - 2. * y0h + y1_h4);
            y2_l4 = tmp + b1_l * y0l + b2_l * y1_l4;
            *(out++) = (T2) (a0_l * (y2_l4 + 2. * y0l + y1_l4));

            y1_h4 = tmpbuf[2] + b1_h * y2_h4 + b2_h * y0h;
            tmp = a0_h * (y1_h4 - 2. * y2_h4 + y0h);
            y1_l4 = tmp + b1_l * y2_l4 + b2_l * y0l;
            *(out++) = (T2) (a0_l * (y1_l4 + 2. * y2_l4 + y0l));
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (hpcut_changed) {
                SetHp(sm(frhp, _smooth[0], smoothCoeff));
            }
            if (lpcut_changed) {
                SetLp(sm(frlp, _smooth[1], smoothCoeff));
            }

            y0h = *(in++) + b1_h * y1_h + b2_h * y2_h;
            tmp = a0_h * (y0h - 2. * y1_h + y2_h);
            y2_h = y1_h;
            y1_h = y0h;
            y0l = tmp + b1_l * y1_l + b2_l * y2_l;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l + y2_l));
            y2_l = y1_l;
            y1_l = y0l;

            y0h = tmpbuf[0] + b1_h * y1_h2 + b2_h * y2_h2;
            tmp = a0_h * (y0h - 2. * y1_h2 + y2_h2);
            y2_h2 = y1_h2;
            y1_h2 = y0h;
            y0l = tmp + b1_l * y1_l2 + b2_l * y2_l2;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l2 + y2_l2));
            y2_l2 = y1_l2;
            y1_l2 = y0l;

            y0h = tmpbuf[0] + b1_h * y1_h3 + b2_h * y2_h3;
            tmp = a0_h * (y0h - 2. * y1_h3 + y2_h3);
            y2_h3 = y1_h3;
            y1_h3 = y0h;
            y0l = tmp + b1_l * y1_l3 + b2_l * y2_l3;
            tmpbuf[0] = (a0_l * (y0l + 2. * y1_l3 + y2_l3));
            y2_l3 = y1_l3;
            y1_l3 = y0l;

            y0h = tmpbuf[0] + b1_h * y1_h4 + b2_h * y2_h4;
            tmp = a0_h * (y0h - 2. * y1_h4 + y2_h4);
            y2_h4 = y1_h4;
            y1_h4 = y0h;
            y0l = tmp + b1_l * y1_l4 + b2_l * y2_l4;
            *(out++) = (T2) (a0_l * (y0l + 2. * y1_l4 + y2_l4));
            y2_l4 = y1_l4;
            y1_l4 = y0l;
        }

        UDD(y1_h);
        UDD(y2_h);
        UDD(y1_l);
        UDD(y2_l);
        UDD(y1_h2);
        UDD(y2_h2);
        UDD(y1_l2);
        UDD(y2_l2);
        UDD(y1_h3);
        UDD(y2_h3);
        UDD(y1_l3);
        UDD(y2_l3);
        UDD(y1_h4);
        UDD(y2_h4);
        UDD(y1_l4);
        UDD(y2_l4);
    }

    template<typename T2>
    void Br6(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;
        T y0, ay;

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;

        MYFLOAT fr = logarithmic ? LOG2NORMAL(br_center->load()) : br_center->load();
        MYFLOAT q = 0.01f + 0.99f * br_bw->load();
        const bool changed = DISTANCE(q, _smooth[1]) > 0.01 || DISTANCE(fr, _smooth[0]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0]), sm(q, _smooth[1], smoothCoeff));


            ay = a1_br * y1_br1;
            y0 = *(in++) - ay - b2_br * y2_br1;
            *(out++) = (T2) (a0_br * (y0 + y2_br1) + ay);

            ay = a1_br * y0;
            y2_br1 = *(in++) - ay - b2_br * y1_br1;
            *(out++) = (T2) (a0_br * (y2_br1 + y1_br1) + ay);

            ay = a1_br * y2_br1;
            y1_br1 = *(in++) - ay - b2_br * y0;
            *(out++) = (T2) (a0_br * (y1_br1 + y0) + ay);
        }

        for (uint32_t i = 0; i < size_rest; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0]), sm(q, _smooth[1], smoothCoeff));

            ay = a1_br * y1_br1;
            y0 = *(in++) - ay - b2_br * y2_br1;
            *(out++) = (T2) (a0_br * (y0 + y2_br1) + ay);
            y2_br1 = y1_br1;
            y1_br1 = y0;
        }
        UDD(y1_br1);
        UDD(y2_br1);
    }

    template<typename T2>
    void Br12(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;

        T y0, ay1, ay2, tmpbuf[3];

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;

        MYFLOAT fr = logarithmic ? LOG2NORMAL(br_center->load()) : br_center->load();
        MYFLOAT q = 0.01f + 0.99f * br_bw->load();
        const bool changed = DISTANCE(q, _smooth[1]) > 0.01 || DISTANCE(fr, _smooth[0]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0]), sm(q, _smooth[1], smoothCoeff));

            ay1 = a1_br * y1_br1;
            y0 = *(in++) - ay1 - b2_br * y2_br1;
            tmpbuf[0] = (a0_br * (y0 + y2_br1) + ay1);

            ay1 = a1_br * y0;
            y2_br1 = *(in++) - ay1 - b2_br * y1_br1;
            tmpbuf[1] = (a0_br * (y2_br1 + y1_br1) + ay1);

            ay1 = a1_br * y2_br1;
            y1_br1 = *(in++) - ay1 - b2_br * y0;
            tmpbuf[2] = (a0_br * (y1_br1 + y0) + ay1);

            ay2 = a1_br * y1_br2;
            y0 = tmpbuf[0] - ay2 - b2_br * y2_br2;
            *(out++) = (T2) (a0_br * (y0 + y2_br2) + ay2);

            ay2 = a1_br * y0;
            y2_br2 = tmpbuf[1] - ay2 - b2_br * y1_br2;
            *(out++) = (T2) (a0_br * (y2_br2 + y1_br2) + ay2);

            ay2 = a1_br * y2_br2;
            y1_br2 = tmpbuf[2] - ay2 - b2_br * y0;
            *(out++) = (T2) (a0_br * (y1_br2 + y0) + ay2);
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0]), sm(q, _smooth[1], smoothCoeff));

            ay1 = a1_br * y1_br1;
            y0 = *(in++) - ay1 - b2_br * y2_br1;
            tmpbuf[0] = a0_br * (y0 + y2_br1) + ay1;
            y2_br1 = y1_br1;
            y1_br1 = y0;
            ay2 = a1_br * y1_br2;
            y0 = tmpbuf[0] - ay2 - b2_br * y2_br2;
            *(out++) = (T2) (a0_br * (y0 + y2_br2) + ay2);
            y2_br2 = y1_br2;
            y1_br2 = y0;
        }
        UDD(y1_br1);
        UDD(y2_br1);
        UDD(y1_br2);
        UDD(y2_br2);
    }

    template<typename T2>
    void Br24(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;

        T2 y0, ay1, ay2, ay3, ay4, tmpbuf[3];

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;

        MYFLOAT fr = logarithmic ? LOG2NORMAL(br_center->load()) : br_center->load();
        MYFLOAT q = 0.01f + 0.99f * br_bw->load();
        const bool changed = DISTANCE(q, _smooth[1]) > 0.01 || DISTANCE(fr, _smooth[0]) > 1.;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            ay1 = a1_br * y1_br1;
            y0 = *(in++) - ay1 - b2_br * y2_br1;
            tmpbuf[0] = (a0_br * (y0 + y2_br1) + ay1);

            ay1 = a1_br * y0;
            y2_br1 = *(in++) - ay1 - b2_br * y1_br1;
            tmpbuf[1] = (a0_br * (y2_br1 + y1_br1) + ay1);

            ay1 = a1_br * y2_br1;
            y1_br1 = *(in++) - ay1 - b2_br * y0;
            tmpbuf[2] = (a0_br * (y1_br1 + y0) + ay1);

            ay2 = a1_br * y1_br2;
            y0 = tmpbuf[0] - ay2 - b2_br * y2_br2;
            tmpbuf[0] = (a0_br * (y0 + y2_br2) + ay2);

            ay2 = a1_br * y0;
            y2_br2 = tmpbuf[1] - ay2 - b2_br * y1_br2;
            tmpbuf[1] = (a0_br * (y2_br2 + y1_br2) + ay2);

            ay2 = a1_br * y2_br2;
            y1_br2 = tmpbuf[2] - ay2 - b2_br * y0;
            tmpbuf[2] = (a0_br * (y1_br2 + y0) + ay2);

            ay3 = a1_br * y1_br3;
            y0 = tmpbuf[0] - ay3 - b2_br * y2_br3;
            tmpbuf[0] = (a0_br * (y0 + y2_br3) + ay3);

            ay3 = a1_br * y0;
            y2_br3 = tmpbuf[1] - ay3 - b2_br * y1_br3;
            tmpbuf[1] = a0_br * (y2_br3 + y1_br3) + ay3;

            ay3 = a1_br * y2_br3;
            y1_br3 = tmpbuf[2] - ay3 - b2_br * y0;
            tmpbuf[2] = a0_br * (y1_br3 + y0) + ay3;

            ay4 = a1_br * y1_br4;
            y0 = tmpbuf[0] - ay4 - b2_br * y2_br4;
            *(out++) = (T2) (a0_br * (y0 + y2_br4) + ay4);

            ay4 = a1_br * y0;
            y2_br4 = tmpbuf[1] - ay4 - b2_br * y1_br4;
            *(out++) = (T2) (a0_br * (y2_br4 + y1_br4) + ay4);

            ay4 = a1_br * y2_br4;
            y1_br4 = tmpbuf[2] - ay4 - b2_br * y0;
            *(out++) = (T2) (a0_br * (y1_br4 + y0) + ay4);
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (changed)
                SetBr(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            ay1 = a1_br * y1_br1;
            y0 = *(in++) - ay1 - b2_br * y2_br1;
            tmpbuf[0] = a0_br * (y0 + y2_br1) + ay1;
            y2_br1 = y1_br1;
            y1_br1 = y0;

            ay2 = a1_br * y1_br2;
            y0 = tmpbuf[0] - ay2 - b2_br * y2_br2;
            tmpbuf[0] = a0_br * (y0 + y2_br2) + ay2;
            y2_br2 = y1_br2;
            y1_br2 = y0;

            ay3 = a1_br * y1_br3;
            y0 = tmpbuf[0] - ay3 - b2_br * y2_br3;
            tmpbuf[0] = a0_br * (y0 + y2_br3) + ay3;
            y2_br3 = y1_br3;
            y1_br3 = y0;

            ay4 = a1_br * y1_br4;
            y0 = tmpbuf[0] - ay4 - b2_br * y2_br4;
            *(out++) = (T2) (a0_br * (y0 + y2_br4) + ay4);
            y2_br4 = y1_br4;
            y1_br4 = y0;
        }
        UDD(y1_br1);
        UDD(y2_br1);
        UDD(y1_br2);
        UDD(y2_br2);
        UDD(y1_br3);
        UDD(y2_br3);
        UDD(y1_br4);
        UDD(y2_br4);
    }

    template<typename T2>
    void Bp6(T2 *in, T2 *out, uint32_t size) {
        if (out == nullptr)
            out = in;
        T y0;

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;
        MYFLOAT fr = logarithmic ? powf(10, bp_center->load() * .05f) : bp_center->load();
        MYFLOAT q = logarithmic ? powf(10, bp_bw->load() * .05f) : bp_bw->load();
        const bool changed = DISTANCE(fr, _smooth[0]) > 1. || DISTANCE(q, _smooth[2]) > 0.01;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (changed)
                SetBp(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            y0 = *(in++) + b1_bp * y1_bp1 + b2_bp * y2_bp1;
            *(out++) = (T2) (a0_bp * (y0 - y2_bp1));

            y2_bp1 = *(in++) + b1_bp * y0 + b2_bp * y1_bp1;
            *(out++) = (T2) (a0_bp * (y2_bp1 - y1_bp1));

            y1_bp1 = *(in++) + b1_bp * y2_bp1 + b2_bp * y0;
            *(out++) = (T2) (a0_bp * (y1_bp1 - y0));
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (changed)
                SetBp(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            y0 = *(in++) + b1_bp * y1_bp1 + b2_bp * y2_bp1;
            *(out++) = (T2) (a0_bp * (y0 - y2_bp1));
            y2_bp1 = y1_bp1;
            y1_bp1 = y0;
        }
        UDD(y1_bp1);
        UDD(y2_bp1);
    }

    template<typename T2>
    void Bp12(T2 *in, T2 *out, uint32_t size) {

        if (out == nullptr)
            out = in;

        const uint32_t size_main_loop = size / 3;
        const uint32_t size_rest = size % 3;

        T y0, tmpbuf[3];


        MYFLOAT fr = logarithmic ? powf(10, bp_center->load() * .05f) : bp_center->load();
        MYFLOAT q = logarithmic ? powf(10, bp_bw->load() * .05f) : bp_bw->load();
        const bool changed = DISTANCE(fr, _smooth[0]) > 1. || DISTANCE(q, _smooth[2]) > 0.01;
        for (uint32_t i = 0; i < size_main_loop; i++) {
            if (changed)
                SetBp(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            y0 = *(in++) + b1_bp * y1_bp1 + b2_bp * y2_bp1;
            tmpbuf[0] = a0_bp * (y0 - y2_bp1);

            y2_bp1 = *(in++) + b1_bp * y0 + b2_bp * y1_bp1;
            tmpbuf[1] = a0_bp * (y2_bp1 - y1_bp1);

            y1_bp1 = *(in++) + b1_bp * y2_bp1 + b2_bp * y0;
            tmpbuf[2] = a0_bp * (y1_bp1 - y0);

            y0 = tmpbuf[0] + b1_bp * y1_bp2 + b2_bp * y2_bp2;
            *(out++) = (MYFLOAT) (a0_bp * (y0 - y2_bp2));

            y2_bp2 = tmpbuf[1] + b1_bp * y0 + b2_bp * y1_bp2;
            *(out++) = (MYFLOAT) (a0_bp * (y2_bp2 - y1_bp2));

            y1_bp2 = tmpbuf[2] + b1_bp * y2_bp2 + b2_bp * y0;
            *(out++) = (MYFLOAT) (a0_bp * (y1_bp2 - y0));
        }
        for (uint32_t i = 0; i < size_rest; i++) {
            if (changed)
                SetBp(sm(fr, _smooth[0], smoothCoeff), sm(q, _smooth[1], smoothCoeff));

            y0 = *(in++) + b1_bp * y1_bp1 + b2_bp * y2_bp1;
            tmpbuf[0] = a0_bp * (y0 - y2_bp1);
            y2_bp1 = y1_bp1;
            y1_bp1 = y0;
            y0 = tmpbuf[0] + b1_bp * y1_bp2 + b2_bp * y2_bp2;
            *(out++) = (MYFLOAT) (a0_bp * (y0 - y2_bp2));
            y2_bp2 = y1_bp2;
            y1_bp2 = y0;
        }
        UDD(y1_bp1);
        UDD(y2_bp1);
        UDD(y1_bp2);
        UDD(y2_bp2);
    }


    void SetHp(T f) {
        T pfreq = f * _STATE->pidsr;
        T C = tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0_h = 1. / (1. + sqrt2C + C2);
        b1_h = 2. * (1. - C2) * a0_h;
        b2_h = -(1. - sqrt2C + C2) * a0_h;
    }

    void SetLp(T f) {
        T pfreq = f * _STATE->pidsr;
        T C = 1. / tan(pfreq);
        T C2 = C * C;
        T sqrt2C = C * ROOT2;
        a0_l = 1. / (1. + sqrt2C + C2);
        b1_l = -2. * (1. - C2) * a0_l;
        b2_l = -(1. - sqrt2C + C2) * a0_l;
    }

    void SetBr(T f, T bw) {
        T pfreq = f * _STATE->pidsr;
        T pbw = bw * pfreq * 0.5;

        T C = tan(pbw);
        T D = 2. * cos(pfreq);

        a0_br = 1. / (1. + C);
        a1_br = -D * a0_br;
        b2_br = (1. - C) * a0_br;
    }

    void SetBp(T f, T bw) {
        T pfreq = f * _STATE->pidsr;
        T pbw = bw * pfreq * 0.5;

        T C = 1. / tan(pbw);
        T D = 2. * cos(pfreq);

        a0_bp = 1. / (1. + C);
        b1_bp = C * D * a0_bp;
        b2_bp = (1. - C) * a0_bp;
    }

private:
    tsl::AppState *_appState{};
    T a0_h, b1_h, b2_h, y1_h, y2_h, y1_h2, y2_h2, y1_h3, y2_h3, y1_h4, y2_h4;
    T a0_l, b1_l, b2_l, y1_l, y2_l, y1_l2, y2_l2, y1_l3, y2_l3, y1_l4, y2_l4;
    T a0_br, a1_br, b2_br, y1_br1, y2_br1, y1_br2, y2_br2, y1_br3, y2_br3, y1_br4, y2_br4;
    T a0_bp, b1_bp, b2_bp, y1_bp1, y2_bp1, y1_bp2, y2_bp2;

    T _smooth[2];
    const T smoothCoeff;
    std::atomic<MYFLOAT> *hp_cut, *lp_cut, *br_center, *br_bw, *bp_center, *bp_bw;
    bool logarithmic;

    static inline T sm(const T in, T &smooth, T smoothCoeff) {
        return smooth = smoothCoeff * (smooth - in) + in;
    }

};


#endif //GRAINSTORM_BUTTERWORTH_H
