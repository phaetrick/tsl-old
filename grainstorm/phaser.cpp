#include "phaser.h"
#include "logger.h"
#include <cstring>
#include "tools/aligned_memalloc.h"
#include "grainstorm.h"
#include "defines.h"
#include "track.h"
#include "lfo.h"
#include "app.h"

PHASER::PHASER(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_PHASER, MONOEFFECT) {
    _lfo = &track->lfo[PHASERBAND];
    _feedback = &_STATE->params[track->index][PHASERFB];
    _prevnotches = *(_notches = &_STATE->params[track->index][PHASERNOTCHES]) * 2;
    _freq = &_STATE->params[track->index][PHASERBAND];
    _mix = &_STATE->params[track->index][PHASERMIX];
    _bypass = &track->bypass[SPACE_PHASER];
}

void PHASER::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT mix;
    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;
    }

    TRACK *track = _track;
    LFO *lfo = *_lfo;
    const int32_t notches = (int) *_notches * 2;
    if (notches != _prevnotches) {
        _fadeinc = -_fadeconst;
    }
    //memset(xarray, 0, notches * sizeof(MYFLOAT));
    //memset(yarray, 0, notches * sizeof(MYFLOAT));
    const MYFLOAT feedback = *_feedback * .99;

    MYFLOAT center_const = LOG2NORMALF(*_freq);

    MYFLOAT range = 0;
    bool lfo_on = false;
    if (lfo && lfo->power()) {
        lfo_on = true;
        MYFLOAT a = LOG2NORMALF(_STATE->controls[track->index][PHASERBAND].lfo_min.load());
        MYFLOAT b = LOG2NORMALF(_STATE->controls[track->index][PHASERBAND].lfo_max.load());
        range = b-a;
        center_const = a;
    }
    //if (coef<=FL(0.0)) coef = -coef; /* frequency will "fold over" if <= 0 Hz */
    /* next two lines implement bilinear z-transform, to convert
     * frequency value into a useable coefficient for the
     * allpass filters.
     */

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT freq = center_const + (lfo_on ? lfo->buf[i] * range : 0);
        if (freq > 20000.)
            freq = 20000.;
        else if (freq < 18.)
            freq = 18.;
        const MYFLOAT omegapi = _STATE->pidsr * (MYFLOAT) freq;
        const MYFLOAT beta = (1. - omegapi) / (1. + omegapi);
        MYFLOAT x = in[i] + _fb * feedback;
        for (int32_t j = 0; j < _prevnotches; j++) {
            /* Difference equation for 1st order
             * allpass filter */
            const MYFLOAT y = beta * (x + _yarray[j]) - _xarray[j];
            /* Stores state values in arrays */
            _xarray[j] = x;
            _yarray[j] = y;
            x = y;
        }
        _fb = x;
        in[i] = x * _smooth1 * _fade + in[i] * (1. - _smooth1);
        sm1(mix);

        if (_fade < 0) {
            _fadeinc = _fadeconst;
            _fade = 0;
            _prevnotches = *_notches * 2;
            reset();
        } else if (_fade > 1) {
            _fadeinc = 0;
            _fade = 1;
        } else {
            _fade += _fadeinc;
        }
    }
}

enum {
    LP1, LP2, HP, AP
};

static constexpr MYFLOAT minfreqs[] = {16., 33., 48., 98., 160., 260.};
static constexpr MYFLOAT maxfreqs[] = {1600., 3300., 4800, 9800., 16000., 20000.};
static constexpr MYFLOAT ranges[] = {1600. - 16., 3300. - 33., 4800 - 48., 9800. - 98., 16000. - 160.,
                                20000. - 260.};


static inline void
update_coefs(MYFLOAT pidsr, MYFLOAT fr, MYFLOAT *a0, MYFLOAT *a1, MYFLOAT *a2, MYFLOAT *b1, MYFLOAT *b2) {
    *a0 = (tan(pidsr * fr) - 1.0) / (tan(pidsr * fr) + 1.0);
    *a1 = 1.0;
    *a2 = 0.0;
    *b1 = *a0;
    *b2 = 0.0;
}

