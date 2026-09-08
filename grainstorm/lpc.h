#pragma once
//
// Created by pr on 26.03.19.
//

#ifndef GRAINSTORM_LPC_H
#define GRAINSTORM_LPC_H


#include "types.h"
#include "correlation.h"
#include <cstdlib>
#include <stdlib.h>
#include <array>

int32_t lpc_analyze3(MYFLOAT *x, MYFLOAT *coefs, int order, MYFLOAT *residue = nullptr);
int32_t lpc_analyze2(MYFLOAT *x, MYFLOAT *coefs, int order, MYFLOAT *residue = nullptr);

void preemphasis( MYFLOAT * x, int32_t len, MYFLOAT alpha = (15.f/16.f));
void deemphasis( MYFLOAT * x, int32_t len, MYFLOAT alpha = (15.f/16.f));


inline void apply_window(MYFLOAT x[], MYFLOAT y[], MYFLOAT w[], int32_t s);


#include <vector>


class Buzz {
public:
    Buzz(int32_t sr) {
        phaseinc = TABLE_LEN / (MYFLOAT) sr;
        MYFLOAT inc = 1. / (MYFLOAT) TABLE_LEN;
        for (int32_t i = 0; i < TABLE_LEN; i++)
            ft[i] = (MYFLOAT) cos(TWOPI_P * i * inc);

    }

    static inline MYFLOAT intpow1(MYFLOAT x, uint32_t n) {
        MYFLOAT ans = 1.0f;
        while (n != 0) {
            if (n & 1u) ans = ans * x;
            n >>= 1u;
            x = x * x;
        }
        return ans;
    }

    void SetPhase(MYFLOAT ph) {
        phase = (int32_t) (ph * (TABLE_LEN - 1));
        prvr = 0.0f;
    }

    void setCps(MYFLOAT Cps) {
        cps = Cps;
    }

    void compute(MYFLOAT *out, int32_t size) {
        int32_t k, km1, kpn, kpnm1;
        MYFLOAT r, absr, num, denom, scal;
        int32_t nn;
        k = (int32_t) offset;                   /* fix k and n  */
        if ((nn = (int32_t) numparts) < 0) nn = -nn;
        if (nn == 0) {              /* n must be > 0 */
            nn = 1;
        }
        km1 = k - 1;
        kpn = k + nn;
        kpnm1 = kpn - 1;
        if ((r = (MYFLOAT) mul) != prvr || nn != prvn) {
            twor = r + r;
            rsqp1 = r * r + 1.0f;
            rtn = intpow1(r, nn);
            rtnp1 = rtn * r;
            if ((absr = std::abs(r)) > 0.999f && absr < 1.001f)
                rsumr = 1.0f / nn;
            else rsumr = (1.0f - absr) / (1.0f - std::abs(rtn));
            prvr = r;
            prvn = (int16_t) nn;
        }
        MYFLOAT gain = 1.0f;
        scal = rsumr * gain;
        MYFLOAT incinc = 0.f;
        MYFLOAT inc;

        if (cps != prvcps) {
            if (port) {
                inc = (int32_t) (prvcps * phaseinc);
                incinc = (cps - prvcps) * phaseinc / (MYFLOAT) size;
            } else {
                inc = (int32_t) (cps * phaseinc);
            }
            prvcps = cps;

        } else
            inc = prvcps * phaseinc;


        for (int32_t i = 0; i < size; i++) {
            //if (ampcod)
            //    scal = rsumr * ampp[i];
            denom = rsqp1 - twor * ft[phase & lenmask];
            num = ft[phase * k & lenmask]
                  - r * ft[phase * km1 & lenmask]
                  - rtn * ft[phase * kpn & lenmask]
                  + rtnp1 * ft[phase * kpnm1 & lenmask];
            if (denom > 0.0002f || denom < -0.0002f) {
                out[i] = last = num / denom * scal;
            } else if (last < 0)
                out[i] = last = -gain;
            else
                out[i] = last = gain;
            //if (afreq.initialized)
            //    inc = (int32_t) ((prevfreq + prevfreq * afreq.Tick()) * sicvt);
            phase += inc;
            phase &= lenmask;

            if (incinc) {
                inc += incinc;
            }
        }
    }


private:
    MYFLOAT cps{}, numparts{5}, mul{1.0}, offset{1};
    bool port{true};
    int32_t prvn{};
    MYFLOAT prvcps{}, prvr{}, twor{}, rsqp1{}, rtn{}, rtnp1{}, rsumr{};
    int32_t phase{};
    MYFLOAT last{1.f};
    uint32_t lenmask{TABLE_LEN - 1};
    MYFLOAT phaseinc;
    std::array<MYFLOAT, TABLE_LEN> ft;
};


class LPC : private tsl::random::RandBase{
public:
    LPC(MYFLOAT sr);

    void compute(MYFLOAT *in, MYFLOAT *ctrl);

    void setOrder(int32_t order);

    void lpPred(MYFLOAT *x);


private:


    void pkpick();

    void pkinterp();

/* autocorrelation CPS
 */
    MYFLOAT lpCps(MYFLOAT sr);


    void coef2Pole(const MYFLOAT *c);

    void pole2Coef();

    void stabiliseAllpole(MYFLOAT *c, int32_t mode);


    void coef2Parm();

    void resonBnk(const MYFLOAT *, MYFLOAT *);

    std::vector<MYFLOAT> r, coeffs, k, pk, am, tmpmem, cf, pp, env;
    MYFLOAT cps{}, rms{};
    std::vector<tsl::complex<MYFLOAT>> pl;
    std::vector<MYFLOAT> del;
    int32_t maxM{}, N{}, M{};
    int32_t ord{};
    int32_t resonord{};
    MYFLOAT yt1[100]{}, yt2[100]{}, c2o[100]{}, c3o[100]{}, c2[100]{}, c3[100]{};
    MYFLOAT sum{};
    Buzz buzz;
    AutoCorrelation autoCorrelation;
    int32_t rp{};
};


#endif //GRAINSTORM_LPC_H
