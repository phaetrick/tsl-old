//
// Created by pr on 19.07.25.
//
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <memory>
#include <algorithm>
#include <complex>

class SophisticatedVocoder {
private:
    static constexpr int NUM_BANDS = 32;
    static constexpr int FFT_SIZE = 1024;
    static constexpr int OVERLAP = 4;
    static constexpr int HOP_SIZE = FFT_SIZE / OVERLAP;
    static constexpr float PI = 3.14159265359f;
    static constexpr float TWO_PI = 2.0f * PI;

    struct FilterBank {
        std::array<float, NUM_BANDS> frequencies;
        std::array<float, NUM_BANDS> bandwidths;
        std::array<std::vector<float>, NUM_BANDS> impulseResponses;
        std::array<std::vector<float>, NUM_BANDS> carrierDelayLines;
        std::array<std::vector<float>, NUM_BANDS> modulatorDelayLines;
        std::array<float, NUM_BANDS> envelopes;
        std::array<float, NUM_BANDS> smoothedEnvelopes;
    };

    struct SpectralProcessor {
        std::vector<std::complex<float>> fftBuffer;
        std::vector<float> window;
        std::vector<float> inputBuffer;
        std::vector<float> outputBuffer;
        std::vector<float> overlapBuffer;
        int bufferPosition = 0;
    };

    // Advanced parameters
    struct VocoderParams {
        float attack = 0.01f;           // Envelope attack time
        float release = 0.1f;           // Envelope release time
        float bandwidth = 1.0f;         // Band bandwidth multiplier
        float formantShift = 1.0f;      // Formant frequency shifting
        float unvoicedMix = 0.1f;       // Mix of unprocessed carrier for unvoiced sounds
        float noiseGate = -60.0f;       // Noise gate threshold in dB
        float compression = 0.5f;       // Dynamic compression ratio
        float spectralTilt = 0.0f;      // Spectral tilt correction
        bool phaseVocoder = true;       // Enable phase vocoder mode
        bool formantCorrection = true;   // Enable formant preservation
    } params;

    FilterBank filterBank;
    SpectralProcessor modProcessor, carrierProcessor;

    float sampleRate;
    float nyquist;

    // Envelope followers with different time constants
    std::array<float, NUM_BANDS> attackCoeffs;
    std::array<float, NUM_BANDS> releaseCoeffs;

    // Formant tracking
    std::array<float, NUM_BANDS> formantFreqs;
    std::array<float, NUM_BANDS> formantAmps;

    // Unvoiced detection
    float spectralCentroid = 0.0f;
    float spectralRolloff = 0.0f;
    float zcr = 0.0f; // Zero crossing rate

public:
    SophisticatedVocoder(float sampleRate) : sampleRate(sampleRate), nyquist(sampleRate * 0.5f) {
        initializeFilterBank();
        initializeSpectralProcessors();
        calculateEnvelopeCoeffs();
    }

    void setParameters(const VocoderParams& newParams) {
        params = newParams;
        calculateEnvelopeCoeffs();
        if (params.bandwidth != 1.0f) {
            updateBandwidths();
        }
    }

    // Main processing function
    void process(const float* modulator, const float* carrier, float* output, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float vocoderOut = 0.0f;

            if (params.phaseVocoder) {
                vocoderOut = processSpectral(modulator[i], carrier[i]);
            } else {
                vocoderOut = processFilterBank(modulator[i], carrier[i]);
            }

            // Unvoiced sound detection and mixing
            float unvoicedLevel = detectUnvoiced(modulator[i]);
            float unvoicedCarrier = carrier[i] * unvoicedLevel * params.unvoicedMix;

            // Apply noise gate
            float gateLevel = applyNoiseGate(modulator[i]);

            // Mix vocoded and unvoiced signals
            output[i] = (vocoderOut * gateLevel) + unvoicedCarrier;

            // Apply compression
            output[i] = applyCompression(output[i]);
        }
    }

