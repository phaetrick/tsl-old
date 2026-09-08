#pragma once
//
// Created by pr on 05.04.20.
//

#ifndef GRAINSTORM_FM_H
#define GRAINSTORM_FM_H


#include <cmath>
#include "defines.h"
#include "grainstorm.h"
#include "random.h"
#include "Biquad.h"
#include "pv.h"
#include <map>

#define INDEX_MIN .8
#define INDEX_MAX 7.5

#define DFM(x) (0.005 * (MYFLOAT)(x) / 16.f)

#define DFM2(x) (((x) - 7.f) / 32.0f)

static const std::map<std::string, int> midiNoteNames = {{"a0",  21},
                                                         {"as0", 22},
                                                         {"b0",  23},
                                                         {"c1",  24},
                                                         {"cs1", 25},
                                                         {"d1",  26},
                                                         {"ds1", 27},
                                                         {"e1",  28},
                                                         {"f1",  29},
                                                         {"fs1", 30},
                                                         {"g1",  31},
                                                         {"gs1", 32},
                                                         {"a1",  33},
                                                         {"as1", 34},
                                                         {"b1",  35},
                                                         {"c2",  36},
                                                         {"cs2", 37},
                                                         {"d2",  38},
                                                         {"ds2", 39},
                                                         {"e2",  40},
                                                         {"f2",  41},
                                                         {"fs2", 42},
                                                         {"g2",  43},
                                                         {"gs2", 44},
                                                         {"a2",  45},
                                                         {"as2", 46},
                                                         {"b2",  47},
                                                         {"c3",  48},
                                                         {"cs3", 49},
                                                         {"d3",  50},
                                                         {"ds3", 51},
                                                         {"e3",  52},
                                                         {"f3",  53},
                                                         {"fs3", 54},
                                                         {"g3",  55},
                                                         {"gs3", 56},
                                                         {"a3",  57},
                                                         {"as3", 58},
                                                         {"b3",  59},
                                                         {"c4",  60},
                                                         {"cs4", 61},
                                                         {"d4",  62},
                                                         {"ds4", 63},
                                                         {"e4",  64},
                                                         {"f4",  65},
                                                         {"fs4", 66},
                                                         {"g4",  67},
                                                         {"gs4", 68},
                                                         {"a4",  69},
                                                         {"as4", 70},
                                                         {"b4",  71},
                                                         {"c5",  72},
                                                         {"cs5", 73},
                                                         {"d5",  74},
                                                         {"ds5", 75},
                                                         {"e5",  76},
                                                         {"f5",  77},
                                                         {"fs5", 78},
                                                         {"g5",  79},
                                                         {"gs5", 80},
                                                         {"a5",  81},
                                                         {"as5", 82},
                                                         {"b5",  83},
                                                         {"c6",  84},
                                                         {"cs6", 85},
                                                         {"d6",  86},
                                                         {"ds6", 87},
                                                         {"e6",  88},
                                                         {"f6",  89},
                                                         {"fs6", 90},
                                                         {"g6",  91},
                                                         {"gs6", 92},
                                                         {"a6",  93},
                                                         {"as6", 94},
                                                         {"b6",  95},
                                                         {"c7",  96},
                                                         {"cs7", 97},
                                                         {"d7",  98},
                                                         {"ds7", 99},
                                                         {"e7",  100},
                                                         {"f7",  101},
                                                         {"fs7", 102},
                                                         {"g7",  103},
                                                         {"gs7", 104},
                                                         {"a7",  105},
                                                         {"as7", 106},
                                                         {"b7",  107},
                                                         {"c8",  108},
                                                         {"cs8", 109},
                                                         {"d8",  110},
                                                         {"ds8", 111},
                                                         {"e8",  112},
                                                         {"f8",  113},
                                                         {"fs8", 114},
                                                         {"g8",  115},
                                                         {"gs8", 116},
                                                         {"a8",  117},
                                                         {"as8", 118},
                                                         {"b8",  119},
                                                         {"c9",  120},
                                                         {"cs9", 121},
                                                         {"d9",  122},
                                                         {"e9",  123},
                                                         {"f9",  124},
                                                         {"fs9", 125},
                                                         {"g9",  126},
                                                         {"gs9", 127}
};

static inline int32_t n2mdx7(std::string noteName) {
    auto findNote = midiNoteNames.find(noteName);
    if (findNote != midiNoteNames.end()) {
        return findNote->second - 21;
    } else {
        return -1;
    }
}

class MidiNotes {
    // In electronic music, pitch is often given by MIDI number: let's call it m for our purposes. m for the note A4 is 69 and increases by one for each equal tempered semitone, so this gives us a simple conversion between frequencies and MIDI numbers (again using 440 Hz as the pitch of A4):

    //  m  =  12*log2(fm/440 Hz) + 69     and    fm  =  2(m−69)/12(440 Hz).

public:



    // The below utils could probably be integrated into the Bela "standard library"
    // Related issue: https://github.com/BelaPlatform/Bela/issues/554



    static inline MYFLOAT midiToFreq(int32_t _note) {
        return 27.5f * powf(2, (((MYFLOAT) _note - 21) / 12));
    }

    static inline int32_t freqToMidi(MYFLOAT _freq) { return (12 / log(2)) * log(_freq / 27.5) + 21; }

    static inline int32_t noteNameToMidi(std::string noteName) {
        auto findNote = midiNoteNames.find(noteName);
        if (findNote != midiNoteNames.end()) {
            return findNote->second;
        } else {
            return -1;
        }
    }

    static inline MYFLOAT noteNameToFreq(std::string noteName) {
        return midiToFreq(noteNameToMidi(noteName));
    }

    static inline std::string midiToNoteName(int32_t _note) {
        std::map<std::string, int>::const_iterator it;

        for (it = midiNoteNames.begin(); it != midiNoteNames.end(); ++it)
            if (it->second == _note) return it->first;

        return "[Model] Error: note out of range?";
    }

    static inline std::string freqToNoteName(MYFLOAT _freq) {
        return midiToNoteName(freqToMidi(_freq));
    }


};


void init(const int32_t sr, const int rates[4], const int levels[4], const int op_output_level,
          const int32_t transposed_note = 0, const int level_scaling_bkpoint = 0,
          const int32_t level_scaling_l_depth = 0,
          const int32_t level_scaling_l_curve = 0, const int level_scaling_r_depth = 0,
          const int32_t level_scaling_r_curve = 0, const int rate_scaling = 0);

class FM : public Effect {
public:
    //dx7_eg testeg[6];

    inline MYFLOAT
    op(MYFLOAT &phs, MYFLOAT cps, MYFLOAT gain, MYFLOAT inc)  {
        phs += inc;
        inc = cps * _STATE->onedsr;
        MYFLOAT ret = _sine[PHS2INT(phs)] * inc * gain;
        phs += inc;
        return ret;
    }

    inline MYFLOAT
    oplast(MYFLOAT &phs, MYFLOAT cps, MYFLOAT gain, MYFLOAT inc)  {
        phs += inc;
        MYFLOAT ret = _sine[PHS2INT(phs)] * gain;
        phs += cps * _STATE->onedsr;
        return ret;
    }

    inline MYFLOAT opfb(MYFLOAT &phs, MYFLOAT cps, MYFLOAT gain,
                      TwoZero &twozero)  {
        phs += twozero.lastOut();
        MYFLOAT inc = cps * _STATE->onedsr;
        MYFLOAT ret = _sine[PHS2INT(phs)] * inc * gain;
        phs += inc;
        twozero.tick(_sine[PHS2INT(phs)] * inc * gain);
        return ret;
    }

    inline MYFLOAT opfbfwvblnk(MYFLOAT &phs, MYFLOAT cps, MYFLOAT gain,
                             TwoZero &twozero)   {
        phs += twozero.lastOut();
        MYFLOAT inc = cps * _STATE->onedsr;
        twozero.tick(_fwvblnk[PHS2INT(phs)] * inc * gain);
        MYFLOAT ret = _fwvblnk[PHS2INT(phs)] * inc * gain;
        phs += inc;
        return ret;
    }

