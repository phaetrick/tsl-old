//
// Created by pr on 19.12.18.
//

#include "logger.h"
#include "Convolver.h"
#include "types.h"
#include "track.h"
#include "grainstorm.h"
#include "ffttools.h"
#include "liveconv.h"

using namespace std;
namespace tsl {
	namespace butter {

#define PI 3.14159

		vector<double> ComputeDenCoeffs(int32_t FilterOrder, double Lcutoff, double Ucutoff);

		vector<double> TrinomialMultiply(int32_t FilterOrder, vector<double> b, vector<double> c);

		vector<double>
			ComputeNumCoeffs(int32_t FilterOrder, double Lcutoff, double Ucutoff, vector<double> DenC);

		vector<double> ComputeLP(int32_t FilterOrder);

		vector<double> ComputeHP(int32_t FilterOrder);

		//vector<double> filter(int32_t ord, vector<double> a, vector<double> b, int np, vector<double> x);

		vector<double> filter(vector<double> x, vector<double> coeff_b, vector<double> coeff_a);

		int32_t tets() {
			vector<double> input, output;
			double fps = 20;

			const int32_t N = 100;
			//
			//for (int32_t i = 0; i < 1000; i++)
			//{
			//	float x;
			//	ifile >> x;
			//	input.push_back(x);
			//}

			//Frequency bands is a vector of values - Lower Frequency Band and Higher Frequency Band

			//First value is lower cutoff and second value is higher cutoff
			double FrequencyBands[2] = { 1.5 / fps * 2, 2.5 / fps *
													   2 };//these values are as a ratio of f/fs, where fs is sampling rate, and f is cutoff frequency
			//and therefore should lie in the range [0 1]
			//Filter Order

			int32_t FiltOrd = 4;

			//Pixel Time Series
			/*int32_t PixelTimeSeries[N];
			int32_t outputSeries[N];
			*/
			//Create the variables for the numerator and denominator coefficients
			vector<double> a;
			vector<double> b;
			//Pass Numerator Coefficients and Denominator Coefficients arrays into function, will return the same

			vector<double> x(N);
			vector<double> y(N);


			//is A in matlab function and the numbers are correct
			a = ComputeDenCoeffs(FiltOrd, FrequencyBands[0], FrequencyBands[1]);
			for (int32_t k = 0; k < a.size(); k++) {
				printf("DenC is: %lf\n", a[k]);
			}

			b = ComputeNumCoeffs(FiltOrd, FrequencyBands[0], FrequencyBands[1], a);
			for (int32_t k = 0; k < b.size(); k++) {
				printf("NumC is: %lf\n", b[k]);
			}

			y = filter(x, b, a);

			return 0;
		}

		vector<double> ComputeDenCoeffs(int32_t FilterOrder, double Lcutoff, double Ucutoff) {
			int32_t k;            // loop variables
			double theta;     // PI * (Ucutoff - Lcutoff) / 2.0
			double cp;        // cosine of phi
			double st;        // sine of theta
			double ct;        // cosine of theta
			double s2t;       // sine of 2*theta
			double c2t;       // cosine 0f 2*theta
			vector<double> RCoeffs(2 * FilterOrder);     // z^-2 coefficients
			vector<double> TCoeffs(2 * FilterOrder);     // z^-1 coefficients
			vector<double> DenomCoeffs;     // dk coefficients
			double PoleAngle;      // pole angle
			double SinPoleAngle;     // sine of pole angle
			double CosPoleAngle;     // cosine of pole angle
			double a;         // workspace variables

			cp = cos(PI * (Ucutoff + Lcutoff) / 2.0);
			theta = PI * (Ucutoff - Lcutoff) / 2.0;
			st = sin(theta);
			ct = cos(theta);
			s2t = 2.0 * st * ct;        // sine of 2*theta
			c2t = 2.0 * ct * ct - 1.0;  // cosine of 2*theta

			for (k = 0; k < FilterOrder; ++k) {
				PoleAngle = PI * (double)(2 * k + 1) / (double)(2 * FilterOrder);
				SinPoleAngle = sin(PoleAngle);
				CosPoleAngle = cos(PoleAngle);
				a = 1.0 + s2t * SinPoleAngle;
				RCoeffs[2 * k] = c2t / a;
				RCoeffs[2 * k + 1] = s2t * CosPoleAngle / a;
				TCoeffs[2 * k] = -2.0 * cp * (ct + st * SinPoleAngle) / a;
				TCoeffs[2 * k + 1] = -2.0 * cp * st * CosPoleAngle / a;
			}

			DenomCoeffs = TrinomialMultiply(FilterOrder, TCoeffs, RCoeffs);

			DenomCoeffs[1] = DenomCoeffs[0];
			DenomCoeffs[0] = 1.0;
			for (k = 3; k <= 2 * FilterOrder; ++k)
				DenomCoeffs[k] = DenomCoeffs[2 * k - 2];

			for (int32_t i = DenomCoeffs.size() - 1; i > FilterOrder * 2 + 1; i--)
				DenomCoeffs.pop_back();

			return DenomCoeffs;
		}

		vector<double> TrinomialMultiply(int32_t FilterOrder, vector<double> b, vector<double> c) {
			int32_t i, j;
			vector<double> RetVal(4 * FilterOrder);

			RetVal[2] = c[0];
			RetVal[3] = c[1];
			RetVal[0] = b[0];
			RetVal[1] = b[1];

			for (i = 1; i < FilterOrder; ++i) {
				RetVal[2 * (2 * i + 1)] += c[2 * i] * RetVal[2 * (2 * i - 1)] -
					c[2 * i + 1] * RetVal[2 * (2 * i - 1) + 1];
				RetVal[2 * (2 * i + 1) + 1] += c[2 * i] * RetVal[2 * (2 * i - 1) + 1] +
					c[2 * i + 1] * RetVal[2 * (2 * i - 1)];

				for (j = 2 * i; j > 1; --j) {
					RetVal[2 * j] += b[2 * i] * RetVal[2 * (j - 1)] -
						b[2 * i + 1] * RetVal[2 * (j - 1) + 1] +
						c[2 * i] * RetVal[2 * (j - 2)] -
						c[2 * i + 1] * RetVal[2 * (j - 2) + 1];
					RetVal[2 * j + 1] += b[2 * i] * RetVal[2 * (j - 1) + 1] +
						b[2 * i + 1] * RetVal[2 * (j - 1)] +
						c[2 * i] * RetVal[2 * (j - 2) + 1] +
						c[2 * i + 1] * RetVal[2 * (j - 2)];
				}

				RetVal[2] += b[2 * i] * RetVal[0] - b[2 * i + 1] * RetVal[1] + c[2 * i];
				RetVal[3] += b[2 * i] * RetVal[1] + b[2 * i + 1] * RetVal[0] + c[2 * i + 1];
				RetVal[0] += b[2 * i];
				RetVal[1] += b[2 * i + 1];
			}

			return RetVal;
		}

		vector<double>
			ComputeNumCoeffs(int32_t FilterOrder, double Lcutoff, double Ucutoff, vector<double> DenC) {
			vector<double> TCoeffs;
			vector<double> NumCoeffs(2 * FilterOrder + 1);
			vector<std::complex<double>> NormalizedKernel(2 * FilterOrder + 1);

			vector<double> Numbers;
			for (double n = 0; n < FilterOrder * 2 + 1; n++)
				Numbers.push_back(n);
			int32_t i;

			TCoeffs = ComputeHP(FilterOrder);

			for (i = 0; i < FilterOrder; ++i) {
				NumCoeffs[2 * i] = TCoeffs[i];
				NumCoeffs[2 * i + 1] = 0.0;
			}
			NumCoeffs[2 * FilterOrder] = TCoeffs[FilterOrder];

			double cp[2];
			double Bw, Wn;
			cp[0] = 2 * 2.0 * tan(PI * Lcutoff / 2.0);
			cp[1] = 2 * 2.0 * tan(PI * Ucutoff / 2.0);

			Bw = cp[1] - cp[0];
			//center frequency
			Wn = sqrt(cp[0] * cp[1]);
			Wn = 2 * atan2(Wn, 4);
			double kern;
			const std::complex<double> result = std::complex<double>(-1, 0);

			for (int32_t k = 0; k < FilterOrder * 2 + 1; k++) {
				NormalizedKernel[k] = std::exp(-sqrt(result) * Wn * Numbers[k]);
			}
			double b = 0;
			double den = 0;
			for (int32_t d = 0; d < FilterOrder * 2 + 1; d++) {
				b += real(NormalizedKernel[d] * NumCoeffs[d]);
				den += real(NormalizedKernel[d] * DenC[d]);
			}
			for (int32_t c = 0; c < FilterOrder * 2 + 1; c++) {
				NumCoeffs[c] = (NumCoeffs[c] * den) / b;
			}

			for (int32_t i = NumCoeffs.size() - 1; i > FilterOrder * 2 + 1; i--)
				NumCoeffs.pop_back();

			return NumCoeffs;
		}

		vector<double> ComputeLP(int32_t FilterOrder) {
			vector<double> NumCoeffs(FilterOrder + 1);
			int32_t m;
			int32_t i;

			NumCoeffs[0] = 1;
			NumCoeffs[1] = FilterOrder;
			m = FilterOrder / 2;
			for (i = 2; i <= m; ++i) {
				NumCoeffs[i] = (double)(FilterOrder - i + 1) * NumCoeffs[i - 1] / i;
				NumCoeffs[FilterOrder - i] = NumCoeffs[i];
			}
			NumCoeffs[FilterOrder - 1] = FilterOrder;
			NumCoeffs[FilterOrder] = 1;

			return NumCoeffs;
		}

		vector<double> ComputeHP(int32_t FilterOrder) {
			vector<double> NumCoeffs;
			int32_t i;

			NumCoeffs = ComputeLP(FilterOrder);

			for (i = 0; i <= FilterOrder; ++i)
				if (i % 2) NumCoeffs[i] = -NumCoeffs[i];

			return NumCoeffs;
		}

		vector<double> filter(vector<double> x, vector<double> coeff_b, vector<double> coeff_a) {
			int32_t len_x = x.size();
			int32_t len_b = coeff_b.size();
			int32_t len_a = coeff_a.size();

			vector<double> zi(len_b);

			vector<double> filter_x(len_x);

			if (len_a == 1) {
				for (int32_t m = 0; m < len_x; m++) {
					filter_x[m] = coeff_b[0] * x[m] + zi[0];
					for (int32_t i = 1; i < len_b; i++) {
						zi[i - 1] = coeff_b[i] * x[m] + zi[i];//-coeff_a[i]*filter_x[m];
					}
				}
			}
			else {
				for (int32_t m = 0; m < len_x; m++) {
					filter_x[m] = coeff_b[0] * x[m] + zi[0];
					for (int32_t i = 1; i < len_b; i++) {
						zi[i - 1] = coeff_b[i] * x[m] + zi[i] - coeff_a[i] * filter_x[m];
					}
				}
			}

			return filter_x;
		}
	}
}

#include <vector>
#include <cmath>
#include <random>
#include <fstream>
#include <iostream>
#include <complex>
#include <functional>
#include <numeric>

/* Dead experimental IR generators (SEASONAL/GOLDENRATIO/LORENZ/... presets).
   Their SpectralDelay mode ids were repurposed for the algorithm set below --
   nothing dispatches here any more, and the code never made a sound worth
   keeping, so it is compiled out wholesale. */
#if 0
class SeasonalIRGenerator {
private:
    double sampleRate;
    std::mt19937 rng;
    std::uniform_real_distribution<double> noise;

public:
    SeasonalIRGenerator(double sr = 44100.0) : sampleRate(sr), rng(std::random_device{}()), noise(-1.0, 1.0) {}

    struct IRParams {
        double length = 4.0;           // seconds
        double seasonSpeed = 1.0;      // season cycle multiplier
        double winterBrightness = 1.5; // high freq emphasis
        double summerWarmth = 0.3;     // low freq emphasis
        double roomSize = 0.8;         // base room characteristics
        double diffusion = 0.7;        // scatter amount
    };

    // Generate stereo IR
    std::pair<std::vector<double>, std::vector<double>> generateIR(const IRParams& params) {
        int samples = static_cast<int>(params.length * sampleRate);
        std::vector<double> leftChannel(samples);
        std::vector<double> rightChannel(samples);

        // Pre-calculate seasonal functions for efficiency
        std::vector<double> winterCurve(samples);
        std::vector<double> springCurve(samples);
        std::vector<double> summerCurve(samples);
        std::vector<double> autumnCurve(samples);

        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double progress = t / params.length;
            double seasonPos = fmod(progress * params.seasonSpeed * 4.0, 4.0);

            // Calculate seasonal weights
            winterCurve[i] = calculateSeasonWeight(seasonPos, 0.0);
            springCurve[i] = calculateSeasonWeight(seasonPos, 1.0);
            summerCurve[i] = calculateSeasonWeight(seasonPos, 2.0);
            autumnCurve[i] = calculateSeasonWeight(seasonPos, 3.0);
        }

        // Generate IR samples
        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double progress = t / params.length;

            // Base decay envelope
            double decay = std::exp(-3.0 * progress) * std::exp(-0.5 * progress * progress);

            // Generate seasonal components
            double sample = 0.0;

            // Winter: Crystalline, bright reflections
            if (winterCurve[i] > 0.01) {
                sample += generateWinterComponent(t, params) * winterCurve[i];
            }

            // Spring: Fresh, growing resonances
            if (springCurve[i] > 0.01) {
                sample += generateSpringComponent(t, params) * springCurve[i];
            }

            // Summer: Lush, warm absorption
            if (summerCurve[i] > 0.01) {
                sample += generateSummerComponent(t, params) * summerCurve[i];
            }

            // Autumn: Rich, mellow tones
            if (autumnCurve[i] > 0.01) {
                sample += generateAutumnComponent(t, params) * autumnCurve[i];
            }

            // Add base room tone and diffusion
            sample += generateRoomTone(t, params) * 0.1;
            sample += generateDiffusion(t, params) * params.diffusion * 0.05;

            // Apply decay and normalize
            sample *= decay * 0.3;

            // Stereo positioning with seasonal movement
            double seasonPos = fmod(progress * params.seasonSpeed * 4.0, 4.0);
            double pan = std::sin(seasonPos * PI_P * 0.5) * 0.4;

            leftChannel[i] = sample * (1.0 - pan * 0.5);
            rightChannel[i] = sample * (1.0 + pan * 0.5);
        }

        return {leftChannel, rightChannel};
    }
    void generateIR(const IRParams& params, tsl::AlignedVector<double> &buf, int chan =  0, double scale = 1.0) {
        int samples = std::min(static_cast<int>(params.length * sampleRate), static_cast<int>(buf.size()));
        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double progress = t / params.length;
            double seasonPos = fmod(progress * params.seasonSpeed * 4.0, 4.0);

            // Calculate seasonal weights

        }

        // Generate IR samples
        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double progress = t / params.length;
            double seasonPos = fmod(progress * params.seasonSpeed * 4.0, 4.0);
            auto winterCurve = calculateSeasonWeight(seasonPos, 0.0);
            auto springCurve = calculateSeasonWeight(seasonPos, 1.0);
            auto summerCurve = calculateSeasonWeight(seasonPos, 2.0);
            auto autumnCurve = calculateSeasonWeight(seasonPos, 3.0);
            // Base decay envelope
            double decay = std::exp(-3.0 * progress) * std::exp(-0.5 * progress * progress);

            // Generate seasonal components
            double sample = 0.0;

            // Winter: Crystalline, bright reflections
            if (winterCurve > 0.01) {
                sample += generateWinterComponent(t, params) * winterCurve;
            }

            // Spring: Fresh, growing resonances
            if (springCurve > 0.01) {
                sample += generateSpringComponent(t, params) * springCurve;
            }

            // Summer: Lush, warm absorption
            if (summerCurve > 0.01) {
                sample += generateSummerComponent(t, params) * summerCurve;
            }

            // Autumn: Rich, mellow tones
            if (autumnCurve > 0.01) {
                sample += generateAutumnComponent(t, params) * autumnCurve;
            }

            // Add base room tone and diffusion
            sample += generateRoomTone(t, params) * 0.1;
            sample += generateDiffusion(t, params) * params.diffusion * 0.05;

            // Apply decay and normalize
            sample *= decay * 0.3;

            // Stereo positioning with seasonal movement
            double pan = std::sin(seasonPos * PI_P * 0.5) * 0.4;

            buf[i] = chan == 0 ? sample * (1.0 - pan * 0.5) * scale : sample * (1.0 + pan * 0.5) * scale;
        }
    }

private:
    // Calculate weight for each season based on position in cycle
    double calculateSeasonWeight(double seasonPos, double seasonCenter) {
        double distance = std::abs(seasonPos - seasonCenter);
        if (distance > 2.0) distance = 4.0 - distance; // Wrap around

        if (distance <= 1.0) {
            return std::cos(distance * PI_P * 0.5); // Smooth transition
        }
        return 0.0;
    }

    // Winter: Bright, crystalline characteristics
    double generateWinterComponent(double t, const IRParams& params) {
        double crystal = std::sin(t * 5000.0 * PI_P) * 0.1;
        double ice = std::sin(t * 3000.0 * PI_P + std::sin(t * 100.0) * 2.0) * 0.15;
        double brightness = std::sin(t * 8000.0 * PI_P) * 0.08;

        return (crystal + ice + brightness) * params.winterBrightness;
    }

    // Spring: Fresh, growing resonances
    double generateSpringComponent(double t, const IRParams& params) {
        double growth = std::sin(t * 800.0 * PI_P + std::sin(t * 50.0) * 3.0) * 0.2;
        double bloom = std::sin(t * 1200.0 * PI_P + std::cos(t * 30.0) * 2.0) * 0.15;
        double fresh = std::sin(t * 2000.0 * PI_P) * 0.1 * std::exp(-t * 2.0);

        return growth + bloom + fresh;
    }

    // Summer: Lush, warm absorption
    double generateSummerComponent(double t, const IRParams& params) {
        double lush = std::sin(t * 400.0 * PI_P + std::sin(t * 20.0) * 4.0) * 0.25;
        double warm = std::sin(t * 200.0 * PI_P) * 0.2;
        double dense = std::sin(t * 600.0 * PI_P + std::cos(t * 15.0) * 3.0) * 0.15;

        return (lush + warm + dense) * (2.0 - params.summerWarmth);
    }

    // Autumn: Rich, mellow characteristics
    double generateAutumnComponent(double t, const IRParams& params) {
        double rich = std::sin(t * 500.0 * PI_P + std::sin(t * 25.0) * 3.5) * 0.22;
        double mellow = std::sin(t * 300.0 * PI_P) * 0.18;
        double golden = std::sin(t * 700.0 * PI_P + std::cos(t * 18.0) * 2.5) * 0.12;

        return rich + mellow + golden;
    }

    // Base room characteristics
    double generateRoomTone(double t, const IRParams& params) {
        double fundamental = std::sin(t * 120.0 * PI_P) * params.roomSize;
        double harmonic2 = std::sin(t * 240.0 * PI_P) * params.roomSize * 0.5;
        double harmonic3 = std::sin(t * 360.0 * PI_P) * params.roomSize * 0.3;

        return fundamental + harmonic2 + harmonic3;
    }

    // Add diffusion and scatter
    double generateDiffusion(double t, const IRParams& params) {
        // Multiple delay lines with different lengths
        double diff1 = std::sin(t * 1500.0 * PI_P + noise(rng) * 0.1) * 0.3;
        double diff2 = std::sin(t * 2300.0 * PI_P + noise(rng) * 0.15) * 0.25;
        double diff3 = std::sin(t * 3700.0 * PI_P + noise(rng) * 0.12) * 0.2;

        return diff1 + diff2 + diff3;
    }
};

// WAV file writer for testing
class WAVWriter {
public:
    static void writeWAV(const std::string& filename,
                         const std::vector<double>& left,
                         const std::vector<double>& right,
                         double sampleRate) {
        std::ofstream file(filename, std::ios::binary);

        int samples = left.size();
        int channels = 2;
        int bitsPerSample = 16;
        int byteRate = static_cast<int>(sampleRate * channels * bitsPerSample / 8);
        int blockAlign = channels * bitsPerSample / 8;
        int dataSize = samples * channels * bitsPerSample / 8;
        int fileSize = 36 + dataSize;

        // WAV header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);

        int fmtSize = 16;
        short audioFormat = 1;
        short numChannels = channels;
        int sampleRateInt = static_cast<int>(sampleRate);
        short bitsPerSampleShort = bitsPerSample;
        short blockAlignShort = blockAlign;

        file.write(reinterpret_cast<const char*>(&fmtSize), 4);
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        file.write(reinterpret_cast<const char*>(&numChannels), 2);
        file.write(reinterpret_cast<const char*>(&sampleRateInt), 4);
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        file.write(reinterpret_cast<const char*>(&blockAlignShort), 2);
        file.write(reinterpret_cast<const char*>(&bitsPerSampleShort), 2);

        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&dataSize), 4);

        // Write interleaved samples
        for (int i = 0; i < samples; ++i) {
            short leftSample = static_cast<short>(std::clamp(left[i], -1.0, 1.0) * 32767);
            short rightSample = static_cast<short>(std::clamp(right[i], -1.0, 1.0) * 32767);

            file.write(reinterpret_cast<const char*>(&leftSample), 2);
            file.write(reinterpret_cast<const char*>(&rightSample), 2);
        }
    }
};

// Usage example
int main2() {
    SeasonalIRGenerator generator(44100.0);

    // Configure parameters
    SeasonalIRGenerator::IRParams params;
    params.length = 5.0;           // 5 second reverb
    params.seasonSpeed = 1.0;      // One full season cycle
    params.winterBrightness = 1.8; // Extra crystalline
    params.summerWarmth = 0.2;     // Very lush
    params.roomSize = 0.9;         // Large room
    params.diffusion = 0.8;        // High scatter

    std::cout << "Generating seasonal IR..." << std::endl;
    auto [left, right] = generator.generateIR(params);

    std::cout << "Writing WAV file..." << std::endl;
    WAVWriter::writeWAV("seasonal_reverb.wav", left, right, 44100.0);

    std::cout << "Generated " << left.size() << " samples" << std::endl;
    std::cout << "Seasonal IR saved as 'seasonal_reverb.wav'" << std::endl;

    return 0;
}

// Alternative: Generate multiple presets
void generatePresets() {
    SeasonalIRGenerator generator(44100.0);

    // Preset configurations
    std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>> presets = {
            {"fast_seasons", {3.0, 2.0, 2.0, 0.1, 0.7, 0.9}},
            {"slow_seasons", {8.0, 0.5, 1.2, 0.4, 1.0, 0.6}},
            {"crystal_cave", {6.0, 1.5, 3.0, 0.8, 0.5, 0.4}},
            {"forest_grove", {4.5, 0.8, 0.8, 0.1, 1.2, 0.9}}
    };

    for (const auto& [name, params] : presets) {
        std::cout << "Generating " << name << "..." << std::endl;
        auto [left, right] = generator.generateIR(params);
        WAVWriter::writeWAV(name + ".wav", left, right, 44100.0);
    }
}



// Generate comprehensive preset collection
void generatePresets2() {
    SeasonalIRGenerator generator(44100.0);

    // Seasonal Movement Presets
    std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>> seasonalPresets = {
            // Speed variations
            {"lightning_seasons", {2.0, 4.0, 2.2, 0.05, 0.6, 1.0}},      // Ultra-fast seasonal changes
            {"racing_seasons", {3.5, 3.0, 1.8, 0.1, 0.8, 0.9}},          // Fast seasonal movement
            {"flowing_seasons", {5.0, 1.5, 1.4, 0.2, 0.9, 0.7}},         // Medium flow
            {"drifting_seasons", {7.0, 0.8, 1.2, 0.3, 1.0, 0.6}},        // Slow drift
            {"eternal_seasons", {12.0, 0.3, 1.0, 0.4, 1.2, 0.5}},        // Very slow evolution
            {"frozen_moment", {6.0, 0.1, 1.6, 0.3, 0.8, 0.4}},           // Almost static
    };

    // Environmental Presets
    std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>> environmentPresets = {
            // Natural spaces
            {"arctic_cathedral", {8.0, 0.6, 3.5, 0.8, 1.5, 0.3}},        // Icy, crystalline, huge
            {"spring_meadow", {4.0, 2.0, 0.8, 0.1, 1.1, 0.9}},           // Fresh, open, airy
            {"summer_forest", {6.0, 1.2, 0.5, 0.05, 1.3, 1.0}},          // Lush, dense, absorbing
            {"autumn_canyon", {9.0, 0.8, 1.0, 0.6, 1.4, 0.7}},           // Deep, resonant, rich

            // Mystical spaces
            {"crystal_caverns", {7.0, 1.8, 4.0, 0.9, 0.4, 0.2}},         // Pure crystal resonance
            {"enchanted_grove", {5.5, 1.4, 0.6, 0.08, 1.6, 1.2}},        // Magical forest
            {"ice_palace", {10.0, 0.9, 3.2, 0.85, 1.8, 0.4}},            // Frozen palace halls
            {"golden_temple", {8.5, 0.7, 1.1, 0.2, 1.7, 0.8}},           // Warm, sacred space

            // Surreal environments
            {"time_spiral", {6.0, 5.0, 2.0, 0.3, 0.9, 0.8}},             // Rapidly cycling time
            {"dream_chamber", {4.5, 0.2, 1.8, 0.15, 0.7, 1.1}},          // Ethereal, floating
            {"memory_vault", {12.0, 0.4, 1.5, 0.5, 2.0, 0.6}},           // Deep, nostalgic
            {"echo_dimension", {15.0, 1.0, 1.3, 0.4, 2.5, 0.9}},         // Infinite space
    };

    // Artistic/Musical Presets
    std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>> artisticPresets = {
            // Instrument-inspired
            {"seasonal_piano", {3.0, 2.5, 1.2, 0.25, 0.8, 0.7}},         // Piano-like resonance
            {"weather_strings", {5.0, 1.6, 1.8, 0.15, 1.1, 0.8}},        // String section ambience
            {"climate_choir", {6.5, 1.3, 1.4, 0.2, 1.3, 0.9}},           // Vocal space simulation
            {"elemental_harp", {4.0, 2.2, 2.1, 0.12, 0.9, 0.6}},         // Harp-like shimmer

            // Emotional landscapes
            {"melancholy_autumn", {8.0, 0.6, 0.9, 0.7, 1.4, 0.5}},       // Sad, contemplative
            {"joyful_spring", {3.5, 2.8, 1.1, 0.08, 1.0, 1.3}},          // Happy, energetic
            {"peaceful_winter", {10.0, 0.4, 2.0, 0.6, 1.6, 0.4}},        // Calm, meditative
            {"passionate_summer", {4.5, 1.8, 0.7, 0.1, 1.2, 1.1}},       // Intense, warm

            // Cinematic
            {"epic_journey", {12.0, 1.0, 1.6, 0.3, 2.2, 0.8}},           // Grand, sweeping
            {"intimate_moment", {2.5, 3.5, 1.3, 0.2, 0.5, 0.9}},         // Close, personal
            {"mysterious_fog", {7.0, 0.8, 1.7, 0.45, 1.5, 1.0}},         // Ethereal, uncertain
            {"heroic_dawn", {6.0, 1.4, 1.9, 0.18, 1.3, 0.7}},            // Triumphant, bright
    };

    // Experimental Presets
    std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>> experimentalPresets = {
            // Extreme parameters
            {"hyper_crystal", {4.0, 2.0, 5.0, 0.95, 0.3, 0.1}},          // Maximum brightness
            {"ultra_lush", {8.0, 1.5, 0.2, 0.01, 2.0, 1.5}},             // Maximum absorption
            {"giant_space", {20.0, 0.5, 1.0, 0.3, 3.0, 0.7}},            // Massive reverb
            {"micro_seasons", {1.5, 8.0, 1.5, 0.2, 0.6, 1.2}},           // Tiny, fast changes

            // Asymmetric seasons
            {"winter_dominant", {6.0, 0.3, 2.8, 0.8, 1.0, 0.5}},         // Mostly winter
            {"summer_focused", {5.0, 0.4, 0.4, 0.02, 1.8, 1.3}},         // Mostly summer
            {"spring_burst", {3.0, 4.0, 1.0, 0.15, 0.9, 1.1}},           // Spring emphasis
            {"autumn_meditation", {10.0, 0.2, 1.1, 0.6, 1.5, 0.4}},      // Autumn focus

            // Unconventional
            {"backwards_seasons", {7.0, -1.0, 1.4, 0.25, 1.1, 0.8}},     // Reverse seasonal flow
            {"chaotic_weather", {5.0, 6.0, 2.5, 0.5, 1.0, 1.4}},         // Unpredictable changes
            {"dual_climate", {8.0, 2.0, 2.0, 0.1, 1.3, 0.6}},            // Two season cycles
            {"temporal_echo", {15.0, 0.1, 1.8, 0.4, 2.5, 0.3}},          // Almost frozen time
    };

    // Generate all presets
    std::vector<std::vector<std::pair<std::string, SeasonalIRGenerator::IRParams>>> allPresets = {
            seasonalPresets, environmentPresets, artisticPresets, experimentalPresets
    };

    std::vector<std::string> categories = {
            "seasonal", "environment", "artistic", "experimental"
    };

    for (size_t cat = 0; cat < allPresets.size(); ++cat) {
        std::cout << "\n=== Generating " << categories[cat] << " presets ===" << std::endl;

        for (const auto& [name, params] : allPresets[cat]) {
            std::cout << "Creating " << name << "..." << std::endl;
            auto [left, right] = generator.generateIR(params);

            std::string filename = categories[cat] + "_" + name + ".wav";
            WAVWriter::writeWAV(filename, left, right, 44100.0);
        }
    }
/*
            SeasonalIRGenerator::IRParams params;
            params.length =  delold * 0.001;           // 5 second reverb
            params.seasonSpeed = 0.1;      // One full season cycle
            params.winterBrightness = 1.8; // Extra crystalline
            params.summerWarmth = 0.4;     // Very lush
            params.roomSize = 2.5;         // Large room
            params.diffusion = 0.3;        // High scatter
            SeasonalIRGenerator generator(_STATE->sr);
*/
    std::cout << "\nGenerated " << (seasonalPresets.size() + environmentPresets.size() +
                                    artisticPresets.size() + experimentalPresets.size())
              << " preset IRs!" << std::endl;
}

