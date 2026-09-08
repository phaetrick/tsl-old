// October 8, 2013
// From: http://www.iowahills.com/A7ExampleCodePage.html
// If you find a problem with this code, please leave us a note on:
// http://www.iowahills.com/feedbackcomments.html

// This code calculates the window values for Hanning, Hamming, Blackman,
// Blackman Harris, Blackman Nuttall, Nuttall,  Kaiser, Kaiser Bessel,
// Cosine,  Sinc, Flattop, Flattop 2,  Dolph Chebyshev, Tukey, and Trapezoid.
// The Trapezoid is adjustable from a triangle to a rectangle.
// Most of these windows are defined on Wikipedia.
// http://en.wikipedia.org/wiki/Window_function
// To apply a window to N pts of data, simply calculate an N pt window, then
// WindowedData[n] = Data[n] * Window[n];

// Some of the window calculations require these math functions.
//---------------------------------------------------------------------------
#include "envelope.h"
#include <type_traits>

#ifndef M_PI
#define M_PI 3.14159265358979323846  
#endif
static constexpr double e{2.718281828459045};

namespace tsl::envelope {
    template<>
    ComputeFn<float> compute<float>[4] = {
            genPoly<float>,
            genLinear<float>,
            genRect<float>,
            genSpline<float>
    };

    template<>
    ComputeFn<MYFLOAT> compute<MYFLOAT>[4] = {
            genPoly<MYFLOAT>,
            genLinear<MYFLOAT>,
            genRect<MYFLOAT>,
            genSpline<MYFLOAT>
    };

    template<>
    GenEnvPhaseFn<float> genEnvPhase<float>[4] = {
            genPolyPhase<float>,
            genLinearPhase<float>,
            genRectPhase<float>,
            genSplinePhase<float>
    };

    template<>
    GenEnvPhaseFn<MYFLOAT> genEnvPhase<MYFLOAT>[4] = {
            genPolyPhase<MYFLOAT>,
            genLinearPhase<MYFLOAT>,
            genRectPhase<MYFLOAT>,
            genSplinePhase<MYFLOAT>
    };

}

template<typename T>
void
tsl::envelope::genSpline(T *fp, int tablesize, T positions[], T values[],
                         int nsegs, bool adjust) {
    T f1, f0, dx01, curx, df0;
    int npts;

    T vals[64];//(nsegs * 2 + 1);
    for (int i = 0; i < nsegs; i++) {
        vals[i * 2] = *values++;

        vals[i * 2 + 1] = tablesize * (positions[i + 1] - positions[i]);
        if (vals[i * 2 + 1] <= 0.0)
            LOGE("gen08 error");
    }
    vals[nsegs * 2] = *values;
    T *valp = &vals[0];

    //if (UNLIKELY((nsegs = (ff->e.pcnt - 5) >> 1) <= 0)) {
    //    return fterror(ff, Str("insufficient arguments"));
    //}
    auto fplim = fp + tablesize;
    f0 = *valp++;                    /* 1st 3 params give vals at x0, x1 */
    if ((dx01 = *valp++) <= 0.0) {      /* and dist between*/
        LOGE("Invalid distance");
        return;
    }
    f1 = *valp++;
    curx = df0 = 0.0;           /* init x to origin; slope at x0 = 0 */
    do {                            /* for each spline segmnt (x0 to x1) */
        MYFLOAT dx12{}, f2{}, df1;
        if (nsegs > 1) {                      /* if another seg to follow  */
            if ((dx12 = *valp++) <= 0.0) {  /*  read its distance  */
                LOGE("Invalid distance");
                return;
            }
            f2 = *valp++;                       /*    and the value at x2    */
            T dx02 = dx01 + dx12;
            df1 = (f2 * dx01 * dx01 + f1 * (dx12 - dx01) * dx02 - f0 * dx12 * dx12)
                  / (dx01 * dx02 * dx12);
        }                                /* df1 is slope of parabola at x1 */
        else df1 = 0.0;
        if ((npts = (int) (dx01 - curx)) > fplim - fp)
            npts = fplim - fp;
        if (npts > 0) {                       /* for non-trivial segment: */
            auto slope = (f1 - f0) / dx01;           /*   get slope x0 to x1     */
            auto resd0 = df0 - slope;                /*   then residual slope    */
            auto resd1 = df1 - slope;                /*     at x0 and x1         */
            auto c3 = (resd0 + resd1) / (dx01 * dx01);
            auto c2 = -(resd1 + 2.0 * resd0) / dx01;
            auto c1 = df0;                           /*   and calc cubic coefs   */
            auto c0 = f0;
            MYFLOAT x = curx;
            for (; npts > 0; --npts) {
                auto R = c3;
                R *= x;
                R += c2;            /* f(x) = ((c3 x + c2) x + c1) x + c0  */
                R *= x;
                R += c1;
                R *= x;
                R += c0;
                *fp++ = adjust ? (R < 0.0 ? 0.0 : (R > 1.0 ? 1.0 : R))
                               : R;                        /* store n pts for this seg */
                x += 1.0;
            }
            curx = x;
        }
        curx -= dx01;                 /* back up x by length last segment */
        dx01 = dx12;                     /* relocate to the next segment */
        f0 = f1;                       /*   by assuming its parameters */
        f1 = f2;
        df0 = df1;
    } while (--nsegs && fp < fplim);      /* loop for remaining segments  */
    while (fp < fplim)
        *fp++ = f0;                       /* & repeat the last value      */
}

