//
// Created by pr on 21.06.19.
//
#include <random>
#include "random.h"
#include "grainstorm.h"
#include <app.h>

struct Seed{
    Seed(){
        srand(time(0));
    }
};
static Seed seed;

#define RANDOMDOUBLE(x,y) ((y) > (x) ? (x) + ((y) - (x)) * rand() / (MYFLOAT) RAND_MAX : (y) + ((x)-(y)) * rand() / (MYFLOAT) RAND_MAX)


MYFLOAT tsl::random::randomfloat(MYFLOAT _x, MYFLOAT y) {
    return RANDOMDOUBLE(_x,y);
};



static inline uint32_t rand(uint32_t held) {
    uint32_t val = (1103515245 * held + 12345) % RANDMAX;
    return val;
}



MYFLOAT tsl::random::Spline::tick() {
    MYFLOAT f0 = num0;
    if (init) {
        init = false;
        goto next;
    }
    phs += si;
    if (phs >= 1.0) {
        next:
        si = (randGab * (cpsMax - cpsMin) + cpsMin) * _onedsr;
        while (phs > 1.0)
            phs -= 1.0;
        f0 = num0 = num1;
        auto f1 = num1 = num2;
        auto f2 = num2 = randGab;
        df0 = df1;
        df1 = (f2 - f0) * .5;
        auto slope = f1 - f0;
        auto resd0 = df0 - slope;
        auto resd1 = df1 - slope;
        c3 = resd0 + resd1;
        c2 = -(resd1 + 2.0 * resd0);
    }
    auto ret = (((c3 * phs + c2) * phs + df0) * phs + f0) *
               (rangeMax - rangeMin) + rangeMin;
    return ret;
}


double randomdouble(double low, double high) {
    double f = (double) rand() / RAND_MAX;
    return low + f * (high - low);
    std::uniform_real_distribution<double> unif(low, high);
    std::default_random_engine re;
    return unif(re);
}

tsl::random::Vibrato::Vibrato() {
    table = getsinewave();
    reset();
}


/* Calculate pseudo-random 32 bit number based on linear congruential method. */
static unsigned long GenerateRandomNumber(void) {
    static unsigned long randSeed = 22222;  /* Change this for different random sequences. */
    randSeed = (randSeed * 196314165) + 907633515;
    return randSeed;
}

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24)
#define PINK_RANDOM_SHIFT      ((sizeof(long)*8)-PINK_RANDOM_BITS)

tsl::random::PinkNoise2::PinkNoise2(float
                     _amp) {
    amp = _amp;
    Reset();
}

void tsl::random::PinkNoise2::Reset() {
    int32_t i;
    long pmax;
    pink_Index = 0;
    pink_IndexMask = (1 << 20) - 1;
/* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
    pmax = (20 + 1) * (1 << (PINK_RANDOM_BITS - 1));
    pink_Scalar = 1.0f / pmax;
/* Initialize rows. */
    for (i = 0; i < 20; i++) pink_Rows[i] = 0;
    pink_RunningSum = 0;
}

/* Generate numRows octave-spaced white bands and sum to pink noise. */
void tsl::random::PinkNoise2::Compute(float *in, int32_t size) {
    long newRandom;
    long sum;
    float output;

    for (int32_t i = 0; i < size; i++) {
/* Increment and mask index. */
        pink_Index = (pink_Index + 1) & pink_IndexMask;

/* If index is zero, don't update any random values. */
        if (pink_Index != 0) {
            /* Determine how many trailing zeros in PinkIndex. */
            /* This algorithm will hang if n==0 so test first. */
            int32_t numZeros = 0;
            int32_t n = pink_Index;
            while ((n & 1) == 0) {
                n = n >> 1;
                numZeros++;
            }

            /* Replace the indexed ROWS random value.
             * Subtract and add back to RunningSum instead of adding all the random
             * values together. Only one changes each time.
             */
            pink_RunningSum -= pink_Rows[numZeros];
            newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
            pink_RunningSum += newRandom;
            pink_Rows[numZeros] = newRandom;
        }

/* Add extra white noise value. */
        newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
        sum = pink_RunningSum + newRandom;

/* Scale to range of -1.0 to 0.9999. */
        in[i] = pink_Scalar * sum * amp;
    }
}