void PHASER4::compute(MYFLOAT *in, int32_t size) {

    MYFLOAT mix;
    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;
    }
    TRACK *track = _track;
    const int32_t stages = (int) *_stages;
    if (stages != _oldstages) {
        _fadeinc = -_fadeconst;
    }

    //memset(xarray, 0, notches * sizeof(MYFLOAT));
    //memset(yarray, 0, notches * sizeof(MYFLOAT));
    const MYFLOAT feedback = (MYFLOAT) *_feedback * .99;

    const MYFLOAT radius = 1.0f - .95 * (MYFLOAT) *_radius;

    MYFLOAT center_const = pow(10, *_freq * .05);
    MYFLOAT center = center_const;

    LFO *lfo_freq = *_lfofreq;
    MYFLOAT range = 0;
    bool lfo_freq_on = false;
    if (lfo_freq && lfo_freq->power()) {
        lfo_freq_on = true;
        MYFLOAT a = LOG2NORMALF(_STATE->controls[track->index][PHASER4BAND].lfo_min.load());
        MYFLOAT b = LOG2NORMALF(_STATE->controls[track->index][PHASER4BAND].lfo_max.load());
        range = b-a;
        center_const = a;
    }
    LFO *lfo_spacing = *_lfospacing;
    MYFLOAT rangespacing = 0;
    MYFLOAT spacing_const = *_spacing;
    bool lfo_spacing_on = false;
    if (lfo_spacing && lfo_spacing->power()) {
        lfo_spacing_on = true;
        MYFLOAT a = _STATE->controls[track->index][PHASER4SPACING].lfo_min.load();
        MYFLOAT b = _STATE->controls[track->index][PHASER4SPACING].lfo_max.load();
        rangespacing = b-a;
        spacing_const = a;
    }

    const int32_t mode = 1;
    for (int32_t i = 0; i < size; i++) {
        const auto freq = (MYFLOAT) (center_const +
                                   (lfo_freq_on ? lfo_freq->buf[i] * range : 0));
        const auto spacing = (MYFLOAT) (spacing_const +
                                      (lfo_spacing_on ? lfo_spacing->buf[i] * rangespacing : 0));

        MYFLOAT x = in[i] + _fb * feedback;
        for (int32_t j = 0; j < _oldstages; j++) {
            MYFLOAT freqstage;
            if (mode == 1)
                freqstage = freq + (freq * spacing * j);
            else {
                //freqstage = freq * kk;
                //kk *= ksep;
                freqstage = freq * powf(spacing, j);
            }
            /* Note similarities of following equations to
             * equations in resonr/resonz. The 2nd-order
             * allpass filter used here is similar to the
             * typical reson filter, with the addition of zeros.
             * The pole angle determines the frequency of the
             * notch, while the pole radius determines the q of
             * the notch.
             */
            const MYFLOAT r = exp(-(freqstage * _STATE->pidsr / radius));
            const MYFLOAT b = -2. * r * cos(freqstage * _STATE->twopidsr);
            const MYFLOAT a = r * r;

            /* Difference equations for implementing canonical
             * 2nd order section. (Direct Form II)
             */
            const MYFLOAT temp = x - b * _xarray[j] - a * _yarray[j];
            const MYFLOAT y = a * temp + b * _xarray[j] + _yarray[j];
            _yarray[j] = _xarray[j];
            _xarray[j] = temp;
            x = y;
        }

        in[i] = x * _smooth1 * _fade + in[i] * (1. - _smooth1);
        sm1(mix);
        _fb = x;

        if (_fade < 0) {
            _fadeinc = _fadeconst;
            _fade = 0;
            _oldstages = *_stages;
            reset();
        } else if (_fade > 1) {
            _fadeinc = 0;
            _fade = 1;
        } else {
            _fade += _fadeinc;
        }

    }

}

PHASER4::PHASER4(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_PHASER4, MONOEFFECT) {
    _spacing = &_STATE->params[track->index][PHASER4SPACING];
    _radius = &_STATE->params[track->index][PHASER4RADIUS];
    _oldstages = *(_stages = &_STATE->params[track->index][PHASER4STAGES]);
    _feedback = &_STATE->params[track->index][PHASER4FB];
    _freq = &_STATE->params[track->index][PHASER4BAND];
    _mix = &_STATE->params[track->index][PHASER4MIX];
    _lfofreq = &track->lfo[PHASER4BAND];

    _lfospacing = &track->lfo[PHASER4SPACING];
    _bypass = &track->bypass[SPACE_PHASER4];
}

#define PHASER_LFO_SHAPE 2
#define ONE_  0.94f        // To prevent LFO ever reaching 1.0f for filter stability purposes
#define ZERO_ 0.00001f        // Same idea as above.
#define RANGE_ (ONE_ - ZERO_)

Phaser::Phaser(TRACK *t) : Effect(t, SPACE_STEREO_QUAD_PHASER, STEREOEFFECT) {
    _feedback = &_STATE->params[t->index][PHASER2FB];
    _phase_offset = &_STATE->params[t->index][PHASER2PH];
    _mix = &_STATE->params[t->index][PHASER2MIX];
    _bypass = &t->bypass[SPACE_STEREO_QUAD_PHASER];
    _lfo = &t->lfo[PHASER2CENTER];
    _wave = _DATA->eq["TRIANGLE"].win;
}

