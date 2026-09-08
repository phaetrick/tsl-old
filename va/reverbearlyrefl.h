//
// Created by pr on 31.08.20.
//

#ifndef GRAINSTORM_REVERBEARLYREFL_H
#define GRAINSTORM_REVERBEARLYREFL_H

#include <cmath>

#include "Biquad.h"
#include "DelayBase.h"

#define EARLYREF_PRESET_DEFAULT 0
#define EARLYREF_PRESET_0 0
#define EARLYREF_PRESET_1 1
#define EARLYREF_PRESET_2 2

#define EARLYREF_PRESET_11 11
#define EARLYREF_PRESET_12 12
#define EARLYREF_PRESET_13 13
#define EARLYREF_PRESET_14 14
#define EARLYREF_PRESET_15 15
#define EARLYREF_PRESET_16 16
#define EARLYREF_PRESET_17 17
#define EARLYREF_PRESET_18 18
#define EARLYREF_PRESET_19 19
#define EARLYREF_PRESET_20 20
#define EARLYREF_PRESET_21 21
#define EARLYREF_PRESET_22 22

template<typename T>
class earlyrefl {
public:
    earlyrefl(T sr){
        currentfs = sr;
        primeMode = true;
        muteOnChange = false;
        setPreDelay(0);
        tapLengthL = tapLengthR = 0;
        this->setdryr(0.8);
        this->setwetr(0.5);
        this->setwidth(0.2);
        setLRDelay(0.3);
        setLRCrossApFreq(750, 4);
        setDiffusionApFreq(150, 4);
        loadPresetReflection(EARLYREF_PRESET_1);
        setoutputlpf(20000);
        setoutputhpf(4);
        mute();
    }

    ~earlyrefl() {
        unloadReflection();
    }


    void setInitialDelay(long numsamples) {
        initialDelay = numsamples;
        preDelay = (T) initialDelay * 1000.0f / (currentfs);
        if (initialDelay >= 0) {
            delayL.setsize(0);
            delayR.setsize(0);
            delayWL.setsize(initialDelay);
            delayWR.setsize(initialDelay);
        } else {
            long dryD = -initialDelay;
            delayL.setsize(dryD);
            delayR.setsize(dryD);
            delayWL.setsize(0);
            delayWR.setsize(0);
        }
    }

    long getInitialDelay() {
        return initialDelay;
    }

    void setPreDelay(T value_ms) {
        setInitialDelay((long) (currentfs * (preDelay = value_ms) / 1000.));
    }

    T getPreDelay() {
        return preDelay;
    }

    long getLatency() {
        return 0;
    }

    void printconfig() {
        LOGE("*** revbase config ***\n");
        LOGE("Fs = %f[Hz] X %ld\n", currentfs, 1);
        LOGE("Wet %f Dry %f Width %f\n", (double) wet, (double) dry,
             (double) width);
    }

    void setwet(T value) {
        wetDB = value;
        wet = LOG2NORMAL(value);
        update_wet();

    }

    T getwet() {
        return wetDB;
    }

    void setwetr(T value) {
        if (value == 0) {
            wet = value;
            wetDB = FP_NAN;
        } else {
            wetDB = LOG10D20(wet = value);
        }
        update_wet();
    }

    T getwetr() {
        return wet;
    }

    void setdry(T value) {
        dryDB = value;
        dry = LOG2NORMAL(value);
    }

    T getdry() {
        return dryDB;
    }

    void setdryr(T value) {
        if (value == 0) {
            dry = value;
            dryDB = FP_NAN;
        } else {
            dryDB = LOG10D20(dry = value);
        }
    }

    T getdryr() {
        return dry;
    }

    void setwidth(T value) {
        width = value;
        update_wet();
    }

    T getwidth() {
        return width;
    }

    void setPrimeMode(bool value) {
        primeMode = value;
    }

    bool getPrimeMode() {
        return primeMode;
    }

    void setMuteOnChange(bool value) {
        muteOnChange = value;
    }

    bool getMuteOnChange() {
        return muteOnChange;
    }

    void mute() {
        delayL.mute();
        delayR.mute();
        delayWL.mute();
        delayWR.mute();
        delayLineL.mute();
        delayLineR.mute();
        delayLtoR.mute();
        delayRtoL.mute();
        allpassXL.mute();
        allpassXR.mute();
        allpassL2.mute();
        allpassR2.mute();
    }