// Generate a specific category
void generateCategory(const std::string& category) {
    SeasonalIRGenerator generator(44100.0);

    if (category == "seasonal") {
        std::cout << "Generating seasonal movement presets..." << std::endl;
        // Add seasonal presets generation code here
    }
    // Add other categories as needed
}


#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <functional>
#include <iostream>

class BreathingSpacesGenerator {
private:
    double sampleRate;
    std::mt19937 rng;
    std::uniform_real_distribution<double> noise;
    std::normal_distribution<double> gaussian;

    // Improved noise generators
    double pinkNoise() {
        static double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        double white = gaussian(rng);
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
        b6 = white * 0.115926;
        return pink * 0.11;
    }

public:
    BreathingSpacesGenerator(double sr = 44100.0) :
            sampleRate(sr),
            rng(std::random_device{}()),
            noise(-1.0, 1.0),
            gaussian(0.0, 1.0) {}

    struct BreathingParams {
        double length = 8.0;           // Total IR length
        double breathRate = 0.25;      // Breathing cycles per second
        double breathDepth = 0.7;      // Amplitude of breathing (increased default)
        double asymmetry = 0.6;        // Inhale/exhale ratio
        double complexity = 0.3;       // Multi-layered breathing
        double organicNoise = 0.15;    // Natural irregularity (increased)
        double spatialMovement = 0.5;  // How much the space "moves"
        double resonanceShift = 0.4;   // Frequency changes during breathing

        enum class BreathType {
            HUMAN_CALM,
            HUMAN_DEEP,
            CREATURE,
            WIND,
            MACHINE,
            ORGANIC_SPACE,
            CELESTIAL,
            UNDERWATER
        } breathType = BreathType::HUMAN_CALM;
    };

    std::pair<std::vector<double>, std::vector<double>> generateBreathingIR(const BreathingParams& params) {
        int samples = static_cast<int>(params.length * sampleRate);
        std::vector<double> leftChannel(samples);
        std::vector<double> rightChannel(samples);

        // Pre-calculate breathing envelopes
        std::vector<double> primaryBreath(samples);
        std::vector<double> secondaryBreath(samples);
        std::vector<double> spatialModulation(samples);
        std::vector<double> resonanceModulation(samples);

        calculateBreathingEnvelopes(params, samples, primaryBreath, secondaryBreath,
                                    spatialModulation, resonanceModulation);

        // Generate IR samples
        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double progress = t / params.length;

            // Base decay with breathing modulation (much stronger effect)
            double baseDecay = std::exp(-1.5 * progress);
            double breathingDecay = baseDecay * (0.3 + primaryBreath[i] * params.breathDepth * 2.0);

            // Generate breath-type-specific base sound
            double sample = generateBreathTypeBase(t, params, progress, primaryBreath[i], resonanceModulation[i]);

            // Apply breathing-specific characteristics
            sample = applyBreathingCharacteristics(sample, t, params, primaryBreath[i],
                                                   secondaryBreath[i], resonanceModulation[i]);

            // Add organic irregularities (more prominent)
            if (params.organicNoise > 0.01) {
                sample += generateOrganicNoise(t, params, primaryBreath[i]) * params.organicNoise;
            }

            // Apply breathing modulation
            sample *= breathingDecay;

            // Enhanced spatial breathing (stereo movement)
            double spatialOffset = spatialModulation[i] * params.spatialMovement;
            double spatialWidth = 0.6; // Wider stereo field
            leftChannel[i] = sample * (1.0 - spatialOffset * spatialWidth);
            rightChannel[i] = sample * (1.0 + spatialOffset * spatialWidth);

            // Add slight delay between channels for more realistic space
            if (i > 10) {
                leftChannel[i] += rightChannel[i-10] * 0.1;
                rightChannel[i] += leftChannel[i-10] * 0.1;
            }
        }

        return {leftChannel, rightChannel};
    }

private:
    void calculateBreathingEnvelopes(const BreathingParams& params, int samples,
                                     std::vector<double>& primary, std::vector<double>& secondary,
                                     std::vector<double>& spatial, std::vector<double>& resonance) {

        for (int i = 0; i < samples; ++i) {
            double t = static_cast<double>(i) / sampleRate;

            // Primary breathing cycle
            primary[i] = calculateBreathCycle(t, params.breathRate, params.asymmetry, params.breathType);

            // Secondary breathing (for complexity)
            if (params.complexity > 0.01) {
                double secondaryRate = params.breathRate * (0.7 + params.complexity * 0.8);
                secondary[i] = calculateBreathCycle(t, secondaryRate, 0.8 - params.asymmetry, params.breathType)
                               * params.complexity * 0.6;
                primary[i] = primary[i] * (1.0 - params.complexity * 0.4) + secondary[i];
            }

            // Enhanced spatial modulation
            spatial[i] = std::sin(t * params.breathRate * 2.0 * PI_P * 0.4) *
                         (1.0 + std::sin(t * params.breathRate * PI_P * 0.15) * 0.5);

            // Resonance modulation
            resonance[i] = calculateResonanceShift(t, params);

            // Add organic irregularity
            if (params.organicNoise > 0.01) {
                primary[i] += gaussian(rng) * params.organicNoise * 0.15;
                primary[i] = std::clamp(primary[i], -1.0, 1.0);
            }
        }
    }

    // NEW: Breath-type-specific base sound generation
    double generateBreathTypeBase(double t, const BreathingParams& params, double progress,
                                  double breathValue, double resonanceMod) {
        double sample = 0.0;
        double decayFactor = std::exp(-progress * 2.0);

        switch (params.breathType) {
            case BreathingParams::BreathType::HUMAN_CALM: {
                // Warm, natural room tone with gentle resonances
                sample += std::sin(t * 220.0 * PI_P * resonanceMod) * 0.4 * decayFactor;
                sample += std::sin(t * 440.0 * PI_P * resonanceMod) * 0.25 * decayFactor;
                sample += std::sin(t * 660.0 * PI_P * resonanceMod) * 0.15 * decayFactor;
                // Add subtle air movement
                sample += pinkNoise() * 0.1 * decayFactor * (0.5 + breathValue * 0.5);
                break;
            }

            case BreathingParams::BreathType::HUMAN_DEEP: {
                // Deeper, more resonant
                sample += std::sin(t * 110.0 * PI_P * resonanceMod) * 0.5 * decayFactor;
                sample += std::sin(t * 330.0 * PI_P * resonanceMod) * 0.35 * decayFactor;
                sample += std::sin(t * 550.0 * PI_P * resonanceMod) * 0.2 * decayFactor;
                // Deep breathing harmonics
                sample += std::sin(t * 55.0 * PI_P * resonanceMod) * breathValue * 0.3 * decayFactor;
                break;
            }

            case BreathingParams::BreathType::CREATURE: {
                // Lower frequencies, growling undertones
                sample += std::sin(t * 60.0 * PI_P * resonanceMod) * 0.6 * decayFactor;
                sample += std::sin(t * 120.0 * PI_P * resonanceMod) * 0.4 * decayFactor;
                sample += std::sin(t * 180.0 * PI_P * resonanceMod) * 0.3 * decayFactor;
                // Add rumbling
                sample += std::sin(t * 30.0 * PI_P + breathValue * 2.0) * 0.4 * decayFactor;
                // Creature-like texture
                sample += pinkNoise() * 0.2 * decayFactor * std::abs(breathValue);
                break;
            }

            case BreathingParams::BreathType::WIND: {
                // High-frequency noise with varying intensity
                sample += pinkNoise() * 0.6 * decayFactor * (0.3 + breathValue * 0.7);
                sample += noise(rng) * 0.4 * decayFactor * (0.2 + breathValue * 0.8);
                // Wind harmonics
                sample += std::sin(t * 880.0 * PI_P + noise(rng) * 0.5) * 0.3 * decayFactor;
                sample += std::sin(t * 1320.0 * PI_P + noise(rng) * 0.3) * 0.2 * decayFactor;
                break;
            }

            case BreathingParams::BreathType::MACHINE: {
                // Precise, mechanical frequencies
                sample += std::sin(t * 440.0 * PI_P) * 0.4 * decayFactor;
                sample += std::sin(t * 880.0 * PI_P) * 0.3 * decayFactor;
                sample += std::sin(t * 1320.0 * PI_P) * 0.2 * decayFactor;
                // Mechanical modulation
                double mechMod = breathValue > 0 ? 1.0 : 0.7; // Sharp on/off
                sample *= mechMod;
                // Add slight mechanical buzz
                sample += std::sin(t * 60.0 * PI_P) * 0.1 * decayFactor;
                break;
            }

            case BreathingParams::BreathType::ORGANIC_SPACE: {
                // Complex harmonic series
                for (int h = 1; h <= 6; ++h) {
                    double harmonic = std::sin(t * 220.0 * h * PI_P * resonanceMod) *
                                      (0.5 / h) * decayFactor;
                    harmonic *= (1.0 + breathValue * 0.3 * std::sin(h * PI_P * 0.5));
                    sample += harmonic;
                }
                // Slow modulation
                sample *= (1.0 + std::sin(t * 0.1 * PI_P) * 0.2);
                break;
            }

            case BreathingParams::BreathType::CELESTIAL: {
                // Ethereal, high frequencies
                sample += std::sin(t * 880.0 * PI_P * resonanceMod) * 0.3 * decayFactor;
                sample += std::sin(t * 1320.0 * PI_P * resonanceMod) * 0.25 * decayFactor;
                sample += std::sin(t * 1760.0 * PI_P * resonanceMod) * 0.2 * decayFactor;
                // Cosmic modulation
                sample *= (1.0 + std::sin(t * 0.05 * PI_P + breathValue) * 0.4);
                // Shimmering effect
                sample += std::sin(t * 2640.0 * PI_P + std::sin(t * 0.3) * 2.0) * 0.15 * decayFactor;
                break;
            }

            case BreathingParams::BreathType::UNDERWATER: {
                // Muffled, bubble-like
                sample += std::sin(t * 110.0 * PI_P * resonanceMod) * 0.5 * decayFactor;
                sample += std::sin(t * 165.0 * PI_P * resonanceMod) * 0.3 * decayFactor;
                // Bubble effects
                double bubblePhase = t * 8.0 + breathValue * 4.0;
                sample += std::sin(bubblePhase * PI_P) * std::exp(-std::fmod(bubblePhase, 1.0) * 5.0) * 0.3;
                // Pressure waves
                sample *= (1.0 + breathValue * 0.5) * std::exp(-progress * 0.5);
                break;
            }
        }

        return sample;
    }

    double calculateBreathCycle(double t, double rate, double asymmetry,
                                BreathingParams::BreathType breathType) {
        double phase = std::fmod(t * rate, 1.0);
        double breathValue = 0.0;

        switch (breathType) {
            case BreathingParams::BreathType::HUMAN_CALM:
                breathValue = calculateHumanCalm(phase, asymmetry);
                break;
            case BreathingParams::BreathType::HUMAN_DEEP:
                breathValue = calculateHumanDeep(phase, asymmetry);
                break;
            case BreathingParams::BreathType::CREATURE:
                breathValue = calculateCreature(phase, asymmetry);
                break;
            case BreathingParams::BreathType::WIND:
                breathValue = calculateWind(phase, asymmetry, t);
                break;
            case BreathingParams::BreathType::MACHINE:
                breathValue = calculateMachine(phase, asymmetry);
                break;
            case BreathingParams::BreathType::ORGANIC_SPACE:
                breathValue = calculateOrganicSpace(phase, asymmetry, t);
                break;
            case BreathingParams::BreathType::CELESTIAL:
                breathValue = calculateCelestial(phase, asymmetry, t);
                break;
            case BreathingParams::BreathType::UNDERWATER:
                breathValue = calculateUnderwater(phase, asymmetry, t);
                break;
        }

        return breathValue;
    }

    // Enhanced breathing pattern implementations
    double calculateHumanCalm(double phase, double asymmetry) {
        if (phase < asymmetry) {
            double inhalePhase = phase / asymmetry;
            return std::pow(std::sin(inhalePhase * PI_P * 0.5), 0.8);
        } else {
            double exhalePhase = (phase - asymmetry) / (1.0 - asymmetry);
            return std::cos(exhalePhase * PI_P * 0.5) * std::exp(-exhalePhase * 0.7);
        }
    }

    double calculateHumanDeep(double phase, double asymmetry) {
        if (phase < asymmetry) {
            double inhalePhase = phase / asymmetry;
            return std::pow(std::sin(inhalePhase * PI_P * 0.5), 0.6);
        } else {
            double exhalePhase = (phase - asymmetry) / (1.0 - asymmetry);
            return std::exp(-exhalePhase * 1.5) * std::cos(exhalePhase * PI_P * 0.4);
        }
    }

    double calculateCreature(double phase, double asymmetry) {
        if (phase < asymmetry) {
            double inhalePhase = phase / asymmetry;
            double rumble = std::sin(inhalePhase * PI_P * 12.0) * 0.15;
            return std::pow(inhalePhase, 0.4) * (1.0 + rumble);
        } else {
            double exhalePhase = (phase - asymmetry) / (1.0 - asymmetry);
            double growl = std::sin(exhalePhase * PI_P * 8.0) * 0.25;
            return (1.0 - exhalePhase) * (0.7 + growl);
        }
    }

    double calculateWind(double phase, double asymmetry, double t) {
        double baseWind = std::sin(phase * PI_P * 2.0);
        double gust = std::sin(t * 0.4 + phase * PI_P) * 0.4;
        double turbulence = pinkNoise() * 0.3;
        return (baseWind + gust + turbulence) * 0.6;
    }

    double calculateMachine(double phase, double asymmetry) {
        // More mechanical with sudden transitions
        if (phase < asymmetry) {
            double inhalePhase = phase / asymmetry;
            return std::min(inhalePhase * 2.0, 1.0); // Fast rise
        } else {
            double exhalePhase = (phase - asymmetry) / (1.0 - asymmetry);
            return std::max(1.0 - exhalePhase * 3.0, 0.0); // Fast fall
        }
    }

    double calculateOrganicSpace(double phase, double asymmetry, double t) {
        double fundamental = std::sin(phase * PI_P * 2.0);
        double harmonic2 = std::sin(phase * PI_P * 3.0) * 0.6;
        double harmonic3 = std::sin(phase * PI_P * 5.0) * 0.4;
        double slowModulation = std::sin(t * 0.08) * 0.3;
        double verySlowMod = std::sin(t * 0.03) * 0.2;

        return (fundamental + harmonic2 + harmonic3 + slowModulation + verySlowMod) * 0.5;
    }

    double calculateCelestial(double phase, double asymmetry, double t) {
        double primary = std::sin(phase * PI_P * 2.0);
        double cosmic = std::sin(phase * PI_P * 2.618) * 0.5; // Golden ratio
        double ethereal = std::sin(phase * PI_P * 1.618) * 0.4;
        double drift = std::sin(t * 0.02 + phase * PI_P) * 0.3;

        return (primary + cosmic + ethereal + drift) * 0.4;
    }

    double calculateUnderwater(double phase, double asymmetry, double t) {
        double bubble = std::sin(phase * PI_P * 2.0) * std::exp(-phase * 2.0);
        double pressure = std::sin(phase * PI_P * 1.3 + std::sin(t * 1.2) * 0.8) * 0.7;
        double underwater = std::sin(phase * PI_P * 0.7) * 0.5; // Slow movement

        return (bubble + pressure + underwater) * 0.6;
    }

    double calculateResonanceShift(double t, const BreathingParams& params) {
        double breathPhase = std::fmod(t * params.breathRate, 1.0);
        double shift = std::sin(breathPhase * PI_P * 2.0) * params.resonanceShift;
        // Add some irregularity
        shift += std::sin(breathPhase * PI_P * 3.14159 + t * 0.1) * params.resonanceShift * 0.3;
        return 1.0 + shift * 0.3; // ±30% frequency shift
    }

    double applyBreathingCharacteristics(double sample, double t, const BreathingParams& params,
                                         double primaryBreath, double secondaryBreath, double resonanceMod) {
        double modifiedSample = sample;

        // Type-specific breathing effects
        switch (params.breathType) {
            case BreathingParams::BreathType::CREATURE:
                // Add rumbling during strong breaths
                if (std::abs(primaryBreath) > 0.5) {
                    modifiedSample += std::sin(t * 40.0 * PI_P) * primaryBreath * 0.4;
                }
                break;

            case BreathingParams::BreathType::WIND:
                // Add gusting
                modifiedSample += pinkNoise() * std::abs(primaryBreath) * 0.3;
                break;

            case BreathingParams::BreathType::UNDERWATER:
                // Muffle high frequencies during exhale
                if (primaryBreath < 0) {
                    modifiedSample *= (1.0 + primaryBreath * 0.4);
                }
                break;

            case BreathingParams::BreathType::CELESTIAL:
                // Add shimmer
                modifiedSample += std::sin(t * 2200.0 * PI_P + primaryBreath * 4.0) * 0.1;
                break;
        }

        // Enhanced breathing harmonics
        double breathHarmonics = 0.0;
        breathHarmonics += std::sin(t * 80.0 * PI_P) * primaryBreath * 0.15;
        breathHarmonics += std::sin(t * 160.0 * PI_P) * primaryBreath * 0.1;

        modifiedSample += breathHarmonics;

        return modifiedSample;
    }

    double generateOrganicNoise(double t, const BreathingParams& params, double breathValue) {
        double organic = 0.0;

        // Breathing-synchronized organic noise
        double breathIntensity = 0.3 + std::abs(breathValue) * 0.7;

        organic += pinkNoise() * 0.4 * breathIntensity;
        organic += noise(rng) * 0.2 * breathIntensity * std::exp(-std::fmod(t * 2.0, 1.0) * 3.0);
        organic += std::sin(t * 17.3 * PI_P + noise(rng) * 0.5) * 0.15 * breathIntensity;

        // Add some crackling for certain types
        if (params.breathType == BreathingParams::BreathType::CREATURE ||
            params.breathType == BreathingParams::BreathType::WIND) {
            if (noise(rng) > 0.98) { // Occasional pops
                organic += noise(rng) * 0.5;
            }
        }

        return organic * 0.25;
    }
};

// Preset generator for breathing spaces
void generateBreathingPresets() {
    BreathingSpacesGenerator generator(44100.0);

    std::vector<std::pair<std::string, BreathingSpacesGenerator::BreathingParams>> presets;

    // Human breathing variations
    BreathingSpacesGenerator::BreathingParams humanCalm;
    humanCalm.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::HUMAN_CALM;
    humanCalm.breathRate = 0.2; // 12 breaths per minute
    humanCalm.breathDepth = 0.3;
    humanCalm.asymmetry = 0.4; // Shorter inhale
    presets.push_back({"human_calm_breathing", humanCalm});

    BreathingSpacesGenerator::BreathingParams humanDeep;
    humanDeep.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::HUMAN_DEEP;
    humanDeep.breathRate = 0.1; // 6 breaths per minute
    humanDeep.breathDepth = 0.6;
    humanDeep.asymmetry = 0.7; // Long inhale
    humanDeep.length = 12.0;
    presets.push_back({"meditation_breathing", humanDeep});

    // Creature breathing
    BreathingSpacesGenerator::BreathingParams creature;
    creature.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::CREATURE;
    creature.breathRate = 0.05; // Very slow, 3 breaths per minute
    creature.breathDepth = 0.8;
    creature.asymmetry = 0.6;
    creature.complexity = 0.4;
    creature.length = 15.0;
    presets.push_back({"dragon_breathing", creature});

    // Wind breathing
    BreathingSpacesGenerator::BreathingParams wind;
    wind.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::WIND;
    wind.breathRate = 0.15;
    wind.breathDepth = 0.7;
    wind.organicNoise = 0.4;
    wind.spatialMovement = 0.8;
    presets.push_back({"wind_cave", wind});

    // Machine breathing
    BreathingSpacesGenerator::BreathingParams machine;
    machine.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::MACHINE;
    machine.breathRate = 0.5; // Fast mechanical
    machine.breathDepth = 0.5;
    machine.asymmetry = 0.5; // Perfectly symmetric
    machine.organicNoise = 0.0; // No irregularity
    presets.push_back({"mechanical_lung", machine});

    // Organic space
    BreathingSpacesGenerator::BreathingParams organicSpace;
    organicSpace.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::ORGANIC_SPACE;
    organicSpace.breathRate = 0.08; // Very slow
    organicSpace.breathDepth = 0.9;
    organicSpace.complexity = 0.6;
    organicSpace.spatialMovement = 0.7;
    organicSpace.length = 20.0;
    presets.push_back({"living_cathedral", organicSpace});

    // Celestial breathing
    BreathingSpacesGenerator::BreathingParams celestial;
    celestial.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::CELESTIAL;
    celestial.breathRate = 0.03; // Extremely slow
    celestial.breathDepth = 0.6;
    celestial.complexity = 0.8;
    celestial.resonanceShift = 0.5;
    celestial.length = 30.0;
    presets.push_back({"cosmic_breathing", celestial});

    // Underwater breathing
    BreathingSpacesGenerator::BreathingParams underwater;
    underwater.breathType = BreathingSpacesGenerator::BreathingParams::BreathType::UNDERWATER;
    underwater.breathRate = 0.12;
    underwater.breathDepth = 0.4;
    underwater.organicNoise = 0.3;
    underwater.resonanceShift = 0.2;
    presets.push_back({"submerged_chamber", underwater});

    // Generate all presets
    for (const auto& [name, params] : presets) {
        std::cout << "Generating breathing space: " << name << "..." << std::endl;
        auto [left, right] = generator.generateBreathingIR(params);

        // Simple WAV writer (same as previous examples)
        // WAVWriter::writeWAV(name + ".wav", left, right, 44100.0);
    }

    std::cout << "Generated " << presets.size() << " breathing space IRs!" << std::endl;
}
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <complex>
#include <functional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

// RMS/Energy normalization - maintains consistent output energy
template<typename T>
void normalize_ir_rms(T& ir) {
    if (ir.empty()) return;

    // Calculate RMS (Root Mean Square)
    using ValueType = typename T::value_type;
    ValueType sum_squares = std::accumulate(ir.begin(), ir.end(), ValueType{0},
                                            [](ValueType sum, ValueType val) { return sum + val * val; });

    ValueType rms = std::sqrt(sum_squares / ir.size());

    if (rms > ValueType{0}) {
        ValueType scale = ValueType{1} / rms;
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });
    }
}

// Energy normalization (without dividing by length)
template<typename T>
void normalize_ir_energy(T& ir) {
    if (ir.empty()) return;

    // Calculate total energy (sum of squares)
    using ValueType = typename T::value_type;
    ValueType energy = std::accumulate(ir.begin(), ir.end(), ValueType{0},
                                       [](ValueType sum, ValueType val) { return sum + val * val; });

    if (energy > ValueType{0}) {
        ValueType scale = ValueType{1} / std::sqrt(energy);
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });
    }
}

// Target RMS normalization - normalize to specific RMS level
template<typename T>
void normalize_ir_to_rms(T& ir, typename T::value_type target_rms = typename T::value_type{1}) {
    if (ir.empty()) return;

    using ValueType = typename T::value_type;
    ValueType sum_squares = std::accumulate(ir.begin(), ir.end(), ValueType{0},
                                            [](ValueType sum, ValueType val) { return sum + val * val; });

    ValueType current_rms = std::sqrt(sum_squares / ir.size());

    if (current_rms > ValueType{0}) {
        ValueType scale = target_rms / current_rms;
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });
    }
}

// Practical audio normalization - combines energy with safety limiting
template<typename T>
void normalize_ir_audio_consistent(T& ir,
                                   typename T::value_type target_rms = typename T::value_type{0.5},
                                   typename T::value_type max_peak = typename T::value_type{0.95}) {
    if (ir.empty()) return;

    using ValueType = typename T::value_type;
    // First normalize by energy to target RMS
    ValueType sum_squares = std::accumulate(ir.begin(), ir.end(), ValueType{0},
                                            [](ValueType sum, ValueType val) { return sum + val * val; });

    ValueType current_rms = std::sqrt(sum_squares / ir.size());

    if (current_rms > ValueType{0}) {
        ValueType scale = target_rms / current_rms;

        // Apply initial scaling
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });

        // Check if any peaks exceed limit and apply additional limiting if needed
        ValueType max_val = *std::max_element(ir.begin(), ir.end(),
                                              [](ValueType a, ValueType b) { return std::abs(a) < std::abs(b); });
        max_val = std::abs(max_val);

        if (max_val > max_peak) {
            ValueType limiter_scale = max_peak / max_val;
            std::transform(ir.begin(), ir.end(), ir.begin(),
                           [limiter_scale](ValueType val) { return val * limiter_scale; });
        }
    }
}

// Test-signal based consistent level normalization
template<typename T, typename U>
void normalize_ir_by_test_response_rms(T& ir, const U& test_signal) {
    if (ir.empty() || test_signal.empty()) return;

    using ValueType = typename T::value_type;
    // Convolve IR with test signal
    size_t conv_size = ir.size() + test_signal.size() - 1;
    std::vector<ValueType> convolved(conv_size, ValueType{0});

    for (size_t i = 0; i < ir.size(); ++i) {
        for (size_t j = 0; j < test_signal.size(); ++j) {
            convolved[i + j] += ir[i] * test_signal[j];
        }
    }

    // Calculate RMS of input and output
    ValueType input_energy = std::accumulate(test_signal.begin(), test_signal.end(), ValueType{0},
                                             [](ValueType sum, ValueType val) { return sum + val * val; });
    ValueType input_rms = std::sqrt(input_energy / test_signal.size());

    ValueType output_energy = std::accumulate(convolved.begin(), convolved.end(), ValueType{0},
                                              [](ValueType sum, ValueType val) { return sum + val * val; });
    ValueType output_rms = std::sqrt(output_energy / convolved.size());

    // Scale IR to match input/output RMS ratio
    if (output_rms > ValueType{0} && input_rms > ValueType{0}) {
        ValueType scale = input_rms / output_rms;
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });
    }
}

// Overload with default test signal
template<typename T>
void normalize_ir_by_test_response_rms(T& ir) {
    using ValueType = typename T::value_type;
    std::vector<ValueType> test_signal = {ValueType{1}};
    normalize_ir_by_test_response_rms(ir, test_signal);
}

// White noise test for consistent level (most robust for audio)
template<typename T>
void normalize_ir_white_noise_test(T& ir, size_t test_length = 1024) {
    if (ir.empty()) return;

    using ValueType = typename T::value_type;
    // Generate white noise test signal
    std::vector<ValueType> noise(test_length);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<ValueType> dist(ValueType{0}, ValueType{1});

    for (auto& sample : noise) {
        sample = dist(gen);
    }

    // Normalize noise to unit RMS
    ValueType noise_energy = std::accumulate(noise.begin(), noise.end(), ValueType{0},
                                             [](ValueType sum, ValueType val) { return sum + val * val; });
    ValueType noise_rms = std::sqrt(noise_energy / noise.size());

    if (noise_rms > ValueType{0}) {
        ValueType noise_scale = ValueType{1} / noise_rms;
        std::transform(noise.begin(), noise.end(), noise.begin(),
                       [noise_scale](ValueType val) { return val * noise_scale; });
    }

    // Convolve with IR
    size_t conv_size = ir.size() + noise.size() - 1;
    std::vector<ValueType> convolved(conv_size, ValueType{0});

    for (size_t i = 0; i < ir.size(); ++i) {
        for (size_t j = 0; j < noise.size(); ++j) {
            convolved[i + j] += ir[i] * noise[j];
        }
    }

    // Calculate output RMS
    ValueType output_energy = std::accumulate(convolved.begin(), convolved.end(), ValueType{0},
                                              [](ValueType sum, ValueType val) { return sum + val * val; });
    ValueType output_rms = std::sqrt(output_energy / convolved.size());

    // Scale IR to maintain unit output RMS
    if (output_rms > ValueType{0}) {
        ValueType scale = ValueType{1} / output_rms;
        std::transform(ir.begin(), ir.end(), ir.begin(),
                       [scale](ValueType val) { return val * scale; });
    }
}

