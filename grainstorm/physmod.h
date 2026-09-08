#pragma once
//
// Created by pr on 22.04.20.
//

#ifndef GRAINSTORM_PHYSMOD_H
#define GRAINSTORM_PHYSMOD_H


#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <atomic>
#include "defines.h"
#include "Biquad.h"
#include "grainstorm.h"
#include "app.h"

#define make_Noise(n) n = FL(0.0)

#define Noise_lastOut(n) (n)

/*******************************************/
/*  Linearly Interpolating Delay Line      */
/*  Object by Perry R. Cook 1995-96        */
/*  Recoded by John ffitch 1997            */
/*******************************************/

struct DLineL {
    MYFLOAT lastOutput;
    int32_t inPoint;
    int32_t outPoint;
    int32_t length;
    MYFLOAT alpha;
    MYFLOAT omAlpha;
    std::vector<MYFLOAT> input;
};

#define DLineL_lastOut(d)       ((d)->lastOutput)

/*******************************************/
/*  Envelope Class, Perry R. Cook, 1995-96 */
/*  This is the base class for envelopes.  */
/*  This one is capable of ramping state   */
/*  from where it is to a target value by  */
/*  a rate.  It also responds to simple    */
/*  KeyOn and KeyOff messages, ramping to  */
/*  1.0 on keyon and to 0.0 on keyoff.     */
/*  There are two tick (update value)      */
/*  methods, one returns the value, and    */
/*  other returns 0 if the envelope is at  */
/*  the target value (the state bit).      */
/*******************************************/

#define RATE_NORM       (FL(22050.0)/    _STATE->sr)

struct EEnvelope {
    MYFLOAT value;
    MYFLOAT target;
    MYFLOAT rate;
    int32_t state;
};


/*******************************************/
/*  One Pole Filter Class,                 */
/*  by Perry R. Cook, 1995-96              */
/*  The parameter gain is an additional    */
/*  gain parameter applied to the filter   */
/*  on top of the normalization that takes */
/*  place automatically.  So the net max   */
/*  gain through the system equals the     */
/*  value of gain.  sgain is the combina-  */
/*  tion of gain and the normalization     */
/*  parameter, so if you set the poleCoeff */
/*  to alpha, sgain is always set to       */
/*  gain * (1.0 - fabs(alpha)).            */
/*******************************************/

struct OnePole {
    MYFLOAT gain;                 /* Start Filter subclass */
    MYFLOAT outputs;
    /*    MYFLOAT *inputs; */
/*     MYFLOAT lastOutput;  */          /* End */
    MYFLOAT poleCoeff;
    MYFLOAT sgain;
};

/*******************************************/
/*  DC Blocking Filter                     */
/*  by Perry R. Cook, 1995-96              */
/*  This guy is very helpful in, uh,       */
/*  blocking DC.  Needed because a simple  */
/*  low-pass reflection filter allows DC   */
/*  to build up inside recursive           */
/*  structures.                            */
/*******************************************/

struct DCBlock {
    MYFLOAT gain;
    MYFLOAT outputs;
    MYFLOAT inputs;
/*     MYFLOAT    lastOutput; */
};

/*******************************************/
/*  ADSR Subclass of the Envelope Class,   */
/*  by Perry R. Cook, 1995-96              */
/*  This is the traditional ADSR (Attack   */
/*  Decay, Sustain, Release) envelope.     */
/*  It responds to simple KeyOn and KeyOff */
/*  messages, keeping track of it's state. */
/*  There are two tick (update value)      */
/*  methods, one returns the value, and    */
/*  other returns the state (0 = A, 1 = D, */
/*  2 = S, 3 = R)                          */
/*******************************************/

#define ATTACK  (0)
#define DECAY   (1)
#define SUSTAIN (2)
#define RELEASE (3)
#define CLEAR1   (4)

struct ADSR {
    MYFLOAT sr;
    MYFLOAT value;                /* Envelope subclass */
    MYFLOAT target;
    MYFLOAT rate;
    int32_t state;                  /* end */
    MYFLOAT attackRate;
    MYFLOAT decayRate;
    MYFLOAT sustainLevel;
    MYFLOAT releaseRate;
};

