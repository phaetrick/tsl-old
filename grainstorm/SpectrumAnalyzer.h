#pragma once
//
// Created by pr on 08.11.22.
//
#include "defines.h"
#include "ffttools.h"
#include <atomic>

namespace tsl
{
    struct AppState;
}
class SpectrumAnalyzer {
public:
    static constexpr int fftsize = 4096;
    static constexpr int fftsized2 = (fftsize >> 1u);
    static constexpr int windowscale = WINDOW_SIZE / (fftsize);
    static constexpr float defaultMinDb = -60.f;
    static constexpr float defaultMaxDb = 15.f;

    SpectrumAnalyzer(tsl::AppState *state, std::atomic<double*>& input) : _appState{ state }, input_{ input }{
    };

    void compute2(MYFLOAT *, MYFLOAT *, MYFLOAT *, MYFLOAT *);

    void compute3(MYFLOAT *, MYFLOAT *, MYFLOAT *, MYFLOAT *);

    void tick(MYFLOAT inputL);

    void reset() { count = 0; }

private:
    MYFLOAT buf[fftsize]{};
    int32_t count{};
    FFT fft{fftsize};
    std::atomic<double*>& input_;

    tsl::AppState *_appState;
};