float tsl::random::PinkNoise2::Tick() {
    long newRandom;
    long sum;
    float output;

/* Increment and mask index. */
    pink_Index = (pink_Index + 1) & pink_IndexMask;

/* If index is zero, don't update any random values. */
    if (pink_Index != 0) {
        /* Determine how many trailing zeros in PinkIndex. */
        /* This algorithm will hang if n==0 so test first. */
        int32_t numZeros = 0;
        int32_t n = pink_Index;
        while ((n & 1) == 0) {
            n = n >> 1;
            numZeros++;
        }

        /* Replace the indexed ROWS random value.
         * Subtract and add back to RunningSum instead of adding all the random
         * values together. Only one changes each time.
         */
        pink_RunningSum -= pink_Rows[numZeros];
        newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
        pink_RunningSum += newRandom;
        pink_Rows[numZeros] = newRandom;
    }

/* Add extra white noise value. */
    newRandom = ((long) GenerateRandomNumber()) >> PINK_RANDOM_SHIFT;
    sum = pink_RunningSum + newRandom;

/* Scale to range of -1.0 to 0.9999. */
    return pink_Scalar * sum * amp;
}

tsl::random::SimpleVibrato::SimpleVibrato(MYFLOAT sr, MYFLOAT rate, MYFLOAT depth) :_onedsr(1./sr) {
    _sine = getsinewave();
    _offset = 0;
    rate = _rate;
    _depth = depth;
    _phinc = rate / sr;
}


uint32_t GetRandomSeedFromTime(void)
{
    return (uint32_t) tsl::time::nanosecondsSinceEpoch();
}


#define MATRIX_A    0x9908B0DFU     /* constant vector a */
#define UPPER_MASK  0x80000000U     /* most significant w-r bits */
#define LOWER_MASK  0x7FFFFFFFU     /* least significant r bits */

#define UInt32toFlt(x) ((double)(x) * (1.0 / 4294967295.03125))
#define unirand(c) ((float) UInt32toFlt(RandMT()))



void tsl::random::Random::MT_update_state() {
    /* mag01[x] = x * MATRIX_A  for x=0,1 */
    const uint32_t mag01[2] = {(uint32_t) 0, (uint32_t) MATRIX_A};
    int32_t i;

    for (i = 0; i < (N - M); i++) {
        uint32_t y = (mt[i] & UPPER_MASK) | (mt[i + 1] & LOWER_MASK);
        mt[i] = mt[i + M] ^ (y >> 1) ^ mag01[y & (uint32_t) 1];
    }
    for (; i < (N - 1); i++) {
        uint32_t y = (mt[i] & UPPER_MASK) | (mt[i + 1] & LOWER_MASK);
        mt[i] = mt[i + (M - N)] ^ (y >> 1) ^ mag01[y & (uint32_t) 1];
    }
    uint32_t y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
    mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & (uint32_t) 1];
}

tsl::random::Random::Random() {
    //randSeed1 = 15937;
    //uint32_t tmp = (uint32_t) GetRandomSeedFromTime();
    //while (tmp >= (uint32_t) 0x7FFFFFFE)
    //    tmp -= (uint32_t) 0x7FFFFFFE;
    //randSeed2 = ((int) tmp + 1);
    SeedRandMT(nullptr, (uint32_t) 5489);
}

void tsl::random::Random::SeedRandMT(const uint32_t *initKey, uint32_t keyLength) {
    /* if array is NULL, use length parameter as simple 32 bit seed */
    uint32_t x = (initKey == nullptr ? keyLength : (uint32_t) 19650218);
    mt[0] = x;
    for (int32_t i = 1; i < N; i++) {
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        x = ((uint32_t) 1812433253 * (x ^ (x >> 30)) + (uint32_t) i);
        mt[i] = x;
    }
    mti = N;
    if (initKey == NULL)
        return;
    int32_t i = 0;
    int32_t j = 0;
    int32_t k = (N > (int32_t) keyLength ? N : (int32_t) keyLength);
    for (; k; k--) {
        x = mt[i++];
        mt[i] = (mt[i] ^ ((x ^ (x >> 30)) * (uint32_t) 1664525))
                + initKey[j] + (uint32_t) j;   /* non linear */
        if (i == (N - 1)) {
            mt[0] = mt[N - 1];
            i = 0;
        }
        if (++j >= (int32_t) keyLength)
            j = 0;
    }
    for (k = (N - 1); k; k--) {
        x = mt[i++];
        mt[i] = (mt[i] ^ ((x ^ (x >> 30)) * (uint32_t) 1566083941))
                - (uint32_t) i;                /* non linear */
        if (i == (N - 1)) {
            mt[0] = mt[N - 1];
            i = 0;
        }
    }
    /* MSB is 1; assuring non-zero initial array */
    mt[0] = (uint32_t) 0x80000000U;
}

