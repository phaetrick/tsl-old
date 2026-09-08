#pragma once
//
// Created by pr on 07.03.18.
//

#ifndef GRAINSTORM_RESAMPLE_H
#define GRAINSTORM_RESAMPLE_H

#include "defines.h"
#include "tools/aligned_memalloc.h"
#include "tools.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <logger.h>
// CPU feature detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define HAS_X86_SIMD 1
#include <immintrin.h>

// Check for AVX support at compile time
#if defined(__AVX__)
#define HAS_AVX 1
#endif
#endif

// Runtime CPU feature detection (optional, for dynamic dispatch)
#ifdef HAS_X86_SIMD
// Runtime CPU detection
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

inline bool cpu_has_sse2() {
	return true; // SSE2 is baseline for x86-64
}

inline bool cpu_has_avx() {
#ifdef HAS_AVX
#ifdef _MSC_VER
	int cpuInfo[4];
	__cpuidex(cpuInfo, 7, 0);
	return (cpuInfo[1] & (1 << 5)) != 0;
#else
	unsigned int eax, ebx, ecx, edx;
	if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
		return false;
	return (ebx & (1 << 5)) != 0;
#endif
#endif
	return false;
}
#endif

#ifdef HAS_X86_SIMD

// Intel SSE2 version (baseline for x86-64)
inline double dotproduct32_sse2(const double* x, const double* coeffs) {
	__m128d sum1 = _mm_setzero_pd();
	__m128d sum2 = _mm_setzero_pd();
	__m128d sum3 = _mm_setzero_pd();
	__m128d sum4 = _mm_setzero_pd();

	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x);
		__m128d c1 = _mm_loadu_pd(coeffs);
		__m128d x2 = _mm_loadu_pd(x + 2);
		__m128d c2 = _mm_loadu_pd(coeffs + 2);

		sum1 = _mm_add_pd(sum1, _mm_mul_pd(x1, c1));
		sum2 = _mm_add_pd(sum2, _mm_mul_pd(x2, c2));

		x += 4;
		coeffs += 4;
	}

	sum1 = _mm_add_pd(sum1, sum2);
	sum3 = _mm_add_pd(sum3, sum4);
	sum1 = _mm_add_pd(sum1, sum3);

	// Horizontal add
	__m128d shuf = _mm_shuffle_pd(sum1, sum1, 1);
	sum1 = _mm_add_sd(sum1, shuf);

	return _mm_cvtsd_f64(sum1);
}

// Intel AVX version (faster)
#ifdef HAS_AVX
inline double dotproduct32_avx(const double* x, const double* coeffs) {
	__m256d sum1 = _mm256_setzero_pd();
	__m256d sum2 = _mm256_setzero_pd();

	for (int i = 0; i < 4; i++) {
		__m256d x1 = _mm256_loadu_pd(x);
		__m256d c1 = _mm256_loadu_pd(coeffs);
		__m256d x2 = _mm256_loadu_pd(x + 4);
		__m256d c2 = _mm256_loadu_pd(coeffs + 4);

		sum1 = _mm256_add_pd(sum1, _mm256_mul_pd(x1, c1));
		sum2 = _mm256_add_pd(sum2, _mm256_mul_pd(x2, c2));

		x += 8;
		coeffs += 8;
	}

	sum1 = _mm256_add_pd(sum1, sum2);

	// Horizontal add across 256-bit register
	__m128d sum_low = _mm256_castpd256_pd128(sum1);
	__m128d sum_high = _mm256_extractf128_pd(sum1, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	__m128d shuf = _mm_shuffle_pd(sum_low, sum_low, 1);
	sum_low = _mm_add_sd(sum_low, shuf);

	return _mm_cvtsd_f64(sum_low);
}
#endif

// 2x upsampling with SSE2
inline void upsample2x_sse2(double out[2], const double* x, const double* coeffs) {
	__m128d sum1_p0 = _mm_setzero_pd();
	__m128d sum2_p0 = _mm_setzero_pd();
	__m128d sum1_p1 = _mm_setzero_pd();
	__m128d sum2_p1 = _mm_setzero_pd();

	const double* x_ptr = x;
	const double* c_ptr = coeffs;

	// Phase 0
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);

		sum1_p0 = _mm_add_pd(sum1_p0, _mm_mul_pd(x1, c1));
		sum2_p0 = _mm_add_pd(sum2_p0, _mm_mul_pd(x2, c2));

		x_ptr += 4;
		c_ptr += 4;
	}

	sum1_p0 = _mm_add_pd(sum1_p0, sum2_p0);
	__m128d shuf = _mm_shuffle_pd(sum1_p0, sum1_p0, 1);
	sum1_p0 = _mm_add_sd(sum1_p0, shuf);
	out[0] = _mm_cvtsd_f64(sum1_p0);

	// Phase 1 (coefficients already advanced)
	x_ptr = x;
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);

		sum1_p1 = _mm_add_pd(sum1_p1, _mm_mul_pd(x1, c1));
		sum2_p1 = _mm_add_pd(sum2_p1, _mm_mul_pd(x2, c2));

		x_ptr += 4;
		c_ptr += 4;
	}

	sum1_p1 = _mm_add_pd(sum1_p1, sum2_p1);
	shuf = _mm_shuffle_pd(sum1_p1, sum1_p1, 1);
	sum1_p1 = _mm_add_sd(sum1_p1, shuf);
	out[1] = _mm_cvtsd_f64(sum1_p1);
}

inline void upsample4x_sse2(double out[4], const double* x, const double* coeffs) {
	const double* x_ptr = x;
	const double* c_ptr = coeffs;

	// Phase 0
	__m128d sum1 = _mm_setzero_pd();
	__m128d sum2 = _mm_setzero_pd();
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);
		sum1 = _mm_add_pd(sum1, _mm_mul_pd(x1, c1));
		sum2 = _mm_add_pd(sum2, _mm_mul_pd(x2, c2));
		x_ptr += 4;
		c_ptr += 4;
	}
	sum1 = _mm_add_pd(sum1, sum2);
	__m128d shuf = _mm_shuffle_pd(sum1, sum1, 1);
	sum1 = _mm_add_sd(sum1, shuf);
	out[0] = _mm_cvtsd_f64(sum1);

	// Phase 1
	x_ptr = x;
	sum1 = _mm_setzero_pd();
	sum2 = _mm_setzero_pd();
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);
		sum1 = _mm_add_pd(sum1, _mm_mul_pd(x1, c1));
		sum2 = _mm_add_pd(sum2, _mm_mul_pd(x2, c2));
		x_ptr += 4;
		c_ptr += 4;
	}
	sum1 = _mm_add_pd(sum1, sum2);
	shuf = _mm_shuffle_pd(sum1, sum1, 1);
	sum1 = _mm_add_sd(sum1, shuf);
	out[1] = _mm_cvtsd_f64(sum1);

	// Phase 2
	x_ptr = x;
	sum1 = _mm_setzero_pd();
	sum2 = _mm_setzero_pd();
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);
		sum1 = _mm_add_pd(sum1, _mm_mul_pd(x1, c1));
		sum2 = _mm_add_pd(sum2, _mm_mul_pd(x2, c2));
		x_ptr += 4;
		c_ptr += 4;
	}
	sum1 = _mm_add_pd(sum1, sum2);
	shuf = _mm_shuffle_pd(sum1, sum1, 1);
	sum1 = _mm_add_sd(sum1, shuf);
	out[2] = _mm_cvtsd_f64(sum1);

	// Phase 3
	x_ptr = x;
	sum1 = _mm_setzero_pd();
	sum2 = _mm_setzero_pd();
	for (int i = 0; i < 8; i++) {
		__m128d x1 = _mm_loadu_pd(x_ptr);
		__m128d c1 = _mm_loadu_pd(c_ptr);
		__m128d x2 = _mm_loadu_pd(x_ptr + 2);
		__m128d c2 = _mm_loadu_pd(c_ptr + 2);
		sum1 = _mm_add_pd(sum1, _mm_mul_pd(x1, c1));
		sum2 = _mm_add_pd(sum2, _mm_mul_pd(x2, c2));
		x_ptr += 4;
		c_ptr += 4;
	}
	sum1 = _mm_add_pd(sum1, sum2);
	shuf = _mm_shuffle_pd(sum1, sum1, 1);
	sum1 = _mm_add_sd(sum1, shuf);
	out[3] = _mm_cvtsd_f64(sum1);
}

