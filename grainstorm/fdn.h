//
// Created by pr on 10.03.26.
//
#pragma once

#include <defines.h>

#include <cmath>
#include <cstring>
#include <array>
#include <algorithm>
#include <vector>
#include <cassert>

namespace fdn_detail {

    bool hasNearRatio(int a, int b, int maxPQ = 12, MYFLOAT thresh = 0.002f) {
        MYFLOAT r = static_cast<MYFLOAT>(a) / static_cast<MYFLOAT>(b);
        for (int p = 1; p <= maxPQ; p++)
            for (int q = 1; q <= maxPQ; q++)
                if (std::abs(r - static_cast<MYFLOAT>(p) / q) < thresh)
                    return true;
        return false;
    }

    constexpr MYFLOAT phi = static_cast<MYFLOAT>(1.6180339887498948);


// ------------------------------------------------------------
// prime utilities
// ------------------------------------------------------------

    inline bool isPrime(int n) {
        if (n < 2) return false;
        if ((n & 1) == 0) return n == 2;

        for (int i = 3; i * i <= n; i += 2)
            if (n % i == 0)
                return false;

        return true;
    }

    inline int nearestPrime(int n) {
        int lo = n;
        int hi = n;

        while (true) {
            if (lo >= 2 && isPrime(lo)) return lo;
            if (isPrime(hi)) return hi;

            --lo;
            ++hi;
        }
    }

// ------------------------------------------------------------
// convert room size → delay range
// ------------------------------------------------------------

    template<typename T>
    void fdnDelayRange(T sampleRate,
                       T minRoomMeters,
                       T maxRoomMeters,
                       int &outMin,
                       int &outMax) {
        constexpr T speedOfSound = static_cast<T>(343.0);

        outMin = static_cast<int>(
                2.0 * minRoomMeters / speedOfSound * sampleRate);

        outMax = static_cast<int>(
                2.0 * maxRoomMeters / speedOfSound * sampleRate);
    }

// ------------------------------------------------------------
// FULL DELAY GENERATOR
// ------------------------------------------------------------

    template<int N, typename T>
    void computeFDNDelays(T sampleRate,
                          T minRoomMeters,
                          T maxRoomMeters,
                          int outDelays[N]) {
        constexpr T kPhi = static_cast<T>(1.6180339887498948);
        constexpr T kSilver = static_cast<T>(2.4142135623730951);
        constexpr T kBronze = static_cast<T>(3.3027756377319946);

        int minD, maxD;
        fdnDelayRange(sampleRate, minRoomMeters, maxRoomMeters, minD, maxD);

        // Collect all primes in range into a pool
        std::vector<int> pool;
        pool.reserve(maxD - minD);
        for (int n = minD; n <= maxD; n++)
            if (isPrime(n)) pool.push_back(n);

        assert((int) pool.size() >= N && "Room range too narrow for N delay lines");

        // Pick N primes using interleaved metallic sequences
        // Map t in [0,1) to pool index
        std::vector<bool> used(pool.size(), false);

        for (int i = 0; i < N; i++) {
            T metal;
            switch (i % 3) {
                case 0:
                    metal = kPhi;
                    break;
                case 1:
                    metal = kSilver;
                    break;
                default:
                    metal = kBronze;
                    break;
            }

            T t = std::fmod(static_cast<T>(i + 1) * metal, static_cast<T>(1));

            // Log-map t into pool index
            T d = static_cast<T>(minD)
                  * std::pow(static_cast<T>(maxD) / static_cast<T>(minD), t);
            int target = static_cast<int>(d);

            // Find nearest unused prime in pool via linear scan from target
            // Binary search to find starting position
            int lo = 0, hi = (int) pool.size() - 1;
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                if (pool[mid] < target) lo = mid + 1;
                else hi = mid;
            }

            // Scan outward from lo to find nearest unused
            int found = -1;
            for (int delta = 0; delta < (int) pool.size(); delta++) {
                int a = lo + delta;
                int b = lo - delta;
                if (a < (int) pool.size() && !used[a]) {
                    found = a;
                    break;
                }
                if (b >= 0 && !used[b]) {
                    found = b;
                    break;
                }
            }

            assert(found >= 0);
            used[found] = true;
            outDelays[i] = pool[found];
        }

        std::sort(outDelays, outDelays + N);
    }

    template<int N>
    void computeModulationSamples(MYFLOAT sampleRate,
                                  MYFLOAT minCps,
                                  MYFLOAT maxCps,
                                  int modSamples[N]) {
        // Convert cps → period in samples
        // minCps → longest period, maxCps → shortest period
        const MYFLOAT maxD = sampleRate / minCps;
        const MYFLOAT minD = sampleRate / maxCps;

        // Golden ratio low-discrepancy sequence over [minD, maxD]
        // Guarantees no two periods have near-rational ratios
        constexpr MYFLOAT kPhi = static_cast<MYFLOAT>(1.6180339887498948);

        for (int i = 0; i < N; i++) {
            MYFLOAT t = std::fmod(static_cast<MYFLOAT>(i + 1) * kPhi,
                                  static_cast<MYFLOAT>(1));
            MYFLOAT d = minD + t * (maxD - minD);
            modSamples[i] = nearestPrime(static_cast<int>(d));
        }

        // Resolve duplicates by nudging to next prime
        for (int i = 0; i < N; i++) {
            bool unique;
            do {
                unique = true;
                for (int j = 0; j < i; j++) {
                    if (modSamples[j] == modSamples[i]) {
                        modSamples[i] = nearestPrime(modSamples[i] + 1);
                        unique = false;
                        break;
                    }
                }
            } while (!unique);
        }

        // Sort ascending: slowest LFO (largest period) at index 0 → shortest delay
        //                 fastest LFO (smallest period) at index N-1 → longest delay
        std::sort(modSamples, modSamples + N);
        std::reverse(modSamples, modSamples + N); // descending: slow→fast
    }

    // Same approach as your delay generation — phi-spaced, short range
