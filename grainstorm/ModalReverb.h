#pragma once
//
// Created by pr on 01.11.21.
//

#ifndef GRAINSTORM_MODALREVERB_H
#define GRAINSTORM_MODALREVERB_H


#include <cmath>
#include "defines.h"
#include "logger.h"
#include "resample.h"
#include "base.h"
#include "tools/aligned_memalloc.h"
#include <mutex>
#include "tools/AudioSemaphore.h"
#include <complex>
#include <array>
#include <algorithm>
#include <thread>
#include <random>
#include <cstring>
#include <memory>
#include "types_grainstorm.h"   // GS_MODALREV_ADAPTIVE
#if GS_MODALREV_ADAPTIVE
#include "PartialTracker.h"
#include "HopfPool.h"
#include "ffttools.h"
#endif

#define root2f 1.41421
#define MAXM 4096

#ifdef __cplusplus
extern "C" {
#endif
#if defined(__aarch64__)
void phasorfilter_double(MYFLOAT *in, MYFLOAT **array, uint32_t M);
void phasorfilterhold_double(MYFLOAT *in, MYFLOAT **array, uint32_t M);
#endif

#ifdef __cplusplus
}
#endif


class ModalReverb {
public:
    ModalReverb();

    ~ModalReverb();

    void setUp(MYFLOAT sr, MYFLOAT modes, MYFLOAT reverbTime, MYFLOAT overs = 1.);

    void setUpNonRand(MYFLOAT sr, MYFLOAT reverbTime = 3, MYFLOAT modes = 1024);

    void setReleaseTime(MYFLOAT sr, MYFLOAT releaseTime = 3.);

    void filter(MYFLOAT *input, int32_t size);

protected:
    MYFLOAT _g{};

    static MYFLOAT get_random() {
        static std::mt19937 gen{std::random_device()()};
        static std::uniform_real_distribution<double> dist{0.0, 1.0};
        return dist(gen);
    }

    // r_begin from the original modalreverb.m: how many of the lowest modes
    // ring the full reverb time, indexed by mode count (128..8192) - NOT
    // reverb-time thresholds (the old name _delaytimes caused that misread)
    static constexpr int32_t _r_begin[] = {3, 6, 12, 14, 18, 22, 26};
    static constexpr MYFLOAT _smoothfactors[] = {.2f, .2f, .1f, .05f, .04f, .02f, .01f};
    static constexpr int32_t _modes[] = {128, 256, 512, 1024, 2048, 4096, 8192};
    static constexpr int32_t _modesperoctave[] = {1, 2, 4, 8, 16, 32, 64, 128};
    static constexpr MYFLOAT f_oct[] = {63., 125., 250., 500., 1000., 2000., 4000., 8000.};
    static constexpr MYFLOAT _f1[] = {63. / root2f, 125. / root2f, 250. / root2f, 500. / root2f,
                                    1000. / root2f, 2000. / root2f, 4000. / root2f,
                                    8000. / root2f}, _f2[] = {63. * root2f, 125. * root2f,
                                                               250. * root2f, 500. * root2f,
                                                               1000. * root2f, 2000. * root2f,
                                                               4000. * root2f, 8000. * root2f};
    int32_t M{};
    int32_t os{1};
    std::vector<MYFLOAT> releaseTimes{}, randomfreqs{};
    // MORPH: two independently rolled mode sets, paired by frequency rank.
    // lw = log mode freq (rad/sample), rt = normalized release time, gp =
    // gain magnitude, phA = phase of set A, phD = shortest-path delta to B
    std::vector<MYFLOAT> lwA, lwB, rtA, rtB, gpA, gpB, phA, phD;
    MYFLOAT _comp{1};

    void rollModeSet(MYFLOAT sr, std::mt19937& gen, MYFLOAT* lw, MYFLOAT* rt,
                     MYFLOAT* gp, MYFLOAT* ph);
    void refreshModes(int32_t from, int32_t to, MYFLOAT sr, MYFLOAT reverbTime,
                      MYFLOAT x, MYFLOAT pitch);

