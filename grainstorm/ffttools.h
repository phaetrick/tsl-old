#pragma once
#ifndef __FFT_TOOLS_H_
#define  __FFT_TOOLS_H_

#include <stdint.h>
#include "types.h"


#include <cmath>
#include "tools/aligned_memalloc.h"
#include "defines.h"
#include "setup.h"
#include "tools.h"
#include "logger.h"
#include "random.h"
#include "envelope.h"
#include <cstring>
#include <atomic>
#include <vector>
#include <PocketFFT.h>
#include <RealFft.h>
#include <complex>

#define anglessss(x, y) atan2f(y,x)
#define phasetsl(x) tsl::realmod(x + PI_P, -TWOPI_P) + PI_P
#define princarg(x) phasetsl(x)
#define absc(x, y) sqrt(x * x + y * y)
//#define MIN(x, y)  (x) < (y) ? (x) : (y)
#define SQRT       sqrt
#define SQR(_a)   ((_a)*(_a))

// Spectra are held in the packed layout [DC, Nyquist, re1, im1, re2, im2, ...]
// throughout. RealFft produces that natively, so there is no repack step; see
// RealFft.h for the engine selection and for GS_FFT_BACKEND=0, which puts the
// original pocketfft double path back.
//
// Note the internal precision: with the fast backend the transform runs in
// float32 (measured round-trip error ~3e-7, about -130 dB). Code that needs a
// double transform -- the Convolver's IR design in particular -- should use
// FFTd below instead.
class FFT {
public:
    using Scalar = RealFft::Scalar;

    FFT(int32_t s) : M(s), NYQ(s / 2), engine(s) {
        buf.resize(s, 0);
        spec.resize(s, 0);
    }

    template<typename T, typename T2>
    void forward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)in[i];
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            out[i] = spec[i];
    }

    template<typename T, typename T2>
    void forwardScaled(T in, T2 out) {
        const Scalar sc = (Scalar)1 / (Scalar)M;
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)in[i];
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            out[i] = spec[i] * sc;
    }

    void forward(double* in) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)in[i];
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            in[i] = spec[i];
    }

    void backward(double* in) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (Scalar)in[i];
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            in[i] = buf[i];
    }

    template<typename T, typename T2>
    void backward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (Scalar)in[i];
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i];
    }

    template<typename T, typename T2>
    void backward_unscaled(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (Scalar)in[i];
        engine.backward(spec.data(), buf.data(), (Scalar)1);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i];
    }

    template<typename T, typename T2>
    void forwardPolar(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)in[i];
        engine.forward(buf.data(), spec.data());
        // Slots 0 and 1 are the two purely real bins (DC, Nyquist); the rest
        // are re/im pairs converted to magnitude/phase in place.
        out[0] = ABS(spec[0]);
        out[1] = ABS(spec[1]);
        for (int32_t k = 1; k < NYQ; k++) {
            const Scalar real = spec[k * 2];
            const Scalar imag = spec[k * 2 + 1];
            out[k * 2] = sqrt(real * real + imag * imag);
            out[k * 2 + 1] = atan2(imag, real);
        }
    }

    template<typename T, typename T2>
    void backwardPolar(T in, T2 out) {
        // in may alias out, so read the whole spectrum before writing anything.
        spec[0] = (Scalar)in[0];
        spec[1] = (Scalar)in[1];
        for (int32_t k = 1; k < NYQ; k++) {
            const double m = (double)in[k * 2];
            const double ph = (double)in[k * 2 + 1];
            spec[k * 2] = (Scalar)(m * cos(ph));
            spec[k * 2 + 1] = (Scalar)(m * sin(ph));
        }
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i];
    }

protected:
    int32_t M{}, NYQ{};
    RealFft engine;
    tsl::AlignedVector<Scalar, 64> buf, spec;
};

// Double-precision real FFT, the original pocketfft path. Kept for the places
// that are numerically sensitive rather than hot -- the Convolver's POCS cap
// iteration builds its IR with this.
class FFTd {
public:
    FFTd(int32_t s) : M(s), NYQ(s / 2), engine(s) {
        buf.resize(s, 0);
        spec.resize(s, 0);
    }