template<typename T>
T tsl::envelope::genSplinePhase(
        T phase,
        T positions[],
        T values[],
        int nsegs,
        bool adjust) {
    if (phase <= positions[0])
        return values[0];

    if (phase >= positions[nsegs])
        return values[nsegs];

    // Find segment
    int i = 0;
    for (; i < nsegs; ++i)
        if (phase < positions[i + 1])
            break;

    T f0 = values[i];
    T f1 = values[i + 1];

    T x0 = positions[i];
    T x1 = positions[i + 1];
    T dx01 = x1 - x0;

    // ----- Compute slope at x0 (df0) -----
    T df0;
    if (i == 0) {
        df0 = static_cast<T>(0); // endpoint rule
    } else {
        T xm1 = positions[i - 1];
        T fm1 = values[i - 1];
        T dx_1 = x0 - xm1;
        T dx02 = dx_1 + dx01;

        df0 = (f1 * dx_1 * dx_1 +
               f0 * (dx01 - dx_1) * dx02 -
               fm1 * dx01 * dx01)
              / (dx_1 * dx02 * dx01);
    }

    // ----- Compute slope at x1 (df1) -----
    T df1;
    if (i >= nsegs - 1) {
        df1 = static_cast<T>(0);
    } else {
        T x2 = positions[i + 2];
        T f2 = values[i + 2];
        T dx12 = x2 - x1;
        T dx02 = dx01 + dx12;

        df1 = (f2 * dx01 * dx01 +
               f1 * (dx12 - dx01) * dx02 -
               f0 * dx12 * dx12)
              / (dx01 * dx02 * dx12);
    }

    // Normalize phase inside segment
    T t = (phase - x0) / dx01;

    // Hermite cubic basis
    T t2 = t * t;
    T t3 = t2 * t;

    T h00 = 2 * t3 - 3 * t2 + 1;
    T h10 = t3 - 2 * t2 + t;
    T h01 = -2 * t3 + 3 * t2;
    T h11 = t3 - t2;

    T out = h00 * f0 + h10 * dx01 * df0
            + h01 * f1 + h11 * dx01 * df1;

    if (adjust) {
        if (out < static_cast<T>(0)) out = static_cast<T>(0);
        else if (out > static_cast<T>(1)) out = static_cast<T>(1);
    }

    return out;
}

