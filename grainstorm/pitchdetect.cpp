//
// Created by pr on 29.06.20.
//

#include <algorithm>
#include <complex>
#include <map>
#include <tuple>
#include <vector>
#include "pitchdetect.h"


static constexpr const float freqs[] = {
        0.f, 20.f, 25.f, 31.5f, 40.f, 50.f, 63.f, 80.f, 100.f, 125.f,
        160.f, 200.f, 250.f, 315.f, 400.f, 500.f, 630.f, 800.f, 1000.f, 1250.f,
        1600.f, 2000.f, 2500.f, 3150.f, 4000.f, 5000.f, 6300.f, 8000.f, 9000.f, 10000.f,
        12500.f, 15000.f, 20000.f, 25100.f, 100000.f
};

static constexpr const float weight[] = {
        -75.8f, -70.1f, -60.8f, -52.1f, -44.2f, -37.5f, -31.3f, -25.6f, -20.9f, -16.5f,
        -12.6f, -9.60f, -7.00f, -4.70f, -3.00f, -1.80f, -0.80f, -0.20f, -0.00f, 0.50,
        1.60f, 3.20f, 5.40f, 7.80f, 8.10f, 5.30f, -2.40f, -11.1f, -12.8f, -12.2f,
        -7.40f, -17.8f, -17.8f, -17.8f
};

int
fvec_min_elem(float *s, int32_t length) {
    int32_t j, pos = 0;
    float tmp = s[0];
    for (j = 0; j < length; j++) {
        pos = (tmp < s[j]) ? pos : j;
        tmp = (tmp < s[j]) ? tmp : s[j];
    }
    return pos;
}

int32_t indexofSmallestElement(float array[], int size) {
    int32_t index = 0;

    for (int32_t i = 1; i < size; i++) {
        if (array[i] < array[index])
            index = i;
    }

    return index;
}


static float q_peak(const float *x, int32_t pos, int length) { //quadratic peak pos
    float s0, s1, s2;
    int32_t x0, x2;
    float half = .5, two = 2.f;
    if (pos == 0 || pos == length - 1) return pos;
    x0 = (pos < 1) ? pos : pos - 1;
    x2 = (pos + 1 < length) ? pos + 1 : pos;
    if (x0 == pos) return (x[pos] <= x[x2]) ? pos : x2;
    if (x2 == pos) return (x[pos] <= x[x0]) ? pos : x0;
    s0 = x[x0];
    s1 = x[pos];
    s2 = x[x2];
    return pos + half * (s0 - s2) / (s0 - two * s1 + s2);
}


inline static float fvec_quadratic_peak_pos(const float *x, int32_t pos, int n) {
    float s0, s1, s2;
    int32_t x0, x2;
    float half = .5f, two = 2.f;
    if (pos == 0 || pos == n - 1) return pos;
    x0 = (pos < 1) ? pos : pos - 1;
    x2 = (pos + 1 < n) ? pos + 1 : pos;
    if (x0 == pos) return (x[pos] <= x[x2]) ? pos : x2;
    if (x2 == pos) return (x[pos] <= x[x0]) ? pos : x0;
    s0 = x[x0];
    s1 = x[pos];
    s2 = x[x2];
    return pos + half * (s0 - s2) / (s0 - two * s1 + s2);
}


static float
pitchyin(const float *input, float *_yin, int32_t N) {
    const float tol = 0.15f;

    int32_t j, tau = 0;
    int32_t period;
    float tmp = 0.f, tmp2 = 0.f;
    _yin[0] = 1.f;
    for (tau = 1; tau < N; tau++) {
        _yin[tau] = 0.f;
        for (j = 0; j < N; j++) {
            tmp = input[j] - input[j + tau];
            _yin[tau] += SQR (tmp);
        }
        tmp2 += _yin[tau];
        if (tmp2 != 0) {
            _yin[tau] *= tau / tmp2;
        } else {
            _yin[tau] = 1.f;
        }
        period = tau - 3;
        if (tau > 4 && (_yin[period] < tol) &&
            (_yin[period] < _yin[period + 1])) {
            return fvec_quadratic_peak_pos(_yin, period, N);
        }
    }
    return fvec_quadratic_peak_pos(_yin, fvec_min_elem(_yin, N), N);
}