template<typename T>
class BeautifulIRGenerator {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<double> uniform;
    std::normal_distribution<double> gaussian;

    static constexpr double TAU = 2.0f * PI_P;

public:
    BeautifulIRGenerator() : rng(std::random_device{}()), uniform(0.0f, 1.0f), gaussian(0.0f, 1.0f) {}
    double computeDecayFactor(double decay_control, int length) {
        // Clamp control input
        decay_control = std::clamp(decay_control, 0.0, 1.0);

        // Map control to decay end ratio (i.e., how long it sustains)
        // Exponential scale: fast decay at low values, slower at higher
        double end_gain = 0.001; // -60 dB

        // We want: envelope = pow(decay_factor, i)
        // And: envelope[length - 1] ≈ end_gain
        // => decay_factor = end_gain ^ (1.0 / (length * decay_control))

        // To avoid division by zero, use small minimum decay_control
        double effective_length = std::max(1.0, decay_control * length);

        return std::pow(end_gain, 1.0 / effective_length);
    }
    // Golden Ratio Decay - Creates naturally pleasing decay curves

    void goldenRatioHall(T& ir, int length, double dec = 1.0f) {
        const double phi = 1.618033988749f; // Golden ratio
        double decay = computeDecayFactor(dec, length);
        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / 44100.0f;

            // Golden ratio modulated reflections
            double envelope = std::pow(decay, i);
            double modulation = std::sin(TAU * t * phi) * std::exp(-t * phi * 0.5f);

            // Fibonacci sequence reflections
            int fib_idx = static_cast<int>(t * phi * 10) % 13;
            double fibonacci_weight = fibonacci(fib_idx) / 233.0f; // Normalize by F(13)

            ir[i] = envelope * modulation * fibonacci_weight;

            // Add sparse early reflections at golden ratio intervals
            if (i > 0 && (i % static_cast<int>(44100 * 0.618f / 20)) == 0) {
                ir[i] += envelope * 0.3f * (uniform(rng) * 2.0f - 1.0f);
            }
        }

        normalizeIR(ir);
    }

    // Lorenz Attractor Reverb - Chaotic but beautiful
    void lorenzChaosReverb(T& ir, int length, double roomSize = 1.0f) {

        // Lorenz attractor parameters
        double sigma = 10.0f, rho = 28.0f, beta = 8.0f / 3.0f;
        double x = 1.0f, y = 1.0f, z = 1.0f;
        double dt = 0.01f * roomSize;

        double decay = std::pow(0.001f, 1.0f / (length * 0.7f));

        for (int i = 0; i < length; ++i) {
            // Evolve Lorenz system
            double dx = sigma * (y - x) * dt;
            double dy = (x * (rho - z) - y) * dt;
            double dz = (x * y - beta * z) * dt;

            x += dx; y += dy; z += dz;

            // Use attractor coordinates to generate reverb
            double envelope = std::pow(decay, i);
            double chaos_amp = std::tanh(x * 0.1f) * std::sin(y * 0.1f) * std::cos(z * 0.1f);

            ir[i] = envelope * chaos_amp * 0.5f;
        }

        normalizeIR(ir);
    }

    // Mandelbrot Set Reverb - Fractal beauty in audio
    void mandelbrotReverb(T& ir, int length, double complexity = 2.0f) {
        double decay = std::pow(0.001f, 1.0f / (length * 0.8f));

        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / length;

            // Map time to complex plane
            std::complex<double> c(-0.8f + t * 1.6f, -0.4f + (i % 100) * 0.008f);
            std::complex<double> z(0.0f, 0.0f);

            int iterations = 0;
            int max_iter = 50;

            // Mandelbrot iteration
            while (std::abs(z) < 2.0f && iterations < max_iter) {
                z = z * z + c;
                ++iterations;
            }

            double envelope = std::pow(decay, i);
            double fractal_intensity = static_cast<double>(iterations) / max_iter;

            // Create musical intervals from fractal
            double harmonic = std::sin(TAU * t * complexity * fractal_intensity * 5.0f);

            ir[i] = envelope * harmonic * fractal_intensity * 0.4f;
        }

        normalizeIR(ir);
    }

    // Crystalline Cathedral - Based on crystal lattice structures
    void crystallineCathedral(T& ir, int length, int harmonics = 8) {
        double decay = std::pow(0.001f, 1.0f / (length * 0.9f));

        // Crystal lattice frequencies (based on quartz structure)
        std::vector<double> crystal_freqs = {1.0f, 1.414f, 1.732f, 2.0f, 2.236f, 2.449f, 2.646f, 2.828f};

        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / 44100.0f;
            double envelope = std::pow(decay, i);
            double sample = 0.0f;

            // Generate crystalline harmonics
            for (int h = 0; h < harmonics && h < crystal_freqs.size(); ++h) {
                double freq = crystal_freqs[h] * 100.0f; // Base frequency
                double harmonic_decay = std::exp(-t * freq * 0.01f);
                double phase_mod = std::sin(TAU * t * freq * 0.1f) * 0.2f;

                sample += std::sin(TAU * t * freq + phase_mod) * harmonic_decay / (h + 1);
            }

            // Add crystalline reflections
            if (i % 2205 == 0) { // Every 50ms at 44.1kHz
                sample += envelope * 0.4f * std::sin(TAU * t * 440.0f);
            }

            ir[i] = envelope * sample * 0.3f;
        }

        normalizeIR(ir);
    }

    // Fibonacci Spiral Reverb - Natural spiral patterns
    void fibonacciSpiral(T& ir, int length, double spiral_speed = 1.0f) {
        double decay = std::pow(0.001f, 1.0f / (length * 0.85f));

        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / 44100.0f;
            double envelope = std::pow(decay, i);

            // Fibonacci spiral coordinates
            double phi = 1.618033988749f;
            double theta = t * spiral_speed * TAU * phi;
            double radius = std::sqrt(t) * phi;

            double x = radius * std::cos(theta);
            double y = radius * std::sin(theta);

            // Generate reflections along spiral
            double spiral_mod = std::sin(x * 0.5f) * std::cos(y * 0.3f);

            // Fibonacci number modulation
            int fib_n = static_cast<int>(t * 10) % 21;
            double fib_weight = fibonacci(fib_n) / 10946.0f; // Normalize

            ir[i] = envelope * spiral_mod * fib_weight * 0.6f;

            // Add golden angle reflections
            if (i > 0 && (i % static_cast<int>(44100 * 2.399963f / 100)) == 0) {
                ir[i] += envelope * 0.2f * (uniform(rng) * 2.0f - 1.0f);
            }
        }

        normalizeIR(ir);
    }

    // Harmonic Series Cathedral - Pure mathematical harmony
    void harmonicCathedral(T& ir, int length, double fundamental = 55.0f) {
        double decay = std::pow(0.001f, 1.0f / (length * 1.2f));

        // Just intonation ratios for pure harmony
        std::vector<double> ratios = {
                1.0f,           // Unison
                9.0f/8.0f,      // Major second
                5.0f/4.0f,      // Major third
                4.0f/3.0f,      // Perfect fourth
                3.0f/2.0f,      // Perfect fifth
                5.0f/3.0f,      // Major sixth
                15.0f/8.0f,     // Major seventh
                2.0f            // Octave
        };

        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / 44100.0f;
            double envelope = std::pow(decay, i);
            double sample = 0.0f;

            // Generate harmonic series with natural decay
            for (int h = 0; h < ratios.size(); ++h) {
                double freq = fundamental * ratios[h];
                double harmonic_envelope = std::exp(-t * freq * 0.002f);
                double phase_drift = uniform(rng) * 0.01f * t; // Slight natural drift

                sample += std::sin(TAU * t * freq + phase_drift) *
                          harmonic_envelope / std::sqrt(ratios[h]);
            }

            // Add cathedral-like early reflections
            double reflection_time = 0.05f + 0.15f * uniform(rng);
            int reflection_idx = static_cast<int>(reflection_time * 44100);
            if (i == reflection_idx) {
                sample += envelope * 0.3f * std::sin(TAU * t * fundamental * 2.0f);
            }

            ir[i] = envelope * sample * 0.25f;
        }

        normalizeIR(ir);
    }

    // Quantum Resonator - Based on quantum harmonic oscillator
    void quantumResonator(T& ir, int length, int quantum_levels = 16) {
        double decay = std::pow(0.001f, 1.0f / (length * 0.6f));

        for (int i = 0; i < length; ++i) {
            double t = static_cast<double>(i) / 44100.0f;
            double envelope = std::pow(decay, i);
            double sample = 0.0f;

            // Quantum harmonic oscillator energy levels
            for (int n = 0; n < quantum_levels; ++n) {
                double energy = (n + 0.5f) * 100.0f; // Scaled for audio
                double probability = std::exp(-energy * t * 0.01f);

                // Hermite polynomial approximation for quantum states
                double hermite = hermitePolynomial(n, t * 10.0f - 5.0f);
                double gaussian = std::exp(-(t * 10.0f - 5.0f) * (t * 10.0f - 5.0f) * 0.5f);

                sample += hermite * gaussian * probability / std::sqrt(factorial(n));
            }

            ir[i] = envelope * sample * 0.1f;
        }

        normalizeIR(ir);
    }

private:
    // Helper functions
    int fibonacci(int n) {
        if (n <= 1) return n;
        int a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            int temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    double hermitePolynomial(int n, double x) {
        if (n == 0) return 1.0f;
        if (n == 1) return 2.0f * x;

        double h0 = 1.0f, h1 = 2.0f * x;
        for (int i = 2; i <= n; ++i) {
            double h2 = 2.0f * x * h1 - 2.0f * (i - 1) * h0;
            h0 = h1;
            h1 = h2;
        }
        return h1;
    }

    int factorial(int n) {
        int result = 1;
        for (int i = 2; i <= n && i <= 10; ++i) { // Cap at 10 for numerical stability
            result *= i;
        }
        return result;
    }




    void normalizeIR2(T& ir) {
        double max_val = 0.0f;
        for (double sample : ir) {
            max_val = std::max(max_val, std::abs(sample));
        }

        if (max_val > 0.0f) {
            double scale = 0.8f / max_val; // Leave some headroom
            for (double& sample : ir) {
                sample *= scale;
            }
        }
    }
};
double computeDecayFactor(double decay_control, int length) {
    // Clamp control input
    decay_control = std::clamp(decay_control, 0.0, 1.0);

    // Map control to decay end ratio (i.e., how long it sustains)
    // Exponential scale: fast decay at low values, slower at higher
    double end_gain = 0.001; // -60 dB

    // We want: envelope = pow(decay_factor, i)
    // And: envelope[length - 1] ≈ end_gain
    // => decay_factor = end_gain ^ (1.0 / (length * decay_control))

    // To avoid division by zero, use small minimum decay_control
    double effective_length = std::max(1.0, decay_control * length);

    return std::pow(end_gain, 1.0 / effective_length);
}

#include <vector>
#include <cmath>
#include <random>

class ShimmerIRGenerator {
private:
    float sampleRate;
    std::mt19937 rng;

    // Generate a smooth envelope with attack and decay
    float envelope(int sample, int attackSamples, int decaySamples, int totalSamples) {
        if (sample < attackSamples) {
            return (float)sample / attackSamples;
        }
        int decayStart = totalSamples - decaySamples;
        if (sample > decayStart) {
            return 1.0f - (float)(sample - decayStart) / decaySamples;
        }
        return 1.0f;
    }

    // Create harmonic content for shimmer effect
    float harmonicTone(int sample, float frequency, int harmonics) {
        float output = 0.0f;
        float time = (float)sample / sampleRate;

        for (int h = 1; h <= harmonics; h++) {
            float harmFreq = frequency * h;
            float amplitude = 1.0f / (h * h); // Gentle harmonic rolloff
            output += amplitude * sin(2.0f * PI_P * harmFreq * time);
        }
        return output * 0.3f; // Scale down
    }

public:
    ShimmerIRGenerator(float sr) : sampleRate(sr), rng(std::random_device{}()) {}

    std::vector<float> generateShimmerIR(float lengthSeconds = 8.0f) {
        int totalSamples = (int)(lengthSeconds * sampleRate);
        std::vector<float> ir(totalSamples, 0.0f);

        // Main reflection pattern - creates the "space"
        struct Reflection {
            float delay;      // in seconds
            float amplitude;
            float frequency;  // for shimmer harmonics
            int harmonics;
        };

        std::vector<Reflection> reflections = {
                {0.0f, 1.0f, 0.0f, 0},        // Direct signal
                {0.023f, 0.6f, 440.0f, 3},    // Early reflection with shimmer
                {0.047f, 0.4f, 659.25f, 2},   // E5 harmonic
                {0.089f, 0.35f, 880.0f, 4},   // Octave up
                {0.134f, 0.25f, 523.25f, 2},  // C5
                {0.201f, 0.2f, 1760.0f, 2},   // Two octaves up
                {0.298f, 0.15f, 1318.5f, 3},  // High shimmer
                {0.445f, 0.12f, 2093.0f, 1},  // Very high sparkle
                {0.667f, 0.1f, 1567.98f, 2},  // G#6
                {1.0f, 0.08f, 2637.0f, 1},    // Ultra high sparkle
        };

        // Generate each reflection
        for (const auto& refl : reflections) {
            int delaySamples = (int)(refl.delay * sampleRate);
            if (delaySamples >= totalSamples) continue;

            int remainingSamples = totalSamples - delaySamples;
            int envLength = std::min(remainingSamples, (int)(0.5f * sampleRate)); // 0.5s envelope

            for (int i = 0; i < envLength; i++) {
                int sample = delaySamples + i;

                float env = envelope(i, envLength / 20, envLength / 3, envLength);
                float signal = 0.0f;

                if (refl.frequency > 0.0f) {
                    // Shimmer component
                    signal = harmonicTone(i, refl.frequency, refl.harmonics);
                } else {
                    // Direct impulse
                    signal = (i == 0) ? 1.0f : 0.0f;
                }

                // Add some gentle modulation for movement
                float modulation = 1.0f + 0.1f * sin(2.0f * PI_P * 0.3f * i / sampleRate);

                ir[sample] += refl.amplitude * env * signal * modulation;
            }
        }

        // Add diffuse tail with golden ratio delays for natural sound
        float goldenRatio = 1.618034f;
        float currentDelay = 1.5f; // Start after main reflections

        std::uniform_real_distribution<float> ampDist(0.03f, 0.08f);
        std::uniform_real_distribution<float> freqDist(800.0f, 3200.0f);

        for (int tap = 0; tap < 12; tap++) {
            int delaySamples = (int)(currentDelay * sampleRate);
            if (delaySamples >= totalSamples - 1000) break;

            float amplitude = ampDist(rng) * exp(-currentDelay * 0.8f); // Natural decay
            float shimmerFreq = freqDist(rng);

            int tailLength = std::min(2000, totalSamples - delaySamples);

            for (int i = 0; i < tailLength; i++) {
                int sample = delaySamples + i;
                float env = exp(-i * 3.0f / tailLength); // Exponential decay
                float shimmer = 0.15f * sin(2.0f * PI_P * shimmerFreq * i / sampleRate);

                ir[sample] += amplitude * env * shimmer;
            }

            currentDelay *= goldenRatio * 0.618f; // Create natural spacing
        }

        // Final normalization
        float maxVal = 0.0f;
        for (float sample : ir) {
            maxVal = std::max(maxVal, std::abs(sample));
        }

        if (maxVal > 0.0f) {
            float normalizer = 0.8f / maxVal;
            for (float& sample : ir) {
                sample *= normalizer;
            }
        }

        return ir;
    }

    // Save as WAV file for testing
    void saveWAV(const std::vector<float>& ir, const std::string& filename) {
        // Simple 32-bit float WAV writer (basic implementation)
        // In practice, you'd use a proper audio library like libsndfile
        // This is just for demonstration
        std::cout << "Generated shimmer IR with " << ir.size() << " samples\n";
        std::cout << "Length: " << (float)ir.size() / sampleRate << " seconds\n";
    }
};

// Simple allpass filter for diffusion
class AllpassFilter {
private:
    std::vector<float> buffer;
    int writeIndex = 0;
    float feedback;

public:
    AllpassFilter(int delaySamples, float fb) : buffer(delaySamples, 0.0f), feedback(fb) {}

    float process(float input) {
        float delayed = buffer[writeIndex];
        float output = -input * feedback + delayed;
        buffer[writeIndex] = input + delayed * feedback;
        writeIndex = (writeIndex + 1) % buffer.size();
        return output;
    }
};

template<typename T>
class BeautifulIRGenerator2 {
    using ValueType = typename T::value_type;

private:

    void normalizeIR(T& ir) {
        ValueType max_val = 0.0;
        for (ValueType sample : ir) {
            max_val = std::max(max_val, std::abs(sample));
        }

        if (max_val > 0.0f) {
            ValueType scale = 0.8f / max_val; // Leave some headroom
            for (ValueType& sample : ir) {
                sample *= scale;
            }
        }
    }
    std::mt19937 rng;
    std::uniform_real_distribution<ValueType> uniform;
    std::normal_distribution<ValueType> gaussian;

    static constexpr ValueType TAU = 2.0f * PI_P;
    ValueType sr_{};

    // Function pointer type for IR generation methods
    using IRFunction = std::function<void(T&, int, ValueType)>;

    // Array of IR generation functions
    std::vector<IRFunction> irFunctions;
    std::vector<std::string> functionNames;

public:
    BeautifulIRGenerator2(ValueType sr) : sr_(sr), rng(std::random_device{}()), uniform(0.0f, 1.0f), gaussian(0.0f, 1.0f) {
        // Initialize the function array
        initializeFunctionArray();
    }
    // Call function by index
    void generateIR(int index, T& ir, int length, ValueType decay = 1.0) {
        if (index >= 0 && index < irFunctions.size()) {
            irFunctions[index](ir, length, decay);
        } else {
        }
    }

    // Get function name by index
    std::string getFunctionName(int index) const {
        if (index >= 0 && index < functionNames.size()) {
            return functionNames[index];
        }
        return "Invalid Index";
    }

    // Get total number of available functions
    int getNumFunctions() const {
        return static_cast<int>(irFunctions.size());
    }

