//
// Created by pr on 01.11.21.
//

#include "ModalReverb.h"
#include "grainstorm.h"
#include "app.h"

#ifdef HAS_X86_SIMD

#include <immintrin.h>
#include <stdint.h>

// SSE2 version - works on ALL x86-64 CPUs, processes 2 complex doubles per iteration
inline void phasorfilter_double(
	MYFLOAT* pre_scale,
	MYFLOAT** array_pointers,
	unsigned int count)
{
	auto _pre = array_pointers[0];
	auto ym_prev = array_pointers[1];
	auto aa = array_pointers[2];
	auto post = array_pointers[3];
	auto sum_real = array_pointers[4];
	auto sum_imag = array_pointers[5];

	__m128d pre = _mm_load1_pd(pre_scale);
	__m128d acc_real = _mm_setzero_pd();
	__m128d acc_imag = _mm_setzero_pd();

	for (size_t i = 0; i < count; i += 2) {
		// Load 2 complex values: [r0, i0, r1, i1]
		__m128d x0 = _mm_loadu_pd(&_pre[i * 2]);      // r0, i0
		__m128d x1 = _mm_loadu_pd(&_pre[i * 2 + 2]);  // r1, i1

		// Deinterleave: extract real and imaginary parts
		__m128d x_real = _mm_shuffle_pd(x0, x1, 0b00); // r0, r1
		__m128d x_imag = _mm_shuffle_pd(x0, x1, 0b11); // i0, i1

		// Scale input
		x_real = _mm_mul_pd(x_real, pre);
		x_imag = _mm_mul_pd(x_imag, pre);

		// Load dampening factors
		__m128d damp = _mm_loadu_pd(&aa[i]);

		// Load previous outputs
		__m128d yp0 = _mm_loadu_pd(&ym_prev[i * 2]);
		__m128d yp1 = _mm_loadu_pd(&ym_prev[i * 2 + 2]);

		__m128d yp_real = _mm_shuffle_pd(yp0, yp1, 0b00);
		__m128d yp_imag = _mm_shuffle_pd(yp0, yp1, 0b11);

		// Apply feedback: y = x + y_prev * damp
		// x_real += yp_real * damp
		x_real = _mm_add_pd(x_real, _mm_mul_pd(yp_real, damp));
		x_imag = _mm_add_pd(x_imag, _mm_mul_pd(yp_imag, damp));

		// Load post-filter coefficients
		__m128d p0 = _mm_loadu_pd(&post[i * 2]);
		__m128d p1 = _mm_loadu_pd(&post[i * 2 + 2]);

		__m128d p_real = _mm_shuffle_pd(p0, p1, 0b00);
		__m128d p_imag = _mm_shuffle_pd(p0, p1, 0b11);

		// Complex multiplication: (x_real + i*x_imag) * (p_real + i*p_imag)
		// res_real = x_real * p_real - x_imag * p_imag
		// res_imag = x_real * p_imag + x_imag * p_real
		__m128d res_real = _mm_sub_pd(_mm_mul_pd(x_real, p_real),
			_mm_mul_pd(x_imag, p_imag));
		__m128d res_imag = _mm_add_pd(_mm_mul_pd(x_real, p_imag),
			_mm_mul_pd(x_imag, p_real));

		// Interleave and store results
		__m128d res0 = _mm_unpacklo_pd(res_real, res_imag); // r0, i0
		__m128d res1 = _mm_unpackhi_pd(res_real, res_imag); // r1, i1

		_mm_storeu_pd(&ym_prev[i * 2], res0);
		_mm_storeu_pd(&ym_prev[i * 2 + 2], res1);

		// Accumulate sums
		acc_real = _mm_add_pd(acc_real, res_real);
		acc_imag = _mm_add_pd(acc_imag, res_imag);
	}

	// Horizontal sum for SSE2
	__m128d sum_r = _mm_add_sd(acc_real, _mm_unpackhi_pd(acc_real, acc_real));
	_mm_store_sd(sum_real, sum_r);

	__m128d sum_i = _mm_add_sd(acc_imag, _mm_unpackhi_pd(acc_imag, acc_imag));
	_mm_store_sd(sum_imag, sum_i);
}

inline void phasorfilterhold_double( //sse2
	MYFLOAT* pre_scale,
	MYFLOAT** array_pointers,
	unsigned int count)
{
	auto _pre = array_pointers[0];
	auto ym_prev = array_pointers[1];
	auto aa = array_pointers[2];
	auto post = array_pointers[3];
	auto sum_real = array_pointers[4];
	auto sum_imag = array_pointers[5];

	__m128d acc_real = _mm_setzero_pd();
	__m128d acc_imag = _mm_setzero_pd();

	for (size_t i = 0; i < count; i += 2) {
		// Load previous outputs (no input scaling in hold mode)
		__m128d yp0 = _mm_loadu_pd(&ym_prev[i * 2]);
		__m128d yp1 = _mm_loadu_pd(&ym_prev[i * 2 + 2]);

		__m128d yp_real = _mm_shuffle_pd(yp0, yp1, 0b00);
		__m128d yp_imag = _mm_shuffle_pd(yp0, yp1, 0b11);

		// Load post-filter coefficients
		__m128d p0 = _mm_loadu_pd(&post[i * 2]);
		__m128d p1 = _mm_loadu_pd(&post[i * 2 + 2]);

		__m128d p_real = _mm_shuffle_pd(p0, p1, 0b00);
		__m128d p_imag = _mm_shuffle_pd(p0, p1, 0b11);

		// Complex multiplication
		__m128d res_real = _mm_sub_pd(_mm_mul_pd(yp_real, p_real),
			_mm_mul_pd(yp_imag, p_imag));
		__m128d res_imag = _mm_add_pd(_mm_mul_pd(yp_real, p_imag),
			_mm_mul_pd(yp_imag, p_real));

		// Interleave and store
		__m128d res0 = _mm_unpacklo_pd(res_real, res_imag);
		__m128d res1 = _mm_unpackhi_pd(res_real, res_imag);

		_mm_storeu_pd(&ym_prev[i * 2], res0);
		_mm_storeu_pd(&ym_prev[i * 2 + 2], res1);

		// Accumulate
		acc_real = _mm_add_pd(acc_real, res_real);
		acc_imag = _mm_add_pd(acc_imag, res_imag);
	}

	// Horizontal sum
	__m128d sum_r = _mm_add_sd(acc_real, _mm_unpackhi_pd(acc_real, acc_real));
	_mm_store_sd(sum_real, sum_r);

	__m128d sum_i = _mm_add_sd(acc_imag, _mm_unpackhi_pd(acc_imag, acc_imag));
	_mm_store_sd(sum_imag, sum_i);
}

#if defined(__AVX2__) && (defined(__FMA__) || defined(_MSC_VER))
#define MODALREV_HAS_AVX2_KERNELS 1

// Complex multiplication: (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
inline void complex_mul_pd(__m256d ar, __m256d ai, __m256d br, __m256d bi,
	__m256d& outr, __m256d& outi) {
	outr = _mm256_sub_pd(_mm256_mul_pd(ar, br), _mm256_mul_pd(ai, bi));
	outi = _mm256_add_pd(_mm256_mul_pd(ar, bi), _mm256_mul_pd(ai, br));
}

