#pragma once
//
// Created by pr on 15.08.20.
//

#ifndef GRAINSTORM_CORRELATION_H
#define GRAINSTORM_CORRELATION_H

#include "tools.h"
#include "setup.h"
#include <ComplexMultiply.h>
#include "ffttools.h"
#include <algorithm>

void compute_welch_window(float* w_data, int32_t s, float* inverse = nullptr);

template<typename T>
class CrossCorrelation {
public:
    CrossCorrelation(uint32_t sizex, uint32_t sizey)
        : _sizex(sizex), _sizey(sizey)
        , N(sizex + sizey - 1)
        , M(next_pow_2(N))
        , fft(M)
    {
        raw_ = static_cast<double*>(aligned_malloc(sizeof(double) * M * 3));
        if (!raw_) throw std::bad_alloc();
        bufx = raw_;
        bufy = raw_ + M;
        out = raw_ + M * 2;
        _maxlag = _sizey - 1;
        reset();
    }

    // Lags beyond sizey - sizex only overlap x with the zero padding behind y, so their
    // score is computed from fewer terms than the rest and is not comparable. Callers that
    // need a like-for-like argmax cap the search here; the default keeps the full range.
    void setMaxLag(int32_t maxlag) {
        _maxlag = std::max(0, std::min(maxlag, _sizey - 1));
    }

    ~CrossCorrelation() {
        aligned_free(raw_);
    }

    void reset() {
        std::fill(bufx, bufx + M, 0.0);
        std::fill(bufy, bufy + M, 0.0);
        std::fill(out, out + M, 0.0);
    }

    void complexmultiply2(uint32_t size) {
#if (__aarch64__)
        auto zero = bufx[0] * bufy[0];
        auto one = bufx[1] * bufy[1];
        complexMultiplyDouble(out, bufx, bufy, size >> 1);
        out[0] = zero;
        out[1] = one;
#else
        out[0] = bufx[0] * bufy[0];
        out[1] = bufx[1] * bufy[1];
        for (int i = 2; i < (int)size; i += 2) {
            auto nimag = i + 1;
            out[i] = bufx[i] * bufy[i] - bufx[nimag] * bufy[nimag];
            out[nimag] = bufx[i] * bufy[nimag] + bufx[nimag] * bufy[i];
        }
#endif
    }

    int32_t Compute(T* x, T* y) {
        std::fill(bufx + _sizex, bufx + M, 0.0);
        std::fill(bufy + _sizey, bufy + M, 0.0);
        for (int32_t i = 0; i < _sizex; i++) { bufx[i] = x[i]; bufy[i] = y[i]; }
        for (int32_t i = _sizex; i < _sizey; i++) bufy[i] = y[i];
        fft.forward(bufx);
        fft.forward(bufy);
        for (int32_t i = 3; i < (int32_t)M; i += 2) bufy[i] *= -1;
        complexmultiply2(M);
        fft.backward(out);
        auto max = out[0];
        int32_t index = 0;
        for (int32_t i = 1; i <= _maxlag; i++) {
            if (out[M - i] > max) { max = out[M - i]; index = i; }
        }
        return index;
    }

private:
    double* raw_;
    double* bufx;
    double* bufy;
    double* out;
    int32_t  _sizex, _sizey;
    int32_t  _maxlag;
    uint32_t N, M;
    FFT      fft;
};

class AutoCorrelation {
public:
    AutoCorrelation(int32_t size)
        : M(size)
        , N(next_pow_2(2 * M - 1))
        , fft(N)
    {
        mem = static_cast<double*>(aligned_malloc(sizeof(double) * N));
        if (!mem) throw std::bad_alloc();
        std::fill(mem, mem + N, 0.0);
    }

    ~AutoCorrelation() {
        aligned_free(mem);
    }

    template<typename T>
    void compute(T x) {
        std::fill(mem + M, mem + N, 0.0);
        for (int32_t i = 0; i < M; i++) mem[i] = x[i];
        fft.forward(mem);
        mem[0] = mem[0] * mem[0];
        mem[1] = mem[1] * mem[1];
        for (int32_t i = 2; i < N; i += 2) {
            mem[i] = mem[i] * mem[i] + mem[i + 1] * mem[i + 1];
            mem[i + 1] = 0;
        }
        fft.backward(mem);
        for (int32_t i = 0; i < M; i++) x[i] = mem[i];
    }

    double* mem;
    int32_t  M, N;
    FFT      fft;
};

#endif //GRAINSTORM_CORRELATION_H