    // Print all available functions
    void printAvailableFunctions() const {
        std::cout << "Available IR Generation Functions:\n";
        for (int i = 0; i < functionNames.size(); ++i) {
            std::cout << i << ": " << functionNames[i] << "\n";
        }
    }

private:
    void initializeFunctionArray() {
        // Add all IR generation functions to the array
        irFunctions = {
                [this](T& ir, int length, ValueType dec) { crystallineCathedral(ir, length); },
                [this](T& ir, int length, ValueType dec) { goldenRatioHall(ir, length); },
                [this](T& ir, int length, ValueType dec) { lorenzChaosReverb(ir, length); },
                [this](T& ir, int length, ValueType dec) { mandelbrotReverb(ir, length); },
                [this](T& ir, int length, ValueType dec) { fibonacciSpiral(ir, length); },
                [this](T& ir, int length, ValueType dec) { harmonicCathedral(ir, length); },
                [this](T& ir, int length, ValueType dec) { quantumResonator(ir, length); },
                [this](T& ir, int length, ValueType dec) { schroederAllpass(ir, length); },
                [this](T& ir, int length, ValueType dec) { plateReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { springReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { roomSimulation(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { concertHall(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { convexHullReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { granularCloud(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { waveguideNetwork(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { hadamardReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { perlinNoiseReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { cellularAutomataReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { duffingOscillator(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { henonMap(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { tentMap(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { spectralShaping(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { shepardToneReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { moirePatternReverb(ir, length, dec); },
                [this](T& ir, int length, ValueType dec) { brownianMotion(ir, length, dec); }
        };

        functionNames = {
                "Crystalline Cathedral",
                "Golden Ratio Hall",
                "Lorenz Chaos Reverb",
                "Mandelbrot Reverb",
                "Fibonacci Spiral",
                "Harmonic Cathedral",
                "Quantum Resonator",
                "Schroeder Allpass",
                "Plate Reverb",
                "Spring Reverb",
                "Room Simulation",
                "Concert Hall",
                "Convex Hull Reverb",
                "Granular Cloud",
                "Waveguide Network",
                "Hadamard Reverb",
                "Perlin Noise Reverb",
                "Cellular Automata Reverb",
                "Duffing Oscillator",
                "Henon Map",
                "Tent Map",
                "Spectral Shaping",
                "Shepard Tone Reverb",
                "Moire Pattern Reverb",
                "Brownian Motion"
        };
    }

    // Helper function to compute decay factor
    ValueType computeDecayFactor(ValueType dec, int length) {
        return std::pow(0.001, dec / length);
    }
    template<typename Container>
    void shapeIR( Container& buffer, int n, ValueType attack, ValueType decay, ValueType midpoint) {
        // Clamp parameters to reasonable ranges
        ValueType attack_ = std::max(0.001f, std::min(10.0f, attack));
        ValueType decay_ = std::max(0.001f, std::min(10.0f, decay));
        ValueType midpoint_ = std::max(0.01f, std::min(0.99, midpoint));

        int midpoint_sample = static_cast<int>(midpoint * n);

        for (int i = 0; i < n; i++) {
            ValueType envelope;
            ValueType t = static_cast<ValueType>(i) / n;

            if (i <= midpoint_sample) {
                // Attack phase: 0 to midpoint
                ValueType attack_t = static_cast<ValueType>(i) / midpoint_sample;
                envelope = std::pow(attack_t, 1.0f / attack);
            } else {
                // Decay phase: midpoint to end
                ValueType decay_t = static_cast<ValueType>(i - midpoint_sample) / (n - midpoint_sample);
                envelope = std::pow(1.0f - decay_t, decay);
            }

            buffer[i] *= envelope;
        }
    }

    // Generate a smooth envelope with attack and decay
    float envelope(int sample, int attackSamples, int decaySamples, int totalSamples) {
        if (sample < attackSamples) {
            return (float)sample / attackSamples;
        }
        int decayStart = totalSamples - decaySamples;
        if (sample > decayStart) {
            return 1.0f - (float)(sample - decayStart) / decaySamples;
        }
        return 1.0f;
    }

// Create harmonic content for shimmer effect
    float harmonicTone(int sample, float frequency, int harmonics, float sampleRate) {
        float output = 0.0f;
        float time = (float)sample / sampleRate;

        for (int h = 1; h <= harmonics; h++) {
            float harmFreq = frequency * h;
            float amplitude = 1.0f / (h * h); // Gentle harmonic rolloff
            output += amplitude * sin(2.0f * PI_P * harmFreq * time);
        }
        return output * 0.3f; // Scale down
    }



    template<typename T2>
    void dendrite(T2& buffer, int n, ValueType) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        // Neural network topology
        struct Neuron {
            int position;
            ValueType strength;
            std::vector<int> connections;
        };

        std::vector<Neuron> neurons;
        static std::mt19937 rng(101112);
        std::uniform_real_distribution<ValueType> strength_dist(0.1f, 0.8f);
        std::uniform_int_distribution<int> connection_dist(2, 5);

        // Create neural network topology
        const int num_neurons = std::min(20, n / 10);
        for (int i = 0; i < num_neurons; i++) {
            Neuron neuron;
            neuron.position = (i + 1) * n / (num_neurons + 1);
            neuron.strength = strength_dist(rng);

            // Each neuron connects to 2-5 others
            int num_connections = connection_dist(rng);
            for (int j = 0; j < num_connections; j++) {
                int target_offset = 10 + j * 15;
                if (neuron.position + target_offset < n) {
                    neuron.connections.push_back(neuron.position + target_offset);
                }
            }

            neurons.push_back(neuron);
        }

        // Initial spike
        if (n > 0) buffer[0] = 1.0;

        // Propagate through neural network
        for (const auto& neuron : neurons) {
            if (neuron.position < n) {
                buffer[neuron.position] += neuron.strength * 0.6;

                // Synaptic connections
                for (int connection : neuron.connections) {
                    if (connection < n) {
                        ValueType synaptic_strength = neuron.strength * 0.3;
                        buffer[connection] += synaptic_strength;
                    }
                }
            }
        }

        // Neural decay
        for (int i = 0; i < n; i++) {
            ValueType neural_decay = std::exp(-2.5f * i / n);
            buffer[i] *= neural_decay;
        }


    // Apply comb filter reflections at golden ratio intervals
    const ValueType phi = 1.618033988749;
    int delay1 = static_cast<int>(n / (phi * 2.0f));
    int delay2 = static_cast<int>(n / (phi * 3.0f));

    for (int i = delay1; i < n; i++) {
        buffer[i] += buffer[i - delay1] * 0.5;
    }
    for (int i = delay2; i < n; i++) {
        buffer[i] += buffer[i - delay2] * 0.3;
    }
}
// Shimmer Cascade - Ethereal, pitch-shifted reflections
        template<typename Container>
        void shimmerCascade(Container& buffer, int n) {
            std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

            // Initial impulse
            if (n > 0) buffer[0] = 1.0;

            // Create pitch-shifted echoes using all-pass interpolation
            const int num_octaves = 4;
            for (int oct = 1; oct <= num_octaves; oct++) {
                ValueType pitch_shift = std::pow(2.0f, oct); // Octave up
                int base_delay = static_cast<int>(n * 0.1f * oct);

                for (int i = base_delay; i < n; i++) {
                    ValueType source_pos = (i - base_delay) / pitch_shift;
                    int source_idx = static_cast<int>(source_pos);

                    if (source_idx < i - base_delay && source_idx >= 0) {
                        ValueType frac = source_pos - source_idx;
                        ValueType interpolated = buffer[source_idx] * (1.0f - frac);
                        if (source_idx + 1 < i) {
                            interpolated += buffer[source_idx + 1] * frac;
                        }

                        ValueType amplitude = 0.3f / oct;
                        buffer[i] += interpolated * amplitude;
                    }
                }
            }

            // Add sparkle with prime delays
            int sparkle_delays[] = {7, 13, 19, 31, 43, 61, 79, 97};
            for (int delay : sparkle_delays) {
                if (delay >= n) break;
                ValueType sparkle = 0.15f * std::exp(-1.0f * delay / n);
                buffer[delay] += sparkle * std::sin(delay * 0.3f);
            }
        }

// Cathedral - Majestic, sacred geometry inspired reverb
        template<typename Container>
        void cathedral(Container& buffer, int n) {
            std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

            // Sacred geometry ratios
            const ValueType phi = 1.618033988749; // Golden ratio
            const ValueType sqrt2 = 1.414213562373;
            const ValueType sqrt3 = 1.732050807569;

            // Initial blessed impulse
            if (n > 0) buffer[0] = 1.0;

            // Gothic arch reflections (pointed arch ratio ~2.6:1)
            int arch_delays[] = {
                    static_cast<int>(n * 0.08f),  // Nave
                    static_cast<int>(n * 0.13f),  // Transept
                    static_cast<int>(n * 0.21f),  // Chancel
                    static_cast<int>(n * 0.34f)   // Apse
            };

            ValueType arch_gains[] = {0.7f, 0.5f, 0.4f, 0.3f};

            for (int i = 0; i < 4; i++) {
                if (arch_delays[i] >= n) continue;

                // Primary reflection
                buffer[arch_delays[i]] += arch_gains[i];

                // Harmonic resonances at sacred ratios
                int harmonic1 = arch_delays[i] + static_cast<int>(arch_delays[i] / phi);
                int harmonic2 = arch_delays[i] + static_cast<int>(arch_delays[i] / sqrt2);

                if (harmonic1 < n) buffer[harmonic1] += arch_gains[i] * 0.3;
                if (harmonic2 < n) buffer[harmonic2] += arch_gains[i] * 0.2;
            }

            // Stone resonance with exponential decay
            for (int i = 1; i < n; i++) {
                ValueType stone_resonance = std::exp(-1.8f * i / n) *
                                        (1.0f + 0.05f * std::sin(i * 0.01f));
                buffer[i] *= stone_resonance;
            }
        }

// Quantum Foam - Probabilistic micro-reflections
        template<typename Container>
        void quantumFoam(Container& buffer, int n) {
            std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

            static std::mt19937 rng(789);
            std::uniform_real_distribution<ValueType> prob_dist(0.0f, 1.0f);
            std::normal_distribution<ValueType> amplitude_dist(0.0f, 0.1f);

            // Heisenberg uncertainty principle inspired
            for (int i = 0; i < n; i++) {
                ValueType t = static_cast<ValueType>(i) / n;

                // Probability wave function
                ValueType wave_function = std::exp(-5.0f * t * t) * std::cos(15.0f * t);
                ValueType probability = wave_function * wave_function;

                // Quantum tunneling events
                if (prob_dist(rng) < probability * 0.3f) {
                    ValueType amplitude = amplitude_dist(rng);
                    buffer[i] += amplitude * 2.0;
                }

                // Virtual particle pairs (short-lived reflections)
                if (i > 2 && i < n - 2 && prob_dist(rng) < 0.02f * probability) {
                    ValueType virtual_amplitude = amplitude_dist(rng) * 0.5;
                    buffer[i] += virtual_amplitude;
                    buffer[i + 1] -= virtual_amplitude; // Annihilation
                }
            }

            // Quantum decoherence envelope
            for (int i = 0; i < n; i++) {
                ValueType decoherence = std::exp(-3.0f * i / n);
                buffer[i] *= decoherence;
            }
        }

// Aurora - Undulating, spectral reverb
        template<typename Container>
        void aurora(Container& buffer, int n) {
            const int num_layers = 5;
            ValueType frequencies[] = {0.003f, 0.007f, 0.011f, 0.017f, 0.023f};
            ValueType phases[] = {0.0f, PI_P/3, 2*PI_P/3, PI_P, 4*PI_P/3};

            for (int i = 0; i < n; i++) {
                ValueType t = static_cast<ValueType>(i);
                buffer[i] = 0.0;

                // Multiple sine wave layers creating aurora-like movement
                for (int layer = 0; layer < num_layers; layer++) {
                    ValueType frequency = frequencies[layer];
                    ValueType phase = phases[layer];

                    // Modulated amplitude creates the undulating effect
                    ValueType mod_freq = frequency * 0.3;
                    ValueType amplitude_mod = 0.5f + 0.5f * std::sin(2*PI_P * mod_freq * t + phase);

                    // Base oscillation
                    ValueType oscillation = std::sin(2*PI_P * frequency * t + phase);

                    // Spectral coloring (different decay for each layer)
                    ValueType spectral_decay = std::exp(-0.8f * (layer + 1) * t / n);

                    buffer[i] += oscillation * amplitude_mod * spectral_decay * 0.3;
                }

                // Overall envelope
                ValueType master_envelope = std::exp(-1.2f * t / n);
                buffer[i] *= master_envelope;
            }
        }

// Möbius Strip - Non-linear, twisted time reflections
        template<typename Container>
        void moebiusStrip(Container& buffer, int n) {
            std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

            if (n > 0) buffer[0] = 1.0;

            // Non-linear time warping inspired by möbius topology
            for (int i = 1; i < n; i++) {
                ValueType t = static_cast<ValueType>(i) / n;

                // Möbius transformation: twist in time domain
                ValueType twisted_t = t + 0.1f * std::sin(4 * PI_P * t) * (1.0f - t);
                twisted_t = std::max(0.0f, std::min(1.0f, twisted_t));

                // Non-linear delay line
                int source_idx = static_cast<int>(twisted_t * (i - 1));
                if (source_idx >= 0 && source_idx < i) {
                    ValueType twist_amplitude = 0.4f * std::exp(-2.0f * t);
                    buffer[i] += buffer[source_idx] * twist_amplitude;
                }

                // Add topology-inspired resonances
                if (i % 7 == 0) { // Septuple symmetry
                    ValueType resonance = 0.2f * std::exp(-3.0f * t) * std::sin(t * 20.0f);
                    buffer[i] += resonance;
                }
            }
        }


// Velvet Noise - Sparse, natural-sounding reverb tail
    template<typename Container>
    void velvetNoise(Container& buffer, int n, ValueType) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        static std::mt19937 rng(123);
        std::uniform_real_distribution<ValueType> pos_dist(0.0f, 1.0f);
        std::uniform_int_distribution<int> sign_dist(0, 1);

        // Density decreases exponentially
        ValueType density = 0.8;

        for (int i = 0; i < n; i++) {
            ValueType t = static_cast<ValueType>(i) / n;
            ValueType current_density = density * std::exp(-2.0f * t);

            if (pos_dist(rng) < current_density / n) {
                ValueType amplitude = std::exp(-1.5f * t);
                buffer[i] = amplitude * (sign_dist(rng) ? 1.0f : -1.0f);
            }
        }
    }

// Stairway Reverb - Cascading geometric reflections
    template<typename Container>
    void stairwayReverb(Container& buffer, int n, ValueType) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        // Initial impulse
        if (n > 0) buffer[0] = 1.0;

        // Create geometric series of reflections
        const int num_stairs = 8;
        const ValueType ratio = 0.618; // Golden ratio conjugate

        for (int stair = 1; stair <= num_stairs && stair < n; stair++) {
            int delay = static_cast<int>(stair * stair * 1.5f);
            if (delay >= n) break;

            ValueType amplitude = std::pow(ratio, stair) * 0.8;
            buffer[delay] += amplitude;

            // Add slight modulation for each reflection
            for (int i = 1; i < 8 && delay + i < n; i++) {
                buffer[delay + i] += amplitude * 0.1f * std::sin(i * 0.5f);
            }
        }

        // Smooth exponential decay envelope
        for (int i = 1; i < n; i++) {
            ValueType envelope = std::exp(-2.0f * i / n);
            buffer[i] *= envelope;
        }
    }


// Crystalline - Sharp, bell-like resonances with harmonic series
    template<typename T2>
    void crystalline2(T& buffer, int n) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        // Fundamental frequency (as a fraction of sample rate)
        const ValueType fundamental = 0.01;

        // Create harmonic series with different decay rates
        for (int harmonic = 1; harmonic <= 12; harmonic++) {
            ValueType freq = fundamental * harmonic;
            ValueType amplitude = 1.0f / (harmonic * harmonic); // 1/n² falloff
            ValueType decay_rate = 0.5f + harmonic * 0.1;

            for (int i = 0; i < n; i++) {
                ValueType t = static_cast<ValueType>(i);
                ValueType envelope = std::exp(-decay_rate * t / n);
                ValueType oscillation = std::sin(2.0f * PI_P * freq * t);
                buffer[i] += amplitude * envelope * oscillation;
            }
        }

        // Add shimmer with prime number spacing
        int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23};
        for (int p : primes) {
            if (p >= n) break;
            ValueType shimmer = 0.1f * std::exp(-2.0f * p / n);
            buffer[p] += shimmer;
        }
    }

// Organic Bloom - Smooth, natural growth and decay
    template<typename T2>
    void organicBloom(T2& buffer, int n) {
        const ValueType peak_pos = 0.15; // Peak at 15% through the IR
        const ValueType growth_rate = 8.0;
        const ValueType decay_rate = 2.5;

        static std::mt19937 rng(456);
        std::normal_distribution<ValueType> noise_dist(0.0f, 0.05f);

        for (int i = 0; i < n; i++) {
            ValueType t = static_cast<ValueType>(i) / n;

            // Asymmetric envelope - quick attack, slow decay
            ValueType envelope;
            if (t < peak_pos) {
                envelope = std::pow(t / peak_pos, 1.0f / growth_rate);
            } else {
                envelope = std::exp(-decay_rate * (t - peak_pos) / (1.0f - peak_pos));
            }

            // Add organic variation
            ValueType variation = 1.0f + noise_dist(rng);
            ValueType flutter = 1.0f + 0.02f * std::sin(t * 50.0f) * envelope;

            buffer[i] = envelope * variation * flutter * 0.8;
        }
    }

    // Stairway Reverb - Cascading geometric reflections
    template<typename Container>
    void stairwayReverb(Container& buffer, int n) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        // Initial impulse
        if (n > 0) buffer[0] = 1.0;

        // Create geometric series of reflections
        const int num_stairs = 8;
        const ValueType ratio = 0.618; // Golden ratio conjugate

        for (int stair = 1; stair <= num_stairs && stair < n; stair++) {
            int delay = static_cast<int>(stair * stair * 1.5f);
            if (delay >= n) break;

            ValueType amplitude = std::pow(ratio, stair) * 0.8;
            buffer[delay] += amplitude;

            // Add slight modulation for each reflection
            for (int i = 1; i < 8 && delay + i < n; i++) {
                buffer[delay + i] += amplitude * 0.1f * std::sin(i * 0.5f);
            }
        }

        // Smooth exponential decay envelope
        for (int i = 1; i < n; i++) {
            ValueType envelope = std::exp(-2.0f * i / n);
            buffer[i] *= envelope;
        }
    }

    template<typename Container>
    void crystalline(Container& buffer, int n) {
        std::fill(buffer.begin(), buffer.begin() + n, 0.0f);

        // Fundamental frequency (as a fraction of sample rate)
        const ValueType fundamental = 0.01;

        // Create harmonic series with different decay rates
        for (int harmonic = 1; harmonic <= 12; harmonic++) {
            ValueType freq = fundamental * harmonic;
            ValueType amplitude = 1.0f / (harmonic * harmonic); // 1/n² falloff
            ValueType decay_rate = 0.5f + harmonic * 0.1;

            for (int i = 0; i < n; i++) {
                ValueType t = static_cast<ValueType>(i);
                ValueType envelope = std::exp(-decay_rate * t / n);
                ValueType oscillation = std::sin(2.0f * PI_P * freq * t);
                buffer[i] += amplitude * envelope * oscillation;
            }
        }

        // Add shimmer with prime number spacing
        int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23};
        for (int p : primes) {
            if (p >= n) break;
            ValueType shimmer = 0.1f * std::exp(-2.0 * p / n);
            buffer[p] += shimmer;
        }
    }

    template<typename Container>
    void organicBloom(Container& buffer, int n, ValueType) {
        const ValueType peak_pos = 0.15; // Peak at 15% through the IR
        const ValueType growth_rate = 8.0;
        const ValueType decay_rate = 2.5;

        static std::mt19937 rng(456);
        std::normal_distribution<ValueType> noise_dist(0.0, 0.05);

        for (int i = 0; i < n; i++) {
            ValueType t = static_cast<ValueType>(i) / n;

            // Asymmetric envelope - quick attack, slow decay
            ValueType envelope;
            if (t < peak_pos) {
                envelope = std::pow(t / peak_pos, 1.0f / growth_rate);
            } else {
                envelope = std::exp(-decay_rate * (t - peak_pos) / (1.0f - peak_pos));
            }

            // Add organic variation
            ValueType variation = 1.0f + noise_dist(rng);
            ValueType flutter = 1.0f + 0.02f * std::sin(t * 50.0f) * envelope;

            buffer[i] = envelope * variation * flutter * 0.8;
        }
    }
    static constexpr ValueType bellReversed(ValueType t) {return 1.0 - std::exp(-6.91f * (1.0f - t) * (1.0f - t));}
    static constexpr ValueType bellReversed2(ValueType t) {return 1.0 - std::exp(-5.5f * (1.0f - t));}

    template<typename Container>
    void goldenRatioHall(Container& ir, int length, ValueType dec = 1.0) {
        const ValueType phi = 1.618033988749; // Golden ratio
        auto tail_start = static_cast<int>(0);
        double tail_length = std::max(1, length -tail_start);
        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;

            // Golden ratio modulated reflections
            ValueType modulation = std::sin(TAU * t * phi) * std::exp(-t * phi * 0.5f);

            // Fibonacci sequence reflections
            int fib_idx = static_cast<int>(t * phi * 10) % 13;
            ValueType fibonacci_weight = fibonacci(fib_idx) / 233.0; // Normalize by F(13)

            ir[i] = modulation * fibonacci_weight;

            // Add sparse early reflections at golden ratio intervals
            if (i > 0 && (i % static_cast<int>(sr_ * 0.618f / 20)) == 0) {
                ir[i] += 0.3f * (uniform(rng) * 2.0f - 1.0f);
            }
            ir[i] = ir[i] * LOG2NORMAL(-24) * (i >= tail_start ? bellReversed(static_cast<ValueType>(i - tail_start) / (length - tail_start)) : 1.0);
        }

    }

    template<typename Container>
    void goldenRatioHall2(Container& ir, int length, ValueType dec = 1.0) {
        const ValueType phi = 1.618033988749; // Golden ratio
        auto tail_start = static_cast<int>(0);
        double tail_length = std::max(1, length - tail_start);

        for (int i = 0; i < length; ++i) {
            // Position within the impulse response (0.0 to 1.0)
            ValueType pos = static_cast<ValueType>(i) / static_cast<ValueType>(length - 1);

            // Time for decay calculations
            ValueType t = static_cast<ValueType>(i) / sr_;

            // Golden ratio modulated reflections - based on position, not time
            // This creates a few cycles across the entire IR length
            ValueType modulation = std::sin(TAU * pos * phi * 3.0) * std::exp(-pos * phi);

            // Alternative: For more controlled oscillations
            // ValueType modulation = std::sin(TAU * pos * phi * 2.0) * std::exp(-pos * 2.0);

            // Fibonacci sequence reflections
            int fib_idx = static_cast<int>(pos * phi * 10) % 13;
            ValueType fibonacci_weight = fibonacci(fib_idx) / 233.0; // Normalize by F(13)

            ir[i] = modulation * fibonacci_weight;

            // Add sparse early reflections at golden ratio intervals
            // This should also be based on position/length, not sample rate
            int golden_interval = static_cast<int>(length * 0.618 / 20);
            if (i > 0 && golden_interval > 0 && (i % golden_interval) == 0) {
                ir[i] += 0.3f * (uniform(rng) * 2.0f - 1.0f);
            }

            // Apply envelope and scaling
            ValueType envelope_pos = static_cast<ValueType>(i - tail_start) / (length - tail_start);
            ir[i] = ir[i]  *
                    (i >= tail_start ? bellReversed(envelope_pos) : 1.0);
        }
    }

// Alternative version with more control over modulation frequency
    template<typename Container>
    void goldenRatioHallControlled(Container& ir, int length, ValueType dec = 1.0,
                                   ValueType mod_cycles = 2.0) {
        const ValueType phi = 1.618033988749;
        auto tail_start = static_cast<int>(0);

        for (int i = 0; i < length; ++i) {
            ValueType pos = static_cast<ValueType>(i) / static_cast<ValueType>(length - 1);

            // Controlled modulation frequency
            ValueType modulation = std::sin(TAU * pos * phi * mod_cycles) *
                                   std::exp(-pos * phi * 0.5);

            int fib_idx = static_cast<int>(pos * phi * 10) % 13;
            ValueType fibonacci_weight = fibonacci(fib_idx) / 233.0;

            ir[i] = modulation * fibonacci_weight;

            // Golden ratio sparse reflections
            int golden_interval = std::max(1, static_cast<int>(length / (phi * 10)));
            if (i > 0 && (i % golden_interval) == 0) {
                ir[i] += 0.3f * (uniform(rng) * 2.0f - 1.0f);
            }

            ValueType envelope_pos = static_cast<ValueType>(i - tail_start) / (length - tail_start);
            ir[i] = ir[i] * LOG2NORMAL(-24) *
                    (i >= tail_start ? bellReversed(envelope_pos) : 1.0);
        }
    }
    // Lorenz Attractor Reverb - Chaotic but beautiful
    template<typename Container>
    void lorenzChaosReverb(Container& ir, int length, ValueType dec = 1.0, ValueType roomSize = 1.0f) {

        // Lorenz attractor parameters
        ValueType sigma = 10.0f, rho = 28.0f, beta = 8.0f / 3.0;
        ValueType x = 1.0f, y = 1.0f, z = 1.0;
        ValueType dt = 0.01f * roomSize;
        auto tail_start = static_cast<int>(0);
        double tail_length = std::max(1, length -tail_start);

        for (int i = 0; i < length; ++i) {
            // Evolve Lorenz system
            ValueType dx = sigma * (y - x) * dt;
            ValueType dy = (x * (rho - z) - y) * dt;
            ValueType dz = (x * y - beta * z) * dt;

            x += dx; y += dy; z += dz;

            // Use attractor coordinates to generate reverb
            ValueType chaos_amp = std::tanh(x * 0.1f) * std::sin(y * 0.1f) * std::cos(z * 0.1f);
            ir[i] = chaos_amp * LOG2NORMAL(-48) * (i >= tail_start ? bellReversed((i -tail_start) / tail_length) : 1.0);
            ;
        }

    }

    // Mandelbrot Set Reverb - Fractal beauty in audio
    template<typename Container>
    void mandelbrotReverb2(Container& ir, int length, ValueType complexity = 2.0f) {

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / length;

            // Map time to complex plane
            std::complex<ValueType> c(-0.8f + t * 1.6f, -0.4f + (i % 100) * 0.008f);
            std::complex<ValueType> z(0.0f, 0.0f);

            int iterations = 0;
            int max_iter = 50;

            // Mandelbrot iteration
            while (std::abs(z) < 2.0f && iterations < max_iter) {
                z = z * z + c;
                ++iterations;
            }

            ValueType fractal_intensity = static_cast<ValueType>(iterations) / max_iter;

            // Create musical intervals from fractal
            ValueType harmonic = std::sin(TAU * t * complexity * fractal_intensity * 5.0f);

            ir[i] = harmonic * fractal_intensity * 0.4;
        }

    }

// Alternative: Julia Set Reverb (related fractal)
    template<typename Container>
    void mandelbrotReverb(Container& ir, int length, std::complex<ValueType> julia_c = {-0.4f, 0.6f}) {
        auto tail_start = static_cast<int>(0);
        ValueType tail_length = std::max(1, length -tail_start);

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / length;

            // Map to complex plane - this time z varies, c is constant
            std::complex<ValueType> z(-1.5f + t * 3.0f, -1.5f + (i % 200) * 0.015f);

            int iterations = 0;
            int max_iter = 50;

            // Julia set iteration: z = z² + constant
            while (std::abs(z) < 2.0f && iterations < max_iter) {
                z = z * z + julia_c;
                ++iterations;
            }

            ValueType fractal_intensity = static_cast<ValueType>(iterations) / max_iter;
            ValueType harmonic = std::sin(TAU * t * fractal_intensity * 8.0f);

            ir[i] = harmonic * fractal_intensity * std::exp(-t * 2.0f) * LOG2NORMAL(-30) * (i >= tail_start ? bellReversed((i -tail_start) / tail_length) : 1.0);
        }
    }



    void crystallineCathedral(T& ir, int length, int harmonics = 8) {
        // Much slower overall decay - should reach -60dB at around 80% of length
        ValueType overall_decay_rate = 3.0f / length;
        auto tail_start = static_cast<int>(0);
        ValueType tail_length = std::max(1, length -tail_start);

        // Crystal lattice frequencies (based on quartz structure)
        std::vector<ValueType> crystal_freqs = {1.0f, 1.414f, 1.732f, 2.0f, 2.236f, 2.449f, 2.646f, 2.828f};

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Overall exponential decay envelope
            ValueType envelope = std::exp(-t * overall_decay_rate);

            // Generate crystalline harmonics
            for (int h = 0; h < harmonics && h < crystal_freqs.size(); ++h) {
                ValueType freq = crystal_freqs[h] * 80.0; // Slightly lower base frequency

                // Much gentler per-harmonic decay - higher frequencies decay slightly faster
                ValueType harmonic_decay = std::exp(-t * (0.5f + h * 0.1f));

                // Reduce phase modulation intensity
                ValueType phase_mod = std::sin(TAU * t * freq * 0.05f) * 0.1;

                sample += std::sin(TAU * t * freq + phase_mod) * harmonic_decay / std::sqrt(h + 1);
            }

            // Add crystalline reflections (make them more subtle and less regular)
            if (i % (2205 + (i/1000)) == 0) { // Slightly irregular timing
                sample += 0.1f * std::sin(TAU * t * 440.0f);
            }

            ir[i] = sample  * LOG2NORMAL(-38) * (i >= tail_start ? bellReversed((i -tail_start) / tail_length) : 1.0) * 0.2;
        }
    }
        void generateAscending(T& ir, int length,ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Linear frequency rise
                ValueType freq_mult = 1.0 + (t * speed * intensity * 2.0);
                ValueType base_freq = 200.0;

                ValueType sample = 0.0;

                // Multiple frequency bands ascending at different rates
                for (int band = 1; band <= 5; ++band) {
                    ValueType band_freq = base_freq * band * freq_mult;
                    ValueType band_decay = std::exp(-t * length/3.0 * band * 0.3);
                    ValueType phase_mod = std::sin(TAU * t * band_freq * 0.1) * 0.2;

                    sample += std::sin(TAU * t * band_freq + phase_mod) *
                              band_decay / (band * 1.5);
                }

                // Add some sparkle with high frequencies appearing later
                if (t > 0.1) {
                    ValueType sparkle_freq = 1000.0 * (1.0 + t * speed * intensity);
                    ValueType sparkle_env = envelope * std::sin(TAU * t * 3.0) * 0.3;
                    sample += std::sin(TAU * t * sparkle_freq) * sparkle_env * 0.2;
                }

                ir[i] = sample * envelope * 0.3;
            }
        }

        void generateDescending(T& ir, int length,ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Exponential frequency fall
                ValueType freq_mult = std::exp(-t * speed * intensity * 2.0);
                ValueType base_freq = 800.0;

                ValueType sample = 0.0;

                // Multiple frequency bands descending
                for (int band = 1; band <= 4; ++band) {
                    ValueType band_freq = base_freq * band * freq_mult;
                    ValueType band_decay = std::exp(-t * length/3.0 * std::sqrt(band));

                    sample += std::sin(TAU * t * band_freq) * band_decay / band;
                }

                // Add low-frequency bloom as high frequencies fade
                ValueType bloom_freq = 100.0 * freq_mult;
                ValueType bloom_env = envelope * (1.0 - std::exp(-t * 5.0));
                sample += std::sin(TAU * t * bloom_freq) * bloom_env * 0.4;

                ir[i] = sample * envelope * 0.3;
            }
        }

        void generateWobble(T& ir, int length,ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Oscillating frequency modulation
                ValueType wobble = std::sin(TAU * t * speed * 2.0) * intensity;
                ValueType freq_mult = 1.0 + wobble * 0.5;

                ValueType sample = 0.0;
                ValueType base_freq = 300.0;

                for (int band = 1; band <= 6; ++band) {
                    ValueType band_freq = base_freq * band * freq_mult;
                    ValueType band_phase = TAU * t * band_freq;

                    // Each band wobbles slightly out of phase
                    ValueType phase_offset = band * 0.3;
                    ValueType band_wobble = std::sin(TAU * t * speed + phase_offset) * 0.1;

                    sample += std::sin(band_phase + band_wobble) *
                              envelope / (band * 1.2);
                }

                ir[i] = sample * 0.25;
            }
        }

        void generateSpiralUp(T& ir, int length, ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Exponential spiral frequency rise
                ValueType spiral_factor = std::pow(2.0, t * speed * intensity * 3.0);
                ValueType base_freq = 150.0;

                ValueType sample = 0.0;

                // Spiral harmonics
                for (int h = 1; h <= 8; ++h) {
                    ValueType harm_freq = base_freq * h * spiral_factor;

                    // Only play harmonics that haven't gone too high
                    if (harm_freq < sr_ * 0.4) {
                        ValueType harm_decay = std::exp(-t * length/3.0 * std::sqrt(h));
                        ValueType spiral_phase = TAU * t * harm_freq;

                        // Add spiral modulation
                        ValueType spiral_mod = std::sin(spiral_phase * 0.1) * 0.3;

                        sample += std::sin(spiral_phase + spiral_mod) *
                                  harm_decay / h;
                    }
                }

                ir[i] = sample * envelope * 0.2;
            }
        }

        void generateSpiralDown(T& ir, int length,ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Exponential spiral frequency fall
                ValueType spiral_factor = std::pow(0.5, t * speed * intensity * 2.0);
                ValueType base_freq = 1000.0;

                ValueType sample = 0.0;

                for (int h = 1; h <= 6; ++h) {
                    ValueType harm_freq = base_freq * h * spiral_factor;

                    if (harm_freq > 50.0) { // Don't go too low
                        ValueType harm_decay = std::exp(-t * length/3.0 * 0.5);
                        sample += std::sin(TAU * t * harm_freq) * harm_decay / h;
                    }
                }

                ir[i] = sample * envelope * 0.3;
            }
        }

        void generateHarmonicBloom(T& ir, int length, ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                ValueType sample = 0.0;
                ValueType base_freq = 220.0; // A3

                // Harmonics appear progressively over time
                for (int h = 1; h <= 16; ++h) {
                    ValueType harm_freq = base_freq * h;

                    // Each harmonic has a delayed onset
                    ValueType onset_time = (h - 1) * speed * 0.1;
                    ValueType harm_env = (t > onset_time) ?
                                         std::exp(-(t - onset_time) * length/3.0 * h * 0.2) : 0.0;

                    // Harmonic strength follows overtone series
                    ValueType harm_strength = 1.0 / (h * (1.0 + intensity));

                    sample += std::sin(TAU * t * harm_freq) * harm_env * harm_strength;
                }

                ir[i] = sample * envelope * 0.2;
            }
        }

        void generateSpectralShift(T& ir, int length, ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                ValueType sample = 0.0;

                // Multiple frequency bands shifting independently
                ValueType bands[] = {200.0, 400.0, 800.0, 1600.0, 3200.0};
                ValueType shifts[] = {1.2, 0.8, 1.5, 0.6, 1.8}; // Different shift rates

                for (int b = 0; b < 5; ++b) {
                    ValueType shift_amount = std::sin(TAU * t * speed * shifts[b]) * intensity;
                    ValueType shifted_freq = bands[b] * (1.0 + shift_amount * 0.3);

                    ValueType band_decay = std::exp(-t * length/3.0 * (b + 1) * 0.3);
                    sample += std::sin(TAU * t * shifted_freq) * band_decay / (b + 2);
                }

                ir[i] = sample * envelope * 0.25;
            }
        }

        void generateDoppler(T& ir, int length,ValueType intensity = 1.0, ValueType speed=1.0) {
            for (int i = 0; i < length; ++i) {
                ValueType t = static_cast<ValueType>(i) / sr_;
                ValueType envelope = std::exp(-t * length/3.0);

                // Simulate Doppler effect with changing frequency
                ValueType velocity = std::sin(TAU * t * speed * 0.5) * intensity;
                ValueType doppler_factor = 1.0 + velocity * 0.1; // Simplified Doppler

                ValueType sample = 0.0;
                ValueType base_freq = 440.0;

                for (int h = 1; h <= 5; ++h) {
                    ValueType harm_freq = base_freq * h * doppler_factor;
                    ValueType harm_decay = std::exp(-t * length/3.0 * h * 0.4);

                    // Add some spatial modulation
                    ValueType spatial_mod = std::cos(TAU * t * harm_freq * 0.01) * 0.2;

                    sample += std::sin(TAU * t * harm_freq + spatial_mod) *
                              harm_decay / h;
                }

                ir[i] = sample * envelope * 0.3;
            }
        }
// Non-repeating Fibonacci Spiral Reverb
    void fibonacciSpiral(T& ir, int length, ValueType spiral_speed = 1.0f) {
        ValueType phi = 1.618033988749895;

        // Overall decay envelope
        ValueType decay_rate = 3.0 / length;

        // Seed for chaotic variations
        ValueType chaos_seed = 0.31415926; // Irrational number

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Overall exponential decay
            ValueType envelope = std::exp(-t * decay_rate);

            // Chaotic spiral coordinates (breaks perfect repetition)
            ValueType chaos = std::sin(t * chaos_seed * 17.0) * 0.1; // Small chaotic variation
            ValueType theta = t * spiral_speed * TAU * (phi + chaos);
            ValueType radius = std::pow(t, 0.6) * phi; // Non-linear radius growth

            ValueType x = radius * std::cos(theta);
            ValueType y = radius * std::sin(theta);

            // Aperiodic modulation using irrational numbers
            ValueType mod1 = std::sin(x * 0.382) * std::cos(y * 0.618); // Golden ratio derivatives
            ValueType mod2 = std::sin(x * 0.271) * std::cos(y * 0.414); // Different irrationals
            ValueType mod3 = std::sin(x * 0.577) * std::cos(y * 0.866); // More irrationals

            // Combine modulations with time-varying weights
            ValueType w1 = 0.5 + 0.3 * std::sin(t * 0.23);
            ValueType w2 = 0.3 + 0.2 * std::sin(t * 0.37);
            ValueType w3 = 0.2 + 0.1 * std::sin(t * 0.41);

            sample = (mod1 * w1 + mod2 * w2 + mod3 * w3) / 3.0;

            // Fibonacci-inspired amplitude modulation (but aperiodic)
            ValueType fib_phase = t * 2.718281828; // Use 'e' instead of integer for aperiodicity
            ValueType fib_mod = std::sin(fib_phase) * std::cos(fib_phase * phi);

            sample *= (0.7 + 0.3 * fib_mod);

            // Sparse, irregular reflections
            ValueType reflection_prob = std::sin(t * 13.0) * std::sin(t * 17.0);
            if (reflection_prob > 0.95 && i > sr_ * 0.01) { // Very sparse, after 10ms
                ValueType refl_strength = 0.1 * envelope * (uniform(rng) * 0.5 + 0.5);
                sample += refl_strength * (uniform(rng) * 2.0 - 1.0);
            }

            // Apply envelope
            ir[i] = sample * envelope * 0.25;
        }

        // Add diffusion to break any remaining patterns
        if (length > 100) {
            std::vector<ValueType> temp(length);
            for (int j = 0; j < length; ++j) {
                temp[j] = ir[j];
            }

            // Simple all-pass diffusion
            ValueType delay_samples[] = {23, 47, 83, 127}; // Prime delays
            ValueType feedback = 0.3;

            for (int d = 0; d < 4; ++d) {
                int delay = static_cast<int>(delay_samples[d]);
                for (int i = delay; i < length; ++i) {
                    ValueType delayed = temp[i - delay];
                    ValueType input = ir[i];
                    ir[i] = delayed + feedback * input;
                    temp[i] = input + feedback * delayed;
                }
            }
        }
    }
    // Fibonacci Spiral Reverb - Natural spiral patterns
    void fibonacciSpiral2(T& ir, int length, ValueType spiral_speed = 1.0f) {

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;

            // Fibonacci spiral coordinates
            ValueType phi = 1.618033988749;
            ValueType theta = t * spiral_speed * TAU * phi;
            ValueType radius = std::sqrt(t) * phi;

            ValueType x = radius * std::cos(theta);
            ValueType y = radius * std::sin(theta);

            // Generate reflections along spiral
            ValueType spiral_mod = std::sin(x * 0.5f) * std::cos(y * 0.3f);

            // Fibonacci number modulation
            int fib_n = static_cast<int>(t * 10) % 21;
            ValueType fib_weight = fibonacci(fib_n) / 10946.0; // Normalize

            ir[i] = spiral_mod * fib_weight * 0.6;

            // Add golden angle reflections
            if (i > 0 && (i % static_cast<int>(sr_ * 2.399963f / 100)) == 0) {
                ir[i] += 0.2f * (uniform(rng) * 2.0f - 1.0f);
            }
        }
    }

    // Harmonic Series Cathedral - Pure mathematical harmony
    void harmonicCathedral(T& ir, int length, ValueType fundamental = 55.0f) {
        // Just intonation ratios for pure harmony
        std::vector<ValueType> ratios = {
                1.0f,           // Unison
                9.0f/8.0f,      // Major second
                5.0f/4.0f,      // Major third
                4.0f/3.0f,      // Perfect fourth
                3.0f/2.0f,      // Perfect fifth
                5.0f/3.0f,      // Major sixth
                15.0f/8.0f,     // Major seventh
                2.0f            // Octave
        };

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Generate harmonic series with natural decay
            for (int h = 0; h < ratios.size(); ++h) {
                ValueType freq = fundamental * ratios[h];
                ValueType harmonic_envelope = std::exp(-t * freq * 0.002f);
                ValueType phase_drift = uniform(rng) * 0.01f * t; // Slight natural drift

                sample += std::sin(TAU * t * freq + phase_drift) *
                          harmonic_envelope / std::sqrt(ratios[h]);
            }

            // Add cathedral-like early reflections
            ValueType reflection_time = 0.05f + 0.15f * uniform(rng);
            int reflection_idx = static_cast<int>(reflection_time * sr_);
            if (i == reflection_idx) {
                sample += 0.3f * std::sin(TAU * t * fundamental * 2.0f);
            }

            ir[i] = sample * 0.25;
        }

    }

    // Quantum Resonator - Based on quantum harmonic oscillator
    void quantumResonator(T& ir, int length, int quantum_levels = 16) {
        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Quantum harmonic oscillator energy levels
            for (int n = 0; n < quantum_levels; ++n) {
                ValueType energy = (n + 0.5f) * 100.0; // Scaled for audio
                ValueType probability = std::exp(-energy * t * 0.01f);

                // Hermite polynomial approximation for quantum states
                ValueType hermite = hermitePolynomial(n, t * 10.0f - 5.0f);
                ValueType gaussian = std::exp(-(t * 10.0f - 5.0f) * (t * 10.0f - 5.0f) * 0.5f);

                sample += hermite * gaussian * probability / std::sqrt(factorial(n));
            }

            ir[i] = sample * 0.1;
        }

    }

    template<typename Container>
    void schroederAllpass(Container& ir, int length) {

        // Schroeder allpass delay times (in samples at 44.1kHz)
        std::vector<int> delays = {556, 441, 341, 225};
        std::vector<ValueType> gains = {0.7f, 0.7f, 0.7f, 0.7f};
        std::vector<std::vector<ValueType>> delay_lines(delays.size());

        for (size_t d = 0; d < delays.size(); ++d) {
            delay_lines[d].resize(delays[d], 0.0f);
        }

        for (int i = 0; i < length; ++i) {
            ValueType input = (i == 0) ? 1.0f : 0.0; // Impulse
            ValueType output = input;

            // Chain of allpass filters
            for (size_t d = 0; d < delays.size(); ++d) {
                int delay_idx = i % delays[d];
                ValueType delayed = delay_lines[d][delay_idx];
                ValueType new_output = -gains[d] * output + delayed;
                delay_lines[d][delay_idx] = output + gains[d] * new_output;
                output = new_output;
            }

            ir[i] = output * 0.3;
        }

    }

    // Plate Reverb Simulation - Based on 2D wave equation
    template<typename Container>
    void plateReverb(Container& ir, int length, ValueType damping = 0.998f) {
        const int plate_width = 100;
        const int plate_height = 80;

        std::vector<std::vector<ValueType>> plate_current(plate_height, std::vector<ValueType>(plate_width, 0.0f));
        std::vector<std::vector<ValueType>> plate_prev(plate_height, std::vector<ValueType>(plate_width, 0.0f));

        // Initial impulse at center
        plate_prev[plate_height/2][plate_width/2] = 1.0;

        for (int i = 0; i < length; ++i) {
            // 2D wave equation simulation
            for (int y = 1; y < plate_height - 1; ++y) {
                for (int x = 1; x < plate_width - 1; ++x) {
                    ValueType laplacian = plate_prev[y-1][x] + plate_prev[y+1][x] +
                                       plate_prev[y][x-1] + plate_prev[y][x+1] -
                                       4.0f * plate_prev[y][x];

                    plate_current[y][x] = damping * (2.0f * plate_prev[y][x] -
                                                     plate_current[y][x] +
                                                     0.3f * laplacian);
                }
            }

            // Sample output from multiple pickup points
            ValueType sample = 0.0;
            sample += plate_current[20][30] * 0.3;
            sample += plate_current[60][70] * 0.25;
            sample += plate_current[40][80] * 0.2;
            sample += plate_current[70][20] * 0.25;

            ir[i] = sample;

            // Swap buffers
            std::swap(plate_current, plate_prev);
        }

    }

    // Spring Reverb - Physical spring model
    template<typename Container>
    void springReverb(Container& ir, int length, ValueType tension = 0.9f) {
        const int spring_segments = 200;
        std::vector<ValueType> spring_current(spring_segments, 0.0f);
        std::vector<ValueType> spring_prev(spring_segments, 0.0f);
        std::vector<ValueType> spring_vel(spring_segments, 0.0f);

        // Initial impulse
        spring_prev[10] = 1.0;

        ValueType damping = 0.9995;
        ValueType coupling = tension * 0.5;

        for (int i = 0; i < length; ++i) {
            // Spring physics simulation
            for (int s = 1; s < spring_segments - 1; ++s) {
                ValueType force = coupling * (spring_prev[s-1] + spring_prev[s+1] - 2.0f * spring_prev[s]);
                spring_vel[s] = spring_vel[s] * damping + force;
                spring_current[s] = spring_prev[s] + spring_vel[s];
            }

            // Output from spring end with reflections
            ValueType sample = spring_current[spring_segments - 5] * 0.5;
            sample += spring_current[5] * 0.3; // Reflection from input end

            // Add some nonlinearity for realism
            sample = std::tanh(sample * 2.0f) * 0.5;

            ir[i] = sample;

            // Swap buffers
            std::swap(spring_current, spring_prev);
        }
    }

    // Room Simulation - Ray tracing based
    template<typename Container>
    void roomSimulation(Container& ir, int length, ValueType room_size = 10.0f, ValueType absorption = 0.1f) {
        std::vector<ValueType> reflection_times;
        std::vector<ValueType> reflection_gains;

        // Generate reflections based on room geometry
        ValueType sound_speed = 343.0; // m/s
        ValueType sample_rate = sr_;

        // Primary reflections (walls, floor, ceiling)
        std::vector<ValueType> wall_distances = {room_size/2, room_size/2, room_size/3, room_size/3, room_size/4, room_size/4};

        for (int reflection_order = 1; reflection_order <= 8; ++reflection_order) {
            int reflections_this_order = std::pow(6, reflection_order);

            for (int r = 0; r < std::min(reflections_this_order, 50); ++r) {
                ValueType path_length = room_size * reflection_order * (0.8f + 0.4f * uniform(rng));
                ValueType arrival_time = path_length / sound_speed;
                ValueType gain = std::pow(1.0f - absorption, reflection_order) / std::sqrt(path_length);

                if (arrival_time * sample_rate < length) {
                    reflection_times.push_back(arrival_time);
                    reflection_gains.push_back(gain);
                }
            }
        }

        // Generate IR from reflections
        std::fill(ir.begin(), ir.begin() + length, 0.0f);
        ir[0] = 1.0; // Direct sound

        for (size_t r = 0; r < reflection_times.size(); ++r) {
            int sample_idx = static_cast<int>(reflection_times[r] * sample_rate);
            if (sample_idx < length) {
                ir[sample_idx] += reflection_gains[r] * (uniform(rng) * 2.0f - 1.0f);
            }
        }

        // Add diffuse tail
        ValueType tail_start = 0.05f * sample_rate; // 50ms

        for (int i = static_cast<int>(tail_start); i < length; ++i) {
            ir[i] += gaussian(rng) * 0.1;
        }
    }

    // Hall Reverb - Large space simulation
    template<typename Container>
    void concertHall(Container& ir, int length, ValueType hall_size = 50.0f) {
        std::fill(ir.begin(), ir.begin() + length, 0.0f);

        ValueType sample_rate = sr_;
        ValueType sound_speed = 343.0;

        // Early reflections pattern for concert hall
        std::vector<std::pair<ValueType, ValueType>> early_reflections = {
                {0.012, 0.8}, {0.025, 0.6}, {0.041, 0.5}, {0.058, 0.4},
                {0.078, 0.35}, {0.095, 0.3}, {0.112, 0.25}, {0.134, 0.2}
        };

        // Direct sound
        ir[0] = 1.0;

        // Early reflections
        for (const auto& reflection : early_reflections) {
            int idx = static_cast<int>(reflection.first * sample_rate);
            if (idx < length) {
                ir[idx] += reflection.second * (1.0f + 0.2f * uniform(rng));
            }
        }

        int tail_start = static_cast<int>(0.85 * length);
        int tail_length = std::max(length - tail_start, 1); // number of samples over which to decay

        constexpr ValueType TAU = static_cast<ValueType>(6.28318530717958647692);

// Modal frequencies (Hz-like)
        std::vector<ValueType> modal_freqs = {62.5, 125, 250, 500, 1000, 2000, 4000};

// Use higher decay rates for higher frequencies
        std::vector<ValueType> decay_rates;
        ValueType decay_base = static_cast<ValueType>(std::log(1000.0)) / tail_length; // base decay

        for (size_t i = 0; i < modal_freqs.size(); ++i) {
            ValueType freq = modal_freqs[i];
            ValueType normalized = freq / modal_freqs.back(); // ∈ [0, 1]
            ValueType decay = decay_base * (0.5 + normalized); // e.g. 0.5× for lows, 1.5× for highs
            decay_rates.push_back(decay);
        }

        for (int i = tail_start; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i - tail_start); // in samples
            ValueType modal_sum = 0.0;

            for (size_t j = 0; j < modal_freqs.size(); ++j) {
                ValueType freq = modal_freqs[j];
                ValueType decay = std::exp(-t * decay_rates[j]);
                ValueType phase = uniform(rng) * TAU;
                modal_sum += std::sin(TAU * freq * t + phase) * decay;
            }

            ir[i] += modal_sum * static_cast<ValueType>(0.15);
            ir[i] += gaussian(rng) * static_cast<ValueType>(0.05);
        }


    }