    void loadPresetReflection(long program) {
        switch (program) {
            case EARLYREF_PRESET_11:
                currentPreset = EARLYREF_PRESET_11;
                loadReflection(preset11_delayL, preset11_gainL, preset11_delayR, preset11_gainR,
                               preset11_sizeL, preset11_sizeR);
                break;
            case EARLYREF_PRESET_12:
                currentPreset = EARLYREF_PRESET_12;
                loadReflection(preset12_delayL, preset12_gainL, preset12_delayR, preset12_gainR,
                               preset12_sizeL, preset12_sizeR);
                break;
            case EARLYREF_PRESET_13:
                currentPreset = EARLYREF_PRESET_13;
                loadReflection(preset13_delayL, preset13_gainL, preset13_delayR, preset13_gainR,
                               preset13_sizeL, preset13_sizeR);
                break;
            case EARLYREF_PRESET_14:
                currentPreset = EARLYREF_PRESET_14;
                loadReflection(preset14_delayL, preset14_gainL, preset14_delayR, preset14_gainR,
                               preset14_sizeL, preset14_sizeR);
                break;
            case EARLYREF_PRESET_15:
                currentPreset = EARLYREF_PRESET_15;
                loadReflection(preset15_delayL, preset15_gainL, preset15_delayR, preset15_gainR,
                               preset15_sizeL, preset15_sizeR);
                break;
            case EARLYREF_PRESET_16:
                currentPreset = EARLYREF_PRESET_16;
                loadReflection(preset16_delayL, preset16_gainL, preset16_delayR, preset16_gainR,
                               preset16_sizeL, preset16_sizeR);
                break;
            case EARLYREF_PRESET_17:
                currentPreset = EARLYREF_PRESET_17;
                loadReflection(preset17_delayL, preset17_gainL, preset17_delayR, preset17_gainR,
                               preset17_sizeL, preset17_sizeR);
                break;
            case EARLYREF_PRESET_18:
                currentPreset = EARLYREF_PRESET_18;
                loadReflection(preset18_delayL, preset18_gainL, preset18_delayR, preset18_gainR,
                               preset18_sizeL, preset18_sizeR);
                break;
            case EARLYREF_PRESET_19:
                currentPreset = EARLYREF_PRESET_19;
                loadReflection(preset19_delayL, preset19_gainL, preset19_delayR, preset19_gainR,
                               preset19_sizeL, preset19_sizeR);
                break;
            case EARLYREF_PRESET_20:
                currentPreset = EARLYREF_PRESET_20;
                loadReflection(preset20_delayL, preset20_gainL, preset20_delayR, preset20_gainR,
                               preset20_sizeL, preset20_sizeR);
                break;
            case EARLYREF_PRESET_21:
                currentPreset = EARLYREF_PRESET_21;
                loadReflection(preset21_delayL, preset21_gainL, preset21_delayR, preset21_gainR,
                               preset21_sizeL, preset21_sizeR);
                break;
            case EARLYREF_PRESET_22:
                currentPreset = EARLYREF_PRESET_22;
                loadReflection(preset22_delayL, preset22_gainL, preset22_delayR, preset22_gainR,
                               preset22_sizeL, preset22_sizeR);
                break;

            case EARLYREF_PRESET_1:
                currentPreset = EARLYREF_PRESET_1;
                loadReflection(preset1_delayL, preset1_gainL, preset1_delayR, preset1_gainR,
                               preset1_size, preset1_size);
                break;
            case EARLYREF_PRESET_2:
                currentPreset = EARLYREF_PRESET_2;
                loadReflection(preset2_delayL, preset2_gainL, preset2_delayR, preset2_gainR,
                               preset2_size, preset2_size);
                break;

            case EARLYREF_PRESET_0:
            default:
                currentPreset = EARLYREF_PRESET_0;
                loadReflection(preset0_delayL, preset0_gainL, preset0_delayR, preset0_gainR,
                               preset0_size, preset0_size);
                break;
        }
    }

    long getCurrentPreset() {
        return currentPreset;
    }

