//
// VOWEL - see GrainVowel.h.
//

#include <cmath>
#include <algorithm>
#include "GrainVowel.h"
#include "VoxFormants.h"
#include "track.h"
#include "grainstorm.h"
#include "random.h"
#include "lfo.h"

GrainVowel::GrainVowel(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_GRAINVOWEL, GRAINEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_GRAINVOWEL];
    _voice = &_STATE->params[t->index][GRAINVOWVOICE];
    _vmin = &_STATE->params[t->index][GRAINVOWMIN];
    _vmax = &_STATE->params[t->index][GRAINVOWMAX];
    _bw = &_STATE->params[t->index][GRAINVOWBW];
    _mix = &_STATE->params[t->index][GRAINVOWMIX];
    _gain = &_STATE->params[t->index][GRAINVOWGAIN];
    _lfo_vowel = &t->lfo[GRAINVOWMIN];
    _lfo_bw = &t->lfo[GRAINVOWBW];
    _smooth2 = dbToLinear60(*_gain);
    _mtpdsr = -TWOPI_P / (MYFLOAT) _STATE->sr;
    _tpidsr = TWOPI_P / (MYFLOAT) _STATE->sr;
}

void GrainVowel::updateFormants(int vc, MYFLOAT morph, MYFLOAT bwScale) {
    _lastVc = vc;
    _lastMorph = morph;
    _lastBwScale = bwScale;
    const int v0 = (int) morph;
    const int v1 = std::min(v0 + 1, 4);
    const MYFLOAT frac = morph - (MYFLOAT) v0;
    for (int f = 0; f < NFORM; f++) {
        const MYFLOAT freq = voxFormFreq[vc][v0][f] + frac * (voxFormFreq[vc][v1][f] - voxFormFreq[vc][v0][f]);
        const MYFLOAT bw = (voxFormBw[vc][v0][f] + frac * (voxFormBw[vc][v1][f] - voxFormBw[vc][v0][f])) * bwScale;
        const MYFLOAT ampDb = voxFormAmpDb[vc][v0][f] + frac * (voxFormAmpDb[vc][v1][f] - voxFormAmpDb[vc][v0][f]);
        // Csound reson, iscl=1: peak response of exactly 1 at the centre frequency
        const MYFLOAT c3 = std::exp(bw * _mtpdsr);
        const MYFLOAT c3t4 = c3 * 4.;
        const MYFLOAT c2 = c3t4 * std::cos(freq * _tpidsr) / (c3 + 1.);
        _c1[f] = (1. - c3) * std::sqrt(1. - c2 * c2 / c3t4);
        _c2[f] = c2;
        _c3[f] = c3;
        _fgain[f] = std::pow(10., ampDb * .05);
    }
}

void GrainVowel::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(*_gain);
    const MYFLOAT mixTarget = (*_bypass || destroyRequested) ? 0. : (MYFLOAT) *_mix;

    const int vc = Effect::limit((int) _voice->load(), 0, 4);

    // BW: a plain per-grain sample-and-hold of the LFO, no range to preserve
    MYFLOAT bwScale = (MYFLOAT) *_bw;
    const LFO *lfoB = _lfo_bw->load();
    if (lfoB && lfoB->power())
        bwScale = lfoB->min(GRAINVOWBW) +
                  lfoB->buf[grainLfoIndex()] * (lfoB->max(GRAINVOWBW) - lfoB->min(GRAINVOWBW));
    bwScale = Effect::limit(bwScale, .25, 4.);

    // One vowel per grain, drawn from a window of width MAX - MIN. An LFO on
    // this destination moves the CENTRE of that window and leaves the width
    // alone, so the cloud keeps scattering grain to grain while the whole
    // scatter glides A -> E -> I -> O -> U. With no LFO the centre is the
    // midpoint of MIN/MAX, which is exactly a draw between them.
    const MYFLOAT vmin = _vmin->load(), vmax = _vmax->load();
    MYFLOAT centre = (vmin + vmax) * .5;
    const MYFLOAT spread = std::fabs(vmax - vmin) * .5;
    const LFO *lfoV = _lfo_vowel->load();
    if (lfoV && lfoV->power()) {
        const MYFLOAT a = lfoV->min(GRAINVOWMIN), b = lfoV->max(GRAINVOWMIN);
        centre = a + lfoV->buf[grainLfoIndex()] * (b - a);
    }
    const MYFLOAT morph = Effect::limit(
            (MYFLOAT) tsl::random::randomfloat(centre - spread, centre + spread), 0., 4.);

    if (vc != _lastVc || std::fabs(morph - _lastMorph) > .002
        || std::fabs(bwScale - _lastBwScale) > .002)
        updateFormants(vc, morph, bwScale);

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT xn = in[i];
        if (std::isnan(xn)) xn = 0.;

        MYFLOAT vox = 0.;
        for (int f = 0; f < NFORM; f++) {
            MYFLOAT yn = _c1[f] * xn + _c2[f] * _y1[f] - _c3[f] * _y2[f];
            if (std::isnan(yn)) yn = 0.;
            _y2[f] = _y1[f];
            _y1[f] = yn;
            vox += yn * _fgain[f];
        }

        in[i] = xn * (1. - smmix()) + vox * smmix() * smgain();
        smmixgain(mixTarget, gain);
    }

    for (int f = 0; f < NFORM; f++) {
        UDD(_y1[f]);
        UDD(_y2[f]);
    }
}
