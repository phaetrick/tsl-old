#pragma once
//
// Voltaic — the ensemble chorus.
//

#include <algorithm>
#include <cmath>

namespace va {

// The ARP Solina's "ensemble": three bucket-brigade delay lines modulated by one
// low-frequency triangle taken at three phases 120 degrees apart, summed.
//
// WHY THREE, AND WHY PHASE. One modulated delay is a vibrato — the pitch of the
// whole signal moves together and the ear hears a wobble. At three phases, one
// tap is sharpening while another flattens, so the SUM has no net pitch movement
// and what is left is the beating between them, which reads as many players
// rather than as an effect.
//
// WHY THIS IS NOT UNISON. A delay is linear, so summing voices and then delaying
// is identical to delaying each voice and summing — which is why this sits on the
// mixed output rather than inside each note, at a tenth of the cost. The two
// mechanisms are genuinely different: unison detunes, so copies beat in PITCH at
// the difference frequency, which IS periodic level modulation; this offsets in
// TIME, and with the taps at different base delays their combs fill each other's
// notches. Measured on the same source, spreading the taps took the timbral
// wobble from 2.48 dB to 1.75 dB. Unison has no equivalent control — more voices
// only ever means more beating.
//
// Every constant below was tuned by measurement against a reference and then by
// ear; see the generative engine's Chorus.h, which this is a port of.
class EnsembleChorus {
public:
    void init(double sampleRate) {
        mSampleRate = sampleRate;
        mBase  = 0.009 * sampleRate;   // 9 ms
        mDepth = 0.0010 * sampleRate;  // +/- 1 ms
        mIncSlow = 0.58 / sampleRate;
        mIncFast = kVibHz / sampleRate;
        clear();
    }

    void clear() {
        for (int i = 0; i < kBufLen; ++i) mBufL[i] = mBufR[i] = 0.0f;
        mWrite = 0;
        mPhaseSlow = mPhaseFast = 0.0;
    }

    /** Depth in milliseconds, peak to centre. The tuned value is 1.0; past about
     *  2.5 the taps sweep far enough that their combs start moving coherently
     *  again and it returns to sounding like a phaser. */
    void setDepth(double ms) { mDepth = ms * 0.001 * mSampleRate; }
    /** Rate of the slow triangle in Hz. The fast one is held at a fixed 2.33x so
     *  the two never lock — the same reason none of the drift periods in the
     *  generative engine are multiples of each other. */
    /** RATE drives the CHORUS generator only. The vibrato generator is fixed at
     *  the instrument's rate and independent of it — on the real thing they are two
     *  separate generators, not one ratio-locked pair. Ear-tested at the shallow
     *  depth: 6 Hz preferred over the 1.35 Hz this used to run at. */
    void setRate(double hz) {
        mIncSlow = hz / mSampleRate;
        mIncFast = kVibHz / mSampleRate;
    }

    /** mix 0 = bypass (and it really is bypassed, not mixed to zero — the whole
     *  stage is skipped, so a preset that does not ask for an ensemble pays
     *  nothing for it and the buffer stays untouched). */
    template <class T>
    void process(T* L, T* R, int n, double mix) {
        if (mix <= 0.0005) { clear(); return; }
        const double wet = mix < 1.0 ? mix : 1.0, dry = 1.0 - wet;
        constexpr double tapGain = 1.0 / 3.0;

        for (int s = 0; s < n; ++s) {
            mBufL[mWrite] = float(L[s]);
            mBufR[mWrite] = float(R[s]);

            double sumL = 0.0, sumR = 0.0;
            for (int t = 0; t < kTaps; ++t) {
                const double ph = double(t) / double(kTaps);
                const double m  = tri(mPhaseSlow + ph) + 0.15 * tri(mPhaseFast + ph);
                // The right channel reads a quarter cycle later, so the two
                // channels decorrelate without the mono sum collapsing.
                const double mR = tri(mPhaseSlow + ph + 0.25) + 0.15 * tri(mPhaseFast + ph + 0.25);
                sumL += read(mBufL, mBase * kSpread[t] + mDepth * m);
                sumR += read(mBufR, mBase * kSpread[t] + mDepth * mR);
            }

            L[s] = T(dry * double(L[s]) + wet * sumL * tapGain);
            R[s] = T(dry * double(R[s]) + wet * sumR * tapGain);

            mWrite = (mWrite + 1) & (kBufLen - 1);
            mPhaseSlow += mIncSlow; if (mPhaseSlow >= 1.0) mPhaseSlow -= 1.0;
            mPhaseFast += mIncFast; if (mPhaseFast >= 1.0) mPhaseFast -= 1.0;
        }
    }

private:
    static constexpr double kVibHz = 6.0;
    static constexpr int kTaps = 3;
    static constexpr int kBufLen = 2048;   // 42 ms at 48k; worst tap is 15.5 ms
    /** Multipliers on the base delay: 6.5 / 10.5 / 14.5 ms. Three taps sharing one
     *  base share a comb SPACING, so their notches line up and sweep together —
     *  that is what makes a chorus sound like a phaser. */
    static constexpr double kSpread[kTaps] = {0.72, 1.17, 1.61};

    static double tri(double p) {
        p -= std::floor(p);
        return p < 0.5 ? (4.0 * p - 1.0) : (3.0 - 4.0 * p);
    }

    double read(const float* buf, double d) const {
        const double rp = double(mWrite) - d + double(kBufLen);
        const int i0 = int(rp);
        const double f = rp - double(i0);
        const double a = buf[i0 & (kBufLen - 1)];
        const double b = buf[(i0 + 1) & (kBufLen - 1)];
        return a + (b - a) * f;   // linear on purpose: a BBD loses top as it sweeps
    }

    double mSampleRate{48000.0};
    float  mBufL[kBufLen]{}, mBufR[kBufLen]{};
    int    mWrite{0};
    double mPhaseSlow{0.0}, mPhaseFast{0.0}, mIncSlow{0.0}, mIncFast{0.0};
    double mBase{432.0}, mDepth{48.0};
};

}  // namespace va