#ifdef HAS_AVX
// 2x upsampling with AVX
inline void upsample2x_avx(double out[2], const double* x, const double* coeffs) {
	__m256d sum1_p0 = _mm256_setzero_pd();
	__m256d sum2_p0 = _mm256_setzero_pd();
	__m256d sum1_p1 = _mm256_setzero_pd();
	__m256d sum2_p1 = _mm256_setzero_pd();

	const double* x_ptr = x;
	const double* c_ptr = coeffs;

	// Phase 0
	for (int i = 0; i < 4; i++) {
		__m256d x1 = _mm256_loadu_pd(x_ptr);
		__m256d c1 = _mm256_loadu_pd(c_ptr);
		__m256d x2 = _mm256_loadu_pd(x_ptr + 4);
		__m256d c2 = _mm256_loadu_pd(c_ptr + 4);

		sum1_p0 = _mm256_add_pd(sum1_p0, _mm256_mul_pd(x1, c1));
		sum2_p0 = _mm256_add_pd(sum2_p0, _mm256_mul_pd(x2, c2));

		x_ptr += 8;
		c_ptr += 8;
	}

	sum1_p0 = _mm256_add_pd(sum1_p0, sum2_p0);
	__m128d sum_low = _mm256_castpd256_pd128(sum1_p0);
	__m128d sum_high = _mm256_extractf128_pd(sum1_p0, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	__m128d shuf = _mm_shuffle_pd(sum_low, sum_low, 1);
	sum_low = _mm_add_sd(sum_low, shuf);
	out[0] = _mm_cvtsd_f64(sum_low);

	// Phase 1
	x_ptr = x;
	for (int i = 0; i < 4; i++) {
		__m256d x1 = _mm256_loadu_pd(x_ptr);
		__m256d c1 = _mm256_loadu_pd(c_ptr);
		__m256d x2 = _mm256_loadu_pd(x_ptr + 4);
		__m256d c2 = _mm256_loadu_pd(c_ptr + 4);

		sum1_p1 = _mm256_add_pd(sum1_p1, _mm256_mul_pd(x1, c1));
		sum2_p1 = _mm256_add_pd(sum2_p1, _mm256_mul_pd(x2, c2));

		x_ptr += 8;
		c_ptr += 8;
	}

	sum1_p1 = _mm256_add_pd(sum1_p1, sum2_p1);
	sum_low = _mm256_castpd256_pd128(sum1_p1);
	sum_high = _mm256_extractf128_pd(sum1_p1, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	shuf = _mm_shuffle_pd(sum_low, sum_low, 1);
	sum_low = _mm_add_sd(sum_low, shuf);
	out[1] = _mm_cvtsd_f64(sum_low);
}
// 4x upsampling with AVX
inline void upsample4x_avx(double out[4], const double* x, const double* coeffs) {
	const double* x_ptr = x;
	const double* c_ptr = coeffs;

	for (int phase = 0; phase < 4; phase++) {
		__m256d sum1 = _mm256_setzero_pd();
		__m256d sum2 = _mm256_setzero_pd();

		x_ptr = x;
		for (int i = 0; i < 4; i++) {
			__m256d x1 = _mm256_loadu_pd(x_ptr);
			__m256d c1 = _mm256_loadu_pd(c_ptr);
			__m256d x2 = _mm256_loadu_pd(x_ptr + 4);
			__m256d c2 = _mm256_loadu_pd(c_ptr + 4);

			sum1 = _mm256_add_pd(sum1, _mm256_mul_pd(x1, c1));
			sum2 = _mm256_add_pd(sum2, _mm256_mul_pd(x2, c2));

			x_ptr += 8;
			c_ptr += 8;
		}

		sum1 = _mm256_add_pd(sum1, sum2);
		__m128d sum_low = _mm256_castpd256_pd128(sum1);
		__m128d sum_high = _mm256_extractf128_pd(sum1, 1);
		sum_low = _mm_add_pd(sum_low, sum_high);
		__m128d shuf = _mm_shuffle_pd(sum_low, sum_low, 1);
		sum_low = _mm_add_sd(sum_low, shuf);
		out[phase] = _mm_cvtsd_f64(sum_low);
	}
}

#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if (__aarch64__)
	void arm_neon_interpolate_linear_32(void* low, void* high, void* mX, void* c1, void* c2);
	void arm_neon_interpolate_cubic_32(void* sum, void* mX, void* c[]);

	float arm_neon_dotproduct64_single(float* mX, float* coefficients);
	float arm_neon_dotproduct128_single(float* mX, float* coefficients);
	double arm_neon_dotproduct32_double(void* mX, void* coefficients);
	double arm_neon_dotproduct16_double(void* mX, void* coefficients);
	void arm_neon_rs2x_32_double(void* res, void* mx, void* coefficients);
	void arm_neon_rs4x_32_double(void* res, void* mx, void* coefficients);
	void arm_test(void* res);

#endif
#ifdef __cplusplus
}
#endif

template<typename T>
class HyperbolicCosineWindow {
public:
	HyperbolicCosineWindow() {
		setStopBandAttenuation(60);
	}

	/**
	 * @param attenuation typical values range from 30 to 90 dB
	 * @return beta
	 */
	T setStopBandAttenuation(T attenuation) {
		T alpha = static_cast<T>(((-325.1e-6 * attenuation + 0.1677) * attenuation) - 3.149);
		setAlpha(alpha);
		return alpha;
	}

	void setAlpha(T alpha) {
		mAlpha = alpha;
		mInverseCoshAlpha = static_cast <T>(1.0 / cosh(alpha));
	}

	/**
	 * @param x ranges from -1.0 to +1.0
	 */
	T operator()(T x) {
		T x2 = x * x;
		if (x2 >= 1.0) return 0.0;
		T w = static_cast<T>(mAlpha * sqrt(1.0 - x2));
		return static_cast<T>(cosh(w) * mInverseCoshAlpha);
	}

private:
	T mAlpha = 0.0;
	T mInverseCoshAlpha = 1.0;
};

class IntegerRatio {
public:
	IntegerRatio(int32_t numerator, int32_t denominator)
		: mNumerator(numerator), mDenominator(denominator) {
	}

	/**
	 * Reduce by removing common prime factors.
	 */
	 // Enough primes to cover the common sample rates.
	constexpr static const int kPrimes[] = {
			2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
			43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97,
			101, 103, 107, 109, 113, 127, 131, 137, 139, 149,
			151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199 };

	void reduce() {
		for (int prime : kPrimes) {
			if (mNumerator < prime || mDenominator < prime) {
				break;
			}

			// Find biggest prime factor for numerator.
			while (true) {
				int top = mNumerator / prime;
				int bottom = mDenominator / prime;
				if ((top >= 1)
					&& (bottom >= 1)
					&& (top * prime == mNumerator) // divided evenly?
					&& (bottom * prime == mDenominator)) {
					mNumerator = top;
					mDenominator = bottom;
				}
				else {
					break;
				}
			}

		}
	}

	int32_t getNumerator() const {
		return mNumerator;
	}

	int32_t getDenominator() const {
		return mDenominator;
	}

private:
	int32_t mNumerator;
	int32_t mDenominator;
};


template<typename T = MYFLOAT, int nTaps = 16, int nRows = 256>
class WindowedSinc {
public:
	WindowedSinc() {
		HyperbolicCosineWindow<T> mCoshWindow;
		const int numTapsHalf = nTaps / 2; // numTaps must be even.
		const T numTapsHalfInverse = 1.0 / numTapsHalf;
		T cutoffScaler = getDownSampleBandWidth(nTaps);
		for (int i = 0; i < nRows + 1; i++) {
			T tapPhase = (double)i / nRows - numTapsHalf;
			T gain = 0.0; // sum of raw coefficients
			for (int tap = 0; tap < nTaps; tap++) {
				if (std::abs(tapPhase) < 1.0e-9) {
					buf[i][tap] = 1.;
					gain += 1.;
				}
				else {
					T radians = tapPhase * PI_P;
					T window = mCoshWindow(tapPhase * numTapsHalfInverse);
					T coefficient = (T)sin(radians * cutoffScaler) / (radians)*window;
					buf[i][tap] = coefficient;
					gain += coefficient;
				}
				tapPhase += 1.0;
			}
			T gainCorrection = 1.0 / gain; // normalize the gain
			for (int tap = 0; tap < nTaps; tap++) {
				buf[i][tap] *= gainCorrection;
			}
		}
	}

	inline T tick(T mem[], T frac) {
		int index = static_cast<int>(frac * nRows) % nRows;
		int index2 = index + 1;
		T sum1{}, sum2{};

#ifdef __aarch64__
		if (nRows == 32) {
			arm_neon_interpolate_linear_32(&sum1, &sum2, mem, buf[index], buf[index2]);
		}
		else if (nRows == 16) {
			sum1 = arm_neon_dotproduct16_double(mem, &buf[index]);
			sum2 = arm_neon_dotproduct16_double(mem, &buf[index2]);
		}
		else {
#endif
			for (int tap = 0; tap < nTaps; tap++) {
				sum1 += mem[tap] * buf[index][tap];
				sum2 += mem[tap] * buf[index2][tap];
			}
#ifdef __aarch64__
		}
#endif
		return sum1 + frac * (sum2 - sum1);
	}

	alignas(ALIGN) T buf[nRows + 1][nTaps];
	struct QualityMapping {
		int base_length;
		T downsample_bandwidth;
		T upsample_bandwidth;
	};

	static constexpr double getDownSampleBandWidth(const int taps) {
		double bw = quality_map[10].downsample_bandwidth;
		for (const auto& i : quality_map)
			if (taps <= i.base_length)
				bw = i.downsample_bandwidth;
		return bw;
	}

	static constexpr double getUpSampleBandWidth(const int taps) {
		double bw = quality_map[10].upsample_bandwidth;
		for (const auto& i : quality_map)
			if (taps <= i.base_length)
				bw = i.upsample_bandwidth;
		return bw;
	}

	static constexpr const struct QualityMapping quality_map[11] = {
			{8,   0.830, 0.860}, /* Q0 */
			{16,  0.850, 0.880}, /* Q1 */
			{32,  0.882, 0.910}, /* Q2 */  /* 82.3% cutoff ( ~60 dB stop) 6  */
			{48,  0.895, 0.917}, /* Q3 */  /* 84.9% cutoff ( ~80 dB stop) 8  */
			{64,  0.921, 0.940}, /* Q4 */  /* 88.7% cutoff ( ~80 dB stop) 8  */
			{80,  0.922, 0.940}, /* Q5 */  /* 89.1% cutoff (~100 dB stop) 10 */
			{96,  0.940, 0.945}, /* Q6 */  /* 91.5% cutoff (~100 dB stop) 10 */
			{128, 0.950, 0.950}, /* Q7 */  /* 93.1% cutoff (~100 dB stop) 10 */
			{160, 0.960, 0.960}, /* Q8 */  /* 94.5% cutoff (~100 dB stop) 10 */
			{192, 0.968, 0.968}, /* Q9 */  /* 95.5% cutoff (~100 dB stop) 10 */
			{256, 0.975, 0.975}, /* Q10 */ /* 96.6% cutoff (~100 dB stop) 10 */
	};

};

template<typename T = MYFLOAT, int nTaps = 16, int nRows = 256>
class WindowedSincDelay : protected WindowedSinc<T, nTaps, nRows> {
public:
	WindowedSincDelay() = default;

	void init(int maxDelay) {
		delayline.resize(next_pow_2(maxDelay + nTaps - 1), 0);
		mask = delayline.size() - 1;
	}

	void reset() {
		std::fill(delayline.begin(), delayline.end(), 0);
		writeoff = 0;
	}

	inline T tick(T in, T delay, T fb = 0) {
		delayline[writeoff] = in;
		T del = writeoff - delay;
		int floorDel = static_cast<int>(floor(del));
		T frac = del - floorDel;
		int index = static_cast<int>(frac * nRows) % nRows;
		int index2 = index + 1;
		alignas(ALIGN) T sum1 {}, sum2{};
		for (int i = 0; i < nTaps; i++)
			tmpBuf[i] = delayline[(floorDel--) & mask];
#ifdef __aarch64__
		if (nRows == 32) {
			arm_neon_interpolate_linear_32(&sum1, &sum2, tmpBuf, WindowedSinc<>::buf[index],
				WindowedSinc<>::buf[index2]);
		}
		else if (nRows == 16) {
			sum1 = arm_neon_dotproduct16_double(tmpBuf, WindowedSinc<>::buf[index]);
			sum2 = arm_neon_dotproduct16_double(tmpBuf, WindowedSinc<>::buf[index2]);
		}
		else {
#endif
			for (int tap = 0; tap < nTaps; tap++) {
				sum1 += tmpBuf[tap] * WindowedSinc<>::buf[index][tap];
				sum2 += tmpBuf[tap] * WindowedSinc<>::buf[index2][tap];
			}
#ifdef __aarch64__
		}
#endif
		auto sum = sum1 + frac * (sum2 - sum1);
		delayline[writeoff] += sum * fb;
		(++writeoff) &= mask;
		return sum;
	}

	inline T tickAP(T in, T delay, T c) {
		T del = writeoff - delay;
		int floorDel = static_cast<int>(floor(del));
		T frac = del - floorDel;
		int index = static_cast<int>(frac * nRows) % nRows;
		int index2 = index + 1;
		alignas(ALIGN) T sum1 {}, sum2{};
		for (int i = 0; i < nTaps; i++)
			tmpBuf[i] = delayline[(floorDel--) & mask];
#ifdef __aarch64__
		if (nRows == 32) {
			arm_neon_interpolate_linear_32(&sum1, &sum2, tmpBuf, WindowedSinc<>::buf[index],
				WindowedSinc<>::buf[index2]);
		}
		else if (nRows == 16) {
			sum1 = arm_neon_dotproduct16_double(tmpBuf, WindowedSinc<>::buf[index]);
			sum2 = arm_neon_dotproduct16_double(tmpBuf, WindowedSinc<>::buf[index2]);
		}
		else {
#endif
			for (int tap = 0; tap < nTaps; tap++) {
				sum1 += tmpBuf[tap] * WindowedSinc<>::buf[index][tap];
				sum2 += tmpBuf[tap] * WindowedSinc<>::buf[index2][tap];
			}
#ifdef __aarch64__
		}
#endif
		auto sum = sum1 + frac * (sum2 - sum1);
		in += c * sum;
		delayline[writeoff] = in;
		(++writeoff) &= mask;
		return sum - in * c;
	}

	inline void write(T in) {
		delayline[writeoff] = in;
	}

	inline void advanceWrite() {
		(++writeoff) &= mask;
	}

private:
	unsigned int writeoff{};
	std::vector<T> delayline;
	unsigned int mask{};
	alignas(ALIGN) T tmpBuf[nTaps]{};
};


constexpr void cubicCoeff(double interp[], const double frac) {
	interp[3] = -0.1666666667 * frac + 0.1666666667 * (frac * frac * frac);
	interp[2] = frac + 0.5 * (frac * frac) - 0.5 * (frac * frac * frac);
	interp[0] =
		-0.3333333333 * frac + 0.5 * (frac * frac) - 0.1666666667 * (frac * frac * frac);
	interp[1] = 1. - interp[3] - interp[2] - interp[0];
}


template<typename T, int numTaps = 32, int numRows = 250>
class PolyPhaseResampler {
public:
	PolyPhaseResampler() = default;

	virtual void clear() {
		for (int i = 0; i < numTaps * 2; i++)mX[i] = 0;
		mCursor = 0;
		mPhase = ceil(mRate);
	}

	virtual void init(T rate) {
		if (mRate == rate)
			return;
		const auto frac = rate - floor(rate);
		rows = 1;
		while (rows <= numRows) {
			if (rows * frac - floor(rows * frac) < 1.e-9)
				break;
			rows++;
		}
		mRate = rate;
		interpolate = rows > numRows;
		mPhase = ceil(mRate);
		generateCoefficients();
	}

	void initRounded(T rate) {
		PolyPhaseResampler::init(std::round(rate * numRows) / (T)numRows);
	}

	bool isWriteNeeded() const {
		return mPhase >= 1.;
	}

	T samplesToWrite(int samplesToRead) {
		return samplesToRead * mRate;
	}

	T samplesToRead(int samplesToWrite) {
		if (mRate != 0)
			return samplesToWrite / mRate;
		else return 0;
	}

	void writeNextFrame(const T sample) {
		writeFrame(sample);
		advanceWrite();
	}

	T readNextFrame() {
		auto ret = interpolate ? readFrameInterpolatedCubic() : readFrame();
		advanceRead();
		return ret;
	}

	double getRate() {
		return mRate;
	}

	int inputSamplesNeeded(int outputSamples) {
		auto phase = mPhase;
		int needed = 0;
		int created = 0;

		while (created < outputSamples) {
			// While phase >= 1.0, the resampler requires an input 'write' 
			// before it can produce the next output 'read'.
			while (phase >= 1.0) {
				phase -= 1.0;
				needed++;
			}
			// Once phase < 1.0, a 'read' occurs
			created++;
			phase += mRate;
		}
		return needed;
	}

	int outputSamplesCreated(int inputSamples) {
		auto phase = mPhase;
		int samplesWritten = 0;
		int created = 0;

		// Simulate writing the batch of input samples
		while (samplesWritten < inputSamples) {
			if (phase >= 1.0) {
				phase -= 1.0;
				samplesWritten++;
			}

			// After a write (or if phase was already low), 
			// can we produce an output?
			while (phase < 1.0) {
				created++;
				phase += mRate;
			}
		}
		return created;
	}

	PolyPhaseResampler<T, numTaps, numRows>& operator=(PolyPhaseResampler<T, numTaps, numRows> const& o) {
		// Guard self assignment
		if (this == &o)
			return *this;
		if (mRate != o.mRate) {
			for (int row = 0; row < numRows + 4; row++)
				for (int tap = 0; tap < numTaps; tap++)
					mCoefficients[row][tap] = o.mCoefficients[row][tap];
			mRate = o.mRate;
			//upSampleBandWidth = o.upSampleBandWidth;
			//downSampleBandWidth = o.downSampleBandWidth;
		}
		mPhase = o.mPhase;
		mCursor = o.mCursor;
		for (int i = 0; i < numTaps * 2; i++)
			mX[i] = o.mX[i];
		interpolate = o.interpolate;
		rows = o.rows;
		return *this;
	}


protected:
	inline void advanceWrite() {
		mPhase -= 1;
	}

	inline void advanceRead() {
		mPhase += mRate;
	}

	inline void writeFrame(const T sample) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--mCursor < 0) {
			mCursor = numTaps - 1;
		}
		mX[mCursor] = mX[mCursor + numTaps] = sample;
		// Write each channel twice so we avoid having to wrap when running the FIR.// Put ordered writes together.
	}

	inline T readFrameInterpolatedCubic() {
		const auto floorphase = floor(mPhase);
		const auto frac = mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		const int indexM1 = index - 1;
		const int indexM2 = index - 2;
		const int indexP1 = index + 1;
		alignas(ALIGN) T interp[4];
		cubicCoeff(interp, frac);
		alignas(ALIGN) T sum[4]{};
#if __aarch64__
		if (numTaps == 32) {
			void* cc[4] = { mCoefficients[indexM2], mCoefficients[indexM1], mCoefficients[index],
						   mCoefficients[indexP1] };
			arm_neon_interpolate_cubic_32(sum, &mX[mCursor], cc);
			return sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
				sum[3] * interp[3];
		}
		else {
#endif
			T sum1{}, sum2{}, sum3{}, sum4{};
			for (int tap = 0; tap < numTaps; tap++) {
				sum1 += mX[mCursor + tap] * mCoefficients[indexM2][tap];
				sum2 += mX[mCursor + tap] * mCoefficients[indexM1][tap];
				sum3 += mX[mCursor + tap] * mCoefficients[index][tap];
				sum4 += mX[mCursor + tap] * mCoefficients[indexP1][tap];
			}
			return sum1 * interp[0] + sum2 * interp[1] + sum3 * interp[2] + sum4 * interp[3];
#if __aarch64__
		}
#endif
	}

	inline T readFrameInterpolatedLinear() {
		const auto floorphase = floor(mPhase);
		const auto frac = mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		const int index2 = index + 1;
#if __aarch64__
		if (numTaps == 32) {
			alignas(ALIGN) T sum1 {}, sum2{};
			arm_neon_rs_readframe_sinc32(&sum1, &sum2, &mX[mCursor],
				mCoefficients[index],
				mCoefficients[index2]);
			return (sum1 + frac * (sum2 - sum1));
		}
		else {
#endif
			alignas(ALIGN) T sum1 {}, sum2{};
			for (int tap = 0; tap < numTaps; tap++) {
				sum1 += mX[mCursor + tap] * mCoefficients[index][tap];
				sum2 += mX[mCursor + tap] * mCoefficients[index2][tap];
			}
			return (sum1 + frac * (sum2 - sum1));
#if __aarch64__
		}
#endif
	}

	virtual inline T readFrame() {
		if (mRate == 1.)
			return mX[mCursor];
		auto coefficients = mCoefficients[static_cast<int>(mPhase * rows) % rows];
#if (__aarch64__)
		if (numTaps == 32)
			return arm_neon_dotproduct32_double(&mX[mCursor],
				coefficients);
		else {
#endif
			alignas(ALIGN) T sum = 0.0;
			auto xFrame = &mX[mCursor];

			const int numLoops = numTaps >> 2; // n/4
			for (int i = 0; i < numLoops; i++) {
				// Manual loop unrolling, might get converted to SIMD.
				sum += *xFrame++ * *coefficients++;
				sum += *xFrame++ * *coefficients++;
				sum += *xFrame++ * *coefficients++;
				sum += *xFrame++ * *coefficients++;
			}
			return sum;
#if (__aarch64__)
		}
#endif

	}

	T sinc(T radians) {
		if (std::abs(radians) < 1.0e-9) return 1.0;   // avoid divide by zero
		return sin(radians) / radians;   // Sinc function
	}

	void generateCoefficients() {
		const T cutoffScaler = (mRate > 1. ? downSampleBandWidth * 1. / mRate
			: upSampleBandWidth);
		const int numTapsHalf = numTaps / 2; // numTaps must be even.
		const T numTapsHalfInverse = 1.0 / numTapsHalf;
		if (interpolate) {
			for (int i = 0; i < numRows + 4; i++) {
				T tapPhase = (i - 2) / (T)numRows - numTapsHalf;
				T gain = 0.0; // sum of raw coefficients
				for (int tap = 0; tap < numTaps; tap++) {
					T radians = tapPhase * PI_P;
					T coefficient =
						sinc(radians * cutoffScaler) *
						hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
					mCoefficients[i][tap] = coefficient;
					gain += coefficient;
					tapPhase += 1.0;
				}
				T gainCorrection = 1.0 / gain; // normalize the gain
				for (int tap = 0; tap < numTaps; tap++) {
					mCoefficients[i][tap] *= gainCorrection;
				}
			}
		}
		else {
			for (int i = 0; i < rows; i++) {
				T tapPhase = i / (T)rows - numTapsHalf;
				T gain = 0.0; // sum of raw coefficients
				for (int tap = 0; tap < numTaps; tap++) {
					T radians = tapPhase * PI_P;
					T coefficient =
						sinc(radians * cutoffScaler) *
						hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
					mCoefficients[i][tap] = coefficient;
					gain += coefficient;
					tapPhase += 1.0;
				}

				// Correct for gain variations.
				T gainCorrection = 1.0 / gain; // normalize the gain
				for (int tap = 0; tap < numTaps; tap++) {
					mCoefficients[i][tap] *= gainCorrection;
				}
			}
		}
	}



	HyperbolicCosineWindow<T> hyperbolicCosineWindow;
	T mRate{};
	int32_t mCursor{};
	T mPhase{ 1. };
	alignas(ALIGN) T mX[numTaps * 2]{};
	alignas(ALIGN) T mCoefficients[numRows + 4][numTaps]{};
	const T upSampleBandWidth{ WindowedSinc<>::getUpSampleBandWidth(numTaps) }, downSampleBandWidth{
			WindowedSinc<>::getDownSampleBandWidth(numTaps) };
	bool interpolate{};
	int rows{ 1 };
};