void Phaser::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    const MYFLOAT feedback = *_feedback * .99;
    const MYFLOAT offset = *_phase_offset / 360.;
    const LFO *lfo = _lfo->load();
    const bool lfoon = lfo && lfo->power();

    MYFLOAT minval = .1;
    MYFLOAT range = 0.;
    MYFLOAT phase = 0.;
    MYFLOAT phinc = 0;
    if (lfoon) {
        if (!lfo->stopped() && !lfo->syncmidi())
            phinc = lfo->phinc() * lfo->dir();
        phase = lfo->phs();
        const MYFLOAT a = lfo->min(PHASER2CENTER);
        const MYFLOAT b = lfo->max(PHASER2CENTER);

        const MYFLOAT min = std::min(a, b);
        const MYFLOAT r = DISTANCEF(a, b);

        minval = ZERO_ + min * RANGE_;
        range = RANGE_ * r;
    }
    MYFLOAT mix;
    if (*_bypass || destroyRequested) {
        mix = 0;
    } else {
        mix = *_mix;
    }

    for (int32_t i = 0; i < s; i++) {
        MYFLOAT freq = minval +
                     (lfoon ? _wave[PHS2INT(phase + i * phinc)] * range : 0);

        const MYFLOAT omegapil = PI_P * freq * .5;
        const MYFLOAT betal = (1.0f - omegapil) / (1.0f + omegapil);

        MYFLOAT betar;

        if (offset) {
            freq = minval + _wave[PHS2INT(phase + offset + i * phinc)] * range;
            const MYFLOAT omegapir = PI_P * freq * .5;
            betar = (1.0f - omegapir) / (1.0f + omegapir);
        } else
            betar = betal;

        MYFLOAT xl = inl[i] + _fb[0] * feedback;
        MYFLOAT xr = inr[i] + _fb[1] * feedback;

        for (int32_t j = 0; j < 12; j++) {
            /* Difference equation for 1st order
             * allpass filter */
            const MYFLOAT yl = betal * (xl + _yarray[0][j]) - _xarray[0][j];
            /* Stores state values in arrays */
            _xarray[0][j] = xl;
            _yarray[0][j] = yl;
            xl = yl;
            const MYFLOAT yr = betar * (xr + _yarray[1][j]) - _xarray[1][j];
            /* Stores state values in arrays */
            _xarray[1][j] = xr;
            _yarray[1][j] = yr;
            xr = yr;
        }
        _fb[0] = xl;
        _fb[1] = xr;
        outl[i] = xl * _smooth1 + inl[i] * (1. - _smooth1);
        outr[i] = xr * _smooth1 + inr[i] * (1. - _smooth1);
        sm1(mix);
    }
}

