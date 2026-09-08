//
// Created by pr on 21.06.19.
//

#ifndef GRAINSTORM_RANDOM_H
#define GRAINSTORM_RANDOM_H


#include <cstdint>
#include <atomic>
#include "defines.h"
#include "tools.h"

#define oneUp31Bit      (double) (4.656612875245796924105750827168e-10)


#define randGab   (MYFLOAT) ((double)     \
    (((holdrand = holdrand * 214013 + 2531011) >> 1)  \
     & 0x7fffffff) * oneUp31Bit)
#define BiRandGab (MYFLOAT) ((double)     \
    (holdrand = holdrand * -214013 + 2531011) * oneUp31Bit)

#define RANDOMFLOAT(x, y) (((x) > (y) ? (y) : (x)) + DISTANCE((x), (y)) * randGab)

extern
MYFLOAT randomfloat(MYFLOAT low, MYFLOAT high);

#define FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_HURST_CONST 0.5
#define FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_BUFSIZE 15

class NoiseBase{
public:
    static MYFLOAT randomMYFLOAT(MYFLOAT min, MYFLOAT max){return min > max ? max  + (min-max) * rand() / (double)RAND_MAX : min + (max-min) * rand()/ (double) RAND_MAX;}
    virtual MYFLOAT tick(){return 0;};
    virtual void process(MYFLOAT buf[], int n, MYFLOAT gain = 1.0){
        return;
    }
protected:
    uint32_t holdrand{(uint32_t) randomMYFLOAT(0, 100000)};
};

template<typename T>
class NoiseBase2{
public:
    inline T tick(){return filter(BiRandGab);};
    void process(T buf[], int n, T gain = 1.0){
        for(int i=0;i<n;i++)
            buf[i] = tick();
    }
protected:
    virtual inline T filter(const T s) = 0;
    uint32_t holdrand{(uint32_t) NoiseBase::randomMYFLOAT(0, 100000)};
};
// +/-0.05dB above 9.2Hz @ 44,100Hz
template<typename T>
class WhiteNoise : public NoiseBase2<T>
{
protected:
    inline T filter(const T s)
    {
        return s;
    }
};

// +/-0.05dB above 9.2Hz @ 44,100Hz
template<typename T>
class PinkingFilter : public NoiseBase2<T>
{
    T b0, b1, b2, b3, b4, b5, b6;
public:
    PinkingFilter() : b0(0), b1(0), b2(0), b3(0), b4(0), b5(0), b6(0) {}

protected:
    inline T filter(const T s)
    {
        b0 = 0.99886 * b0 + s * 0.0555179;
        b1 = 0.99332 * b1 + s * 0.0750759;
        b2 = 0.96900 * b2 + s * 0.1538520;
        b3 = 0.86650 * b3 + s * 0.3104856;
        b4 = 0.55000 * b4 + s * 0.5329522;
        b5 = -0.7616 * b5 - s * 0.0168980;
        const T pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + (s * 0.5362)) * 0.11;
        b6 = s * 0.115926;
        return pink;
    }
};
template<typename T>
class BrowningFilter : public NoiseBase2<T>
{
    T l;
public:
    BrowningFilter() : l(0) {}

protected:
    inline T filter(const T s)
    {
        T brown = (l + (0.02 * s)) / 1.02;
        l = brown;
        return brown * 3.5; // compensate for gain
    }
};

#include <random>
#include <typeinfo>
#include <vector>

class PinkNoise2 : public NoiseBase{
public:
    PinkNoise2(){
        for ( int i = 0; i < _b; i++ )
        {
            _u[i] = ( MYFLOAT ) rand ( ) / ( MYFLOAT ) ( RAND_MAX ) - 0.5f;
        }
    }
    inline MYFLOAT tick() override {
        return ran1f();
    }

    void process(MYFLOAT buf[], int n, MYFLOAT gain = 1.0) override {
        for(int i=0;i<n;i++)
            buf[i] = ran1f()*gain;
    }

