#pragma once
//
// Created by pr on 24.04.20.
//

#ifndef GRAINSTORM_PV_H
#define GRAINSTORM_PV_H

#include <cstdint>
#include "defines.h"
#include "types.h"
#include "base.h"
#include "envelope.h"
#include "ffttools.h"
#include "tools/CircularBuffer.h"
#include "DelayBase.h"
#include "app.h"

enum pveffect {
    CROSS_POLAR,
    CROSS_FILTER1,
    CROSS_FILTER2,
    CROSS_INTER,
    CROSS_VOCODER,
    CROSS_CONV,
    CROSS_LPC,
    CROSS_TRANSPORT,
    CROSS_STACK,
    CROSS_DUCK,
    NUM_PV_EFFECTS
};

// Peaks CROSS_STACK asks the modulator for. The modulator packs them into
// fft_out as { count, bin0, mag0, bin1, mag1, ... }, ascending in frequency.
#define CROSS_STACK_MAX_PEAKS 16


struct NodePV {
    float mag;
    float phi;
    float psi;
};
#define CHECKFFT(_M) int32_t _index = (int) std::log2(_M); if(track->ffts[channel][_index].get() == nullptr) track->ffts[channel][_index] = std::make_unique<FFT>(_M);auto fft = track->ffts[channel][_index].get();