PitchDetectYinFFT::PitchDetectYinFFT(int32_t size, int sr) : fft(size) {
    M = size;
    N = size / 2;
    fftbuf.reserve(M);
    squaremag.resize(M);
    yinfft.resize(N);//[SPECTRAL_FILTER_FFTSIZE / 2];
    tol = 0.85;
    win.resize(M);//[SPECTRAL_FILTER_FFTSIZE];
    weight.resize(N + 1);//[SPECTRAL_FILTER_FFTSIZE / 2 + 1];
    int32_t i = 0, j = 1;
    float freq = 0, a0 = 0, a1 = 0, f0 = 0, f1 = 0;
    PVAmps::compute_hanning(win.data(), M);
    for (i = 0; i < N + 1; i++) {
        freq = (float) i / (float) M * (float) sr;
        while (freq > freqs[j]) {
            j += 1;
        }
        a0 = weight[j - 1];
        f0 = freqs[j - 1];
        a1 = weight[j];
        f1 = freqs[j];
        if (f0 == f1) {           // just in case
            weight[i] = a0;
        } else if (f0 == 0) {     // y = ax+b
            weight[i] = (a1 - a0) / f1 * freq + a0;
        } else {
            weight[i] = (a1 - a0) / (f1 - f0) * freq +
                        (a0 - (a1 - a0) / (f1 / f0 - 1.f));
        }
        while (freq > freqs[j]) {
            j += 1;
        }
        weight[i] = LOG2NORMALF (weight[i]);
    }
    // check for octave errors above 1300 Hz
    short_p = (int) round(sr / 1300.);
}


#define ISSMALLER(a, b) (a) < (b) ? (a) : (b)


float PitchDetectYinFFT::compute(const float *in) {



    float pitch;
    int32_t tau, l;
    int32_t halfperiod;
    float tmp = 0.f, sum = 0.f;
    // window the input

    for (int32_t i = 0; i < M; i++) {
        fftbuf[i] = in[i] * win[i];
    }

    // get the real / imag parts of its fft
    fft.forward(fftbuf.data(), fftbuf.data());

    // get the squared magnitude spectrum, applying some weight
    squaremag[0] = SQR(fftbuf[0]);
    squaremag[0] *= weight[0];

    for (l = 1; l < N; l++) {
        squaremag[l] = SQR(fftbuf[l * 2]) + SQR(fftbuf[l * 2 + 1]);
        squaremag[l] *= weight[l];
        squaremag[M - l] = squaremag[l];
    }
    squaremag[N] = SQR(fftbuf[1]);
    squaremag[N] *= weight[N];
    // get sum of weighted squared mags
    for (l = 0; l < N; l++) {
        sum += squaremag[l];
    }
    sum *= 2.;
    // get the real / imag parts of the fft of the squared magnitude

    fft.forward(squaremag.data(), fftbuf.data());

    yinfft[0] = 1.f;
    for (tau = 1; tau < N; tau++) {
        // compute the square differences
        yinfft[tau] = sum - fftbuf[tau * 2];
        // and the cumulative mean normalized difference function
        tmp += yinfft[tau];
        if (tmp != 0) {
            yinfft[tau] *= tau / tmp;
        } else {
            yinfft[tau] = 1.f;
        }
    }
    // find best candidates
    tau = indexofSmallestElement(yinfft.data(), N);
    if (yinfft[tau] < tol) {
        // no interpolation, directly return the period as an integer
        //output->data[0] = tau;
        //return;

        // 3 point quadratic interpolation
        //return fvec_quadratic_peak_pos (yin,tau,1);
        /* additional check for (unlikely) octave doubling in higher frequencies */
        if (tau > short_p) {
            pitch = q_peak(yinfft.data(), tau, N);
        } else {
            /* should compare the minimum value of each interpolated peaks */
            halfperiod = (int) floor(tau / 2 + .5);
            if (yinfft[halfperiod] < tol)
                pitch = q_peak(yinfft.data(), halfperiod, N);
            else
                pitch = q_peak(yinfft.data(), tau, N);
        }
    } else {
        pitch = 0.f;
    }

    return pitch;
}


