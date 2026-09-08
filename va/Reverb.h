//
// Created by pr on 15.08.20.
//

#ifndef GRAINSTORM_REVERB_H
#define GRAINSTORM_REVERB_H


#include <cmath>
#include <cstring>
#include "Allpass.h"

class Reverb {
public:

    Reverb(void) {
    }

    void init(float fsamp) {
        int i, k1, k2;

        _fsamp = fsamp;
        _cntA1 = 1;
        _cntA2 = 0;
        _cntB1 = 1;
        _cntB2 = 0;
        _cntC1 = 1;
        _cntC2 = 0;

        _ipdel = 0.04f;
        _xover = 200.0f;
        _rtlow = 3.0f;
        _rtmid = 2.0f;
        _fdamp = 3e3f;
        _opmix = 0.5f;

        _g0 = 0;
        _g1 = 0;

        _vdelay0.init((int) (0.1f * _fsamp));
        _vdelay1.init((int) (0.1f * _fsamp));
        for (i = 0; i < 8; i++) {
            k1 = (int) (floorf(_tdiff1[i] * _fsamp + 0.5f));
            k2 = (int) (floorf(_tdelay[i] * _fsamp + 0.5f));
            _diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
            _delay[i].init(k2 - k1);
        }

        prepare();
    }


    void prepare() {
        if (_cntA1 != _cntA2) {
            int k = (int) (floorf((_ipdel - 0.020f) * _fsamp + 0.5f));
            _vdelay0.set_delay(k);
            _vdelay1.set_delay(k);
            _cntA2 = _cntA1;
        }

        if (_cntB1 != _cntB2) {
            float chi;
            float wlo = 6.2832f * _xover / _fsamp;
            if (_fdamp > 0.49f * _fsamp) chi = 2;
            else chi = 1 - cosf(6.2832f * _fdamp / _fsamp);
            for (int i = 0; i < 8; i++) {
                _filt1[i].set_params(_tdelay[i], _rtmid, _rtlow, wlo, 0.5f * _rtmid, chi);
            }
            _cntB2 = _cntB1;
        }

        if (_cntC1 != _cntC2) {
            {
                _g0 = (1 - _opmix) * (1 + _opmix);
                _g1 = 0.7f * _opmix * (2 - _opmix) / sqrtf(_rtmid);
            }
            _cntC2 = _cntC1;
        }

        // _pareq1.prepare (nfram);
        // _pareq2.prepare (nfram);
    }

    void process(float *inl, float *inr, float *outl, float *outr, int size) {
        float *p0, *p1;
        float *q0, *q1, *q2, *q3;

        float g = sqrtf(0.125f);

        for (int i = 0; i < size; i++) {
            _vdelay0.write(inl[i]);
            _vdelay1.write(inr[i]);

            float t = 0.3f * _vdelay0.read();
            float x0 = _diff1[0].process(_delay[0].read() + t);
            float x1 = _diff1[1].process(_delay[1].read() + t);
            float x2 = _diff1[2].process(_delay[2].read() - t);
            float x3 = _diff1[3].process(_delay[3].read() - t);
            t = 0.3f * _vdelay1.read();
            float x4 = _diff1[4].process(_delay[4].read() + t);
            float x5 = _diff1[5].process(_delay[5].read() + t);
            float x6 = _diff1[6].process(_delay[6].read() - t);
            float x7 = _diff1[7].process(_delay[7].read() - t);

            t = x0 - x1;
            x0 += x1;
            x1 = t;
            t = x2 - x3;
            x2 += x3;
            x3 = t;
            t = x4 - x5;
            x4 += x5;
            x5 = t;
            t = x6 - x7;
            x6 += x7;
            x7 = t;

            t = x0 - x2;
            x0 += x2;
            x2 = t;
            t = x1 - x3;
            x1 += x3;
            x3 = t;
            t = x4 - x6;
            x4 += x6;
            x6 = t;
            t = x5 - x7;
            x5 += x7;
            x7 = t;

            t = x0 - x4;
            x0 += x4;
            x4 = t;
            t = x1 - x5;
            x1 += x5;
            x5 = t;
            t = x2 - x6;
            x2 += x6;
            x6 = t;
            t = x3 - x7;
            x3 += x7;
            x7 = t;

            {
                outl[i] = _g1 * (x1 + x2) + _g0 * inl[i];
                outr[i] = _g1 * (x1 - x2) + _g0 * inr[i];
            }

            _delay[0].write(_filt1[0].process(g * x0));
            _delay[1].write(_filt1[1].process(g * x1));
            _delay[2].write(_filt1[2].process(g * x2));
            _delay[3].write(_filt1[3].process(g * x3));
            _delay[4].write(_filt1[4].process(g * x4));
            _delay[5].write(_filt1[5].process(g * x5));
            _delay[6].write(_filt1[6].process(g * x6));
            _delay[7].write(_filt1[7].process(g * x7));
        }
/*
    n = _ambis ? 4 : 2;
    _pareq1.process (nfram, n, out);
    _pareq2.process (nfram, n, out);
    if (!_ambis)
    {
        for (i = 0; i < nfram; i++)
        {
            _g0 += _d0;
            q0 [i] += _g0 * p0 [i];
            q1 [i] += _g0 * p1 [i];
        }
    }
    */
    }


    void set_delay(float v) {
        _ipdel = v;
        _cntA1++;
    }

    void set_xover(float v) {
        _xover = v;
        _cntB1++;
    }

    void set_rtlow(float v) {
        _rtlow = v;
        _cntB1++;
    }

    void set_rtmid(float v) {
        _rtmid = v;
        _cntB1++;
        _cntC1++;
    }

    void set_fdamp(float v) {
        _fdamp = v;
        _cntB1++;
    }

    void set_opmix(float v) {
        _opmix = v;
        _cntC1++;
    }

    void reset() {
        _g0 = 0;
        _g1 = 0;
        _vdelay0.reset();
        _vdelay1.reset();
        for (int i = 0; i < 8; i++) {
            _diff1[i].reset();
            _delay[i].reset();
            _filt1[i].reset();
        }
    }

private:
    float _fsamp;
    Vdelay<float> _vdelay0;
    Vdelay<float> _vdelay1;
    Ap<float> _diff1[8];
    Filt1<float> _filt1[8];
    Delay<float> _delay[8];

