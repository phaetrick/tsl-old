#pragma once
//
// Real FFT backend for the packed spectrum layout Grainstorm uses everywhere:
//
//     [ DC, Nyquist, re1, im1, re2, im2, ... re(N/2-1), im(N/2-1) ]   (N floats)
//
// Two interchangeable engines, selected by GS_FFT_BACKEND:
//
//   1 (default)  RealFftFast   - pffft (NEON), and on Apple vDSP above
//                                GS_FFT_VDSP_MIN_N where it measured faster.
//                                float32.
//   0            RealFftPocket - the original pocketfft C path, double.
//                                Set GS_FFT_BACKEND=0 to put the old
//                                behaviour back without touching call sites.
//
// Measured on an M4 (round trip, vs the pocketfft path this replaces):
//   N=512 2.7x  1024 2.7x  2048 2.9x  4096 3.0x  8192 3.4x  32768 2.7x for
//   pffft; vDSP pulls ahead only from 8192 up (5.1x at 8192, 5.6x at 32768)
//   and is notably WORSE than pffft at 1024/2048, hence the size threshold
//   rather than "vDSP everywhere on Apple".
//
// Scaling contract (identical for both engines):
//   forward(x, X)          X = unnormalised DFT of x, packed as above
//   backward(X, y, scale)  y = inverse DFT of X, times scale.
//                          Pass scale = 1/N to recover the original signal.
//
// Sizes must be a power of two (pffft additionally wants N >= 32; smaller
// sizes fall back to the pocketfft engine automatically).

#include <cstddef>

#ifndef GS_FFT_BACKEND
#define GS_FFT_BACKEND 1
#endif

#if GS_FFT_BACKEND && defined(__APPLE__)
#define GS_FFT_HAVE_VDSP 1
// Below this size pffft wins; at and above it vDSP wins. Measured, not guessed.
#ifndef GS_FFT_VDSP_MIN_N
#define GS_FFT_VDSP_MIN_N 8192
#endif
#else
#define GS_FFT_HAVE_VDSP 0
#endif

struct PFFFT_Setup;

// ---------------------------------------------------------------- pocketfft
// Double-precision reference engine. Also the fallback for sizes the fast
// engine cannot handle, so it is always compiled in.
class RealFftPocket {
public:
    using Scalar = double;

    explicit RealFftPocket(int n);
    ~RealFftPocket();
    RealFftPocket(const RealFftPocket&) = delete;
    RealFftPocket& operator=(const RealFftPocket&) = delete;

    int size() const { return _n; }

    void forward(const double* in, double* outPacked);
    void backward(const double* inPacked, double* out, double scale);

private:
    int _n;
    void* _plan;       // rfft_plan
    double* _scratch;  // n doubles, halfcomplex staging
};

#if GS_FFT_BACKEND

// -------------------------------------------------------------------- fast
// pffft, or vDSP for large N on Apple. Pointers passed in may be unaligned;
// the engine stages through its own aligned scratch when it has to.
class RealFftFast {
public:
    using Scalar = float;

    explicit RealFftFast(int n);
    ~RealFftFast();
    RealFftFast(const RealFftFast&) = delete;
    RealFftFast& operator=(const RealFftFast&) = delete;

    int size() const { return _n; }

    void forward(const float* in, float* outPacked);
    void backward(const float* inPacked, float* out, float scale);

private:
    int _n;

    // pffft
    PFFFT_Setup* _pf   = nullptr;
    float*       _pfIn = nullptr;   // aligned staging: n floats
    float*       _pfOut= nullptr;   // aligned staging: n floats
    float*       _work = nullptr;   // aligned scratch: n floats

#if GS_FFT_HAVE_VDSP
    void*  _dftFwd = nullptr;       // vDSP_DFT_Setup
    void*  _dftInv = nullptr;
    float* _re     = nullptr;       // n/2 floats
    float* _im     = nullptr;       // n/2 floats
    bool   _useVdsp = false;
#endif

    // Set when neither fast path can serve this size (n < 32, or not a power
    // of two); we then defer to the double engine and convert at the edges.
    RealFftPocket* _fallback = nullptr;
    double*        _fbBuf    = nullptr;   // n doubles
};

using RealFft = RealFftFast;

#else   // GS_FFT_BACKEND == 0

using RealFft = RealFftPocket;

#endif
