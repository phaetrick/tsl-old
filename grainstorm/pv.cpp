//
// Created by pr on 24.04.20.
//

#include "grainstorm.h"
#include "ffttools.h"
#include "lfo.h"
#include "pv.h"
#include <cstring>
#include "Convolver.h"
#include "vocoder.h"
#include <random>

void
(*pv_funcs[])(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) = {dephase,
                                                                                          dephase_locked,
                                                                                          formant_move,
                                                                                          random_phase,
                                                                                          dorobot,
                                                                                          pvosc,
                                                                                          freqwarp,
                                                                                          hpss,
                                                                                          spectral_contrast,
                                                                                          spectral_snap,
                                                                                          spectral_resonator,
                                                                                          spectral_freeze};
// dephase_tracked (PH CORRECTION III) is implemented below but not listed: by
// ear it did not improve on PH CORRECTION II, so it is parked. To re-enable,
// append it here, append "PH CORRECTION III" to PV_TYPES, move PVPH3LOCK/
// PVPH3TRANS/PVPH3PEAKS back before NUM_PARAMS, and restore its ParameterInit
// block and its view in gui/space_pv.cpp -- at DUMMYPV7 now, SPECTRAL FREEZE
// took DUMMYPV6 (the view slot is SPACE_PV_PH1 + algorithm index).

void (*cross_funcs[])(TRACK *track, uint8_t channel, uint32_t fft_size, bool) = {cross_mag_phase,
                                                                                 docepstrum,
                                                                                 docepstrum2,
                                                                                 spec_interpol,
                                                                                 vocode,
                                                                                 cross_convolve,
                                                                                 cross_lpc,
                                                                                 cross_transport,
                                                                                 cross_stack,
                                                                                 cross_duck};

static_assert(ARRAY_LEN(pv_hpss_mask_types) == 4,
              "hpss() decodes PVHPSSMASK as 0..3");
static_assert(ARRAY_LEN(pv_snap_modes) == 5,
              "spectral_snap() decodes PVSNAPMODE as 0..4");
static_assert(ARRAY_LEN(pv_res_modes) == 5,
              "spectral_resonator() decodes PVRESMODE as 0..4");
static_assert(ARRAY_LEN(cross_funcs) == NUM_PV_EFFECTS,
              "cross_funcs must line up with enum pveffect and CROSS_TYPES");
static_assert(ARRAY_LEN(pv_funcs) == ARRAY_LEN(pv_types),
              "pv_funcs must line up with PV_TYPES");
static_assert(ARRAY_LEN(cross_types) == NUM_PV_EFFECTS,
              "CROSS_TYPES must line up with enum pveffect");
static_assert(ARRAY_LEN(cross_phase_types) == PHASE_MULTIPLY + 1,
              "CROSS_PHASE_TYPES must line up with enum cross_phase_type");
static_assert(ARRAY_LEN(cross_mag_types) == MAG_MAX + 1,
              "CROSS_MAG_TYPES must line up with enum cross_magnitute_type");
static_assert(ARRAY_LEN(cross_duck_modes) == 2,
              "cross_duck() decodes CROSSDUCKMODE as 0..1");


// ---------------------------------------------------------------------------
// Grain output levelling.
//
// The algorithms come out at wildly different natural levels (CEPSTRUM
// concentrates energy into a spiky grain, VOCODER/CONVOLUTION are dense, LPC
// depends on its own filter gain), so each one's output has to be referred to a
// common target: the source grain's own RMS (measured in granulate,
// track->srcRmsTmp) times a global trim. Matching RMS -- not peak -- is what
// makes them equal loudness: peak-matching equalised peaks while loudness
// follows RMS, and the crest factors differ by tens of dB. Referencing the
// source RMS (rather than the old maxTmp_src*maxTmp_mod peak product) also means
// the output cannot collapse to silence when a peak reference happens to be zero
// -- which is what muted CROSS_LPC.
//
// WHAT THE GAIN MUST *NOT* DO is follow the level grain by grain. Forcing every
// grain's RMS onto the target exactly (which is what this did until now) is a
// limiter with infinite ratio at grain rate: the wet output's loudness becomes a
// copy of the carrier's, and everything else -- the modulator's dynamics, the
// algorithm's own spectral gating, the grain-to-grain variation that makes a
// granular texture breathe -- is flattened out. That is the "static" sound.
// Worse, when the modulator falls silent the algorithm's near-silent output is
// dragged back up to carrier level, so the gaps fill with amplified residue.
//
// So the gain is split in two. The *calibration* -- how far this algorithm sits
// from the target on average -- is slow, and the per-grain deviation from that
// average passes through untouched. It is computed as the ratio of two
// independently smoothed energies, never as a smoothed ratio: smoothing
// target/rms directly is what failed before, because a near-silent grain makes
// that ratio explode and the smoother then carries the explosion onto the next
// full-level grain (clicks, and a surge on every algorithm switch). Smoothing
// the numerator and the denominator separately is immune to that -- a quiet
// grain contributes a small number to *both* accumulators and the quotient
// barely moves. The estimator is a running mean for its first grains and only
// then hands over to the one-pole, so an algorithm or parameter switch is
// calibrated from the first grain instead of sliding in over a second.
//
// CROSS_LEVEL_TRACK is the escape hatch back to the old behaviour: 0 = pure slow
// calibration (dynamics intact), 1 = the old exact per-grain lock, in between a
// partial follow in the log domain. If this ever wants to be a knob, that is the
// value to expose.
//
// The crest cap stays instantaneous and stays referred to *this grain's* source
// RMS, not to the smoothed average -- it measures the algorithm's crest factor
// against its own input, so it bounds a spiky grain's peak without touching
// level dynamics (a genuinely loud passage raises the reference with it). It can
// only ever pull down. `extra` is a per-algorithm trim (CONVOLUTION overlap-adds
// a 2N grain, so it piles up more overlaps and needs pulling down).
//
// `adapt` = false is for the pass-through fallback branches (a silent modulator
// leaves TRANSPORT/STACK undefined and they emit the carrier): those grains are
// not the algorithm's output, so they are levelled exactly the old way and kept
// out of the estimator, which would otherwise be calibrated by them whenever the
// modulator has gaps.
// ---------------------------------------------------------------------------
static constexpr MYFLOAT CROSS_LEVEL_TRIM = 0.7;    // global level, ear-tunable
static constexpr MYFLOAT CROSS_CREST_MAX = 8.;      // peak ceiling = crest cap
static constexpr MYFLOAT CROSS_CONV_TRIM = 0.707;   // CONVOLUTION overlaps ~2x -> -3 dB
static constexpr MYFLOAT CROSS_LEVEL_TAU_DN = 1.0;  // calibration falls, seconds
static constexpr MYFLOAT CROSS_LEVEL_TAU_UP = 6.0;  // calibration rises, seconds
static constexpr MYFLOAT CROSS_LEVEL_TRACK = 0.;    // 0 = dynamics through, 1 = old lock
static constexpr MYFLOAT CROSS_LEVEL_FLOOR = 1e-12; // mean-square below this = silence
static constexpr MYFLOAT CROSS_GAIN_MIN = 1e-3;     // calibration clamp, -60 dB
static constexpr MYFLOAT CROSS_GAIN_MAX = 1e3;      // calibration clamp, +60 dB

static void normalize_grain(TRACK *track, uint8_t channel, MYFLOAT *buf, int32_t n,
                            MYFLOAT extra = 1., bool adapt = true) {
    if (n <= 0) return;
    MYFLOAT peak = 0, ms = 0;
    for (int32_t i = 0; i < n; ++i) {
        const MYFLOAT a = std::abs(buf[i]);
        if (a > peak) peak = a;
        ms += buf[i] * buf[i];
    }
    ms /= (MYFLOAT) n;
    const MYFLOAT rms = std::sqrt(ms);
    const MYFLOAT target = track->srcRmsTmp[channel] * CROSS_LEVEL_TRIM * extra;
    if (!(rms > 0.) || !std::isfinite(rms) || !(target > 0.)) return;

    MYFLOAT scale;
    if (!adapt) {
        scale = target / rms;
    } else {
        auto _appState = track->_appState;
        // Grain hop in seconds: grains leave at DENSITY per second whatever the
        // playback speed does to the read position, so the calibration adapts at
        // the same rate in time regardless of the grain rate.
        const MYFLOAT dens = _STATE->params[track->index][DENSITY].load();
        const MYFLOAT hop = dens > 0. ? (MYFLOAT) 1. / dens : (MYFLOAT) 0.01;
        MYFLOAT aDn = 1. - std::exp(-hop / CROSS_LEVEL_TAU_DN);
        MYFLOAT aUp = 1. - std::exp(-hop / CROSS_LEVEL_TAU_UP);
        if (!(aDn > 0.) || !std::isfinite(aDn) || aDn > 1.) aDn = 1.;
        if (!(aUp > 0.) || !std::isfinite(aUp) || aUp > 1.) aUp = 1.;

        const MYFLOAT msRef = target * target;
        if (ms > CROSS_LEVEL_FLOOR && msRef > CROSS_LEVEL_FLOOR) {
            // Asymmetric, and the direction is decided in the ratio the gain is
            // made of, not on either energy alone -- both accumulators then move
            // with the same coefficient, so the quotient stays an honest
            // energy ratio. A grain asking for MORE gain than the current
            // calibration is usually a hole in the material (a modulator that
            // stopped, a gated band), so it moves the calibration slowly and the
            // hole stays a hole; a grain asking for LESS is the algorithm
            // genuinely running hot, which is worth correcting quickly. Measured
            // on a 1 s modulator gap: 0.7 dB of drift instead of 2.9 dB
            // symmetric, while a 12 dB jump is corrected in 1.5 s instead of 2.3.
            const bool up = msRef * track->crossOutMs[channel] >
                            ms * track->crossRefMs[channel];
            const MYFLOAT a = up ? aUp : aDn;
            // Running mean while the count is below the slowest pole's memory,
            // then the pole: converged from grain one, no ramp-in after a
            // switch. The count has to run down to the SLOWEST of the two or
            // the warm-up weight floors the slow branch and the asymmetry does
            // nothing.
            const MYFLOAT aw = (MYFLOAT) 1. / (MYFLOAT) (track->crossLvlN[channel] + 1);
            if (aw > aUp) ++track->crossLvlN[channel];
            const MYFLOAT ae = aw > a ? aw : a;
            track->crossRefMs[channel] += ae * (msRef - track->crossRefMs[channel]);
            track->crossOutMs[channel] += ae * (ms - track->crossOutMs[channel]);
        }

        MYFLOAT gSlow = 1.;
        if (track->crossOutMs[channel] > CROSS_LEVEL_FLOOR && track->crossRefMs[channel] > 0.)
            gSlow = std::sqrt(track->crossRefMs[channel] / track->crossOutMs[channel]);
        if (!std::isfinite(gSlow)) gSlow = 1.;
        if (gSlow < CROSS_GAIN_MIN) gSlow = CROSS_GAIN_MIN;
        if (gSlow > CROSS_GAIN_MAX) gSlow = CROSS_GAIN_MAX;

        scale = gSlow;
        if constexpr (CROSS_LEVEL_TRACK > (MYFLOAT) 0.)
            scale = gSlow * std::pow((target / rms) / gSlow, CROSS_LEVEL_TRACK);
    }

    const MYFLOAT peakCeil = target * CROSS_CREST_MAX;
    if (peak > 0. && peakCeil > 0. && peak * scale > peakCeil) scale = peakCeil / peak;

    for (int32_t i = 0; i < n; ++i) buf[i] *= scale;
}

void dephase_locked(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {

    if (playbackspeed == 0.0 || playbackspeed == 1.0) {
        // Skipping the grain also invalidates the stored frame: coming back
        // through 1.0 used to accumulate phase against whatever frame preceded
        // the last excursion, which could be seconds old, and that showed up as
        // a jump. Re-seed instead on the next grain we actually process.
        track->pvLockedInit[channel] = true;
        return;
    }
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];
    int32_t current_index = track->current_nodes_pv[channel];
    int32_t prev_index = current_index == 0 ? 1 : 0;
    NodePV **current_nodes = track->nodes_pv[channel][current_index];
    NodePV **prev_nodes = track->nodes_pv[channel][prev_index];
    int32_t *current_peaks = track->pv_peaks[channel][current_index];
    int32_t *prev_peaks = track->pv_peaks[channel][prev_index];

    const int32_t nprevpeaks = track->pv_nprevpeaks[channel];
    const MYFLOAT step_length =
            _STATE->sr / _STATE->params[track->index][DENSITY].load() * playbackspeed;
    const MYFLOAT tstretch_ratio = 1. / playbackspeed;
    const int32_t M2 = fft_size >> 1u;
    const MYFLOAT pomegapre = TWOPI_P * step_length / (MYFLOAT) fft_size;


    tsl::fft::fftshift(fft_in, fft_size);
    fft->forwardPolar(fft_in, fft_in);

    for (int32_t i = 0; i < M2; i++) {
        auto mag = fft_in[i * 2];
        auto phi = fft_in[i * 2 + 1];
        current_nodes[i]->phi = isnan(phi) || isinf(phi) ? 0 : phi;
        current_nodes[i]->mag = mag;
    }

    int32_t npeaks = 0;
    int32_t j = 2;
    while (j < M2 - 2) {
        if (current_nodes[j]->mag > current_nodes[j - 1]->mag &&
            current_nodes[j]->mag > current_nodes[j - 2]->mag &&
            current_nodes[j]->mag > current_nodes[j + 1]->mag &&
            current_nodes[j]->mag > current_nodes[j + 2]->mag) {
            current_peaks[npeaks] = j;
            j += 3;
            npeaks++;
        } else
            j++;
    }

    if (track->pvLockedInit[channel]) {
        for (int32_t i = 0; i < M2; i++) {
            current_nodes[i]->psi = current_nodes[i]->phi;
        }
        track->pvLockedInit[channel] = false;
    } else if (npeaks > 0 && nprevpeaks > 0) {
        int32_t prev_p = 0;
        for (int32_t p = 0; p < npeaks; p++) {
            int32_t p2 = current_peaks[p];
            while (prev_p < nprevpeaks &&
                   ABS(p2 - prev_peaks[prev_p + 1]) < ABS(p2 - prev_peaks[prev_p]))
                prev_p++;
            int32_t p1 = prev_peaks[prev_p];
            MYFLOAT avg_p = (p1 + p2) * .5;
            // Expected phase advance for bin k over one hop is 2*pi*h*k/N =
            // pomegapre*k. There is no -1: bins here are 0-based (the fallback
            // branch below uses pomegapre*i), so the offset only shifted the
            // princarg branch decision by one bin's worth of advance and made
            // moving partials jump 2*pi*h/N.
            MYFLOAT pomega = pomegapre * avg_p;
            MYFLOAT peak_delta_phi =
                    pomega + princarg(current_nodes[p2]->phi - prev_nodes[p1]->phi - pomega);
            MYFLOAT peak_target_phase =
                    princarg(prev_nodes[p1]->psi + peak_delta_phi * tstretch_ratio);
            MYFLOAT peak_phase_rotation = princarg(peak_target_phase - current_nodes[p2]->phi);
            int32_t bin1, bin2;
            if (npeaks == 1) {
                bin1 = 0;
                bin2 = M2;
            } else if (p == 0) {
                bin1 = 0;
                bin2 = M2;
            } else if (p == npeaks - 1) {
                bin1 = (int) round((current_peaks[p - 1] + p2) * .5);
                bin2 = M2;
            } else {
                bin1 = (int) round((current_peaks[p - 1] + p2) * .5) + 1;
                bin2 = (int) round((current_peaks[p + 1] + p2) * .5);
            }

            for (int32_t i = bin1; i < bin2; i++) {
                MYFLOAT psi = princarg(current_nodes[i]->phi + peak_phase_rotation);
                UDF(psi)
                current_nodes[i]->psi = psi;
            }
        }
    } else {
        for (int32_t i = 0; i < M2; i++) {
            MYFLOAT omega = pomegapre * i;
            MYFLOAT delta_phi =
                    omega + princarg(current_nodes[i]->phi - prev_nodes[i]->phi - omega);
            MYFLOAT psi = princarg(prev_nodes[i]->psi + delta_phi * tstretch_ratio);
            UDF(psi)
            current_nodes[i]->psi = psi;
        }
    }

    for (int32_t i = 2; i < M2; i++) {
        fft_in[i * 2] = current_nodes[i]->mag;
        fft_in[i * 2 + 1] = current_nodes[i]->psi;
    }
    fft->backwardPolar(fft_in, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);

    track->current_nodes_pv[channel] = current_index == 0 ? (uint8_t) 1 : (uint8_t) 0;
    track->pv_nprevpeaks[channel] = npeaks;
}


