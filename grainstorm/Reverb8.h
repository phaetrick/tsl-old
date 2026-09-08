//
// Created by pr on 19.07.25.
//

#ifndef GRAINSTORM_REVERB8_H
#define GRAINSTORM_REVERB8_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <complex>

// Advanced interpolation for fractional delays
class InterpolatedBuffer {
private:
    std::vector<float> buffer;
    float writePos;
    int size;

public:
    explicit InterpolatedBuffer(int bufferSize) : size(bufferSize), writePos(0.0f) {
        buffer.resize(bufferSize, 0.0f);
    }

    void write(float sample) {
        int idx = (int)writePos;
        buffer[idx] = sample;
        writePos = fmodf(writePos + 1.0f, size);
    }

    float readHermite(float delayTime) {
        float readPos = writePos - delayTime;
        if (readPos < 0) readPos += size;

        int idx = (int)readPos;
        float frac = readPos - idx;

        int idx0 = (idx - 1 + size) % size;
        int idx1 = idx;
        int idx2 = (idx + 1) % size;
        int idx3 = (idx + 2) % size;

        float y0 = buffer[idx0];
        float y1 = buffer[idx1];
        float y2 = buffer[idx2];
        float y3 = buffer[idx3];

        // 4-point Hermite interpolation
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
};

// Multi-tap modulated all-pass filter
class ModulatedAllPass {
private:
    InterpolatedBuffer buffer;
    float feedback;
    float baseDelay;
    float modDepth;
    float modRate;
    float modPhase;
    float sampleRate;

public:
    ModulatedAllPass(int maxDelay, float fb, float sr)
            : buffer(maxDelay), feedback(fb), sampleRate(sr), modPhase(0.0f) {
        baseDelay = maxDelay * 0.7f;
        modDepth = maxDelay * 0.1f;
        modRate = 0.37f;
    }

    float process(float input) {
        float lfo = sinf(modPhase) * modDepth;
        float currentDelay = baseDelay + lfo;

        float delayed = buffer.readHermite(currentDelay);
        float output = -feedback * input + delayed;

        buffer.write(input + feedback * delayed);

        modPhase += (2.0f * M_PI * modRate) / sampleRate;
        if (modPhase >= 2.0f * M_PI) modPhase -= 2.0f * M_PI;

        return output;
    }

    void setModulation(float rate, float depth) {
        modRate = rate;
        modDepth = depth;
    }
};

// State Variable Filter for advanced filtering
class StateVariableFilter {
private:
    float frequency{};
    float resonance{};
    float f{}, q{};
    float low, high, band, notch;
    float sampleRate;

public:
    StateVariableFilter(float sr) : sampleRate(sr), low(0), high(0), band(0), notch(0) {
        setParameters(1000.0f, 0.7f);
    }

    void setParameters(float freq, float res) {
        frequency = std::clamp(freq, 20.0f, sampleRate * 0.45f);
        resonance = std::clamp(res, 0.1f, 20.0f);
        f = 2.0f * sinf(M_PI * frequency / sampleRate);
        q = 1.0f / resonance;
    }

    void process(float input) {
        low += f * band;
        high = input - low - q * band;
        band += f * high;
        notch = high + low;
    }

    float getLowpass() { return low; }
    float getHighpass() { return high; }
    float getBandpass() { return band; }
    float getNotch() { return notch; }
};

// Advanced diffusion network
class DiffusionNetwork {
private:
    static const int STAGES = 12;
    ModulatedAllPass* stages[STAGES]{};
    StateVariableFilter* filters[STAGES/3]{};
    float crossfeed[STAGES]{};

public:
    explicit DiffusionNetwork(float sampleRate) {
        // Prime-based delay lengths for maximum decorrelation
        int delays[STAGES] = {142, 179, 227, 281, 337, 401, 467, 541, 613, 691, 773, 857};
        float feedbacks[STAGES] = {0.625f, 0.7f, 0.65f, 0.72f, 0.68f, 0.74f,
                                   0.66f, 0.71f, 0.69f, 0.73f, 0.67f, 0.75f};

        for (int i = 0; i < STAGES; i++) {
            stages[i] = new ModulatedAllPass(delays[i], feedbacks[i], sampleRate);
            crossfeed[i] = (i % 2 == 0) ? 0.3f : -0.3f;

            // Set different modulation rates for each stage
            stages[i]->setModulation(0.17f + i * 0.03f, delays[i] * 0.05f);
        }

        // Filters for tone shaping
        for (int i = 0; i < STAGES/3; i++) {
            filters[i] = new StateVariableFilter(sampleRate);
            filters[i]->setParameters(800.0f + i * 400.0f, 0.7f);
        }
    }

    ~DiffusionNetwork() {
        for (int i = 0; i < STAGES; i++) {
            delete stages[i];
        }
        for (int i = 0; i < STAGES/3; i++) {
            delete filters[i];
        }
    }

    float process(float input, float diffusion) {
        float signal = input;

        for (int i = 0; i < STAGES; i++) {
            // Cross-feed between stages for complexity
            if (i > 0) {
                signal += stages[i-1]->process(0) * crossfeed[i] * diffusion;
            }

            signal = stages[i]->process(signal * diffusion);

            // Apply filtering every 3 stages
            if (i % 3 == 2) {
                filters[i/3]->process(signal);
                signal = filters[i/3]->getLowpass() * 0.7f +
                         filters[i/3]->getBandpass() * 0.3f;
            }
        }

        return signal;
    }
};

// Spectral processing using overlap-add FFT
class SpectralReverb {
private:
    static const int FFT_SIZE = 2048;
    static const int OVERLAP = FFT_SIZE / 4;

    std::vector<std::complex<float>> fftBuffer;
    std::vector<float> window;
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::vector<std::complex<float>> impulseResponse;
    int bufferPos;

    void generateWindow() {
        window.resize(FFT_SIZE);
        for (int i = 0; i < FFT_SIZE; i++) {
            window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
        }
    }

    void fft(std::vector<std::complex<float>>& data, bool inverse = false) {
        int N = data.size();
        if (N <= 1) return;

        // Bit-reverse permutation
        for (int i = 0, j = 0; i < N; i++) {
            if (i < j) std::swap(data[i], data[j]);
            int k = N >> 1;
            while (j & k) {
                j ^= k;
                k >>= 1;
            }
            j ^= k;
        }

        // FFT computation
        for (int len = 2; len <= N; len <<= 1) {
            float ang = (inverse ? 1 : -1) * 2.0f * M_PI / len;
            std::complex<float> wlen(cosf(ang), sinf(ang));

            for (int i = 0; i < N; i += len) {
                std::complex<float> w(1);
                for (int j = 0; j < len / 2; j++) {
                    std::complex<float> u = data[i + j];
                    std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse) {
            for (auto& x : data) x /= N;
        }
    }

public:
    SpectralReverb() : bufferPos(0) {
        fftBuffer.resize(FFT_SIZE);
        inputBuffer.resize(FFT_SIZE, 0.0f);
        outputBuffer.resize(FFT_SIZE, 0.0f);
        impulseResponse.resize(FFT_SIZE);
        generateWindow();

        // Create spectral impulse response (synthetic room)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float magnitude = expf(-i * 0.001f) * (0.5f + 0.5f * dist(gen));
            float phase = dist(gen) * M_PI;
            impulseResponse[i] = std::polar(magnitude, phase);
            impulseResponse[FFT_SIZE - 1 - i] = std::conj(impulseResponse[i]);
        }
    }

    float process(float input) {
        inputBuffer[bufferPos] = input * window[bufferPos];
        float output = outputBuffer[bufferPos];

        bufferPos++;
        if (bufferPos >= FFT_SIZE) {
            bufferPos = 0;

            // Copy to FFT buffer
            for (int i = 0; i < FFT_SIZE; i++) {
                fftBuffer[i] = inputBuffer[i];
            }

            // Forward FFT
            fft(fftBuffer);

            // Convolution in frequency domain
            for (int i = 0; i < FFT_SIZE; i++) {
                fftBuffer[i] *= impulseResponse[i];
            }

            // Inverse FFT
            fft(fftBuffer, true);

            // Overlap-add
            for (int i = 0; i < FFT_SIZE; i++) {
                outputBuffer[i] = fftBuffer[i].real() * window[i];
            }

            // Shift input buffer for overlap
            std::copy(inputBuffer.begin() + OVERLAP, inputBuffer.end(),
                      inputBuffer.begin());
            std::fill(inputBuffer.begin() + FFT_SIZE - OVERLAP,
                      inputBuffer.end(), 0.0f);
        }

        return output;
    }
};
// R2C FFT class interface
class FFT10 {
private:
    int size;
    int complexSize;

public:
    FFT10(int fftSize) : size(fftSize), complexSize(fftSize / 2 + 1) {}

    void forward(std::vector<float>& realData, std::vector<std::complex<float>>& complexData) {
        // Real-to-complex FFT
        complexData.resize(complexSize);

        // Pack real data into complex buffer (first half)
        std::vector<std::complex<float>> temp(size);
        for (int i = 0; i < size; i++) {
            temp[i] = std::complex<float>(realData[i], 0.0f);
        }

        // Perform full complex FFT
        fft(temp, false);

        // Extract positive frequencies only (R2C result)
        for (int i = 0; i < complexSize; i++) {
            complexData[i] = temp[i];
        }
    }

