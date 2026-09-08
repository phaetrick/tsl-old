//
// Created by pr on 15.08.20.
//

#include "Reverb.h"
#include "track.h"
#include "defines.h"
#include "tools.h"


/*
 * Rlinctl (X_window   *parent,
             X_callback *cbobj,
             RotaryImg  *image,
             int        xp,
             int        yp,
             int        cm,
             int        dd,
             double     vmin,
             double     vmax,
             double     vini,
             int        cbind = 0);

 *
_rotary [R_DELAY] = new Rlinctl (this, this, &r_delay_img, x, 0, 160, 5,  0.02,  0.100,  0.04, R_DELAY);
_rotary [R_XOVER] = new Rlogctl (this, this, &r_xover_img, x, 0, 200, 5,  50.0, 1000.0, 200.0, R_XOVER);
_rotary [R_RTLOW] = new Rlogctl (this, this, &r_rtlow_img, x, 0, 200, 5,   1.0,    8.0,   3.0, R_RTLOW);
_rotary [R_RTMID] = new Rlogctl (this, this, &r_rtmid_img, x, 0, 200, 5,   1.0,    8.0,   2.0, R_RTMID);
_rotary [R_FDAMP] = new Rlogctl (this, this, &r_fdamp_img, x, 0, 200, 5, 1.5e3, 24.0e3, 6.0e3, R_FDAMP);
x += 315;
_rotary [R_EQ1FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5,  40.0,  2.5e3, 160.0, R_EQ1FR);
_rotary [R_EQ1GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ1GN);
x += 110;
_rotary [R_EQ2FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5, 160.0,   10e3, 2.5e3, R_EQ2FR);
_rotary [R_EQ2GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ2GN);
x += 110;
_rotary [R_OPMIX] = new Rlinctl (this, this, &r_opmix_img, x, 0, 180, 5,   0.0 ,   1.0,   0.5, R_OPMIX);
_rotary [R_RGXYZ] = new Rlinctl (this, this, &r_rgxyz_img, x, 0, 180, 5,  -9.0 ,   9.0,   0.0, R_RGXYZ);
*/
Pareq::Pareq(void) :
        _touch0(0),
        _touch1(0),
        _state(BYPASS),
        _g0(1),
        _g1(1),
        _f0(1e3f),
        _f1(1e3f) {
    setfsamp(0.0f);
}


Pareq::~Pareq(void) {
}


void Pareq::setfsamp(float fsamp) {
    _fsamp = fsamp;
    reset();
}


void Pareq::reset(void) {
    memset(_z1, 0, sizeof(float) * MAXCH);
    memset(_z2, 0, sizeof(float) * MAXCH);
}


void Pareq::prepare(int nsamp) {
    bool upd = false;
    float g, f;

    if (_touch1 != _touch0) {
        g = _g0;
        f = _f0;
        if (g != _g1) {
            upd = true;
            if (g > 2 * _g1) _g1 *= 2;
            else if (_g1 > 2 * g) _g1 /= 2;
            else _g1 = g;
        }
        if (f != _f1) {
            upd = true;
            if (f > 2 * _f1) _f1 *= 2;
            else if (_f1 > 2 * f) _f1 /= 2;
            else _f1 = f;
        }
        if (upd) {
            if ((_state == BYPASS) && (_g1 == 1)) {
                calcpar1(0, _g1, _f1);
            } else {
                _state = SMOOTH;
                calcpar1(nsamp, _g1, _f1);
            }
        } else {
            _touch1 = _touch0;
            if (fabs(_g1 - 1) < 0.001f) {
                _state = BYPASS;
                reset();
            } else {
                _state = STATIC;
            }
        }
    }
}


void Pareq::calcpar1(int nsamp, float g, float f) {
    float b, c1, c2, gg;

    f *= float(M_PI) / _fsamp;
    b = 2 * f / sqrtf(g);
    gg = 0.5f * (g - 1);
    c1 = -cosf(2 * f);
    c2 = (1 - b) / (1 + b);
    if (nsamp) {
        _dc1 = (c1 - _c1) / nsamp + 1e-30f;
        _dc2 = (c2 - _c2) / nsamp + 1e-30f;
        _dgg = (gg - _gg) / nsamp + 1e-30f;
    } else {
        _c1 = c1;
        _c2 = c2;
        _gg = gg;
    }
}


