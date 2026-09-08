#pragma once
//
// Created by pr on 25.02.20.
//

#ifndef GRAINSTORM_INSTRUMENT_H
#define GRAINSTORM_INSTRUMENT_H


#include "logger.h"
#include "grainstorm.h"
#include "tools.h"
#include "track.h"
#include "zreverb.h"
#include "vco.h"
#include "queue.h"

#define DEVIATIONPERCENT 0.0f
#define PLAYBACKSPEED 1.0f

float *getSine();


struct Note {
    int32_t midinum;
    float cps;
    int32_t index;
    float *l, *r;

    Note() {
        cps = 0.0f;
        midinum = 0;
        index = 0;
        r = l = nullptr;
    }
};

class Envelope {
public:

    Envelope(float *pos, float *val, const int32_t nseg) {
        genpoly(env, WINDOW_SIZE, pos, val, nseg, false);
    }

    static inline void normalize(float *in, int32_t s, float gain = .99f) {
        float max = 0;

        for (int32_t i = 0; i < s; i++)
            max = in[i] > max ? in[i] : max;

        LOGE("%f", max);
        if (max != 0)
            for (int32_t i = 0; i < s; i++)
                in[i] *= (gain / max);
    }


    static void
    genpoly(float *fp, int32_t tablesize, float *positions, float *values,
            const int32_t nsegs, bool limit = false) {
        float *end = fp + tablesize;;
        float y, diffd2, vala;
        int32_t pntno, npts;

        float extreme, inflect, a, b;
        for (int32_t seg = 0; seg < nsegs; seg++) {
            float length = tablesize * (positions[seg + 1] - positions[seg]) * .5f;
            if ((npts = (int) length) < 0) {
                return;
            }
            a = values[seg];
            b = values[seg + 1];
            extreme = a;
            inflect = (a + b) * .5f;
            pntno = 0;
            diffd2 = (inflect - extreme) * (0.5f);
            for (; npts > 0 && fp < end; pntno++, npts--) {
                y = (float) pntno / length;
                *fp++ = ((3.0f) - y) * y * y * diffd2 + extreme;
            }
            extreme = b;
            npts = (int) length;
            pntno = npts;
            diffd2 = (inflect - extreme) * (0.5f);
            for (; npts > 0 && fp < end; pntno--, npts--) {
                y = (float) pntno / length;
                vala = ((3.0f) - y) * y * y * diffd2 + extreme;
                vala = limit ? (vala > 1.0f ? 1.0f : (vala < 0.0f ? 0.0f : vala)) : vala;
                *fp++ = vala;
            }
        }
        while (fp < end)                 /* if 2**n pnts, add guardpt */
            *fp++ = vala;
        return;
    }

    float *getTable() {
        return env;
    }

    inline float Tick(float phase) {
        return env[PHS2INT(phase)];
    }

private:
    float env[WINDOW_SIZE];
};

static float positionsenvslowattack[] = {0.0f, 0.5f, 1.f};
static float valuesenvslowattack[] = {0.f, 1.f, 0.f};

static Envelope envslowattack((float *) positionsenvslowattack, (float *) valuesenvslowattack, 2);

static float positionsstrike[] = {0.0f, 0.01f, 0.1, 1.f};
static float valuesstrike[] = {0.f, 1.f, .2f, 0.f};

static Envelope envstrike((float *) positionsstrike, (float *) valuesstrike, 3);

static float positionsenvsawdecay[] = {0.0f, 1.f};
static float valuesenvsawdecay[] = {1.f, 0.f};

static Envelope envsawdecay((float *) positionsenvsawdecay, (float *) valuesenvsawdecay, 1);

static float positionsdecaying[] = {0.0f, .1f, 1.f};
static float valuesdecaying[] = {0.f, 1.f, 0.f};

static Envelope envdecaying((float *) positionsdecaying, (float *) valuesdecaying, 1);


class Instrument {
public:
    static void moogvcf(double sr, float *in, int32_t size, float *freqtab, float q) {
        double xnm1, y1nm1, y2nm1, y3nm1, y1n, y2n, y3n, y4n;

        double onedsr = 1. / sr;
        xnm1 = y1nm1 = y2nm1 = y3nm1 = 0.0;
        y1n = y2n = y3n = y4n = 0.0;

        float inc = 1.f / (float) (size - 1);
        float sp = 0.0f;
        for (int32_t i = 0; i < size; i++) {
            float center = freqtab[PHS2INT(sp)];
            if (center > 8000)
                center = 8000;
            else if (center < 18)
                center = 18;
            double fcon = 2.0 * center * onedsr; /* normalised freq. 0 to Nyquist */
            double kk = 3.6 * fcon - 1.6 * fcon * fcon - 1.0;     /* Emperical tuning   */
            double pp = (kk + 1.0) * 0.5;                   /* Timesaver          */
            double scale = exp((1.0 - pp) * 1.386249);      /* Scaling factor     */
            double k = q * scale;
            double xn = (double) in[i] - k * y4n;

            y1n = (xn + xnm1) * pp - kk * y1n;
            y2n = (y1n + y1nm1) * pp - kk * y2n;
            y3n = (y2n + y2nm1) * pp - kk * y3n;
            y4n = (y3n + y3nm1) * pp - kk * y4n;

            y4n = y4n - y4n * y4n * y4n / 6.0;
            xnm1 = xn;       /* Update Xn-1  */
            y1nm1 = y1n;      /* Update Y1n-1 */
            y2nm1 = y2n;      /* Update Y2n-1 */
            y3nm1 = y3n;      /* Update Y3n-1 */
            in[i] = (MYFLT) (y4n);
            sp += inc;
        }
    }

#define THERMAL (0.000025) /* (1.0 / 40000.0) transistor thermal voltage  */