// PH CORRECTION III. Four things it does that dephase_locked does not:
//
//   1. Peaks are refined to a fraction of a bin by a parabola through the log
//      magnitudes, so the expected phase advance is taken at the partial's real
//      frequency instead of at the nearest bin centre.
//   2. Peaks are MATCHED to the previous frame by frequency, nearest first,
//      with a tolerance. dephase_locked walks a monotone index and will happily
//      pair a peak with an unrelated one when partials appear or vanish.
//   3. A peak with no match inside the tolerance is a NEW partial, and is
//      seeded from its own analysis phase rather than inheriting a stranger's
//      accumulator.
//   4. Onsets are detected (half-wave-rectified spectral flux) and reset every
//      phase to the analysis phase.
//   5. After the reset the stretch is HELD: phase advances at the natural rate
//      (ratio 1) until the attack has cleared the analysis window. The reset
//      alone fixes only the grain containing the attack -- the same attack sits
//      inside the fft_size/step_length neighbouring grains too, and stretching
//      those smears their copies around the reset one as pre/post-echo.
//      Measured on a click train at 4x overlap: reset-only recovers a crest of
//      30.5 dB against the input's 33.0, reset+hold 32.5. Stretching pauses for
//      the hold (at most PV_PH3_HOLD_MAX grains); a transient is not a thing
//      that can be stretched, so the time is taken from the material around it.
//
// Deliberately NOT the magnitude-gradient method (RTPGHI). Measured on this
// exact convention: with the window the same length as the FFT -- which is what
// a grain gives us, there is no zero padding to widen the mainlobe -- the
// magnitude-derived instantaneous frequency errs 1.22 bins on average against
// 0.086 bins for the two-frame estimator below, and the frequency-direction
// gradient is 100% wrong at hops above about N/8. Those relations need spectral
// oversampling that this architecture does not have.
void dephase_tracked(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {

    if (playbackspeed == 0.0 || playbackspeed == 1.0) {
        // Same reasoning as dephase/dephase_locked: a skipped grain leaves the
        // stored frame stale, so re-seed rather than accumulate against it. A
        // hold in flight dies with the frame it was measured against.
        track->pv3Init[channel] = true;
        track->pv3Hold[channel] = 0;
        return;
    }
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];

    const int32_t current_index = track->current_nodes_pv[channel];
    const int32_t prev_index = current_index == 0 ? 1 : 0;
    NodePV **current_nodes = track->nodes_pv[channel][current_index];
    NodePV **prev_nodes = track->nodes_pv[channel][prev_index];
    MYFLOAT *current_peaks = track->pv3Peak[channel][current_index];
    MYFLOAT *prev_peaks = track->pv3Peak[channel][prev_index];
    int32_t *current_pbin = track->pv3PeakBin[channel][current_index];
    int32_t *prev_pbin = track->pv3PeakBin[channel][prev_index];
    const int32_t nprev = track->pv3NPeaks[channel][prev_index];

    const MYFLOAT step_length =
            _STATE->sr / _STATE->params[track->index][DENSITY].load() * playbackspeed;
    const MYFLOAT tstretch_ratio = 1. / playbackspeed;
    const int32_t M2 = fft_size >> 1u;
    const MYFLOAT omegapre = TWOPI_P * step_length / (MYFLOAT) fft_size;

    const MYFLOAT lock = _STATE->params[track->index][PVPH3LOCK].load();
    const MYFLOAT transAmt = _STATE->params[track->index][PVPH3TRANS].load();
    const MYFLOAT peakSel = _STATE->params[track->index][PVPH3PEAKS].load();

    tsl::fft::fftshift(fft_in, fft_size);
    fft->forwardPolar(fft_in, fft_in);

    MYFLOAT frameMax = 0., fluxUp = 0., prevSum = 0.;
    for (int32_t i = 1; i < M2; i++) {
        MYFLOAT mag = fft_in[i * 2];
        MYFLOAT phi = fft_in[i * 2 + 1];
        if (!std::isfinite(mag)) mag = 0.;
        if (!std::isfinite(phi)) phi = 0.;
        current_nodes[i]->mag = mag;
        current_nodes[i]->phi = phi;
        if (mag > frameMax) frameMax = mag;
        const MYFLOAT d = mag - prev_nodes[i]->mag;
        if (d > 0.) fluxUp += d;
        prevSum += prev_nodes[i]->mag;
    }

    // Onset test. Normalising the rectified flux by the previous frame's own
    // magnitude sum keeps this a measure of spectral CHANGE rather than of
    // level, so it does not fire on a crescendo and does fire on a quiet hit.
    // TRANS at 0 disables it entirely (some material is better served by the
    // smear than by a reset every few grains). Detection pauses while a hold is
    // running -- the hold IS the response, retriggering would only extend it.
    bool transient = false;
    if (transAmt > 0. && prevSum > 0. && track->pv3Hold[channel] == 0) {
        const MYFLOAT thr = PV_PH3_FLUX_HI -
                            (PV_PH3_FLUX_HI - PV_PH3_FLUX_LO) * transAmt;
        transient = (fluxUp / prevSum) > thr;
    }
    if (transient) {
        int32_t hold = (int32_t)((MYFLOAT) fft_size / step_length) + 1;
        if (hold > PV_PH3_HOLD_MAX) hold = PV_PH3_HOLD_MAX;
        track->pv3Hold[channel] = hold;
    }
    // The ratio every propagation below uses: natural rate while an attack is
    // inside the window, the stretch otherwise. The reset frame itself consumes
    // the first hold grain (its psi is the analysis phase either way).
    MYFLOAT ratio = tstretch_ratio;
    if (track->pv3Hold[channel] > 0) {
        ratio = 1.0;
        track->pv3Hold[channel]--;
    }

    const bool reseed = track->pv3Init[channel] || transient;

    // Peak picking. A bin that beats both neighbours either side is a partial:
    // the mainlobe of the full-length Hann this path applies is four bins wide,
    // so nothing narrower can be real. The parabola through the three log
    // magnitudes then places it to a fraction of a bin.
    const MYFLOAT peakFloor =
            frameMax * (MYFLOAT) std::pow(10., -(PV_PH3_FLOOR_LO +
                                                 (PV_PH3_FLOOR_HI - PV_PH3_FLOOR_LO) * peakSel) / 20.);
    int32_t npeaks = 0;
    for (int32_t j = 2; j < M2 - 2 && npeaks < M2; ) {
        const MYFLOAT m = current_nodes[j]->mag;
        if (m > peakFloor &&
            m > current_nodes[j - 1]->mag && m > current_nodes[j - 2]->mag &&
            m > current_nodes[j + 1]->mag && m > current_nodes[j + 2]->mag) {
            const MYFLOAT a1 = std::log(current_nodes[j - 1]->mag > 1e-20 ? current_nodes[j - 1]->mag : 1e-20);
            const MYFLOAT a2 = std::log(m > 1e-20 ? m : 1e-20);
            const MYFLOAT a3 = std::log(current_nodes[j + 1]->mag > 1e-20 ? current_nodes[j + 1]->mag : 1e-20);
            const MYFLOAT den = a1 - 2. * a2 + a3;
            MYFLOAT d = den != 0. ? .5 * (a1 - a3) / den : 0.;
            if (!std::isfinite(d)) d = 0.;
            else if (d > .5) d = .5;
            else if (d < -.5) d = -.5;
            current_pbin[npeaks] = j;
            current_peaks[npeaks] = (MYFLOAT) j + d;
            npeaks++;
            j += 3;
        } else j++;
    }

    if (reseed) {
        for (int32_t i = 1; i < M2; i++)
            current_nodes[i]->psi = current_nodes[i]->phi;
        track->pv3Init[channel] = false;
    } else {
        // Per-bin propagation first. This is the whole result at LOCK 0, and
        // the thing the locked rotation is blended against above it.
        for (int32_t i = 1; i < M2; i++) {
            const MYFLOAT omega = omegapre * i;
            const MYFLOAT delta_phi =
                    omega + princarg(current_nodes[i]->phi - prev_nodes[i]->phi - omega);
            MYFLOAT psi = princarg(prev_nodes[i]->psi + delta_phi * ratio);
            UDF(psi)
            current_nodes[i]->psi = psi;
        }

        if (lock > 0. && npeaks > 0 && nprev > 0) {
            int32_t lo = 1;
            for (int32_t p = 0; p < npeaks; p++) {
                // Region ends at the spectral VALLEY before the next peak, not
                // at the midpoint between them: the mainlobe has to arrive
                // intact for the rotation to mean anything, and the midpoint
                // cuts through it whenever two partials sit close together.
                int32_t hi;
                if (p == npeaks - 1) hi = M2;
                else {
                    int32_t best = current_pbin[p];
                    MYFLOAT bm = current_nodes[best]->mag;
                    for (int32_t q = current_pbin[p] + 1; q <= current_pbin[p + 1]; q++)
                        if (current_nodes[q]->mag < bm) { bm = current_nodes[q]->mag; best = q; }
                    hi = best;
                }

                // Nearest previous peak in frequency, not in index order.
                const MYFLOAT pk = current_peaks[p];
                int32_t bestj = -1;
                MYFLOAT bestd = 0.;
                for (int32_t q = 0; q < nprev; q++) {
                    const MYFLOAT dd = ABS(prev_peaks[q] - pk);
                    if (bestj < 0 || dd < bestd) { bestd = dd; bestj = q; }
                }

                if (bestj >= 0 && bestd <= PV_PH3_TRACK_TOL) {
                    const int32_t p1 = prev_pbin[bestj], p2 = current_pbin[p];
                    const MYFLOAT avg_p = (prev_peaks[bestj] + pk) * .5;
                    const MYFLOAT omega = omegapre * avg_p;
                    const MYFLOAT peak_delta_phi =
                            omega + princarg(current_nodes[p2]->phi - prev_nodes[p1]->phi - omega);
                    const MYFLOAT target =
                            princarg(prev_nodes[p1]->psi + peak_delta_phi * ratio);
                    const MYFLOAT rot = princarg(target - current_nodes[p2]->phi);
                    for (int32_t i = lo; i < hi; i++) {
                        const MYFLOAT locked = princarg(current_nodes[i]->phi + rot);
                        // Blend along the shorter arc so LOCK 0 is exactly the
                        // per-bin result and LOCK 1 exactly the locked one.
                        MYFLOAT psi = princarg(current_nodes[i]->psi +
                                               lock * princarg(locked - current_nodes[i]->psi));
                        UDF(psi)
                        current_nodes[i]->psi = psi;
                    }
                } else {
                    // No partial near this one last frame, so there is no phase
                    // to continue. Start it from its own analysis phase; the
                    // alternative is to inherit an accumulator belonging to a
                    // different partial, which is what smears note onsets.
                    for (int32_t i = lo; i < hi; i++) {
                        MYFLOAT psi = current_nodes[i]->phi;
                        UDF(psi)
                        current_nodes[i]->psi = psi;
                    }
                }
                lo = hi;
            }
        }
    }

    for (int32_t i = 1; i < M2; i++) {
        fft_in[i * 2] = current_nodes[i]->mag;
        fft_in[i * 2 + 1] = current_nodes[i]->psi;
    }
    fft->backwardPolar(fft_in, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);

    track->pv3NPeaks[channel][current_index] = npeaks;
    track->current_nodes_pv[channel] = current_index == 0 ? (uint8_t) 1 : (uint8_t) 0;
}


void
docepstrum(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];
    auto fft_out = track->fft_out[channel];
    fft->forward(fft_in, fft_out);
    auto control = track->destinationz->fft_out[channel];
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    // Slots 0 and 1 are the packed DC/Nyquist reals, each under its own
    // envelope value; the pairs start at bin 1.
    fft_out[0] *= control[0];
    fft_out[1] *= control[1];
    for (int32_t i = 1; i < (fft_size >> 1u); i++) {
        auto m = control[i * 2];
        fft_out[i * 2] *= m;
        fft_out[i * 2 + 1] *= m;
    }
    fft->backward(fft_out, fft_in);
    for (int32_t n = 0; n < fft_size; ++n) UDD(fft_in[n])
    normalize_grain(track, channel, fft_in, fft_size);
}

void docepstrum2(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];
    auto fft_out = track->fft_out[channel];
    auto fft_help1 = track->fft_help1[channel];
    auto fft_help2 = track->fft_help2[channel];
    int32_t fftsized2 = fft_size >> 1u;
    const auto cut_off = ((uint32_t) floor(fft_size * (CUTOFFMIN + CUTOFFRANGE * (LOG2NORMAL2F(
                                                                                          _STATE->params[track->index][CROSSCEP2CUTSRC].load()) -
                                                                                  CUTOFFMIN)))) &
                         ~1u; //
    auto control = track->destinationz->fft_out[channel];
    tsl::fft::cepstrum(fft, fft_in, fft_out, fft_help1, fft_help2, fft_size, cut_off, false);
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    // DEPTH scales the legacy exponent pair in the log domain: the gain per
    // bin is exp(depth * (2*Lmod_cep - Lsrc_cep)), so depth .5 is EXACTLY the
    // legacy exp(control - .5*fft_out), 0 passes the carrier untouched, and 1
    // doubles the effect with the legacy impose:whiten ratio kept. Do NOT
    // "fix" the exponent asymmetry to matched whiten/impose: full whitening
    // flattens the carrier to its noise floor (near-silent bins boosted tens
    // of dB) and was heard to erase the character of both signals.
    const MYFLOAT depth = _STATE->params[track->index][CROSSCEP2DEPTH].load();
    static constexpr MYFLOAT CEP2_MAX_LOG_DIFF = 40.;
    auto cepgain = [&](int32_t slot) {
        MYFLOAT d = 2. * control[slot] - fft_out[slot];
        if (!std::isfinite(d)) d = 0;
        else if (d > CEP2_MAX_LOG_DIFF) d = CEP2_MAX_LOG_DIFF;
        else if (d < -CEP2_MAX_LOG_DIFF) d = -CEP2_MAX_LOG_DIFF;
        return exp(depth * d);
    };
    // Slots 0 and 1 are the packed DC/Nyquist reals.
    fft_help2[0] = fft_help1[0] * cepgain(0);
    fft_help2[1] = fft_help1[1] * cepgain(1);
    for (int32_t i = 1; i < fftsized2; i++) {
        const MYFLOAT m = cepgain(i * 2);
        fft_help2[i * 2] =
                fft_help1[i * 2] * m;
        fft_help2[i * 2 + 1] =
                fft_help1[i * 2 + 1] * m;
    }
    fft->backward(fft_help2, fft_in);
    for (int32_t n = 0; n < fft_size; ++n) UDD(fft_in[n])
    normalize_grain(track, channel, fft_in, fft_size);
}


// ---------------------------------------------------------------------------
// CROSS_LPC -- the modulator's vocal tract driven by the source grain.
//
// The modulator side (granulate_fft.cpp) hands over reflection coefficients
// rather than direct-form a[k]; see LPC2::computeReflection for why, and for
// the lag window and bandwidth expansion applied to the autocorrelation before
// Levinson. Here the tract runs as a continuous all-pole lattice whose
// coefficients ramp linearly across the grain. It used to be a direct-form IIR
// whose state was zeroed at the top of every grain, which cut off all formant
// ringing longer than the hop and overlap-added phase-incoherent copies of the
// same carrier -- audible as combing at the grain rate.
//
// Level is set on the excitation, before the filter (see the gain block below).
// Scaling the filter's continuous output per grain would step the level at
// every grain boundary.
// ---------------------------------------------------------------------------
static constexpr MYFLOAT CROSS_LPC_CHAIN_SMOOTH = 0.3;  // filter-gain tracker, ~3 grains
static constexpr MYFLOAT CROSS_LPC_MAX_GAIN = 1e3;      // excitation-gain ceiling
static constexpr MYFLOAT CROSS_LPC_CLAMP = 64.;         // runaway backstop

void cross_lpc(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    int32_t order = track->crossLPCOrderTmp;
    if (order < 1) order = 1;
    if (order > CROSS_LPC_MAX_ORDER) order = CROSS_LPC_MAX_ORDER;

    const bool w = _STATE->params[track->index][CROSSLPCW].load() == 1.0;
    auto car = track->grain_buffer[channel];
    auto &st = track->crossLpc[channel];

    // Carrier whitening, if enabled: flattens the source so the tract decides
    // the timbre instead of the source's own resonances doubling up with it.
    MYFLOAT kCarTgt[CROSS_LPC_MAX_ORDER + 1]{};
    int32_t carOrder = 0;
    if (w) {
        // The carrier fit does not need the tract's resolution and a high order
        // here just whitens away the source's character entirely.
        carOrder = order < 12 ? order : 12;
        CHECKFFT(fft_size * 2)
        LPC2::computeReflection(car, kCarTgt, track->fft_help1[channel], carOrder,
                                fft_size, *fft, _STATE->sr);
    }

#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif

    const MYFLOAT *kModTgt = track->destinationz->fft_out[channel];

    // Stages above the running order start from zero, and their delay state is
    // cleared so a later order increase does not restart them on stale samples.
    if (order > st.runOrder)
        for (int32_t m = st.runOrder + 1; m <= order; ++m) st.kMod[m] = st.bMod[m] = 0;
    if (carOrder > st.runCarOrder)
        for (int32_t m = st.runCarOrder + 1; m <= carOrder; ++m) st.kCar[m] = st.bCar[m] = 0;

    // Retired stages ramp their k down to zero over this grain rather than
    // being dropped outright, so an order change is a slide and not a step.
    //
    // WHITE going on or off is the exception: there the whitener's gain must
    // change in ONE step, because the only thing that can cancel it is the
    // excitation gain stepping with it at the same sample. Ramped, the two
    // curves do not match -- the whitening collapses far faster than a linear
    // gain ramp can follow, which put +21 dB in the middle of the crossover
    // grain even with the target level correct at both ends. Stepped and
    // cancelled, the level is continuous and only the excitation's *character*
    // changes, which is what the switch is for.
    const bool whiteToggled = (carOrder > 0) != (st.runCarOrder > 0);
    const int32_t runOrder = order > st.runOrder ? order : st.runOrder;
    const int32_t runCarOrder = whiteToggled
                                ? carOrder
                                : (carOrder > st.runCarOrder ? carOrder : st.runCarOrder);

    const MYFLOAT invN = 1. / (MYFLOAT) fft_size;
    for (int32_t m = 1; m <= runOrder; ++m) {
        const MYFLOAT tgt = m <= order ? kModTgt[m] : (MYFLOAT) 0.;
        if (st.init) st.kMod[m] = tgt;
        st.kModInc[m] = (tgt - st.kMod[m]) * invN;
    }
    for (int32_t m = 1; m <= runCarOrder; ++m) {
        const MYFLOAT tgt = m <= carOrder ? kCarTgt[m] : (MYFLOAT) 0.;
        if (st.init || whiteToggled) st.kCar[m] = tgt;
        st.kCarInc[m] = (tgt - st.kCar[m]) * invN;
    }

    // Excitation gain. The filter runs continuously, so its output cannot be
    // rescaled per grain without stepping the level at every grain boundary --
    // the gain goes on the excitation instead, ramped smoothly across the grain.
    //
    // Target: output RMS = source RMS * CROSS_LEVEL_TRIM, the same level the
    // other algorithms normalise to. Because output RMS = excite*chainGain*srcRMS
    // and we want it = TRIM*srcRMS, the source RMS cancels and the excitation
    // gain is simply TRIM/chainGain -- so this needs no source-level measurement
    // and cannot be zeroed by one. The chain gain moves only when the
    // coefficients do, so a source/modulator amplitude change passes straight
    // through with no surge. This replaces an earlier peak-product target that
    // could evaluate to zero and mute the whole algorithm.
    //
    // The chain is TWO filters, so it is two gains: the whitener's, MEASURED on
    // the grain that is about to be played, and the tract's, which can only be
    // tracked because it has not run yet. Carrying their product as one number
    // (which is what this did until now) bakes the whitener's prediction gain --
    // 30 dB on tonal material -- into the excitation, so switching WHITE off
    // removed that attenuation while the excitation was still calibrated for it:
    // +29 dB, walked back over the ~8 grains the tracker needed. The other
    // direction was worse and quieter, so it never got reported: -31 dB, held
    // for ~15 grains. Both measured, see the note on the whitening pass below.
    //
    // A reset would have been the wrong lever: it reseeds from the *modulator's*
    // residual, which describes the tract only, and `init` snaps the reflection
    // coefficients instead of ramping them.
    if (st.init) {
        // Seed the tract gain from the modulator's LPC residual (carried in
        // kModTgt[0]); an all-pole tract's gain is ~1/residual, so this stops
        // the first grain after a switch from being loud while the tracker
        // converges. Fall back to unity if the residual is degenerate.
        const MYFLOAT residual = kModTgt[0];
        st.tractGain = (residual > 1e-4 && residual <= 1.) ? (MYFLOAT) (1. / residual)
                                                           : (MYFLOAT) 1.;
    }

    MYFLOAT dryMs = 0;
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) dryMs += car[n] * car[n];
    dryMs *= invN;

    // Whitening runs as its own pass, BEFORE the gain is decided, so the gain
    // sees what the whitener actually did to this grain instead of a prediction
    // of it. A prediction cannot be good enough here: the lattice interpolates
    // its coefficients across the grain and carries state between grains, so its
    // gain is neither the theoretical residual nor last grain's value on exactly
    // the grains where being wrong is audible. Measured, the whitener's
    // contribution cancels out of the level completely -- including on the grain
    // the switch lands on -- and no smoothing or seeding is needed.
    //
    // The ratio is level-independent (the carrier's own dynamics divide out), so
    // compensating it exactly does not flatten anything; it removes the
    // whitener's gain, not the material's.
    MYFLOAT *exc = car;                 // in place when there is nothing to whiten
    MYFLOAT whiteMs = dryMs;
    if (runCarOrder > 0) {
        exc = track->fft_help1[channel];    // free again: computeReflection is done
        whiteMs = 0;
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) {
            // FIR lattice. Ascending, so one temp carries b[m-1] over from the
            // previous sample before it is overwritten.
            MYFLOAT bPrev = st.bCar[0];
            st.bCar[0] = car[n];
            MYFLOAT f = car[n];
            for (int32_t m = 1; m <= runCarOrder; ++m) {
                const MYFLOAT kk = (st.kCar[m] += st.kCarInc[m]);
                const MYFLOAT fNext = f + kk * bPrev;
                const MYFLOAT bNext = bPrev + kk * f;
                bPrev = st.bCar[m];
                st.bCar[m] = bNext;
                f = fNext;
            }
            UDD(f)
            exc[n] = f;
            whiteMs += f * f;
        }
        whiteMs *= invN;
    }
    // Hold the last ratio through a silent grain rather than calling it unity.
    if (dryMs > 1e-12 && whiteMs > 0.) {
        const MYFLOAT wInst = std::sqrt(whiteMs / dryMs);
        if (std::isfinite(wInst) && wInst > 0.) st.whitenGain = wInst;
    }
    if (!(st.whitenGain > 0.) || !std::isfinite(st.whitenGain)) st.whitenGain = 1.;

    const MYFLOAT chainGain = st.tractGain * st.whitenGain;

    MYFLOAT gTarget = st.exciteGain;
    if (chainGain > 1e-9 && std::isfinite(chainGain))
        gTarget = CROSS_LEVEL_TRIM / chainGain;
    if (!(gTarget > 0.) || !std::isfinite(gTarget)) gTarget = 1.;
    if (gTarget > CROSS_LPC_MAX_GAIN) gTarget = CROSS_LPC_MAX_GAIN;
    // The step that cancels the whitener's step. Everywhere else the excitation
    // ramps across the grain, because everywhere else the chain is continuous.
    if (st.init || whiteToggled) st.exciteGain = gTarget;
    st.exciteGainInc = (gTarget - st.exciteGain) * invN;
    const MYFLOAT excAvg = (st.exciteGain + gTarget) * (MYFLOAT) 0.5;   // mean over the grain
    st.init = false;

    MYFLOAT outMs = 0;
    MYFLOAT outPeak = 0;
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) {
        MYFLOAT x = exc[n];

        st.exciteGain += st.exciteGainInc;
        x *= st.exciteGain;

        // Vocal tract: all-pole lattice. Descending, so writing b[m] after
        // reading b[m-1] leaves every read seeing the previous sample and no
        // temp is needed. Stable as long as every |k| < 1, which the analysis
        // clamps to and linear interpolation preserves.
        MYFLOAT f = x;
        for (int32_t m = runOrder; m >= 1; --m) {
            const MYFLOAT kk = (st.kMod[m] += st.kModInc[m]);
            f -= kk * st.bMod[m - 1];
            st.bMod[m] = st.bMod[m - 1] + kk * f;
        }
        if (!std::isfinite(f)) {
            // A blow-up would otherwise persist in the state for good, now that
            // the state is no longer cleared every grain.
            st.reset();
            f = 0;
        } else if (f > CROSS_LPC_CLAMP) f = CROSS_LPC_CLAMP;
        else if (f < -CROSS_LPC_CLAMP) f = -CROSS_LPC_CLAMP;
        st.bMod[0] = f;
        UDD(f)
        car[n] = f;
        outMs += f * f;
        const MYFLOAT a = std::abs(f);
        if (a > outPeak) outPeak = a;
    }
    outMs *= invN;

    // Tract gain, updated from what actually happened this grain and measured
    // against what actually entered the tract -- so it is independent of whether
    // whitening ran, and comes through a WHITE toggle untouched. Only meaningful
    // when the signal carried energy; hold otherwise so a silent patch does not
    // reset the level. Smoothed slowly and clamped so a single pathological
    // grain cannot swing the gain.
    if (whiteMs > 1e-12 && outMs > 0. && excAvg > 1e-12) {
        MYFLOAT tInst = std::sqrt(outMs / whiteMs) / excAvg;
        if (std::isfinite(tInst) && tInst > 0.) {
            if (tInst < 1e-6) tInst = 1e-6;
            else if (tInst > 1e6) tInst = 1e6;
            st.tractGain += CROSS_LPC_CHAIN_SMOOTH * (tInst - st.tractGain);
        }
    }
    if (!(st.tractGain > 0.) || !std::isfinite(st.tractGain)) st.tractGain = 1.;

    st.runOrder = order;
    st.runCarOrder = carOrder;
}