    void
    loadReflection(const float *delayL, const float *gainL, const float *delayR, const float *gainR,
                   long sizeL, long sizeR) {
        unloadReflection();
        tapLengthL = sizeL;
        tapLengthR = sizeR;
        for (long i = 0; i < sizeL; i++) {
            gainTableL[i] = gainL[i];
            delayTableL[i] = currentfs * delayL[i];
        }
        for (long i = 0; i < sizeR; i++) {
            gainTableR[i] = gainR[i];
            delayTableR[i] = currentfs * delayR[i];
        }
        long maxLengthL = (long) (maxDelay(delayTableL, tapLengthL) + 10);
        long maxLengthR = (long) (maxDelay(delayTableR, tapLengthR) + 10);
        delayLineL.setsize(maxLengthL);
        delayLineR.setsize(maxLengthR);
        mute();
    }


    void unloadReflection() {
        tapLengthL = tapLengthR = 0;
    }

    void processreplace(T *inputL, T *inputR, T *outputL, T *outputR, long numsamples) {
        if (numsamples <= 0) return;

        while (numsamples-- > 0) {
            *outputL = this->delayL(*inputL) * this->dry;
            *outputR = this->delayR(*inputR) * this->dry;
            T wetL = 0, wetR = 0;
            delayLineL.process(*inputL);
            delayLineR.process(*inputR);
            for (int i = 0; i < tapLengthL; i++) {
                wetL += gainTableL[i] * delayLineL.at(delayTableL[i]);
            }
            for (int i = 0; i < tapLengthR; i++) {
                wetR += gainTableR[i] * delayLineR.at(delayTableR[i]);
            }
            // width = -1 ~ +1
            wetL = this->delayWL(wetL);
            wetR = this->delayWR(wetR);
            *outputL += out1_lpf(
                    out1_hpf(allpassL2(this->wet1 * wetL +
                                       this->wet2 * allpassXL(delayRtoL(*inputR + wetR)))));
            *outputR += out2_lpf(
                    out2_hpf(allpassR2(this->wet1 * wetR +
                                       this->wet2 * allpassXR(delayLtoR(*inputL + wetL)))));
            inputL++;
            inputR++;
            outputL++;
            outputR++;
        }
    }

    inline void tick(T inl, T inr, T *outl, T *outr) {
        *outl = inl * this->dry; //this->delayL(inl) * this->dry;
        *outr = inr * this->dry;//this->delayR(inr) * this->dry;
        T wetL = 0, wetR = 0;
        delayLineL.process(inl);
        delayLineR.process(inr);
        for (int i = 0; i < tapLengthL; i++) {
            wetL += gainTableL[i] * delayLineL.at(delayTableL[i]);
        }
        for (int i = 0; i < tapLengthR; i++) {
            wetR += gainTableR[i] * delayLineR.at(delayTableR[i]);
        }
        // width = -1 ~ +1
        //wetL = this->delayWL(wetL);
        //wetR = this->delayWR(wetR);
        *outl += out1_lpf(
                out1_hpf(allpassL2(
                        wet1 * wetL + wet2 * allpassXL(delayRtoL(inr + wetR)))));
        *outr += out2_lpf(
                out2_hpf(allpassR2(
                        wet1 * wetR + wet2 * allpassXR(delayLtoR(inl + wetL)))));
    }


    void setLRDelay(T value_ms) {
        lrDelay = (long) ((T) this->currentfs * value_ms / 1000.0f);
        delayRtoL.setsize(lrDelay);
        delayLtoR.setsize(lrDelay);
    }

    T getLRDelay() {
        return (T) lrDelay * 1000.0f / (T) this->currentfs;
    }

    void setLRCrossApFreq(T fc, T bw) {
        lrCrossApFq = fc, lrCrossApBw = bw;
        allpassXL.setAPF_RBJ(fc, bw, this->currentfs, BIQUAD_RBJ_BW);
        allpassXR.setAPF_RBJ(fc, bw, this->currentfs, BIQUAD_RBJ_BW);
    }

    T getLRCrossApFreq() {
        return lrCrossApFq;
    }

    void setDiffusionApFreq(T fc, T bw) {
        diffApFq = fc, diffApBw = bw;
        allpassL2.setAPF_RBJ(fc, bw, this->currentfs, BIQUAD_RBJ_BW);
        allpassR2.setAPF_RBJ(fc, bw, this->currentfs, BIQUAD_RBJ_BW);
    }

    T getDiffusionApFreq() {
        return diffApFq;
    }