void PHASER_SSB::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    const MYFLOAT mix = (*_bypass || destroyRequested) ? 0. : 1.;
    MYFLOAT inc = 0., phase = 0.;
    MYFLOAT dir = 1;
    LFO *lfo = *_lfo;
    bool lfo_on = lfo && lfo->power();
    if (lfo_on) {
        if (!lfo->stopped() && !lfo->syncmidi())
            inc = TWOPI_F_P * lfo->phinc();//MYFLOAT) (_twopidsr * pow(10, lfo->freq() * .05));
        dir = lfo->dir();
        phase = lfo->phs() * TWOPI_F_P;
    }

    bool dolp = false, dohp = false, hp_cut_ch = false, lp_cut_ch = false;
    MYFLOAT y0_h_left, y1_h_left, y2_h_left, y0_h_right, y1_h_right, y2_h_right, a0_h, b1_h, b2_h, a0_sl_h, b1_sl_h, b2_sl_h;
    MYFLOAT y0_l_left, y1_l_left, y2_l_left, y0_l_right, y1_l_right, y2_l_right, a0_l, b1_l, b2_l, a0_sl_l, b1_sl_l, b2_sl_l;

    if (*_hp_cut != LOG10D20F(18.)) {
        dohp = true;
        y1_h_left = _y1_h_left;
        y2_h_left = _y2_h_left;
        y1_h_right = _y1_h_right;
        y2_h_right = _y2_h_right;
        a0_h = _a0_h;
        b1_h = _b1_h;
        b2_h = _b2_h;
        if (*_hp_cut != _prev_hp_cut) {
            hp_cut_ch = true;
            _prev_hp_cut = *_hp_cut;
            MYFLOAT freq = pow(10, *_hp_cut * .05);
            MYFLOAT sl = 1. / (MYFLOAT) s;  // SLOPE
            MYFLOAT pfreq = freq * _STATE->pidsr;

            MYFLOAT C = tan(pfreq);
            MYFLOAT C2 = C * C;
            MYFLOAT sqrt2C = C * ROOT2;
            MYFLOAT a0_n = 1. / (1. + sqrt2C + C2);
            MYFLOAT b1_n = 2. * (1. - C2) * a0_n;
            MYFLOAT b2_n = -(1. - sqrt2C + C2) * a0_n;

            a0_sl_h = (a0_n - a0_h) * sl;
            b1_sl_h = (b1_n - b1_h) * sl;
            b2_sl_h = (b2_n - b2_h) * sl;

            _a0_h = a0_n;
            _b1_h = b1_n;
            _b2_h = b2_n;
        }

    }

    if (*_lp_cut != LOG10D20F(20000.)) {
        dolp = true;
        y1_l_left = _y1_l_left;
        y2_l_left = _y2_l_left;
        y1_l_right = _y1_l_right;
        y2_l_right = _y2_l_right;
        a0_l = _a0_l;
        b1_l = _b1_l;
        b2_l = _b2_l;
        if (*_lp_cut != _prev_lp_cut) {
            lp_cut_ch = true;
            _prev_lp_cut = *_lp_cut;
            MYFLOAT freq = pow(10, *_lp_cut * .05);
            MYFLOAT sl = 1. / (MYFLOAT) s;  // SLOPE
            MYFLOAT pfreq = freq * _STATE->pidsr;

            MYFLOAT C = 1. / tan(pfreq);
            MYFLOAT C2 = C * C;
            MYFLOAT sqrt2C = C * ROOT2;
            MYFLOAT a0_n = 1. / (1. + sqrt2C + C2);
            MYFLOAT b1_n = -2. * (1. - C2) * a0_n;
            MYFLOAT b2_n = -(1. - sqrt2C + C2) * a0_n;

            a0_sl_l = (a0_n - a0_l) * sl;
            b1_sl_l = (b1_n - b1_l) * sl;
            b2_sl_l = (b2_n - b2_l) * sl;

            _a0_l = a0_n;
            _b1_l = b1_n;
            _b2_l = b2_n;
        }

    }

    for (int32_t i = 0; i < s; i++) {
        MYFLOAT inll = .5 * inl[i], inrr = .5 * inr[i];
        MYFLOAT xn1 = (inll + inrr) * .5;
        MYFLOAT xn2 = xn1;

        /* 6th order allpass filter for sine output. Structure is
         * 6 first-order allpass sections in series. Coefficients
         * taken from arrays calculated at i-time.
         */
        for (int32_t j = 0; j < 6; j++) {
            const MYFLOAT yn1 = _coef[j] * (xn1 - _yarray[j]) + _xarray[j];
            _xarray[j] = xn1;
            _yarray[j] = yn1;
            xn1 = yn1;
        }
        /* 6th order allpass filter for cosine output. Structure is
         * 6 first-order allpass sections in series. Coefficients
         * taken from arrays calculated at i-time.
         */
        for (int32_t j = 6; j < 12; j++) {
            const MYFLOAT yn2 = _coef[j] * (xn2 - _yarray[j]) + _xarray[j];
            _xarray[j] = xn2;
            _yarray[j] = yn2;
            xn2 = yn2;
        }

        const MYFLOAT coscarrier = cosf(phase);
        const MYFLOAT sincarrier = sinf(phase);

        if (dohp) {
            if (hp_cut_ch) {
                y0_h_left = xn1 + b1_h * y1_h_left + b2_h * y2_h_left;
                xn1 = (MYFLOAT) (a0_h * (y0_h_left - 2. * y1_h_left + y2_h_left));
                y2_h_left = y1_h_left;
                y1_h_left = y0_h_left;

                y0_h_right = xn2 + b1_h * y1_h_right + b2_h * y2_h_right;
                xn2 = (MYFLOAT) (a0_h * (y0_h_right - 2. * y1_h_right + y2_h_right));
                y2_h_right = y1_h_right;
                y1_h_right = y0_h_right;

                a0_h += a0_sl_h;
                b1_h += b1_sl_h;
                b2_h += b2_sl_h;
            } else {
                y0_h_left = xn1 + b1_h * y1_h_left + b2_h * y2_h_left;
                xn1 = (MYFLOAT) (a0_h * (y0_h_left - 2. * y1_h_left + y2_h_left));
                y2_h_left = y1_h_left;
                y1_h_left = y0_h_left;

                y0_h_right = xn2 + b1_h * y1_h_right + b2_h * y2_h_right;
                xn2 = (MYFLOAT) (a0_h * (y0_h_right - 2. * y1_h_right + y2_h_right));
                y2_h_right = y1_h_right;
                y1_h_right = y0_h_right;
            }
        }

        if (dolp) {
            if (lp_cut_ch) {
                y0_l_left = xn1 + b1_l * y1_l_left + b2_l * y2_l_left;
                xn1 = (MYFLOAT) (a0_l * (y0_l_left + 2. * y1_l_left + y2_l_left));
                y2_l_left = y1_l_left;
                y1_l_left = y0_l_left;

                y0_l_right = xn2 + b1_l * y1_l_right + b2_l * y2_l_right;
                xn2 = (MYFLOAT) (a0_l * (y0_l_right + 2. * y1_l_right + y2_l_right));
                y2_l_right = y1_l_right;
                y1_l_right = y0_l_right;

                a0_l += a0_sl_l;
                b1_l += b1_sl_l;
                b2_l += b2_sl_l;
            } else {
                y0_l_left = xn1 + b1_l * y1_l_left + b2_l * y2_l_left;
                xn1 = (MYFLOAT) (a0_l * (y0_l_left + 2. * y1_l_left + y2_l_left));
                y2_l_left = y1_l_left;
                y1_l_left = y0_l_left;

                y0_l_right = xn2 + b1_l * y1_l_right + b2_l * y2_l_right;
                xn2 = (MYFLOAT) (a0_l * (y0_l_right + 2. * y1_l_right + y2_l_right));
                y2_l_right = y1_l_right;
                y1_l_right = y0_l_right;
            }
        }

        const MYFLOAT mixsrc = 1. - _smooth1;
        outl[i] = (inll + xn2 * coscarrier - xn1 * sincarrier) * _smooth1 + inl[i] * mixsrc;
        outr[i] = (inrr + xn2 * coscarrier + xn1 * sincarrier) * _smooth1 + inr[i] * mixsrc;
        sm1(mix);

        /*
        if(env_on){
            MYFLOAT gain = channel == 0 ? env->detectL(in[i]) : env->detectR(in[i]);
            sincoef = add == DETECT_SUB ? sincoef_const -
                                          (sincoef_const - sub_min) * gain : sincoef_const + add_max * gain;
        }
        if (lfo_on) {
            sincoef = env_on ? sincoef + func(phase, addsub == LFO_ADD ? sub_max - sincoef : sincoef - sub_min) * depth : sincoef_const + func(phase, addsub == LFO_ADD ? sub_max - sincoef_const : sincoef_const - sub_min) * depth;
            phase+= inc * dir;
            if (phase >= 1.0)
                phase -= 1.0;
            if (phase < 0.0)
                phase += 1.0;
        }
         */
        if (lfo_on) {
            phase -= inc * dir;
            if (phase >= TWOPI_F_P)
                phase -= TWOPI_F_P;
            if (phase < 0)
                phase += TWOPI_F_P;
        }
        //out1[n] = yn2;  // cos
        //out2[n] = yn1;  // sin
    }
    if (dolp) {
        _y1_l_left = y1_l_left;
        _y2_l_left = y2_l_left;
        _y1_l_right = y1_l_right;
        _y2_l_right = y2_l_right;
    }
    if (dohp) {
        _y1_h_left = y1_h_left;
        _y2_h_left = y2_h_left;
        _y1_h_right = y1_h_right;
        _y2_h_right = y2_h_right;
    }
};