/*******************************************/
/*  BiQuad (2-pole, 2-zero) Filter Class,  */
/*  by Perry R. Cook, 1995-96              */
/*  See books on filters to understand     */
/*  more about how this works.  Nothing    */
/*  out of the ordinary in this version.   */
/*******************************************/

struct BiQuad {
    MYFLOAT gain;                 /* Start if filter subclass */
    MYFLOAT inputs[2];
    MYFLOAT lastOutput;           /* End */
    MYFLOAT poleCoeffs[2];
    MYFLOAT zeroCoeffs[2];
};
#define BiQuad_lastOut(x)       (x)->lastOutput

class Physutils {
public:

    MYFLOAT Noise;


};


struct BowTabl {
    MYFLOAT offSet;
    MYFLOAT _slope;
    MYFLOAT lastOutput;
};

#define BiQuad_setGain(b, aValue)        ((b).gain = aValue)
#define BiQuad_setEqualGainZeroes(b)    \
        { (b).zeroCoeffs[1] = -FL(1.0); (b).zeroCoeffs[0] = FL(0.0); }
#define BiQuad_setFreqAndReson(b, freq, reson, tpidsr)    \
        { (b).poleCoeffs[1]= -((reson)*(reson)); \
          (b).poleCoeffs[0]= FL(2.0)*(reson)*\
          (MYFLOAT)cos((double)(freq)*(double)tpidsr); }


class Bowed : public Effect {
/*
 *  Control Change Numbers:
       - Bow Pressure = 2
       - Bow Position = 4
       - Vibrato Frequency = 11
       - Vibrato Gain = 1
       - Bow Velocity = 100
       - Frequency = 101
       - Volume = 128

    void Bowed :: controlChange( int32_t number, StkFloat value )
    {

        StkFloat normalizedValue = value * ONE_OVER_128;
        if ( number == __SK_BowPressure_ ) { // 2
            if ( normalizedValue > 0.0 ) bowDown_ = true;
            else bowDown_ = false;
            bowTable_.setSlope( 5.0 - (4.0 * normalizedValue) );
        }
        else if ( number == __SK_BowPosition_ ) { // 4
            betaRatio_ = normalizedValue;
            bridgeDelay_.setDelay( baseDelay_ * betaRatio_ );
            neckDelay_.setDelay( baseDelay_ * (1.0 - betaRatio_) );
        }
        else if ( number == __SK_ModFrequency_ ) // 11
            vibrato_.setFrequency( normalizedValue * 12.0 );
        else if ( number == __SK_ModWheel_ ) // 1
            vibratoGain_ = ( normalizedValue * 0.4 );
        else if ( number == 100 ) // 100: set instantaneous bow velocity
            adsr_.setTarget( normalizedValue );
        else if ( number == 101 ) // 101: set instantaneous value of frequency
            this->setFrequency( value );
        else if (number == __SK_AfterTouch_Cont_) // 128
            adsr_.setTarget( normalizedValue );
*/
public:
    void make_DLineL(DLineL *p, int32_t max_length) {
        p->length = max_length;
        p->input.resize(max_length, 0);
        p->outPoint = 0;
        p->lastOutput = 0.0;
        p->inPoint = max_length >> 1;
    }

    void DLineL_setDelay(DLineL *p, MYFLOAT lag) {
        MYFLOAT outputPointer = p->inPoint - lag; /* read chases write, +1 for interp. */
        while (outputPointer < 0.f)
            outputPointer += (MYFLOAT) p->length;           /* modulo maximum length */
        while (outputPointer >= (MYFLOAT) p->length)
            outputPointer -= (MYFLOAT) p->length;           /* modulo maximum length */
        p->outPoint = (int32_t) outputPointer;           /* integer part */
        p->alpha = outputPointer - (MYFLOAT) p->outPoint; /* fractional part */
        p->omAlpha = 1.f - p->alpha;               /* 1.0 - fractional part */
    }