    void backward(std::vector<std::complex<float>>& complexData, std::vector<float>& realData) {
        // Complex-to-real IFFT
        realData.resize(size);

        // Reconstruct full complex spectrum (hermitian symmetry)
        std::vector<std::complex<float>> temp(size);
        temp[0] = complexData[0];  // DC component

        for (int i = 1; i < complexSize - 1; i++) {
            temp[i] = complexData[i];
            temp[size - i] = std::conj(complexData[i]);  // Hermitian symmetry
        }

        if (size % 2 == 0) {
            temp[size / 2] = complexData[complexSize - 1];  // Nyquist frequency
        }

        // Perform inverse FFT
        fft(temp, true);

        // Extract real part
        for (int i = 0; i < size; i++) {
            realData[i] = temp[i].real();
        }
    }

private:
    void fft(std::vector<std::complex<float>>& data, bool inverse = false) {
        int N = data.size();
        if (N <= 1) return;

        // Bit-reverse permutation
        for (int i = 0, j = 0; i < N; i++) {
            if (i < j) std::swap(data[i], data[j]);
            int k = N >> 1;
            while (j & k) {
                j ^= k;
                k >>= 1;
            }
            j ^= k;
        }

        // FFT computation
        for (int len = 2; len <= N; len <<= 1) {
            float ang = (inverse ? 1 : -1) * 2.0f * M_PI / len;
            std::complex<float> wlen(cosf(ang), sinf(ang));

            for (int i = 0; i < N; i += len) {
                std::complex<float> w(1);
                for (int j = 0; j < len / 2; j++) {
                    std::complex<float> u = data[i + j];
                    std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse) {
            for (auto& x : data) x /= N;
        }
    }
};

// Spectral processing using overlap-add FFT
class SpectralReverbR2C {
private:
    static const int FFT_SIZE = 2048;
    static const int OVERLAP = FFT_SIZE / 4;

    FFT10* fftProcessor;
    std::vector<float> window;
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::vector<std::complex<float>> frequencyBuffer;
    std::vector<std::complex<float>> impulseResponse;
    int bufferPos;

    void generateWindow() {
        window.resize(FFT_SIZE);
        for (int i = 0; i < FFT_SIZE; i++) {
            window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
        }
    }

public:
    SpectralReverbR2C() : bufferPos(0) {
        fftProcessor = new FFT10(FFT_SIZE);
        frequencyBuffer.resize(FFT_SIZE / 2 + 1);
        inputBuffer.resize(FFT_SIZE, 0.0f);
        outputBuffer.resize(FFT_SIZE, 0.0f);
        impulseResponse.resize(FFT_SIZE / 2 + 1);
        generateWindow();

        // Create spectral impulse response (synthetic room) - R2C format
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < FFT_SIZE / 2 + 1; i++) {
            float magnitude = expf(-i * 0.001f) * (0.5f + 0.5f * dist(gen));
            float phase = dist(gen) * M_PI;
            impulseResponse[i] = std::polar(magnitude, phase);
        }

        // Ensure DC and Nyquist are real
        impulseResponse[0] = std::complex<float>(std::abs(impulseResponse[0]), 0);
        if (FFT_SIZE % 2 == 0) {
            impulseResponse[FFT_SIZE / 2] = std::complex<float>(std::abs(impulseResponse[FFT_SIZE / 2]), 0);
        }
    }

    ~SpectralReverbR2C() {
        delete fftProcessor;
    }

    float process(float input) {
        inputBuffer[bufferPos] = input * window[bufferPos];
        float output = outputBuffer[bufferPos];

        bufferPos++;
        if (bufferPos >= FFT_SIZE) {
            bufferPos = 0;

            // Forward R2C FFT
            fftProcessor->forward(inputBuffer, frequencyBuffer);

            // Convolution in frequency domain
            for (int i = 0; i < frequencyBuffer.size(); i++) {
                frequencyBuffer[i] *= impulseResponse[i];
            }

            // Inverse C2R FFT
            fftProcessor->backward(frequencyBuffer, outputBuffer);

            // Apply window and prepare for overlap-add
            for (int i = 0; i < FFT_SIZE; i++) {
                outputBuffer[i] *= window[i];
            }

            // Shift input buffer for overlap
            std::copy(inputBuffer.begin() + OVERLAP, inputBuffer.end(),
                      inputBuffer.begin());
            std::fill(inputBuffer.begin() + FFT_SIZE - OVERLAP,
                      inputBuffer.end(), 0.0f);
        }

        return output;
    }
};

// Main advanced reverb engine
class AdvancedReverbEngine {
private:
    DiffusionNetwork* earlyReflections;
    DiffusionNetwork* lateReverb;
    SpectralReverbR2C* spectralProcessor;
    StateVariableFilter* inputFilter;
    StateVariableFilter* outputFilter;

    // Multi-band processing
    StateVariableFilter* multibandFilters[3]{};
    DiffusionNetwork* multibandProcessors[3]{};

    float earlyLevel;
    float lateLevel;
    float spectralLevel;
    float diffusion;
    float dampening;
    float roomSize;
    float predelay;
    float width;

    std::vector<float> predelayBuffer;
    int predelayPos;

public:
    explicit AdvancedReverbEngine(float sampleRate) : predelayPos(0) {
        earlyReflections = new DiffusionNetwork(sampleRate);
        lateReverb = new DiffusionNetwork(sampleRate);
        spectralProcessor = new SpectralReverbR2C();
        inputFilter = new StateVariableFilter(sampleRate);
        outputFilter = new StateVariableFilter(sampleRate);

        // Multi-band setup
        for (int i = 0; i < 3; i++) {
            multibandFilters[i] = new StateVariableFilter(sampleRate);
            multibandProcessors[i] = new DiffusionNetwork(sampleRate);
        }

        // Filter frequencies for 3-band split
        multibandFilters[0]->setParameters(300.0f, 0.7f);  // Low
        multibandFilters[1]->setParameters(2000.0f, 0.7f); // Mid
        multibandFilters[2]->setParameters(8000.0f, 0.7f); // High

        predelayBuffer.resize((int)(sampleRate * 0.3f), 0.0f); // 300ms max predelay

        // Default parameters
        earlyLevel = 0.3f;
        lateLevel = 0.4f;
        spectralLevel = 0.2f;
        diffusion = 0.7f;
        dampening = 0.3f;
        roomSize = 0.8f;
        predelay = 0.03f * sampleRate;
        width = 0.8f;
    }

    ~AdvancedReverbEngine() {
        delete earlyReflections;
        delete lateReverb;
        delete spectralProcessor;
        delete inputFilter;
        delete outputFilter;
        for (int i = 0; i < 3; i++) {
            delete multibandFilters[i];
            delete multibandProcessors[i];
        }
    }

    float process(float input) {
        // Input filtering and conditioning
        inputFilter->setParameters(50.0f + dampening * 5000.0f, 0.7f);
        inputFilter->process(input);
        float filtered = inputFilter->getHighpass() * 0.8f +
                         inputFilter->getBandpass() * 0.2f;
        // Predelay
        float delayed = predelayBuffer[predelayPos];
        predelayBuffer[predelayPos] = filtered;
        predelayPos = (predelayPos + 1) % predelayBuffer.size();
        // Multi-band processing
        float bandOutputs[3];
        for (int i = 0; i < 3; i++) {
            multibandFilters[i]->process(delayed);
            float bandSignal;
            switch(i) {
                case 0: bandSignal = multibandFilters[i]->getLowpass(); break;
                case 1: bandSignal = multibandFilters[i]->getBandpass(); break;
                case 2: bandSignal = multibandFilters[i]->getHighpass(); break;
            }
            bandOutputs[i] = multibandProcessors[i]->process(bandSignal, diffusion);
        }

        // Combine bands with frequency-dependent processing
        float multibandOutput = bandOutputs[0] * 0.4f +
                                bandOutputs[1] * 0.4f +
                                bandOutputs[2] * 0.2f * (1.0f - dampening);
        float late = lateReverb->process(multibandOutput, diffusion);

        return late;
        float early = earlyReflections->process(delayed, diffusion * 0.8f);

        // Main reverb processing
        float spectral = spectralProcessor->process(delayed * 0.5f);
        // Mix reverb components
        float reverbSum = early * earlyLevel +
                          late * lateLevel +
                          spectral * spectralLevel;
        // Output filtering
        outputFilter->setParameters(20000.0f * (1.0f - dampening * 0.8f), 1.5f);
        outputFilter->process(reverbSum);
        float finalReverb = outputFilter->getLowpass();

        return finalReverb;
    }

    // Parameter setters
    void setRoomSize(float size) { roomSize = std::clamp(size, 0.1f, 1.0f); }
    void setDiffusion(float diff) { diffusion = std::clamp(diff, 0.0f, 1.0f); }
    void setDampening(float damp) { dampening = std::clamp(damp, 0.0f, 1.0f); }
    void setEarlyLevel(float level) { earlyLevel = std::clamp(level, 0.0f, 1.0f); }
    void setLateLevel(float level) { lateLevel = std::clamp(level, 0.0f, 1.0f); }
    void setSpectralLevel(float level) { spectralLevel = std::clamp(level, 0.0f, 1.0f); }
    void setPredelay(float delay) {
        predelay = std::clamp(delay, 0.0f, (float)predelayBuffer.size());
    }
    void setWidth(float w) { width = std::clamp(w, 0.0f, 2.0f); }
};




// Stereo version with advanced spatial processing
class AdvancedStereoReverb {
private:
    AdvancedReverbEngine* leftEngine;
    AdvancedReverbEngine* rightEngine;

    // Stereo enhancement
    float crossMix;
    float decorrelation;

    // Modulation matrices
    float modulationPhase[4]{};
    float sampleRate;

public:
    AdvancedStereoReverb(float sr) : sampleRate(sr), crossMix(0.3f), decorrelation(0.6f) {
        leftEngine = new AdvancedReverbEngine(sr);
        rightEngine = new AdvancedReverbEngine(sr);

        // Slight parameter differences for stereo decorrelation
        rightEngine->setRoomSize(0.82f);
        rightEngine->setDiffusion(0.73f);

        for (int i = 0; i < 4; i++) {
            modulationPhase[i] = i * M_PI * 0.5f;
        }
    }
    ~AdvancedStereoReverb() {
        delete leftEngine;
        delete rightEngine;
    }

    void process(float inputL, float inputR, float& outputL, float& outputR,
                 float wetLevel, float dryLevel) {

        // Advanced stereo matrix processing
        float matrixL = inputL + inputR * crossMix;
        float matrixR = inputR + inputL * crossMix;

        // Process through engines
        float reverbL = leftEngine->process(matrixL);
        float reverbR = rightEngine->process(matrixR);

        // Decorrelation with modulated cross-feed
        float mod1 = sinf(modulationPhase[0]) * decorrelation;
        float mod2 = sinf(modulationPhase[1]) * decorrelation;

        float decorrelatedL = reverbL + reverbR * mod1;
        float decorrelatedR = reverbR + reverbL * mod2;

        // Update modulation
        for (int i = 0; i < 4; i++) {
            modulationPhase[i] += (2.0f * M_PI * (0.13f + i * 0.07f)) / sampleRate;
            if (modulationPhase[i] >= 2.0f * M_PI)
                modulationPhase[i] -= 2.0f * M_PI;
        }

        // Final mix
        outputL = inputL * dryLevel + decorrelatedL * wetLevel;
        outputR = inputR * dryLevel + decorrelatedR * wetLevel;
    }

    // Unified parameter control
    void setParameters(float roomSize, float diffusion, float dampening,
                       float early, float late, float spectral, float predelay) {
        leftEngine->setRoomSize(roomSize);
        leftEngine->setDiffusion(diffusion);
        leftEngine->setDampening(dampening);
        leftEngine->setEarlyLevel(early);
        leftEngine->setLateLevel(late);
        leftEngine->setSpectralLevel(spectral);
        leftEngine->setPredelay(predelay);

        // Slightly different for right channel
        rightEngine->setRoomSize(roomSize * 1.03f);
        rightEngine->setDiffusion(diffusion * 1.02f);
        rightEngine->setDampening(dampening);
        rightEngine->setEarlyLevel(early * 0.98f);
        rightEngine->setLateLevel(late * 1.02f);
        rightEngine->setSpectralLevel(spectral);
        rightEngine->setPredelay(predelay * 1.05f);
    }

    void setStereoWidth(float width) {
        crossMix = width * 0.5f;
        decorrelation = width * 0.8f;
    }
};

// Usage example
#include <iostream>




#include <vector>
#include <complex>
#include <cmath>
#include <random>
#include <algorithm>

class ImpulseResponseGenerator {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<float> uniform;
    std::normal_distribution<float> normal;

public:
    ImpulseResponseGenerator() : uniform(0.0f, 1.0f), normal(0.0f, 1.0f) {
        std::random_device rd;
        rng.seed(rd());
    }

