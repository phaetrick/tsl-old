//
// Created by pr on 08.12.18.
//

#include "logger.h"
#include "vocoder.h"
#include "defines.h"
#include "track.h"


/*
coeff = (MYFLOAT)(exp(log(0.01f)/( millisecondsToReach99percent * sampleRate * 0.001)));

and then per sample

MYFLOAT processLP(MYFLOAT inVal)

{

    smoothed = coeff * (smoothed - inVal) + inVal;

    return smoothed;

}

*/


Vocoder::Vocoder(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_VOCODER, MONOEFFECT),
                                                  _pidsr(PI_F_P * _STATE->onedsr) {
    _bypass = &track->bypass[SPACE_VOCODER];
    _q = &_STATE->params[track->index][VOCBW];
    _band_low = &_STATE->params[track->index][VOCA];
    _band_high = &_STATE->params[track->index][VOCB];
    _channels = &_STATE->params[track->index][VOCCHANS];
    _mix = &_STATE->params[track->index][VOCMIX];
    _gain = &_STATE->params[track->index][VOCGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _att = &_STATE->params[track->index][VOCATT];
    _rel = &_STATE->params[track->index][VOCREL];
    int32_t modindex = _track->index < 3 ? _track->index + 1 : 0;
    modbuf = _DATA->tracks[modindex]->envf_buffer[_chan];
    setupHP(std::min((MYFLOAT) 6000.f, _STATE->sr * .35f), _hp);
}

void Vocoder::clear() {
    for (int32_t i = 0; i < VOCODER_CHANNELS; i++) {
        _envstateSRC[i] = _envfstateMOD[i] = 0.f;
        for (int32_t s = 3; s < 7; s++)
            _filtstateSrc[i][s] = 0.f;
        _analyticMod[i][3] = _analyticMod[i][4] = 0.f;
        _analyticSrc[i][3] = _analyticSrc[i][4] = 0.f;
    }
    _hpModState[0] = _hpModState[1] = _hpSrcState[0] = _hpSrcState[1] = 0.f;
    _envSibMod = _envSibSrc = 0.f;
}

void Vocoder::reset() {
    retune(true);
    _hpModState[0] = _hpModState[1] = _hpSrcState[0] = _hpSrcState[1] = 0.f;
    _envSibMod = _envSibSrc = 0.f;
}

void Vocoder::retune(bool clearStates) {
    _qold = *_q;
    _band_low_old = *_band_low;
    _band_high_old = *_band_high;
    _channelsold = *_channels;
    _nchan = (int32_t) _channelsold;

    if (_nchan < 2) _nchan = 2; // the spacing below needs at least one interval

    const MYFLOAT fmax = std::min((MYFLOAT) 16000.f, _STATE->sr * .45f);
    const MYFLOAT low = std::min(LOG2NORMALF(std::min(_band_low_old, _band_high_old)), fmax);
    const MYFLOAT high = std::min(LOG2NORMALF(std::max(_band_low_old, _band_high_old)), fmax);

    // Bark puts the bands where the formants are instead of spreading them
    // evenly in octaves, so the spacing ratio now varies band to band and the
    // bandwidth has to be derived per band from its own neighbour gap
    if (kBarkSpacing) {
        const MYFLOAT z0 = hzToBark(low), z1 = hzToBark(high);
        const MYFLOAT dz = (z1 - z0) / (MYFLOAT) (_nchan - 1);
        for (int32_t i = 0; i < _nchan; i++)
            _centers[i] = std::min(barkToHz(z0 + dz * (MYFLOAT) i), fmax);
    } else {
        const MYFLOAT coeff = pow(high / low, 1 / ((MYFLOAT) _nchan - 1));
        MYFLOAT center = low;
        for (int32_t i = 0; i < _nchan; i++) {
            _centers[i] = std::min(center, fmax);
            center *= coeff;
        }
    }

    // Q param (0..1) maps to a multiplier around the neutral fractional
    // bandwidth where adjacent bands cross near -3 dB, so the summed response
    // stays flat when channel count or bounds change; below 0.5 it narrows
    // steeply (down to 1/16x, the old resonant/ringy extreme), above it
    // widens up to 4x
    const MYFLOAT mult = exp2f((_qold - .5f) * (_qold < .5f ? 8.f : 4.f));
    _norm = kEnvTrim / sqrtf(mult);
    if (_synthFilter == SYNTH_RESONZ)
        _norm *= kResonZTrim;

    for (int32_t i = 0; i < _nchan; i++) {
        const MYFLOAT center = _centers[i];
        // neutral fractional bandwidth from this band's own gap to its
        // neighbour; the last band reuses the gap below it
        const MYFLOAT ratio = i + 1 < _nchan ? _centers[i + 1] / center
                                             : center / _centers[i - 1];
        const MYFLOAT fbneutral = std::max((MYFLOAT) (sqrtf(ratio) - 1.f / sqrtf(ratio)),
                                           (MYFLOAT) 0.01f);
        MYFLOAT fb = fbneutral * mult;
        if (center * (1.f + fb * .5f) > _STATE->sr * .45f)
            fb = 2.f * (_STATE->sr * .45f / center - 1.f);
        fb = limit(fb, (MYFLOAT) 0.01f, (MYFLOAT) 0.95f);

        // one setupBP section spans fb * center * .5 Hz; kCascadeBW brings that
        // down to the width the cascaded pair actually has, which is the width
        // every bank here is matched to
        const MYFLOAT bw = fb * center * .5f * kCascadeBW;
        const double r = exp(-bw * _pidsr);

        MYFLOAT srcPeak; // what the synthesis filter puts out at its centre
        if (_synthFilter == SYNTH_RESONZ) {
            setupResonZ(center, bw, _filtstateSrc[i]);
            srcPeak = resonZPeakGain(center, r);
        } else {
            setupBP(center, fb, _filtstateSrc[i]);
            srcPeak = 1.f; // butterbp is unity at centre, and so is the cascade
        }
        // the modulator bank is gone, so its analytic filter just needs unity
        // peak to read the band's true amplitude
        setupAnalytic(center, r, 1.f, _analyticMod[i]);
        setupAnalytic(center, r, srcPeak, _analyticSrc[i]);

        if (clearStates) {
            for (int32_t s = 3; s < 7; s++)
                _filtstateSrc[i][s] = 0.f;
            _analyticMod[i][3] = _analyticMod[i][4] = 0.f;
            _analyticSrc[i][3] = _analyticSrc[i][4] = 0.f;
            _envstateSRC[i] = _envfstateMOD[i] = 0.f;
        }
    }
    updateEnvCoeffs();
}

void Vocoder::updateEnvCoeffs() {
    const MYFLOAT srms = _STATE->sr * 0.001f;
    // These used to be floored per band (500/fc, 2500/fc ms) so the follower
    // could not track the rectified waveform itself in the low bands. The
    // analytic filters hand over a true magnitude with no ripple to track, so
    // the floors are gone and every band now honours the user's attack/release
    // -- that is where the extra transient definition comes from. Kept per band
    // so a future per-band scheme has somewhere to live.
    const MYFLOAT att = std::exp(ANALOG_TC / ((_attackMs + .25f) * srms));
    const MYFLOAT rel = std::exp(ANALOG_TC / ((_releaseMs + .25f) * srms));
    for (int32_t i = 0; i < _nchan; i++) {
        _attackB[i] = att;
        _releaseB[i] = rel;
    }
}

void Vocoder::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT mix, gain = dbToLinear60(*_gain);
    if (*_bypass || destroyRequested) {
        mix = 0.f;
    } else {
        mix = *_mix;
    }

    if (*_channels != _channelsold) {
        // band count changes restructure the bank: fade out, reset, fade in
        _fadeinc = -_fadeconst;
    } else if (*_q != _qold || *_band_low != _band_low_old || *_band_high != _band_high_old) {
        // coefficient-only changes retune in place, keeping filter and
        // envelope states, so knob sweeps don't stutter
        retune(false);
    }

    const MYFLOAT attack = *_att;
    const MYFLOAT release = *_rel;
    if (_attackOld != attack || _releaseOld != release) {
        setAttackTime(attack);
        setReleaseTime(release);
        updateEnvCoeffs();
    }

    const bool reson = _synthFilter == SYNTH_RESONZ;

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT modsmpl = modbuf[i];
        UDD(modsmpl);
        MYFLOAT srcsmpl = in[i];
        UDD(srcsmpl);
        MYFLOAT res = 0;
        for (int32_t chan = 0; chan < _nchan; chan++) {
            // both envelopes come off analytic filters, so tickEnvF is smoothing
            // a true magnitude rather than a rectified waveform
            const MYFLOAT rms_env = tickEnvF(
                    tickAnalytic(modsmpl, _analyticMod[chan]), _envfstateMOD[chan],
                    _attackB[chan], _releaseB[chan]);
            const MYFLOAT sou_filt = reson ? tickResonZ(srcsmpl, _filtstateSrc[chan])
                                           : tickBP12(srcsmpl, _filtstateSrc[chan]);
            // _analyticSrc is scaled to the synthesis filter's own peak gain, so
            // this still reads sou_filt's amplitude and sou_filt/rms_sou stays
            // the unit-amplitude band it always was
            const MYFLOAT rms_sou = tickEnvF(
                    tickAnalytic(srcsmpl, _analyticSrc[chan]), _envstateSRC[chan],
                    _attackB[chan], _releaseB[chan]);
            res += sou_filt * std::min(rms_env / (rms_sou + kEnvEps), kMaxBoost);
        }
        // sibilance/unvoiced path: the bank stops at BOUND B, so recover
        // consonant energy above it from the highpassed carrier, with a
        // small noise floor for carriers that have no HF content
        const MYFLOAT sibenv = tickEnvF(tickHP(modsmpl, _hpModState), _envSibMod,
                                        _attack, _release);
        const MYFLOAT sibsrc = tickHP(srcsmpl + kSibNoise * BiRandGab, _hpSrcState);
        const MYFLOAT sibsou = tickEnvF(sibsrc, _envSibSrc, _attack, _release);
        res += sibsrc * std::min(sibenv / (sibsou + kEnvEps), kMaxBoost);

        if (!std::isfinite(res)) {
            res = 0;
            clear();
        }
        in[i] = srcsmpl * (1.f - _smooth1) + res * _norm * _smooth1 * _fade * _smooth2;
        smmixgain(mix, gain);
        _fade += _fadeinc;
        if (_fade <= 0) {
            reset();
            _fade = 0;
            _fadeinc = _fadeconst;
        } else if (_fade >= 1.f) {
            _fade = 1.f;
            _fadeinc = 0.f;
        }
    }
}

