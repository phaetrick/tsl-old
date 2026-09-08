//
// Created by pr on 09.12.23.
//

#ifndef EASY_RESAMPLER_H
#define EASY_RESAMPLER_H

#include <cstdint>
#include <cmath>

namespace EasyResampler {

    template<typename T, int numTaps = 32, int maxRows = 250>
    class ResamplerBase {
    public:
        ResamplerBase() = default;

        static_assert(numTaps >= 4, "numTaps must at least be 4!");
        static_assert((numTaps & 3) == 0, "numTaps must be a multiple of 4!");

        void init(T rate) {
            if (mRate == rate)
                return;
            const auto frac = rate - floor(rate);
            rows = 1;
            while (rows <= maxRows) {
                if (rows * frac - floor(rows * frac) == 0.0)
                    break;
                rows++;
            }
            mRate = rate;
            interpolate = rows > maxRows;
            mPhase = ceil(mRate);
            generateCoefficients();
        }

        void init(T inputRate, T outputRate) {
            init(inputRate / outputRate);
        }

        bool isWriteNeeded() const {
            return mPhase >= 1.;
        }

    protected:
        static constexpr const T mPi = 3.141592653589793238462643383279502884197;

        static constexpr T mAbs(T x) { return x < 0 ? -1. * x : x; }

        static constexpr T sinc(T radians) {
            if (mAbs(radians) < 1.0e-9) return 1.0;
            return sin(radians) / radians;
        }

        void generateCoefficients() {
            const T scale = (mRate > 1. ? downSampleBandWidth * 1. / mRate
                                        : upSampleBandWidth);
            const int numTapsHalf = numTaps / 2;
            const T numTapsHalfInverse = 1.0 / numTapsHalf;
            if (interpolate) {
                for (int i = 0; i < maxRows + 3; i++) {
                    T tapPhase = (i - 2) / (T) maxRows - numTapsHalf;
                    T gain = 0.0;
                    for (int tap = 0; tap < numTaps; tap++) {
                        T radians = tapPhase * mPi;
                        T coefficient =
                                sinc(radians * scale) *
                                hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
                        mCoefficients[i][tap] = coefficient;
                        gain += coefficient;
                        tapPhase += 1.0;
                    }
                    T gainCorrection = 1.0 / gain; // Probably not needed
                    for (int tap = 0; tap < numTaps; tap++) {
                        mCoefficients[i][tap] *= gainCorrection;
                    }
                }
            } else {
                for (int i = 0; i < rows; i++) {
                    T tapPhase = i / (T) rows - numTapsHalf;
                    T gain = 0.0;
                    for (int tap = 0; tap < numTaps; tap++) {
                        T radians = tapPhase * mPi;
                        T coefficient =
                                sinc(radians * scale) *
                                hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
                        mCoefficients[i][tap] = coefficient;
                        gain += coefficient;
                        tapPhase += 1.0;
                    }


                    T gainCorrection = 1.0 / gain; // Probably not needed
                    for (int tap = 0; tap < numTaps; tap++) {
                        mCoefficients[i][tap] *= gainCorrection;
                    }
                }
            }
        }


        struct QualityMapping {
            int base_length;
            T downsample_bandwidth;
            T upsample_bandwidth;
            T att;
        };

        static constexpr T getDownSampleBandWidth(const int taps) {
            T bw = quality_map[10].downsample_bandwidth;
            for (int i = 0; i < 11; i++)
                if (taps <= quality_map[i].base_length)
                    bw = quality_map[i].downsample_bandwidth;
            return bw;
        }

        static constexpr T getUpSampleBandWidth(const int taps) {
            T bw = quality_map[10].upsample_bandwidth;
            for (int i = 0; i < 11; i++)
                if (taps <= quality_map[i].base_length)
                    bw = quality_map[i].upsample_bandwidth;
            return bw;
        }

        static constexpr T getAtt(const int taps) {
            T att = quality_map[10].att;
            for (int i = 0; i < 11; i++)
                if (taps <= quality_map[i].base_length)
                    att = quality_map[i].att;
            return att;
        }

        /* The following is from Speex Resampler. Seems plausible */
        static constexpr const struct QualityMapping quality_map[11] = {
                {8,   0.830, 0.860, 60},
                {16,  0.850, 0.880, 60},
                {32,  0.882, 0.910, 60},
                {48,  0.895, 0.917, 80},
                {64,  0.921, 0.940, 80},
                {80,  0.922, 0.940, 100},
                {96,  0.940, 0.945, 100},
                {128, 0.950, 0.950, 100},
                {160, 0.960, 0.960, 100},
                {192, 0.968, 0.968, 100},
                {256, 0.975, 0.975, 100},
        };

