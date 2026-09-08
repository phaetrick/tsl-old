#pragma once
//
// Created by pr on 29.01.18.
//

#ifndef GRAINSTORM_BIQUAD_H
#define GRAINSTORM_BIQUAD_H

#include <atomic>
#include <complex>
#include "types.h"
#include "defines.h"
#include "tools.h"
#include "base.h"
#include <app.h>

enum {
    BIQUAD_RBJ_BW = 0, BIQUAD_RBJ_Q = 1, BIQUAD_RBJ_S = 2,
};

/*
int32_t tone(CSOUND *csound, TONE *p) {
    IGN(csound);
    MYFLT *ar, *asig;
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early = p->h.insdshead->ksmps_no_end;
    uint32_t n, nsmps = CS_KSMPS;
    MYFLOAT c1 = p->c1, c2 = p->c2;
    MYFLOAT yt1 = p->yt1;

    if (*p->khp != (MYFLT) p->prvhp) {
        MYFLOAT b;
        p->prvhp = (MYFLOAT) *p->khp;
        b = 2.0 - cos((MYFLOAT) (p->prvhp * csound->tpidsr));
        p->c2 = c2 = b -
        sqrt(b * b - 1.0);
        p->c1 = c1 = 1.0 - c2;
    }
    ar = p->ar;
    asig = p->asig;
    if (UNLIKELY(offset)) memset(ar, '\0', offset * sizeof(MYFLT));
    if (UNLIKELY(early)) {
        nsmps -= early;
        memset(&ar[nsmps], '\0', early * sizeof(MYFLT));
    }
    for (n = offset; n < nsmps; n++) {
        yt1 = c1 * (MYFLOAT) (asig[n]) + c2 * yt1;
        ar[n] = (MYFLT) yt1;
    }
    p->yt1 = yt1;
    return OK;
}
*/

template<typename T>
class biquad {
public:
    biquad() {
        a1 = a2 = b0 = b1 = b2 = 0;
        mute();
    }

    void mute() {
        i1 = i2 = o1 = o2 = t0 = t1 = t2 = 0;
    }

    void setCoefficients(T _b0, T _b1, T _b2, T _a1, T _a2) {
        b0 = _b0;
        b1 = _b1;
        b2 = _b2;
        a1 = _a1;
        a2 = _a2;
    }

/*
  Second-Order IIR Butterworth Filters
  The majority of the definitions and helper functions below have been
  derived from the source code of Steve Harris's SWH plugins.
  Biquad filter (adapted from lisp code by Eli Brandt, http://www.cs.cmu.edu/~eli/)
  See the Cookbook formulae for audio EQ biquad filter coefficients
  by Robert Bristow-Johnson <rbj@audioimagination.com> for more details.
*/

    static inline T BQ_LIMIT(T v, T l, T u) { return ((v) < (l) ? (l) : ((v) > (u) ? (u) : (v))); }

    void setAPF_RBJ(T fc, T bw, T fs, unsigned mode) {
        // fc : determines how steep the slope of the phase response is, when it passes through -180 degrees
        T omega = 2.0 * PI_P * fc / fs;
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        T a0r = 1.0 / (1.0 + alpha);
        b0 = a0r * (1.0 - alpha);
        b1 = a0r * (-2.0 * cs);
        b2 = a0r * (1.0 + alpha);
        a1 = a0r * (-2.0 * cs);
        a2 = a0r * (1.0 - alpha);
    }


    void setLPF_RBJ(T fc, T bw, T fs, unsigned mode) {
        T omega = 2.0 * PI_P * fc / fs;
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        T a0r = 1.0 / (1.0 + alpha);
        b0 = a0r * (1.0 - cs) * 0.5;
        b1 = a0r * (1.0 - cs);
        b2 = a0r * (1.0 - cs) * 0.5;
        a1 = a0r * (-2.0 * cs);
        a2 = a0r * (1.0 - alpha);
        /*
          d = Scalar, damping factor (default: square root of 2)
          if nargin < 3 d = sqrt(2); end
          beta = 0.5 * ( ( 1 - ( d / 2 ) * sin( 2 * pi * (fc / fs) ) ) / ( 1 + ( d / 2 ) * sin( 2 * pi * (fc / fs) ) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 0.5 + beta - gamma ) / 4;
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) + b(3)*x(n-2) - a(2)*y(n-1) - a(3)*y(n-2)
          b(1) = 2*alfa;
          b(2) = 4*alfa;
          b(3) = 2*alfa;
          a(1) = 1;
          a(2) = -2*gamma; % notice the sign!
          a(3) = 2*beta;   % notice the sign!
        */
    }