    int _cntA1;
    int _cntB1;
    int _cntC1;
    int _cntA2;
    int _cntB2;
    int _cntC2;

    float _ipdel;
    float _xover;
    float _rtlow;
    float _rtmid;
    float _fdamp;
    float _opmix;

    float _g0;
    float _g1;

    float _tdiff1[8]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
            22904e-6f,
            29291e-6f,
            13458e-6f,
            19123e-6f
    };
    float _tdelay[8]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
            174713e-6f,
            192303e-6f,
            125000e-6f,
            219991e-6f
    };
};

class ReverbFDN {
public:

    ReverbFDN(void) {
    }

    void init(float fsamp) {
        _sr = fsamp;
        float sum = 0;
        _scale = _sr / 48000.f;
        for (int i = 0; i < 8; i++) {
            int k1 = (int) (floorf(_tdiff1[i] * _scale * _sr + 0.5f));
            int k2 = (int) (floorf(_tdelay[i] * _scale * _sr + 0.5f));
            //_diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
            _diff1[i].init(_sr, k1 / _sr, 64 * _scale / _sr);
            _diff1[i].setDiff((i & 1) ? -0.6f : 0.6f);
            _delay[i].init(k2 - k1);
            sum += k2;
        }
        _averagedelay = sum / 8.f;
    }

    void process(float *inl, float *inr, float *outl, float *outr, int size) {
        for (int i = 0; i < size; i++) {
            //_vdelay0.write(inl[i]);
            //_vdelay1.write(inr[i]);

            float t = inl[i];//_vdelay0.read();
            float x0 = _diff1[0].tickok(_delay[0].read() + t);
            float x1 = _diff1[1].tickok(_delay[1].read() + t);
            float x2 = _diff1[2].tickok(_delay[2].read() - t);
            float x3 = _diff1[3].tickok(_delay[3].read() - t);
            t = inr[i];//_vdelay1.read();
            float x4 = _diff1[4].tickok(_delay[4].read() + t);
            float x5 = _diff1[5].tickok(_delay[5].read() + t);
            float x6 = _diff1[6].tickok(_delay[6].read() - t);
            float x7 = _diff1[7].tickok(_delay[7].read() - t);

            t = x0 - x1;
            x0 += x1;
            x1 = t;
            t = x2 - x3;
            x2 += x3;
            x3 = t;
            t = x4 - x5;
            x4 += x5;
            x5 = t;
            t = x6 - x7;
            x6 += x7;
            x7 = t;

            t = x0 - x2;
            x0 += x2;
            x2 = t;
            t = x1 - x3;
            x1 += x3;
            x3 = t;
            t = x4 - x6;
            x4 += x6;
            x6 = t;
            t = x5 - x7;
            x5 += x7;
            x7 = t;

            t = x0 - x4;
            x0 += x4;
            x4 = t;
            t = x1 - x5;
            x1 += x5;
            x5 = t;
            t = x2 - x6;
            x2 += x6;
            x6 = t;
            t = x3 - x7;
            x3 += x7;
            x7 = t;

            {
                outl[i] = _mix * (x1 + x2) + _mixsrc * inl[i];
                outr[i] = _mix * (x1 - x2) + _mixsrc * inr[i];
            }

            _filtstate[0] = (x0 * (1 - _c) + _c * _filtstate[0]);
            _filtstate[1] = (x1 * (1 - _c) + _c * _filtstate[1]);
            _filtstate[2] = (x2 * (1 - _c) + _c * _filtstate[2]);
            _filtstate[3] = (x3 * (1 - _c) + _c * _filtstate[3]);
            _filtstate[4] = (x4 * (1 - _c) + _c * _filtstate[4]);
            _filtstate[5] = (x5 * (1 - _c) + _c * _filtstate[5]);
            _filtstate[6] = (x6 * (1 - _c) + _c * _filtstate[6]);
            _filtstate[7] = (x7 * (1 - _c) + _c * _filtstate[7]);


            _delay[0].write(_feedback[0] * _filtstate[0]);
            _delay[1].write(_feedback[1] * _filtstate[1]);
            _delay[2].write(_feedback[2] * _filtstate[2]);
            _delay[3].write(_feedback[3] * _filtstate[3]);
            _delay[4].write(_feedback[4] * _filtstate[4]);
            _delay[5].write(_feedback[5] * _filtstate[5]);
            _delay[6].write(_feedback[6] * _filtstate[6]);
            _delay[7].write(_feedback[7] * _filtstate[7]);
        }
/*
    n = _ambis ? 4 : 2;
    _pareq1.process (nfram, n, out);
    _pareq2.process (nfram, n, out);
    if (!_ambis)
    {
        for (i = 0; i < nfram; i++)
        {
            _g0 += _d0;
            q0 [i] += _g0 * p0 [i];
            q1 [i] += _g0 * p1 [i];
        }
    }
    */
    }

    void reset() {
        for (int i = 0; i < 4; i++) {
            _diff1[i].reset();
            _delay[i].reset();
            _filtstate[i] = 0;
        }
    }

    void setDamp(float damp) {
        _c = damp;
    }

    void setT60(float T60) {
        if (T60 == 0) {
            for (int i = 0; i < 8; i++) {
                _feedback[i] = 0;
            }
            _gainfact = 1.f;
        } else {
            for (int i = 0; i < 8; i++) {
                _feedback[i] =
                        powf(10.0, (-3.0f * (int) (floorf(_tdelay[i] * _scale * _sr + 0.5f)) /
                                    (T60 * _sr))) * .25f;
            }
            _gainfact = 1 / (powf(0.001f, (_averagedelay) / (T60 * _sr)) * 3.f);
        }
        if (_gainfact > 1)
            _gainfact = 1;
        _rt = T60;
        set_mix(_opmix);
    }


