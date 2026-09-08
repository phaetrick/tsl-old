//
// Created by pr on 27.02.21.
//

#ifndef GRAINSTORM_MOOGLADDER_H
#define GRAINSTORM_MOOGLADDER_H


#include <cmath>
#include <stdint.h>
#include <array>


#include "types.h"
#include "defines.h"

/*
 *  How do I perform ring modulation digitally?

To ring modulate 2 signals, X and Y, at sampling rate fs you need to upsample to 2fs to avoid aliasing. Why? Because if you multiply to signals with frequencies of fs/2 you produce the sum and difference of the signals the former being fs and the later being 0 so you must oversample to 2fs and filter to avoid this aliasing. A recipe to multiply signals X and Y with sampling rate fs without aliasing:

    Upsample X to 2fs by adding zeros between all the samples (this MYFLOATs the spectrum produces frequencies above fs/2 even though the original signal was bandlimited) (b) brick-wall lopass with cutoff @ fs/2 to correctly bandlimit.
    Lather, rinse, repeat with Y
    Multiply X * Y
    Brick-wall lopass X * Y with cutoff at fs/2
    Downsample back to fs by discarding every other sample (decimation)

 To avoid denormals, don't let any value get too small. Luckily for Audio applications, any number small enough to become denormal is inaudible.

    Test for denormals, and nuke them to 0 when you find them. You can use this handy macro if you wish:
    #DEFINE IS_DENORMAL(f) (((*(unsigned int *)&(f))&0x7f800000) == 0)

    You can use this on each value you put into your reverb, but for IIR filters, a better approach is to nuke denormals to 0 only in the coefficient memory, and only after each block of input samples has been processed. Thus, worst case, you'll take a denormal hit on one block of samples. If you want to avoid that, you can nuke any number that is "very small" to 0. Use this macro for the test:
    #DEFINE IS_ALMOST_DENORMAL(f) (((*(unsigned int *)&(f))&0x7f800000) < 0x08000000)

    or simple choose a number and compare in MYFLOATing point, if your CPU makes that faster:
    #DEFINE IS_ALMOST_DENORMAL(f) (fabs(f) < 3.e-34)
    A simple way to avoid denormals - is to add a very small number to a variable before multiplication to cause it to never reach denormal state. As a 24-bit converter with p2p range of -1.0 to 1.0 puts the quantization point at about .00000011920928955078 (which is about 1e-7) you can add noise that's another seven digits less significant than that and still stay well above the denormal floor. I e, if you add noise of the magnitude 1e-14, there is no way that this noise will be amplified so that it's actually hearable, and it's still sufficient to prevent pretty much any system to go into denormal degradation. You dont' have to use high-quality noise, a 32-element table you cycle through is probably quite sufficient.So, instead of:
    y = x*a0 + y*feedback_coeff;

    we write:
    #define TOOSMALL 0.0000000000000000000000001f
    y = x*a0 + y*feedback_coeff + TOOSMALL;
    You can use special FPU modes that treat denormals as 0, available in Pentium III and Pentium 4 SIMD mode, and probably on other platforms, too. This usually requires assembler. As an interesting side note, it is said that the Intel 486 built-in FPU treated 0 as a denormal number (because it had a 0 exponent) and thus high-performance code went to great lengths to try to avoid using the value 0. These days, you luckily don't have to worry about that :-)

There's also a very nice PDF file by Laurent de Soras that explains denormals in greater detail: http://ldesoras.free.fr/doc/articles/denormal.pdf

In ISO C FLT_MIN from gives 1.17549435e-38f as the smallest number that can be expressed in 32-bit MYFLOATing point. http://www.rustyspigot.com/Programming/IEEE%20754%20Floating%20Point%20Standard.htm#denormal
 */

#define MOOG_E         2.71828182845904523536028747135266250
#define MOOG_LOG2E     1.44269504088896340735992468100189214
#define MOOG_LOG10E    0.434294481903251827651128918916605082
#define MOOG_LN2       0.693147180559945309417232121458176568
#define MOOG_LN10      2.30258509299404568401799145468436421
#define MOOG_PI        3.14159265358979323846264338327950288
#define MOOG_PI_2      1.57079632679489661923132169163975144
#define MOOG_PI_4      0.785398163397448309615660845819875721
#define MOOG_1_PI      0.318309886183790671537767526745028724
#define MOOG_2_PI      0.636619772367581343075535053490057448
#define MOOG_2_SQRTPI  1.12837916709551257389615890312154517
#define MOOG_SQRT2     1.41421356237309504880168872420969808
#define MOOG_SQRT1_2   0.707106781186547524400844362104849039
#define MOOG_INV_PI_2  0.159154943091895

#define NO_COPY(C) C(const C &) = delete; C & operator = (const C &) = delete
#define NO_MOVE(C) NO_COPY(C); C(C &&) = delete; C & operator = (const C &&) = delete




#define HZ_TO_RAD(f) (MOOG_PI_2 * f)
#define RAD_TO_HZ(omega) (MOOG_INV_PI_2 * omega)

#ifdef __GNUC__
	#define ctz(N) __builtin_ctz(N)
#else
	template<typename T>
	inline int ctz(T x)
	{
		int p, b;
		for (p = 0, b = 1; !(b & x); b <<= 1, ++p)
			;
		return p;
	}
#endif


class BiQuadBase
{
public:

    BiQuadBase()
    {
        bCoef = {{0.0f, 0.0f, 0.0f}};
        aCoef = {{0.0f, 0.0f}};
        w = {{0.0f, 0.0f}};
    }

    ~BiQuadBase()
    {

    }

    // DF-II impl
    void Process(MYFLOAT * samples, const uint32_t n)
    {
        MYFLOAT out = 0;
        for (int s = 0; s < n; ++s)
        {
            out = bCoef[0] * samples[s] + w[0];
            w[0] = bCoef[1] * samples[s] - aCoef[0] * out + w[1];
            w[1] = bCoef[2] * samples[s] - aCoef[1] * out;
            samples[s] = out;
        }
    }

    MYFLOAT Tick(MYFLOAT s)
    {
        MYFLOAT out = bCoef[0] * s + w[0];
        w[0] = bCoef[1] * s - aCoef[0] * out + w[1];
        w[1] = bCoef[2] * s - aCoef[1] * out;
        return out;
    }

    void SetBiquadCoefs(std::array<MYFLOAT, 3> b, std::array<MYFLOAT, 2> a)
    {
        bCoef = b;
        aCoef = a;
    }

protected:
    std::array<MYFLOAT, 3> bCoef; // b0, b1, b2
    std::array<MYFLOAT, 2> aCoef; // a1, a2
    std::array<MYFLOAT, 2> w; // delays
};


class RBJFilter : public BiQuadBase
{
public:

    enum FilterType
    {
        LOWPASS,
        HIGHPASS,
        BANDPASS,
        ALLPASS,
        NOTCH,
        PEAK,
        LOW_SHELF,
        HIGH_SHELF
    };

    RBJFilter(FilterType type = FilterType::LOWPASS, MYFLOAT cutoff = 1, MYFLOAT sampleRate = 44100) : sampleRate(sampleRate), t(type)
    {
        Q = 1;
        A = 1;

        a = {{0.0f, 0.0f, 0.0f}};
        b = {{0.0f, 0.0f, 0.0f}};

        SetCutoff(cutoff);
    }

    ~RBJFilter()
    {

    }