template<typename T>
void
tsl::envelope::genPoly(T *fp, int tablesize, T positions[], T values[],
                       int nsegs, bool limit) {
    T *end = fp + tablesize;;
    T y, diffd2, vala;
    int pntno, npts;

    T extreme, inflect, a, b;
    for (int seg = 0; seg < nsegs; seg++) {
        T length = tablesize * (positions[seg + 1] - positions[seg]) * .5;
        if ((npts = (int) length) < 0) {
            return;
        }
        a = values[seg];
        b = values[seg + 1];
        extreme = a;
        inflect = (a + b) * .5;
        pntno = 0;
        diffd2 = (inflect - extreme) * (0.5);
        for (; npts > 0 && fp < end; pntno++, npts--) {
            y = (T) pntno / length;
            *fp++ = ((3.0) - y) * y * y * diffd2 + extreme;
        }
        extreme = b;
        npts = (int) length;
        pntno = npts;
        diffd2 = (inflect - extreme) * (0.5);
        for (; npts > 0 && fp < end; pntno--, npts--) {
            y = (T) pntno / length;
            vala = ((3.0) - y) * y * y * diffd2 + extreme;
            vala = limit ? (vala > 1.0 ? 1.0 : (vala < 0.0 ? 0.0 : vala)) : vala;
            *fp++ = vala;
        }
    }
    while (fp < end)                 /* if 2**n pnts, add guardpt */
        *fp++ = vala;
}

template<typename T>
T tsl::envelope::genPolyPhase(
        T phase,
        T positions[],
        T values[],
        int nsegs,
        bool limit) {
    if (phase <= positions[0])
        return values[0];
    if (phase >= positions[nsegs])
        return values[nsegs];

    int seg = 0;
    while (seg < nsegs && phase > positions[seg + 1])
        seg++;

    T p0 = positions[seg];
    T p1 = positions[seg + 1];
    T a = values[seg];
    T b = values[seg + 1];

    T seglen = p1 - p0;
    if (seglen <= T(0))
        return a;

    // t in [0..1] across full segment
    T t = (phase - p0) / seglen;

    T inflect = (a + b) * T(0.5);
    T extreme, diffd2, y;

    if (t < T(0.5)) {
        // First half: mirrors table's first loop
        // pntno goes 0..npts, y = pntno/length, so y goes 0→1
        extreme = a;
        diffd2 = (inflect - extreme) * T(0.5);
        y = t * T(2);          // remap [0, 0.5) → [0, 1)
    } else {
        // Second half: mirrors table's second loop
        // pntno counts DOWN from npts→0, so y = pntno/length goes 1→0
        // meaning it also goes extreme→inflect, same shape
        extreme = b;
        diffd2 = (inflect - extreme) * T(0.5);
        y = (T(1) - t) * T(2); // remap (0.5, 1] → (1, 0], i.e. 1→0
    }

    T result = ((T(3) - y) * y * y) * diffd2 + extreme;

    if (limit) {
        if (result > T(1)) result = T(1);
        if (result < T(0)) result = T(0);
    }
    return result;
}

template<typename T>
void
tsl::envelope::genLinear(T *fp, int tablesize, T positions[], T values[],
                         int nsegs, bool limit) {
    int seglen;
    T *end = fp + tablesize;
    T incr;
    T posa, posb, vala = 0, valb;
    for (int i = 0; i < nsegs; i++) {
        posa = positions[i];
        posb = positions[i + 1];
        if (!(seglen = (int) ((posb - posa) * tablesize))) continue;
        if (seglen < 0) {
            LOGE("Error segsize < 0");
            return;
        }
        vala = values[i];
        valb = values[i + 1];

        incr = (valb - vala) / (T) seglen;
        while (seglen--) {
            *fp++ = vala;
            vala += incr;
            vala = limit ? (vala > 1.0 ? 1.0 : (vala < 0.0 ? 0.0 : vala)) : vala;
            if (fp >= end)
                return;
        }
    }
    while (fp < end)                 /* if 2**n pnts, add guardpt */
        *fp++ = vala;
}

