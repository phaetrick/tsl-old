#include "RealFft.h"
#include "PocketFFT.h"

#include <cstdlib>
#include <cstring>
#include <new>

#if GS_FFT_BACKEND
#include "pffft.h"
#endif
#if GS_FFT_HAVE_VDSP
#include <Accelerate/Accelerate.h>
#endif

namespace {

bool isPow2(int n) { return n > 0 && (n & (n - 1)) == 0; }

int log2i(int n) {
    int k = 0;
    while ((1 << k) < n) ++k;
    return k;
}

#if GS_FFT_BACKEND
bool aligned16(const void* p) { return (reinterpret_cast<uintptr_t>(p) & 15u) == 0; }
#endif

// pocketfft's halfcomplex layout is [DC, re1, im1, ... , reNyq]; ours puts the
// Nyquist bin in slot 1 instead of at the end.
inline void hcToPacked(const double* hc, double* p, int n) {
    p[0] = hc[0];
    p[1] = hc[n - 1];
    for (int i = 2; i < n; ++i) p[i] = hc[i - 1];
}

inline void packedToHc(const double* p, double* hc, int n) {
    hc[0] = p[0];
    hc[n - 1] = p[1];
    for (int i = 2; i < n; ++i) hc[i - 1] = p[i];
}

void* alignedAlloc(size_t bytes) {
    void* p = nullptr;
#if defined(_WIN32)
    p = _aligned_malloc(bytes, 64);
#else
    if (posix_memalign(&p, 64, bytes) != 0) p = nullptr;
#endif
    if (!p) throw std::bad_alloc();
    std::memset(p, 0, bytes);
    return p;
}

void alignedFree(void* p) {
    if (!p) return;
#if defined(_WIN32)
    _aligned_free(p);
#else
    free(p);
#endif
}

}  // namespace

// ============================================================== pocketfft

RealFftPocket::RealFftPocket(int n) : _n(n) {
    _plan = make_rfft_plan(static_cast<size_t>(n));
    _scratch = static_cast<double*>(alignedAlloc(sizeof(double) * static_cast<size_t>(n)));
}

RealFftPocket::~RealFftPocket() {
    if (_plan) destroy_rfft_plan(static_cast<rfft_plan>(_plan));
    alignedFree(_scratch);
}

void RealFftPocket::forward(const double* in, double* outPacked) {
    std::memcpy(_scratch, in, sizeof(double) * static_cast<size_t>(_n));
    (void)rfft_forward(static_cast<rfft_plan>(_plan), _scratch, 1.0);
    hcToPacked(_scratch, outPacked, _n);
}

void RealFftPocket::backward(const double* inPacked, double* out, double scale) {
    packedToHc(inPacked, _scratch, _n);
    (void)rfft_backward(static_cast<rfft_plan>(_plan), _scratch, scale);
    std::memcpy(out, _scratch, sizeof(double) * static_cast<size_t>(_n));
}

// =================================================================== fast

#if GS_FFT_BACKEND

RealFftFast::RealFftFast(int n) : _n(n) {
#if GS_FFT_HAVE_VDSP
    // vDSP only wins from GS_FFT_VDSP_MIN_N up; below that pffft is faster and
    // at 1024/2048 vDSP is markedly slower, so this threshold is deliberate.
    if (isPow2(n) && n >= GS_FFT_VDSP_MIN_N) {
        _dftFwd = vDSP_DFT_zrop_CreateSetup(nullptr, (vDSP_Length)n, vDSP_DFT_FORWARD);
        // Sharing the forward setup lets Accelerate reuse its twiddles.
        _dftInv = vDSP_DFT_zrop_CreateSetup((vDSP_DFT_Setup)_dftFwd, (vDSP_Length)n,
                                            vDSP_DFT_INVERSE);
        if (_dftFwd && _dftInv) {
            const size_t half = static_cast<size_t>(n / 2);
            _re = static_cast<float*>(alignedAlloc(sizeof(float) * half));
            _im = static_cast<float*>(alignedAlloc(sizeof(float) * half));
            _pfIn  = static_cast<float*>(alignedAlloc(sizeof(float) * static_cast<size_t>(n)));
            _pfOut = static_cast<float*>(alignedAlloc(sizeof(float) * static_cast<size_t>(n)));
            _useVdsp = true;
            return;
        }
        if (_dftInv) { vDSP_DFT_DestroySetup((vDSP_DFT_Setup)_dftInv); _dftInv = nullptr; }
        if (_dftFwd) { vDSP_DFT_DestroySetup((vDSP_DFT_Setup)_dftFwd); _dftFwd = nullptr; }
    }
#endif

    // pffft real transforms need n to be a multiple of 32.
    if (isPow2(n) && n >= 32) {
        _pf = pffft_new_setup(n, PFFFT_REAL);
    }
    if (_pf) {
        _pfIn  = static_cast<float*>(alignedAlloc(sizeof(float) * static_cast<size_t>(n)));
        _pfOut = static_cast<float*>(alignedAlloc(sizeof(float) * static_cast<size_t>(n)));
        _work  = static_cast<float*>(alignedAlloc(sizeof(float) * static_cast<size_t>(n)));
        return;
    }

    // Nothing fast can take this size: run the double engine and convert.
    _fallback = new RealFftPocket(n);
    _fbBuf = static_cast<double*>(alignedAlloc(sizeof(double) * static_cast<size_t>(n)));
}