// AVX2 version - processes 4 complex doubles per iteration
inline void phasorfilter_double_avx2(
	MYFLOAT* pre_scale,
	MYFLOAT** array_pointers,
	unsigned int count)
{
	auto _pre = array_pointers[0];
	auto ym_prev = array_pointers[1];
	auto aa = array_pointers[2];
	auto post = array_pointers[3];
	auto sum_real = array_pointers[4];
	auto sum_imag = array_pointers[5];

	__m256d pre = _mm256_broadcast_sd(pre_scale);
	__m256d acc_real = _mm256_setzero_pd();
	__m256d acc_imag = _mm256_setzero_pd();

	for (size_t i = 0; i < count; i += 4) {
		// Load input complex values (interleaved: r0,i0,r1,i1,r2,i2,r3,i3)
		__m256d x0_ri = _mm256_loadu_pd(&_pre[i * 2]);
		__m256d x1_ri = _mm256_loadu_pd(&_pre[i * 2 + 4]);

		// Deinterleave real and imaginary parts
		__m256d x_real = _mm256_shuffle_pd(x0_ri, x1_ri, 0b0000); // r0,r1,r2,r3
		__m256d x_imag = _mm256_shuffle_pd(x0_ri, x1_ri, 0b1111); // i0,i1,i2,i3
		x_real = _mm256_permute4x64_pd(x_real, 0b11011000);
		x_imag = _mm256_permute4x64_pd(x_imag, 0b11011000);

		// Scale input
		x_real = _mm256_mul_pd(x_real, pre);
		x_imag = _mm256_mul_pd(x_imag, pre);

		// Load dampening factors
		__m256d damp = _mm256_loadu_pd(&aa[i]);

		// Load previous outputs
		__m256d yp0_ri = _mm256_loadu_pd(&ym_prev[i * 2]);
		__m256d yp1_ri = _mm256_loadu_pd(&ym_prev[i * 2 + 4]);

		__m256d yp_real = _mm256_shuffle_pd(yp0_ri, yp1_ri, 0b0000);
		__m256d yp_imag = _mm256_shuffle_pd(yp0_ri, yp1_ri, 0b1111);
		yp_real = _mm256_permute4x64_pd(yp_real, 0b11011000);
		yp_imag = _mm256_permute4x64_pd(yp_imag, 0b11011000);

		// Apply feedback: y = x + y_prev * damp
		x_real = _mm256_fmadd_pd(yp_real, damp, x_real);
		x_imag = _mm256_fmadd_pd(yp_imag, damp, x_imag);

		// Load post-filter coefficients
		__m256d p0_ri = _mm256_loadu_pd(&post[i * 2]);
		__m256d p1_ri = _mm256_loadu_pd(&post[i * 2 + 4]);

		__m256d p_real = _mm256_shuffle_pd(p0_ri, p1_ri, 0b0000);
		__m256d p_imag = _mm256_shuffle_pd(p0_ri, p1_ri, 0b1111);
		p_real = _mm256_permute4x64_pd(p_real, 0b11011000);
		p_imag = _mm256_permute4x64_pd(p_imag, 0b11011000);

		// Complex multiplication: result = y * post
		__m256d res_real, res_imag;
		complex_mul_pd(x_real, x_imag, p_real, p_imag, res_real, res_imag);

		// Interleave and store results: unpack gives (r0,i0,r2,i2)/(r1,i1,r3,i3),
		// the 128-bit halves must be recombined to (r0,i0,r1,i1)/(r2,i2,r3,i3)
		// so the load deinterleave recovers this state on the next sample
		__m256d res0 = _mm256_unpacklo_pd(res_real, res_imag);
		__m256d res1 = _mm256_unpackhi_pd(res_real, res_imag);

		_mm256_storeu_pd(&ym_prev[i * 2], _mm256_permute2f128_pd(res0, res1, 0x20));
		_mm256_storeu_pd(&ym_prev[i * 2 + 4], _mm256_permute2f128_pd(res0, res1, 0x31));

		// Accumulate sums
		acc_real = _mm256_add_pd(acc_real, res_real);
		acc_imag = _mm256_add_pd(acc_imag, res_imag);
	}

	// Horizontal sum
	__m128d sum_low = _mm256_castpd256_pd128(acc_real);
	__m128d sum_high = _mm256_extractf128_pd(acc_real, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	sum_low = _mm_hadd_pd(sum_low, sum_low);
	_mm_store_sd(sum_real, sum_low);

	sum_low = _mm256_castpd256_pd128(acc_imag);
	sum_high = _mm256_extractf128_pd(acc_imag, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	sum_low = _mm_hadd_pd(sum_low, sum_low);
	_mm_store_sd(sum_imag, sum_low);
}

// Hold version - uses previous output without input scaling
inline void phasorfilterhold_double_avx2(
	MYFLOAT* pre_scale,
	MYFLOAT** array_pointers,
	unsigned int count)
{

	auto _pre = array_pointers[0];
	auto ym_prev = array_pointers[1];
	auto aa = array_pointers[2];
	auto post = array_pointers[3];
	auto sum_real = array_pointers[4];
	auto sum_imag = array_pointers[5];

	__m256d acc_real = _mm256_setzero_pd();
	__m256d acc_imag = _mm256_setzero_pd();

	for (size_t i = 0; i < count; i += 4) {
		// Load previous outputs (no input scaling in hold mode)
		__m256d yp0_ri = _mm256_loadu_pd(&ym_prev[i * 2]);
		__m256d yp1_ri = _mm256_loadu_pd(&ym_prev[i * 2 + 4]);

		__m256d yp_real = _mm256_shuffle_pd(yp0_ri, yp1_ri, 0b0000);
		__m256d yp_imag = _mm256_shuffle_pd(yp0_ri, yp1_ri, 0b1111);
		yp_real = _mm256_permute4x64_pd(yp_real, 0b11011000);
		yp_imag = _mm256_permute4x64_pd(yp_imag, 0b11011000);

		// Load post-filter coefficients
		__m256d p0_ri = _mm256_loadu_pd(&post[i * 2]);
		__m256d p1_ri = _mm256_loadu_pd(&post[i * 2 + 4]);

		__m256d p_real = _mm256_shuffle_pd(p0_ri, p1_ri, 0b0000);
		__m256d p_imag = _mm256_shuffle_pd(p0_ri, p1_ri, 0b1111);
		p_real = _mm256_permute4x64_pd(p_real, 0b11011000);
		p_imag = _mm256_permute4x64_pd(p_imag, 0b11011000);

		// Complex multiplication
		__m256d res_real, res_imag;
		complex_mul_pd(yp_real, yp_imag, p_real, p_imag, res_real, res_imag);

		// Interleave and store (same halves recombination as the filter version)
		__m256d res0 = _mm256_unpacklo_pd(res_real, res_imag);
		__m256d res1 = _mm256_unpackhi_pd(res_real, res_imag);

		_mm256_storeu_pd(&ym_prev[i * 2], _mm256_permute2f128_pd(res0, res1, 0x20));
		_mm256_storeu_pd(&ym_prev[i * 2 + 4], _mm256_permute2f128_pd(res0, res1, 0x31));

		// Accumulate
		acc_real = _mm256_add_pd(acc_real, res_real);
		acc_imag = _mm256_add_pd(acc_imag, res_imag);
	}

	// Horizontal sum
	__m128d sum_low = _mm256_castpd256_pd128(acc_real);
	__m128d sum_high = _mm256_extractf128_pd(acc_real, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	sum_low = _mm_hadd_pd(sum_low, sum_low);
	_mm_store_sd(sum_real, sum_low);

	sum_low = _mm256_castpd256_pd128(acc_imag);
	sum_high = _mm256_extractf128_pd(acc_imag, 1);
	sum_low = _mm_add_pd(sum_low, sum_high);
	sum_low = _mm_hadd_pd(sum_low, sum_low);
	_mm_store_sd(sum_imag, sum_low);
}
#endif

#endif

#if defined(__aarch64__)
static void (*const func[2])(MYFLOAT*, MYFLOAT**, unsigned int) = { phasorfilter_double, phasorfilterhold_double };
#elif defined(MODALREV_HAS_AVX2_KERNELS)
static void (*const func[2])(MYFLOAT*, MYFLOAT**, unsigned int) = { phasorfilter_double_avx2, phasorfilterhold_double_avx2 };
#elif defined(HAS_X86_SIMD)
static void (*const func[2])(MYFLOAT*, MYFLOAT**, unsigned int) = { phasorfilter_double, phasorfilterhold_double };
#endif


ModalReverb::~ModalReverb() = default;

ModalReverb::ModalReverb() {
	int64_t offset = 0;
	offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(offset)
		offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(offset)
		offset += MAXM * sizeof(MYFLOAT);
	AA(offset)
		offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(offset)
		offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(offset)
		offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(offset)
		offset += MAXM * sizeof(MYFLOAT);
	AA(offset)

		buf.resize(offset);
	auto _buf = buf.data(); // static_cast<unsigned char*>(aligned_malloc(offset));


	int64_t _offset = 0;
	_pre = reinterpret_cast<std::complex<MYFLOAT> *>(_buf);
	_offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(_offset)
		_ym_prev = reinterpret_cast<std::complex<MYFLOAT> *>(_buf + _offset);
	_offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(_offset)
		_aa = reinterpret_cast<MYFLOAT*>(_buf + _offset);
	_offset += MAXM * sizeof(MYFLOAT);
	AA(_offset)
		_post = reinterpret_cast<std::complex<MYFLOAT> *>(_buf + _offset);
	_offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(_offset)
		_gain = reinterpret_cast<std::complex<MYFLOAT> *>(_buf + _offset);
	_offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(_offset)
		_w = reinterpret_cast<std::complex<MYFLOAT> *>(_buf + _offset);
	_offset += MAXM * sizeof(std::complex<MYFLOAT>);
	AA(_offset)
		_helpbuf = reinterpret_cast<MYFLOAT*>(_buf + _offset);
	_offset += MAXM * sizeof(MYFLOAT);
	AA(_offset)

		if (_offset != offset) {
			LOGE("MEMORY ERROR.");
		};

	releaseTimes.resize(MAXM, 0);
	randomfreqs.resize(MAXM, 0);
	tmp1.resize(MAXM, 0);
	for (auto* v : { &lwA, &lwB, &rtA, &rtB, &gpA, &gpB, &phA, &phD })
		v->resize(MAXM, 0);
	//setUpFrequencies();
}


// MATLAB smooth(y, frac, 'loess') on a uniform grid: local weighted
// QUADRATIC regression, tricube weights over the ns nearest neighbors
// (Cleveland's sliding neighborhood). The original .m uses 'loess', not
// 'lowess' - the previous CppLowess port fitted local LINEAR models, which
// biases the curved RT contour. in must not alias out.
static void
loess_quad(const MYFLOAT* in, MYFLOAT* out, int32_t n, MYFLOAT frac) {
	int32_t ns = (int32_t)(frac * (MYFLOAT)n);
	if (ns > n) ns = n;
	if (ns < 4) ns = 4;
	int32_t nleft = 0;
	for (int32_t i = 0; i < n; i++) {
		while (nleft + ns < n && i - nleft > nleft + ns - i)
			nleft++;
		const int32_t nright = nleft + ns - 1;
		const MYFLOAT h = (MYFLOAT)std::max(i - nleft, nright - i);
		MYFLOAT m0 = 0, m1 = 0, m2 = 0, m3 = 0, m4 = 0, r0 = 0, r1 = 0, r2 = 0;
		for (int32_t j = nleft; j <= nright; j++) {
			const MYFLOAT xi = (MYFLOAT)(j - i);
			MYFLOAT u = std::abs(xi) / h;
			MYFLOAT w = 1. - u * u * u;
			if (w <= 0.) continue;
			w = w * w * w;
			const MYFLOAT wx = w * xi, wxx = wx * xi;
			m0 += w; m1 += wx; m2 += wxx; m3 += wxx * xi; m4 += wxx * xi * xi;
			r0 += w * in[j]; r1 += wx * in[j]; r2 += wxx * in[j];
		}
		const MYFLOAT A = m2 * m4 - m3 * m3, B = m1 * m4 - m2 * m3, C = m1 * m3 - m2 * m2;
		const MYFLOAT det = m0 * A - m1 * B + m2 * C;
		if (std::abs(det) > 1e-10 * m0 * m2 * m2 + 1e-300)
			out[i] = (r0 * A - r1 * B + r2 * C) / det;
		else
			out[i] = r0 / m0;   // degenerate window: weighted mean
	}
}

void ModalReverb::filter(MYFLOAT* input, int32_t size) {
	for (int32_t i = 0; i < size; i++) {
		MYFLOAT tmp = input[i];
		std::complex<MYFLOAT> out = 0;
		for (int32_t mode = 0; mode < M; mode++) {
			out += (_ym_prev[mode] = _gain[mode] * tmp + _w[mode] * _ym_prev[mode]);
		}
		input[i] += out.real() / (MYFLOAT)M;
	}
}

// One complete random space: frequencies (as log of rad/sample, sorted),
// smoothed release-time contour, gain magnitudes and phases - the random
// parts of the original .m, factored out so setUp can roll TWO sets for
// MORPH. Fresh roll every setup, by choice: freezing a seed and evening the
// spacing out were both tried and rejected by ear; the clumps, their slow
// beating, and the per-setup variation are part of the character.
void ModalReverb::rollModeSet(MYFLOAT sr, std::mt19937& gen, MYFLOAT* lw, MYFLOAT* rt,
	MYFLOAT* gp, MYFLOAT* ph) {
	std::uniform_real_distribution<double> dist{0.0, 1.0};
	auto rnd = [&] { return (MYFLOAT)dist(gen); };

	int32_t modesperoctave[8] = { 1, 1, 2, 4, 8, 16, 32, 64 };

	if (M >= _modes[1]) {
		for (int32_t i = 0; i < 8; i++) modesperoctave[i] = _modesperoctave[i];
	}

	for (int32_t k = 2; M >= _modes[k]; k++) {
		for (auto& x : modesperoctave)
			x *= 2;
		if (k + 1 >= ARRAY_LEN(_modes))
			break;
	}

	int32_t sum = 0;
	for (auto x : modesperoctave)
		sum += x;

	while (sum++ < M)
		modesperoctave[7]++;

	int32_t index = 0;
	for (int32_t i = 0; i < 8; i++) {
		int32_t j;
		for (j = 0; j < modesperoctave[i]; j++, index++) {
			if (index >= M) {
				break;
			}
			MYFLOAT temp = (_f2[i] - _f1[i]) * rnd() + _f1[i];
			tmp1[index] = TWOPI_P * temp / sr;
		}
	}

	std::sort(tmp1.begin(), tmp1.begin() + M);
	for (int32_t i = 0; i < M; i++)
		lw[i] = log(tmp1[i]);

	// r_begin / smooth_factor in the original .m are indexed by MODE COUNT
	// (l = which of 128..8192), NOT by reverb time: the lowest r_begin[li]
	// modes ring the full reverb time and the loess span is 0.04/0.02 for
	// the mode counts we ship. (An old port compared r_begin against the
	// delay in seconds -> 1..7 head modes and a 5-10x too wide span.)
	int32_t li = 0;
	for (int32_t k = 1; k < (int32_t)ARRAY_LEN(_modes); k++)
		if (M >= _modes[k]) li = k;
	const int32_t nhead = _r_begin[li];
	const MYFLOAT smoothfact = _smoothfactors[li];

	for (int32_t i = 0; i < M; i++) {
		if (i < nhead)
			rt[i] = 1.;
		else
			rt[i] = (.1 * rnd() + .9) * exp(-4. * i / (MYFLOAT)(M - 1));
	}

	// RT = smooth(RT, smooth_factor(l), 'loess'); reads raw data (no alias),
	// and loess can overshoot at the edges - a release time <= 0 would flip
	// exp(-log(1000)/(rt * sr * T)) to >= 1 and make that mode unstable
	loess_quad(rt, tmp1.data(), M, smoothfact);
	for (int32_t i = 0; i < M; i++)
		rt[i] = tmp1[i] < (MYFLOAT)1e-4 ? (MYFLOAT)1e-4 : tmp1[i];

	int32_t low = (int)round((MYFLOAT)M / 25.);
	for (int32_t i = 0; i < low; i++) {
		gp[i] = .3 * rnd() + .7;
	}
	for (int32_t i = low; i < M; i++) {
		gp[i] = .89 * rnd() + .01;
	}
	for (int32_t i = 0; i < M; i++) {
		gp[i] *= sqrt(pow(10., i * -.5 / (MYFLOAT)(M - 1)));
	}

	auto max = *std::max_element(gp, gp + M);
	for (int32_t i = 0; i < M; i++)
		gp[i] /= max;

	for (int32_t i = 0; i < M; i++)
		ph[i] = rnd() * TWOPI_F_P;
}

// Recompute the live per-mode coefficients for [from, to) at morph position
// x: rank-paired lerp of sets A/B (log frequency, release time, gain,
// shortest-path phase), then damping from the reverb time and rotation from
// the pitch factor. Called for the whole bank on setup, and by the workers
// as a rolling per-chunk refresh that tracks the live params.
void ModalReverb::refreshModes(int32_t from, int32_t to, MYFLOAT sr, MYFLOAT reverbTime,
	MYFLOAT x, MYFLOAT pitch) {
	const MYFLOAT k = log(1000.) / ((sr / os) * reverbTime);
	for (int32_t i = from; i < to; i++) {
		const MYFLOAT w = exp(lwA[i] + x * (lwB[i] - lwA[i]));
		const MYFLOAT rt = rtA[i] + x * (rtB[i] - rtA[i]);
		const MYFLOAT gp = gpA[i] + x * (gpB[i] - gpA[i]);
		const MYFLOAT phi = phA[i] + x * phD[i];
		randomfreqs[i] = w;
		releaseTimes[i] = rt;
		_aa[i] = exp(-k / rt);
		_post[i] = exp(I * (w * pitch));
		// the original injects g = gp*exp(i*phase) per mode; the live kernel
		// computes (pre*x + aa*ym)*post, so pre = g*exp(-i*w*pitch) makes the
		// injection pre*post == g. _comp restores the tail energy the random
		// amplitudes remove - a constant factor, since the .m normalizes its
		// output offline - keeping the WET calibration.
#if GS_MODALREV_ADAPTIVE
		if (!_isAdaptive.empty() && _isAdaptive[i]) {
			// gp is 0/1 here (allocated or silent). A mode sitting exactly on
			// its partial reaches |pre|/(1-aa), and the bank output is scaled
			// by _g = os/M, so this lands each tracked partial at _adaptAmp
			// independently of RT60 and of how many modes are active. No _comp:
			// that factor exists to restore the energy the RANDOM gain
			// distribution removes, and this branch sets the level directly.
			_pre[i] = gp * _adaptAmp * (1. - _aa[i]) * (MYFLOAT)M / (MYFLOAT)os
				* exp(I * (phi - w * pitch));
		} else {
			_pre[i] = _comp * gp * _randAmp * exp(I * (phi - w * pitch));
		}
#else
		_pre[i] = _comp * gp * exp(I * (phi - w * pitch));
#endif
	}
}

void ModalReverb::setUp(MYFLOAT sr, MYFLOAT modes, MYFLOAT reverbTime, MYFLOAT overs) {
	os = overs;
	M = modes;
	_g = os / (MYFLOAT)modes;

	clear();

	// local generator (not the shared static) so concurrent setups don't race
	std::mt19937 gen{std::random_device()()};

	rollModeSet(sr, gen, lwA.data(), rtA.data(), gpA.data(), phA.data());
	rollModeSet(sr, gen, lwB.data(), rtB.data(), gpB.data(), phD.data());

	// phD -> shortest-path phase delta A->B so the morph lerp never takes
	// the long way around the circle
	for (int32_t i = 0; i < M; i++) {
		MYFLOAT d = phD[i] - phA[i];
		phD[i] = d - TWOPI_P * floor(d / TWOPI_P + .5);
	}

	// wet-level compensation (see refreshModes); both sets are statistically
	// identical, so computed from A once and held fixed across the morph.
	// Computed BEFORE the adaptive partition zeroes any gains, so _comp keeps
	// meaning the same thing it always did and PURITY 0 stays bit-identical.
	MYFLOAT sq = 0;
	for (int32_t i = 0; i < M; i++)
		sq += gpA[i] * gpA[i];
	_comp = 1. / sqrt(sq / (MYFLOAT)M);

	// Adaptive MODES (10..12): the first half of each worker's M/4 block is
	// handed to the partial tracker, the second half keeps the random roll so
	// PURITY can blend resynthesis against the classic tail. Unallocated
	// tracker slots must be SILENT (gp 0) or they would ring as random modes.
#if GS_MODALREV_ADAPTIVE
	_isAdaptive.assign(M, (unsigned char)0);
	if (_oldadaptSetup) {
		const int32_t quarter = M / 4;
		for (int32_t w = 0; w < 4; w++)
			for (int32_t i = 0; i < quarter / 2; i++) {
				const int32_t sl = w * quarter + i;
				_isAdaptive[sl] = 1;
				gpA[sl] = gpB[sl] = 0.;
				rtA[sl] = rtB[sl] = 1.;
				phA[sl] = phD[sl] = 0.;
			}
	}
#endif

	// commit set A; the downsampled effect re-refreshes with the live
	// morph/pitch params right after
	refreshModes(0, M, sr, reverbTime, 0., 1.);

	// keep the base-class filter() arrays coherent
	for (int32_t i = 0; i < M; i++) {
		_gain[i] = _pre[i] / _comp;
		MYFLOAT tmp = log(1000.) / (releaseTimes[i] * (sr / os) * reverbTime);
		_w[i] = exp(I * randomfreqs[i] - tmp);
	}
}

void ModalReverb::setUpNonRand(MYFLOAT sr, MYFLOAT reverbTime, MYFLOAT modes) {
	M = modes;
	_g = os / (MYFLOAT)modes;

	clear();

	int32_t modesperoctave[8] = { 1, 1, 2, 4, 8, 16, 32, 64 };

	if (M >= _modes[1]) {
		for (int32_t i = 0; i < 8; i++) modesperoctave[i] = _modesperoctave[i];
	}

	for (int32_t k = 2; M >= _modes[k]; k++) {
		for (auto& x : modesperoctave)
			x *= 2;
		if (k + 1 >= ARRAY_LEN(_modes))
			break;
	}

	int32_t sum = 0;
	for (auto x : modesperoctave)
		sum += x;

	while (sum++ < M)
		modesperoctave[7]++;


	MYFLOAT start = _f1[0];
	MYFLOAT stop = _f2[7];

	MYFLOAT inc = (stop - start) / (MYFLOAT)M;
	for (int32_t i = 0; i < M; i++) {
		releaseTimes[i] = (M - i) / (MYFLOAT)M;//expf(-4. * i / (MYFLOAT) M);
		randomfreqs[i] = TWOPI_F_P * start / sr;
		start += inc;
	}

	/*
	for(int32_t i=0;i<M;i++){
		MYFLOAT temp = _f1[0] + exp(-4 + 4. * i / (MYFLOAT) M) * inc;
		releaseTimes[i] = expf(-4. * i / (MYFLOAT) M);
		randomfreqs[i] = TWOPI_F_P * temp / _STATE->sr;
	}
	std::sort(std::begin(randomfreqs), std::begin(randomfreqs) + M);
	smooth(randomfreqs, randomfreqs, M, .25);

	MYFLOAT smoothfact = _smoothfactors[0];
*/
	for (int32_t i = 0; i < M; i++) {
		MYFLOAT tmp = logf(1000.) / (releaseTimes[i] * (sr / os) * reverbTime);
		//MYFLOAT a = powf(10.0, (-3. / (RTT[i] * _STATE->sr)));
		_w[i] = exp(I * randomfreqs[i] - tmp);
	}

	for (int32_t i = 0; i < M; i++) {
		_helpbuf[i] = (M - i) / (MYFLOAT)M;
	}

	for (int32_t i = 0; i < M; i++) {
		MYFLOAT tmp = _helpbuf[i];
		MYFLOAT phase = get_random() * TWOPI_F_P;
		_gain[i] = tmp * exp(I * phase);
	}
}


void ModalReverb::setReleaseTime(MYFLOAT sr, MYFLOAT t) {
	for (int32_t i = 0; i < M; i++) {
		MYFLOAT tmp = log(1000.) / (releaseTimes[i] * (sr / os) * t);
		_aa[i] = exp(-tmp);

		//MYFLOAT a = powf(10.0, (-3. / (RTT[i] * _STATE->sr)));
		_w[i] = exp(I * randomfreqs[i] - tmp);
	}
};


ModalReverbDownSampled::ModalReverbDownSampled(TRACK* t) : Effect(t, SPACE_MODALREV, STEREOEFFECT) {
	_olddelay = *(_delay = &t->_STATE->params[t->index][MODALREVDELAY]);
	_oldmode = clampMode(*(_mode = &t->_STATE->params[t->index][MODALREVMODES]));
	_dry = &t->_STATE->params[t->index][MODALREVDRY];
	_wet = &t->_STATE->params[t->index][MODALREVWET];
	_bypass = &t->bypass[SPACE_MODALREV];
	_pitch = &t->_STATE->params[t->index][MODALREVPITCH];
	_width = &t->_STATE->params[t->index][MODALREVWIDTH];
	_morph = &t->_STATE->params[t->index][MODALREVMORPH];
	_duck = &t->_STATE->params[t->index][MODALREVDUCK];
#if GS_MODALREV_ADAPTIVE
	_purity = &t->_STATE->params[t->index][MODALREVPURITY];
	_oldhopf = (uint32_t)_oldmode >= HOPF_FIRST;
	_oldadapt = (uint32_t)_oldmode >= ADAPT_FIRST && !_oldhopf;
	_oldadaptSetup = _oldadapt;
#endif

	_isupdating = true;

	int32_t bufsize = t->_STATE->maxBufSize * sizeof(MYFLOAT);

	int64_t offset = 0;
	offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)
		offset += bufsize;
	AA(offset)

		_buf.resize(offset, 0);

	int64_t _offset = 0;
	inbuf = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outl[0] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outr[0] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outl[1] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outr[1] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outl[2] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outr[2] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outl[3] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)
		_outr[3] = reinterpret_cast<MYFLOAT*>(_buf.data() + _offset);
	_offset += bufsize;
	AA(_offset)

		if (_offset != offset)
			LOGE("WLSDNSKLÖDJÖ");
	_setupThread = std::thread(&ModalReverbDownSampled::setupFunc, this);

	for (int32_t i = 0; i < 4; i++) {
		threads[i] = std::thread(&ModalReverbDownSampled::threadFunc, this, i);
		//thread_datas[j * channels + i] = thread_data;
	}
	_setupSem.release();

}





void ModalReverbDownSampled::check() {
	// delay/pitch/morph changes flow through the workers' rolling refresh
	// (refreshModes); check only arms the mode-change fade and samples HOLD.
	// Not once the effect is being torn down: arming the fade is what
	// eventually releases _setupSem, and the destructor JOINS that thread on
	// the AUDIO THREAD - the fx queue is reaped inside the per-track
	// processing loop (fx_queue.del in granulate.cpp), so a setup pass caught
	// in flight (up to 4096 modes drawn from random_device) stalls audio for
	// its whole duration. destroyRequested is set a fade-out before
	// readyToDestroy, so blocking new passes here leaves the worker idle by
	// deletion time.
	if (!destroyRequested && clampMode(*_mode) != _oldmode) {
		_fadeinc = -_fadeconst;
	}
#if GS_MODALREV_ADAPTIVE
	// PURITY is a pure gain blend between the two mode groups and rides the
	// workers' rolling refresh - no rebuild. In an adaptive MODE the random
	// group is only M/2 modes, hence the root2 make-up.
	if (_oldadapt) {
		const MYFLOAT p = _purity->load();
		_adaptAmp = p;
		_randAmp = (1. - p) * (MYFLOAT)root2f;
		_hopfAmp = 0.;
	} else if (_oldhopf) {
		// the modal bank stays FULLY random here (no partition), so no root2
		const MYFLOAT p = _purity->load();
		_adaptAmp = 0.;
		_randAmp = 1. - p;
		_hopfAmp = p;
	} else {
		_adaptAmp = 0.;
		_randAmp = 1.;
		_hopfAmp = 0.;
	}
#endif
	hold =(int)_STATE->params[_track->index][MODALREVHOLD].load();
}

void ModalReverbDownSampled::threadFunc(int32_t num) {
	tprio(-19);
#ifdef HAS_X86_SIMD
	// FTZ+DAZ: the decaying mode recursions pass through the denormal range,
	// which stalls x86 FPUs; worker threads don't inherit the host's MXCSR
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	MYFLOAT* outl = _outl[num];
	MYFLOAT* outr = _outr[num];

	while (true) {
		sem[num].acquire();
		if (_quit) {
			break;
		}
		const int32_t size = M / 4;
		const int32_t offset = size * num;
		const uint32_t bufsize = (
#if defined PLUGIN_MODE || defined STANDALONE_MODE
			fillBufSize >> _shifts[(int)_oldmode]);
#else
			_STATE->currentBufSize >> _shifts[(int)_oldmode]);
#endif
		{
			// rolling refresh: recompute a window of this worker's modes from
			// the live MORPH/PITCH/T60 params; a full pass over the bank
			// completes every ~16 chunks (~20 ms at 48k/64), which both makes
			// morphs glide and replaces the old audio-thread M-sized rewrites
			// on pitch/delay changes
			const MYFLOAT x = _morph->load();
			const MYFLOAT pitch = pow(2, _pitch->load() / 12.);
			const MYFLOAT T = LOG2NORMALF(_delay->load());
			int32_t chunk = size / 16;
			if (chunk < 1) chunk = size;
			int32_t from = offset + _refreshPos[num];
			int32_t to = from + chunk;
			if (to > offset + size) to = offset + size;
			refreshModes(from, to, _STATE->sr, T, x, pitch);
			_refreshPos[num] = (to - offset) % size;
		}
#if defined(__aarch64__) || defined(HAS_X86_SIMD)
		auto ff = func[hold];
		MYFLOAT yl{}, yr{};
		MYFLOAT* array[6] = { &tester[0][offset * 2], &tester[1][offset * 2], &tester[2][offset],
							 &tester[3][offset * 2], &yl, &yr };
		for (int32_t i = 0; i < bufsize; i++) {
			ff(&inbuf[i], &array[0], size);
			outl[i] = yl;
			outr[i] = yr;
		}
#else
		const int32_t h = hold;
		for (int32_t i = 0; i < bufsize; i++) {
			std::complex<MYFLOAT> out{};
			if (h) {
				for (int32_t mode = offset; mode < offset + size; mode++) {
					out += (_ym_prev[mode] = _post[mode] * _ym_prev[mode]);
				}
				outl[i] = out.real();
				outr[i] = out.imag();
			}
			else {
				for (int32_t mode = offset; mode < offset + size; mode++) {
					out += (_ym_prev[mode] = (_pre[mode] * inbuf[i] + _aa[mode] * _ym_prev[mode]) * _post[mode]);
				}
				outl[i] = out.real();
				outr[i] = out.imag();
			}

		}
#endif
#if GS_MODALREV_ADAPTIVE
		// adaptive-oscillator pool (MODE 13..15): each worker runs its own
		// oscillator slice and ACCUMULATES into its outl/outr after the modal
		// kernel has written them. The (M/os) factor pre-cancels the _g scale
		// applied in compute's mix stage, so a tracked partial of amplitude a
		// comes back at _hopfAmp * a - the same calibration as MODE 10..12.
		// The pitch factor transposes only the pool's OUTPUT phasors: the
		// tracking side stays on the input, so PITCH shifts the tail without
		// breaking the locks.
		if (_hopfAmp > 0.) {
			const MYFLOAT T = LOG2NORMALF(_delay->load());
			const MYFLOAT pitch = pow(2, _pitch->load() / 12.);
			_hopf.processSlice(num, inbuf, outl, outr, bufsize, T, pitch,
				_hopfAmp * (MYFLOAT)M / (MYFLOAT)os, hold);
		}
#endif
		workerLatch.done();
	}
}

