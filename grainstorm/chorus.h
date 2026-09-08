#pragma once
//
// Created by pr on 14.08.20.
//

#ifndef GRAINSTORM_CHORUS_H
#define GRAINSTORM_CHORUS_H
class Follower;
/*
 * August 24, 1998
 * Copyright (C) 1998 Juergen Mueller And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Juergen Mueller And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*

  CHANGES

  - Adapted for fluidsynth, Peter Hanappe, March 2002

  - Variable delay line implementation using bandlimited
    interpolation, code reorganization: Markus Nentwig May 2002

 */


/*
 * 	Chorus effect.
 *
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_CHORUS ):
 *
 *        * gain-in                                           ___
 * ibuff -----+--------------------------------------------->|   |
 *            |      _________                               |   |
 *            |     |         |                   * level 1  |   |
 *            +---->| delay 1 |----------------------------->|   |
 *            |     |_________|                              |   |
 *            |        /|\                                   |   |
 *            :         |                                    |   |
 *            : +-----------------+   +--------------+       | + |
 *            : | Delay control 1 |<--| mod. speed 1 |       |   |
 *            : +-----------------+   +--------------+       |   |
 *            |      _________                               |   |
 *            |     |         |                   * level n  |   |
 *            +---->| delay n |----------------------------->|   |
 *                  |_________|                              |   |
 *                     /|\                                   |___|
 *                      |                                      |
 *              +-----------------+   +--------------+         | * gain-out
 *              | Delay control n |<--| mod. speed n |         |
 *              +-----------------+   +--------------+         +----->obuff
 *
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <= i <= n).
 *
 * The delay of each block is modulated between 0..depth ms
 *
 */


#include "logger.h"
#include <cmath>
#include <vector>
#include "defines.h"
#include "base.h"
#include "DelayBase.h"


/* Variable delay line implementation
 * ==================================
 *
 * The modulated delay needs the value of the delayed signal between
 * samples.  A lowpass filter is used to obtain intermediate values
 * between samples (bandlimited interpolation).  The sample pulse
 * train is convoluted with the impulse response of the low pass
 * filter (sinc function).  To make it work with a small number of
 * samples, the sinc function is windowed (Hamming window).
 *
 */

enum fluid_chorus_mod {
    FLUID_CHORUS_MOD_SINE = 0,
    FLUID_CHORUS_MOD_TRIANGLE = 1
};

/* Those are the default settings for the chorus. */
#define FLUID_CHORUS_DEFAULT_N 3
#define FLUID_CHORUS_DEFAULT_SPEED 0.3
#define FLUID_CHORUS_DEFAULT_DEPTH 8.0
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE

#define MAX_CHORUS    99
#define MAX_DELAY    100
#define MAX_DEPTH    10
#define CHORUS_MIN_SPEED_HZ    0.29
#define CHORUS_MAX_SPEED_HZ    5

/* Length of one delay line in samples:
 * Set through MAX_SAMPLES_LN2.
 * For example:
 * MAX_SAMPLES_LN2=12
 * => MAX_SAMPLES=pow(2,12)=4096
 * => MAX_SAMPLES_ANDMASK=4095
 */
#define MAX_SAMPLES_LN2 12

#define MAX_SAMPLES (1 << (MAX_SAMPLES_LN2-1))
#define MAX_SAMPLES_ANDMASK (MAX_SAMPLES-1)


/* Interpolate how many steps between samples? Must be power of two
   For example: 8 => use a resolution of 256 steps between any two
   samples
*/
#define INTERPOLATION_SUBSAMPLES_LN2 8
#define INTERPOLATION_SUBSAMPLES (1 << (INTERPOLATION_SUBSAMPLES_LN2-1))
#define INTERPOLATION_SUBSAMPLES_ANDMASK (INTERPOLATION_SUBSAMPLES-1)

