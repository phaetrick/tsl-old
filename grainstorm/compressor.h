#pragma once
#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <cstdint>
#include <atomic>
#include <cmath>
#include <cstring>
#include "types.h"
#include "defines.h"
#include "tools.h"
#include "base.h"
#include "view.h"
#include "DelayBase.h"
#include <app.h>

namespace tsl {
    namespace graphics {
        class CompressorView : public View {
        public:
            CompressorView(tsl::AppState *, float sc, int32_t as, int al);

            void render(void *ctx);
        };
    }
}

template<typename T>
class Rms {
public:
    void init(int32_t max_){
        max = max_;
        buf.resize(max, 0);
    }

    T compute(T smpl) {
        if (size == 0)
            return std::fabs(smpl);
        sum -= buf[index];
        T squared = smpl * smpl;
        buf[index] = squared;
        sum += squared;
        if (sum < 0)
            sum = 0;
        offset++;
        if (offset >= size)
            offset = size;
        index++;
        if (index == size)
            index = 0;
        return std::sqrt(sum / (T) offset);
    }

    void setSize(int32_t size_) {
        if (size_ > max)
            size_ = max;
        size = size_;
        sum =0;
        index = offset = 0;
        std::fill(buf.begin(), buf.begin() + size_, 0);
    }

private:
    std::vector<T> buf;
    T sum{};
    int32_t index{};
    int32_t size{};
    int32_t offset{};
    int32_t max;
};

class MonoCompressor {
public:
    MonoCompressor(MYFLOAT sr);

    static MYFLOAT IR(MYFLOAT y_dB, MYFLOAT threshold, MYFLOAT ratio, MYFLOAT knee, MYFLOAT makeup) {
        MYFLOAT lowknee = threshold - knee / 2;
        MYFLOAT highknee = threshold + knee / 2;
        MYFLOAT y_G;
        // MYFLOAT y_dB = 20 * log10(in);
        MYFLOAT x_G = y_dB;
        MYFLOAT overshoot = x_G - threshold;
        if (x_G <= lowknee)
            return makeup;
        else if ((x_G > lowknee) && (x_G < highknee))
            return  makeup -ratio * powf(overshoot + knee * .5f, 2) / (2 * knee);
        else // if (x_G > Threshold + knee / 2)
            return makeup -ratio * overshoot;
    };

    static MYFLOAT IR3(MYFLOAT y_dB, MYFLOAT threshold, MYFLOAT ratio, MYFLOAT knee, MYFLOAT makeup) {
        MYFLOAT lowknee = threshold - knee / 2;
        MYFLOAT highknee = threshold + knee / 2;
        // MYFLOAT y_dB = 20 * log10(in);
        MYFLOAT x_G = y_dB;
        MYFLOAT undershoot = threshold - x_G;
        MYFLOAT slope = 1.f / (1.f / (1.f + ratio)) - 1.f; //% Feed-forward topology
        MYFLOAT y_G;

        if (x_G >= highknee)
            y_G = x_G;
        else if ((x_G > lowknee) && (x_G < highknee))
            y_G = x_G + slope * powf(undershoot + knee * .5f, 2) / (2 * knee);
        else // if (x_G > Threshold + knee / 2)
        {
            y_G = x_G + slope * undershoot;
        }
        MYFLOAT gain = y_G - x_G;
        gain = makeup - gain;
        // LOGE("y %f gain %f", y_dB, gain);
        return gain;
    };

    static MYFLOAT IR2(MYFLOAT y_dB, MYFLOAT threshold, MYFLOAT ratio, MYFLOAT knee, MYFLOAT makeup) {
        MYFLOAT Threshold = powf(10, threshold * .05f);
        MYFLOAT threshold_log = logf(Threshold);
        MYFLOAT lowClip = Threshold * LOG2NORMALF(-knee);
        MYFLOAT highClip = Threshold * LOG2NORMALF(knee);
        MYFLOAT r = -(1 - 1 / ((.1f + .9f * ratio) * 10.f));
        MYFLOAT log_soft = std::log(LOG2NORMALF(knee));

        MYFLOAT env = powf(10, y_dB * .05f);
        if (env >= highClip) {
            return LOG10D20F(std::exp((std::log(env) - threshold_log) * r));
        }
        if (env >= lowClip) {
            MYFLOAT dif = std::log(env) - threshold_log + log_soft;
            return LOG10D20F(std::exp(dif * dif * r / 4.0f / log_soft));
        }
        return 0;
    }

    void setRMS(long value) {
        rms.setSize(value);
    }

    void setAttack(MYFLOAT value) {
        Attack = value;
        if (Attack > 0)
            attackDelta = expf(-1.0f / Attack);
        else
            attackDelta = 0;
    }

    void setRelease(MYFLOAT value) {
        Release = value;
        if (Release > 0)
            releaseDelta = expf(-1.0f / Release);
        else
            releaseDelta = 0.0;
    }

    virtual void setThreshold(MYFLOAT value) {
        Threshold = value;
        threshold_log = std::log(Threshold);
        update();
    }

    void setThresholddB(MYFLOAT value) {
        ThresholddB = value;
        update();
    }