    static std::vector<MYFLOAT> white(int length)
    {
        std::vector<MYFLOAT> whitelist(length);
        std::random_device randev;
        std::mt19937 gen(randev());
        std::uniform_real_distribution<> unf_dist(0, length);
        for(auto j : whitelist)
        {
            whitelist.push_back(unf_dist(gen));
        }
        return whitelist;
    }

private:
    inline void cdelay2 ( int m, int *q )

/****************************************************************************80
//
//  Purpose:
//
//    CDELAY2 is a circular buffer implementation of M-fold delay.
//
//  Example:
//
//    Suppose we call CDELAY2 12 times, always with M = 3, and with
//    Q having the input value 3 on the first call.  Q will go through
//    the following sequence of values over the 12 calls:
//
//    I   M  Qin  Qout
//
//    1   3   3   2
//    2   3   2   1
//    3   3   1   0
//    4   3   0   3
//    5   3   3   2
//    6   3   2   1
//    7   3   1   0
//    8   3   0   3
//    9   3   3   2
//   10   3   2   1
//   11   3   1   0
//   12   3   0   3
//
//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    31 May 2010
//
//  Author:
//
//    Original C version by Sophocles Orfanidis.
//    This C++ version by John Burkardt.
//
//  Reference:
//
//    Sophocles Orfanidis,
//    Introduction to Signal Processing,
//    Prentice-Hall, 1995,
//    ISBN: 0-13-209172-0,
//    LC: TK5102.5.O246.
//
//  Parameters:
//
//    Input, int M, the maximum value that Q can have.
//
//    Input/output, int *Q, a counter which is decremented on every call.
//    However, the value "after" 0 is M.
*/
    {
//
//  Decrement the offset.
//
        *q = *q - 1;
//
//  Q = - 1 wraps to Q = M.
//
        wrap2 ( m, q );

        return;
    }

    inline MYFLOAT ran1f ()

/****************************************************************************80
//
//  Purpose:
//
//    RAN1F is a 1/F random number generator.
//
//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    31 May 2010
//
//  Author:
//
//    Original C version by Sophocles Orfanidis.
//    This C++ version by John Burkardt.
//
//  Reference:
//
//    Sophocles Orfanidis,
//    Introduction to Signal Processing,
//    Prentice-Hall, 1995,
//    ISBN: 0-13-209172-0,
//    LC: TK5102.5.O246.
//
//  Parameters:
//
//    Input, int B, the number of signals to combine.
//    For this algorithm, B cannot be more than 31!
//
//    Input/output, double U[B], the signals to combine.  It is expected
//    that each of the initial values of U will be drawn from a distribution
//    with zero mean.
//
//    Input/output, int Q[B], a set of counters that determine when each
//    entry of U is to be updated.
//
//    Output, double RAN1F, the value.
*/
    {
        MYFLOAT y = 0.0;

        int j = 1;
        for ( int i = 0; i < _b; i++ )
        {
            y = y + ranh ( j, _u+i, _q+i );
            j = j * 2;
        }
            y = y / ( MYFLOAT ) _b;

        return y;
    }
//****************************************************************************80

    inline MYFLOAT ranh ( int d, MYFLOAT *u, int *q )

/****************************************************************************80
//
//  Purpose:
//
//    RANH is a hold random number generator of period D.
//
//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    31 May 2010
//
//  Author:
//
//    Original C version by Sophocles Orfanidis.
//    This C++ version by John Burkardt.
//
//  Reference:
//
//    Sophocles Orfanidis,
//    Introduction to Signal Processing,
//    Prentice-Hall, 1995,
//    ISBN: 0-13-209172-0,
//    LC: TK5102.5.O246.
//
//  Parameters:
//
//    Input, int D, the hold period.  D must be at least 1.
//
//    Input/output, double *U, a value to be held until Q has decremented
//    to 0, when Q will be reset to D, and U will be randomly reset.
//
//    Input/output, int *Q, a counter which is decremented by 1 on each call
//    until reaching 0.
//
//    Output, double RANH, the input value of U.
*/
    {
//  Hold this sample for D calls.
//
        MYFLOAT y = *u;
//
//  Decrement Q and wrap mod D.
//
        cdelay2 ( d - 1, q );
//
//  Every D calls, get a new U with zero mean.
//
        if ( *q == 0 )
        {
            *u = BiRandGab;
        }
        return y;
    }



