#pragma once
//
// Created by pr on 29.06.20.
//

#ifndef GRAINSTORM_PITCHDETECT_H
#define GRAINSTORM_PITCHDETECT_H

#include <complex>
#include <stdexcept>
#include <vector>
#include <array>
#include "ffttools.h"
#include "Biquad.h"
#include "grainstorm.h"
#include "app.h"

#define PDETECTSIZE 2048
#define PDETECTHOP 512

/*
 * The pitch namespace contains the functions:
 *
 * 	pitch::mpm(data, sample_rate)
 * 	pitch::yin(data, sample_rate)
 * 	pitch::pyin(data, sample_rate)
 * 	pitch::pmpm(data, sample_rate)
 *
 * It will auto-allocate any buffers.
 */
namespace pitch {

    template<typename T>
    T
    yin(const std::vector<T> &, int);

    template<typename T>
    T
    mpm(const std::vector<T> &, int);
} // namespace pitch

/*
 * This namespace is useful for repeated calls to pitch for the same size of
 * buffer.
 *
 * It contains the classes Yin and Mpm which contain the allocated buffers
 * and each implement a `pitch(data, sample_rate)` and
 * `probablistic_pitch(data, sample_rate)` method.
 */

namespace pitch_alloc {

    template<typename T>
    class BaseAlloc {
    public:
        BaseAlloc(long audio_buffer_size)
                : N(audio_buffer_size), buf(N*2,0),
                  out_real(std::vector<T>(N)) {
            plan = make_cfft_plan(N * 2);
//            detail::init_pitch_bins();
        }

        ~BaseAlloc() {
            destroy_cfft_plan(plan);
        }

        long N;
        tsl::AlignedVector<std::complex<double>> buf;
        std::vector<T> out_real;
        cfft_plan plan;

    protected:
        void
        clear() {
            std::fill(buf.begin(), buf.end(), std::complex<double>(0.0, 0.0));
        }
    };

/*
 * Allocate the buffers for MPM for re-use.
 * Intended for multiple consistently-sized audio buffers.
 *
 * Usage: pitch_alloc::Mpm ma(1024)
 *
 * It will throw std::bad_alloc for invalid sizes (<1)
 */
    template<typename T>
    class Mpm : public BaseAlloc<T> {
    public:
        Mpm(long audio_buffer_size) : BaseAlloc<T>(audio_buffer_size) {};

        // min_pitch/max_pitch (Hz, <=0 disables) restrict the lag search range
        T
        pitch(const std::vector<T> &, int, T min_pitch = -1, T max_pitch = -1);

    };


/*
 * Allocate the buffers for YIN for re-use.
 * Intended for multiple consistently-sized audio buffers.
 *
 * Usage: pitch_alloc::Yin ya(1024)
 *
 * It will throw std::bad_alloc for invalid sizes (<2)
 */
    template<typename T>
    class Yin : public BaseAlloc<T> {
    public:
        std::vector<T> yin_buffer;

        Yin(long audio_buffer_size)
                : BaseAlloc<T>(audio_buffer_size),
                  yin_buffer(std::vector<T>(audio_buffer_size / 2)) {
            if (audio_buffer_size / 2 == 0) {
                ;// throw std::bad_alloc();
            }
        }

        T
        pitch(const std::vector<T> &, int);
    };
} // namespace pitch_alloc

namespace util {
    template<typename T>
    std::pair<T, T>
    parabolic_interpolation(const std::vector<T> &, int);

    template<typename T>
    void
    acorr_r(const std::vector<T> &, pitch_alloc::BaseAlloc<T> *);
} // namespace util


class PitchDetectYinFFT {
public:
    PitchDetectYinFFT(int32_t size, int sr);

    float compute(const float *in);

    FFT fft;
    std::vector<float> fftbuf;
    //float *fftbuf;
    std::vector<float> squaremag;
    std::vector<float> yinfft;
    float tol;
    std::vector<float> win;
    std::vector<float> weight;
    int32_t short_p;
    int32_t M, N;
};


#define PDETECTSMOOTHMAXMS 500


class PitchDetect : public Effect {
public:
    PitchDetect(TRACK *track, int32_t chan, std::atomic<MYFLOAT> *smooth,
                std::atomic<MYFLOAT> *prelp, std::atomic<MYFLOAT> *bounda,
                std::atomic<MYFLOAT> *boundb, std::atomic<MYFLOAT> *transpose,
                std::atomic<MYFLOAT> *lastpitch, std::atomic<MYFLOAT> *lastout,
                std::atomic<MYFLOAT> *src,
                MYFLOAT *outbuf, std::atomic<double> *bypass) : Effect(track, chan, SPACE_PDETECT,
                                                                   MONOEFFECT, [this]() {
                _track->fx_queue[_chan].append_first(shared_from_this());
            }, [this]() { destroyRequested = readyToDestroy = true; }),
                                                            butter(track->_appState, nullptr,
                                                                   prelp, nullptr,
                                                                   nullptr, nullptr,
                                                                   nullptr, true),
                                                            _pdetect(PDETECTSIZE) {
        _trackid = track->index;
        _fftoffset = 0;
        _bypass = bypass;
        _outbuf = outbuf;
        _bounda = bounda;
        _boundb = boundb;
        _transpose = transpose;
        _lastpitch = lastpitch;
        _lastout = lastout;
        _src = src;
        _sm = smooth;
        _fftbuf.resize(PDETECTSIZE);
        _ring.resize(PDETECTSIZE);
        tone.init(1.f);
    }


