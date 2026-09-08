#include "ffttools.h"
#include <logger.h>
#include <cstring>
#include "envelope.h"

#define abs(x) fabsf(x)

float realmodf(float a, float m) {
    return (a - m * floorf(a / m));
}

double realmod(double a, double m) {
    return (a - m * floor(a / m));
}

void disp2(float *vect, size_t s) {
    for (int i = 0; i < s; i++) {
        printf("%f %f ", vect[i * 2], vect[i * 2 + 1]);
    }
    printf("\n");

}

float max(float *vect, size_t s) {
    float max = -100.f;
    for (int i = 0; i < s; i++)
        max = max > vect[i * 2] ? max : vect[i * 2];
    return max;
}

float minimum(float *vect, size_t s) {
    float min1 = 1000.f;
    for (int i = 0; i < s; i++)
        min1 = min1 < vect[i * 2] ? min1 : vect[i * 2];
    return min1;
}

void disp(float *vect, size_t s) {
    float max0 = max(vect, s);
    float min0 = minimum(vect, s);
    float max1 = max(vect + 1, s);
    float min1 = minimum(vect + 1, s);
   // LOGE("0: %f %f +1: %f %f\n", min0, max0, min1, max1);
}


void fft_get_norm(const float *in, float *out, uint32_t s) {
    int i = 0;
    out[0] = abs(in[0]);
    out[1] = abs(in[0]);
    for (i = 1; i < s >> 1; i++) {
        out[i * 2] = SQRT(SQR(in[i * 2])
                          + SQR(in[i * 2 + 1]));
        out[i * 2 + 1] = 0.0f;
    }
}

void fft_rect_to_polar(float *in, float *out, uint32_t s) {
    int i = 0;
    out[0] = abs(in[0]);
    out[1] = 0.0f;
    for (i = 1; i < s >> 1; i++) {
        float real = in[i * 2];
        float imag = in[i * 2 + 1];
        out[i * 2] = sqrtf(real * real + imag * imag);
        out[i * 2 + 1] = atan2f(imag, real);
    }
    out[s] =
            abs(in[s]);
    out[s + 1] = 0.0f;
}


void fft_psd_asm(float *in, uint32_t s) {
    float nyq = powf(abs(in[1]), 2);
    in[1] = 0;
    fft_psd(in, s);
    in[1] = nyq;
}


void fft_testasm(float *in, uint32_t s) {
    fft_psd(in, s);
}

void fft_testnormal(float *in, uint32_t s) {
    for (int i = 0; i < s; i += 2) {
        in[i] = powf(sqrt(in[i] * in[i] + in[i + 1] * in[i + 1]), 2);
        in[i + 1] = 0;
    }
}