    void UpdateCoefficients()
    {
        cosOmega = cos(omega);
        sinOmega = sin(omega);

        switch (t)
        {
            case LOWPASS:
            {
                alpha = sinOmega / (2.0 * Q);
                b[0] = (1 - cosOmega) / 2;
                b[1] = 1 - cosOmega;
                b[2] = b[0];
                a[0] = 1 + alpha;
                a[1] = -2 * cosOmega;
                a[2] = 1 - alpha;
            } break;

            case HIGHPASS:
            {
                alpha = sinOmega / (2.0 * Q);
                b[0] = (1 + cosOmega) / 2;
                b[1] = -(1 + cosOmega);
                b[2] = b[0];
                a[0] = 1 + alpha;
                a[1] = -2 * cosOmega;
                a[2] = 1 - alpha;
            } break;

            case BANDPASS:
            {
                alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
                b[0] = sinOmega / 2;
                b[1] = 0;
                b[2] = -b[0];
                a[0] = 1 + alpha;
                a[1] = -2 * cosOmega;
                a[2] = 1 - alpha;
            } break;

            case ALLPASS:
            {
                alpha = sinOmega / (2.0 * Q);
                b[0] = 1 - alpha;
                b[1] = -2 * cosOmega;
                b[2] = 1 + alpha;
                a[0] = b[2];
                a[1] = b[1];
                a[2] = b[0];
            } break;

            case NOTCH:
            {
                alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
                b[0] = 1;
                b[1] = -2 * cosOmega;
                b[2] = 1;
                a[0] = 1 + alpha;
                a[1] = b[1];
                a[2] = 1 - alpha;
            } break;

            case PEAK:
            {
                alpha = sinOmega * sinhf(logf(2.0) / 2.0 * Q * omega/sinOmega);
                b[0] = 1 + (alpha * A);
                b[1] = -2 * cosOmega;
                b[2] = 1 - (alpha * A);
                a[0] = 1 + (alpha / A);
                a[1] = b[1];
                a[2] = 1 - (alpha / A);
            } break;

            case LOW_SHELF:
            {
                alpha = sinOmega / 2.0 * sqrt( (A + 1.0 / A) * (1.0 / Q - 1.0) + 2.0);
                b[0] = A * ((A + 1) - ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
                b[1] = 2 * A * ((A - 1) - ((A + 1) * cosOmega));
                b[2] = A * ((A + 1) - ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
                a[0] = ((A + 1) + ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
                a[1] = -2 * ((A - 1) + ((A + 1) * cosOmega));
                a[2] = ((A + 1) + ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
            } break;

            case HIGH_SHELF:
            {
                alpha = sinOmega / 2.0 * sqrt( (A + 1.0 / A) * (1.0 / Q - 1.0) + 2.0);
                b[0] = A * ((A + 1) + ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
                b[1] = -2 * A * ((A - 1) + ((A + 1) * cosOmega));
                b[2] = A * ((A + 1) + ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
                a[0] = ((A + 1) - ((A - 1) * cosOmega) + (2 * sqrtf(A) * alpha));
                a[1] = 2 * ((A - 1) - ((A + 1) * cosOmega));
                a[2] = ((A + 1) - ((A - 1) * cosOmega) - (2 * sqrtf(A) * alpha));
            } break;
        }

        // Normalize filter coefficients
        MYFLOAT factor = 1.0f / a[0];

        std::array<MYFLOAT, 2> aNorm;
        std::array<MYFLOAT, 3> bNorm;

        aNorm[0] = a[1] * factor;
        aNorm[1] = a[2] * factor;

        bNorm[0] = b[0] * factor;
        bNorm[1] = b[1] * factor;
        bNorm[2] = b[2] * factor;

        SetBiquadCoefs(bNorm, aNorm);
    }

    // In Hertz, 0 to Nyquist
    void SetCutoff(MYFLOAT c)
    {
        omega = HZ_TO_RAD(c) / sampleRate;
        UpdateCoefficients();
    }

    MYFLOAT GetCutoff()
    {
        return omega;
    }

    // Arbitrary, from 0.01f to ~20
    void SetQValue(MYFLOAT q)
    {
        Q = q;
        UpdateCoefficients();
    }

    MYFLOAT GetQValue()
    {
        return Q;
    }

    void SetType(FilterType newType)
    {
        t = newType;
        UpdateCoefficients();
    }

    FilterType GetType()
    {
        return t;
    }

private:

    MYFLOAT sampleRate;

    MYFLOAT omega;
    MYFLOAT cosOmega;
    MYFLOAT sinOmega;

    MYFLOAT Q;
    MYFLOAT alpha;
    MYFLOAT A;

    std::array<MYFLOAT, 3> a;
    std::array<MYFLOAT, 3> b;

    FilterType t;
};



#define FILTER_CUT_MAX 8001

class Filter {
public:
    Filter(int sr = 1.0) : sampleRate(sr) {};

    virtual void reset() = 0;

    virtual      ~Filter() {};

    virtual inline MYFLOAT tick(MYFLOAT in, MYFLOAT cf, MYFLOAT q)  {
        return in;
    }
    virtual inline MYFLOAT tick(MYFLOAT in)  {
        return in;
    }
    virtual void compute(MYFLOAT *, uint32_t) = 0;


    virtual void compute(MYFLOAT *, uint32_t, MYFLOAT, MYFLOAT) = 0;

    static inline MYFLOAT dud(MYFLOAT in) {
        return std::fpclassify(in) != FP_NORMAL && std::fpclassify(in) != FP_ZERO ? 0 : in;
    }

    static inline MYFLOAT fud(MYFLOAT in) {
        return std::fpclassify(in) != FP_NORMAL && std::fpclassify(in) != FP_ZERO ? 0 : in;
    }

    static MYFLOAT clip(const MYFLOAT x) {
        return x / (1 + abs(x));
    }

    static inline MYFLOAT vox_fasttanh2(const MYFLOAT x) {
        const MYFLOAT ax = fabs(x);
        const MYFLOAT x2 = x * x;

        return (x * (2.45550750702956 + 2.45550750702956 * ax +
                     (0.893229853513558 + 0.821226666969744 * ax) * x2) /
                (2.44506634652299 + (2.44506634652299 + x2) *
                                    fabs(x + 0.814642734961073 * x * ax)));
    }


    static inline MYFLOAT fast_tanh(MYFLOAT x)
    {
        MYFLOAT x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }
    static inline
    MYFLOAT sfast_tanh(MYFLOAT x) {
        MYFLOAT x2 = x * x;
        MYFLOAT a = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
        MYFLOAT b = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
        return a / b;
    }

    static inline MYFLOAT TanH(MYFLOAT x) {
        /* use the fact that (-x) = - tanh(x)
           and if x>~4 tanh is approx constant 1
           and for small x tanh(x) =~ x
           So giving a cheap approximation */
        int sign = 1;
        if (x < 0) sign = -1, x = -x;
        if (x >= 4.0) {
            return sign;
        }
        if (x < 0.5) return x * sign;
        return sign * fast_tanh(x);
    }


// Imitate the (tanh) clipping function of a transistor pair.
// to 4th order, tanh is x - x*x*x/3; this cubic's
// plateaus are at +/- 1 so clip to 1 and evaluate the cubic.
// This is pretty coarse - for instance if you clip a sinusoid this way you
// can sometimes hear the discontinuity in 4th derivative at the clip point
    static inline MYFLOAT clip(MYFLOAT value, MYFLOAT saturation, MYFLOAT saturationinverse)
    {
        MYFLOAT v2 = (value * saturationinverse > 1 ? 1 :
                    (value * saturationinverse < -1 ? -1:
                     value * saturationinverse));
        return (saturation * (v2 - (1./3.) * v2 * v2 * v2));
    }


// Linear interpolation, used to crossfade a gain table
    static inline MYFLOAT moog_lerp(MYFLOAT amount, MYFLOAT a, MYFLOAT b)
    {
        return (1.0f - amount) * a + amount * b;
    }

    static inline MYFLOAT moog_min(MYFLOAT a, MYFLOAT b)
    {
        a = b - a;
        a += fabs(a);
        a *= 0.5f;
        a = b - a;
        return a;
    }

// Clamp without branching
// If input - _limit < 0, then it really substracts, and the 0.5 to make it half the 2 inputs.
// If > 0 then they just cancel, and keeps input normal.
// The easiest way to understand it is check what happends on both cases.
    static inline MYFLOAT moog_saturate(MYFLOAT input)
    {
        MYFLOAT x1 = fabs(input + 0.95);
        MYFLOAT x2 = fabs(input - 0.95);
        return 0.5f * (x1 - x2);
    }

protected:

    MYFLOAT cutoff;
    MYFLOAT resonance;
    MYFLOAT sampleRate;
};


class ResLp : public Filter {
public:
    inline MYFLOAT tick(const MYFLOAT in, const MYFLOAT cf, const MYFLOAT q_) {
        const MYFLOAT qq = 1. + 25. * q_;
        const MYFLOAT temp = -PI_P * cf / qq;
        const MYFLOAT aa = 2.0 * cos(cf * TWOPI_P) * exp(temp);
        const MYFLOAT bb = exp(temp + temp);
        const MYFLOAT cc = 1.0 - a + b;
        const MYFLOAT ret = (MYFLOAT) (yn = dud(aa * ynm1 - bb * ynm2 + cc * (MYFLOAT) in)) * .125f;
        ynm2 = ynm1;
        ynm1 = yn;
        return ret;
    }

    void compute(MYFLOAT *in, uint32_t s) {
        for (int i = 0; i < s; i++) {
            const MYFLOAT ret = (MYFLOAT) (yn = dud(a * ynm1 - b * ynm2 + c * (MYFLOAT) in[i])) * .125f;
            ynm2 = ynm1;
            ynm1 = yn;
            in[i] = ret;
        }
    }

    void compute(MYFLOAT *in, uint32_t s, const MYFLOAT cf, const MYFLOAT q) {
        setParams(cf, q);
        compute(in, s);
    }

    void reset() {
        ynm1 = ynm2 = 0.0;
    }

    void setParams(MYFLOAT cf, MYFLOAT q) {
        const MYFLOAT qq = 1. + 25. * q;
        const MYFLOAT temp = -PI_P * cf / qq;
        a = 2.0 * cos(cf * TWOPI_P) * exp(temp);
        b = exp(temp + temp);
        c = 1.0 - a + b;
    }

    /*
    void ComputeCoeffs(MYFLOAT q_) {
        MYFLOAT qq = logarithmic ? LOG2NORMAL(q_) : q_;
        qq = 1 + 25 * qq;
        for (int i = 1; i < FILTER_CUT_MAX; i++) {
            MYFLOAT temp = (MYFLOAT) (pidsr * i / qq);
            a[i] = 2.0 * cos((MYFLOAT) (i * tpidsr)) * exp(temp);
            b[i] = exp(temp + temp);
            c[i] = 1.0 - a[i] + b[i];
        }

    }
*/
private:
    MYFLOAT yn{}, ynm1{}, ynm2{};
    MYFLOAT a, b, c;
    int type;
};


class Reson2 : public Filter {
public:
    Reson2(int type_ = 0) {
        scaletype = 1;
        type = type_;
    }

    inline MYFLOAT tick(const MYFLOAT in, const MYFLOAT cf, const MYFLOAT q) {
        const MYFLOAT qq = 0.01 + .99 * q;
        const MYFLOAT bw = cf * qq;
        const MYFLOAT r = exp(-(MYFLOAT) (bw * PI_P));
        const MYFLOAT c1 = 2.0 * r * cos((MYFLOAT) (TWOPI_P * cf));
        const MYFLOAT c2 = r * r;
        const MYFLOAT scale = scaletype == 1 ? (1.0 - c2) * 0.5 : sqrt((1.0 - c2) * 0.5);
        xn = (MYFLOAT) in;
        const MYFLOAT ret = (MYFLOAT) (yn = dud(
                scale * (xn - xnm2) + c1 * ynm1 - c2 * ynm2));

        xnm2 = xnm1;
        xnm1 = xn;
        ynm2 = ynm1;
        ynm1 = yn;
        return ret;

/*
            if (type == 0) {
                if (scaletype == 1)
                    scale[i] = (1.0 - c2[i]) * 0.5;
                else if (scaletype == 2)
                    scale[i] = sqrt((1.0 - c2[i]) * 0.5);
            } else {
                if (scaletype == 1)
                    scale[i] = 1.0 - r[i];
                else if (scaletype == 2)
                    scale[i] = sqrt(1.0 - r[i]);
            }


             void computeenv(MYFLOAT *in, int s, int *env, int envsize, MYFLOAT gain = 1.0) {
        MYFLOAT qq = q->load();
        if (prevq != qq) {
            prevq = qq;
            ComputeCoeffs(qq);
        }
        if (type == 0)
            for (int i = 0; i < s; i++) {
                xn = (MYFLOAT) in[i];
                in[i] = (MYFLT) (yn = dud(
                        scale[env[i]] * (xn - xnm2) + c1[env[i]] * ynm1 - c2[env[i]] * ynm2)) *
                        gain;

                xnm2 = xnm1;
                xnm1 = xn;
                ynm2 = ynm1;
                ynm1 = yn;

            }
        else {
            for (int i = 0; i < s; i++) {
                xn = (MYFLOAT) in[i];
                in[i] = (MYFLT) (yn = dud(
                        scale[i] * (xn - r[i] * xnm2) + c1[i] * ynm1 - c2[i] * ynm2));
                xnm2 = xnm1;
                xnm1 = xn;
                ynm2 = ynm1;
                ynm1 = yn;

            }
        }
    }



            */
    }

    void compute(MYFLOAT *in, uint32_t s) {
        for (int i = 0; i < s; i++) {
            xn = (MYFLOAT) in[i];
            in[i] = (MYFLOAT) (yn = dud(
                    scale * (xn - xnm2) + c1 * ynm1 - c2 * ynm2));

            xnm2 = xnm1;
            xnm1 = xn;
            ynm2 = ynm1;
            ynm1 = yn;
        }
    }

    void compute(MYFLOAT *in, uint32_t s, const MYFLOAT cf, const MYFLOAT q) {
        setParams(cf, q);
        compute(in, s);
    }

    void setParams(MYFLOAT cf, MYFLOAT q) {
        const MYFLOAT qq = 0.01 + .99 * q;
        const MYFLOAT bw = cf * qq;
        const MYFLOAT r = exp(-(MYFLOAT) (bw * PI_P));
        c1 = 2.0 * r * cos((MYFLOAT) (TWOPI_P * cf));
        c2 = r * r;
        scale = scaletype == 1 ? (1.0 - c2) * 0.5 : sqrt((1.0 - c2) * 0.5);
    }

    void reset() {
        xnm1 = xnm2 = ynm1 = ynm2 = 0.0;
    }

    /*
    void ComputeCoeffs(MYFLOAT q_) {
        MYFLOAT qq = logarithmic ? LOG2NORMAL(q_) : q_;
        qq = 0.01 + .99 * qq;
        for (int i = 1; i < FILTER_CUT_MAX; i++) {
            MYFLOAT bw = i * qq;
            r[i] = exp(-(MYFLOAT) (bw * pidsr));
            c1[i] = 2.0 * r[i] * cos((MYFLOAT) (tpidsr * i));
            c2[i] = r[i] * r[i];

            if (type == 0) {
                if (scaletype == 1)
                    scale[i] = (1.0 - c2[i]) * 0.5;
                else if (scaletype == 2)
                    scale[i] = sqrt((1.0 - c2[i]) * 0.5);
            } else {
                if (scaletype == 1)
                    scale[i] = 1.0 - r[i];
                else if (scaletype == 2)
                    scale[i] = sqrt(1.0 - r[i]);
            }
        }

    }
*/
private:
    MYFLOAT xn{}, yn{}, xnm1{}, xnm2{}, ynm1{}, ynm2{};
    int scaletype{1};
    int type{0};
    MYFLOAT scale, c1, c2;
};


class MoogVCF : public Filter {
public:
    inline MYFLOAT tick(const MYFLOAT in, const MYFLOAT cf, const MYFLOAT q_) {
        const MYFLOAT q = q_ * .85;
        const MYFLOAT fcon = 2.0 * cf; /* normalised freq. 0 to Nyquist */
        const MYFLOAT kp = 3.6 * fcon - 1.6 * fcon * fcon - 1.0;     /* Emperical tuning   */
        const MYFLOAT pp1d2 = (kp + 1.0) * 0.5;                   /* Timesaver          */
        const MYFLOAT scale = exp((1.0 - pp1d2) * 1.386249);      /* Scaling factor     */
        const MYFLOAT k = q * scale;
        const MYFLOAT pp = pp1d2;
        const MYFLOAT kk = kp;

        const MYFLOAT xn = dud((MYFLOAT) in - k * y4n);

        y1n = (xn + xnm1) * pp - kk * y1n;
        y2n = (y1n + y1nm1) * pp - kk * y2n;
        y3n = (y2n + y2nm1) * pp - kk * y3n;
        y4n = (y3n + y3nm1) * pp - kk * y4n;

        y4n = y4n - y4n * y4n * y4n / 6.0;
        xnm1 = xn;       /* Update Xn-1  */
        y1nm1 = y1n;      /* Update Y1n-1 */
        y2nm1 = y2n;      /* Update Y2n-1 */
        y3nm1 = y3n;      /* Update Y3n-1 */
        return y4n;
    }

    void compute(MYFLOAT *in, uint32_t s) {
        for (int i = 0; i < s; i++) {
            const MYFLOAT xn = dud((MYFLOAT) in[i] - k * y4n);

            y1n = (xn + xnm1) * pp - kk * y1n;
            y2n = (y1n + y1nm1) * pp - kk * y2n;
            y3n = (y2n + y2nm1) * pp - kk * y3n;
            y4n = (y3n + y3nm1) * pp - kk * y4n;

            y4n = y4n - y4n * y4n * y4n / 6.0;

            UDD(y4n)

            xnm1 = xn;       /* Update Xn-1  */
            y1nm1 = y1n;      /* Update Y1n-1 */
            y2nm1 = y2n;      /* Update Y2n-1 */
            y3nm1 = y3n;      /* Update Y3n-1 */
            in[i] = y4n;
        }
    }


    void compute(MYFLOAT *in, uint32_t s, const MYFLOAT cf, const MYFLOAT q_) {
        setParams(cf, q_);
        compute(in, s);
    }

    void setParams(MYFLOAT cf, MYFLOAT q_) {
        const MYFLOAT q = q_ * .85;
        const MYFLOAT fcon = 2.0 * cf; /* normalised freq. 0 to Nyquist */
        const MYFLOAT kp = 3.6 * fcon - 1.6 * fcon * fcon - 1.0;     /* Emperical tuning   */
        const MYFLOAT pp1d2 = (kp + 1.0) * 0.5;                   /* Timesaver          */
        const MYFLOAT scale = exp((1.0 - pp1d2) * 1.386249);      /* Scaling factor     */
        k = q * scale;
        pp = pp1d2;
        kk = kp;
    }

    void reset() {
        xnm1 = y1nm1 = y2nm1 = y3nm1 = 0.0;
        y1n = y2n = y3n = y4n = 0.0;
    }


private:
    MYFLOAT xnm1{}, y1nm1{}, y2nm1{}, y3nm1{}, y1n{}, y2n{}, y3n{}, y4n{}, pp{}, kk{}, k{};
};


class MoogVCF2 : public Filter {
public:
    inline MYFLOAT tick(const MYFLOAT in, const MYFLOAT cf, const MYFLOAT q_) {
        const MYFLOAT q = q_ * .85;
        const MYFLOAT dd = 2.0;

        const MYFLOAT kfcn = 2.0 * cf;

        const MYFLOAT ax1 = lastin;
        const MYFLOAT ay11 = ay1;
        const MYFLOAT ay31 = ay2;

        const MYFLOAT kp = ((2.7528 * kfcn + 3.0429) * kfcn + 1.718) * kfcn - 0.9984;
        const MYFLOAT kp1h = (kp + 1.0) * 0.5;
        const MYFLOAT kp1 = kp + 1.0;
        const MYFLOAT ress = q * (((-2.7079 * kp1 + 10.963) * kp1
                                  - 14.934) * kp1 + 8.4974);

        lastin = dud(in - vox_fasttanh2(ress * aout));
        ay1 = kp1h * (lastin + ax1) - kp * ay1;
        ay2 = kp1h * (ay1 + ay11) - kp * ay2;
        aout = kp1h * (ay2 + ay31) - kp * aout;

        const MYFLOAT ddist = 1.0 + (dd * (1.5 + 2.0 * (MYFLOAT) ress * (1.0 - kfcn)));

        return (MYFLOAT) dud((vox_fasttanh2(aout * ddist)) * .25f);


    }

    void compute(MYFLOAT *in, uint32_t s) {
        for (int i = 0; i < s; i++) {
            const MYFLOAT ax1 = lastin;
            const MYFLOAT ay11 = ay1;
            const MYFLOAT ay31 = ay2;
            lastin = dud(in[i] - vox_fasttanh2(ress * aout));
            ay1 = kp1h * (lastin + ax1) - kp * ay1;
            ay2 = kp1h * (ay1 + ay11) - kp * ay2;
            aout = kp1h * (ay2 + ay31) - kp * aout;


            in[i] = (MYFLOAT) dud((vox_fasttanh2(aout * ddist)) * .25f);
        }
    }


    void compute(MYFLOAT *in, uint32_t s, const MYFLOAT cf, const MYFLOAT q_) {
        setParams(cf, q_);
        compute(in, s);
    }

    void setParams(MYFLOAT cf, MYFLOAT q_) {
        const MYFLOAT q = q_ * .85;
        const MYFLOAT dd = 3.0;

        const MYFLOAT kfcn = 2.0 * cf;

        kp = ((2.7528 * kfcn + 3.0429) * kfcn + 1.718) * kfcn - 0.9984;
        kp1h = (kp + 1.0) * 0.5;
        const MYFLOAT kp1 = kp + 1.0;
        ress = q * (((-2.7079 * kp1 + 10.963) * kp1
                     - 14.934) * kp1 + 8.4974);
        ddist = 1.0 + (dd * (1.5 + 2.0 * (MYFLOAT) ress * (1.0 - kfcn)));
    }

    void reset() {
        ay1 = ay2 = aout = lastin = 0.0;
    }

private:
    MYFLOAT ay1{}, ay2{}, aout{}, lastin{}, kp1h, ress, kp, ddist;
};

class MoogLadder : public Filter {
public:
    inline MYFLOAT tick(const MYFLOAT in, const MYFLOAT cf, const MYFLOAT q_) {
        const MYFLOAT f = 0.5 * cf;
        const MYFLOAT fc2 = cf * cf;
        const MYFLOAT fc3 = fc2 * cf;
        /* frequency & amplitude correction  */
        const MYFLOAT fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * cf + 0.9988;
        const MYFLOAT acr = -3.9364 * fc2 + 1.8409 * cf + 0.9968;
        const MYFLOAT tune = (1.0 - exp(-(TWOPI_P * f * fcr))) /
                            (0.000025) /* (1.0 / 40000.0) transistor thermal voltage  */;   /* filter tuning  */
        const MYFLOAT res4 = 3.8 * q_ * acr;
        MYFLOAT stg[4];
        for (int j = 0; j < 2; j++) {
            /* filter stages  */
            MYFLOAT input = in - res4 * _delay[5];
            _delay[0] = stg[0] =
                    _delay[0] + tune * (Filter::TanH(input * (0.000025)) - _tanhstg[0]);
            input = stg[0];
            stg[1] = _delay[1] +
                     tune * ((_tanhstg[0] = Filter::TanH(input * (0.000025))) - _tanhstg[1]);
            input = _delay[1] = stg[1];
            stg[2] = _delay[2] +
                     tune * ((_tanhstg[1] = Filter::TanH(input * (0.000025))) - _tanhstg[2]);
            input = _delay[2] = stg[2];
            stg[3] = _delay[3] + tune * ((_tanhstg[2] =
                                                  Filter::TanH(input * (0.000025))) -
                                         Filter::TanH(_delay[3] * (0.000025)));
            _delay[3] = stg[3];
            /* 1/2-sample delay for phase compensation  */
            _delay[5] = (stg[3] + _delay[4]) * 0.5;
            _delay[4] = stg[3];
        }
        return _delay[5];
    }

    void reset() {
        memset(_delay, 0, 6 * sizeof(MYFLOAT));
        memset(_tanhstg, 0, 3 * sizeof(MYFLOAT));
    }

private:
    MYFLOAT _delay[6]{};
    MYFLOAT _tanhstg[3]{};
};


class DiodeLadderFilter : public Filter {
public:

    void compute(MYFLOAT *in, int size, MYFLOAT gain = 1.0) {
    }

    DiodeLadderFilter() {
        std::fill(z, z + 5, 0);
        compute_q(.5);
        set_feedback_hpf_cutoff(80.f / 48000.f);
    }

    // fc: normalized cutoff frequency in the range [0..1] => 0 HZ .. Nyquist
    void set_feedback_hpf_cutoff(const MYFLOAT fc) {
        const MYFLOAT K = fc * M_PI;
        ah = (K - 2) / (K + 2);
        bh = 2 / (K + 2);
    }

    void reset() {
        if (_k < 17) std::fill(z, z + 5, 0);
    }

    // q: resonance in the range [0..1]
    void compute_q(const MYFLOAT q) {
        _k = 20 * q;
        _A = 1 + 0.5 * _k; // resonance gain compensation
    }

    // Process one sample.
    //
    // x: input signal
    // fc: normalized cutoff frequency in the range [0..1] => 0 HZ .. Nyquist
    inline MYFLOAT tick(const MYFLOAT x, const MYFLOAT fc, const MYFLOAT q_) {
        const MYFLOAT q = 0.01 + q_ * 0.98;
        const MYFLOAT k = 20 * q;
        const MYFLOAT A = 1 + 0.5 * k; // resonance gain compensation
        //assert(fc > 0 && fc < 1);
        //assert(fc > 0 && fc < 1);
        const MYFLOAT a = TWOPI_P * fc; // PI is Nyquist frequency
        // a = 2 * tan(0.5*a); // dewarping, not required with 2x oversampling
        const MYFLOAT ainv = 1 / a;
        const MYFLOAT a2 = a * a;
        const MYFLOAT b = 2 * a + 1;
        const MYFLOAT b2 = b * b;
        const MYFLOAT c = 1 / (2 * a2 * a2 - 4 * a2 * b2 + b2 * b2);
        const MYFLOAT g0 = 2 * a2 * a2 * c;
        const MYFLOAT g = g0 * bh;

        // current state
        const MYFLOAT s0 = (a2 * a * z[0] + a2 * b * z[1] + z[2] * (b2 - 2 * a2) * a +
                           z[3] * (b2 - 3 * a2) * b) * c;
        const MYFLOAT s = bh * s0 - z[4];

        // solve feedback loop (linear)
        MYFLOAT y5 = (g * x + s) / (1 + g * k);

        // input clipping
        const MYFLOAT y0 = dud(clip(x - k * y5));
        y5 = g * y0 + s;

        // compute integrator outputs
        const MYFLOAT y4 = g0 * y0 + s0;
        const MYFLOAT y3 = (b * y4 - z[3]) * ainv;
        const MYFLOAT y2 = (b * y3 - a * y4 - z[2]) * ainv;
        const MYFLOAT y1 = (b * y2 - a * y3 - z[1]) * ainv;

        // update filter state
        z[0] += 4 * a * (y0 - y1 + y2);
        z[1] += 2 * a * (y1 - 2 * y2 + y3);
        z[2] += 2 * a * (y2 - 2 * y3 + y4);
        z[3] += 2 * a * (y3 - 2 * y4);
        z[4] = bh * y4 + ah * y5;

        return A * y4;
    }

    void setParams(MYFLOAT cf, MYFLOAT q_) {
        const MYFLOAT q = 0.01 + q_ * 0.98;
        k = 20 * q;
        A = 1 + 0.5 * k; // resonance gain compensation
        //assert(fc > 0 && fc < 1);
        //assert(fc > 0 && fc < 1);
        a = TWOPI_P * cf; // PI is Nyquist frequency
        // a = 2 * tan(0.5*a); // dewarping, not required with 2x oversampling
        ainv = 1 / a;
        a2 = a * a;
        b = 2 * a + 1;
        b2 = b * b;
        c = 1 / (2 * a2 * a2 - 4 * a2 * b2 + b2 * b2);
        g0 = 2 * a2 * a2 * c;
        g = g0 * bh;
    }


    inline MYFLOAT tick(const MYFLOAT x, const MYFLOAT fc) {
        a = TWOPI_P * fc; // PI is Nyquist frequency
        // a = 2 * tan(0.5*a); // dewarping, not required with 2x oversampling
        ainv = 1 / a;
        a2 = a * a;
        b = 2 * a + 1;
        b2 = b * b;
        c = 1 / (2 * a2 * a2 - 4 * a2 * b2 + b2 * b2);
        g0 = 2 * a2 * a2 * c;
        g = g0 * bh;

        // current state
        const MYFLOAT s0 = (a2 * a * z[0] + a2 * b * z[1] + z[2] * (b2 - 2 * a2) * a +
                           z[3] * (b2 - 3 * a2) * b) * c;
        const MYFLOAT s = bh * s0 - z[4];

        // solve feedback loop (linear)
        MYFLOAT y5 = (g * x + s) / (1 + g * _k);

        // input clipping
        const MYFLOAT y0 = dud(clip(x - _k * y5));
        y5 = g * y0 + s;

        // compute integrator outputs
        const MYFLOAT y4 = g0 * y0 + s0;
        const MYFLOAT y3 = (b * y4 - z[3]) * ainv;
        const MYFLOAT y2 = (b * y3 - a * y4 - z[2]) * ainv;
        const MYFLOAT y1 = (b * y2 - a * y3 - z[1]) * ainv;

        // update filter state
        z[0] += 4 * a * (y0 - y1 + y2);
        z[1] += 2 * a * (y1 - 2 * y2 + y3);
        z[2] += 2 * a * (y2 - 2 * y3 + y4);
        z[3] += 2 * a * (y3 - 2 * y4);
        z[4] = bh * y4 + ah * y5;

        return _A * y4;
    }

    void compute(MYFLOAT *in, uint32_t size) {


        for (int i = 0; i < size; i++) {
            // current state
            const MYFLOAT s0 = (a2 * a * z[0] + a2 * b * z[1] + z[2] * (b2 - 2 * a2) * a +
                               z[3] * (b2 - 3 * a2) * b) * c;
            const MYFLOAT s = bh * s0 - z[4];

            const MYFLOAT x = in[i];
            // solve feedback loop (linear)
            MYFLOAT y5 = (g * x + s) / (1 + g * k);

            // input clipping
            const MYFLOAT y0 = dud(clip(x - k * y5));
            y5 = g * y0 + s;

            // compute integrator outputs
            const MYFLOAT y4 = g0 * y0 + s0;
            const MYFLOAT y3 = (b * y4 - z[3]) * ainv;
            const MYFLOAT y2 = (b * y3 - a * y4 - z[2]) * ainv;
            const MYFLOAT y1 = (b * y2 - a * y3 - z[1]) * ainv;

            // update filter state
            z[0] += 4 * a * (y0 - y1 + y2);
            z[1] += 2 * a * (y1 - 2 * y2 + y3);
            z[2] += 2 * a * (y2 - 2 * y3 + y4);
            z[3] += 2 * a * (y3 - 2 * y4);
            z[4] = bh * y4 + ah * y5;

            in[i] = A * y4;
        }
    }

    void compute(MYFLOAT *in, uint32_t size, const MYFLOAT cf, const MYFLOAT q_) {
        setParams(cf, q_);
        compute(in, size);
    }


private:
    MYFLOAT _k{}, _A{};
    MYFLOAT z[5]{}; // filter memory (4 integrators plus 1st order HPF)
    MYFLOAT ah, bh; // feedback HPF coeffs
    MYFLOAT g, g0, b, a, a2, b2, c, k, ainv, A;

};
#define SNAP_TO_ZERO(n)    if (! (n < -1.0e-8 || n > 1.0e-8)) n = 0;


class VAOnePole {
public:

    VAOnePole(MYFLOAT sr = 1.f) : sampleRate(sr) {
        Reset();
    }

    void Reset() {
        alpha = 1.0;
        beta = 0.0;
        gamma = 1.0;
        delta = 0.0;
        epsilon = 0.0;
        a0 = 1.0;
        feedback = 0.0;
        z1 = 0.0;
    }

    MYFLOAT Tick(MYFLOAT s) {
        s = s * gamma + feedback + epsilon * GetFeedbackOutput();
        MYFLOAT vn = (a0 * s - z1) * alpha;
        MYFLOAT out = vn + z1;
        z1 = vn + out;
        return out;
    }

    void SetFeedback(MYFLOAT fb) { feedback = fb; }

    MYFLOAT GetFeedbackOutput() { return beta * (z1 + feedback * delta); }

    void SetAlpha(MYFLOAT a) { alpha = a; };

    void SetBeta(MYFLOAT b) { beta = b; };

private:

    MYFLOAT sampleRate;
    MYFLOAT alpha;
    MYFLOAT beta;
    MYFLOAT gamma;
    MYFLOAT delta;
    MYFLOAT epsilon;
    MYFLOAT a0;
    MYFLOAT feedback;
    MYFLOAT z1;
};

// See: http://www.willpirkle.com/forum/licensing-and-book-code/licensing-and-using-book-code/
// The license is "You may also use the code from the FX and Synth books without licensing or fees.
// The code is for you to develop your own plugins for your own use or for commercial use."

class OberheimVariationMoog : public Filter {

public:

    OberheimVariationMoog(MYFLOAT sampleRate = 1.f) : Filter(sampleRate) {
        LPF1 = new VAOnePole(sampleRate);
        LPF2 = new VAOnePole(sampleRate);
        LPF3 = new VAOnePole(sampleRate);
        LPF4 = new VAOnePole(sampleRate);

        saturation = 1.0;
        Q = 3.0;

        SetCutoff(1000.f);
        SetResonance(0.1f);
    }

    virtual ~OberheimVariationMoog() {
        delete LPF1;
        delete LPF2;
        delete LPF3;
        delete LPF4;
    }

    virtual void compute(MYFLOAT *in, uint32_t n, MYFLOAT cf, MYFLOAT q) noexcept override {
        SetResonance(q);
        SetCutoff(cf);
        compute(in, n);
    }




    virtual void compute(MYFLOAT *samples, uint32_t n) noexcept override {
        for (int s = 0; s < n; ++s) {
            MYFLOAT input = samples[s];

            MYFLOAT sigma =
                    LPF1->GetFeedbackOutput() +
                    LPF2->GetFeedbackOutput() +
                    LPF3->GetFeedbackOutput() +
                    LPF4->GetFeedbackOutput();

            input *= 1.0 + K;

            // calculate input to first filter
            MYFLOAT u = (input - K * sigma) * alpha0;

            u = fast_tanh(saturation * u);

            MYFLOAT stage1 = LPF1->Tick(u);
            MYFLOAT stage2 = LPF2->Tick(stage1);
            MYFLOAT stage3 = LPF3->Tick(stage2);
            MYFLOAT stage4 = LPF4->Tick(stage3);

            // Oberheim variations
            samples[s] =
                    oberheimCoefs[0] * u +
                    oberheimCoefs[1] * stage1 +
                    oberheimCoefs[2] * stage2 +
                    oberheimCoefs[3] * stage3 +
                    oberheimCoefs[4] * stage4;
        }
    }

    void reset() override {
        LPF1->Reset();
        LPF2->Reset();
        LPF3->Reset();
        LPF4->Reset();
    }

    void SetResonance(MYFLOAT r) {
        r*= 15;
        // this maps resonance = 1->10 to K = 0 -> 4
        K = (4.0) * (r - 1.0) / (10.0 - 1.0);
    }

    void SetCutoff(MYFLOAT c) {
        cutoff = c;

        // prewarp for BZT
        MYFLOAT wd = 2.0 * PI_P * cutoff;
        MYFLOAT T = 1.0 / sampleRate;
        MYFLOAT wa = (2.0 / T) * tan(wd * T / 2.0);
        MYFLOAT g = wa * T / 2.0;

        // Feedforward coeff
        MYFLOAT G = g / (1.0 + g);

        LPF1->SetAlpha(G);
        LPF2->SetAlpha(G);
        LPF3->SetAlpha(G);
        LPF4->SetAlpha(G);

        LPF1->SetBeta(G * G * G / (1.0 + g));
        LPF2->SetBeta(G * G / (1.0 + g));
        LPF3->SetBeta(G / (1.0 + g));
        LPF4->SetBeta(1.0 / (1.0 + g));

        gamma = G * G * G * G;
        alpha0 = 1.0 / (1.0 + K * gamma);

        // Oberheim variations / LPF4
        oberheimCoefs[0] = 0.0;
        oberheimCoefs[1] = 0.0;
        oberheimCoefs[2] = 0.0;
        oberheimCoefs[3] = 0.0;
        oberheimCoefs[4] = 1.0;
    }

private:

    VAOnePole *LPF1;
    VAOnePole *LPF2;
    VAOnePole *LPF3;
    VAOnePole *LPF4;

    MYFLOAT K;
    MYFLOAT gamma;
    MYFLOAT alpha0;
    MYFLOAT Q;
    MYFLOAT saturation;

    MYFLOAT oberheimCoefs[5];
};


/*
Huovilainen developed an improved and physically correct model of the Moog
Ladder filter that builds upon the work done by Smith and Stilson. This model
inserts nonlinearities inside each of the 4 one-pole sections on account of the
smoothly saturating function of analog transistors. The base-emitter voltages of
the transistors are considered with an experimental value of 1.22070313 which
maintains the characteristic sound of the analog Moog. This model also permits
self-oscillation for resonances greater than 1. The model depends on five
hyperbolic tangent functions (tanh) for each sample, and an oversampling factor
of two (preferably higher, if possible). Although a more faithful
representation of the Moog ladder, these dependencies increase the processing
time of the filter significantly. Lastly, a half-sample delay is introduced for
phase compensation at the final stage of the filter.
References: Huovilainen (2004), Huovilainen (2010), DAFX - Zolzer (ed) (2nd ed)
Original implementation: Victor Lazzarini for CSound5
Considerations for oversampling:
http://music.columbia.edu/pipermail/music-dsp/2005-February/062778.html
http://www.synthmaker.co.uk/dokuwiki/doku.php?id=tutorials:oversampling
*/

class HuovilainenMoog : public Filter {
public:

    HuovilainenMoog(MYFLOAT sampleRate = 1.f) : Filter(sampleRate), thermal(0.000025) {
        reset();
    }

    virtual ~HuovilainenMoog() {

    }

    void reset() override {
        memset(stage, 0, sizeof(stage));
        memset(delay, 0, sizeof(delay));
        memset(stageTanh, 0, sizeof(stageTanh));
    }

    // Multimode via ladder-stage tap mixing (the classic Oberheim variation).
    // The 4-pole ladder exposes a tap after each 1-pole section; a weighted sum
    // of the input (y0) and those taps (y1..y4) synthesises LP/BP/HP/notch
    // responses without adding any extra filter state. Resonance stays the same
    // global feedback loop, so the resonant peak sits at cutoff in every mode.
    // Coefficients follow the binomial (1-H)^n identity for an ideal 1-pole H:
    //   LPn = H^n, HPn = (1-H)^n, BP = LP*HP, notch = LP2 + HP2.
    // makeup gains compensate the lower passband gain of the band-limited modes
    // (BP2 peaks at 0.25, BP4 at 0.0625 with no resonance) and are voiced by ear
    // rather than to exact unity — tweak freely.
    // NOTE: default is LP4 (bit-identical to the pre-multimode filter), so
    // existing voices/presets are unaffected unless setMode() is called.
    enum Mode {
        MODE_LP4 = 0, MODE_LP2 = 1,
        MODE_BP2 = 2, MODE_BP4 = 3,
        MODE_HP2 = 4, MODE_HP4 = 5,
        MODE_NOTCH = 6
    };

    void setMode(int mode) {
        switch (mode) {
            case MODE_LP2: // 2-pole low-pass: y2
                oberheimCoefs[0] = 0; oberheimCoefs[1] = 0; oberheimCoefs[2] = 1;
                oberheimCoefs[3] = 0; oberheimCoefs[4] = 0;
                modeGain = 1.0f;
                break;
            case MODE_BP2: // 2-pole band-pass: H - H^2 = y1 - y2
                oberheimCoefs[0] = 0; oberheimCoefs[1] = 1; oberheimCoefs[2] = -1;
                oberheimCoefs[3] = 0; oberheimCoefs[4] = 0;
                modeGain = 4.0f;
                break;
            case MODE_BP4: // 4-pole band-pass: (H - H^2)^2 = y2 - 2*y3 + y4
                oberheimCoefs[0] = 0; oberheimCoefs[1] = 0; oberheimCoefs[2] = 1;
                oberheimCoefs[3] = -2; oberheimCoefs[4] = 1;
                modeGain = 12.0f;
                break;
            case MODE_HP2: // 2-pole high-pass: (1-H)^2 = y0 - 2*y1 + y2
                oberheimCoefs[0] = 1; oberheimCoefs[1] = -2; oberheimCoefs[2] = 1;
                oberheimCoefs[3] = 0; oberheimCoefs[4] = 0;
                modeGain = 1.0f;
                break;
            case MODE_HP4: // 4-pole high-pass: (1-H)^4 = y0 -4y1 +6y2 -4y3 +y4
                oberheimCoefs[0] = 1; oberheimCoefs[1] = -4; oberheimCoefs[2] = 6;
                oberheimCoefs[3] = -4; oberheimCoefs[4] = 1;
                modeGain = 1.0f;
                break;
            case MODE_NOTCH: // LP2 + HP2 = 1 - 2H + 2H^2 = y0 - 2*y1 + 2*y2
                oberheimCoefs[0] = 1; oberheimCoefs[1] = -2; oberheimCoefs[2] = 2;
                oberheimCoefs[3] = 0; oberheimCoefs[4] = 0;
                modeGain = 1.0f;
                break;
            case MODE_LP4: // 4-pole low-pass: y4 (unchanged original behaviour)
            default:
                oberheimCoefs[0] = 0; oberheimCoefs[1] = 0; oberheimCoefs[2] = 0;
                oberheimCoefs[3] = 0; oberheimCoefs[4] = 1;
                modeGain = 1.0f;
                break;
        }
    }

    virtual void compute(MYFLOAT *samples, uint32_t n, MYFLOAT cf, MYFLOAT q) override {
        setParams(cf, q);
        compute(samples, n);
    }


    virtual void compute(MYFLOAT *samples, uint32_t n) override {
        for (int s = 0; s < n; ++s) {
            const MYFLOAT in = samples[s];
            // Oversample
            for (int j = 0; j < 2; j++) {
                MYFLOAT input = in - resQuad * delay[5];
                delay[0] = stage[0] = delay[0] + tune * (fast_tanh(input * thermal) - stageTanh[0]);
                for (int k = 1; k < 4; k++) {
                    input = stage[k - 1];
                    stage[k] = delay[k] + tune * ((stageTanh[k - 1] = fast_tanh(input * thermal)) -
                                                  (k != 3 ? stageTanh[k] : fast_tanh(
                                                          delay[k] * thermal)));
                    delay[k] = stage[k];
                }
                // 0.5 sample delay for phase compensation
                delay[5] = (stage[3] + delay[4]) * 0.5;
                delay[4] = stage[3];
            }
            samples[s] = mixOutput(in);
        }

    }

    inline void setParams(MYFLOAT c, MYFLOAT r) {
        cutoff = c;

        MYFLOAT fc = cutoff / sampleRate;
        MYFLOAT f = fc * 0.5; // oversampled
        MYFLOAT fc2 = fc * fc;
        MYFLOAT fc3 = fc * fc * fc;

        MYFLOAT fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
        acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;

        tune = (1.0 - exp(-((2 * PI_P) * f * fcr))) / thermal;
        resonance = r * .99;
        resQuad = 4.0 * resonance * acr;

    }

    inline MYFLOAT tick(MYFLOAT in, MYFLOAT c, MYFLOAT r) override {
        setParams(c,r);
        for (int j = 0; j < 2; j++) {
            MYFLOAT input = in - resQuad * delay[5];
            delay[0] = stage[0] = delay[0] + tune * (fast_tanh(input * thermal) - stageTanh[0]);
            for (int k = 1; k < 4; k++) {
                input = stage[k - 1];
                stage[k] = delay[k] + tune * ((stageTanh[k - 1] = fast_tanh(input * thermal)) -
                                              (k != 3 ? stageTanh[k] : fast_tanh(
                                                      delay[k] * thermal)));
                delay[k] = stage[k];
            }
            // 0.5 sample delay for phase compensation
            delay[5] = (stage[3] + delay[4]) * 0.5;
            delay[4] = stage[3];
        }
        return mixOutput(in);
    };

    inline MYFLOAT tick(MYFLOAT in) override {
        for (int j = 0; j < 2; j++) {
            MYFLOAT input = in - resQuad * delay[5];
            delay[0] = stage[0] = delay[0] + tune * (fast_tanh(input * thermal) - stageTanh[0]);
            for (int k = 1; k < 4; k++) {
                input = stage[k - 1];
                stage[k] = delay[k] + tune * ((stageTanh[k - 1] = fast_tanh(input * thermal)) -
                                              (k != 3 ? stageTanh[k] : fast_tanh(
                                                      delay[k] * thermal)));
                delay[k] = stage[k];
            }
            // 0.5 sample delay for phase compensation
            delay[5] = (stage[3] + delay[4]) * 0.5;
            delay[4] = stage[3];
        }
        return mixOutput(in);
    };

private:

    // Weighted sum of the ladder input (y0) and the four stage taps (y1..y4).
    // y4 uses the phase-compensated output so LP mode is bit-identical to the
    // original filter.
    inline MYFLOAT mixOutput(MYFLOAT in) const {
        return modeGain * (oberheimCoefs[0] * in
                         + oberheimCoefs[1] * stage[0]
                         + oberheimCoefs[2] * stage[1]
                         + oberheimCoefs[3] * stage[2]
                         + oberheimCoefs[4] * delay[5]);
    }

    MYFLOAT stage[4];
    MYFLOAT stageTanh[3];
    MYFLOAT delay[6];

    MYFLOAT thermal;
    MYFLOAT tune;
    MYFLOAT acr;
    MYFLOAT resQuad;

    // Tap-mix coefficients + makeup gain; default = 4-pole low-pass.
    MYFLOAT oberheimCoefs[5]{0, 0, 0, 0, 1};
    MYFLOAT modeGain{1.f};

};



/*
This model is based on a reference implementation of an algorithm developed by
Stefano D'Angelo and Vesa Valimaki, presented in a paper published at ICASSP in 2013.
This improved model is based on a circuit analysis and compared against a reference
Ngspice simulation. In the paper, it is noted that this particular model is
more accurate in preserving the self-oscillating nature of the real filter.
References: "An Improved Virtual Analog Model of the Moog Ladder Filter"
Original Implementation: D'Angelo, Valimaki
*/

// Thermal voltage (26 milliwats at room temperature)
#define VT 0.312

class ImprovedMoog : public Filter {
public:

    ImprovedMoog(MYFLOAT sampleRate = 1.f) : Filter(sampleRate) {
        reset();
        drive = 1.0f;
    }

    virtual ~ImprovedMoog() {}

    void reset() override {
        memset(V, 0, sizeof(V));
        memset(dV, 0, sizeof(dV));
        memset(tV, 0, sizeof(tV));
    }

    virtual void compute(MYFLOAT *samples, uint32_t n) override {
        MYFLOAT dV0, dV1, dV2, dV3;

        for (int i = 0; i < n; i++) {
            dV0 = -g * (fast_tanh((drive * samples[i] + resonance * V[3]) / (2.0 * VT)) + tV[0]);
            V[0] += (dV0 + dV[0]) / (2.0 * sampleRate);
            dV[0] = dV0;
            tV[0] = fast_tanh(V[0] / (2.0 * VT));

            dV1 = g * (tV[0] - tV[1]);
            V[1] += (dV1 + dV[1]) / (2.0 * sampleRate);
            dV[1] = dV1;
            tV[1] = fast_tanh(V[1] / (2.0 * VT));

            dV2 = g * (tV[1] - tV[2]);
            V[2] += (dV2 + dV[2]) / (2.0 * sampleRate);
            dV[2] = dV2;
            tV[2] = fast_tanh(V[2] / (2.0 * VT));

            dV3 = g * (tV[2] - tV[3]);
            V[3] += (dV3 + dV[3]) / (2.0 * sampleRate);
            dV[3] = dV3;
            tV[3] = fast_tanh(V[3] / (2.0 * VT));

            samples[i] = V[3];
        }
    }

    void compute(MYFLOAT *samples, uint32_t n, MYFLOAT cf, MYFLOAT r) override {
        setParams(cf, r);
        compute(samples, n);
    }

    void setParams(MYFLOAT c, MYFLOAT r) {
        SetResonance(r);
        SetCutoff(c);
    }

    void SetResonance(MYFLOAT r) {
        resonance = r * 2.2;
    }

    void SetCutoff(MYFLOAT c) {
        cutoff = c;
        x = (PI_P * cutoff) / sampleRate;
        g = 4.0 * PI_P * VT * cutoff * (1.0 - x) / (1.0 + x);
    }

private:

    MYFLOAT V[4];
    MYFLOAT dV[4];
    MYFLOAT tV[4];

    MYFLOAT x;
    MYFLOAT g;
    MYFLOAT drive;
};


/*
This class implements Tim Stilson's MoogVCF filter
using 'compromise' poles at z = -0.3
Several improments are built in, such as corrections
for cutoff and resonance parameters, removal of the
        necessity of the separation table, audio rate update
of cutoff and resonance and a smoothly saturating
tanh() function, clamping output and creating inherent
nonlinearities.
This code is Unlicensed (i.e. public domain); in an email exchange on
4.21.2018 Aaron Krajeski stated: "That work is under no copyright.
You may use it however you might like."
Source: http://song-swap.com/MUMT618/aaron/Presentation/demo.html
*/

class KrajeskiMoog final : public Filter
{

public:

    KrajeskiMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {
        reset();

        drive = 1.0;
        gComp = 1.0;
    }

    void reset() override {
        memset(state, 0, sizeof(state));
        memset(delay, 0, sizeof(delay));
    }

    virtual ~KrajeskiMoog() { }

    virtual void compute(MYFLOAT * samples, const uint32_t n) override
    {
        for (int s = 0; s < n; ++s)
        {
            state[0] = fast_tanh(drive * (samples[s] - 4 * gRes * (state[4] - gComp * samples[s])));

            for(int i = 0; i < 4; i++)
            {
                state[i+1] = g * (0.3 / 1.3 * state[i] + 1 / 1.3 * delay[i] - state[i + 1]) + state[i + 1];
                delay[i] = state[i];
            }
            samples[s] = state[4];
        }
    }


    virtual void compute(MYFLOAT * samples, const uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }


    void setParams(MYFLOAT c, MYFLOAT r){
        SetCutoff(c);
        SetResonance(r);
    }

    void SetResonance(MYFLOAT r)
    {
        resonance = r * 2;
        gRes = resonance * (1.0029 + 0.0526 * wc - 0.926 * pow(wc, 2) + 0.0218 * pow(wc, 3));
    }

    void SetCutoff(MYFLOAT c)
    {
        cutoff = c;
        wc = 2 * PI_P * cutoff / sampleRate;
        g = 0.9892 * wc - 0.4342 * pow(wc, 2) + 0.1381 * pow(wc, 3) - 0.0202 * pow(wc, 4);
    }

private:

    MYFLOAT state[5];
    MYFLOAT delay[5];
    MYFLOAT wc; // The angular frequency of the cutoff.
    MYFLOAT g; // A derived parameter for the cutoff frequency
    MYFLOAT gRes; // A similar derived parameter for resonance.
    MYFLOAT gComp; // Compensation factor.
    MYFLOAT drive; // A parameter that controls intensity of nonlinearities.

};


class MicrotrackerMoog : public Filter
{

public:

    MicrotrackerMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {

        reset();
    }

    void reset() override {
        p0 = p1 = p2 = p3 = p32 = p33 = p34 = 0.0;
    }

    virtual ~MicrotrackerMoog() {}

    virtual void compute(MYFLOAT * samples, uint32_t n) override
    {
        MYFLOAT k = resonance * 4;
        for (int s = 0; s < n; ++s)
        {
            // Coefficients optimized using differential evolution
            // to make feedback gain 4.0 correspond closely to the
            // border of instability, for all values of omega.
            MYFLOAT out = p3 * 0.360891 + p32 * 0.417290 + p33 * 0.177896 + p34 * 0.0439725;

            p34 = p33;
            p33 = p32;
            p32 = p3;

            p0 += (fast_tanh(samples[s] - k * out) - fast_tanh(p0)) * cutoff;
            p1 += (fast_tanh(p0) - fast_tanh(p1)) * cutoff;
            p2 += (fast_tanh(p1) - fast_tanh(p2)) * cutoff;
            p3 += (fast_tanh(p2) - fast_tanh(p3)) * cutoff;

            samples[s] = out;
        }
    }

    virtual void compute(MYFLOAT * samples, uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }

    void setParams(MYFLOAT c, MYFLOAT r){
        SetResonance(r);
        SetCutoff(c);
    }

    void SetResonance(MYFLOAT r)
    {
        resonance = r;
    }

    void SetCutoff(MYFLOAT c)
    {
        cutoff = c * 2 * PI_P / sampleRate;
        cutoff = moog_min(cutoff, 1.0);
    }

private:

    MYFLOAT p0;
    MYFLOAT p1;
    MYFLOAT p2;
    MYFLOAT p3;
    MYFLOAT p32;
    MYFLOAT p33;
    MYFLOAT p34;
};



class MusicDSPMoog : public Filter
{

public:

    MusicDSPMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {
        reset();
    }

    void reset() override {
        memset(stage, 0, sizeof(stage));
        memset(delay, 0, sizeof(delay));
    }

    virtual ~MusicDSPMoog()
    {

    }

    virtual void compute(MYFLOAT * samples, uint32_t n) override
    {
        for (int s = 0; s < n; ++s)
        {
            MYFLOAT x = samples[s] - resonance * stage[3];

            // Four cascaded one-pole filters (bilinear transform)
            stage[0] = x * p + delay[0]  * p - k * stage[0];
            stage[1] = stage[0] * p + delay[1] * p - k * stage[1];
            stage[2] = stage[1] * p + delay[2] * p - k * stage[2];
            stage[3] = stage[2] * p + delay[3] * p - k * stage[3];

            // Clipping band-limited sigmoid
            stage[3] -= (stage[3] * stage[3] * stage[3]) / 6.0;

            delay[0] = x;
            delay[1] = stage[0];
            delay[2] = stage[1];
            delay[3] = stage[2];

            samples[s] = stage[3];
        }
    }


    virtual void compute(MYFLOAT * samples, uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }

    void setParams(MYFLOAT c, MYFLOAT r)
    {
        r*=.5;
        cutoff = 2.0 * c / sampleRate;

        p = cutoff * (1.8 - 0.8 * cutoff);
        k = 2.0 * sin(cutoff * PI_P * 0.5) - 1.0;
        t1 = (1.0 - p) * 1.386249;
        t2 = 12.0 + t1 * t1;

        resonance = r * (t2 + 6.0 * t1) / (t2 - 6.0 * t1);
    }

private:

    MYFLOAT stage[4];
    MYFLOAT delay[4];

    MYFLOAT p;
    MYFLOAT k;
    MYFLOAT t1;
    MYFLOAT t2;

};



/*
Imitates a Moog resonant filter by Runge-Kutte numerical integration of
a differential equation approximately describing the dynamics of the circuit.

Useful references:
	* Tim Stilson
	"Analyzing the Moog VCF with Considerations for Digital Implementation"
		Sections 1 and 2 are a reasonably good introduction but the
		model they use is highly idealized.
	* Timothy E. Stinchcombe
	"Analysis of the Moog Transistor Ladder and Derivative Filters"
		Long, but a very thorough description of how the filter works including
		its nonlinearities
	* Antti Huovilainen
	"Non-linear digital implementation of the moog ladder filter"
		Comes close to giving a differential equation for a reasonably realistic
		model of the filter
The differential equations are:
	y1' = k * (S(x - r * y4) - S(y1))
	y2' = k * (S(y1) - S(y2))
	y3' = k * (S(y2) - S(y3))
	y4' = k * (S(y3) - S(y4))
where k controls the cutoff frequency, r is feedback (<= 4 for stability), and S(x) is a saturation function.
*/

class RKSimulationMoog : public Filter
{

public:

    RKSimulationMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {
        reset();
        saturation = 3.0;
        saturationInv = 1.0 / saturation;

        oversampleFactor = 1;

        stepSize = 1.0 / (oversampleFactor * sampleRate);
    }

    void reset() override {
        memset(state, 0, sizeof(state));
    }

    virtual ~RKSimulationMoog()
    {
    }

    virtual void compute(MYFLOAT * samples, uint32_t n) override
    {
        for (int s = 0; s < n; ++s)
        {
            for (int j = 0; j < oversampleFactor; j++)
            {
                rungekutteSolver(samples[s], state);
            }

            samples[s] = state[3];
        }
    }

    virtual void compute(MYFLOAT * samples, uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }

    void setParams(MYFLOAT c, MYFLOAT r){
        SetResonance(r);
        SetCutoff(c);
    }

    void SetResonance(MYFLOAT r)
    {
        // 0 to 10
        resonance = r * 4;
    }

    void SetCutoff(MYFLOAT c)
    {
        cutoff = (2.0 * PI_P * c);
    }

private:

    void calculateDerivatives(MYFLOAT input, MYFLOAT * dstate, MYFLOAT * state)
    {
        MYFLOAT satstate0 = clip(state[0], saturation, saturationInv);
        MYFLOAT satstate1 = clip(state[1], saturation, saturationInv);
        MYFLOAT satstate2 = clip(state[2], saturation, saturationInv);

        dstate[0] = cutoff * (clip(input - resonance * state[3], saturation, saturationInv) - satstate0);
        dstate[1] = cutoff * (satstate0 - satstate1);
        dstate[2] = cutoff * (satstate1 - satstate2);
        dstate[3] = cutoff * (satstate2 - clip(state[3], saturation, saturationInv));
    }

    void rungekutteSolver(MYFLOAT input, MYFLOAT * state)
    {
        int i;
        MYFLOAT deriv1[4], deriv2[4], deriv3[4], deriv4[4], tempState[4];

        calculateDerivatives(input, deriv1, state);

        for (i = 0; i < 4; i++)
            tempState[i] = state[i] + 0.5 * stepSize * deriv1[i];

        calculateDerivatives(input, deriv2, tempState);

        for (i = 0; i < 4; i++)
            tempState[i] = state[i] + 0.5 * stepSize * deriv2[i];

        calculateDerivatives(input, deriv3, tempState);

        for (i = 0; i < 4; i++)
            tempState[i] = state[i] + stepSize * deriv3[i];

        calculateDerivatives(input, deriv4, tempState);

        for (i = 0; i < 4; i++)
            state[i] += (1.0 / 6.0) * stepSize * (deriv1[i] + 2.0 * deriv2[i] + 2.0 * deriv3[i] + deriv4[i]);
    }

    MYFLOAT state[4];
    MYFLOAT saturation, saturationInv;
    int oversampleFactor;
    MYFLOAT stepSize;

};




/*
The simplified nonlinear Moog filter is based on the full Huovilainen model,
with five nonlinear (tanh) functions (4 first-order sections and a feedback).
Like the original, this model needs an oversampling factor of at least two when
these nonlinear functions are used to reduce possible aliasing. This model
maintains the ability to self oscillate when the feedback gain is >= 1.0.
References: DAFX - Zolzer (ed) (2nd ed)
Original implementation: Valimaki, Bilbao, Smith, Abel, Pakarinen, Berners (DAFX)
This is a transliteration into C++ of the original matlab source (moogvcf.m)
Considerations for oversampling:
http://music.columbia.edu/pipermail/music-dsp/2005-February/062778.html
http://www.synthmaker.co.uk/dokuwiki/doku.php?id=tutorials:oversampling
*/

class SimplifiedMoog : public Filter
{
public:

    SimplifiedMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {
        // To keep the overall level approximately constant, comp should be set
        // to 0.5 resulting in a 6 dB passband gain decrease at the maximum resonance
        // (compared to a 12 dB decrease in the original Moog model
        gainCompensation = 0.5;
        reset();

    }

    void reset() override {
        memset(stage, 0, sizeof(stage));
        memset(stageZ1, 0, sizeof(stageZ1));
        memset(stageTanh, 0, sizeof(stageTanh));
    }

    virtual ~SimplifiedMoog()
    {

    }

    // This system is nonlinear so we are probably going to create a signal with components that exceed nyquist.
    // To prevent aliasing distortion, we need to oversample this processing chunk. Where do these extra samples
    // come from? Todo! We can use polynomial interpolation to generate the extra samples, but this is expensive.
    // The cheap solution is to zero-stuff the incoming sample buffer.
    // With resampling, numSamples should be 2x the frame size of the existing sample rate.
    // The output of this filter needs to be run through a decimator to return to the original samplerate.
    virtual void compute(MYFLOAT * samples, uint32_t n) override
    {
        // Processing still happens at sample rate...
        for (int s = 0; s < n; ++s)
        {
            for (int stageIdx = 0; stageIdx < 4; ++stageIdx)
            {
                if (stageIdx)
                {
                    input = stage[stageIdx-1];
                    stageTanh[stageIdx-1] = fast_tanh(input);
                    stage[stageIdx] = (h * stageZ1[stageIdx] + h0 * stageTanh[stageIdx-1]) + (1.0 - g) * (stageIdx != 3 ? stageTanh[stageIdx] : fast_tanh(stageZ1[stageIdx]));
                }
                else
                {
                    input = samples[s] - ((4.0 * resonance) * (output - gainCompensation * samples[s]));
                    stage[stageIdx] = (h * fast_tanh(input) + h0 * stageZ1[stageIdx]) + (1.0 - g) * stageTanh[stageIdx];
                }

                stageZ1[stageIdx] = stage[stageIdx];
            }

            output = stage[3];
            SNAP_TO_ZERO(output);
            samples[s] = output;
        }
    }

    virtual void compute(MYFLOAT * samples, uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }


    void setParams(MYFLOAT c, MYFLOAT r){
        SetResonance(r);
        SetCutoff(c);
    }

    void SetResonance(MYFLOAT r)
    {
        resonance = r;
    }

    void SetCutoff(MYFLOAT c)
    {
        cutoff = c;

        // Not being oversampled at the moment... * 2 when functional
        MYFLOAT fs2 = sampleRate;

        // Normalized cutoff [0, 1] in radians: ((2*pi) * cutoff / samplerate)
        g = (2 * PI_P) * cutoff / fs2; // feedback coefficient at fs*2 because of MYFLOATsampling
        g *= PI_P / 1.3; // correction factor that allows _cutoff to be supplied Hertz

        // FIR part with gain g
        h = g / 1.3;
        h0 = g * 0.3 / 1.3;
    }

private:

    MYFLOAT output;
    MYFLOAT lastStage;

    MYFLOAT stage[4];
    MYFLOAT stageZ1[4];
    MYFLOAT stageTanh[3];

    MYFLOAT input;
    MYFLOAT h;
    MYFLOAT h0;
    MYFLOAT g;

    MYFLOAT gainCompensation;
};

/*
A digital model of the classic Moog filter was presented first by Stilson and
Smith. This model uses a cascade of one-pole IIR filters in series with a global
feedback to produce resonance. A digital realization of this filter introduces a
unit delay, effectively making it a fifth-order filter. Unfortunately, this
delay also has the effect of coupling the cutoff and resonance parameters,
uncharacteristic of the uncoupled control of the original Moog ladder. As a
compromise, a zero can be inserted at z = -0.3 inside each one pole section to
minimize the coupling the parameters (humans are not particularly sensitive to
variations in Q factor). Although fast coefficient updates can be achieved since
the nonlinearities of the Moog are not considered, the filter becomes unstable
with very large resonance values and does not enter self-oscillation.
References: Stilson and Smith (1996), DAFX - Zolzer (ed) (2nd ed)
Original implementation: Tim Stilson, David Lowenfels
*/

static MYFLOAT S_STILSON_GAINTABLE[199] =
        {
                0.999969, 0.990082, 0.980347, 0.970764, 0.961304, 0.951996, 0.94281, 0.933777, 0.924866, 0.916077,
                0.90741, 0.898865, 0.890442, 0.882141 , 0.873962, 0.865906, 0.857941, 0.850067, 0.842346, 0.834686,
                0.827148, 0.819733, 0.812378, 0.805145, 0.798004, 0.790955, 0.783997, 0.77713, 0.770355, 0.763672,
                0.75708 , 0.75058, 0.744141, 0.737793, 0.731537, 0.725342, 0.719238, 0.713196, 0.707245, 0.701355,
                0.695557, 0.689819, 0.684174, 0.678558, 0.673035, 0.667572, 0.66217, 0.65686, 0.651581, 0.646393,
                0.641235, 0.636169, 0.631134, 0.62619, 0.621277, 0.616425, 0.611633, 0.606903, 0.602234, 0.597626,
                0.593048, 0.588531, 0.584045, 0.579651, 0.575287 , 0.570953, 0.566681, 0.562469, 0.558289, 0.554169,
                0.550079, 0.546051, 0.542053, 0.538116, 0.53421, 0.530334, 0.52652, 0.522736, 0.518982, 0.515289,
                0.511627, 0.507996 , 0.504425, 0.500885, 0.497375, 0.493896, 0.490448, 0.487061, 0.483704, 0.480377,
                0.477081, 0.473816, 0.470581, 0.467377, 0.464203, 0.46109, 0.457977, 0.454926, 0.451874, 0.448883,
                0.445892, 0.442932, 0.440033, 0.437134, 0.434265, 0.431427, 0.428619, 0.425842, 0.423096, 0.42038,
                0.417664, 0.415009, 0.412354, 0.409729, 0.407135, 0.404572, 0.402008, 0.399506, 0.397003, 0.394501,
                0.392059, 0.389618, 0.387207, 0.384827, 0.382477, 0.380127, 0.377808, 0.375488, 0.37323, 0.370972,
                0.368713, 0.366516, 0.364319, 0.362122, 0.359985, 0.357849, 0.355713, 0.353607, 0.351532, 0.349457,
                0.347412, 0.345398, 0.343384, 0.34137, 0.339417, 0.337463, 0.33551, 0.333588, 0.331665, 0.329773,
                0.327911, 0.32605, 0.324188, 0.322357, 0.320557, 0.318756, 0.316986, 0.315216, 0.313446, 0.311707,
                0.309998, 0.308289, 0.30658, 0.304901, 0.303223, 0.301575, 0.299927, 0.298309, 0.296692, 0.295074,
                0.293488, 0.291931, 0.290375, 0.288818, 0.287262, 0.285736, 0.284241, 0.282715, 0.28125, 0.279755,
                0.27829, 0.276825, 0.275391, 0.273956, 0.272552, 0.271118, 0.269745, 0.268341, 0.266968, 0.265594,
                0.264252, 0.262909, 0.261566, 0.260223, 0.258911, 0.257599, 0.256317, 0.255035, 0.25375
        };

class StilsonMoog : public Filter
{
public:

    StilsonMoog(MYFLOAT sampleRate = 1.0) : Filter(sampleRate)
    {
        reset();
    }

    void reset() override {
        memset(state, 0, sizeof(state));

    }

    virtual ~StilsonMoog()
    {

    }

    virtual void compute(MYFLOAT * samples, uint32_t n) override
    {
        MYFLOAT localState;

        for (int s = 0; s < n; ++s)
        {
            // Scale by arbitrary value on account of our saturation function
            const MYFLOAT input = samples[s] * 0.65;

            // Negative Feedback
            output = 0.25 * (input - output);

            for (int pole = 0; pole < 4; ++pole)
            {
                localState = state[pole];
                output = moog_saturate(output + p * (output - localState));
                state[pole] = output;
                output = moog_saturate(output + localState);
            }

            SNAP_TO_ZERO(output);
            samples[s] = output;
            output *= Q; // Scale stateful output by Q
        }
    }

    virtual void compute(MYFLOAT * samples, uint32_t n, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        compute(samples, n);
    }


    MYFLOAT tick( MYFLOAT input, MYFLOAT c, MYFLOAT r) override
    {
        setParams(c, r);
        input *=  0.65;

        // Negative Feedback
        output = 0.25 * (input - output);

        MYFLOAT localState;
        for (int pole = 0; pole < 4; ++pole)
        {
            localState = state[pole];
            output = moog_saturate(output + p * (output - localState));
            state[pole] = output;
            output = moog_saturate(output + localState);
        }

        SNAP_TO_ZERO(output);
        auto ret  = output;
        output *= Q; // Scale stateful output by Q
        return ret;
    }


    inline void setParams(MYFLOAT c, MYFLOAT r){
        SetCutoff(c);
        SetResonance(r);
    }

    inline void SetResonance(MYFLOAT r)
    {
        r = moog_min(r, 1.);
        resonance = r;

        MYFLOAT ix;
        MYFLOAT ixfrac;
        int ixint;

        ix = p * 99;
        ixint = floor(ix);
        ixfrac = ix - ixint;

        Q = r * moog_lerp(ixfrac, S_STILSON_GAINTABLE[ixint + 99], S_STILSON_GAINTABLE[ixint + 100]);
    }

    inline void SetCutoff(MYFLOAT c)
    {
        cutoff = c;

        // Normalized cutoff between [0, 1]
        MYFLOAT fc = (cutoff) / sampleRate;
        MYFLOAT x2 = fc * fc;
        MYFLOAT x3 = fc * fc * fc;

        // Frequency & amplitude correction (Cubic Fit)
        p = -0.69346 * x3 - 0.59515 * x2 + 3.2937 * fc - 1.0072;

        SetResonance(resonance);
    }

private:

    MYFLOAT p;
    MYFLOAT Q;
    MYFLOAT state[4];
    MYFLOAT output;
};



#endif //GRAINSTORM_MOOGLADDER_H