    MYFLOAT DLineL_tick(DLineL *p, MYFLOAT sample) /* Take one, yield one */
    {
        MYFLOAT lastOutput;

        p->input[p->inPoint++] = sample; /*  Input next sample */
        if (UNLIKELY(p->inPoint == p->length))         /* Check for end condition */
            p->inPoint -= p->length;
        /* first 1/2 of interpolation */
        lastOutput = p->input[p->outPoint++] * p->omAlpha;
        if (p->outPoint < p->length) {         /*  Check for end condition */
            /* second 1/2 of interpolation    */
            lastOutput += p->input[p->outPoint] * p->alpha;
        } else {                      /*  if at end . . .  */
            /* second 1/2 of interpolation */
            lastOutput += p->input[0] * p->alpha;
            p->outPoint -= p->length;
        }
        return (p->lastOutput = lastOutput);
    }

    void make_OnePole(OnePole *p) {
        p->poleCoeff = 0.9;
        p->gain = 1.0;
        p->sgain = 0.1;
        p->outputs = 0.0;
    }

    void OnePole_setPole(OnePole *p, MYFLOAT aValue) {
        p->poleCoeff = aValue;
        if (p->poleCoeff > 0.0)           /*  Normalize gain to 1.0 max */
            p->sgain = p->gain * 1.0 - p->poleCoeff;
        else
            p->sgain = p->gain * 1.0 + p->poleCoeff;
    }

    void OnePole_setGain(OnePole *p, MYFLOAT aValue) {
        p->gain = aValue;
        if (p->poleCoeff > 0.0)
            p->sgain = p->gain * (1.0 - p->poleCoeff);  /* Normalize gain 1.0 max */
        else
            p->sgain = p->gain * (1.0 + p->poleCoeff);
    }

    MYFLOAT OnePole_tick(OnePole *p, MYFLOAT sample)  /*   Perform Filter Operation */
    {
        p->outputs = (p->sgain * sample) + (p->poleCoeff * p->outputs);
        return p->outputs;
    }


    /*  Perform Table Lookup    */
    MYFLOAT BowTabl_lookup(BowTabl *b,
                         MYFLOAT sample) {                                              /*  sample is differential  */
        MYFLOAT lastOutput;                          /*  string vs. bow velocity */
        MYFLOAT input;
        input = sample /* + b->offSet*/ ;          /*  add bias to sample      */
        input *= b->_slope;                         /*  scale it                */
        lastOutput = std::abs(input) + 0.75; /*  below min delta, frict = 1 */
        lastOutput = powf(lastOutput, -4L);
/* if (lastOutput < FL(0.0) ) lastOutput = FL(0.0); */ /* minimum frict is 0.0 */
        if (lastOutput > 1.0) lastOutput = 1.0; /*  maximum friction is 1.0 */
        return lastOutput;
    }


/*******************************************/
/*  Envelope Class, Perry R. Cook, 1995-96 */
/*  This is the base class for envelopes.  */
/*  This one is capable of ramping state   */
/*  from where it is to a target value by  */
/*  a rate.                                */
/*******************************************/

    void make_Envelope(EEnvelope *e) {
        e->target = 0.0;
        e->value = 0.0;
        e->rate = 0.001;
        e->state = 1;
    }

    void Envelope_keyOn(EEnvelope *e) {
        e->target = 1.0;
        if (e->value != e->target) e->state = 1;
    }

    void Envelope_keyOff(EEnvelope *e) {
        e->target = 0.0;
        if (e->value != e->target) e->state = 1;
    }

    void Envelope_setRate(EEnvelope *e, MYFLOAT aRate) {
        if (UNLIKELY(aRate < 0.0)) {
            e->rate = -aRate;
        } else
            e->rate = aRate;
        //    printf("Env setRate: %p rate=%f value=%f target=%f\n", e,
        //           e->rate, e->value, e->target);
    }

    void Envelope_setTarget(EEnvelope *e, MYFLOAT aTarget) {
        e->target = aTarget;
        if (e->value != e->target) e->state = 1;
    }


    void Envelope_setValue(EEnvelope *e, MYFLOAT aValue) {
        e->state = 0;
        e->target = aValue;
        e->value = aValue;
    }