    void setHPF_RBJ(T fc, T bw, T fs, unsigned mode) {
        T omega = 2.0 * PI_P * fc / fs;
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        MYFLOAT a0r = 1.0 / (1.0 + alpha);
        b0 = a0r * (1.0 + cs) * 0.5;
        b1 = a0r * -(1.0 + cs);
        b2 = a0r * (1.0 + cs) * 0.5;
        a1 = -1.0 * a0r * (2.0 * cs);
        a2 = -1.0 * a0r * (alpha - 1.0);
        /*
          d = Scalar, damping factor (default: square root of 2)
          if nargin < 3 d = sqrt(2); end
          beta = 0.5 * ( ( 1 - ( d / 2 ) * sin( 2 * pi * (fc / fs) ) ) / ( 1 + ( d / 2 ) * sin( 2 * pi * (fc / fs) ) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 0.5 + beta + gamma ) / 4;
          a = zeros(1,3);
          b = zeros(1,3);
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) + b(3)*x(n-2) - a(2)*y(n-1) - a(3)*y(n-2)
          b(1) = 2*alfa;
          b(2) = -4*alfa;
          b(3) = 2*alfa;
          a(1) = 1;
          a(2) = -2*gamma; % notice the sign!
          a(3) = 2*beta;   % notice the sign!
        */
    }

    void setBPF_RBJ(T fc, T bw, T fs, unsigned mode) {
        // constant 0 dB peak gain
        T omega = 2.0 * PI_P * fc / fs;
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        T a0r = 1.0 / (1.0 + alpha);
        b0 = a0r * alpha;
        b1 = 0;
        b2 = a0r * (-1.0 * alpha);
        a1 = a0r * (-2.0 * cs);
        a2 = a0r * (1.0 - alpha);
        /*
          Q = Scalar, quality factor (default: 1)
          if nargin < 3 Q = 1; end
          beta = 0.5 * ( ( 1 - tan((2 * pi * (fc / fs)) / (2 * Q)) ) / ( 1 + tan((2 * pi * (fc / fs)) / (2 * Q)) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 0.5 - beta ) / 2;
          a = zeros(1,3);
          b = zeros(1,3);
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) + b(3)*x(n-2) - a(2)*y(n-1) - a(3)*y(n-2)
          b(1) = 2*alfa;
          b(2) = 0;
          b(3) = -2*alfa;
          a(1) = 1;
          a(2) = -2*gamma; % notice the sign!
          a(3) = 2*beta;   % notice the sign!
         */
    }


    void setBPFP_RBJ(T fc, T bw, T fs, unsigned mode) {
        // constant skirt gain, peak gain = Q
        T omega = 2.0 * PI_P * fc / fs;
        T sn = std::sin(omega);
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        T a0r = 1.0 / (1.0 + alpha);
        b0 = a0r * (0.5 * sn);
        b1 = 0;
        b2 = a0r * (-0.5 * sn);
        a1 = a0r * (-2.0 * cs);
        a2 = a0r * (1.0 - alpha);
        /*
          Second-Order IIR Butterworth Peaking Filter
          Q = Scalar, quality factor (default: 1)
          g = Scalar, boost/cut gain (in dB)
          mu = Scalar, bandpass output scaling factor
          if nargin < 4 Q = 1; end
          mu = 10^(g/20);
          beta = 0.5 * ( ( 1 - (4 / (1 + mu)) * tan((2 * pi * (fc / fs)) / (2 * Q)) ) / ( 1 + (4 / (1 + mu)) * tan((2 * pi * (fc / fs)) / (2 * Q)) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 0.5 - beta ) / 2;
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) + b(3)*x(n-2) - a(2)*y(n-1) - a(3)*y(n-2)
          b(1) = 2*alfa;
          b(2) = 0;
          b(3) = -2*alfa;
          a(1) = 1;
          a(2) = 2*gamma;
          a(3) = -2*beta;
         */
    }