    inline void wrap2 ( int m, int *q )

/****************************************************************************80
//
//  Purpose:
//
//    WRAP2 is a circular wrap of the pointer offset Q.
//
//  Discussion:
//
//    Input values of Q between 0 and M are "legal".
//    Values of Q below 0 are incremented by M + 1 until they are legal.
//    Values of Q above M are decremented by M + 1 until they become legal.
//    The legal value is the output value of the function.
//
//  Example:
//
//    M  Qin  Qout
//
//    3  -5   3
//    3  -4   0
//    3  -3   1
//    3  -2   2
//    3  -1   3
//    3   0   0
//    3   1   1
//    3   2   2
//    3   3   3
//    3   4   0
//    3   5   1
//    3   6   2
//    3   7   3
//    3   8   0
//
//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    31 May 2010
//
//  Author:
//
//    Original C version by Sophocles Orfanidis.
//    This C++ version by John Burkardt.
//
//  Reference:
//
//    Sophocles Orfanidis,
//    Introduction to Signal Processing,
//    Prentice-Hall, 1995,
//    ISBN: 0-13-209172-0,
//    LC: TK5102.5.O246.
//
//  Parameters:
//
//    Input, int M, the maximum acceptable value for outputs.
//    M must be at least 0.
//
//    Input/output, int *Q, the value to be wrapped.
*/
    {
        if ( m < 0 )
        {
            return;
        }
//
//  When Q = M + 1, it wraps to Q = 0.
//
        while ( m < *q )
        {
            *q = *q - m - 1;
        }
//
//  When Q = - 1, it wraps to Q = M.
//
        while ( *q < 0 )
        {
            *q = *q + m + 1;
        }
        return;
    }

    int _b{30};
    MYFLOAT _u[30];
    int _q[30]{};
};


class PinkNoise : public NoiseBase{
public:
    PinkNoise(MYFLOAT _amp = 1.0);

    void process(MYFLOAT *in, int n, MYFLOAT gain);

    void Reset();

    MYFLOAT tick();
private:
    MYFLOAT amp;
    unsigned long pink_Rows[30];
    unsigned long pink_RunningSum;   /* Used to optimize summing of generators. */
    uint32_t pink_Index;        /* Incremented each sample. */
    uint32_t pink_IndexMask;    /* Index wrapped by ANDing with this mask. */
    MYFLOAT pink_Scalar;       /* Used to scale within range of -1.0 to +1.0 */
    unsigned long randSeed{22222};  /* Change this for different random sequences. */
};


#define PINK_NOISE_NUM_STAGES 3

class PinkNoise3 : public NoiseBase{
public:
    PinkNoise3() {
        srand ( time(NULL) ); // initialize random generator
        clear();
    }

    void clear() {
        for( size_t i=0; i< PINK_NOISE_NUM_STAGES; i++ )
            state[ i ] = 0.0;
    }

    inline MYFLOAT tick() override {
        static const MYFLOAT RMI2 = 2.0 / MYFLOAT(RAND_MAX); // + 1.0; // change for range [0,1)
        static const MYFLOAT offset = A[0] + A[1] + A[2];

        // unrolled loop
        ;
        MYFLOAT temp = holdrand = holdrand * -214013 + 2531011;//MYFLOAT( rand() );
        state[0] = P[0] * (state[0] - temp) + temp;
        temp = holdrand = holdrand * -214013 + 2531011;//MYFLOAT( rand() );
        state[1] = P[1] * (state[1] - temp) + temp;
        temp = holdrand = holdrand * -214013 + 2531011;//MYFLOAT( rand() );
        state[2] = P[2] * (state[2] - temp) + temp;
        return ( A[0]*state[0] + A[1]*state[1] + A[2]*state[2] )*RMI2 - offset;
    }

    void process(MYFLOAT *in, int n, MYFLOAT gain) override {
        for(int i=0;i<n;i++)
            in[i] = tick() * gain;
    }

protected:
    MYFLOAT state[ PINK_NOISE_NUM_STAGES ];
    constexpr static const MYFLOAT A[ PINK_NOISE_NUM_STAGES ]= { 0.02109238, 0.07113478, 0.68873558 };;
    constexpr static const MYFLOAT P[ PINK_NOISE_NUM_STAGES ] = { 0.3190,  0.7756,  0.9613  };
};