void cross_mag_phase(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto grainbuffer = track->grain_buffer[channel];
    fft->forwardPolar(grainbuffer, grainbuffer);
    auto control = track->destinationz->fft_out[channel];
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    MYFLOAT *r, *phi = nullptr;
    MYFLOAT dc = grainbuffer[0];
    MYFLOAT nyq = grainbuffer[1];
    switch ((int) _STATE->params[track->index][CROSS_MAG].load()) {
        case MAG_SOURCE:
            r = grainbuffer;
            dc = grainbuffer[0];
            break;
        case MAG_MODULATOR:
            r = control;
            dc = control[0];
            break;
        case MAG_MULTIPLY:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2] = grainbuffer[i * 2] * control[i * 2];
            r = grainbuffer;
            dc = grainbuffer[0] * control[0];
            break;
        case MAG_ADD:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2] = grainbuffer[i * 2] + control[i * 2];
            r = grainbuffer;
            dc = grainbuffer[0] + control[0];
            break;
        case MAG_SUBSTRACT:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2] = grainbuffer[i * 2] - control[i * 2];
            r = grainbuffer;
            dc = grainbuffer[0] - control[0];
            break;
        case MAG_MIN:
            // The spectral intersection: a bin sounds only as loud as BOTH
            // sides have it, so what comes through is what the two sounds
            // share. MAX is the union without ADD's level pile-up.
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2] = std::min(grainbuffer[i * 2], control[i * 2]);
            r = grainbuffer;
            dc = std::min(grainbuffer[0], control[0]);
            break;
        case MAG_MAX:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2] = std::max(grainbuffer[i * 2], control[i * 2]);
            r = grainbuffer;
            dc = std::max(grainbuffer[0], control[0]);
            break;
        default:
            r = grainbuffer;
            break;
    }

    switch ((int) _STATE->params[track->index][CROSS_PHASE].load()) {
        case PHASE_SOURCE:
            phi = grainbuffer;
            nyq = grainbuffer[1];
            break;
        case PHASE_MODULATOR:
            phi = control;
            nyq = control[1];
            break;
        case PHASE_ADD:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2 + 1] = grainbuffer[i * 2 + 1] + control[i * 2 + 1];
            phi = grainbuffer;
            nyq = grainbuffer[1] + control[1];
            break;
        case PHASE_SUBSTRACT:
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2 + 1] = grainbuffer[i * 2 + 1] - control[i * 2 + 1];
            phi = grainbuffer;
            nyq = grainbuffer[1] - control[1];
            break;
        case PHASE_MULTIPLY:
            // was grainbuffer * grainbuffer, i.e. the source phase squared and
            // the modulator ignored entirely
            for (int32_t i = 1; i < fft_size >> 1u; i++)
                grainbuffer[i * 2 + 1] =
                        princarg(grainbuffer[i * 2 + 1] * control[i * 2 + 1]);
            phi = grainbuffer;
            nyq = grainbuffer[1] * control[1];
            break;
        default:
            phi = grainbuffer;
            nyq = grainbuffer[1];
            break;
    }

    grainbuffer[0] = dc;
    grainbuffer[1] = nyq;
    for (int32_t i = 1; i < fft_size >> 1u; i++) {
        grainbuffer[i * 2] = r[i * 2];
        grainbuffer[i * 2 + 1] = phi[i * 2 + 1];
    }
    fft->backwardPolar(grainbuffer, grainbuffer);
    for (int32_t n = 0; n < fft_size; ++n) UDD(grainbuffer[n])
    normalize_grain(track, channel, grainbuffer, fft_size);
}

void spec_interpol(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto k1 = _STATE->params[track->index][IPOL].load();
    LFO *lfo = track->lfo[IPOL].load();
    if (lfo && lfo->power()) {
        auto a = _STATE->controls[track->index][IPOL].lfo_min.load();
        auto b = _STATE->controls[track->index][IPOL].lfo_max.load();
        k1 = a + lfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }

    //LOGE("%f %f %g %g", percentage, k1, track->offset-track->off_start, track->off_stop - track->off_start);
    auto k2 = 1. - k1;
    auto fft_in = track->grain_buffer[channel];
    auto fft_out = track->fft_out[channel];
    auto fft_help1 = track->fft_help1[channel];
    auto fft_help2 = track->fft_help2[channel];
    auto cut_off = (uint32_t) floor(fft_size * (CUTOFFMIN + CUTOFFRANGE * (LOG2NORMAL2F(
                                                                                   _STATE->params[track->index][CROSSINTCUTSRC].load()) -
                                                                           CUTOFFMIN))) &
                   ~1u; //
    auto cep2 = track->destinationz->fft_out[channel];
    auto fft2 = track->destinationz->fft_help1[channel];
    tsl::fft::fftshift(fft_in, fft_size);
    tsl::fft::cepstrum(fft, fft_in, fft_out, fft_help1, fft_help2, fft_size, cut_off, false);
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif

    // Each spectrum is whitened by its own cepstral envelope, the whitened parts
    // are blended, and the interpolated envelope is re-imposed. Written out
    // literally that is (X1/E1 * k2 + X2/E2 * k1) * E, and E1 or E2 underflows
    // to zero on a near-silent bin, which put infinities into the ring buffer.
    // Since all three envelopes are exponentials of cepstral values, both gains
    // collapse to the exponential of a *difference* of log envelopes:
    //   E/E1 = exp(k1 * (l2 - l1)),  E/E2 = exp(k2 * (l1 - l2))
    // which is bounded by the actual spectral distance between the two sources
    // and needs no division at all. The clamp is a backstop for a degenerate
    // envelope, at a spread far wider than any real material.
    // The fine structure MUST stay a linear complex blend. In overlap-add the
    // blend is the sum of two independently phase-coherent resyntheses, each
    // continuing its own waveform across grains -- there is no interference
    // "flutter" to fix. A per-bin dominant-phase magnitude morph was tried
    // here and REMOVED: stealing the quieter source's phase destroys its
    // coherence from grain to grain and demotes it to noise riding on the
    // louder one, which audibly erased the modulator. Do not re-add it.
    // DC and Nyquist (slots 0/1) are real; each blends under its own
    // envelope value.
    static constexpr MYFLOAT MAX_LOG_DIFF = 40.;   // e^40, ~350 dB
    auto envdiff = [&](int32_t slot) {
        MYFLOAT d = 2 * (cep2[slot] - fft_out[slot]);
        if (!std::isfinite(d)) d = 0;
        else if (d > MAX_LOG_DIFF) d = MAX_LOG_DIFF;
        else if (d < -MAX_LOG_DIFF) d = -MAX_LOG_DIFF;
        return d;
    };
    for (int32_t slot = 0; slot < 2; slot++) {
        const MYFLOAT d = envdiff(slot);
        fft_help2[slot] = fft_help1[slot] * k2 * exp(k1 * d) * .25 +
                          fft2[slot] * k1 * exp(-k2 * d) * .25;
    }
    for (int32_t i = 1; i < (fft_size >> 1u); i++) {
        const MYFLOAT d = envdiff(i * 2);
        const MYFLOAT g1 = k2 * exp(k1 * d) * .25;
        const MYFLOAT g2 = k1 * exp(-k2 * d) * .25;
        fft_help2[i * 2] = fft_help1[i * 2] * g1 + fft2[i * 2] * g2;
        fft_help2[i * 2 + 1] = fft_help1[i * 2 + 1] * g1 + fft2[i * 2 + 1] * g2;
    }
    fft->backward(fft_help2, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);
    for (int32_t n = 0; n < fft_size; ++n) UDD(fft_in[n])
    normalize_grain(track, channel, fft_in, fft_size);
}

void dorobot(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];
    auto fft_out = track->fft_out[channel];
    auto fft_help1 = track->fft_help1[channel];
    tsl::fft::fftshift(fft_in, fft_size);
    fft->forward(fft_in, fft_out);
    tsl::fft::fft_get_norm(fft_out, fft_help1, fft_size);
    fft->backward(fft_help1, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);
}

void formant_move(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    auto fft_in = track->grain_buffer[channel];
    auto fft_out = track->fft_out[channel];
    auto fft_help1 = track->fft_help1[channel];
    auto fft_help2 = track->fft_help2[channel];
    CHECKFFT(fft_size)

    const auto cut_off =
            (uint32_t) floor(
                    fft_size *
                    (0.001 + .999 * _STATE->params[track->index][CUTSTRETCH].load())) &
            ~1u;
    MYFLOAT max1 = 0;
    for (int i = 0; i < fft_size; i++)
        max1 = std::abs(fft_in[i]) > max1 ? std::abs(fft_in[i]) : max1;

    tsl::fft::cepstrum(fft, fft_in, fft_out, fft_help1, fft_help2, fft_size, cut_off, false);

    auto lfo = track->lfo[STRETCHCOEFF].load();

    MYFLOAT stretchfact;
    if (lfo && lfo->power()) {
        auto a = _STATE->controls[track->index][STRETCHCOEFF].lfo_min.load();
        auto b = _STATE->controls[track->index][STRETCHCOEFF].lfo_max.load();
        auto range = DISTANCE(a, b);
        auto start = a;
        if (a > b) range *= -1;
        stretchfact = start + lfo->buf[(int) track->step_point_grain[channel].load()] * range;
    } else stretchfact = _STATE->params[track->index][STRETCHCOEFF];

    auto table = tsl::fft::compute_warped_lookup_table(track->warp_table[channel], fft_size,
                                                       LOG2NORMAL(stretchfact));


    for (int32_t i = 0; i < fft_size; i++) {
        fft_help2[i] = fft_out[table[i]];
    }

    fft_out[0] = fft_help1[1];
    fft_out[1] = 0.;

    for (int32_t i = 2; i < (fft_size >> 1u); i++) {
        auto mod = exp(2 * (fft_help2[i * 2] - fft_out[i * 2]));/*
        std::complex<MYFLOAT> z(fft_help1[i * 2], fft_help1[i * 2 + 1]);
        auto mag = std::abs(z) * mod;
        auto phs = std::arg(z);
        auto res = std::polar(mag, phs);
        fft_out[i * 2] = res.real();
        fft_out[i * 2 + 1] =
                res.imag();*/
        fft_out[i * 2] = fft_help1[i * 2] * mod;
        fft_out[i * 2 + 1] = fft_help1[i * 2 + 1] * mod;
    }
    fft->backward(fft_out, fft_in);

    MYFLOAT max2 = 0;
    for (int i = 0; i < fft_size; i++)
        max2 = std::abs(fft_in[i]) > max2 ? std::abs(fft_in[i]) : max2;
    auto scale = max2 > 0 ? max1 / max2 : 1.0;
    for (int32_t i = 0; i < fft_size; i++) {
        UDD(fft_in[i])
        fft_in[i] *= scale;
    }
}


void random_phase(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    CHECKFFT(fft_size)
    auto fft_in = track->grain_buffer[channel];
    tsl::fft::fftshift(fft_in, fft_size);
    fft->forward(fft_in, fft_in);
    // Bins run to fft_size/2, not fft_size -- the old bound spent half its work
    // randomising the dead upper half of the packed buffer. Bin 0 is skipped
    // because it packs {DC, Nyquist} rather than a complex pair, so rotating it
    // as if it had a phase just mixes the two.
    for (int32_t i = 1; i < (fft_size >> 1u); i++) {
        auto phi = tsl::random::randomfloat((MYFLOAT) 0., (MYFLOAT) TWOPI_P);
        auto r = fft_in[i * 2];
        auto im = fft_in[i * 2 + 1];
        auto mag = sqrt(r * r + im * im);
        fft_in[i * 2] = cos(phi) * mag;
        fft_in[i * 2 + 1] = sin(phi) * mag;
    }
    fft->backward(fft_in, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);
}

// ---------------------------------------------------------------------------
// CROSS_VOCODER -- the modulator's band energies imposed on the source.
//
// Bands are geometric between ~31 Hz and 16 kHz with edges at the midpoints
// between centres, as before. What is new is that each band's gain now goes
// through an envelope follower instead of being taken raw from the current
// grain: a bare per-grain magnitude ratio is what makes a vocoder chatter and
// bubble. The time constants come from the grain rate, so they mean the same
// thing in seconds whatever DENSITY and FFT SIZE are set to.
// ---------------------------------------------------------------------------
static constexpr MYFLOAT VOC_ATTACK_S = 0.005;
static constexpr MYFLOAT VOC_RELEASE_S = 0.040;
static constexpr MYFLOAT VOC_LOW_HZ = 31.;
static constexpr MYFLOAT VOC_HIGH_HZ = 16000.;

void
vocode(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t fftsized2 = (int32_t) (fft_size >> 1u);
    int32_t channels = (int) _STATE->params[track->index][VOC2CHANS].load();
    if (channels < 2) channels = 2;
    if (channels > CROSS_VOC_MAX_BANDS) channels = CROSS_VOC_MAX_BANDS;
    if (channels > fftsized2) channels = fftsized2;

    auto grainbuffer = track->grain_buffer[channel];
    auto modulator = track->destinationz->fft_out[channel];
    auto carrier = (MYFLOAT (*)[2]) grainbuffer;
    auto modc = (MYFLOAT (*)[2]) modulator;
    fft->forward(grainbuffer, grainbuffer);
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif

    const MYFLOAT sr = _STATE->sr;
    const MYFLOAT binHz = sr / (MYFLOAT) fft_size;

    // Follower coefficients. One grain is one sample of these envelopes, so the
    // hop in seconds sets the coefficient; a hop longer than the time constant
    // lands at 1, i.e. no smoothing, which is the right degenerate case.
    MYFLOAT density = _STATE->params[track->index][DENSITY].load();
    if (density < 0.001) density = 0.001;
    const MYFLOAT hopS = 1. / density;
    MYFLOAT aAtk = 1. - exp(-hopS / VOC_ATTACK_S);
    MYFLOAT aRel = 1. - exp(-hopS / VOC_RELEASE_S);
    if (aAtk > 1.) aAtk = 1.;
    if (aRel > 1.) aRel = 1.;

    auto bandGain = track->vocBandGain[channel];
    if (track->vocBands[channel] != channels) {
        // band layout changed, the stored envelopes describe a different bank
        for (int32_t b = 0; b < CROSS_VOC_MAX_BANDS; ++b) bandGain[b] = 0;
        track->vocBands[channel] = channels;
        aAtk = aRel = 1.;
    }

    MYFLOAT startbin = 1;
    while (startbin * binHz < VOC_LOW_HZ && startbin < fftsized2 - 1)
        startbin++;
    int32_t endbin = fftsized2 - 1;
    while (endbin * binHz > VOC_HIGH_HZ && endbin > startbin + 1)
        endbin--;

    const MYFLOAT coeff2 = pow(endbin / startbin, 1. / ((MYFLOAT) channels - 1));

    auto output = carrier;
    MYFLOAT next_bin = startbin * coeff2;
    MYFLOAT start = 1;
    MYFLOAT stop = startbin + (next_bin - startbin) * .5;

    for (int32_t b = 0; b < channels; b++) {
        // The last band always runs to Nyquist. Previously the loop stopped
        // short and every bin past the final edge kept the raw carrier at unity.
        int32_t j0 = (int32_t) start, j1 = (int32_t) stop;
        if (b == channels - 1 || j1 >= fftsized2) j1 = fftsized2;
        if (j0 < 1) j0 = 1;
        if (j1 > fftsized2) j1 = fftsized2;

        if (j1 > j0) {
            MYFLOAT m{}, c{};
            for (int32_t j = j0; j < j1; j++) {
                c += sqrt(carrier[j][0] * carrier[j][0] + carrier[j][1] * carrier[j][1]);
                m += sqrt(modc[j][0] * modc[j][0] + modc[j][1] * modc[j][1]);
            }
            if (c < 1e-12) c = 1e-12;
            MYFLOAT wanted = m / c;
            if (!std::isfinite(wanted)) wanted = 0;

            MYFLOAT g = bandGain[b];
            g += (wanted > g ? aAtk : aRel) * (wanted - g);
            UDD(g)
            bandGain[b] = g;

            for (int32_t j = j0; j < j1; j++) {
                output[j][0] = carrier[j][0] * g;
                output[j][1] = carrier[j][1] * g;
            }
        }

        if (j1 >= fftsized2) break;
        start = stop;
        startbin = next_bin;
        next_bin *= coeff2;
        stop = startbin + (next_bin - startbin) * .5;
    }

    // High-frequency pass-through: everything above this frequency comes
    // straight from the modulator, which is what keeps sibilants intelligible.
    // VOC2HP stores 20*log10(f), so pow(10, hp*.05) is f in Hz; the bin it lands
    // in is f * fft_size / sr. The old expression carried an extra factor of two
    // (f * 2 / sr * fft_size) and then used the result as a bin index, so the
    // pass-through started an octave above the frequency on the knob. The loop
    // also ran to fft_size rather than fft_size/2, writing half its iterations
    // into the dead upper half of the packed buffer.
    const MYFLOAT hpHz = pow(10, _STATE->params[track->index][VOC2HP].load() * .05);
    int32_t cut = (int32_t) (hpHz / binHz);
    if (cut < 0) cut = 0;
    for (int32_t i = 1 + cut; i < fftsized2; i++) {
        grainbuffer[i * 2] = modulator[i * 2];
        grainbuffer[i * 2 + 1] = modulator[i * 2 + 1];
    }

    fft->backward(grainbuffer, grainbuffer);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(grainbuffer[n])
    normalize_grain(track, channel, grainbuffer, fft_size);
}


