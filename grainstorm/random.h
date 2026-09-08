#pragma once
//
// Created by pr on 21.06.19.
//

#ifndef RANDOM_H
#define RANDOM_H

#include <atomic>
#include "defines.h"
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <vector>
#include <logger.h>
#include <app.h>


namespace tsl {
    namespace random {
        MYFLOAT randomfloat(MYFLOAT x, MYFLOAT y);


        class RandBase {
        protected:
            int32_t holdrand{rand()};

        public:
        };

        class gaussTrig : private RandBase {
        public:

            void setSampleRate(MYFLOAT sr) {
                _sr = sr;
            }

            void setFreq(MYFLOAT f) {
                _prvfreq = _freq = f;
            }

            void setDev(MYFLOAT d) {
                _prvdev = _dev = d;
            }


            void setFreq2(MYFLOAT f) {
                if (_prvfreq != f) {
                    _count = 0;
                }
                _prvfreq = _freq = f;
            }

            void setDev2(MYFLOAT d) {
                if (_prvdev != d) {
                    _count = 0;
                }
                _prvdev = _dev = d;
            }

            bool tick() {
                if (_count <= 0) {
                    auto nextsamps = (int32_t) (_sr / _prvfreq);
                    auto r1 = randGab;
                    auto r2 = randGab;
                    auto nextcount = sqrt(-2.0 * log(r1)) * sin(r2 * TWOPI_P);
                    if (nextcount < -1.0) {
                        auto diff = -1.0 - nextcount;
                        nextcount = (1.0 < -1.0 + diff ? 1.0 : -1.0 + diff);
                    } else if (nextcount > 1.0) {
                        auto diff = nextcount - 1.0;
                        nextcount = (-1.0 > 1.0 - diff ? -1.0 : 1.0 - diff);
                    }
                    _count = (int32_t) (nextsamps + nextcount * _dev * nextsamps);
                    _count--;
                    return true;
                } else {
                    _count--;
                    return false;
                }
            }

            void reset() {
                _count = 0;
            }

            void setCount(int32_t c) {
                _count = c;
            }


            int32_t Count() {
                return _count;
            }

        private:
            MYFLOAT _prvfreq{}, _prvdev{}, _freq{}, _dev{}, _sr{48000.};
            int32_t _count{};
        };


        class randomi : private RandBase {
        public:
            randomi(MYFLOAT a, MYFLOAT b, MYFLOAT cps) : _a{a}, _b{b}, _cps{cps} {
                if (_a > _b)std::swap(a, b);
                _lastval = randGab;
                _nextval = randGab;
                _diff = _nextval - _lastval;
            }

            MYFLOAT tick(MYFLOAT sr) {
                auto ret = (_lastval + _diff * phs) * (_b - _a) + _a;
                phs += _cps / sr;
                if (phs >= 1.0) {
                    phs -= 1.0;
                    _lastval = _nextval;
                    _nextval = randGab;
                    _diff = _nextval - _lastval;
                }
                return ret;
            }

        private:
            MYFLOAT _a{}, _b{}, _cps{}, _lastval{}, phs{}, _nextval{}, _diff{};
        };

        template<typename T>
        class NoiseBase : protected RandBase {
        public:
            inline T tick() { return filter(BiRandGab); };

            void process(T buf[], int32_t n, T gain = 1.0) {
                for (int32_t i = 0; i < n; i++)
                    buf[i] = tick();
            }

        protected:
            virtual inline T filter(const T s) = 0;
        };

// +/-0.05dB above 9.2Hz @ 44,100Hz
        template<typename T>
        class WhiteNoise : public NoiseBase<T> {
        protected:
            inline T filter(const T s) {
                return s;
            }
        };

// +/-0.05dB above 9.2Hz @ 44,100Hz
        template<typename T>
        class PinkNoise : public NoiseBase<T> {
            T b0, b1, b2, b3, b4, b5, b6;
        public:
            PinkNoise() : b0(0), b1(0), b2(0), b3(0), b4(0), b5(0), b6(0) {}