    void set_mix(float mix) {
        _opmix = mix;
        _mixsrc = (1 - mix) * (1 + mix);
        _mix = 0.7f * mix * (2 - mix) / sqrtf(_rt);
    }

    void setGain(float g) {
        _gain = g;
    };


private:
    float _sr, _scale, _rt{1}, _opmix{1};
    Ap1<float> _diff1[8];
    Delay<float> _delay[8];
    float _filtstate[8]{};
    float _feedback[8]{};
    float _c{}, _gain{1};
    float _mix{1}, _mixsrc{0};
    float _averagedelay, _gainfact{.5};
    float _tdiff1[8]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
            22904e-6f,
            29291e-6f,
            13458e-6f,
            19123e-6f
    };
    float _tdelay[8]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
            174713e-6f,
            192303e-6f,
            125000e-6f,
            219991e-6f
    };
};

#define GRAINSIZEDAREV 8192

class DaRev : public Effect {
public:
    DaRev(TRACK *t);

    void compute(float *inl, float *inr, float *outl, float *outr) {
        check();
        reverbFdn.process(inl, inr, inl, inr, _size);
    }

private:
    void check() {
        if (_olddamp != *_damp) {
            _olddamp = *_damp;
            reverbFdn.setDamp(.5f + _olddamp * .45f);
        }
        if (_oldt60 != *_t60) {
            _oldt60 = *_t60;
            reverbFdn.setT60(LOG2NORMALF(_oldt60));
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverbFdn.set_mix(_oldmix);
        }
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            reverbFdn.setGain(LOG2NORMALF(_oldgain));
        }
    }

    std::atomic<float> *_damp, *_t60;
    float _olddamp{-100000000}, _oldt60{-1000000000}, _oldmix{-1}, _oldgain{-1000};
    ReverbFDN reverbFdn;
    float _window[GRAINSIZEDAREV];

};

#define NUMCOMBS 4
#define NUMALLPASS 2

class SimpleReverb : public Effect {
public:
    SimpleReverb(TRACK *track, int channel);

    void compute(float *in, int size);

    void check() {
        float t60 = *_t60;
        if (_prvt60 != t60) {
            _prvt60 = t60;
            setT60(_prvt60 * 20);
        }
    }

private:
    float _prvt60{}, _prdmp{};
    std::atomic<float> *_t60, *_damp;
    Ap<float> _diff1[4];
    Delay<float> _delay[4];
    float _filtstate[4]{};
    float _feedback[4]{};
    float _c{};
    float _averagedelay, _gainfact{.5};
    float _tdiff1[4]{
            20346e-6f,
            24421e-6f,
            31604e-6f,
            27333e-6f,
    };
    float _tdelay[4]{
            153129e-6f,
            210389e-6f,
            127837e-6f,
            256891e-6f,
    };

    void setDamp(float damp) {
        _c = damp * .75f;
    }

    void setT60(float T60) {
        if (T60 == 0) {
            for (int i = 0; i < 4; i++) {
                _feedback[i] = 0;
            }
            _gainfact = 1.f;
        } else {
            for (int i = 0; i < 4; i++) {
                _feedback[i] = powf(10.0, (-3.0f * (int) (floorf(_tdelay[i] * _sr + 0.5f)) /
                                           (T60 * _sr))) * .5f;
            }
            _gainfact = 1 / (powf(0.001f, (_averagedelay) / (T60 * _sr)) * 2.f);
        }
        if (_gainfact > 1)
            _gainfact = 1;
    }

    void init() {
        float sum = 0;
        for (int i = 0; i < 4; i++) {
            int k1 = (int) ((floorf(_tdiff1[i] * _sr + 0.5f)) * (_chan == 1 ? 1. : 1.01f));
            int k2 = (int) ((floorf(_tdelay[i] * _sr + 0.5f)) * (_chan == 1 ? 1. : 1.01f));
            _diff1[i].init(k1, (i & 1) ? -0.6f : 0.6f);
            _delay[i].init(k2 - k1);
            sum += k2;
        }
        _averagedelay = sum / 4.f;
    }
};


class SimpleReverb2 : public Effect {
public:
    SimpleReverb2(TRACK *track, int channel);

    int compsize() {
        int32_t s = 0;
        for (int i = 0; i < NUMCOMBS + NUMALLPASS; i++)
            s += MYFLT2LRND(_lpt[i] * _sr);
        return s;
    }

    void compute(float *in, int size);

    void compute(float *inl, float *inr, float *outl, float *outr) {
        float mix = (float) *_mix;
        float mixsrc = 1.0f - mix;
        if (*_rvt != _prvt) {
            _prvt = *_rvt;
            reset();
        }
        for (int i = 0; i < _size; i++) {
            float inn = inl[i];
            float tmp = *_xp[0];
            *_xp[0] *= _coef[0];
            *_xp[0] += inn;
            float out0 = tmp;
            if (++_xp[0] >= _end[0])
                _xp[0] = _start[0];

            tmp = *_xp[1];
            *_xp[1] *= _coef[1];
            *_xp[1] += inn;
            float out1 = tmp;
            if (++_xp[1] >= _end[1])
                _xp[1] = _start[1];

            tmp = *_xp[2];
            *_xp[2] *= _coef[2];
            *_xp[2] += inn;
            float out2 = tmp;
            if (++_xp[2] >= _end[2])
                _xp[2] = _start[2];

            tmp = *_xp[3];
            *_xp[3] *= _coef[3];
            *_xp[3] += inn;
            float out3 = tmp;
            if (++_xp[3] >= _end[3])
                _xp[3] = _start[3];

            float sum = out0 + out1 + out2 + out3;

            float y = *_xp[4], z;
            *_xp[4] = z = _coef[4] * y + sum;
            float out4 = y - _coef[4] * z;
            if (++_xp[4] >= _end[4])
                _xp[4] = _start[4];

            y = *_xp[5];
            *_xp[5] = z = _coef[5] * y + out4;
            float res = y - _coef[5] * z;
            if (++_xp[5] >= _end[5])
                _xp[5] = _start[5];

            outl[i] = outr[i] = inn * mixsrc + res * mix;
        }
    }