void dephase(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {


    if (playbackspeed == 0.0 || playbackspeed == 1.0) {
        // See dephase_locked: a skipped grain leaves the stored frame stale, so
        // the next processed grain re-seeds rather than accumulating against it.
        track->pvInit[channel] = true;
        return;
    }
    auto _appState = track->_appState;
    auto fft_in = track->grain_buffer[channel];

    int32_t current_index = track->current_nodes_pv[channel];
    int32_t prev_index = current_index == 0 ? 1 : 0;
    NodePV **current_nodes = track->nodes_pv[channel][current_index];
    NodePV **prev_nodes = track->nodes_pv[channel][prev_index];

    auto step_length =
            _STATE->sr / _STATE->params[track->index][DENSITY] * playbackspeed;
    auto tstretch_ratio = 1. / playbackspeed;
    tsl::fft::fftshift(fft_in, fft_size);
    CHECKFFT(fft_size)
    fft->forwardPolar(fft_in, fft_in);
    //ffts_execute(plan_normal, fft_in, fft_out);


    auto omegapre = TWOPI_P * step_length / (MYFLOAT) fft_size;

    const bool reseed = track->pvInit[channel];
    const int32_t M2 = fft_size >> 1u;

    for (int32_t i = 1; i < M2; i++) {
        auto mag = fft_in[i * 2];
        auto phi = fft_in[i * 2 + 1];
        UDF(phi)
        current_nodes[i]->phi = phi;
        MYFLOAT psi;
        if (reseed)
            psi = phi;
        else {
            auto omega = omegapre * i;
            auto delta_phi = omega + princarg(phi - prev_nodes[i]->phi - omega);
            psi = princarg(prev_nodes[i]->psi + delta_phi * tstretch_ratio);
        }
        UDF(psi)
        current_nodes[i]->psi = psi;
        fft_in[i * 2] = mag;
        fft_in[i * 2 + 1] = psi;
    }
    track->pvInit[channel] = false;

    fft->backwardPolar(fft_in, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);
    track->current_nodes_pv[channel] = current_index == 0 ? (uint8_t) 1 : (uint8_t) 0;
}

const MYFLOAT lowfr = 0.001;
const MYFLOAT highfr = 20000.f;
const MYFLOAT frrange = highfr - lowfr;
const MYFLOAT lowlog = LIN2SCALED4(lowfr);
const MYFLOAT highlog = LIN2SCALED4(highfr);
const MYFLOAT rangelog = LIN2SCALED4(frrange);


void pvosc(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    MYFLOAT sr = _STATE->sr;
    auto twopidffts = TWOPI_P / (MYFLOAT) fft_size;
    auto twopidsr = TWOPI_P / sr;

    int fftd2 = fft_size >> 1u;

    auto indices = reinterpret_cast<int *>(track->fft_help1[channel]);
    MYFLOAT *amp = track->fft_help1[channel] + fftd2;
    MYFLOAT *phases = track->fft_help2[channel];
    MYFLOAT *amps = track->fft_help2[channel] + fftd2;

    int32_t indexcount = 0;

    long phs = _DATA->offset + track->step_point_grain[channel];

    CHECKFFT(fft_size)

    int dophase = _STATE->params[track->index][PVAMPS2PHASE].load();
    int channels = _STATE->params[track->index][PVAMPS2RANGE].load();
    // PVAMPS2RANGE tops out at 100 and the smallest half-spectrum is 128, so
    // this only bites if either ever moves -- but nth_element with a negative
    // offset is undefined behaviour, not a wrong note.
    if (channels < 1) channels = 1;
    if (channels > fftd2) channels = fftd2;
    int n = fftd2 - channels;
    auto inbuf = (tsl::complex<MYFLOAT> *) track->fft_out[channel];
    auto in = track->grain_buffer[channel];


    fft->forwardPolar(in, track->fft_out[channel]);
    std::transform(inbuf, inbuf + fftd2, amps,
                   [](tsl::complex<MYFLOAT> f) { return f.r; });
    std::nth_element(amps, amps + n, amps + fftd2);
    auto thrsh = amps[n];
    int index = 0;
    std::transform(inbuf, inbuf + fftd2, inbuf,
                   [&amp, &phases, &indexcount, &index, &indices, thrsh, channels](
                           tsl::complex<MYFLOAT> f) {
                       // >= thrsh can select more than `channels` bins when
                       // amplitudes tie at the threshold, which used to walk
                       // past the end of amp[]/phases[]
                       if (f.r >= thrsh && indexcount < channels) {
                           amp[indexcount] = f.r;
                           phases[indexcount] = f.i;

                           indices[indexcount++] = index;
                           index++;
                           return f;
                       };
                       index++;
                       return tsl::complex<MYFLOAT>{0., 0.};

                   });

    std::memset(inbuf, 0, sizeof(MYFLOAT) * fft_size);

    // ...and fewer than `channels` when they tie below it. Synthesise only what
    // was actually picked; the tail of indices[] still holds the previous
    // grain's bins, which used to be resynthesised with this grain's amplitudes.
    for (int32_t wave = 0; wave < indexcount; wave++) {
        MYFLOAT phase;
        index = indices[wave];
        if (index >= fftd2)
            index = fftd2 - 1;
        if (dophase == 0)
            phase = phases[wave];
        else if (dophase == 1) {
            phase = fmod(index / (MYFLOAT) fft_size * phs * TWOPI_P, TWOPI_P);
        } else if (dophase == 2) phase = tsl::random::randomfloat(0., TWOPI_F_P);
        else phase = 0;
        inbuf[index].r = amp[wave] * cos(phase);
        inbuf[index].i = amp[wave] * sin(phase);
    }
    inbuf[0].i = 0.0;
    fft->backward(track->fft_out[channel], track->fft_out[channel]);

    auto wet = LOG2NORMAL(_STATE->params[track->index][PVAMPS2WET].load());
    auto dry = LOG2NORMAL(_STATE->params[track->index][PVAMPS2DRY].load());

    for (int32_t i = 0; i < fft_size; i++)
        in[i] = track->fft_out[channel][i] * wet + in[i] * dry;

};


void freqwarp(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    const int32_t M2 = fft_size >> 1u;

    CHECKFFT(fft_size)

    static constexpr MYFLOAT low{0.1};

    auto lfo = track->lfo[PVSPECBOUNDBIN].load();
    MYFLOAT binb;
    if (lfo && lfo->power()) {
        auto min = lfo->min(PVSPECBOUNDBIN), max = lfo->max(PVSPECBOUNDBIN), range = DISTANCE(
                min,
                max), start = min;
        if (min > max)range *= -1.;
        binb = start +
               track->lfobuffer[lfo->index][(int) track->step_point_grain[channel]] * range;
        if (binb > 1.0) binb = 1.0;
        else if (binb < 0) binb = 0;
    } else binb = _STATE->params[track->index][PVSPECBOUNDBIN];

    auto boundain =
            (LOG2NORMAL(
                     -20 +
                     20.82785370316 * _STATE->params[track->index][PVSPECBOUNDAIN].load()) -
             low) *
            (MYFLOAT) M2;
    auto boundbin =
            (LOG2NORMAL(-20 + 20.82785370316 * binb) - low) *
            (MYFLOAT) M2;
    auto boundaout =
            (LOG2NORMALF(
                     -20 +
                     20.82785370316 * _STATE->params[track->index][PVSPECBOUNDAOUT].load()) -
             low) *
            (MYFLOAT) M2;
    auto boundbout =
            (LOG2NORMALF(
                     -20 +
                     20.82785370316 * _STATE->params[track->index][PVSPECBOUNDBOUT].load()) -
             low) *
            (MYFLOAT) M2;

    auto incin = DISTANCE(boundain, boundbin) / (MYFLOAT) M2;
    auto incout = DISTANCE(boundaout, boundbout) / (MYFLOAT) M2;

    if (boundain > boundbin) {
        std::swap(boundain, boundbin);
    }
    if (boundaout > boundbout) {
        std::swap(boundaout, boundbout);
    }


    auto in = track->grain_buffer[channel], out = track->fft_out[channel];
    fft->forwardPolar(in, in);
    in[1] = 0;

    auto inbuf = (tsl::complex<MYFLOAT> *) in, outbuf = (tsl::complex<MYFLOAT> *) out;

    if (_STATE->params[track->index][PVSPECINV].load() == 1.0) {
        boundaout = boundbout;
        incout *= -1;
    }
    memset(out, 0, sizeof(tsl::complex<MYFLOAT>) * M2);
    auto counter = track->fft_help1[channel];       // summed weight per out bin
    std::memset(counter, 0, sizeof(MYFLOAT) * M2);

    for (int32_t i = 0; i < M2; i++) {
        // Source magnitude, linearly interpolated between the two bins the
        // fractional read position straddles -- the old code truncated the read
        // index, so when incin < 1 several steps read the identical bin.
        const int32_t si = (int32_t) boundain;
        const MYFLOAT sf = boundain - si;
        const int32_t si1 = si + 1 < M2 ? si + 1 : si;
        const MYFLOAT srcMag = inbuf[si].r * (1 - sf) + inbuf[si1].r * sf;

        // Spread that magnitude across the two output bins the write position
        // straddles, so a non-integer step neither piles onto one bin nor skips
        // bins (which the memset would have left silent). Only magnitude is
        // moved -- see the phase note after the loop.
        const int32_t oi = (int32_t) boundaout;
        const MYFLOAT of = boundaout - oi;
        const int32_t oi1 = oi + 1 < M2 ? oi + 1 : oi;

        outbuf[oi].r += srcMag * (1 - of);
        counter[oi] += (1 - of);
        if (oi1 != oi) {
            outbuf[oi1].r += srcMag * of;
            counter[oi1] += of;
        }

        boundain += incin;
        boundaout += incout;
        if (boundain >= M2 - 1) boundain = M2 - 1;
        else if (boundain < 0) boundain = 0;
        if (boundaout >= M2 - 1) boundaout = M2 - 1;
        else if (boundaout < 0) boundaout = 0;
    }

    // Phase stays native: each output bin keeps the source phase already at that
    // bin, i.e. the phase that belongs to that bin's own frequency. Carrying the
    // source *read* bin's phase to a shifted output bin would make the phase
    // advance at the wrong rate between hops (it is the advance for the read
    // frequency, not the write frequency), so overlap-added frames would no
    // longer align -- fixing that would need phase-vocoder phase propagation
    // (accumulate psi_j += omega_j * hop). Keeping the native phase is coherent
    // frame-to-frame for free; the warp moves the spectral-envelope shape only.
    for (int32_t i = 0; i < M2; i++) {
        if (counter[i] != 0)
            outbuf[i].r /= counter[i];
        outbuf[i].i = inbuf[i].i;
    }
    out[1] = 0;
    fft->backwardPolar(out, in);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(in[n])
};

void cross_convolve(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto in = track->grain_buffer[channel];
    auto out = track->fft_out[channel];
    auto M = fft_size << 1u;
    CHECKFFT(M)
    std::memset(in + fft_size, 0, fft_size * sizeof(MYFLOAT));
    fft->forward(in, in);
    auto control = track->destinationz->fft_out[channel];
#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif

#if (__aarch64__)
    auto zero = in[0] * control[0];
    auto one = in[1] * control[1];

    complexMultiplyDouble(out, in, control, fft_size);

    out[0] = zero;
    out[1] = one;
#else
    out[0] = in[0] * control[0];
    out[1] = in[1] * control[1];

    for (int i = 2; i < M; i += 2) {
        auto nimag = i + 1;
        out[i] = in[i] * control[i] - in[nimag] * control[nimag]; // real part
        out[nimag] = in[i] * control[nimag] + in[nimag] * control[i]; // imaginary part
    }
#endif
    fft->backward(out, in);
    for (int32_t n = 0; n < (int32_t) M; ++n) UDD(in[n])
    // The output is 2N (granulate doubles grainsize_int for this alg): RMS over
    // the whole 2N, and CROSS_CONV_TRIM compensates the extra OLA overlap that
    // the doubled grain length brings at the same hop.
    normalize_grain(track, channel, in, M, CROSS_CONV_TRIM);
}

// ---------------------------------------------------------------------------
// A one-pole low-pass run forward and then backward along the bin axis. Two
// passes rather than one because a single pass is a delay: it drags every peak
// upward in frequency, and a transport map built from dragged peaks moves energy
// to the wrong place. Run both ways the phase cancels and peaks stay put.
//
// `pole` is the recursion coefficient, so the smoothing width is about
// 1/(1-pole) bins.
// ---------------------------------------------------------------------------
static void smooth_bins(MYFLOAT *x, int32_t n, MYFLOAT pole) {
    if (n < 2 || !(pole > 0.)) return;
    if (pole > 0.999) pole = 0.999;
    const MYFLOAT g = 1. - pole;
    MYFLOAT y = x[0];
    for (int32_t i = 0; i < n; ++i) { y = g * x[i] + pole * y; x[i] = y; }
    y = x[n - 1];
    for (int32_t i = n - 1; i >= 0; --i) { y = g * x[i] + pole * y; x[i] = y; }
}

// ---------------------------------------------------------------------------
// CROSS_TRANSPORT -- the carrier's energy relocated onto the modulator's
// frequency distribution (1-D optimal transport between the two spectra).
//
// Every other cross algorithm rescales the carrier bin by bin: the modulator
// decides how loud each frequency is, and nothing ever moves along the frequency
// axis. That is why CROSS_INTER at half way sounds like two sounds crossfading
// rather than one sound becoming another -- amplitude morphing cannot slide a
// partial from where it is to where the target's partial is; it can only fade
// one down while the other comes up.
//
// This one builds the normalised cumulative magnitude distribution of each
// spectrum and maps the carrier's quantiles onto the modulator's. Bin i of the
// carrier holds some fraction u of the carrier's total magnitude below it; its
// target is the bin where the modulator holds the same fraction u. AMOUNT
// interpolates the read position between the two, so sweeping it makes the
// carrier's partials *glide* into the modulator's positions and land exactly on
// its harmonic grid at 1.0.
//
// Energy is summed, not averaged, at the destination: the map is
// mass-preserving by construction (equal quantiles carry equal mass), so summing
// is what reproduces the modulator's shape at AMOUNT 1. freqwarp averages
// instead because its map comes from four knobs and has no such guarantee.
//
// SMOOTH sets how blobby the map is. At 0 the map is per-bin, and every ripple
// in either spectrum is a local displacement -- surgical, grainy, unstable
// between grains. Turned up, the map is built from formant-scale estimates and
// energy moves in coherent lumps, which is the gliding sound. Only the map is
// built from the smoothed spectra; what actually gets moved is the carrier's
// raw magnitudes, so smoothing changes where energy goes and never blurs it.
// Past about 0.8 both spectra smooth into the same broad ramp, the two CDFs
// converge and the map slides back towards the identity -- so the top of the
// knob is "barely moves" rather than "moves furthest". Measured: at 0 the
// carrier's partials land on the modulator's bins exactly, at 0.35 within a
// bin or two, at 0.9 they have hardly left home.
//
// FLOOR removes a fraction of the modulator's own peak from its distribution
// before the CDF is taken. Without it a modulator with any noise floor spreads
// target mass evenly across the whole spectrum, and the map degenerates towards
// the identity no matter how loud the modulator's actual partials are.
//
// Phase stays native, for the reason set out in freqwarp: each output bin keeps
// the phase that belongs to its own frequency, so consecutive overlap-added
// grains stay aligned without phase-vocoder propagation.
// ---------------------------------------------------------------------------
void cross_transport(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    auto car = track->grain_buffer[channel];
    fft->forwardPolar(car, car);

    MYFLOAT t = _STATE->params[track->index][CROSSTRANSAMT].load();
    LFO *lfo = track->lfo[CROSSTRANSAMT].load();
    if (lfo && lfo->power()) {
        const auto a = _STATE->controls[track->index][CROSSTRANSAMT].lfo_min.load();
        const auto b = _STATE->controls[track->index][CROSSTRANSAMT].lfo_max.load();
        t = a + lfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (t < 0.) t = 0.; else if (t > 1.) t = 1.;

    const MYFLOAT sm = _STATE->params[track->index][CROSSTRANSSMOOTH].load();
    // cubed so the knob's lower half covers the few-bin widths, where the
    // character actually changes, instead of compressing them into the first
    // tenth of the travel.
    const MYFLOAT pole = 1. - (1. - sm) * (1. - sm) * (1. - sm);
    const MYFLOAT fl = _STATE->params[track->index][CROSSTRANSFLOOR].load();
    const MYFLOAT floorRel = 0.5 * fl * fl * fl;

#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    const MYFLOAT *mod = track->destinationz->fft_out[channel];

    MYFLOAT *ca = track->fft_help1[channel];        // carrier magnitudes
    MYFLOAT *ma = track->fft_help1[channel] + M2;   // modulator magnitudes
    MYFLOAT *cc = track->fft_help2[channel];        // carrier CDF
    MYFLOAT *mc = track->fft_help2[channel] + M2;   // modulator CDF
    MYFLOAT *out = track->fft_out[channel];

    // Bin 0 packs {DC, Nyquist} rather than a magnitude/phase pair, so it is
    // excluded from both distributions and the output.
    ca[0] = ma[0] = 0;
    for (int32_t i = 1; i < M2; ++i) {
        const MYFLOAT a = car[i * 2], b = mod[i * 2];
        ca[i] = (a > 0. && std::isfinite(a)) ? a : 0.;
        ma[i] = (b > 0. && std::isfinite(b)) ? b : 0.;
    }

    smooth_bins(ca, M2, pole);
    smooth_bins(ma, M2, pole);

    MYFLOAT mpeak = 0;
    for (int32_t i = 1; i < M2; ++i) if (ma[i] > mpeak) mpeak = ma[i];
    const MYFLOAT sub = mpeak * floorRel;
    MYFLOAT ctot = 0, mtot = 0;
    for (int32_t i = 1; i < M2; ++i) {
        ma[i] = ma[i] > sub ? ma[i] - sub : 0.;
        ctot += ca[i];
        mtot += ma[i];
    }

    if (!(ctot > 0.) || !(mtot > 0.)) {
        // A silent side leaves the map undefined; pass the carrier through
        // rather than collapsing the grain. Levelled but not learned from --
        // this is not the algorithm's output (see normalize_grain).
        fft->backwardPolar(car, car);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(car[n])
        normalize_grain(track, channel, car, fft_size, 1., false);
        return;
    }

    {
        const MYFLOAT invc = 1. / ctot, invm = 1. / mtot;
        MYFLOAT acc = 0;
        for (int32_t i = 1; i < M2; ++i) { acc += ma[i]; mc[i] = acc * invm; }
        acc = 0;
        // Midpoint CDF on the carrier side: using the running total *after*
        // adding bin i biases every mapping half a bin high.
        for (int32_t i = 1; i < M2; ++i) {
            const MYFLOAT prev = acc;
            acc += ca[i];
            cc[i] = (prev + acc) * .5 * invc;
        }
    }

    std::memset(out, 0, fft_size * sizeof(MYFLOAT));

    // cc is non-decreasing in i, so the search for the matching modulator
    // quantile only ever walks forward: the whole map costs one pass.
    int32_t j = 1;
    for (int32_t i = 1; i < M2; ++i) {
        const MYFLOAT srcMag = car[i * 2];
        if (!(srcMag > 0.) || !std::isfinite(srcMag)) continue;
        const MYFLOAT u = cc[i];
        while (j < M2 - 1 && mc[j] < u) ++j;
        const MYFLOAT lo = j > 1 ? mc[j - 1] : 0.;
        const MYFLOAT w = mc[j] - lo;
        MYFLOAT frac = w > 1e-15 ? (u - lo) / w : .5;
        if (frac < 0.) frac = 0.; else if (frac > 1.) frac = 1.;

        MYFLOAT pos = (MYFLOAT) i + t * (((MYFLOAT) j - .5 + frac) - (MYFLOAT) i);
        if (pos < 1.) pos = 1.;
        else if (pos > (MYFLOAT) (M2 - 1)) pos = (MYFLOAT) (M2 - 1);

        const int32_t p0 = (int32_t) pos;
        const MYFLOAT pf = pos - (MYFLOAT) p0;
        out[p0 * 2] += srcMag * (1. - pf);
        if (p0 + 1 < M2) out[(p0 + 1) * 2] += srcMag * pf;
    }

    for (int32_t i = 1; i < M2; ++i) out[i * 2 + 1] = car[i * 2 + 1];
    out[0] = out[1] = 0;

    fft->backwardPolar(out, car);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(car[n])
    normalize_grain(track, channel, car, fft_size);
}


// ---------------------------------------------------------------------------
// CROSS_STACK -- the carrier transposed onto every partial the modulator plays.
//
// The modulator side (granulate_fft.cpp) picks its PEAKS strongest spectral
// peaks and hands them over as { count, bin, mag, ... } ascending in frequency.
// Here the lowest of them is taken as the root, and the carrier's magnitude
// spectrum is resampled by each peak's ratio to that root and summed. The first
// copy is therefore always the carrier untransposed, and the rest sit at
// whatever intervals the modulator's spectrum implies.
//
// VOCODER and LPC transplant the modulator's *timbre* and leave the carrier's
// pitch alone; nothing until now transferred pitch. A voice modulator makes a
// sustained sample sing the voice's chord, a bell makes it play the bell's
// inharmonic stack, and a modulator with one strong partial reduces to a plain
// unison. That is the whole range from one control.
//
// KEEP FORMANTS divides each copy by the carrier's own spectral envelope at the
// position it was read from and multiplies by the envelope at the position it
// lands on, so the transposed copies keep the carrier's formants where they were
// instead of sliding up with the pitch. Off is the chipmunked version, which is
// the right answer for inharmonic and percussive carriers.
//
// TILT is the exponent on each peak's magnitude as a mixing weight: 0 stacks all
// the peaks equally regardless of how loud they are in the modulator (dense,
// organ-like), 1 mixes them in proportion, 2 lets the modulator's loudest
// partial dominate.
// ---------------------------------------------------------------------------
static constexpr MYFLOAT CROSS_STACK_MAX_RATIO = 8.;   // three octaves

void cross_stack(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    auto car = track->grain_buffer[channel];
    fft->forwardPolar(car, car);

    const bool keepForm = _STATE->params[track->index][CROSSSTACKFORM].load() == 1.0;

    MYFLOAT tilt = _STATE->params[track->index][CROSSSTACKTILT].load();
    LFO *tiltLfo = track->lfo[CROSSSTACKTILT].load();
    if (tiltLfo && tiltLfo->power()) {
        const auto a = _STATE->controls[track->index][CROSSSTACKTILT].lfo_min.load();
        const auto b = _STATE->controls[track->index][CROSSSTACKTILT].lfo_max.load();
        tilt = a + tiltLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (tilt < 0.) tilt = 0.; else if (tilt > 1.) tilt = 1.;
    const MYFLOAT tiltExp = tilt * 2.;

    MYFLOAT nPeaks = _STATE->params[track->index][CROSSSTACKN].load();
    LFO *nLfo = track->lfo[CROSSSTACKN].load();
    if (nLfo && nLfo->power()) {
        const auto a = _STATE->controls[track->index][CROSSSTACKN].lfo_min.load();
        const auto b = _STATE->controls[track->index][CROSSSTACKN].lfo_max.load();
        nPeaks = a + nLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }

#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    const MYFLOAT *mod = track->destinationz->fft_out[channel];

    // The modulator hands over its CROSS_STACK_MAX_PEAKS strongest peaks,
    // ascending in frequency; PEAKS keeps the strongest K of those and the
    // lowest survivor becomes the root. Narrowing here rather than on the
    // modulator side is what lets PEAKS take an LFO.
    int32_t avail = (int32_t) mod[0];
    if (avail < 0) avail = 0;   // fft_out before the modulator's first write
    else if (avail > CROSS_STACK_MAX_PEAKS) avail = CROSS_STACK_MAX_PEAKS;
    int32_t K = (int32_t) std::lround(nPeaks);
    if (K < 1) K = 1;
    if (K > CROSS_STACK_MAX_PEAKS) K = CROSS_STACK_MAX_PEAKS;
    if (K > avail) K = avail;

    MYFLOAT pkBin[CROSS_STACK_MAX_PEAKS], pkMag[CROSS_STACK_MAX_PEAKS];
    int32_t nsel = 0;
    if (avail > 0 && K > 0) {
        bool taken[CROSS_STACK_MAX_PEAKS]{};
        for (int32_t s = 0; s < K; ++s) {
            int32_t best = -1;
            for (int32_t q = 0; q < avail; ++q)
                if (!taken[q] && (best < 0 || mod[2 + 2 * q] > mod[2 + 2 * best])) best = q;
            if (best < 0) break;
            taken[best] = true;
        }
        // mod is already ascending in frequency, so collecting in order keeps it so
        for (int32_t q = 0; q < avail; ++q)
            if (taken[q]) { pkBin[nsel] = mod[1 + 2 * q]; pkMag[nsel] = mod[2 + 2 * q]; ++nsel; }
    }

    const MYFLOAT root = nsel > 0 ? pkBin[0] : 0.;

    if (nsel < 1 || !(root > 0.)) {
        // No peak found -- a silent or featureless modulator grain. Pass the
        // carrier rather than muting it. Levelled but not learned from --
        // this is not the algorithm's output (see normalize_grain).
        fft->backwardPolar(car, car);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(car[n])
        normalize_grain(track, channel, car, fft_size, 1., false);
        return;
    }

    MYFLOAT *mag = track->fft_help1[channel];
    MYFLOAT *env = track->fft_help1[channel] + M2;
    MYFLOAT *out = track->fft_out[channel];

    mag[0] = 0;
    for (int32_t i = 1; i < M2; ++i) {
        const MYFLOAT m = car[i * 2];
        mag[i] = (m > 0. && std::isfinite(m)) ? m : 0.;
    }
    if (keepForm) {
        for (int32_t i = 0; i < M2; ++i) env[i] = mag[i];
        // Envelope width as a fixed fraction of the spectrum, so it stays the
        // same shape in Hz whatever FFT SIZE is set to.
        smooth_bins(env, M2, 1. - 32. / (MYFLOAT) (M2 > 32 ? M2 : 32));
    }

    MYFLOAT mref = 0;
    for (int32_t k = 0; k < nsel; ++k) if (pkMag[k] > mref) mref = pkMag[k];

    std::memset(out, 0, fft_size * sizeof(MYFLOAT));
    MYFLOAT wsum = 0;

    for (int32_t k = 0; k < nsel; ++k) {
        const MYFLOAT ratio = pkBin[k] / root;
        if (!(ratio > 0.) || ratio > CROSS_STACK_MAX_RATIO) continue;

        MYFLOAT w = mref > 0. ? pkMag[k] / mref : 1.;
        if (w < 1e-9) w = 1e-9;
        if (tiltExp <= 0.) w = 1.;
        else if (tiltExp != 1.) w = std::exp2(tiltExp * std::log2(w));
        if (!(w > 0.) || !std::isfinite(w)) continue;
        wsum += w;

        const MYFLOAT inv = 1. / ratio;
        for (int32_t i = 1; i < M2; ++i) {
            const MYFLOAT s = (MYFLOAT) i * inv;
            if (s < 1. || s >= (MYFLOAT) (M2 - 1)) continue;
            const int32_t s0 = (int32_t) s;
            const MYFLOAT sf = s - (MYFLOAT) s0;
            MYFLOAT m = mag[s0] * (1. - sf) + mag[s0 + 1] * sf;
            if (keepForm) {
                const MYFLOAT es = env[s0] * (1. - sf) + env[s0 + 1] * sf;
                m = es > 1e-15 ? m * env[i] / es : 0.;
            }
            out[i * 2] += w * m;
        }
    }

    if (wsum > 0.) {
        const MYFLOAT g = 1. / wsum;
        for (int32_t i = 1; i < M2; ++i) out[i * 2] *= g;
    }
    for (int32_t i = 1; i < M2; ++i) out[i * 2 + 1] = car[i * 2 + 1];
    out[0] = out[1] = 0;

    fft->backwardPolar(out, car);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(car[n])
    normalize_grain(track, channel, car, fft_size);
}


// ---------------------------------------------------------------------------
// CROSS_DUCK -- the modulator's spectrum as a per-bin gate on the carrier.
//
// Every other cross algorithm imprints the modulator's envelope continuously;
// this one makes a binary decision per bin and the carrier keeps its own
// timbre on whichever side of it survives. DUCK passes the carrier where the
// modulator is QUIET -- a spectral sidechain, two loops interlocking without
// ever summing -- and KEY is the inverse, the carrier audible only where the
// modulator has energy.
//
// The threshold is relative to the modulator frame's own RMS, not absolute:
// the gate then keys on the modulator's spectral SHAPE and holds its behaviour
// when either track's level moves (the level calibration downstream would
// fight an absolute threshold anyway). SOFT is a knee in dB around the
// threshold -- at 0 the mask is binary, the maximal-contrast setting. SMOOTH
// is a per-bin one-pole on the gain across grains: the raw per-grain decision
// flickers at the grain rate on material that hovers around the threshold, and
// the pole turns that flicker into an envelope. FLOOR is how far down a closed
// bin goes (0 = fully removed, -60..0 dB otherwise) -- a gate range, so the
// carrier can shine through the holes instead of vanishing into them.
//
// A silent modulator opens every bin in DUCK and closes every bin in KEY; the
// KEY silence is the algorithm's output, not a fallback, so the grain is
// levelled normally (a near-silent grain barely moves the calibration).
// ---------------------------------------------------------------------------
void cross_duck(TRACK *track, uint8_t channel, uint32_t fft_size, bool multi) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 2) return;
    auto car = track->grain_buffer[channel];
    fft->forwardPolar(car, car);

    MYFLOAT th = _STATE->params[track->index][CROSSDUCKTHRESH].load();
    if (th < 0.) th = 0.; else if (th > 1.) th = 1.;
    MYFLOAT soft = _STATE->params[track->index][CROSSDUCKSOFT].load();
    if (soft < 0.) soft = 0.; else if (soft > 1.) soft = 1.;
    MYFLOAT smoo = _STATE->params[track->index][CROSSDUCKSMOOTH].load();
    if (smoo < 0.) smoo = 0.; else if (smoo > 1.) smoo = 1.;
    MYFLOAT fl = _STATE->params[track->index][CROSSDUCKFLOOR].load();
    if (fl < 0.) fl = 0.; else if (fl > 1.) fl = 1.;
    const bool key = (int) _STATE->params[track->index][CROSSDUCKMODE].load() == 1;

#ifdef IS_MULTITHREADED
    if (multi)
        track->destinationz->sem_cross[channel].wait();
#endif
    const MYFLOAT *mod = track->destinationz->fft_out[channel];

    // The modulator frame's RMS magnitude, the reference the threshold hangs
    // off. Bin 0 packs {DC, Nyquist}, excluded as everywhere else.
    MYFLOAT acc = 0;
    for (int32_t i = 1; i < M2; ++i) {
        const MYFLOAT m = mod[i * 2];
        if (m > 0. && std::isfinite(m)) acc += m * m;
    }
    const MYFLOAT ref = std::sqrt(acc / (MYFLOAT) (M2 - 1));

    // -60..+20 dB relative to the frame RMS.
    const MYFLOAT thLin = ref * std::pow((MYFLOAT) 10., ((MYFLOAT) -60. + (MYFLOAT) 80. * th) / (MYFLOAT) 20.);
    // Knee width in dB, squared so the low half of the knob stays near-binary;
    // floored at 0.75 dB, which is a hard gate for any musical purpose.
    MYFLOAT knee = (MYFLOAT) 40. * soft * soft;
    if (knee < (MYFLOAT) .75) knee = (MYFLOAT) .75;
    // -60..0 dB gate range; exactly 0 removes the bin entirely.
    const MYFLOAT floorLin = fl > 0. ? std::pow((MYFLOAT) 10., (MYFLOAT) 3. * (fl - (MYFLOAT) 1.)) : (MYFLOAT) 0.;

    // The pole is set from the grain hop in TIME (like the level calibration):
    // 0 = every grain decides alone, 1 = about two seconds of memory.
    const MYFLOAT dens = _STATE->params[track->index][DENSITY].load();
    const MYFLOAT hop = dens > 0. ? (MYFLOAT) 1. / dens : (MYFLOAT) 0.01;
    const MYFLOAT tau = (MYFLOAT) 2. * smoo * smoo * smoo;
    MYFLOAT pole = tau > (MYFLOAT) 1e-4 ? std::exp(-hop / tau) : (MYFLOAT) 0.;
    if (!(pole >= 0.) || pole > (MYFLOAT) .9999) pole = pole > 0. ? (MYFLOAT) .9999 : (MYFLOAT) 0.;

    MYFLOAT *gs = track->duckGain[channel];
    const bool seed = track->duckFill[channel] || track->duckSize[channel] != M2;

    const bool refDead = !(thLin > (MYFLOAT) 1e-12);
    for (int32_t i = 1; i < M2; ++i) {
        MYFLOAT g;
        if (refDead) {
            // Digital silence on the modulator: no reference to gate against,
            // so the mask is all-open (DUCK) or all-closed (KEY).
            g = key ? (MYFLOAT) 0. : (MYFLOAT) 1.;
        } else {
            const MYFLOAT m = mod[i * 2];
            MYFLOAT x;
            if (m > 0. && std::isfinite(m)) {
                const MYFLOAT db = (MYFLOAT) 20. * std::log10(m / thLin);
                x = db / knee + (MYFLOAT) .5;
                if (x < 0.) x = 0.; else if (x > 1.) x = 1.;
            } else
                x = 0.;
            g = x * x * ((MYFLOAT) 3. - (MYFLOAT) 2. * x);
            if (!key) g = (MYFLOAT) 1. - g;
        }
        g = floorLin + ((MYFLOAT) 1. - floorLin) * g;

        if (seed) gs[i] = g;
        else gs[i] = g + (gs[i] - g) * pole;
        UDF(gs[i])
        car[i * 2] *= gs[i];
    }
    if (seed) {
        track->duckSize[channel] = M2;
        track->duckFill[channel] = false;
    }
    car[0] = car[1] = 0;

    fft->backwardPolar(car, car);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(car[n])
    normalize_grain(track, channel, car, fft_size);
}


// ---------------------------------------------------------------------------
// Sliding-window median helpers for hpss(). The window is kept sorted, so each
// step is one binary search plus one memmove -- O(width) rather than the
// O(width log width) a per-bin re-sort would cost, which matters because this
// runs over every bin of every grain.
// ---------------------------------------------------------------------------
static void median_win_remove(MYFLOAT *w, int32_t n, MYFLOAT v) {
    int32_t lo = 0, hi = n;
    while (lo < hi) {
        const int32_t m = (lo + hi) >> 1;
        if (w[m] < v) lo = m + 1; else hi = m;
    }
    if (lo >= n) lo = n - 1;
    std::memmove(w + lo, w + lo + 1, (size_t) (n - 1 - lo) * sizeof(MYFLOAT));
}

static void median_win_insert(MYFLOAT *w, int32_t n, MYFLOAT v) {
    int32_t i = n;
    while (i > 0 && w[i - 1] > v) { w[i] = w[i - 1]; --i; }
    w[i] = v;
}

// ---------------------------------------------------------------------------
// PV: HARM/PERC -- median-filter harmonic/percussive separation (Fitzgerald).
//
// A sustained partial is steady in time and narrow in frequency; a transient is
// the other way round. So the median of a bin across the last few grains
// suppresses transients and estimates the harmonic part, and the median across
// neighbouring bins within one grain suppresses narrow partials and estimates
// the percussive part. The two estimates are turned into complementary masks and
// applied to the grain's own magnitudes.
//
// Nothing else in the PV list touches time structure at all, and the payoff is
// larger here than in an offline tool: granulate only the harmonic part and the
// transients stop machine-gunning at the grain rate; granulate only the
// percussive part and any sustained sample becomes a rhythm source.
//
// The time median runs over the last PV_HPSS_FRAMES *grains*, which in a
// granulator are not consecutive frames of one signal -- with a random read
// offset they are not even in order. That is deliberate and it still works: what
// the median rejects is any bin that is loud in a minority of the frames, and a
// transient is exactly that however the frames were gathered. It does mean the
// estimate degrades as read positions scatter, which is audible as the harmonic
// side thinning out at high RND READ, not as an artefact.
//
// MIX crossfades so that 0.5 returns the input untouched (the masks sum to one),
// 0 is pure percussive and 1 is pure harmonic.
// ---------------------------------------------------------------------------
void hpss(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 4) return;
    auto spec = track->grain_buffer[channel];
    fft->forwardPolar(spec, spec);

    const int32_t stride = WINDOW_SIZE / 2;
    MYFLOAT *hist = track->hpssHist[channel];

    int32_t pos = track->hpssPos[channel];
    if (track->hpssSize[channel] != M2 || track->hpssFill[channel]) {
        // A different half-spectrum, or the algorithm was just selected: the
        // stored frames describe another layout (or another sound), so seed every
        // slot with the current grain instead of medianing across the change.
        pos = 0;
        for (int32_t f = 0; f < PV_HPSS_FRAMES; ++f) {
            MYFLOAT *dst = hist + f * stride;
            dst[0] = 0;
            for (int32_t i = 1; i < M2; ++i) {
                const MYFLOAT m = spec[i * 2];
                dst[i] = (m > 0. && std::isfinite(m)) ? m : 0.;
            }
        }
        track->hpssSize[channel] = M2;
        track->hpssFill[channel] = false;
    } else {
        pos = pos + 1 >= PV_HPSS_FRAMES ? 0 : pos + 1;
        MYFLOAT *dst = hist + pos * stride;
        dst[0] = 0;
        for (int32_t i = 1; i < M2; ++i) {
            const MYFLOAT m = spec[i * 2];
            dst[i] = (m > 0. && std::isfinite(m)) ? m : 0.;
        }
    }
    track->hpssPos[channel] = pos;
    const MYFLOAT *cur = hist + pos * stride;

    MYFLOAT wx = _STATE->params[track->index][PVHPSSWIDTH].load();
    if (wx < 0.) wx = 0.; else if (wx > 1.) wx = 1.;
    int32_t W = 3 + 2 * (int32_t) std::lround(wx * ((PV_HPSS_MAX_WIDTH - 3) * .5));
    if (W > M2 - 1) W = ((M2 - 1) | 1) - 2;
    if (W < 3) W = 3;
    const int32_t half = W >> 1;

    // Frequency median of the current frame, edge-clamped into a padded copy so
    // the window can slide without a special case at either end.
    MYFLOAT *ext = track->fft_help2[channel];
    MYFLOAT *perc = track->fft_help1[channel];
    for (int32_t i = 0; i < half; ++i) ext[i] = cur[1];
    for (int32_t i = 0; i < M2; ++i) ext[half + i] = cur[i];
    for (int32_t i = 0; i < half; ++i) ext[half + M2 + i] = cur[M2 - 1];

    MYFLOAT win[PV_HPSS_MAX_WIDTH];
    for (int32_t i = 0; i < W; ++i) median_win_insert(win, i, ext[i]);
    perc[0] = win[half];
    for (int32_t i = 1; i < M2; ++i) {
        median_win_remove(win, W, ext[i - 1]);
        median_win_insert(win, W - 1, ext[i + W - 1]);
        perc[i] = win[half];
    }

    const int32_t mask = (int32_t) _STATE->params[track->index][PVHPSSMASK].load();
    MYFLOAT mix = _STATE->params[track->index][PVHPSSMIX].load();
    LFO *mixLfo = track->lfo[PVHPSSMIX].load();
    if (mixLfo && mixLfo->power()) {
        const auto a = _STATE->controls[track->index][PVHPSSMIX].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVHPSSMIX].lfo_max.load();
        mix = a + mixLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (mix < 0.) mix = 0.; else if (mix > 1.) mix = 1.;

    MYFLOAT frames[PV_HPSS_FRAMES];
    for (int32_t i = 1; i < M2; ++i) {
        for (int32_t f = 0; f < PV_HPSS_FRAMES; ++f) frames[f] = hist[f * stride + i];
        // insertion sort, PV_HPSS_FRAMES is 5
        for (int32_t a = 1; a < PV_HPSS_FRAMES; ++a) {
            const MYFLOAT v = frames[a];
            int32_t b = a - 1;
            while (b >= 0 && frames[b] > v) { frames[b + 1] = frames[b]; --b; }
            frames[b + 1] = v;
        }
        const MYFLOAT h = frames[PV_HPSS_FRAMES >> 1];
        const MYFLOAT p = perc[i];

        MYFLOAT mh;
        switch (mask) {
            case 0: { const MYFLOAT d = h + p; mh = d > 1e-20 ? h / d : .5; break; }
            case 1: { const MYFLOAT hh = h * h, pp = p * p, d = hh + pp;
                      mh = d > 1e-20 ? hh / d : .5; break; }
            case 2: { MYFLOAT hh = h * h; hh *= hh;
                      MYFLOAT pp = p * p; pp *= pp;
                      const MYFLOAT d = hh + pp;
                      mh = d > 1e-20 ? hh / d : .5; break; }
            default: mh = h > p ? 1. : 0.; break;
        }
        const MYFLOAT mp = 1. - mh;
        // Never exceeds one, and passes the grain through untouched at 0.5.
        const MYFLOAT g = mix <= .5 ? mp + (2. * mix) * mh
                                    : mh + (2. - 2. * mix) * mp;
        spec[i * 2] *= g;
    }
    spec[0] = spec[1] = 0;

    fft->backwardPolar(spec, spec);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
}