private:
    void initializeFilterBank() {
        // Calculate logarithmic frequency distribution
        float minFreq = 80.0f;
        float maxFreq = nyquist * 0.9f;
        float logMin = std::log(minFreq);
        float logMax = std::log(maxFreq);

        for (int i = 0; i < NUM_BANDS; ++i) {
            float t = static_cast<float>(i) / (NUM_BANDS - 1);
            filterBank.frequencies[i] = std::exp(logMin + t * (logMax - logMin));

            // Calculate bandwidth using ERB (Equivalent Rectangular Bandwidth)
            float erb = 24.7f * (4.37f * filterBank.frequencies[i] / 1000.0f + 1.0f);
            filterBank.bandwidths[i] = erb * params.bandwidth;

            // Initialize delay lines
            int delaySize = static_cast<int>(sampleRate * 0.01f); // 10ms delay
            filterBank.carrierDelayLines[i].resize(delaySize, 0.0f);
            filterBank.modulatorDelayLines[i].resize(delaySize, 0.0f);

            // Generate impulse responses for bandpass filters
            generateBandpassFilter(i);
        }
    }

    void generateBandpassFilter(int band) {
        float centerFreq = filterBank.frequencies[band];
        float bandwidth = filterBank.bandwidths[band];

        // Calculate filter coefficients for 4th order Butterworth bandpass
        int filterLength = 128;
        filterBank.impulseResponses[band].resize(filterLength);

        // Generate windowed sinc filter
        float normalizedFreq = centerFreq / sampleRate;
        float normalizedBW = bandwidth / sampleRate;

        for (int n = 0; n < filterLength; ++n) {
            float t = n - filterLength / 2;
            float sinc = (t == 0) ? 1.0f : std::sin(PI * normalizedFreq * t) / (PI * normalizedFreq * t);

            // Apply Hanning window
            float window = 0.5f * (1.0f - std::cos(TWO_PI * n / (filterLength - 1)));
            filterBank.impulseResponses[band][n] = sinc * window;
        }
    }

    void initializeSpectralProcessors() {
        // Initialize FFT buffers
        modProcessor.fftBuffer.resize(FFT_SIZE);
        carrierProcessor.fftBuffer.resize(FFT_SIZE);

        modProcessor.inputBuffer.resize(FFT_SIZE, 0.0f);
        carrierProcessor.inputBuffer.resize(FFT_SIZE, 0.0f);

        modProcessor.outputBuffer.resize(FFT_SIZE, 0.0f);
        carrierProcessor.outputBuffer.resize(FFT_SIZE, 0.0f);

        modProcessor.overlapBuffer.resize(FFT_SIZE, 0.0f);
        carrierProcessor.overlapBuffer.resize(FFT_SIZE, 0.0f);

        // Generate Hann window
        modProcessor.window.resize(FFT_SIZE);
        carrierProcessor.window.resize(FFT_SIZE);

        for (int i = 0; i < FFT_SIZE; ++i) {
            float w = 0.5f * (1.0f - std::cos(TWO_PI * i / (FFT_SIZE - 1)));
            modProcessor.window[i] = w;
            carrierProcessor.window[i] = w;
        }
    }

    void calculateEnvelopeCoeffs() {
        float attackTime = params.attack * sampleRate;
        float releaseTime = params.release * sampleRate;

        for (int i = 0; i < NUM_BANDS; ++i) {
            attackCoeffs[i] = 1.0f - std::exp(-1.0f / attackTime);
            releaseCoeffs[i] = 1.0f - std::exp(-1.0f / releaseTime);
        }
    }

    float processFilterBank(float modulator, float carrier) {
        float output = 0.0f;

        for (int band = 0; band < NUM_BANDS; ++band) {
            // Filter both modulator and carrier through bandpass filters
            float modFiltered = applyBandpassFilter(modulator, band, true);
            float carrierFiltered = applyBandpassFilter(carrier, band, false);

            // Extract envelope from modulator
            float envelope = std::abs(modFiltered);

            // Apply envelope following with attack/release
            float coeff = (envelope > filterBank.smoothedEnvelopes[band]) ?
                          attackCoeffs[band] : releaseCoeffs[band];

            filterBank.smoothedEnvelopes[band] +=
                    coeff * (envelope - filterBank.smoothedEnvelopes[band]);

            // Apply formant correction if enabled
            if (params.formantCorrection) {
                carrierFiltered = applyFormantCorrection(carrierFiltered, band);
            }

            // Amplitude modulation
            float bandOutput = carrierFiltered * filterBank.smoothedEnvelopes[band];

            // Apply spectral tilt correction
            float tiltGain = std::pow(10.0f, params.spectralTilt * band / (NUM_BANDS * 20.0f));
            bandOutput *= tiltGain;

            output += bandOutput;
        }

        return output / NUM_BANDS;
    }

    float processSpectral(float modulator, float carrier) {
        // Add samples to input buffers
        modProcessor.inputBuffer[modProcessor.bufferPosition] = modulator;
        carrierProcessor.inputBuffer[carrierProcessor.bufferPosition] = carrier;

        modProcessor.bufferPosition++;
        carrierProcessor.bufferPosition++;

        if (modProcessor.bufferPosition >= HOP_SIZE) {
            performSpectralProcessing();
            modProcessor.bufferPosition = 0;
            carrierProcessor.bufferPosition = 0;
        }

        // Output from overlap buffer
        float output = modProcessor.overlapBuffer[modProcessor.bufferPosition];
        return output;
    }

    void performSpectralProcessing() {
        // Apply window and perform FFT on both signals
        performFFT(modProcessor);
        performFFT(carrierProcessor);

        // Spectral processing: transfer magnitude from modulator to carrier
        for (int bin = 0; bin < FFT_SIZE / 2 + 1; ++bin) {
            float modMagnitude = std::abs(modProcessor.fftBuffer[bin]);
            std::complex<float> carrierComplex = carrierProcessor.fftBuffer[bin];
            float carrierPhase = std::arg(carrierComplex);

            // Apply formant shifting
            int shiftedBin = static_cast<int>(bin * params.formantShift);
            if (shiftedBin < FFT_SIZE / 2 + 1) {
                carrierProcessor.fftBuffer[bin] = std::polar(modMagnitude, carrierPhase);
            }
        }

        // Inverse FFT and overlap-add
        performIFFT(carrierProcessor);
        overlapAdd(carrierProcessor);
    }

    float applyBandpassFilter(float input, int band, bool isModulator) {
        auto& delayLine = isModulator ?
                          filterBank.modulatorDelayLines[band] :
                          filterBank.carrierDelayLines[band];
        auto& impulse = filterBank.impulseResponses[band];

        // Circular buffer for delay line
        static std::array<int, NUM_BANDS> delayIndices = {};
        int& idx = delayIndices[band];

        delayLine[idx] = input;

        // Convolution
        float output = 0.0f;
        for (size_t i = 0; i < impulse.size() && i < delayLine.size(); ++i) {
            int tapIdx = (idx - i + delayLine.size()) % delayLine.size();
            output += delayLine[tapIdx] * impulse[i];
        }

        idx = (idx + 1) % delayLine.size();
        return output;
    }

    float detectUnvoiced(float modulator) {
        // Simple unvoiced detection based on spectral characteristics
        // In a real implementation, you'd use more sophisticated methods

        static float prevSample = 0.0f;
        static float spectralEnergy = 0.0f;
        static int sampleCount = 0;

        // Zero crossing rate
        if ((modulator > 0 && prevSample <= 0) || (modulator <= 0 && prevSample > 0)) {
            zcr += 1.0f;
        }

        spectralEnergy += modulator * modulator;
        sampleCount++;

        if (sampleCount >= 256) { // Update every 256 samples
            zcr /= sampleCount;
            spectralEnergy /= sampleCount;

            // High ZCR and low energy suggests unvoiced sound
            float unvoicedLevel = std::min(1.0f, zcr * 10.0f);

            zcr = 0.0f;
            spectralEnergy = 0.0f;
            sampleCount = 0;

            return unvoicedLevel;
        }

        prevSample = modulator;
        return 0.0f;
    }

    float applyNoiseGate(float input) {
        float inputDb = 20.0f * std::log10(std::max(std::abs(input), 1e-10f));

        if (inputDb < params.noiseGate) {
            return 0.0f;
        }

        // Smooth transition above threshold
        float gateRange = 6.0f; // 6dB transition range
        if (inputDb < params.noiseGate + gateRange) {
            float ratio = (inputDb - params.noiseGate) / gateRange;
            return ratio * ratio; // Smooth curve
        }

        return 1.0f;
    }

    float applyCompression(float input) {
        float threshold = -12.0f; // dB
        float inputDb = 20.0f * std::log10(std::max(std::abs(input), 1e-10f));

        if (inputDb > threshold) {
            float excess = inputDb - threshold;
            float compressedExcess = excess * params.compression;
            float outputDb = threshold + compressedExcess;
            float gainReduction = outputDb - inputDb;
            return input * std::pow(10.0f, gainReduction / 20.0f);
        }

        return input;
    }

    float applyFormantCorrection(float input, int band) {
        // Simplified formant correction
        // In practice, you'd use more sophisticated formant tracking
        return input; // Placeholder
    }

    // Simplified FFT functions (you'd use a proper FFT library like FFTW or kiss_fft)
    void performFFT(SpectralProcessor& processor) {
        // Apply window
        for (int i = 0; i < FFT_SIZE; ++i) {
            processor.fftBuffer[i] = std::complex<float>(
                    processor.inputBuffer[i] * processor.window[i], 0.0f);
        }
        // FFT would go here - using placeholder
    }

    void performIFFT(SpectralProcessor& processor) {
        // IFFT would go here - using placeholder
        for (int i = 0; i < FFT_SIZE; ++i) {
            processor.outputBuffer[i] = processor.fftBuffer[i].real();
        }
    }

    void overlapAdd(SpectralProcessor& processor) {
        // Overlap-add synthesis
        for (int i = 0; i < FFT_SIZE; ++i) {
            processor.overlapBuffer[i] += processor.outputBuffer[i] * processor.window[i];
        }

        // Shift buffer
        std::copy(processor.overlapBuffer.begin() + HOP_SIZE,
                  processor.overlapBuffer.end(),
                  processor.overlapBuffer.begin());
        std::fill(processor.overlapBuffer.end() - HOP_SIZE,
                  processor.overlapBuffer.end(), 0.0f);
    }

    void updateBandwidths() {
        for (int i = 0; i < NUM_BANDS; ++i) {
            float erb = 24.7f * (4.37f * filterBank.frequencies[i] / 1000.0f + 1.0f);
            filterBank.bandwidths[i] = erb * params.bandwidth;
            generateBandpassFilter(i);
        }
    }
};