/*
void
LPC2::computeLPC(std::array<MYFLOAT, LPC_BUFLEN> &x, std::array<double, LPC_MAX_ORDER + 1> &coeffs,
                 int32_t order) {
    r = x;
    rms = autoCorr.Compute(r.data());


    double lpc[LPC_MAX_ORDER];

    double err = coeffs[0] = r[0];

    uint32_t j, i;


    if (err > 0) {
        for (i = 0; i < order; i++) {
            double rr = -r[i + 1];
            for (j = 0; j < i; j++)
                rr -= lpc[j] * r[i - j];
            rr /= err;

            lpc[i] = rr;
            for (j = 0; j < (i >> 1u); j++) {
                double tmp = lpc[j];
                lpc[j] += rr * lpc[i - 1 - j];
                lpc[i - 1 - j] += rr * tmp;
            }
            if (i & 1u)
                lpc[j] += lpc[j] * rr;

            err *= (1.0 - rr * rr);

            if (err == 0.0) {
                break;
            }
        }
        maxOrder = i;
        coeffs[0] = err;
        for (j = 0; j < maxOrder; j++)
            coeffs[j + 1] = (-lpc[j]);

    } else {
        coeffs[0] = rms = 0;
        maxOrder = 0;
    }

}
*/




/*
 std::vector<double> lpcSynthesis(const std::vector<double>& coeffs, const std::vector<double>& residual) {
    std::vector<double> synthesized(residual.size(), 0.0);
    for (size_t n = 0; n < residual.size(); ++n) {
        synthesized[n] = residual[n];
        for (size_t i = 1; i <= coeffs.size() && n >= i; ++i) {
            synthesized[n] += coeffs[i - 1] * synthesized[n - i];
        }
    }
    return synthesized;
}

std::vector<double> normalizeResidual(const std::vector<double>& residual) {
    double mean = std::accumulate(residual.begin(), residual.end(), 0.0) / residual.size();
    double variance = 0.0;
    for (const auto& value : residual) {
        variance += (value - mean) * (value - mean);
    }
    variance /= residual.size();
    double stddev = std::sqrt(variance);

    std::vector<double> normalizedResidual(residual.size());
    std::transform(residual.begin(), residual.end(), normalizedResidual.begin(),
                   [mean, stddev](double value) { return (value - mean) / stddev; });
    return normalizedResidual;
}

 std::vector<double> normalizeResidual(const std::vector<double>& residual) {
    double mean = std::accumulate(residual.begin(), residual.end(), 0.0) / residual.size();
    double variance = 0.0;
    for (const auto& value : residual) {
        variance += (value - mean) * (value - mean);
    }
    variance /= residual.size();
    double stddev = std::sqrt(variance);

    std::vector<double> normalizedResidual(residual.size());
    std::transform(residual.begin(), residual.end(), normalizedResidual.begin(),
                   [mean, stddev](double value) { return (value - mean) / stddev; });
    return normalizedResidual;
}
 // Function declarations
void preEmphasis(std::vector<double>& signal, double alpha = 0.97);
std::vector<std::vector<double>> frameBlocking(const std::vector<double>& signal, size_t frameSize, size_t frameShift);
std::vector<double> hammingWindow(size_t frameSize);
std::vector<double> applyWindow(const std::vector<double>& frame, const std::vector<double>& window);
std::vector<double> levinsonDurbin(const std::vector<double>& autocorr, size_t order);
std::vector<double> lpcSynthesis(const std::vector<double>& coeffs, const std::vector<double>& residual);

int main() {
    // Example audio signal (replace with actual audio data)
    std::vector<double> signal = { };

// Pre-emphasis
preEmphasis(signal);

// Parameters
size_t frameSize = 256;
size_t frameShift = 128;
size_t lpcOrder = 16;

// Frame blocking
auto frames = frameBlocking(signal, frameSize, frameShift);

// Windowing
auto window = hammingWindow(frameSize);

// LPC Analysis and Synthesis
std::vector<double> synthesizedSignal(signal.size(), 0.0);
size_t synthesizedIndex = 0;
for (const auto& frame : frames) {
auto windowedFrame = applyWindow(frame, window);

// Autocorrelation
std::vector<double> autocorr(lpcOrder + 1, 0.0);
for (size_t lag = 0; lag <= lpcOrder; ++lag) {
for (size_t n = 0; n < windowedFrame.size() - lag; ++n) {
autocorr[lag] += windowedFrame[n] * windowedFrame[n + lag];
}
}

// LPC Coefficients
auto lpcCoeffs = levinsonDurbin(autocorr, lpcOrder);

// LPC Synthesis (using a simple excitation signal for demonstration)
std::vector<double> residual(frameSize, 0.0);
for (size_t i = 0; i < frameSize; i += frameShift) {
residual[i] = 1.0;  // Simple pulse train
}

auto synthesizedFrame = lpcSynthesis(lpcCoeffs, residual);

// Overlap-add
for (size_t i = 0; i < synthesizedFrame.size(); ++i) {
if (synthesizedIndex + i < synthesizedSignal.size()) {
synthesizedSignal[synthesizedIndex + i] += synthesizedFrame[i];
}
}
synthesizedIndex += frameShift;
}

// Output the synthesized signal
std::cout << "Synthesized Signal: ";
for (const auto& sample : synthesizedSignal) {
std::cout << sample << " ";
}
std::cout << std::endl;

return 0;
}

void preEmphasis(std::vector<double>& signal, double alpha) {
    for (size_t i = signal.size() - 1; i > 0; --i) {
        signal[i] -= alpha * signal[i - 1];
    }
}

std::vector<std::vector<double>> frameBlocking(const std::vector<double>& signal, size_t frameSize, size_t frameShift) {
    std::vector<std::vector<double>> frames;
    for (size_t i = 0; i + frameSize <= signal.size(); i += frameShift) {
        frames.emplace_back(signal.begin() + i, signal.begin() + i + frameSize);
    }
    return frames;
}

std::vector<double> hammingWindow(size_t frameSize) {
    std::vector<double> window(frameSize);
    for (size_t i = 0; i < frameSize; ++i) {
        window[i] = 0.54 - 0.46 * std::cos(2 * PI_P * i / (frameSize - 1));
    }
    return window;
}

std::vector<double> applyWindow(const std::vector<double>& frame, const std::vector<double>& window) {
    std::vector<double> windowedFrame(frame.size());
    for (size_t i = 0; i < frame.size(); ++i) {
        windowedFrame[i] = frame[i] * window[i];
    }
    return windowedFrame;
}

std::vector<double> levinsonDurbin(const std::vector<double>& autocorr, size_t order) {
    std::vector<double> a(order + 1, 0.0);
    std::vector<double> e(order + 1, 0.0);
    std::vector<double> k(order + 1, 0.0);
    std::vector<double> temp_a(order + 1, 0.0);

    e[0] = autocorr[0];

    for (size_t i = 1; i <= order; ++i) {
        double sum = 0.0;
        for (size_t j = 1; j < i; ++j) {
            sum += a[j] * autocorr[i - j];
        }
        k[i] = (autocorr[i] - sum) / e[i - 1];
        a[i] = k[i];

        for (size_t j = 1; j < i; ++j) {
            temp_a[j] = a[j] - k[i] * a[i - j];
        }

        for (size_t j = 1; j < i; ++j) {
            a[j] = temp_a[j];
        }

        e[i] = (1 - k[i] * k[i]) * e[i - 1];
    }

    return std::vector<double>(a.begin() + 1, a.end());
}

std::vector<double> lpcSynthesis(const std::vector<double>& coeffs, const std::vector<double>& residual) {
    std::vector<double> synthesized(residual.size(), 0.0);
    for (size_t n = 0; n < residual.size(); ++n) {
        synthesized[n] = residual[n];
        for (size_t i = 1; i <= coeffs.size() && n >= i; ++i) {
            synthesized[n] += coeffs[i - 1] * synthesized[n - i];
        }
    }
    return synthesized;
}
 */