// ---------------------------------------------------------------------------
// PV: CONTRAST -- per-bin magnitude exponent with per-band energy held.
//
// Raising every magnitude to a power stretches or compresses the distance
// between the loud and the quiet parts of the spectrum. Above 1 only the peaks
// survive and the sound turns pure and glassy, the noise between partials gone.
// Below 1 the spectrum flattens towards white: the sample's spectral *shape* is
// still there but its pitch dissolves into a formant cloud, and the room, the
// hiss and the reverb tail come up level with the notes.
//
// The exponent alone would also destroy the spectral envelope, so each band's
// energy is measured before and restored after: the operation changes contrast
// *within* bands and leaves the coarse spectral shape (and therefore the
// loudness) where it was. BANDS at 1 is the ungoverned version, which thins the
// sound drastically -- that is a usable extreme rather than a mistake.
//
// FLOOR is the level, relative to the frame's peak, below which a bin is taken
// as empty. It matters only for exponents below 1, where without it the pow
// lifts numerical dust and quantisation noise into an audible hiss bed along
// with the material worth hearing.
//
// Magnitudes are normalised by the band peak before the exponent so the pow
// operates on values in [0,1] whatever the incoming scale; the band's energy
// restore puts the level back.
// ---------------------------------------------------------------------------
void spectral_contrast(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 2) return;
    auto spec = track->grain_buffer[channel];
    fft->forwardPolar(spec, spec);

    MYFLOAT x = _STATE->params[track->index][PVCONTRAST].load();
    LFO *lfo = track->lfo[PVCONTRAST].load();
    if (lfo && lfo->power()) {
        const auto a = _STATE->controls[track->index][PVCONTRAST].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVCONTRAST].lfo_max.load();
        x = a + lfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (x < 0.) x = 0.; else if (x > 1.) x = 1.;
    const MYFLOAT gamma = std::exp2((x - .5) * 4.);        // 0.25 .. 4, unity at 0.5

    MYFLOAT peak = 0;
    for (int32_t i = 1; i < M2; ++i) if (spec[i * 2] > peak) peak = spec[i * 2];

    if (std::abs(gamma - 1.) < 1e-4 || !(peak > 0.)) {
        fft->backwardPolar(spec, spec);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
        return;
    }

    int32_t bands = 1 + (int32_t) std::lround(
            _STATE->params[track->index][PVCONTRASTBANDS].load() * 47.);
    if (bands < 1) bands = 1; else if (bands > 48) bands = 48;

    MYFLOAT fl = _STATE->params[track->index][PVCONTRASTFLOOR].load();
    LFO *flLfo = track->lfo[PVCONTRASTFLOOR].load();
    if (flLfo && flLfo->power()) {
        const auto a = _STATE->controls[track->index][PVCONTRASTFLOOR].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVCONTRASTFLOOR].lfo_max.load();
        fl = a + flLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (fl < 0.) fl = 0.; else if (fl > 1.) fl = 1.;
    const MYFLOAT cut = peak * std::pow(10., (-120. + fl * 108.) * .05);

    const MYFLOAT step = std::pow((MYFLOAT) M2, 1. / (MYFLOAT) bands);
    MYFLOAT edge = 1.;
    int32_t b0 = 1;
    for (int32_t b = 0; b < bands && b0 < M2; ++b) {
        edge *= step;
        int32_t b1 = (b == bands - 1) ? M2 : (int32_t) edge;
        if (b1 > M2) b1 = M2;
        if (b1 <= b0) continue;

        MYFLOAT bp = 0, e0 = 0;
        for (int32_t i = b0; i < b1; ++i) {
            const MYFLOAT m = spec[i * 2];
            if (m > bp) bp = m;
            e0 += m * m;
        }
        if (bp > 0. && e0 > 0.) {
            const MYFLOAT invbp = 1. / bp;
            MYFLOAT e1 = 0;
            for (int32_t i = b0; i < b1; ++i) {
                MYFLOAT m = spec[i * 2];
                if (m < cut || !(m > 0.)) m = 0.;
                else m = bp * std::exp2(gamma * std::log2(m * invbp));
                spec[i * 2] = m;
                e1 += m * m;
            }
            if (e1 > 1e-30) {
                const MYFLOAT g = std::sqrt(e0 / e1);
                for (int32_t i = b0; i < b1; ++i) spec[i * 2] *= g;
            }
        }
        b0 = b1;
    }
    spec[0] = spec[1] = 0;

    fft->backwardPolar(spec, spec);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
}