    inline MYFLOAT oplastfwvblnkfb(MYFLOAT &phs, MYFLOAT cps, MYFLOAT gain,
                                 TwoZero &twozero)  {
        phs += twozero.lastOut();
        MYFLOAT inc = cps * _STATE->onedsr;
        twozero.tick(_fwvblnk[PHS2INT(phs)] * inc * gain);
        MYFLOAT ret = _fwvblnk[PHS2INT(phs)] * gain;
        phs += inc;
        return ret;
    }

    static void gen12(MYFLOAT *table, int32_t size, MYFLOAT scale) {
        static const MYFLOAT coefs[] = {3.5156229, 3.0899424, 1.2067492,
                                       0.2659732, 0.0360768, 0.0045813};
        const MYFLOAT *coefp, *cplim = coefs + 6;
        MYFLOAT sum, tsquare, evenpowr;
        int32_t n;
        MYFLOAT *fp;
        MYFLOAT xscale;

        xscale = (MYFLOAT) scale / (MYFLOAT) size / 3.75;
        for (n = 0; n < size; n++) {
            tsquare = (MYFLOAT) n * xscale;
            tsquare *= tsquare;
            for (sum = evenpowr = 1.0, coefp = coefs; coefp < cplim; coefp++) {
                evenpowr *= tsquare;
                sum += *coefp * evenpowr;
            }
            table[n] = (MYFLOAT) log(sum);
        }
    }

    /*t(const int32_t sr, const int rates[4], const int levels[4], const int op_output_level,
              int32_t transposed_note = 0, int level_scaling_bkpoint = 0, int level_scaling_l_depth = 0,
              int32_t level_scaling_l_curve = 0, int level_scaling_r_depth = 0,
              int32_t level_scaling_r_curve = 0, int rate_scaling = 0) {
     *
     */

    FM(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_FM, MONOEFFECT), simpleVibrato(_STATE->sr) {
        int32_t index = track->index;
        _cps = &_STATE->params[track->index][FMCPS];
        _cpslfo = &track->lfo[FMCPS];
        _indexlfo = &track->lfo[FMI];

        for (int32_t i = 0; i < 6; i++) {
            _pow[i] = &_STATE->params[track->index][FMPOW0 + i];
            _oscsemitones[i] = &_STATE->params[track->index][FMSEM0 + i];
            _multi[i] = 1;
            _gains[i] = &_STATE->params[track->index][FMGAIN0 + i];
            _type[i] = &_STATE->params[track->index][FMTYPE0 + i];
            _twoZeroflute[i].setB2(-1.0);
            _twoZeroflute[i].setGain(0.0);
            _twoZerometal[i].setB2(-1.0);
            _twoZerometal[i].setGain(2.0);
            _twoZerowurley[i].setB2(-1.0);
            _twoZerowurley[i].setGain(2.0);
            _twoZeroB3[i].setB2(-1.0);
            _twoZeroB3[i].setGain(0.1);
            _twoZerorhodes[i].setGain(1.0);
            _twoZerorhodes[i].setB2(-1.0);
            _twozerotest[i].setB2(-1.);
            _twozerokalimba[i].setGain(0);
            _twozerokalimba[i].setB2(-1.0);
            _twozerotamboura[i].setGain(0);
            _twozerotamboura[i].setB2(-1.0);
            _twozerobrass1[i].setGain(0);
            _twozerobrass1[i].setB2(-1.0);
            // testeg[i].init(track->_STATE->sr, ratesviolin[i], valsviolin[i], outviolin[i],
            //                MidiNotes::freqToMidi(LOG2NORMALF(_cps->load())),
            //                MidiNotes::noteNameToMidi(break_points_violin[i]) - 21, 0, 0,
            //                depth_right_violin[i], 0, rate_scaling_violin[i]);
        }
        for (int32_t i = 0; i < NUM_FM_INSTR; i++)
            for (int32_t osc = 0; osc < 6; osc++) {
                _twozero[i][osc].setB2(-1);
                _twozero[i][osc].setGain(1.0);
            }


        _vibratea = &_STATE->params[track->index][FMVIBRATEA];
        _vibdeptha = &_STATE->params[track->index][FMVIBDEPTHA];
        _vibrateb = &_STATE->params[track->index][FMVIBRATEB];
        _vibdepthb = &_STATE->params[track->index][FMVIBDEPTHB];
        _cps = &_STATE->params[track->index][FMCPS];

        _I = &_STATE->params[track->index][FMI];
        _r = &_STATE->params[track->index][FMR];
        _s = &_STATE->params[track->index][FMS];

        _rat = &_STATE->params[track->index][FMRATIO];
        _detune = &_STATE->params[track->index][FMDETUNE];
        _detlr = &_STATE->params[track->index][FMDETLR];
        _dry = &_STATE->params[track->index][FMDRY];
        _wet = &_STATE->params[track->index][FMWET];
        _bypass = &track->bypass[SPACE_FM];
        _pdetectout = track->pitchdetectoutbuf[chan];
        _follow = &_STATE->params[track->index][FMFOLLOW];
        _hold = &_STATE->params[track->index][FMHOLD];
        _instr = &_STATE->params[track->index][FMINSTR];
        _cpsold = &_STATE->params[track->index][FMCPSOLD0 + _chan];

        _sine = getsinewave();
        _cosine = getcosinewave();
        _fulltri = getfulltri();

        /*memcpy(_fulltri, tmp, sizeof(MYFLOAT) * WINDOW_SIZE);
        int32_t channel = chan;
        CHECKFFT(WINDOW_SIZE)
        fft->forward(_fulltri, _fulltri);
        memset(&_fulltri[256], 0, sizeof(MYFLOAT) * (WINDOW_SIZE - 256));
        fft->backward(_fulltri, _fulltri);
*/
        _envf = &_STATE->followerMap[track->index].at(FMI);
        _envfvibrate = &_STATE->followerMap[track->index].at(FMVIBRATEA);
        _envfvibdepth = &_STATE->followerMap[track->index].at(FMVIBDEPTHA);
        _envfgain = &_STATE->followerMap[track->index].at(FMWET);

        _detlrold = 10000000;
        MYFLOAT temp = 1.f;
        for (int32_t i = 99; i >= 0; i--) {
            _amps[i] = temp;
            temp *= 0.933033;
        }

        temp = 1.0;
        for (int32_t i = 15; i >= 0; i--) {
            _sustains[i] = temp;
            temp *= 0.707101;
        }

        for (int32_t i = 0; i < WINDOW_SIZE >> 1; i++)
            _fwvblnk[i] = std::abs(sin((MYFLOAT) i / (MYFLOAT) (WINDOW_SIZE >> 1) * TWOPI_F_P));

        _cpsbuf.resize(_STATE->maxBufSize);
        _indexbuf.resize(_STATE->maxBufSize);
        _outbuf.resize(_STATE->maxBufSize);
        //gen12(tbl.data(), WINDOW_SIZE, 20.);
    }

    void update(bool follow) {
        MYFLOAT temp;
        if ((temp = _detlr->load()) != _detlrold) {
            _detlrold = temp;
            _detunelr = _chan == 0 ? powf(2.f, -(temp * .5f / 1200.f)) : powf(2.f,
                                                                              (temp * .5f /
                                                                               1200.f));
        }
        for (int32_t i = 0; i < 6; i++) {
            if ((temp = _oscsemitones[i]->load()) != _oscsemitonesold[i]) {
                _oscsemitonesold[i] = temp;
                _multi[i] = pow(2, temp / 12);
            }
        }
        if (!follow)
            _cpsold->store(LOG2NORMAL(_cps->load()));
    }