template<typename T, int chans = 2, int numTaps = 32, int numRows = 250>
class MultiChanPolyPhaseResampler : public PolyPhaseResampler<T, numTaps, numRows> {
	using Base = PolyPhaseResampler<T, numTaps, numRows>;
public:
	void writeNextFrame(const T sample[]) {
		writeFrame(sample);
		Base::advanceWrite();
	}

	void readNextFrame(T out[], int active[]) {
		Base::interpolate ? readFrameInterpolatedCubic(out, active) : readFrame(out, active);
		Base::advanceRead();
	}
	void readNextFrame(T out[]) {
		Base::interpolate ? readFrameInterpolatedCubic(out) : readFrame(out);
		Base::advanceRead();
	}
private:
	inline void readFrameInterpolatedCubic(T out[]) {
		const auto floorphase = floor(Base::mPhase);
		const auto frac = Base::mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		const int indexM1 = index - 1;
		const int indexM2 = index - 2;
		const int indexP1 = index + 1;
		alignas(ALIGN) T interp[4];
		cubicCoeff(interp, frac);
#if __aarch64__
		if (numTaps == 32) {
			void* cc[4] = { Base::mCoefficients[indexM2], Base::mCoefficients[indexM1],
						   Base::mCoefficients[index],
						   Base::mCoefficients[indexP1] };
			for (int channel = 0; channel < chans; channel++) {
				alignas(ALIGN) T sum[4]{};
				arm_neon_interpolate_cubic_32(sum, &mX[channel][Base::mCursor], cc);
				out[channel] = sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
					sum[3] * interp[3];
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				alignas(ALIGN) T sum[4]{};
				for (int tap = 0; tap < numTaps; tap++) {
					sum[0] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexM2][tap];
					sum[1] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexM1][tap];
					sum[2] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index][tap];
					sum[3] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexP1][tap];
				}
				out[channel] = sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
					sum[3] * interp[3];
			}