void fft_psd(float *in, uint32_t s) {
    in[0] = powf(abs(in[0]), 2);
    in[1] = powf(abs(in[1]), 2);
    in[2] = powf(sqrt(in[2] * in[2] + in[3] * in[3]), 2);
    in[3] = 0;
    in[4] = powf(sqrt(in[4] * in[4] + in[5] * in[5]), 2);
    in[5] = 0;
    in[6] = powf(sqrt(in[6] * in[6] + in[7] * in[7]), 2);
    in[7] = 0;
    in[8] = powf(sqrt(in[8] * in[8] + in[9] * in[9]), 2);
    in[9] = 0;
    in[10] = powf(sqrt(in[10] * in[10] + in[11] * in[11]), 2);
    in[11] = 0;
    in[12] = powf(sqrt(in[12] * in[12] + in[13] * in[13]), 2);
    in[13] = 0;
    in[14] = powf(sqrt(in[14] * in[14] + in[15] * in[15]), 2);
    in[15] = 0;
    in[16] = powf(sqrt(in[16] * in[16] + in[17] * in[17]), 2);
    in[17] = 0;
    in[18] = powf(sqrt(in[18] * in[18] + in[19] * in[19]), 2);
    in[19] = 0;
    in[20] = powf(sqrt(in[20] * in[20] + in[21] * in[21]), 2);
    in[21] = 0;
    in[22] = powf(sqrt(in[22] * in[22] + in[23] * in[23]), 2);
    in[23] = 0;
    in[24] = powf(sqrt(in[24] * in[24] + in[25] * in[25]), 2);
    in[25] = 0;
    in[26] = powf(sqrt(in[26] * in[26] + in[27] * in[27]), 2);
    in[27] = 0;
    in[28] = powf(sqrt(in[28] * in[28] + in[29] * in[29]), 2);
    in[29] = 0;
    in[30] = powf(sqrt(in[30] * in[30] + in[31] * in[31]), 2);
    in[31] = 0;

    float *end = in + s;
    in = in + 32;

    for (; in < end; in += 32) {
        in[0] = powf(sqrt(in[0] * in[0] + in[1] * in[1]), 2);
        in[1] = 0;
        in[2] = powf(sqrt(in[2] * in[2] + in[3] * in[3]), 2);
        in[3] = 0;
        in[4] = powf(sqrt(in[4] * in[4] + in[5] * in[5]), 2);
        in[5] = 0;
        in[6] = powf(sqrt(in[6] * in[6] + in[7] * in[7]), 2);
        in[7] = 0;
        in[8] = powf(sqrt(in[8] * in[8] + in[9] * in[9]), 2);
        in[9] = 0;
        in[10] = powf(sqrt(in[10] * in[10] + in[11] * in[11]), 2);
        in[11] = 0;
        in[12] = powf(sqrt(in[12] * in[12] + in[13] * in[13]), 2);
        in[13] = 0;
        in[14] = powf(sqrt(in[14] * in[14] + in[15] * in[15]), 2);
        in[15] = 0;
        in[16] = powf(sqrt(in[16] * in[16] + in[17] * in[17]), 2);
        in[17] = 0;
        in[18] = powf(sqrt(in[18] * in[18] + in[19] * in[19]), 2);
        in[19] = 0;
        in[20] = powf(sqrt(in[20] * in[20] + in[21] * in[21]), 2);
        in[21] = 0;
        in[22] = powf(sqrt(in[22] * in[22] + in[23] * in[23]), 2);
        in[23] = 0;
        in[24] = powf(sqrt(in[24] * in[24] + in[25] * in[25]), 2);
        in[25] = 0;
        in[26] = powf(sqrt(in[26] * in[26] + in[27] * in[27]), 2);
        in[27] = 0;
        in[28] = powf(sqrt(in[28] * in[28] + in[29] * in[29]), 2);
        in[29] = 0;
        in[30] = powf(sqrt(in[30] * in[30] + in[31] * in[31]), 2);
        in[31] = 0;
    }
}


void fft_mag_asm(float *in, float *out, uint32_t s) {
    float nyq = in[1];
    in[1] = 0;
    fft_mag(in, out, s);
    in[1] = nyq;
    out[1] = abs(nyq);
}


void fft_mag(float *in, float *out, uint32_t s) {
    out[0] = abs(in[0]);
    out[1] = abs(in[1]);
    float *end = out + s;
    out = out + 2;
    in = in + 2;
    while (out < end) {
        float real = *(in++);
        float imag = *(in++);
        *(out++) = sqrtf(real * real + imag * imag);
        *(out++) = 0.0f;
    }
}

void fft_maglog_asm(float *in, float *out, uint32_t s) {
    float nyq = in[1];
    in[1] = 0;
    fft_mag_asm(in, out, s);
    in[1] = nyq;
    out[1] = logf(0.00001f + abs(nyq));
    for (int i = 0; i < s; i += 2) {
        out[i] = logf(0.00001f + out[i]);
    }
}