    void setBSF_RBJ(T fc, T bw, T fs, unsigned mode) {
        T omega = 2.0 * PI_P * fc / fs;
        //T sn = std::sin(omega);
        T cs = std::cos(omega);
        T alpha = calcAlpha(fc, bw, fs, mode);
        T a0r = 1.0 / (1.0 + alpha);
        b0 = a0r;
        b1 = a0r * (-2.0 * cs);
        b2 = a0r;
        a1 = a0r * (-2.0 * cs);
        a2 = a0r * (1.0 - alpha);
        /*
          Second-Order IIR Butterworth Band-Stop Filter
          Q = Scalar, quality factor (default: 1)
          if nargin < 3 Q = 1; end
          beta = 0.5 * ( ( 1 - tan((2 * pi * (fc / fs)) / (2 * Q)) ) / ( 1 + tan((2 * pi * (fc / fs)) / (2 * Q)) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 0.5 + beta ) / 2;
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) + b(3)*x(n-2) - a(2)*y(n-1) - a(3)*y(n-2)
          b(1) = 2*alfa;
          b(2) = -2*gamma;
          b(3) = 2*alfa;
          a(1) = 1;
          a(2) = -2*gamma; % notice the sign!
          a(3) = 2*beta;   % notice the sign!
        */
    }


    void setPeakEQ_RBJ(T fc, T gain, T bw, T fs) {
        T w = 2.0 * PI_P * BQ_LIMIT(fc, 1.0, fs / 2.0) / fs;
        T cw = std::cos(w);
        T sw = std::sin(w);
        T J1 = std::pow(10.0, gain * 0.025);
        T g = sw * std::sinh(LN_2_2 * BQ_LIMIT(bw, 0.0001, 4.0) * w / sw);
        T a0r = 1.0 / (1.0 + (g / J1));
        b0 = (1.0 + (g * J1)) * a0r;
        b1 = (-2.0 * cw) * a0r;
        b2 = (1.0 - (g * J1)) * a0r;
        a1 = b1;
        a2 = -1.0 * ((g / J1) - 1.0) * a0r;
    }

    void setLSF_RBJ(T fc, T gain, T slope, T fs) {
        T w = 2.0 * PI_P * BQ_LIMIT(fc, 1.0, fs / 2.0) / fs;
        T cw = std::cos(w);
        T sw = std::sin(w);
        T A = std::pow((T) 10.0, gain * (T) 0.025);
        T b = std::sqrt(((1.0 + A * A) / BQ_LIMIT(slope, 0.0001, 1.0)) - ((A - 1.0) * (A - 1.0)));
        T apc = cw * (A + 1.0);
        T amc = cw * (A - 1.0);
        T bs = b * sw;
        T a0r = 1.0 / (A + 1.0 + amc + bs);
        b0 = a0r * A * (A + 1.0 - amc + bs);
        b1 = a0r * 2.0 * A * (A - 1.0 - apc);
        b2 = a0r * A * (A + 1.0f - amc - bs);
        a1 = -1.0 * a0r * 2.0 * (A - 1.0 + apc);
        a2 = -1.0 * a0r * (-A - 1.0 - amc + bs);
    }


    void setHSF_RBJ(T fc, T gain, T slope, T fs) {
        T w = 2.0 * PI_P * BQ_LIMIT(fc, 1.0, fs / 2.0) / fs;
        T cw = std::cos(w);
        T sw = std::sin(w);
        T A = std::pow((T) 10.0, gain * (T) 0.025);
        T b = std::sqrt(((1.0 + A * A) / BQ_LIMIT(slope, 0.0001, 1.0)) - ((A - 1.0) * (A - 1.0)));
        T apc = cw * (A + 1.0);
        T amc = cw * (A - 1.0);
        T bs = b * sw;
        T a0r = 1.0f / (A + 1.0 - amc + bs);
        b0 = a0r * A * (A + 1.0 + amc + bs);
        b1 = a0r * -2.0 * A * (A - 1.0 + apc);
        b2 = a0r * A * (A + 1.0 + amc - bs);
        a1 = -1.0 * a0r * -2.0 * (A - 1.0 - apc);
        a2 = -1.0 * a0r * (-A - 1.0 + amc + bs);
    }