#if __aarch64__
		}
#endif
	}

	inline void readFrame(T out[]) {
		auto coefficients = Base::mCoefficients[static_cast<int>(Base::mPhase * Base::rows) %
			Base::rows];

#if __aarch64__
		if (numTaps == 32) {
			for (int channel = 0; channel < chans; channel++) {
				out[channel] = arm_neon_dotproduct32_double(&mX[channel][Base::mCursor],
					coefficients);
			}
		}
		else if (numTaps == 16) {
			for (int channel = 0; channel < chans; channel++) {
				out[channel] = arm_neon_dotproduct16_double(&mX[channel][Base::mCursor],
					coefficients);
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				out[channel] = 0.0;
				for (int tap = 0; tap < numTaps; tap++) {
					out[channel] +=
						mX[channel][Base::mCursor + tap] * coefficients[tap];
				}
			}
#if __aarch64__
		}
#endif
	}


	inline void readFrameInterpolatedLinear(T out[]) {
		const auto floorphase = floor(Base::mPhase);
		const auto frac = Base::mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		//auto index = static_cast<int>(Base::mPhase * nRows) % nRows;
		const auto index2 = index + 1;

#if __aarch64__
		if (numTaps == 32) {
			for (int channel = 0; channel < chans; channel++) {
				alignas(ALIGN) T sum1 {}, sum2{};
				arm_neon_rs_readframe_sinc32(&sum1, &sum2, &mX[channel][Base::mCursor],
					Base::mCoefficients[index],
					Base::mCoefficients[index2]);
				out[channel] = sum1 + frac * (sum2 - sum1);
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				alignas(ALIGN) T sum1 {}, sum2{};
				for (int tap = 0; tap < numTaps; tap++) {
					sum1 += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index][tap];
					sum2 += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index2][tap];
				}
				out[channel] = sum1 + frac * (sum2 - sum1);
			}