    void compute(MYFLOAT *in, int32_t s) override {
        if (*_bypass)
            return;
        MYFLOAT sm = _sm->load();
        if (sm != _prevsm) {
            tone.Setup(SMOOTH2POLE(sm));
            _prevsm = sm;
        }


        MYFLOAT *buf;
        int32_t src = _src->load();
        if (_trackid != src) {
            in = _DATA->tracks[src]->envf_buffer[_chan];
        }
        MYFLOAT a = LOG2NORMALF(_bounda->load());
        MYFLOAT b = LOG2NORMALF(_boundb->load());
        MYFLOAT min = std::min(a, b);
        MYFLOAT max = std::max(a, b);

        MYFLOAT lastpitch = _lastpitch->load();


        int32_t pos = 0;
        MYFLOAT transpose = powf(2, _transpose->load() / 12.f);

        while (pos < s) {
            _ring[_ringpos] = butter.TickLp(in[pos]);
            if (++_ringpos >= PDETECTSIZE)
                _ringpos = 0;
            if (_filled < PDETECTSIZE)
                _filled++;
            if (++_hopcount >= PDETECTHOP) {
                _hopcount = 0;
                if (_filled >= PDETECTSIZE) {
                    // linearize the ring (oldest sample first): linear
                    // autocorrelation is not invariant under rotation
                    std::copy(_ring.begin() + _ringpos, _ring.end(), _fftbuf.begin());
                    std::copy(_ring.begin(), _ring.begin() + _ringpos,
                              _fftbuf.begin() + (PDETECTSIZE - _ringpos));
                    MYFLOAT p = _pdetect.pitch(_fftbuf, _STATE->sr, min, max);
                    if (p > 0.f)
                        lastpitch = p;
                    if (lastpitch < min)
                        lastpitch = min;
                    else if (lastpitch > max)
                        lastpitch = max;
                }
            }

            lastpitch = tone.tickn(lastpitch);

            _outbuf[pos++] = lastpitch * transpose;
        }
        _lastpitch->store(lastpitch);
        _lastout->store(lastpitch * transpose);
    }

    int32_t _fftoffset{};
    int32_t _ringpos{}, _hopcount{}, _filled{};
    std::atomic<MYFLOAT> *_src, *_lastpitch, *_lastout, *_transpose, *_bounda, *_boundb, *_sm;
    MYFLOAT _prevsm{};
    int32_t _trackid;
    Butterworth<double> butter;
    MYFLOAT *_outbuf;
    pitch_alloc::Mpm<MYFLOAT> _pdetect;
    std::vector<MYFLOAT> _fftbuf, _ring;
    Tone tone;
};

class PitchDetectGrain : public Effect {
public:
    PitchDetectGrain(TRACK *track, int32_t chan, std::atomic<MYFLOAT> *smooth,
                     std::atomic<MYFLOAT> *prelp, std::atomic<MYFLOAT> *bounda,
                     std::atomic<MYFLOAT> *boundb, std::atomic<MYFLOAT> *transpose,
                     std::atomic<MYFLOAT> *lastpitch, std::atomic<MYFLOAT> *lastout,
                     std::atomic<double> *bypass) : Effect(track, chan, SPACE_PDETECTGRAIN,
                                                         GRAINEFFECT, [this]() {
                _track->fx_queue_grain[_chan].append_first(shared_from_this());
            }, [this]() { destroyRequested = readyToDestroy = true; }), _pdetect(PDETECTSIZE),
                                                  butter(track->_appState, nullptr,
                                                         prelp, nullptr,
                                                         nullptr, nullptr,
                                                         nullptr, true) {
        _bypass = bypass;
        _bounda = bounda;
        _boundb = boundb;
        _transpose = transpose;
        _lastpitch = lastpitch;
        _lastout = lastout;
        _fftbuf.resize(PDETECTSIZE);
        _sm = smooth;
    }

    void compute(MYFLOAT *in, int32_t s) override {
        if (_bypass->load())
            return;
        MYFLOAT smooth = _sm->load();
        if (smooth != prevsm) {
            prevsm = smooth;
            tone.Setup(SMOOTH2POLE(smooth));
        }
        MYFLOAT lastpitch = _lastpitch->load();
        MYFLOAT transpose = powf(2, _transpose->load() / 12.f);


        TRACK *src = _DATA->tracks[(int) _STATE->params[_track->index][DISTRSOURCE].load()];
        long offset = src->tmpoffset[_chan].load();
		auto rec = src->filebuffer.load();
        auto filebuffer1 = rec != nullptr ? rec->buffer[_chan].data() : nullptr;

        const long off = rec != nullptr ? rec->off : 0;

        for (int32_t i = 0; i < PDETECTSIZE; i++) {
            long offf = offset + i;
            _fftbuf[i] =
                    butter.TickLp(offf >= 0 && offf < off ? (MYFLOAT) (filebuffer1[offf]) : 0.0);
        }

        MYFLOAT a = LOG2NORMALF(_bounda->load());
        MYFLOAT b = LOG2NORMALF(_boundb->load());
        MYFLOAT min = std::min(a, b);
        MYFLOAT max = std::max(a, b);
        MYFLOAT p = _pdetect.pitch(_fftbuf, _STATE->sr, min, max);

        if (p > 0)
            lastpitch = p;
        //lastpitch = tone.tick(p);
        if (lastpitch < min)
            lastpitch = min;
        else if (lastpitch > max)
            lastpitch = max;
        lastpitch = tone.tickn(lastpitch);

        _lastpitch->store(lastpitch);
        _lastout->store(lastpitch * transpose);
    }

    std::atomic<MYFLOAT> *_lastpitch, *_lastout, *_transpose, *_bounda, *_boundb, *_sm;
    Butterworth<double> butter;
    pitch_alloc::Mpm<MYFLOAT> _pdetect;
    std::vector<MYFLOAT> _fftbuf;
    MYFLOAT prevsm{};
    Tone tone{1.0};
};


#endif //GRAINSTORM_PITCHDETECT_H