    const std::complex<MYFLOAT> I{0., 1.};
    std::complex<MYFLOAT> *_gain, *_w, *_pre, *_post, *_ym_prev;
    MYFLOAT *_helpbuf, *_aa;

    void clear() {
        std::memset(_ym_prev, 0, sizeof(std::complex<MYFLOAT>) * M);
    }

#if GS_MODALREV_ADAPTIVE
    // --- content-adaptive modes (PURITY) ------------------------------------
    // _isAdaptive marks slots owned by the partial tracker rather than by the
    // random roll. Their injection gain cannot be a plain constant: a mode
    // driven at exactly its own frequency reaches |pre|/(1-aa) in steady state,
    // and 1-aa is ~3.6e-5 at RT60 4 s - i.e. +90 dB of build-up that the random
    // bank never sees because its modes are never on the input's partials.
    // refreshModes therefore normalizes adaptive modes by (1-aa) so each
    // tracked partial comes back at _adaptAmp regardless of the reverb time.
    std::vector<unsigned char> _isAdaptive;
    MYFLOAT _adaptAmp{};        // = PURITY inside the adaptive MODES, else 0
    MYFLOAT _randAmp{ 1. };     // random-group gain; 1 outside the adaptive MODES
    // latched with the mode at fade-bottom; setUp reads it to partition, so it
    // must be a separate flag from _oldadapt (which the UI thread also reads)
    bool _oldadaptSetup{};
#endif

    int32_t hold{};
    std::vector<MYFLOAT> tmp1;
private:
    tsl::AlignedVector<unsigned char> buf;
};

class ModalReverbDownSampled : public ModalReverb, public Effect{
public:
    explicit ModalReverbDownSampled(TRACK *t);
    ~ModalReverbDownSampled() override {
        _quit = true;
        _setupSem.release();
        if(_setupThread.joinable())
        _setupThread.join();
        for (auto &s :  sem)
            s.release();
        for(auto &t :threads)if(t.joinable())t.join();
    };
    void compute(MYFLOAT *inl, MYFLOAT *inr, MYFLOAT *outl, MYFLOAT *outr, int32_t s)override ;

protected:
    std::atomic<MYFLOAT> *_delay, *_width, *_wet, *_dry, *_mode, *_pitch, *_morph, *_duck;
    MYFLOAT _olddelay{}, _oldtone2{}, _oldmode{};
#if GS_MODALREV_ADAPTIVE
    std::atomic<MYFLOAT> *_purity;
    bool _oldadapt{};   // latched with _oldmode: FFT-adaptive mode (10..12)?
    bool _oldhopf{};    // latched with _oldmode: oscillator-pool mode (13..15)?
    tsl::HopfPool _hopf;
    MYFLOAT _hopfAmp{}; // = PURITY inside the hopf MODES, else 0

    // --- adaptive analysis (audio thread, run while the workers are idle) ----
    // 4096/1024 at full rate: 85 ms window, 46.9 frames/s. Kept at the host
    // rate rather than the bank's sr/os so the window length does not change
    // with the MODE oversampling.
    static constexpr int32_t ADAPT_FFT{ 4096 };
    static constexpr int32_t ADAPT_HOP{ 1024 };
    tsl::PartialAnalyzer _ana;
    tsl::PartialTracker _tracker;
    tsl::ModeAllocator _alloc;
    std::unique_ptr<FFT> _fft;
    std::vector<MYFLOAT> _polar, _mag, _slotAmp, _anaMono;
    int32_t _adaptPerWorker{};

    void setupAdaptive();
    void updateAdaptive(const MYFLOAT* mono, int32_t n);
    void tuneAdaptiveSlot(int32_t slot, MYFLOAT freqHz);
#endif