    void computea(int32_t osc, MYFLOAT *in, int size) { //asym2
        if (_bypass->load())
            return;
        update(false);
        MYFLOAT dry = LOG2NORMALF(_dry->load());
        MYFLOAT wet = LOG2NORMALF(_wet->load());
        bool env_on = _envf->prepare(_chan);
        MYFLOAT I_const = INDEX_MIN + DISTANCE(INDEX_MIN, INDEX_MAX) * _I->load();
        MYFLOAT *envbuf = nullptr;
        if (env_on) {
            auto src = _envf->source.load();
            envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
        }

        bool follow = _follow->load() == 1.0 && _hold->load() == 0;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());
        MYFLOAT r = LOG2NORMAL(_r->load());
        r = (1.f + 19 * r);


        bool pow[6];
        int32_t count = 0;
        for (int32_t i = 0; i < 6; i++) {
            pow[i] = _pow[i]->load() == 1.0;
            if (pow[i])
                count++;
        }
        wet /= (MYFLOAT) count;

        MYFLOAT cps = *_cpsold;
        MYFLOAT cpsmod = cps * ratio;
        MYFLOAT cpscar = cps * _detunelr;

        MYFLOAT I = I_const;
        MYFLOAT k1 = I * .5 * (r - 1. / r);
        MYFLOAT k2 = I * .5 * (r + 1. / r) * cpsmod;
        MYFLOAT scal = 1. / exp(k1);

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            if (follow) {
                cps = _pdetectout[i];
                if (cps < 20)
                    cps = 20;
                else if (cps > 5000.)
                    cps = 5000.;
                cpscar = cps * _detunelr;
                cpsmod = cps * ratio;
                k2 = I * .5f * (r + 1. / r) * cpsmod;
            }

            if (env_on) {
                I = _chan == 0 ? _envf->detectL(envbuf[i]) : _envf->detectR(envbuf[i]);
                k1 = I * .5 * (r - 1. / r);
                k2 = I * .5 * (r + 1. / r) * cpsmod;
                scal = 1. / exp(k1);
            }
            for (int32_t osc = 0; osc < 6; osc++) {
                int32_t phmod = PHS2INT(_phasesmod[osc]);
                tmp += (scal * exp(k1 * _cosine[phmod]) * _sine[PHS2INT(_phasescarr[osc])]);
                _phasesmod[osc] += (cpsmod * _multi[osc]) * _STATE->onedsr;
                while (_phasesmod[osc] >= 1.f)
                    _phasesmod[osc] -= 1.f;
                _phasescarr[osc] += (cpscar * _multi[osc] + k2 * _sine[phmod]) *
                                    _STATE->onedsr; //(cpscar * _multi[osc] + s * I * _sine[phmod]) * _STATE->onedsr);
                while (_phasescarr[osc] >= 1.f)
                    _phasescarr[osc] -= 1.f;
                while (_phasescarr[osc] < 0.f)
                    _phasescarr[osc] += 1.f;
            }
            in[i] = in[i] * dry + tmp * wet;
        }
        _cpsold->store(cps);
    }

    void computemod(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //MODFM
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / 0.251167f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());


        for (int32_t i = 0; i < size; i++) {
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cpscar = cps * _detunelr;
            const MYFLOAT cpsmod = cps * ratio;

            const MYFLOAT scal = 1.f / exp(_indexbuf[i]);

            int32_t phmod = PHS2INT(_phasesmod[osc]);
            _outbuf[i] += (scal * exp(_indexbuf[i] * _cosine[phmod]) *
                           _cosine[PHS2INT(_phasescarr[osc])]) * gain;
            _phasesmod[osc] += (cpsmod) * _STATE->onedsr;
            while (_phasesmod[osc] >= 1.f)
                _phasesmod[osc] -= 1.f;
            _phasescarr[osc] += (cpscar) *
                                _STATE->onedsr; //(cpscar  + s * I * _sine[phmod]) * _STATE->onedsr);
            while (_phasescarr[osc] >= 1.f)
                _phasescarr[osc] -= 1.f;
        }
    }