    void reset() {
        for (int i = 0; i < NUMCOMBS; i++) {
            double exp_arg = (double) (log001 * _lpt[i] / _prvt);
            if (exp_arg < -36.8413615)    /* ln(1.0e-16) */
                _coef[i] = 0.0;
            else
                _coef[i] = (float) exp(exp_arg);
        }
    }

private:
    const float _lpt[
            NUMCOMBS + NUMALLPASS] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.005f, 0.02291f};
    double _prvt;
    std::atomic<float> *_rvt;
    float _filtstate{};
    float _coef[NUMCOMBS + NUMALLPASS];
    float *_xp[NUMCOMBS + NUMALLPASS];
    float *_start[NUMCOMBS + NUMALLPASS];
    float *_end[NUMCOMBS + NUMALLPASS];
    std::vector<float> buf;
};


struct delayLine {
    int32_t writePos;
    int32_t bufferSize;
    int32_t bufandmask;
    int32_t readPos;
    int32_t readPosFrac;
    int32_t readPosFrac_inc;
    int32_t dummy;
    int32_t seedVal;
    int32_t randLine_cnt;
    float filterState;
    float buf[1];
};

template<typename T>
class allpass {
public:

    void setbuffer(T *buf, int size) {
        buffer = buf;
        bufsize = size;
    }

    inline T tick(T x) {
        T z = buffer[bufidx];
        x += feedback * z;
        buffer[bufidx] = x;
        if (++bufidx == bufsize) bufidx = 0;
        return z - feedback * x;
    }

    inline T tickfreeverb(T x) {
        T z = buffer[bufidx];

        buffer[bufidx] = x + (z * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return -x + z;
    }

    void mute() {
        for (int i = 0; i < bufsize; i++)
            buffer[i] = 0;
    }

    void setfeedback(T val) {
        feedback = val;
    }

    T getfeedback() {
        return feedback;
    }

private:
    T feedback{};
    T *buffer{};
    int bufsize{};
    int bufidx{};
};

template<typename T>
class comb {
public:
    comb() {
        filterstore = 0;
        bufidx = 0;
    }

    void setbuffer(T *buf, int size) {
        buffer = buf;
        bufsize = size;
    }

    inline T ticklp(T input) {
        T output = buffer[bufidx];

        filterstore = output * damp2 + filterstore * damp1;

        buffer[bufidx] = input + (filterstore * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return output;
    }

    inline T tick(T input) {
        T output = buffer[bufidx];

        buffer[bufidx] = input + (output * feedback);

        if (++bufidx >= bufsize) bufidx = 0;

        return output;
    }

    inline T tickdelay(T input) {
        T output = buffer[bufidx];

        buffer[bufidx++] = input;

        if (bufidx >= bufsize) bufidx = 0;

        return output;
    }


    void mute() {
        for (int i = 0; i < bufsize; i++)
            buffer[i] = 0;
    }

    void setdamp(T val) {
        damp1 = val;
        damp2 = 1 - val;
    }

    T getdamp() {
        return damp1;
    }

    void setfeedback(T val) {
        feedback = val;
    }

    T getfeedback() {
        return feedback;
    }

    void undenormalize() {
        UNDENORMAL(filterstore);
    }

private:
    T feedback;
    T filterstore;
    T damp1;
    T damp2;
    T *buffer;
    int bufsize;
    int bufidx;
};


const int numcombs = 8;
const int numallpasses = 4;
const float muted = 0;
const float fixedgain = 0.015f;
const float scalewet = 3;
const float scaledry = 2;
const float scaledamp = 0.4f;
const float scaleroom = 0.28f;
const float offsetroom = 0.7f;
const float initialroom = 0.5f;
const float initialdamp = 0.5f;
const float initialwet = 1 / scalewet;
const float initialdry = 0;
const float initialwidth = 1;
const int stereospread = 23;


// These values assume 44.1KHz sample rate
// they will probably be OK for 48KHz sample rate
// but would need scaling for 96KHz (or other) sample rates.
// The values were obtained by listening tests.
const float combtuningL1 = (float) (1116);
const float combtuningR1 = (float) ((1116 + stereospread));
const float combtuningL2 = (float) (1188);
const float combtuningR2 = (float) ((1188 + stereospread));
const float combtuningL3 = (float) (1277);
const float combtuningR3 = (float) ((1277 + stereospread));
const float combtuningL4 = (float) (1356);
const float combtuningR4 = (float) ((1356 + stereospread));
const float combtuningL5 = (float) (1422);
const float combtuningR5 = (float) ((1422 + stereospread));
const float combtuningL6 = (float) (1491);
const float combtuningR6 = (float) ((1491 + stereospread));
const float combtuningL7 = (float) (1557);
const float combtuningR7 = (float) ((1557 + stereospread));
const float combtuningL8 = (float) (1617);
const float combtuningR8 = (float) ((1617 + stereospread));
const float allpasstuningL1 = (float) (556);
const float allpasstuningR1 = (float) ((556 + stereospread));
const float allpasstuningL2 = (float) (441);
const float allpasstuningR2 = (float) ((441 + stereospread));
const float allpasstuningL3 = (float) (341);
const float allpasstuningR3 = (float) ((341 + stereospread));
const float allpasstuningL4 = (float) (225);
const float allpasstuningR4 = (float) ((225 + stereospread));


template<typename T>
class revmodel {
public:
    revmodel(T sr) {
        _sr = sr;
        double scaler = sr / 44100.;
        int total = 0;
        for (int i = 0; i < 24; i++) {
            int delay = (int) floor(scaler * tunings[i]);
            //if ((delay & 1) == 0) delay++;
            //while (!ispprime(delay)) delay += 2;
            tunings[i] = delay;
            total += delay;
        }
        mem.resize(total);
        int offset = 0;
        for (int i = 0; i < 8; i++) {
            combL[i].setbuffer(mem.data() + offset, (int) tunings[i]);
            offset += (int) tunings[i];
        }
        for (int i = 0; i < 8; i++) {
            combR[i].setbuffer(mem.data() + offset, (int) tunings[i + 8]);
            offset += (int) tunings[i + 8];
        }
        for (int i = 0; i < 4; i++) {
            allpassL[i].setbuffer(mem.data() + offset, (int) tunings[i + 16]);
            offset += (int) tunings[i + 16];
        }
        for (int i = 0; i < 4; i++) {
            allpassR[i].setbuffer(mem.data() + offset, (int) tunings[i + 20]);
            offset += (int) tunings[i + 20];
        }

        // Set default values
        allpassL[0].setfeedback(0.5f);
        allpassR[0].setfeedback(0.5f);
        allpassL[1].setfeedback(0.5f);
        allpassR[1].setfeedback(0.5f);
        allpassL[2].setfeedback(0.5f);
        allpassR[2].setfeedback(0.5f);
        allpassL[3].setfeedback(0.5f);
        allpassR[3].setfeedback(0.5f);
        setwet(initialwet);
        setdry(initialdry);
        setdamp(initialdamp);
        setwidth(initialwidth);

        // Buffer will be full of rubbish - so we MUST mute them
        mute();
    }