    void setoutputlpf(T value) {
        if (value < 0) value = 0;
        if (value > this->currentfs / 2) value = this->currentfs / 2;
        outputlpf = value;
        out1_lpf.setLPF_BW(outputlpf, this->currentfs);
        out2_lpf.setLPF_BW(outputlpf, this->currentfs);
    }

    T getoutputlpf() {
        return outputlpf;
    }

    void setoutputhpf(T value) {
        if (value < 0) value = 0;
        if (value > this->currentfs / 2) value = this->currentfs / 2;
        outputhpf = value;
        out1_hpf.setHPF_BW(outputhpf, this->currentfs);
        out2_hpf.setHPF_BW(outputhpf, this->currentfs);
    }

    T getoutputhpf() {
        return outputhpf;
    }

    void setFsFactors() {
        setLRDelay(0.2);
        setLRCrossApFreq(lrCrossApFq, lrCrossApBw);
        setDiffusionApFreq(diffApFq, diffApBw);
        loadPresetReflection(currentPreset);
    }

private:
    long initialDelay;
    delay<T> delayL, delayR, delayWL, delayWR;
    T currentfs, preDelay, wetDB, wet, wet1, wet2, dryDB, dry, width;

    void update_wet() {
        wet1 = wet * (width / 2 + 0.5f);
        wet2 = wet * ((1 - width) / 2);
    }


    T limFs2(T fq) {
        if (fq < 0) fq = 0;
        if (fq > currentfs() / 2) fq = currentfs() / 2;
        return fq;
    }

    long f_(long def, T factor) { return f_((T) def, factor); }

    long f_(T def, T factor) {
        long fact = (long) (def * factor);
        if (fact <= 0) fact = 1;
        return fact;
    }

    long p_(long def, T factor) { return p_((T) def, factor); }

    long p_(T def, T factor) {
        long base = f_(def, factor);
        if (primeMode) { while (!isPrime(base)) base++; }
        return base;
    }


    bool primeMode, muteOnChange;
    delayline<T> delayLineL, delayLineR;
    delay<T> delayLtoR, delayRtoL;
    biquad<T> allpassXL, allpassL2, allpassXR, allpassR2;
    iir_1st<T> out1_lpf, out2_lpf, out1_hpf, out2_hpf;
    long currentPreset, tapLengthL, tapLengthR, lrDelay;
    T lrCrossApFq, lrCrossApBw, diffApFq, diffApBw, outputlpf, outputhpf;
    T gainTableL[18], gainTableR[18], delayTableL[18], delayTableR[18];


    T maxDelay(const T *delaySet, long size) {
        T max = 0;
        for (long i = 0; i < size; i++) {
            if (max < delaySet[i]) max = delaySet[i];
        }
        return max;
    }


    static constexpr long preset0_size = 18;
    static constexpr float  preset0_delayL[] = {0.0043, 0.0215, 0.0225, 0.0268, 0.0270, 0.0298, 0.0458,
                                                0.0485, 0.0572, 0.0587, 0.0595, 0.0612, 0.0707, 0.0708,
                                                0.0726, 0.0741, 0.0753, 0.0797,};
    static constexpr float preset0_delayR[] = {0.0053, 0.0225, 0.0235, 0.0278, 0.0290, 0.0288, 0.0468,
                                               0.0475, 0.0582, 0.0577, 0.0575, 0.0622, 0.0697, 0.0718,
                                               0.0736, 0.0751, 0.0763, 0.0817,};
    static constexpr float preset0_gainL[] = {0.841, 0.504, 0.491, 0.379, 0.380, 0.346, 0.289, 0.272,
                                              0.192, 0.193, 0.217, 0.181, 0.180, 0.181, 0.176, 0.142,
                                              0.167, 0.134,};
    static constexpr float preset0_gainR[] = {0.842, 0.506, 0.489, 0.382, 0.300, 0.346, 0.290, 0.271,
                                              0.193, 0.192, 0.217, 0.195, 0.192, 0.166, 0.186, 0.131,
                                              0.168, 0.133,};

    static constexpr long preset1_size = 6;
    static constexpr float preset1_delayL[] = {0.0199, 0.0354, 0.0389, 0.0414, 0.0699, 0.0796,};
    static constexpr float preset1_delayR[] = {0.0209, 0.0364, 0.0399, 0.0424, 0.0709, 0.0806,};
    static constexpr float preset1_gainL[] = {1.020, 0.818, 0.635, 0.719, 0.267, 0.242,};
    static constexpr float preset1_gainR[] = {1.021, 0.820, 0.633, 0.722, 0.187, 0.243,};