    static void moogladder(double sr, float *in, int32_t size, float *freqtab, float q) {
        double delay[6], tanhstg[3];
        double res = q * .95;
        memset(delay, 0, 6 * sizeof(double));
        memset(tanhstg, 0, 3 * sizeof(double));
        double stg[4], input;
        double acr, tune;
        int32_t j;

        double f, fc, fc2, fc3, fcr, res4;
        float inc = 1.f / (float) (size - 1);
        float sp = 0.0f;

        for (int32_t i = 0; i < size; i++) {
            double freq = freqtab[PHS2INT(sp)];
            /* sr is half the actual filter sampling rate  */
            fc = freq / (double) sr;
            f = 0.5 * fc;
            fc2 = fc * fc;
            fc3 = fc2 * fc;
            /* frequency & amplitude correction  */
            fcr = 1.8730 * fc3 + 0.4955 * fc2 - 0.6490 * fc + 0.9988;
            acr = -3.9364 * fc2 + 1.8409 * fc + 0.9968;
            tune = (1.0 - exp(-(TWOPI_P * f * fcr))) / THERMAL;   /* filter tuning  */
            res4 = 4.0 * res * acr;
            /* oversampling  */
            for (j = 0; j < 2; j++) {
                /* filter stages  */
                input = in[i] - res4 * delay[5];
                delay[0] = stg[0] = delay[0] + tune * (tanh(input * THERMAL) - tanhstg[0]);
                input = stg[0];
                stg[1] = delay[1] + tune * ((tanhstg[0] = tanh(input * THERMAL)) - tanhstg[1]);
                input = delay[1] = stg[1];
                stg[2] = delay[2] + tune * ((tanhstg[1] = tanh(input * THERMAL)) - tanhstg[2]);
                input = delay[2] = stg[2];
                stg[3] = delay[3] + tune * ((tanhstg[2] =
                                                     tanh(input * THERMAL)) -
                                            tanh(delay[3] * THERMAL));
                delay[3] = stg[3];
                /* 1/2-sample delay for phase compensation  */
                delay[5] = (stg[3] + delay[4]) * 0.5;
                delay[4] = stg[3];
            }
            in[i] = (float) delay[5];
            sp += inc;
        }
    }


    Instrument() {
        _STATE->sr = 48000;
        _STATE->onedsr = 1.0f / (float) _STATE->sr;
        _offset = _notelength = 0;
        _envinc = _envoff = 0;
        _env = nullptr;
        _on = false;
        _nextNote = 0;
        _looptime = 0;
        _gain = 1.0;
    }

    void SetEnv(Envelope *env) {
        _env = env;
    }

    void SetGain(float gain) {
        _gain = gain;
    }

    void SetNoteLength(float seconds, float deviation = DEVIATIONPERCENT) {
        _lengthseconds = seconds;
        _notelength = (int) (_STATE->sr * (_lengthseconds - randomfloat(0, _lengthseconds * deviation)));
        _envinc = 1.f / (float) _notelength;
        //LOGE("Notelength %d %f %f %f %f", notelength, lengthseconds, envinc, gain, looptime);
    }

    virtual void NoteOn() {
        _envoff = 0;
        _on = true;
        //LOGE("%d %f %f %f %f", notelength, lengthseconds, envinc, gain, looptime);
    }


    virtual void SetNextNote(float deviation = DEVIATIONPERCENT, float speed = PLAYBACKSPEED) {
        float next = _STATE->sr * _looptime * speed;//(int) randomfloat(minpause, maxpause) * speed;
        _nextNote = (int) (next + (int) randomfloat(0, next * deviation));
        // LOGE("%d %f %f %f", nextNote, next, looptime, speed);
    }

    virtual void SetNextNoteTimed(float seconds) {
        _nextNote = (int) (_STATE->sr * seconds);//(int) randomfloat(minpause, maxpause) * speed;
        // LOGE("%d %f %f %f", nextNote, next, looptime, speed);
    }

    virtual void OnFinish() = 0;

    void SetLooptime(float seconds) {
        _looptime = seconds;
    }

    float Tick() {
        _nextNote--;
        if (_on) {
            _envoff += _envinc;
            if (_envoff >= 1.0) {
                _on = false;
                return 0.0f;
            } else return tick();
        } else {
            if (_nextNote <= 0) {
                SetNextNote();
                NoteOn();
            }
            return 0.0f;
        }
    }

