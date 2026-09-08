//
// Created by pr on 21.06.19.
//

#include <random>
#include "random.h"
#include "grainstorm.h"
//#include "instrument.h"

uint32_t rand(uint32_t held) {
    uint32_t val = (1103515245 * held + 12345) % RANDMAX;
    return val;
}

Spline::Spline() {
    initialized = false;
}

Spline::Spline(int sr, MYFLOAT rangemin,
               MYFLOAT rangemax, MYFLOAT cpsmin, MYFLOAT cpsmax) {
    Init(sr, rangemin, rangemax, cpsmin, cpsmax);
}

void Spline::Init(int sr, MYFLOAT rangemin,
                  MYFLOAT rangemax, MYFLOAT cpsmin, MYFLOAT cpsmax) {
    holdrand = rand(0);
    num1 = randGab;
    num2 = randGab;
    df1 = 0.0;
    phs = 0.0;
    init = true;
    cpsMax = cpsmax;
    cpsMin = cpsmin;
    rangeMax = rangemax;
    rangeMin = rangemin;
    onedsr = 1.0f / (MYFLOAT) sr;
    initialized = true;
}

MYFLOAT Spline::Tick() {
    MYFLOAT f0 = num0;

    if (init) {
        init = false;
        goto next;
    }

    phs += si;
    if (phs >= 1.0) {
        MYFLOAT slope, resd1, resd0, f2, f1;
        next:
        si = (randGab * (cpsMax - cpsMin) + cpsMin) * onedsr;
        while (phs > 1.0) phs -= 1.0;
        f0 = num0 = num1;
        f1 = num1 = num2;
        f2 = num2 = BiRandGab;
        df0 = df1;
        df1 = (f2 - f0) * 0.5f;
        slope = f1 - f0;
        resd0 = df0 - slope;
        resd1 = df1 - slope;
        c3 = resd0 + resd1;
        c2 = -(resd1 + 2.0f * resd0);
    }

    MYFLOAT x = (MYFLOAT) phs;

    return (((c3 * x + c2) * x + df0) * x + f0) *
           (rangeMax - rangeMin) + rangeMin;
}

struct seed{
    seed(){
        srand(time(0));
    }
};

static seed s{};

MYFLOAT randomfloat(MYFLOAT low, MYFLOAT high) {
    MYFLOAT f = (MYFLOAT) rand() / RAND_MAX;
    return low + f * (high - low);
    std::uniform_real_distribution<double> unif(low, high);
    std::default_random_engine re;
    return unif(re);
}

// Vibrato::Vibrato() removed — now default-constructed; call setSinewave() before use


/* Calculate pseudo-random 32 bit number based on linear congruential method. */
static unsigned long GenerateRandomNumber(unsigned long &randSeed) {
    randSeed = (randSeed * 196314165) + 907633515;
    return randSeed;
}

#define PINK_MAX_RANDOM_ROWS   (30)
#define PINK_RANDOM_BITS       (24u)
#define PINK_RANDOM_SHIFT      ((sizeof(long)*8)-PINK_RANDOM_BITS)

PinkNoise::PinkNoise(MYFLOAT
                     _amp) {
    amp = _amp;
    Reset();
}

void PinkNoise::Reset() {
    pink_Index = 0;
    pink_IndexMask = (1u << 20u) - 1;
/* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
    long pmax = (20 + 1) * (1u << (PINK_RANDOM_BITS - 1u));
    pink_Scalar = 1.0f / (MYFLOAT) pmax;
/* Initialize rows. */
    for (int i = 0; i < 20; i++) pink_Rows[i] = 0;
    pink_RunningSum = 0;
}

/* Generate numRows octave-spaced white bands and sum to pink noise. */
void PinkNoise::process(MYFLOAT *in, int size, MYFLOAT gain) {
    for (int i = 0; i < size; i++) {
/* Increment and mask index. */
        pink_Index = (pink_Index + 1u) & pink_IndexMask;

/* If index is zero, don't update any random values. */
        if (pink_Index != 0) {
            /* Determine how many trailing zeros in PinkIndex. */
            /* This algorithm will hang if n==0 so test first. */
            int numZeros = 0;
            uint32_t n = pink_Index;
            while ((n & 1u) == 0) {
                n = n >> 1u;
                numZeros++;
            }

            /* Replace the indexed ROWS random value.
             * Subtract and add back to RunningSum instead of adding all the random
             * values together. Only one changes each time.
             */
            pink_RunningSum -= pink_Rows[numZeros];
            unsigned long newRandom = (GenerateRandomNumber(randSeed)) >> PINK_RANDOM_SHIFT;
            pink_RunningSum += newRandom;
            pink_Rows[numZeros] = newRandom;
        }

/* Add extra white noise value. */
        unsigned long newRandom = ( GenerateRandomNumber(randSeed)) >> PINK_RANDOM_SHIFT;
        unsigned long sum = pink_RunningSum + newRandom;

/* Scale to range of -1.0 to 0.9999. */
        in[i] = pink_Scalar * sum * gain;
    }
}

inline MYFLOAT PinkNoise::tick() {

/* Increment and mask index. */
    pink_Index = (pink_Index + 1) & pink_IndexMask;

/* If index is zero, don't update any random values. */
    if (pink_Index != 0) {
        /* Determine how many trailing zeros in PinkIndex. */
        /* This algorithm will hang if n==0 so test first. */
        int numZeros = 0;
        uint32_t n = pink_Index;
        while ((n & 1u) == 0) {
            n = n >> 1u;
            numZeros++;
        }

        /* Replace the indexed ROWS random value.
         * Subtract and add back to RunningSum instead of adding all the random
         * values together. Only one changes each time.
         */
        pink_RunningSum -= pink_Rows[numZeros];
        unsigned long newRandom = ( GenerateRandomNumber(randSeed)) >> PINK_RANDOM_SHIFT;
        pink_RunningSum += newRandom;
        pink_Rows[numZeros] = newRandom;
    }

/* Add extra white noise value. */
    unsigned long newRandom = ( GenerateRandomNumber(randSeed)) >> PINK_RANDOM_SHIFT;
    unsigned long sum = pink_RunningSum + newRandom;

/* Scale to range of -1.0 to 0.9999. */
    return pink_Scalar * sum * amp;
}

SimpleVibrato::SimpleVibrato(int sr, MYFLOAT* sinewave, MYFLOAT rate, MYFLOAT depth) {
    _onedsr = 1.f / (MYFLOAT) sr;
    _sine = sinewave;
    _offset = 0;
    rate = _rate;
    _depth = depth;
    _phinc = rate * _onedsr;
}