class PinkNoise4 : public NoiseBase{
public:
    PinkNoise4() {
        srand ( time(NULL) ); // initialize random generator
        clear();
    }

    void clear() {
        for( size_t i=0; i< PINK_NOISE_NUM_STAGES; i++ )
            state[ i ] = 0.0;
    }

    inline MYFLOAT tick() override {
        static const MYFLOAT RMI2 = 2.0 / MYFLOAT(RAND_MAX); // + 1.0; // change for range [0,1)
        static const MYFLOAT offset = A[0] + A[1] + A[2];

        // unrolled loop
        ;
        MYFLOAT temp = MYFLOAT( rand() );
        state[0] = P[0] * (state[0] - temp) + temp;
        temp = holdrand = MYFLOAT( rand() );
        state[1] = P[1] * (state[1] - temp) + temp;
        temp = holdrand = MYFLOAT( rand() );
        state[2] = P[2] * (state[2] - temp) + temp;
        return ( A[0]*state[0] + A[1]*state[1] + A[2]*state[2] )*RMI2 - offset;
    }

    void process(MYFLOAT *in, int n, MYFLOAT gain) override {
        for(int i=0;i<n;i++)
            in[i] = tick() * gain;
    }

protected:
    MYFLOAT state[ PINK_NOISE_NUM_STAGES ];
    constexpr static const MYFLOAT A[ PINK_NOISE_NUM_STAGES ]= { 0.02109238, 0.07113478, 0.68873558 };;
    constexpr static const MYFLOAT P[ PINK_NOISE_NUM_STAGES ] = { 0.3190,  0.7756,  0.9613  };
};


template<typename T>
class pinknoise_frac {
public:
    pinknoise_frac() {
        setParams(FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_HURST_CONST,
                  FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_BUFSIZE);
        _holdrand = NoiseBase::randomMYFLOAT(0, 100000);
    }

    /**
     * Set the pink noise parameters.
     * @param[in] param1 The Hurst constant, between 0 and 0.9999 (fractal dimension).
     * @param[in] length The fractal noise generator size will be 2^length.
     */
    void setParams(T param1, long length) {
        pfn1_param = param1;
        pfn1_length = (1 << length);
        if (pfn1_slot.size() != pfn1_length) {
            pfn1_slot.resize(pfn1_length, 0);
            pfn1_count = 0;
        }
        mute();
    }

    void mute() {
        std::fill(pfn1_slot.begin(), pfn1_slot.end(), 0);
        pfn1_count = 0;
    }

    inline T process() {
        if (pfn1_count == 0) {
            fractal(pfn1_slot.data(), pfn1_length, pfn1_param);
            pfn1_count = pfn1_length;
        }
        pfn1_count--;
        return pfn1_slot.data()[pfn1_length - (pfn1_count + 1)];
    }

private:
    /**
     * generate fractal pattern using Midpoint Displacement Method
     * v: buffer of MYFLOATs to output fractal pattern to
     * N: length of v, MUST be integer power of 2 (ie 128, 256, ...)
     * H: Hurst constant, between 0 and 0.9999 (fractal dimension)
     * based on the tap-plugins
     */
    void fractal(T *v, int N, T H) {
        int l = N, k, c;
        T r = 2.0 * (H * H) + 0.3;
        v[0] = 0;
        while (l > 1) {
            k = N / l;
            for (c = 0; c < k; c++) {
                v[c * l + l / 2] = (v[c * l] + v[((c + 1) * l) % N]) / 2.0
                                   +
                                   2.0 * r * ((T) std::rand() - (T) RAND_MAX / 2.0) / (T) RAND_MAX;
                if (v[c * l + l / 2] < -1.) v[c * l + l / 2] = -1.;
                if (v[c * l + l / 2] > 1.) v[c * l + l / 2] = 1.;
            }
            l /= 2;
            r /= std::pow((T) 2., H);
        }
    }

    T pfn1_param;
    long pfn1_length, pfn1_count;
    std::vector<T> pfn1_slot;
    uint32_t _holdrand;
};


class Spline {
public:
    Spline();