#pragma once
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <memory>
#include <functional>

class GranularGrainGenerators {
private:
    static constexpr float PI = 3.14159265359f;
    static constexpr float TWO_PI = 2.0f * PI;

    std::mt19937 rng;
    std::uniform_real_distribution<float> uniform01{0.0f, 1.0f};
    std::normal_distribution<float> gaussian{0.0f, 1.0f};

    float sampleRate;
    float currentTime = 0.0f;

    // Physics simulation state for bouncing ball
    struct BouncingBallState {
        float position = 1.0f;
        float velocity = 0.0f;
        float gravity = -9.8f;
        float damping = 0.8f;
        float groundLevel = 0.0f;
        float lastBounceTime = 0.0f;
    } ballState;

    // Swing pendulum state
    struct SwingState {
        float angle = 0.0f;
        float angularVelocity = 0.0f;
        float length = 1.0f;
        float gravity = 9.8f;
        float damping = 0.995f;
        float amplitude = PI * 0.25f;
    } swingState;

    // Rain system state
    struct RainState {
        std::vector<float> droplets;
        float intensity = 0.5f;
        float windFactor = 0.0f;
        int maxDroplets = 100;
        float dropletLifetime = 2.0f;
    } rainState;

public:
    struct GrainParameters {
        float duration = 0.1f;      // Grain duration in seconds
        float pitch = 1.0f;         // Pitch multiplier
        float pan = 0.0f;           // Stereo pan (-1 to 1)
        float amplitude = 1.0f;     // Grain amplitude
        float startPos = 0.0f;      // Start position in source material
        int windowType = 0;         // 0=Hann, 1=Gaussian, 2=Tukey, 3=Blackman
    };