#if __aarch64__
		}
#endif
	}


	inline void readFrameInterpolatedCubic(T out[], int active[]) {
		const auto floorphase = floor(Base::mPhase);
		const auto frac = Base::mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		const int indexM1 = index - 1;
		const int indexM2 = index - 2;
		const int indexP1 = index + 1;
		alignas(ALIGN) T interp[4];
		cubicCoeff(interp, frac);
#if __aarch64__
		if (numTaps == 32) {
			void* cc[4] = { Base::mCoefficients[indexM2], Base::mCoefficients[indexM1],
						   Base::mCoefficients[index],
						   Base::mCoefficients[indexP1] };
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				alignas(ALIGN) T sum[4]{};
				arm_neon_interpolate_cubic_32(sum, &mX[channel][Base::mCursor], cc);
				out[channel] = sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
					sum[3] * interp[3];
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				alignas(ALIGN) T sum[4]{};
				for (int tap = 0; tap < numTaps; tap++) {
					sum[0] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexM2][tap];
					sum[1] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexM1][tap];
					sum[2] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index][tap];
					sum[3] += mX[channel][Base::mCursor + tap] * Base::mCoefficients[indexP1][tap];
				}
				out[channel] = sum[0] * interp[0] + sum[1] * interp[1] + sum[2] * interp[2] +
					sum[3] * interp[3];
			}