    void TickStereo(float *l, float *r) {
        _nextNote--;
        if (_on) {
            _envoff += _envinc;
            if (_envoff >= 1.0) {
                _on = false;
                OnFinish();
                return;
            } else {
                tickstereo(l, r);
            }
        } else {
            if (_nextNote <= 0) {
                SetNextNote();
                NoteOn();
            }
            return;
        }
    }

    virtual void SetFrequency(Note note) {}

    virtual void SetFrequency(float _cps) {}

    virtual inline float tick() = 0;

    virtual inline void tickstereo(float *l, float *r) = 0;


    int32_t _STATE->sr;
    float _STATE->onedsr;
    int32_t _offset;
    int32_t _notelength;
    float _lengthseconds;
    float _envinc;
    float _envoff;
    float _gain;
    bool _on;
    int32_t _nextNote;
    float _looptime;
    Envelope *_env;
};


class Osc : public Instrument {
public:
    static enum OscLoopModes {
        LOOPED,
        ONESHOT
    };

    Osc() : Instrument() {
        _phase = 0;
        _mode = LOOPED;
    }

    void NoteOn() {
        Instrument::NoteOn();
        _phase = 0;
    }

    void setWaveform(float *waveform, int32_t length) {
        _waveform = waveform;
        _waveformlength = length;
    }

    void setWaveformStereo(float *l, float *r, int32_t length) {
        _waveforml = l;
        _waveformr = r;
        _waveformlength = length;
    }

    void SetMode(int32_t mode = LOOPED) {
        _mode = mode;
    }

    void OnFinish() {
    };

protected:
    virtual inline float tick() {
        if (_mode == ONESHOT && _phase >= _waveformlength) {
            return 0.0f;

        } else {
            if (_phase >= _waveformlength) {
                _phase = 0;
            }
            return _waveform[_phase++] * _gain * _env->Tick(_envoff);
        }
    }

    virtual inline void tickstereo(float *l, float *r) {
        if (_mode == ONESHOT && _phase >= _waveformlength) {
            return;

        } else {
            if (_phase >= _waveformlength) {
                _phase = 0;
            }
            float env = _env->Tick(_envoff) * _gain;
            *l = _waveforml[_phase] * env;
            *r = _waveformr[_phase++] * env;

            return;
        }
    }

    float *_waveforml, *_waveformr;

private:
    float *_waveform;
    int32_t _waveformlength;
    int32_t _phase;
    int32_t _mode;
};


#define BASEA  220.f

#define FREQUENCYDIFFERENCEINCENTS(x, y) (1200.f * std::log2((x) / (y)))

static const float wtpscalefactors[] = {1.f / 1.f, 3.f / 2.f, 9.f / 8.f, 7.f / 4.f, 21.f / 16.f,
                                        63.f / 32.f, 189.f / 128.f, 567.f / 512.f, 49.f / 32.f,
                                        147.f / 128.f, 441.f / 256.f, 1323.f / 1024.f};
static const std::array<float, 7> consonantdifferencesincent = {701.96f, 498.05f, 968.83f, 266.87f,
                                                                435.08f, 933.13f, 764.92f};
static const std::array<float, 8> consonantratios = {1.f, 3.f / 2.f, 4.f / 3.f, 7.f / 4.f,
                                                     7.f / 6.f, 9.f / 7.f, 12.f / 7.f, 14.f / 9.f};
static const std::array<float, 4> overtoneratios = {1.f, 3.f / 2.f, 4.f / 3.f, 7.f / 4.f};

static const std::array<float, 5> harmonics = {1.f, 3.f, 4.f, 7.f};
static const std::array<float, 5> amps = {1.f, .7, .7, .7};


static std::array<Note, 36> nwtp;

static Note *actualnote = nullptr;



class OscSpecial;

static OscSpecial *oscspecialarray[2];


class OscSpecial : public Osc{
public:



    OscSpecial (int32_t size) : Osc(){
        _waveforml = new float[size];
        _waveformr = new float[size];

    }

    static Note *findnextnote() {
        if (actualnote == nullptr) {
            actualnote = &nwtp[0];
        } else {
            std::array<Note, 36> shuffled = nwtp;
            std::random_shuffle(shuffled.begin(), shuffled.end());

            std::array<float, 7> consonanceshuffled = consonantdifferencesincent;
            std::random_shuffle(consonanceshuffled.begin(), consonanceshuffled.end());

            for (Note &note : shuffled) {
                for (float consonant : consonanceshuffled) {
                    //LOGE("NOTE %d %f %f DIFFX %f / %f", note.index, note.cps, actualnote->cps, (1200.f * LOG2(actualnote->cps / note.cps)), FREQUENCYDIFFERENCEINCENTS(actualnote->cps, note.cps));

                    if (std::abs((int) FREQUENCYDIFFERENCEINCENTS(actualnote->cps, note.cps)) ==
                        (int) consonant && actualnote->index != note.index) {
                        actualnote = &nwtp[note.index];
                    }
                }
            }
        }
        LOGE("INDEX %d", actualnote->index);
        return actualnote;
    }