        static constexpr void cubicCoeff(double interp[], const double frac) {
            interp[3] = -0.1666666667 * frac + 0.1666666667 * (frac * frac * frac);
            interp[2] = frac + 0.5 * (frac * frac) - 0.5 * (frac * frac * frac);
            interp[0] =
                    -0.3333333333 * frac + 0.5 * (frac * frac) -
                    0.1666666667 * (frac * frac * frac);
            interp[1] = 1. - interp[3] - interp[2] - interp[0];
        }

        /* The window class is from Google Oboe project. According to them this window gives less distortion */
        class HyperbolicCosineWindow {
        public:
            HyperbolicCosineWindow(T att) {
                setStopBandAttenuation(att);
            }

            /**
             * @param attenuation typical values range from 30 to 90 dB
             * @return beta
             */
            T setStopBandAttenuation(T attenuation) {
                T alpha = ((-325.1e-6 * attenuation + 0.1677) * attenuation) - 3.149;
                setAlpha(alpha);
                return alpha;
            }

            void setAlpha(T alpha) {
                mAlpha = alpha;
                mInverseCoshAlpha = 1.0 / cosh(alpha);
            }

            /**
             * @param x ranges from -1.0 to +1.0
             */
            T operator()(T x) {
                T x2 = x * x;
                if (x2 >= 1.0) return 0.0;
                T w = mAlpha * sqrt(1.0 - x2);
                return cosh(w) * mInverseCoshAlpha;
            }

        private:
            T mAlpha = 0.0;
            T mInverseCoshAlpha = 1.0;
        };

        HyperbolicCosineWindow hyperbolicCosineWindow{getAtt(numTaps)};
        T mRate{};
        int32_t readPos{};
        T mPhase{1.};
        T mCoefficients[maxRows + 3][numTaps]{};
        const T upSampleBandWidth{getUpSampleBandWidth(numTaps)}, downSampleBandWidth{
                getDownSampleBandWidth(numTaps)};
        bool interpolate{};
        int32_t rows{1};
    };


    template<typename T, int numTaps = 32, int maxRows = 250>
    class MonoResampler : public ResamplerBase<T, numTaps, maxRows> {
        using Base = ResamplerBase<T, numTaps, maxRows>;
    public:
        void clear() {
            for (int i = 0; i < numTaps * 2; i++)mem[i] = 0;
            Base::readPos = 0;
            Base::mPhase = ceil(Base::mRate);
        }

        void write(const T sample) {
            if (--Base::readPos < 0) {
                Base::readPos = numTaps - 1;
            }
            mem[Base::readPos] = mem[Base::readPos + numTaps] = sample;
            Base::mPhase -= 1;
        }

        T read() {
            auto ret = Base::interpolate ? readPrivInterpolatedCubic() : readPriv();
            Base::mPhase += Base::mRate;
            return ret;
        }

    private:
        T mem[numTaps * 2]{};

        inline T readPrivInterpolatedCubic() {
            T phase = Base::mPhase * maxRows;
            T floorphase = floor(phase);
            T frac = phase - floorphase;
            const int index = 2 + static_cast<int>(floorphase) % maxRows;
            const int indexM1 = index - 1;
            const int indexM2 = index - 2;
            const int indexP1 = index + 1;
            T interp[4];
            Base::cubicCoeff(interp, frac);
            T sum1{}, sum2{}, sum3{}, sum4{};
            for (int tap = 0; tap < numTaps; tap++) {
                sum1 += mem[Base::readPos + tap] * Base::mCoefficients[indexM2][tap];
                sum2 += mem[Base::readPos + tap] * Base::mCoefficients[indexM1][tap];
                sum3 += mem[Base::readPos + tap] * Base::mCoefficients[index][tap];
                sum4 += mem[Base::readPos + tap] * Base::mCoefficients[indexP1][tap];
            }
            return sum1 * interp[0] + sum2 * interp[1] + sum3 * interp[2] + sum4 * interp[3];
        }

        inline T readPrivInterpolatedLinear() {
            T phase = Base::mPhase * maxRows;
            T floorphase = floor(phase);
            T frac = phase - floorphase;
            const int index = 2 + static_cast<int>(floorphase) % maxRows;
            const int index2 = index + 1;
            T sum1{}, sum2{};
            for (int tap = 0; tap < numTaps; tap++) {
                sum1 += mem[Base::readPos + tap] * Base::mCoefficients[index][tap];
                sum2 += mem[Base::readPos + tap] * Base::mCoefficients[index2][tap];
            }
            return (sum1 + frac * (sum2 - sum1));
        }

        virtual inline T readPriv() {
            if (Base::mRate == 1.)
                return mem[Base::readPos];
            auto coefficients = Base::mCoefficients[static_cast<int>(Base::mPhase * Base::rows) %
                                                    Base::rows];
            T sum = 0.0;
            auto ptr = &mem[Base::readPos];

            const int numLoops = numTaps >> 2;
            for (int i = 0; i < numLoops; i++) {
                sum += *ptr++ * *coefficients++;
                sum += *ptr++ * *coefficients++;
                sum += *ptr++ * *coefficients++;
                sum += *ptr++ * *coefficients++;
            }
            return sum;
        }

    };