void
LPC2::computeLPC(const MYFLOAT x[], MYFLOAT coeffs[],
                 int32_t order) {

    for (int32_t i = 0; i < LPC_BUFLEN; i++) {
        r[i] = x[i] * env[i];
    }

    autoCorr.compute(r);
/*
        e[0] = r[0];

        for (size_t i = 1; i <= order; ++i) {
            double sum = 0.0;
            for (size_t j = 1; j < i; ++j) {
                sum += a[j] * r[i - j];
            }
            k[i] = (r[i] - sum) / e[i - 1];
            a[i] = k[i];

            for (size_t j = 1; j < i; ++j) {
                temp_a[j] = a[j] - k[i] * a[i - j];
            }

            for (size_t j = 1; j < i; ++j) {
                a[j] = temp_a[j];
            }

            e[i] = (1 - k[i] * k[i]) * e[i - 1];
        }
*/

    if (r[0] == 0.) {
        std::memset(coeffs, 0, sizeof(MYFLOAT) * (order + 1));
        return;
    }

    uint32_t i, j;


    MYFLOAT err = r[0];
    if (order > LPC_MAX_ORDER) order = LPC_MAX_ORDER;
    MYFLOAT lpc[LPC_MAX_ORDER + 1]{};   // was std::vector -- this runs on the audio thread

    for (i = 0; i < order; i++) {
        /* Sum up this iteration's reflection coefficient. */
        MYFLOAT rr = -r[i + 1];
        for (j = 0; j < i; j++)
            rr -= lpc[j] * r[i - j];
        rr /= err;

        /* Update LPC coefficients and total error. */
        lpc[i] = rr;
        for (j = 0; j < (i >> 1u); j++) {
            MYFLOAT tmp = lpc[j];
            lpc[j] += rr * lpc[i - 1 - j];
            lpc[i - 1 - j] += rr * tmp;
        }
        if (i & 1u)
            lpc[j] += lpc[j] * rr;

        MYFLOAT tmp = err * (1. - rr * rr);

        /* see SF bug https://sourceforge.net/p/flac/bugs/234/ */
        if (tmp <= 0) break; else err = tmp;
    }

    for (int z = 0; z < i; z++) {
        coeffs[1 + z] = lpc[z];
    }
    for (; i < order; i++)
        coeffs[1 + i] = 0;

    coeffs[0] = std::sqrt(err); /* negative error is possible */
}