    static constexpr long preset2_size = 4;
    static constexpr float preset2_delayL[] = {0.0090, 0.0118, 0.0205, 0.0213,};
    static constexpr float preset2_delayR[] = {0.0098, 0.0145, 0.0203, 0.0230,};
    static constexpr float preset2_gainL[] = {1.35, -1.15, 1.15, -1.14,};
    static constexpr float preset2_gainR[] = {1.36, -1.16, -1.00, 1.14,};

    static constexpr long preset11_sizeL = 11;
    static constexpr long preset11_sizeR = 11;
    static constexpr float preset11_delayL[] = {0.003568, 0.011703, 0.019526, 0.024870, 0.037740,
                                                0.048089, 0.053948, 0.061333, 0.061344, 0.070073,
                                                0.077130,};
    static constexpr float preset11_gainL[] = {0.963333, -0.806667, 0.706667, -0.656667, -0.556667,
                                               0.526667, -0.506667, 0.503333, 0.450000, -0.470000,
                                               0.456667,};
    static constexpr float preset11_delayR[] = {0.002818, 0.009115, 0.017042, 0.023885, 0.033068,
                                                0.042307, 0.051234, 0.059896, 0.067984, 0.067995,
                                                0.076458,};
    static constexpr float preset11_gainR[] = {0.980000, -0.850000, 0.726667, -0.660000, 0.583333,
                                               0.553333, -0.503333, 0.486667, -0.473333, -0.466667,
                                               0.463333,};

    static constexpr long preset12_sizeL = 12;
    static constexpr long preset12_sizeR = 12;
    static constexpr float preset12_delayL[] = {0.006344, 0.012286, 0.023385, 0.028495, 0.036385,
                                                0.045750, 0.053427, 0.059266, 0.063281, 0.063292,
                                                0.066437, 0.066448,};
    static constexpr float preset12_gainL[] = {0.896667, 0.796667, -0.653333, 0.623333, -0.570000,
                                               0.530000, -0.496667, 0.483333, 0.480000, 0.406667,
                                               -0.470000, -0.470000,};
    static constexpr float preset12_delayR[] = {0.004568, 0.011266, 0.015687, 0.030203, 0.030214,
                                                0.038740, 0.048172, 0.054073, 0.058870, 0.062417,
                                                0.064510, 0.064521,};
    static constexpr float preset12_gainR[] = {0.946667, 0.823333, -0.753333, 0.593333, 0.600000,
                                               0.556667, 0.530000, -0.516667, 0.490000, -0.480000,
                                               0.476667, 0.410000,};

    static constexpr long preset13_sizeL = 10;
    static constexpr long preset13_sizeR = 12;
    static constexpr float preset13_delayL[] = {0.003568, 0.011703, 0.019526, 0.024870, 0.037740,
                                                0.048089, 0.053948, 0.061333, 0.070073, 0.077125,};
    static constexpr float preset13_gainL[] = {0.963333, -0.813333, 0.700000, -0.650000, -0.560000,
                                               0.526667, -0.500000, 0.486667, -0.463333, 0.463333,};
    static constexpr float preset13_delayR[] = {0.002818, 0.009115, 0.017042, 0.023885, 0.033062,
                                                0.033073, 0.033120, 0.042307, 0.051234, 0.059901,
                                                0.067990, 0.076458,};
    static constexpr float preset13_gainR[] = {0.980000, -0.856667, 0.723333, -0.660000, 0.580000,
                                               0.580000, 0.166667, 0.550000, -0.506667, 0.476667,
                                               -0.466667, 0.460000,};

    static constexpr long preset14_sizeL = 11;
    static constexpr long preset14_sizeR = 13;
    static constexpr float preset14_delayL[] = {0.002062, 0.008031, 0.023010, 0.042016, 0.055766,
                                                0.059432, 0.064536, 0.071240, 0.076490, 0.079250,
                                                0.079292,};
    static constexpr float preset14_gainL[] = {0.993333, -0.873333, 0.663333, -0.543333, 0.503333,
                                               -0.486667, 0.480000, -0.466667, 0.466667, -0.463333,
                                               -0.186667,};
    static constexpr float preset14_delayR[] = {0.001396, 0.009427, 0.023260, 0.039740, 0.039786,
                                                0.052427, 0.052479, 0.053552, 0.062042, 0.067380,
                                                0.067391, 0.073630, 0.076490,};
    static constexpr float preset14_gainR[] = {1.013333, 0.833333, -0.660000, 0.546667, 0.190000,
                                               0.506667, 0.150000, -0.506667, 0.493333, 0.473333,
                                               0.473333, -0.463333, 0.473333,};