template<typename T>
T tsl::envelope::genLinearPhase(
        T phase,
        T positions[],
        T values[],
        int nsegs,
        bool limit) {
    // Clamp phase
    if (phase <= positions[0])
        return values[0];

    if (phase >= positions[nsegs])
        return values[nsegs];

    // Find segment
    int seg = 0;
    for (; seg < nsegs; ++seg) {
        if (phase < positions[seg + 1])
            break;
    }

    T posa = positions[seg];
    T posb = positions[seg + 1];

    T vala = values[seg];
    T valb = values[seg + 1];

    T seglen = posb - posa;
    if (seglen <= static_cast<T>(0))
        return vala; // safety

    // normalized position inside segment
    T t = (phase - posa) / seglen;

    T out = vala + t * (valb - vala);

    if (limit) {
        if (out > static_cast<T>(1)) out = static_cast<T>(1);
        else if (out < static_cast<T>(0)) out = static_cast<T>(0);
    }

    return out;
}

template<typename T>
void
tsl::envelope::genRect(T *fp, const int tablesize, T positions[], T values[],
                       const int nsegs, const bool limit) {
    int seglen;
    T *end = fp + tablesize;
    T posa, posb, vala = 0;
    for (int i = 0; i < nsegs; i++) {
        posa = positions[i];
        posb = positions[i + 1];
        if (!(seglen = (int) ((posb - posa) * tablesize))) continue;
        if (seglen < 0) {
            LOGE("Error segsize < 0");
            return;
        }
        vala = values[i];
        vala = limit ? (vala > 1.0 ? 1.0 : (vala < 0.0 ? 0.0 : vala)) : vala;
        while (seglen--) {
            *fp++ = vala;
            if (fp >= end) {
                vala = values[nsegs];
                *(fp - 1) = limit ? (vala > 1.0 ? 1.0 : (vala < 0.0 ? 0.0 : vala)) : vala;
                return;
            }
        }
    }
    while (fp < end) {                /* if 2**n pnts, add guardpt */
        *fp++ = vala;
    }
    vala = values[nsegs];
    *(fp - 1) = limit ? (vala > 1.0 ? 1.0 : (vala < 0.0 ? 0.0 : vala)) : vala;
}

template<typename T>
T tsl::envelope::genRectPhase(
        T phase,
        T positions[],
        T values[],
        int nsegs,
        bool limit) {
    // Before first point
    if (phase <= positions[0]) {
        T out = values[0];
        if (limit) {
            if (out > static_cast<T>(1)) out = static_cast<T>(1);
            else if (out < static_cast<T>(0)) out = static_cast<T>(0);
        }
        return out;
    }

    // Find segment
    for (int i = 0; i < nsegs; ++i) {
        if (phase < positions[i + 1]) {
            T out = values[i];

            if (limit) {
                if (out > static_cast<T>(1)) out = static_cast<T>(1);
                else if (out < static_cast<T>(0)) out = static_cast<T>(0);
            }

            return out;
        }
    }

    // After last segment
    T out = values[nsegs];

    if (limit) {
        if (out > static_cast<T>(1)) out = static_cast<T>(1);
        else if (out < static_cast<T>(0)) out = static_cast<T>(0);
    }

    return out;
}

template<typename T>
T tsl::envelope::Sinc2(T x) {
    if (x > -1.0E-5 && x < 1.0E-5)
        return (1.0);
    return (sin(x) / x);
}

//---------------------------------------------------------------------------
template<typename T>
T tsl::envelope::Bessel(T x) {
    T Sum = 0.0, XtoIpower;
    int i, j, Factorial = 1;
    for (i = 1; i < 10; i++) {
        XtoIpower = pow(x / 2.0, (T) i);
        Sum += pow(XtoIpower / (T) Factorial, 2.0);
    }
    for (j = 1; j <= i; j++)
        Factorial *= j;
    return (1.0 + Sum);
}


float func_saw(float phase) {
    return (float) phase;
}

float func_full_saw(float phase) {
    return (float) (-1. + 2 * phase);
}

float func_full_pulse(float phase) {
    return phase < 0.5 ? -1.0 : 1.0;
}