    // Convex Hull Reverb - Based on architectural geometry
    template<typename Container>
    void convexHullReverb(Container& ir, int length, int num_vertices = 12) {
        std::fill(ir.begin(), ir.begin() + length, 0.0f);

        // Generate random convex hull vertices (simplified 2D)
        std::vector<std::pair<ValueType, ValueType>> vertices;
        for (int i = 0; i < num_vertices; ++i) {
            ValueType angle = TAU * i / num_vertices;
            ValueType radius = 5.0f + 15.0f * uniform(rng);
            vertices.push_back({radius * std::cos(angle), radius * std::sin(angle)});
        }

        ValueType sample_rate = sr_;
        ValueType sound_speed = 343.0;

        ir[0] = 1.0; // Direct sound

        // Calculate reflections from hull surfaces
        for (int order = 1; order <= 6; ++order) {
            int num_rays = 20 / order; // Fewer rays for higher orders

            for (int ray = 0; ray < num_rays; ++ray) {
                ValueType path_length = 0.0;
                ValueType total_absorption = 1.0;

                // Trace ray through convex hull
                for (int bounce = 0; bounce < order; ++bounce) {
                    int vertex_idx = static_cast<int>(uniform(rng) * vertices.size());
                    ValueType segment_length = 3.0f + 10.0f * uniform(rng);
                    path_length += segment_length;
                    total_absorption *= 0.85; // Surface absorption
                }

                ValueType arrival_time = path_length / sound_speed;
                int sample_idx = static_cast<int>(arrival_time * sample_rate);

                if (sample_idx < length) {
                    ValueType gain = total_absorption / std::sqrt(path_length);
                    ir[sample_idx] += gain * (uniform(rng) * 2.0f - 1.0f) * 0.3;
                }
            }
        }

        int tail_start = static_cast<int>(0.92f * length);
        int tail_length = std::max(length - tail_start, 1); // ensure at least 1 sample
        float decay_db = 60.0f;
        float decay_rate = std::log(std::pow(10.0f, decay_db / 20.0f)); // ~6.908

            for (int i = tail_start; i < length; ++i) {
                float time_offset = static_cast<float>(i - tail_start) / tail_length; // 0 to 1
                float decay_factor = std::exp(-decay_rate * time_offset); // goes from 1 → 0.001
                ir[i] += decay_factor * gaussian(rng) * 0.08f;
            }
    }

    // Granular Cloud Reverb - Particle-based approach
    template<typename Container>
    void granularCloud(Container& ir, int length, int num_grains = 500) {
        std::fill(ir.begin(), ir.begin() + length, 0.0f);

        ValueType sample_rate = sr_;
        ir[0] = 1.0; // Direct sound

        // Generate grain cloud
        for (int grain = 0; grain < num_grains; ++grain) {
            // Random grain parameters
            ValueType delay_time = 0.01f + uniform(rng) * 3.0; // 10ms to 3s
            ValueType grain_dur = 0.005f + uniform(rng) * 0.1; // 5ms to 100ms
            ValueType gain = 0.1f + 0.4f * std::exp(-delay_time * 0.5f); // Distance attenuation
            ValueType pitch_shift = 0.5f + uniform(rng); // Pitch variation

            int start_sample = static_cast<int>(delay_time * sample_rate);
            int grain_length = static_cast<int>(grain_dur * sample_rate);

            for (int s = 0; s < grain_length && (start_sample + s) < length; ++s) {
                ValueType grain_env = std::sin(PI_P * s / grain_length); // Hanning window
                ValueType phase = TAU * s * pitch_shift * 440.0f / sample_rate;
                ValueType grain_sample = std::sin(phase) * grain_env * gain;

                ir[start_sample + s] += grain_sample * 0.02;
            }
        }
    }

    // Waveguide Network - Physical modeling approach
    template<typename Container>
    void waveguideNetwork(Container& ir, int length, int network_size = 8) {
        const int max_delay = 4410; // 100ms max delay

        // Create waveguide network
        std::vector<std::vector<ValueType>> delay_lines(network_size);
        std::vector<int> delays(network_size);
        std::vector<ValueType> gains(network_size);
        std::vector<int> write_pos(network_size, 0);

        for (int i = 0; i < network_size; ++i) {
            delays[i] = 200 + static_cast<int>(uniform(rng) * max_delay);
            delay_lines[i].resize(delays[i], 0.0f);
            gains[i] = 0.7f + 0.25f * uniform(rng);
        }

        for (int sample = 0; sample < length; ++sample) {
            ValueType input = (sample == 0) ? 1.0f : 0.0;
            ValueType output = 0.0;

            // Process each waveguide
            for (int w = 0; w < network_size; ++w) {
                // Read from delay line
                ValueType delayed = delay_lines[w][write_pos[w]];

                // Mix with input and feedback from other guides
                ValueType feedback = 0.0;
                for (int f = 0; f < network_size; ++f) {
                    if (f != w) {
                        int read_pos = (write_pos[f] + delays[f] - delays[f]/4) % delays[f];
                        feedback += delay_lines[f][read_pos] * 0.1;
                    }
                }

                // Write to delay line
                delay_lines[w][write_pos[w]] = input * 0.1f + delayed * gains[w] + feedback;

                // Advance write position
                write_pos[w] = (write_pos[w] + 1) % delays[w];

                output += delayed;
            }

            ir[sample] = output * 0.1;
        }
    }

    // Hadamard Transform Reverb - Using Walsh functions
    template<typename Container>
    void hadamardReverb(Container& ir, int length, int order = 8,ValueType xx = 1.0) {
        int matrix_size = 1 << order; // 2^order
        std::vector<std::vector<int>> hadamard_matrix(matrix_size, std::vector<int>(matrix_size));

        // Generate Hadamard matrix
        generateHadamardMatrix(hadamard_matrix, order);


        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Use Hadamard transform for orthogonal reflection patterns
            int row_idx = i % matrix_size;
            for (int col = 0; col < matrix_size; ++col) {
                ValueType walsh_value = hadamard_matrix[row_idx][col];
                ValueType freq = (col + 1) * 50.0; // Base frequency scaling
                ValueType phase = TAU * t * freq;

                sample += walsh_value * std::sin(phase) * std::exp(-t * freq * 0.01f);
            }

            ir[i] = sample * 0.03;
        }
}

    // Perlin Noise Reverb - Natural randomness
    template<typename Container>
    void perlinNoiseReverb(Container& ir, int length, ValueType noise_scale = 0.01f) {

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;

            // Multi-octave Perlin-like noise
            ValueType noise = 0.0;
            ValueType amplitude = 1.0;
            ValueType frequency = noise_scale;

            for (int octave = 0; octave < 6; ++octave) {
                noise += amplitude * perlinNoise(t * frequency) *
                         std::sin(TAU * t * (100.0f + octave * 50.0f));
                amplitude *= 0.5;
                frequency *= 2.0;
            }

            ir[i] = noise * 0.2;
        }
    }

    // Cellular Automata Reverb - Conway's Game of Life inspired
    template<typename Container>
    void cellularAutomataReverb(Container& ir, int length, int grid_size = 32) {
        std::vector<std::vector<bool>> grid(grid_size, std::vector<bool>(grid_size, false));
        std::vector<std::vector<bool>> next_grid(grid_size, std::vector<bool>(grid_size, false));

        // Initialize with random pattern
        for (int y = 0; y < grid_size; ++y) {
            for (int x = 0; x < grid_size; ++x) {
                grid[y][x] = uniform(rng) > 0.7;
            }
        }


        for (int i = 0; i < length; ++i) {

            // Evolve cellular automaton every 100 samples
            if (i % 100 == 0) {
                for (int y = 1; y < grid_size - 1; ++y) {
                    for (int x = 1; x < grid_size - 1; ++x) {
                        int neighbors = 0;
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0) continue;
                                if (grid[y + dy][x + dx]) neighbors++;
                            }
                        }

                        // Modified Conway rules for audio
                        next_grid[y][x] = (neighbors == 3) || (grid[y][x] && neighbors == 2);
                    }
                }
                std::swap(grid, next_grid);
            }

            // Convert grid state to audio
            ValueType sample = 0.0;
            int active_cells = 0;
            for (int y = 0; y < grid_size; ++y) {
                for (int x = 0; x < grid_size; ++x) {
                    if (grid[y][x]) {
                        ValueType freq = 100.0f + (x + y) * 20.0;
                        ValueType t = static_cast<ValueType>(i) / sr_;
                        sample += std::sin(TAU * t * freq);
                        active_cells++;
                    }
                }
            }

            if (active_cells > 0) {
                sample /= active_cells;
            }

            ir[i] = sample * 0.15;
        }
    }

    // Duffing Oscillator Reverb - Nonlinear dynamics
    template<typename Container>
    void duffingOscillator(Container& ir, int length, ValueType nonlinearity = 0.5f) {
        ValueType x = 0.1f, v = 0.0; // Position and velocity
        ValueType dt = 1.0f / sr_;
        // Duffing equation parameters
        ValueType alpha = -1.0;
        ValueType beta = nonlinearity;
        ValueType gamma = 0.3;
        ValueType omega = TAU * 220.0; // Drive frequency

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            // Duffing equation: x'' + delta*x' + alpha*x + beta*x^3 = gamma*cos(omega*t)
            ValueType drive = gamma * std::cos(omega * t);
            ValueType damping = 0.1f * v;
            ValueType restoring = alpha * x + beta * x * x * x;

            ValueType acceleration = drive - damping - restoring;
            v += acceleration * dt;
            x += v * dt;

            ir[i] = x * 0.3;
        }

        normalizeIR(ir, length);
    }

    // Henon Map Reverb - Chaotic attractor
    template<typename Container>
    void henonMap(Container& ir, int length, ValueType dec = 1.0, ValueType a = 1.4f, ValueType b = 0.3f) {
        ValueType x = 0.1f, y = 0.1;
        const ValueType decay = computeDecayFactor(dec, length);

        for (int i = 0; i < length; ++i) {
            ValueType envelope = std::pow(decay, i);

            // Henon map iteration
            ValueType x_new = 1.0f - a * x * x + y;
            ValueType y_new = b * x;

            x = x_new;
            y = y_new;

            // Convert to audio with multiple voices
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = std::tanh(x) * std::sin(TAU * t * 440.0f * (1.0f + y * 0.1f));

            ir[i] = envelope * sample * 0.4;
        }

        normalizeIR(ir, length);
    }

    // Tent Map Reverb - Simple chaos
    template<typename Container>
    void tentMap(Container& ir, int length, ValueType mu = 1.9f,ValueType xx = 1.0) {
        ValueType x = 0.5; // Initial condition

        for (int i = 0; i < length; ++i) {

            // Tent map iteration
            if (x < 0.5f) {
                x = mu * x;
            } else {
                x = mu * (1.0f - x);
            }

            // Convert to multiple harmonic voices
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            for (int h = 1; h <= 8; ++h) {
                ValueType freq = 110.0f * h * (1.0f + x * 0.2f);
                sample += std::sin(TAU * t * freq) / h;
            }

            ir[i] = sample * x * 0.2;
        }
}

    // Frequency Domain Convolution - Spectral shaping
    template<typename Container>
    void spectralShaping(Container& ir, int length, int num_bands = 16) {
        for (int i = 0; i < length; ++i) {
            ir[i] = 0.0;
        }
        // Create frequency bands with different characteristics
        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            for (int band = 0; band < num_bands; ++band) {
                ValueType center_freq = 55.0f * std::pow(2.0f, band / 3.0f); // Musical intervals
                ValueType bandwidth = center_freq * 0.1;

                // Band-specific modulation
                ValueType mod_rate = 0.5f + band * 0.2;
                ValueType mod_depth = std::exp(-t * mod_rate);

                // Multiple oscillators per band
                for (int osc = 0; osc < 3; ++osc) {
                    ValueType freq = center_freq + (osc - 1) * bandwidth;
                    ValueType phase_mod = std::sin(TAU * t * mod_rate) * mod_depth * 0.5;

                    sample += std::sin(TAU * t * freq + phase_mod) *
                              std::exp(-t * freq * 0.003f) / (band + 1);
                }
            }

            ir[i] = sample * 0.05;
        }
}

    // Shepard Tone Reverb - Infinite ascending/descending scales
    template<typename Container>
    void shepardToneReverb(Container& ir, int length, bool ascending = true) {
        int num_octaves = 8;

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType sample = 0.0;

            // Shepard tone construction
            for (int octave = 0; octave < num_octaves; ++octave) {
                ValueType base_freq = 55.0f * std::pow(2.0f, octave);

                // Frequency sweep
                ValueType sweep_rate = ascending ? 0.1f : -0.1;
                ValueType current_freq = base_freq * std::pow(2.0f, sweep_rate * t);

                // Gaussian envelope for smooth octave transitions
                ValueType octave_center = num_octaves / 2.0;
                ValueType octave_distance = std::abs(octave - octave_center);
                ValueType octave_weight = std::exp(-octave_distance * octave_distance * 0.5f);

                sample += std::sin(TAU * t * current_freq) * octave_weight;
            }

            ir[i] = sample * 0.1;
        }

    }

    // Moire Pattern Reverb - Interference patterns
    template<typename Container>
    void moirePatternReverb(Container& ir, int length, ValueType pattern_freq = 0.5f) {

        for (int i = 0; i < length; ++i) {
            ValueType t = static_cast<ValueType>(i) / sr_;

            // Two slightly detuned pattern generators
            ValueType freq1 = 440.0f + pattern_freq;
            ValueType freq2 = 440.0f - pattern_freq;

            // Create moire interference pattern
            ValueType pattern1 = std::sin(TAU * t * freq1) * std::sin(TAU * t * freq1 * 0.1f);
            ValueType pattern2 = std::sin(TAU * t * freq2) * std::sin(TAU * t * freq2 * 0.1f);

            // Interference creates moire pattern
            ValueType moire = pattern1 * pattern2;

            // Multiple interference layers
            for (int layer = 2; layer <= 5; ++layer) {
                ValueType layer_freq1 = 440.0f * layer + pattern_freq * layer;
                ValueType layer_freq2 = 440.0f * layer - pattern_freq * layer;

                ValueType layer_pattern1 = std::sin(TAU * t * layer_freq1) *
                                        std::sin(TAU * t * layer_freq1 * 0.05f);
                ValueType layer_pattern2 = std::sin(TAU * t * layer_freq2) *
                                        std::sin(TAU * t * layer_freq2 * 0.05f);

                moire += (layer_pattern1 * layer_pattern2) / layer;
            }

            ir[i] = moire * 0.3;
        }
    }

    // Brownian Motion Reverb - Random walk
    template<typename Container>
    void brownianMotion(Container& ir, int length, ValueType step_size = 0.01f) {
        ValueType position = 0.0;

        for (int i = 0; i < length; ++i) {
            // Brownian motion step
            position += (gaussian(rng) * step_size);
            position = std::max(-5.0, std::min(5.0, (ValueType)position)); // Boundary conditions

            // Convert position to audio
            ValueType t = static_cast<ValueType>(i) / sr_;
            ValueType freq = 440.0f * std::pow(2.0f, position * 0.1f); // Position controls pitch

            // Multiple voices based on position
            ValueType sample = std::sin(TAU * t * freq);
            sample += std::sin(TAU * t * freq * 1.5f) * 0.5; // Fifth
            sample += std::sin(TAU * t * freq * 2.0f) * 0.25; // Octave

            // Position also controls timbre
            sample = std::tanh(sample * (1.0f + std::abs(position) * 0.2f));

            ir[i] = sample * 0.3;
        }

    }


    // Continue with remaining methods using the same pattern...
    // [Rest of the methods follow the same template pattern]