/* Use how many samples for interpolation? Must be odd.  '7' sounds
   relatively clean, when listening to the modulated delay signal
   alone.  For a demo on aliasing try '1' With '3', the aliasing is
   still quite pronounced for some input frequencies
*/
#define INTERPOLATION_SAMPLES 9


void fluid_chorus_triangle(int32_t *buf, int len, int depth);

void fluid_chorus_sine(int32_t *buf, int len, int depth);

template<typename T>
class Chorus3 {
public:
    void init(T sr) {
        _sr = sr;
        /* Lookup table for the SI function (impulse response of an ideal low pass) */

        /* ii: Offset in terms of fractional samples ('subsamples') */
        for (int32_t ii = 0; ii < INTERPOLATION_SUBSAMPLES; ii++) {
            /* i: Offset in terms of whole samples */
            for (int32_t i = 0; i < INTERPOLATION_SAMPLES; i++) {

                /* Move the origin into the center of the table */
                double i_shifted = ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
                                    + (double) ii / (double) INTERPOLATION_SUBSAMPLES);
                if (fabs(i_shifted) < 0.000001) {
                    /* sinc(0) cannot be calculated straightforward (limit needed
                       for 0/0) */
                    _sinc_table[ii][i] = (MYFLOAT) 1.;

                } else {
                    _sinc_table[ii][i] = (MYFLOAT) sin(i_shifted * PI_P) / (PI_P * i_shifted);
                    /* Hamming window */
                    _sinc_table[ii][i] *= (MYFLOAT) 0.5 * (1.0 + cos(2.0 * PI_P * i_shifted /
                                                                     (MYFLOAT) INTERPOLATION_SAMPLES));
                };
            };
        };

        /* allocate lookup tables */
        _lookup_tab.resize((unsigned long) ceil(_sr / CHORUS_MIN_SPEED_HZ));
        /* allocate sample buffer */

        _chorusbuf.resize(MAX_SAMPLES, 0);