    MYFLOAT *tester[6] = {reinterpret_cast<MYFLOAT *>(_pre), reinterpret_cast<MYFLOAT *>(_ym_prev), _aa,
                        reinterpret_cast<MYFLOAT *>(_post), nullptr, nullptr};

    void check();

    // 0..8 are the original algorithms - do not reorder or retune them.
    // 9..11 (shown as 10..12): FFT partial tracker retunes the bank, os 1.
    // 12..14 (shown as 13..15): adaptive-oscillator (Hopf/PLL) pool of
    // 128/256/512 oscillators; the modal bank stays fully random there and is
    // only the PURITY blend partner.
    // These tables must stay the same length as modalrevmodes (gs_common.h).
#if GS_MODALREV_ADAPTIVE
    static constexpr uint32_t ADAPT_FIRST{ 9 };
    static constexpr uint32_t HOPF_FIRST{ 12 };
    static constexpr uint32_t _hopfsizes[] = {128, 256, 512};
    static constexpr uint32_t _oversvals[] = {4, 4, 4, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    static constexpr uint32_t _nummodes[] = {2048, 3072, 4096, 2048, 3072, 4096, 2048, 3072, 4096,
                                             2048, 3072, 4096, 2048, 2048, 2048};
    static constexpr uint32_t _shifts[] = {2u, 2u, 2u, 1u, 1u, 1u, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#else
    static constexpr uint32_t _oversvals[] = {4, 4, 4, 2, 2, 2, 1, 1, 1};
    static constexpr uint32_t _nummodes[] = {2048, 3072, 4096, 2048, 3072, 4096, 2048, 3072, 4096};
    static constexpr uint32_t _shifts[] = {2u, 2u, 2u, 1u, 1u, 1u, 0, 0, 0};
#endif

    // A project saved by a GS_MODALREV_ADAPTIVE build stores MODE 9..14, which
    // would index every table above out of bounds here. Everything that reads
    // _oldmode goes through this, so a stale value degrades to MODE 9 instead.
    static constexpr MYFLOAT clampMode(MYFLOAT m) {
        constexpr MYFLOAT hi = (MYFLOAT)(sizeof(_nummodes) / sizeof(_nummodes[0]) - 1);
        return m < (MYFLOAT)0. ? (MYFLOAT)0. : (m > hi ? hi : m);
    }

    std::atomic_bool _isupdating{};
    std::thread _setupThread;
    std::mutex _setupMutex;
private:
    tsl::BinarySemaphore sem[4]{ tsl::BinarySemaphore{},tsl::BinarySemaphore{},tsl::BinarySemaphore{},tsl::BinarySemaphore{}};
    tsl::BinarySemaphore _setupSem{};
    std::thread threads[4];
    int32_t _refreshPos[4]{};   // per-worker rolling refresh cursor
    MYFLOAT _duckEnv{};         // sidechain envelope for DUCK
    void threadFunc(int32_t num);
    void setupFunc();
    bool _quit{};

    tsl::WorkerLatch workerLatch;
    MYFLOAT *inbuf;
    MYFLOAT *_outl[4]{};
    MYFLOAT *_outr[4]{};
    tsl::AlignedVector<unsigned char> _buf{};
    PolyPhaseResampler2x<MYFLOAT> resampler2XL{1.,32,2}, resampler4xL{1.,32,4},resampler2XR{1.,32,2}, resampler4xR{1.,32,4};

#if defined PLUGIN_MODE || defined STANDALONE_MODE
    MYFLOAT fillBuf[64]{};
    int fillBufPos{};
	const int fillBufSize{ 64 };
    MYFLOAT outputBufL[64]{};    // Left output buffer
    MYFLOAT outputBufR[64]{};    // Right output buffer  
    int outputBufPos{ 0 };         // Read position in output buffer
    int outputBufAvail{ 0 };       // Number of samples available to read
#endif

};

#endif //GRAINSTORM_MODALREVERB_H