        protected:
            inline T filter(const T s) {
                b0 = 0.99886 * b0 + s * 0.0555179;
                b1 = 0.99332 * b1 + s * 0.0750759;
                b2 = 0.96900 * b2 + s * 0.1538520;
                b3 = 0.86650 * b3 + s * 0.3104856;
                b4 = 0.55000 * b4 + s * 0.5329522;
                b5 = -0.7616 * b5 - s * 0.0168980;
                const T pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + (s * 0.5362)) * 0.11;
                b6 = s * 0.115926;
                return pink;
            }
        };

        template<typename T>
        class BrownNoise : public NoiseBase<T> {
            T l;
        public:
            BrownNoise() : l(0) {}

        protected:
            inline T filter(const T s) {
                T brown = (l + (0.02 * s)) / 1.02;
                l = brown;
                return brown * 3.5; // compensate for gain
            }
        };

        class Spline : private RandBase {
        public:
            Spline() = default;
            Spline(MYFLOAT sr){setSampleRate(sr);};
            Spline(MYFLOAT sr, MYFLOAT cpsmin, MYFLOAT cpsmax, MYFLOAT rangemin,
                   MYFLOAT rangemax) {setSampleRate(sr);
                setUp(cpsmin, cpsmax, rangemin, rangemax);
            }
             void setSampleRate(MYFLOAT sr){
                 _onedsr = 1./sr;;
            }
            MYFLOAT tick();
            void setUp(MYFLOAT cpsmin, MYFLOAT cpsmax, MYFLOAT rangemin, MYFLOAT rangemax) {
                if (cpsmin > cpsmax) {
                    cpsMin = cpsmax;
                    cpsMax = cpsmin;
                } else {
                    cpsMin = cpsmin;
                    cpsMax = cpsmax;
                }
                if (rangemin > rangemax) {
                    rangeMin = rangemax;
                    rangeMax = rangemin;
                } else {
                    rangeMin = rangemin;
                    rangeMax = rangemax;
                }
            }
            void reset(){
                num1 = randGab;
                num2 = randGab;
                df1 = 0.0;
                phs = 0.0;
                init = true;
            }
        private:
            MYFLOAT _onedsr{1./48000.};
            MYFLOAT rangeMin{}, rangeMax{}, cpsMin{}, cpsMax{};
            MYFLOAT si{};
            MYFLOAT phs{};
            MYFLOAT num0{}, num1{randGab}, num2{randGab}, df0{}, df1{}, c3{}, c2{};
            bool init{true};
        };


        class Random {
        public:
            Random();

            uint32_t RandMT();

            float unifrand(float range = 1.f);

            float linrand(float range = 1.f);

            float trirand(float range = 1.f);

            float exprand(float range = 1.f);

            float biexprand(float range = 1.f);

            template<typename T>
            inline T gaussrand(T range = 1.) {
                int64_t r1 = -((int64_t) 0xFFFFFFFFU * 6);
                int32_t n = 12;

                do {
                    r1 += (int64_t) RandMT();
                } while (--n);
                double x = (double) r1;
                return (T) (x * ((double) range * (1.0 / (3.83 * 4294967295.03125))));
            }

            float cauchrand(float a = 1.f);

            float pcauchrand(float a = 1.f);

            float weibrand(float s, float t);

            float poissrand(float lambda);

            float betarand(float range, float a, float b);

            template<typename T>
            static inline T randomfloat(T min, T max) {
                return min + (rand() / (T) RAND_MAX) * DISTANCE(min, max);
            }