/*

    (definstrument violin (beg end frequency amplitude fm-index)
    (let* ((frq-scl (hz->radians frequency))
    (maxdev (* frq-scl fm-index))
    (index1 (* maxdev (/ 5.0 (log frequency))))
    (index2 (* maxdev 3.0 (/ (- 8.5 (log frequency)) (+ 3.0 (/ frequency 1000)))))
    (index3 (* maxdev (/ 4.0 (sqrt frequency))))
    (carrier (make-oscil :frequency frequency))
    (fmosc1 (make-oscil :frequency frequency))
    (fmosc2 (make-oscil :frequency (* 3 frequency)))
    (fmosc3 (make-oscil :frequency (* 4 frequency)))
    (ampf  (make-env :envelope '(0 0 25 1 75 1 100 0) :scaler amplitude))
            (indf1 (make-env :envelope '(0 1 25 .4 75 .6 100 0) :scaler index1))
    (indf2 (make-env :envelope '(0 1 25 .4 75 .6 100 0) :scaler index2))
            (indf3 (make-env :envelope '(0 1 25 .4 75 .6 100 0) :scaler index3))
    (pervib (make-triangle-wave :frequency 5 :amplitude (* .0025 frq-scl)))
    (ranvib (make-randi :frequency 16 :amplitude (* .005 frq-scl)))
    (vib 0.0))
    (run
    (loop for i from beg to end do
    (setf vib (+ (triangle-wave pervib) (randi ranvib)))
    (outa i (* (env ampf)
    (oscil carrier
            (+ vib
    (* (env indf1) (oscil fmosc1 vib))
    (* (env indf2) (oscil fmosc2 (* 3.0 vib)))
    (* (env indf3) (oscil fmosc3 (* 4.0 vib)))))))))))

    (with-sound () (violin 0 10000 440 .1 2.5))
*/

    void computeviolin(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //VIOLIN
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .251186f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;

            const MYFLOAT logfrq = log(cps);
            const MYFLOAT sqrtfrq = sqrt(cps);

            const MYFLOAT cpsmod0 = cps * ratio;
            const MYFLOAT cpsmod1 = cps * ratio * 3;
            const MYFLOAT cpsmod2 = cps * ratio * 4;
            const MYFLOAT cpscar = cps * _detunelr;

            int32_t phmod0 = PHS2INT(_phasesmod2[osc][0]);
            int32_t phmod1 = PHS2INT(_phasesmod2[osc][1]);
            int32_t phmod2 = PHS2INT(_phasesmod2[osc][2]);

            _outbuf[i] += _sine[PHS2INT(_phasescarr2[osc])] * gain;


            MYFLOAT I = _indexbuf[i];

            MYFLOAT index0 = I * 5.0f / logfrq;
            //if (index1 > .5f) index1 = 5.f;

          //  * maxdev 3.0 (/ (- 8.5 (log frequency)) (+ 3.0 (/ frequency 1000)))


            MYFLOAT index1 = I * 3.0f * (8.5f - logfrq) / (3.0f + cps * .001f);
            //if (index2 > .5f) index2 = .5f;
            MYFLOAT index2 = I * 4.0f / sqrtfrq;
            //if (index3 > .5f) index3 = .5f;

            index0 *= cpsmod0;
            index1 *= cpsmod1;
            index2 *= cpsmod2;


            _phasescarr2[osc] += (cpscar +
                                  (index0 * _sine[phmod0] + index1 * _sine[phmod1] +
                                   index2 * _sine[phmod2])) *
                                 _STATE->onedsr; //(cpscar  + s * I * _sine[phmod]) * _STATE->onedsr);

            _phasesmod2[osc][0] += (cpsmod0) * _STATE->onedsr;
            _phasesmod2[osc][1] += (cpsmod1) * _STATE->onedsr;
            _phasesmod2[osc][2] += (cpsmod2) * _STATE->onedsr;

            while (_phasescarr2[osc] >= 1.f)
                _phasescarr2[osc] -= 1.f;
            while (_phasescarr2[osc] < 0.f)
                _phasescarr2[osc] += 1.f;
            while (_phasesmod2[osc][0] >= 1.f)
                _phasesmod2[osc][0] -= 1.f;
            while (_phasesmod2[osc][1] >= 1.f)
                _phasesmod2[osc][1] -= 1.f;
            while (_phasesmod2[osc][2] >= 1.f)
                _phasesmod2[osc][2] -= 1.f;
        }
    }

    void computegong(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //GONG
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .251189f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        bool updateindex = false;

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cpsmod1 = cps * ratio * 1.16f;
            const MYFLOAT cpsmod2 = cps * ratio * 3.14f;
            const MYFLOAT cpsmod3 = cps * ratio * 1.005f;

            int32_t phmod1 = PHS2INT(_phasesmod2[osc][0]);
            int32_t phmod2 = PHS2INT(_phasesmod2[osc][1]);
            int32_t phmod3 = PHS2INT(_phasesmod2[osc][2]);

            _outbuf[i] += _sine[PHS2INT(_phasescarr2[osc])] * gain;

            MYFLOAT index1 = .01f + (.3f - .01f) * _indexbuf[i] * cpsmod1;
            MYFLOAT index2 = .01f + (.38f - .01f) * _indexbuf[i] * cpsmod2;
            MYFLOAT index3 = .01f + (.50f - .01f) * _indexbuf[i] * cpsmod3;


            _phasescarr2[osc] += (cps * _detunelr +
                                  (index1 * _sine[phmod1] + index2 * _sine[phmod2] +
                                   index3 * _sine[phmod3])) *
                                 _STATE->onedsr; //(cpscar * _multi[osc] + s * I * _sine[phmod]) * _STATE->onedsr);
            while (_phasescarr2[osc] >= 1.f)
                _phasescarr2[osc] -= 1.f;
            while (_phasescarr2[osc] < 0.f)
                _phasescarr2[osc] += 1.f;
            _phasesmod2[osc][0] += (cpsmod1 * _multi[osc]) * _STATE->onedsr;
            _phasesmod2[osc][1] += (cpsmod2 * _multi[osc]) * _STATE->onedsr;
            _phasesmod2[osc][2] += (cpsmod3 * _multi[osc]) * _STATE->onedsr;

            while (_phasesmod2[osc][0] >= 1.f)
                _phasesmod2[osc][0] -= 1.f;
            while (_phasesmod2[osc][1] >= 1.f)
                _phasesmod2[osc][1] -= 1.f;
            while (_phasesmod2[osc][2] >= 1.f)
                _phasesmod2[osc][2] -= 1.f;
        }
    }

    void computebell(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //BELL
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / (1.27f * .256874f);
        MYFLOAT ratio = LOG2NORMALF(_rat->load());


/*
        (definstrument fm-bell (startime dur frequency amplitude
        amp-env index-env index
        &optional (degree 0.0)
        (distance 1.0)
        (reverb-amount 0.005))
        (let* ((beg (floor (* startime *srate*)))
                (end (+ beg (floor (* dur *srate*))))
                (fmInd1 (hz->radians (* 32.0 frequency)))
        (fmInd2 (hz->radians (* 4.0 (- 8.0 (/ frequency 50.0)))))
        (fmInd3 (* fmInd2 0.705 (- 1.4 (/ frequency 250.0))))
        ;;(no hz->radians because included in fmInd2)
        (fmInd4 (hz->radians (* 32.0 (- 20 (/ frequency 20)))))
        (fmenv 0.0)
        (loc (make-locsig :degree degree
        :distance distance
        :reverb reverb-amount))
        (mod1 (make-oscil :frequency (* frequency 2)))
        (mod2 (make-oscil :frequency (* frequency 1.41)))
        (mod3 (make-oscil :frequency (* frequency 2.82)))
        (mod4 (make-oscil :frequency (* frequency 2.4)))
        (car1 (make-oscil :frequency frequency))
        (car2 (make-oscil :frequency frequency))
        (car3 (make-oscil :frequency (* frequency 2.4)))
        (indf (make-env index-env index dur))
        (ampf (make-env amp-env amplitude dur)))
        (run
                (loop for i from beg to end do
            (setf fmenv (env indf))
        (locsig loc i
        (* (env ampf)
        (+        (oscil car1 (* fmenv fmInd1 (oscil mod1)))
        (* .15 (oscil car2 (* fmenv
                (+ (* fmInd2 (oscil mod2))
        (* fmInd3
                (oscil mod3))))))
        (* .15 (oscil car3 (* fmenv
        fmInd4
                (oscil mod4)))))))))))
*/


        for (int32_t i = 0; i < size; i++) {
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cpsmod1 = cps * ratio * 2.f;
            const MYFLOAT cpsmod2 = cps * ratio * 1.41f;
            const MYFLOAT cpsmod3 = cps * ratio * 2.82f;
            const MYFLOAT cpsmod4 = cps * ratio * 2.4f;
            const MYFLOAT cpscar1 = cps * _detunelr;
            const MYFLOAT cpscar2 = cps * _detunelr;
            const MYFLOAT cpscar3 = cps * 2.4f * _detunelr;

            _outbuf[i] += (_sine[PHS2INT(_phasescarrierbell[osc][0])] +
                           .15f * _sine[PHS2INT(_phasescarrierbell[osc][1])] +
                           .15f * _sine[PHS2INT(_phasescarrierbell[osc][2])]) * gain;

            MYFLOAT index1 = 32.f * cps;
            MYFLOAT index2 = 4.0f * (8.f - (cps / 50.f));
            MYFLOAT index3 = index2 * .705f * (1.4f - (cps / 250.f));
            MYFLOAT index4 = 32.f * (20.f - (cps / 20.f));
            _phasescarrierbell[osc][0] +=
                    (cpscar1 + _indexbuf[i] * index1 * _sine[PHS2INT(_phasesmodbell[osc][0])]) *
                    _STATE->onedsr;
            _phasescarrierbell[osc][1] += (cpscar2 + _indexbuf[i] * (index2 * _sine[PHS2INT(
                    _phasesmodbell[osc][1])] + index3 *
                                               _sine[PHS2INT(_phasesmodbell[osc][2])])) *
                                          _STATE->onedsr;
            _phasescarrierbell[osc][2] +=
                    (cpscar3 + _indexbuf[i] * index4 * _sine[PHS2INT(_phasesmodbell[osc][3])]) *
                    _STATE->onedsr;
            while (_phasescarrierbell[osc][0] >= 1.f)
                _phasescarrierbell[osc][0] -= 1.f;
            while (_phasescarrierbell[osc][1] >= 1.f)
                _phasescarrierbell[osc][1] -= 1.f;
            while (_phasescarrierbell[osc][2] >= 1.f)
                _phasescarrierbell[osc][2] -= 1.0f;
            while (_phasescarrierbell[osc][0] < 0.f)
                _phasescarrierbell[osc][0] += 1.f;
            while (_phasescarrierbell[osc][1] < 0.f)
                _phasescarrierbell[osc][1] += 1.f;
            while (_phasescarrierbell[osc][2] < 0.f)
                _phasescarrierbell[osc][2] += 1.0f;
            _phasesmodbell[osc][0] += (cpsmod1) * _STATE->onedsr;
            _phasesmodbell[osc][1] += (cpsmod2) * _STATE->onedsr;
            _phasesmodbell[osc][2] += (cpsmod3) * _STATE->onedsr;
            _phasesmodbell[osc][3] += (cpsmod4) * _STATE->onedsr;

            while (_phasesmodbell[osc][0] >= 1.f)
                _phasesmodbell[osc][0] -= 1.f;
            while (_phasesmodbell[osc][1] >= 1.f)
                _phasesmodbell[osc][1] -= 1.f;
            while (_phasesmodbell[osc][2] >= 1.f)
                _phasesmodbell[osc][2] -= 1.f;
            while (_phasesmodbell[osc][3] >= 1.f)
                _phasesmodbell[osc][3] -= 1.f;
        }
    }


    void computetub(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //TUBBELL
        /*
    (definstrument  tubular-bell (start-time duration frequency amplitude
    &key
            ;; individual amplitude scalings for each of the carrier amp envelopes
    ;; this enables the balance of their respective contributions to the overall
    ;; timbre to be adjusted if need be.
            (amp1 0.5) (amp2 0.25) (amp3 0.25)
    (c1-ratio 2.0) (m1-ratio 5.0)
    (c2-ratio 0.6) (m2-ratio 4.8)
    (c3-ratio 0.22) (m3-ratio 0.83)
    (index-min 0.15)
    ;; individual scalings for each of the modulator indexes -
            ;; thier final values has some dependency on amplitude and frequency
            ;; as can be seen below
    (index1-scl 3.0) (index2-scl 2.0) (index3-scl 1.0)
    (amp-env '(0.0 1.2 3.066 1.0 22.0 0.4 35.0 0.17 50.0 0.05 75.0 0.02 100.0 0.0))
            (index-env '(0.000 1.000 5.109 0.413 13.577 0.144 34.161 0.015 100.000 0.000))
            (degree 0.45) (distance 1.0) (reverb-amount 0.005))

    (let* ((beg (floor (* start-time sampling-rate)))
    (end (+ beg (floor (* duration sampling-rate))))
    ;; to obtain the maximum index value for each modulator, the index-scl
            ;; (above) is multiplied by the amplitude, and the cube of a reference
    ;; frequency (261.5 Hz, c4) divided by the input frequency. This
            ;; latter operation producing exponentially greater values for frequencies
            ;; lower than c4, and exponentially smaller values for frequencies
            ;; higher than c4 -- clearly the higher the tone the more `pure' it
            ;; will tend to be.
            (index1-max (* amplitude index1-scl (expt (/ 261.5 frequency) 3)))
    (index2-max (* amplitude index2-scl (expt (/ 261.5 frequency) 3)))
    (index3-max (* amplitude index3-scl (expt (/ 261.5 frequency) 3)))
    ;; what follows is simply an duplication of structures already seen
    (carrier1 (make-oscil :frequency (* c1-ratio frequency)))
    (carrier2 (make-oscil :frequency (* c2-ratio frequency)))
    (carrier3 (make-oscil :frequency (* c3-ratio frequency)))
    (modulator1 (make-oscil :frequency (* m1-ratio frequency)))
    (modulator2 (make-oscil :frequency (* m2-ratio frequency)))
    (modulator3 (make-oscil :frequency (* m3-ratio frequency)))
    (car-1-env (make-env :envelope amp-env
    :scaler (* amplitude amp1)
    :start-time start-time
    :duration duration))
    (ind-1-env (make-env :envelope index-env
    :offset (in-Hz (* index-min m1-ratio frequency))
    :scaler (in-Hz (* (- index1-max index-min)
    m1-ratio frequency))
    :start-time start-time
    :duration duration))
    (car-2-env (make-env :envelope amp-env
    :scaler (* amplitude amp2)
    :start-time start-time
    :duration duration))
    (ind-2-env (make-env :envelope index-env
    :offset (in-Hz (* index-min m2-ratio frequency))
    :scaler (in-Hz (* (- index2-max index-min)
    m2-ratio frequency))
    :start-time start-time
    :duration duration))
    (car-3-env (make-env :envelope amp-env
    :scaler (* amplitude amp3)
    :start-time start-time
    :duration duration))
    (ind-3-env (make-env :envelope index-env
    :offset (in-Hz (* index-min m3-ratio frequency))
    :scaler (in-Hz (* (- index3-max index-min)
    m3-ratio frequency))
    :start-time start-time
    :duration duration))
    (loc (make-locsig :degree degree
    :distance distance
    :revscale reverb-amount)))

    (Run
            (loop for i from beg to end do
        (locsig loc i (+  ;; the output of each c/m pair is summed together
    (* (env car-1-env)
    (oscil carrier1 (* (env ind-1-env)
    (oscil modulator1))))
    (* (env car-2-env)
    (oscil carrier2 (* (env ind-2-env)
    (oscil modulator2))))
    (* (env car-3-env)
    (oscil carrier3 (* (env ind-3-env)
    (oscil modulator3))))))))))
    */
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .245191f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());


        const MYFLOAT index_min = .15f;
        const MYFLOAT index1_scl = 3.0;
        const MYFLOAT index2_scl = 2.0;
        const MYFLOAT index3_scl = 1.0;
        const MYFLOAT amp1 = .5f;
        const MYFLOAT amp2 = .25f;
        const MYFLOAT amp3 = .25f;

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cpsmod1 = cps * ratio * 5.f;
            const MYFLOAT cpsmod2 = cps * ratio * 4.8f;
            const MYFLOAT cpsmod3 = cps * ratio * .83f;
            const MYFLOAT cpscar1 = cps * 2.0f * _detunelr;
            const MYFLOAT cpscar2 = cps * .6f * _detunelr;
            const MYFLOAT cpscar3 = cps * .22f * _detunelr;

            _outbuf[i] += (amp1 * _sine[PHS2INT(_phasescarriertub[osc][0])] +
                           amp2 * _sine[PHS2INT(_phasescarriertub[osc][1])] +
                           amp3 * _sine[PHS2INT(_phasescarriertub[osc][2])]) * gain;
            MYFLOAT index1_max = index1_scl * powf(261.5f / (cps), 1.f / 3.f);
            MYFLOAT index2_max = index2_scl * powf(261.5f / (cps), 1.f / 3.f);
            MYFLOAT index3_max = index3_scl * powf(261.5f / (cps), 1.f / 3.f);
            MYFLOAT index1 = index_min + (index1_max - index_min) * _indexbuf[i];
            MYFLOAT index2 = index_min + (index2_max - index_min) * _indexbuf[i];
            MYFLOAT index3 = index_min + (index3_max - index_min) * _indexbuf[i];
            _phasescarriertub[osc][0] += (cpscar1 + index1 * cpsmod1 *
                                                    _sine[PHS2INT(_phasesmodtub[osc][0])]) *
                                         _STATE->onedsr;
            _phasescarriertub[osc][1] += (cpscar2 + index2 * cpsmod2 *
                                                    _sine[PHS2INT(_phasesmodtub[osc][0])]) *
                                         _STATE->onedsr;
            _phasescarriertub[osc][2] += (cpscar3 + index3 * cpsmod3 *
                                                    _sine[PHS2INT(_phasesmodtub[osc][0])]) *
                                         _STATE->onedsr;
            while (_phasescarriertub[osc][0] >= 1.f)
                _phasescarriertub[osc][0] -= 1.f;
            while (_phasescarriertub[osc][1] >= 1.f)
                _phasescarriertub[osc][1] -= 1.f;
            while (_phasescarriertub[osc][2] >= 1.f)
                _phasescarriertub[osc][2] -= 1.0f;
            while (_phasescarriertub[osc][0] < 0.f)
                _phasescarriertub[osc][0] += 1.f;
            while (_phasescarriertub[osc][1] < 0.f)
                _phasescarriertub[osc][1] += 1.f;
            while (_phasescarriertub[osc][2] < 0.f)
                _phasescarriertub[osc][2] += 1.0f;
            _phasesmodtub[osc][0] += (cpsmod1) * _STATE->onedsr;
            _phasesmodtub[osc][1] += (cpsmod2) * _STATE->onedsr;
            _phasesmodtub[osc][2] += (cpsmod3) * _STATE->onedsr;
            while (_phasesmodtub[osc][0] >= 1.f)
                _phasesmodtub[osc][0] -= 1.f;
            while (_phasesmodtub[osc][1] >= 1.f)
                _phasesmodtub[osc][1] -= 1.f;
            while (_phasesmodtub[osc][2] >= 1.f)
                _phasesmodtub[osc][2] -= 1.f;
        }
    }

    void computeext(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) { //EXTMODFM
        if (_bypass->load())
            return;
        const bool follow = _follow->load() == 1.0 && _hold->load() == 0 && _track->fxpower[SPACE_PDETECT].load();
        update(follow);
        MYFLOAT dry = LOG2NORMALF(_dry->load());
        MYFLOAT wet = LOG2NORMALF(_wet->load()) * .125f;
        bool env_on = _envf->prepare(_chan);
        MYFLOAT I_const = INDEX_MIN + DISTANCE(INDEX_MIN, INDEX_MAX) * _I->load();
        MYFLOAT *envbuf = nullptr;
        if (env_on) {
            auto src = _envf->source.load();
            envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
        }

        MYFLOAT ratio = LOG2NORMALF(_rat->load());
        MYFLOAT r = -.001 + pow(10, _r->load() * .05f) * 1.001;
        MYFLOAT s = _s->load();

        MYFLOAT I = I_const;
        MYFLOAT scal = 1.f / expf(r * I);


        bool pow[6];
        for (int32_t i = 0; i < 6; i++)
            pow[i] = _pow[i]->load() == 1.0;

        MYFLOAT cps = *_cpsold;

        MYFLOAT cpsmod = cps * ratio;
        MYFLOAT cpscar = cps * _detunelr;

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            if (follow) {
                cps = _pdetectout[i];
                if (cps < 20)
                    cps = 20;
                else if (cps > 5000.f)
                    cps = 5000.f;
                cpsmod = cps * ratio;
                cpscar = cps * _detunelr;
            }

            if (env_on) {
                I = _chan == 0 ? _envf->detectL(envbuf[i]) : _envf->detectR(envbuf[i]);
                scal = 1. / exp(r * I);
            }
            for (int32_t osc = 0; osc < 1; osc++) {
                int32_t phmod = PHS2INT(_phasesmod[osc]);
                tmp += scal * exp(I * r * _cosine[phmod]) * _cosine[PHS2INT(_phasescarr[osc])];
                _phasesmod[osc] += (cpsmod * _multi[osc]) * _STATE->onedsr;
                while (_phasesmod[osc] >= 1.f)
                    _phasesmod[osc] -= 1.f;
                _phasescarr[osc] +=
                        (cpscar * _multi[osc] + cpsmod * s * I * _sine[phmod]) * _STATE->onedsr;
                while (_phasescarr[osc] >= 1.f)
                    _phasescarr[osc] -= 1.f;
                while (_phasescarr[osc] < 0)
                    _phasescarr[osc] += 1.f;
            }
            in[i] = in[i] * dry + tmp * wet;
        }
        _cpsold->store(cps);
    }

    void computeflute(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) {
        const MYFLOAT detune = _detune->load();

        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .125594f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        const MYFLOAT gain0 = _amps[99] * .5f;
        const MYFLOAT gain1 = _amps[71] * 5.f;
        const MYFLOAT gain2 = _amps[93] * .5f;
        const MYFLOAT gain3 = _amps[85] * .5f;

        twozero.setGain(_r->load() * 2);

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cps0 = cps * 1.50f * 1.000f * _detunelr;
            const MYFLOAT cps1 = cps * ratio * 3.f * (1.f - 0.005f * detune);
            const MYFLOAT cps2 = cps * ratio * 3.f * (1.f + 0.006f * detune);
            const MYFLOAT cps3 = cps * ratio * 6.f * (1.f - 0.003f * detune);

            const MYFLOAT index = _indexbuf[i];
            _outbuf[i] += gain * (oplast(phases[0], cps0, gain0, (index * 3 *
                                                                  (op(phases[1], cps1, gain1, 0) +
                                                                   op(phases[2], cps2, gain2,
                                                                      opfb(phases[3], cps3, gain3,
                                                                           twozero))))));
            while (phases[3] >= 1.0)
                phases[3] -= 1.0;
            while (phases[3] < 0)
                phases[3] += 1.0;
            while (phases[2] >= 1.0)
                phases[2] -= 1.0;
            while (phases[2] < 0)
                phases[2] += 1.0;
            while (phases[1] >= 1.0)
                phases[1] -= 1.0;
            while (phases[1] < 0)
                phases[1] += 1.0;
            while (phases[0] >= 1.0)
                phases[0] -= 1.0;
            while (phases[0] < 0)
                phases[0] += 1.0;
        }

    }

    void computemetal(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) {
        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .054668f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        const MYFLOAT gain0 = _amps[92] * .5f * _sustains[14];
        const MYFLOAT gain1 = _amps[76] * 5.f * _sustains[13];
        const MYFLOAT gain2 = _amps[91] * .5f * _sustains[11];
        const MYFLOAT gain3 = _amps[68] * .5f * _sustains[13];


        const MYFLOAT detune = _detune->load();
        twozero.setGain(_r->load() * 2.0f);

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cps0 = cps * 1.000f * _detunelr;
            const MYFLOAT cps1 = cps * ratio * 4.f * (1 - 0.001f * detune);
            const MYFLOAT cps2 = cps * ratio * 3.f * (1 + 0.001f * detune);
            const MYFLOAT cps3 = cps * ratio * .5f * (1 + 0.002f * detune);
            /*
         *
         *  temp = vibrato_.tick() * modDepth_ * 0.2;
waves_[0]->setFrequency(baseFrequency_ * (1.0 + temp) * ratios_[0]);
waves_[1]->setFrequency(baseFrequency_ * (1.0 + temp) * ratios_[1]);
waves_[2]->setFrequency(baseFrequency_ * (1.0 + temp) * ratios_[2]);
waves_[3]->setFrequency(baseFrequency_ * (1.0 + temp) * ratios_[3]);

temp = gains_[2] * adsr_[2]->tick() * waves_[2]->tick();
waves_[1]->addPhaseOffset( temp );

waves_[3]->addPhaseOffset( twozero_.lastOut() );
temp = (1.0 - (control2_ * 0.5)) * gains_[3] * adsr_[3]->tick() * waves_[3]->tick();
twozero_.tick(temp);

temp += control2_ * 0.5 * gains_[1] * adsr_[1]->tick() * waves_[1]->tick();
temp = temp * control1_;

waves_[0]->addPhaseOffset( temp );
temp = gains_[0] * adsr_[0]->tick() * waves_[0]->tick();

lastFrame_[0] = temp * 0.5;
         */


            MYFLOAT temp = gain2 * cps2 * _fulltri[PHS2INT(phases[2])];
            phases[2] += ((cps2) * _STATE->onedsr);
            phases[1] += (temp * _STATE->onedsr);
            phases[3] += (twozero.lastOut() * _STATE->onedsr);
            temp = .5f * gain3 * cps3 *
                   _sine[PHS2INT(phases[3])];
            twozero.tick(temp);
            temp += /*control2*/
                    0.5f * gain1 * cps1 * _fulltri[PHS2INT(phases[1])];
            phases[1] += (cps1 * _STATE->onedsr);
            temp = temp * (5 + 35 * _indexbuf[i]) /*control1_*/;
            phases[0] += ((temp) * _STATE->onedsr);
            temp = gain0 * _sine[PHS2INT(phases[0])];
            phases[0] += ((cps0) * _STATE->onedsr);
            _outbuf[i] += temp * gain;
            while (phases[3] >= 1.0)
                phases[3] -= 1.0;
            while (phases[3] < 0)
                phases[3] += 1.0;
            while (phases[2] >= 1.0)
                phases[2] -= 1.0;
            while (phases[2] < 0)
                phases[2] += 1.0;
            while (phases[1] >= 1.0)
                phases[1] -= 1.0;
            while (phases[1] < 0)
                phases[1] += 1.0;
            while (phases[0] >= 1.0)
                phases[0] -= 1.0;
            while (phases[0] < 0)
                phases[0] += 1.0;
        }

    }

    void computewurley(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) {

        const MYFLOAT detune = _detune->load();
        const MYFLOAT gain0 = _amps[99] * .5f;// * _sustains[14];
        const MYFLOAT gain1 = _amps[82] * 5.f;// * _sustains[13];
        const MYFLOAT gain2 = _amps[82] * .5f;// * _sustains[11];
        const MYFLOAT gain3 = _amps[68] * .5f;// * _sustains[13];

        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .082102f;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        twozero.setGain(_r->load() * 0.1);
/*
 *
 * temp = gains_[1] * adsr_[1]->tick() * waves_[1]->tick();
  temp = temp * control1_;

  waves_[0]->addPhaseOffset( temp );
  waves_[3]->addPhaseOffset( twozero_.lastOut() );
  temp = gains_[3] * adsr_[3]->tick() * waves_[3]->tick();
  twozero_.tick(temp);

  waves_[2]->addPhaseOffset( temp );
  temp = ( 1.0 - (control2_ * 0.5)) * gains_[0] * adsr_[0]->tick() * waves_[0]->tick();
  temp += control2_ * 0.5 * gains_[2] * adsr_[2]->tick() * waves_[2]->tick();

  // Calculate amplitude modulation and apply it to output.
  temp2 = vibrato_.tick() * modDepth_;
  temp = temp * (1.0 + temp2);

  lastFrame_[0] = temp * 0.5;
 */
        const MYFLOAT cps2 = _detunelr * -510.f;
        const MYFLOAT cps3 = ratio * -510.f;

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            MYFLOAT cps0 = cps * _detunelr;
            MYFLOAT cps1 = cps * ratio * (4.f + .05f * detune);

            MYFLOAT index = _indexbuf[i];
            _outbuf[i] += gain * (.5f * (oplast(phases[0], cps0, gain0,
                                                index * op(phases[1], cps1, gain1, 0)) +
                                         oplast(phases[2], cps2, gain2,
                                                opfbfwvblnk(phases[3], cps3, gain3, twozero))));
            while (phases[3] >= 1.0)
                phases[3] -= 1.0;
            while (phases[3] < 0)
                phases[3] += 1.0;
            while (phases[2] >= 1.0)
                phases[2] -= 1.0;
            while (phases[2] < 0)
                phases[2] += 1.0;
            while (phases[1] >= 1.0)
                phases[1] -= 1.0;
            while (phases[1] < 0)
                phases[1] += 1.0;
            while (phases[0] >= 1.0)
                phases[0] -= 1.0;
            while (phases[0] < 0)
                phases[0] += 1.0;
        }

    }

    void computeb3(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) {
        const MYFLOAT gain0 = _amps[95];// * _sustains[14];
        const MYFLOAT gain1 = _amps[95];// * _sustains[13];
        const MYFLOAT gain2 = _amps[99];// * _sustains[11];
        const MYFLOAT gain3 = _amps[95];// * _sustains[13];

        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) * (.125f / .077989f);
        bool follow = _follow->load() == 1.0 && _hold->load() == 0;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        MYFLOAT detune = _detune->load();
        twozero.setGain(_r->load() * 0.1);

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cps0 = cps * (1 - 0.001f * detune);
            const MYFLOAT cps1 = cps * (2 - 0.003f * detune);
            const MYFLOAT cps2 = cps * (3 + 0.006f * detune);
            const MYFLOAT cps3 = cps * (6 + 0.009f * detune);
            /*
         *  temp = 1.0 + ( modDepth_ * vibrato_.tick() * 0.1 );
waves_[0]->setFrequency( baseFrequency_ * temp * ratios_[0] );
waves_[1]->setFrequency( baseFrequency_ * temp * ratios_[1] );
waves_[2]->setFrequency( baseFrequency_ * temp * ratios_[2] );
waves_[3]->setFrequency( baseFrequency_ * temp * ratios_[3] );
}

waves_[3]->addPhaseOffset( twozero_.lastOut() );
temp = control1_ * 2.0 * gains_[3] * adsr_[3]->tick() * waves_[3]->tick();
twozero_.tick( temp );

temp += control2_ * 2.0 * gains_[2] * adsr_[2]->tick() * waves_[2]->tick();
temp += gains_[1] * adsr_[1]->tick() * waves_[1]->tick();
temp += gains_[0] * adsr_[0]->tick() * waves_[0]->tick();

lastFrame_[0] = temp * 0.125;
         */
            _outbuf[i] += gain *
                          (oplast(phases[0], cps0, gain0, 0) + oplast(phases[1], cps1, gain1, 0) +
                           oplast(phases[2], cps2, gain2, 0) +
                           oplastfwvblnkfb(phases[3], cps3, gain3, twozero));
        }
        while (phases[3] >= 1.0)
            phases[3] -= 1.0;
        while (phases[3] < 0)
            phases[3] += 1.0;
        while (phases[2] >= 1.0)
            phases[2] -= 1.0;
        while (phases[2] < 0)
            phases[2] += 1.0;
        while (phases[1] >= 1.0)
            phases[1] -= 1.0;
        while (phases[1] < 0)
            phases[1] += 1.0;
        while (phases[0] >= 1.0)
            phases[0] -= 1.0;
        while (phases[0] < 0)
            phases[0] += 1.0;
    }

    void computerhodes(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero) {
        const MYFLOAT gain0 = _amps[99];// * _sustains[14];
        const MYFLOAT gain1 = _amps[90];// * _sustains[13];
        const MYFLOAT gain2 = _amps[99];// * _sustains[11];
        const MYFLOAT gain3 = _amps[67];// * _sustains[13];

        MYFLOAT gain = dbToLinear60(_gains[osc]->load()) / .251182f;
        bool follow = _follow->load() == 1.0 && _hold->load() == 0;
        MYFLOAT ratio = LOG2NORMALF(_rat->load());

        twozero.setGain(_r->load() * 1.0);

        for (int32_t i = 0; i < size; i++) {
            MYFLOAT tmp = 0;
            MYFLOAT cps = _cpsbuf[i] * _multi[osc];
            if (cps < 20)
                cps = 20;
            else if (cps > 5000.f)
                cps = 5000.f;
            const MYFLOAT cps0 = cps * _detunelr * 1.f;
            const MYFLOAT cps1 = cps * ratio * .5f;
            const MYFLOAT cps2 = cps * _detunelr * 1.f;
            const MYFLOAT cps3 = cps * ratio * 15.f;
            /*
        temp = gains_[1] * adsr_[1]->tick() * waves_[1]->tick();
temp = temp * control1_;

waves_[0]->addPhaseOffset( temp );
waves_[3]->addPhaseOffset( twozero_.lastOut() );
temp = gains_[3] * adsr_[3]->tick() * waves_[3]->tick();
twozero_.tick(temp);

waves_[2]->addPhaseOffset( temp );
temp = ( 1.0 - (control2_ * 0.5)) * gains_[0] * adsr_[0]->tick() * waves_[0]->tick();
temp += control2_ * 0.5 * gains_[2] * adsr_[2]->tick() * waves_[2]->tick();

// Calculate amplitude modulation and apply it to output.
temp2 = vibrato_.tick() * modDepth_;
temp = temp * (1.0 + temp2);

lastFrame_[0] = temp * 0.5;
         */
            MYFLOAT index = _indexbuf[i] * 5;
            _outbuf[i] += gain * (.5f * (oplast(phases[0], cps0, gain0,
                                                index * op(phases[1], cps1, gain1, 0)) +
                                         oplast(phases[2], cps2, gain2,
                                                opfbfwvblnk(phases[3], cps3, gain3, twozero))));
        }
        while (phases[3] >= 1.0)
            phases[3] -= 1.0;
        while (phases[3] < 0)
            phases[3] += 1.0;
        while (phases[2] >= 1.0)
            phases[2] -= 1.0;
        while (phases[2] < 0)
            phases[2] += 1.0;
        while (phases[1] >= 1.0)
            phases[1] -= 1.0;
        while (phases[1] < 0)
            phases[1] += 1.0;
        while (phases[0] >= 1.0)
            phases[0] -= 1.0;
        while (phases[0] < 0)
            phases[0] += 1.0;
    }


    typedef void (FM::*FMInstr)(int32_t osc, MYFLOAT *in, int size, MYFLOAT phases[], TwoZero &twozero);


    void compute(MYFLOAT *in, int32_t size) {
        const bool follow = _follow->load() == 1.0 && _hold->load() == 0 && _track->fxpower[SPACE_PDETECT].load();
        update(follow);
        const bool bypass = (*_bypass || destroyRequested);
        const MYFLOAT dry = bypass ? 1.f : LOG2NORMALF(_dry->load());
        const MYFLOAT wet = bypass ? 0.f : LOG2NORMALF(_wet->load());
        const bool env_on = _envf->prepare(_chan);
        MYFLOAT I_const = _I->load();
        MYFLOAT *envbuf = nullptr;
        if (env_on) {
            auto src = _envf->source.load();
            envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
        }


        MYFLOAT indexrange = 0;
        bool indexlfo_on = false;
        LFO *indexlfo = _indexlfo->load();
        if (indexlfo && indexlfo->power()) {
            indexlfo_on = true;
            MYFLOAT a = _STATE->controls[_track->index][FMI].lfo_min.load();
            MYFLOAT b = _STATE->controls[_track->index][FMI].lfo_max.load();
            indexrange = b-a;
            I_const = a;
        }


        bool envvibdepth_on = _envfvibdepth->prepare(_chan);
        MYFLOAT vibdepth_const = _vibdeptha->load();
        MYFLOAT *envbufvibdepth = nullptr;
        if (envvibdepth_on) {
            auto src = _envfvibdepth->source.load();
            envbufvibdepth = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
        }

        bool envvibrate_on = _envfvibrate->prepare(_chan);
        MYFLOAT vibrate_const = _vibratea->load();
        MYFLOAT *envbufvibrate = nullptr;
        if (envvibrate_on) {
            auto src = _envfvibrate->source.load();
            envbufvibrate = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
        }
        MYFLOAT vibrate = vibrate_const;
        MYFLOAT vibdepth = vibdepth_const;


        MYFLOAT cps = _cpsold->load();

        MYFLOAT cpsrange = 0;
        bool cpslfo_on = false;
        LFO *cpslfo = _cpslfo->load();
        if (cpslfo && cpslfo->power()) {
            cpslfo_on = true;
            MYFLOAT a = powf(10, _STATE->controls[_track->index][FMCPS].lfo_min.load() * .05f);
            MYFLOAT b = powf(10, _STATE->controls[_track->index][FMCPS].lfo_max.load() * .05f);
            cpsrange = b-a;
            cps = a;
        }

        for (int32_t i = 0; i < size; i++) {
            if (envvibrate_on) {
                vibrate = _chan == 0 ? _envfvibrate->detectL(envbufvibrate[i])
                                   : _envfvibrate->detectR(
                                envbufvibrate[i]);
            }
            if (envvibdepth_on) {
                vibdepth = _chan == 0 ? _envfvibdepth->detectL(envbufvibdepth[i])
                                      : _envfvibdepth->detectR(
                                envbufvibdepth[i]);
            }
            if (follow)
                cps = _pdetectout[i];
            MYFLOAT vib = 1.0f + simpleVibrato.tick(vibrate) * vibdepth * .05f;
            _cpsbuf[i] = (cps +
                          (cpslfo_on ? cpslfo->buf[i]*
                                                       cpsrange : 0)) * vib;

            _indexbuf[i] = I_const + (indexlfo_on ? indexlfo->buf[i]*indexrange : 0);


            if (env_on) {
                _indexbuf[i] = _chan == 0 ? _envf->detectL(envbuf[i]) : _envf->detectL(envbuf[i]);
            }
        }

        std::fill(_outbuf.begin(), _outbuf.end(), 0);
        int32_t count = 0;
        for (int32_t i = 0; i < 6; i++) {
            if (_pow[i]->load() == 1.0) {
                count++;
                int32_t t = (int) _type[i]->load();
                FMInstr func = computefuncs[t];

                (this->*func)(i, in, size,
                              _phases[t][i],
                              _twozero[t][i]);
                /*
                MYFLOAT max = 0;
                for (int32_t j = 0; j < size; j++) {
                    max =
                            _outbuf[j] > max ? _outbuf[j] : max;
                }
                LOGE("%s %f", fminstnames[findIndexDouble(fminstvalues, t)] , max);
                 */
                //dx7_alg15(i, in, size, _phases[t][i], _twozero[t][i], dx7_strings3);
            }
        }


        if (count > 0) {
            const MYFLOAT div = 1. / (MYFLOAT) count;
            const bool envgon = _envfgain->prepare(_chan);

            for (int32_t i = 0; i < size; i++) {
                in[i] = in[i] * _smooth2 +
                        _outbuf[i] * (envgon ? LOG2NORMAL(_chan == 0 ?  _envfgain->detectL(in[i]) : _envfgain->detectR(in[i])) : _smooth1) * div;
                smwetdry(wet, dry);
            }
        }

        if (follow) {
            _cpsold->store(_pdetectout[size - 1]);
        }
    }