    MYFLOAT Envelope_tick(EEnvelope *e) {
        //    printf("(Envelope_tick: %p state=%d target=%f, rate=%f, value=%f => ", e,
        //           e->state, e->target, e->rate, e->value);
        if (e->state) {
            if (e->target > e->value) {
                e->value += e->rate;
                if (e->value >= e->target) {
                    e->value = e->target;
                    e->state = 0;
                }
            } else {
                e->value -= e->rate;
                if (e->value <= e->target) {
                    e->value = e->target;
                    e->state = 0;
                }
            }
        }
        //           printf("%f) ", e->value);
        return e->value;
    }

    void make_ADSR(ADSR *a, int32_t sr) {
        a->sr = sr;
        make_Envelope((EEnvelope *) a);
        a->target = FL(0.0);
        a->value = FL(0.0);
        a->attackRate = FL(0.001);
        a->decayRate = FL(0.001);
        a->sustainLevel = FL(0.5);
        a->releaseRate = FL(0.01);
        a->state = ATTACK;
    }

    void ADSR_keyOn(ADSR *a) {
        a->target = FL(1.0);
        a->rate = a->attackRate;
        a->state = ATTACK;
    }

    void ADSR_keyOff(ADSR *a) {
        a->target = FL(0.0);
        a->rate = a->releaseRate;
        a->state = RELEASE;
    }

    void ADSR_setAttackRate(ADSR *a, MYFLOAT aRate) {
        if (UNLIKELY(aRate < FL(0.0))) {
            a->attackRate = -aRate;
        } else a->attackRate = aRate;
        a->attackRate *= RATE_NORM;
    }

    void ADSR_setDecayRate(ADSR *a, MYFLOAT aRate) {
        if (UNLIKELY(aRate < FL(0.0))) {
            a->decayRate = -aRate;
        } else a->decayRate = aRate;
        a->decayRate *= RATE_NORM;
    }

    void ADSR_setSustainLevel(ADSR *a, MYFLOAT aLevel) {
        if (UNLIKELY(aLevel < FL(0.0))) {
            a->sustainLevel = FL(0.0);
        } else a->sustainLevel = aLevel;
    }

    void ADSR_setReleaseRate(ADSR *a, MYFLOAT aRate) {
        if (UNLIKELY(aRate < FL(0.0))) {
            a->releaseRate = -aRate;
        } else a->releaseRate = aRate;
        a->releaseRate *= RATE_NORM;
    }

    void ADSR_setAttackTime(ADSR *a, MYFLOAT aTime) {
        if (UNLIKELY(aTime < FL(0.0))) {
            a->attackRate = FL(1.0) / (-aTime * a->sr);
        } else a->attackRate = FL(1.0) / (aTime * a->sr);
    }

    void ADSR_setDecayTime(ADSR *a, MYFLOAT aTime) {
        if (UNLIKELY(aTime < FL(0.0))) {
            a->decayRate = FL(1.0) / (-aTime * a->sr);
        } else a->decayRate = FL(1.0) / (aTime * a->sr);
    }

    void ADSR_setReleaseTime(ADSR *a, MYFLOAT aTime) {
        if (UNLIKELY(aTime < FL(0.0))) {
            a->releaseRate = FL(1.0) / (-aTime * a->sr);
        } else a->releaseRate = FL(1.0) / (aTime * a->sr);
    }

    void ADSR_setAllTimes(ADSR *a, MYFLOAT attTime, MYFLOAT decTime,
                          MYFLOAT susLevel, MYFLOAT relTime) {
        ADSR_setAttackTime(a, attTime);
        ADSR_setDecayTime(a, decTime);
        ADSR_setSustainLevel(a, susLevel);
        ADSR_setReleaseTime(a, relTime);
    }

    void ADSR_setAll(ADSR *a, MYFLOAT attRate, MYFLOAT decRate,
                     MYFLOAT susLevel, MYFLOAT relRate) {
        ADSR_setAttackRate(a, attRate);
        ADSR_setDecayRate(a, decRate);
        ADSR_setSustainLevel(a, susLevel);
        ADSR_setReleaseRate(a, relRate);
    }

    void ADSR_setTarget(ADSR *a, MYFLOAT aTarget) {
        a->target = aTarget;
        if (a->value < a->target) {
            a->state = ATTACK;
            ADSR_setSustainLevel(a, a->target);
            a->rate = a->attackRate;
        }
        if (a->value > a->target) {
            ADSR_setSustainLevel(a, a->target);
            a->state = DECAY;
            a->rate = a->decayRate;
        }
    }