    void printconfig() {
        LOGD("<< BiQuad Filter Coefficients >>\n");
        LOGD("(in)--+----*b0-->+----------+->(out) \n");
        LOGD("      |          ^          |        \n");
        LOGD("      v          |          v        \n");
        LOGD("  [z^-1]---*b1-->+<-*(-a1)-[z^-1]    \n");
        LOGD("      |          ^          |        \n");
        LOGD("      v          |          v        \n");
        LOGD("  [z^-1]---*b2-->+<-*(-a2)-[z^-1]    \n\n");
        LOGD("b0 = %1.8f, b1 = %1.8f, b2 = %1.8f\n", (MYFLOAT) b0, (MYFLOAT) b1, (MYFLOAT) b2);
        LOGD("a1 = %1.8f, a2 = %1.8f\n\n", (MYFLOAT) a1, (MYFLOAT) a2);
    }

    inline T process(T input) {
        return this->processd1(input);
    }

    inline T operator()(T input) { return this->process(input); }

    // Direct form I
    inline T processd1(T input) {
        T i0 = input;
        input *= b0;
        input += b1 * i1 + b2 * i2;
        input -= a1 * o1 + a2 * o2;
        if(isDouble){
           UDD(input)
        }
        else{
            UDF(input)
        }
        i2 = i1;
        i1 = i0;
        o2 = o1;
        o1 = input;
        return input;
    }

    // Direct form II
    inline T processd2(T input) {
        input -= a1 * t1 + a2 * t2;
        t0 = input;
        input *= b0;
        input += b1 * t1 + b2 * t2;
        UNDENORMAL(input);
        t2 = t1;
        t1 = t0;
        return input;
    }

private:
    const bool isDouble = sizeof(T) == sizeof(MYFLOAT);
    biquad<T>(const biquad<T> &x);

    biquad<T> &operator=(const biquad<T> &x);

    T calcAlpha(T fc, T bw, T fs, unsigned mode) {
        T omega = 2.0 * PI_P * fc / fs;
        T sn = std::sin(omega);
        switch (mode) {
            case BIQUAD_RBJ_BW:
                return 0;// sn* std::sinh(std::log2 / 2.0 * bw * omega / sn);
            case BIQUAD_RBJ_Q:
                return sn * (2. * bw);
            case BIQUAD_RBJ_S:
            default:
                break;
        }
        return 0;
    }

    T a1, a2, b0, b1, b2;
    T i1, i2, o1, o2, t0, t1, t2;

};

template<typename T>
class iir_1st {
public:

    iir_1st() {
        mute();
    }

    void mute() {
        y1 = 0;
    }

    void printconfig() {
        std::fprintf(stderr, "<< 1st order IIR Filter Coefficients >>\n");
        std::fprintf(stderr, "(in)--+----*b1-->+----------+->(out) \n");
        std::fprintf(stderr, "      |          ^          |        \n");
        std::fprintf(stderr, "      v          |          v        \n");
        std::fprintf(stderr, "  [z^-1]---*b2-->+<--*a2---[z^-1]    \n");
        std::fprintf(stderr, "b1 = %f, b2 = %f\n", b1, b2);
        std::fprintf(stderr, "a1 = 1, a2 = %f\n", a2);
    }

    void setCoefficients(T _b1, T _b2, T _a2) {
        b1 = _b1;
        b2 = _b2;
        a2 = _a2;
    }

/*
  First-Order IIR Butterworth Low-Pass Filter
*/

    void setLPF_BW(T fc, T fs) {
        T omega_2 = PI_P * fc / fs;
        T tan_omega_2 = std::tan(omega_2);
        b1 = b2 = tan_omega_2 / (1 + tan_omega_2);
        a2 = (1 - tan_omega_2) / (1 + tan_omega_2);
        /*
          AudioFilteringToolkit
          John Lane, Jayant Datta, Brent Karley, Jay Norwood, "DSP Filters",
          PROMPT Publications(an imprint32_t of Sams Technical Publishing),
          Indianapolis, IN, 2001, page 43. Book's website: www.dspaudiocookbook.com

          beta = 0.5 * ( ( 1 - sin( 2 * pi * (fc / fs) ) ) / ( 1 + sin( 2 * pi * (fc / fs) ) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 1 - gamma ) / 2;
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) - a(2)*y(n-1)
          a(1) = 1;
          a(2) = -gamma; % notice the sign!
          b(1) = alfa;
          b(2) = alfa;
        */
    }