void
LPC2::computeLPC(const MYFLOAT x[], MYFLOAT coeffs[], MYFLOAT r[],
                 int32_t order, int size, FFT &fft) {

   // MYFLOAT tmp = 0;

    for (int32_t i = 0; i < size; i++) {
        r[i] = x[i];/* - tmp * .97;
        tmp = x[i];*/
    }
    for (int32_t i = size; i < size*2; i++) {
        r[i] = 0.0;
    }

    fft.forward(r);
    r[0] = r[0] * r[0];
    r[1] = r[1] * r[1];
    for (int32_t i = 2; i < size*2; i+=2) {
        r[i] = r[i] * r[i] + r[i+1] * r[i+1];
        r[i+1] = 0;
    }
    fft.backward(r);


    if (r[0] == 0.) {
        std::memset(coeffs, 0, sizeof(MYFLOAT) * (order + 1));
        return;
    }



    MYFLOAT err = r[0];
    if (order > LPC_MAX_ORDER) order = LPC_MAX_ORDER;
    MYFLOAT lpc[LPC_MAX_ORDER + 1]{};   // was std::vector -- this runs on the audio thread

    uint32_t i, j;

    for (i = 0; i < order; i++) {
        /* Sum up this iteration's reflection coefficient. */
        MYFLOAT rr = -r[i + 1];
        for (j = 0; j < i; j++)
            rr -= lpc[j] * r[i - j];
        rr /= err;

        /* Update LPC coefficients and total error. */
        lpc[i] = rr;
        for (j = 0; j < (i >> 1u); j++) {
            MYFLOAT tmp = lpc[j];
            lpc[j] += rr * lpc[i - 1 - j];
            lpc[i - 1 - j] += rr * tmp;
        }
        if (i & 1u)
            lpc[j] += lpc[j] * rr;

        MYFLOAT tmp = err * (1. - rr * rr);

        /* see SF bug https://sourceforge.net/p/flac/bugs/234/ */
        if (tmp <= 0) break; else err = tmp;
    }

    for (int z = 0; z < i; z++) {
        coeffs[1 + z] = lpc[z];
    }
    for (; i < order; i++)
        coeffs[1 + i] = 0;

    coeffs[0] = std::sqrt(err); /* negative error is possible */
}