    template<typename T, int chans = 2, int numTaps = 32, int numRows = 250>
    class MultiChannelResampler : public ResamplerBase<T, numTaps, numRows> {
        using Base = ResamplerBase<T, numTaps, numRows>;
    public:
        void clear() {
            for (int channel = 0; channel < chans; channel++) {
                for (int i = 0; i < numTaps * 2; i++)mem[channel][i] = 0;
            }
            Base::readPos = 0;
            Base::mPhase = ceil(Base::mRate);
        }

        void write(const T sample[]) {
            if (--Base::readPos < 0) {
                Base::readPos = numTaps - 1;
            }
            for (int channel = 0; channel < chans; channel++) {
                mem[channel][Base::readPos] = mem[channel][Base::readPos +
                                                           numTaps] = sample[channel];
            }
            Base::mPhase -= 1.;
        }

        void read(T out[]) {
            Base::interpolate ? readPrivInterpolatedCubic(out) : readPriv(out);
            Base::mPhase += Base::mRate;
        }

    private:
        inline void readPrivInterpolatedCubic(T out[]) {
            T phase = Base::mPhase * numRows;
            T floorphase = floor(phase);
            T frac = phase - floorphase;
            const auto index = 2 + static_cast<int>(floorphase) % numRows;
            const auto indexM1 = index - 1;
            const auto indexM2 = index - 2;
            const auto indexP1 = index + 1;
            T interp[4];
            Base::cubicCoeff(interp, frac);
            for (int channel = 0; channel < chans; channel++) {
                T sum[4]{};
                for (int tap = 0; tap < numTaps; tap++) {
                    sum[0] += mem[channel][Base::readPos + tap] * Base::mCoefficients[indexM2][tap];
                    sum[1] += mem[channel][Base::readPos + tap] * Base::mCoefficients[indexM1][tap];
                    sum[2] += mem[channel][Base::readPos + tap] * Base::mCoefficients[index][tap];
                    sum[3] += mem[channel][Base::readPos + tap] * Base::mCoefficients[indexP1][tap];
                }
                out[channel] = sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
                               sum[3] * interp[3];
            }
        }

        inline void readPriv(T out[]) {
            auto coefficients = Base::mCoefficients[static_cast<int>(Base::mPhase * Base::rows) %
                                                    Base::rows];

            for (int channel = 0; channel < chans; channel++) {
                out[channel] = 0.0;
                for (int tap = 0; tap < numTaps; tap++) {
                    out[channel] +=
                            mem[channel][Base::readPos + tap] * coefficients[tap];
                }
            }
        }

        inline void readPrivInterpolatedLinear(T out[]) {
            T phase = Base::mPhase * numRows;
            T floorphase = floor(phase);
            T frac = phase - floorphase;
            int index = 2 + static_cast<int>(floorphase) % numRows;
            int index2 = index + 1;

            for (int channel = 0; channel < chans; channel++) {
                T sum1{}, sum2{};
                for (int tap = 0; tap < numTaps; tap++) {
                    sum1 += mem[channel][Base::readPos + tap] * Base::mCoefficients[index][tap];
                    sum2 += mem[channel][Base::readPos + tap] * Base::mCoefficients[index2][tap];
                }
                out[channel] = sum1 + frac * (sum2 - sum1);
            }
        }

        T mem[chans][numTaps * 2]{};
    };
}

#endif

#include <cstdio>

using namespace EasyResampler;

int main() {
    const double inRate = 49823., outRate = 29137.;
    MonoResampler<double, 128, 16> rs; //EasyResampler<double, 32> rs
    rs.init(inRate, outRate);
    auto fd = fopen("test.raw", "wb");
    if (!fd) {
        printf("Could not open output file.\n");
        return 1;
    }
    double phs = 0;
    const double twoPI = 6.283185307179586476925286766559005768394;
    double phaseInc = 200. / inRate;
    const double phaseIncInc = (20000. - 200.) / (10 * inRate); //This should alias at some point
    int inSamples = 0;
    while (inSamples < 10 * static_cast<int>(inRate)) {
        if (rs.isWriteNeeded()) {
            rs.write(sin(twoPI * phs / inRate));
            phs += phaseInc;
            phaseInc += phaseIncInc;
            ++inSamples;
        } else {
            auto smpl = static_cast<int16_t>(rs.read() * 32767.);
            if (fwrite(&smpl, sizeof(int16_t), 1, fd) != 1) {
                fclose(fd);
                printf("Write Error\n");
                return 1;
            }
        }
    }
    fclose(fd);
    printf("Written to test.raw \n");
    printf("Use e.g. 'ffplay -f s16le -ar 29137 -ac 1 test.raw' to listen.\n");
    return 0;
}


#endif //GRAINSTORM_TEST_H