// ---------------------------------------------------------------------------
// PV: SPECTRAL SNAP -- every spectral peak dragged onto a tuning grid.
//
// The peaks are found and the spectrum split into one region per peak exactly as
// dephase_locked does it; instead of rotating each region's phase, each region is
// *translated* bodily to put its peak on the nearest grid frequency. Translating
// rather than scaling is the point: a peak's shape is the analysis window's
// mainlobe, and it has to arrive intact or the partial turns to mush.
//
// Noise gets a pitch. A cymbal becomes a chord, speech becomes a choir, and in
// HARMONIC mode anything at all collapses onto one fundamental's harmonic
// series, which is the strongest version of the effect.
//
// The grain is fftshifted before analysis so that it is centred: a centred
// grain's mainlobe carries a near-constant phase, so the translated block lands
// as a clean partial at its new bin. Without the shift the phase ramps across
// the lobe and the translated copy smears.
//
// Where two regions land on the same bin the louder one wins outright, phase and
// all, rather than the magnitudes being summed: one bin can only carry one
// phase, and summing magnitudes under a phase that belongs to just one of the
// two overstates the result. That discards energy, so the grain's magnitude
// energy is matched back to the input's at the end -- with a coarse grid most
// peaks collide, and without the match SNAP would drop several dB the moment it
// starts working.
// ---------------------------------------------------------------------------
void spectral_snap(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 8) return;
    auto spec = track->grain_buffer[channel];

    tsl::fft::fftshift(spec, fft_size);
    fft->forwardPolar(spec, spec);

    MYFLOAT amt = _STATE->params[track->index][PVSNAPAMT].load();
    LFO *lfo = track->lfo[PVSNAPAMT].load();
    if (lfo && lfo->power()) {
        const auto a = _STATE->controls[track->index][PVSNAPAMT].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVSNAPAMT].lfo_max.load();
        amt = a + lfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (amt < 0.) amt = 0.; else if (amt > 1.) amt = 1.;

    // The parameter is stored as 20*log10(f), so interpolating the LFO in the
    // stored domain sweeps the root linearly in pitch rather than in hertz --
    // which is what makes an LFO on the root play a melody with the grid instead
    // of smearing the low end and skipping the top.
    MYFLOAT rootStored = _STATE->params[track->index][PVSNAPROOT].load();
    LFO *rootLfo = track->lfo[PVSNAPROOT].load();
    if (rootLfo && rootLfo->power()) {
        const auto a = _STATE->controls[track->index][PVSNAPROOT].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVSNAPROOT].lfo_max.load();
        rootStored = a + rootLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    const MYFLOAT root = std::pow(10., rootStored * .05);
    const MYFLOAT binHz = _STATE->sr / (MYFLOAT) fft_size;

    if (amt <= 0. || !(root > 0.) || !(binHz > 0.)) {
        fft->backwardPolar(spec, spec);
        tsl::fft::fftshift(spec, fft_size);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
        return;
    }

    const int32_t mode = (int32_t) _STATE->params[track->index][PVSNAPMODE].load();

    MYFLOAT *pk = track->fft_help1[channel];
    MYFLOAT *out = track->fft_out[channel];

    int32_t np = 0;
    for (int32_t j = 2; j < M2 - 2 && np < M2; ) {
        const MYFLOAT m = spec[j * 2];
        if (m > 0. &&
            m > spec[(j - 1) * 2] && m > spec[(j - 2) * 2] &&
            m > spec[(j + 1) * 2] && m > spec[(j + 2) * 2]) {
            pk[np++] = (MYFLOAT) j;
            j += 3;
        } else ++j;
    }

    if (np == 0) {
        fft->backwardPolar(spec, spec);
        tsl::fft::fftshift(spec, fft_size);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
        return;
    }

    MYFLOAT eIn = 0;
    for (int32_t i = 1; i < M2; ++i) eIn += spec[i * 2] * spec[i * 2];

    std::memset(out, 0, fft_size * sizeof(MYFLOAT));

    for (int32_t p = 0; p < np; ++p) {
        const int32_t b = (int32_t) pk[p];
        // Parabolic interpolation on the three magnitudes around the peak: the
        // grid target is decided in Hz, and a whole-bin frequency estimate is
        // wrong by up to half a bin, which at a low FFT SIZE is more than a
        // semitone and picks the wrong grid step.
        const MYFLOAT a1 = spec[(b - 1) * 2], a2 = spec[b * 2], a3 = spec[(b + 1) * 2];
        const MYFLOAT den = a1 - 2. * a2 + a3;
        MYFLOAT d = den != 0. ? .5 * (a1 - a3) / den : 0.;
        if (d > .5) d = .5; else if (d < -.5) d = -.5;
        if (!std::isfinite(d)) d = 0.;

        const MYFLOAT bf = (MYFLOAT) b + d;
        const MYFLOAT f = bf * binHz;
        if (!(f > 0.)) continue;

        const MYFLOAT oct = std::log2(f / root);
        MYFLOAT target;
        switch (mode) {
            case 0: {   // HARMONIC: integer multiples of the root
                MYFLOAT n = std::round(f / root);
                if (n < 1.) n = 1.;
                target = n * root;
                break;
            }
            case 1: target = root * std::exp2(std::round(oct)); break;                      // OCTAVES
            case 2: target = root * std::exp2(std::round(oct * (12. / 7.)) * (7. / 12.));   // FIFTHS
                break;
            case 3: target = root * std::exp2(std::round(oct * 6.) / 6.); break;            // WHOLE TONE
            default: target = root * std::exp2(std::round(oct * 12.) / 12.); break;         // CHROMATIC
        }

        const MYFLOAT ratio = target / f;
        if (!(ratio > 0.) || !std::isfinite(ratio)) continue;
        // Partial snapping interpolates in the log domain, so half way is half a
        // ratio and not half a hertz.
        const MYFLOAT r = amt >= 1. ? ratio : std::exp2(amt * std::log2(ratio));

        // Integer bin shift: a fractional one would need the mainlobe resampled,
        // and resampling it is what translation exists to avoid.
        const int32_t shift = (int32_t) std::lround(bf * (r - 1.));

        const int32_t b1 = p == 0 ? 1 : (int32_t) ((pk[p - 1] + pk[p]) * .5) + 1;
        const int32_t b2 = p == np - 1 ? M2 : (int32_t) ((pk[p + 1] + pk[p]) * .5) + 1;

        for (int32_t i = b1 < 1 ? 1 : b1; i < b2 && i < M2; ++i) {
            const int32_t dst = i + shift;
            if (dst < 1 || dst >= M2) continue;
            const MYFLOAT m = spec[i * 2];
            if (m > out[dst * 2]) {
                out[dst * 2] = m;
                out[dst * 2 + 1] = spec[i * 2 + 1];
            }
        }
    }

    MYFLOAT eOut = 0;
    for (int32_t i = 1; i < M2; ++i) eOut += out[i * 2] * out[i * 2];
    if (eIn > 1e-30 && eOut > 1e-30) {
        const MYFLOAT g = std::sqrt(eIn / eOut);
        for (int32_t i = 1; i < M2; ++i) out[i * 2] *= g;
    }
    out[0] = out[1] = 0;

    fft->backwardPolar(out, spec);
    tsl::fft::fftshift(spec, fft_size);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
}


// ---------------------------------------------------------------------------
// Transform of the Hann analysis window at a fractional bin offset, normalised
// to 1/2 at the centre. The grain is fftshifted before the transform, so the
// window transform is real and this one number is the whole atom: a sinusoid at
// fractional bin p with complex amplitude A occupies bins as A * W(i - p).
//
//   W(d) = 0.5*sinc(d) + 0.25*sinc(d-1) + 0.25*sinc(d+1)
//        = sin(pi d) / (2 pi d (1 - d^2))
//
// which is where the familiar 0.5 / 0.25 / 0.25 three-bin Hann spread comes
// from -- those are the removable singularities at d = 0 and |d| = 1. |W| is
// below 0.013 past |d| = 2, so +-3 bins is the whole mainlobe and then some.
// ---------------------------------------------------------------------------
static inline MYFLOAT res_hann_kernel(MYFLOAT d) {
    const MYFLOAT ad = std::abs(d);
    if (ad < 1e-6) return .5;
    if (std::abs(ad - 1.) < 1e-6) return .25;
    return std::sin(PI_P * d) / (2. * PI_P * d * (1. - d * d));
}

// Bins either side of a partial that the projection and the resynthesis touch.
#define PV_RES_HALFWIDTH 3