// 0.5ms to 5ms at 48kHz = 24 to 240 samples
    template<int N>
    void computeFeedbackAllpassDelays(MYFLOAT sampleRate, int outDelays[N]) {
        constexpr MYFLOAT kPhi = static_cast<MYFLOAT>(1.6180339887498948);

        const int minD = static_cast<int>(static_cast<MYFLOAT>(0.5)
                                          * sampleRate / static_cast<MYFLOAT>(1000));
        const int maxD = static_cast<int>(static_cast<MYFLOAT>(5.0)
                                          * sampleRate / static_cast<MYFLOAT>(1000));

        for (int i = 0; i < N; i++) {
            MYFLOAT t = std::fmod(static_cast<MYFLOAT>(i + 1) * kPhi,
                                  static_cast<MYFLOAT>(1));
            outDelays[i] = static_cast<int>(minD + t * (maxD - minD));
        }

        // Deduplicate
        std::sort(outDelays, outDelays + N);
        for (int i = 1; i < N; i++)
            if (outDelays[i] <= outDelays[i - 1])
                outDelays[i] = outDelays[i - 1] + 1;
    }
// ---------------------------------------------------------------------------
// Single Schroeder allpass:
//   y[n] = -g * x[n] + x[n-D] + g * y[n-D]
//
// Lossless for |g| < 1. Flat magnitude response, non-linear phase.
// Use for diffusion, not tone shaping.
// ---------------------------------------------------------------------------

// MaxDelaySamples: static buffer size. 4096 covers ~85ms @ 48kHz.
    template<int MaxDelaySamples = 4096>
    struct Allpass {
        MYFLOAT buf[MaxDelaySamples];
        int pos = 0;
        int D = 0;
        MYFLOAT g = static_cast<MYFLOAT>(0.625);

        Allpass() { clear(); }

        // delayMs: delay in milliseconds
        void init(MYFLOAT delayMs, MYFLOAT sampleRate, MYFLOAT gain = static_cast<MYFLOAT>(0.625)) {
            g = gain;
            D = static_cast<int>(delayMs * sampleRate / static_cast<MYFLOAT>(1000));
            D = std::max(1, std::min(D, MaxDelaySamples - 1));
        }

        // delaySamples: delay in samples directly
        void initSamples(int delaySamples, MYFLOAT gain = static_cast<MYFLOAT>(0.625)) {
            g = gain;
            D = std::max(1, std::min(delaySamples, MaxDelaySamples - 1));
        }

        inline MYFLOAT tick(MYFLOAT x) {
            const MYFLOAT delayed = buf[(pos - D + MaxDelaySamples) & (MaxDelaySamples - 1)];
            const MYFLOAT out = -g * x + delayed;
            buf[pos] = x + g * delayed;
            pos = (pos + 1) & (MaxDelaySamples - 1);
            return out;
        }

        void clear() {
            std::memset(buf, 0, sizeof(buf));
            pos = 0;
        }
    };

// MYFLOAT must be defined before including this header:
//   using MYFLOAT = float;   or   using MYFLOAT = double;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------


// In-place unnormalized fast Walsh-Hadamard transform (power-of-2 N only)
    template<int N>
    inline void fwht(MYFLOAT *x) {
        static_assert((N & (N - 1)) == 0, "N must be power of 2");
        for (int stride = 1; stride < N; stride <<= 1) {
            for (int i = 0; i < N; i += stride << 1) {
                for (int j = i; j < i + stride; ++j) {
                    MYFLOAT a = x[j], b = x[j + stride];
                    x[j] = a + b;
                    x[j + stride] = a - b;
                }
            }
        }
    }

// Scale factor to make Hadamard orthonormal: 1/sqrt(N)
    template<int N>
    inline constexpr MYFLOAT hadamardScale() {
        return static_cast<MYFLOAT>(1.0 / std::sqrt(static_cast<MYFLOAT>(N)));
    }

// Compute feedback gain for a single delay line so that it reaches -60 dB
// after rt60 seconds.
//   g = 10^(-3 * delaySamples / (rt60 * sampleRate))
    inline MYFLOAT feedbackGain(int delaySamples, MYFLOAT rt60, MYFLOAT sampleRate) {
        return static_cast<MYFLOAT>(
                std::pow(10.0, -3.0 * delaySamples /
                               (static_cast<double>(rt60) * static_cast<double>(sampleRate)))
        );
    }

// ---------------------------------------------------------------------------
// Single circular delay line
// ---------------------------------------------------------------------------
    struct DelayLine {
        DelayLine(int size = 4096) {
            MaxLen = size;
            buf.resize(MaxLen + 2, 0);
        }

        std::vector<MYFLOAT> buf;
        int MaxLen;
        int pos = 0;

        inline void write(MYFLOAT v) {
            buf[pos] = v;
            if (++pos == MaxLen) pos = 0;
        }

        inline MYFLOAT readInterpol(MYFLOAT delaySamples) const {
            int dInt = static_cast<int>(delaySamples);
            MYFLOAT frac = delaySamples - dInt;

            int rp0 = pos - dInt;
            if (rp0 < 0) rp0 += MaxLen;

            int rp1 = rp0 + 1;
            if (rp1 >= MaxLen) rp1 -= MaxLen;

            MYFLOAT s0 = buf[rp0];
            MYFLOAT s1 = buf[rp1];

            return s0 + frac * (s1 - s0);
        }

        inline MYFLOAT read(int delaySamples) const {
            int rp = pos - delaySamples;
            if (rp < 0) rp += MaxLen;
            return buf[rp];

        }

        void clear() {
            std::fill(buf.begin(), buf.begin() + buf.size(), 0);
        }
    };

// ---------------------------------------------------------------------------
// First-order lowpass (one-pole) for air absorption inside feedback path.
// Processes in-place.
// ---------------------------------------------------------------------------
    struct Lowpass1 {
        MYFLOAT state = static_cast<MYFLOAT>(0);
        MYFLOAT coeff = static_cast<MYFLOAT>(0); // [0,1): higher = darker

        // cutoffHz: frequency in Hz, sampleRate: sample rate in Hz
        void setCoeff(MYFLOAT cutoffHz, MYFLOAT sampleRate) {
            // Bilinear-adjacent one-pole: c = exp(-2pi*fc/fs)
            // Then H(z) = (1-c) / (1 - c*z^-1)
            const MYFLOAT w = static_cast<MYFLOAT>(2.0 * PI_P) * cutoffHz / sampleRate;
            coeff = std::exp(-w);
        }

        inline MYFLOAT process(MYFLOAT x) {
            state = (static_cast<MYFLOAT>(1) - coeff) * x + coeff * state;
            return state;
        }

        void clear() { state = static_cast<MYFLOAT>(0); }
    };

    // First order allpass — phase dispersion only, no internal feedback loop