    virtual void setSoftKnee(MYFLOAT dB) {
        SoftKnee = dB;
        log_soft = std::log(LOG2NORMALF(SoftKnee));
        update();
    }

    virtual void setRatio(MYFLOAT value) {
        Ratio = value;
        r = -(1 - 1 / ((.1f + .9f * Ratio) * 10.f));
        slope = Ratio; //% Feed-forward topology

        // slope = 1.f / (1.f / Ratio) - 1.f; //% Feed-forward topology
    }

    MYFLOAT getEnv() {
        return env;
    }

    void setEnv(MYFLOAT dB) {
        env = dB;
    }

    void setMakeUp(MYFLOAT dB) {
        MakeUp = dB;
        MakeUpR = powf(10, MakeUp * .05f);
    }

    inline MYFLOAT tick2(MYFLOAT input) {
        UDD(input)
        MYFLOAT rmsf = rms.compute(input);
        MYFLOAT theta = rmsf > env ? attackDelta : releaseDelta;
        env = (1.0f - theta) * rmsf + theta * env;
        UDD(env);
        if (env < 0)
            env = 0;
        if (env >= highClip) {
            return std::exp((std::log(env) - threshold_log) * r) * MakeUpR;
        }
        if (env >= lowClip) {
            MYFLOAT dif = std::log(env) - threshold_log + log_soft;
            return std::exp(dif * dif * r / 4.0f / log_soft) * MakeUpR;
        }
        return 1;
    }

    inline MYFLOAT tick(MYFLOAT input) {
        UDD(input);
        const auto tmp = rms.compute(input);
        const MYFLOAT x_G = tmp == 0.0 ? -120 : LOG10D20(tmp);

        MYFLOAT overshoot = x_G - ThresholddB;
        MYFLOAT x_T;
        if (x_G <= lowKnee)
            x_T = 0;
        else if ((x_G > lowKnee) && (x_G < highKnee))
            x_T =  slope * pow(overshoot + SoftKnee * .5, 2.) / (2. * SoftKnee);
        else // if (x_G > Threshold + knee / 2)
            x_T = slope * overshoot;

        if (x_T > env)
            env = attackDelta * env + (1 - attackDelta) * x_T;
        else
            env = releaseDelta * env + (1 - releaseDelta) * x_T;

        UDD(env);

        return LOG2NORMALF(MakeUp - env);
    }

    inline MYFLOAT tick3(MYFLOAT input) {
        UDD(input);
        const MYFLOAT x_G = LOG10D20F(rms.compute(input));

        MYFLOAT undershoot = ThresholddB - x_G;
        MYFLOAT y_G;
        if (x_G >= highKnee)
            y_G = x_G;
        else if (x_G > lowKnee && x_G < highKnee)
            y_G = x_G + slope * powf(undershoot + SoftKnee * .5, 2) / (2 * SoftKnee);
        else // if (x_G > Threshold + knee / 2)
        {
            y_G = x_G + (1. - slope) * undershoot;
        }
        MYFLOAT x_T = y_G - x_G;

        if (x_T > env)
            env = attackDelta * env + (1 - attackDelta) * x_T;
        else
            env = releaseDelta * env + (1 - releaseDelta) * x_T;

        UDD(env);

        return LOG2NORMALF(MakeUp - env);
    }


private:
    void update() {
        lowClip = Threshold * LOG2NORMALF(-SoftKnee);
        highClip = Threshold * LOG2NORMALF(SoftKnee);
        lowKnee = ThresholddB - SoftKnee * .5f;
        highKnee = ThresholddB + SoftKnee * .5f;
    }

    MYFLOAT Attack, Release, Threshold, ThresholddB, threshold_log, MakeUp, MakeUpR;
    MYFLOAT attackDelta, releaseDelta, env, Ratio, r, slope;
    MYFLOAT SoftKnee, log_soft, lowClip, highClip, lowKnee, highKnee;
    Rms<MYFLOAT> rms;
    int32_t runs = 0;
};

class compfullstereo : public Effect {
public:
    compfullstereo(TRACK *track);

    ~compfullstereo() {
        *currentgain = 0;
    }

    void compute(MYFLOAT *inputL, MYFLOAT *inputR, MYFLOAT *outputL, MYFLOAT *outputR, int32_t s) override {

        if (*attack != prv_attack) {
            prv_attack = *attack;
            setAttack(*attack);
        }
        if (*release != prv_release) {
            prv_release = *release;
            setRelease(*release);
        }
        if (*ratio != prv_ratio) {
            prv_ratio = *ratio;
            setRatio(*ratio);
        }
        if (*threshold != prv_threshold) {
            prv_threshold = *threshold;
            setThreshold(*threshold);
        }
        if (*lookahead != prv_lookahead) {
            prv_lookahead = *lookahead;
            setLookahead(*lookahead);
        }
        if (*rmssize != prv_rmssize) {
            prv_rmssize = *rmssize;
            setRMS(*rmssize);
        }
        if (*makeup != prv_makeup) {
            prv_makeup = *makeup;
            setMakeUp(*makeup);
        }
        if (*knee != prv_knee) {
            prv_knee = *knee;
            setSoftKnee(*knee);
        }

        const MYFLOAT mix = (*_bypass || destroyRequested) ? 0. : 1.;

        for (long i = 0; i < s; i++) {
            MYFLOAT gainL = compL.tick(inputL[i]);
            MYFLOAT gainR = compR.tick(inputR[i]);
            if (gainL > gainR)
                currentGain = gainR;
            else
                currentGain = gainL;
            const MYFLOAT mm = _smooth1 >= 0.999 ? 1. : _smooth1;
            const MYFLOAT mixsrc = 1.f - mm;
            outputL[i] = inputL[i] * mixsrc + lookaL.tick(inputL[i]) * currentGain * mm;
            outputR[i] = inputR[i] * mixsrc + lookaR.tick(inputR[i]) * currentGain * mm;
            sm1(mix);
        }

        *currentgain = getEnv();
    }