            static double betapdf2(double x, double a, double b) {
                //if (x < 0 || x > 1) return 0;
                double tmp = pow((1 - x), (b - 1)) * pow(x, (a - 1));
                double iB = exp(lgamma(a + b) - lgamma(a) - lgamma(b)); // 1/B
                return tmp * iB;
            }
/*
    static double gammapdf(double value, double alpha, double beta) {
        return (std::pow(beta, alpha) * std::pow(value, (alpha - 1)) *
                std::pow(M_E, (-1 * beta * value))) / tgamma(alpha);
    }
*/
        private:
            static constexpr int32_t N{624}, M{397};
            int32_t mti;
            uint32_t mt[N];

            void MT_update_state();

            void SeedRandMT(const uint32_t *initKey, uint32_t keyLength);
        };


        uint32_t GetRandomSeedFromTime(void);

#define FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_HURST_CONST 0.5
#define FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_BUFSIZE 15

        template<typename T>
        class pinknoise_frac {
        public:
            pinknoise_frac() {
                setParams(FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_HURST_CONST,
                          FV3_NOISEGEN_PINK_FRACTAL_1_DEFAULT_BUFSIZE);
            }

            /**
             * Set the pink noise parameters.
             * @param[in] param1 The Hurst constant, between 0 and 0.9999 (fractal dimension).
             * @param[in] length The fractal noise generator size will be 2^length.
             */
            void setParams(T param1, long length) {
                pfn1_param = param1;
                pfn1_length = (1 << length);
                if (pfn1_slot.size() != pfn1_length) {
                    pfn1_slot.resize(pfn1_length, 0);
                    pfn1_count = 0;
                }
                mute();
            }

            void mute() {
                std::fill(pfn1_slot.begin(), pfn1_slot.end(), 0);
                pfn1_count = 0;
            }

            inline T process() {
                if (pfn1_count == 0) {
                    fractal(pfn1_slot.data(), pfn1_length, pfn1_param);
                    pfn1_count = pfn1_length;
                }
                pfn1_count--;
                return pfn1_slot.data()[pfn1_length - (pfn1_count + 1)];
            }

        private:
            /**
             * generate fractal pattern using Midpoint Displacement Method
             * v: buffer of floats to output fractal pattern to
             * N: length of v, MUST be integer power of 2 (ie 128, 256, ...)
             * H: Hurst constant, between 0 and 0.9999 (fractal dimension)
             * based on the tap-plugins
             */
            void fractal(T *v, int32_t N, T H) {
                int32_t l = N, k, c;
                T r = 2.0 * (H * H) + 0.3;
                v[0] = 0;
                while (l > 1) {
                    k = N / l;
                    for (c = 0; c < k; c++) {
                        v[c * l + l / 2] = (v[c * l] + v[((c + 1) * l) % N]) / 2.0
                                           +
                                           2.0 * r * ((T) std::rand() - (T) RAND_MAX / 2.0) /
                                           (T) RAND_MAX;
                        if (v[c * l + l / 2] < -1.) v[c * l + l / 2] = -1.;
                        if (v[c * l + l / 2] > 1.) v[c * l + l / 2] = 1.;
                    }
                    l /= 2;
                    r /= std::pow((T) 2., H);
                }
            }

            T pfn1_param;
            long pfn1_length, pfn1_count;
            std::vector<T> pfn1_slot;
        };


#define RANDAMOUNTAMP   1.59055f
#define RANDAMOUNTFREQ  0.629921f
#define AMPMINRATE      1.f
#define AMPMAXRATE      3.f
#define CPSMINRATE      1.19377f
#define CPSMAXRATE      2.28100f

        class Vibrato {
        public:
            Vibrato();