    void ADSR_setValue(ADSR *a, MYFLOAT aValue) {
        a->state = SUSTAIN;
        a->target = aValue;
        a->value = aValue;
        ADSR_setSustainLevel(a, aValue);
        a->rate = FL(0.0);
    }

    MYFLOAT ADSR_tick(ADSR *a) {
        if (a->state == ATTACK) {
            a->value += a->rate;
            if (a->value >= a->target) {
                a->value = a->target;
                a->rate = a->decayRate;
                a->target = a->sustainLevel;
                a->state = DECAY;
            }
        } else if (a->state == DECAY) {
            a->value -= a->decayRate;
            if (a->value <= a->sustainLevel) {
                a->value = a->sustainLevel;
                a->rate = FL(0.0);
                a->state = SUSTAIN;
            }
        } else if (a->state == RELEASE) {
            a->value -= a->releaseRate;
            if (a->value <= FL(0.0)) {
                a->value = FL(0.0);
                a->state = CLEAR1;
            }
        }
        return a->value;
    }

/*******************************************/
/*  BiQuad (2-pole, 2-zero) Filter Class,  */
/*  by Perry R. Cook, 1995-96              */
/*  See books on filters to understand     */
/*  more about how this works.  Nothing    */
/*  out of the ordinary in this version.   */
/*******************************************/

    void make_BiQuad(BiQuad *b) {
        b->zeroCoeffs[0] = FL(0.0);
        b->zeroCoeffs[1] = FL(0.0);
        b->poleCoeffs[0] = FL(0.0);
        b->poleCoeffs[1] = FL(0.0);
        b->gain = FL(1.0);
/*     BiQuad_clear(b); */
        b->inputs[0] = FL(0.0);
        b->inputs[1] = FL(0.0);
        b->lastOutput = FL(0.0);
    }

    void BiQuad_clear(BiQuad *b) {
        b->inputs[0] = FL(0.0);
        b->inputs[1] = FL(0.0);
        b->lastOutput = FL(0.0);
    }

    void BiQuad_setPoleCoeffs(BiQuad *b, MYFLOAT *coeffs) {
        b->poleCoeffs[0] = coeffs[0];
        b->poleCoeffs[1] = coeffs[1];
    }

    void BiQuad_setZeroCoeffs(BiQuad *b, MYFLOAT *coeffs) {
        b->zeroCoeffs[0] = coeffs[0];
        b->zeroCoeffs[1] = coeffs[1];
    }

    MYFLOAT BiQuad_tick(BiQuad *b, MYFLOAT sample) /*   Perform Filter Operation   */
    {                               /*  Biquad is two pole, two zero filter  */
        MYFLOAT temp;                 /*  Look it up in your favorite DSP text */

        temp = sample * b->gain;                     /* Here's the math for the  */
        temp += b->inputs[0] * b->poleCoeffs[0];     /* version which implements */
        temp += b->inputs[1] * b->poleCoeffs[1];     /* only 2 state variables.  */

        b->lastOutput = temp;                               /* This form takes   */
        b->lastOutput += (b->inputs[0] * b->zeroCoeffs[0]); /* 5 multiplies and  */
        b->lastOutput += (b->inputs[1] * b->zeroCoeffs[1]); /* 4 adds            */
        b->inputs[1] = b->inputs[0];                        /* and 3 moves       */
        b->inputs[0] = temp;                        /* like the 2 state-var form */

        return b->lastOutput;

    }


