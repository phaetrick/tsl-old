#include "filter.h"
#include "logger.h"
#include "defines.h"
#include "grainstorm.h"
#include "track.h"
#include "lfo.h"
#include "random.h"
#include "app.h"



void RESON::compute(MYFLOAT *in, int32_t size) {

    /*
     *
     * An implementation of the 2-pole, 2-zero reson filter
     * described by Julius O. Smith and James B. Angell in
     * "A Constant Gain Digital Resonator Tuned by a Single
     * Coefficient," Computer Music Journal, Vol. 6, No. 4,
     * Winter 1982, p.36-39. resonr implements the version
     * where the zeros are located at + and - the square root
     * of r, where r is the pole radius of the reson filter.
     *
     */

    MYFLOAT cf_const = LOG2NORMAL(*_freq);
    auto cf = cf_const;
    auto &fol = _STATE->followerMap[_track->index].at(BPCENTER);
    auto src = fol.source.load();
    MYFLOAT *envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    auto env_on = fol.prepare(_chan);

    MYFLOAT rangecf = 0;
    LFO *lfo_cf = *_lfo_freq;
    bool lfocf_on = lfo_cf && lfo_cf->power();
    if (lfocf_on) {
        auto a = LOG2NORMAL(_STATE->controls[_track->index][BPCENTER].lfo_min.load());
        auto b = LOG2NORMAL(_STATE->controls[_track->index][BPCENTER].lfo_max.load());
        cf_const = a;
        rangecf = b-a;
    }

    MYFLOAT bw_pre_const = LOG2NORMAL(*_bw);
    auto bw_pre = bw_pre_const;
    auto bw = cf * bw_pre;

    MYFLOAT rangebw = 0;
    LFO *lfo_bw = *_lfo_bw;
    bool lfobw_on = lfo_bw && lfo_bw->power();
    if (lfobw_on) {
        auto a = LOG2NORMAL(_STATE->controls[_track->index][BPBW].lfo_min.load());
        auto b = LOG2NORMAL(_STATE->controls[_track->index][BPBW].lfo_max.load());
        bw_pre_const = a;
        rangebw = b-a;
    }
    MYFLOAT gain = dbToLinear60(*_gain), mix;
    if (*_bypass || destroyRequested) {
        mix = 0;
    } else {
        mix = *_mix;
    }
    MYFLOAT lcf = -1., lbw = -1.;
    MYFLOAT r = 0.0; /* radius & scaling factor */
    MYFLOAT c1 = 0.0, c2 = 0.0;   /* filter coefficients */
    MYFLOAT scalefact = 1. / std::sqrt(bw_pre_const);

    for (int32_t n = 0; n < size; n++) {
        if (env_on) {
            cf =  _chan == 0 ? fol.detectL(envbuf[n]) : fol.detectR(envbuf[n]);
            bw = bw_pre_const * cf;
        }
        if (lfocf_on) {
            cf = env_on ? 18. + DISTANCE(18., cf) *  lfo_cf->buf[n] : cf_const +
                 lfo_cf->buf[n] * rangecf;
            bw = bw_pre_const * cf;
            //phasecf += inccf * dircf;
        }
        if (lfobw_on) {
            auto val = bw_pre_const +
                         lfo_bw->buf[n] * rangebw;
            bw = val * cf;
            scalefact = 1. / sqrt(val);
        }
        if (cf != lcf || bw != lbw) {
            lcf = cf;
            lbw = bw;
            r = std::exp(bw * _minuspidsr);
            c1 = 2. * r * cos(cf * _twopidsr);
            c2 = r * r;
        }
        auto xn = in[n];
        if(isnan(xn))xn=0;
        MYFLOAT yn;
        in[n] = (MYFLOAT) xn * (1. - _smooth1) +
                 _smooth1 * scalefact * ((yn = (1. - r) * (xn - r * _x2) + c1 * _y1 - c2 * _y2)) *
                _smooth2;
        if(isnan(yn))yn = 0. ;
        smmixgain(mix, gain);
        _x2 = _x1;
        _x1 = xn;
        _y2 = _y1;
        _y1 = yn;
    }
}