PHASER_SSB::PHASER_SSB(TRACK *track) : Effect(track, SPACE_PHASER3, STEREOEFFECT) {
    _bypass = &track->bypass[SPACE_PHASER3];
    _lp_cut = &_STATE->params[track->index][PHASER3LP];
    _prev_lp_cut = *_lp_cut;
    _hp_cut = &_STATE->params[track->index][PHASER3HP];
    _lfo = &track->lfo[PHASER3CENTER];
    _prev_hp_cut = *_hp_cut;
    MYFLOAT pfreq = pow(10, *_hp_cut * .05f) * _STATE->pidsr;
    MYFLOAT C = tan(pfreq);
    MYFLOAT C2 = C * C;
    MYFLOAT sqrt2C = C * ROOT2;
    _a0_h = 1. / (1. + sqrt2C + C2);
    _b1_h = 2. * (1. - C2) * _a0_h;
    _b2_h = -(1. - sqrt2C + C2) * _a0_h;

    pfreq = pow(10, *_lp_cut * .05f) * _STATE->pidsr;
    C = 1. / tan(pfreq);
    C2 = C * C;
    _a0_l = 1. / (1. + sqrt2C + C2);
    _b1_l = -2. * (1. - C2) * _a0_l;
    _b2_l = -(1. - sqrt2C + C2) * _a0_l;

    /* pole values taken from Bernie Hutchins, "Musical Engineer's Handbook" */
    MYFLOAT poles[12] = {0.3609, 2.7412, 11.1573, 44.7581, 179.6242, 798.4578,
                        1.2524, 5.5671, 22.3423, 89.6271, 364.7914, 2770.1114};
    MYFLOAT polefreq, rc, alpha, beta;
    /* calculate coefficients for allpass filters, based on sampling rate */
    for (int32_t i = 0; i < 12; i++) {
        /*      _DATA->coef[j] = (1 - (15 * PI * pole[j]) / CS_ESR) /
                (1 + (15 * PI * pole[j]) / CS_ESR); */
        polefreq = poles[i] * 15.;
        rc = 1. / (2. * PI_F_P * polefreq);
        alpha = 1. / rc;
        alpha = alpha * 0.5 * _STATE->onedsr;
        beta = (1. - alpha) / (1. + alpha);
        _xarray[i] = _yarray[i] = 0.0f;
        _coef[i] = -(MYFLOAT) beta;
    }
}