    Spline(int sr = 48000, MYFLOAT rangemin = 0.f,
           MYFLOAT rangemax = 1.f, MYFLOAT cpsmin = .1f, MYFLOAT cpsmax = 3.f);

    void Init(int sr = 48000, MYFLOAT rangemin = 0.f,
              MYFLOAT rangemax = 1.f, MYFLOAT cpsmin = .1f, MYFLOAT cpsmax = 3.f);

    MYFLOAT Tick();

    void SetUp(MYFLOAT cpsmin = .1f, MYFLOAT cpsmax = 3.f, MYFLOAT rangemin = 0.f,
               MYFLOAT rangemax = 1.f) {
        rangeMin = rangemin;
        rangeMax = rangemax;
        cpsMin = cpsmin;
        cpsMax = cpsmax;
    };
    bool initialized;

private:
    bool init;
    MYFLOAT rangeMin, rangeMax, cpsMin, cpsMax;
    double si;
    double phs;
    MYFLOAT num0, num1, num2, df0, df1, c3, c2, onedsr;
    uint32_t holdrand{};
};


#define RANDAMOUNTAMP   1.59055f
#define RANDAMOUNTFREQ  0.629921f
#define AMPMINRATE      1.f
#define AMPMAXRATE      3.f
#define CPSMINRATE      1.19377f
#define CPSMAXRATE      2.28100f



class Vibrato {
public:
    Vibrato() = default;
    void setSinewave(MYFLOAT* sine) { table = sine; }

    void Init(int sr, MYFLOAT averageFreq, MYFLOAT averageAmp, MYFLOAT randAmountAmp = RANDAMOUNTAMP,
              MYFLOAT randAmountFreq = RANDAMOUNTFREQ, MYFLOAT cpsMinRate = CPSMINRATE,
              MYFLOAT cpsMaxRate = CPSMAXRATE, MYFLOAT ampMinRate = AMPMINRATE,
              MYFLOAT ampMaxRate = AMPMAXRATE) {
        _cpsMinRate = cpsMinRate, _cpsMaxRate = cpsMaxRate, _ampMinRate = ampMinRate, _ampMaxRate = ampMaxRate, _AverageFreq = averageFreq, _AverageAmp = averageAmp;
        xcpsAmpRate = randGab * (cpsMaxRate - cpsMinRate) + cpsMinRate;
        xcpsFreqRate = randGab * (ampMaxRate - ampMinRate) + ampMinRate;
        krate = sr / (MYFLOAT) KSAMPLES;
        tablenUPkr = WINDOW_SIZE / krate;//p->tablenUPkr = p->tablen * CS_ONEDKR;
        kicvt = FMAXLEN / krate;
        _randAmountAmp = randAmountAmp;
        _randAmountFreq = randAmountFreq;
    }

    void reset() {
        phs = num1amp = num2amp = num1freq = num2freq = dfdmaxAmp = dfdmaxFreq = 0.0f;
        phsAmpRate = phsFreqRate = 0;
    }


    MYFLOAT tick() {
        MYFLOAT *ftab, fract, v1;
        MYFLOAT RandAmountAmp, RandAmountFreq;

        RandAmountAmp = (num1amp + phsAmpRate * dfdmaxAmp) *
                        _randAmountAmp;
        RandAmountFreq = (num1freq + phsFreqRate * dfdmaxFreq) *
                         _randAmountFreq;

        fract = (MYFLOAT) (phs - (int32_t) phs);
        ftab = table + (int32_t) phs;
        v1 = *ftab++;
        MYFLOAT ret = (v1 + (*ftab - v1) * fract) *
                    (_AverageAmp * powf(2.f, RandAmountAmp));
        MYFLOAT inc = (_AverageFreq * powf(2.f, RandAmountFreq)) * tablenUPkr;
        phs += inc;
        while (phs >= WINDOW_SIZE)
            phs -= WINDOW_SIZE;
        while (phs < 0.0)
            phs += WINDOW_SIZE;
        phsAmpRate += (int32_t) (xcpsAmpRate * kicvt);
        if (phsAmpRate >= MAXLEN) {
            xcpsAmpRate = randGab * (_ampMaxRate - _ampMinRate) +
                          _ampMinRate;
            phsAmpRate &= PHMASK;
            num1amp = num2amp;
            num2amp = BiRandGab;
            dfdmaxAmp = (num2amp - num1amp) / FMAXLEN;
        }
        phsFreqRate += (int32_t) (xcpsFreqRate * kicvt);
        if (phsFreqRate >= MAXLEN) {
            xcpsFreqRate = randGab * (_cpsMaxRate - _cpsMinRate) +
                           _cpsMinRate;
            phsFreqRate &= PHMASK;
            num1freq = num2freq;
            num2freq = BiRandGab;
            dfdmaxFreq = (num2freq - num1freq) / FMAXLEN;
        }
        return ret;
    }

private:
    MYFLOAT tablenUPkr, kicvt, krate;
    MYFLOAT xcpsAmpRate, xcpsFreqRate;
    MYFLOAT _AverageFreq, _AverageAmp, _cpsMinRate, _cpsMaxRate, _ampMinRate, _ampMaxRate, _randAmountAmp, _randAmountFreq;
    MYFLOAT phs;
    int phsFreqRate, phsAmpRate;
    MYFLOAT dfdmaxFreq, dfdmaxAmp;
    MYFLOAT *table;
    MYFLOAT num1amp, num2amp, num1freq, num2freq;
    uint32_t holdrand{};
};