void RESONGRAIN::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT gain, mix;

    if (_bypass->load() || destroyRequested) {
        gain = 1.;
        mix = 0.;
    } else {
        gain = dbToLinear60(*_gain);
        mix = *_mix;
    }

    const MYFLOAT twopidsr = _twopidsr;
    const MYFLOAT minuspidsr = _minuspidsr;
    MYFLOAT r = 0.0; /* radius & scaling factor */
    MYFLOAT c1 = 0.0, c2 = 0.0;   /* filter coefficients */
    MYFLOAT xn, yn;

    MYFLOAT cf =
            pow(10., .05 * tsl::random::randomfloat(*_freq_min, *_freq_max));

    MYFLOAT bw_pre = pow(10, .05 * tsl::random::randomfloat(*_bw_min, *_bw_max));

    MYFLOAT bw = cf * bw_pre;

    MYFLOAT lcf = 1., lbw = 1.;

    MYFLOAT x1 = _x1;
    MYFLOAT x2 = _x2;
    MYFLOAT y1 = _y1;
    MYFLOAT y2 = _y2;

    MYFLOAT *comp = _temp_buffer.data();
    std::memcpy(comp, in, size * sizeof(MYFLOAT));
    MYFLOAT maxsrc = *std::max_element(in, in + size);

    if (cf != lcf || bw != lbw) {
        lcf = cf;
        lbw = bw;
        r = exp((MYFLOAT) (bw * minuspidsr));
        c1 = 2.0 * r * cos((MYFLOAT) (cf * twopidsr));
        c2 = r * r;
    }
    for (int32_t n = 0; n < size; n++) {
        xn = (MYFLOAT) in[n];
        in[n] = (MYFLOAT) (yn = (1.0 - r) * (xn - r * x2) + c1 * y1 - c2 * y2);
        x2 = x1;
        x1 = xn;
        y2 = y1;
        y1 = yn;
    }
    UDD(x1);
    UDD(x2);
    UDD(y1);
    UDD(y2);
    _x1 = x1;
    _x2 = x2;
    _y1 = y1;
    _y2 = y2;
    MYFLOAT maxout = *std::max_element(in, in + size);
    const MYFLOAT adjust = maxout > 0.f ? maxsrc / maxout : 0.f;
    //LOGE("%g %g %f", center, q, adjust);

    for (int32_t i = 0; i < size; i++) {
        in[i] = (in[i] * adjust * _smooth1 + comp[i] * (1.f - _smooth1)) * _smooth2;
        sm1(mix);
        sm2(gain);
    }
}

RESON::RESON(TRACK *track, int32_t channel, bool isGrain) : Effect(track, channel,
                                                               isGrain ? SPACE_GRAIN_BP
                                                                       : SPACE_BANDPASS,
                                                               isGrain ? GRAINEFFECT : MONOEFFECT) {
    _twopidsr = TWOPI_P / (MYFLOAT) _STATE->sr;
    _minuspidsr = -(PI_P / (MYFLOAT) _STATE->sr);
    _temp_buffer.resize(_DATA->maxgrainsize);
    std::fill(_temp_buffer.begin(), _temp_buffer.end(), 0);
    _bypass = &track->bypass[SPACE_BANDPASS];
    _mix = &_STATE->params[track->index][BPMIX];
    _gain = &_STATE->params[track->index][BPGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _freq = &_STATE->params[track->index][BPCENTER];
    _bw = &_STATE->params[track->index][BPBW];
    _lfo_bw = &track->lfo[BPBW];
    _lfo_freq = &track->lfo[BPCENTER];
}


RESONGRAIN::RESONGRAIN(TRACK *track, int32_t channel) : RESON(track, channel, true) {
    _bypass = &track->bypass[SPACE_GRAIN_BP];
    _mix = &_STATE->params[track->index][GRAINBPMIX];
    _gain = &_STATE->params[track->index][GRAINBPGAIN];
    _freq_min = &_STATE->params[track->index][GRAINBPCENTERMIN];
    _freq_max = &_STATE->params[track->index][GRAINBPCENTERMAX];
    _bw_min = &_STATE->params[track->index][GRAINBPBWMIN];
    _bw_max = &_STATE->params[track->index][GRAINBPBWMAX];
}

void DCBLOCKER::compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s) {
    const MYFLOAT mix = (*_bypass || destroyRequested) ? 0. : 1.;
    for (int32_t i = 0; i < s; i++) {
        const MYFLOAT mixsrc = 1.f - _smooth1;
        auto templ = inl[i];
        outl[i] = inl[i] * mixsrc + (_ytl = templ - _xtl + .995 * _ytl) * _smooth1;
        _xtl = templ;
        auto tempr = inr[i];
        outr[i] = inr[i] * mixsrc + (_ytr = tempr - _xtr + .995 * _ytr) * _smooth1;
        _xtr = tempr;
        sm1(mix);
    }
    UDD(_xtl);
    UDD(_xtr);
    UDD(_ytl);
    UDD(_ytr);
}