float func_full_sine(float phase) {
    return sin(TWOPI_P * phase);
}

float func_hann(float phase) {
    return (.5 - .5 * cos(TWOPI_P * phase));
}

float func_black(float phase) {
    return (1.0 - .16) / 2.0
           - 0.50 * cos((TWOPI_P * phase)
                        + .16 / 2.0 * cos(TWOPI_P * 2.0 * phase));
}

/*
 This software is part of iowahills_dsp, a set of DSP routines under MIT License.
 2016 By Daniel Klostermann, Iowa Hills Software, LLC  IowaHills.com
 Copyright (c) 2021  Hayati Ayguen <h_ayguen@web.de>
 All rights reserved.


 These are the window definitions. These windows can be used for either
 FIR filter design or with an FFT for spectral analysis.
 For definitions, see this article:  http://en.wikipedia.org/wiki/Window_function

 This function has 6 inputs
 Data is the array, of length N, containing the data to to be windowed.
 This data is either an FIR filter sinc pulse, or the data to be analyzed by an fft.

 WindowType is an enum defined in the header file.
 e.g. wtKAISER, wtSINC, wtHANNING, wtHAMMING, wtBLACKMAN, ...

 Alpha sets the width of the flat top.
 Windows such as the Tukey and Trapezoid are defined to have a variably wide flat top.
 As can be seen by its definition, the Tukey is just a Hanning window with a flat top.
 Alpha can be used to give any of these windows a partial flat top, except the Flattop and Kaiser.
 Alpha = 0 gives the original window. (i.e. no flat top)
 To generate a Tukey window, use a Hanning with 0 < Alpha < 1
 To generate a Bartlett window (triangular), use a Trapezoid window with Alpha = 0.
 Alpha = 1 generates a rectangular window in all cases. (except the Flattop and Kaiser)


 Beta is used with the Kaiser, Sinc, and Sine windows only.
 These three windows are used primarily for FIR filter design. Then
 Beta controls the filter's transition bandwidth and the sidelobe levels.
 All other windows ignore Beta.

 UnityGain controls whether the gain of these windows is set to unity.
 Only the Flattop window has unity gain by design. The Hanning window, for example, has a gain
 of 1/2.  UnityGain = true  sets the gain to 1, which preserves the signal's energy level
 when these windows are used for spectral analysis.

 Don't use this with FIR filter design however. Since most of the enegy in an FIR sinc pulse
 is in the middle of the window, the window needs a peak amplitude of one, not unity gain.
 Setting UnityGain = true will simply cause the resulting FIR filter to have excess gain.

 If using these windows for FIR filters, start with the Kaiser, Sinc, or Sine windows and
 adjust Beta for the desired transition BW and sidelobe levels (set Alpha = 0).
 While the FlatTop is an excellent window for spectral analysis, don't use it for FIR filter design.
 It has a peak amplitude of ~ 4.7 which causes the resulting FIR filter to have about this much gain.
 It works poorly for FIR filters even if you adjust its peak amplitude.
 The Trapezoid also works poorly for FIR filter design.

 If using these windows with an fft for spectral analysis, start with the Hanning, Gauss, or Flattop.
 When choosing a window for spectral analysis, you must trade off between resolution and amplitude
 accuracy. The Hanning has the best resolution while the Flatop has the best amplitude accuracy.
 The Gauss is midway between these two for both accuracy and resolution. These three were
 the only windows available in the HP 89410A Vector Signal Analyzer. Which is to say, these three
 are the probably the best windows for general purpose signal analysis.
*/

//---------------------------------------------------------------------------



// This gets used with the Kaiser window.
double iowa_Bessel(double x) {
    double Sum = 0.0, XtoIpower;
    int i, j, Factorial;
    for (i = 1; i < 10; i++) {
        XtoIpower = pow(x / 2.0, (double) i);
        Factorial = 1;
        for (j = 1; j <= i; j++)Factorial *= j;
        Sum += pow(XtoIpower / (double) Factorial, 2.0);
    }
    return (1.0 + Sum);
}