private:
    // Helper functions
    int fibonacci(int n) {
        if (n <= 1) return n;
        int a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            int temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    ValueType hermitePolynomial(int n, ValueType x) {
        if (n == 0) return 1.0;
        if (n == 1) return 2.0f * x;

        ValueType h0 = 1.0f, h1 = 2.0f * x;
        for (int i = 2; i <= n; ++i) {
            ValueType h2 = 2.0f * x * h1 - 2.0f * (i - 1) * h0;
            h0 = h1;
            h1 = h2;
        }
        return h1;
    }

    int factorial(int n) {
        int result = 1;
        for (int i = 2; i <= n && i <= 10; ++i) { // Cap at 10 for numerical stability
            result *= i;
        }
        return result;
    }

    template<typename Container>
    void normalizeIR(Container& ir, int length) {
        ValueType max_val = 0.0;
        for (int i = 0; i < length; ++i) {
            max_val = std::max(max_val, std::abs(static_cast<ValueType>(ir[i])));
        }

        if (max_val > 0.0f) {
            ValueType scale = 0.8f / max_val; // Leave some headroom
            for (int i = 0; i < length; ++i) {
                ir[i] = static_cast<typename Container::value_type>(static_cast<ValueType>(ir[i]) * scale);
            }
        }
    }

    // Helper for Hadamard matrix generation
    void generateHadamardMatrix(std::vector<std::vector<int>>& matrix, int order) {
        int size = 1 << order;

        if (order == 0) {
            matrix[0][0] = 1;
            return;
        }

        // Recursive Hadamard construction
        int half_size = size / 2;
        std::vector<std::vector<int>> half_matrix(half_size, std::vector<int>(half_size));
        generateHadamardMatrix(half_matrix, order - 1);

        // Fill quadrants
        for (int i = 0; i < half_size; ++i) {
            for (int j = 0; j < half_size; ++j) {
                matrix[i][j] = half_matrix[i][j];
                matrix[i][j + half_size] = half_matrix[i][j];
                matrix[i + half_size][j] = half_matrix[i][j];
                matrix[i + half_size][j + half_size] = -half_matrix[i][j];
            }
        }
    }

    // Simple Perlin-like noise function
    ValueType perlinNoise(ValueType x) {
        int i = static_cast<int>(std::floor(x));
        ValueType f = x - i;

        // Simple interpolation
        ValueType u = f * f * (3.0f - 2.0f * f);

        // Pseudo-random gradients
        ValueType grad1 = std::sin(i * 127.1f + 311.7f) * 43758.5453;
        ValueType grad2 = std::sin((i + 1) * 127.1f + 311.7f) * 43758.5453;

        grad1 = grad1 - std::floor(grad1);
        grad2 = grad2 - std::floor(grad2);

        return grad1 * (1.0f - u) + grad2 * u;
    }
};
#endif /* dead experimental IR generators */

#include <type_traits>

/* the selector names, the sub-space map and the DSP dispatch all index the
   same algorithm ids */
static_assert(SpectralDelay::SPECDELNUMMODES == (int)std::size(specdelmodes),
	"SPECTRAL DELAY algorithm list out of step with specdelmodes in gs_common.h");
static_assert(SpectralDelay::SPECDELUP == 0 && SpectralDelay::SPECDELADAPT == 11
	&& SpectralDelay::SPECDELVERB == 12 && SpectralDelay::SPECDELPUREVERB == 13,
	"SPECTRAL DELAY algorithm ids must stay contiguous from 0");

enum class PhaseMode {
    Linear,
    Random,
    Reverse
};

enum class SpectralShape {
    Linear,
    Hann,
    Quadratic,
    Sigmoid
};

template <typename T>
void generateSpectralDelayIR(T& ir,
                             int fftSize,
                             typename T::value_type maxDelaySamples,
                             typename T::value_type startNorm = 0.0,
                             typename T::value_type stopNorm = 1.0,
                             PhaseMode mode = PhaseMode::Linear,
                             SpectralShape shape = SpectralShape::Linear)
{
    using value_type = typename T::value_type;
    static_assert(std::is_floating_point<value_type>::value, "Container must hold real numbers");

    const int nyq = fftSize / 2;
    const int startBin = std::clamp(static_cast<int>(startNorm * nyq), 1, nyq - 1);
    const int stopBin  = std::clamp(static_cast<int>(stopNorm  * nyq), 1, nyq - 1);
    const int numBins = std::max(1, stopBin - startBin);
    /* a band of exactly one bin used to divide 0/0 here and write NaN into
       the IR - the loop runs once with bin == startBin, so any nonzero
       denominator gives the correct t = 0 */
    const value_type tDenom = static_cast<value_type>(numBins > 1 ? numBins - 1 : 1);

    std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<value_type> uniform(static_cast<value_type>(0), static_cast<value_type>(1));

    ir[0] = static_cast<value_type>(1); // DC
    ir[1] = static_cast<value_type>(1); // Nyquist

    for (int bin = startBin; bin < stopBin; ++bin) {
        value_type t = static_cast<value_type>(bin - startBin) / tDenom;
        value_type delayFactor = static_cast<value_type>(0);

        switch (shape) {
            case SpectralShape::Linear:    delayFactor = t; break;
            case SpectralShape::Hann:      delayFactor = static_cast<value_type>(0.5) * (1 - std::cos(t * PI_P)); break;
            case SpectralShape::Quadratic: delayFactor = t * t; break;
            case SpectralShape::Sigmoid:   delayFactor = static_cast<value_type>(1) / (static_cast<value_type>(1) + std::exp(-(t - 0.5f) * 10)); break;
        }

        value_type delaySamples = delayFactor * maxDelaySamples;

        value_type phase = static_cast<value_type>(0);
        switch (mode) {
            case PhaseMode::Linear:
                phase = -TWOPI_P * delaySamples * static_cast<value_type>(bin) / static_cast<value_type>(fftSize);
                break;
            case PhaseMode::Reverse:
                phase = +TWOPI_P * delaySamples * static_cast<value_type>(bin) / static_cast<value_type>(fftSize);
                break;
            case PhaseMode::Random:
                phase = uniform(rng) * TWOPI_P;
                break;
        }

        const int realIdx = 2 * bin + 0;
        const int imagIdx = 2 * bin + 1;

        ir[realIdx] = std::cos(phase);
        ir[imagIdx] = std::sin(phase);
    }
}


/* One unit-magnitude tap per bin, arrival time from a delay law.
   law(t, fnorm) -> 0..1 fraction of the IR; t is the position inside the
   processed band, fnorm the bin's frequency as a fraction of Nyquist.
   Packed real layout [DC, NYQ, re, im, ...]; bins outside the band stay
   zero (= removed), matching the original RAMP modes. */
template <typename Law>
static void generateLawIR(MYFLOAT* fir, int n, MYFLOAT a, MYFLOAT b, Law&& law)
{
    const int nyq = n / 2;
    const int startBin = std::clamp((int)(a * nyq), 1, nyq - 1);
    const int stopBin = std::clamp((int)(b * nyq), 1, nyq - 1);
    fir[0] = FL(1.0); // DC
    fir[1] = FL(1.0); // Nyquist
    const int numBins = stopBin - startBin;
    if (numBins < 1)
        return;
    const MYFLOAT tDenom = (MYFLOAT)(numBins > 1 ? numBins - 1 : 1);
    for (int bin = startBin; bin < stopBin; ++bin) {
        const MYFLOAT t = (MYFLOAT)(bin - startBin) / tDenom;
        MYFLOAT d = law(t, (MYFLOAT)bin / (MYFLOAT)nyq);
        d = d < FL(0.0) ? FL(0.0) : (d > FL(1.0) ? FL(1.0) : d);
        /* n-1, not n: a full-length delay is congruent to 0 in a circular
           spectrum, which would park the top of the law at t=0 instead */
        const MYFLOAT delay = d * (MYFLOAT)(n - 1);
        const MYFLOAT phase = -TWOPI_P * delay * (MYFLOAT)bin / (MYFLOAT)n;
        fir[2 * bin] = std::cos(phase);
        fir[2 * bin + 1] = std::sin(phase);
    }
}

/* Per-bin delay WITH FEEDBACK: each bin gets a decaying tap series instead of
   a single tap - a delay line with feedback r, one per bin, baked into the IR
   (the IR length simply caps the tail). The first arrival follows the RAMP UP
   law over the first half of the IR so several repeats fit; DAMP shortens the
   series toward the highs like a lowpass in the feedback path. Each bin's
   series is energy-normalised so FB changes density, not level. */
static void generateFeedbackIR(MYFLOAT* fir, int n, MYFLOAT a, MYFLOAT b,
    MYFLOAT r, MYFLOAT dampAmt)
{
    const int nyq = n / 2;
    const int startBin = std::clamp((int)(a * nyq), 1, nyq - 1);
    const int stopBin = std::clamp((int)(b * nyq), 1, nyq - 1);
    fir[0] = FL(1.0);
    fir[1] = FL(1.0);
    const int numBins = stopBin - startBin;
    if (numBins < 1)
        return;
    const MYFLOAT tDenom = (MYFLOAT)(numBins > 1 ? numBins - 1 : 1);
    for (int bin = startBin; bin < stopBin; ++bin) {
        const MYFLOAT t = (MYFLOAT)(bin - startBin) / tDenom;
        const MYFLOAT fnorm = (MYFLOAT)bin / (MYFLOAT)nyq;
        /* first arrival: n/16 at the bottom of the band, n/2 at the top */
        const MYFLOAT D = (MYFLOAT)n * (FL(0.0625) + t * FL(0.4375));
        const MYFLOAT g = r * std::exp(FL(-3.0) * dampAmt * fnorm);
        MYFLOAT re = FL(0.0), im = FL(0.0), gain = FL(1.0), e2 = FL(0.0);
        for (int m = 1; m <= 64; ++m) {
            const MYFLOAT delay = (MYFLOAT)m * D;
            if (delay >= (MYFLOAT)(n - 1))
                break;
            const MYFLOAT phase = -TWOPI_P * delay * (MYFLOAT)bin / (MYFLOAT)n;
            re += gain * std::cos(phase);
            im += gain * std::sin(phase);
            e2 += gain * gain;
            gain *= g;
            if (gain < FL(1e-3))
                break;
        }
        const MYFLOAT norm = e2 > FL(0.0) ? FL(1.0) / std::sqrt(e2) : FL(0.0);
        fir[2 * bin] = re * norm;
        fir[2 * bin + 1] = im * norm;
    }
}


enum class ShepardDirection { Ascend, Descend, Static };

template<typename T>
void generateShepardReverbIR(T& ir,
                             int n,
                             double startNormFreq = 0.0,
                             double stopNormFreq  = 1.0,
                             double octaveSpan = 3.0,  // How many octaves to span across frequency range
                             double fadeSpreadRatio = 0.2, // e.g. 2% of fftSize
                             ShepardDirection direction = ShepardDirection::Ascend)
{
    const size_t fftSize = n;
    const size_t nyquistBin = fftSize / 2;

    std::fill(ir.begin(), ir.begin()+n, 0.0);

    // Frequency bounds in normalized units (0-1 maps to 0-Nyquist)
    const double freqMinNorm = std::clamp(startNormFreq, 0.0, 1.0);
    const double freqMaxNorm = std::clamp(stopNormFreq, 0.0, 1.0);

    const double fadeSpreadBins = std::max(1.0, fftSize * fadeSpreadRatio);
    const double centerTimeNorm = 0.5;

    for (size_t bin = 1; bin < nyquistBin; ++bin) // Skip DC
    {
        // Normalized frequency (0-1 from DC to Nyquist)
        double freqNorm = (double)bin / nyquistBin;

        if (freqNorm < freqMinNorm || freqNorm > freqMaxNorm)
            continue;

        // Map frequency range to pitch class using octave span
        double freqRangePosition = (freqNorm - freqMinNorm) / (freqMaxNorm - freqMinNorm);
        double pitchClass = std::fmod(freqRangePosition * octaveSpan, 1.0);

        double tNorm;
        switch (direction) {
            case ShepardDirection::Ascend:  tNorm = pitchClass; break;
            case ShepardDirection::Descend: tNorm = 1.0 - pitchClass; break;
            case ShepardDirection::Static:  tNorm = centerTimeNorm; break;
        }

        // Bin index of energy center
        double centerBin = tNorm * fftSize;

        // Gaussian envelope
        double env = std::exp(-0.5 * std::pow((double)bin - centerBin, 2.0) / (fadeSpreadBins * fadeSpreadBins));

        // Phase for time offset (normalized units)
        double delayNormalized = tNorm;
        double binNormalized = (double)bin / fftSize;
        double phase = -2.0 * PI_P * binNormalized * delayNormalized * fftSize;

        double re = env * std::cos(phase);
        double im = env * std::sin(phase);

        // Write to interleaved real format: ir[2*bin] = re, ir[2*bin+1] = im
        size_t index = bin * 2;
        if (index + 1 < fftSize)
        {
            ir[index] = re;
            ir[index + 1] = im;
        }
    }

    // DC and Nyquist explicitly zero
    ir[0] = 0.0; // DC
    ir[1] = 0.0; // Nyquist
}


/* nearest prime at or below x. Two delay lines sharing a factor share that
   many modes; primes share only DC. Used for both the LONGVERB FDN line
   lengths and the allpasses nested inside them. */
static int32_t specdelPrimeAtMost(int32_t x) {
	auto isP = [](int32_t v) {
		if (v < 2) return false;
		for (int32_t d = 2; d * d <= v; d++)
			if (v % d == 0) return false;
		return true;
	};
	while (x > 2 && !isP(x)) x--;
	return x < 2 ? 2 : x;
}

/* ===================== PureKernelGen (PUREVERB kernel) ===================
   The looped-IR draw: wet feedback goes straight through this kernel and
   nothing else (see the PUREVERB block in SpectralDelay::compute). The draw is synthesised from an explicit BOUNDED
   DELAY-DENSITY: per bin a group delay tau drawn from a target density
   p(tau), phases = the integral of tau over frequency.
   v7/v8: the density is EXPONENTIAL with fixed time constant T/4 from a
   fixed ~20 ms floor - decoupled from RT60. v11 replaces the per-bin
   MAGNITUDE law |H_k| = e^(-lam*tau_k)/F with the EXACT pole condition
   it was only ever approximating - see below.
   (v6 kept |H| = 1 at every bin and squeezed the density onto [0.2T, T]
   with an exp(-2*lam*tau) tilt instead - correct decay, but a kernel with
   NO energy before 0.2T turns rhythmic input into audible repeats at the
   loop period plus a 0.2T onset gap: the "delay in the tail" report.
   And lowering v6's floor without the magnitude law is not an option:
   fast paths at |H| = 1 die at -ln(F)/tau per second, leaving sparse
   slow modes = the v4 static-ringing failure again. v7 tried a UNIFORM
   density: at long RT60 the kernel envelope became a flat-topped
   rectangle - a beat "sweeps in and out and the sweep repeats at TIME"
   (heard). Renewal theory names the cure: the envelope ripple of a
   recirculating kernel at the loop period vanishes exactly when the
   per-pass energy density is MEMORYLESS, so the density must be
   exponential-from-~zero; and it must NOT be tied to lam or it
   degenerates both ways - flat when RT60 >> T, a discrete spike when
   RT60 << T. Fixed T/4 keeps the shape at every setting.) Properties:
   - every recirculation path decays at exactly the RT60 rate no matter
     its delay, so the support reaches down to ~20 ms: full-length
     dispersion at every RT60 - a burst comes back as wash, not repeats;
   - bounded recirculation: slowest populated delay is T and the modes
     there are DENSE, so the tail ends in wash, not tones;
   - EVOLVE = a Metropolis walk on the taus: small steps, preserves the
     density exactly, and the integrated-phase bound keeps consecutive
     draws hybrid-safe (sigma chosen so delta-phi <= ~0.15 rad).
   v11 - THE EXACT POLE CONDITION. Every version up to v10 controlled the
   decay with a per-bin proxy: give bin k the group delay tau_k and the
   magnitude e^(-lam*tau_k)/F so that one trip round the loop costs
   exactly the RT60 rate. That is only correct when the kernel's energy
   near a frequency really does sit at ONE delay. Where it is spread over
   a RANGE of delays the proxy reads the centroid and under-counts the
   late part, so those poles decay SLOWER than RT60 - the v5..v10
   "unconstrainable between-bin group delay" saga, patched with an
   interpolated-law cap (v7) and then a rate-slack cap (v10) without ever
   being cured. It has a closed form. Closed-loop poles are the roots of
   Q(u) = 1 with u = z^-1 and Q(u) = g*sum_t h[t]*u^t, a POLYNOMIAL, so
   analytic; max-modulus on the disc |u| <= e^lam then says: if |Q| <= 1
   on the contour |u| = e^lam there is no root inside it, i.e. EVERY pole
   satisfies |z| <= e^-lam and nothing can ring longer than RT60. And on
   that contour Q is exactly g times the spectrum of the kernel with the
   decay divided out. One line, one convex set:

       |FFT( h[t] * e^(lam*t) )| <= 1/F     for all w.

   For a single delay this reduces to F*|H| <= e^(-lam*tau) - the old law
   is just its local-delay approximation - but it is exact for any kernel.
   Three things fall out. (1) The cap is now FLAT (a scalar), where the
   law's cap jumped bin to bin with the iid taus; a jumpy cap is what a
   smooth-envelope kernel cannot follow, which is why v9's tau GROUPING
   reduced erosion - and grouping was rejected by ear, because coherent
   frequency runs read as delay. The flat cap buys the same thing with
   the taus left fully iid. (2) The design is lam-INDEPENDENT: work with
   the warped kernel w[t] = h[t]*e^(lam*t), whose target envelope is just
   sqrt(p(t)) and whose cap is a constant, then h = w*e^(-lam*t) at the
   very end. RT60 no longer touches the draw at all. (3) The whole
   magnitude-law/slack/interpolated-cap machinery disappears.
   POCS therefore runs over three CONVEX sets in w: {support n} /
   {|w(t)| <= sqrt(p(t)), cos^2-tapered end} / {dense-grid |W| <= 1/F}.
   The cap carries a 1% nudge (CAPN) because the dense grid is 4x
   oversampled and the true DTFT peaks slightly between its points; 0.99
   at 4x measured tighter than an exact cap at 16x, for free.
   Measured against v10, same taus, same density: slowest pole 1.50x RT60
   -> 1.01x (i.e. the stragglers that rang half again as long as
   everything else are gone - they are the sparse survivors that beat
   against each other, heard as a swing that accelerates as the survivor
   set thins); band-RT60 spread 1.55x -> 1.21x; envelope ripple 9.7 ->
   3.3 dB at RT60/T = 1; onset, echo bump and late-kernel tapness all
   unchanged, so no return of the delay percept. What it does NOT fix:
   the surviving poles are still only ~2% of the total, because erosion
   (dense |W| median ~0.85 of cap) costs -ln(r)/tau and so hurts SHORT
   delays most, where the density is highest. That erosion is
   irreducible: a finite sequence has perfectly flat |DTFT| only if it is
   an impulse - i.e. zero erosion and "a tap" are the same kernel. POCS
   depth, cap width and density time-constant were all swept and move it
   by <2%; a non-convex magnitude FILL raises it but puts the slow poles
   straight back (one band at 2.9x RT60 - measured). Convex => convergent
   and nonexpansive (evolution continuity guaranteed). After POCS the
   kernel is energy-normalised and the inverse folded into _loopg: wet
   loudness stays constant across RT60/DAMP while the loop product
   F*|W| - the pole condition - is untouched. */