    GranularGrainGenerators(float sr) : sampleRate(sr), rng(std::random_device{}()) {
        rainState.droplets.reserve(rainState.maxDroplets);
    }

    void advanceTime(float deltaTime) {
        currentTime += deltaTime;
    }

    void reset() {
        currentTime = 0.0f;
        ballState = BouncingBallState{};
        swingState = SwingState{};
        rainState.droplets.clear();
    }

    // ============================================================================
    // WATER DROP ALGORITHM
    // Generates grains with organic timing based on fluid dynamics
    // ============================================================================
    std::vector<GrainParameters> generateWaterDrop(float intensity = 0.5f,
                                                   float viscosity = 0.3f,
                                                   float surface_tension = 0.7f) {
        std::vector<GrainParameters> grains;

        // Water drop formation probability based on surface tension
        float dropFormationRate = intensity * (1.0f - surface_tension) * 10.0f;

        // Check if we should generate a drop
        if (uniform01(rng) < dropFormationRate * (1.0f / sampleRate)) {
            GrainParameters grain;

            // Drop size affects duration and pitch
            float dropSize = 0.3f + uniform01(rng) * 0.7f;
            grain.duration = 0.05f + dropSize * 0.15f;

            // Surface tension affects pitch (smaller drops = higher pitch)
            grain.pitch = 0.8f + (1.0f - dropSize) * 1.2f + surface_tension * 0.5f;

            // Viscosity affects timing irregularity
            float timing_jitter = viscosity * 0.02f * gaussian(rng);
            grain.startPos = std::max(0.0f, timing_jitter);

            // Drop impact creates amplitude burst
            grain.amplitude = 0.3f + dropSize * 0.7f;

            // Slight stereo spread for realism
            grain.pan = gaussian(rng) * 0.2f;
            grain.windowType = 1; // Gaussian for organic feel

            grains.push_back(grain);

            // Sometimes generate micro-splashes
            if (dropSize > 0.6f && uniform01(rng) < 0.3f) {
                for (int i = 0; i < 2 + static_cast<int>(uniform01(rng) * 3); ++i) {
                    GrainParameters splash = grain;
                    splash.duration *= 0.3f;
                    splash.pitch *= 1.5f + uniform01(rng) * 2.0f;
                    splash.amplitude *= 0.2f + uniform01(rng) * 0.3f;
                    splash.pan = gaussian(rng) * 0.6f;
                    splash.startPos += (i + 1) * 0.001f; // Slight delay
                    grains.push_back(splash);
                }
            }
        }

        return grains;
    }