// H(z) = (c + z^-1) / (1 + c*z^-1)   |c| < 1
    struct FeedforwardAllpass {
        MYFLOAT state = static_cast<MYFLOAT>(0);
        MYFLOAT c = static_cast<MYFLOAT>(0.3);

        void init(MYFLOAT coeff) { c = coeff; }

        inline MYFLOAT tick(MYFLOAT x) {
            MYFLOAT y = c * x + state;
            state = x - c * y;
            return y;
        }

        void clear() { state = static_cast<MYFLOAT>(0); }
    };

    struct VelvetTail {
        static constexpr int kGridSize = 20;   // one tap per 20 samples
        static constexpr int kMaxTaps = 5000; // 2s at 48kHz / 20

        struct Tap {
            int delay;  // sample offset
            MYFLOAT gain;   // ±1 * distance attenuation
        };

        Tap taps_[kMaxTaps];
        int numTaps_ = 0;
        MYFLOAT buf_[96000 * 2] = {}; // 2s at 48kHz
        int writePos_ = 0;
        int bufSize_ = 0;

        void init(MYFLOAT rt60, MYFLOAT sampleRate) {
            bufSize_ = static_cast<int>(rt60 * sampleRate);
            numTaps_ = bufSize_ / kGridSize;

            // xorshift RNG for deterministic tap positions
            uint32_t rng = 0xdeadbeef;
            auto rand = [&]() -> uint32_t {
                rng ^= rng << 13;
                rng ^= rng >> 17;
                rng ^= rng << 5;
                return rng;
            };

            int sign = 1;
            for (int i = 0; i < numTaps_; i++) {
                // One tap per grid cell, random position within cell
                int offset = rand() % kGridSize;
                taps_[i].delay = i * kGridSize + offset + 1;

                // Gain: decays with distance, alternating sign
                MYFLOAT t = static_cast<MYFLOAT>(i) / numTaps_;
                taps_[i].gain = sign
                                * std::pow(static_cast<MYFLOAT>(10),
                                           static_cast<MYFLOAT>(-3) * t * rt60
                                           / rt60);
                sign = -sign;
            }
        }

        inline MYFLOAT tick(MYFLOAT input) {
            buf_[writePos_] = input;

            MYFLOAT out = static_cast<MYFLOAT>(0);
            for (int i = 0; i < numTaps_; i++) {
                int rp = writePos_ - taps_[i].delay;
                if (rp < 0) rp += bufSize_;
                out += buf_[rp] * taps_[i].gain;
            }

            writePos_ = (writePos_ + 1) % bufSize_;
            return out;
        }
    };

    struct AllpassLine {
        std::vector<MYFLOAT> buf;
        int pos = 0;
        int D = 0;
        int bufSize = 0;
        MYFLOAT g = static_cast<MYFLOAT>(0.5);

        void init(int delaySamples, MYFLOAT coeff) {
            g = coeff;
            D = delaySamples;
            bufSize = delaySamples + 2;
            buf.assign(bufSize, static_cast<MYFLOAT>(0));
            pos = 0;
        }

        // Transposed direct form II — no sample-to-sample feedback
        // w[n] = x[n] + g * w[n-D]
        // y[n] = w[n-D] - g * w[n]
        inline MYFLOAT process(MYFLOAT x) {
            int rp = pos - D;
            if (rp < 0) rp += bufSize;

            MYFLOAT wD = buf[rp];              // w[n-D]
            MYFLOAT w = x + g * wD;           // w[n]
            MYFLOAT y = wD - g * w;           // y[n]

            buf[pos] = w;
            if (++pos >= bufSize) pos = 0;

            return y;
        }

        void clear() {
            std::fill(buf.begin(), buf.end(), static_cast<MYFLOAT>(0));
            pos = 0;
        }
    };

} // namespace fdn_detail

// ---------------------------------------------------------------------------
// Generic FDN implementation — parameterised on N (must be power of 2)
// and MaxDelay (maximum delay length in samples across all lines).
//
// Template parameters:
//   N         : number of delay lines
//   MaxDelay  : maximum delay line length in samples (for static allocation)
// ---------------------------------------------------------------------------



template<int N>
class FDN {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static constexpr int sineWaveSize = 4096;
    static constexpr int sineWaveMask = sineWaveSize - 1;
    static constexpr int modulationDepth = 1;

public:
    // -----------------------------------------------------------------------
    // Construction / initialisation
    // -----------------------------------------------------------------------

    FDN() {
        MYFLOAT phaseInc = static_cast<MYFLOAT>(2.0 * PI_P) /
                           static_cast<MYFLOAT>(sineWaveSize);

        for (int i = 0; i < sineWaveSize; i++)
            sineWave[i] = std::sin(i * phaseInc);
    }