    void mute() {

        for (int i = 0; i < numcombs; i++) {
            combL[i].mute();
            combR[i].mute();
        }
        for (int i = 0; i < numallpasses; i++) {
            allpassL[i].mute();
            allpassR[i].mute();
        }
    }

    void
    processreplace(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples) {
        while (numsamples-- > 0) {
            T outL = 0, outR = 0;
            T input = (*inputL + *inputR);

            // Accumulate comb filters in parallel
            for (int i = 0; i < numcombs; i++) {
                outL += combL[i].ticklp(input);
                outR += combR[i].ticklp(input);
            }

            // Feed through allpasses in series
            for (int i = 0; i < numallpasses; i++) {
                outL = allpassL[i].tick(outL);
                outR = allpassR[i].tick(outR);
            }

            // Calculate output REPLACING anything already there
            //*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
            //*outputR = outR*wet1 + outL*wet2 + *inputR*dry;
            *outputL = outL * wet + *inputL * dry;
            *outputR = outR * wet + *inputR * dry;
            // Increment sample pointers, allowing for interleave (if any)
            inputL++;
            inputR++;
            outputL++;
            outputR++;
        }

        for (int i = 0; i < numcombs; i++) {
            combL[i].undenormalize();
            combR[i].undenormalize();
        }
    }

    void setdamp(T value) {
        for (int i = 0; i < numcombs; i++) {
            combL[i].setdamp(value * scaledamp);
            combR[i].setdamp(value * scaledamp);
        }
    }

    void setwet(T value) {
        wet = value * .125 * .5;
        // wet1 = wet * (width / 2 + 0.5);
        // wet2 = wet * ((1 - width) / 2);

    }

    void setdry(T value) {
        dry = value;
    }

    void setwidth(T value) {
        width = value;
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int i = 0; i < numcombs; i++) {
                combL[i].setfeedback(0);
                combR[i].setfeedback(0);
            }
        } else
            for (int i = 0; i < numcombs; i++) {
                combL[i].setfeedback(pow(10.0, (-3.0 * tunings[i] / (T60 * _sr))));
                combR[i].setfeedback(pow(10.0, (-3.0 * tunings[i + 8] / (T60 * _sr))));
            }
    }

private:
    T _sr;
    T tunings[24] = {combtuningL1, combtuningL2, combtuningL3, combtuningL4, combtuningL5,
                     combtuningL6, combtuningL7, combtuningL8, combtuningR1, combtuningR2,
                     combtuningR3, combtuningR4, combtuningR5, combtuningR6, combtuningR7,
                     combtuningR8, allpasstuningL1, allpasstuningL2, allpasstuningL3,
                     allpasstuningL4, allpasstuningR1, allpasstuningR2, allpasstuningR3,
                     allpasstuningR4};
    T wet, wet1, wet2;
    T dry;
    T width;
    // The following are all declared inline
    // to remove the need for dynamic allocation
    // with its subsequent error-checking messiness

    // Comb filters
    comb<T> combL[8];
    comb<T> combR[8];

    // Allpass filters
    allpass<T> allpassL[4];
    allpass<T> allpassR[4];
    std::vector<T> mem;
};


#include "effects.h"


class Freeverb : public Effect {
public:
    Freeverb(TRACK *t) : Effect(t), reverb(_sr) {
        _mix = &t->params[REV1MIX];
        _gain = &t->params[REV1GAIN];
        _room = &t->params[REV1T60];
        _damp = &t->params[REV1DAMP];
        _bypass = &t->bypass[SPACE_REVERB1];
    }

    void compute(float *inl, float *inr, float *outl, float *outr) {
        if (*_bypass)
            return;
        update();
        reverb.processreplace(inl, inr, outl, outr, _size);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = LOG2NORMALF(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }
        if (_olddamp != *_damp) {
            _olddamp = *_damp;
            reverb.setdamp(_olddamp);
        }
    }

private:
    std::atomic<float> *_mix, *_gain, *_room, *_damp;
    float _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    revmodel<float> reverb;
};


template<typename T>
class nrev {
public:
    nrev(T sr) {
        _sr = sr;
        T scaler = sr / 25641.0;

        int delaysum = 0;
        for (int i = 0; i < 15; i++) {
            int delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);
            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int sumtmp = 0;
        for (int i = 0; i < 6; i++) {
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            sumtmp += _lengths[i];
        }

        for (int i = 0; i < 8; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 6]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i + 6];
        }
    }

    void process(T *inl, T *inr, T *outl, T *outr, int size) {
        for (int i = 0; i < size; i++) {
            T in = inl[i] + inr[i];
            T temp0 = 0.0;
            for (int n = 0; n < 6; n++) {
                temp0 += _combs[n].tick(in);
            }

            temp0 /= 12.;

            for (int n = 0; n < 3; n++) {
                temp0 = _aps[n].tick(temp0);
            }

            // One-pole lowpass filter.
            _lowpassState = 0.9 * _lowpassState + 0.1 * temp0;

            temp0 = _aps[3].tick(_lowpassState);

            outl[i] = _wet * _aps[4].tick(temp0) + _dry * inl[i];
            outr[i] = _wet * _aps[5].tick(temp0) + _dry * inr[i];
        }
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int i = 0; i < 6; i++)
                _combs[i].setfeedback(0);
        } else
            for (int i = 0; i < 6; i++)
                _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i] / (T60 * _sr))));
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }

private:
    int _lengths[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    T _sr;
    allpass<T> _aps[8];
    comb<T> _combs[6];
    T _lowpassState{};
    std::vector<T> _buf;
    T _wet{.5}, _dry{.5};
};


#define NREVB_NUM_COMB    6
#define NREVB_NUM_COMB_2    12
#define NREVB_NUM_ALLPASS 5
#define NREVB_NUM_ALLPASS_2 3
#define NREVB_SCALE_WET    0.4
#define NREV_STEREO_SPREAD 13
#define NREV_DEFAULT_FS 25641.
#define NREVB_DEFAULT_FEEDBACK .7
#define NREVOS 1

template<typename T>
class nrev2 {
public:
    nrev2(T sr) {
        _sr = sr / (T) NREVOS;
        T scaler = (_sr / NREV_DEFAULT_FS);
        const int stereospread = scaler * NREV_STEREO_SPREAD;

        int delaysum = 0;
        for (int i = 0; i < NREVB_NUM_COMB + NREVB_NUM_ALLPASS; i++) {
            int delay = (int) floor(scaler * _lengthsL[i]);
            delay = findNextPrime(delay);
            _lengthsL[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthsR[i] = delay;
            delaysum += delay;
        }

        for (int i = 0; i < NREVB_NUM_COMB_2; i++) {
            int delay = (int) floor(scaler * _lengthscomb2L[i]);
            delay = findNextPrime(delay);
            _lengthscomb2L[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthscomb2R[i] = delay;
            delaysum += delay;
        }

        for (int i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            int delay = (int) floor(scaler * _lengthsaps2L[i]);
            delay = findNextPrime(delay);
            _lengthsaps2L[i] = delay;
            delaysum += delay;

            delay += stereospread;
            _lengthsaps2R[i] = delay;
            delaysum += delay;
        }


        _buf.resize(delaysum, 0);
        int sumtmp = 0;

        for (int i = 0; i < NREVB_NUM_COMB; i++) {
            _combsL[i].setbuffer(_buf.data() + sumtmp, _lengthsL[i]);
            sumtmp += _lengthsL[i];
        }

        for (int i = 0; i < NREVB_NUM_COMB_2; i++) {
            _combs2L[i].setbuffer(_buf.data() + sumtmp, _lengthscomb2L[i]);
            sumtmp += _lengthscomb2L[i];
        }

        for (int i = 0; i < NREVB_NUM_ALLPASS; i++) {
            _apsL[i].setbuffer(_buf.data() + sumtmp, _lengthsL[i + 6]);
            _apsL[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsL[i + 6];
        }

        for (int i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            _aps2L[i].setbuffer(_buf.data() + sumtmp, _lengthsaps2L[i]);
            _aps2L[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsaps2L[i];
        }

        for (int i = 0; i < NREVB_NUM_COMB; i++) {
            _combsR[i].setbuffer(_buf.data() + sumtmp, _lengthsR[i]);
            sumtmp += _lengthsR[i];
        }

        for (int i = 0; i < NREVB_NUM_COMB_2; i++) {
            _combs2R[i].setbuffer(_buf.data() + sumtmp, _lengthscomb2R[i]);
            sumtmp += _lengthscomb2R[i];
        }

        for (int i = 0; i < NREVB_NUM_ALLPASS; i++) {
            _apsR[i].setbuffer(_buf.data() + sumtmp, _lengthsR[i + 6]);
            _apsR[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsR[i + 6];
        }

        for (int i = 0; i < NREVB_NUM_ALLPASS_2; i++) {
            _aps2R[i].setbuffer(_buf.data() + sumtmp, _lengthsaps2R[i]);
            _aps2R[i].setfeedback(NREVB_DEFAULT_FEEDBACK);
            sumtmp += _lengthsaps2R[i];
        }
        if (sumtmp != delaysum)
            LOGE("111111 -------------------------------------   FATAAAAAAALLLLLLLLLLLLLLLLLLLLLL");

    }

    /*
    T z = _line[_writepos];
    x += _c * z;
    _line[_writepos] = x;
    if (++_writepos >= _maxdelaysmpls) _writepos = 0;
    return z - _c * x;
     */
    void process(T *inl, T *inr, T *outl, T *outr, int size) {
        T outL, outR, tmpL, tmpR, apfeedback = .5;

        for (int n = 0; n < size; n += NREVOS) {
            outL = outR = tmpL = tmpR =/* _lowpassStatein = .6 * _lowpassStatein + .4 * */(inl[n] +
                                                                                           inr[n]);
            outL += .5 * lastL;
            lastL -= .5 * outL;
            for (int i = 0; i < NREVB_NUM_COMB; i++) outL += _combsL[i].tick(tmpL);
            for (int i = 0; i < NREVB_NUM_COMB_2; i++) outL += _combs2L[i].tick(tmpL);
            for (int i = 0; i < 3; i++) outL = _apsL[i].tick(outL);
            for (int i = 0; i < NREVB_NUM_ALLPASS_2; i++) outL = _aps2L[i].tick(outL);
            _lowpassStateL = .7 * _lowpassStateL + .3 * outL;
            outL = _apsL[3].tick(_lowpassStateL);
            outL = _dcL.process(_apsL[4].tick(outL));
            outl[n] *= _dry;
            outl[n] += _wet * lastL;
            //outl[n+1] *= _dry;
            //outl[n+1] += _wet * lastL;

            outR += apfeedback * lastR;
            lastR -= apfeedback * outR;
            for (int i = 0; i < NREVB_NUM_COMB; i++) outR += _combsR[i].tick(tmpR);
            for (int i = 0; i < NREVB_NUM_COMB_2; i++) outR += _combs2R[i].tick(tmpR);
            for (int i = 0; i < 3; i++) outR = _apsR[i].tick(outR);
            for (int i = 0; i < NREVB_NUM_ALLPASS_2; i++) outR = _aps2R[i].tick(outR);
            _lowpassStateR = .7 * _lowpassStateR + .3 * outR;
            outR = _apsR[3].tick(_lowpassStateR);
            outR = _dcR.process(_apsR[4].tick(outR));
            outr[n] *= _dry;
            outr[n] += _wet * lastR;
            //outr[n+1] *= _dry;
            //outr[n+1] += _wet * lastR;
            lastL = outL;
            lastR = outR;
        }
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int i = 0; i < NREVB_NUM_COMB_2; i++) {
                _combs2L[i].setfeedback(0);
                _combs2R[i].setfeedback(0);
            }
            for (int i = 0; i < NREVB_NUM_COMB; i++) {
                _combsL[i].setfeedback(0);
                _combsR[i].setfeedback(0);
            }
        } else {
            for (int i = 0; i < NREVB_NUM_COMB_2; i++) {
                _combs2L[i].setfeedback(pow(10.0, (-3.0 * _lengthscomb2L[i] / (T60 * _sr))));
                _combs2R[i].setfeedback(pow(10.0, (-3.0 * _lengthscomb2R[i] / (T60 * _sr))));
            }
            for (int i = 0; i < NREVB_NUM_COMB; i++) {
                _combsL[i].setfeedback(pow(10.0, (-3.0 * _lengthsL[i] / (T60 * _sr))));
                _combsR[i].setfeedback(pow(10.0, (-3.0 * _lengthsR[i] / (T60 * _sr))));
            }
        }
    }


    void setwet(T value) {
        _wet = value * (1. / (NREVB_NUM_COMB + NREVB_NUM_COMB_2));
    }

    void setdry(T value) {
        _dry = value;
    }

private:
    int _lengthsL[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    int _lengthsR[15]{1433, 1601, 1867, 2053, 2251, 2399, 347, 113, 37, 59, 53, 43, 37, 29, 19};
    int _lengthscomb2L[NREVB_NUM_COMB_2]{1257, 1333, 1499, 1501, 1535, 1637, 1738, 1904, 1999, 2020,
                                         2120, 2509};
    int _lengthscomb2R[NREVB_NUM_COMB_2]{1257, 1333, 1499, 1501, 1535, 1637, 1738, 1904, 1999, 2020,
                                         2120, 2509};
    int _lengthsaps2L[NREVB_NUM_ALLPASS_2]{57, 87, 103,};
    int _lengthsaps2R[NREVB_NUM_ALLPASS_2]{57, 87, 103,};
    T _sr;

    allpass<T> _apsL[NREVB_NUM_ALLPASS], _aps2L[NREVB_NUM_ALLPASS_2];
    allpass<T> _apsR[NREVB_NUM_ALLPASS], _aps2R[NREVB_NUM_ALLPASS_2];
    comb<T> _combsL[NREVB_NUM_COMB], _combs2L[NREVB_NUM_COMB_2];
    comb<T> _combsR[NREVB_NUM_COMB], _combs2R[NREVB_NUM_COMB_2];
    DCBlocker<T> _dcL, _dcR;
    T lastL{}, lastR{}, _lowpassStateL{}, _lowpassStateR{}, _lowpassStatein{};
    std::vector<T> _buf;
    T _wet{.5}, _dry{.5};
};

class NRev : public Effect {
public:
    NRev(TRACK *t) : Effect(t), reverb(_sr) {
        _mix = &t->params[REV2MIX];
        _gain = &t->params[REV2GAIN];
        _room = &t->params[REV2T60];
        _bypass = &t->bypass[SPACE_REVERB2];
        //_damp = &t->params[REV1DAMP];
    }

    void compute(float *inl, float *inr, float *outl, float *outr) {
        if (*_bypass)
            return;
        update();
        reverb.process(inl, inr, outl, outr, _size);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = LOG2NORMALF(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(LOG2NORMALF(_oldroom));
        }
    }

private:
    std::atomic<float> *_mix, *_gain, *_room;
    float _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    nrev<float> reverb;
};

template<typename T>
class jcrev {
public:
    jcrev(T sr) {
        _sr = sr;
        double scaler = sr / 44100.0;
        int delaysum = 0;

        for (int i = 0; i < 9; i++) {
            int delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);
            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int sumtmp = 0;


        for (int i = 0; i < 3; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 4]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i + 4];
        }

        for (int i = 0; i < 4; i++) {
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            sumtmp += _lengths[i];
            _combs[i].setdamp(0.2);
        }

        _leftdel.setbuffer(_buf.data() + sumtmp, _lengths[7]);
        sumtmp += _lengths[7];
        _rightdel.setbuffer(_buf.data() + sumtmp, _lengths[8]);
        sumtmp += _lengths[8];
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }


    void setT60(T T60) {
        if (T60 == 0) {
            for (int i = 0; i < 4; i++)
                _combs[i].setfeedback(0);
        }
        for (int i = 0; i < 4; i++)
            _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i] / (T60 * _sr))));
    }

    void process(T *inl, T *inr, T *outl, T *outr, int size) {
        for (int i = 0; i < size; i++) {
            T in = (inl[i] + inr[i]);
            for (int n = 0; n < 3; n++)
                in = _aps[n].tick(in);

            T sum = 0.0;

            for (int n = 0; n < 4; n++)
                sum += _combs[n].ticklp(in);

            sum *= .125;

            outl[i] = _wet * _leftdel.tickdelay(sum) + _dry * inl[i];
            outr[i] = _wet * _rightdel.tickdelay(sum) + _dry * inr[i];
        }

    }