    // ============================================================================
    // RAIN ALGORITHM
    // Generates multiple overlapping grains with weather-like patterns
    // ============================================================================
    std::vector<GrainParameters> generateRain(float intensity = 0.5f,
                                              float windSpeed = 0.0f,
                                              float dropSize = 0.5f) {
        std::vector<GrainParameters> grains;

        rainState.intensity = intensity;
        rainState.windFactor = windSpeed;

        // Update existing droplets
        for (auto it = rainState.droplets.begin(); it != rainState.droplets.end();) {
            *it -= 1.0f / sampleRate;
            if (*it <= 0.0f) {
                it = rainState.droplets.erase(it);
            } else {
                ++it;
            }
        }

        // Generate new droplets based on intensity
        float dropletRate = intensity * 50.0f; // Base rate
        int newDroplets = std::poisson_distribution<int>(dropletRate / sampleRate)(rng);

        for (int i = 0; i < newDroplets && rainState.droplets.size() < rainState.maxDroplets; ++i) {
            rainState.droplets.push_back(rainState.dropletLifetime * uniform01(rng));

            GrainParameters grain;

            // Droplet size variation
            float size = dropSize * (0.5f + uniform01(rng) * 0.5f);
            grain.duration = 0.02f + size * 0.08f;

            // Wind affects pitch and timing
            float windEffect = windSpeed * gaussian(rng) * 0.1f;
            grain.pitch = 0.7f + size * 0.8f + windEffect;

            // Rain creates stereo field
            grain.pan = gaussian(rng) * 0.8f + windSpeed * 0.3f;
            grain.pan = std::clamp(grain.pan, -1.0f, 1.0f);

            // Amplitude varies with drop size and intensity
            grain.amplitude = size * intensity * (0.1f + uniform01(rng) * 0.2f);

            // Slight timing variations for realism
            grain.startPos = uniform01(rng) * 0.01f;
            grain.windowType = 0; // Hann window for rain

            grains.push_back(grain);
        }

        return grains;
    }

    // ============================================================================
    // BOUNCING BALL ALGORITHM
    // Physics-based grain generation with realistic bouncing dynamics
    // ============================================================================
    std::vector<GrainParameters> generateBouncingBall(float gravity = 9.8f,
                                                      float damping = 0.8f,
                                                      float surface = 0.7f) {
        std::vector<GrainParameters> grains;

        ballState.gravity = -gravity;
        ballState.damping = damping;

        float deltaTime = 1.0f / sampleRate;

        // Update physics
        ballState.velocity += ballState.gravity * deltaTime;
        ballState.position += ballState.velocity * deltaTime;

        // Check for ground collision
        if (ballState.position <= ballState.groundLevel && ballState.velocity < 0) {
            ballState.position = ballState.groundLevel;
            ballState.velocity *= -ballState.damping;

            // Generate bounce grain
            float bounceIntensity = std::abs(ballState.velocity) / 10.0f;

            if (bounceIntensity > 0.05f) { // Minimum threshold for audible bounce
                GrainParameters grain;

                // Bounce intensity affects all parameters
                grain.amplitude = std::min(1.0f, bounceIntensity);
                grain.duration = 0.03f + bounceIntensity * 0.1f;

                // Higher bounces = higher pitch, surface hardness affects tone
                grain.pitch = 0.5f + bounceIntensity * 2.0f + surface * 1.5f;

                // Ball position affects stereo placement
                grain.pan = 0.0f; // Center for now, could add horizontal movement

                // Surface type affects window shape
                grain.windowType = surface > 0.5f ? 3 : 0; // Blackman for hard, Hann for soft

                grains.push_back(grain);

                // Add resonant tail for hard surfaces
                if (surface > 0.6f && bounceIntensity > 0.2f) {
                    GrainParameters resonance = grain;
                    resonance.duration *= 2.0f;
                    resonance.amplitude *= 0.3f;
                    resonance.pitch *= 1.2f;
                    resonance.startPos = 0.01f; // Slight delay
                    resonance.windowType = 1; // Gaussian for resonance
                    grains.push_back(resonance);
                }
            }

            ballState.lastBounceTime = currentTime;
        }

        // Reset if ball gets too low energy
        if (std::abs(ballState.velocity) < 0.1f && ballState.position <= 0.01f) {
            ballState.position = 2.0f + uniform01(rng) * 3.0f; // Random drop height
            ballState.velocity = 0.0f;
        }

        return grains;
    }