MYFLOAT
LPC2::computeReflection(const MYFLOAT x[], MYFLOAT k[], MYFLOAT r[],
                        int32_t order, int size, FFT &fft, MYFLOAT sr) {
    if (order < 1) order = 1;
    if (order > LPC_MAX_ORDER) order = LPC_MAX_ORDER;
    for (int32_t i = 0; i <= order; ++i) k[i] = 0;

    // Autocorrelation via a zero-padded 2N transform: |X(w)|^2 back to the lag
    // domain. That is why r[] has to be 2 * size long.
    const int32_t M = size * 2;
    for (int32_t i = 0; i < size; ++i) r[i] = x[i];
    for (int32_t i = size; i < M; ++i) r[i] = 0.;

    fft.forward(r);
    r[0] = r[0] * r[0];
    r[1] = r[1] * r[1];
    for (int32_t i = 2; i < M; i += 2) {
        r[i] = r[i] * r[i] + r[i + 1] * r[i + 1];
        r[i + 1] = 0.;
    }
    fft.backward(r);

    if (!(r[0] > 0.) || !std::isfinite(r[0])) return 0.;

    // Conditioning, as LpcReflection does it for the LPC vocoder:
    //   white-noise correction -- keeps the recursion off a singular matrix
    //   Gaussian lag window    -- damps the noisy high-lag estimates. On a short
    //                             grain those come from very few products, and
    //                             without this they produce razor-thin poles
    //                             that ring for the whole grain.
    //   exponential g^i        -- exactly equivalent to bandwidth expansion
    //                             a[k] *= g^k, i.e. it maps every pole p -> p*g,
    //                             so Levinson hands back reflection coefficients
    //                             that are already expanded and no step-down
    //                             conversion is needed.
    // Normalising by r[0] afterwards leaves err a pure ratio, independent of
    // input level and of the transform's scaling convention.
    const double gauss = TWOPI_P * 60. / (double) sr;      // 60 Hz lag window
    const double g = std::exp(-PI_P * 40. / (double) sr);  // 40 Hz expansion
    double gpow = g;
    r[0] = r[0] * 1.0001 + 1e-12;
    for (int32_t i = 1; i <= order; ++i) {
        const double lag = gauss * i;
        r[i] *= std::exp(-0.5 * lag * lag) * gpow;
        gpow *= g;
    }
    const double inv = 1. / r[0];
    for (int32_t i = 0; i <= order; ++i) r[i] *= inv;

    static constexpr double MAX_REFL = 0.999;
    double a[LPC_MAX_ORDER + 1]{}, tmp[LPC_MAX_ORDER + 1]{};
    double err = r[0];                          // == 1
    for (int32_t i = 1; i <= order; ++i) {
        double acc = r[i];
        for (int32_t j = 1; j < i; ++j) acc += a[j] * r[i - j];
        double refl = -acc / err;
        if (!std::isfinite(refl)) break;
        if (refl > MAX_REFL) refl = MAX_REFL;
        else if (refl < -MAX_REFL) refl = -MAX_REFL;
        k[i] = (MYFLOAT) refl;
        a[i] = refl;
        for (int32_t j = 1; j < i; ++j) tmp[j] = a[j] + refl * a[i - j];
        for (int32_t j = 1; j < i; ++j) a[j] = tmp[j];
        err *= (1. - refl * refl);
        if (err <= 1e-12) break;                // perfect predictor, rest stay 0
    }
    // Residual (normalised prediction error, 0..1). k[0] is not a reflection
    // coefficient -- the synthesis lattice reads k[1..order] -- so it carries
    // this out to the caller as a filter-gain estimate (the all-pole tract's
    // gain is ~1/residual), used to seed the excitation gain on the first grain.
    const MYFLOAT residual = (MYFLOAT) std::sqrt(err > 0. ? err : 0.);
    k[0] = residual;
    return residual;
}


void LPCVocoder3::compute(MYFLOAT *in, int32_t s) {
    resonBnk(in,s);
    return;
    MYFLOAT gain = dbToLinear60(_gain->load()), mix;
    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;

    };
    const MYFLOAT emph = emphasis->load() == 1.0 ? 0.97 : 0;


    MYFLOAT gcar = coeffscarr[0];

    const int32_t carorder = 4;

    for (int32_t i = 0; i < s; i++) {
        MYFLOAT intmp = in[i];
        MYFLOAT inmod = ctrlbuf[i];
        modbuf[tmpoff] = inmod - pree * emph;
        pree = inmod;
        carrierbuf[tmpoff] = intmp;
        if (++tmpoff >= LPC_BUFLEN)
            tmpoff = 0;

        if (--hopcount == 0) {
            oldorder = nextorder;
            nextorder = (int) _order->load();
            oldwhite = white->load() == 1.0;

            hopcount = LPCHOP;
            for (int32_t tmp = tmpoff, n = 0; n < LPC_BUFLEN; n++, tmp++) {
                int32_t offset = tmp % LPC_BUFLEN;
                computebufmod[n] = modbuf[offset];
                computebufcarrier[n] = carrierbuf[offset];
            }

            // if (emph)
            //   preemphasis(computebufmod.data(), LPC_BUFLEN, .97f);

            coeffsmod = nextcoeffsmod;
            lpc.computeLPC(computebufmod, nextcoeffsmod.data(), nextorder);
            for (int j = oldorder + 1; j < nextorder + 1; j++)coeffsmod[i] = 0.0;
            for (int j = 0; j < nextorder + 1; j++) {
                coeffsmodInc[i] = (nextcoeffsmod[j] - coeffsmod[i]) / LPCHOP;
            }

            if (oldwhite) {
                coeffscarr = nextcoeffscarr;
                lpc.computeLPC(computebufcarrier, nextcoeffscarr.data(), nextorder);
                for (int j = oldorder + 1; j < nextorder + 1; j++)coeffscarr[i] = 0.0;
                for (int j = 0; j < nextorder + 1; j++) {
                    coeffscarrInc[i] = (nextcoeffscarr[j] - coeffscarr[i]) / LPCHOP;
                }
            }
        }
        MYFLOAT yy;
        MYFLOAT gmod = coeffsmod[0];
        coeffsmod[0] += coeffsmodInc[0];
        if (oldwhite) {
            yy = intmp;
            for (int32_t j = 0; j < carorder; j++) {
                yy += firmem[j] * cfscar[j];
                cfscar[j] += cfscarInc[j];
            };
            std::memmove(&firmem[1], &firmem[0], sizeof(MYFLOAT) * (oldorder - 1));
            firmem[0] = intmp;
            coeffscarr[0] += coeffscarrInc[0];
        } else
            yy = intmp * gmod;

        if (gmod > 0) {
            for (int32_t m = 0; m < nextorder; m++) {
                yy -= cfsmod[m] * iirmem[m];
                cfsmod[m] += cfsmodInc[m];
            }
        }
        std::memmove(&iirmem[1], &iirmem[0], sizeof(MYFLOAT) * (nextorder - 1));
        iirmem[0] = yy;

        in[i] = (MYFLOAT) (_smooth2 * _smooth1 * yy + (1. - _smooth1) * intmp);
        smmixgain(mix, gain);
    }
}