private:
    int _lengths[9]{1116, 1356, 1422, 1617, 225, 341, 441, 211, 179};
    T _sr;
    allpass<T> _aps[3];
    comb<T> _combs[4];
    // OnePole2 _lps[4];
    comb<T> _leftdel;
    comb<T> _rightdel;
    T _wet, _dry;
    std::vector<T> _buf;
};

class JCRev : public Effect {
public:
    JCRev(TRACK *t) : Effect(t), reverb(_sr) {
        _mix = &t->params[REV3MIX];
        _gain = &t->params[REV3GAIN];
        _room = &t->params[REV3REF];
        _bypass = &t->bypass[SPACE_REVERB3];
        //_damp = &t->params[REV1DAMP];
    }

    void compute(float *inl, float *inr, float *outl, float *outr) {
        if (*_bypass)
            return;
        update();
        reverb.process(inl, inr, outl, outr, _size);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = LOG2NORMALF(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(_oldroom * 10);
        }
    }

private:
    std::atomic<float> *_mix, *_gain, *_room;
    float _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    jcrev<float> reverb;
};


template<typename T>
class prcrev {
public:
    prcrev(T sr) {
        _sr = sr;
        double scaler = sr / 44100.0;
        int delaysum = 0;

        for (int i = 0; i < 4; i++) {
            int delay = (int) floor(scaler * _lengths[i]);
            _lengths[i] = findNextPrime(delay);

            delaysum += delay;
        }
        _buf.resize(delaysum, 0);
        int sumtmp = 0;


        for (int i = 0; i < 2; i++) {
            _aps[i].setbuffer(_buf.data() + sumtmp, _lengths[i]);
            _aps[i].setfeedback(0.7);
            sumtmp += _lengths[i];
            _combs[i].setbuffer(_buf.data() + sumtmp, _lengths[i + 2]);
            sumtmp += _lengths[i + 2];
        }
    }