void Pareq::process1(int nsamp, int nchan, float *data[]) {
    int i, j;
    float c1, c2, gg;
    float x, y, z1, z2;
    float *p;

    c1 = _c1;
    c2 = _c2;
    gg = _gg;
    if (_state == SMOOTH) {
        for (i = 0; i < nchan; i++) {
            p = data[i];
            z1 = _z1[i];
            z2 = _z2[i];
            c1 = _c1;
            c2 = _c2;
            gg = _gg;
            for (j = 0; j < nsamp; j++) {
                c1 += _dc1;
                c2 += _dc2;
                gg += _dgg;
                x = *p;
                y = x - c2 * z2;
                *p++ = x - gg * (z2 + c2 * y - x);
                y -= c1 * z1;
                z2 = z1 + c1 * y;
                z1 = y + 1e-20f;
            }
            _z1[i] = z1;
            _z2[i] = z2;
        }
        _c1 = c1;
        _c2 = c2;
        _gg = gg;
    } else {
        for (i = 0; i < nchan; i++) {
            p = data[i];
            z1 = _z1[i];
            z2 = _z2[i];
            for (j = 0; j < nsamp; j++) {
                x = *p;
                y = x - c2 * z2;
                *p++ = x - gg * (z2 + c2 * y - x);
                y -= c1 * z1;
                z2 = z1 + c1 * y;
                z1 = y + 1e-20f;
            }
            _z1[i] = z1;
            _z2[i] = z2;
        }
    }
}
// -----------------------------------------------------------------------


#define MAX_PITCHMOD    20.0
#define DELAYPOS_SHIFT  28
#define DELAYPOS_SCALE  0x10000000
#define DELAYPOS_MASK   0x0FFFFFFF


static const float jpScale = 0.25;


static const double reverbParams2[8][4] = {
        {2473.0, 0.0010, 3.100, 1966.0},
        {2767.0, 0.0011, 3.500, 29491.0},
        {3217.0, 0.0017, 1.110, 22937.0},
        {3557.0, 0.0006, 3.973, 9830.0},
        {3907.0, 0.0010, 2.341, 20643.0},
        {4127.0, 0.0011, 1.897, 22937.0},
        {2143.0, 0.0017, 0.891, 29491.0},
        {1933.0, 0.0006, 3.221, 14417.0}
};


int32_t REVERB5::delay_line_max_samples(int32_t n) {
    double maxDel = reverbParams[n][0];
    maxDel += (reverbParams[n][1] * (double) 1.125);
    return (int32_t) next_pow_2(maxDel * sampleRate + 16.5);
}

int32_t REVERB5::delay_line_bytes_alloc(int32_t n) {
    int32_t nBytes = (int32_t) sizeof(delayLine) - (int32_t) sizeof(float);
    nBytes += (delay_line_max_samples(n) * (int32_t) sizeof(float));
    nBytes = (nBytes + 15) & (~15);
    return nBytes;
}


void REVERB5::next_random_lineseg(delayLine *lp, int32_t n) {
    /* update random seed */
    if (lp->seedVal < 0)
        lp->seedVal += 0x10000;
    lp->seedVal = (lp->seedVal * 15625 + 1) & 0xFFFF;
    if (lp->seedVal >= 0x8000)
        lp->seedVal -= 0x10000;
    /* length of next segment in samples */
    lp->randLine_cnt = (int32_t) ((sampleRate / reverbParams[n][2]) + 0.5);
    float prvDel = (float) lp->writePos;
    prvDel -= ((float) lp->readPos
               + ((float) lp->readPosFrac / (float) DELAYPOS_SCALE));
    while (prvDel < 0.0)
        prvDel += (float) lp->bufferSize;
    prvDel = prvDel / sampleRate;    /* previous delay time in seconds */
    float nxtDel = (float) lp->seedVal * reverbParams[n][1] / 32768.f;
    /* next delay time in seconds */
    nxtDel = reverbParams[n][0] + (nxtDel);

    /* calculate phase increment per sample */
    float phs_incVal = (prvDel - nxtDel) / (float) lp->randLine_cnt;
    phs_incVal = phs_incVal * sampleRate + 1.0f;
    lp->readPosFrac_inc = (int32_t) (phs_incVal * DELAYPOS_SCALE + 0.5);
}


