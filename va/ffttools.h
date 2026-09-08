#ifndef __FFT_TOOLS_H_
#define  __FFT_TOOLS_H_

#include <stdint.h>
#include "types.h"

#define M_PI_F 3.141593f
#define M_2PI_F 6.283185f
#define anglessss(x, y) atan2f(y,x)
#define phase(x) realmodf(x + M_PI_F, -M_2PI_F) + M_PI_F
#define princarg(x) phase(x)
#define absc(x, y) sqrtf(x * x + y * y)
//#define MIN(x, y)  (x) < (y) ? (x) : (y)
#define SQRT       sqrtf
#define SQR(_a)   ((_a)*(_a))

//#include <SuperpoweredFFT.h>
#include <cmath>
#include "tools/aligned_memalloc.h"
#include "defines.h"
#include "setup.h"
#include "tools.h"
#include <logger.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <PocketFFT.h>


struct complex {
    float real;
    float imag;
};

class FFT {
public:
    FFT(int s) : M(s), NYQ(s/2) {
        plan = make_rfft_plan(s);
        buf.resize(s, 0);
    }

    ~FFT() {
        destroy_rfft_plan(plan);
    }

    template<typename T, typename T2>
    void forward(T in, T2 out) {
        for(int i=0;i<M;i++)
            buf[i] = in[i];
        rfft_forward(plan, buf.data(), 1.);
        out[0] = buf[0];
        out[1] = buf[M-1];
        for(int i=2;i<M;i++)
            out[i] = buf[i-1];
    }

    void forward(double *in){
        rfft_forward(plan, in, 1.);
        auto tmp = in[M-1];
        std::memmove(in+2, in+1, (M-2)*sizeof(double));
        in[1] = tmp;
    }
    void backward(double *in){
        auto tmp = in[1];
        std::memmove(in+1, in+2, (M-2)*sizeof(double));
        in[M-1] = tmp;
        rfft_backward(plan, in, 1./M);
    }

    template<typename T, typename T2>
    void backward(T in, T2 out) {
        buf[0] = in[0];
        buf[M-1] = in[1];
        for(int i=1;i<M-1;i++)
            buf[i] = in[i+1];
        rfft_backward(plan, buf.data(), 1./M);
        for(int i=0;i<M;i++)
            out[i] = buf[i];
    }

    template<typename T, typename T2>
    void backward_unscaled(T in, T2 out) {
        buf[0] = in[0];
        buf[M-1] = in[1];
        for(int i=1;i<M-1;i++)
            buf[i] = in[i+1];
        rfft_backward(plan, buf.data(), 1.);
        for(int i=0;i<M;i++)
            out[i] = buf[i];

    }
    template<typename T, typename T2>
    void forwardPolar(T in, T2 out) {
        for(int i=0;i<M;i++)
            buf[i] = in[i];
        rfft_forward(plan, buf.data(), 1.);
        auto end = out + M;
        *out++ = ABS(buf[0]);
        *out++ = ABS(buf[M-1]);
        auto b = buf.data()+1;
        while (out < end) {
            auto real = *b++;
            auto imag = *b++;
            *out++ = sqrt(real * real + imag * imag);
            *out++ = atan2(imag, real);
        }
    }

    template<typename T, typename T2>
    void backwardPolar(T in, T2 out) {
        auto end = in + M;
        auto tmp = buf.data();
        *tmp++ = *in++;
        buf[M-1] = *in++;
        while (in < end) {
            float m = *(in++);
            float ph = *(in++);
            *tmp++ = m * cos(ph);
            *tmp++ = m * sin(ph);
        }
        rfft_backward(plan, buf.data(), 1. / M);
        for(int i=0;i<M;i++)
            out[i] = buf[i];
    }

protected:
    int M{}, NYQ{};
    rfft_plan plan{};
    tsl::AlignedVector<double> buf;
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


static inline double mod2Pi(double x) {
    x = fmod(x, TWOPI_P);
    if (x <= -PI_P) {
        return x + TWOPI_P;
    } else if (x > PI_P) {
        return x - TWOPI_P;
    } else
        return x;
}
float realmodf(float a, float m);

double realmod(double a, double m);

void cepstrum(FFT *fft, float *in, float *out, float *help1, float *help2, const uint32_t fft_size,
              const uint32_t cut_off,
              bool doexp);

float *cepstrum2(float *in, float *out, float *help1, float *help2, const uint32_t fft_size,
                 const uint32_t cut_off, bool doexp);

void disp2(float *vect, size_t s);

void disp(float *vect, size_t s);

void fft_get_norm(const float *in, float *out, uint32_t s);

void fft_get_magnitude_log(const float *in, float *out, uint32_t s);

void fft_get_magnitude_log2(const float *in, float *out, uint32_t s);

int *compute_warped_lookup_table(int *table, uint32_t fft_size, float coeff);

void mirrorfft(float *in, size_t s);

void fftshift(float *vect, uint32_t size);

void ifft(float *in, float *out, uint32_t fft_size);

void fft_rect_to_polar(float *in, float *out, uint32_t s);

void fft_psd(float *in, uint32_t s);

void fft_psd_asm(float *in, uint32_t s);

void fft_mag(float *in, float *out, uint32_t s);

void fft_mag_asm(float *in, float *out, uint32_t s);

void fft_maglog(float *in, float *out, uint32_t s);

void fft_maglog_asm(float *in, float *out, uint32_t s);

void fft_testasm(float *in, uint32_t s);

void fft_testnormal(float *in, uint32_t s);

#endif