    Bowed(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_BOWED, MONOEFFECT),
                                    _presold(-1),
                                    _posold(-1),
                                    _vibold(-1),
                                    _velold(-1),
                                    tonelp(1.f /
                                           (_STATE->sr /
                                            2048.f * 50)),
                                    tone(1.0, &tonelp,
                                         false) {
        onedsr = 1. / _STATE->sr;
        _cps = &_STATE->params[track->index][BOWEDCPS];

        _cpsold = &_STATE->params[track->index][BOWEDOLD0 + _chan];
        _pres = &_STATE->params[track->index][BOWEDPRES];
        _pos = &_STATE->params[track->index][BOWEDPOS];
        _vib = &_STATE->params[track->index][BOWEDVIB];
        _vibgain = &_STATE->params[track->index][BOWEDVIBGAIN];
        _dry = &_STATE->params[track->index][BOWEDDRY];
        _wet = &_STATE->params[track->index][BOWEDWET];
        _follow = &_STATE->params[track->index][BOWEDFOLLOW];
        _hold = &_STATE->params[track->index][BOWEDHOLD];
        _pdetectout = track->pitchdetectoutbuf[_chan];
        _bypass = &track->bypass[SPACE_BOWED];
        fol = &track->_STATE->followerMap[track->index].at(BOWEDPOS);
        init();
        sinewave = getsinewave();
    }

    void update2() {
        /*
        if (_presold != _pres->load())
            bowTabl.slope = _presold = _pres->load();
*/
        /* Set Frequency if changed */
        if (_follow->load() == 0 && _hold->load() == 0.0) {
            /* delay - approx. filter delay */
            _cpsold->store(LOG2NORMAL(_cps->load()));
            _baseDelay = _STATE->sr / _cpsold->load() - FL(4.0);
            freq_changed = 1;
        }
        if (_posold != _pos->load() ||
            freq_changed) {         /* Reset delays if changed */
            _posold = _pos->load();
            DLineL_setDelay(&_bridgeDelay, /* bow to bridge length */
                            _baseDelay * (.006f + (0.988f - 0.006f) * _posold));
            DLineL_setDelay(&_neckDelay, /* bow to nut (finger) length */
                            _baseDelay * (FL(1.0) - (.006f + (0.988f - 0.006f) * _posold)));
        }
        //if (_presold != *_pres)
        //    bowTabl._slope = 1.f + 4.f * (_presold = _pres->load());

        /*
        if (_vibold != _vib->load()) {
            _vibold = _vib->load();
            _vrate = _vibold * WINDOW_SIZE * onedsr;
        }
        _vibgainold = _vibgain->load();
*/
    }


    void init() {
        _vtime = 0;
        int32_t length;
        MYFLOAT *vibr;

        length = (int32_t) (_STATE->sr / 20.f + FL(1.0));

        make_DLineL(&_neckDelay, length);
        length = length >> 1; /* Unsure about this; seems correct in later code */
        make_DLineL(&_bridgeDelay, length);

        /*  p->bowTabl.offSet = FL(0.0);*/
        /* offset is a bias, really not needed unless */
        /* friction is different in each direction    */

        /* p->bowTabl.slope contrls width of friction pulse, related to bowForce */
        //bowTabl._slope = 1.f + 4.f * (_presold = _pres->load());
        make_OnePole(&reflFilt);
        make_BiQuad(&bodyFilt);
        make_ADSR(&adsr, _STATE->sr);

        DLineL_setDelay(&_neckDelay, FL(100.0));
        DLineL_setDelay(&_bridgeDelay, FL(29.0));

        OnePole_setPole(&reflFilt, FL(0.6) - (FL(0.1) * RATE_NORM));
        OnePole_setGain(&reflFilt, FL(0.95));

        BiQuad_setFreqAndReson(bodyFilt, FL(500.0), FL(0.85), TWOPI_P / (double) _STATE->sr);
        BiQuad_setEqualGainZeroes(bodyFilt);
        BiQuad_setGain(bodyFilt, FL(0.2));

        ADSR_setAllTimes(&adsr, FL(0.02), FL(0.005), FL(0.9), FL(0.01));
/*        ADSR_setAll(&p->adsr, 0.002f,0.01f,0.9f,0.01f); */

        adsr.target = FL(1.0);
        adsr.rate = adsr.attackRate;
        adsr.state = ATTACK;
        _maxVelocity = FL(0.03) + (FL(0.2) * 1.0f);
        bowTabl._slope = .25f;
    }