    // 1. PHYSICAL ROOM MODELING
    std::vector<float> generateRoomImpulse(float roomLength, float roomWidth, float roomHeight,
                                           float absorptionCoeff, float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> impulse(samples, 0.0f);

        // Speed of sound
        float c = 343.0f; // m/s

        // Source and listener positions (normalized 0-1)
        float srcX = 0.3f, srcY = 0.4f, srcZ = 0.5f;
        float lisX = 0.7f, lisY = 0.6f, lisZ = 0.5f;

        // Generate early reflections using image source method
        for (int nx = -3; nx <= 3; nx++) {
            for (int ny = -3; ny <= 3; ny++) {
                for (int nz = -2; nz <= 2; nz++) {
                    // Image source position
                    float imgX = (nx % 2 == 0) ? srcX + nx * roomLength : (nx + 1) * roomLength - srcX;
                    float imgY = (ny % 2 == 0) ? srcY + ny * roomWidth : (ny + 1) * roomWidth - srcY;
                    float imgZ = (nz % 2 == 0) ? srcZ + nz * roomHeight : (nz + 1) * roomHeight - srcZ;

                    // Distance to image source
                    float dx = imgX - lisX;
                    float dy = imgY - lisY;
                    float dz = imgZ - lisZ;
                    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

                    // Time delay
                    float delay = distance / c;
                    int delaySamples = (int)(delay * sampleRate);

                    if (delaySamples < samples) {
                        // Amplitude with absorption
                        int reflections = abs(nx) + abs(ny) + abs(nz);
                        float amplitude = powf(1.0f - absorptionCoeff, reflections) / distance;

                        // Add to impulse with some spreading
                        for (int spread = 0; spread < 3; spread++) {
                            int idx = delaySamples + spread;
                            if (idx < samples) {
                                impulse[idx] += amplitude * expf(-spread * 0.5f);
                            }
                        }
                    }
                }
            }
        }

        return impulse;
    }

    // 2. EXPONENTIAL DECAY ENVELOPE with MODAL RESONANCES
    std::vector<float> generateModalDecay(float rt60, float sampleRate, float duration, int numModes = 50) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> impulse(samples, 0.0f);

        // Generate modal frequencies and decay rates
        for (int mode = 0; mode < numModes; mode++) {
            // Modal frequency (roughly based on room modes)
            float frequency = 50.0f + mode * (8000.0f / numModes) + normal(rng) * 20.0f;

            // Frequency-dependent decay time (high frequencies decay faster)
            float modeRT60 = rt60 * expf(-frequency / 4000.0f);
            float decayRate = -6.91f / modeRT60; // -60dB decay rate

            // Modal amplitude (frequency-dependent)
            float amplitude = 1.0f / sqrtf(frequency / 100.0f) * (0.5f + 0.5f * uniform(rng));

            // Phase randomization
            float phase = uniform(rng) * 2.0f * M_PI;

            // Generate modal response
            for (int n = 0; n < samples; n++) {
                float t = n / sampleRate;
                float envelope = amplitude * expf(decayRate * t);
                float oscillation = sinf(2.0f * M_PI * frequency * t + phase);
                impulse[n] += envelope * oscillation;
            }
        }

        return impulse;
    }

    // 3. STATISTICAL SCATTERING MODEL
    std::vector<float> generateScatteringImpulse(float scatteringDensity, float decayTime,
                                                 float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> impulse(samples, 0.0f);

        // Scattering event rate (events per second)
        float eventRate = scatteringDensity * sampleRate;

        // Generate scattering events
        float currentTime = 0.0f;
        while (currentTime < duration) {
            // Inter-arrival time (exponential distribution)
            float interArrival = -logf(uniform(rng)) / eventRate;
            currentTime += interArrival;

            int sampleIdx = (int)(currentTime * sampleRate);
            if (sampleIdx >= samples) break;

            // Scattering amplitude with decay
            float amplitude = expf(-currentTime / decayTime);

            // Random amplitude variation
            amplitude *= (0.1f + 0.9f * uniform(rng));

            // Random polarity
            if (uniform(rng) < 0.5f) amplitude = -amplitude;

            impulse[sampleIdx] += amplitude;

            // Increase event rate over time (density increases)
            eventRate *= 1.001f;
        }

        // Apply smoothing to avoid clicks
        for (int i = 1; i < samples - 1; i++) {
            impulse[i] = 0.25f * impulse[i-1] + 0.5f * impulse[i] + 0.25f * impulse[i+1];
        }

        return impulse;
    }

    // 4. FEEDBACK DELAY NETWORK SIMULATION
    std::vector<float> generateFDNImpulse(const std::vector<int>& delayLengths,
                                          const std::vector<float>& gains,
                                          float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        int numDelays = delayLengths.size();

        // Initialize delay lines
        std::vector<std::vector<float>> delayLines(numDelays);
        std::vector<int> readPos(numDelays, 0);

        for (int i = 0; i < numDelays; i++) {
            delayLines[i].resize(delayLengths[i], 0.0f);
        }

        std::vector<float> impulse(samples, 0.0f);

        // Feedback matrix (Householder matrix for energy preservation)
        std::vector<std::vector<float>> feedbackMatrix(numDelays, std::vector<float>(numDelays));
        generateHouseholderMatrix(feedbackMatrix, numDelays);

        // Process impulse response
        for (int n = 0; n < samples; n++) {
            float input = (n == 0) ? 1.0f : 0.0f; // Delta function input

            // Read from delay lines
            std::vector<float> delayOutputs(numDelays);
            for (int i = 0; i < numDelays; i++) {
                delayOutputs[i] = delayLines[i][readPos[i]];
            }

            // Apply feedback matrix
            std::vector<float> fedbackSignals(numDelays, 0.0f);
            for (int i = 0; i < numDelays; i++) {
                for (int j = 0; j < numDelays; j++) {
                    fedbackSignals[i] += feedbackMatrix[i][j] * delayOutputs[j];
                }
            }

            // Write to delay lines with input injection
            for (int i = 0; i < numDelays; i++) {
                delayLines[i][readPos[i]] = input * (i == 0 ? 1.0f : 0.0f) +
                                            fedbackSignals[i] * gains[i];
                readPos[i] = (readPos[i] + 1) % delayLengths[i];
            }

            // Sum outputs for impulse response
            float output = 0.0f;
            for (int i = 0; i < numDelays; i++) {
                output += delayOutputs[i] * 0.25f; // Output mixing
            }

            impulse[n] = output;
        }

        return impulse;
    }

    // 5. FREQUENCY-DOMAIN SHAPING
    std::vector<std::complex<float>> generateFrequencyShapedImpulse(int fftSize, float sampleRate) {
        int complexSize = fftSize / 2 + 1;
        std::vector<std::complex<float>> frequencyResponse(complexSize);

        for (int i = 0; i < complexSize; i++) {
            float frequency = (float)i * sampleRate / fftSize;

            // Create magnitude response curve
            float magnitude = 1.0f;

            // Low-frequency boost (warmth)
            if (frequency < 200.0f) {
                magnitude *= 1.0f + 0.3f * expf(-(frequency - 60.0f) * (frequency - 60.0f) / (2.0f * 40.0f * 40.0f));
            }

            // Mid-frequency shaping (presence)
            if (frequency > 1000.0f && frequency < 5000.0f) {
                magnitude *= 1.0f + 0.2f * expf(-(frequency - 2500.0f) * (frequency - 2500.0f) / (2.0f * 800.0f * 800.0f));
            }

            // High-frequency roll-off (air absorption)
            if (frequency > 3000.0f) {
                magnitude *= expf(-frequency / 8000.0f);
            }

            // Add random variations
            magnitude *= (0.8f + 0.4f * uniform(rng));

            // Random phase for diffusion
            float phase = uniform(rng) * 2.0f * M_PI;

            frequencyResponse[i] = std::polar(magnitude, phase);
        }

        // Ensure DC and Nyquist are real
        frequencyResponse[0] = std::complex<float>(std::abs(frequencyResponse[0]), 0);
        if (fftSize % 2 == 0) {
            frequencyResponse[complexSize - 1] = std::complex<float>(std::abs(frequencyResponse[complexSize - 1]), 0);
        }

        return frequencyResponse;
    }

    // 6. GRANULAR TEXTURE SYNTHESIS
    std::vector<float> generateGranularTexture(float grainDensity, float grainSize,
                                               float pitchVariation, float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> impulse(samples, 0.0f);

        int grainSamples = (int)(grainSize * sampleRate);
        float grainRate = grainDensity * sampleRate;

        // Generate grain envelope (Hann window)
        std::vector<float> grainEnvelope(grainSamples);
        for (int i = 0; i < grainSamples; i++) {
            grainEnvelope[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (grainSamples - 1)));
        }

        // Place grains randomly
        float currentTime = 0.0f;
        while (currentTime < duration) {
            // Inter-grain time
            float interGrain = -logf(uniform(rng)) / grainRate;
            currentTime += interGrain;

            int startSample = (int)(currentTime * sampleRate);
            if (startSample + grainSamples >= samples) break;

            // Random grain frequency
            float baseFreq = 200.0f + uniform(rng) * 2000.0f;
            float pitchMult = powf(2.0f, (uniform(rng) - 0.5f) * pitchVariation);
            float grainFreq = baseFreq * pitchMult;

            // Grain amplitude with decay over time
            float amplitude = expf(-currentTime / (duration * 0.5f)) * (0.2f + 0.8f * uniform(rng));

            // Generate grain
            for (int i = 0; i < grainSamples && (startSample + i) < samples; i++) {
                float phase = 2.0f * M_PI * grainFreq * i / sampleRate;
                float grain = amplitude * grainEnvelope[i] * sinf(phase);
                impulse[startSample + i] += grain;
            }
        }

        return impulse;
    }