//-----------------------------------------------------------------------------

// This gets used with the Sinc window.
static double iowa_Sinc(double x) {
    if (x > -1.0E-5 && x < 1.0E-5)return (1.0);
    return (sin(x) / x);
}

// Returns the window value at normalized phase position.
// phase: 0.0 = start, 1.0 = end (wraps automatically)
// alpha: only used by wtKAISER (beta), wtTUKEY (taper ratio), wtGAUSSIAN (sigma)
template<typename T>
T tsl::envelope::WindowFunc(T phase, WINDOWTYPE type, double alpha, double beta) {
    const double p = phase - floor(phase); // wrap to [0,1)

    switch (type) {
        case SAW:
            return (T) p;
        case FULL_SAW:
            return (T) (-1.0 + 2.0 * p);
        case RECTPULS:
            return (T) (p >= 0.5 ? 1.0 : 0.0);
        case FULL_RECTPULS:
            return (T) (p < 0.5 ? -1.0 : 1.0);
        case SINE_FULL:
            return (T) sin(TWOPI_P * p);
        case COSINE_FULL:
            return (T) cos(TWOPI_P * p);
        case wtHANNING:
        case SINE:
        case SINE_LFO:
            return (T) (0.5 - 0.5 * cos(TWOPI_P * p));
        case wtTRAPEZOID:
        case TRIANGLE:
            return (T) (p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p);
        case TRIANGLE_FULL:
            return (T) (p < 0.25 ? 4.0 * p
                                 : p < 0.50 ? 2.0 - 4.0 * p
                                            : p < 0.75 ? -4.0 * (p - 0.5)
                                                       : 4.0 * p - 4.0);
        case wtWELCH:
            return (T) (1.0 - pow((p - 0.5) / 0.5, 2.0));
        case wtBOHMANN: {
            double x = fabs(p - 0.5) / 0.5;
            return (T) ((1.0 - x) * cos(PI_P * x) + (1.0 / PI_P) * sin(PI_P * x));
        }
        case wtLOWSIDELOBE: {
            const double a[] = {0.471492057, 0.17553428, 0.028497078, 0.001261367};
            double v = 0.0;
            for (int k = 0; k <= 3; k++)
                v += pow(-1.0, k) * a[k] * cos(k * TWOPI_P * p);
            return (T) v;
        }
        case Rectangular:
            return (T) 1.0;
        case wtHAMMING:
            return (T) (0.54 - 0.46 * cos(TWOPI_P * p));
        case wtBLACKMAN:
            return (T) ((1.0 - 0.16) / 2.0 - 0.5 * cos(TWOPI_P * p) +
                        0.08 * cos(TWOPI_P * 2.0 * p));
        case wtBLACKMAN_HARRIS:
            return (T) (0.35875 - 0.48829 * cos(TWOPI_P * p) + 0.14128 * cos(TWOPI_P * 2.0 * p) -
                        0.01168 * cos(TWOPI_P * 3.0 * p));
        case wtBLACKMAN_NUTTALL:
            return (T) (0.3535819 - 0.4891775 * cos(TWOPI_P * p) +
                        0.1365995 * cos(TWOPI_P * 2.0 * p) - 0.0106411 * cos(TWOPI_P * 3.0 * p));
        case wtNUTTALL:
            return (T) (0.355768 - 0.487396 * cos(TWOPI_P * p) + 0.144232 * cos(TWOPI_P * 2.0 * p) -
                        0.012604 * cos(TWOPI_P * 3.0 * p));
        case wtKAISER_BESSEL:
            return (T) (0.402 - 0.498 * cos(TWOPI_P * p) + 0.098 * cos(2.0 * TWOPI_P * p) +
                        0.001 * cos(3.0 * TWOPI_P * p));
        case wtCOSINE:
            return (T) sin(PI_P * p);
        case wtSINC:
            return (T) Sinc2((2.0 * p - 1.0) * PI_P);
        case wtFLATTOP:
            return (T) (0.28106 - 0.520987 * cos(TWOPI_P * p) + 0.19804 * cos(TWOPI_P * 2.0 * p));
        case wtFLATTOP2:
            return (T) ((1.0 - 1.93 * cos(TWOPI_P * p) + 1.29 * cos(TWOPI_P * 2.0 * p) -
                         0.388 * cos(TWOPI_P * 3.0 * p) + 0.032 * cos(TWOPI_P * 4.0 * p)) / 3.8);
        case wtKAISER: {
            double beta1 = std::max(0.0, std::min(10.0, beta));
            double arg = beta1 * sqrt(1.0 - pow(2.0 * p - 1.0, 2.0));
            return (T) (Bessel(arg) / Bessel(beta1));
        }
        case wtTUKEY: {
            double a = std::clamp((double) alpha, 0.0001, 1.0);

            double left = a * 0.5;
            double right = 1.0 - left;

            if (p < left) {
                return (T) (0.5 * (1.0 +
                                   cos(PI_P * (2.0 * p / a - 1.0))));
            } else if (p > right) {
                return (T) (0.5 * (1.0 +
                                   cos(PI_P * (2.0 * p / a - 2.0 / a + 1.0))));
            } else {
                return (T) 1.0;
            }
        }
        case wtGAUSSIAN:
            return (T) pow(e, -0.5 * pow((p - 0.5) / (alpha * 0.5), 2.0));
        default:
            return (T) 0.0;
    }
}