    static constexpr long preset15_sizeL = 11;
    static constexpr long preset15_sizeR = 10;
    static constexpr float preset15_delayL[] = {0.009531, 0.014042, 0.028557, 0.037885, 0.043745,
                                                0.051255, 0.057661, 0.066589, 0.075375, 0.084964,
                                                0.084974,};
    static constexpr float preset15_gainL[] = {-0.636667, 1.013333, 0.770000, 0.866667, 0.983333,
                                               0.700000, 0.786667, 0.763333, 0.796667, 0.680000,
                                               0.690000,};
    static constexpr float preset15_delayR[] = {0.006198, 0.016792, 0.022510, 0.033880, 0.048922,
                                                0.056766, 0.067510, 0.074745, 0.080651, 0.083865,};
    static constexpr float preset15_gainR[] = {1.006667, 0.736667, 0.973333, 0.760000, 0.713333,
                                               0.963333, 0.980000, 0.920000, -0.670000, 0.870000,};

    static constexpr long preset16_sizeL = 12;
    static constexpr long preset16_sizeR = 11;
    static constexpr float preset16_delayL[] = {0.003021, 0.008531, 0.010703, 0.012203, 0.014682,
                                                0.018547, 0.018604, 0.025391, 0.034026, 0.038948,
                                                0.047807, 0.047818,};
    static constexpr float preset16_gainL[] = {0.823333, 0.773333, -0.810000, -0.873333, 0.770000,
                                               0.723333, 0.166667, -0.766667, -0.753333, 0.656667,
                                               0.533333, 0.533333,};
    static constexpr float preset16_delayR[] = {0.002526, 0.005193, 0.005255, 0.006682, 0.013500,
                                                0.016937, 0.020109, 0.022115, 0.028370, 0.036130,
                                                0.043328,};
    static constexpr float preset16_gainR[] = {0.963333, 0.883333, 0.170000, 0.736667, -0.870000,
                                               -0.790000, -0.766667, -0.763333, 0.750000, 0.653333,
                                               0.520000,};

    static constexpr long preset17_sizeL = 11;
    static constexpr long preset17_sizeR = 11;
    static constexpr float preset17_delayL[] = {0.002964, 0.003031, 0.009786, 0.010870, 0.018380,
                                                0.019984, 0.027745, 0.043411, 0.048651, 0.053406,
                                                0.058391,};
    static constexpr float preset17_gainL[] = {0.913333, 0.143333, 0.626667, 0.610000, 0.800000,
                                               0.643333, 0.773333, -0.773333, 0.630000, -0.806667,
                                               -0.683333,};
    static constexpr float preset17_delayR[] = {0.004193, 0.009203, 0.010198, 0.010208, 0.010266,
                                                0.015708, 0.016917, 0.030208, 0.036193, 0.042406,
                                                0.048135,};
    static constexpr float preset17_gainR[] = {0.976667, 0.726667, 0.716667, 0.716667, 0.143333,
                                               -0.683333, -0.730000, -0.893333, 0.770000, -0.856667,
                                               -0.850000,};

    static constexpr long preset18_sizeL = 11;
    static constexpr long preset18_sizeR = 13;
    static constexpr float preset18_delayL[] = {0.004693, 0.009786, 0.012036, 0.015625, 0.019521,
                                                0.019531, 0.024615, 0.032896, 0.040786, 0.046208,
                                                0.050464,};
    static constexpr float preset18_gainL[] = {0.810000, 0.853333, -0.733333, 1.023333, 0.690000,
                                               0.683333, 0.906667, -0.903333, 0.666667, 0.726667,
                                               0.660000,};
    static constexpr float preset18_delayR[] = {0.004026, 0.009510, 0.014000, 0.015312, 0.019276,
                                                0.024344, 0.024406, 0.031292, 0.031354, 0.042391,
                                                0.045625, 0.045693, 0.050609,};
    static constexpr float preset18_gainR[] = {0.883333, 0.803333, -0.876667, 0.823333, 0.836667,
                                               0.853333, 0.166667, -0.903333, -0.173333, 0.783333,
                                               0.880000, 0.140000, 0.790000,};