    // Call before use.
    //   delayLengths : array of N delay lengths in samples (caller-supplied)
    //   rt60         : reverberation time in seconds
    //   sampleRate   : sample rate in Hz
    //   lpCutoff     : lowpass cutoff Hz for air absorption (per feedback path)
    void init(const int delayLengths[N],
              MYFLOAT rt60,
              MYFLOAT sampleRate,
              MYFLOAT lpCutoff = static_cast<MYFLOAT>(8000)) {
        sampleRate_ = sampleRate;
        rt60_ = rt60;
        velvetTail.init(.2, sampleRate);
// In init(), replace old rot init block:
// In init:

        MYFLOAT apCoeffs_[N];

// In init:
        constexpr MYFLOAT kPhi = static_cast<MYFLOAT>(1.6180339887498948);
        for (int i = 0; i < N; i++) {
            lengths_[i] = delayLengths[i];
            MYFLOAT t = std::fmod(static_cast<MYFLOAT>(i + 1) * kPhi,
                                  static_cast<MYFLOAT>(1));
            apCoeffs_[i] = static_cast<MYFLOAT>(0.2)
                           + t * static_cast<MYFLOAT>(0.35);
            apLines_[i].init(delayLengths[i], apCoeffs_[i]);
            MYFLOAT effectiveDelay = allpassCenterDelay(delayLengths[i], apCoeffs_[i]);
            apGains_[i] = fdn_detail::feedbackGain(
                    static_cast<int>(effectiveDelay), rt60_, sampleRate_);
        }

        for (int i = 0; i < N; i++) {
            lengths_[i] = delayLengths[i];
            lines_[i] = fdn_detail::DelayLine(lengths_[i] + 50);
            gains_[i] = fdn_detail::feedbackGain(lengths_[i], rt60_, sampleRate_);
            lp_[i].setCoeff(lpCutoff, sampleRate);
        }
        globalLp_.setCoeff(lpCutoff, sampleRate);

        scale_ = fdn_detail::hadamardScale<N>();
        fdn_detail::computeModulationSamples<N>(sampleRate, 0.1, 0.3, modSamples);
        // Crossfade period: 200ms
        // In init, replace phase init:
        for (int i = 0; i < numGivensPairs_; i++) {
            // Phase in radians, phi-spaced initial values
            givensPhase_[i] = std::fmod(static_cast<MYFLOAT>(i + 1) * kPhi,
                                        static_cast<MYFLOAT>(1))
                              * static_cast<MYFLOAT>(2.0 * PI_P);

            // Rate: 0.001–0.02 Hz in radians/sample
            MYFLOAT t = std::fmod(static_cast<MYFLOAT>(i + 2) * kPhi,
                                  static_cast<MYFLOAT>(1));
            MYFLOAT freqHz = static_cast<MYFLOAT>(0.001)
                             + t * static_cast<MYFLOAT>(0.019);
            givensInc_[i] = freqHz * static_cast<MYFLOAT>(2.0 * PI_P) / sampleRate_;

            givensS_[i] = std::sin(givensPhase_[i]);
            givensC_[i] = std::cos(givensPhase_[i]);
        }


        clear();

    }

    inline void tickStereoSimpleAp(MYFLOAT input, MYFLOAT &outL, MYFLOAT &outR) {
        MYFLOAT v[N];

        // 1. Process through allpass lines — returns current output
        for (int i = 0; i < N; i++)
            v[i] = apLines_[i].process(feedback_[i]);

        // 2. Hadamard mix
        fdn_detail::fwht<N>(v);
        for (int i = 0; i < N; i++) v[i] *= scale_;

        // 3. Gain
        for (int i = 0; i < N; i++) v[i] *= apGains_[i];

        // 4. Output taps
        outL = outR = static_cast<MYFLOAT>(0);
        int li = 0, ri = 0;
        for (int i = 0; i < N; i += 2) outL += (li++ & 1) ? -v[i] : v[i];
        for (int i = 1; i < N; i += 2) outR += (ri++ & 1) ? -v[i] : v[i];

        const MYFLOAT norm = static_cast<MYFLOAT>(2) / static_cast<MYFLOAT>(N);
        outL *= norm;
        outR *= norm;

        // 5. Store feedback for next tick + inject input
        for (int i = 0; i < N; i++)
            feedback_[i] = v[i] + input;
    }

    // Recompute gains only (e.g. when RT60 or cutoff changes at runtime).
    void setRT60(MYFLOAT rt60) {
        rt60_ = rt60;
        for (int i = 0; i < N; i++)
            gains_[i] = fdn_detail::feedbackGain(lengths_[i], rt60_, sampleRate_);
    }

    void setLPCutoff(MYFLOAT cutoffHz) {
        for (int i = 0; i < N; i++)
            lp_[i].setCoeff(cutoffHz, sampleRate_);
    }

    void clear() {
        for (int i = 0; i < N; i++) {
            lines_[i].clear();
            lp_[i].clear();
        }
    }

    // -----------------------------------------------------------------------
    // Process one sample (mono in, mono out).
    // Input is distributed to all delay lines; output is the sum of even lines.
    // -----------------------------------------------------------------------
    inline MYFLOAT tick(MYFLOAT input) {
        MYFLOAT v[N];
        // 1. Read from delay lines
        for (int i = 0; i < N; i++) {
            /*
            int idx = (phases[i] * sineWaveSize) / modSamples[i];
            idx &= sineWaveMask;

            MYFLOAT mod = sineWave[idx] * 0.5;
*/
            v[i] = lines_[i].read(lengths_[i]);

            //          if (++phases[i] >= modSamples[i])
            //              phases[i] = 0;
        }
        // 2. Unitary Hadamard mix
        fdn_detail::fwht<N>(v);
        for (int i = 0; i < N; i++)
            v[i] *= scale_;

        // 3. Per-line feedback gain, air-absorption LP, input injection
        for (int i = 0; i < N; i++) {
            v[i] = lp_[i].process(v[i] * gains_[i]) + input;
        }

        // 4. Write back
        for (int i = 0; i < N; i++)
            lines_[i].write(v[i]);

        // 5. Sum alternate lines for decorrelated stereo-ready output
        MYFLOAT out = static_cast<MYFLOAT>(0);
        for (int i = 0; i < N; i++)
            out += (i & 1) ? -v[i] : v[i];  // alternating signs kills correlated buildup
        out /= static_cast<MYFLOAT>(N);
        return out; // normalise
    }

