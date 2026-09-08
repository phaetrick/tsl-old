#pragma once
//
// Created by pr on 27.12.22.
//

#ifndef GRAINSTORM_GRAINGENERATOR_H
#define GRAINSTORM_GRAINGENERATOR_H
#define randGab1   (MYFLOAT) ((double)     \
    (((holdrand[chan] = holdrand[chan] * 214013 + 2531011) >> 1)  \
     & 0x7fffffff) * oneUp31Bit)

#define randGab11(chan)   (MYFLOAT) ((double)     \
    (((holdrand[(chan)] = holdrand[(chan)] * 214013 + 2531011) >> 1)  \
     & 0x7fffffff) * oneUp31Bit)
#define BiRandGab1 (MYFLOAT) ((double)     \
    (holdrand[chan] = holdrand[chan] * -214013 + 2531011) * oneUp31Bit)


#include "random.h"
#include "tools.h"

struct TRACK;
struct LFO;

class graingenerator {
public:
    graingenerator(TRACK *track, tsl::AppState*);

    int tick(int chan, int smpl, MYFLOAT& seqgain, MYFLOAT& seqpitch, MYFLOAT& seqsize);

    void prepare();
    void reset();

    void onSampleRateChanged();
    int* buf[2];
	MYFLOAT* pitchBuf[2]{}, * gainBuf[2]{}, * sizeBuf[2]{};
    tsl::parameters::Event getState();
    void setState(tsl::parameters::Event&);

private:
    TRACK *_track;
    int32_t _count[MAX_CHANNELS]{};
    int32_t holdrand[MAX_CHANNELS]{rand(), rand()};
    int32_t _alg{}, _oldalg{-1};
    bool _dodeviationlfo{}, _dodenslfo{};

    LFO *deviation_lfo, *dens_lfo;
    MYFLOAT _prvfreq[MAX_CHANNELS]{}, _prvdev[MAX_CHANNELS]{}, _freq[MAX_CHANNELS]{}, _dev[MAX_CHANNELS]{}, dens_range{}, dens_min{}, devation_range{}, deviation_a{};
    MYFLOAT dx01[2]{}, f0[2]{}, f1[2]{};

    void tickPriv(int chan, int smpl);

    void setFreq(MYFLOAT f, int32_t chan) {
        _prvfreq[chan] = _freq[chan] = f;
    }

    void setDev(MYFLOAT d, int32_t chan) {
        _prvdev[chan] = _dev[chan] = d;
    }


    void setFreq2(MYFLOAT f, int32_t chan) {
        if (_prvfreq[chan] != f) {
           ;// _count[chan] = 0;
        }
        _prvfreq[chan] = _freq[chan] = f;
    }

    void setDev2(MYFLOAT d, int32_t chan) {
        if (_prvdev[chan] != d) {
           ;// _count[chan] = 0;
        }
        _prvdev[chan] = _dev[chan] = d;
    }
    // Every algorithm's period goes through here, so no branch can ask for a
    // grain per sample (a runaway that costs CPU and sounds like noise) or park
    // for good on a poisoned value.
    int32_t clampPeriod(double samples) const {
        if (!(samples > _minPeriod))          // also catches NaN
            return _minPeriod;
        return samples > _maxPeriod ? _maxPeriod : (int32_t)samples;
    }

    // BOUNCE ball state, PER CHANNEL: the two channels tick alternately, so one
    // shared height/direction gave unsynced stereo a trajectory that was neither.
    MYFLOAT _height[MAX_CHANNELS]{5., 5.};
    bool _rising[MAX_CHANNELS]{};

    // CHIMES / FIGURE / GLISS gesture state, PER CHANNEL for the same reason.
    struct ChimeState {
        MYFLOAT wind{0.3};    // current wind level 0..1, updated at k-rate
        int32_t tube{};       // pentatonic tube the clapper hit last
    };
    struct FigureState {
        int32_t shape{1};     // resolved shape this figure (never ANY)
        int32_t left{};       // notes left; <= 0 starts a new figure
        int32_t deg{};        // current pentatonic degree
        int32_t dir{1};
        int32_t upA{2}, dnB{1};  // ZIGZAG lace intervals
        MYFLOAT stepmul{1.};  // running accel product across the figure
        MYFLOAT accel{1.};
        MYFLOAT u{}, du{};    // 0..1 position across the figure, for the fades
    };
    struct GlissState {
        MYFLOAT phase{};      // 0..1 through the sweep
        MYFLOAT from{}, to{}; // WANDER endpoints in semitones
        bool resting{};
    };
    ChimeState _chime[MAX_CHANNELS]{};
    FigureState _fig[MAX_CHANNELS]{};
    GlissState _gliss[MAX_CHANNELS]{};

    MYFLOAT _swingval[2]{1., 1.};
    tsl::random::Spline spline[MAX_CHANNELS];
    int32_t kcount[MAX_CHANNELS]{};
    int32_t nextcount[MAX_CHANNELS]{}, swingstep[MAX_CHANNELS]{};
    Follower follower;
    int32_t _minPeriod{1}, _maxPeriod{1 << 30};
};


#endif //GRAINSTORM_GRAINGENERATOR_H
