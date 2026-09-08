#include "ffttools.h"
#include "logger.h"
#include <cstring>
#include "grainstorm.h"
#include "track.h"
#include "envelope.h"
#include <app.h>
template<typename T>
void tsl::fft::fft_get_magnitude_log(const T* in, T* out, int32_t s) {
    // The -4 floor is the WHITENING LIMITER, not just a log-of-zero guard: it
    // caps how far below "loud" an envelope can report, which bounds every
    // exp(-envelope) inverse gain (spec_interpol's g2, docepstrum2's source
    // whitening). Deepening it to log(1e-4) was tried and REVERTED: silent
    // bands whiten up ~e^5 harder, the modulator's noise floor surfaced as
    // broadband hash the moment IPOL left zero. The threshold must be e^-4 so
    // floor == log(threshold): the old 1e-4 threshold mapped magnitudes just
    // above it to logs ~45 dB BELOW the floor, a non-monotonic rail that rang
    // through the cepstrum. Change the pair only together.
    const T LOG_FLOOR = (T) -4.;
    const T MAG_FLOOR = (T) 0.018315638888734179;  // e^-4
    auto dc = std::abs(in[0]);
    out[0] = dc <= MAG_FLOOR ? LOG_FLOOR : log(dc);
    dc = std::abs(in[1]);
    out[1] = dc <= MAG_FLOOR ? LOG_FLOOR : log(dc);
    T* end = out + s;
    out = out + 2;
    in = in + 2;
    auto cpx = reinterpret_cast<const std::complex<T>*>(in);
    while (out < end) {
        auto res = std::abs(*(cpx++));
        if (res > MAG_FLOOR)
            *(out++) = log(res);
        else
            *(out++) = LOG_FLOOR;
        *(out++) = 0.0;
    }

}

template
void tsl::fft::fft_get_magnitude_log<MYFLOAT>(const MYFLOAT* in, MYFLOAT* out, int32_t s);

template<typename T>
void tsl::fft::cepstrum(FFT* fft, T* in, T* out, T* help1, T* help2, int32_t fft_size,
    const uint32_t cut_off, bool doexp, T scale) {
    const int32_t M2 = fft_size >> 1;
    // Full legacy reach on purpose, including past M2 into the mirror half of
    // the even cepstrum (double-weights fine detail): that hyper-detailed
    // envelope at high CUT settings is part of the sound. A true-envelope
    // iteration (Roebel/Rodet) was tried here and REMOVED: it rides the
    // spectral peaks and lifts the valleys between the modulator's formants,
    // which is exactly the contrast the cross algorithms imprint -- it
    // audibly erased the modulator. Do not re-add it.
    int32_t cut = (int32_t) cut_off;
    if (cut > fft_size - CEP_LIFTER_TAPER - 1) cut = fft_size - CEP_LIFTER_TAPER - 1;
    if (cut < 2) cut = 2;

    fft->forward(in, help1);
    fft_get_magnitude_log(help1, help2, fft_size);
    fft->backward(help2, out);
    std::memset(help2, 0, sizeof(T) * fft_size);
    help2[0] = out[0];
    help2[1] = 0.;
    for (int32_t i = 2; i <= cut; i++)
        help2[i] = out[i];
    // Raised-cosine edge instead of the old half-sample step: same reach,
    // less Gibbs ripple in the envelope.
    for (int32_t j = 1; j <= CEP_LIFTER_TAPER; j++)
        help2[cut + j] = out[cut + j] *
            (T) (.5 * (1. + cos(PI_P * (double) j / (double) (CEP_LIFTER_TAPER + 1))));
    fft->forward(help2, out);
    if (doexp) {
        // Envelope values live at slot 0 (DC), slot 1 (Nyquist) and the even
        // slots; the odd slots are the imaginary leakage of the half-cepstrum
        // and are cleared rather than exponentiated.
        out[0] = exp((T) 2. * out[0]);
        out[1] = exp((T) 2. * out[1]);
        for (int32_t i = 1; i < M2; i++) {
            out[i * 2] = exp((T) 2. * out[i * 2]);
            out[i * 2 + 1] = 0.;
        }
    }
}
template
void tsl::fft::cepstrum<MYFLOAT>(FFT* fft, MYFLOAT* in, MYFLOAT* out, MYFLOAT* help1, MYFLOAT* help2, int32_t fft_size,
    const uint32_t cut_off, bool doexp, MYFLOAT scale);

template<typename T>
T max(const T* vect, size_t s) {
    T max = -100.;
    for (int32_t i = 0; i < s; i++)
        max = max > vect[i * 2] ? max : vect[i * 2];
    return max;
}
template<typename T>
T minimum(const T* vect, size_t s) {
    T min1 = 1000.;
    for (int32_t i = 0; i < s; i++)
        min1 = min1 < vect[i * 2] ? min1 : vect[i * 2];
    return min1;
}



FFT3::FFT3(MYFLOAT sr, int32_t size, double overlap) : M(size), NYQ(size / 2), engine(size) {
    IO = M / overlap;
    oldinphase.resize(NYQ + 1);
    oldoutphase.resize(NYQ + 1);
    swin.resize(M + 3);
    awin.resize(M + 3);
    compute_window();
    RoverTwoPi = sr / TWOPI_F_P;
    TwoPioverR = TWOPI_F_P / (float)sr;
    Fexact = sr / (float)M;
    buf.resize(size, 0);
    spec.resize(size, 0);
}