    inline void tickStereoSimple(MYFLOAT input, MYFLOAT &outL, MYFLOAT &outR, bool giv = false)
    {
        MYFLOAT v[N];

        // 1. Read
        for (int i = 0; i < N; i++)
            v[i] = lines_[i].read(lengths_[i]);

        // 2. Hadamard + normalize
        fdn_detail::fwht<N>(v);
        for (int i = 0; i < N; i++)
            v[i] *= scale_;

        // 3. Single Givens post-Hadamard — exact sin/cos, no table
        if(giv){
            const int     pi = givensPi_[givensCursor_];
            const int     pj = givensPj_[givensCursor_];
            const MYFLOAT s  = givensS_[givensCursor_];
            const MYFLOAT c  = givensC_[givensCursor_];

            const MYFLOAT a = v[pi];
            const MYFLOAT b = v[pj];
            v[pi] = c * a - s * b;
            v[pj] = s * a + c * b;

            // Advance angle — exact sin/cos guarantees s²+c²=1, no energy drift
            givensPhase_[givensCursor_] += givensInc_[givensCursor_];
            givensS_[givensCursor_] = std::sin(givensPhase_[givensCursor_]);
            givensC_[givensCursor_] = std::cos(givensPhase_[givensCursor_]);

            if (++givensCursor_ >= numGivensPairs_)
                givensCursor_ = 0;
        }

        // 4. Gain + LP + output tap + write
        outL = outR = static_cast<MYFLOAT>(0);
        int li = 0, ri = 0;

        for (int i = 0; i < N; i++)
        {
            v[i] = lp_[i].process(v[i] * gains_[i]);

            if ((i & 1) == 0) outL += (li++ & 1) ? -v[i] :  v[i];
            else              outR += (ri++ & 1) ? -v[i] :  v[i];

            lines_[i].write(v[i] + input);
        }

        const MYFLOAT norm = static_cast<MYFLOAT>(2) / static_cast<MYFLOAT>(N);
        outL *= norm;
        outR *= norm;
    }
    inline void tickStereo(MYFLOAT input, MYFLOAT &outL, MYFLOAT &outR) {

        MYFLOAT v[N];

        // ------------------------------------------------------------------
        // 1. Read with interpolated modulation
        // ------------------------------------------------------------------
        for (int i = 0; i < N; i++) {
            int idx = (phases[i] * sineWaveSize) / modSamples[i];
            idx &= sineWaveMask;

            MYFLOAT mod = sineWave[idx] * 0.5;
            v[i] = lines_[i].read(static_cast<MYFLOAT>(lengths_[i]) + mod);

            if (++phases[i] >= modSamples[i])
                phases[i] = 0;
        }
// 2. Unitary Hadamard mix
        fdn_detail::fwht<N>(v);
        for (int i = 0; i < N; i++)
            v[i] *= scale_;

        // ------------------------------------------------------------------
        // 4. Symmetry breaker
        // ------------------------------------------------------------------
        v[0] = -v[0];

        // ------------------------------------------------------------------
        // 5. Gains + LP + injection
        // ------------------------------------------------------------------
        // One allpass per feedback line, inside the loop
        // Very short delay ~1-3ms, g=0.5
        for (int i = 0; i < N; i++) {
            v[i] *= gains_[i];
            v[i] = fbAllpass_[i].tick(v[i]); // smears modal peaks
            v[i] = lp_[i].process(v[i]);
            v[i] += input;
        }

        // ------------------------------------------------------------------
        // 6. Write back
        // ------------------------------------------------------------------
        for (int i = 0; i < N; i++)
            lines_[i].write(v[i]);

        // ------------------------------------------------------------------
        // 7. Output taps
        // ------------------------------------------------------------------
        outL = outR = static_cast<MYFLOAT>(0);
        int li = 0, ri = 0;
        for (int i = 0; i < N; i += 2) outL += (li++ & 1) ? -v[i] : v[i];
        for (int i = 1; i < N; i += 2) outR += (ri++ & 1) ? -v[i] : v[i];

        const MYFLOAT norm = static_cast<MYFLOAT>(2) / static_cast<MYFLOAT>(N);
        outL *= norm;
        outR *= norm;
    }

private:
    fdn_detail::AllpassLine apLines_[N];
    MYFLOAT feedback_[N] = {};
    MYFLOAT apCoeffs_[N] = {};
    MYFLOAT apGains_[N] = {};
    fdn_detail::DelayLine lines_[N];
    fdn_detail::Lowpass1 lp_[N];
    fdn_detail::Lowpass1 globalLp_;
    fdn_detail::FeedforwardAllpass fbAllpass_[N];
    int lengths_[N] = {};
    int fbApDelays[N];
    MYFLOAT gains_[N] = {};
    MYFLOAT scale_ = static_cast<MYFLOAT>(1);
    MYFLOAT sampleRate_ = static_cast<MYFLOAT>(48000);
    MYFLOAT rt60_ = static_cast<MYFLOAT>(2);
    int modSamples[N];
    int phases[N]{};
    MYFLOAT sineWave[sineWaveSize];
    // Givens rotation pairs
    static constexpr int kMaxPairs = N * (N - 1) / 2;

    int givensPi_[kMaxPairs];
    int givensPj_[kMaxPairs];
    MYFLOAT givensPhase_[kMaxPairs];
    MYFLOAT givensInc_[kMaxPairs];
    MYFLOAT givensS_[kMaxPairs];
    MYFLOAT givensC_[kMaxPairs];
    int numGivensPairs_ = 0;
    int givensCursor_ = 0;
    fdn_detail::VelvetTail velvetTail;

    // Center group delay of allpass at ω=π/2 (mid-frequency)
// τ_center ≈ D (approximately, for moderate g)
// Exact: τ = D*(1-g²)/(1+g²) at ω=π/2
    inline MYFLOAT allpassCenterDelay(int D, MYFLOAT g) {
        return static_cast<MYFLOAT>(D)
               * (static_cast<MYFLOAT>(1) - g * g)
               / (static_cast<MYFLOAT>(1) + g * g);
    }

};

// ---------------------------------------------------------------------------
// Ready-made typedefs with built-in prime delay tables
// ---------------------------------------------------------------------------

// Primes for N=16, range ~15ms–80ms @ 48kHz (719–3833 samples)
namespace fdn_primes {

    inline constexpr int delays16[16] = {
            719, 773, 839, 907, 983,
            1061, 1151, 1237, 1327, 1429,
            1543, 1657, 1789, 1931, 2083,
            2239
    };

// Primes for N=64, range ~5ms–120ms @ 48kHz (241–5779 samples)
// Log-spaced to avoid harmonic clumping
    inline constexpr int delays64[64] = {
            241, 251, 269, 283, 307, 331, 353, 379,
            409, 433, 461, 491, 523, 557, 593, 631,
            673, 719, 769, 821, 877, 937, 1009, 1087,
            1163, 1249, 1327, 1423, 1523, 1627, 1741, 1861,
            1993, 2131, 2281, 2441, 2609, 2789, 2971, 3181,
            3389, 3613, 3851, 4111, 4397, 4691, 5003, 5347,
            5693, 5741, 5749, 5779, 4027, 4049, 4057, 4073,
            3299, 3307, 3313, 3319, 2683, 2687, 2689, 2693
    };

} // namespace fdn_primes