    void setwet(T value) {
        _wet = value;
    }

    void setdry(T value) {
        _dry = value;
    }

    void setT60(T T60) {
        if (T60 == 0) {
            for (int i = 0; i < 2; i++)
                _combs[i].setfeedback(0);
        }
        for (int i = 0; i < 2; i++)
            _combs[i].setfeedback(pow(10.0, (-3.0 * _lengths[i + 2] / (T60 * _sr))));
    }

    void process(T *inl, T *inr, T *outl, T *outr, int size) {
        for (int i = 0; i < size; i++) {
            T in = _aps[1].tick(_aps[0].tick((inl[i] + inr[i]) * .5));
            outl[i] = _wet * _combs[0].tick(in) + _dry * inl[i];
            outr[i] = _wet * _combs[1].tick(in) + _dry * inr[i];
        }

    }

private:
    int _lengths[4]{341, 613, 1557, 2137};
    T _sr;
    allpass<T> _aps[2];
    comb<T> _combs[2];
    T _wet, _dry;
    std::vector<T> _buf;
};


class PRCRev : public Effect {
public:
    PRCRev(TRACK *t) : Effect(t), reverb(_sr) {
        _mix = &t->params[REV2MIX];
        _gain = &t->params[REV2GAIN];
        _room = &t->params[REV2REF];
        _bypass = &t->bypass[SPACE_REVERB2];
        //_damp = &t->params[REV1DAMP];
    }

    void compute(float *inl, float *inr, float *outl, float *outr) {
        if (*_bypass)
            return;
        update();
        reverb.process(inl, inr, outl, outr, _size);
    }

    void update() {
        if (_oldgain != *_gain) {
            _oldgain = *_gain;
            _gainfact = LOG2NORMALF(_oldgain);
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldmix != *_mix) {
            _oldmix = *_mix;
            reverb.setwet(_oldmix * _gainfact);
            reverb.setdry((1.f - _oldmix) * _gainfact);
        }
        if (_oldroom != *_room) {
            _oldroom = *_room;
            reverb.setT60(_oldroom * 10);
        }
    }

private:
    std::atomic<float> *_mix, *_gain, *_room;
    float _olddamp{-100000000}, _oldmix{-100000000}, _oldgain{-100000000}, _oldroom{
            -100000000}, _gainfact{};
    prcrev<float> reverb;
};

class REVERB5 : public Effect {
public:
    REVERB5(TRACK *track);

    void compute(float *inl, float *inr, float *outl, float *outr);

    void computecubic(float *inl, float *inr, float *outl, float *outr);

private:
    void updateFilters();

    void init_delay_line(delayLine *lp, int32_t n);

    void next_random_lineseg(delayLine *lp, int32_t n);

    int32_t delay_line_bytes_alloc(int32_t n);

    int32_t delay_line_max_samples(int32_t n);

    double _tdelay[8];
    float _averagedelay{};
    std::atomic<float> *kFeedBack{}, *kLPFreq{}, *_xover, *_t60low, *_t60mid, *_damp, *_predelay;
    float _oldxover{}, _oldt60low{}, _oldt60mid{}, _olddamp{}, _predelayprev{};
    float sampleRate{};
    double _dampFact{};
    double prv_LPFreq{};
    double reverbParams[8][4]{};
    float _xt{}, _yt{};
    Filt1<float> _filt[8];
    /*
    Ap1<double> _delayline[8];
    double _filterstate[8]{};
     */
    delayLine *delayLines[8]{};
    std::vector<unsigned char> auxData;
    float _gainfact{};
    SimpleDelay2<float> _predelayL, _predelayR;
};

class REVERB5b : public Effect {
public:
    REVERB5b(TRACK *track);

    void compute(float *inl, float *inr, float *outl, float *outr);

    void computecubic(float *inl, float *inr, float *outl, float *outr);

private:
    void init_delay_line(delayLine *lp, int32_t n);

    void next_random_lineseg(delayLine *lp, int32_t n);

    int32_t delay_line_bytes_alloc(int32_t n);

    int32_t delay_line_max_samples(int32_t n);

    double _tdelay[8];
    float _averagedelay{};
    std::atomic<float> *_fb{}, *_predelay, *_lpcut, *_hpcut;
    float _oldlpcut{}, _oldhpcut{}, _predelayprev{};
    float sampleRate{};
    double reverbParams[8][4]{};
    float _xt{}, _yt{};
    ReverbButter1<float> _filt[8];
    delayLine *delayLines[8]{};
    std::vector<unsigned char> auxData;
    float _gainfact{};
    SimpleDelay2<float> _predelayL, _predelayR;
};


#endif //GRAINSTORM_REVERB_H