void LPCVocoder3::resonBnk(MYFLOAT *in, int32_t size) {
    double c1 = 1.;
    const MYFLOAT fmin = 20., fmax = 10000.;


    const double tpidsr = _STATE->twopidsr;
    const double mtpidsr = -tpidsr;


    const int32_t order = (int) _order->load();
    if (oldorder != order) {
        oldorder = order;
        reset();
    }
    MYFLOAT mix;
    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;

    };
    const bool w = white->load() == 1.0;
    const bool emph = emphasis->load() == 1.0;
    const MYFLOAT gain = dbToLinear60(_gain->load());

    double gmod = coeffsmod[0];

    double gcar = coeffscarr[0];

    const int32_t carorder = 4;

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT intmp = in[i];
        modbuf[tmpoff] = ctrlbuf[i];
        carrierbuf[tmpoff] = intmp;
        if (++tmpoff >= LPC_BUFLEN)
            tmpoff = 0;

        if (--hopcount == 0) {
            hopcount = LPCHOP;
            for (int32_t tmp = tmpoff, n = 0; n < LPC_BUFLEN; n++, tmp++) {
                int32_t offset = tmp % LPC_BUFLEN;
                computebufmod[n] = modbuf[offset];
                computebufcarrier[n] = carrierbuf[offset];
            }

            //if (emph)preemphasis(computebufmod, LPC_BUFLEN, .97f);

            // lpc.computeLPC2(computebufmod, coeffsmod, order);

            //  double t[order + 1];
            //  memcpy(t, coeffsmod.data(), sizeof(double) * (order + 1));


            lpc.computeLPC(computebufmod, coeffsmod.data(), order);

            //for (int32_t g = 0; g < order + 1; g++) {
            ;//LOGE("%d %g %g", g, t[g], coeffsmod[g]);
            //}

            gmod = coeffsmod[0];

            if (w) {
                lpc.computeLPC(computebufcarrier, coeffscarr.data(), carorder);
                gcar = std::sqrt(std::abs(coeffscarr[0]));
            }

            coef2Parm();
            for (int32_t formant = 0, j = 0; formant < resonord; j++, formant += 2) {

                c3o[j] = c3[j];
                c2o[j] = c2[j];
                double center = pp[formant];
                double bw = pp[formant + 1];
                while (center > fmax) {
                    center *= .5;
                }
                while (center < fmin) {
                    center *= 2.;
                }

                LOGE("%d %g %g", formant, center, bw);
                if (center > fmin && center < fmax) {
                    while (bw > center * .5)
                        bw *= .5f;
                    double cosf = cos(center * tpidsr);
                    c3[j] = exp(bw * mtpidsr);
                    double c3p1 = c3[j] + 1.0;
                    double c3t4 = c3[j] * 4.0;
                    c2[j] = c3t4 * cosf / c3p1;
                }
            }

        }
        double yy;

        if (w) {
            double tmp = 0.;
            for (int32_t j = 0; j < carorder; j++) tmp -= firmem[j] * cfscar[j];
            memmove(&firmem[1], &firmem[0], sizeof(double) * (carorder - 1));
            firmem[0] = intmp;
            yy = ((double) intmp - tmp);
        } else
            yy = (double) intmp * gmod;

        MYFLOAT interp = (MYFLOAT) (LPCHOP - hopcount) / (MYFLOAT) LPCHOP;
        const int32_t mod = 1;
        const int32_t scale = 2;
        double out = 0;
        for (int32_t formant = 0, j = 0; formant < resonord; j++, formant += 2) {
            double cc2 = c2o[j] + (c2[j] - c2o[j]) * interp;
            double cc3 = c3o[j] + (c3[j] - c3o[j]) * interp;
            if (scale) {
                double omc3 = 1.0 - cc3;
                double c2sqr = cc2 * cc2;
                double c3p1 = cc3 + 1.0;
                if (scale == 1)
                    c1 = omc3 * sqrt(1.0 - (c2sqr / (4 * cc3)));
                else if (scale == 2)
                    c1 = sqrt((c3p1 * c3p1 - c2sqr) * omc3 / c3p1);
            }
            double x = c1 * yy + cc2 * yt1[j] - cc3 * yt2[j];
            yt2[j] = yt1[j];
            yt1[j] = x;
            if (mod) out += x; // parallel
        }
        if (!mod) out = yy;
        else
            out /= (resonord * .5);
        in[i] = _smooth1 * _smooth2 *out + (1.-_smooth1) * intmp;
        smmixgain(mix, gain);

    }
}


void LPCVocoder4::compute(MYFLOAT *in, int32_t s) {
    MYFLOAT gain = dbToLinear60(_gain->load()), mix;
    if (_bypass->load() || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;

    };

    for (int32_t i = 0; i < s; i++) {
        in[i] = _smooth2 * _smooth1 * _tick(in[i], ctrlbuf[i]) +
                (1. - _smooth1) * in[i];
        pree = ctrlbuf[i];
        smmixgain(mix, gain);
    }
}

void LPCVocoder4::onBufferReady(MYFLOAT *car, MYFLOAT *mod, int size) {
    const int32_t order = (int) _order->load();
    int32_t carorder = order;
    const bool w = white->load() == 1.0;
    //lpc.computeLPC(mod, coeffsmod, order);
    lpc.computeLPC(mod, coeffsmod, order);
    //gmod = coeffsmod[0];
    auto gmod = coeffsmod[0];
    if (w) {
        lpc.computeLPC(car, coeffscarr, carorder);
    }
    for (size_t n = 0; n < order; ++n) firmem[n] = iirmem[n] = 0.0;

    MYFLOAT maxMod{}, maxCar{}, maxRes{};
    for (size_t n = 0; n < LPC_BUFLEN; ++n) {
        auto absCar = std::abs(car[n]), absMod = std::abs(ctrlbuf[n]);
        if(absCar > maxCar) maxCar = absCar;
        if(absMod > maxMod) maxMod = absMod;
    }
    for (size_t n = 0; n < LPC_BUFLEN; ++n) {
        MYFLOAT yy = car[n];
        if (w) {
            for (int32_t j = 0; j < carorder; j++) yy += firmem[j] * cfscar[j];
            std::memmove(&firmem[1], &firmem[0], sizeof(MYFLOAT) * (carorder - 1));
            firmem[0] = car[n];
        }
        yy *= gmod;
        for (int32_t m = 0; m < order; m++) yy -= cfsmod[m] * iirmem[m];
        std::memmove(&iirmem[1], &iirmem[0], sizeof(MYFLOAT) * (order - 1));
        car[n] = (iirmem[0] = yy) * env[n];
        auto absRes = std::abs(car[n]);
        if(absRes>maxRes)maxRes = absRes;
    }
    if(maxRes>0){
        auto scale = (maxMod * maxCar)/maxRes;
        for (size_t n = 0; n < LPC_BUFLEN; ++n) car[n]*=scale;
    }

}