// Concrete types — MaxDelay sized to the largest prime in each table + margin
using FDN16 = FDN<16>;
using FDN64 = FDN<64>;
// ---------------------------------------------------------------------------
// Convenience factory functions
// ---------------------------------------------------------------------------

inline FDN16 makeFDN16(MYFLOAT rt60 = static_cast<MYFLOAT>(2),
                       MYFLOAT sampleRate = static_cast<MYFLOAT>(48000),
                       MYFLOAT lpCutoff = static_cast<MYFLOAT>(8000)) {
    FDN16 fdn;
    fdn.init(fdn_primes::delays16, rt60, sampleRate, lpCutoff);
    return fdn;
}


inline FDN64 makeFDN64(MYFLOAT rt60 = static_cast<MYFLOAT>(2),
                       MYFLOAT sampleRate = static_cast<MYFLOAT>(48000),
                       MYFLOAT lpCutoff = static_cast<MYFLOAT>(8000)) {
    FDN64 fdn;
    fdn.init(fdn_primes::delays64, rt60, sampleRate, lpCutoff);
    return fdn;
}


template<int N>
inline FDN<N> makeFDN(MYFLOAT rt60 = static_cast<MYFLOAT>(2),
                      MYFLOAT sampleRate = static_cast<MYFLOAT>(48000),
                      MYFLOAT lpCutoff = static_cast<MYFLOAT>(20000),
                      MYFLOAT roomMin = 0.5,
                      MYFLOAT roomMax = 2.0) {
    int delays[N];
    fdn_detail::computeFDNDelays<N>(sampleRate, roomMin, roomMax, delays);
    FDN<N> fdn;
    fdn.init(delays, rt60, sampleRate, lpCutoff);
    return fdn;
}
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

// MYFLOAT must be defined before including this header.

// ---------------------------------------------------------------------------
// Trained + SVD-projected orthogonal feedback matrix
// Source: diff-fdn-colorless, Householder init, 100 epochs, --num 48000
// Delays: [257,271,331,421,431,557,631,647,727,743,877,1009,1033,1061,1181,1223]
// Range:  5.4ms – 25.5ms @ 48kHz
// Orthogonality: machine precision after SVD projection
// ---------------------------------------------------------------------------

static constexpr int kFDN16_N = 16;

static constexpr float kFDN16_A[kFDN16_N][kFDN16_N] = {
        {  0.19137138f,  0.17536086f,  0.01448451f,  0.23224567f, -0.22204849f,  0.40522334f, -0.25984538f,  0.26193005f, -0.29013905f,  0.20640633f,  0.21314985f,  0.25372818f,  0.03479620f, -0.27766612f,  0.41020298f,  0.21370167f },
        {  0.22623415f,  0.32503733f,  0.29664183f, -0.38746509f,  0.36878446f,  0.32581463f, -0.26971641f, -0.23137844f,  0.15030719f, -0.13889748f,  0.24083754f, -0.10033841f, -0.05745704f, -0.15161322f, -0.25992221f,  0.18110906f },
        {  0.04284588f, -0.08822039f,  0.37179148f,  0.25959599f, -0.06889071f, -0.17434137f, -0.51256871f,  0.16094449f, -0.30208993f, -0.30522630f, -0.22598505f, -0.31641245f,  0.10649290f,  0.26108107f, -0.10992914f,  0.18383618f },
        {  0.05743301f, -0.08547027f, -0.08003454f, -0.31907290f, -0.28427714f, -0.23299140f,  0.11584717f, -0.23443782f, -0.33394262f, -0.08915738f,  0.36132085f, -0.23084460f, -0.45405290f,  0.14228494f,  0.23076375f,  0.30778778f },
        {  0.12328979f, -0.06855241f,  0.29520446f,  0.17751876f, -0.30858546f,  0.42050964f,  0.41977516f, -0.11487124f, -0.17591362f,  0.02367868f, -0.29310259f, -0.23687337f, -0.27318883f, -0.22523475f, -0.30798322f, -0.07899865f },
        { -0.00601149f, -0.27924284f, -0.23098657f,  0.24692775f,  0.05565625f,  0.26145926f, -0.00601011f,  0.40909401f,  0.36831483f,  0.08394400f,  0.35547286f, -0.26687661f, -0.22113417f,  0.23459612f, -0.25409395f,  0.24499126f },
        {  0.36934739f,  0.06601661f,  0.08027185f, -0.16438606f, -0.11995447f,  0.04428630f, -0.05026630f, -0.11695720f,  0.29857010f,  0.53759360f, -0.36962655f, -0.18307264f,  0.06137083f,  0.39348850f,  0.25473803f,  0.14635754f },
        { -0.16708297f, -0.13230082f,  0.46421123f, -0.12505230f,  0.15828446f, -0.05924603f,  0.24980718f,  0.14987792f, -0.32070491f,  0.42693719f,  0.24583718f,  0.28818098f,  0.17393148f,  0.27430671f, -0.24585661f,  0.12436017f },
        { -0.54478288f, -0.05716186f,  0.38281545f,  0.10098738f,  0.07027853f,  0.13715719f, -0.23370880f, -0.14132112f,  0.19428279f,  0.14647697f,  0.07152317f, -0.08375804f, -0.38526547f,  0.04501643f,  0.35590404f, -0.31048262f },
        { -0.07257697f, -0.38548127f, -0.10279737f, -0.05510172f,  0.13117892f,  0.05116883f, -0.21212384f, -0.14854194f,  0.02003587f, -0.00367726f, -0.40255362f,  0.50912923f, -0.36097205f, -0.08094400f, -0.11650078f,  0.41658580f },
        { -0.09023546f, -0.59670252f,  0.11090051f, -0.08943848f, -0.04651891f,  0.14494094f,  0.04373267f, -0.25834784f,  0.12411517f, -0.06012104f,  0.11478027f, -0.21496587f,  0.52630568f, -0.28065312f,  0.21846953f,  0.20440359f },
        { -0.56559378f,  0.35728890f, -0.23215146f, -0.09931418f,  0.04867964f,  0.34356925f,  0.16622673f, -0.00243754f, -0.14825794f, -0.10274398f, -0.23966244f, -0.15685992f,  0.18481606f,  0.21371825f,  0.07098322f,  0.37575659f },
        {  0.22220662f, -0.06821352f,  0.01542544f,  0.28184646f,  0.04979721f,  0.31682110f,  0.13807942f, -0.36841637f, -0.01002757f, -0.36961582f,  0.15388958f,  0.31548780f,  0.07222047f,  0.55391294f,  0.14175910f, -0.12877071f },
        { -0.01972063f,  0.11915303f,  0.40326229f, -0.16425258f, -0.31257495f, -0.10904276f,  0.25408059f,  0.36271620f,  0.44406420f, -0.39293915f, -0.03984434f,  0.24321726f, -0.02313042f, -0.00734090f,  0.16912769f,  0.22038889f },
        {  0.13025859f, -0.29647169f, -0.06168555f, -0.50153685f,  0.21678312f,  0.29612261f,  0.00856680f,  0.44665492f, -0.23627131f, -0.17331985f, -0.18106602f, -0.06270783f, -0.07364040f,  0.11516681f,  0.21372277f, -0.34436995f },
        { -0.17511004f, -0.04068301f, -0.12145590f, -0.32021138f, -0.64614940f,  0.18402623f, -0.35993338f, -0.06867755f,  0.05660355f,  0.03749047f,  0.09889673f,  0.16908364f,  0.13918847f,  0.15002124f, -0.35594714f, -0.23063770f },
};