    void setHPF_BW(T fc, T fs) {
        T omega_2 = PI_P * fc / fs;
        T tan_omega_2 = std::tan(omega_2);
        b1 = 1 / (1 + tan_omega_2);
        b2 = -1 * b1;
        a2 = (1 - tan_omega_2) / (1 + tan_omega_2);
        /*
          beta = 0.5 * ( ( 1 - sin( 2 * pi * (fc / fs) ) ) / ( 1 + sin( 2 * pi * (fc / fs) ) ) );
          gamma = ( 0.5 + beta ) * cos ( 2 * pi * (fc / fs) );
          alfa = ( 1 + gamma ) / 2;
          a = zeros(1,2);
          b = zeros(1,2);
          a(1)*y(n) = b(1)*x(n) + b(2)*x(n-1) - a(2)*y(n-1)
          a(1) = 1;
          a(2) = -gamma; % notice the sign!
          b(1) = alfa;
          b(2) = -alfa;
        */
    }

/*
  The functions below have been derived from the book of An Audio Cookbook by Christopher Moore.
  First Order Digital Filters--An Audio Cookbook. Application Note AN-11 by Christopher Moore
  http://www.sevenwoodsaudio.com/AN11.pdf
*/

    void setLPF_A(T fc, T fs) {
        a2 = std::exp(-1 * PI_P * fc / (fs / 2.));
        b1 = 1.;
        b2 = .12;
        T norm = (1 - a2) / (b1 + b2);
        b1 *= norm;
        b2 *= norm;
    }

    void setHPF_A(T fc, T fs) {
        a2 = std::exp(-1 * PI_P * fc / (fs / 2.));
        b1 = 1.;
        b2 = -1.;
        T norm = (1 + a2) / 2.;
        b1 *= norm;
        b2 *= norm;
    }

    void setLSF_A(T f1, T f2, T fs) {
        a2 = -1 * std::exp(-1 * PI_P * f1 / (fs / 2.));
        b1 = -1.;
        b2 = std::exp(-1 * PI_P * f2 / (fs / 2.));
    }


    void setHSF_A(T f1, T f2, T fs) {
        a2 = std::exp(-1 * PI_P * f1 / (fs / 2.));
        b1 = -1.;
        b2 = std::exp(-1 * PI_P * f2 / (fs / 2.));
        T norm = (1 - a2) / (b1 + b2);
        b1 *= norm;
        b2 *= norm;
    }

    void setHPFwLFS_A(T fc, T fs) {
        b1 = -1.;
        b2 = std::exp(-1 * PI_P * fc / (fs / 2.));
        a2 = -.12;
        T norm = (1 - a2) / std::abs(b1 + b2);
        b1 *= norm;
        b2 *= norm;
    }

    void setLPF_C(T fc, T fs) {
        b1 = b2 = fc / (fs + fc);
        a2 = (fs - fc) / (fs + fc);
    }

    void setHPF_C(T fc, T fs) {
        b1 = fs / (fs + fc);
        b2 = -1 * b1;
        a2 = (fs - fc) / (fs + fc);
    }

    void setPole(T v) {
        a2 = v;
        b1 = 1;
        b2 = 0;
        T norm = 1. - std::abs(a2);
        b1 *= norm;
        b2 *= norm;
    }

    void setZero(T v) {
        a2 = 0;
        b1 = -1.;
        b2 = v;
        T norm = std::abs(b1) + std::abs(b2);
        b1 *= norm;
        b2 *= norm;
    }

    void setPoleLPF(T fc, T fs) {
        T omega = 2. * PI_P * fc / fs;
        T cos_omega = std::cos(omega);
        T coeff = (2 - cos_omega) - std::sqrt((2 - cos_omega) * (2 - cos_omega) - 1);
        a2 = coeff;
        b1 = 1 - coeff;
        b2 = 0;
    }

    void setPoleHPF(T fc, T fs) {
        T omega = 2. * PI_P * fc / fs;
        T cos_omega = std::cos(omega);
        T coeff = (2 + cos_omega) - std::sqrt((2 + cos_omega) * (2 + cos_omega) - 1);
        a2 = -1 * coeff;
        b1 = coeff - 1;
        b2 = 0;
    }

    void setZeroLPF(T fc, T fs) {
        // fc > fs/4
        T omega = 2. * PI_P * fc / fs;
        T cos_omega = std::cos(omega);
        T coeff = (1 - 2 * cos_omega) - std::sqrt((1 - 2 * cos_omega) * (1 - 2 * cos_omega) - 1);
        a2 = 0;
        b1 = 1 / (1 + coeff);
        b2 = coeff / (1 + coeff);
    }