template double tsl::envelope::WindowFunc<double>(
        double, WINDOWTYPE, double, double);

template float tsl::envelope::WindowFunc<float>(
        float, WINDOWTYPE, double, double);


template<typename T>
void tsl::envelope::GenerateWindow(T *win, int N, tsl::envelope::WINDOWTYPE WindowType,
                                   MYFLOAT alpha, MYFLOAT beta) {

    if (N <= 0 || !win) return;

    int j;
    T denom = (T) N;
    const double PI = 3.14159265358979323846;
    const double TWO_PI = 6.28318530717958647692;

    // Normalize parameters
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 10.0) beta = 10.0;

    // 1. Initial Clear
    for (j = 0; j < N; j++) win[j] = (T) 0.0;
    switch (WindowType) {
        case SAW:
        case FULL_SAW:
        case RECTPULS:
        case FULL_RECTPULS:
        case Rectangular:
        case SINE_FULL:
        case COSINE_FULL:
        case TRIANGLE_FULL:
        case wtTUKEY:
            for (j = 0; j < N; j++) {
                T p = static_cast<T>(j) / static_cast<T>(N);
                win[j] = WindowFunc(p, WindowType, alpha, beta);
            }
            win[N] = win[0];
            return;
    }
    // --- SYMMETRIC WINDOWS (The "Fold and Fill" group) ---
    // These windows use Alpha to create a flat-top middle section.

    int TopWidth = (int) ((double) alpha * (double) N);
    if (TopWidth % 2 != 0) TopWidth++;
    int M = N - TopWidth;
    double dM = (double) M;

    for (j = 0; j <= M / 2; j++) {
        T p = static_cast<T>(j) / static_cast<T>(N);
        win[j] = WindowFunc(p, WindowType, alpha, beta);
    }

    // Mirror
    for (j = 0; j < M / 2; j++)
        win[N - 1 - j] = win[j];

    // Fill the Flat Top (only if not Kaiser/Flattop which handle their own scaling)
    if (WindowType != wtKAISER && WindowType != wtFLATTOP && WindowType != wtFLATTOP2) {
        for (j = M / 2; j < N - M / 2; j++) {
            win[j] = (T) 1.0;
        }
    }
    win[N] = win[0];
}

template void
tsl::envelope::GenerateWindow<double>(double *win, int N, tsl::envelope::WINDOWTYPE WindowType,
                                      MYFLOAT alpha, MYFLOAT);

template void
tsl::envelope::GenerateWindow<float>(float *win, int N, tsl::envelope::WINDOWTYPE WindowType,
                                     MYFLOAT alpha, MYFLOAT);


//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