static constexpr float kFDN16_B[kFDN16_N] = {
        0.19105175f, -0.02882057f, -0.64769816f, -0.27396315f,
        0.20737721f,  0.09755663f,  0.06052971f,  0.28939810f,
        0.42710990f, -0.35273683f,  0.41964722f, -0.59099299f,
        0.44160441f, -0.48380956f,  0.51484728f, -0.07557948f
};

static constexpr float kFDN16_C[kFDN16_N] = {
        -0.21103486f, -0.14867105f, -0.06592327f, -0.26020712f,
        -0.21695997f, -0.40185708f, -0.21492596f, -0.20643879f,
        -0.17432469f, -0.30480933f,  0.34179515f, -0.21857582f,
        0.40075305f, -0.28132132f,  0.32909185f, -0.19974211f
};

// ---------------------------------------------------------------------------
// Support structs
// ---------------------------------------------------------------------------

namespace fdn16_detail {

    struct DelayLine {
        std::vector<MYFLOAT> buf;
        int pos    = 0;
        int maxLen = 0;

        void init(int size) {
            maxLen = size;
            buf.assign(size + 2, static_cast<MYFLOAT>(0));
            pos = 0;
        }

        inline void write(MYFLOAT v) {
            buf[pos] = v;
            if (++pos >= maxLen) pos = 0;
        }

        inline MYFLOAT read(int d) const {
            int rp = pos - d;
            if (rp < 0) rp += maxLen;
            return buf[rp];
        }

        inline MYFLOAT readInterp(MYFLOAT d) const {
            int     di   = static_cast<int>(d);
            MYFLOAT frac = d - static_cast<MYFLOAT>(di);
            int     r0   = pos - di; if (r0 < 0) r0 += maxLen;
            int     r1   = r0 - 1;  if (r1 < 0) r1 += maxLen;
            return buf[r0] + frac * (buf[r1] - buf[r0]);
        }

        void clear() {
            std::fill(buf.begin(), buf.end(), static_cast<MYFLOAT>(0));
            pos = 0;
        }
    };

    struct Lowpass1 {
        MYFLOAT state = static_cast<MYFLOAT>(0);
        MYFLOAT coeff = static_cast<MYFLOAT>(0);

        void setCoeff(MYFLOAT cutoffHz, MYFLOAT sampleRate) {
            const MYFLOAT w = static_cast<MYFLOAT>(2.0 * PI_P) * cutoffHz / sampleRate;
            coeff = std::exp(-w);
        }

        void setCoeffDirect(MYFLOAT c) { coeff = c; }

        inline MYFLOAT process(MYFLOAT x) {
            state = (static_cast<MYFLOAT>(1) - coeff) * x + coeff * state;
            return state;
        }

        void clear() { state = static_cast<MYFLOAT>(0); }
    };

    inline MYFLOAT feedbackGain(int delaySamples, MYFLOAT rt60, MYFLOAT sampleRate) {
        return static_cast<MYFLOAT>(
                std::pow(10.0, -3.0 * delaySamples /
                               (static_cast<double>(rt60) * static_cast<double>(sampleRate))));
    }

} // namespace fdn16_detail

// ---------------------------------------------------------------------------
// FDN16Trained
//
// Usage:
//   FDN16Trained verb;
//   verb.init(2.0f, 48000.0f);
//   float outL, outR;
//   verb.tick(input, outL, outR);
// ---------------------------------------------------------------------------

class FDN16Trained {
public:
    static constexpr int N = kFDN16_N;

    static constexpr int kDelays[N] = {
            257, 271, 331, 421, 431, 557, 631, 647,
            727, 743, 877, 1009, 1033, 1061, 1181, 1223
    };

    FDN16Trained() = default;

    void init(MYFLOAT rt60,
              MYFLOAT sampleRate,
              MYFLOAT lpCutoff = static_cast<MYFLOAT>(8000))
    {
        sampleRate_ = sampleRate;
        rt60_       = rt60;

        for (int i = 0; i < N; i++) {
            lines_[i].init(kDelays[i] + 2);
            gains_[i] = fdn16_detail::feedbackGain(kDelays[i], rt60, sampleRate);
            lp_[i].setCoeff(lpCutoff, sampleRate);
        }

        clear();
    }

    void setRT60(MYFLOAT rt60) {
        rt60_ = rt60;
        for (int i = 0; i < N; i++)
            gains_[i] = fdn16_detail::feedbackGain(kDelays[i], rt60_, sampleRate_);
    }