#if __aarch64__
		}
#endif
	}

	inline void readFrame(T out[], int active[]) {
		auto coefficients = Base::mCoefficients[static_cast<int>(Base::mPhase * Base::rows) %
			Base::rows];

#if __aarch64__
		if (numTaps == 32) {
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				out[channel] = arm_neon_dotproduct32_double(&mX[channel][Base::mCursor],
					coefficients);
			}
		}
		else if (numTaps == 16) {

			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				out[channel] = arm_neon_dotproduct16_double(&mX[channel][Base::mCursor],
					coefficients);
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				out[channel] = 0.0;
				for (int tap = 0; tap < numTaps; tap++) {
					out[channel] +=
						mX[channel][Base::mCursor + tap] * coefficients[tap];
				}
			}
#if __aarch64__
		}
#endif
	}


	inline void readFrameInterpolatedLinear(T out[], int active[]) {
		const auto floorphase = floor(Base::mPhase);
		const auto frac = Base::mPhase - floorphase;
		const int index = 2 + static_cast<int>(frac * numRows);
		//auto index = static_cast<int>(Base::mPhase * nRows) % nRows;
		const auto index2 = index + 1;

#if __aarch64__
		if (numTaps == 32) {
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				alignas(ALIGN) T sum1 {}, sum2{};
				arm_neon_rs_readframe_sinc32(&sum1, &sum2, &mX[channel][Base::mCursor],
					Base::mCoefficients[index],
					Base::mCoefficients[index2]);
				out[channel] = sum1 + frac * (sum2 - sum1);
			}
		}
		else {
#endif
			for (int channel = 0; channel < chans; channel++) {
				if (!active[channel]) {
					out[channel] = 0.0;
					continue;
				}
				alignas(ALIGN) T sum1 {}, sum2{};
				for (int tap = 0; tap < numTaps; tap++) {
					sum1 += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index][tap];
					sum2 += mX[channel][Base::mCursor + tap] * Base::mCoefficients[index2][tap];
				}
				out[channel] = sum1 + frac * (sum2 - sum1);
			}
#if __aarch64__
		}
#endif
	}





	inline void writeFrame(const T frame[]) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--Base::mCursor < 0) {
			Base::mCursor = numTaps - 1;
		}
		for (int channel = 0; channel < chans; channel++) {
			// Write twice so we avoid having to wrap when reading.
			mX[channel][Base::mCursor] = mX[channel][Base::mCursor + numTaps] = frame[channel];
		}

	}

	alignas(ALIGN) T mX[chans][numTaps * 2]{};
};


template<typename T>
class PolyPhaseResampler2x {
public:
	PolyPhaseResampler2x(T cutoff = 1.0, int numTaps = 32, int os = 2) : _os{
			os }, mNumTaps(numTaps), mXDown(numTaps * 2, 0),
			mXUp(numTaps *
				2,
				0) {
		upSampleBandwidth = WindowedSinc<>::getUpSampleBandWidth(numTaps);
		downsampleBandwith = WindowedSinc<>::getDownSampleBandWidth(numTaps);
		generateCoefficients(mCoefficientsUp, mNumTaps, 1, _os, upSampleBandwidth);
		generateCoefficients(mCoefficientsDown, mNumTaps, _os, 1, cutoff * downsampleBandwith);
	}

	void setCutOff(T cutoff = 1.0) {
		generateCoefficients(mCoefficientsDown, mNumTaps, _os, 1, cutoff * downsampleBandwith);
	}

	inline void tickUp(const T sample, T out[]) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--mCursorUp < 0) {
			mCursorUp = mNumTaps - 1;
		}
		mXUp[mCursorUp] = mXUp[mCursorUp + mNumTaps] = sample;
#if (__aarch64__)
		arm_neon_rs2x_32_double(out, &mXUp[mCursorUp], mCoefficientsUp.data());
#elif defined(HAS_X86_SIMD)
#ifdef HAS_AVX
		if (cpu_has_avx()) {
			upsample2x_avx(out, &mXUp[mCursorUp], mCoefficientsUp.data());
			return;
		}
#endif
		upsample2x_sse2(out, &mXUp[mCursorUp], mCoefficientsUp.data());
#else
		T sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		auto coefficients = mCoefficientsUp.data();
		auto xFrame = &mXUp[mCursorUp];
		const int numLoops = mNumTaps >> 2; // n/4
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[0] = sum;
		sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		xFrame = &mXUp[mCursorUp];
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[1] = sum;
#endif
	}

	inline T tickDown(const T samples[]) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[0];

		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[1];
#if(__aarch64__)
		return arm_neon_dotproduct32_double(&mXDown[mCursorDown], mCoefficientsDown.data());
#elif defined(HAS_X86_SIMD)
#ifdef HAS_AVX
		if (cpu_has_avx()) {
			return dotproduct32_avx(&mXDown[mCursorDown], mCoefficientsDown.data());
		}
#endif
		return dotproduct32_sse2(&mXDown[mCursorDown], mCoefficientsDown.data());
#else
		T sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		auto coefficients = mCoefficientsDown.data();
		auto xFrame = &mXDown[mCursorDown];
		const int numLoops = mNumTaps >> 2; // n/4
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		return sum;
#endif
	}

	inline void tickUp4x(const T sample, T out[]) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--mCursorUp < 0) {
			mCursorUp = mNumTaps - 1;
		}
		mXUp[mCursorUp] = mXUp[mCursorUp + mNumTaps] = sample;
#if(__aarch64__)
		arm_neon_rs4x_32_double(out, &mXUp[mCursorUp], mCoefficientsUp.data());