void REVERB5::init_delay_line(delayLine *lp, int32_t n) {
    /* int32_t     i; */

    /* calculate length of delay line */
    lp->bufferSize = delay_line_max_samples(n);
    lp->bufandmask = lp->bufferSize - 1;
    lp->dummy = 0;
    lp->writePos = 0;
    /* set random seed */
    lp->seedVal = (int32_t) (reverbParams[n][3] + 0.5);

    /* set initial delay time */
    double readPos = (double) lp->seedVal * reverbParams[n][1] / 32768.f;
    readPos = reverbParams[n][0] + (readPos);
    readPos = (double) lp->bufferSize - (readPos * sampleRate);
    lp->readPos = (int32_t) readPos;
    readPos = (readPos - (double) lp->readPos) * (double) DELAYPOS_SCALE;
    lp->readPosFrac = (int32_t) (readPos + 0.5);
    /* initialise first random line segment */
    next_random_lineseg(lp, n);
    /* clear delay line to zero */
    lp->filterState = 0.0;
    memset(lp->buf, 0, sizeof(float) * lp->bufferSize);
    /* for (i = 0; i < lp->bufferSize; i++) */
    /*   lp->buf[i] = FL(0.0); */
}

REVERB5::REVERB5(TRACK *track) : Effect(track) {
    int32_t i;
    int32_t nBytes;
    _bypass = &track->bypass[SPACE_REVERB5];
    sampleRate = _sr;
    kFeedBack = &tsl::app::params[track->index][REVERB5FB];
    kLPFreq = &tsl::app::params[track->index][REVERB5DAMP];
    _mix = &tsl::app::params[track->index][REVERB5MIX];
    _gain = &tsl::app::params[track->index][REVERB5GAIN];

    memcpy(reverbParams, reverbParams2, sizeof(double) * 8 * 4);
    for (i = 0; i < 8; i++) {
        reverbParams[i][0] *= _sr / 44100.;

        reverbParams[i][0] /= (double) 44100;
        _tdelay[i] = reverbParams[i][0];
        _averagedelay += _tdelay[i] * _sr;

        /*
        _delayline[i].init(_sr, _tdelay[i], reverbParams[i][1]);
        _delayline[i].setRndRate(reverbParams[i][2]);
         */
    }
    _averagedelay *= .125;

    nBytes = 0;
    for (i = 0; i < 8; i++)
        nBytes += delay_line_bytes_alloc(i);
    auxData.resize(nBytes);

    /* set up delay lines */
    nBytes = 0;
    for (i = 0; i < 8; i++) {
        delayLines[i] = (delayLine *) (auxData.data() + (int32_t) nBytes);
        init_delay_line(delayLines[i], i);
        nBytes += delay_line_bytes_alloc(i);
    }
    _dampFact = 1.0;
    prv_LPFreq = 0.0f;

    _damp = &tsl::app::params[track->index][REV5DAMP2];
    _xover = &tsl::app::params[track->index][REV5XOVER];
    _t60low = &tsl::app::params[track->index][REV5T60LOW];
    _t60mid = &tsl::app::params[track->index][REV5T60HI];

    _predelay = &tsl::app::params[track->index][REV5PREDELAY];
    _predelayprev = *_predelay;
    _predelayL.init(_sr * 1.05, _predelayprev * _sr * 0.001);
    _predelayR.init(_sr * 1.05, _predelayprev * _sr * 0.001);
/*
float r[_size]{}, l[_size]{};
    MEASUSEINIT

    for (int i = 0; i < 100; i++)
        compute(l, r, l, r);
    MEASURESTOP
    MEASURESTART
    for (int i = 0; i < 100; i++)
        compute2(l, r, l, r);
    MEASURESTOP
*/
}