    static void Computebuffers2(OscSpecial &oscSpecial) {
        int32_t size = oscSpecial._notelength;
        int32_t sr = oscSpecial._STATE->sr;
        float *l = oscSpecial._waveforml;
        float *r = oscSpecial._waveformr;
        Reverb reverb;
        reverb.init(sr, false);
        reverb.set_delay(.04f);
        reverb.set_xover(200.f);
        reverb.set_rtlow(8.f);
        reverb.set_rtmid(8.f);
        reverb.set_fdamp(6.0e3);
        reverb.set_opmix(.5f);


        memset(l, 0, sizeof(float) * size);
        memset(r, 0, sizeof(float) *size);

        float *in[2] = {l, r};
        float *out[2] = {l, r};
        Note *note = findnextnote();
        float cps = note->cps;

        Vco vco(sr);

        float inc =  randomfloat(.01, .01);
        float start = -4 * inc;
        float stop = 5*inc;
        for (float hzoff = start; hzoff <= 0; hzoff += inc) {
            for (float fact : overtoneratios) {
                vco.SetMode(0);
                vco.Compute(l, size, cps * fact + hzoff, 1.f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(l, size,
                            cps * fact + hzoff,
                            1.5f / fact, 0);

            }
        }

        for (float hzoff = 0; hzoff < stop; hzoff += inc) {
            for (float fact : overtoneratios) {
                vco.SetMode(0);
                vco.Compute(r, size, cps * fact + hzoff, 1.0f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(r, size,
                            cps * fact + hzoff,
                            1.5f / fact, 0);
            }
        }


        SimpleDelay simpleDelayl((int) (sr * .017));
        SimpleDelay simpleDelayr((int) (sr * .017));

        simpleDelayl.reset();
        simpleDelayr.reset();
        for (int32_t i = 0; i < size; i++) {
            l[i] = (l[i] + simpleDelayl.tapwrite(l[i])) * .5f;
            r[i] = (r[i] + simpleDelayr.tapwrite(r[i])) * .5f;
        }
        Envelope::normalize(envdecaying.getTable(), WINDOW_SIZE, cps * 7);
        Instrument::moogladder(sr, l, size, envdecaying.getTable(), .3f);
        Instrument::moogladder(sr, r, size, envdecaying.getTable(), .3f);


        reverb.prepare();

        reverb.process(size, in, out);

        //reverb.prepare(sine.notelength);

        //reverb.process(sine.notelength, in, out);

        Envelope::normalize(l, size, .99);
        Envelope::normalize(r, size, .99);

    }
    void NoteOn(){
            Osc::NoteOn();
            OscSpecial *theother =  oscspecialarray[0] == this ? oscspecialarray[1] : oscspecialarray[0];
            std::thread thread(Computebuffers2, std::ref(*theother));
            thread.detach();
    }


};




class LooperSpecial : public Instrument{
public:
    static void Computebuffers2(LooperSpecial *looperSpecial) {


        Reverb reverb;
        reverb.init(looperSpecial->_STATE->sr, false);
        reverb.set_delay(.04f);
        reverb.set_xover(200.f);
        reverb.set_rtlow(8.f);
        reverb.set_rtmid(8.f);
        reverb.set_fdamp(6.0e3);
        reverb.set_opmix(.5f);

        float *l = looperSpecial->_l;
        float *r = looperSpecial->_r;
        memset(l, 0, sizeof(float) * looperSpecial->_buflength);
        memset(r, 0, sizeof(float) * looperSpecial->_buflength);

        float *in[2] = {l, r};
        float *out[2] = {l, r};
        Note *note = LooperSpecial::findnextnote();
        int32_t size = looperSpecial->_buflength;
        looperSpecial->_cps = note->cps;
        float cps = looperSpecial->_cps;
        int32_t sr = looperSpecial->_STATE->sr;

        Vco vco(sr);

        float inc =  randomfloat(.05, .05);
        float start = -4 * inc;
        float stop = 5*inc;
        for (float hzoff = start; hzoff <= 0; hzoff += inc) {
            for (float fact : overtoneratios) {
                vco.SetMode(0);
                vco.Compute(l, size, cps * fact + hzoff, 1.f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(l, size,
                            cps * fact + hzoff,
                            1.5f / fact, 0);

            }
        }

        for (float hzoff = 0; hzoff < stop; hzoff += inc) {
            for (float fact : overtoneratios) {
                vco.SetMode(0);
                vco.Compute(r, size, cps * fact + hzoff, 1.0f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(r, size,
                            cps * fact + hzoff,
                            1.5f / fact, 0);
            }
        }


        SimpleDelay simpleDelayl((int) (sr * .017));
        SimpleDelay simpleDelayr((int) (sr * .017));

        simpleDelayl.reset();
        simpleDelayr.reset();
        for (int32_t i = 0; i < size; i++) {
            l[i] = (l[i] + simpleDelayl.tapwrite(l[i])) * .5f;
            r[i] = (r[i] + simpleDelayr.tapwrite(r[i])) * .5f;
        }
        Envelope::normalize(envdecaying.getTable(), WINDOW_SIZE, cps * 7);
        Instrument::moogladder(sr, l, size, envdecaying.getTable(), .3f);
        Instrument::moogladder(sr, r, size, envdecaying.getTable(), .3f);


        reverb.prepare();

        reverb.process(size, in, out);

        //reverb.prepare(sine.notelength);

        //reverb.process(sine.notelength, in, out);

        Envelope::normalize(l, size, .99);
        Envelope::normalize(r, size, .99);

        looperSpecial->iscomputing.store(false);
    }


    static Note *findnextnote() {
        if (actualnote == nullptr) {
            actualnote = &nwtp[0];
        } else {
            std::array<Note, 36> shuffled = nwtp;
            std::random_shuffle(shuffled.begin(), shuffled.end());

            std::array<float, 7> consonanceshuffled = consonantdifferencesincent;
            std::random_shuffle(consonanceshuffled.begin(), consonanceshuffled.end());

            for (Note &note : shuffled) {
                for (float consonant : consonanceshuffled) {
                    //LOGE("NOTE %d %f %f DIFFX %f / %f", note.index, note.cps, actualnote->cps, (1200.f * LOG2(actualnote->cps / note.cps)), FREQUENCYDIFFERENCEINCENTS(actualnote->cps, note.cps));

                    if (std::abs((int) FREQUENCYDIFFERENCEINCENTS(actualnote->cps, note.cps)) ==
                        (int) consonant && actualnote->index != note.index) {
                        actualnote = &nwtp[note.index];
                    }
                }
            }
        }
        LOGE("INDEX %d", actualnote->index);
        return actualnote;
    }


    void Init(float looptime, float prelength, float postlength, Envelope *preenv,
              Envelope *postenv, float _gain) {
         _cps = 100.f;
        _sineOsc.SetNoteLength(prelength);
        _sineOsc.SetGain(1.0);
        _sineOsc.SetEnv(preenv);
        _sineOsc.SetLooptime(looptime);
        _sineOsc.SetMode(Osc::ONESHOT);
        //_sineOsc.SetNextNote();
        //_sineOsc.NoteOn();
        _buflength = prelength * 3 * _STATE->sr;
        //SetNextNote();
        //NoteOn();
        float pos[3] = {0.0f, prelength / postlength, 1.0f};
        float val[3] = {0.f, 1.f, 0.f};
        _delayenv = new Envelope(pos, val, 2);
        SetLooptime(looptime);
        SetEnv(postenv);
        SetNoteLength(postlength);
        _l = (float *) calloc(1, sizeof(float) * _buflength);
        _r = (float *) calloc(1, sizeof(float) * _buflength);
        _sineOsc.setWaveformStereo(_l, _r, _sineOsc._notelength);

        SetGain(_gain);
        Computebuffers2(this);

        InitDelayLoop();
    }

    void InitDelayLoop(float factor = 1.0) {
        _delaylength = _buflength * .39 * factor;
        _lengthfade = _delaylength * .4f;
        _delaystart = _lengthfade + _buflength * .05;
        _delaytmpstart = _delaystart;
        _delayoff = _buflength - _lengthfade - _buflength * .05;
        _delaytmpoff = _delaystart + _delaylength;
        _delayinc = 1;
        _delayoffset = _delaystart;//randomfloat(_delaystart, _delaytmpoff - 1);
        _delayposdir = 1;
        //incinc = (randomfloat(0.f, 1.f) > .5f ? 1. : -1) *  ;
    }

    void NoteOn() {
        Instrument::NoteOn();
        _delayinc = 1;
        _delayoffset = _delaystart;
        _pingpong = (int) randomfloat(0.0f, 2.0f) < 1.0f;
        //SetNoteLength(_lengthseconds);
        //SetNextNote(2.5);

        float factor = randomfloat(.3, .03f);
        InitDelayLoop(factor);
        _incinc = 0;//randomfloat(-.1f, .1f) / (float) _notelength;
        _sineOsc.SetNextNote();
        _sineOsc.NoteOn();
    }

    void OnFinish() {
        iscomputing = true;
        thread = new std::thread(Computebuffers2, this);
    };


    inline float tick2() {
        _delayoffset += _delayinc;
        _delayinc += _incinc;
        if (_pingpong) {
            if (_delayoffset >= _delaytmpoff) {
                _delayinc *= -1;
            } else if (_delayoffset >= _delaytmpoff - _lengthfade && _delayinc >= 0) {
                int32_t pos = _delaytmpoff - _delayoffset;
                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                return (_outbuf[_delaytmpoff + pos] * fadein +
                        _outbuf[(int) _delayoffset] * fadeout) *
                       _gain *
                       _delayenv->Tick(_envoff) + _sineOsc.Tick();
            }

            if (_delayinc < 0) {
                if (_delayoffset < _delaytmpstart) {
                    _delayinc *= -1;
                } else if (_delayoffset < _delaytmpstart + _lengthfade) {
                    int32_t pos = _delayoffset - _delaytmpstart;
                    float fadeout = pos / (float) _lengthfade;
                    float fadein = 1.f - fadeout;
                    return (_outbuf[_delaytmpstart - pos] * fadein +
                            _outbuf[(int) _delayoffset] * fadeout) *
                           _gain *
                           _delayenv->Tick(_envoff) + _sineOsc.Tick();
                }
            }

            return _outbuf[(int) _delayoffset] * _gain * _delayenv->Tick(_envoff) + _sineOsc.Tick();

        } else {
            _delayoffset += _delayinc;
            _delayinc += _incinc;
            if (_delayoffset >= _delaytmpoff) {
                _delayoffset = _delaytmpstart + _delayinc - DISTANCEF(_delayoffset, _delaytmpoff);
            }

            if (_delayoffset >= _delaytmpoff - _lengthfade) {
                int32_t pos = _delaytmpoff - _delayoffset;

                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                return (_outbuf[_delaytmpstart - pos] * fadein +
                        _outbuf[(int) _delayoffset] * fadeout) *
                       _gain *
                       _delayenv->Tick(_envoff) + _sineOsc.Tick();
            }
            return _outbuf[(int) _delayoffset] * _gain * _delayenv->Tick(_envoff) + _sineOsc.Tick();
        }
        //return sin(phase * TWOPI_F_P);// * gain * envelope[PHS2INT(envoff)];
    }


    inline void tickstereo2(float *l, float *r) {
        _delayoffset += _delayinc;
        _delayinc += _incinc;
        if (_delayoffset >= _delaytmpoff) {
            _delayinc *= -1;
        } else if (_delayoffset >= _delaytmpoff - _lengthfade && _delayinc >= 0) {
            int32_t pos = _delaytmpoff - _delayoffset;
            float fadeout = pos / (float) _lengthfade;
            float fadein = 1.f - fadeout;
            //_sineOsc.TickStereo(&prel, &prer);
            float env = _delayenv->Tick(_envoff) * _gain;
            *l += (_l[_delaytmpoff + pos] * fadein + _l[(int) _delayoffset] * fadeout) * env;
            *r += (_r[_delaytmpoff + pos] * fadein + _r[(int) _delayoffset] * fadeout) * env;
        }

        if (_delayinc < 0) {
            if (_delayoffset < _delaytmpstart) {
                _delayinc *= -1;
            } else if (_delayoffset < _delaytmpstart + _lengthfade) {
                int32_t pos = _delayoffset - _delaytmpstart;
                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                float env = _delayenv->Tick(_envoff) * _gain;
                *l += (_l[_delaytmpstart - pos] * fadein +
                       _l[(int) _delayoffset] * fadeout) * env;
                *r += (_r[_delaytmpstart - pos] * fadein +
                       _r[(int) _delayoffset] * fadeout) * env;
            }
        }

        float env = _delayenv->Tick(_envoff) * _gain;
        *l += (_l[(int) _delayoffset] * env);
        *r += (_r[(int) _delayoffset] * env);
        return;

        //return sin(phase * TWOPI_F_P);// * gain * envelope[PHS2INT(envoff)];
    }

    inline float tick() {
        _delayoffset += _delayinc;
        _delayinc += _incinc;
        if (true) {
            if (_delayoffset >= _delaytmpoff && _delayinc >= 0) {
                _delayinc *= -1;
                int32_t change = _lengthfade * _delayposdir;
                _delaytmpoff += change;
                if (_delaytmpoff >= _delayoff) {
                    change = -(_lengthfade - DISTANCE(_delayoff, _delaytmpoff));
                    _delaytmpoff = _delayoff + change;
                    _delayposdir = -1;
                }
                if (_delaytmpoff < _delaystart + _delaylength) {
                    change = (_lengthfade - DISTANCE(_delaytmpoff, _delaystart + _delaylength));
                    _delaytmpoff = _delaystart + _delaylength + change;
                    _delayposdir = 1;
                }
                _delaytmpstart += change;
            } else if (_delayoffset >= _delaytmpoff - _lengthfade && _delayinc >= 0) {
                int32_t pos = _delaytmpoff - _delayoffset;
                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                return (_outbuf[_delaytmpoff + pos] * fadein +
                        _outbuf[(int) _delayoffset] * fadeout) *
                       _gain *
                       _delayenv->Tick(_envoff) + _sineOsc.Tick();
            }

            if (_delayinc < 0) {
                if (_delayoffset < _delaytmpstart) {
                    _delayinc *= -1;
                } else if (_delayoffset < _delaytmpstart + _lengthfade) {
                    int32_t pos = _delayoffset - _delaytmpstart;
                    float fadeout = pos / (float) _lengthfade;
                    float fadein = 1.f - fadeout;
                    return (_outbuf[_delaytmpstart - pos] * fadein +
                            _outbuf[(int) _delayoffset] * fadeout) *
                           _gain *
                           _delayenv->Tick(_envoff) + _sineOsc.Tick();
                }
            }

            return _outbuf[(int) _delayoffset] * _gain * _delayenv->Tick(_envoff) + _sineOsc.Tick();

        } else {
            _delayoffset += _delayinc;
            _delayinc += _incinc;
            if (_delayoffset >= _delaytmpoff) {
                _delayoffset = _delaytmpstart + _delayinc - DISTANCEF(_delayoffset, _delaytmpoff);
            }

            if (_delayoffset >= _delaytmpoff - _lengthfade) {
                int32_t pos = _delaytmpoff - _delayoffset;

                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                return (_outbuf[_delaytmpstart - pos] * fadein +
                        _outbuf[(int) _delayoffset] * fadeout) *
                       _gain *
                       _delayenv->Tick(_envoff) + _sineOsc.Tick();
            }
            return _outbuf[(int) _delayoffset] * _gain * _delayenv->Tick(_envoff) + _sineOsc.Tick();
        }
        //return sin(phase * TWOPI_F_P);// * gain * envelope[PHS2INT(envoff)];
    }

    inline void tickstereo(float *l, float *r) {
        _delayoffset += _delayinc;
        _delayinc += _incinc;
        float env = _delayenv->Tick(_envoff) * _gain;

        if (true) {
            if (_delayinc > 0) {
                if (_delayoffset >= _delaytmpoff) {
                    _delayoffset -= 2 * _delayinc;
                    _delayinc = -1;
                    int32_t change = _lengthfade * _delayposdir * .1;
                    //_delaytmpoff += change;
                    _delaytmpstart += change;
                    if (_delaytmpstart >= _delaystart + _delaylength) {
                        change = (_lengthfade - DISTANCE( _delaystart + _delaylength, _delaytmpstart));
                        _delaytmpstart -= change;
                        _delayposdir = -1;
                    }
                    else if (_delaytmpstart < _delaystart) {
                        change = (_lengthfade - DISTANCE(_delaystart, _delaytmpstart));
                        _delaytmpstart += change;
                        _delayposdir = 1;
                    }

                } else if (_delayoffset >= _delaytmpoff - _lengthfade && _delayinc >= 0) {
                    int32_t pos = _delaytmpoff - _delayoffset;
                    float fadeout = pos / (float) _lengthfade;
                    float fadein = 1.f - fadeout;
                    *l += ((_l[_delaytmpoff + pos] * fadein + _l[(int) _delayoffset] * fadeout) *
                           env);
                    *r += ((_r[_delaytmpoff + pos] * fadein + _r[(int) _delayoffset] * fadeout) *
                           env);
                    return;
                }
            } else if (_delayinc < 0) {
                if (_delayoffset < _delaytmpstart) {
                    _delayinc = 1;
                    _delayoffset += 2 * _delayinc;
                    _delaytmpoff = _delaytmpstart + _delaylength;
                } else if (_delayoffset < _delaytmpstart + _lengthfade) {
                    int32_t pos = _delayoffset - _delaytmpstart;
                    float fadeout = pos / (float) _lengthfade;
                    float fadein = 1.f - fadeout;
                    *l += (_l[_delaytmpstart - pos] * fadein +
                           _l[(int) _delayoffset] * fadeout) * env;
                    *r += (_r[_delaytmpstart - pos] * fadein +
                           _r[(int) _delayoffset] * fadeout) * env;
                    return;
                }
            }
            *l += _l[(int) _delayoffset] * env;
            *r += _r[(int) _delayoffset] * env;
            return;

        } else {
            _delayoffset += _delayinc;
            _delayinc += _incinc;
            float sl = 0.0f, sr = 0.0f;
            _sineOsc.TickStereo(&sl, &sr);
            if (_delayoffset >= _delaytmpoff) {
                _delayoffset = _delaytmpstart + _delayinc - DISTANCEF(_delayoffset, _delaytmpoff);
            }
            else if (_delayoffset >= _delaytmpoff - _lengthfade) {
                int32_t pos = _delaytmpoff - _delayoffset;
                float fadeout = pos / (float) _lengthfade;
                float fadein = 1.f - fadeout;
                *l += ((_l[_delaytmpstart - pos] * fadein +
                        _l[(int) _delayoffset] * fadeout) * env + sl);
                *r += ((_r[_delaytmpstart - pos] * fadein +
                        _r[(int) _delayoffset] * fadeout) * env + sr);
                return;
            }
            else{
                _delaytmpoff += _delayposdir;
                _delaytmpstart += _delayposdir;
                if(_delaytmpoff >= _delayoff)
                    _delayposdir = -1;
                if(_delaytmpstart < _delaystart)
                    _delayposdir = 1;
            }
            *l += (_l[(int) _delayoffset] * env + sl);
            *r += (_r[(int) _delayoffset] * env + sr);
            return;
        }
    }

    int32_t _buflength;
    float *_outbuf, *_l, *_r;
    std::atomic_bool iscomputing;
protected:
    float _delayinc, _incinc, _delayoffset, _delayposinc, _delayposdir;
    int32_t _delaytmpoff, _delayoff, _delaystart, _delaytmpstart, _lengthfade, _delaylength;
    bool _pingpong;
    Osc _sineOsc;
    Envelope *_delayenv;
    std::thread *thread;
    float _cps;
};


class Drone1 {
public:


    static void Computebuffers(int32_t sr, Note *note, int size, Envelope *filterenv) {

        /*
        Reverb reverb;
        reverb.init(sr, false);
        reverb.set_delay(.04f);
        reverb.set_xover(200.f);
        reverb.set_rtlow(8.f);
        reverb.set_rtmid(8.f);
        reverb.set_fdamp(6.0e3);
        reverb.set_opmix(.5f);
*/
        note->l = (float *) calloc(1, sizeof(float) * size);
        note->r = (float *) calloc(1, sizeof(float) * size);


        float *l = note->l;

        float *r = note->r;

        float *in[2] = {l, r};
        float *out[2] = {l, r};

        Vco vco(sr);

        for (float hzoff = -.4f; hzoff <= 0; hzoff += .1f) {
            for (float fact : consonantratios) {
                vco.SetMode(0);
                vco.Compute(l, size, note->cps * fact + hzoff, 1.f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(l, size,
                            note->cps * fact + hzoff,
                            1.5f / fact, 0);
            }
        }

        for (float hzoff = 0; hzoff < .5f; hzoff += .1f) {
            for (float fact : consonantratios) {
                vco.SetMode(0);
                vco.Compute(r, size, note->cps * fact + hzoff, 1.0f / fact,
                            0);
                vco.SetMode(4);
                vco.Compute(r, size,
                            note->cps * fact + hzoff,
                            1.5f / fact, 0);
            }
        }


        SimpleDelay simpleDelayl((int) (sr * .017));
        SimpleDelay simpleDelayr((int) (sr * .017));

        simpleDelayl.reset();
        simpleDelayr.reset();
        for (int32_t i = 0; i < size; i++) {
            l[i] = (l[i] + simpleDelayl.tapwrite(l[i])) * .5f;
            r[i] = (r[i] + simpleDelayr.tapwrite(l[i])) * .5f;
        }
        Envelope::normalize(filterenv->getTable(), WINDOW_SIZE, note->cps * 7);
        Instrument::moogladder(sr, l, size, filterenv->getTable(), .3f);
        Instrument::moogladder(sr, r, size, filterenv->getTable(), .3f);


        //reverb.prepare();

        //reverb.process(sine._notelength, in, out);

        //reverb.prepare(sine.notelength);

        //reverb.process(sine.notelength, in, out);

        Envelope::normalize(l, size, .99);
        Envelope::normalize(r, size, .99);
    }


    Drone1() {
        nwtp[0].cps = BASEA * (128.f / 189.f);

        for (int32_t i = 0; i < 12; i++) {
            nwtp[i].index = i;
            nwtp[i].cps = nwtp[0].cps * wtpscalefactors[i];
            //Computebuffers(48000, &nwtp[i], 20 * 48000, &envdecaying);
        }
        for (int32_t i = 12; i < 24; i++) {
            nwtp[i].index = i;
            nwtp[i].cps = nwtp[0].cps * wtpscalefactors[i - 12] * .5f;
            //Computebuffers(48000, &nwtp[i], 20 * 48000, &envdecaying);
        }
        for (int32_t i = 24; i < 36; i++) {
            nwtp[i].index = i;
            nwtp[i].cps = nwtp[0].cps * wtpscalefactors[i - 24] * 2.f;
            //Computebuffers(48000, &nwtp[i], 20 * 48000, &envdecaying);
        }

        Init(320.f *  4, 8.f, 240.f * 4, &envstrike, &envslowattack, LOG2NORMALF(-13));
    }

    void InitSolo(){
        OscSpecial *oscSpecial1 = new OscSpecial(48000 * 100);
        oscspecialarray[0] = oscSpecial1;
        oscSpecial1->SetLooptime(101);
        oscSpecial1->SetNoteLength(100);
        oscSpecial1->Computebuffers2(*oscSpecial1);
        oscSpecial1->SetEnv(&envslowattack);
        oscSpecial1->SetNextNote();
        OscSpecial *oscSpecial2 = new OscSpecial(48000 * 100);
        oscspecialarray[1] = oscSpecial2;
        oscSpecial2->SetLooptime(101);
        oscSpecial2->SetNoteLength(100);
        //oscSpecial->Computebuffers2(oscSpecial);
        oscSpecial2->SetEnv(&envslowattack);
        oscSpecial2->SetNextNote();
        oscSpecial1->NoteOn();
    }

    void Init(float looptime, float prelength, float postlength, Envelope *preenv,
              Envelope *postenv, float _gain) {
        looperSpecial[0].Init(looptime, prelength, postlength, preenv, postenv, _gain);
        looperSpecial[0].SetNextNote();
        looperSpecial[0].NoteOn();
        looperSpecial[1].Init(looptime, prelength, postlength, preenv, postenv, _gain);
        looperSpecial[1].SetNextNoteTimed(60.f * 4);
        looperSpecial[2].Init(looptime, prelength, postlength, preenv, postenv, _gain);
        looperSpecial[2].SetNextNoteTimed(120.f * 4);
        looperSpecial[3].Init(looptime, prelength, postlength, preenv, postenv, _gain);
        looperSpecial[3].SetNextNoteTimed(180.f * 4);
    }



    void Tick(float *l, float *r) {
        //oscspecialarray[0]->TickStereo(l, r);
        //oscspecialarray[1]->TickStereo(l, r);

        looperSpecial[0].TickStereo(l, r);
        looperSpecial[1].TickStereo(l, r);
        looperSpecial[2].TickStereo(l, r);
        looperSpecial[3].TickStereo(l, r);
    }

private:
    LooperSpecial looperSpecial[4];
};

#endif //GRAINSTORM_INSTRUMENT_H