#if GS_MODALREV_ADAPTIVE
// Called under _setupMutex right after setUp(), so the pool layout always
// matches the partition setUp just wrote.
void ModalReverbDownSampled::setupAdaptive() {
	const MYFLOAT sr = _appState->sr;
	const int32_t quarter = M / 4;
	_adaptPerWorker = _oldadaptSetup ? quarter / 2 : 0;

	_ana.setUp(ADAPT_FFT, ADAPT_HOP);
	_tracker.setUp(sr, ADAPT_FFT);
	// the bank runs at sr/os - a partial above its Nyquist cannot be
	// represented at all, so at MODE 0..2 (os 4, 6 kHz ceiling) the tracker is
	// simply given a lower ceiling rather than the oversampling being forced
	const MYFLOAT bankNyq = .5 * sr / (MYFLOAT)os;
	_tracker.fMax = std::min((MYFLOAT)12000., bankNyq * (MYFLOAT).9);

	_alloc.setUp(M, _adaptPerWorker, 4, quarter);
	_slotAmp.assign(M, (MYFLOAT)0.);

	if (!_fft) _fft = std::make_unique<FFT>(ADAPT_FFT);
	_polar.assign(ADAPT_FFT, (MYFLOAT)0.);
	_mag.assign(ADAPT_FFT / 2 + 1, (MYFLOAT)0.);
}