    void setZeroHPF(T fc, T fs) {
        // fc < fs/4
        T omega = 2. * PI_P * fc / fs;
        T cos_omega = std::cos(omega);
        T coeff = (1 + 2 * cos_omega) - std::sqrt((1 + 2 * cos_omega) * (1 + 2 * cos_omega) - 1);
        a2 = 0;
        b1 = 1 / (1 + coeff);
        b2 = -1 * coeff / (1 + coeff);
    }


    inline T process(T input) {
        return this->processd1(input);
    }

    inline T operator()(T input) { return this->process(input); }

    // Direct form I
    inline T processd1(T input) {
        T output = input * b1 + y1;
        UDD(output)
        y1 = output * a2 + input * b2;
        UDD(y1)
        return output;
    }

private:
    iir_1st<T>(const iir_1st<T> &x);
    iir_1st<T> &operator=(const iir_1st<T> &x);
    T a2, b1, b2, y1;
};



class CParamSmooth
{
public:
    CParamSmooth(MYFLOAT sr) {
        const MYFLOAT rc = 0.0033;// fc = 48, rc = 1/(2*pi * fc)
        _coeff = 1.f / (rc * sr + 1); };
    inline MYFLOAT Process(MYFLOAT in) {
        _smoothed += _coeff * (in - _smoothed);
    return _smoothed;
    }
private:
    MYFLOAT _coeff{}, _smoothed{.5f};
};


// A 0..1 SMOOTH control mapped onto the -60..0 dB cutoff span Tone::Setup below
// actually takes, REVERSED, so the knob rises the way its name reads: 0 is none,
// 1 is most. These parameters used to BE that cutoff in dB and so ran backwards
// -- their minimum smoothed hardest. Every one of them defaulted to -30 dB,
// which is exactly where 0.5 lands, so a 0.5 default is the old sound unchanged.
// Lives here rather than beside LOG2NORMALF in defines.h: that header reaches
// translation units that never include <cmath>, and unlike the macros around it
// a function body needs pow() declared at the point it is defined.
inline MYFLOAT SMOOTH2POLE(MYFLOAT smooth) {
    if (smooth < 0.) smooth = 0.;
    else if (smooth > 1.) smooth = 1.;
    return LOG2NORMALF(-60. * smooth);
}

class Tone {
public:
    Tone(MYFLOAT sr, std::atomic<MYFLOAT> *_hp = nullptr, bool _logarithmic = false) {
       init(sr, _hp, _logarithmic);
    }

    Tone(){};

    void init(MYFLOAT sr, std::atomic<MYFLOAT> *_hp = nullptr, bool _logarithmic = false) {
        logarithmic = _logarithmic;
        twopidr = TWOPI_P / sr;
        hp = _hp;
        srd2 = sr * .5;
        oldhp = -1;
        c1 = c2 = 0;
        Reset();
    }

    void Reset() {
        yt1 = 0.0;
    }

    void Setup(MYFLOAT f = -1) {
        if (f == -1 && hp != nullptr)
            f = hp->load();
        oldhp = f;
        MYFLOAT fr = (logarithmic ? pow(10., f * .05) : f) * srd2;
        MYFLOAT b = 2.0 - cos(fr * twopidr);
        c2 = b - sqrt(b * b - 1.0);
        c1 = 1.0 - c2;
    }

    void compute(MYFLOAT *in, int32_t size) {
        if (hp != nullptr && hp->load() != oldhp)
            Setup(-1.);
        for (int32_t i = 0; i < size; i++) {
            yt1 = c1 * (MYFLOAT) in[i] + c2 * yt1;
            in[i] = (MYFLOAT) yt1;
        }
    }

    inline MYFLOAT tick(MYFLOAT in) {
        if (hp != nullptr && hp->load() != oldhp)
            Setup(-1.);
        yt1 = c1 * (MYFLOAT) in + c2 * yt1;
        return (MYFLOAT) yt1;
    }

    inline MYFLOAT tickn(MYFLOAT in) {
        yt1 = c1 * (MYFLOAT) in + c2 * yt1;
        return (MYFLOAT) yt1;
    }

    void SetState(MYFLOAT x) {
        yt1 = (x - c1 * x) / c2;
    }