// ---------------------------------------------------------------------------
// PV: SPECTRAL RES -- a bank of decaying resonators tuned to a partial series.
//
// The grain is projected onto a set of partials built from ROOT, each partial
// gets its own complex accumulator that decays and is re-excited every grain,
// and the accumulators are written back into the spectrum. What comes out is
// pitched whatever went in: a hi-hat becomes a chord, breath becomes a string,
// a spoken word becomes the same word sung on one note. It is the one thing in
// the PV list that gives the granulator sustain -- everything else here maps a
// grain to a grain, and this rings on for as long as DECAY says after the
// source has stopped.
//
// The accumulator is the whole algorithm and it has exactly three moves per
// grain: rotate by one hop of the partial's own frequency, multiply by the
// decay, add this grain's excitation.
//
// **The rotation is what makes it a resonator rather than a spectral freeze.**
// Consecutive grains land one hop apart in the output, and a partial that is
// meant to sustain has to arrive at each of them with the phase it would have
// had if it had simply kept oscillating -- 2*pi*f*hop further on each time.
// Rotate correctly and the overlapped grains add coherently into one continuous
// partial; leave the phase alone and successive copies fight each other, the
// tail loses several dB to the cancellation and turns into a wash that is
// detuned by however far the bin centre sits from the partial. Note that the
// rotation uses the partial's frequency, not its bin's: that difference is the
// entire error.
//
// Both directions use the analysis window's transform (res_hann_kernel), so
// excitation is the projection of the grain onto the partial's atom and the
// synthesis is that atom scaled -- adjoint operations, and dividing by the
// atom's own energy makes the round trip exactly unity for an isolated partial.
// That is what lets this algorithm skip the grain normaliser the cross
// algorithms need: the level is right by construction, and it has to be, since
// a normaliser would drag every decay tail back up to input level and there
// would be no decay left to hear.
//
// Excitation is scaled by sqrt(1 - a^2) rather than the (1 - a) a one-pole
// would use. Grains arrive with unrelated phases here (RND READ alone sees to
// that), so their excitations add as energy, not as amplitude, and sqrt(1-a^2)
// is the scaling that leaves the steady-state energy where it started. (1-a)
// would be right only if every grain excited the bank in phase, and would leave
// long decays inaudibly quiet.
//
// DAMP shortens the decay of high partials against the root, which is the
// difference between a bank of identical filters and something that sounds like
// a struck object. INHARM applies the stiff-string stretch
// f_n = n*root*sqrt(1 + B n^2) -- the piano's own inharmonicity formula -- which
// walks the series from string through bell to gong.
//
// The hop is taken from DENSITY, so decays are in real seconds whatever FFT
// SIZE is. Grain scheduling jitters around that nominal hop (DEVIATION, and the
// block-boundary rounding), so the rotation is slightly off on any given grain
// and long tails shimmer instead of standing perfectly still. That is a fair
// description of a real resonator and not worth chasing.
// ---------------------------------------------------------------------------
void spectral_resonator(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 8) return;
    auto spec = track->grain_buffer[channel];

    // Centre the grain: only then is the window transform real, and only then
    // can a partial be written into the spectrum as one real kernel times one
    // complex amplitude. Rectangular rather than polar throughout -- every
    // operation here is a sum of complex numbers, and the polar transforms
    // would spend a sin/cos per bin converting into a form nothing wants.
    tsl::fft::fftshift(spec, fft_size);
    fft->forward(spec, spec);

    MYFLOAT *st = track->resState[channel];
    if (track->resFill[channel]) {
        std::memset(st, 0, 2 * PV_RES_MAX_PARTIALS * sizeof(MYFLOAT));
        track->resFill[channel] = false;
    }

    // The root is stored as 20*log10(f) and the LFO interpolates in that stored
    // domain, so a swept root moves linearly in pitch and the bank plays a
    // melody rather than crawling through the bottom octave and skipping the
    // top. Same reasoning (and same storage) as PVSNAPROOT.
    MYFLOAT rootStored = _STATE->params[track->index][PVRESROOT].load();
    LFO *rootLfo = track->lfo[PVRESROOT].load();
    if (rootLfo && rootLfo->power()) {
        const auto a = _STATE->controls[track->index][PVRESROOT].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVRESROOT].lfo_max.load();
        rootStored = a + rootLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    const MYFLOAT root = std::pow(10., rootStored * .05);

    MYFLOAT dec = _STATE->params[track->index][PVRESDECAY].load();
    LFO *decLfo = track->lfo[PVRESDECAY].load();
    if (decLfo && decLfo->power()) {
        const auto a = _STATE->controls[track->index][PVRESDECAY].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVRESDECAY].lfo_max.load();
        dec = a + decLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (dec < 0.) dec = 0.; else if (dec > 1.) dec = 1.;

    MYFLOAT mix = _STATE->params[track->index][PVRESMIX].load();
    LFO *mixLfo = track->lfo[PVRESMIX].load();
    if (mixLfo && mixLfo->power()) {
        const auto a = _STATE->controls[track->index][PVRESMIX].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVRESMIX].lfo_max.load();
        mix = a + mixLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (mix < 0.) mix = 0.; else if (mix > 1.) mix = 1.;

    MYFLOAT dampX = _STATE->params[track->index][PVRESDAMP].load();
    if (dampX < 0.) dampX = 0.; else if (dampX > 1.) dampX = 1.;
    MYFLOAT inhX = _STATE->params[track->index][PVRESINHARM].load();
    if (inhX < 0.) inhX = 0.; else if (inhX > 1.) inhX = 1.;
    const int32_t mode = (int32_t) _STATE->params[track->index][PVRESMODE].load();

    const MYFLOAT binHz = _STATE->sr / (MYFLOAT) fft_size;
    const MYFLOAT nyq = _STATE->sr * .5;
    if (!(root > 0.) || !(binHz > 0.)) {
        fft->backward(spec, spec);
        tsl::fft::fftshift(spec, fft_size);
        for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
        return;
    }

    // Real time between grains. Everything below is per hop, so DECAY reads in
    // seconds at any FFT SIZE. Clamped at one second: below one grain a second
    // there is nothing to sustain and the phase wrap loses precision.
    MYFLOAT density = _STATE->params[track->index][DENSITY].load();
    if (density < 1.) density = 1.;
    const MYFLOAT hopS = 1. / density;

    const MYFLOAT T60 = .02 * std::pow(1000., dec);     // 20 ms .. 20 s
    // a = 10^(-3 hop / T60) per hop, written as an exp so the per-partial
    // damping only costs the r^damp factor.
    const MYFLOAT kDec = -3. * hopS / T60 * 2.302585092994046;
    const MYFLOAT damp = dampX * 2.;                    // T60 scales as r^-damp
    const MYFLOAT B = .004 * inhX * inhX * inhX;        // stiff-string coefficient

    MYFLOAT *out = track->fft_out[channel];
    std::memset(out, 0, fft_size * sizeof(MYFLOAT));

    MYFLOAT wk[2 * PV_RES_HALFWIDTH + 2];

    // Set once, the first time a partial falls outside the band, and answers
    // "is there anything parked up there worth walking to". See the ageing note
    // in the loop: without parked energy the bank ends where the band does.
    bool tailChecked = false, tailParked = false;

    for (int32_t k = 0; k < PV_RES_MAX_PARTIALS; ++k) {
        // Ratio to the root before stiffness. HARMONIC and ODD are series a
        // physical object makes; the grid modes are not, and turn the bank into
        // a chord or a tuned reverb.
        MYFLOAT r;
        switch (mode) {
            case 1: r = (MYFLOAT) (2 * k + 1); break;                   // ODD
            case 2: r = std::exp2((MYFLOAT) k); break;                  // OCTAVES
            case 3: r = std::exp2((MYFLOAT) k * (7. / 12.)); break;     // FIFTHS
            case 4: r = std::exp2((MYFLOAT) k / 12.); break;            // CHROMATIC
            default: r = (MYFLOAT) (k + 1); break;                      // HARMONIC
        }
        const MYFLOAT f = root * r * std::sqrt(1. + B * r * r);
        const MYFLOAT p = f / binHz;

        // A partial that ROOT has pushed out of the band still has to age.
        // Leaving its accumulator alone parks the energy indefinitely: sweep the
        // root up, wait as long as you like, sweep it back, and the bank hands
        // the partials back at the amplitude they left with -- 87 dB above the
        // silence they returned into, in the case that found this. So anything
        // still holding energy is rotated and decayed exactly as if it were
        // being rendered, and only the rendering is skipped.
        //
        // The bank is walked no further than it has to be. Past the band edge
        // the array is almost always empty, and one scan settles it: nothing
        // parked, and the loop stops where the band does, which is what it cost
        // before any of this.
        const bool live = f > 0. && f < nyq && p >= 1. && p <= (MYFLOAT) (M2 - 1);
        if (!live) {
            if (!tailChecked) {
                tailChecked = true;
                for (int32_t j = k * 2; j < 2 * PV_RES_MAX_PARTIALS; ++j)
                    if (st[j] != 0.) { tailParked = true; break; }
            }
            if (!tailParked) break;
            if (st[k * 2] == 0. && st[k * 2 + 1] == 0.) continue;
        }

        const MYFLOAT rp = damp > 0. ? std::exp2(damp * std::log2(r)) : 1.;
        MYFLOAT a = std::exp(kDec * rp);
        if (!(a >= 0.)) a = 0.; else if (a > .9995) a = .9995;
        const MYFLOAT g = std::sqrt(1. - a * a);

        // One hop of this partial's own phase. Reduced to a fraction of a turn
        // before scaling: f*hop can run into the thousands and cos of that has
        // already lost most of the precision that matters here.
        MYFLOAT turns = f * hopS;
        turns -= std::floor(turns);
        const MYFLOAT ph = turns * TWOPI_P;
        const MYFLOAT c = std::cos(ph), s = std::sin(ph);

        const MYFLOAT sre = st[k * 2], sim = st[k * 2 + 1];
        MYFLOAT nre = (sre * c - sim * s) * a;
        MYFLOAT nim = (sre * s + sim * c) * a;

        int32_t i0 = 0, i1 = -1;
        if (live) {
            i0 = (int32_t) std::ceil(p - (MYFLOAT) PV_RES_HALFWIDTH);
            i1 = (int32_t) std::floor(p + (MYFLOAT) PV_RES_HALFWIDTH);
            if (i0 < 1) i0 = 1;
            if (i1 > M2 - 1) i1 = M2 - 1;

            // Excitation: the grain projected onto this partial's atom, divided
            // by the atom's own energy so that projecting and resynthesising an
            // isolated partial returns it unchanged. A partial outside the band
            // gets none -- there is nothing there to project onto -- so it only
            // ages.
            MYFLOAT er = 0, ei = 0, w2 = 0;
            for (int32_t i = i0, j = 0; i <= i1; ++i, ++j) {
                const MYFLOAT w = res_hann_kernel((MYFLOAT) i - p);
                wk[j] = w;
                er += w * spec[i * 2];
                ei += w * spec[i * 2 + 1];
                w2 += w * w;
            }
            if (w2 > 1e-9) {
                nre += g * er / w2;
                nim += g * ei / w2;
            }
        }

        // Flushes denormals as well as anything non-finite. A resonator decays
        // towards zero and never reaches it, and at a short DECAY the state is
        // in denormal territory a second or two after the source stops -- which
        // is precisely when nothing is masking the stall. It is also what
        // finally empties a parked partial, so the walk above can stop again.
        UDD(nre)
        UDD(nim)
        st[k * 2] = nre;
        st[k * 2 + 1] = nim;

        for (int32_t i = i0, j = 0; i <= i1; ++i, ++j) {
            out[i * 2] += wk[j] * nre;
            out[i * 2 + 1] += wk[j] * nim;
        }
    }

    // DC and Nyquist have no resonator, so they follow the dry side alone --
    // giving them to the wet side would just leave a hole at MIX 1.
    if (mix >= 1.) {
        out[0] = out[1] = 0.;
    } else {
        const MYFLOAT d = 1. - mix;
        out[0] = spec[0] * d;
        out[1] = spec[1] * d;
        for (int32_t i = 2; i < (int32_t) fft_size; ++i)
            out[i] = out[i] * mix + spec[i] * d;
    }

    fft->backward(out, spec);
    tsl::fft::fftshift(spec, fft_size);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(spec[n])
}


// ---------------------------------------------------------------------------
// PV: SPECTRAL FREEZE -- hold the spectrum, keep the phases moving.
//
// Per bin, a magnitude and a per-grain phase advance are slewed towards the
// input at rate (1 - FREEZE), and a synthesis phase accumulator advances by
// the held rate every grain. At FREEZE 0 the held values track exactly and the
// output is the input (at speed 1 the phase identity is exact); at 1 nothing
// tracks and the captured spectrum sustains for as long as the knob stays up.
// In between, the spectrum is a lagging average of the input -- the slew IS
// the interpolation to a freeze, there is no separate crossfade.
//
// The phase advance is MEASURED (two-frame heterodyne, the dephase estimator),
// not the bin centre: resynthesising a held bin at its centre frequency snaps
// every partial to a multiple of sr/FFTSIZE, which detunes 440 Hz to 445 and
// combs at 23 Hz -- the SpectralFilter lesson. The measured advance is
// converted to the OUTPUT hop (divided by the playback speed): grains land at
// sr/DENSITY apart whatever the read position does, and it is in output time
// that the frozen partial has to stay periodic. At speed 0 consecutive input
// frames are identical and the estimate is undefined, so the held rate simply
// stops updating -- frozen material keeps ringing at the last measured pitch.
//
// PROB is a stable per-bin lottery (hash of the bin number): raising it freezes
// more bins, lowering it releases the same ones, so an LFO on it breathes
// between the live and the frozen spectrum. A mainlobe's bins can split across
// the lottery -- that is the partial-freeze sound, not a defect. MIX is a
// complex blend with the dry grain: both sides are independently
// phase-coherent, so the sum flanges rather than flutters (the spec_interpol
// argument). DC and Nyquist follow the dry side alone.
//
// State is per bin (freezeSize/freezeFill re-seed on an FFT SIZE change or an
// algorithm switch, like hpssHist). The seed grain passes through untouched.
// ---------------------------------------------------------------------------
static inline MYFLOAT freeze_hash01(uint32_t i) {
    uint32_t h = i * 2654435761u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;
    return (MYFLOAT) (h & 0xFFFFFFu) * (MYFLOAT) (1. / 16777216.);
}

void spectral_freeze(TRACK *track, uint8_t channel, uint32_t fft_size, MYFLOAT playbackspeed) {
    auto _appState = track->_appState;
    CHECKFFT(fft_size)
    const int32_t M2 = (int32_t) (fft_size >> 1u);
    if (M2 < 2) return;
    auto fft_in = track->grain_buffer[channel];
    tsl::fft::fftshift(fft_in, fft_size);
    fft->forwardPolar(fft_in, fft_in);

    MYFLOAT fr = _STATE->params[track->index][PVFREEZEAMT].load();
    LFO *frLfo = track->lfo[PVFREEZEAMT].load();
    if (frLfo && frLfo->power()) {
        const auto a = _STATE->controls[track->index][PVFREEZEAMT].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVFREEZEAMT].lfo_max.load();
        fr = a + frLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (fr < 0.) fr = 0.; else if (fr > 1.) fr = 1.;

    MYFLOAT prob = _STATE->params[track->index][PVFREEZEPROB].load();
    LFO *prLfo = track->lfo[PVFREEZEPROB].load();
    if (prLfo && prLfo->power()) {
        const auto a = _STATE->controls[track->index][PVFREEZEPROB].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVFREEZEPROB].lfo_max.load();
        prob = a + prLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (prob < 0.) prob = 0.; else if (prob > 1.) prob = 1.;

    MYFLOAT mix = _STATE->params[track->index][PVFREEZEMIX].load();
    LFO *mxLfo = track->lfo[PVFREEZEMIX].load();
    if (mxLfo && mxLfo->power()) {
        const auto a = _STATE->controls[track->index][PVFREEZEMIX].lfo_min.load();
        const auto b = _STATE->controls[track->index][PVFREEZEMIX].lfo_max.load();
        mix = a + mxLfo->buf[(int) track->step_point_grain[channel].load()] * (b - a);
    }
    if (mix < 0.) mix = 0.; else if (mix > 1.) mix = 1.;

    const MYFLOAT dens = _STATE->params[track->index][DENSITY].load();
    const MYFLOAT hopOut = dens > 0. ? _STATE->sr / dens : 0.;
    const MYFLOAT stepIn = hopOut * playbackspeed;
    const MYFLOAT omegapre = TWOPI_P * stepIn / (MYFLOAT) fft_size;
    const MYFLOAT omegaOutPre = TWOPI_P * hopOut / (MYFLOAT) fft_size;
    const MYFLOAT ratio = playbackspeed != 0. ? (MYFLOAT) 1. / playbackspeed : (MYFLOAT) 0.;

    MYFLOAT *hm = track->freezeMag[channel];
    MYFLOAT *hf = track->freezeFreq[channel];
    MYFLOAT *ps = track->freezePsi[channel];
    MYFLOAT *pp = track->freezePrevPhi[channel];

    const bool seed = track->freezeFill[channel] || track->freezeSize[channel] != M2;
    const MYFLOAT trackAmt = 1. - fr;
    const MYFLOAT dmix = 1. - mix;
    MYFLOAT *out = track->fft_help1[channel];
    out[0] = fft_in[0];
    out[1] = fft_in[1];
    for (int32_t i = 1; i < M2; ++i) {
        MYFLOAT mag = fft_in[i * 2];
        MYFLOAT phi = fft_in[i * 2 + 1];
        if (!(mag >= 0.) || !std::isfinite(mag)) mag = 0.;
        if (!std::isfinite(phi)) phi = 0.;
        if (seed) {
            hm[i] = mag;
            hf[i] = omegaOutPre * (MYFLOAT) i;
            ps[i] = phi;
            pp[i] = phi;
        } else {
            if (playbackspeed != 0.) {
                const MYFLOAT omega = omegapre * (MYFLOAT) i;
                const MYFLOAT adv = (omega + princarg(phi - pp[i] - omega)) * ratio;
                hf[i] += trackAmt * (adv - hf[i]);
            }
            pp[i] = phi;
            hm[i] += trackAmt * (mag - hm[i]);
            UDF(hm[i])
            ps[i] = princarg(ps[i] + hf[i]);
        }
        const bool frozen = freeze_hash01((uint32_t) i) < prob;
        const MYFLOAT wm = frozen ? hm[i] : mag;
        const MYFLOAT wp = frozen ? ps[i] : phi;
        // Assembled in rectangular form: the MIX blend is a complex sum, and
        // backwardPolar would spend the same cos/sin per bin anyway.
        if (dmix > 0.) {
            out[i * 2] = mix * wm * std::cos(wp) + dmix * mag * std::cos(phi);
            out[i * 2 + 1] = mix * wm * std::sin(wp) + dmix * mag * std::sin(phi);
        } else {
            out[i * 2] = wm * std::cos(wp);
            out[i * 2 + 1] = wm * std::sin(wp);
        }
    }
    if (seed) {
        track->freezeSize[channel] = M2;
        track->freezeFill[channel] = false;
    }

    fft->backward(out, fft_in);
    tsl::fft::fftshift(fft_in, fft_size);
    for (int32_t n = 0; n < (int32_t) fft_size; ++n) UDD(fft_in[n])
}


template<typename T>
static T x_at_freq(const T f, const T width = 1.) {
    return width * log((f + 20.) / 20.) / log(1001.);
}


// Ceiling for the per-bin resonator's centre frequency, in cycles per frame. Half a
// cycle is the frame Nyquist, where the pole lands on the negative real axis: the top
// of the cutoff knob used to resonate at the frame rate -- an audible buzz at the
// point where it should be getting out of the way -- rather than open up.
#define SPECFILT_FMAX 0.40

// How many times faster than the magnitude path the frequency path is allowed to
// run while MAG LP is engaged. MAG LP holds a bin's level long after the content
// that earned it is gone; if the frequency track stays wide open at the same time,
// whatever quiet residue now occupies the bin steers that still-loud bin in raw
// frame-rate steps -- up to +-46.9 Hz every 10.7 ms -- which is the clicking at
// PHASE 0 with MAG up. Capping the frequency cutoff at a multiple of the magnitude
// cutoff means the frequency memory is never much shorter than the level memory it
// has to cover for; it is the PHASE-up remedy applied automatically, and PHASE
// still adds smoothing beyond the cap as before. At MAG 0 the cap sits above
// SPECFILT_FMAX, so the default sound is bit-identical.
#define SPECFILT_FCOUPLE 4.0

// Crossfade, in samples, from the undelayed dry to the delayed one once the latency
// delay has filled. See compute(). 512 = 10.7 ms at 48 kHz, long enough that the
// one-buffer time jump does not read as an edge and short enough to stay inside the
// wet/dry fade-in.
#define SPECFILT_DRY_SPLICE 512

// Pole sharpness. RES fades out over the top half of the cutoff range so the end of
// the knob is a genuine bypass whatever RES says; before this, RES 1 still held the
// pole radius at 0.59 with the cutoff wide open.
static inline MYFLOAT specfilt_kres(MYFLOAT f, MYFLOAT res) {
    MYFLOAT t = (SPECFILT_FMAX - f) / (SPECFILT_FMAX * .5);
    t = t < 0. ? 0. : (t > 1. ? 1. : t);
    return .1 + 2.9 * res * t;
}

// STEP-response peak of c/(1 - a z^-1 + b z^-2) with c = 1-a+b, i.e. relative to a DC
// gain of 1. This is what the magnitude filter is normalised by, so that a bin can
// never be resynthesised above the level a sustained input would hold it at.
//
// It used to be normalised by the FREQUENCY-response peak instead. Same intent, wrong
// failure mode: the input here is a magnitude envelope, not a sinusoid sitting on the
// resonance, so what can actually overshoot is a step. The frequency peak reaches 3.08
// at RES 1 where the step peak is only 1.59 -- a 5.7 dB over-correction, and since the
// division also sets the DC gain, RES was acting as a 6 dB fader. Measured on
// percussive material, output level across the RES sweep: 6.6 dB of drift -> 2.5 dB at
// MAG 0.3, 6.3 -> 3.8 at MAG 0.6; a full-scale sine peaks at -2.4 dBFS instead of -5.0
// and still never exceeds 0.
//
// Closed form: for the pole pair r e^(+-j th), the equivalent damping ratio is
// z = 1/sqrt(1 + 4Q^2) with Q = th / -ln(b), and z/sqrt(1-z^2) collapses to 1/(2Q), so
// the classic exp(-pi z/sqrt(1-z^2)) overshoot is just exp(-pi/(2Q)). Taken from the
// coefficients rather than from Q directly so it stays right while they slew.
static inline MYFLOAT specfilt_steppeak(MYFLOAT a, MYFLOAT b) {
    if (b <= 1.0E-12)
        return 1.;                              // no second pole, nothing to overshoot
    const MYFLOAT r = sqrt(b);
    const MYFLOAT ct = a / (2. * r);            // cos of the pole angle
    if (ct >= 1. || ct <= -1.)
        return 1.;                              // real poles, monotonic step response
    const MYFLOAT q = acos(ct) / -log(b);
    if (q <= 1.0E-6)
        return 1.;
    return 1. + exp(-PI_P / (2. * q));
}


SpectralFilter::SpectralFilter(TRACK *track, int32_t
channel) : Effect(track, channel,
                  SPACE_SPECTRAL_FILTER,
                  MONOEFFECT),
           fft(_STATE->sr,
               SPECTRAL_FILTER_OVERLAP),
           CircularBuffer(
                   SPECTRAL_FILTER_FFTSIZE,
                   SPECTRAL_FILTER_OVERLAP) {
    lpcuta = &_STATE->params[track->index][SPECFILTMAGLPCUT2];
    lpcutf = &_STATE->params[track->index][SPECFILTFREQLPCUT2];
    res = &_STATE->params[track->index][SPECFILTRES];
    _dry = &_STATE->params[track->index][SPECFILTDRY];
    _wet = &_STATE->params[track->index][SPECFILTWET];
    _lfo_mag = &track->lfo[SPECFILTMAGLPCUT2];
    _lfo_phase = &track->lfo[SPECFILTFREQLPCUT2];
    _bypass = &track->bypass[SPACE_SPECTRAL_FILTER];
    internalsr = _STATE->sr / (SPECTRAL_FILTER_FFTSIZE / SPECTRAL_FILTER_OVERLAP);
    mpidsr = -PI_P / internalsr;
    twopidsr = TWOPI_P / internalsr;
    pidsr = PI_P / internalsr;
    del.setsize(getDelay());
}