// Retune one slot. Only ever called for a FRESH allocation - an adoption
// deliberately leaves the mode where it is.
void ModalReverbDownSampled::tuneAdaptiveSlot(int32_t slot, MYFLOAT freqHz) {
	const MYFLOAT sr = _appState->sr;
	const MYFLOAT w = TWOPI_P * freqHz / (sr / (MYFLOAT)os);
	lwA[slot] = lwB[slot] = log(w);
	rtA[slot] = rtB[slot] = 1.;
	gpA[slot] = gpB[slot] = 1.;
	phA[slot] = phD[slot] = 0.;
	// the slot we take is the quietest in its block by construction, so
	// clearing it costs less than bending a still-ringing resonator to a new
	// frequency would
	_ym_prev[slot] = 0.;
	// make it live now: waiting for the rolling refresh would leave the mode
	// at its old frequency for up to ~20 ms and then jump
	refreshModes(slot, slot + 1, sr, LOG2NORMALF(_delay->load()),
		_morph->load(), pow(2, _pitch->load() / 12.));
}

// Audio thread, called with _setupMutex held and the workers idle.
void ModalReverbDownSampled::updateAdaptive(const MYFLOAT* mono, int32_t n) {
	// gate on the pool, NOT on _adaptAmp: with the PURITY knob at 0 the
	// tracker keeps following the input so turning the knob up is instant
	if (_adaptPerWorker <= 0) return;

	const MYFLOAT sr = _appState->sr;
	const MYFLOAT T = LOG2NORMALF(_delay->load());
	// the tracker owns its own model of every slot's ring amplitude rather than
	// reading _ym_prev, which the workers are free to write: the ranking is all
	// that matters and this way there is no shared state at all
	const MYFLOAT dec = pow(10., -3. * ((MYFLOAT)ADAPT_HOP / sr) / (T > 0. ? T : 1.));

	_ana.push(mono, n, [&](const MYFLOAT* frame) {
		_fft->forwardPolar(frame, _polar.data());
		// forwardPolar packs [ |DC|, |Nyquist|, m1, p1, m2, p2, ... ]
		const int32_t nb = ADAPT_FFT / 2;
		_mag[0] = _polar[0];
		for (int32_t k = 1; k < nb; k++)
			_mag[k] = _polar[2 * k];
		_mag[nb] = _polar[1];

		_tracker.processSpectrum(_mag.data(), nb + 1);

		for (auto& a : _slotAmp) a *= dec;

		for (int32_t idx : _tracker.births()) {
			auto& p = _tracker.partials()[idx];
			const int32_t before = _alloc.allocations();
			const int32_t slot = _alloc.acquire(p.freq, _slotAmp.data());
			if (slot < 0) continue;
			p.user = slot;
			_slotAmp[slot] = 1.;
			if (_alloc.allocations() > before)
				tuneAdaptiveSlot(slot, p.freq);
		}
		for (int32_t slot : _tracker.deaths())
			_alloc.release(slot);
		// a live partial is still being driven by the input, so its slot must
		// not look quiet to the steal ranking
		for (auto& p : _tracker.partials())
			if (p.confirmed && p.user >= 0) _slotAmp[p.user] = 1.;
	});
}
#endif