#define YIN_THRESHOLD 0.20
#define PYIN_PA 0.01
#define PYIN_N_THRESHOLDS 100
#define PYIN_MIN_THRESHOLD 0.01

static constexpr const float Beta_Distribution[100] = {0.012614, 0.022715, 0.030646,
                                             0.036712, 0.041184, 0.044301, 0.046277, 0.047298, 0.047528, 0.047110,
                                             0.046171, 0.044817, 0.043144, 0.041231, 0.039147, 0.036950, 0.034690,
                                             0.032406, 0.030133, 0.027898, 0.025722, 0.023624, 0.021614, 0.019704,
                                             0.017900, 0.016205, 0.014621, 0.013148, 0.011785, 0.010530, 0.009377,
                                             0.008324, 0.007366, 0.006497, 0.005712, 0.005005, 0.004372, 0.003806,
                                             0.003302, 0.002855, 0.002460, 0.002112, 0.001806, 0.001539, 0.001307,
                                             0.001105, 0.000931, 0.000781, 0.000652, 0.000542, 0.000449, 0.000370,
                                             0.000303, 0.000247, 0.000201, 0.000162, 0.000130, 0.000104, 0.000082,
                                             0.000065, 0.000051, 0.000039, 0.000030, 0.000023, 0.000018, 0.000013,
                                             0.000010, 0.000007, 0.000005, 0.000004, 0.000003, 0.000002, 0.000001,
                                             0.000001, 0.000001, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
                                             0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
                                             0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
                                             0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};


#define F0 440.0
#define N_BINS 108
#define N_NOTES 12
#define NOTE_OFFSET 57

#define YIN_TRUST 0.5

#define TRANSITION_WIDTH 13
#define SELF_TRANS 0.99

template <typename T>
static int
absolute_threshold(const std::vector<T> &yin_buffer)
{
    size_t size = yin_buffer.size();
    int32_t tau;
    for (tau = 2; tau < size; tau++) {
        if (yin_buffer[tau] < YIN_THRESHOLD) {
            while (tau + 1 < size && yin_buffer[tau + 1] < yin_buffer[tau]) {
                tau++;
            }
            break;
        }
    }
    return (tau == size || yin_buffer[tau] >= YIN_THRESHOLD) ? -1 : tau;
}

template <typename T>
static void
difference(const std::vector<T> &audio_buffer, pitch_alloc::Yin<T> *ya)
{
    util::acorr_r(audio_buffer, ya);

    for (int32_t tau = 0; tau < ya->N / 2; tau++)
        ya->yin_buffer[tau] =
                ya->out_real[0] + ya->out_real[1] - 2 * ya->out_real[tau];
}

template <typename T>
static void
cumulative_mean_normalized_difference(std::vector<T> &yin_buffer)
{
    double running_sum = 0.0f;

    yin_buffer[0] = 1;

    for (int32_t tau = 1; tau < signed(yin_buffer.size()); tau++) {
        running_sum += yin_buffer[tau];
        yin_buffer[tau] *= tau / running_sum;
    }
}

template <typename T>
T
pitch_alloc::Yin<T>::pitch(const std::vector<T> &audio_buffer, int32_t sample_rate)
{
    int32_t tau_estimate;

    difference(audio_buffer, this);

    cumulative_mean_normalized_difference(this->yin_buffer);
    tau_estimate = absolute_threshold(this->yin_buffer);

    auto ret = (tau_estimate != -1)
               ? sample_rate / std::get<0>(util::parabolic_interpolation(
                    this->yin_buffer, tau_estimate))
               : -1;

    this->clear();
    return ret;
}

template <typename T>
T
pitch::yin(const std::vector<T> &audio_buffer, int32_t sample_rate)
{

    pitch_alloc::Yin<T> ya(audio_buffer.size());
    return ya.pitch(audio_buffer, sample_rate);
}