    template<typename T, typename T2>
    void forward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (double)in[i];
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            out[i] = spec[i];
    }

    void forward(double* in) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = in[i];
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            in[i] = spec[i];
    }

    void backward(double* in) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = in[i];
        engine.backward(spec.data(), buf.data(), 1. / M);
        for (int32_t i = 0; i < M; i++)
            in[i] = buf[i];
    }

    template<typename T, typename T2>
    void backward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (double)in[i];
        engine.backward(spec.data(), buf.data(), 1. / M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i];
    }

protected:
    int32_t M{}, NYQ{};
    RealFftPocket engine;
    tsl::AlignedVector<double, 64> buf, spec;
};

namespace tsl {
    template<typename T>
    struct complex {
        T r, i;
    };
    template<typename T>
    T realmod(T a, T m) {
        return (a - m * floor(a / m));
    }
}

namespace tsl {
    namespace fft {
        template<typename T>
        void fft_get_norm(const T* in, T* out, uint32_t s) {
            int32_t i = 0;
            out[0] = abs(in[0]);
            out[1] = abs(in[0]);
            for (i = 1; i < s >> 1u; i++) {
                out[i * 2] = SQRT(SQR(in[i * 2]) + SQR(in[i * 2 + 1]));
                out[i * 2 + 1] = 0.0;
            }
        }


        template<typename T>
        void fft_get_magnitude_log(const T* in, T* out, int32_t s);

        template<typename T>
        int* compute_warped_lookup_table(int32_t* table, uint32_t fft_size, T warping_coef) {
            auto s = fft_size >> 1u;
            for (int32_t i = 0; i < s; i++) {
                int32_t tmp = (uint32_t)floor(std::min((T)i / warping_coef, (T)(s - 1))) << 1u;
                table[i * 2] = tmp;
                table[i * 2 + 1] = tmp + 1;
                // table[s2 - i*2] = table[i*2];
                // table[s2 - i*2+1] = table[i*2+1];
            }
            return table;
        }


        // Raised-cosine lifter edge width (quefrency samples).
        static constexpr int32_t CEP_LIFTER_TAPER = 8;

        template<typename T>
        void cepstrum(FFT* fft, T* in, T* out, T* help1, T* help2, int32_t fft_size,
            const uint32_t cut_off, bool doexp, T scale = 2);
        template<typename T>
        void cepstrum2(FFT* fft, T* in, T* out, T* help1, T* help2, int32_t fft_size,
            const uint32_t cut_off, bool doexp) {
            fft->forward(in, help1);
            fft_get_magnitude_log(help1, help2, fft_size);
            fft->backward(help2, out);
            memset(help2, 0, sizeof(T) * fft_size);
            help2[0] = out[0] * .5;
            help2[1] = 0.0f;
            for (int32_t i = 2; i < cut_off; i++) {
                help2[i] = out[i];
            }
            fft->forward(help2, out);
            if (doexp) {
                T* end = out + fft_size;
                while (out < end) {
                    float tmp = exp(2 * (*out));
                    *out++ = tmp;
                }
            }
        }

        template<typename T>
        void mirrorfft(T* in, int32_t s) {
            int32_t end = 4 * s;
            for (int32_t bin = 1; bin < s; bin++) {
                in[end - (bin << 2)] = in[bin << 2];;
                in[end - ((bin << 2) + 1)] = -in[(bin << 2) + 1];;
            }
        }

        template<typename T>
        void fftshift(T* vect, uint32_t size) {
            const uint32_t s = size >> 1u;
            T* stop = vect + s;
            T* head = vect;
            T* tail = stop;
            while (head < stop) {
                T tmp = *head;
                *head++ = *tail;
                *tail++ = tmp;
            }
        }


    }

}



class FFT3 {
public:
    FFT3(MYFLOAT sr, int32_t size, double overlap);
    ~FFT3() = default;

    using Scalar = RealFft::Scalar;