void SpectralFilter::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT wet, dry;
    if (_bypass->load() || destroyRequested) {
        dry = 1.;
        wet = 0;
    } else {
        dry = LOG2NORMALF(*_dry);
        wet = LOG2NORMALF(*_wet);
    }

    const MYFLOAT magConst = Effect::limit((MYFLOAT) lpcuta->load(), (MYFLOAT) 0., (MYFLOAT) 1.);
    const MYFLOAT phaseConst = Effect::limit((MYFLOAT) lpcutf->load(), (MYFLOAT) 0., (MYFLOAT) 1.);

    // MAG LP / PHASE LP modulation, wired the way RESON and VOX2 do it: follower
    // first, then the LFO either scales that or spans its own min..max range.
    auto &folM = _STATE->followerMap[_track->index].at(SPECFILTMAGLPCUT2);
    auto &folP = _STATE->followerMap[_track->index].at(SPECFILTFREQLPCUT2);
    const auto srcM = folM.source.load(), srcP = folP.source.load();
    MYFLOAT *envMbuf = srcM == _track->index ? in : _DATA->tracks[srcM]->envf_buffer[_chan];
    MYFLOAT *envPbuf = srcP == _track->index ? in : _DATA->tracks[srcP]->envf_buffer[_chan];
    const bool envM_on = folM.prepare(_chan);
    const bool envP_on = folP.prepare(_chan);

    LFO *lfoM = *_lfo_mag;
    LFO *lfoP = *_lfo_phase;
    const bool lfoM_on = lfoM && lfoM->power();
    const bool lfoP_on = lfoP && lfoP->power();
    MYFLOAT magA = 0., magRange = 0., phA = 0., phRange = 0.;
    if (lfoM_on) {
        magA = _STATE->controls[_track->index][SPECFILTMAGLPCUT2].lfo_min.load();
        magRange = _STATE->controls[_track->index][SPECFILTMAGLPCUT2].lfo_max.load() - magA;
    }
    if (lfoP_on) {
        phA = _STATE->controls[_track->index][SPECFILTFREQLPCUT2].lfo_min.load();
        phRange = _STATE->controls[_track->index][SPECFILTFREQLPCUT2].lfo_max.load() - phA;
    }

    magNow = magConst;
    phaseNow = phaseConst;

    //_compute(in, tmpbuf.data(), size);
    for (int32_t i = 0; i < size; i++) {
        if (envM_on || lfoM_on) {
            MYFLOAT m = magConst;
            if (envM_on)
                m = _chan == 0 ? folM.detectL(envMbuf[i]) : folM.detectR(envMbuf[i]);
            if (lfoM_on)
                m = envM_on ? m * (MYFLOAT) lfoM->buf[i] : magA + (MYFLOAT) lfoM->buf[i] * magRange;
            magNow = Effect::limit(m, (MYFLOAT) 0., (MYFLOAT) 1.);
        }
        if (envP_on || lfoP_on) {
            MYFLOAT p = phaseConst;
            if (envP_on)
                p = _chan == 0 ? folP.detectL(envPbuf[i]) : folP.detectR(envPbuf[i]);
            if (lfoP_on)
                p = envP_on ? p * (MYFLOAT) lfoP->buf[i] : phA + (MYFLOAT) lfoP->buf[i] * phRange;
            phaseNow = Effect::limit(p, (MYFLOAT) 0., (MYFLOAT) 1.);
        }
        // The dry path is delayed to line up with the wet one, and that delay line is
        // built empty. So for its first getDelay() samples it outputs SILENCE, not the
        // dry signal -- and silence is what the wet/dry smoother spends its 200 ms
        // fading out of. Switching the effect on muted the track for 42.7 ms and then
        // hard-edged back in from digital zero, which is a click on anything dense.
        // (DRY defaults to -60 dB, but _smooth2 starts at 1: the fade is FROM full dry,
        // so the gap is there whatever DRY is set to.)
        //
        // Pass the dry through undelayed until the line has filled, then splice across.
        // The two are the same signal one buffer apart, so they are uncorrelated at the
        // join and want an equal-power crossfade rather than a linear one.
        const MYFLOAT xn = in[i];
        const MYFLOAT dly = del.process(xn);
        MYFLOAT drysig;
        if (dryFill < (int64_t) getDelay()) {
            drysig = xn;
            dryFill++;
        } else if (dryFill < (int64_t) getDelay() + SPECFILT_DRY_SPLICE) {
            const MYFLOAT a = (MYFLOAT)(dryFill - getDelay()) * (MYFLOAT)(1. / SPECFILT_DRY_SPLICE);
            drysig = cos(a * PI_P * .5) * xn + sin(a * PI_P * .5) * dly;
            dryFill++;
        } else {
            drysig = dly;
        }
        // No trim on the wet path: compute_window now normalises the overlap-add to
        // exactly unity, so WET 0 dB is unity. The .5 that used to sit here was
        // papering over a windowing shortfall and left the wet path 11 dB down.
        in[i] = _smooth2 * drysig + _smooth1 * _tick(xn);
        smwetdry(wet, dry);
    }
};


void SpectralFilter::onBufferReady(MYFLOAT *buf, int32_t size) {
    fft.forwardFrequency(buf, buf);
    // The modulated values compute() left for this hop, not the raw knobs.
    const MYFLOAT framp = magNow;
    const MYFLOAT frfr = phaseNow;
    const MYFLOAT rr = res->load();

    if (rr != oldres || framp != oldcuta || frfr != oldcutf) {
        const MYFLOAT sr = internalsr;
        oldres = rr;
        oldcutf = frfr;
        oldcuta = framp;
        // SMOOTH2POLE walks the same -60..0 dB span these two used to be stated in,
        // reversed, so the knobs now rise the way their names read: 0 is wide open
        // (the old 0 dB default) and 1 is the most filtering.
        const MYFLOAT fmin = .1 / sr;
        MYFLOAT ffa = fmin + (SPECFILT_FMAX - fmin) * SMOOTH2POLE(oldcuta);
        MYFLOAT fff = fmin + (SPECFILT_FMAX - fmin) * SMOOTH2POLE(oldcutf);
        // See SPECFILT_FCOUPLE: with MAG LP engaged, the frequency track may not be
        // left running at frame rate underneath the level smear.
        const MYFLOAT fcap = SPECFILT_FCOUPLE * ffa;
        if (fff > fcap) fff = fcap;
        MYFLOAT temp = -PI_P * ffa / specfilt_kres(ffa, oldres);
        aaTarget = 2.0 * cos(ffa * TWOPI_P) * exp(temp);
        baTarget = exp(temp + temp);

        temp = -PI_P * fff / specfilt_kres(fff, oldres);
        afTarget = 2.0 * cos(fff * TWOPI_P) * exp(temp);
        bfTarget = exp(temp + temp);
    }

    // Slew towards the targets rather than stepping. Interpolating a and b is safe:
    // the stability region of this form is the triangle b<1, b>a-1, b>-a-1, which is
    // convex, so a point between two stable pairs is itself stable. c is derived from
    // the interpolated pair so the DC gain stays exactly 1 throughout.
    if (coeffsInit) {
        constexpr MYFLOAT k = .2;   // ~5 frames, 50 ms at the 93.75 Hz frame rate
        aa += (aaTarget - aa) * k;
        ba += (baTarget - ba) * k;
        af += (afTarget - af) * k;
        bf += (bfTarget - bf) * k;
    } else {
        aa = aaTarget;
        ba = baTarget;
        af = afTarget;
        bf = bfTarget;
        coeffsInit = true;
    }
    // Magnitude path is normalised so its STEP response peaks at 1, so RES shapes
    // without adding level and a bin cannot come out above the level a sustained input
    // holds it at. At RES 0 there is no overshoot and this is unity DC, exactly as
    // before. The frequency path keeps unity DC: its overshoot is a pitch excursion,
    // not a level, and clamping it would only flatten what PHASE LP is for.
    ca = (1.0 - aa + ba) / specfilt_steppeak(aa, ba);
    cf = 1.0 - af + bf;



    /*
     * double temp = (double) (pidsr * i / qq);
                a[i] = 2.0 * cos((double) (i * tpidsr)) * exp(temp);
                b[i] = exp(temp + temp);
                c[i] = 1.0 - a[i] + b[i];
        const double sr = xx->internalsr;

        double fa = .1 + (sr / 2 * .1) * pow(10, xx->lpcuta->load() * .05);
        double kres = .1 + 2.9 * xx->res->load();

        double temp = xx->mpidsr * fa / kres;
        double aa = 2.0 * cos(fa * xx->twopidsr) * exp(temp);
        double ba = exp(temp + temp);
        double ca = 1.0 - aa + ba;

        double ff = .1 + (sr / 2 * .1) * pow(10, xx->lpcutf->load() * .05);
        temp = xx->mpidsr * ff / kres;
        double af = 2.0 * cos(ff * xx->twopidsr) * exp(temp);
        double bf = exp(temp + temp);
        double cf = 1.0 - af + bf;
    */
    // The first OVERLAP-1 hops hand up an analysis window that is still part
    // zero-fill from before the effect existed. Its spectrum is the spectrum of an
    // edge, not of the signal, and MAG LP then holds that broadband frame for its
    // whole time constant -- a burst of noise every time the effect is switched on.
    // Stay silent until the window has filled, and seed the filters off the first
    // complete frame instead.
    //
    // Zeroing only the MAGNITUDES matters: the frequencies still have to go through
    // backwardFrequency, because that is what advances the per-bin synthesis phase,
    // and each bin advances by a different amount. Skipping it leaves the bins of a
    // mainlobe permanently out of step and the tone cancels itself -- measured 25 dB
    // down for good, on a signal the effect was supposed to be passing.
    if (framesSeen < (int32_t) SPECTRAL_FILTER_OVERLAP - 1) {
        framesSeen++;
        for (int32_t i = 0; i < SPECTRAL_FILTER_FFTSIZE / 2; i++)
            buf[i * 2] = 0.;
        fft.backwardFrequency(buf, buf);
        return;
    }

    MYFLOAT yn;

    // Confidence gate on the frequency track. MAG LP smears a bin's level forward in
    // time; the frequency track has no such memory, so in the smear the resynthesis
    // was driving a bin that is still loud with whatever the now-empty input says.
    // For a truly empty bin forwardFrequency reports the bin CENTRE, so every bin
    // fell back onto the analysis grid at once and the tail came out as a harmonic
    // comb at sr/FFTSIZE -- 23.4 Hz here. Measured: 95% of the tail energy landed on
    // that comb and the output was 114% amplitude-modulated at 23.4 Hz and its
    // harmonics. That buzz is the reported noise and clicking, and it is why winding
    // PHASE LP up hides it: the frequency filter's own memory stands in for the hold.
    //
    // So: track the input frequency while the bin's incoming level still supports the
    // level being output, and hold the last supported frequency once it does not.
    // SPECFILT_FTRACK is the point where support runs out, as a fraction of the held
    // magnitude. It is deliberately deep (~34 dB down) so that anything still audible
    // in the bin tracks normally: at 1.0 the gate misfires on the frame-to-frame
    // ripple of a steady tone and costs 70 dB of SNR, at 0.02 a steady tone and white
    // noise both come through unchanged and the 23.4 Hz modulation drops 3.4-8.5x.
    //
    // This ramp reaches zero only at digital silence, so between hits real material
    // (residue 30..60 dB down) still walks fhold slowly across its junk frequencies.
    // That walk is deliberate: an absolute hold below -46 dB (a dead zone in this
    // ramp) was tried and REJECTED by ear -- hard-frozen bins sit pinned a few Hz
    // apart for the whole smear tail and beat PERIODICALLY, a steady wobble at
    // MAG+PHASE high, and losing the walk's slow diffusion audibly shrank what the
    // two knobs do. The walk is only ever a click problem when it reaches synthesis
    // unsmoothed, and the SPECFILT_FCOUPLE cap on the frequency cutoff already
    // prevents exactly that.
    constexpr MYFLOAT SPECFILT_FTRACK = .02;

    // UDD on both states: at a low cutoff the pole radius is ~0.998, so once the
    // input stops these ring down into denormals and stay there.
    for (int32_t i = 0; i < SPECTRAL_FILTER_FFTSIZE / 2; i++) {
        const MYFLOAT magIn = buf[i * 2], freqIn = buf[i * 2 + 1];
        if (!filterInit) {
            // Seed from the first frame instead of starting at zero, where every
            // bin's frequency track would glide up from 0 Hz over the filter's time
            // constant -- seconds of it at a low cutoff. Seeding both states with x
            // makes this frame pass through exactly, since the DC gain is 1.
            yl1a[i] = yl2a[i] = magIn;
            yl1f[i] = yl2f[i] = freqIn;
            fhold[i] = freqIn;
        }
        yn = aa * yl1a[i] - ba * yl2a[i] + ca * magIn;
        UDD(yn)
        yl2a[i] = yl1a[i];
        yl1a[i] = yn;
        // A resonant lowpass fed a non-negative signal rings BELOW zero on the way
        // down -- any Q above 0.5 does, on a step towards silence. backwardFrequency
        // resynthesises a bin as mag*cos(phase), so a negative magnitude is a pi phase
        // flip on that bin for one frame, and frame-to-frame flips are a buzz at the
        // frame rate. At RES 1 that was 11% of all bin-frames. A magnitude cannot be
        // negative: clamp it. Measured on a gated tone at MAG 0.6, the tail in the
        // silent gap goes -22.5 dB -> -50.2 dB with no change to a steady tone.
        //
        // The clamp is on the OUTPUT only -- the filter state above keeps the signed
        // value. Feeding the clamped value back rectifies the resonance itself, which
        // measured worse (grain click metric 6.1% -> 10.1%, another 1.2 dB of level).
        if (yn < 0.) yn = 0.;
        buf[i * 2] = yn;

        // yn is the magnitude this bin is about to be resynthesised at. With MAG LP
        // wide open it equals magIn, w is 1, and fhold follows the input exactly --
        // the whole gate collapses to a pass-through, so the default sound is
        // untouched.
        MYFLOAT w = 1.;
        if (yn > (MYFLOAT) 1.0E-12) {
            w = magIn / (yn * SPECFILT_FTRACK);
            if (w > 1.) w = 1.;
        }
        fhold[i] += (freqIn - fhold[i]) * w;

        yn = af * yl1f[i] - bf * yl2f[i] + cf * fhold[i];
        UDD(yn)
        buf[i * 2 + 1] = yn;
        yl2f[i] = yl1f[i];
        yl1f[i] = yn;
    }
    filterInit = true;

    fft.backwardFrequency(buf, buf);
}


SpectralDelay2::SpectralDelay2(TRACK *track_, int32_t
channel_) : Effect(track_, channel_,
                   SPACE_SPECDEL2,
                   MONOEFFECT),
            fft(SPECDELFFTSIZE),
            CircularBuffer(SPECDELFFTSIZE,
                           4.0) {

    int32_t index = _track->index;
    numffts = (int) (_STATE->sr * 10.f / SPECDELOVERLAP);
    numfftsm1 = numffts - 1;
    int32_t fftbufsize = (numffts + 1) * SPECDELFFTSIZE * 2;
    delaybuf = new MYFLOAT[fftbufsize]();
    MYFLOAT *tmp = delaybuf;
    for (int32_t numfft = 0; numfft < numffts; numfft++) {
        fftbuf.push_back(tmp);
        tmp += SPECDELFFTSIZE * 2;
    }
    PVAmps::compute_hanning(win, SPECDELFFTSIZE);
    writepos = 0;
    update = &_STATE->params[_track->index][SPECDEL2RECOMPUTE1 + channel_];
    _mix = &_STATE->params[_track->index][SPECDEL2MIX];
    _gain = &_STATE->params[_track->index][SPECDEL2GAIN];
    _smooth2 = dbToLinear60(*_gain);
    fb = &_STATE->params[_track->index][SPECDEL2FB];
    del = &_STATE->params[_track->index][SPECDEL2DEL];
    rnd = &_STATE->params[_track->index][SPECDEL2RND];

    _bypass = &_track->bypass[SPACE_SPECDEL2];
    std::memset(delays, 0, sizeof(int) * SPECDELFFTSIZE / 2);

    *update = 1.0;
    delay.setsize(getDelay());

    const float binsize = _STATE->sr / (float) SPECDELFFTSIZE;

    for (int32_t i = 0; i < SPECDELFFTSIZE / 2; i++) {
        xatfreq[i] =
                (uint32_t) (x_at_freq((MYFLOAT) i * binsize, (MYFLOAT) TBLSIZE3)) & TBLMASK3;
    }
    /*
    if(channel_ == 0){
        int32_t offx = SPECDEL2X0;;
        int32_t offy = SPECDEL2Y0;
        int32_t nsegs = _STATE->params[track->index][SPECDEL2NSEGS].reference[index]->load();
        int32_t curve = _STATE->params[track->index][SPECDEL2CURVE].reference[index]->load();


        std::atomic<float> *xx[nsegs + 1];
        std::atomic<float> *yy[nsegs + 1];

        for (int32_t i = 0; i < nsegs + 1; i++) {
            xx[i] = _STATE->params[track->index][offx + i].reference[index];
            yy[i] = _STATE->params[track->index][offy + i].reference[index];

        }
        _DATA->synth->lfoeditfunc[curve](delaydraw, WINDOW_SIZE, xx[0], yy[0], nsegs, true);
    }
     */
}

void SpectralDelay2::onBufferReady(MYFLOAT *in, int32_t size) {

    for (int32_t i = 0; i < size; i++) {
        in[i] *= win[i];
    }

    const MYFLOAT binsize = _STATE->sr / (SPECDELFFTSIZE);

    if (*update == 1.0) {
        *update = 0;
        GrainFilter::renderenv(_track, SPECDEL2X0, _env);
        for (int32_t i = 0; i < SPECDELFFTSIZE / 2; i++) {
            //int32_t off = (int) (LIN2SCALED4(i * inc) * (WINDOW_SIZE - 1));
            // LOGE("%d %d", i, off);
            int32_t off = xatfreq[i];
            delays[i] = (int) (_env[off] * numfftsm1);
        }
    }


    MYFLOAT fbb = *fb;
    fft.forward(in, fftbuf[writepos]);




    //LOGE("%f",  sqrtf(SQR(*(fftbuf[writepos] + SPECDELFFTSIZE / 2)) + SQR(*(fftbuf[writepos] + SPECDELFFTSIZE / 2 +1))));
    auto maxdel = *del * .1;
    auto intmp = in;


    const bool fol = *rnd == 1.0;
    auto writebuf = fftbuf[writepos];
    for (int32_t i = 0; i < SPECDELFFTSIZE / 2; i++) {
        int32_t readpos = writepos - (int) (delays[i] * maxdel * (fol ? TOP(
                sqrt(SQR(*(fftbuf[writepos] + i * 2)) + SQR(*(fftbuf[writepos] + i * 2 + 1))),
                1.) : 1.));
        if (readpos < 0)
            readpos += numffts;
        *(writebuf++) += (*(intmp++) = *(fftbuf[readpos] + i * 2)) * fbb;
        *(writebuf++) += (*(intmp++) = *(fftbuf[readpos] + i * 2 + 1)) * fbb;
    }

    in[1] = 0.;
    fft.backward(in, in);
    writepos++;
    if (writepos > numfftsm1)
        writepos = 0;

    for (int32_t i = 0; i < size; i++) {
        in[i] *= win[i];
    }
}

void SpectralDelay2::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT gain = dbToLinear60(*_gain), mix;
    if (*_bypass || destroyRequested) {
        mix = 0.;
    } else {
        mix = *_mix;
    }
    for (int32_t i = 0; i < size; i++) {
        in[i] = delay.process(in[i]) * (1. - _smooth1) + _tick(in[i]) * _smooth1 *
                                                         _smooth2;
        smmixgain(mix, gain);
    }
}