private:
    std::atomic<MYFLOAT> *_type[6], *_cps, *_pow[6], *_oscsemitones[6], *_gains[6], *_dry, *_wet, *_follow, *_hold, *_detlr, *_detune, *_I, *_r, *_s, *_rat, *_instr, *_cpsold, *_vibratea, *_vibrateb, *_vibdeptha, *_vibdepthb;
    MYFLOAT *_pdetectout;
    MYFLOAT _oscsemitonesold[6]{}, _detlrold{};
    MYFLOAT *_sine, *_cosine;
    MYFLOAT *_fulltri;
    Follower *_envf, *_envfvibrate, *_envfvibdepth, *_envfgain;
    std::atomic<LFO *> *_cpslfo, *_indexlfo;

    MYFLOAT _phasesmod[6]{}, _phasescarr[6]{}, _phasescarr2[6]{}, _multi[6]{}, _phasesmod2[6][3]{}, _phasesmodbell[6][4]{}, _phasescarrierbell[6][3]{}, _phasesmodtub[6][3]{}, _phasescarriertub[6][3]{}, _phasesflute[6][4]{}, _phasesmetal[6][4]{}, _phaseswurley[6][4]{}, _phasesb3[6][4]{}, _phasesrhodes[6][4]{}, _phaseskalimba[6][6]{}, _phasestamboura[6][6]{}, _phasesbrass1[6][6]{};
    MYFLOAT _detunelr{};
    MYFLOAT _amps[100]{}, _sustains[15]{};
    TwoZero _twoZeroflute[6], _twoZerometal[6], _twoZerowurley[6], _twoZeroB3[6], _twoZerorhodes[6], _twozerotest[6], _twozerokalimba[6], _twozerotamboura[6], _twozerobrass1[6];
    MYFLOAT _fwvblnk[WINDOW_SIZE]{};
    tsl::random::SimpleVibrato simpleVibrato;
    MYFLOAT _phases[NUM_FM_INSTR][6][6]{};
    TwoZero _twozero[NUM_FM_INSTR][6];

    FMInstr computefuncs[NUM_FM_INSTR] = {&FM::computemod, &FM::computeviolin,
                                          &FM::computetub, &FM::computegong,
                                          &FM::computebell, &FM::computeflute,
                                          &FM::computewurley,
                                          &FM::computeb3, &FM::computerhodes,
                                          &FM::computemetal, nullptr,
                                          nullptr, nullptr,
                                          nullptr, nullptr,
                                          nullptr, nullptr,
                                          nullptr, nullptr,
                                          nullptr, nullptr,
                                          nullptr, nullptr, nullptr};
    std::vector<MYFLOAT> _outbuf, _indexbuf, _cpsbuf;
};

#endif //GRAINSTORM_FM_H