void ROTARY_SSB::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    const bool bypass = (*_bypass || destroyRequested);
    MYFLOAT mix;
    if (bypass)
        mix = 0.;
    else
        mix = 1.;
    MYFLOAT inc = 0., phase = 0.;
    MYFLOAT dir = 1;
    LFO *lfo = *_lfo;
    bool lfo_on = lfo && lfo->power();
    if (lfo_on) {
        if (!lfo->stopped() && !lfo->syncmidi())
            inc = (MYFLOAT) (TWOPI_F_P * lfo->phinc());
        else
            inc = 0.;
        dir = lfo->dir();
        phase = lfo->phs() * TWOPI_F_P;
    } else
        phase = (MYFLOAT) (360 - *_pos) / 360. * PI_F_P;// _phase;
    //MYFLOAT inc = (MYFLOAT) (M_2_PI / (MYFLOAT) _STATE->sr * pow(10, *_carrier_freq * .05));
    MYFLOAT xn1, yn1 = 0., xn2, yn2 = 0., upperl, lowerl, upperr, lowerr, upperfilteredl, lowerfilteredl, upperfilteredr, lowerfilteredr, final_leftl, final_leftr, final_rightl, final_rightr = 0.;
    MYFLOAT *coef = _coef, *xarrayl = _xarray[0], *yarrayl = _yarray[0], *xarrayr = _xarray[1], *yarrayr = _yarray[1], *xtlowerl = _xt[0][0], *xtupperl = _xt[0][1], *xtlowerr = _xt[1][0], *xtupperr = _xt[1][1], ytlowerl = _ytlowerl, ytupperl = _ytupperl, ytlowerr = _ytlowerr, ytupperr = _ytupperr;
    for (int32_t i = 0; i < s; i++) {
        xn1 = xn2 = (inl[i] + inr[i]) * .5;
        /* 6th order allpass filter for sine output.
         */
        for (int32_t j = 0; j < 6; j++) {
            yn1 = coef[j] * (xn1 - yarrayl[j]) + xarrayl[j];
            xarrayl[j] = xn1;
            yarrayl[j] = yn1;
            xn1 = yn1;
        }
        //xn2 = inl[i];
        /* 6th order allpass filter for cosine output.
         */
        for (int32_t j = 6; j < 12; j++) {
            yn2 = coef[j] * (xn2 - yarrayl[j]) + xarrayl[j];
            xarrayl[j] = xn2;
            yarrayl[j] = yn2;
            xn2 = yn2;
        }
        const MYFLOAT coscarrier = cosf(phase);
        const MYFLOAT sincarrier = sinf(phase);

        upperl = yn2 * coscarrier - yn1 * sincarrier;
        lowerl = yn2 * coscarrier + yn1 * sincarrier;

        upperfilteredl = .95f * .5 * xtupperl[3] + 0.5 * ytupperl;
        ytupperl = upperfilteredl;
        xtupperl[3] = xtupperl[2];
        xtupperl[2] = xtupperl[1];
        xtupperl[1] = xtupperl[0];
        xtupperl[0] = upperl;

        lowerfilteredl = .95f * .5 * xtlowerl[3] + 0.5 * ytlowerl;
        ytlowerl = lowerfilteredl;
        xtlowerl[3] = xtlowerl[2];
        xtlowerl[2] = xtlowerl[1];
        xtlowerl[1] = xtlowerl[0];
        xtlowerl[0] = lowerl;


        const MYFLOAT mixsrc = 1. - _smooth1;
        outl[i] = inl[i] * mixsrc + (upperl + lowerfilteredl) * .5 * _smooth1;
        outr[i] = inr[i] * mixsrc + (lowerl + upperfilteredl) * .5 * _smooth1;
        sm1(mix);
        //final_leftl = upperl + lowerfilteredl;
        //final_rightl = lowerl + upperfilteredl;

/*
        xn1 = xn2 = inr[i];

        for (int32_t j = 0; j < 6; j++) {
            yn1 = coef[j] * (xn1 - yarrayr[j]) + xarrayr[j];  //sine
            xarrayr[j] = xn1;
            yarrayr[j] = yn1;
            xn1 = yn1;
        }


        for (int32_t j = 6; j < 12; j++) {
            yn2 = coef[j] * (xn2 - yarrayr[j]) + xarrayr[j]; //cosine
            xarrayr[j] = xn2;
            yarrayr[j] = yn2;
            xn2 = yn2;
        }

        upperr = yn1 * coscarrier - yn2 * sincarrier;
        lowerr = yn1 * coscarrier + yn2 * sincarrier;

        upperfilteredr = .95f * .5 * xtupperr[3] + 0.5 * ytupperr;
        ytupperr = upperfilteredr;
        xtupperr[3] = xtupperr[2];
        xtupperr[2] = xtupperr[1];
        xtupperr[1] = xtupperr[0];
        xtupperr[0] = upperr;

        lowerfilteredr = .95f * .5 * xtlowerr[3] + 0.5 * ytlowerr;
        ytlowerr = lowerfilteredr;
        xtlowerr[3] = xtlowerr[2];
        xtlowerr[2] = xtlowerr[1];
        xtlowerr[1] = xtlowerr[0];
        xtlowerr[0] = lowerr;

        final_leftr = upperr + lowerfilteredr;
        final_rightr = lowerr + upperfilteredr;

        outl[i] = (final_leftl + final_leftr) * .5;
        outr[i] = (final_rightr + final_rightl) * .5;
*/


        if (lfo_on) {
            phase += inc * dir;
            if (phase >= TWOPI_F_P)
                phase -= TWOPI_F_P;
            if (phase < 0)
                phase += TWOPI_F_P;
        }
        //out1[n] = yn2;  // cos
        //out2[n] = yn1;  // sin
    }
    _ytupperl = ytupperl;
    _ytlowerl = ytlowerl;
    // _ytupperr = ytupperr;
    // _ytlowerr = ytlowerr;
}


ROTARY_SSB::ROTARY_SSB(TRACK *track) : Effect(track, SPACE_ROTARY, STEREOEFFECT) {
    _bypass = &track->bypass[SPACE_ROTARY];
    _pos = &_STATE->params[track->index][ROTPOS];
    _lfo = &track->lfo[ROTPOS];
    /* pole values taken from Bernie Hutchins, "Musical Engineer's Handbook" */
    MYFLOAT poles[12] = {0.3609, 2.7412, 11.1573, 44.7581, 179.6242, 798.4578,
                        1.2524, 5.5671, 22.3423, 89.6271, 364.7914, 2770.1114};
    MYFLOAT polefreq, rc, alpha, beta;
    /* calculate coefficients for allpass filters, based on sampling rate */
    for (int32_t i = 0; i < 12; i++) {
        /*      _DATA->coef[j] = (1 - (15 * PI * pole[j]) / CS_ESR) /
                (1 + (15 * PI * pole[j]) / CS_ESR); */
        polefreq = poles[i] * 15.;
        rc = 1. / (2. * PI_F_P * polefreq);
        alpha = 1. / rc;
        alpha = alpha * 0.5 * _STATE->onedsr;
        beta = (1. - alpha) / (1. + alpha);
        _xarray[0][i] = _yarray[0][i] = _xarray[1][i] = _yarray[1][i] = 0.0f;
        _coef[i] = -(MYFLOAT) beta;
    }
}