    void setRMS(MYFLOAT msec);

    MYFLOAT getRMS() const;

    void setLookahead(MYFLOAT msec);

    MYFLOAT getLookahead() const;

    void setAttack(MYFLOAT msec);

    MYFLOAT getAttack() const;

    void setRelease(MYFLOAT msec);

    MYFLOAT getRelease() const;

    virtual void setThreshold(MYFLOAT dB);

    MYFLOAT getThreshold() const;

    virtual void setSoftKnee(MYFLOAT dB);

    MYFLOAT getSoftKnee() const;

    virtual void setRatio(MYFLOAT value);

    MYFLOAT getEnvL() {
        return compL.getEnv();
    };

    MYFLOAT getEnvR() {
        return compR.getEnv();
    };

    MYFLOAT getCurrentGain() {
        return currentGain;
    }

    MYFLOAT getEnv() {
        return std::max(compL.getEnv(), compR.getEnv());
    }

    void setCurrentGain(MYFLOAT gain) {
        currentGain = gain;
    }

    void setMakeUp(MYFLOAT val);

    std::atomic<MYFLOAT> *attack, *release, *threshold, *ratio, *lookahead, *rmssize, *makeup, *knee, *currentgain;
    MYFLOAT prv_attack, prv_release, prv_threshold, prv_ratio, prv_lookahead, prv_rmssize, prv_makeup, prv_knee;

private:
    compfullstereo(const compfullstereo &x);

    compfullstereo &operator=(const compfullstereo &x);

    MYFLOAT currentfs, RMS, Lookahead, Attack, Release, Threshold, Ratio, SoftKnee;
    MonoCompressor compL, compR;
    SimpleDelay2<MYFLOAT> lookaL, lookaR;
    MYFLOAT currentGain;
    MYFLOAT makeUp;
};

class compfullmono : public Effect {
public:
    compfullmono(TRACK *track, int32_t chan);

    ~compfullmono() {
        *currentgain = 0;
    }

    void setSampleRate(MYFLOAT fs);

    MYFLOAT getSampleRate() const;

    void setRMS(MYFLOAT msec);

    MYFLOAT getRMS() const;

    void setLookahead(MYFLOAT msec);

    MYFLOAT getLookahead() const;

    void setAttack(MYFLOAT msec);

    MYFLOAT getAttack() const;

    void setRelease(MYFLOAT msec);

    MYFLOAT getRelease() const;

    virtual void setThreshold(MYFLOAT dB);

    virtual void setSoftKnee(MYFLOAT dB);

    virtual void setRatio(MYFLOAT value);

    void mute();

    void process(MYFLOAT *input, MYFLOAT *dest, long numsamples) {
        const MYFLOAT mix = (*_bypass || destroyRequested) ? 0.f : 1.f;
        for (long i = 0; i < numsamples; i++) {
            const MYFLOAT gain = comp.tick(input[i]);
            const MYFLOAT mm = _smooth1 >= 0.999 ? 1. : _smooth1;
            dest[i] = input[i] * (1.f - mm) + looka.tick(dest[i]) * gain * mm;
            sm1(mix);
        }
    }

    void compute(MYFLOAT *in, int32_t size);

    MYFLOAT getEnv() {
        return comp.getEnv();
    };

    void setEnv(MYFLOAT dB) {
        comp.setEnv(dB);
    };

    MYFLOAT getCurrentGain() {
        return currentGain;
    }

    void setCurrentGain(MYFLOAT gain) {
        currentGain = gain;
    }

    void setMakeUp(MYFLOAT val);

    std::atomic<MYFLOAT> *attack, *release, *threshold, *ratio, *lookahead, *rmssize, *makeup, *knee, *currentgain;

private:
    compfullmono(const compfullmono &x);

    compfullmono &operator=(const compfullmono &x);

    MYFLOAT RMS{}, Lookahead{}, Attack{}, Release{}, Threshold{}, Ratio{}, SoftKnee{};
    MonoCompressor comp;
    SimpleDelay2<MYFLOAT> looka;
    MYFLOAT currentGain{};

    MYFLOAT prv_attack, prv_release, prv_threshold, prv_ratio, prv_lookahead, prv_rmssize, prv_makeup, prv_knee;
};

void render_ir_window(tsl::graphics::View *view, void *context);



#endif