#elif defined(HAS_X86_SIMD)
#ifdef HAS_AVX
		if (cpu_has_avx()) {
			upsample4x_avx(out, &mXUp[mCursorUp], mCoefficientsUp.data());
			return;
		}
#endif
		upsample4x_sse2(out, &mXUp[mCursorUp], mCoefficientsUp.data());
#else
		T sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		auto coefficients = mCoefficientsUp.data();
		auto xFrame = &mXUp[mCursorUp];
		const int numLoops = mNumTaps >> 2; // n/4
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[0] = sum;
		sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		xFrame = &mXUp[mCursorUp];
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[1] = sum;
		sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		xFrame = &mXUp[mCursorUp];
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[2] = sum;
		sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		xFrame = &mXUp[mCursorUp];
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		out[3] = sum;
#endif
	}

	inline T tickDown4x(const T samples[]) {
		// Move cursor before write so that cursor points to last written frame in read.
		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[0];

		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[1];

		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[2];

		if (--mCursorDown < 0) {
			mCursorDown = mNumTaps - 1;
		}
		mXDown[mCursorDown] = mXDown[mCursorDown + mNumTaps] = samples[3];
#if(__aarch64__)
		return arm_neon_dotproduct32_double(&mXDown[mCursorDown], mCoefficientsDown.data());
#elif defined(HAS_X86_SIMD)
#ifdef HAS_AVX
		if (cpu_has_avx()) {
			return dotproduct32_avx(&mXDown[mCursorDown], mCoefficientsDown.data());
		}
#endif
		return dotproduct32_sse2(&mXDown[mCursorDown], mCoefficientsDown.data());
#else
		alignas(ALIGN) T sum = 0.0;

		// Multiply input times precomputed windowed sinc function.
		auto coefficients = mCoefficientsDown.data();
		auto xFrame = &mXDown[mCursorDown];
		const int numLoops = mNumTaps >> 2; // n/4
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		return sum;
#endif
	}

	static void
		generateCoefficients(std::vector<T>& coeffs, int numTaps, int32_t inputRate, int32_t outputRate,
			T normalizedCutoff) {
		HyperbolicCosineWindow<T> win;
		IntegerRatio integerRatio(inputRate, outputRate);
		integerRatio.reduce();
		int32_t numRows = integerRatio.getDenominator();
		T phaseIncrement = (T)inputRate / (T)outputRate;
		coeffs.resize(numTaps * (numRows + 1));
		int coefficientIndex = 0;
		T phase = 0.0; // ranges from 0.0 to 1.0, fraction between samples
		// Stretch the sinc function for low pass filtering.

		const T cutoffScaler = outputRate < inputRate
			? normalizedCutoff * (T)outputRate / inputRate
			: normalizedCutoff;
		const int numTapsHalf = numTaps / 2; // numTaps must be even.
		const T numTapsHalfInverse = 1.0 / numTapsHalf;
		for (int i = 0; i <= numRows; i++) {
			T tapPhase = phase - numTapsHalf;
			T gain = 0.0; // sum of raw coefficients
			int gainCursor = coefficientIndex;
			for (int tap = 0; tap < numTaps; tap++) {
				T radians = tapPhase * PI_P;

				T window = win(tapPhase * numTapsHalfInverse);
				T coefficient = sinc(radians * cutoffScaler) * window;
				coeffs.at(coefficientIndex++) = coefficient;
				gain += coefficient;
				tapPhase += 1.0;
			}
			phase += phaseIncrement;
			while (phase >= 1.0) {
				phase -= 1.0;
			}

			// Correct for gain variations.
			T gainCorrection = 1.0 / gain; // normalize the gain
			for (int tap = 0; tap < numTaps; tap++) {
				coeffs.at(gainCursor + tap) *= gainCorrection;
			}
		}
	}

private:

	static T sinc(T radians) {
		if (std::abs(radians) < 1.0e-9) return 1.0;   // avoid divide by zero
		return sin(radians) / radians;   // Sinc function
	}

	const int _os;
	const int mNumTaps{ 32 };
	int mCursorUp{}, mCursorDown{};
	std::vector<T> mXUp, mXDown;           // delayed input values for the FIR
	std::vector<T> mCoefficientsUp, mCoefficientsDown;
	T upSampleBandwidth{}, downsampleBandwith{};
};

template<typename T, int numTaps = 32, int numRows = 250>
class ResamplerRounded : public PolyPhaseResampler<T, numTaps, numRows> {
	using Base = PolyPhaseResampler<T, numTaps, numRows>;
public:

	void init(T rate) override {
		Base::initRounded(rate);
	}
};

class ResamplerRoundedFloat64 : public PolyPhaseResampler<float, 64, 250> {
	using Base = PolyPhaseResampler<float, 64, 250>;
public:

protected:
	inline float readFrame() override {
		// Clear accumulator.
		// Multiply input times precomputed windowed sinc function.
		if (mRate == 1.)
			return mX[mCursor];
#if (__aarch64__)
		return arm_neon_dotproduct64_single(&Base::mX[mCursor],
			&Base::mCoefficients[static_cast<int>(mPhase * 250) %
			250][0]);

#else
		float sum = 0.0;
		auto coefficients = &Base::mCoefficients[static_cast<int>(mPhase * 250) % 250][0];
		auto xFrame = &Base::mX[mCursor];
		const int numLoops = 64 >> 2; // n/4
		for (int i = 0; i < numLoops; i++) {
			// Manual loop unrolling, might get converted to SIMD.
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
			sum += *xFrame++ * *coefficients++;
		}
		return sum;

#endif
	}
};


template<typename T, int numTaps = 32, int maxRows = 250>
class PolyPhaseSincResampler {
public:
	PolyPhaseSincResampler() = default;

	virtual void clear() {
		for (int i = 0; i < numTaps * 2; i++)mem[i] = 0;
		readPos = 0;
		mPhase = ceil(mRate);
	}

	virtual void init(T rate) {
		if (mRate == rate)
			return;
		const auto frac = rate - floor(rate);
		rows = 1;
		while (rows <= maxRows) {
			if (rows * frac - floor(rows * frac) < 1.e-9)
				break;
			rows++;
		}
		mRate = rate;
		interpolate = rows > maxRows;
		mPhase = ceil(mRate);
		generateCoefficients();
	}

	[[nodiscard]] bool isWriteNeeded() const {
		return mPhase >= 1.;
	}

	void write(const T sample) {
		if (--readPos < 0) {
			readPos = numTaps - 1;
		}
		mem[readPos] = mem[readPos + numTaps] = sample;
		mPhase -= 1;
	}

	T read() {
		auto ret = interpolate ? readPrivInterpolatedCubic() : readPriv();
		mPhase += mRate;
		return ret;
	}

protected:
	inline T readPrivInterpolatedCubic() {
		T phase = mPhase * maxRows;
		T floorphase = floor(phase);
		T frac = phase - floorphase;
		const int index = 2 + static_cast<int>(floorphase) % maxRows;
		const int indexM1 = index - 1;
		const int indexM2 = index - 2;
		const int indexP1 = index + 1;
		alignas(ALIGN) T interp[4];
		cubicCoeff(interp, frac);
		alignas(ALIGN)T sum1 {}, sum2{}, sum3{}, sum4{};
		for (int tap = 0; tap < numTaps; tap++) {
			sum1 += mem[readPos + tap] * mCoefficients[indexM2][tap];
			sum2 += mem[readPos + tap] * mCoefficients[indexM1][tap];
			sum3 += mem[readPos + tap] * mCoefficients[index][tap];
			sum4 += mem[readPos + tap] * mCoefficients[indexP1][tap];
		}
		return sum1 * interp[0] + sum2 * interp[1] + sum3 * interp[2] + sum4 * interp[3];
	}