    std::atomic<MYFLOAT> *hp;
    MYFLOAT oldhp;
    MYFLOAT twopidr;
    bool logarithmic;
    MYFLOAT c1, c2, yt1;
    MYFLOAT srd2;
};

class Reson {
public:
    Reson(int32_t sr, std::atomic<MYFLOAT> *_center, std::atomic<MYFLOAT> *_bw,
          std::atomic<MYFLOAT> *_gain,
          std::atomic<MYFLOAT> *_mix, bool _logarithmic = false) {
        Reset();
        pidsr = PI_P / (MYFLOAT) sr;
        center = _center;
        q = _bw;
        mix = _mix;
        gain = _gain;
        logarithmic = _logarithmic;
        if (center != nullptr && q != nullptr) {
            center_saved = center->load();
            q_saved = q->load();
            centerold = logarithmic ? pow(10, center->load() * .05) : center->load();
            qold = q->load();
            Setup();
        }
    };

    std::atomic<MYFLOAT> *center, *q, *gain, *mix;
    std::atomic<MYFLOAT> *centermin, *centermax, *qmin, *qmax, *gaingrain, *mixgrain;

    MYFLOAT centerold, qold, center_saved, q_saved, a0, b1, b2, y11, y21;
    bool logarithmic;
    std::atomic<bool> *bypass;
    MYFLOAT pidsr;

    void Setup(MYFLOAT f = -1, MYFLOAT bw = -1) {
        if (f != -1) {
            centerold = logarithmic ? pow(10, f * .05) : f;
        }
        if (bw != -1) {
            qold = logarithmic ? pow(10, bw * .05) : bw;
        }
        MYFLOAT pfreq = centerold * pidsr;
        MYFLOAT B = pfreq * qold;
        MYFLOAT R = 1. - B * 0.5;
        MYFLOAT twoR = 2. * R;
        MYFLOAT R2 = R * R;
        MYFLOAT cost = (twoR * cos(centerold)) / (1. + R2);
        b1 = twoR * cost;
        b2 = -R2;
        a0 = (1. - R2) * 0.5;
    }

    void compute(MYFLOAT *in, MYFLOAT *out, uint32_t size);

    void Reset() {
        a0 = b1 = b2 = y11 = y21 = 0.0;
    };


    MYFLOAT Tick(MYFLOAT in);


    static Reson *
    init(int32_t sr, std::atomic<MYFLOAT> *center, std::atomic<MYFLOAT> *bw,
         std::atomic<MYFLOAT> *mix,
         std::atomic<MYFLOAT> *gain, std::atomic<bool> *bypass) {
        Reson *b = new Reson(sr, center, bw, gain, mix, true);
        b->bypass = bypass;
        return b;
    }

    static Reson *
    initGrain(int32_t sr, std::atomic<MYFLOAT> *centermin, std::atomic<MYFLOAT> *centermax,
              std::atomic<MYFLOAT> *qmin, std::atomic<MYFLOAT> *qmax,
              std::atomic<MYFLOAT> *mix,
              std::atomic<MYFLOAT> *gain, std::atomic<bool> *bypass);

    static void ComputeGrain(Reson *xx, MYFLOAT *in, uint32_t size);

    static void Compute(Reson *xx, MYFLOAT *in, uint32_t size) {
        if (xx->bypass->load())
            return;
        xx->compute(in, in, size);
    }
};

#define DCBLOCK2ORDER 128
#define DELAY1SIZE (DCBLOCK2ORDER - 1) * 2
#define dc2scale 1./(MYFLOAT) DCBLOCK2ORDER

template<typename T>
class DCBlock2 {
public:
    void compute(T *in, int32_t size) {
        for (int32_t i = 0; i < size; i++) {

            /* long delay */
            MYFLOAT del = delay1[p1];
            MYFLOAT x1 = delay1[p1] = (MYFLOAT) in[i];

            /* IIR cascade */
            for (int32_t j = 0; j < 4; j++) {
                MYFLOAT x2 = iirdelay[j][p2];
                iirdelay[j][p2] = x1;
                MYFLOAT y = x1 - x2 + ydels[j];
                ydels[j] = y;
                x1 = y * dc2scale;
            }
            in[i] = (T) (del - x1);

            p1 = (p1 == DELAY1SIZE - 1 ? 0 : p1 + 1);
            p2 = (p2 == DCBLOCK2ORDER - 1 ? 0 : p2 + 1);
        }
    }