void dorobot(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void docepstrum(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void random_phase(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void docepstrum2(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void vocode(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void formant_move(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void spec_interpol(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void dephase(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void cross_mag_phase(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void freqwarp(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void dephase_locked(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void dephase_tracked(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void pvosc(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void cross_convolve(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void cross_lpc(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void cross_transport(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void cross_stack(TRACK *track, uint8_t channel, uint32_t fft_size, bool);
void hpss(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void spectral_contrast(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void spectral_snap(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void spectral_resonator(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void spectral_freeze(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);
void cross_duck(TRACK *track, uint8_t channel, uint32_t fft_size, bool);

extern  void(*pv_funcs[])(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed);// = {dephase, dephase_locked, formant_move, random_phase, dorobot, pvosc, freqwarp};
extern  void(*cross_funcs[])(TRACK *track, uint8_t channel, uint32_t fft_size, bool);// = {cross_mag_phase , docepstrum, docepstrum2, spec_interpol, vocode, cross_convolve};




#define SPECTRAL_FILTER_FFTSIZE 2048
#define SPECTRAL_FILTER_OVERLAP 4.0

template<typename T, int M>
class FFT2 {
public:
    FFT2(MYFLOAT sr, T overlap = 4.) : fft(M), NYQ{M/2}{
        IO = M / overlap;
        hop = (int32_t)(M / overlap);
        compute_window();
        Fexact = sr / (MYFLOAT)M;
        // A bin's phase advances by expct*i over one hop even when it sits exactly
        // on its centre frequency, and the deviation left over is what carries the
        // frequency. Both conversions are therefore per hop, not per sample.
        expct = TWOPI_P * (T)hop / (T)M;
        FreqPerRad = sr / (TWOPI_P * (T)hop);
        RadPerFreq = TWOPI_P * (T)hop / sr;
    }

    void forwardFrequency(T *in, T *out){
        /* analysis: The analysis subroutine computes the complex output at
           time n of (N/2 + 1) of the phase vocoder channels.  It operates
           on input samples (n - analWinLen) thru (n + analWinLen) and
           expects to find these in input[(n +- analWinLen) mod ibuflen].
     It expects analWindow to point to the center of a
           symmetric window of length (2 * analWinLen +1).  It is the
           responsibility of the main program to ensure that these values
           are correct!  The results are returned in anal as succesive
           pairs of real and imaginary values for the lowest (N/2 + 1)
           channels.   The subroutines fft and reals together implement
           one efficient FFT call for a real input sequence.  */

        /* for (i = 0; i < N+2; i++)
         *(anal + i) = FL(0.0);  */
        /*initialize*/

        //   csound->RealFFTnp2(csound, anal, N);
        /* conversion: The real and imaginary values in anal are converted to
           magnitude and angle-difference-per-second (assuming an
      intermediate sampling rate of rIn) and are returned in
           anal. */
        /*if (format==PVS_AMP_FREQ) {*/
        for (int32_t i = 0; i < M; i++)
            in[i] *= awin[i];
        fft.forward(in, (T*)mem);

        for (int32_t i = 0 /*,i0=anal,i1=anal+1,oi=oldInPhase*/;
             i < NYQ;
             i++ /*i0+=2,i1+=2, oi++*/)
        {
            const T real = mem[i].r /* *i0 */;
            const T imag = mem[i].i /* *i1 */;
            const T mag = sqrt(real * real + imag * imag);
            /**i0*/ out[i * 2] = mag;

            // Nominal advance of this bin over one hop. Subtracting it before
            // wrapping is what turns the raw phase difference into a deviation.
            // Without it the wrap sees the whole advance, which for a partial at f
            // is 2*pi*f*hop/sr -- so whether a bin lands near the +-pi boundary
            // depends on the signal, and a steady 1000 Hz sine measured 60..630 Hz
            // of frame-to-frame swing on bins that should have read a flat 1000.
            const T om = expct * (T)i;
            T phase, dev;
            /* phase unwrapping */
            /*if (*i0 == 0.)*/
            if (mag < (T)(1.0E-10)) {
                // Silent bin: run its phase on at its own centre frequency, so the
                // first frame after a gap differences against something current
                // rather than against a phase from before the silence.
                phase = oldinphase[i] + om;
                dev = 0.0;
            } else {
                phase = atan2(imag, real);
                dev = seedPhase ? (T)0.0 : princarg(phase - oldinphase[i] - om);
            }
            /* *oi */ oldinphase[i] = princarg(phase);
            // First frame has no previous phase to difference against: start the
            // synthesis side on the analysed phase so the frame passes through
            // clean instead of resynthesising from zero.
            if (seedPhase)
                oldoutphase[i] = princarg(phase - om);

            /* add in filter center freq.*/
            /* *i1 */ out[i * 2 + 1] = (T)i * Fexact + dev * FreqPerRad;
        }
        seedPhase = false;
    }

    void backwardFrequency(T *in, T *out)
    {
        for (int32_t i = 0; i < NYQ; i++)
        {
            T mag = in[i * 2]; /* *i0; */
            /* RWD variation to keep phase wrapped within +- TWOPI */
            /* this is spread across several frame cycles, as the problem does not
               develop for a while */

            T angledif = expct * (T)i +
                         (/* *i1 */ in[i * 2 + 1] - ((T)i * Fexact)) * RadPerFreq;
            // Wrapped every frame, not "spread across several frame cycles": the
            // advance is up to M/2 radians at the top bins, so an accumulator left
            // to run loses phase resolution in minutes.
            T the_phase = princarg(/* *(oldOutPhase + i) */ oldoutphase[i] + angledif);
            /* *(oldOutPhase + i) = the_phase; */
            oldoutphase[i] = the_phase;
            /* *i0 */ mem[i].r = mag * cos(the_phase);
            /* *i1 */ mem[i].i = mag * sin(the_phase);
        }
        mem[0].i = mem[NYQ].r = mem[NYQ].i = 0.0;
        fft.backward((T*)mem, out);
        for (int32_t i = 0; i < M; i++)
            out[i] *= swin[i];
    }

    void compute_window(){
        T *analwinhalf;
        T *synwinhalf;
        T sum;
        int32_t halfwinsize, buflen;
        halfwinsize = M / 2;
        // IO = (double) overlap;         /* always, no time-scaling possible */

        // _DATA->arate = csound->esr / (MYFLT) overlap;
        // _DATA->fund = csound->esr / (MYFLT) N;
        int32_t MMf = 1 - M % 2;
        /* deal with iinit later on! */

        synwinhalf = swin + halfwinsize;

        /* have to make analysis window to get amp scaling */
        /* so this ~could~ be a local alloc and free...*/
        T HALFPI = PI_P * .5;
        T dN = (double)M;
        analwinhalf = awin + halfwinsize;

        // The last argument is alpha -- the width of a flat top, not an amplitude.
        // At 1.0 GenerateWindow fills the whole span with ones, so this used to ask
        // for a Hamming and get a rectangle: a 1 kHz sine leaked over the entire
        // spectrum and never fell below -56 dB.
        // Hann rather than the Csound original's Hamming: Hamming stops at 0.08 and
        // that endpoint step holds its sidelobe rolloff to 6 dB/octave, so the far
        // field floors out around -63 dB. Hann reaches zero, rolls off at 18, and
        // takes the same measurement past -100 dB.
        tsl::envelope::GenerateWindow(awin, M, tsl::envelope::wtHANNING, 0.0);

        for (int32_t i = 1; i <= halfwinsize; i++)
        {
            analwinhalf[-i] = analwinhalf[i - MMf];
        }

        // sinc function
        if (MMf)
        {
            *analwinhalf *= (dN * sin(HALFPI / dN) / (HALFPI));
        }
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf + i) *= (dN * sin(PI_P * (i + 0.5 * MMf) / dN)) / (PI_P * (i + 0.5 * MMf));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf - i) = *(analwinhalf + i - MMf);

        /* get net amp */
        sum = 0.0f;
        for (int32_t i = -halfwinsize; i <= halfwinsize; i++)
            sum += *(analwinhalf + i);
        sum = 2.0f / sum; /* factor of 2 comes in later in trig identity */

        tsl::envelope::GenerateWindow(swin, M, tsl::envelope::wtHANNING, 0.0);

        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        if (MMf)
            *synwinhalf *= (IO * sin(HALFPI / IO) / HALFPI);
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf + i) *= (IO * sin(PI_P * (i + 0.5 * MMf) / IO) /
                                         (PI_P * (i + 0.5 * MMf)));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        /*
                if (!(N & (N - 1L)))
                    sum = csound->GetInverseRealFFTScale(csound, (int32_t) N) / sum;
                else
                    sum = 1.0f / sum;
        */
        sum *= (M / 4);
        for (int i = -halfwinsize; i <= halfwinsize; i++)
            *(synwinhalf + i) *= sum;

        // Exact overlap-add normalisation. The synthesis sinc has zeros at multiples
        // of the hop, so the frames do not sum flat on their own: measured over a
        // hop the gain ran 0.520..0.573 -- 11 dB down on unity with a 0.85 dB ripple
        // at the frame rate. Every output sample is the sum of awin*swin at one hop
        // phase, so dividing each synthesis sample by the sum for its own hop phase
        // makes an unmodified round trip exactly unity, whatever window or overlap.
        for (int32_t j = 0; j < hop; j++) {
            T g = 0.;
            for (int32_t n = j; n < M; n += hop)
                g += awin[n] * swin[n];
            if (g > (T)1.0E-12)
                for (int32_t n = j; n < M; n += hop)
                    swin[n] /= g;
        }
    }
private:
    T IO;
    T Fexact, expct, FreqPerRad, RadPerFreq;
    int NYQ;
    int32_t hop{};
    bool seedPhase{true};
    // GenerateWindow and the mirror loops below both write index M, so these are
    // M+1 long: at M they overran into the next array.
    alignas(64) T oldinphase[M/2+1]{}, oldoutphase[M/2+1]{}, awin[M+1]{}, swin[M+1]{};
    alignas(64) tsl::complex<T> mem[M/2+1]{};
    FFT fft;
};




class SpectralFilter : public Effect, private tsl::CircularBuffer<MYFLOAT> {
public:
    SpectralFilter(TRACK *track, int32_t channel);

    void compute(MYFLOAT *in, int32_t size) override ;

    void onBufferReady(MYFLOAT *buf, int32_t size) override ;

    MYFLOAT internalsr, mpidsr, twopidsr, pidsr;
    FFT2<MYFLOAT, SPECTRAL_FILTER_FFTSIZE> fft;

    std::atomic<MYFLOAT> *lpcuta, *lpcutf, *res, *update{};
    // -1 so the first frame always recomputes: all three of these are 0..1 now, so
    // the old sentinel of 1 was a reachable value and a patch sitting on it at every
    // one of the three would have left the coefficients at zero.
    MYFLOAT oldcuta{-1.}, oldcutf{-1.}, oldres{-1.};
    // What onBufferReady actually filters with. compute() writes these per sample
    // from the knob plus its LFO and follower, and the frame picks up whatever is
    // current when the hop lands -- the filter only runs at the 93.75 Hz frame rate,
    // so there is nothing finer for a modulator to reach here.
    MYFLOAT magNow{}, phaseNow{};
    alignas(64) MYFLOAT yl1a[SPECTRAL_FILTER_FFTSIZE / 2]{}, yl2a[SPECTRAL_FILTER_FFTSIZE / 2]{};
    alignas(64) MYFLOAT yl1f[SPECTRAL_FILTER_FFTSIZE / 2]{}, yl2f[SPECTRAL_FILTER_FFTSIZE / 2]{};
    // Last frequency each bin was measured at while it still had real energy in it.
    // See onBufferReady: this is what a bin falls back on once MAG LP is holding its
    // level up past the point where the input still supports it.
    alignas(64) MYFLOAT fhold[SPECTRAL_FILTER_FFTSIZE / 2]{};
    MYFLOAT aa{}, ba{}, ca{};
    MYFLOAT af{}, bf{}, cf{};
    // Coefficients are slewed towards these, so a moving knob does not step a
    // near-unit-radius pole in one frame.
    MYFLOAT aaTarget{}, baTarget{}, afTarget{}, bfTarget{};
    int32_t framesSeen{};
    bool coeffsInit{}, filterInit{};
private:
    std::atomic<MYFLOAT> *_wet, *_dry;
    std::atomic<LFO *> *_lfo_mag, *_lfo_phase;
    delay<MYFLOAT> del;
    // How much of the latency-compensation delay has actually been written yet. Until
    // it has filled, its output is not the dry signal, it is silence -- see compute().
    int64_t dryFill{};
};



#define SPECDELFFTSIZE 2048
#define SPECDELOVERLAP 512


class SpectralDelay2 : public Effect, private  tsl::CircularBuffer<MYFLOAT>{
public:
    SpectralDelay2(TRACK *track_, int32_t channel_);
    ~SpectralDelay2() {
        delete[] delaybuf;
    }

    void onBufferReady(MYFLOAT *in, int32_t size) override ;


    void compute(MYFLOAT *in, int32_t size) override ;

private:
    alignas(64) MYFLOAT _env[TBLSIZE3]{};
    int32_t numffts, numfftsm1;
    MYFLOAT *delaybuf;
    int32_t delays[SPECDELFFTSIZE / 2]{};
    FFT fft;
    std::atomic<bool> changed{};
    std::vector<MYFLOAT *> fftbuf;
    alignas(64) MYFLOAT win[SPECDELFFTSIZE]{};
    int32_t writepos;
    std::atomic<MYFLOAT> *update, *del, *fb, *rnd;
    delay<MYFLOAT> delay;
    uint32_t xatfreq[SPECDELFFTSIZE/2]{};
};





#endif //GRAINSTORM_PV_H