    template<typename T, typename T2>
    void forward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)(in[i] * awin[i]);
        engine.forward(buf.data(), spec.data());
        for (int32_t i = 0; i < M; i++)
            out[i] = spec[i];
    }

    template<typename T, typename T2>
    void backward(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (Scalar)in[i];
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i] * swin[i];
    }

    template<typename T, typename T2>
    void backward_unscaled(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            spec[i] = (Scalar)in[i];
        engine.backward(spec.data(), buf.data(), (Scalar)1);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i] * swin[i];
    }

    template<typename T, typename T2>
    void forwardPolar(T in, T2 out) {
        for (int32_t i = 0; i < M; i++)
            buf[i] = (Scalar)(in[i] * awin[i]);
        engine.forward(buf.data(), spec.data());
        out[0] = ABS(spec[0]);
        out[1] = ABS(spec[1]);
        for (int32_t k = 1; k < NYQ; k++) {
            const Scalar real = spec[k * 2];
            const Scalar imag = spec[k * 2 + 1];
            out[k * 2] = sqrt(real * real + imag * imag);
            out[k * 2 + 1] = atan2(imag, real);
        }
    }

    template<typename T, typename T2>
    void backwardPolar(T in, T2 out) {
        spec[0] = (Scalar)in[0];
        spec[1] = (Scalar)in[1];
        for (int32_t k = 1; k < NYQ; k++) {
            const float m = (float)in[k * 2];
            const float ph = (float)in[k * 2 + 1];
            spec[k * 2] = (Scalar)(m * cos(ph));
            spec[k * 2 + 1] = (Scalar)(m * sin(ph));
        }
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i] * swin[i];
    }

    template<typename T, typename T2>
    void forwardFrequency(T in, T2 out) {
        T angleDif, real, imag, phase;
        double rratio;

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
            buf[i] = (Scalar)(in[i] * awin[i]);
        // The engine already delivers [DC, Nyquist, re, im, ...], which is the
        // layout the loop below expects -- the old manual shift is gone.
        engine.forward(buf.data(), spec.data());

        for (int32_t i = 0 /*,i0=anal,i1=anal+1,oi=oldInPhase*/;
            i < NYQ;
            i++ /*i0+=2,i1+=2, oi++*/) {
            real = spec[i * 2] /* *i0 */;
            imag = spec[i * 2 + 1] /* *i1 */;
            /**i0*/ out[i * 2] = sqrtf(real * real + imag * imag);
            /* phase unwrapping */
            /*if (*i0 == 0.)*/
            if (out[i * 2] < (float)(1.0E-10))
                angleDif = 0.0f;
            else {
                rratio = atan2((double)imag, (double)real);
                angleDif = (phase = (float)rratio) - /**oi*/ oldinphase[i];
                /* *oi */ oldinphase[i] = phase;
            }

            if (angleDif > PI_F_P)
                angleDif = angleDif - TWOPI_F_P;
            if (angleDif < -PI_F_P)
                angleDif = angleDif + TWOPI_F_P;

            /* add in filter center freq.*/
            /* *i1 */ out[i * 2 + 1] = angleDif * RoverTwoPi + ((float)i * Fexact);
        }
    }

    template<typename T, typename T2>
    void backwardFrequency(T in, T2 out) {
        for (int32_t i = 0; i < NYQ; i++) {
            float mag = in[i * 2]; /* *i0; */
            /* RWD variation to keep phase wrapped within +- TWOPI */
            /* this is spread across several frame cycles, as the problem does not
               develop for a while */

            float angledif = TwoPioverR * (/* *i1 */ in[i * 2 + 1] - ((float)i * Fexact));
            float the_phase = /* *(oldOutPhase + i) */ oldoutphase[i] + angledif;
            // if (i == _DATA->bin_index)
            //    the_phase = (float) fmod(the_phase, TWOPI_P);
            /* *(oldOutPhase + i) = the_phase; */
            oldoutphase[i] = the_phase;
            /* *i0 */ spec[i * 2] = (Scalar)((double)mag * cos((double)the_phase));
            /* *i1 */ spec[i * 2 + 1] = (Scalar)((double)mag * sin((double)the_phase));
        }
        // Was backward(buf, buf), which relied on that overload shifting its
        // input in place; the spectrum now has its own buffer, so no aliasing.
        engine.backward(spec.data(), buf.data(), (Scalar)1 / (Scalar)M);
        for (int32_t i = 0; i < M; i++)
            out[i] = buf[i] * swin[i];
    }

    void compute_window() {
        float* analwinhalf;
        float* synwinhalf;
        float sum;
        int32_t halfwinsize;
        halfwinsize = M / 2;
        // IO = (double) overlap;         /* always, no time-scaling possible */

        // _DATA->arate = csound->esr / (MYFLT) overlap;
        // _DATA->fund = csound->esr / (MYFLT) N;
        int32_t MMf = 1 - M % 2;
        /* deal with iinit later on! */

        synwinhalf = swin.data() + halfwinsize;

        /* have to make analysis window to get amp scaling */
        /* so this ~could~ be a local alloc and free...*/
        double HALFPI = PI_P * .5f;
        double dN = (double)M;
        analwinhalf = awin.data() + halfwinsize;

        tsl::envelope::GenerateWindow(awin.data(), M, tsl::envelope::wtHAMMING, 1.0);

        for (int32_t i = 1; i <= halfwinsize; i++) {
            analwinhalf[-i] = analwinhalf[i - MMf];
        }

        // sinc function
        if (MMf) {
            *analwinhalf *= (float)(dN * sin(HALFPI / dN) / (HALFPI));
        }
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf + i) *= (float)(dN * sin((double)(PI_P * (i + 0.5 * MMf) / dN)) /
                (PI_P * (i + 0.5 * MMf)));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf - i) = *(analwinhalf + i - MMf);

        /* get net amp */
        sum = 0.0f;
        for (int32_t i = -halfwinsize; i <= halfwinsize; i++)
            sum += *(analwinhalf + i);
        sum = 2.0f / sum; /* factor of 2 comes in later in trig identity */

        tsl::envelope::GenerateWindow(swin.data(), M, tsl::envelope::wtHAMMING, 1.0);

        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        if (MMf)
            *synwinhalf *= (float)(IO * sin((double)(HALFPI / IO)) / (HALFPI));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf + i) *= (float)((double)IO * sin((double)(PI_P * (i + 0.5 * MMf) / IO)) /
                (PI_P * (i + 0.5 * (double)MMf)));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        /*
                if (!(N & (N - 1L)))
                    sum = csound->GetInverseRealFFTScale(csound, (int32_t) N) / sum;
                else
                    sum = 1.0f / sum;
        */
        sum *= (M / 4);
        for (int32_t i = -halfwinsize; i <= halfwinsize; i++)
            *(synwinhalf + i) *= sum;
    }

    tsl::AlignedVector<float> awin, swin;