    inline T tick(T in) {

        /* long delay */
        MYFLOAT del = delay1[p1];
        MYFLOAT x1 = delay1[p1] = (MYFLOAT) in;

        /* IIR cascade */
        for (int32_t j = 0; j < 4; j++) {
            MYFLOAT x2 = iirdelay[j][p2];
            iirdelay[j][p2] = x1;
            MYFLOAT y = x1 - x2 + ydels[j];
            ydels[j] = y;
            x1 = y * dc2scale;
        }

        p1 = (p1 == DELAY1SIZE - 1 ? 0 : p1 + 1);
        p2 = (p2 == DCBLOCK2ORDER - 1 ? 0 : p2 + 1);
        return (T) (del - x1);

    }

private:
    MYFLOAT ydels[4]{};
    MYFLOAT delay1[DELAY1SIZE]{};
    MYFLOAT iirdelay[4][DCBLOCK2ORDER]{};
    int32_t p1{}, p2{};
};

class TwoZero {
public:
    
    //! Set the b[0] coefficient value.
    void setB0(MYFLOAT b0) { b_[0] = b0; };

    //! Set the b[1] coefficient value.
    void setB1(MYFLOAT b1) { b_[1] = b1; };

    //! Set the b[2] coefficient value.
    void setB2(MYFLOAT b2) { b_[2] = b2; };

    //! Set all filter coefficients.
    void setCoefficients(MYFLOAT b0, MYFLOAT b1, MYFLOAT b2, bool clearState = false) {
        b_[0] = b0;
        b_[1] = b1;
        b_[2] = b2;

        if (clearState) this->clear();
    }

    void clear() {
        inputs_[0] = inputs_[1] = inputs_[2] = lastFrame_ = 0.0;
    }
    //! Sets the filter coefficients for a "notch" at \e frequency (in Hz).
    /*!
      This method determines the filter coefficients corresponding to
      two complex-conjugate zeros with the given \e frequency (in Hz)
      and \e radius from the z-plane origin.  The coefficients are then
      normalized to produce a maximum filter gain of one (independent of
      the filter \e gain parameter).  The resulting filter frequency
      response has a "notch" or anti-resonance at the given \e
      frequency.  The closer the zeros are to the unit-circle (\e radius
      close to or equal to one), the narrower the resulting notch width.
      The \e frequency value should be between zero and half the sample
      rate.  The \e radius value should be positive.
    */
    void setNotch(MYFLOAT sr, MYFLOAT frequency, MYFLOAT radius) {

        b_[2] = radius * radius;
        b_[1] = -2.0 * radius * cos(TWOPI_F_P * frequency / sr);

        // Normalize the filter gain.
        if (b_[1] > 0.0) // Maximum at z = 0.
            b_[0] = 1.0 / (1.0 + b_[1] + b_[2]);
        else            // Maximum at z = -1.
            b_[0] = 1.0 / (1.0 - b_[1] + b_[2]);
        b_[1] *= b_[0];
        b_[2] *= b_[0];
    }

    //! Return the last computed output value.
    inline MYFLOAT lastOut(void) const { return lastFrame_; };

    //! Input one sample to the filter and return one output.
    inline MYFLOAT tick(MYFLOAT input) {
        inputs_[0] = gain_ * input;
        lastFrame_ = b_[2] * inputs_[2] + b_[1] * inputs_[1] + b_[0] * inputs_[0];
        inputs_[2] = inputs_[1];
        inputs_[1] = inputs_[0];

        return lastFrame_;
    }

    void setGain(MYFLOAT gain) {
        gain_ = gain;
    }

protected:
    MYFLOAT gain_{1};
    MYFLOAT b_[3]{1., 0., 0.}, inputs_[3]{}, lastFrame_{};
};

#include "Butterworth.h"
class ButterLPHP : public Effect {
public:
    ButterLPHP(TRACK *t, int32_t chan);

    void compute(MYFLOAT *in, int32_t size)override ;

private:
    Butterworth<MYFLOAT> filter;
    std::vector<MYFLOAT> helpbuf;
};

class ButterBR : public Effect {
public:
    ButterBR(TRACK *t, int32_t chan);

    void compute(MYFLOAT *in, int32_t size)override ;

private:
    Butterworth<MYFLOAT> filter;
    std::vector<MYFLOAT> helpbuf;
};

#endif //GRAINSTORM_BIQUAD_H