void ModalReverbDownSampled::setupFunc() {
	while (true) {
		_setupSem.acquire();
		if (_quit)
			break;
		{
			std::lock_guard lk(_setupMutex);
			//  MYFLOAT sr, MYFLOAT modes, MYFLOAT reverbTime, MYFLOAT overs

			// _adaptAmp is latched with _oldmode, so setUp's partition and
			// setupAdaptive's pool layout always agree
			setUp(_appState->sr, _nummodes[(int)_oldmode], LOG2NORMALF(_olddelay), _oversvals[(int)_oldmode]);
#if GS_MODALREV_ADAPTIVE
			setupAdaptive();
			if (_oldhopf) {
				// pool runs at the worker rate (os is 1 in these modes)
				const uint32_t hi = (uint32_t)_oldmode - HOPF_FIRST;
				_hopf.setUp(_appState->sr / os,
					_hopfsizes[hi < 3 ? hi : 2], 4, 35., 11000.);
			}
#endif

			// bring the freshly rolled bank to the live param state at once -
			// the workers' rolling refresh would take ~20 ms to sweep the bank
			refreshModes(0, M, _appState->sr, LOG2NORMALF(_delay->load()),
				_morph->load(), pow(2, _pitch->load() / 12.));

			_STATE->params[_track->index][MODALREVHOLD].store(false);
			_STATE->toUiThreadQueue.try_push([this] {
				if (_STATE->parameters[MODALREVHOLD].view->visible_)
					_STATE->parameters[MODALREVHOLD].view->redraw();
			});
			_isupdating = false;
			_state = READY;
		}
	}
}