private:
    int32_t M{}, NYQ{};
    RealFft engine;
    tsl::AlignedVector<Scalar, 64> buf, spec;
    double IO;
    tsl::AlignedVector<float> oldinphase, oldoutphase;
    float RoverTwoPi, TwoPioverR, Fexact;
};

/*
template<typename T>
class FFT {
public:
    FFT(size_t size) {
        M = size;
        NYQ = size / 2;
        cpx.resize(NYQ + 1, 0);
        shape.push_back(size);
        stridereal.push_back(sizeof(T));
        stridecpx.push_back(
                sizeof(std::complex<T>));
    }

    size_t getSize() { return M; }

    void forward(T *in, T *out) {
        pocketfft::r2c(shape, stridereal, stridecpx, 0, pocketfft::FORWARD, in, cpx.data(),
                       static_cast<T>(1));
        std::memcpy(out, cpx.data(), sizeof(T) * M);
        out[1] = cpx[NYQ].real();
    }

    void backward(T *in, T *out) {
        std::memcpy(cpx.data(), in, sizeof(T) * M);
        cpx[NYQ] = {in[1], 0};
        cpx[0] = {in[0], 0};
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>(1. / M));
    }

    void forwardPolar(T *in, T *out) {
        pocketfft::r2c(shape, stridereal, stridecpx, 0, pocketfft::FORWARD, in, cpx.data(),
                       static_cast<T>(1));
        out[0] = std::abs(cpx[0]);
        out[1] = std::abs(cpx[NYQ]);
        for (int32_t i = 1; i < NYQ; i++) {
            out[i * 2] = std::abs(cpx[i]);
            out[i * 2 + 1] = std::arg(cpx[i]);
        }
    }


    void backwardPolar(T *in, T *out) {
        cpx[0] = std::polar(in[0], static_cast<T>(0));
        cpx[NYQ] = std::polar(in[1], static_cast<T>(0));
        for (int32_t i = 1; i < NYQ; i++) {
            cpx[i] = std::polar(in[i * 2], in[i * 2 + 1]);
        }
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>(1. / M));
    }

    void backwardUnscaled(T *in, T *out) {
        std::memcpy(cpx.data(), in, sizeof(T) * M);
        static_cast<T(&)[2]>(cpx)[M] = static_cast<T(&)[2]>(cpx)[1];
        static_cast<T(&)[2]>(cpx)[1] = 0;
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>(1.));
    }

private:
    pocketfft::shape_t shape;
    pocketfft::stride_t stridereal, stridecpx;
    int32_t M, NYQ;
    tsl::AlignedVector<std::complex<T>, 64> cpx;
};

template<typename T>
class FFT3 {
public:
    FFT3(uint32_t size, int32_t sr, double overlap = 4.) {
        M = size;
        NYQ = size / 2;
        cpx.resize(NYQ + 1, 0);
        shape.push_back(size);
        stridereal.push_back(sizeof(T));
        stridecpx.push_back(
                sizeof(std::complex<T>));
        IO = M / overlap;
        oldinphase.resize(NYQ + 1, (T)0);
        oldoutphase.resize(NYQ + 1, (T)0);
        swin.resize(M + 3, (T)0);
        awin.resize(M + 3, (T)0);
        compute_window();
        RoverTwoPi = (T) sr / (T) TWOPI_P;
        TwoPioverR = (T) TWOPI_P / (T) sr;
        Fexact = (T) sr / (T) M;
    };

    void forward(T *in, T *out) {
        for (int32_t i = 0; i < M; i++)
            in[i] *= awin[i];
        pocketfft::r2c(shape, stridereal, stridecpx, 0, pocketfft::FORWARD, in, cpx.data(),
                       static_cast<T>(1));
        std::memcpy(out, cpx.data(), sizeof(T) * M);
        out[1] = cpx[NYQ].real();
    }


    void backward(T *in, T *out) {
        std::memcpy(cpx.data(), in, sizeof(T) * M);
        cpx[NYQ] = {in[1], 0};
        cpx[0] = {in[0], 0};
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>(1. / M));
        for (int32_t i = 0; i < M; i++)
            out[i] *= swin[i];
    }

    void forwardPolar(T *in, T *out) {
        for (int32_t i = 0; i < M; i++)
            in[i] *= awin[i];
        pocketfft::r2c(shape, stridereal, stridecpx, 0, pocketfft::FORWARD, in, cpx.data(),
                       static_cast<T>(1));
        out[0] = std::abs(cpx[0]);
        out[1] = std::abs(cpx[NYQ]);
        for (int32_t i = 1; i < NYQ; i++) {
            out[i * 2] = std::abs(cpx[i]);
            out[i * 2 + 1] = std::arg(cpx[i]);
        }
    }


    void backwardPolar(T *in, T *out) {
        cpx[0] = std::polar(in[0], static_cast<T>(0));
        cpx[NYQ] = std::polar(in[1], static_cast<T>(0));
        for (int32_t i = 1; i < NYQ; i++) {
            cpx[i] = std::polar(in[i * 2], in[i * 2 + 1]);
        }
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>(1. / M));
        for (int32_t i = 0; i < M; i++)
            out[i] *= swin[i];
    }


    void forwardFrequency(T *in, T *out) {
        for (int32_t i = 0; i < M; i++)
            in[i] *= awin[i];
        pocketfft::r2c(shape, stridereal, stridecpx, 0, pocketfft::FORWARD, in, cpx.data(),
                       static_cast<T>(1));
        for (int32_t i = 0;
             i < NYQ;
             i++) {

            out[i * 2] = std::abs(cpx[i]);
            T angleDif;
            if (out[i * 2] < (T) (1.0E-10))
                angleDif = 0.0;
            else {
                T phase;
                auto rratio = std::arg(cpx[i]);
                angleDif = (phase = rratio) - oldinphase[i];
                oldinphase[i] = phase;
            }

            while (angleDif > PI_P)
                angleDif -= TWOPI_P;
            while (angleDif < -PI_P)
                angleDif += TWOPI_P;

            out[i * 2 + 1] = angleDif * RoverTwoPi + ((T) i * Fexact);
        }
    }

    void backwardFrequency(T *in, T *out) {
        for (int32_t i = 0; i < NYQ; i++) {
            T mag = in[i * 2];
            T angledif = TwoPioverR * (in[i * 2 + 1] - ((T) i * Fexact));
            T the_phase =oldoutphase[i] + angledif;
            oldoutphase[i] = the_phase;
            cpx[i] = std::polar(mag, the_phase);
        }
        pocketfft::c2r(shape, stridecpx, stridereal, 0, pocketfft::BACKWARD, cpx.data(), out,
                       static_cast<T>((T) 1. / (T) M));
        for (int32_t i = 0; i < M; i++)
            out[i] *= swin[i];
    }

    void compute_window() {
        const int32_t halfwinsize = M / 2;
        T *analwinhalf = awin.data() + halfwinsize;
        T *synwinhalf = swin.data() + halfwinsize;
        int32_t MMf = 1 - M % 2;
        T HALFPI = PI_P * .5;
        T dN = (T) M;


        Envelope::GenerateWindow(awin.data(), M, Envelope::wtHAMMING, 1.0);

        for (int32_t i = 1; i <= halfwinsize; i++) {
            analwinhalf[-i] = analwinhalf[i - MMf];
        }
        if (MMf) {
            *analwinhalf *= (T) (dN * sin(HALFPI / dN) / (HALFPI));
        }
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf + i) *= (T) (dN * sin((double) (PI_P * (i + 0.5 * MMf) / dN)) /
                                           (PI_P * (i + 0.5 * MMf)));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(analwinhalf - i) = *(analwinhalf + i - MMf);

        T sum = 0.0;
        for (int32_t i = -halfwinsize; i <= halfwinsize; i++)
            sum += *(analwinhalf + i);
        sum = 2.0 / sum;

        Envelope::GenerateWindow(swin.data(), M, Envelope::wtHAMMING, 1.0);

        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        if (MMf)
            *synwinhalf *= (T) (IO * sin((double) (HALFPI / IO)) / (HALFPI));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf + i) *= (T) ((double) IO *
                                          sin((double) (PI_P * (i + 0.5 * MMf) / IO)) /
                                          (PI_P * (i + 0.5 * (double) MMf)));
        for (int32_t i = 1; i <= halfwinsize; i++)
            *(synwinhalf - i) = *(synwinhalf + i - MMf);

        sum *= (M / 4);
        for (int32_t i = -halfwinsize; i <= halfwinsize; i++)
            *(synwinhalf + i) *= sum;
    }

    tsl::AlignedVector<T, 64> awin, swin;
private:
    T IO;
    tsl::AlignedVector<T, 64> oldinphase, oldoutphase;
    T RoverTwoPi, TwoPioverR, Fexact;
    pocketfft::shape_t shape;
    pocketfft::stride_t stridereal, stridecpx;
    int32_t M, NYQ;
    tsl::AlignedVector<std::complex<T>, 64> cpx;
};
*/

template<typename T>
static inline T mod2Pi(T x) {
    x = fmod(x, TWOPI_P);
    if (x <= -PI_P) {
        return x + TWOPI_P;
    }
    else if (x > PI_P) {
        return x - TWOPI_P;
    }
    else
        return x;
}

void disp2(float* vect, size_t s);

void disp(float* vect, size_t s);

#endif