RealFftFast::~RealFftFast() {
#if GS_FFT_HAVE_VDSP
    if (_dftInv) vDSP_DFT_DestroySetup((vDSP_DFT_Setup)_dftInv);
    if (_dftFwd) vDSP_DFT_DestroySetup((vDSP_DFT_Setup)_dftFwd);
    alignedFree(_re);
    alignedFree(_im);
#endif
    if (_pf) pffft_destroy_setup(_pf);
    alignedFree(_pfIn);
    alignedFree(_pfOut);
    alignedFree(_work);
    delete _fallback;
    alignedFree(_fbBuf);
}

void RealFftFast::forward(const float* in, float* outPacked) {
    const size_t nb = sizeof(float) * static_cast<size_t>(_n);

#if GS_FFT_HAVE_VDSP
    if (_useVdsp) {
        // Treat the n real samples as n/2 interleaved complex values, which is
        // what the zrop (real-to-even/odd) transform consumes.
        DSPSplitComplex split{_re, _im};
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(in), 2, &split, 1,
                  (vDSP_Length)(_n / 2));
        vDSP_DFT_Execute((vDSP_DFT_Setup)_dftFwd, _re, _im, _pfIn, _pfOut);

        // Interleaving the split result lands exactly on our packed layout:
        // slot 0 = DC, slot 1 = Nyquist, then re/im pairs.
        DSPSplitComplex res{_pfIn, _pfOut};
        vDSP_ztoc(&res, 1, reinterpret_cast<DSPComplex*>(outPacked), 2,
                  (vDSP_Length)(_n / 2));
        // zrop carries a factor of two; strip it so the spectrum is a true DFT.
        const float half = 0.5f;
        vDSP_vsmul(outPacked, 1, &half, outPacked, 1, (vDSP_Length)_n);
        return;
    }
#endif

    if (_pf) {
        const float* src = in;
        if (!aligned16(in)) {
            std::memcpy(_pfIn, in, nb);
            src = _pfIn;
        }
        float* dst = aligned16(outPacked) ? outPacked : _pfOut;
        // pffft's ordered real output is already [DC, Nyq, re, im, ...].
        pffft_transform_ordered(_pf, src, dst, _work, PFFFT_FORWARD);
        if (dst != outPacked) std::memcpy(outPacked, dst, nb);
        return;
    }

    for (int i = 0; i < _n; ++i) _fbBuf[i] = in[i];
    _fallback->forward(_fbBuf, _fbBuf);
    for (int i = 0; i < _n; ++i) outPacked[i] = static_cast<float>(_fbBuf[i]);
}

void RealFftFast::backward(const float* inPacked, float* out, float scale) {
    const size_t nb = sizeof(float) * static_cast<size_t>(_n);

#if GS_FFT_HAVE_VDSP
    if (_useVdsp) {
        DSPSplitComplex split{_re, _im};
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(inPacked), 2, &split, 1,
                  (vDSP_Length)(_n / 2));
        vDSP_DFT_Execute((vDSP_DFT_Setup)_dftInv, _re, _im, _pfIn, _pfOut);
        DSPSplitComplex res{_pfIn, _pfOut};
        vDSP_ztoc(&res, 1, reinterpret_cast<DSPComplex*>(out), 2, (vDSP_Length)(_n / 2));
        // Forward already removed the 2x, so the inverse of a true DFT comes
        // back as n*x -- the caller's 1/n scale is the whole correction.
        vDSP_vsmul(out, 1, &scale, out, 1, (vDSP_Length)_n);
        return;
    }
#endif

    if (_pf) {
        const float* src = inPacked;
        if (!aligned16(inPacked)) {
            std::memcpy(_pfIn, inPacked, nb);
            src = _pfIn;
        }
        float* dst = aligned16(out) ? out : _pfOut;
        pffft_transform_ordered(_pf, src, dst, _work, PFFFT_BACKWARD);
        for (int i = 0; i < _n; ++i) dst[i] *= scale;
        if (dst != out) std::memcpy(out, dst, nb);
        return;
    }

    for (int i = 0; i < _n; ++i) _fbBuf[i] = inPacked[i];
    _fallback->backward(_fbBuf, _fbBuf, scale);
    for (int i = 0; i < _n; ++i) out[i] = static_cast<float>(_fbBuf[i]);
}

#endif  // GS_FFT_BACKEND