class SimpleVibrato {
public:
    SimpleVibrato(int sr, MYFLOAT* sinewave, MYFLOAT rate = 0.f, MYFLOAT depth = 0.f);

    MYFLOAT tick() {
        _offset += _phinc;
        if (_offset >= 1.0)
            _offset -= 1.0;
        return _sine[PHS2INT(_offset)];
    }

    MYFLOAT tick(MYFLOAT rate = 0.0) {
        _offset += rate * _onedsr;
        if (_offset >= 1.0)
            _offset -= 1.0;
        return _sine[PHS2INT(_offset)];
    }

private:
    MYFLOAT _depth;
    MYFLOAT _offset;
    MYFLOAT *_sine;
    MYFLOAT _rate;
    MYFLOAT _phinc;
    MYFLOAT _onedsr;
};
/*

class Vibrato{
public:
    Vibrato(MYFLOAT averageFreq, MYFLOAT averageAmp, MYFLOAT cpsMinRate, MYFLOAT cpsMaxRate, MYFLOAT ampMinRate, MYFLOAT ampMaxRate){
        _cpsMinRate = cpsMinRate, _cpsMaxRate = cpsMaxRate, _ampMinRate = ampMinRate, _ampMaxRate = ampMaxRate, _AverageFreq = averageFreq, _AverageAmp = averageAmp;
        xcpsAmpRate = randGab *(cpsMaxRate - cpsMinRate) + cpsMinRate;
        xcpsFreqRate = randGab *(ampMaxRate - ampMinRate) + ampMinRate;
        //p->tablenUPkr = p->tablen * CS_ONEDKR;
        num1amp = num2amp = num1freq = num2freq = 0.0f;
    }


    void compute()
    {
        MYFLOAT      phs, inc;
        MYFLOAT       *ftab, fract, v1;
        MYFLOAT       RandAmountAmp,RandAmountFreq;

        RandAmountAmp = (num1amp + phsAmpRate * dfdmaxAmp) *
                        randAmountAmp ;
        RandAmountFreq = (num1freq + phsFreqRate * dfdmaxFreq) *
                         randAmountFreq ;

        fract = (MYFLT) (phs - (int32_t)phs);
        ftab = table + (int32_t)phs;
        v1 = *ftab++;
        *p->out = (v1 + (*ftab - v1) * fract) *
                  (_AverageAmp * powf(2.f,RandAmountAmp));
        inc = ( _AverageFreq * powf(2.f,RandAmountFreq)) * tablenUPkr;
        phs += inc;
        while (phs >= WINDOW_SIZE)
            phs -= WINDOW_SIZE;
        while (phs < 0.0 )
            phs += WINDOW_SIZE;
        phsAmpRate += (int32_t)(xcpsAmpRate * CS_KICVT);
        if (phsAmpRate >= MAXLEN) {
            xcpsAmpRate =  randGab  * (_ampMaxRate - _ampMinRate) +
                           _ampMinRate;
            phsAmpRate &= PHMASK;
            num1amp = num2amp;
            num2amp = BiRandGab ;
            dfdmaxAmp = (num2amp - num1amp) / FMAXLEN;
        }
        phsFreqRate += (int32_t)(xcpsFreqRate * CS_KICVT);
        if (phsFreqRate >= MAXLEN) {
            xcpsFreqRate =  randGab  * (_cpsMaxRate - _cpsMinRate) +
                            _cpsMinRate;
            phsFreqRate &= PHMASK;
            num1freq = num2freq;
            num2freq = BiRandGab;
            dfdmaxFreq = (num2freq - num1freq) / FMAXLEN;
        }
    }
private:
    MYFLOAT onedtablelen;
    MYFLOAT xcpsAmpRate, xcpsFreqRate;
    MYFLOAT _AverageFreq, _AverageAmp, _cpsMinRate, _cpsMaxRate, _ampMinRate, _ampMaxRate, dfdmaxAmp,  randAmountAmp;
    MYFLOAT phs;
    int phsFreqRate,phsAmpRate;
    MYFLOAT  dfdmaxFreq, randAmountFreq;
    MYFLOAT table[WINDOW_SIZE];
    MYFLOAT   num1amp, num2amp, num1freq, num2freq;
};

*/