// ===========================================================================
// LPCVocoder5
// ===========================================================================

void LpcReflection::analyse(const MYFLOAT x[], MYFLOAT k[], int32_t order) {
    if (order < 1) order = 1;
    if (order > LPC_MAX_ORDER) order = LPC_MAX_ORDER;

    for (int32_t i = 1; i <= order; ++i) k[i] = 0;

    double energy = 0.;
    for (int32_t i = 0; i < LPC_BUFLEN; ++i) {
        const double w = (double) x[i] * win[i];
        r[i] = w;
        energy += w * w;
    }
    if (energy <= 0.) return;

    autoCorr.compute(r);            // r[i] = sum_n x[n]x[n+i], unnormalised
    if (r[0] <= 0.) return;

    // White-noise correction, then the conditioning window, then normalise by
    // r[0], which keeps the recursion well conditioned and its error term a
    // pure ratio regardless of input level or FFT scaling convention.
    r[0] = r[0] * 1.0001 + 1e-12;
    for (int32_t i = 1; i <= order; ++i) r[i] *= rWin[i];
    const double inv = 1. / r[0];
    for (int32_t i = 0; i <= order; ++i) r[i] *= inv;

    double err = r[0];              // == 1, so err stays a pure ratio throughout
    int32_t i;
    for (i = 1; i <= order; ++i) {
        double acc = r[i];
        for (int32_t j = 1; j < i; ++j) acc += a[j] * r[i - j];
        double refl = -acc / err;
        if (refl > MAX_REFL) refl = MAX_REFL;
        else if (refl < -MAX_REFL) refl = -MAX_REFL;
        k[i] = (MYFLOAT) refl;
        a[i] = refl;
        for (int32_t j = 1; j < i; ++j) tmp[j] = a[j] + refl * a[i - j];
        for (int32_t j = 1; j < i; ++j) a[j] = tmp[j];
        err *= (1. - refl * refl);
        if (err <= 1e-12) break;    // perfect predictor, remaining k stay zero
    }
}

void LPCVocoder5::analyse() {
    // unroll the rings into contiguous frames, oldest sample first
    for (int32_t n = 0, p = ringPos; n < LPC_BUFLEN; ++n) {
        modFrame[n] = modRing[p];
        carFrame[n] = carRing[p];
        if (++p >= LPC_BUFLEN) p = 0;
    }

    // Stages retired on the previous hop have finished ramping their k to zero
    // by now. Drop them from the loop and clear their delay state, so a later
    // order increase does not restart those stages from stale samples.
    if (runModOrder > tgtModOrder) {
        for (int32_t m = tgtModOrder + 1; m <= runModOrder; ++m) { kMod[m] = 0; bMod[m] = 0; }
        runModOrder = tgtModOrder;
    }
    if (runCarOrder > tgtCarOrder) {
        for (int32_t m = tgtCarOrder + 1; m <= runCarOrder; ++m) { kCar[m] = 0; bCar[m] = 0; }
        runCarOrder = tgtCarOrder;
    }

    const int32_t modOrd = limit((int32_t) (_modOrder->load() * orderScale + 0.5f), 1, LPC_MAX_ORDER);
    lpc.analyse(modFrame, kModTgt, modOrd);
    for (int32_t m = modOrd + 1; m <= runModOrder; ++m) kModTgt[m] = 0;
    const int32_t modRun = runModOrder > modOrd ? runModOrder : modOrd;
    for (int32_t m = 1; m <= modRun; ++m)
        kModInc[m] = (kModTgt[m] - kMod[m]) * (MYFLOAT) (1. / LPC_HOP);
    runModOrder = modRun;
    tgtModOrder = modOrd;

    if (white->load() == 1.0) {
        // Fixed: exposed as a knob first, but it made no audible difference
        // across its whole range. Deep enough to flatten the carrier's gross
        // spectral shape, shallow enough to leave its harmonics intact.
        const int32_t carOrd = limit((int32_t) (CAR_ORDER_AT_48K * orderScale + 0.5f), 1, LPC_MAX_ORDER);
        lpc.analyse(carFrame, kCarTgt, carOrd);
        for (int32_t m = carOrd + 1; m <= runCarOrder; ++m) kCarTgt[m] = 0;
        const int32_t carRun = runCarOrder > carOrd ? runCarOrder : carOrd;
        for (int32_t m = 1; m <= carRun; ++m)
            kCarInc[m] = (kCarTgt[m] - kCar[m]) * (MYFLOAT) (1. / LPC_HOP);
        runCarOrder = carRun;
        tgtCarOrder = carOrd;
    } else {
        // ramp the whitening filter out over one hop instead of switching it
        // off between samples, so toggling WHITE does not click
        for (int32_t m = 1; m <= runCarOrder; ++m)
            kCarInc[m] = -kCar[m] * (MYFLOAT) (1. / LPC_HOP);
        tgtCarOrder = 0;
    }

    // Levels over the hop just finished.
    const int32_t hopLen = hopSamples > 0 ? hopSamples : 1;
    const double modMs = modMsAccum / hopLen;      // modulator, raw
    const double dryMs = dryMsAccum / hopLen;      // carrier as it came in
    const double outMs = outMsAccum / hopLen;      // what the lattice put out
    const double exgAvg = gainAccum / hopLen;      // gain actually applied
    modMsAccum = dryMsAccum = outMsAccum = gainAccum = 0.;
    hopSamples = 0;

    // Gain of the whole chain -- whitening plus tract filter -- from the DRY
    // carrier to the output, per unit exciteGain. Dividing by both dryMs and
    // exgAvg takes the carrier's level and our own gain back out, so what is
    // left depends only on the filter shapes. That matters twice over: it lets
    // the carrier's own dynamics through untouched (measuring output per unit
    // exciteGain alone would have pinned the output to a slow carrier average
    // and squashed anything faster), and referencing the dry rather than the
    // whitened carrier is what keeps WHITE a timbre switch -- the whitened
    // signal is far quieter than the raw one, and an impulse train besides,
    // whose per-hop RMS swings ~20x.
    if (exgAvg > 1e-9 && outMs > 0. && dryMs > 1e-18) {
        const double g = std::sqrt(outMs / dryMs) / exgAvg;
        const double clamped = g < 1e-4 ? 1e-4 : (g > 1e5 ? 1e5 : g);
        // Nothing flows while the modulator is silent, so the stored value goes
        // stale and the first hop back would overshoot. Snap on resume.
        if (chainGainStale) { chainGain = clamped; chainGainStale = false; }
        else chainGain += CHAIN_GAIN_SMOOTH * (clamped - chainGain);
    } else {
        chainGainStale = true;
    }

    // The modulator contributes its envelope *shape* only, as a ratio against
    // its own recent loud level. Instant attack / slow release on that
    // reference keeps the ratio <= 1 nearly always, so the vocoder ducks with
    // speech and never surges above the carrier -- in particular it cannot
    // surge after a pause, which a plain average reference would do while it
    // recovered.
    if (modMs > modRefMs) modRefMs = modMs;
    else modRefMs += modRefRelease * (modMs - modRefMs);

    double duck = modRefMs > 1e-12 ? std::sqrt(modMs / modRefMs) : 0.;
    if (duck > 1.) duck = 1.;

    // A hop is 5.3 ms but a low-pitched modulator has an 8+ ms period, so the
    // raw per-hop ratio beats against the pitch and adds several dB of spurious
    // range. Instant attack keeps consonants sharp, short release smooths that.
    if (duck > duckSm) duckSm = duck;
    else duckSm += duckRelease * (duck - duckSm);

    // Output lands on (carrier level) x (modulator envelope), both live.
    MYFLOAT target = (MYFLOAT) (duckSm / chainGain);
    if (target > MAX_EXCITE_GAIN) target = MAX_EXCITE_GAIN;
    // Cap how fast the gain may climb. A hop is 5.3 ms at 48 kHz, so +6 dB per
    // hop still opens far quicker than any consonant onset, while stopping a
    // stale chainGain from blipping on the first hop after a silent stretch.
    const MYFLOAT ceiling = exciteGain * MAX_GAIN_RISE_PER_HOP;
    if (exciteGain > 1e-9f && target > ceiling) target = ceiling;
    exciteGainInc = (target - exciteGain) * (MYFLOAT) (1. / LPC_HOP);
}