void
ModalReverbDownSampled::compute(MYFLOAT* inl, MYFLOAT* inr, MYFLOAT* outl, MYFLOAT* outr, int32_t s) {
	if (_state == STARTING) {
		for (int i = 0; i < s; i++) {
			outl[i] = inl[i];
			outr[i] = inr[i];
		}
		return;
	}
	const bool bypass = (_bypass->load() || destroyRequested);
	const MYFLOAT wet = bypass ? 0. : LOG2NORMALF(*_wet);
	const MYFLOAT dry = bypass ? 1. : LOG2NORMALF(*_dry);

	check();
	bool updating = _isupdating.load();
	const uint32_t shift = _shifts[(int)_oldmode];

	// DUCK: wet is attenuated while the dry input is loud and blooms back in
	// the gaps; peak follower with 5 ms attack / 300 ms release, soft knee
	// around -26 dBFS via env/(env+.05)
	const MYFLOAT duckamt = bypass ? 0. : _duck->load();
	const MYFLOAT datt = 1. - exp(-1. / (.005 * _STATE->sr));
	const MYFLOAT drel = 1. - exp(-1. / (.3 * _STATE->sr));

#if defined PLUGIN_MODE || defined STANDALONE_MODE
	const MYFLOAT spread = .5 * (1. - _width->load());
	const MYFLOAT orig = 1. - spread;

	int outputIdx = 0;  // Index into output arrays

	// Process all input samples
	for (int inputIdx = 0; inputIdx < s; inputIdx++) {
		// Add input sample to buffer (mono mix)
		fillBuf[fillBufPos++] = (inl[inputIdx] + inr[inputIdx]) * 0.5;

		// When we have a complete chunk, process it
		if (fillBufPos >= fillBufSize) {
			// ===== DOWNSAMPLE INPUT =====
			for (uint32_t i = 0; i < fillBufSize; i += os) {
				if (os == 2)
					inbuf[(i >> shift)] = resampler2XL.tickDown(&fillBuf[i]);
				else if (os == 4)
					inbuf[(i >> shift)] = resampler4xL.tickDown4x(&fillBuf[i]);
				else
					inbuf[i] = fillBuf[i];
			}

			// ===== PROCESS REVERB =====
			if (_setupMutex.try_lock()) {
				// analysis runs HERE, between acquiring the mutex and releasing
				// the workers: the setup thread is excluded and the workers are
				// still blocked on their semaphores, so retuning a mode cannot
				// race anything. Full-rate mono, not the downsampled inbuf, so
				// the 85 ms window does not change with MODE.
#if GS_MODALREV_ADAPTIVE
				updateAdaptive(fillBuf, fillBufSize);
#endif
				workerLatch.reset(4);
				for (auto& ss : sem)
					ss.release();
				workerLatch.wait();
				_setupMutex.unlock();
			}
			else {
				// Clear output if processing failed
				int processSize = fillBufSize >> shift;
				for (int index = 0; index < processSize; index++) {
					_outl[0][index] = _outl[1][index] = _outl[2][index] = _outl[3][index] = 0.;
					_outr[0][index] = _outr[1][index] = _outr[2][index] = _outr[3][index] = 0.;
				}
			}

			// ===== UPSAMPLE AND MIX OUTPUT =====
			for (uint32_t i = 0; i < fillBufSize; i += os) {
				uint32_t index = (i >> shift);
				MYFLOAT tmpl[4], tmpr[4];

				if (os == 2) {
					resampler2XL.tickUp(_outl[0][index] + _outl[1][index] + _outl[2][index] + _outl[3][index], tmpl);
					resampler2XR.tickUp(_outr[0][index] + _outr[1][index] + _outr[2][index] + _outr[3][index], tmpr);
				}
				else if (os == 4) {
					resampler4xL.tickUp4x(_outl[0][index] + _outl[1][index] + _outl[2][index] + _outl[3][index], tmpl);
					resampler4xR.tickUp4x(_outr[0][index] + _outr[1][index] + _outr[2][index] + _outr[3][index], tmpr);
				}
				else {
					tmpl[0] = _outl[0][i] + _outl[1][i] + _outl[2][i] + _outl[3][i];
					tmpr[0] = _outr[0][i] + _outr[1][i] + _outr[2][i] + _outr[3][i];
				}

				// Process each oversampled frame
				for (int32_t o = 0; o < os; o++) {
					MYFLOAT yl = tmpl[o], yr = tmpr[o];
					const MYFLOAT g = _smooth1 * _fade * _g;

					// Store in output buffer
					outputBufL[outputBufAvail] = ((yl * orig + yr * spread) * g);
					outputBufR[outputBufAvail] = ((yr * orig + yl * spread) * g);
					outputBufAvail++;

					smwetdry(wet, dry);

					// Handle fade
					if (_fade < 0) {
						_fadeinc = _fadeconst;
						_fade = 0;
						_oldmode = clampMode(*_mode);
						_olddelay = *_delay;
#if GS_MODALREV_ADAPTIVE
						_oldhopf = (uint32_t)_oldmode >= HOPF_FIRST;
						_oldadapt = (uint32_t)_oldmode >= ADAPT_FIRST && !_oldhopf;
						_oldadaptSetup = _oldadapt;
#endif
						/* see check(): no new bank build once the effect is
						   being torn down. NOT setting _isupdating matters -
						   it is what gates the fade back in, and only the
						   setup thread clears it, so raising it without
						   releasing the semaphore would freeze the fade. */
						if (!destroyRequested) {
							_isupdating = updating = true;
							_setupSem.release();
						}
					}
					else if (_fade > 1) {
						_fadeinc = 0;
						_fade = 1;
					}
					else {
						if (!updating)
							_fade += _fadeinc;
					}
				}
			}

			fillBufPos = 0;  // Reset input buffer
		}

		// Output samples as they become available
		if (outputBufAvail > 0 && outputIdx < s) {
			MYFLOAT dryGain = smdry();
			const MYFLOAT mono = std::abs((inl[outputIdx] + inr[outputIdx]) * (MYFLOAT).5);
			_duckEnv += (mono > _duckEnv ? datt : drel) * (mono - _duckEnv);
			const MYFLOAT duckg = 1. - duckamt * _duckEnv / (_duckEnv + (MYFLOAT).05);
			outl[outputIdx] = inl[outputIdx] * dryGain + outputBufL[outputBufPos] * duckg;
			outr[outputIdx] = inr[outputIdx] * dryGain + outputBufR[outputBufPos] * duckg;

			outputBufPos++;
			outputBufAvail--;
			outputIdx++;

			// Wrap output buffer position
			if (outputBufPos >= fillBufSize) {
				outputBufPos = 0;
			}
		}
	}

	// If we couldn't output all samples (shouldn't happen in steady state)
	// output dry signal for remaining samples
	while (outputIdx < s) {
		MYFLOAT dryGain = smdry();
		outl[outputIdx] = inl[outputIdx] * dryGain;
		outr[outputIdx] = inr[outputIdx] * dryGain;
		outputIdx++;
	}

#else

#if GS_MODALREV_ADAPTIVE
	if ((int32_t)_anaMono.size() < s)
		_anaMono.resize(s);
	for (int32_t i = 0; i < s; i++)
		_anaMono[i] = (inl[i] + inr[i]) * (MYFLOAT).5;
#endif

	for (uint32_t i = 0; i < s; i += os) {
		MYFLOAT tmp[4];
		for (int32_t o = 0; o < os; o++)
			tmp[o] = (inl[i + o] + inr[i + o]) * .5;
		if (os == 2) inbuf[(i >> shift)] = resampler2XL.tickDown(tmp);
		else if (os == 4)
			inbuf[(i >> shift)] = resampler4xL.tickDown4x(tmp);
		else
			inbuf[i] = tmp[0];
	}
	if (_setupMutex.try_lock()) {
#if GS_MODALREV_ADAPTIVE
		updateAdaptive(_anaMono.data(), s);
#endif
        workerLatch.reset(4);
		for (auto& ss : sem)
			ss.release();
        workerLatch.wait();
		_setupMutex.unlock();
	}
	else {
		for (int index = 0; index < s; index++)
			_outl[0][index] = _outl[1][index] = _outl[2][index] = _outl[3][index] = _outr[0][index] = _outr[1][index] =
			_outr[2][index] =
			_outr[3][index] = 0.;
	}

	const MYFLOAT spread = .5 * (1. - _width->load());
	const MYFLOAT orig = 1. - spread;
	for (uint32_t i = 0; i < s; i += os) {
		uint32_t index = (i >> shift);
		MYFLOAT tmpl[4], tmpr[4];

		if (os == 2) {
			resampler2XL.tickUp(_outl[0][index] + _outl[1][index] + _outl[2][index] +
				_outl[3][index], tmpl);
			resampler2XR.tickUp(_outr[0][index] + _outr[1][index] + _outr[2][index] +
				_outr[3][index], tmpr);
		}
		else if (os == 4) {
			resampler4xL.tickUp4x(_outl[0][index] + _outl[1][index] + _outl[2][index] +
				_outl[3][index], tmpl);
			resampler4xR.tickUp4x(_outr[0][index] + _outr[1][index] + _outr[2][index] +
				_outr[3][index], tmpr);
		}
		else {
			tmpl[0] = _outl[0][i] + _outl[1][i] + _outl[2][i] + _outl[3][i];
			tmpr[0] = _outr[0][i] + _outr[1][i] + _outr[2][i] + _outr[3][i];
		}

		for (int32_t o = 0; o < os; o++) {
			MYFLOAT yl = tmpl[o], yr = tmpr[o];
			const MYFLOAT g = _smooth1 * _fade * _g;

			const MYFLOAT mono = std::abs((inl[i + o] + inr[i + o]) * (MYFLOAT).5);
			_duckEnv += (mono > _duckEnv ? datt : drel) * (mono - _duckEnv);
			const MYFLOAT duckg = 1. - duckamt * _duckEnv / (_duckEnv + (MYFLOAT).05);

			inl[i + o] *= smdry();
			inr[i + o] *= smdry();
			inl[i + o] += ((yl * orig + yr * spread) * g) * duckg;
			inr[i + o] += ((yr * orig + yl * spread) * g) * duckg;
			//inl[i] += (yl * g);
			//inr[i] += (yr * g);
			smwetdry(wet, dry);

			if (_fade < 0) {
				_fadeinc = _fadeconst;
				_fade = 0;
				_oldmode = clampMode(*_mode);
				_olddelay = *_delay;
#if GS_MODALREV_ADAPTIVE
				_oldhopf = (uint32_t)_oldmode >= HOPF_FIRST;
				_oldadapt = (uint32_t)_oldmode >= ADAPT_FIRST && !_oldhopf;
				_oldadaptSetup = _oldadapt;
#endif
				/* see check(): no new bank build once the effect is being torn
				   down. NOT setting _isupdating matters - it is what gates the
				   fade back in, and only the setup thread clears it, so
				   raising it without releasing the semaphore would freeze the
				   fade. */
				if (!destroyRequested) {
					_isupdating = updating = true;
					_setupSem.release();
				}
			}
			else if (_fade > 1) {
				_fadeinc = 0;
				_fade = 1;
			}
			else {
				if (!updating)
					_fade += _fadeinc;
			}
		}
	}
#endif
}