void REVERB5::computecubic(float *inl, float *inr, float *outl, float *outr) {
    if (*_bypass)
        return;

    float mix = (float) *_mix;
    float mixsrc = 1.0f - mix;
    float gain = (float) pow(10, *_gain * .05);


    float xt = _xt;
    float yt = _yt;


    /* update delay lines */

    if (_olddamp != *_damp || _oldt60low != *_t60low || _oldt60mid != *_t60mid ||
        _oldxover != *_xover)
        updateFilters();

    if (_predelayprev != *_predelay) {
        _predelayprev = *_predelay;
        _predelayL.setDelay(_predelayprev * _sr * 0.001);
        _predelayR.setDelay(_predelayprev * _sr * 0.001);
    }

    for (int i = 0; i < _size; i++) {
        /* calculate "resultant junction pressure" and mix to input signals */


        float ainL = 0.0;
        for (int n = 0; n < 8; n++) {
            ainL += delayLines[n]->filterState;
        }
        float temp = ainL;
        yt = temp - xt + .995f * yt;
        UNDENORMAL(yt);
        ainL = yt * jpScale;
        xt = temp;
        float ainR = ainL + (float) _predelayL.tick(inr[i]);
        ainL = ainL + (float) _predelayR.tick(inl[i]);
        /* loop through all delay lines */
        float aoutL = 0.0;
        float aoutR = 0.0;

        for (int n = 0; n < 8; n++) {
            delayLine *lp = delayLines[n];
            /* send input signal and feedback to delay line */
            lp->buf[lp->writePos++] = (float) ((n & 1 ? ainR : ainL)
                                               - lp->filterState);
            lp->writePos &= lp->bufandmask;
            /* read from delay line with cubic interpolation */
            if (lp->readPosFrac >= DELAYPOS_SCALE) {
                lp->readPos += (lp->readPosFrac >> DELAYPOS_SHIFT);
                lp->readPosFrac &= DELAYPOS_MASK;
            }

            float frac = (float) lp->readPosFrac * (1.f / (float) DELAYPOS_SCALE);
            /* calculate interpolation coefficients */

            float a2 = frac * frac;
            a2 -= 1.0;
            a2 *= (1.0 / 6.0);
            float a1 = frac;
            a1 += 1.0;
            a1 *= 0.5;
            float am1 = a1 - 1.f;
            float a0 = 3.f * a2;
            a1 -= a0;
            am1 -= a2;
            a0 -= frac;


            lp->readPos &= lp->bufandmask;

            int32_t readPos = lp->readPos;
            float v0 = lp->buf[readPos];
            --readPos;
            readPos &= lp->bufandmask;
            float vm1 = lp->buf[readPos];
            readPos += 2;
            readPos &= lp->bufandmask;
            float v1 = lp->buf[readPos];
            ++readPos;
            readPos &= lp->bufandmask;
            float v2 = lp->buf[readPos];
            lp->filterState = v0 = _filt[n].process(
                    (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0);

            /* mix to output */
            if (n & 1)
                aoutR += v0;
            else
                aoutL += v0;
            lp->readPosFrac += lp->readPosFrac_inc;

            /* start next random line segment if current one has reached endpoint */
            if (--(lp->randLine_cnt) <= 0)
                next_random_lineseg(lp, n);
        }
        outl[i] = (float) (aoutL * mix + inl[i] * mixsrc) * gain;
        outr[i] = (float) (aoutR * mix + inr[i] * mixsrc) * gain;
    }
    _xt = xt;
    _yt = yt;
}

void REVERB5::compute(float *inl, float *inr, float *outl, float *outr) {
    if (*_bypass)
        return;

    auto mix = (float) *_mix;
    float mixsrc = 1.0f - mix;
    auto gain = (float) pow(10, *_gain * .05);
    mix *= gain * _gainfact;
    mixsrc *= gain;

    float xt = _xt;
    float yt = _yt;


    /* update delay lines */

    if (_olddamp != *_damp || _oldt60low != *_t60low || _oldt60mid != *_t60mid ||
        _oldxover != *_xover)
        updateFilters();

    if (_predelayprev != *_predelay) {
        _predelayprev = *_predelay;
        _predelayL.setDelay(_predelayprev * _sr * 0.001);
        _predelayR.setDelay(_predelayprev * _sr * 0.001);
    }

    for (int i = 0; i < _size; i++) {
        UNDENORMAL(inl[i]);
        UNDENORMAL(inr[i]);

        /* calculate "resultant junction pressure" and mix to input signals */

        float ainL = 0.0;
        for(auto &d : delayLines)
            ainL += d->filterState;

        float temp = ainL;
        yt = temp - xt + .995f * yt;
        ainL = yt * jpScale;
        xt = temp;
        float ainR = ainL + (float) _predelayL.tick(inr[i]);
        ainL = ainL + (float) _predelayR.tick(inl[i]);
        /* loop through all delay lines */
        float aoutL = 0.0;
        float aoutR = 0.0;

        for (int n = 0; n < 8; n++) {
            delayLine *lp = delayLines[n];
            /* send input signal and feedback to delay line */
            lp->buf[lp->writePos++] = (float) ((n & 1 ? ainR : ainL)
                                               - lp->filterState);
            lp->writePos &= lp->bufandmask;
            /* read from delay line with cubic interpolation */
            if (lp->readPosFrac >= DELAYPOS_SCALE) {
                lp->readPos += (lp->readPosFrac >> DELAYPOS_SHIFT);
                lp->readPosFrac &= DELAYPOS_MASK;
            }

            float frac = (float) lp->readPosFrac * (1.f / (float) DELAYPOS_SCALE);

            lp->readPos &= lp->bufandmask;

            lp->filterState = _filt[n].process((lp->buf[lp->readPos] +
                                                frac *
                                                (lp->buf[((lp->readPos + 1) & lp->bufandmask)] -
                                                 lp->buf[lp->readPos])));

            /* mix to output */
            if (n & 1u)
                aoutR += lp->filterState;
            else
                aoutL += lp->filterState;

            lp->readPosFrac += lp->readPosFrac_inc;

            /* start next random line segment if current one has reached endpoint */
            if (--(lp->randLine_cnt) <= 0)
                next_random_lineseg(lp, n);
        }
        outl[i] *= mixsrc;
        outl[i] += aoutL * mix;
        outr[i] *= mixsrc;
        outr[i] += aoutR * mix;
    }
    for (auto &d : delayLines) {
        auto tmp = d->filterState;
        if (std::fpclassify(tmp) != FP_NORMAL && std::fpclassify(tmp) != FP_ZERO) {
            for (auto &dd : delayLines) { UNDENORMAL(dd->filterState)}
            std::fill(auxData.begin(), auxData.end(), 0);
            UNDENORMAL(yt);
            UNDENORMAL(xt);
            for(auto &f : _filt)
                f.reset();
            break;
        }
    }
    _xt = xt;
    _yt = yt;
}

void REVERB5::updateFilters() {
    _oldt60low = *_t60low;
    _oldt60mid = *_t60mid;
    _oldxover = *_xover;
    _olddamp = *_damp;
    const float crossover = LOG2NORMALF(_oldxover);
    double chi;
    double wlo = TWOPI_P * crossover / _sr;
    if (LOG2NORMAL(_olddamp) > 0.49f * _sr) chi = 2;
    else chi = 1 - cos(TWOPI_P * LOG2NORMAL(_olddamp) / _sr);

    for (int i = 0; i < 8; i++) {
        _filt[i].set_params(_tdelay[i], LOG2NORMALF(_oldt60mid), LOG2NORMALF(_oldt60low), wlo,
                            0.5f * LOG2NORMALF(_oldt60mid), chi);
    }

    const float tot = 10.f;
    float low = log2(crossover / 20.f) / tot *
                powf(0.001f, (_averagedelay) / (LOG2NORMALF(_oldt60low) * _sr));
    float high = log2(20000.f / crossover) / tot * powf(0.001f, (_averagedelay) /
                                                                (LOG2NORMALF(_oldt60mid) *
                                                                 _sr));//pow(0.001, (3500) / (LOG2NORMAL(_oldt60mid) * _sr));
    _gainfact = 1.f / ((high + low) * 4.f);

    // LOGE("%f %f %f", _gainfact, low, high);
}


int32_t REVERB5b::delay_line_max_samples(int32_t n) {
    double maxDel = reverbParams[n][0];
    maxDel += (reverbParams[n][1] * (double) 1.125);
    return (int32_t) next_pow_2(maxDel * sampleRate + 16.5);
}

int32_t REVERB5b::delay_line_bytes_alloc(int32_t n) {
    int32_t nBytes = (int32_t) sizeof(delayLine) - (int32_t) sizeof(float);
    nBytes += (delay_line_max_samples(n) * (int32_t) sizeof(float));
    nBytes = (nBytes + 15) & (~15);
    return nBytes;
}


void REVERB5b::next_random_lineseg(delayLine *lp, int32_t n) {
    lp->randLine_cnt = (int32_t) ((sampleRate / reverbParams[n][2]) + 0.5);
    if (lp->seedVal < 0)
        lp->seedVal += 0x10000;
    lp->seedVal = (lp->seedVal * 15625 + 1) & 0xFFFF;
    if (lp->seedVal >= 0x8000)
        lp->seedVal -= 0x10000;
    float prvDel = (float) lp->writePos;
    prvDel -= ((float) lp->readPos
               + ((float) lp->readPosFrac / (float) DELAYPOS_SCALE));
    while (prvDel < 0.0)
        prvDel += (float) lp->bufferSize;
    prvDel = prvDel / sampleRate;    /* previous delay time in seconds */
    float nxtDel = (float) lp->seedVal * reverbParams[n][1] / 32768.f;
    /* next delay time in seconds */
    nxtDel = reverbParams[n][0] + (nxtDel);

    /* calculate phase increment per sample */
    float phs_incVal = (prvDel - nxtDel) / (float) lp->randLine_cnt;


    phs_incVal = phs_incVal * sampleRate + 1.0f;

    //LOGE("%d %f",n,  SHIFT2SEMITONES(1 + reverbParams[n][1] * _sr / (float) lp->randLine_cnt));


    lp->readPosFrac_inc = (int32_t) (phs_incVal * DELAYPOS_SCALE + 0.5);
}


void REVERB5b::init_delay_line(delayLine *lp, int32_t n) {
    /* int32_t     i; */

    /* calculate length of delay line */
    lp->bufferSize = delay_line_max_samples(n);
    lp->bufandmask = lp->bufferSize - 1;
    lp->dummy = 0;
    lp->writePos = 0;
    /* set random seed */
    lp->seedVal = (int32_t) (reverbParams[n][3] + 0.5);

    /* set initial delay time */
    double readPos = (double) lp->seedVal * reverbParams[n][1] / 32768.f;
    readPos = reverbParams[n][0] + (readPos);
    readPos = (double) lp->bufferSize - (readPos * sampleRate);
    lp->readPos = (int32_t) readPos;
    readPos = (readPos - (double) lp->readPos) * (double) DELAYPOS_SCALE;
    lp->readPosFrac = (int32_t) (readPos + 0.5);
    /* initialise first random line segment */
    next_random_lineseg(lp, n);
    /* clear delay line to zero */
    lp->filterState = 0.0;
    memset(lp->buf, 0, sizeof(float) * lp->bufferSize);
    /* for (i = 0; i < lp->bufferSize; i++) */
    /*   lp->buf[i] = FL(0.0); */
}

REVERB5b::REVERB5b(TRACK *track) : Effect(track) {
    int32_t i;
    int32_t nBytes;
    _bypass = &track->bypass[SPACE_REVERB5];
    sampleRate = _sr;
    _fb = &tsl::app::params[track->index][REVERB5FB];
    _lpcut = &tsl::app::params[track->index][REV5LPCUT];
    _oldlpcut = *_lpcut;
    _hpcut = &tsl::app::params[track->index][REV5HPCUT];
    _oldhpcut = *_hpcut;
    _mix = &tsl::app::params[track->index][REVERB5MIX];
    _gain = &tsl::app::params[track->index][REVERB5GAIN];

    memcpy(reverbParams, reverbParams2, sizeof(double) * 8 * 4);
    for (i = 0; i < 8; i++) {
        reverbParams[i][0] *= _sr / 44100.;

        reverbParams[i][0] /= (double) 44100;
        //  reverbParams[i][0] += reverbParams[i][1] * .5f;
        //reverbParams[i][0] = findNextPrime((int) (reverbParams[i][0] * _sr)) / _sr - reverbParams[i][1] * .5f ;
        _tdelay[i] = reverbParams[i][0];
        _averagedelay += _tdelay[i] * _sr;
        _filt[i].init(_sr, LOG2NORMALF(_oldhpcut), LOG2NORMALF(_oldlpcut));
    }
    _averagedelay *= .125;

    nBytes = 0;
    for (i = 0; i < 8; i++)
        nBytes += delay_line_bytes_alloc(i);
    auxData.resize(nBytes);

    /* set up delay lines */
    nBytes = 0;
    for (i = 0; i < 8; i++) {
        delayLines[i] = (delayLine *) (auxData.data() + (int32_t) nBytes);
        init_delay_line(delayLines[i], i);
        nBytes += delay_line_bytes_alloc(i);
    }

    _predelay = &tsl::app::params[track->index][REV5PREDELAY];
    _predelayprev = *_predelay;
    _predelayL.init(_sr * 1.05, _predelayprev * _sr * 0.001);
    _predelayR.init(_sr * 1.05, _predelayprev * _sr * 0.001);
/*
float r[_size]{}, l[_size]{};
    MEASUSEINIT

    for (int i = 0; i < 100; i++)
        compute(l, r, l, r);
    MEASURESTOP
    MEASURESTART
    for (int i = 0; i < 100; i++)
        compute2(l, r, l, r);
    MEASURESTOP
*/
}


void REVERB5b::computecubic(float *inl, float *inr, float *outl, float *outr) {
    if (*_bypass)
        return;
    float fb = .6f + *_fb * .395;

    float mix = (float) *_mix;
    float mixsrc = 1.0f - mix;
    float gain = (float) pow(10, *_gain * .05);
    mixsrc *= gain;
    mix *= gain * (1 - sqrt(.5f * fb));

    float xt = _xt;
    float yt = _yt;

    /* update delay lines */


    if (_oldhpcut != *_hpcut) {
        _oldhpcut = *_hpcut;
        for (int i = 0; i < 8; i++)
            _filt[i].setNextHp(LOG2NORMALF(_oldhpcut));
    }

    if (_oldlpcut != *_lpcut) {
        _oldlpcut = *_lpcut;
        for (int i = 0; i < 8; i++)
            _filt[i].setNextLp(LOG2NORMALF(_oldlpcut));
    }

    if (_predelayprev != *_predelay) {
        _predelayprev = *_predelay;
        _predelayL.setDelay(_predelayprev * _sr * 0.001);
        _predelayR.setDelay(_predelayprev * _sr * 0.001);
    }


    for (int i = 0; i < _size; i++) {
        /* calculate "resultant junction pressure" and mix to input signals */
        float aoutL = 0.0;
        float aoutR = 0.0;
        float ainL = 0.0;
        for (int n = 0; n < 8; n++) {
            ainL += delayLines[n]->filterState;
        }
        float temp = ainL;
        yt = temp - xt + .995f * yt;
        ainL = yt * jpScale;
        xt = temp;
        float ainR = ainL + (float) _predelayL.tick(inr[i]);
        ainL = ainL + (float) _predelayR.tick(inl[i]);
        /* loop through all delay lines */

        for (uint32_t n = 0; n < 8; n++) {
            delayLine *lp = delayLines[n];
            /* send input signal and feedback to delay line */
            lp->buf[lp->writePos++] = (float) ((n & 1u ? ainR : ainL)
                                               - lp->filterState);
            lp->writePos &= lp->bufandmask;
            /* read from delay line with cubic interpolation */
            if (lp->readPosFrac >= DELAYPOS_SCALE) {
                lp->readPos += (lp->readPosFrac >> DELAYPOS_SHIFT);
                lp->readPosFrac &= DELAYPOS_MASK;
            }

            float frac = (float) lp->readPosFrac * (1.f / (float) DELAYPOS_SCALE);

            float a2 = frac * frac;
            a2 -= 1.0;
            a2 *= (1.0 / 6.0);
            float a1 = frac;
            a1 += 1.0;
            a1 *= 0.5;
            float am1 = a1 - 1.f;
            float a0 = 3.f * a2;
            a1 -= a0;
            am1 -= a2;
            a0 -= frac;


            lp->readPos &= lp->bufandmask;

            int32_t readPos = lp->readPos;
            float v0 = lp->buf[readPos];
            --readPos;
            readPos &= lp->bufandmask;
            float vm1 = lp->buf[readPos];
            readPos += 2;
            readPos &= lp->bufandmask;
            float v1 = lp->buf[readPos];
            ++readPos;
            readPos &= lp->bufandmask;
            float v2 = lp->buf[readPos];
            lp->filterState = _filt[n].ticklphp(
                    (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0);

            /* mix to output */
            if (n & 1)
                aoutR += lp->filterState;
            else
                aoutL += lp->filterState;

            lp->filterState *= fb;

            lp->readPosFrac += lp->readPosFrac_inc;

            /* start next random line segment if current one has reached endpoint */
            if (--(lp->randLine_cnt) <= 0)
                next_random_lineseg(lp, n);
        }
        outl[i] *= mixsrc;
        outl[i] += aoutL * mix;
        outr[i] *= mixsrc;
        outr[i] += aoutR * mix;
    }
    UNDENORMAL(yt);
    UNDENORMAL(xt);
    for (int i = 0; i < 8; i++) {
        _filt[i].undenormalize();
        UNDENORMAL(delayLines[i]->filterState);
    }
    _xt = xt;
    _yt = yt;
}


void REVERB5b::compute(float *inl, float *inr, float *outl, float *outr) {
    if (*_bypass)
        return;
    float fb = .6f + *_fb * .395;

    float mix = (float) *_mix;
    float mixsrc = 1.0f - mix;
    float gain = (float) pow(10, *_gain * .05);
    mixsrc *= gain;
    mix *= gain * (1 - sqrt(.5f * fb));

    float xt = _xt;
    float yt = _yt;

    /* update delay lines */


    if (_oldhpcut != *_hpcut) {
        _oldhpcut = *_hpcut;
        for (int i = 0; i < 8; i++)
            _filt[i].setNextHp(LOG2NORMALF(_oldhpcut));
    }

    if (_oldlpcut != *_lpcut) {
        _oldlpcut = *_lpcut;
        for (int i = 0; i < 8; i++)
            _filt[i].setNextLp(LOG2NORMALF(_oldlpcut));
    }

    if (_predelayprev != *_predelay) {
        _predelayprev = *_predelay;
        _predelayL.setDelay(_predelayprev * _sr * 0.001);
        _predelayR.setDelay(_predelayprev * _sr * 0.001);
    }


    for (int i = 0; i < _size; i++) {
        /* calculate "resultant junction pressure" and mix to input signals */
        float aoutL = 0.0;
        float aoutR = 0.0;
        float ainL = 0.0;
        for (int n = 0; n < 8; n++) {
            ainL += delayLines[n]->filterState;
        }
        float temp = ainL;
        yt = temp - xt + .995f * yt;
        ainL = yt * jpScale;
        xt = temp;
        float ainR = ainL + (float) _predelayL.tick(inr[i]);
        ainL = ainL + (float) _predelayR.tick(inl[i]);
        /* loop through all delay lines */

        for (int n = 0; n < 8; n++) {
            delayLine *lp = delayLines[n];
            /* send input signal and feedback to delay line */
            lp->buf[lp->writePos++] = (float) ((n & 1 ? ainR : ainL)
                                               - lp->filterState);
            lp->writePos &= lp->bufandmask;
            /* read from delay line with cubic interpolation */
            if (lp->readPosFrac >= DELAYPOS_SCALE) {
                lp->readPos += (lp->readPosFrac >> DELAYPOS_SHIFT);
                lp->readPosFrac &= DELAYPOS_MASK;
            }

            float frac = (float) lp->readPosFrac * (1.f / (float) DELAYPOS_SCALE);

            lp->readPos &= lp->bufandmask;

            lp->filterState = _filt[n].tick((lp->buf[lp->readPos] +
                                             frac *
                                             (lp->buf[((lp->readPos + 1) & lp->bufandmask)] -
                                              lp->buf[lp->readPos])));

            /* mix to output */
            if (n & 1)
                aoutR += lp->filterState;
            else
                aoutL += lp->filterState;

            lp->filterState *= fb;

            lp->readPosFrac += lp->readPosFrac_inc;

            /* start next random line segment if current one has reached endpoint */
            if (--(lp->randLine_cnt) <= 0)
                next_random_lineseg(lp, n);
        }
        outl[i] *= mixsrc;
        outl[i] += aoutL * mix;
        outr[i] *= mixsrc;
        outr[i] += aoutR * mix;
    }
    UNDENORMAL(yt);
    UNDENORMAL(xt);
    for (int i = 0; i < 8; i++) {
        _filt[i].undenormalize();
        UNDENORMAL(delayLines[i]->filterState);
    }
    _xt = xt;
    _yt = yt;
}

DaRev::DaRev(TRACK *t) : Effect(t) {
    //_t60 = &t->params[REV4T60];
    _gain = &t->params[REV4GAIN];
    _mix = &t->params[REV4MIX];
    reverbFdn.init(_sr);
};