    // 7. WAVEGUIDE NETWORK SIMULATION
    std::vector<float> generateWaveguideNetwork(float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> impulse(samples, 0.0f);

        // Create a network of connected waveguides
        struct WaveguideSegment {
            std::vector<float> rightGoing, leftGoing;
            int length;
            float loss;

            WaveguideSegment(int len, float l) : length(len), loss(l) {
                rightGoing.resize(len, 0.0f);
                leftGoing.resize(len, 0.0f);
            }
        };

        // Create network segments
        std::vector<WaveguideSegment> segments = {
                WaveguideSegment(100, 0.995f),  // Main resonator
                WaveguideSegment(150, 0.990f),  // Secondary
                WaveguideSegment(75, 0.985f),   // Tertiary
                WaveguideSegment(200, 0.992f)   // Long tail
        };

        // Junction coefficients
        std::vector<float> junctionCoeffs = {0.3f, -0.2f, 0.4f, -0.1f};

        // Simulate waveguide network
        for (int n = 0; n < samples; n++) {
            // Input impulse at start
            if (n == 0) segments[0].rightGoing[0] = 1.0f;

            // Update each segment
            for (auto& segment : segments) {
                // Shift waves
                for (int i = segment.length - 1; i > 0; i--) {
                    segment.rightGoing[i] = segment.rightGoing[i-1] * segment.loss;
                    segment.leftGoing[i-1] = segment.leftGoing[i] * segment.loss;
                }

                // Boundary reflections
                segment.leftGoing[segment.length-1] = -segment.rightGoing[segment.length-1] * 0.9f;
                segment.rightGoing[0] = -segment.leftGoing[0] * 0.8f;
            }

            // Junction coupling between segments
            for (int i = 0; i < segments.size() - 1; i++) {
                float coupling = junctionCoeffs[i];
                float temp = segments[i].rightGoing[segments[i].length-1];
                segments[i].rightGoing[segments[i].length-1] += coupling * segments[i+1].leftGoing[0];
                segments[i+1].leftGoing[0] += coupling * temp;
            }

            // Output is sum of all waveguide outputs
            float output = 0.0f;
            for (const auto& segment : segments) {
                output += (segment.rightGoing[segment.length/2] + segment.leftGoing[segment.length/2]) * 0.25f;
            }

            impulse[n] = output;
        }

        return impulse;
    }

private:
    void generateHouseholderMatrix(std::vector<std::vector<float>>& matrix, int size) {
        // Generate random unit vector
        std::vector<float> v(size);
        float norm = 0.0f;
        for (int i = 0; i < size; i++) {
            v[i] = normal(rng);
            norm += v[i] * v[i];
        }
        norm = sqrtf(norm);
        for (int i = 0; i < size; i++) {
            v[i] /= norm;
        }

        // Create Householder matrix H = I - 2vv^T
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                matrix[i][j] = (i == j ? 1.0f : 0.0f) - 2.0f * v[i] * v[j];
            }
        }
    }
};

// Usage examples and combinations
class AdvancedImpulseGenerator {
private:
    ImpulseResponseGenerator generator;

public:
    // Combine multiple algorithms for rich textures
    std::vector<float> generateHybridImpulse(float sampleRate, float duration) {
        int samples = (int)(duration * sampleRate);
        std::vector<float> result(samples, 0.0f);

        // 1. Physical room foundation
        auto room = generator.generateRoomImpulse(8.0f, 6.0f, 3.0f, 0.15f, sampleRate, duration);

        // 2. Modal resonances for warmth
        auto modal = generator.generateModalDecay(2.5f, sampleRate, duration, 30);

        // 3. Scattering for late diffusion
        auto scatter = generator.generateScatteringImpulse(500.0f, 1.5f, sampleRate, duration);

        // 4. Granular texture for complexity
        auto granular = generator.generateGranularTexture(50.0f, 0.02f, 2.0f, sampleRate, duration * 0.5f);

        // Combine with weighted mixing
        for (int i = 0; i < samples; i++) {
            float t = (float)i / samples;

            result[i] = room[i] * 0.4f +                          // Early reflections
                        modal[i] * 0.3f +                         // Modal resonance
                        (i < scatter.size() ? scatter[i] : 0.0f) * 0.2f * (1.0f - t) + // Scattering (fade out)
                        (i < granular.size() ? granular[i] : 0.0f) * 0.1f * t;         // Granular (fade in)
        }

        return result;
    }

    // Space-specific impulse generators
    std::vector<float> generateCathedralImpulse(float sampleRate, float duration) {
        // Large space with long RT60 and specific modal characteristics
        auto modal = generator.generateModalDecay(8.0f, sampleRate, duration, 100);
        auto fdn = generator.generateFDNImpulse({1000, 1300, 1700, 2300},
                                                {0.85f, 0.82f, 0.78f, 0.75f},
                                                sampleRate, duration);

        int samples = std::min(modal.size(), fdn.size());
        std::vector<float> result(samples);

        for (int i = 0; i < samples; i++) {
            float t = (float)i / samples;
            result[i] = modal[i] * 0.6f + fdn[i] * 0.4f * (1.0f - expf(-t * 2.0f));
        }

        return result;
    }

    std::vector<float> generatePlateImpulse(float sampleRate, float duration) {
        // Plate reverb characteristics - dense, metallic
        auto granular = generator.generateGranularTexture(1000.0f, 0.005f, 4.0f, sampleRate, duration);
        auto modal = generator.generateModalDecay(3.0f, sampleRate, duration, 80);

        int samples = std::min(granular.size(), modal.size());
        std::vector<float> result(samples);

        for (int i = 0; i < samples; i++) {
            result[i] = granular[i] * 0.7f + modal[i] * 0.3f;
        }

        return result;
    }
};

#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <complex>

class IRGenerator {
public:
    static constexpr float PI = 3.14159265359f;
    static constexpr float TWOPI = 2.0f * PI;

    // Velvet Noise Impulse Response Generator
    static std::vector<float> generateVelvetNoise(int length, float sampleRate,
                                                  float density = 2000.0f, float decayTime = 2.0f) {
        std::vector<float> ir(length, 0.0f);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        std::uniform_int_distribution<int> sign(0, 1);

        float avgInterval = sampleRate / density;
        float decayConstant = -3.0f / (decayTime * sampleRate); // -60dB decay

        float nextImpulse = avgInterval;

        for (int i = 0; i < length; ++i) {
            if (i >= nextImpulse) {
                // Add velvet impulse
                float amplitude = std::exp(decayConstant * i);
                ir[i] = amplitude * (sign(gen) ? 1.0f : -1.0f);

                // Schedule next impulse with random jitter
                nextImpulse += avgInterval * (0.5f + dis(gen));
            }
        }

        return ir;
    }

    // Modal Synthesis Impulse Response
    static std::vector<float> generateModalIR(int length, float sampleRate,
                                              const std::vector<float>& frequencies,
                                              const std::vector<float>& amplitudes,
                                              const std::vector<float>& decayTimes) {
        std::vector<float> ir(length, 0.0f);

        for (size_t mode = 0; mode < frequencies.size(); ++mode) {
            float freq = frequencies[mode];
            float amp = amplitudes[mode];
            float decay = -3.0f / (decayTimes[mode] * sampleRate);
            float phase = 0.0f; // Could randomize this

            for (int i = 0; i < length; ++i) {
                float t = i / sampleRate;
                ir[i] += amp * std::exp(decay * i) * std::sin(TWOPI * freq * t + phase);
            }
        }

        return ir;
    }

    // Feedback Delay Network Synthesis
    class FDNGenerator {
    private:
        struct DelayLine {
            std::vector<float> buffer;
            int writeIndex;
            int length;
            float feedback;
            float damping;
            float lastOutput;

            DelayLine(int len, float fb, float damp) :
                    buffer(len, 0.0f), writeIndex(0), length(len),
                    feedback(fb), damping(damp), lastOutput(0.0f) {}

            float process(float input) {
                buffer[writeIndex] = input;
                float output = buffer[writeIndex] * feedback;

                // Simple lowpass damping
                lastOutput = lastOutput * damping + output * (1.0f - damping);

                writeIndex = (writeIndex + 1) % length;
                return lastOutput;
            }
        };

        std::vector<DelayLine> delays;
        std::vector<std::vector<float>> feedbackMatrix;

    public:
        FDNGenerator(const std::vector<int>& delayLengths,
                     const std::vector<float>& feedbacks,
                     const std::vector<float>& dampings) {

            int numDelays = delayLengths.size();

            // Create delay lines
            for (int i = 0; i < numDelays; ++i) {
                delays.emplace_back(delayLengths[i], feedbacks[i], dampings[i]);
            }

            // Create Hadamard-like feedback matrix
            feedbackMatrix.resize(numDelays, std::vector<float>(numDelays));
            float scale = 1.0f / std::sqrt(numDelays);

            for (int i = 0; i < numDelays; ++i) {
                for (int j = 0; j < numDelays; ++j) {
                    feedbackMatrix[i][j] = scale * ((i + j) % 2 == 0 ? 1.0f : -1.0f);
                }
            }
        }

        std::vector<float> generateIR(int length, float inputGain = 1.0f) {
            std::vector<float> ir(length, 0.0f);

            // Impulse at start
            std::vector<float> delayOutputs(delays.size());
            std::vector<float> delayInputs(delays.size(), 0.0f);

            for (int sample = 0; sample < length; ++sample) {
                float input = (sample == 0) ? inputGain : 0.0f;

                // Process delay lines
                for (size_t i = 0; i < delays.size(); ++i) {
                    delayOutputs[i] = delays[i].process(input + delayInputs[i]);
                }

                // Apply feedback matrix
                std::fill(delayInputs.begin(), delayInputs.end(), 0.0f);
                for (size_t i = 0; i < delays.size(); ++i) {
                    for (size_t j = 0; j < delays.size(); ++j) {
                        delayInputs[i] += feedbackMatrix[i][j] * delayOutputs[j];
                    }
                }

                // Sum outputs for stereo/mono
                float output = 0.0f;
                for (float delayOut : delayOutputs) {
                    output += delayOut;
                }
                ir[sample] = output * 0.5f; // Scale down
            }

            return ir;
        }
    };

    // Exponential Decay Noise Generator
    static std::vector<float> generateExponentialNoise(int length, float sampleRate,
                                                       float decayTime = 2.0f,
                                                       float highFreqDecay = 0.8f) {
        std::vector<float> ir(length);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> noise(0.0f, 1.0f);

        float decayConstant = -3.0f / (decayTime * sampleRate);

        // Generate noise with exponential envelope
        for (int i = 0; i < length; ++i) {
            float envelope = std::exp(decayConstant * i);
            ir[i] = noise(gen) * envelope;
        }

        // Apply frequency-dependent damping
        std::vector<float> dampingFilter = {0.1f, 0.2f, 0.4f, 0.2f, 0.1f}; // Simple lowpass
        applyFilter(ir, dampingFilter, highFreqDecay);

        return ir;
    }

    // Schroeder Reverb IR Capture
    class SchroederGenerator {
    private:
        struct CombFilter {
            std::vector<float> buffer;
            int writeIndex;
            float feedback;
            float damping;
            float lastOutput;

            CombFilter(int delay, float fb, float damp = 0.0f) :
                    buffer(delay, 0.0f), writeIndex(0), feedback(fb),
                    damping(damp), lastOutput(0.0f) {}

            float process(float input) {
                float output = buffer[writeIndex];

                // Damping
                lastOutput = lastOutput * damping + output * (1.0f - damping);
                buffer[writeIndex] = input + lastOutput * feedback;

                writeIndex = (writeIndex + 1) % buffer.size();
                return output;
            }
        };

        struct AllpassFilter {
            std::vector<float> buffer;
            int writeIndex;
            float feedback;

            AllpassFilter(int delay, float fb) :
                    buffer(delay, 0.0f), writeIndex(0), feedback(fb) {}

            float process(float input) {
                float delayed = buffer[writeIndex];
                float output = delayed - feedback * input;
                buffer[writeIndex] = input + feedback * delayed;

                writeIndex = (writeIndex + 1) % buffer.size();
                return output;
            }
        };

        std::vector<CombFilter> combs;
        std::vector<AllpassFilter> allpasses;