MYFLOAT PureKernelGen::build(MYFLOAT* ir, int n, MYFLOAT sr, MYFLOAT lam,
	MYFLOAT dmp, MYFLOAT bandA, MYFLOAT bandB, std::mt19937& rng) {

	const int nb = n / 2;
	const int n4 = 4 * n;
	const int fo = n / 4;
	/* kernel band, in bins of the n-grid. Out-of-band bins carry ZERO: a
	   zero is under the flat cap, so no pole can live there and the pole
	   bound is untouched; and {0} is a convex set, so the POCS projection
	   below (clamp in band, zero outside) stays nonexpansive. sb starts at
	   1 - packed DC stays 0 by design. */
	const int sb = std::clamp((int)(bandA * (MYFLOAT)nb), 1, nb);
	const int eb = std::clamp((int)(bandB * (MYFLOAT)nb), sb, nb);
	std::uniform_real_distribution<MYFLOAT> uni(FL(0.0), FL(1.0));

	/* the loop feedback gain the pole condition is referenced to */
	constexpr MYFLOAT F = FL(0.94);
	/* dense-grid cap on the WARPED spectrum |FFT(h*e^(lam t))|: the
	   exact no-pole-slower-than-RT60 condition (see header). CAPN
	   pays for the 4x grid missing the true between-point DTFT peaks
	   - 0.99 measured tighter than an exact cap at 16x oversampling.
	   NOTE the v9 lesson before adding anything else here: grouped
	   taus and a flat count-floor both fixed late-tail sparsity in
	   sim but read as DELAY by ear - coherent frequency runs are
	   localized time blobs, and flat density mass is a sustained
	   rectangle; late kernel energy must stay iid-incoherent and
	   exponential. The flat cap is what makes that affordable. */
	const MYFLOAT tB = (MYFLOAT)n - FL(2.0);
	/* fixed ~20 ms support floor: paths below -ln(F)/lam cannot
	   sustain RT60 at loop gain F (magnitude clamps at 1, they decay
	   a touch fast) but they are DENSE, so no sparse ringing - and the
	   gap-free onset is what kills the delay percept */
	const MYFLOAT tA = std::min(FL(0.02) * sr, FL(0.7) * tB);
	const MYFLOAT mu = (tB - tA) * FL(0.25);
	const MYFLOAT rampI = std::max(FL(64.0), sr * FL(0.020));
	const MYFLOAT rampO = FL(0.25) * (tB - tA);
	/* CAPN is ADAPTIVE, because the cost of a given cap overshoot is a
	   RATE: a pole whose loop product overshoots by r rings slower by
	   ln(r)/tau, so the relative damage is ln(r)/(lam*tau). At ordinary
	   settings lam*tau ~ 0.25 and the 4x grid's ~1.9% between-point
	   overshoot is worth ~1% of RT60; at RT60 >> TIME it collapses -
	   at RT60/TIME = 250, lam*tau ~ 0.01 and the SAME overshoot rings
	   modes at several times the target (measured 72 s against a 32 s
	   setting, sitting in the low band because that is where the mode
	   count is low enough to hear individuals). So pick CAPN to bound
	   the slowdown at ~10% of RT60 whatever the ratio, floor ~0.981. */
	const MYFLOAT taubar = tA + (tB - tA) * FL(0.25);
	const MYFLOAT CAPN = std::min(FL(0.99),
		exp(FL(0.1) * lam * taubar) / FL(1.019));
	const MYFLOAT WCAP = CAPN / F;
	/* v8: EXPONENTIAL delay density, fixed time constant T/4,
	   decoupled from RT60 - memoryless per-pass energy = no envelope
	   ripple at the loop period (see the header comment) */
	auto dens = [&](MYFLOAT t) -> MYFLOAT {
		if (t < tA || t > tB) return FL(0.0);
		MYFLOAT p = exp(-(t - tA) / mu);
		p *= std::min(FL(1.0), (t - tA) / rampI);
		p *= std::min(FL(1.0), (tB - t) / rampO);
		return std::max(p, FL(1e-20));
	};
	cdfOld.swap(cdf);
	oldA = gridA;
	oldB = gridB;
	if ((int)cdf.size() != KGRID)
		cdf.resize(KGRID);
	gridA = tA;
	gridB = tB;
	{
		MYFLOAT acc = FL(0.0);
		const MYFLOAT dt = (gridB - gridA) / (MYFLOAT)(KGRID - 1);
		for (int i = 0; i < KGRID; i++) {
			acc += dens(gridA + dt * (MYFLOAT)i);
			cdf[i] = acc;
		}
		for (int i = 0; i < KGRID; i++)
			cdf[i] /= acc;
	}
	auto icdf = [&](MYFLOAT u) -> MYFLOAT {
		int lo = 0, hi = KGRID - 1;
		while (lo < hi) {
			const int mid = (lo + hi) >> 1;
			if (cdf[mid] < u) lo = mid + 1; else hi = mid;
		}
		return gridA + (gridB - gridA) * (MYFLOAT)lo / (MYFLOAT)(KGRID - 1);
	};
	/* Metropolis step size from the hybrid bound:
	   delta_phi(nyq) ~ (2pi/n)*sqrt(nb/3)*sigma <= ~0.15 rad */
	const MYFLOAT sigma = FL(0.06) * sqrt((MYFLOAT)n);
	const bool densChanged = tA != tAold;
	tAold = tA;

	FFTd fftn(n);
	FFTd fft4(n4);
	spec.resize(n);
	dense.resize(n4);
	dense2.resize(n4);
	cap.resize(n);

	/* FFT round-trip scale of the 4n transform, probed empirically so
	   the POCS cap threshold is in TRUE spectral-gain units */
	MYFLOAT c4;
	{
		memset(dense.data(), 0, sizeof(MYFLOAT) * n4);
		dense[0] = FL(1.0);
		fft4.forward(dense.data(), dense2.data());
		fft4.backward(dense2.data());
		c4 = dense2[0];
		if (!(fabs(c4) > FL(1e-12)))
			c4 = (MYFLOAT)n4;
	}

	auto& tv = tau;
	if ((int)tv.size() != nb) {
		/* TIME changed: fresh draw from the density */
		tv.resize(nb);
		for (int k = 1; k < nb; k++)
			tv[k] = icdf(uni(rng));
	}
	else {
		if (densChanged && !cdfOld.empty() && oldB > oldA) {
			/* quantile remap old density -> new density */
			for (int k = 1; k < nb; k++) {
				MYFLOAT x = (tv[k] - oldA) / (oldB - oldA) *
					(MYFLOAT)(KGRID - 1);
				x = std::clamp(x, FL(0.0), (MYFLOAT)(KGRID - 1));
				const int i0 = std::min((int)x, KGRID - 2);
				const MYFLOAT fr = x - (MYFLOAT)i0;
				const MYFLOAT u = cdfOld[i0] +
					fr * (cdfOld[i0 + 1] - cdfOld[i0]);
				tv[k] = icdf(u);
			}
		}
		/* EVOLVE: one Metropolis step per bin - preserves p(tau),
		   moves every recirculation path a little */
		for (int k = 1; k < nb; k++) {
			MYFLOAT t2 = tv[k] + sigma * (uni(rng) * FL(2.0) - FL(1.0));
			if (t2 < gridA) t2 = FL(2.0) * gridA - t2;
			if (t2 > gridB) t2 = FL(2.0) * gridB - t2;
			t2 = std::clamp(t2, gridA, gridB);
			if (dens(t2) >= dens(tv[k]) * uni(rng))
				tv[k] = t2;
		}
	}

	/* the buffer now carries the WARPED kernel w = h*e^(lam*t)
	   until the un-warp at the end of POCS. Its magnitude is
	   FLAT at the cap - the taus enter through the phase only,
	   and lam does not enter the draw at all (see header).
	   Phases = integral of tau over frequency; packed
	   DC/Nyquist stay 0 (subsonics are the loop's slowest
	   decay). */
	memset(ir, 0, sizeof(MYFLOAT) * n);
	{
		MYFLOAT phi = FL(0.0);
		const MYFLOAT dw = TWOPI_P / (MYFLOAT)n;
		for (int k = 1; k < nb; k++) {
			/* phase accumulates across the whole axis so a band edge
			   move does not re-seed everything above it */
			phi -= tv[k] * dw;
			while (phi < FL(-PI_P))
				phi += FL(TWOPI_P);
			if (k < sb || k >= eb)
				continue;
			ir[2 * k] = WCAP * cos(phi);
			ir[2 * k + 1] = WCAP * sin(phi);
		}
	}
	fftn.backward(ir);
	/* calibrate the TRUE bin |W| to the cap (absorbs the FFT
	   round-trip scale, no convention assumptions); the design
	   is flat IN BAND, so the in-band bin RMS is the robust probe */
	{
		fftn.forward(ir, spec.data());
		MYFLOAT acc = FL(0.0);
		for (int k = sb; k < eb && k < nb; k++)
			acc += spec[2 * k] * spec[2 * k] +
				spec[2 * k + 1] * spec[2 * k + 1];
		const MYFLOAT m1 = sqrt(acc /
			(MYFLOAT)std::max(1, eb - sb));
		if (m1 > FL(1e-12)) {
			const MYFLOAT sc = WCAP / m1;
			for (int i = 0; i < n; i++)
				ir[i] *= sc;
		}
	}

	/* envelope cap on the WARPED kernel - the decay is divided
	   out, so it is exactly sqrt(p(t)): the time-domain guard
	   that energy may not sit later than its tau claims. Flat
	   before tA, and POCS uses that slack to back-fill the early
	   time, which is what kills the onset gap; cos^2 to zero over
	   the last quarter (v1's cliff).
	   HOW TIGHT: 4*rms decaying with 2*mu. DO NOT LOOSEN THIS -
	   8*rms / 3*mu was tried and REJECTED BY EAR: "now the delay
	   based behaviour returned", the same regression the design law
	   warns about. The cap is the ONLY thing holding the late kernel
	   energy down to what its tau claims; loosen it and every pass
	   carries audible energy all the way to T, so the loop's repeat
	   at the kernel length becomes a delay.
	   The measurements said it was nearly free and they were WRONG -
	   worth remembering. Loosening genuinely does cut erosion (F|W|
	   median 0.684 -> 0.892, p05 0.370 -> 0.721, late-tail spread
	   roughly halved in dB at every RT60/T from 8 to 500) with onset,
	   envelope ripple, tapness, r@T and the pole bound all neutral or
	   better. The ONLY metric that moved the wrong way was the echo
	   bump, +0.3..0.5 dB - and that one is the ear's whole verdict.
	   Weight it accordingly next time.
	   So erosion is NOT to be bought with envelope slack. Note also
	   what is NOT a lever: POCS iterations 12 -> 120 change F|W|
	   median by 0.001, and beyond 8*rms/3*mu the cap stops binding
	   entirely (12*mu and no cap at all measure identical), so there
	   is no useful middle ground further out either. */
	{
		const int i0 = (int)tA;
		const int i1 = std::min(n,
			(int)(tA + ((MYFLOAT)n - tA) * FL(0.1)));
		MYFLOAT rms = FL(0.0);
		for (int i = i0; i < i1; i++)
			rms += ir[i] * ir[i];
		rms = sqrt(rms / (MYFLOAT)std::max(1, i1 - i0));
		const MYFLOAT capBody = FL(4.0) * rms;
		for (int i = 0; i < n; i++) {
			const MYFLOAT t = (MYFLOAT)i;
			cap[i] = capBody * (t <= tA ? FL(1.0)
				: exp(-(t - tA) / (FL(2.0) * mu)));
		}
		for (int i = 0; i < fo; i++)
			cap[n - fo + i] *= FL(0.5) + FL(0.5) *
				cos(FL(PI_P) * (MYFLOAT)i / (MYFLOAT)fo);
	}

	/* POCS: envelope clip -> dense |W| <= flat cap -> support
	   truncate. Packed 4n layout: [0] = DC = dense bin 0,
	   [1] = Nyquist = dense bin 2n, pair [2j],[2j+1] = bin j. */
	/* the band on the 4x dense grid; the envelope clip re-leaks a little
	   energy out of band every iteration and this projection takes it back
	   out, so the final kernel's out-of-band residue is one clip's worth */
	const int jLo = 4 * sb, jHi = 4 * eb;
	for (int it = 0; it < 12; it++) {
		for (int i = 0; i < n; i++)
			ir[i] = std::clamp(ir[i], -cap[i], cap[i]);
		memcpy(dense.data(), ir, sizeof(MYFLOAT) * n);
		memset(dense.data() + n, 0, sizeof(MYFLOAT) * (n4 - n));
		fft4.forward(dense.data(), dense2.data());
		/* packed DC and Nyquist are out of band by construction */
		dense2[0] = FL(0.0);
		dense2[1] = FL(0.0);
		for (int j = 1; j < 2 * n; j++) {
			if (j < jLo || j >= jHi) {
				dense2[2 * j] = FL(0.0);
				dense2[2 * j + 1] = FL(0.0);
				continue;
			}
			const MYFLOAT m = sqrt(dense2[2 * j] * dense2[2 * j] +
				dense2[2 * j + 1] * dense2[2 * j + 1]);
			if (m > WCAP) {
				const MYFLOAT f = WCAP / m;
				dense2[2 * j] *= f;
				dense2[2 * j + 1] *= f;
			}
		}
		fft4.backward(dense2.data());
		const MYFLOAT isc = FL(1.0) / c4;
		for (int i = 0; i < n; i++)
			ir[i] = dense2[i] * isc;
	}
	for (int i = 0; i < n; i++)
		ir[i] = std::clamp(ir[i], -cap[i], cap[i]);

	/* UN-WARP, PER BAND: h[t] = sum_b w_b[t]*e^(-lam_b*t), where w_b is
	   w restricted to the bins whose target decay rate is lam_b. This is
	   the only place RT60 and DAMP enter the kernel, and it is what makes
	   DAMP a RATE.
	   What it replaces and why: DAMP used to be a one-pole lowpass in the
	   feedback path plus a matched glide baked into the draw, i.e. a fixed
	   attenuation PER PASS. Nothing in that accounted for how many passes
	   fit in RT60, so its effect scaled with RT60/tau. Measured at TIME 2 s
	   / RT60 120 s / DAMP 0.8: 6 kHz decayed in 1.1 s, 1 kHz in 2.3 s,
	   200 Hz in 11.8 s, sub-200 at the full 120 s - a two-decade tilt
	   nobody asked for, and it got worse the longer the reverb. As a rate
	   the tilt is fixed: DAMP sets the RATIO lam_hf/lam_lf, so the balance
	   is identical at every RT60 and every TIME.
	   Exactness: for bin k the pole rate is lam - ln(g|W_k|)/tau_k, and
	   bin k's energy sits at tau_k, so multiplying its band by e^(-lam_b t)
	   adds exactly lam_b - lam to that bin's rate. No per-bin cap is
	   needed, so the dense cap stays FLAT - the whole point of v11 (a
	   tau-dependent cap is jumpy bin-to-bin, which is what erodes the
	   spectrum). The masks are a partition of unity, so at DAMP = 0 every
	   bin lands on lam and this is bit-identical to the plain e^(-lam t).
	   The pole bound survives: |W_b| <= |W| <= WCAP bin by bin. */
	{
		/* damping profile: the rate multiplier rises GEOMETRICALLY with log
		   frequency, D(f) = dmax^u with u = log(f/f0)/log(f1/f0) clamped to
		   [0,1]. Every octave multiplies the decay rate by the same factor,
		   which is what an absorbing room does and what puts the mid band
		   properly BETWEEN the low and the high.
		   NOT A SHELF. v11.0 used D = 1+(dmax-1)*f^2/(f^2+fc^2) with DAMP
		   sweeping fc down and dmax up together. That saturates: by DAMP 1
		   fc was 758 Hz and dmax 15, so every bin above ~1.5 kHz landed on
		   the SAME rate. Measured at RT60 8 s / DAMP 1, slowest-pole RT60
		   per band was 6.8 s (60-160 Hz), 3.4, 1.2, 0.7, 0.6 (2.5-6.5 kHz):
		   a 12x split with no mid in it - highs and mids gone together
		   while the low still rang. The shelf also reached down INTO the
		   low band, so full DAMP shortened the whole reverb rather than
		   tilting it.
		   DAMP ABSORBS AS WELL AS TILTS. A tilt alone anchors the bottom
		   at D = 1, so no amount of DAMP could shorten the low band: it
		   measured 27.2 s at every DAMP setting on a 40 s target while the
		   top octave fell to 9.2 s. That is not a darker room, it is a
		   stranded bass - real absorption shortens the WHOLE spectrum and
		   tilts on top of that. So the profile is
		       D(f) = dall * dtil^u
		   with dall the whole-spectrum absorption and dtil the extra
		   reaching the top. At full DAMP that is 2x everywhere and 5x at
		   16 kHz (ratio 2.5, in the range real absorptive rooms measure).
		   The pole bound is untouched: dall >= 1 keeps every band's rate at
		   or above lam, so nothing rings longer than RT60. */
		const MYFLOAT dall = FL(1.0) + FL(1.0) * dmp;
		const MYFLOAT dtil = FL(1.0) + FL(1.5) * dmp;
		const MYFLOAT dmax = dall * dtil;
		if (!(dmax > FL(1.0000001))) {
			for (int i = 0; i < n; i++)
				ir[i] *= exp(-lam * (MYFLOAT)i);
		}
		else {
			/* NLEV levels uniformly spaced in log D, and since log D is
			   linear in log f the levels are equal log-frequency slices:
			   20 over 7.3 octaves = 0.37 octave each, finer than a
			   third-octave band. */
			constexpr int NLEV = 20;
			constexpr MYFLOAT TF0 = FL(100.0), TF1 = FL(16000.0);
			const MYFLOAT itsp = FL(1.0) / log(TF1 / TF0);
			/* levels span [dall, dall*dtil] geometrically, so the level
			   index is still exactly u*(NLEV-1) */
			const MYFLOAT ldm = log(std::max(dtil, FL(1.0000001)));
			/* n-point round-trip scale, probed like c4 */
			MYFLOAT cn;
			{
				memset(dense.data(), 0, sizeof(MYFLOAT) * n);
				dense[0] = FL(1.0);
				fftn.forward(dense.data(), dense2.data());
				fftn.backward(dense2.data());
				cn = dense2[0];
				if (!(fabs(cn) > FL(1e-12)))
					cn = (MYFLOAT)n;
			}
			fftn.forward(ir, spec.data());
			MYFLOAT* acc = dense2.data();
			MYFLOAT* work = dense.data();
			memset(acc, 0, sizeof(MYFLOAT) * n);
			/* per-bin weight onto the two neighbouring rate levels */
			for (int b = 0; b < NLEV; b++) {
				const MYFLOAT lev = dall * exp(ldm * (MYFLOAT)b /
					(MYFLOAT)(NLEV - 1));
				bool any = false;
				memset(work, 0, sizeof(MYFLOAT) * n);
				for (int k = 0; k <= n / 2; k++) {
					const MYFLOAT f = (MYFLOAT)k * sr / (MYFLOAT)n;
					/* x = log(D)/log(dmax)*(NLEV-1) = u*(NLEV-1) */
					const MYFLOAT x = std::clamp(
						log(std::max(f, TF0) / TF0) * itsp,
						FL(0.0), FL(1.0)) * (MYFLOAT)(NLEV - 1);
					const int b0 = std::clamp((int)x, 0, NLEV - 2);
					const MYFLOAT fr = std::clamp(x - (MYFLOAT)b0,
						FL(0.0), FL(1.0));
					MYFLOAT wgt = FL(0.0);
					if (b == b0) wgt = FL(1.0) - fr;
					else if (b == b0 + 1) wgt = fr;
					if (wgt <= FL(0.0))
						continue;
					any = true;
					if (k == 0)
						work[0] = spec[0] * wgt;       /* packed DC */
					else if (k == n / 2)
						work[1] = spec[1] * wgt;       /* packed Nyquist */
					else {
						work[2 * k] = spec[2 * k] * wgt;
						work[2 * k + 1] = spec[2 * k + 1] * wgt;
					}
				}
				if (!any)
					continue;
				fftn.backward(work);
				const MYFLOAT lb = lam * lev, isc = FL(1.0) / cn;
				for (int i = 0; i < n; i++)
					acc[i] += work[i] * isc * exp(-lb * (MYFLOAT)i);
			}
			memcpy(ir, acc, sizeof(MYFLOAT) * n);
		}
	}

	/* energy-normalise for constant wet loudness across RT60/DAMP
	   and fold the inverse into the loop gain: the LOOP product
	   F*|H| - the decay law - is untouched by the scaling */
	{
		MYFLOAT se = FL(0.0);
		for (int32_t i = 0; i < n; i++)
			se += ir[i] * ir[i];
		if (se > FL(1e-20)) {
			const MYFLOAT s = sqrt(FL(0.8) / se);
			for (int32_t i = 0; i < n; i++)
				ir[i] *= s;
			return std::min(F / s, FL(4.0));
		}
		return FL(0.0);
	}
}