template class pitch_alloc::Yin<double>;
template class pitch_alloc::Yin<float>;

template double
pitch::yin<double>(const std::vector<double> &audio_buffer, int32_t sample_rate);

template float
pitch::yin<float>(const std::vector<float> &audio_buffer, int32_t sample_rate);



template <typename T>
void
util::acorr_r(const std::vector<T> &audio_buffer, pitch_alloc::BaseAlloc<T> *ba)
{
    std::transform(audio_buffer.begin(), audio_buffer.begin() + ba->N,
                   ba->buf.begin(), [](T x) -> std::complex<double> {
                return std::complex<double>(x, 0.0);
            });
   cfft_forward(ba->plan, reinterpret_cast<double *>(ba->buf.data()), 1.);

    // square ALL 2N bins of the zero-padded transform: leaving the upper
    // half unsquared mixes raw spectrum into the correlation as
    // level-dependent noise
    for (int32_t i = 0; i < 2 * ba->N; ++i)
        ba->buf[i] *= std::conj(ba->buf[i]);

    cfft_backward(ba->plan, reinterpret_cast<double *>(ba->buf.data()), 1.);

    std::transform(ba->buf.begin(), ba->buf.begin() + ba->N,
                   ba->out_real.begin(),
                   [](std::complex<double> cplx) -> T { return std::real(cplx); });
}

template void
util::acorr_r<double>(const std::vector<double> &audio_buffer,
                      pitch_alloc::BaseAlloc<double> *ba);

template void
util::acorr_r<float>(
        const std::vector<float> &audio_buffer, pitch_alloc::BaseAlloc<float> *ba);


template <typename T>
std::pair<T, T>
util::parabolic_interpolation(const std::vector<T> &array, int32_t x_)
{
    int32_t x_adjusted;
    T x = (T)x_;

    if (x < 1) {
        x_adjusted = (array[x] <= array[x + 1]) ? x : x + 1;
    } else if (x > signed(array.size()) - 1) {
        x_adjusted = (array[x] <= array[x - 1]) ? x : x - 1;
    } else {
        T den = array[x + 1] + array[x - 1] - 2 * array[x];
        T delta = array[x - 1] - array[x + 1];
        return (!den) ? std::make_pair(x, array[x])
                      : std::make_pair(x + delta / (2 * den),
                                       array[x] - delta * delta / (8 * den));
    }
    return std::make_pair(x_adjusted, array[x_adjusted]);
}

template std::pair<double, double>
util::parabolic_interpolation<double>(const std::vector<double> &array, int32_t x);
template std::pair<float, float>
util::parabolic_interpolation<float>(const std::vector<float> &array, int32_t x);


#define MPM_CUTOFF 0.93
#define MPM_SMALL_CUTOFF 0.5
#define MPM_LOWER_PITCH_CUTOFF 80.0
#define PMPM_PA 0.01
#define PMPM_N_CUTOFFS 20
#define PMPM_PROB_DIST 0.05
#define PMPM_CUTOFF_BEGIN 0.8
#define PMPM_CUTOFF_STEP 0.01

template <typename T>
static std::vector<int>
peak_picking(const std::vector<T> &nsdf, int32_t tau_min, int32_t tau_max)
{
    std::vector<int> max_positions{};
    int32_t pos = 0;
    int32_t cur_max_pos = 0;
    int32_t size = (int32_t)nsdf.size();

    if (tau_max > size - 1)
        tau_max = size - 1;

    while (pos < (size - 1) / 3 && nsdf[pos] > 0)
        pos++;
    while (pos < tau_max && nsdf[pos] <= 0.0)
        pos++;

    if (pos < tau_min)
        pos = tau_min;
    if (pos == 0)
        pos = 1;

    while (pos < tau_max) {
        if (nsdf[pos] > nsdf[pos - 1] && nsdf[pos] >= nsdf[pos + 1] &&
            (cur_max_pos == 0 || nsdf[pos] > nsdf[cur_max_pos])) {
            cur_max_pos = pos;
        }
        pos++;
        if (pos < tau_max && nsdf[pos] <= 0) {
            if (cur_max_pos > 0) {
                max_positions.push_back(cur_max_pos);
                cur_max_pos = 0;
            }
            while (pos < tau_max && nsdf[pos] <= 0.0) {
                pos++;
            }
        }
    }
    if (cur_max_pos > 0) {
        max_positions.push_back(cur_max_pos);
    }
    return max_positions;
}