    public:
        SchroederGenerator(float sampleRate, float roomSize = 1.0f, float damping = 0.3f) {
            // Classic Schroeder delay times (scaled by room size)
            std::vector<int> combDelays = {
                    int(1116 * roomSize), int(1188 * roomSize),
                    int(1277 * roomSize), int(1356 * roomSize),
                    int(1422 * roomSize), int(1491 * roomSize),
                    int(1557 * roomSize), int(1617 * roomSize)
            };

            std::vector<int> allpassDelays = {
                    int(556 * roomSize), int(441 * roomSize),
                    int(341 * roomSize), int(225 * roomSize)
            };

            // Create comb filters
            for (int delay : combDelays) {
                combs.emplace_back(delay, 0.84f, damping);
            }

            // Create allpass filters
            for (int delay : allpassDelays) {
                allpasses.emplace_back(delay, 0.7f);
            }
        }

        std::vector<float> generateIR(int length) {
            std::vector<float> ir(length, 0.0f);

            for (int sample = 0; sample < length; ++sample) {
                float input = (sample == 0) ? 1.0f : 0.0f;

                // Sum comb filter outputs
                float combSum = 0.0f;
                for (auto& comb : combs) {
                    combSum += comb.process(input);
                }

                // Process through allpass chain
                float output = combSum;
                for (auto& allpass : allpasses) {
                    output = allpass.process(output);
                }

                ir[sample] = output * 0.1f; // Scale down
            }

            return ir;
        }
    };

    // Convolution-based Room Simulation
    static std::vector<float> generateRoomIR(int length, float sampleRate,
                                             float roomWidth, float roomHeight, float roomDepth,
                                             float absorption = 0.3f) {
        std::vector<float> ir(length, 0.0f);

        // Speed of sound
        const float c = 343.0f; // m/s

        // Generate early reflections based on room dimensions
        std::vector<std::tuple<float, float, int>> reflections; // time, amplitude, order

        // Direct path
        reflections.emplace_back(0.0f, 1.0f, 0);

        // First-order reflections (6 walls)
        float walls[] = {roomWidth, roomWidth, roomHeight, roomHeight, roomDepth, roomDepth};
        for (int i = 0; i < 6; ++i) {
            float distance = walls[i];
            float time = distance / c;
            float amplitude = std::pow(1.0f - absorption, 1);

            int sampleDelay = int(time * sampleRate);
            if (sampleDelay < length) {
                reflections.emplace_back(time, amplitude, 1);
            }
        }

        // Second-order reflections (simplified)
        for (int i = 0; i < 6; ++i) {
            for (int j = i + 1; j < 6; ++j) {
                float distance = walls[i] + walls[j];
                float time = distance / c;
                float amplitude = std::pow(1.0f - absorption, 2) * 0.5f;

                int sampleDelay = int(time * sampleRate);
                if (sampleDelay < length) {
                    reflections.emplace_back(time, amplitude, 2);
                }
            }
        }

        // Place reflections in IR
        for (const auto& reflection : reflections) {
            float time = std::get<0>(reflection);
            float amplitude = std::get<1>(reflection);
            int sampleDelay = int(time * sampleRate);

            if (sampleDelay < length) {
                ir[sampleDelay] += amplitude;
            }
        }

        // Add diffuse tail using velvet noise
        auto tail = generateVelvetNoise(length - 1000, sampleRate, 1500.0f, 2.0f);
        for (int i = 1000; i < length && i - 1000 < tail.size(); ++i) {
            ir[i] += tail[i - 1000] * 0.3f;
        }

        return ir;
    }

private:
    // Utility function for filtering
    static void applyFilter(std::vector<float>& signal,
                            const std::vector<float>& filter,
                            float strength) {
        std::vector<float> filtered(signal.size(), 0.0f);
        int filterSize = filter.size();
        int halfFilter = filterSize / 2;

        for (int i = halfFilter; i < signal.size() - halfFilter; ++i) {
            for (int j = 0; j < filterSize; ++j) {
                filtered[i] += signal[i - halfFilter + j] * filter[j];
            }
        }

        // Blend original and filtered
        for (int i = 0; i < signal.size(); ++i) {
            signal[i] = signal[i] * (1.0f - strength) + filtered[i] * strength;
        }
    }

};

#include <vector>
#include <string>
#include <map>
#include <cmath>

struct ModalData {
    std::vector<float> frequencies;    // Hz
    std::vector<float> amplitudes;     // Linear (0.0-1.0)
    std::vector<float> decayTimes;     // Seconds
    std::vector<float> phases;         // Radians (optional)
    float fundamentalFreq;             // Base frequency for scaling
    std::string description;
};

class ModalDatabase {
public:
    std::map<std::string, ModalData> objects;

