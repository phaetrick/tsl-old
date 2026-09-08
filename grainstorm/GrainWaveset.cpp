//
// WAVESET - see GrainWaveset.h.
//

#include <cmath>
#include <algorithm>
#include "GrainWaveset.h"
#include "track.h"
#include "grainstorm.h"
#include "random.h"
#include "lfo.h"

GrainWaveset::GrainWaveset(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_GRAINWAVESET, GRAINEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_GRAINWAVESET];
    _mode = &_STATE->params[t->index][GRAINWSMODE];
    _amt = &_STATE->params[t->index][GRAINWSAMT];
    _group = &_STATE->params[t->index][GRAINWSGROUP];
    _mix = &_STATE->params[t->index][GRAINWSMIX];
    _gain = &_STATE->params[t->index][GRAINWSGAIN];
    _smooth2 = dbToLinear60(*_gain);
    _lfo_amt = &t->lfo[GRAINWSAMT];
    // Allocated here, on the caller of the power button - never on the audio
    // thread. A waveset is at least one sample, so the boundary list can be as
    // long as the grain itself.
    _scratch.assign(_DATA->maxgrainsize, 0.);
    _starts.assign(_DATA->maxgrainsize + 2, 0);
}

void GrainWaveset::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(*_gain);
    const MYFLOAT mixTarget = (*_bypass || destroyRequested) ? 0. : (MYFLOAT) *_mix;

    if (size < 4 || size > (int32_t) _scratch.size()) {
        // Still has to run the smoother, otherwise a destroyed effect never
        // reaches readyToDestroy and the reaper never frees it.
        for (int32_t i = 0; i < size; i++)
            smmixgain(mixTarget, gain);
        return;
    }

    const int mode = Effect::limit((int) _mode->load(), 0, (int) MODE_SINE);

    // Single param, so the LFO replaces it outright (per grain). Note REPEAT
    // and OMIT quantise AMOUNT into a count, so a swept LFO steps the stutter
    // rate rather than gliding it - that is the mode, not the modulation.
    MYFLOAT amt = (MYFLOAT) *_amt;
    const LFO *lfoA = _lfo_amt->load();
    if (lfoA && lfoA->power())
        amt = lfoA->min(GRAINWSAMT) +
              lfoA->buf[grainLfoIndex()] * (lfoA->max(GRAINWSAMT) - lfoA->min(GRAINWSAMT));
    amt = Effect::limit(amt, 0., 1.);
    const int group = Effect::limit((int) (_group->load() + .5), 1, 8);

    // ── 1. cut the grain at upward zero crossings ────────────────────────────
    int32_t n = 0;
    _starts[n++] = 0;
    for (int32_t i = 1; i < size; i++)
        if (in[i - 1] < 0. && in[i] >= 0.)
            _starts[n++] = i;
    _starts[n] = size;              // sentinel, so unit k always ends at _starts[k+1]

    const int32_t nUnits = (n - 1) / group + 1;
    if (n < 2 || nUnits < 1) {      // DC, silence, or one long waveset: nothing to do
        for (int32_t i = 0; i < size; i++)
            smmixgain(mixTarget, gain);
        return;
    }

    MYFLOAT grainPeak = 0.;
    if (mode == MODE_NORMALIZE)
        for (int32_t i = 0; i < size; i++)
            grainPeak = std::max(grainPeak, std::abs(in[i]));

    // unit k covers input samples [unitStart(k), unitStart(k+1))
    const auto unitStart = [&](int32_t k) {
        const int32_t idx = std::min(k * group, n);
        return _starts[idx];
    };

    // ── 2. rebuild into the scratch buffer ───────────────────────────────────
    MYFLOAT *out = _scratch.data();
    int32_t w = 0;

    // Writes one unit, optionally reversed / rescaled / replaced by a sine.
    const auto emit = [&](int32_t k, bool reverse, MYFLOAT scale, MYFLOAT sineAmt) {
        const int32_t b = unitStart(k), e = unitStart(k + 1);
        const int32_t len = e - b;
        if (len <= 0)
            return;
        MYFLOAT peak = 0.;
        if (sineAmt > 0.)
            for (int32_t i = b; i < e; i++)
                peak = std::max(peak, std::abs(in[i]));
        const int32_t take = std::min(len, size - w);
        for (int32_t i = 0; i < take; i++) {
            MYFLOAT v = in[reverse ? (e - 1 - i) : (b + i)] * scale;
            if (sineAmt > 0.) {
                // one full cycle across the unit, at the unit's own peak: the
                // classic substitution, everything becomes a buzzing tone
                const MYFLOAT s = peak * std::sin(TWOPI_P * (MYFLOAT) i / (MYFLOAT) len);
                v += sineAmt * (s - v);
            }
            out[w + i] = v;
        }
        w += take;
    };

    // A mode can under-fill the buffer (OMIT always does), and leaving the rest
    // silent would turn the AMOUNT knob into a fade. Instead the unit list is
    // walked again until the grain is full. Concatenating at waveset boundaries
    // is click-free by construction: every boundary is an upward zero crossing,
    // so any two wavesets butt together at zero. The exception is the last unit,
    // which ends at the grain edge rather than at a crossing - it is emitted on
    // the first pass (so AMOUNT 0 is exactly the input) and skipped on the
    // repeats.
    for (int pass = 0; pass < MAX_PASSES && w < size; pass++) {
        const int32_t passUnits = (pass == 0 || nUnits < 2) ? nUnits : nUnits - 1;
        const int32_t wBefore = w;

        switch (mode) {
            case MODE_REPEAT: {
                const int reps = 1 + (int) (amt * 7. + .5);
                for (int32_t k = 0; k < passUnits && w < size; k++)
                    for (int r = 0; r < reps && w < size; r++)
                        emit(k, false, 1., 0.);
                break;
            }
            case MODE_OMIT: {
                // keep one, drop `drop`: a cyclic pattern rather than a random
                // one, so it thins the material rhythmically
                const int drop = (int) (amt * 7. + .5);
                for (int32_t k = 0; k < passUnits && w < size; k++)
                    if (drop == 0 || (k % (drop + 1)) == 0)
                        emit(k, false, 1., 0.);
                break;
            }
            case MODE_REVERSE: {
                for (int32_t k = 0; k < passUnits && w < size; k++)
                    emit(k, tsl::random::randomfloat(0.f, 1.f) < amt, 1., 0.);
                break;
            }
            case MODE_NORMALIZE: {
                for (int32_t k = 0; k < passUnits && w < size; k++) {
                    const int32_t b = unitStart(k), e = unitStart(k + 1);
                    MYFLOAT peak = 0.;
                    for (int32_t i = b; i < e; i++)
                        peak = std::max(peak, std::abs(in[i]));
                    MYFLOAT scale = 1.;
                    if (peak > 1e-9)
                        scale = std::min(grainPeak / peak, NORM_MAX_BOOST);
                    emit(k, false, 1. + amt * (scale - 1.), 0.);
                }
                break;
            }
            case MODE_SHUFFLE: {
                // each output slot is drawn from a window around its own
                // position, so AMOUNT slides from "in order" to "anywhere"
                const MYFLOAT span = 1. + amt * (MYFLOAT) (nUnits - 1);
                for (int32_t k = 0; k < passUnits && w < size; k++) {
                    const int32_t j = Effect::limit(
                            (int32_t) (k + (int32_t) tsl::random::randomfloat(-span, span)),
                            (int32_t) 0, passUnits - 1);
                    emit(j, false, 1., 0.);
                }
                break;
            }
            case MODE_SINE:
            default: {
                for (int32_t k = 0; k < passUnits && w < size; k++)
                    emit(k, false, 1., amt);
                break;
            }
        }

        if (w == wBefore)               // nothing emitted: zero-length units only
            break;
    }

    for (int32_t i = w; i < size; i++)
        out[i] = 0.;

    // ── 3. mix back ──────────────────────────────────────────────────────────
    for (int32_t i = 0; i < size; i++) {
        in[i] = in[i] * (1. - smmix()) + out[i] * smmix() * smgain();
        smmixgain(mixTarget, gain);
    }
}