    // ============================================================================
    // SWING/PENDULUM ALGORITHM
    // Generates grains based on pendulum motion with realistic physics
    // ============================================================================
    std::vector<GrainParameters> generateSwing(float length = 1.0f,
                                               float amplitude = 0.25f,
                                               float friction = 0.995f) {
        std::vector<GrainParameters> grains;

        swingState.length = length;
        swingState.amplitude = amplitude * PI;
        swingState.damping = friction;

        float deltaTime = 1.0f / sampleRate;

        // Pendulum physics: θ'' = -(g/L) * sin(θ) - damping * θ'
        float angularAccel = -(swingState.gravity / swingState.length) * std::sin(swingState.angle);
        swingState.angularVelocity += angularAccel * deltaTime;
        swingState.angularVelocity *= swingState.damping; // Apply damping
        swingState.angle += swingState.angularVelocity * deltaTime;

        // Generate grains at extreme points (direction changes)
        static float lastAngularVelocity = 0.0f;
        bool directionChanged = (lastAngularVelocity * swingState.angularVelocity <= 0) &&
                                std::abs(swingState.angularVelocity) > 0.01f;

        if (directionChanged) {
            GrainParameters grain;

            // Position affects pitch and pan
            float normalizedAngle = swingState.angle / swingState.amplitude;
            grain.pan = std::clamp(normalizedAngle, -1.0f, 1.0f);

            // Speed affects grain characteristics
            float speed = std::abs(swingState.angularVelocity);
            grain.amplitude = 0.2f + speed * 0.5f;
            grain.duration = 0.1f + (1.0f / (speed + 1.0f)) * 0.3f;

            // Pendulum length affects base pitch
            grain.pitch = 0.5f + (2.0f / std::sqrt(swingState.length)) + speed * 0.5f;

            // Wind chime-like quality
            grain.windowType = 1; // Gaussian for bell-like quality

            grains.push_back(grain);

            // Add harmonics for metallic swing sound
            if (speed > 0.5f) {
                for (int harmonic = 2; harmonic <= 4; ++harmonic) {
                    GrainParameters harm = grain;
                    harm.pitch *= harmonic;
                    harm.amplitude *= 0.3f / harmonic;
                    harm.duration *= 0.7f;
                    harm.startPos = harmonic * 0.001f; // Slight phase offset
                    grains.push_back(harm);
                }
            }
        }

        lastAngularVelocity = swingState.angularVelocity;

        // Restart swing if energy too low
        if (std::abs(swingState.angularVelocity) < 0.01f && std::abs(swingState.angle) < 0.01f) {
            swingState.angle = swingState.amplitude * (0.5f + uniform01(rng) * 0.5f);
            swingState.angularVelocity = 0.0f;
        }

        return grains;
    }