    static constexpr long preset19_sizeL = 10;
    static constexpr long preset19_sizeR = 11;
    static constexpr float preset19_delayL[] = {0.006031, 0.019214, 0.032687, 0.041682, 0.047917,
                                                0.055474, 0.059266, 0.064068, 0.068786, 0.075062,};
    static constexpr float preset19_gainL[] = {0.933333, 0.833333, 0.793333, 0.783333, 0.840000,
                                               -0.793333, 0.860000, 0.716667, -0.810000, 0.733333,};
    static constexpr float preset19_delayR[] = {0.007031, 0.012599, 0.025599, 0.034943, 0.044250,
                                                0.050255, 0.062771, 0.066646, 0.066656, 0.071807,
                                                0.078354,};
    static constexpr float preset19_gainR[] = {0.706667, 0.873333, 0.846667, 0.846667, -0.836667,
                                               0.826667, -0.860000, 0.780000, 0.776667, -0.890000,
                                               -0.836667,};

    static constexpr long preset20_sizeL = 12;
    static constexpr long preset20_sizeR = 11;
    static constexpr float preset20_delayL[] = {0.004292, 0.013469, 0.023667, 0.029073, 0.033005,
                                                0.044031, 0.053677, 0.060505, 0.071667, 0.071677,
                                                0.079833, 0.079844,};
    static constexpr float preset20_gainL[] = {0.950000, -0.786667, 0.680000, 0.620000, -0.556667,
                                               0.520000, 0.500000, -0.476667, 0.466667, 0.470000,
                                               -0.450000, -0.456667,};
    static constexpr float preset20_delayR[] = {0.007917, 0.014245, 0.026464, 0.030865, 0.041781,
                                                0.052109, 0.057672, 0.065104, 0.074745, 0.082375,
                                                0.082385,};
    static constexpr float preset20_gainR[] = {0.863333, 0.736667, -0.643333, -0.593333, 0.536667,
                                               0.523333, -0.483333, 0.486667, -0.463333, 0.450000,
                                               0.453333,};

    static constexpr long preset21_sizeL = 11;
    static constexpr long preset21_sizeR = 11;
    static constexpr float preset21_delayL[] = {0.004474, 0.014313, 0.023130, 0.030646, 0.038906,
                                                0.048729, 0.056156, 0.064510, 0.064521, 0.072859,
                                                0.081219,};
    static constexpr float preset21_gainL[] = {0.950000, 0.756667, 0.680000, 0.616667, 0.563333,
                                               0.523333, 0.493333, 0.493333, 0.446667, 0.460000,
                                               0.456667,};
    static constexpr float preset21_delayR[] = {0.007729, 0.007797, 0.017010, 0.025641, 0.033901,
                                                0.040318, 0.051432, 0.058495, 0.066859, 0.075224,
                                                0.085417,};
    static constexpr float preset21_gainR[] = {0.893333, 0.146667, 0.733333, 0.656667, 0.576667,
                                               0.543333, 0.510000, 0.503333, 0.466667, 0.470000,
                                               0.480000,};

    static constexpr long preset22_sizeL = 10;
    static constexpr long preset22_sizeR = 10;
    static constexpr float preset22_delayL[] = {0.003276, 0.010714, 0.019526, 0.024656, 0.033474,
                                                0.037687, 0.044661, 0.052552, 0.052599, 0.069260,};
    static constexpr float preset22_gainL[] = {0.963333, 0.810000, 0.720000, 0.640000, 0.596667,
                                               0.540000, 0.520000, 0.493333, 0.173333, 0.473333,};
    static constexpr float preset22_delayR[] = {0.003276, 0.010714, 0.019526, 0.024656, 0.033474,
                                                0.037682, 0.044661, 0.052552, 0.058604, 0.069266,};
    static constexpr float preset22_gainR[] = {0.970000, 0.803333, 0.720000, 0.646667, 0.596667,
                                               0.543333, 0.523333, 0.503333, 0.483333, 0.470000,};
};


#endif //GRAINSTORM_REVERBEARLYREFL_H
