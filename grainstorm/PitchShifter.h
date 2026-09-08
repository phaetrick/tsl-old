#pragma once
//
// Created by pr on 06.08.20.
//

#ifndef GRAINSTORM_PITCHSHIFTER_H
#define GRAINSTORM_PITCHSHIFTER_H

#include "correlation.h"
#include "base.h"
#include <resample.h>

template<typename T>
class PitchShiftCorr : protected ResamplerRounded<double, 32, 250> {
    /*
    static constexpr int32_t CROSSFADE = 650;
    static constexpr int32_t SEARCHAREA = 2048 - CROSSFADE;
    static constexpr int32_t SEGSIZE = 2400;
*/
    static constexpr int32_t CROSSFADE = 1500;
    static constexpr int32_t SEARCHAREA = 4096 - CROSSFADE;
    static constexpr int32_t SEGSIZE = 2400;

public:
    PitchShiftCorr() {
        _delayLine.resize(next_pow_2(SEARCHAREA + CROSSFADE + SEGSIZE), 0);
        _andmask = _delayLine.size() - 1;
        // Only lags where the template still fits inside the search area are scored on
        // equal terms; past that the tail of the template only meets zero padding.
        _corr.setMaxLag(SEARCHAREA - CROSSFADE);
        setShift(1);
        nextlag();
        for (int32_t i = 0; i < CROSSFADE; i++) {
            _winideal[i] = 0.5 - 0.5 * cos(TWOPI_P * i / (T) CROSSFADE);
            _env1[i] = i / (T) CROSSFADE;
            _env2[i] = 1. - _env1[i];
        }
    }

    PitchShiftCorr<T> &operator=(PitchShiftCorr<T> const &o) {
        if (this == &o)
            return *this;
        _writeoff = o._writeoff;
        _count = o._count;
        _readoff = o._readoff;
        _readoffcross = o._readoffcross;
        _delayLine = o._delayLine;
        _shift = o._shift;
        _nextshift = o._nextshift;
        _oldshift = o._oldshift;
        if (mRate != o.mRate) {
            for (int32_t row = 0; row < 250 + 3; row++)
                for (int32_t tap = 0; tap < 32; tap++)
                    mCoefficients[row][tap] = o.mCoefficients[row][tap];
            mRate = o.mRate;
            //upSampleBandWidth = o.upSampleBandWidth;
            //downSampleBandWidth = o.downSampleBandWidth;
        }
        mPhase = o.mPhase;
        mCursor = o.mCursor;
        for (int32_t i = 0; i < 32 * 2; i++)
            mX[i] = o.mX[i];
        interpolate = o.interpolate;
        rows = o.rows;
        return *this;
    }

    void clear() override {
        std::fill(_delayLine.begin(), _delayLine.begin() + _delayLine.size(), 0);
        _writeoff = _count = _readoff = _readoffcross = 0;
    }

    void setShift(T shift) {
        _nextshift = pow(2, shift / 12.);
    }

    void nextlag() {
        if (_oldshift != _nextshift) {
            _oldshift = _nextshift;
            init(_oldshift);
            _shift = getRate();
        }
        int32_t offset = _readoff;
        for (int32_t i = 0; i < CROSSFADE; i++) {
            offset &= _andmask;
            _crossbufx[i] = _delayLine[offset] * _winideal[i];
            offset++;
        }
        _readoffcross = _readoff;

        if (_shift > 1)
            _readoff = offset =
                    _writeoff - SEARCHAREA - samplesToWrite(CROSSFADE + SEGSIZE) - 1 + CROSSFADE +
                    SEGSIZE;
        else _readoff = offset = _writeoff - SEARCHAREA;
        // The search area must NOT be windowed: a taper across it makes the correlation
        // score depend on where in the search area a candidate sits, so the argmax gets
        // pulled towards the middle instead of towards the best-matching splice point.
        // Only the template (_winideal) is weighted - that one is a genuine match weight.
        for (int32_t i = 0; i < SEARCHAREA; i++) {
            offset &= _andmask;
            _crossbufy[i] = _delayLine[offset];
            offset++;
        }
        _readoff += _corr.Compute(_crossbufx, _crossbufy);
    }

    inline T tick(T input) {
        if (_count >= CROSSFADE) {
            while (isWriteNeeded()) {
                _readoff &= _andmask;
                writeNextFrame(_delayLine[_readoff++]);
            }
        } else {
            while (isWriteNeeded()) {
                _readoff &= _andmask;
                _readoffcross &= _andmask;
                writeNextFrame(
                        _env1[_count] * _delayLine[_readoff++] +
                        _env2[_count] * _delayLine[_readoffcross++]);
            }
        }
        if (++_count >= SEGSIZE) {
            _count = 0;
            nextlag();
        }
        _delayLine[_writeoff++] = input;
        _writeoff &= _andmask;
        return readNextFrame();
    }


protected:
    std::vector<T> _delayLine;
    int32_t _andmask;
    T _shift{1}, _nextshift{-1000}, _oldshift{-1000};
    T _winideal[CROSSFADE];
    T _crossbufx[CROSSFADE];
    T _crossbufy[SEARCHAREA];
    T _env1[CROSSFADE], _env2[CROSSFADE];
    CrossCorrelation<T> _corr{CROSSFADE, SEARCHAREA};
    int32_t _writeoff{};
    int32_t _count{};
    int32_t _readoff{};
    int32_t _readoffcross{};
};

class PITCHSHIFT : public Effect {
public:
    PITCHSHIFT(TRACK *track, int32_t chan);

    void compute(MYFLOAT *in, int32_t size) override;

private:
    std::atomic<MYFLOAT> *_shift, *_mode;
    PitchShiftCorr<MYFLOAT> _pitchUp, _pitchDown;
};

#endif //GRAINSTORM_PITCHSHIFTER_H