        update();
    }

    void initstereo(T sr) {
        _sr = sr;
        /* Lookup table for the SI function (impulse response of an ideal low pass) */
        /* ii: Offset in terms of fractional samples ('subsamples') */
        for (int32_t ii = 0; ii < INTERPOLATION_SUBSAMPLES; ii++) {

            /* i: Offset in terms of whole samples */
            for (int32_t i = 0; i < INTERPOLATION_SAMPLES; i++) {

                /* Move the origin into the center of the table */
                double i_shifted = ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
                                    + (double) ii / (double) INTERPOLATION_SUBSAMPLES);
                if (fabs(i_shifted) < 0.000001) {
                    /* sinc(0) cannot be calculated straightforward (limit needed
                       for 0/0) */
                    _sinc_table[ii][i] = (MYFLOAT) 1.;

                } else {
                    _sinc_table[ii][i] = (MYFLOAT) sin(i_shifted * PI_P) / (PI_P * i_shifted);
                    /* Hamming window */
                    _sinc_table[ii][i] *= (MYFLOAT) 0.5 * (1.0 + cos(2.0 * PI_P * i_shifted /
                                                                     (MYFLOAT) INTERPOLATION_SAMPLES));
                };
            };
        };

        /* allocate lookup tables */
        _lookup_tab.resize((unsigned long) ceil(_sr / CHORUS_MIN_SPEED_HZ));
        /* allocate sample buffer */

        _chorusbufL.resize(MAX_SAMPLES, 0);
        _chorusbufR.resize(MAX_SAMPLES, 0);

        update();
    }


    void compute(T *left_in, T *right_in,
                 T *left_out, T *right_out, int32_t size) {
        T d_in, d_out;

        for (int32_t sample_index = 0; sample_index < size; sample_index++) {

            d_in = (left_in[sample_index] + right_in[sample_index]) * .5;
            d_out = 0.0;

            /* Write the current sample into the circular buffer */
            _chorusbuf[_counter] = d_in;

            for (int32_t i = 0; i < _number_blocks; i++) {
                /* Calculate the delay in subsamples for the delay line of chorus block nr. */

                /* The value in the lookup table is so, that this expression
                 * will always be positive.  It will always include a number of
                 * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
                 * remain positive at all times. */
                int32_t pos_subsamples = (INTERPOLATION_SUBSAMPLES * _counter
                                      - _lookup_tab[_phase[i]]);

                int32_t pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

                /* modulo divide by INTERPOLATION_SUBSAMPLES */
                pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

                for (int32_t ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
                    /* Add the delayed signal to the chorus sum d_out Note: The
                     * delay in the delay line moves backwards for increasing
                     * delay!*/

                    /* The & in chorusbuf[...] is equivalent to a division modulo
                       MAX_SAMPLES, only faster. */
                    d_out += _chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
                             * _sinc_table[pos_subsamples][ii];

                    pos_samples--;
                };
                /* Cycle the phase of the modulating LFO */
                _phase[i]++;
                _phase[i] %= (_modulation_period_samples);
            } /* foreach chorus block */

            d_out *= _level;

            /* Add the chorus sum d_out to output */
            left_out[sample_index] = d_out;
            right_out[sample_index] = d_out;

            /* Move forward in circular buffer */
            _counter++;
            _counter &= MAX_SAMPLES_ANDMASK;

        } /* foreach sample */
    }


    inline T tickmono(T in) {
        MYFLOAT d_out = 0.0f;
        /* Write the current sample into the circular buffer */
        _chorusbuf[_counter] = in;

        for (int32_t i = 0; i < _number_blocks; i++) {
            /* Calculate the delay in subsamples for the delay line of chorus block nr. */

            /* The value in the lookup table is so, that this expression
             * will always be positive.  It will always include a number of
             * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
             * remain positive at all times. */
            int32_t pos_subsamples = (INTERPOLATION_SUBSAMPLES * _counter
                                  - _lookup_tab[_phase[i]]);

            int32_t pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

            /* modulo divide by INTERPOLATION_SUBSAMPLES */
            pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

            for (int32_t ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
                /* Add the delayed signal to the chorus sum d_out Note: The
                 * delay in the delay line moves backwards for increasing
                 * delay!*/

                /* The & in chorusbuf[...] is equivalent to a division modulo
                   MAX_SAMPLES, only faster. */
                d_out += _chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
                         * _sinc_table[pos_subsamples][ii];

                pos_samples--;
            };
            /* Cycle the phase of the modulating LFO */
            _phase[i]++;
            _phase[i] %= (_modulation_period_samples);
        } /* foreach chorus block */

        /* Move forward in circular buffer */

        _counter++;
        _counter &= MAX_SAMPLES_ANDMASK;

        return d_out * _level;
    }

    inline void tickstereo(T inl, T inr, T *outl, T *outr) {
        /* Write the current sample into the circular buffer */
        _chorusbufL[_counter] = inl * _wet1 + inr * _wet2;
        _chorusbufR[_counter] = inr * _wet1 + inr * _wet2;

        for (int32_t i = 0; i < _number_blocks; i++) {
            /* Calculate the delay in subsamples for the delay line of chorus block nr. */

            /* The value in the lookup table is so, that this expression
             * will always be positive.  It will always include a number of
             * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
             * remain positive at all times. */
            int32_t pos_subsamples = (INTERPOLATION_SUBSAMPLES * _counter
                                  - (int) (_lookup_tab[_phase[i]]));

            int32_t pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

            /* modulo divide by INTERPOLATION_SUBSAMPLES */
            pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;
            T d_out = 0.0;
            if (!(i & 1)) {
                for (int32_t ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
                    /* Add the delayed signal to the chorus sum d_out Note: The
                     * delay in the delay line moves backwards for increasing
                     * delay!*/

                    /* The & in chorusbuf[...] is equivalent to a division modulo
                       MAX_SAMPLES, only faster. */
                    d_out += _chorusbufL[pos_samples & MAX_SAMPLES_ANDMASK]
                             * _sinc_table[pos_subsamples][ii];

                    pos_samples--;
                };
                *outl += d_out;
            } else {
                for (int32_t ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
                    /* Add the delayed signal to the chorus sum d_out Note: The
                     * delay in the delay line moves backwards for increasing
                     * delay!*/

                    /* The & in chorusbuf[...] is equivalent to a division modulo
                       MAX_SAMPLES, only faster. */
                    d_out += _chorusbufR[pos_samples & MAX_SAMPLES_ANDMASK]
                             * _sinc_table[pos_subsamples][ii];

                    pos_samples--;
                };
                *outr += d_out;
            }
            /* Cycle the phase of the modulating LFO */
            _phase[i]++;
            _phase[i] %= (_modulation_period_samples);
        } /* foreach chorus block */

        /* Move forward in circular buffer */
        *outl *= _level;
        *outr *= _level;
        _counter++;
        _counter %= MAX_SAMPLES;
    }


    void set_width(MYFLOAT width) {
        _wet1 = (width / 2 + 0.5);
        _wet2 = ((1 - width) / 2);
        _width = width;
    }


    void set_nr(int32_t nr) {
        _number_blocks = nr;
    }

    void set_speed_Hz(MYFLOAT speed_Hz) {
        _speed_Hz = speed_Hz;
    }

    void set_depth_ms(MYFLOAT depth_ms) {
        _depth_ms = depth_ms;
    }

    void set_type(int32_t type) {
        _type = type;
    }


    void set_phase_diff(MYFLOAT diff) {
        phasediff = diff;
    }

    void
    update() {

        _modulation_period_samples = _sr / _speed_Hz;

        /* The variation in delay time is x: */
        int32_t modulation_depth_samples = (int)
                (_depth_ms / 1000.0  /* convert modulation depth in ms to s*/
                 * _sr);

        if (modulation_depth_samples > MAX_SAMPLES) {
            LOGE("chorus: Too high depth. Setting it to max (%d).", MAX_SAMPLES);
            modulation_depth_samples = MAX_SAMPLES;
        }
        /* initialize LFO table */
        if (_type == FLUID_CHORUS_MOD_SINE) {
            fluid_chorus_sine(_lookup_tab, _modulation_period_samples,
                              modulation_depth_samples);
        } else if (_type == FLUID_CHORUS_MOD_TRIANGLE) {
            fluid_chorus_triangle(_lookup_tab, _modulation_period_samples,
                                  modulation_depth_samples);
        } else {
            LOGE("chorus: Unknown modulation type. Using sinewave.");
            _type = FLUID_CHORUS_MOD_SINE;
            fluid_chorus_sine(_lookup_tab, _modulation_period_samples,
                              modulation_depth_samples);
        };

        for (int32_t i = 0; i < _number_blocks; i++) {
            /* Set the phase of the chorus blocks equally spaced */
            _phase[i] = (int) ((double) _modulation_period_samples
                               * (double) i / (double) _number_blocks);
        }

        /* Start of the circular buffer */
        _counter = 0;

        _level = 1. / (T) _number_blocks;
    }


    void
    updatestereo() {
        _modulation_period_samples = _sr / _speed_Hz;

        /* The variation in delay time is x: */
        int32_t modulation_depth_samples = (int)
                (_depth_ms * _sr * 0.001);

        if (modulation_depth_samples > MAX_SAMPLES) {
            LOGE("chorus: Too high depth. Setting it to max (%d).", MAX_SAMPLES);
            modulation_depth_samples = MAX_SAMPLES;
        }
        /* initialize LFO table */
        if (_type == FLUID_CHORUS_MOD_SINE) {
            fluid_chorus_sine(_lookup_tab, _modulation_period_samples,
                              modulation_depth_samples);
        } else if (_type == FLUID_CHORUS_MOD_TRIANGLE) {
            fluid_chorus_triangle(_lookup_tab, _modulation_period_samples,
                                  modulation_depth_samples);
        } else {
            LOGE("chorus: Unknown modulation type. Using sinewave.");
            _type = FLUID_CHORUS_MOD_SINE;
            fluid_chorus_sine(_lookup_tab, _modulation_period_samples,
                              modulation_depth_samples);
        };

        double diff = phasediff * ((double) _modulation_period_samples
                                   / (double) (_number_blocks / 2)) * 0.5;

        for (int32_t i = 0; i < _number_blocks / 2; i++) {
            /* Set the phase of the chorus blocks equally spaced */
            _phase[i * 2] = (int) ((double) _modulation_period_samples
                                   * (double) i / (double) (_number_blocks / 2));

            _phase[i * 2 + 1] = _phase[i * 2] + (int) diff;
        }

        /* Start of the circular buffer */
        _counter = 0;

        _level = 2. / (T) _number_blocks;
    }


    /* Purpose:
 *
 * Calculates a modulation waveform (sine) Its value ( modulo
 * MAXSAMPLES) varies between 0 and depth*INTERPOLATION_SUBSAMPLES.
 * Its period length is len.  The waveform data will be used modulo
 * MAXSAMPLES only.  Since MAXSAMPLES is substracted from the waveform
 * a couple of times here, the resulting (current position in
 * buffer)-(waveform sample) will always be positive.
 */
    void fluid_chorus_sine(std::vector<int> &vec, long len, int32_t depth) {
        for (long i = 0; i < len; i++) {
            double val = sin((double) i / (double) len * TWOPI_P);
            vec[i] = (int) ((1.0 + val) * (double) depth / 2.0 * (double) INTERPOLATION_SUBSAMPLES);
            vec[i] -= 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
            //    printf("%i %i\n",i,buf[i]);
        }
    }