static MYFLOAT poles[12] = {0.3609, 2.7412, 11.1573, 44.7581, 179.6242, 798.4578,
                           1.2524, 5.5671, 22.3423, 89.6271, 364.7914, 2770.1114};

FreqShift::FreqShift(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_SSB, MONOEFFECT) {
    _bypass = &track->bypass[SPACE_SSB];
    _lfo = &track->lfo[SSBMODRATE];
    _freq = &_STATE->params[track->index][SSBMODRATE];
//        osccps = LOG10D20(.25);
    _mix = &_STATE->params[track->index][SSBMODMIX];
    _type = &_STATE->params[track->index][SSBMODTYPE];
    MYFLOAT polefreq, rc, alpha, beta;
    /* calculate coefficients for allpass filters, based on sampling rate */
    for (int32_t i = 0; i < 12; i++) {
        polefreq = poles[i] * 15.;
        rc = 1. / (2. * PI_F_P * polefreq);
        alpha = 1. / rc;
        alpha = alpha * 0.5 * _STATE->onedsr;
        beta = (1. - alpha) / (1. + alpha);
        _xarray[i] = _yarray[i] = 0.0f;
        _coef[i] = -(MYFLOAT) beta;
    }
        _sinewave = _DATA->eq["FULL SINE"].win;
    _oscphase = 0;
}

void FreqShift::compute(MYFLOAT *in, int32_t s) {
    const MYFLOAT mixx = (*_bypass || destroyRequested) ? 0. : _mix->load();
    MYFLOAT fr = LOG2NORMALF(*_freq);
    MYFLOAT phinc_const = fr * _STATE->onedsr;
    MYFLOAT phinc = phinc_const;
    int32_t t = (int) *_type;

    MYFLOAT *envbuf = nullptr;
    auto &fol = _STATE->followerMap[_track->index].at(SSBMODRATE);
    bool env_on = fol.prepare(_chan);
    if (env_on) {
        auto src = fol.source.load();
        envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    }


    MYFLOAT range = 0;
    LFO *lfo = *_lfo;
    bool lfo_on = lfo && lfo->power();
    if (lfo_on) {
        MYFLOAT a = pow(10, _STATE->controls[_track->index][SSBMODRATE].lfo_min.load() * .05);
        MYFLOAT b = pow(10, _STATE->controls[_track->index][SSBMODRATE].lfo_max.load() * .05);
        range = (b-a) * _STATE->onedsr;
        phinc_const = phinc = a * _STATE->onedsr;
    }

    MYFLOAT xn1, yn1 = 0., xn2, yn2 = 0.;

    for (int32_t i = 0; i < s; i++) {
        xn1 = in[i];
        /* 6th order allpass filter for sine output. Structure is
         * 6 first-order allpass sections in series. Coefficients
         * taken from arrays calculated at i-time.
         */
        for (int32_t j = 0; j < 6; j++) {
            yn1 = _coef[j] * (xn1 - _yarray[j]) + _xarray[j];
            _xarray[j] = xn1;
            _yarray[j] = yn1;
            xn1 = yn1;
        }
        xn2 = in[i];
        /* 6th order allpass filter for cosine output. Structure is
         * 6 first-order allpass sections in series. Coefficients
         * taken from arrays calculated at i-time.
         */
        for (int32_t j = 6; j < 12; j++) {
            yn2 = _coef[j] * (xn2 - _yarray[j]) + _xarray[j];
            _xarray[j] = xn2;
            _yarray[j] = yn2;
            xn2 = yn2;
        }
        int32_t sinindex = PHS2INT(_oscphase);
        int32_t cosindex = PHS2INT(_oscphase + .25);

        if (t == UPPER_SIDEBAND)
            in[i] = (MYFLOAT) (in[i] * (1. - _smooth1) +
                             _smooth1 * (yn1 * _sinewave[sinindex] - yn2 * _sinewave[cosindex]));
        else if (t == LOWER_SIDEBAND)
            in[i] = (MYFLOAT) (in[i] * (1. - _smooth1) +
                             _smooth1 * (yn1 * _sinewave[sinindex] + yn2 * _sinewave[cosindex]));
        else if (t == DOUBLE_SIDEBAND)
            in[i] = (MYFLOAT) (in[i] * (1. - _smooth1) +
                             _smooth1 * yn1 * _sinewave[cosindex]);
        sm1(mixx);

        if (env_on) {
            phinc = (_chan == 0 ? fol.detectL(envbuf[i]) : fol.detectR(envbuf[i])) * _STATE->onedsr;
        }
        else if (lfo_on) {
            phinc = phinc_const + lfo->buf[i] * range;
        }
        _oscphase += phinc;
        while (_oscphase > 1.0)
            _oscphase -= 1.0;
    }


}