    ModalDatabase() {
        initializeDatabase();
    }

private:
    void initializeDatabase() {

        // METALLIC OBJECTS

        // Steel Bar (1m length, 2cm diameter)
        objects["steel_bar"] = {
                {196.0f, 540.0f, 1056.0f, 1740.0f, 2592.0f, 3612.0f, 4800.0f, 6156.0f},
                {1.0f, 0.64f, 0.41f, 0.25f, 0.16f, 0.10f, 0.064f, 0.041f},
                {4.2f, 3.8f, 3.4f, 3.0f, 2.6f, 2.2f, 1.8f, 1.4f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                196.0f,
                "Cylindrical steel bar, longitudinal modes"
        };

        // Brass Plate (30cm x 20cm x 2mm)
        objects["brass_plate"] = {
                {127.0f, 254.0f, 381.0f, 508.0f, 762.0f, 889.0f, 1143.0f, 1397.0f, 1651.0f},
                {1.0f, 0.71f, 0.45f, 0.32f, 0.25f, 0.20f, 0.16f, 0.13f, 0.10f},
                {6.8f, 5.9f, 5.1f, 4.4f, 3.8f, 3.3f, 2.9f, 2.5f, 2.2f},
                {0.0f, 0.79f, 1.57f, 2.36f, 0.0f, 0.79f, 1.57f, 2.36f, 0.0f},
                127.0f,
                "Rectangular brass plate, bending modes"
        };

        // Church Bell (large)
        objects["church_bell"] = {
                {196.0f, 392.0f, 466.0f, 554.0f, 698.0f, 831.0f, 1047.0f, 1245.0f, 1568.0f},
                {1.0f, 0.45f, 0.32f, 0.28f, 0.22f, 0.18f, 0.15f, 0.12f, 0.09f},
                {12.5f, 11.2f, 8.7f, 7.3f, 6.1f, 5.2f, 4.4f, 3.8f, 3.2f},
                {0.0f, 1.57f, 3.14f, 4.71f, 0.0f, 1.57f, 3.14f, 4.71f, 0.0f},
                196.0f,
                "Bronze church bell, traditional profile"
        };

        // Cymbal (18 inch crash)
        objects["crash_cymbal"] = {
                {234.0f, 468.0f, 702.0f, 936.0f, 1404.0f, 1872.0f, 2574.0f, 3510.0f, 4680.0f},
                {1.0f, 0.68f, 0.42f, 0.25f, 0.18f, 0.13f, 0.09f, 0.065f, 0.045f},
                {3.2f, 2.8f, 2.4f, 2.0f, 1.6f, 1.3f, 1.0f, 0.8f, 0.6f},
                {0.0f, 2.09f, 4.19f, 0.52f, 2.62f, 4.71f, 1.05f, 3.14f, 5.24f},
                234.0f,
                "Bronze cymbal, complex mode structure"
        };

        // Aluminum Can
        objects["aluminum_can"] = {
                {312.0f, 624.0f, 936.0f, 1248.0f, 1872.0f, 2496.0f, 3120.0f, 3744.0f},
                {1.0f, 0.52f, 0.28f, 0.16f, 0.11f, 0.08f, 0.06f, 0.045f},
                {1.8f, 1.5f, 1.2f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                312.0f,
                "Empty aluminum beverage can"
        };

        // WOODEN OBJECTS

        // Wooden Box (guitar body size)
        objects["wooden_box"] = {
                {85.0f, 110.0f, 165.0f, 220.0f, 275.0f, 330.0f, 440.0f, 550.0f, 660.0f},
                {1.0f, 0.78f, 0.61f, 0.48f, 0.38f, 0.30f, 0.24f, 0.19f, 0.15f},
                {2.1f, 1.9f, 1.7f, 1.5f, 1.3f, 1.2f, 1.0f, 0.9f, 0.8f},
                {0.0f, 0.52f, 1.05f, 1.57f, 2.09f, 2.62f, 3.14f, 3.67f, 4.19f},
                85.0f,
                "Hollow wooden box, spruce construction"
        };

        // Wooden Plank (2x4 lumber)
        objects["wood_plank"] = {
                {92.0f, 254.0f, 498.0f, 824.0f, 1232.0f, 1722.0f, 2294.0f, 2948.0f},
                {1.0f, 0.58f, 0.33f, 0.19f, 0.11f, 0.065f, 0.038f, 0.022f},
                {1.4f, 1.2f, 1.0f, 0.85f, 0.7f, 0.6f, 0.5f, 0.4f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                92.0f,
                "Pine lumber plank, bending modes"
        };

        // Marimba Bar (medium pitch)
        objects["marimba_bar"] = {
                {261.6f, 1637.5f, 3265.9f, 5100.0f, 7140.6f, 9387.5f},
                {1.0f, 0.12f, 0.04f, 0.02f, 0.012f, 0.008f},
                {3.5f, 2.1f, 1.2f, 0.8f, 0.5f, 0.3f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                261.6f,
                "Rosewood marimba bar, fundamental + overtones"
        };

        // GLASS OBJECTS

        // Wine Glass (crystal)
        objects["wine_glass"] = {
                {440.0f, 1320.0f, 2200.0f, 3520.0f, 5280.0f, 7480.0f, 10120.0f},
                {1.0f, 0.35f, 0.15f, 0.08f, 0.045f, 0.025f, 0.014f},
                {8.2f, 6.5f, 4.8f, 3.2f, 2.1f, 1.4f, 0.9f},
                {0.0f, 2.09f, 4.19f, 0.52f, 2.62f, 4.71f, 1.05f},
                440.0f,
                "Crystal wine glass, rim excitation"
        };

        // Glass Bottle
        objects["glass_bottle"] = {
                {165.0f, 330.0f, 495.0f, 825.0f, 1155.0f, 1650.0f, 2310.0f, 3135.0f},
                {1.0f, 0.45f, 0.22f, 0.15f, 0.10f, 0.07f, 0.05f, 0.035f},
                {4.5f, 3.8f, 3.1f, 2.5f, 2.0f, 1.6f, 1.2f, 0.9f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                165.0f,
                "Glass bottle, Helmholtz + body resonances"
        };

        // CERAMIC/CLAY OBJECTS

        // Clay Pot (medium size)
        objects["clay_pot"] = {
                {145.0f, 290.0f, 435.0f, 725.0f, 1015.0f, 1450.0f, 2030.0f, 2755.0f},
                {1.0f, 0.52f, 0.28f, 0.18f, 0.12f, 0.08f, 0.055f, 0.038f},
                {3.2f, 2.8f, 2.4f, 2.0f, 1.7f, 1.4f, 1.1f, 0.9f},
                {0.0f, 1.05f, 2.09f, 3.14f, 4.19f, 5.24f, 0.52f, 1.57f},
                145.0f,
                "Terra cotta clay pot"
        };

        // Porcelain Bowl
        objects["porcelain_bowl"] = {
                {520.0f, 1040.0f, 1560.0f, 2600.0f, 3640.0f, 5200.0f, 7280.0f, 9880.0f},
                {1.0f, 0.38f, 0.16f, 0.10f, 0.065f, 0.042f, 0.027f, 0.018f},
                {6.8f, 5.2f, 3.8f, 2.7f, 1.9f, 1.3f, 0.9f, 0.6f},
                {0.0f, 2.09f, 4.19f, 0.52f, 2.62f, 4.71f, 1.05f, 3.14f},
                520.0f,
                "Thin porcelain bowl"
        };

        // PLASTIC OBJECTS

        // Plastic Bucket
        objects["plastic_bucket"] = {
                {78.0f, 156.0f, 234.0f, 390.0f, 546.0f, 780.0f, 1092.0f, 1482.0f},
                {1.0f, 0.62f, 0.38f, 0.24f, 0.16f, 0.11f, 0.075f, 0.052f},
                {1.2f, 1.0f, 0.85f, 0.7f, 0.6f, 0.5f, 0.42f, 0.35f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                78.0f,
                "HDPE plastic bucket"
        };

        // STRINGS AND MEMBRANES

        // Steel String (guitar E string)
        objects["guitar_string_e"] = {
                {329.6f, 659.3f, 988.9f, 1318.5f, 1648.1f, 1977.7f, 2307.4f, 2637.0f},
                {1.0f, 0.32f, 0.18f, 0.11f, 0.075f, 0.053f, 0.039f, 0.029f},
                {4.5f, 3.2f, 2.1f, 1.4f, 0.95f, 0.65f, 0.45f, 0.31f},
                {0.0f, 3.14f, 0.0f, 3.14f, 0.0f, 3.14f, 0.0f, 3.14f},
                329.6f,
                "Steel guitar string, 1st string open E"
        };

        // Drumhead (snare drum)
        objects["snare_drumhead"] = {
                {200.0f, 346.4f, 400.0f, 547.7f, 600.0f, 692.8f, 800.0f, 948.7f},
                {1.0f, 0.45f, 0.71f, 0.28f, 0.45f, 0.22f, 0.32f, 0.18f},
                {0.8f, 0.65f, 0.7f, 0.55f, 0.6f, 0.5f, 0.55f, 0.45f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                200.0f,
                "Mylar drumhead, circular membrane modes"
        };

        // COMPLEX OBJECTS

        // Piano String + Soundboard Coupling
        objects["piano_string_c4"] = {
                {261.6f, 523.3f, 784.9f, 1046.5f, 1570.0f, 2093.3f, 2616.6f, 3140.0f},
                {1.0f, 0.28f, 0.15f, 0.088f, 0.065f, 0.048f, 0.036f, 0.027f},
                {8.2f, 6.5f, 4.8f, 3.2f, 2.4f, 1.8f, 1.35f, 1.0f},
                {0.0f, 3.14f, 0.0f, 3.14f, 0.0f, 3.14f, 0.0f, 3.14f},
                261.6f,
                "Piano string with soundboard coupling"
        };

        // Tuning Fork (A440)
        objects["tuning_fork_a440"] = {
                {440.0f, 1143.0f, 2500.0f, 4290.0f, 6512.0f, 9166.0f},
                {1.0f, 0.15f, 0.045f, 0.018f, 0.008f, 0.004f},
                {15.0f, 8.5f, 4.2f, 2.1f, 1.2f, 0.7f},
                {0.0f, 1.57f, 0.0f, 1.57f, 0.0f, 1.57f},
                440.0f,
                "Steel tuning fork, fundamental + bending modes"
        };
    }

public:
    // Get modal data for an object
    ModalData getModalData(const std::string& objectName) const {
        auto it = objects.find(objectName);
        if (it != objects.end()) {
            return it->second;
        }
        return ModalData(); // Return empty if not found
    }

    // Scale modal data to a different fundamental frequency
    ModalData scaledModalData(const std::string& objectName, float newFundamental) const {
        ModalData data = getModalData(objectName);
        if (data.frequencies.empty()) return data;

        float scaleFactor = newFundamental / data.fundamentalFreq;

        // Scale frequencies
        for (auto& freq : data.frequencies) {
            freq *= scaleFactor;
        }
        data.fundamentalFreq = newFundamental;

        // Decay times scale inversely with frequency (approximately)
        float decayScale = std::sqrt(1.0f / scaleFactor);
        for (auto& decay : data.decayTimes) {
            decay *= decayScale;
        }

        return data;
    }

    // Get list of available objects
    std::vector<std::string> getAvailableObjects() const {
        std::vector<std::string> names;
        for (const auto& pair : objects) {
            names.push_back(pair.first);
        }
        return names;
    }

    // Generate physical variations (material/size changes)
    ModalData generateVariation(const std::string& objectName,
                                float sizeScale = 1.0f,
                                float densityScale = 1.0f,
                                float dampingScale = 1.0f) const {
        ModalData data = getModalData(objectName);
        if (data.frequencies.empty()) return data;

        // Frequency scaling: inversely proportional to size
        float freqScale = 1.0f / sizeScale;

        // Density affects frequency: higher density = lower frequency
        freqScale *= std::sqrt(1.0f / densityScale);

        for (auto& freq : data.frequencies) {
            freq *= freqScale;
        }

        // Decay time scaling: larger objects ring longer, denser materials ring longer
        float decayScale = sizeScale * std::sqrt(densityScale) * (1.0f / dampingScale);
        for (auto& decay : data.decayTimes) {
            decay *= decayScale;
        }

        data.fundamentalFreq *= freqScale;

        return data;
    }
};



#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <random>
#include <complex>
#include <algorithm>

// Include the previous ModalDatabase class here
// [Previous ModalDatabase code would go here - omitted for brevity]

class ModalIRGenerator {
private:
    std::random_device rd;
    mutable std::mt19937 gen;

public:
    ModalIRGenerator() : gen(rd()) {}

    // Basic modal synthesis from database
    std::vector<float> generateModalIR(const std::string& objectName,
                                       int length,
                                       float sampleRate,
                                       float velocity = 1.0f,
                                       float excitationWidth = 0.001f) const {

        ModalData data = database.getModalData(objectName);
        if (data.frequencies.empty()) {
            return std::vector<float>(length, 0.0f);
        }

        return synthesizeFromModalData(data, length, sampleRate, velocity, excitationWidth);
    }

    // Generate scaled version
    std::vector<float> generateScaledModalIR(const std::string& objectName,
                                             float newFundamental,
                                             int length,
                                             float sampleRate,
                                             float velocity = 1.0f) const {

        ModalData data = database.scaledModalData(objectName, newFundamental);
        return synthesizeFromModalData(data, length, sampleRate, velocity);
    }

    // Generate with physical variations
    std::vector<float> generateVariationIR(const std::string& objectName,
                                           int length,
                                           float sampleRate,
                                           float sizeScale = 1.0f,
                                           float densityScale = 1.0f,
                                           float dampingScale = 1.0f,
                                           float velocity = 1.0f) const {

        ModalData data = database.generateVariation(objectName, sizeScale, densityScale, dampingScale);
        return synthesizeFromModalData(data, length, sampleRate, velocity);
    }

    // Advanced: Multiple strike positions (different mode excitation patterns)
    std::vector<float> generatePositionalIR(const std::string& objectName,
                                            int length,
                                            float sampleRate,
                                            float strikePosition = 0.5f, // 0.0 to 1.0
                                            float velocity = 1.0f) const {

        ModalData data = database.getModalData(objectName);
        if (data.frequencies.empty()) {
            return std::vector<float>(length, 0.0f);
        }

        // Modify amplitudes based on strike position
        // Different modes are excited differently at different positions
        for (size_t i = 0; i < data.amplitudes.size(); ++i) {
            float modeNumber = i + 1;
            // Sine wave envelope - some modes are cancelled at certain positions
            float positionFactor = std::abs(std::sin(modeNumber * M_PI * strikePosition));
            data.amplitudes[i] *= positionFactor;
        }

        return synthesizeFromModalData(data, length, sampleRate, velocity);
    }

    // Nonlinear excitation (harder strikes excite higher modes more)
    std::vector<float> generateNonlinearIR(const std::string& objectName,
                                           int length,
                                           float sampleRate,
                                           float velocity = 1.0f,
                                           float nonlinearity = 0.3f) const {

        ModalData data = database.getModalData(objectName);
        if (data.frequencies.empty()) {
            return std::vector<float>(length, 0.0f);
        }

        // Higher velocity emphasizes higher modes nonlinearly
        float velocityCurve = std::pow(velocity, 1.0f + nonlinearity);

        for (size_t i = 0; i < data.amplitudes.size(); ++i) {
            float modeNumber = i + 1;
            // Higher modes are excited more with higher velocity
            float modeExcitation = std::pow(velocityCurve, modeNumber * 0.1f);
            data.amplitudes[i] *= modeExcitation;
        }

        return synthesizeFromModalData(data, length, sampleRate, velocity);
    }

    // Generate ensemble (multiple slightly detuned copies)
    std::vector<float> generateEnsembleIR(const std::string& objectName,
                                          int numVoices,
                                          int length,
                                          float sampleRate,
                                          float detuneAmount = 0.02f, // ±2%
                                          float velocity = 1.0f) const {

        std::vector<float> ir(length, 0.0f);
        std::uniform_real_distribution<float> detune(-detuneAmount, detuneAmount);
        std::uniform_real_distribution<float> phase(0.0f, 2.0f * M_PI);

        for (int voice = 0; voice < numVoices; ++voice) {
            ModalData data = database.getModalData(objectName);

            // Detune each voice slightly
            for (auto& freq : data.frequencies) {
                freq *= (1.0f + detune(gen));
            }

            // Randomize phases
            for (auto& ph : data.phases) {
                ph = phase(gen);
            }

            // Generate individual IR
            auto voiceIR = synthesizeFromModalData(data, length, sampleRate, velocity / numVoices);

            // Add to ensemble
            for (int i = 0; i < length; ++i) {
                ir[i] += voiceIR[i];
            }
        }

        return ir;
    }

    // Generate with sympathetic resonance (modes of one object exciting another)
    std::vector<float> generateSympatheticIR(const std::string& primaryObject,
                                             const std::string& sympatheticObject,
                                             int length,
                                             float sampleRate,
                                             float couplingStrength = 0.1f,
                                             float velocity = 1.0f) const {

        // Generate primary object IR
        auto primaryIR = generateModalIR(primaryObject, length, sampleRate, velocity);

        // Get sympathetic object data
        ModalData sympData = database.getModalData(sympatheticObject);
        if (sympData.frequencies.empty()) {
            return primaryIR;
        }

        // Find which sympathetic modes are excited by primary frequencies
        ModalData primaryData = database.getModalData(primaryObject);
        std::vector<float> sympatheticAmps(sympData.amplitudes.size(), 0.0f);

        for (size_t i = 0; i < sympData.frequencies.size(); ++i) {
            float sympFreq = sympData.frequencies[i];

            // Check for frequency matches with primary modes
            for (size_t j = 0; j < primaryData.frequencies.size(); ++j) {
                float primaryFreq = primaryData.frequencies[j];
                float freqRatio = std::abs(sympFreq - primaryFreq) / primaryFreq;

                // Strong coupling for exact matches, weaker for harmonics
                if (freqRatio < 0.02f) { // Within 2%
                    sympatheticAmps[i] += primaryData.amplitudes[j] * couplingStrength;
                } else if (freqRatio < 0.05f) { // Within 5%
                    sympatheticAmps[i] += primaryData.amplitudes[j] * couplingStrength * 0.3f;
                }
            }
        }

        // Replace sympathetic amplitudes
        sympData.amplitudes = sympatheticAmps;

        // Generate sympathetic response with delay
        int delayMs = 10; // 10ms coupling delay
        int delaySamples = (delayMs * sampleRate) / 1000;

        auto sympatheticIR = synthesizeFromModalData(sympData, length - delaySamples, sampleRate, 1.0f);

        // Combine with primary IR
        for (int i = delaySamples; i < length && (i - delaySamples) < sympatheticIR.size(); ++i) {
            primaryIR[i] += sympatheticIR[i - delaySamples];
        }

        return primaryIR;
    }

    // Generate with room coupling (modes affected by room resonances)
    std::vector<float> generateRoomCoupledIR(const std::string& objectName,
                                             int length,
                                             float sampleRate,
                                             float roomVolume = 100.0f, // m³
                                             float absorption = 0.2f,
                                             float velocity = 1.0f) const {

        // Generate base object IR
        auto objectIR = generateModalIR(objectName, length, sampleRate, velocity);

        // Calculate room modes (simplified rectangular room)
        float roomLength = std::cbrt(roomVolume * 2.0f); // Approximate dimensions
        float roomWidth = roomLength * 0.8f;
        float roomHeight = roomLength * 0.4f;

        std::vector<float> roomFreqs;
        std::vector<float> roomAmps;
        std::vector<float> roomDecays;

        // Generate first few room modes
        for (int nx = 0; nx < 4; ++nx) {
            for (int ny = 0; ny < 3; ++ny) {
                for (int nz = 0; nz < 3; ++nz) {
                    if (nx == 0 && ny == 0 && nz == 0) continue;

                    float freq = 343.0f * 0.5f * std::sqrt(
                            (nx * nx) / (roomLength * roomLength) +
                            (ny * ny) / (roomWidth * roomWidth) +
                            (nz * nz) / (roomHeight * roomHeight)
                    );

                    if (freq < 500.0f) { // Only low frequency room modes
                        roomFreqs.push_back(freq);
                        roomAmps.push_back(0.1f / (1.0f + nx + ny + nz)); // Decay with mode order
                        roomDecays.push_back(roomVolume * (1.0f - absorption) * 0.01f); // Sabine formula approximation
                    }
                }
            }
        }

        // Create room response
        ModalData roomData;
        roomData.frequencies = roomFreqs;
        roomData.amplitudes = roomAmps;
        roomData.decayTimes = roomDecays;
        roomData.phases.resize(roomFreqs.size(), 0.0f);

        // Generate room IR with longer length
        int roomIRLength = length * 2; // Room response is longer
        auto roomIR = synthesizeFromModalData(roomData, roomIRLength, sampleRate, 0.3f);

        // Convolve object with room (simplified - use FFT convolution in practice)
        return simpleConvolve(objectIR, roomIR, length);
    }

    ModalDatabase database;
private:
    // Core synthesis function
    std::vector<float> synthesizeFromModalData(const ModalData& data,
                                               int length,
                                               float sampleRate,
                                               float velocity = 1.0f,
                                               float excitationWidth = 0.001f) const {

        std::vector<float> ir(length, 0.0f);

        // Generate excitation signal (brief impulse or shaped attack)
        int excitationSamples = std::max(1, int(excitationWidth * sampleRate));
        std::vector<float> excitation(excitationSamples);

        // Shaped excitation (avoids harsh transients)
        for (int i = 0; i < excitationSamples; ++i) {
            float t = float(i) / excitationSamples;
            excitation[i] = std::sin(M_PI * t) * velocity; // Half-sine pulse
        }

        // Synthesize each mode
        for (size_t mode = 0; mode < data.frequencies.size(); ++mode) {
            float freq = data.frequencies[mode];
            float amp = data.amplitudes[mode];
            float decay = data.decayTimes[mode];
            float phase = (mode < data.phases.size()) ? data.phases[mode] : 0.0f;

            if (freq <= 0.0f || amp <= 0.0f) continue;

            float omega = 2.0f * M_PI * freq / sampleRate;
            float decayRate = -6.91f / (decay * sampleRate); // -60dB decay

            // Add excitation-convolved modal response
            for (int i = 0; i < length; ++i) {
                float modalResponse = 0.0f;

                // Convolve excitation with modal response
                for (int j = 0; j < excitationSamples && (i - j) >= 0; ++j) {
                    int sampleIndex = i - j;
                    float envelope = std::exp(decayRate * sampleIndex);
                    float oscillation = std::sin(omega * sampleIndex + phase);
                    modalResponse += excitation[j] * envelope * oscillation;
                }

                ir[i] += amp * modalResponse;
            }
        }

        // Normalize to prevent clipping
        normalizeIR(ir);

        return ir;
    }

    // Simple convolution (use FFT-based for production)
    std::vector<float> simpleConvolve(const std::vector<float>& signal1,
                                      const std::vector<float>& signal2,
                                      int outputLength) const {

        std::vector<float> result(outputLength, 0.0f);

        for (int i = 0; i < outputLength; ++i) {
            for (int j = 0; j < std::min(int(signal2.size()), i + 1); ++j) {
                if (i - j < signal1.size()) {
                    result[i] += signal1[i - j] * signal2[j] * 0.1f; // Scale down room contribution
                }
            }
        }

        return result;
    }

    // Normalize IR to prevent clipping
    void normalizeIR(std::vector<float>& ir) const {
        float maxVal = 0.0f;
        for (float sample : ir) {
            maxVal = std::max(maxVal, std::abs(sample));
        }

        if (maxVal > 0.0f) {
            float scale = 0.95f / maxVal; // Leave some headroom
            for (float& sample : ir) {
                sample *= scale;
            }
        }
    }
};

// Complete usage example
class ModalIRFactory {
private:
    ModalIRGenerator generator;

public:
    // Preset generators for common scenarios

    // Single object hit
    std::vector<float> createObjectHit(const std::string& objectName,
                                       float pitch = 0.0f, // Semitones from original
                                       float velocity = 1.0f,
                                       float sampleRate = 44100.0f,
                                       float duration = 5.0f) {

        int length = int(duration * sampleRate);

        if (pitch == 0.0f) {
            return generator.generateModalIR(objectName, length, sampleRate, velocity);
        } else {
            // Calculate new fundamental frequency
            ModalData data = generator.database.getModalData(objectName);
            float newFund = data.fundamentalFreq * std::pow(2.0f, pitch / 12.0f);
            return generator.generateScaledModalIR(objectName, newFund, length, sampleRate, velocity);
        }
    }

    // Material variation
    std::vector<float> createMaterialVariation(const std::string& baseObject,
                                               const std::string& material, // "steel", "brass", "wood", etc.
                                               float sampleRate = 44100.0f,
                                               float duration = 5.0f) {

        int length = int(duration * sampleRate);
        float density = 1.0f, damping = 1.0f;

        // Material property mappings
        if (material == "steel") { density = 1.2f; damping = 0.7f; }
        else if (material == "brass") { density = 1.4f; damping = 0.9f; }
        else if (material == "aluminum") { density = 0.6f; damping = 0.8f; }
        else if (material == "wood") { density = 0.3f; damping = 1.5f; }
        else if (material == "plastic") { density = 0.2f; damping = 2.0f; }

        return generator.generateVariationIR(baseObject, length, sampleRate, 1.0f, density, damping);
    }

    // Ensemble/chorus effect
    std::vector<float> createEnsemble(const std::string& objectName,
                                      int numVoices = 3,
                                      float detune = 0.02f,
                                      float sampleRate = 44100.0f,
                                      float duration = 5.0f) {

        int length = int(duration * sampleRate);
        return generator.generateEnsembleIR(objectName, numVoices, length, sampleRate, detune);
    }

    // Room-placed object
    std::vector<float> createRoomObject(const std::string& objectName,
                                        float roomSize = 100.0f, // m³
                                        float roomDamping = 0.2f,
                                        float sampleRate = 44100.0f,
                                        float duration = 8.0f) {

        int length = int(duration * sampleRate);
        return generator.generateRoomCoupledIR(objectName, length, sampleRate, roomSize, roomDamping);
    }
};
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Simple FFT implementation for convolution
class SimpleFFT {
public:
    static void fft(std::vector<std::complex<float>>& data, bool inverse = false) {
        int N = data.size();
        if (N <= 1) return;

        // Bit-reverse permutation
        for (int i = 0, j = 0; i < N; i++) {
            if (i < j) std::swap(data[i], data[j]);
            int k = N >> 1;
            while (j & k) {
                j ^= k;
                k >>= 1;
            }
            j ^= k;
        }

        // FFT computation
        for (int len = 2; len <= N; len <<= 1) {
            float ang = (inverse ? 1 : -1) * 2.0f * M_PI / len;
            std::complex<float> wlen(cosf(ang), sinf(ang));

            for (int i = 0; i < N; i += len) {
                std::complex<float> w(1);
                for (int j = 0; j < len / 2; j++) {
                    std::complex<float> u = data[i + j];
                    std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse) {
            for (auto& x : data) x /= N;
        }
    }
};

// True convolution reverb using overlap-save method
class ConvolutionReverb {
private:
    std::vector<float> impulseResponse;
    std::vector<std::complex<float>> impulseFFT;
    std::vector<std::complex<float>> inputBuffer;
    std::vector<std::complex<float>> outputBuffer;
    std::vector<float> overlapBuffer;

    int blockSize;
    int fftSize;
    int impulseLength;
    int bufferPos;

    void generateImpulseResponse() {
        // Generate a realistic room impulse response
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);

        impulseResponse.resize(impulseLength);

        // Early reflections (first 50ms)
        int earlyLength = std::min(impulseLength, (int)(impulseLength * 0.1f));
        for (int i = 0; i < earlyLength; i++) {
            float t = (float)i / impulseLength;
            impulseResponse[i] = expf(-t * 5.0f) * dist(gen) * 0.3f;
        }

        // Late reverb tail
        for (int i = earlyLength; i < impulseLength; i++) {
            float t = (float)i / impulseLength;
            float envelope = expf(-t * 3.0f); // Exponential decay

            // Add some random variation to simulate room modes
            float variation = 1.0f + 0.3f * sinf(t * 50.0f) + 0.2f * sinf(t * 137.0f);

            impulseResponse[i] = envelope * variation * dist(gen) * 0.1f;
        }

        // Normalize
        float maxVal = 0.0f;
        for (float sample : impulseResponse) {
            maxVal = std::max(maxVal, std::abs(sample));
        }
        if (maxVal > 0.0f) {
            for (float& sample : impulseResponse) {
                sample /= maxVal * 2.0f; // Scale down to prevent clipping
            }
        }
    }

    void computeImpulseFFT() {
        // Zero-pad impulse to FFT size
        std::vector<std::complex<float>> paddedImpulse(fftSize, std::complex<float>(0, 0));
        for (int i = 0; i < impulseLength && i < fftSize; i++) {
            paddedImpulse[i] = std::complex<float>(impulseResponse[i], 0);
        }

        // Compute FFT of impulse
        impulseFFT = paddedImpulse;
        SimpleFFT::fft(impulseFFT);
    }

public:
    ConvolutionReverb(int blockSz = 512, int impulseLengthMs = 1000, float sampleRate = 44100.0f)
            : blockSize(blockSz), bufferPos(0) {

        impulseLength = (int)(impulseLengthMs * sampleRate / 1000.0f);
        fftSize = blockSize * 2; // For overlap-save, FFT size = 2 * block size

        // Make sure FFT size is power of 2
        int powerOf2 = 1;
        while (powerOf2 < fftSize) powerOf2 *= 2;
        fftSize = powerOf2;

        inputBuffer.resize(fftSize, std::complex<float>(0, 0));
        outputBuffer.resize(fftSize);
        overlapBuffer.resize(blockSize, 0.0f);

        generateImpulseResponse();
        computeImpulseFFT();
    }

    void processBlock(const float* input, float* output, int numSamples) {
        // Process in blocks for efficiency
        for (int i = 0; i < numSamples; i += blockSize) {
            int currentBlockSize = std::min(blockSize, numSamples - i);

            // Fill input buffer (overlap-save method)
            // Keep last blockSize samples, add new blockSize samples
            for (int j = 0; j < blockSize; j++) {
                inputBuffer[j] = inputBuffer[j + blockSize];
            }

            for (int j = 0; j < currentBlockSize; j++) {
                inputBuffer[blockSize + j] = std::complex<float>(input[i + j], 0);
            }

            // Zero pad if necessary
            for (int j = currentBlockSize; j < blockSize; j++) {
                inputBuffer[blockSize + j] = std::complex<float>(0, 0);
            }

            // Forward FFT
            outputBuffer = inputBuffer;
            SimpleFFT::fft(outputBuffer);

            // Convolution in frequency domain
            for (int j = 0; j < fftSize; j++) {
                outputBuffer[j] *= impulseFFT[j];
            }

            // Inverse FFT
            SimpleFFT::fft(outputBuffer, true);

            // Extract valid output samples (discard first blockSize samples)
            for (int j = 0; j < currentBlockSize; j++) {
                output[i + j] = outputBuffer[blockSize + j].real();
            }
        }
    }

    float processSample(float input) {
        static std::vector<float> inputBlock(512);
        static std::vector<float> outputBlock(512);
        static int blockPos = 0;
        static bool hasOutput = false;

        // Buffer input samples
        inputBlock[blockPos] = input;
        blockPos++;

        float result = 0.0f;

        if (blockPos >= blockSize) {
            // Process the block
            processBlock(inputBlock.data(), outputBlock.data(), blockSize);
            blockPos = 0;
            hasOutput = true;
        }

        if (hasOutput) {
            static int outputPos = 0;
            result = outputBlock[outputPos];
            outputPos++;
            if (outputPos >= blockSize) {
                outputPos = 0;
            }
        }

        return result;
    }

    void setRoomSize(float size) {
        // Regenerate impulse with different decay time
        float decayFactor = 1.0f + size * 4.0f; // 1-5x decay time

        for (int i = 0; i < impulseLength; i++) {
            float t = (float)i / impulseLength;
            float originalEnvelope = expf(-t * 3.0f);
            float newEnvelope = expf(-t * 3.0f / decayFactor);

            if (originalEnvelope > 0.0f) {
                impulseResponse[i] *= (newEnvelope / originalEnvelope);
            }
        }

        computeImpulseFFT();
    }

    void setDampening(float dampening) {
        // Apply frequency-dependent dampening to impulse
        // This is a simplified approach - real implementation would modify impulseFFT directly

        // High-frequency roll-off based on dampening
        for (int i = 0; i < impulseLength; i++) {
            float t = (float)i / impulseLength;
            float hfRolloff = 1.0f - dampening * (1.0f - expf(-t * 10.0f));

            // Simple high-frequency attenuation simulation
            if (i > 0) {
                impulseResponse[i] = impulseResponse[i] * hfRolloff +
                                     impulseResponse[i-1] * (1.0f - hfRolloff) * 0.5f;
            }
        }

        computeImpulseFFT();
    }
};

// Optimized delay line for traditional reverb algorithms
class OptimizedDelayLine {
private:
    std::vector<float> buffer;
    int writeIndex;
    int size;

public:
    explicit OptimizedDelayLine(int maxDelay) : size(maxDelay), writeIndex(0) {
        buffer.resize(maxDelay, 0.0f);
    }

    void write(float sample) {
        buffer[writeIndex] = sample;
        writeIndex = (writeIndex + 1) % size;
    }

    float read(int delaySamples) {
        int readIndex = (writeIndex - delaySamples + size) % size;
        return buffer[readIndex];
    }

    float readInterp(float delaySamples) {
        float readPos = writeIndex - delaySamples;
        if (readPos < 0) readPos += size;

        int idx = (int)readPos;
        float frac = readPos - idx;

        int nextIdx = (idx + 1) % size;

        return buffer[idx] * (1.0f - frac) + buffer[nextIdx] * frac;
    }
};

// Simple all-pass filter
class SimpleAllPass {
private:
    OptimizedDelayLine delay;
    float feedback;

public:
    SimpleAllPass(int delayLength, float fb) : delay(delayLength), feedback(fb) {}

    float process(float input) {
        float delayed = delay.read(0);
        float output = -feedback * input + delayed;
        delay.write(input + feedback * delayed);
        return output;
    }
};

// Hybrid reverb combining convolution with algorithmic reverb
class HybridReverbEngine {
private:
    ConvolutionReverb* convolution;

    // Traditional algorithmic components
    SimpleAllPass* allpass[4];
    OptimizedDelayLine* combFilters[4];
    float combFeedback[4];
    float combDelayTimes[4];

    float convolutionLevel;
    float algorithmicLevel;
    float roomSize;
    float dampening;

public:
    HybridReverbEngine(float sampleRate, int convolutionLength = 500) {
        // Initialize convolution reverb for early reflections
        convolution = new ConvolutionReverb(256, convolutionLength, sampleRate);

        // Initialize algorithmic reverb for late tail
        int allpassDelays[4] = {347, 113, 37, 59};
        for (int i = 0; i < 4; i++) {
            allpass[i] = new SimpleAllPass(allpassDelays[i], 0.7f);
        }

        // Comb filters for late reverb
        combDelayTimes[0] = 1116;
        combDelayTimes[1] = 1188;
        combDelayTimes[2] = 1277;
        combDelayTimes[3] = 1356;

        for (int i = 0; i < 4; i++) {
            combFilters[i] = new OptimizedDelayLine((int)combDelayTimes[i]);
            combFeedback[i] = 0.84f;
        }

        convolutionLevel = 0.4f;
        algorithmicLevel = 0.6f;
        roomSize = 0.7f;
        dampening = 0.3f;
    }

    ~HybridReverbEngine() {
        delete convolution;
        for (int i = 0; i < 4; i++) {
            delete allpass[i];
            delete combFilters[i];
        }
    }

    float process(float input) {
        // Early reflections via convolution
        float early = convolution->processSample(input) * convolutionLevel;

        // Late reverb via algorithmic approach
        float late = input;

        // All-pass diffusion
        for (int i = 0; i < 4; i++) {
            late = allpass[i]->process(late);
        }

        // Parallel comb filters
        float combSum = 0.0f;
        for (int i = 0; i < 4; i++) {
            float delayed = combFilters[i]->read((int)combDelayTimes[i]);
            float filtered = delayed * combFeedback[i];
            combFilters[i]->write(late + filtered);
            combSum += filtered;
        }

        late = combSum * algorithmicLevel;

        return early + late;
    }

    void setRoomSize(float size) {
        roomSize = std::clamp(size, 0.1f, 1.0f);
        convolution->setRoomSize(size);

        // Adjust comb feedback for room size
        for (int i = 0; i < 4; i++) {
            combFeedback[i] = 0.7f + size * 0.2f;
        }
    }

    void setDampening(float damp) {
        dampening = std::clamp(damp, 0.0f, 1.0f);
        convolution->setDampening(damp);

        // Adjust comb feedback for dampening
        for (int i = 0; i < 4; i++) {
            combFeedback[i] *= (1.0f - damp * 0.3f);
        }
    }

    void setMix(float convLevel, float algoLevel) {
        convolutionLevel = std::clamp(convLevel, 0.0f, 1.0f);
        algorithmicLevel = std::clamp(algoLevel, 0.0f, 1.0f);
    }
};

// Stereo version
class StereoHybridReverb {
private:
    HybridReverbEngine* leftEngine;
    HybridReverbEngine* rightEngine;

public:
    StereoHybridReverb(float sampleRate) {
        leftEngine = new HybridReverbEngine(sampleRate, 400);
        rightEngine = new HybridReverbEngine(sampleRate, 450); // Slightly different for stereo
    }

    ~StereoHybridReverb() {
        delete leftEngine;
        delete rightEngine;
    }

    void process(float inputL, float inputR, float& outputL, float& outputR,
                 float wetLevel = 0.3f, float dryLevel = 0.7f) {

        // Cross-mix inputs slightly for stereo width
        float mixL = inputL + inputR * 0.1f;
        float mixR = inputR + inputL * 0.1f;

        float reverbL = leftEngine->process(mixL);
        float reverbR = rightEngine->process(mixR);

        outputL = inputL * dryLevel + reverbL * wetLevel;
        outputR = inputR * dryLevel + reverbR * wetLevel;
    }

    void setParameters(float roomSize, float dampening, float convLevel = 0.4f, float algoLevel = 0.6f) {
        leftEngine->setRoomSize(roomSize);
        leftEngine->setDampening(dampening);
        leftEngine->setMix(convLevel, algoLevel);

        rightEngine->setRoomSize(roomSize * 1.02f);
        rightEngine->setDampening(dampening);
        rightEngine->setMix(convLevel, algoLevel);
    }
};

/*
Usage example:

StereoHybridReverb reverb(44100.0f);
reverb.setParameters(0.8f, 0.3f, 0.4f, 0.6f); // room, dampening, conv level, algo level

// In audio callback:
float outL, outR;
reverb.process(inputL, inputR, outL, outR, 0.25f, 0.75f); // 25% wet, 75% dry
*/
#endif //GRAINSTORM_REVERB8_H