/* Purpose:
 * Calculates a modulation waveform (triangle)
 * See fluid_chorus_sine for comments.
 */
    void fluid_chorus_triangle(std::vector<int> &vec, long len, int32_t depth) {
        long i = 0;
        long ii = len - 1;
        while (i <= ii) {
            double val = i * 2.0 / len * (double) depth * (double) INTERPOLATION_SUBSAMPLES;
            double val2 = (int) (val + 0.5) - 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
            vec[i++] = (int) val2;
            vec[ii--] = (int) val2;
        }
    }


private:
    int32_t _type{FLUID_CHORUS_MOD_SINE};                  /* current value */
    MYFLOAT _depth_ms{FLUID_CHORUS_DEFAULT_DEPTH};      /* current value */
    T _level{1. / FLUID_CHORUS_DEFAULT_N};         /* current value */
    MYFLOAT _speed_Hz{FLUID_CHORUS_DEFAULT_SPEED};      /* current value */
    int32_t _number_blocks{FLUID_CHORUS_DEFAULT_N};         /* current value */
    MYFLOAT phasediff{1};
    int32_t _counter{};
    long _modulation_period_samples{};
    /* sinc lookup table */
    T _sinc_table[INTERPOLATION_SUBSAMPLES][INTERPOLATION_SAMPLES];
    long _phase[MAX_CHORUS];
    std::vector<T> _chorusbuf, _chorusbufL, _chorusbufR;
    std::vector<int> _lookup_tab;
    T _sr;


    T _wet1{1}, _wet2{0}, _width{1};
};


class Chorus : public Effect {

public:
    Chorus(TRACK *track);

    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) override;

    void check();

private:
    std::atomic<MYFLOAT> *depth, *freq, *ntaps, *width, *_spread, *_delay, *_mod;
    MYFLOAT depth_prev{};// 0.75;
    MYFLOAT freq_prev{};// 0.75;
    MYFLOAT ntaps_prev{};// 0.75;
    MYFLOAT width_prev{};
    MYFLOAT spread_prev{};
    MYFLOAT _delay_prev{};
    MYFLOAT _mod_prev{};

    Chorus3<MYFLOAT> chorus;
    SimpleDelay2<MYFLOAT> _delayL, _delayR;
    Follower *fol{};
};

#endif //GRAINSTORM_CHORUS_H