void SpectralDelay::setupFunc() {

	N = (int)(_STATE->sr * SPECDELMAXDELMS * .001);
	ND2 = N / 2;
	M = N;
	fft = std::make_unique<FFT>(M);
	convolverNonUniform = std::make_unique<ConvolverNonUniform>(N);
	NYQ = M / 2;
	_buf.resize(M);
	fir = _buf.data();
	/* LONGVERB FDN storage - allocated here so the audio thread never
	   allocates (it only touches these once READY). The delay lines are sized
	   for the largest room SIZE can reach; the allpasses are fixed at the
	   SMALLEST room, so they are always a minor part of the round trip and
	   their lengths never change under the audio thread. */
	{
		for (int32_t u = 0; u < NFDN; u++) {
			const int32_t dmax = (int32_t)(_STATE->sr * FDNMAXMS * FL(0.001) *
				FDNRATIO[u] * FL(1.1)) + 8;
			_fdnDl[u].assign((size_t)dmax, FL(0.0));
			const int32_t mmin = (int32_t)(_STATE->sr * FDNMINMS * FL(0.001) *
				FDNRATIO[u]);
			for (int32_t k = 0; k < NFDNAP; k++) {
				const int32_t L = specdelPrimeAtMost(std::max(5,
					mmin / (k == 0 ? 5 : 11)));
				_fdnApLen[u][k] = L;
				_fdnAp[u][k].assign((size_t)L, FL(0.0));
			}
		}
	}
	std::mt19937 rng{ std::random_device{}() };
	do {
		lock.wait(false, std::memory_order_acquire);
		if (shouldExit)break;

		auto n = (int)(_STATE->sr * delold * 0.001);
		memset(fir, 0, sizeof(MYFLOAT) * n);
		const int imode = (int)modeold;
		if (imode == SPECDELPUREVERB) {
			/* the loop kernel: built whole by the generator (its own POCS
			   and FFTs), nothing downstream may touch it - the pole
			   condition |FFT(h*e^(lam t))| <= 1/F is exact on the buffer as
			   built, and any later shaping (DECAY/DAMPHF/envelopes) would
			   break it. BOUND is honoured INSIDE the generator (band mask
			   in the POCS projection), so only the band reverberates and
			   the rest of the spectrum passes dry. DAMP is hardwired: this
			   alg has no knobs of its own. */
			n &= ~1;
			n = std::clamp(n, 256, N);
			MYFLOAT a = LOG2NORMAL(boundaold) / _STATE->sr * FL(2.0);
			MYFLOAT b = LOG2NORMAL(boundbold) / _STATE->sr * FL(2.0);
			if (a > b)
				std::swap(a, b);
			/* RT60 = the honest ceiling of this topology, tied to DELAY */
			const MYFLOAT rt = PUREVERBRTX * (MYFLOAT)n / _STATE->sr;
			const MYFLOAT lam = FL(3.0) * FL(2.302585092994046) /
				(rt * _STATE->sr);
			_loopg.store(_pkGen.build(fir, n, _STATE->sr, lam,
				PUREVERBDAMP, a, b, rng));
			convolverNonUniform->loadIR(_buf.begin(), _buf.begin() + n);
			_pureKernelIn.store(true, std::memory_order_release);
			/* input diffuser chain (see the member comment in Convolver.h).
			   Rebuilt on DELAY changes only; loadIR copies, so reusing the
			   scratch after the main load is safe. */
			if (_pvPreN.load(std::memory_order_relaxed) != n ||
				a != _pvPreA || b != _pvPreB) {
				for (int32_t k = 0; k < PVNPRE; k++)
					if (!_pvPre[k])
						/* alloc >= 2*PARTSIZE4: the ctor sizes its last
						   stage as maxIRLen - PARTSIZE4, so anything
						   smaller is a negative segment. Short LOADS into
						   a large allocation are fine - inactive
						   partitions are skipped. */
						_pvPre[k] = std::make_unique<ConvolverNonUniform>(
							std::max(N >> (2 * (PVNPRE - k)),
								2 * PARTSIZE4));
				std::uniform_real_distribution<MYFLOAT> u01(FL(0.0),
					TWOPI_P);
				for (int32_t k = 0; k < PVNPRE; k++) {
					const int32_t L =
						std::clamp(n >> (2 * (PVNPRE - k)), 512, n) & ~1;
					/* decay = 60 dB over the NEXT stage's length (the
					   main kernel for the last stage) */
					const int32_t Ln = k + 1 < PVNPRE
						? (std::clamp(n >> (2 * (PVNPRE - 1 - k)), 512, n)
							& ~1)
						: n;
					memset(fir, 0, sizeof(MYFLOAT) * L);
					/* band-limited like the main kernel - the ER tap puts
					   this signal straight into the wet */
					const int32_t sbk = std::clamp((int32_t)(a * L / 2),
						1, L / 2);
					const int32_t ebk = std::clamp((int32_t)(b * L / 2),
						sbk, L / 2);
					for (int32_t j = sbk; j < ebk && j < L / 2; j++) {
						const MYFLOAT ph = u01(rng);
						fir[2 * j] = cos(ph);
						fir[2 * j + 1] = sin(ph);
					}
					FFTd lf(L);
					lf.backward(fir);
					const MYFLOAT rate = FL(6.9078) / (MYFLOAT)Ln;
					const int32_t fo = std::max(1, L / 8);
					for (int32_t i = 0; i < L; i++) {
						MYFLOAT e = exp(-rate * (MYFLOAT)i);
						if (i >= L - fo) {
							const MYFLOAT t = (MYFLOAT)(i - (L - fo)) /
								(MYFLOAT)fo;
							e *= FL(0.5) + FL(0.5) * cos(FL(PI_P) * t);
						}
						fir[i] *= e;
					}
					/* unit energy: the chain must smear, not level-shift */
					MYFLOAT se = FL(0.0);
					for (int32_t i = 0; i < L; i++)
						se += fir[i] * fir[i];
					if (se > FL(1e-20)) {
						const MYFLOAT sc = sqrt(FL(1.0) / se);
						for (int32_t i = 0; i < L; i++)
							fir[i] *= sc;
					}
					_pvPre[k]->loadIR(_buf.begin(), _buf.begin() + L);
				}
				_pvPreA = a;
				_pvPreB = b;
				_pvPreN.store(n, std::memory_order_relaxed);
				_pvPreReady.store(true, std::memory_order_release);
			}
			lock.clear(std::memory_order_release);
			lock.notify_all();
			_state.store(READY);
			if (shouldExit.load(std::memory_order_acquire)) break;
			continue;
		}
		/* any other algorithm's kernel is NOT loop-safe */
		_pureKernelIn.store(false, std::memory_order_release);
		if (imode != SPECDELGAUSS) {

			MYFLOAT a = LOG2NORMAL(boundaold) / _STATE->sr * 2.;
			MYFLOAT b = LOG2NORMAL(boundbold) / _STATE->sr * 2.;
			if (a > b) {
				std::swap(a, b);
			}
			n &= ~1; /* packed real FFT layout ([DC, NYQ, re, im, ...]) needs an even size;
			            pocketfft itself accepts any n, so no pow2 rounding required */
			/* SHAPE bends the ramp laws: .5 = linear (the original curve),
			   below sqrt-ish, above squared-ish */
			const MYFLOAT curve = exp2((shape->load() * FL(2.0) - FL(1.0)) * FL(2.0));

			switch (imode) {
			case SPECDELUP:
				generateLawIR(fir, n, a, b,
					[curve](MYFLOAT t, MYFLOAT) { return pow(t, curve); });
				break;
			case SPECDELDOWN:
				generateLawIR(fir, n, a, b,
					[curve](MYFLOAT t, MYFLOAT) { return FL(1.0) - pow(t, curve); });
				break;
			default:
			case SPECDELRAND:
				generateSpectralDelayIR(_buf, n, n - 1, a, b, PhaseMode::Random);
				break;
			case SPECDELFEEDBACK:
				generateFeedbackIR(fir, n, a, b, fb->load() * FL(0.97), damp->load());
				break;
			case SPECDELSTEPPED: {
				/* quantise the up-ramp into K arrival slots: the band turns
				   into K discrete echoes, a spectrum arpeggio */
				const MYFLOAT K = std::clamp((MYFLOAT)(int)steps->load(), FL(2.0), FL(64.0));
				generateLawIR(fir, n, a, b, [K](MYFLOAT t, MYFLOAT) {
					return std::floor(std::min(t, FL(0.9999)) * K) / K; });
				break;
			}
			case SPECDELRIPPLE: {
				/* alternating early/late bands: RIPPLES = how many, PHASE
				   slides the pattern across the spectrum */
				const MYFLOAT R = exp2(ripples->load() * FL(4.0));
				const MYFLOAT ph = ripplph->load() * TWOPI_P;
				generateLawIR(fir, n, a, b, [R, ph](MYFLOAT t, MYFLOAT) {
					return FL(0.5) + FL(0.5) * sin(TWOPI_P * R * t + ph); });
				break;
			}
			case SPECDELOCTAVE: {
				/* arrival by pitch class: all octaves of ROOT land together,
				   everything else spreads by its position in the octave */
				const MYFLOAT f0 = LOG2NORMAL(root->load());
				const MYFLOAT nyqHz = _STATE->sr * FL(0.5);
				generateLawIR(fir, n, a, b, [f0, nyqHz](MYFLOAT, MYFLOAT fnorm) {
					const MYFLOAT f = fnorm * nyqHz;
					if (f <= FL(0.0))
						return FL(0.0);
					const MYFLOAT pc = log2(f / f0);
					return pc - std::floor(pc); });
				break;
			}
			case SPECDELDRIFT: {
				/* persistent per-bin random walk. AMOUNT is the step size;
				   at the top it degenerates into a full re-roll, which is
				   the perpetually-reshuffling RANDOM cloud. Indexed by
				   normalised frequency so a DELAY change keeps the walk. */
				if ((int)_driftLaw.size() != NYQ + 1) {
					_driftLaw.assign(NYQ + 1, FL(0.0));
					_driftInit = false;
				}
				const MYFLOAT amt = driftamt->load();
				std::uniform_real_distribution<MYFLOAT> uni(FL(0.0), FL(1.0));
				if (!_driftInit || amt >= FL(0.999)) {
					for (auto& v : _driftLaw)
						v = uni(rng);
					_driftInit = true;
				}
				else if (amt > FL(0.0)) {
					std::normal_distribution<MYFLOAT> gauss(FL(0.0), amt * amt * FL(0.5));
					for (auto& v : _driftLaw) {
						v += gauss(rng);
						/* reflect back into 0..1 so the walk doesn't pile up
						   at the edges */
						while (v < FL(0.0) || v > FL(1.0))
							v = v < FL(0.0) ? -v : FL(2.0) - v;
					}
				}
				generateLawIR(fir, n, a, b, [this](MYFLOAT, MYFLOAT fnorm) {
					int idx = (int)(fnorm * NYQ);
					idx = idx < 0 ? 0 : (idx > NYQ ? NYQ : idx);
					return _driftLaw[idx]; });
				break;
			}
			case SPECDELBARBER: {
				/* the ripple law with its phase advancing every
				   regeneration: the early/late pattern sweeps endlessly
				   across the band, SPEED paces the regenerations */
				_barberPhase += FL(1.0) / FL(32.0);
				_barberPhase -= std::floor(_barberPhase);
				const MYFLOAT R = exp2(barbn->load() * FL(4.0));
				const MYFLOAT ph = _barberPhase * TWOPI_P;
				generateLawIR(fir, n, a, b, [R, ph](MYFLOAT t, MYFLOAT) {
					return FL(0.5) + FL(0.5) * sin(TWOPI_P * R * t + ph); });
				break;
			}
			case SPECDELTIDE: {
				/* triangle scan between the UP and DOWN laws. NOT a lerp of
				   the two ramps: halfway through, that becomes a constant -
				   every bin at the same delay - and the morph front then has
				   a moment where the old taps are erased and the new ones not
				   yet written for ALL bins at once = a full dropout (user
				   heard it). Instead the ramp's peak pivots across the band,
				   so the arrival times stay spread over the whole IR at every
				   scan position: UP = peak at the top edge, DOWN = peak at
				   the bottom, mid-scan = a tent. */
				_tidePos += FL(1.0) / FL(32.0);
				if (_tidePos >= FL(2.0))
					_tidePos -= FL(2.0);
				const MYFLOAT sc = _tidePos < FL(1.0) ? _tidePos : FL(2.0) - _tidePos;
				const MYFLOAT pk = FL(1.0) - sc; /* 1 = UP, 0 = DOWN */
				generateLawIR(fir, n, a, b, [pk, curve](MYFLOAT t, MYFLOAT) {
					const MYFLOAT d = t < pk
						? (pk > FL(0.0) ? t / pk : FL(1.0))
						: (pk < FL(1.0) ? (FL(1.0) - t) / (FL(1.0) - pk) : FL(1.0));
					return pow(d, curve); });
				break;
			}
			case SPECDELADAPT: {
				/* the spectral envelope of the recent input steers the law:
				   loud bands arrive early, quiet bands smear late (INVERT
				   flips that), DEPTH blends against the plain up-ramp */
				if (!_adaptFft)
					_adaptFft = std::make_unique<FFT>(ADAPTN);
				const int anyq = ADAPTN / 2;
				std::vector<MYFLOAT> snap(ADAPTN);
				const int wpos = _adaptW; /* racy by design, see _adaptRing */
				for (int i = 0; i < ADAPTN; i++)
					snap[i] = _adaptRing[(wpos + i) & (ADAPTN - 1)] *
						(FL(0.5) - FL(0.5) * cos(TWOPI_P * (MYFLOAT)i / (MYFLOAT)ADAPTN));
				_adaptFft->forward(snap.data());
				std::vector<MYFLOAT> env(anyq + 1);
				env[0] = log(fabs(snap[0]) + FL(1e-8));
				env[anyq] = log(fabs(snap[1]) + FL(1e-8));
				for (int i = 1; i < anyq; i++)
					env[i] = FL(0.5) * log(snap[2 * i] * snap[2 * i] +
						snap[2 * i + 1] * snap[2 * i + 1] + FL(1e-16));
				/* two box-filter passes ~ a smooth envelope, cheap */
				const int wlen = std::max(3, anyq / 48);
				std::vector<MYFLOAT> sm(anyq + 1);
				for (int pass = 0; pass < 2; pass++) {
					MYFLOAT acc = FL(0.0);
					int cnt = 0;
					for (int i = 0; i <= anyq; i++) {
						acc += env[i];
						cnt++;
						if (i >= wlen) {
							acc -= env[i - wlen];
							cnt--;
						}
						sm[i] = acc / (MYFLOAT)cnt;
					}
					env = sm;
				}
				/* normalise over the processed band */
				const int sb = std::clamp((int)(a * anyq), 1, anyq - 1);
				const int eb = std::clamp((int)(b * anyq), 1, anyq - 1);
				MYFLOAT mn = env[sb], mx = env[sb];
				for (int i = sb; i < std::max(sb + 1, eb); i++) {
					mn = std::min(mn, env[i]);
					mx = std::max(mx, env[i]);
				}
				const MYFLOAT range = mx - mn;
				const MYFLOAT depth = adepth->load();
				const bool inv = ainv->load() != FL(0.0);
				generateLawIR(fir, n, a, b,
					[&env, anyq, mn, range, depth, inv](MYFLOAT t, MYFLOAT fnorm) {
						/* a flat envelope (silence, or no contrast left after
						   smoothing - under ~1 dB across the band) says
						   nothing, and at DEPTH 1 it would collapse the law
						   to a constant = the same all-bins-in-the-gap
						   dropout TIDE had. Fall back to the plain ramp,
						   which is the DEPTH 0 sound. */
						if (range < FL(0.1))
							return t;
						int idx = (int)(fnorm * anyq);
						idx = idx < 0 ? 0 : (idx > anyq ? anyq : idx);
						const MYFLOAT e = (env[idx] - mn) / range;
						const MYFLOAT d = inv ? e : FL(1.0) - e;
						return (FL(1.0) - depth) * t + depth * d; });
				break;
			}
			case SPECDELVERB: {
				/* flat magnitude + PERSISTENT per-bin phases. Independent
				   re-rolls are out: the onset envelope below makes the
				   kernel head-heavy, so two independent draws differ
				   audibly there, and every completed morph sweep landed as
				   a lurch with period ~= one sweep (the reported
				   discontinuity at DELAY). EVOLVE now WALKS the phases -
				   bounded step, consecutive draws E[cos d] ~= 0.96
				   correlated - so the front crossfades between nearly
				   identical kernels and evolution reads as drift. The
				   kernel is outside the loop; a walk has no poles to move.
				   VERBPHSTEP is the drift-speed lever (EVOLVE already sets
				   the roll RATE). */
				const int32_t nyq2 = n / 2;
				const int32_t sb = std::clamp((int32_t)(a * nyq2), 1, nyq2 - 1);
				const int32_t eb = std::clamp((int32_t)(b * nyq2), 1, nyq2 - 1);
				if (_verbPhN != n) {
					std::uniform_real_distribution<MYFLOAT> u01(FL(0.0), TWOPI_P);
					if (_verbPhN > 0 && !_verbPh.empty()) {
						/* DELAY change: resample phases onto the new bin
						   grid. Group delay in samples is -dphi/dw and the
						   frequency AXIS does not move, only the bin
						   spacing - so nearest-bin phase transfer keeps
						   every tap where it was and the morph front
						   blends two aligned kernels. */
						const int32_t oldNyq = (int32_t)_verbPh.size();
						std::vector<MYFLOAT> np((size_t)nyq2);
						for (int32_t k = 0; k < nyq2; k++) {
							int32_t j = (int32_t)((int64_t)k * oldNyq / nyq2);
							if (j >= oldNyq) j = oldNyq - 1;
							np[(size_t)k] = _verbPh[(size_t)j];
						}
						_verbPh.swap(np);
					}
					else {
						_verbPh.resize((size_t)nyq2);
						for (int32_t k = 0; k < nyq2; k++)
							_verbPh[(size_t)k] = u01(rng);
					}
					_verbPhN = n;
				}
				else {
					constexpr MYFLOAT VERBPHSTEP = FL(0.5);
					std::uniform_real_distribution<MYFLOAT> stp(-VERBPHSTEP,
						VERBPHSTEP);
					for (int32_t k = sb; k < eb; k++)
						_verbPh[(size_t)k] += stp(rng);
				}
				fir[0] = FL(0.0);  /* packed DC - keep it out of the wet */
				fir[1] = FL(0.0);  /* packed Nyquist */
				for (int32_t k = sb; k < eb; k++) {
					fir[2 * k] = cos(_verbPh[(size_t)k]);
					fir[2 * k + 1] = sin(_verbPh[(size_t)k]);
				}
				break;
			}
			}

			FFTd fft2(n);
			fft2.backward(fir);
			if (imode == SPECDELRAND) {
				for (int32_t i = 0; i < n; i++) {
					auto decayAmount = pow(0.9, i / (double)n * 30) *2;
					fir[i] *= decayAmount;
				}
			}
		}
		else {
			tsl::random::Random random1;
			auto scale = tsl::random::randomfloat(0., 0.4) + 0.1;
			auto sampleDensity = tsl::random::randomfloat(0., .5) + .2;
			auto decayRate = tsl::random::randomfloat(0., 2e-4) + 2e-4;
			MYFLOAT a = LOG2NORMAL(boundaold);
			MYFLOAT b = LOG2NORMAL(boundbold);
			if (a > b) {
				std::swap(a, b);
			}

			double FrequencyBands[2] = { a, b/*
						500 * pow(2., tsl::Random::randomfloat(0., 4.) - 4) / _STATE->sr * 2.,
						500 * pow(2., tsl::Random::randomfloat(0., 4.)) / _STATE->sr * 2.*/ };

			Butterworth<MYFLOAT> filt{ _appState };
			filt.Reset();
			filt.SetHp(FrequencyBands[0]);
			filt.SetLp(FrequencyBands[1]);
			Butterworth<MYFLOAT> bw{ _appState };
			bw.Reset();
			bw.SetLp(250.);

			for (int32_t i = 1; i < n; i++) {
				auto t = i / _STATE->sr;
				auto sampleProbability = 1. - pow(sampleDensity, t);
				auto sampleOccurences = tsl::random::randomfloat(0., 1.) < sampleProbability;
				if (sampleOccurences) {
					auto sampleSigns = tsl::random::randomfloat(0., 1.) < 0.5 ? -1. : 1.;
					_buf[i] = (random1.gaussrand(0.05) + 1.) * sampleSigns;
				}
				else {
					_buf[i] = random1.gaussrand(scale);
				}
				auto decayAmount = pow(decayRate, (t / (delold * 0.002)));
				_buf[i] = (filt.tickLpHp6(_buf[i]) * decayAmount +
					bw.tickLp12(random1.gaussrand(0.0003))) * 0.005;
			}
			_buf[0] = (random1.gaussrand(0.05) + 1.) *
				(tsl::random::randomfloat(0., 1.) < 0.5 ? -1. : 1.);
		}

		/* Global IR shaping, every algorithm. The delay laws are unit-gain to
		   their very last tap, so the tail ends abruptly when the input stops;
		   DECAY is a plain exponential envelope (up to -80 dB across the IR)
		   and DAMP a one-pole lowpass whose cutoff glides down along the IR,
		   so the highs die first. Both 0 = the raw generated IR. */
		{
			const MYFLOAT dec = decay->load();
			const MYFLOAT dmp = damphf->load();
			if (dec > FL(0.0) && n > 1) {
				const MYFLOAT gstep = pow(FL(10.0), FL(-4.0) * dec / (MYFLOAT)n);
				MYFLOAT g = FL(1.0);
				for (int32_t i = 0; i < n; i++) {
					_buf[i] *= g;
					g *= gstep;
				}
			}
			/* ...except for LONGVERB, whose kernel must stay uncoloured: it is
			   OUTSIDE the loop, so a lowpass baked into it is a fixed EQ on
			   the wet, NOT damping - it darkens the reverb without making the
			   highs decay any faster, and it does so identically at every
			   RT60. Frequency-dependent DECAY only exists if the filter is
			   INSIDE the loop, which is where the FDN's per-line one-poles
			   are (see compute). One damping control, one place. */
			if (dmp > FL(0.0) && n > 1 && imode != SPECDELVERB) {
				/* cutoff as a fraction of sr, from 0.5 (transparent) down to
				   10^(-2.5*damp) of it - ~70 Hz at full damp and 48k */
				const MYFLOAT fcstep = pow(FL(10.0), FL(-2.5) * dmp / (MYFLOAT)n);
				MYFLOAT fc = FL(0.5);
				MYFLOAT y = FL(0.0);
				for (int32_t i = 0; i < n; i++) {
					const MYFLOAT a = FL(1.0) - exp(-TWOPI_P * fc);
					y += a * (_buf[i] - y);
					_buf[i] = y;
					fc *= fcstep;
				}
			}
		}

		/* LONGVERB: the kernel is a DIFFUSION/COLOUR stage now - convolved
		   once on the way into the FDN, never recirculated - so it must NOT
		   carry the RT60 (the FDN owns that) and it must NOT be a rectangle.
		   A flat random block DELAY ms long integrates the input instead of
		   starting it: the onset arrives as a slow swell, measured at 2.5% of
		   the kernel energy in the first 50 ms. Exponential ending 60 dB down
		   at the kernel's end, with a cos^2 landing so the tail does not step
		   (28.9% in the first 50 ms after). */
		if (imode == SPECDELVERB && n > 1) {
			const MYFLOAT dec = FL(6.9078) / (MYFLOAT)n;
			const int32_t fo = std::max(1, n / 8);
			for (int32_t i = 0; i < n; i++) {
				MYFLOAT e = exp(-dec * (MYFLOAT)i);
				if (i >= n - fo) {
					const MYFLOAT t = (MYFLOAT)(i - (n - fo)) / (MYFLOAT)fo;
					e *= FL(0.5) + FL(0.5) * cos(FL(PI_P) * t);
				}
				_buf[i] *= e;
			}
			/* unit energy: the kernel is colour and density only, so the tail
			   level must not move with DELAY, DECAY/DAMP or a re-roll */
			MYFLOAT se = FL(0.0);
			for (int32_t i = 0; i < n; i++)
				se += _buf[i] * _buf[i];
			if (se > FL(1e-20)) {
				const MYFLOAT sc = sqrt(FL(1.0) / se);
				for (int32_t i = 0; i < n; i++)
					_buf[i] *= sc;
			}
		}
         convolverNonUniform->loadIR(_buf.begin(), _buf.begin() + n);
         lock.clear(std::memory_order_release);
         lock.notify_all();
         _state.store(READY);
         /* the destructor may have published shouldExit while this pass was
            running; without this re-check the worker would go straight back
            into wait(false) on a flag it just cleared and never see it */
         if (shouldExit.load(std::memory_order_acquire)) break;
     } while (true);

 }



 void SpectralDelay::compute(MYFLOAT* in, int32_t s) {
     MYFLOAT ba = bounda->load();
     MYFLOAT bb = boundb->load();
     MYFLOAT mod = mode->load();
     MYFLOAT delay = del->load();
     const int imode = (int)mod;

     /* law-shaping per-algorithm params: any change regenerates the IR, the
        same as the four main knobs. The evo/speed params are deliberately NOT
        in this fingerprint - they only pace future self-regenerations. */
     const MYFLOAT sig = imode == SPECDELPUREVERB
         /* PUREVERB regenerates on DELAY and BOUND only (both fingerprinted
            separately below): RT60/DAMP/EVOLVE are hardwired and the global
            shaping knobs never touch a loop kernel */
         ? FL(0.0)
         : shape->load() + fb->load() + damp->load() +
         steps->load() + ripples->load() + ripplph->load() + root->load() +
         driftamt->load() + barbn->load() + adepth->load() + ainv->load() +
         decay->load() +
         /* the global DAMP does not reach the LONGVERB kernel, so it must not
            regenerate it either - otherwise the knob costs a re-roll and
            changes nothing */
         (imode == SPECDELVERB ? FL(0.0) : damphf->load());
     /* LONGVERB's own knobs are ALL compute-side now: the FDN owns the decay,
        so RT60/DAMP/SIZE/XFEED are per-block arithmetic and never touch the
        kernel. Only DELAY (the kernel length) and EVOLVE regenerate. */

     /* The evolving algorithms retrigger their own regeneration. Pace comes
        from the per-algorithm rate param, gated on the previous swap being
        fully in (idle) - so the morph RATE/FRONT still shape every handover,
        and a frozen front pauses evolution instead of piling up loads. */
     MYFLOAT evoSec = FL(0.0);
     switch (imode) {
     case SPECDELRAND: evoSec = evoSeconds(randevo->load()); break;
     case SPECDELGAUSS: evoSec = evoSeconds(gaussevo->load()); break;
     case SPECDELDRIFT: evoSec = evoSeconds(driftrate->load()); break;
     case SPECDELBARBER: evoSec = evoSeconds(barbspeed->load()); break;
     case SPECDELTIDE: evoSec = evoSeconds(tidespeed->load()); break;
     case SPECDELADAPT: evoSec = evoSeconds(arate->load()); break;
     case SPECDELVERB: evoSec = evoSeconds(verbevo->load()); break;
     /* hardwired continuous: idle-gating turns this into one Metropolis
        step of the taus per completed sweep, which is the ear-approved
        cadence - a static loop kernel rings at its accidental modes */
     case SPECDELPUREVERB: evoSec = FL(0.05); break;
     }
     bool evoDue = false;
     if (evoSec > FL(0.0) && _state.load() == READY && convolverNonUniform->idle()) {
         _evoCnt += s;
         evoDue = _evoCnt >= (int64_t)(evoSec * _STATE->sr);
     }
     else if (evoSec <= FL(0.0))
         _evoCnt = 0;

     /* Once the effect is on its way out, stop feeding the worker. The
        destructor joins it, and that join runs on the AUDIO THREAD: the fx
        queue is reaped inside the per-track processing loop (fx_queue.del in
        granulate.cpp), so a worker caught mid-pass - here a full-length FFT
        kernel build - stalls audio for its whole duration. destroyRequested is
        set a fade-out before readyToDestroy, so with new passes blocked the
        worker is idle by deletion time and the join costs nothing. Skipping
        the *old value writes below with it is deliberate and harmless: the
        effect is dying, and a stale fingerprint can only re-trigger a pass
        that is now blocked anyway. */
     if (!destroyRequested &&
         (delay != delold || bb != boundbold || ba != boundaold || mod != modeold ||
         sig != sigold || evoDue) && !lock.test_and_set(std::memory_order_acquire)) {
         modeold = mod;
         boundaold = ba;
         boundbold = bb;
         delold = delay;
         sigold = sig;
         _evoCnt = 0;
         lock.notify_one();
     }

     int state = _state.load();

     if (state == STARTING) {
         for (int32_t i = 0; i < s; i++)
             smmixgain(0.f, 1.f);
         return;
     }

     /* ADAPTIVE listens to its own input; keep the ring current so the next
        regeneration sees the last ~85 ms */
     if (imode == SPECDELADAPT) {
         for (int32_t i = 0; i < s; i++)
             _adaptRing[(_adaptW + i) & (ADAPTN - 1)] = in[i];
         _adaptW = (_adaptW + s) & (ADAPTN - 1);
     }

     /* shape how the next IR is swapped in - once per block, the stages read
        these while they walk the replacement front. RATE is log-scaled with an
        offset, so the bottom of its range comes out as exactly 0 = parked. */
     MYFLOAT morphr = LOG2NORMAL(morphrate->load()) - SPECDELRATEOFFS;
     if (morphr < 0.)
         morphr = 0.;
     /* PUREVERB: morph is SWEEPS PER SECOND, not taps per sample. The front
        advances at `rate` taps/sample, so a fixed rate sweeps a kernel in
        T/rate seconds - the same knob decorrelated a 1 s kernel in 0.5 s and
        a 10 s one in 5 s, leaving long kernels effectively static within a
        pass (= replays its smear, the delay percept). What must be smeared
        is a mode ringing for RT60, so decorrelation time must not scale
        with T. Cap 6 is a CPU bound: one completed sweep costs one build()
        (~0.45 us/tap on the setup thread). */
     if (imode == SPECDELPUREVERB)
         morphr = std::min(morphr * std::max(delay * FL(0.001), FL(0.05)),
             FL(6.0));
     convolverNonUniform->setMorph(morphr, morphfront->load(),
         morphrev->load() != 0.);

     /* LONGVERB: an 8-line FDN in front of the convolver, u = FDN(in),
        wet = conv(u). Everything that recirculates is a pure delay or a
        Schroeder allpass - the only two elements with exactly unit |H| at
        every frequency - so each line decays at precisely the rate its gain
        sets, and the kernel, sitting outside the loop with no feedback around
        it, has no poles at all: its ripple is colour, never decay. That is
        what decouples RT60 from DELAY. Per block we recompute the line
        lengths, gains, damping coefficients and the mixing angle; nothing
        here regenerates the kernel. */
     const bool loop = imode == SPECDELVERB;
     MYFLOAT fdnG[NFDN] = {}, fdnA[NFDN] = {};
     MYFLOAT cth = FL(1.0), sth = FL(0.0);
     if (loop) {
         const MYFLOAT rt = LOG2NORMAL(verbrt60->load());
         const MYFLOAT dmp = std::clamp(verbdamp->load(), FL(0.0), FL(1.0));
         /* DAMP absorbs AND tilts. A tilt alone leaves the bass sitting at
            the full RT60 while everything above it dies, which reads as a
            resonant low remnant rather than a darker room; dall shortens the
            whole spectrum, dtil is the extra that reaches the top. */
         const MYFLOAT dall = FL(1.0) + FL(1.0) * dmp;
         const MYFLOAT dtil = FL(1.0) + FL(3.0) * dmp;
         const MYFLOAT rtLo = rt / dall;
         const MYFLOAT rtHi = rt / (dall * dtil);
         /* Settled lengths are PRIME INTEGERS, recomputed only when SIZE
            actually moves. Integer matters as much as prime: at a fractional
            distance the interpolated read is a lowpass, and a lowpass inside
            the loop is a per-pass HF loss - measured -1.9 dB per pass at
            10 kHz, which at a 40 ms round trip strips the top off the tail in
            a couple of seconds no matter what RT60 says. Landing exactly on
            an integer makes frac 0 and the read exact, so the loop is once
            again nothing but unit-magnitude elements. */
         const MYFLOAT szv = std::clamp(verbsize->load(), FL(0.0), FL(1.0));
         if (szv != _fdnSizeOld) {
             _fdnSizeOld = szv;
             const MYFLOAT baseMs = FDNMINMS * pow(FDNMAXMS / FDNMINMS, szv);
             for (int32_t u = 0; u < NFDN; u++) {
                 const MYFLOAT L = _STATE->sr * baseMs * FL(0.001) *
                     FDNRATIO[u] * _fdnDetune;
                 _fdnTgt[u] = (MYFLOAT)specdelPrimeAtMost((int32_t)std::clamp(L,
                     FL(8.0), (MYFLOAT)_fdnDl[u].size() - FL(4.0)));
             }
         }
         for (int32_t u = 0; u < NFDN; u++) {
             /* the allpasses sit inside the line, so their delay counts
                toward the round trip the gain is computed for */
             const MYFLOAT sec = (_fdnTgt[u] + (MYFLOAT)_fdnApLen[u][0] +
                 (MYFLOAT)_fdnApLen[u][1]) / _STATE->sr;
             MYFLOAT gLo = rtLo > FL(0.0)
                 ? pow(FL(10.0), FL(-3.0) * sec / rtLo) : FL(0.0);
             MYFLOAT gHi = rtHi > FL(0.0)
                 ? pow(FL(10.0), FL(-3.0) * sec / rtHi) : FL(0.0);
             gLo = std::min(gLo, FL(0.9995));
             gHi = std::min(gHi, gLo);
             /* One-pole y += a*(x-y), i.e. y[n] = a*x[n] + (1-a)*y[n-1], so
                H(1) = 1 (DC untouched, the low band keeps rtLo exactly) and
                we place |H| = r at a REFERENCE FREQUENCY. Solving
                r^2*|1 - p*e^-jw|^2 = (1-p)^2 for the pole p = 1-a gives
                p^2*A + p*B + A = 0 with A = r^2-1, B = 2 - 2*r^2*cos(w);
                the roots multiply to 1, so the '+' root is the stable one.
                REFERENCE MATTERS: the textbook a = 2r/(1+r) is this same
                solution at w = pi, which puts the designed tilt at NYQUIST,
                so the bend happens above the audible band - measured, full
                DAMP then delivered only 1.17x lo/hi. Referencing FDNDAMPREF
                instead brings it into the band (3.4x measured).
                NOT (1-r)/(1+r) - that is the pole, not the coefficient, and
                it lands on a = 0 at r = 1, a null filter rather than a
                passthrough, which kills the feedback outright at DAMP 0. */
             const MYFLOAT r = std::clamp(gLo > FL(1e-9) ? gHi / gLo : FL(1.0),
                 FL(0.02), FL(1.0));
             MYFLOAT aco = FL(1.0);
             if (r < FL(0.9999)) {
                 const MYFLOAT w = TWOPI_P *
                     std::min(FDNDAMPREF, FL(0.45) * _STATE->sr) / _STATE->sr;
                 const MYFLOAT rr = r * r;
                 const MYFLOAT A = rr - FL(1.0);
                 const MYFLOAT B = FL(2.0) - FL(2.0) * rr * cos(w);
                 const MYFLOAT D = std::max(B * B - FL(4.0) * A * A, FL(0.0));
                 const MYFLOAT p = std::clamp((-B + sqrt(D)) / (FL(2.0) * A),
                     FL(0.0), FL(0.999));
                 aco = FL(1.0) - p;
             }
             fdnG[u] = gLo;
             fdnA[u] = aco;
         }
         /* XFEED is a mixing ANGLE, not a lerp between identity and a mixing
            matrix: the mixer sits INSIDE the loop, so the decay only survives
            it if it is a contraction. A butterfly of Givens rotations at a
            common angle is exactly orthogonal at EVERY setting (identity at
            0, Hadamard-equivalent at 1), so XFEED cannot lengthen or shorten
            the tail. A row-normalised lerp reaches ||M||2 = 1.41 halfway and
            rings ~40% long on its common mode. */
         const MYFLOAT th = std::clamp(verbxfeed->load(), FL(0.0), FL(1.0)) *
             FL(PI_P) * FL(0.25);
         cth = cos(th);
         sth = sin(th);
     }
     if (loop != _loopOn) {
         /* fresh engage: the lines hold whatever the last engagement left.
            ~160 KB of fill, once, on an algorithm switch that is already
            reloading the whole convolver - cheaper than gating the reads,
            which would mute the first 180 ms and then let it all in at once. */
         for (int32_t u = 0; u < NFDN; u++) {
             std::fill(_fdnDl[u].begin(), _fdnDl[u].end(), FL(0.0));
             for (int32_t k = 0; k < NFDNAP; k++) {
                 std::fill(_fdnAp[u][k].begin(), _fdnAp[u][k].end(), FL(0.0));
                 _fdnApPos[u][k] = 0;
             }
             _fdnW[u] = 0;
             _fdnTap[u] = FL(0.0);
             _fdnLp[u] = FL(0.0);
             _fdnLen[u] = FL(-1.0);  /* snap to target, do not glide from 0 */
         }
         _loopOn = loop;
     }

     /* PUREVERB: wet feedback straight through the kernel, nothing else in
        the loop. Feedback stays OPEN until the setup thread has loaded a
        pole-bounded draw AND the morph front has fully swept it in: closing
        g ~ 0.94 around a leftover colour kernel (unit energy, +20 dB
        spectral ripple) is an over-unity loop. Once latched it stays closed
        - later sweeps are hybrids of walked draws, which are bounded by the
        walk's step size. */
     const bool pure = imode == SPECDELPUREVERB;
     MYFLOAT pvFbg = FL(0.0), pvHpa = FL(0.0);
     if (pure != _pureOn) {
         _pvFbPrev = FL(0.0);
         _pvHpLp = FL(0.0);
         _pvFbOn = false;
         _pureOn = pure;
         /* switched away: flush the diffuser chain with zero-input ticks so
            a later re-engage does not replay stale history. Each stage only
            needs its own length of zeros; the longest is n/4, margin on top. */
         if (!pure && _pvPreReady.load(std::memory_order_acquire))
             _pvDrain = (int64_t)_pvPreN.load(std::memory_order_relaxed) / 2 +
                 8192;
     }
     const bool pvChain = pure && _pvPreReady.load(std::memory_order_acquire);
     /* ER tap level - see PUREVERBER in Convolver.h. Zero below 1 s DELAY. */
     MYFLOAT pvErg = FL(0.0);
     if (pvChain) {
         const MYFLOAT tsec = std::max(delay * FL(0.001), FL(0.05));
         pvErg = PUREVERBER *
             sqrt(std::max(FL(0.0), FL(1.0) - FL(1.0) / tsec));
     }
     if (!pure && _pvDrain > 0 &&
         _pvPreReady.load(std::memory_order_acquire)) {
         const int64_t dn = std::min<int64_t>(_pvDrain, s);
         for (int64_t i = 0; i < dn; i++)
             for (int32_t k = 0; k < PVNPRE; k++)
                 (void)_pvPre[k]->tick(FL(0.0));
         _pvDrain -= dn;
     }
     if (pure) {
         if (!_pvFbOn && _pureKernelIn.load(std::memory_order_acquire) &&
             _state.load() == READY && convolverNonUniform->idle())
             _pvFbOn = true;
         if (_pvFbOn)
             pvFbg = _loopg.load();
         /* fixed ~35 Hz one-pole highpass in the loop: DC and subsonics are
            the slowest-decaying content and otherwise accumulate toward
            1/(1-g) */
         pvHpa = FL(1.0) - exp(-TWOPI_P * FL(35.0) / _STATE->sr);
     }

     MYFLOAT mix, gain = dbToLinear60(*_gain);
     const bool bypass = (_bypass->load() || destroyRequested);
     if (bypass) {
         mix = 0.;
     }
     else {
         mix = _mix->load();
     }
     const MYFLOAT mixsrc = 1. - mix;

     const bool env_on = fol->prepare(_chan);
     MYFLOAT* envbuf = nullptr;
     if (env_on) {
         auto src = fol->source.load();
         envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
     }

     for (int32_t i = 0; i < s; i++) {
         MYFLOAT mixfin = (env_on && !bypass) ? (_chan == 0 ? fol->detectL(envbuf[i]) : fol->detectR(envbuf[i])) : _smooth1;
         MYFLOAT mixsrcfin = 1. - mixfin;
         MYFLOAT u = in[i];
         MYFLOAT pvEr = FL(0.0);
         if (loop) {
             /* mix last sample's line outputs through the butterfly */
             MYFLOAT v[NFDN];
             for (int32_t q = 0; q < NFDN; q++)
                 v[q] = _fdnTap[q];
             for (int32_t bit = 1; bit < NFDN; bit <<= 1) {
                 for (int32_t p = 0; p < NFDN; p++) {
                     const int32_t q = p ^ bit;
                     if (q <= p)
                         continue;
                     const MYFLOAT a = v[p], b = v[q];
                     v[p] = cth * a - sth * b;
                     v[q] = sth * a + cth * b;
                 }
             }
             MYFLOAT sum = FL(0.0);
             for (int32_t q = 0; q < NFDN; q++) {
                 /* per-line damping: DC gain 1, HF gain r, scaled by the
                    line's RT60 gain */
                 _fdnLp[q] += fdnA[q] * (v[q] - _fdnLp[q]);
                 /* input injected with alternating signs; the output picks a
                    DIFFERENT sign pattern, and the two are orthogonal, so no
                    common mode survives straight through the network */
                 MYFLOAT x = fdnG[q] * _fdnLp[q] +
                     ((q & 1) ? -in[i] : in[i]);
                 /* NaN/runaway guard - drop the feedback, keep the input */
                 if (!(fabs(x) < FL(256.0))) {
                     x = FL(0.0);
                     _fdnLp[q] = FL(0.0);
                 }
                 /* nested Schroeder allpasses INSIDE the line: exactly unit
                    magnitude, so they multiply echo density without touching
                    the decay rate at any frequency */
                 for (int32_t k = 0; k < NFDNAP; k++) {
                     const int32_t L = _fdnApLen[q][k];
                     MYFLOAT& z = _fdnAp[q][k][(size_t)_fdnApPos[q][k]];
                     const MYFLOAT vv = x + FL(0.65) * z;
                     const MYFLOAT y = z - FL(0.65) * vv;
                     z = vv;
                     if (++_fdnApPos[q][k] >= L)
                         _fdnApPos[q][k] = 0;
                     x = y;
                 }
                 /* the pure delay line, read FRACTIONALLY at a rate-limited
                    distance: SIZE then bends the pitch slightly on its way to
                    the new room instead of jumping the read pointer into
                    content that no longer lines up (which is heard as a burst
                    of noise on the knob) */
                 const int32_t sz = (int32_t)_fdnDl[q].size();
                 const MYFLOAT dtg = _fdnTgt[q] - _fdnLen[q];
                 if (_fdnLen[q] < FL(0.0) || fabs(dtg) < FL(0.01))
                     _fdnLen[q] = _fdnTgt[q];  /* SNAP: frac must reach 0 */
                 else
                     _fdnLen[q] += std::clamp(dtg * FL(0.0002),
                         FL(-0.05), FL(0.05));
                 _fdnDl[q][(size_t)_fdnW[q]] = x;
                 if (++_fdnW[q] >= sz)
                     _fdnW[q] = 0;
                 MYFLOAT rp = (MYFLOAT)_fdnW[q] - _fdnLen[q];
                 if (rp < FL(0.0))
                     rp += (MYFLOAT)sz;
                 int32_t r0 = (int32_t)rp;
                 const MYFLOAT frac = rp - (MYFLOAT)r0;
                 if (r0 >= sz)
                     r0 -= sz;
                 int32_t r1 = r0 + 1;
                 if (r1 >= sz)
                     r1 = 0;
                 const MYFLOAT y = _fdnDl[q][r0] +
                     frac * (_fdnDl[q][r1] - _fdnDl[q][r0]);
                 _fdnTap[q] = y;
                 sum += ((q & 2) ? -y : y);
             }
             u = sum * (FL(1.0) / sqrt((MYFLOAT)NFDN));
         }
         if (pvChain) {
             /* input diffuser chain, shortest stage first, OUTPUTS TAPPED
                AND SUMMED - pre-smears the hit BEFORE the feedback joins.
                The serial-only version fed the main kernel nothing but the
                full smear, which traded the attack away entirely (0% of
                energy in the first 20 ms at DELAY 10 s). Summing the taps
                gives one arrival per smear length: the first tap keeps a
                near-sharp attack at -12 dB, each later tap doubles the
                smear twice over, and the last carries the wash - the
                early-reflections-into-reverb envelope, dense from the
                first milliseconds. The echo-generation risk does not come
                back at full strength: the sharp tap holds 1/4 of the
                energy, not all of it. Taps are convolutions with
                INDEPENDENT draws, hence decorrelated: 1/sqrt(4) keeps the
                summed energy at unity. */
             MYFLOAT tsum = FL(0.0);
             for (int32_t k = 0; k < PVNPRE; k++) {
                 u = _pvPre[k]->tick(u);
                 tsum += u;
             }
             u = tsum * FL(0.5);
             pvEr = u;   /* the ER tap - into the OUTPUT only, never the loop */
         }
         if (pure) {
             /* last sample's convolver output IS the feedback - the noise
                IR is the loop, nothing else is in the path but the HP */
             const MYFLOAT d = _pvFbPrev;
             _pvHpLp += pvHpa * (d - _pvHpLp);
             MYFLOAT fbs = pvFbg * (d - _pvHpLp);
             /* NaN/runaway guard - drop the feedback, keep the input */
             if (!(fabs(fbs) < FL(256.0))) {
                 fbs = FL(0.0);
                 _pvHpLp = FL(0.0);
                 _pvFbPrev = FL(0.0);
             }
             u += fbs;
         }
         const MYFLOAT wet = convolverNonUniform->tick(u);
         if (pure)
             _pvFbPrev = wet;   /* the loop taps the main convolver ONLY */
         in[i] = in[i] * mixsrcfin +
             (wet + pvErg * pvEr) * mixfin * _smooth2;
         sm1(mix);
         sm2(gain);
     }

 }