class FreqShiftBand {
public:
    FreqShiftBand(tsl::AppState *_appState, int32_t sr_, MYFLOAT center, std::atomic<MYFLOAT> *rate_,
                  std::atomic<MYFLOAT> *_depth, std::atomic<MYFLOAT> *_fr, std::atomic<MYFLOAT> *mix_,
                  int32_t type_ = 1) {
        sr = sr_;
        pidsr = PI_P / sr;
        onedsr = 1. / sr;
        freq = _fr;
        mix = mix_;
        type = type_;
        sinewave = _DATA->eq["FULL SINE"].win;
        Reset();
        ComputeCoeffs(center);
        rate = rate_;
        depth = _depth;

        delayline = new MYFLOAT[48000 * 2];

        currentdel = tsl::random::randomfloat(0., sr) * depth->load();
        setDelay();
        writeoff = 0;
    }

    void setDelay() {
        nextdelay = tsl::random::randomfloat(0., sr) * depth->load();
        nextchange = sr / LOG2NORMAL(rate->load());
        delayinc = (nextdelay - currentdel) / (MYFLOAT) nextchange;
        oldrate = rate->load();
        olddepth = depth->load();
        currentchange = 0;
    }


    void Reset() {
        y1_1 = y2_1 = y1_2 = y2_2 = 0.0;
        for (int32_t i = 0; i < 4; i++) {
            xarray[i] = yarray[i] = 0.0;
        }
        oscphase = 0;
    }


    void ComputeCoeffs(MYFLOAT fc) {
        MYFLOAT pfreq = fc * pidsr;
        MYFLOAT pbw = .5 * pfreq;

        MYFLOAT C = 1. / tan(pbw);
        MYFLOAT D = 2. * cos(pfreq);

        a0 = 1. / (1. + C);
        b1 = C * D * a0;
        b2 = (1. - C) * a0;

        MYFLOAT c = -(fc / sr);
        coef[0] = exp(.9511 * c);
        coef[1] = exp(10.52 * c);
        coef[0] = exp(3.751 * c);
        coef[0] = exp(41.50 * c);
    }

    void compute(MYFLOAT *in, MYFLOAT *out, int32_t s, MYFLOAT *noise) {
        MYFLOAT mixx = mix->load();
        MYFLOAT mixsrc = 1.0 - mixx;
        MYFLOAT phinc_const = LOG2NORMAL(*freq) * onedsr;
        MYFLOAT phinc = phinc_const;

        if (*rate != oldrate || *depth != olddepth)
            setDelay();

        MYFLOAT xn1, yn1 = 0., xn2, yn2 = 0., oo;
        for (int32_t i = 0; i < s; i++) {
            MYFLOAT y0 = in[i] + b1 * y1_1 + b2 * y2_1;
            MYFLOAT stage1 = a0 * (y0 - y2_1);
            y2_1 = y1_1;
            y1_1 = y0;
            y0 = stage1 + b1 * y1_2 + b2 * y2_2;
            xn1 = xn2 = a0 * (y0 - y2_2) + noise[i];
            y2_2 = y1_2;
            y1_2 = y0;
            //xn1 = in[i];
            /* yn1 = sine output
             */
            for (int32_t j = 0; j < 2; j++) {
                yn1 = coef[j] * (xn1 - yarray[j]) + xarray[j];
                xarray[j] = xn1;
                yarray[j] = yn1;
                xn1 = yn1;
            }
            /* yn2 = cos output
             */
            for (int32_t j = 2; j < 4; j++) {
                yn2 = coef[j] * (xn2 - yarray[j]) + xarray[j];
                xarray[j] = xn2;
                yarray[j] = yn2;
                xn2 = yn2;
            }
            int32_t sinindex = PHS2INT(oscphase);
            int32_t cosindex = PHS2INT(oscphase + .25);
            if (type == UPPER_SIDEBAND)
                delayline[writeoff] = (MYFLOAT) (in[i] * mixsrc +
                                               mixx * ((yn1 * sinewave[sinindex] -
                                                        yn2 * sinewave[cosindex])) + noise[i]);
            else if (type == LOWER_SIDEBAND)
                delayline[writeoff] = (MYFLOAT) (in[i] * mixsrc +
                                               mixx * ((yn1 * sinewave[sinindex] +
                                                        yn2 * sinewave[cosindex])) + noise[i]);
            else if (type == DOUBLE_SIDEBAND)
                delayline[writeoff] = (MYFLOAT) (in[i] * mixsrc +
                                               (mixx * yn1 * sinewave[cosindex]) + noise[i]);
            oscphase += phinc;
            while (oscphase > 1.0)
                oscphase -= 1.0;

            int32_t read = writeoff - currentdel;
            if (read < 0)
                read += sr;

            out[i] += delayline[read];

            if (++writeoff >= sr)
                writeoff -= sr;


            currentdel += delayinc;
            if (++currentchange >= nextchange)
                setDelay();

        }
    }

private:
    MYFLOAT b1, b2, a0;
    MYFLOAT y1_1, y2_1, y1_2, y2_2;
    MYFLOAT coef[4];
    MYFLOAT xarray[4];
    MYFLOAT yarray[4];
    int32_t type;
    MYFLOAT sr, pidsr;
    std::atomic<MYFLOAT> *freq, *mix, *rate, *depth;
    MYFLOAT oldrate, olddepth;
    MYFLOAT oscphase;
    MYFLOAT onedsr;
    MYFLOAT *sinewave;
    MYFLOAT *delayline;
    int32_t writeoff;
    MYFLOAT delayinc;
    MYFLOAT currentdel;
    MYFLOAT nextdelay;
    int32_t nextchange, currentchange;
};