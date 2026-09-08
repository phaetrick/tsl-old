//
// Created by phaet on 22.05.2024.
//

#include "Noise.h"
#include <random>
#include "grainstorm.h"

class NoiseBase {
public:
    NoiseBase(MYFLOAT amp = 0.1) {
        std::random_device rd;
        gen = std::mt19937(rd());
        dis = std::uniform_real_distribution<>(-amp, amp);
    }

    virtual MYFLOAT tick() = 0;

protected:
    std::mt19937 gen;
    std::uniform_real_distribution<> dis;

};

class WhiteNoise : public NoiseBase {
public:
    WhiteNoise() : NoiseBase(0.99) {};

    MYFLOAT tick() override {
        return dis(gen);
    }
};


class PinkNoise : public NoiseBase {
public:
    PinkNoise() : NoiseBase(0.1) {
        Reset();
    }

    void Reset() {
        pink_Index = 0;
        pink_IndexMask = (1u << 20u) - 1;
/* Calculate maximum possible signed random value. Extra 1 for white noise always added. */
        /* Initialize rows. */
        for (int i = 0; i < 20; i++) pink_Rows[i] = 0.;
        pink_RunningSum = 0.;
    };

    MYFLOAT tick() override {
/* Increment and mask index. */
        pink_Index = (pink_Index + 1) & pink_IndexMask;

/* If index is zero, don't update any random values. */
        if (pink_Index != 0) {
            /* Determine how many trailing zeros in PinkIndex. */
            /* This algorithm will hang if n==0 so test first. */
            int numZeros = 0;
            uint32_t n = pink_Index;
            while ((n & 1u) == 0) {
                n = n >> 1u;
                numZeros++;
            }

            /* Replace the indexed ROWS random value.
             * Subtract and add back to RunningSum instead of adding all the random
             * values together. Only one changes each time.
             */
            pink_RunningSum -= pink_Rows[numZeros];
            auto newRandom = dis(gen);
            pink_RunningSum += newRandom;
            pink_Rows[numZeros] = newRandom;
        }

/* Add extra white noise value. */
        auto newRandom = dis(gen);
        return pink_RunningSum + dis(gen);
    }

private:
    MYFLOAT pink_Rows[30]{};
    MYFLOAT pink_RunningSum{};   /* Used to optimize summing of generators. */
    uint32_t pink_Index;        /* Incremented each sample. */
    uint32_t pink_IndexMask;    /* Index wrapped by ANDing with this mask. */
};


class BrownNoise : public NoiseBase {
public:
    MYFLOAT tick() override {
        double white = dis(gen);
        // Integrate to get Brown noise
        brown += white;
        brown = brown - 0.02 * brown;
        return brown;
    }

private:
    MYFLOAT brown{};
};


#include <types.h>
#include <app.h>

#include "track.h"

NoiseEffect::NoiseEffect(TRACK *t, int32_t chan, int type) : Effect(t, chan, type == MONOEFFECT
                                                                             ? NOISEMONOEFFECT
                                                                             : NOISEGRAINEFFECT,
                                                                    type),
                                                             noises{std::make_unique<WhiteNoise>(),
                                                                    std::make_unique<PinkNoise>(),
                                                                    std::make_unique<BrownNoise>()} {
    if (type == MONOEFFECT) {
        _noisetype = &t->_appState->params[_track->index][NOISEMONOTYPE];
        _mix = &t->_appState->params[_track->index][NOISEMONOMIX];
        _gain = &t->_appState->params[_track->index][NOISEMONOGAIN];
        _bypass = &t->bypass[NOISEMONOEFFECT];

    } else {
        _noisetype = &t->_appState->params[_track->index][NOISEGRAINTYPE];
        _mix = &t->_appState->params[_track->index][NOISEGRAINMIX];
        _gain = &t->_appState->params[_track->index][NOISEGRAINGAIN];
        _bypass = &t->bypass[NOISEGRAINEFFECT];
    }
}

void NoiseEffect::compute(MYFLOAT *buf, int samples) {
    LFO *lfo = _type == MONOEFFECT ? _track->lfo[NOISEMONOGAIN].load() : nullptr;
    const bool lfoon = lfo && lfo->power();
    MYFLOAT gainlfo = 0, gainlforange = 0.;
    MYFLOAT *lfobuf;
    if (lfoon) {
        MYFLOAT a = _STATE->controls[_track->index][NOISEMONOGAIN].lfo_min.load();
        MYFLOAT b = _STATE->controls[_track->index][NOISEMONOGAIN].lfo_max.load();
        gainlforange = DISTANCE(a, b);
        gainlfo = std::min(a, b);
        lfobuf = lfo->buf;
    }

    auto &noise = noises[static_cast<int>(_noisetype->load())];
    MYFLOAT gain = dbToLinear60(_gain->load()), mix;
    if(_type == GRAINEFFECT){
        gain *= dbToLinear60(_STATE->params[_track->index][PREGAIN].load());
        auto lfograin = _track->lfo[NOISEGRAINGAIN].load();
        const bool lfoon2 = lfograin && lfograin->power();
        if(lfoon2){
            MYFLOAT a = _STATE->controls[_track->index][NOISEGRAINGAIN].lfo_min.load();
            MYFLOAT b = _STATE->controls[_track->index][NOISEGRAINGAIN].lfo_max.load();
            gainlforange = DISTANCE(a, b);
            gainlfo = a;
            if(a>b)gainlforange*=-1.;
            gain *= dbToLinear60(gainlfo + gainlforange * lfograin->buf[static_cast<int>(_track->step_point_grain[_chan].load())]);

        }
    }
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = _mix->load();
    }

    auto fol = _type == MONOEFFECT ? &_STATE->followerMap[_track->index].at(NOISEMONOMIX) : nullptr;

     if (fol && fol->prepare(_chan)) {
         auto src = fol->source.load();
         MYFLOAT *envbuf = src == _track->index ? buf : _DATA->tracks[src]->envf_buffer[_chan];

        for (int i = 0; i < samples; i++) {
            const auto mmix = _chan == 0 ? fol->detectL(envbuf[i]) : fol->detectR(envbuf[i]);
            buf[i] = buf[i] * (1. - mmix) + noise->tick() * mmix * (lfoon ? dbToLinear60(gainlfo + lfobuf[i] * gainlforange) : _smooth2);
            smmixgain(mix, gain);
        }
    } else {
        for (int i = 0; i < samples; i++) {
            buf[i] = buf[i] * (1. - _smooth1) + noise->tick() * _smooth1 * (lfoon ? dbToLinear60(gainlfo + lfobuf[i] * gainlforange) : _smooth2);
            smmixgain(mix, gain);
        }
    }

}

NoiseEffect::~NoiseEffect() {};