    // ============================================================================
    // WIND CHIMES ALGORITHM
    // Random collision-based grain generation
    // ============================================================================
    std::vector<GrainParameters> generateWindChimes(float windStrength = 0.3f,
                                                    int numChimes = 8,
                                                    float materialDensity = 0.5f) {
        std::vector<GrainParameters> grains;

        // Wind affects collision probability
        float collisionRate = windStrength * windStrength * 5.0f;

        if (uniform01(rng) < collisionRate * (1.0f / sampleRate)) {
            // Choose random chime
            int chimeIndex = static_cast<int>(uniform01(rng) * numChimes);

            GrainParameters grain;

            // Each chime has a base frequency
            float baseFreq = 200.0f + chimeIndex * 150.0f;
            grain.pitch = baseFreq / 440.0f; // Convert to pitch multiplier

            // Material affects tone and duration
            grain.duration = 0.5f + materialDensity * 2.0f;
            grain.amplitude = 0.2f + uniform01(rng) * 0.6f;

            // Wind affects stereo movement
            grain.pan = gaussian(rng) * windStrength;

            // Metallic harmonics
            grain.windowType = 2; // Tukey window for metallic attack

            grains.push_back(grain);

            // Add metallic overtones
            std::vector<float> overtones = {2.76f, 5.4f, 8.93f}; // Typical bell ratios
            for (float ratio : overtones) {
                if (uniform01(rng) < 0.7f) { // Not all overtones every time
                    GrainParameters overtone = grain;
                    overtone.pitch *= ratio;
                    overtone.amplitude *= 0.3f / ratio;
                    overtone.duration *= 0.8f;
                    grains.push_back(overtone);
                }
            }
        }

        return grains;
    }

    // ============================================================================
    // BUBBLE ALGORITHM
    // Organic bubble formation and popping
    // ============================================================================
    std::vector<GrainParameters> generateBubbles(float density = 0.3f,
                                                 float viscosity = 0.5f,
                                                 float temperature = 0.5f) {
        std::vector<GrainParameters> grains;

        // Bubble formation rate
        float bubbleRate = density * (1.0f + temperature) * 3.0f;

        if (uniform01(rng) < bubbleRate * (1.0f / sampleRate)) {
            GrainParameters grain;

            // Bubble size affects everything
            float bubbleSize = uniform01(rng);
            bubbleSize = std::pow(bubbleSize, 1.0f + viscosity); // Viscosity affects size distribution

            // Small bubbles = high pitch, large = low pitch
            grain.pitch = 2.0f + (1.0f - bubbleSize) * 3.0f;
            grain.duration = 0.02f + bubbleSize * 0.1f;
            grain.amplitude = 0.1f + bubbleSize * 0.4f;

            // Temperature affects bubble behavior
            float thermal_jitter = temperature * gaussian(rng) * 0.1f;
            grain.pitch += thermal_jitter;

            // Bubbles have random stereo placement
            grain.pan = gaussian(rng) * 0.5f;

            // Quick attack, exponential decay
            grain.windowType = 0; // Hann window

            grains.push_back(grain);

            // Large bubbles sometimes create multiple pops
            if (bubbleSize > 0.7f && uniform01(rng) < 0.4f) {
                for (int i = 0; i < 2; ++i) {
                    GrainParameters subBubble = grain;
                    subBubble.pitch *= 1.5f + uniform01(rng);
                    subBubble.amplitude *= 0.3f;
                    subBubble.duration *= 0.5f;
                    subBubble.startPos = (i + 1) * 0.005f;
                    grains.push_back(subBubble);
                }
            }
        }

        return grains;
    }