	inline T readPrivInterpolatedLinear() {
		T phase = mPhase * maxRows;
		T floorphase = floor(phase);
		T frac = phase - floorphase;
		const int index = 2 + static_cast<int>(floorphase) % maxRows;
		const int index2 = index + 1;
		alignas(ALIGN)T sum1 {}, sum2{};
		for (int tap = 0; tap < numTaps; tap++) {
			sum1 += mem[readPos + tap] * mCoefficients[index][tap];
			sum2 += mem[readPos + tap] * mCoefficients[index2][tap];
		}
		return (sum1 + frac * (sum2 - sum1));
	}

	virtual inline T readPriv() {
		if (mRate == 1.)
			return mem[readPos];
		auto coefficients = mCoefficients[static_cast<int>(mPhase * rows) % rows];
		alignas(ALIGN)T sum = 0.0;
		auto ptr = &mem[readPos];

		const int numLoops = numTaps >> 2;
		for (int i = 0; i < numLoops; i++) {
			sum += *ptr++ * *coefficients++;
			sum += *ptr++ * *coefficients++;
			sum += *ptr++ * *coefficients++;
			sum += *ptr++ * *coefficients++;
		}
		return sum;
	}

	static constexpr T sinc(T radians) {
		if (std::abs(radians) < 1.0e-9) return 1.0;
		return sin(radians) / radians;
	}

	void generateCoefficients() {
		const T scale = (mRate > 1. ? downSampleBandWidth * 1. / mRate
			: upSampleBandWidth);
		const int numTapsHalf = numTaps / 2;
		const T numTapsHalfInverse = 1.0 / numTapsHalf;
		if (interpolate) {
			for (int i = 0; i < maxRows + 3; i++) {
				T tapPhase = (i - 2) / (T)maxRows - numTapsHalf;
				T gain = 0.0;
				for (int tap = 0; tap < numTaps; tap++) {
					T radians = tapPhase * PI_P;
					T coefficient =
						sinc(radians * scale) *
						hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
					mCoefficients[i][tap] = coefficient;
					gain += coefficient;
					tapPhase += 1.0;
				}
				T gainCorrection = 1.0 / gain; // Probably not needed
				for (int tap = 0; tap < numTaps; tap++) {
					mCoefficients[i][tap] *= gainCorrection;
				}
			}
		}
		else {
			for (int i = 0; i < rows; i++) {
				T tapPhase = i / (T)rows - numTapsHalf;
				T gain = 0.0;
				for (int tap = 0; tap < numTaps; tap++) {
					T radians = tapPhase * PI_P;
					T coefficient =
						sinc(radians * scale) *
						hyperbolicCosineWindow(tapPhase * numTapsHalfInverse);
					mCoefficients[i][tap] = coefficient;
					gain += coefficient;
					tapPhase += 1.0;
				}


				T gainCorrection = 1.0 / gain; // Probably not needed
				for (int tap = 0; tap < numTaps; tap++) {
					mCoefficients[i][tap] *= gainCorrection;
				}
			}
		}
	}

	/* The window class is from Google Oboe project. According to them this window gives less distortion */
	class HyperbolicCosineWindow {
	public:
		explicit HyperbolicCosineWindow(T att) {
			setStopBandAttenuation(att);
		}

		/**
		 * @param attenuation typical values range from 30 to 90 dB
		 * @return beta
		 */
		T setStopBandAttenuation(T attenuation) {
			T alpha = ((-325.1e-6 * attenuation + 0.1677) * attenuation) - 3.149;
			setAlpha(alpha);
			return alpha;
		}

		void setAlpha(T alpha) {
			mAlpha = alpha;
			mInverseCoshAlpha = 1.0 / cosh(alpha);
		}

		/**
		 * @param x ranges from -1.0 to +1.0
		 */
		T operator()(T x) {
			T x2 = x * x;
			if (x2 >= 1.0) return 0.0;
			T w = mAlpha * sqrt(1.0 - x2);
			return cosh(w) * mInverseCoshAlpha;
		}

	private:
		T mAlpha = 0.0;
		T mInverseCoshAlpha = 1.0;
	};

	HyperbolicCosineWindow hyperbolicCosineWindow;
	T mRate{};
	int32_t readPos{};
	T mPhase{ 1. };
	alignas(ALIGN) T mem[numTaps * 2]{};
	alignas(ALIGN) T mCoefficients[maxRows + 3][numTaps]{};
	const T upSampleBandWidth{ getUpSampleBandWidth(numTaps) }, downSampleBandWidth{
			getDownSampleBandWidth(numTaps) };
	bool interpolate{};
	int32_t rows{ 1 };

	struct QualityMapping {
		int base_length;
		T downsample_bandwidth;
		T upsample_bandwidth;
		T att;
	};

	static constexpr double getDownSampleBandWidth(const int taps) {
		double bw = quality_map[10].downsample_bandwidth;
		for (const auto& i : quality_map)
			if (taps <= i.base_length)
				bw = i.downsample_bandwidth;
		return bw;
	}

	static constexpr double getUpSampleBandWidth(const int taps) {
		double bw = quality_map[10].upsample_bandwidth;
		for (const auto& i : quality_map)
			if (taps <= i.base_length)
				bw = i.upsample_bandwidth;
		return bw;
	}

	static constexpr double getAtt(const int taps) {
		double att = quality_map[10].att;
		for (const auto& i : quality_map)
			if (taps <= i.base_length)
				att = i.att;
		return att;
	}

	/* The following is from Speex Resampler. Seems plausible */
	static constexpr const struct QualityMapping quality_map[11] = {
			{8,   0.830, 0.860, 60}, /* Q0 */
			{16,  0.850, 0.880, 60}, /* Q1 */
			{32,  0.882, 0.910, 60}, /* Q2 */  /* 82.3% cutoff ( ~60 dB stop) 6  */
			{48,  0.895, 0.917, 80}, /* Q3 */  /* 84.9% cutoff ( ~80 dB stop) 8  */
			{64,  0.921, 0.940, 80}, /* Q4 */  /* 88.7% cutoff ( ~80 dB stop) 8  */
			{80,  0.922, 0.940, 100}, /* Q5 */  /* 89.1% cutoff (~100 dB stop) 10 */
			{96,  0.940, 0.945, 100}, /* Q6 */  /* 91.5% cutoff (~100 dB stop) 10 */
			{128, 0.950, 0.950, 100}, /* Q7 */  /* 93.1% cutoff (~100 dB stop) 10 */
			{160, 0.960, 0.960, 100}, /* Q8 */  /* 94.5% cutoff (~100 dB stop) 10 */
			{192, 0.968, 0.968, 100}, /* Q9 */  /* 95.5% cutoff (~100 dB stop) 10 */
			{256, 0.975, 0.975, 100}, /* Q10 */ /* 96.6% cutoff (~100 dB stop) 10 */
	};

};

#endif //GRAINSTORM_RESAMPLE_H