    void compute(MYFLOAT *buf, int32_t size) {
        MYFLOAT dry, wet;
        if (_bypass->load() || destroyRequested){
            dry = 1.f; wet = 0;
        }
        else{
            dry = LOG2NORMALF(*_dry);
            wet = LOG2NORMALF(*_wet);
        }
        update2();
        double pos_const = _posold;
        double pos = pos_const;
        const bool env_on = fol->prepare(_chan);
        MYFLOAT *envbuf = nullptr;
        if (env_on) {
            auto src = fol->source.load();
            envbuf = src == _track->index ? buf : _DATA->tracks[src]->envf_buffer[_chan];
        }

        if (false) {
            ADSR_setDecayRate(&adsr, (FL(1.0) - adsr.value) * FL(0.005));
            adsr.target = FL(0.0);
            adsr.rate = adsr.releaseRate;
            adsr.state = RELEASE;
        }

        bool follow = _follow->load() == 1.0 && _hold->load() == 0;
        bool update = false;
        MYFLOAT cps = _cpsold->load();
        MYFLOAT gainf = 1.0;
        for (int32_t n = 0; n < size; n++) {
            if (follow) {
                cps = _pdetectout[n];
                if (cps < 20.f)
                    cps = 20.f;
                else if (cps > 5000.f)
                    cps = 5000.f;
                _baseDelay = _STATE->sr / cps - FL(4.0);
                update = true;
            }
            if (env_on) {
                pos = _chan == 0 ? fol->detectL(envbuf[n]) : fol->detectR(envbuf[n]);
                _posold = pos;
                update = true;

            }
            if (update) {
                DLineL_setDelay(&_bridgeDelay, /* bow to bridge length */
                                _baseDelay * (.006f + (0.988f - 0.006f) * _posold));
                DLineL_setDelay(&_neckDelay, /* bow to nut (finger) length */
                                _baseDelay * (FL(1.0) - (.006f + (0.988f - 0.006f) * _posold)));
            }

            MYFLOAT bowVelocity = _maxVelocity * ADSR_tick(&adsr);

            /* Bridge Reflection      */
            MYFLOAT bridgeRefl = -OnePole_tick(&reflFilt, _bridgeDelay.lastOutput);
            MYFLOAT nutRefl = -_neckDelay.lastOutput;       /* Nut Reflection  */
            MYFLOAT stringVel = bridgeRefl + nutRefl;          /* Sum is String Velocity */
            MYFLOAT velDiff = bowVelocity - stringVel;         /* Differential Velocity  */
            /* Non-Lin Bow Function   */
            MYFLOAT newVel = velDiff * BowTabl_lookup(&bowTabl, velDiff);
            DLineL_tick(&_neckDelay, bridgeRefl + newVel);  /* Do string       */
            DLineL_tick(&_bridgeDelay, nutRefl + newVel);   /*   propagations  */

            /*
            if (_vibgainold > FL(0.0)) {
                _vtime += _vrate;
                while (_vtime >= WINDOW_SIZE)
                    _vtime -= WINDOW_SIZE;
                while (_vtime < FL(0.0))
                    _vtime += WINDOW_SIZE;
                DLineL_setDelay(&_neckDelay,
                                (_baseDelay * (FL(1.0) - _posold)) +
                                (_baseDelay * _vibgainold * sinewave[(int) _vtime]));
            } else*/
            DLineL_setDelay(&_neckDelay,
                            (_baseDelay * (FL(1.0) - _posold)));

            buf[n] = buf[n] * _smooth2 + BiQuad_tick(&bodyFilt, _bridgeDelay.lastOutput) * FL(1.8) * _smooth1;
            smwetdry(wet, dry);
        }
        _cpsold->store(cps);
    }


private:
    std::atomic<MYFLOAT> *_cps, *_cpsold, *_pres, _presold, *_pos, _posold, *_vib, _vibold, *_vibgain, _vibgainold, _velold, *_dry, *_wet, _oldwet, *_follow, *_hold;
    std::atomic<MYFLOAT> tonelp;
    MYFLOAT *_pdetectout;
    Tone tone;
    MYFLOAT _baseDelay, _maxVelocity, _vrate, _vtime;
    MYFLOAT onedsr;
    bool freq_changed;
    MYFLOAT *sinewave;
    BowTabl bowTabl;
    ADSR adsr;
    OnePole reflFilt;
    BiQuad bodyFilt;
    DLineL _neckDelay, _bridgeDelay;
    Follower *fol{};
};


#endif //GRAINSTORM_PHYSMOD_H