            void
            Init(int32_t sr, float averageFreq, float averageAmp, float randAmountAmp = RANDAMOUNTAMP,
                 float randAmountFreq = RANDAMOUNTFREQ, float cpsMinRate = CPSMINRATE,
                 float cpsMaxRate = CPSMAXRATE, float ampMinRate = AMPMINRATE,
                 float ampMaxRate = AMPMAXRATE) {
                _cpsMinRate = cpsMinRate, _cpsMaxRate = cpsMaxRate, _ampMinRate = ampMinRate, _ampMaxRate = ampMaxRate, _AverageFreq = averageFreq, _AverageAmp = averageAmp;
                xcpsAmpRate = randGab * (cpsMaxRate - cpsMinRate) + cpsMinRate;
                xcpsFreqRate = randGab * (ampMaxRate - ampMinRate) + ampMinRate;
                krate = sr / (float) KSAMPLES;
                tablenUPkr = WINDOW_SIZE / krate;//p->tablenUPkr = p->tablen * CS_ONEDKR;
                kicvt = FMAXLEN / krate;
                _randAmountAmp = randAmountAmp;
                _randAmountFreq = randAmountFreq;
            }

            void reset() {
                phs = num1amp = num2amp = num1freq = num2freq = dfdmaxAmp = dfdmaxFreq = 0.0f;
                phsAmpRate = phsFreqRate = 0;
            }


            float tick() {
                float inc;
                MYFLOAT *ftab, fract, v1;
                float RandAmountAmp, RandAmountFreq;

                RandAmountAmp = (num1amp + phsAmpRate * dfdmaxAmp) *
                                _randAmountAmp;
                RandAmountFreq = (num1freq + phsFreqRate * dfdmaxFreq) *
                                 _randAmountFreq;

                fract = (float) (phs - (int32_t) phs);
                ftab = table + (int32_t) phs;
                v1 = *ftab++;
                float ret = (v1 + (*ftab - v1) * fract) *
                            (_AverageAmp * powf(2.f, RandAmountAmp));
                inc = (_AverageFreq * powf(2.f, RandAmountFreq)) * tablenUPkr;
                phs += inc;
                while (phs >= WINDOW_SIZE)
                    phs -= WINDOW_SIZE;
                while (phs < 0.0)
                    phs += WINDOW_SIZE;
                phsAmpRate += (int32_t) (xcpsAmpRate * kicvt);
                if (phsAmpRate >= MAXLEN) {
                    xcpsAmpRate = randGab * (_ampMaxRate - _ampMinRate) +
                                  _ampMinRate;
                    phsAmpRate &= PHMASK;
                    num1amp = num2amp;
                    num2amp = BiRandGab;
                    dfdmaxAmp = (num2amp - num1amp) / FMAXLEN;
                }
                phsFreqRate += (int32_t) (xcpsFreqRate * kicvt);
                if (phsFreqRate >= MAXLEN) {
                    xcpsFreqRate = randGab * (_cpsMaxRate - _cpsMinRate) +
                                   _cpsMinRate;
                    phsFreqRate &= PHMASK;
                    num1freq = num2freq;
                    num2freq = BiRandGab;
                    dfdmaxFreq = (num2freq - num1freq) / FMAXLEN;
                }
                return ret;
            }

        private:
            float tablenUPkr, kicvt, krate;
            float xcpsAmpRate, xcpsFreqRate;
            float _AverageFreq, _AverageAmp, _cpsMinRate, _cpsMaxRate, _ampMinRate, _ampMaxRate, _randAmountAmp, _randAmountFreq;
            float phs;
            int32_t phsFreqRate, phsAmpRate;
            float dfdmaxFreq, dfdmaxAmp;
            MYFLOAT *table;
            float num1amp, num2amp, num1freq, num2freq;
            uint32_t holdrand{};
        };


        class SimpleVibrato {
        public:
            SimpleVibrato(MYFLOAT sr, MYFLOAT rate = 0.f, MYFLOAT depth = 0.f) ;

            MYFLOAT tick() {
                _offset += _phinc;
                if (_offset >= 1.0)
                    _offset -= 1.0;
                return _sine[PHS2INT(_offset)];
            }

            MYFLOAT tick(MYFLOAT rate = 0.0) {
                _offset += rate * _onedsr;
                if (_offset >= 1.0)
                    _offset -= 1.0;
                return _sine[PHS2INT(_offset)];
            }