uint32_t tsl::random::Random::RandMT() {
    int32_t i = mti;
    if (i >= N) {                   /* generate N words at one time */
        MT_update_state();
        i = 0;
    }
    uint32_t y = mt[i];
    mti = i + 1;
    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & (uint32_t)
            0x9D2C5680U;
    y ^= (y << 15) & (uint32_t)
            0xEFC60000U;
    y ^= (y >> 18);

    return y;
}


inline float tsl::random::Random::unifrand(float range)
{
    return (range * unirand(csound));
}

/* linear distribution routine */

inline float tsl::random::Random::linrand(float range)
{
    uint32_t r1 = RandMT();
    uint32_t r2 = RandMT();

    return ((float)UInt32toFlt(r1 < r2 ? r1 : r2) * range);
}

/* triangle distribution routine */

inline float tsl::random::Random::trirand(float range)
{
    auto  r1 = (uint64_t)RandMT();
    r1 += (uint64_t)RandMT();

    return ((float) ((double)((int64_t)r1 - (int64_t)0xFFFFFFFFU)
                     * (1.0 / 4294967295.03125)) * range);
}

/* exponential distribution routine */

float tsl::random::Random::exprand(float lambda)
{
    uint32_t  r1;

    if (UNLIKELY(lambda < FL(0.0))) return (FL(0.0)); /* for safety */

    do {
      r1 = RandMT();
    } while (!r1);

    return -((float)log(UInt32toFlt(r1)) * lambda);
}

/* bilateral exponential distribution routine */

float tsl::random::Random::biexprand(float range)
{
    int32_t r1;

    if (UNLIKELY(range < FL(0.0))) return (FL(0.0)); /* For safety */

    while ((r1 = (int32_t)RandMT())==0);

    if (r1 < (int32_t)0) {
      return -(log(-(r1) * (FL(1.0) / FL(2147483648.0))) * range);
    }
    return (log(r1 * (FL(1.0) / FL(2147483648.0))) * range);
}



/* cauchy distribution routine */

float tsl::random::Random::cauchrand(float a)
{
    uint32_t  r1;
    float     x;

    do {
      r1 = RandMT(); /* Limit range artificially */
    } while (r1 > (uint32_t)2143188560U && r1 < (uint32_t)2151778735U);
    x = tan((float)r1 * (PI_F_P / FL(4294967295.0))) * (FL(1.0) / FL(318.3));
    return (x * a);
}

/* positive cauchy distribution routine */

float tsl::random::Random::pcauchrand(float a)
{
    uint32_t  r1;
    do {
      r1 = RandMT();
    } while (r1 > (uint32_t)4286377121U);      /* Limit range artificially */
    float x = tan((float)r1 * (PI_F_P*.5f) / FL(4294967295.0))
      * (FL(1.0) / FL(318.3));
    return (x * a);
}


/* weibull distribution routine */

float tsl::random::Random::weibrand(float s, float t)
{
    uint32_t  r1;
    double    r2;

    if (UNLIKELY(t <= FL(0.0))) return FL(0.0);

    do {
      r1 = RandMT();
    } while (!r1 || r1 == (uint32_t)0xFFFFFFFFU);

    r2 = 1.0 - ((double)r1 * (1.0 / 4294967295.0));

    return (s * (float)pow(-(log(r2)), (1.0 / (double)t)));
}


/* Poisson distribution routine */

float tsl::random::Random::poissrand(float lambda)
{
    float r1, r2, r3;

    if (UNLIKELY(lambda < FL(0.0))) return FL(0.0);

    r1 = unirand();
    r2 = exp(-lambda);
    r3 = FL(0.0);

    while (r1 >= r2) {
      r3++;
      r1 *= unirand(csound);
    }

    return (r3);
}
float tsl::random::Random::betarand(float range, float a, float b) {
    double r1, r2;
    double aa, bb;
    if (UNLIKELY(a <= FL(0.0) || b <= FL(0.0)))
        return FL(0.0);

    aa = (double) a;
    bb = (double) b;
    do {
        uint32_t tmp;
        do {
            tmp = RandMT();
        } while (!tmp);
        r1 = pow(UInt32toFlt(tmp), 1.0 / aa);
        do {
            tmp = RandMT();
        } while (!tmp);
        r2 = r1 + pow(UInt32toFlt(tmp), 1.0 / bb);
    } while (r2 > 1.0);
    return (((float) r1 / (float) r2) * range);
}