template <typename T>
T
pitch_alloc::Mpm<T>::pitch(const std::vector<T> &audio_buffer, int32_t sample_rate,
        T min_pitch, T max_pitch)
{
    util::acorr_r(audio_buffer, this);

    // Normalize to McLeod's NSDF: n(tau) = 2*r'(tau) / m'(tau), so the
    // clarity cutoffs below operate on level-independent values in [-1, 1].
    // m'(tau) = m'(tau-1) - x[N-tau]^2 - x[tau-1]^2
    double sumsq = 0.0;
    for (int32_t j = 0; j < this->N; j++)
        sumsq += (double)audio_buffer[j] * (double)audio_buffer[j];

    if (sumsq <= 0.0 || this->out_real[0] <= 0) {
        this->clear();
        return -1;
    }

    // acorr_r's FFT round trip leaves r'(tau) scaled by an
    // implementation-dependent factor; r'(0) == sumsq calibrates it exactly
    const double fft_scale = (double)this->out_real[0] / sumsq;
    double msum = 2.0 * sumsq;

    this->out_real[0] = 1;
    for (int32_t tau = 1; tau < this->N; tau++) {
        const double xa = audio_buffer[this->N - tau];
        const double xb = audio_buffer[tau - 1];
        msum -= xa * xa + xb * xb;
        this->out_real[tau] =
                msum > 0.0 ? (T)(2.0 * ((double)this->out_real[tau] / fft_scale) / msum)
                           : (T)0;
    }

    int32_t tau_min = 1;
    if (max_pitch > 0)
        tau_min = std::max<int32_t>(1, (int32_t)(sample_rate / max_pitch));
    // beyond N/2 too little of the window overlaps for a reliable estimate
    int32_t tau_max = (int32_t)this->N / 2;
    if (min_pitch > 0)
        tau_max = std::min<int32_t>(tau_max, (int32_t)(sample_rate / min_pitch) + 1);

    std::vector<int> max_positions = peak_picking(this->out_real, tau_min, tau_max);
    std::vector<std::pair<T, T>> estimates;

    T highest_amplitude = std::numeric_limits<T>::lowest();

    for (int32_t i : max_positions) {
        highest_amplitude = std::max(highest_amplitude, this->out_real[i]);
        if (this->out_real[i] > MPM_SMALL_CUTOFF) {
            auto x = util::parabolic_interpolation(this->out_real, i);
            estimates.push_back(x);
            highest_amplitude = std::max(highest_amplitude, std::get<1>(x));
        }
    }

    if (estimates.empty()) {
        this->clear();
        return -1;
    }

    T actual_cutoff = MPM_CUTOFF * highest_amplitude;
    T period = 0;

    for (auto i : estimates) {
        if (std::get<1>(i) >= actual_cutoff) {
            period = std::get<0>(i);
            break;
        }
    }

    this->clear();

    if (period <= 0)
        return -1;

    T pitch_estimate = (sample_rate / period);
    T lower = min_pitch > 0 ? min_pitch : (T)MPM_LOWER_PITCH_CUTOFF;

    return (pitch_estimate > lower) ? pitch_estimate : -1;
}

template <typename T>
T
pitch::mpm(const std::vector<T> &audio_buffer, int32_t sample_rate)
{
    pitch_alloc::Mpm<T> ma(audio_buffer.size());
    return ma.pitch(audio_buffer, sample_rate);
}

template class pitch_alloc::Mpm<double>;
template class pitch_alloc::Mpm<float>;

template double
pitch::mpm<double>(const std::vector<double> &audio_buffer, int32_t sample_rate);

template float
pitch::mpm<float>(const std::vector<float> &audio_buffer, int32_t sample_rate);