    // ============================================================================
    // CRACKLING FIRE ALGORITHM
    // Random pops and crackles with varying intensity
    // ============================================================================
    std::vector<GrainParameters> generateFire(float intensity = 0.4f,
                                              float woodDensity = 0.5f,
                                              float moisture = 0.2f) {
        std::vector<GrainParameters> grains;

        // Fire activity based on intensity and fuel properties
        float baseRate = intensity * (1.0f - moisture * 0.5f) * 8.0f;

        // Different types of fire sounds
        if (uniform01(rng) < baseRate * (1.0f / sampleRate)) {
            GrainParameters grain;

            float soundType = uniform01(rng);

            if (soundType < 0.6f) {
                // Pop/crack
                grain.duration = 0.01f + uniform01(rng) * 0.05f;
                grain.pitch = 1.0f + uniform01(rng) * 4.0f;
                grain.amplitude = 0.3f + uniform01(rng) * 0.7f;
                grain.windowType = 3; // Blackman for sharp attack
            }
            else if (soundType < 0.85f) {
                // Hiss/sizzle
                grain.duration = 0.1f + uniform01(rng) * 0.3f;
                grain.pitch = 3.0f + uniform01(rng) * 5.0f;
                grain.amplitude = 0.1f + uniform01(rng) * 0.3f;
                grain.windowType = 1; // Gaussian for continuous sound
            }
            else {
                // Deep wood crack
                grain.duration = 0.05f + woodDensity * 0.15f;
                grain.pitch = 0.3f + uniform01(rng) * 0.7f;
                grain.amplitude = 0.4f + uniform01(rng) * 0.6f;
                grain.windowType = 0; // Hann window
            }

            // Stereo placement
            grain.pan = gaussian(rng) * 0.4f;

            // Moisture affects timing
            if (moisture > 0.3f && uniform01(rng) < moisture) {
                grain.startPos = uniform01(rng) * 0.02f; // Slight delay for wet wood
            }

            grains.push_back(grain);
        }

        return grains;
    }
};
// Example usage
#include "Reverb8.h"
int IRGENTEST() {
    const int sampleRate = 44100;
    const int irLength = sampleRate * 3; // 3 seconds

    // Generate velvet noise IR
    auto velvetIR = IRGenerator::generateVelvetNoise(irLength, sampleRate, 2000.0f, 2.5f);

    // Generate modal IR (bell-like)
    std::vector<float> freqs = {200.0f, 345.0f, 567.0f, 789.0f, 1234.0f};
    std::vector<float> amps = {1.0f, 0.8f, 0.6f, 0.4f, 0.3f};
    std::vector<float> decays = {3.0f, 2.5f, 2.0f, 1.5f, 1.0f};
    auto modalIR = IRGenerator::generateModalIR(irLength, sampleRate, freqs, amps, decays);

    // Generate FDN reverb IR
    IRGenerator::FDNGenerator fdn({347, 113, 37, 59, 43, 37, 29, 19},
                                  {0.85f, 0.82f, 0.78f, 0.75f, 0.72f, 0.69f, 0.66f, 0.63f},
                                  {0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f});
    auto fdnIR = fdn.generateIR(irLength);

    // Generate Schroeder reverb IR
    IRGenerator::SchroederGenerator schroeder(sampleRate, 1.2f, 0.25f);
    auto schroederIR = schroeder.generateIR(irLength);

    // Generate room simulation IR
    auto roomIR = IRGenerator::generateRoomIR(irLength, sampleRate, 8.0f, 3.0f, 12.0f, 0.2f);

    return 0;
}
// Example usage
int MODALIRTEST() {
    ModalIRFactory factory;

    // Create various IRs
    auto bellIR = factory.createObjectHit("church_bell", 0.0f, 1.0f);
    auto steelPlateIR = factory.createMaterialVariation("brass_plate", "steel");
    auto cymbalEnsemble = factory.createEnsemble("crash_cymbal", 4, 0.03f);
    auto roomBell = factory.createRoomObject("church_bell", 200.0f, 0.15f);

    // These IRs can now be saved as .wav files or used for convolution
    // saveWaveFile("bell.wav", bellIR, 44100);

    return 0;
}
// Usage example function
void demonstrateModalDatabase() {
    ModalDatabase db;

    // Get available objects
    auto objects = db.getAvailableObjects();

    // Get modal data for a church bell
    ModalData bellData = db.getModalData("church_bell");

    // Scale to different pitch
    ModalData smallBell = db.scaledModalData("church_bell", 392.0f); // G4

    // Create material variations
    ModalData ironBell = db.generateVariation("church_bell", 1.0f, 1.3f, 0.8f); // Iron: denser, less damped
    ModalData largeBell = db.generateVariation("church_bell", 1.5f, 1.0f, 1.0f); // 1.5x size

    // Generate impulse response using modal data
    // (This would integrate with the previous IRGenerator code)
}
int revtest() {
    const float SAMPLE_RATE = 48000.0f;
    AdvancedStereoReverb reverb(SAMPLE_RATE);

    // Configure advanced parameters
    reverb.setParameters(
            0.85f,  // Room size
            0.75f,  // Diffusion
            0.35f,  // Dampening
            0.25f,  // Early reflections
            0.45f,  // Late reverb
            0.20f,  // Spectral processing
            0.03f * SAMPLE_RATE  // Predelay (30ms)
    );

    reverb.setStereoWidth(0.8f);

    // Process samples
    float inputL = 1.0f, inputR = 0.8f;
    float outputL, outputR;

    reverb.process(inputL, inputR, outputL, outputR, 0.4f, 0.6f);

    std::cout << "Advanced Reverb Engine Ready" << std::endl;
    std::cout << "Input: L=" << inputL << ", R=" << inputR << std::endl;
    std::cout << "Output: L=" << outputL << ", R=" << outputR << std::endl;

    return 0;
}