    // LF and HF RT60 independently — set equal for flat frequency decay
    void setRT60WithHFDamping(MYFLOAT lfRt60, MYFLOAT hfRt60) {
        rt60_ = lfRt60;
        for (int i = 0; i < N; i++)
            gains_[i] = fdn16_detail::feedbackGain(kDelays[i], lfRt60, sampleRate_);

        MYFLOAT avgDelay = static_cast<MYFLOAT>(0);
        for (int i = 0; i < N; i++) avgDelay += kDelays[i];
        avgDelay /= static_cast<MYFLOAT>(N);

        MYFLOAT numPasses     = hfRt60 * sampleRate_ / avgDelay;
        MYFLOAT lpGainPerPass = std::pow(static_cast<MYFLOAT>(10),
                                         static_cast<MYFLOAT>(-3) / numPasses);
        MYFLOAT c = (static_cast<MYFLOAT>(1) - lpGainPerPass)
                    / (static_cast<MYFLOAT>(1) + lpGainPerPass);
        for (int i = 0; i < N; i++)
            lp_[i].setCoeffDirect(c);
    }

    void setLPCutoff(MYFLOAT cutoffHz) {
        for (int i = 0; i < N; i++)
            lp_[i].setCoeff(cutoffHz, sampleRate_);
    }

    void clear() {
        for (int i = 0; i < N; i++) {
            lines_[i].clear();
            lp_[i].clear();
        }
    }

    inline void tick(MYFLOAT input, MYFLOAT& outL, MYFLOAT& outR)
    {
        MYFLOAT v[N];

        // 1. Read
        for (int i = 0; i < N; i++)
            v[i] = lines_[i].read(kDelays[i]);

        // 2. Trained matrix
        MYFLOAT w[N] = {};
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                w[i] += static_cast<MYFLOAT>(kFDN16_A[i][j]) * v[j];

        // 3. Gain + LP
        for (int i = 0; i < N; i++)
            w[i] = lp_[i].process(w[i] * gains_[i]);

        // 4. Output — uniform alternating signs, not trained C
        outL = outR = static_cast<MYFLOAT>(0);
        int li = 0, ri = 0;
        for (int i = 0; i < N; i += 2) outL += (li++ & 1) ? -w[i] :  w[i];
        for (int i = 1; i < N; i += 2) outR += (ri++ & 1) ? -w[i] :  w[i];

        const MYFLOAT norm = static_cast<MYFLOAT>(2) / static_cast<MYFLOAT>(N);
        outL *= norm;
        outR *= norm;

        // 5. Write — uniform injection, not trained B
        for (int i = 0; i < N; i++)
            lines_[i].write(w[i] + input);
    }

    inline MYFLOAT tickMono(MYFLOAT input) {
        MYFLOAT outL, outR;
        tick(input, outL, outR);
        return (outL + outR) * static_cast<MYFLOAT>(0.5);
    }

private:
    fdn16_detail::DelayLine lines_[N];
    fdn16_detail::Lowpass1  lp_[N];
    MYFLOAT                 gains_[N]   = {};
    MYFLOAT                 sampleRate_ = static_cast<MYFLOAT>(48000);
    MYFLOAT                 rt60_       = static_cast<MYFLOAT>(2);
};

template<int N>
class FDNTrained {
public:
    void init(const float  A[N][N],
              const float  B[N],
              const float  C[N],
              const int    delays[N],
              MYFLOAT      rt60,
              MYFLOAT      sampleRate)
    {
        sampleRate_ = sampleRate;

        for (int i = 0; i < N; i++) {
            delays_[i] = delays[i];  // populate first
            for (int j = 0; j < N; j++)
                A_[i][j] = static_cast<MYFLOAT>(A[i][j]);
            B_[i] = static_cast<MYFLOAT>(B[i]);
            C_[i] = static_cast<MYFLOAT>(C[i]);
            lines_[i].init(delays[i] + 2);
        }

        setRT60(rt60);  // now delays_[] are valid
        clear();
    }
    void setRT60(MYFLOAT rt60) {
        for (int i = 0; i < N; i++)
            gamma_[i] = std::pow(static_cast<MYFLOAT>(10),
                                 static_cast<MYFLOAT>(-3) * delays_[i]
                                 / (rt60 * sampleRate_));
    }

    void clear() {
        for (int i = 0; i < N; i++)
            lines_[i].clear();
    }

    inline void tick(MYFLOAT input, MYFLOAT& outL, MYFLOAT& outR)
    {
        MYFLOAT v[N];

        // 1. Read
        for (int i = 0; i < N; i++)
            v[i] = lines_[i].read(delays_[i]);

        // 2. Matrix multiply: w = A * v
        MYFLOAT w[N] = {};
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                w[i] += A_[i][j] * v[j];

        // 3. Apply per-delay Gamma — this is V*Gamma in their formulation
        for (int i = 0; i < N; i++)
            w[i] *= gamma_[i];

        // 4. Output via trained C
        outL = outR = static_cast<MYFLOAT>(0);
        for (int i = 0; i < N; i++) {
            MYFLOAT out = C_[i] * w[i];
            if (i & 1) outR += out;
            else       outL += out;
        }
        const MYFLOAT norm = static_cast<MYFLOAT>(2) / static_cast<MYFLOAT>(N);
        outL *= norm;
        outR *= norm;

        // 5. Write: feedback + input via trained B
        for (int i = 0; i < N; i++)
            lines_[i].write(w[i] + B_[i] * input);
    }
    inline MYFLOAT tickMono(MYFLOAT input) {
        MYFLOAT outL, outR;
        tick(input, outL, outR);
        return (outL + outR) * static_cast<MYFLOAT>(0.5);
    }

private:
    struct DelayLine {
        std::vector<MYFLOAT> buf;
        int pos = 0, maxLen = 0;

        void init(int size) {
            maxLen = size;
            buf.assign(size + 2, static_cast<MYFLOAT>(0));
            pos = 0;
        }

        inline void write(MYFLOAT v) {
            buf[pos] = v;
            if (++pos >= maxLen) pos = 0;
        }

        inline MYFLOAT read(int d) const {
            int rp = pos - d;
            if (rp < 0) rp += maxLen;
            return buf[rp];
        }

        void clear() {
            std::fill(buf.begin(), buf.end(), static_cast<MYFLOAT>(0));
            pos = 0;
        }
    };



    DelayLine lines_[N];
    MYFLOAT   A_[N][N]       = {};
    MYFLOAT   B_[N]          = {};
    MYFLOAT   C_[N]          = {};
    MYFLOAT   gamma_[N]          = {};
    int       delays_[N]     = {};
    MYFLOAT   sampleRate_    = static_cast<MYFLOAT>(48000);
};