void LPCVocoder5::compute(MYFLOAT *in, int32_t s) {
    const MYFLOAT gain = dbToLinear60(_gain->load());
    const MYFLOAT mix = (_bypass->load() || destroyRequested) ? (MYFLOAT) 0. : (MYFLOAT) *_mix;

    // PREEMPH reads 0..1 but everything audible happens between alpha 0.7 and
    // 0.97, so spread that across most of the travel with a fourth root:
    // knob 0.25 -> 0.70, 0.5 -> 0.83, 0.9 -> 0.96, 1.0 -> 0.99. Still exactly
    // zero at the bottom, i.e. genuinely off.
    const MYFLOAT emphKnob = limit(emphasis->load(), (MYFLOAT) 0., (MYFLOAT) 1.);
    preAlpha = (MYFLOAT) 0.99 * std::sqrt(std::sqrt(emphKnob));

    for (int32_t i = 0; i < s; i++) {
        const MYFLOAT dry = in[i];
        const MYFLOAT mod = ctrlbuf[i];

        // Pre-emphasis feeds the analysis buffer only. It flattens the spectral
        // tilt so the fit spends its poles on formants instead of the low end;
        // keeping it off the audible path avoids needing a matching de-emphasis
        // stage, whose DC gain at alpha 0.97 is about 33x.
        modRing[ringPos] = mod - preAlpha * preState;
        preState = mod;
        modMsAccum += (double) mod * mod;      // raw, i.e. pre-emphasis excluded
        dryMsAccum += (double) dry * dry;
        carRing[ringPos] = dry;
        if (++ringPos >= LPC_BUFLEN) ringPos = 0;

        if (--hopCount <= 0) {
            hopCount = LPC_HOP;
            analyse();
        }

        // Carrier whitening: FIR lattice. Ascending, so it needs one temp to
        // carry b[m-1] from the previous sample before it gets overwritten.
        MYFLOAT x = dry;
        if (runCarOrder > 0) {
            MYFLOAT bPrev = bCar[0];
            bCar[0] = x;
            MYFLOAT f = x;
            for (int32_t m = 1; m <= runCarOrder; ++m) {
                const MYFLOAT kk = (kCar[m] += kCarInc[m]);
                const MYFLOAT fNext = f + kk * bPrev;
                const MYFLOAT bNext = bPrev + kk * f;
                bPrev = bCar[m];
                bCar[m] = bNext;
                f = fNext;
            }
            x = f;
        }
        ++hopSamples;

        exciteGain += exciteGainInc;
        gainAccum += exciteGain;
        denormFlip = -denormFlip;
        x = x * exciteGain + denormFlip;

        // Vocal tract: all-pole lattice. Descending, so writing b[m] after
        // reading b[m-1] leaves every read seeing the previous sample's value
        // and no temp is needed. Stable for as long as every |k| < 1, which
        // linear interpolation between two stable sets preserves.
        MYFLOAT f = x;
        for (int32_t m = runModOrder; m >= 1; --m) {
            const MYFLOAT kk = (kMod[m] += kModInc[m]);
            f -= kk * bMod[m - 1];
            bMod[m] = bMod[m - 1] + kk * f;
        }
        bMod[0] = f;
        outMsAccum += (double) f * f;

        in[i] = _smooth2 * _smooth1 * f + (1.f - _smooth1) * dry;
        smmixgain(mix, gain);
    }
}