        private:
            MYFLOAT _depth;
            MYFLOAT _offset;
            MYFLOAT *_sine;
            MYFLOAT _rate;
            MYFLOAT _phinc;
            MYFLOAT _onedsr;
        };


        class PinkNoise2 {
        public:
            PinkNoise2(float _amp);

            void Compute(float *in, int32_t size);

            void Reset();

            float Tick();

            float amp;
            long pink_Rows[30];
            long pink_RunningSum;   /* Used to optimize summing of generators. */
            int32_t pink_Index;        /* Incremented each sample. */
            int32_t pink_IndexMask;    /* Index wrapped by ANDing with this mask. */
            float pink_Scalar;       /* Used to scale within range of -1.0 to +1.0 */

        };

/*

class Vibrato{
public:
    Vibrato(float averageFreq, float averageAmp, float cpsMinRate, float cpsMaxRate, float ampMinRate, float ampMaxRate){
        _cpsMinRate = cpsMinRate, _cpsMaxRate = cpsMaxRate, _ampMinRate = ampMinRate, _ampMaxRate = ampMaxRate, _AverageFreq = averageFreq, _AverageAmp = averageAmp;
        xcpsAmpRate = randGab *(cpsMaxRate - cpsMinRate) + cpsMinRate;
        xcpsFreqRate = randGab *(ampMaxRate - ampMinRate) + ampMinRate;
        //p->tablenUPkr = p->tablen * CS_ONEDKR;
        num1amp = num2amp = num1freq = num2freq = 0.0f;
    }


    void compute()
    {
        float      phs, inc;
        float       *ftab, fract, v1;
        float       RandAmountAmp,RandAmountFreq;

        RandAmountAmp = (num1amp + phsAmpRate * dfdmaxAmp) *
                        randAmountAmp ;
        RandAmountFreq = (num1freq + phsFreqRate * dfdmaxFreq) *
                         randAmountFreq ;

        fract = (MYFLT) (phs - (int32_t)phs);
        ftab = table + (int32_t)phs;
        v1 = *ftab++;
        *p->out = (v1 + (*ftab - v1) * fract) *
                  (_AverageAmp * powf(2.f,RandAmountAmp));
        inc = ( _AverageFreq * powf(2.f,RandAmountFreq)) * tablenUPkr;
        phs += inc;
        while (phs >= WINDOW_SIZE)
            phs -= WINDOW_SIZE;
        while (phs < 0.0 )
            phs += WINDOW_SIZE;
        phsAmpRate += (int32_t)(xcpsAmpRate * CS_KICVT);
        if (phsAmpRate >= MAXLEN) {
            xcpsAmpRate =  randGab  * (_ampMaxRate - _ampMinRate) +
                           _ampMinRate;
            phsAmpRate &= PHMASK;
            num1amp = num2amp;
            num2amp = BiRandGab ;
            dfdmaxAmp = (num2amp - num1amp) / FMAXLEN;
        }
        phsFreqRate += (int32_t)(xcpsFreqRate * CS_KICVT);
        if (phsFreqRate >= MAXLEN) {
            xcpsFreqRate =  randGab  * (_cpsMaxRate - _cpsMinRate) +
                            _cpsMinRate;
            phsFreqRate &= PHMASK;
            num1freq = num2freq;
            num2freq = BiRandGab;
            dfdmaxFreq = (num2freq - num1freq) / FMAXLEN;
        }
    }
private:
    float onedtablelen;
    float xcpsAmpRate, xcpsFreqRate;
    float _AverageFreq, _AverageAmp, _cpsMinRate, _cpsMaxRate, _ampMinRate, _ampMaxRate, dfdmaxAmp,  randAmountAmp;
    float phs;
    int32_t phsFreqRate,phsAmpRate;
    float  dfdmaxFreq, randAmountFreq;
    float table[WINDOW_SIZE];
    float   num1amp, num2amp, num1freq, num2freq;
};

*/
    }
}

#endif