template<typename T>
class gaussTrig {
public:
    void setSamplerate(MYFLOAT sr) {
        _sr = sr;
    }

    void setFreq(MYFLOAT f) {
        _prvfreq = _freq = f;
    }

    void setDev(MYFLOAT d) {
        _prvdev = _dev = d;
    }


    void setFreq2(MYFLOAT f) {
        if(_prvfreq != f){
            _count = 0;
        }
        _prvfreq = _freq = f;
    }

    void setDev2(MYFLOAT d) {
        if(_prvdev != d){
            _count = 0;
        }
        _prvdev = _dev = d;
    }

    void setAmp(T amp) {
        _amp = amp;
    }

    T tick(){/*
    if (_prvfreq != _freq || _prvdev != _dev) {
        _prvdev = _dev;

        int32_t nextsamps = (int32_t) (_sr / _freq);
        MYFLOAT r1 = randGab;
        MYFLOAT r2 = randGab;
        MYFLOAT nextcount = sqrtf(-2.0f * logf(r1)) * sinf(r2 * TWOPI_F_P);
        if (nextcount < -1.0) {
            MYFLOAT diff = -1.0f - nextcount;
            nextcount = (1.0f < -1.0f + diff ? 1.0f : -1.0f + diff);
        } else if (nextcount > 1.0) {
            MYFLOAT diff = nextcount - 1.0f;
            nextcount = (-1.0f > 1.0f - diff ? -1.0f : 1.0f - diff);
        }
        _count = (int32_t) (nextsamps + nextcount * _dev * nextsamps);

        _prvfreq = _freq;
    }
*/

        if (_count <= 0) {
            int32_t nextsamps = (int32_t) (_sr / _prvfreq);
            MYFLOAT r1 = randGab;
            MYFLOAT r2 = randGab;
            MYFLOAT nextcount = sqrtf(-2.0f * logf(r1)) * sinf(r2 * TWOPI_F_P);
            if (nextcount < -1.0) {
                MYFLOAT diff = -1.0f - nextcount;
                nextcount = (1.0f < -1.0f + diff ? 1.0f : -1.0f + diff);
            } else if (nextcount > 1.0) {
                MYFLOAT diff = nextcount - 1.0f;
                nextcount = (-1.0f > 1.0f - diff ? -1.0f : 1.0f - diff);
            }
            _count = (int32_t) (nextsamps + nextcount * _dev * nextsamps);
            _count--;
            return _amp;
        } else {
            _count--;
            return 0;
        }
    }

    void reset(){
        _count = 0;
    }

    void setCount(int32_t c){
        _count = c;
    }


    int32_t Count(){
        return _count;
    }
private:
    MYFLOAT _prvfreq{}, _prvdev{}, _freq{}, _dev{}, _sr{48000.f};
    T _amp{1};
    int32_t _count{}, holdrand{(int32_t) NoiseBase::randomMYFLOAT(0, 1000000)};
};

#endif //GRAINSTORM_RANDOM_H