void DCBLOCKER::compute(MYFLOAT *in, int32_t size) {
    for (int32_t i = 0; i < size; i++) {
        auto templ = in[i];
        in[i] = _ytl = templ - _xtl + .995 * _ytl;
        _xtl = templ;
    }
    UDF(_xtl);
    UDF(_ytl);
}

DCBLOCKER::DCBLOCKER(TRACK *track) : Effect(track, SPACE_DCS, STEREOEFFECT) {
    _bypass = &track->bypass[SPACE_DCS];
};

MOOGLADDER::MOOGLADDER(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_MOOGLADDER,
                                                           MONOEFFECT) {
    _bypass = &track->bypass[SPACE_MOOGLADDER];
    _track = track;
    _freq = &_STATE->params[track->index][MOOGCUT];
    _res = &_STATE->params[track->index][MOOGRES];
    _mix = &_STATE->params[track->index][MOOGMIX];
    _gain = &_STATE->params[track->index][MOOGGAIN];
    _smooth2 = dbToLinear60(*_gain);
    memset(_delay, '\0', 6 * sizeof(MYFLOAT));
    memset(_tanhstg, '\0', 3 * sizeof(MYFLOAT));
    _oldfreq = 0.0f;
    _oldres = -1.0f;     /* ensure calculation on first cycle */
}

#define THERMAL (0.000025) /* (1.0 / 40000.0) transistor thermal voltage  */

void MOOGLADDER::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT gain = dbToLinear60(*_gain), mix;
    if (*_bypass || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;
    }
    const MYFLOAT freq_const = pow(10, *_freq * .05);
    const MYFLOAT res = *_res * .95;
    MYFLOAT *delay = _delay;
    MYFLOAT *tanhstg = _tanhstg;
    MYFLOAT stg[4], input;
    auto &fol = _STATE->followerMap[_track->index].at(MOOGCUT);
    auto src = fol.source.load();
    MYFLOAT *envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    auto env_on = fol.prepare(_chan);

    MYFLOAT tune = _tune, res4 = _res4;

    if ((!env_on && freq_const != _oldfreq) || res != _oldres) {
        _oldfreq = freq_const;
        _oldres = res;
        const MYFLOAT fc = freq_const / (MYFLOAT) _STATE->sr;
        const MYFLOAT f = 0.5 * fc;
        const MYFLOAT fc2 = fc * fc;
        const MYFLOAT fc3 = fc2 * fc;
        /* frequency & amplitude correction  */
        const MYFLOAT fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
        const MYFLOAT acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
        tune = (1.0 - exp(-(TWOPI_P * f * fcr))) / THERMAL;   /* filter tuning  */
        res4 = 4.0 * res * acr;
    }

    for (int32_t i = 0; i < size; i++) {
        if (env_on) {
            MYFLOAT freq = _chan == 0 ? fol.detectL(envbuf[i]) : fol.detectR(envbuf[i]);
            if (freq != _oldfreq) {
                _oldfreq = freq;
                const MYFLOAT fc = freq / (MYFLOAT) _STATE->sr;
                const MYFLOAT f = 0.5 * fc;
                const MYFLOAT fc2 = fc * fc;
                const MYFLOAT fc3 = fc2 * fc;
                /* frequency & amplitude correction  */
                const MYFLOAT fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
                const MYFLOAT acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
                tune = (1.0 - exp(-(TWOPI_P * f * fcr))) / THERMAL;   /* filter tuning  */
                res4 = 4.0 * res * acr;
            }
        }

        /* sr is half the actual filter sampling rate  */

        /* oversampling  */
        for (int32_t j = 0; j < 2; j++) {
            /* filter stages  */
            input = in[i] - res4 * delay[5];
            delay[0] = stg[0] = delay[0] + tune * (tanh(input * THERMAL) - tanhstg[0]);
            input = stg[0];
            stg[1] = delay[1] + tune * ((tanhstg[0] = tanh(input * THERMAL)) - tanhstg[1]);
            input = delay[1] = stg[1];
            stg[2] = delay[2] + tune * ((tanhstg[1] = tanh(input * THERMAL)) - tanhstg[2]);
            input = delay[2] = stg[2];
            stg[3] = delay[3] + tune * ((tanhstg[2] =
                                                 tanh(input * THERMAL)) - tanh(delay[3] * THERMAL));
            delay[3] = stg[3];
            /* 1/2-sample delay for phase compensation  */
            delay[5] = (stg[3] + delay[4]) * 0.5;
            delay[4] = stg[3];
        }
        in[i] = in[i] * (1.f - _smooth1) + delay[5] * _smooth1 * _smooth2;
        smmixgain(mix, gain);
    }
    _tune = tune;
    _res4 = res4;
}