void fft_maglog(float *in, float *out, uint32_t s) {
    int i = 0;
    out[0] = logf(0.00001f + abs(in[0]));
    out[1] = logf(0.00001f + abs(in[1]));
    for (i = 1; i < (s >> 1); i++) {
        out[i * 2] = logf(0.00001f + SQRT(SQR(in[i * 2])
                                          + SQR(in[i * 2 + 1])));
        out[i * 2 + 1] = 0.0f;
    }
}


void fft_get_magnitude_log(const float *in, float *out, uint32_t s) {
    out[0] = logf(0.00001f + abs(in[0]));
    out[1] = logf(0.00001f + abs(in[1]));
    float *end = out + s;
    out = out + 2;
    in = in + 2;
    while (out < end) {
        float real = *(in++);
        float imag = *(in++);
        *(out++) = logf(0.00001f + sqrtf(real * real + imag * imag));
        *(out++) = 0.0f;
    }
}


int *compute_warped_lookup_table(int *table, uint32_t fft_size, float warping_coef) {
    uint32_t s = fft_size >> 1u;
    for (int i = 0; i < s; i++) {
        uint32_t tmp = (uint32_t) floor(MIN(i / warping_coef, (float) (s - 1))) << 1u;
        table[i * 2] = tmp;
        table[i * 2 + 1] = tmp + 1;
        //table[s2 - i*2] = table[i*2];
        //table[s2 - i*2+1] = table[i*2+1];
    }
    return table;
}

float *fftshift_complex(float *vect, uint32_t size) {
    float temp[2];
    uint32_t s = size / 2;
    for (int i = 0; i < s; i++) {
        temp[0] = vect[i * 2];
        temp[1] = vect[i * 2 + 1];
        vect[i * 2] = vect[size + i * 2];
        vect[i * 2 + 1] = vect[size + i * 2 + 1];
        vect[size + i * 2] = temp[0];
        vect[size + i * 2 + 1] = temp[1];
    }
    return vect;
}

void fftshift(float *vect, uint32_t size) {
    float tmp;
    uint32_t s = size >> 1u;
    float *stop = vect + s;
    float *head = vect;
    float *tail = stop;
    while (head < stop) {
        tmp = *head;
        *head++ = *tail;
        *tail++ = tmp;
    }
}


void cepstrum(FFT *fft, float *in, float *out, float *help1, float *help2, const uint32_t fft_size,
              const uint32_t cut_off, bool doexp) {
    fft->forward(in, help1);
    fft_get_magnitude_log(help1, help2, fft_size);
    fft->backward(help2, out);
    memset(help2, 0, sizeof(float) * fft_size);
    help2[0] = out[0];
    help2[1] = 0.0f;
    for (int i = 2; i < cut_off; i++) {
        help2[i] = out[i];
    }
    help2[cut_off] = out[cut_off] * .5f;
    help2[cut_off + 1] = out[cut_off + 1] * .5f;

    fft->forward(help2, out);
    if (doexp) {
        float *end = out + fft_size;
        while (out < end) {
            float tmp = expf(2 * (*out));
            *out++ = tmp;
        }
    }
}

void cepstrum2(FFT *fft, float *in, float *out, float *help1, float *help2, const uint32_t fft_size,
               const uint32_t cut_off, bool doexp) {
    fft->forward(in, help1);
    fft_get_magnitude_log(help1, help2, fft_size);
    fft->backward(help2, out);
    memset(help2, 0, sizeof(float) * fft_size);
    help2[0] = out[0] * .5f;
    help2[1] = 0.0f;
    for (int i = 2; i < cut_off; i++) {
        help2[i] = out[i];
    }
    fft->forward(help2, out);
    if (doexp) {
        float *end = out + fft_size;
        while (out < end) {
            float tmp = expf(2 * (*out));
            *out++ = tmp;
        }
    }
}

void mirrorfft(float *in, size_t s) {
    size_t end = 4 * s;
    for (int bin = 1; bin < s; bin++) {
        in[end - (bin << 2)] = in[bin << 2];;
        in[end - ((bin << 2) + 1)] = -in[(bin << 2) + 1];;
    }
}