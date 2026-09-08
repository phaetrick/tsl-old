#pragma once
//
// Created by pr on 10.12.19.
//

#ifndef GRAINSTORM_LFO_H
#define GRAINSTORM_LFO_H


#include <atomic>
#include <cmath>
#include "types.h"
#include "defines.h"
#include "envelope.h"
#include "ControlItem.h"
#include "random.h"

#define LFO_TBL_SIZE TBLSIZE3
#define LFO_FREQ_MAX 20
#define LFO_FREQ_MIN 0.001
#define LOGMIN LOG10D20F(LFO_FREQ_MIN)
#define LOGMAX LOG10D20F(LFO_FREQ_MAX)
#define LOGRANGE DISTANCEF(LOGMIN, LOGMAX)

#define MAX_SEGMENTS_LFO 16
#define MAX_LFO_ZOOM 10.f

enum lfo_envelopes {
    LFO_SINE,
    LFO_TRIANGLE,
    LFO_RAMP,
    LFO_PULSE,
    LFO_USER,
    LFO_RND
};

class LFO;

MYFLOAT lfo_sine_direct(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_user(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_sine_add(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_ramp_add(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_triangle_add(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_pulse_add(LFO *lfo, MYFLOAT phase, MYFLOAT range);


MYFLOAT lfo_sine_sub(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_ramp_sub(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_triangle_sub(LFO *lfo, MYFLOAT phase, MYFLOAT range);

MYFLOAT lfo_pulse_sub(LFO *lfo, MYFLOAT phase, MYFLOAT range);


struct LFO : SyncTarget {
public:
    LFO(tsl::AppState *appState, TRACK *t, int32_t _index, const char *_name) : _appState(appState), track(t), index(_index), name(_name),
                                                   SyncTarget(appState, _name, t, LFO1CONTROLSACTIVE +
                                                                        _index * LFONUMPARAMS,
                                                              LFO1SYNCFACT +
                                                              _index * LFONUMPARAMS) {

    }

    const char *name;
    int32_t index;
    TRACK *track;
    MYFLOAT *env[5]{nullptr, nullptr, nullptr, nullptr, computeTable};
    MYFLOAT computeTable[WINDOW_SIZE];
   // MYFLOAT drawTable[LFO_TBL_SIZE];
    bool redrawExchange();
	std::atomic<bool> updateRenderThread{ false };
    MYFLOAT min(int) const;

    MYFLOAT max(int) const;

    MYFLOAT gp(int32_t num) const;

    bool power() const;

    MYFLOAT freq() const;

    MYFLOAT phs() const;

    MYFLOAT phinc() const;

    int32_t func() const;

    MYFLOAT dir() const;

    int32_t segments() const;

    bool stopped() const;

    bool syncmidi() const;

    MYFLOAT clockmulti() const;

    MYFLOAT inc() const;

    void store(int32_t par, MYFLOAT value) const;

    int32_t dest() const;

    MYFLOAT quant() const;

    bool joinends() const;

    void phsforw();

    void phsback();

    int32_t editfunc() const;

    MYFLOAT zoom() const;

    MYFLOAT pos() const;

    std::atomic<MYFLOAT> *getpos0() const;

    std::atomic<MYFLOAT> *getval0() const;

    void reCompute();

    void start() const;

    void stop() const;

    void rev() const;

    double getSamples() override;

    tsl::Syncing::SyncResult control(uint16_t todo, std::vector<tsl::parameters::Event>&) override;

    tsl::Syncing::SyncResult syncBySamples(double samples, std::vector<tsl::parameters::Event>&) override;

    static constexpr MYFLOAT
    (*functions[5] )(LFO *, MYFLOAT phase, MYFLOAT range) {lfo_sine_add, lfo_triangle_add, lfo_ramp_add,
                                                       lfo_pulse_add, lfo_user};

    inline MYFLOAT draw(int32_t func, MYFLOAT phase, MYFLOAT range) const {
        return func < 4 ? env[func][PHS2INT(phase)] * range :
               0.0;
    }
    

    inline MYFLOAT compute(int32_t func, MYFLOAT phase, MYFLOAT range) const {
        return env[func][PHS2INT(phase)] * range;
    }

    bool powertmp{};

    void setUpBuffer();

    MYFLOAT *buf;

    void getAB(MYFLOAT &a, MYFLOAT &b) {
        a = _a;
        b = _b;
    }

    MYFLOAT getRandomPhinc() {
        return _randomPhaseInc;
    }
    void setRandomPhinc(MYFLOAT val) {
        _randomPhaseInc = val;
    }

#if defined(PLUGIN_MODE) || defined(OS_IOS)
    MYFLOAT CyclesPerBar() const;
    bool DetectLfoDiscontinuity();
#endif

    bool _rndDraw{};
    int32_t _oldsize{};
    tsl::AppState *_appState{};
    std::atomic<MYFLOAT*> drawBuf{};
private:
    MYFLOAT _internalDrawBuf[LFO_TBL_SIZE]{};
    int32_t _writeoffset{};
    int32_t _prevFunc{};
    MYFLOAT _a{}, _b{}, _randomPhaseInc{}, _oldR{};
    MYFLOAT _prevRndCpsA{-10000}, _prevRndCpsB{-10000}, _prevRndR{-10000};
    int32_t _prevRndDistr{-10000}, _prevRndCurve{-10000};
    int32_t holdrand{rand()};
    MYFLOAT num0{}, num1{}, num2{}, df0{}, df1{}, c3{}, c2{}, _vel{}, _accel{};
    void getNextRndTarget(const int32_t type, MYFLOAT &a, MYFLOAT &b, const MYFLOAT r, MYFLOAT &vel, MYFLOAT &accel);
    bool init{};
    void getNextRndTarget();
    void getNextFreq();
};

static inline MYFLOAT phasor(MYFLOAT inc, MYFLOAT phase, int32_t i) {
    phase += inc * i;
    while (phase >= 1.0)
        phase -= 1.0;
    while (phase < 0)
        phase += 1.0;
    return phase;
}


#endif //GRAINSTORM_LFO_H
