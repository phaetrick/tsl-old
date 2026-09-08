//
// VOX2 — mono-FX formant filter bank. See Vox2.h.
//

#include <cmath>
#include "Vox2.h"
#include "VoxFormants.h"
#include "track.h"
#include "grainstorm.h"

Vox2::Vox2(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_VOX2, MONOEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_VOX2];
    _vowel = &_STATE->params[t->index][VOX2VOWEL];
    _voice = &_STATE->params[t->index][VOX2VOICE];
    _bw = &_STATE->params[t->index][VOX2BW];
    _wet = &_STATE->params[t->index][VOX2WET];
    _dry = &_STATE->params[t->index][VOX2DRY];
    _lfo_vowel = &t->lfo[VOX2VOWEL];
    _lfo_bw = &t->lfo[VOX2BW];
    _mtpdsr = -TWOPI_P / (MYFLOAT) _STATE->sr;
    _tpidsr = TWOPI_P / (MYFLOAT) _STATE->sr;
}

void Vox2::updateFormants(int vc, float morph, float bwScale) {
    _lastVc = vc;
    _lastMorph = morph;
    _lastBwScale = bwScale;
    const int v0 = (int) morph;
    const int v1 = std::min(v0 + 1, 4);
    const float frac = morph - (float) v0;
    for (int f = 0; f < NFORM; f++) {
        const float freq = voxFormFreq[vc][v0][f] + frac * (voxFormFreq[vc][v1][f] - voxFormFreq[vc][v0][f]);
        const float bw = (voxFormBw[vc][v0][f] + frac * (voxFormBw[vc][v1][f] - voxFormBw[vc][v0][f])) * bwScale;
        const float ampDb = voxFormAmpDb[vc][v0][f] + frac * (voxFormAmpDb[vc][v1][f] - voxFormAmpDb[vc][v0][f]);
        // Csound reson, iscl=1: peak response of exactly 1 at the centre frequency
        const MYFLOAT c3 = std::exp((MYFLOAT) bw * _mtpdsr);
        const MYFLOAT c3t4 = c3 * 4.;
        const MYFLOAT c2 = c3t4 * std::cos((MYFLOAT) freq * _tpidsr) / (c3 + 1.);
        _c1[f] = (1. - c3) * std::sqrt(1. - c2 * c2 / c3t4);
        _c2[f] = c2;
        _c3[f] = c3;
        _fgain[f] = powf(10.f, ampDb * .05f);
    }
}

void Vox2::compute(MYFLOAT *in, int32_t size) {
    auto _appState = this->_appState;
    // RESON-style: never stop processing on bypass/power-off — drive the wet/dry
    // targets to (0,1) instead, so the smoother completes the fade and smwetdry
    // can set readyToDestroy for the reaper.
    const bool inactive = *_bypass == 1.0 || destroyRequested;

    const int vc = Effect::limit((int) _voice->load(), 0, 4);
    const float morphConst = Effect::limit((float) _vowel->load(), 0.f, 4.f);
    const float bwConst = Effect::limit((float) _bw->load(), .25f, 4.f);

    // VOWEL: envelope follower (grainstorm follower system, like RESON CF)
    auto &folV = _STATE->followerMap[_track->index].at(VOX2VOWEL);
    auto srcV = folV.source.load();
    MYFLOAT *envVbuf = srcV == _track->index ? in : _DATA->tracks[srcV]->envf_buffer[_chan];
    const bool envV_on = folV.prepare(_chan);

    // VOWEL: LFO
    LFO *lfoV = *_lfo_vowel;
    const bool lfoV_on = lfoV && lfoV->power();
    float vowelA = 0.f, vowelRange = 0.f;
    if (lfoV_on) {
        vowelA = (float) _STATE->controls[_track->index][VOX2VOWEL].lfo_min.load();
        vowelRange = (float) _STATE->controls[_track->index][VOX2VOWEL].lfo_max.load() - vowelA;
    }

    // BW: LFO
    LFO *lfoB = *_lfo_bw;
    const bool lfoB_on = lfoB && lfoB->power();
    float bwA = 0.f, bwRange = 0.f;
    if (lfoB_on) {
        bwA = (float) _STATE->controls[_track->index][VOX2BW].lfo_min.load();
        bwRange = (float) _STATE->controls[_track->index][VOX2BW].lfo_max.load() - bwA;
    }

    // WET: envelope follower
    auto &folW = _STATE->followerMap[_track->index].at(VOX2WET);
    auto srcW = folW.source.load();
    MYFLOAT *envWbuf = srcW == _track->index ? in : _DATA->tracks[srcW]->envf_buffer[_chan];
    const bool envW_on = folW.prepare(_chan);

    if (vc != _lastVc || std::fabs(morphConst - _lastMorph) > .002f
        || std::fabs(bwConst - _lastBwScale) > .002f)
        updateFormants(vc, morphConst, bwConst);

    const float wetConst = inactive ? 0.f : LOG2NORMALF(_wet->load());
    const float dry = inactive ? 1.f : LOG2NORMALF(_dry->load());

    for (int32_t i = 0; i < size; i++) {
        MYFLOAT xn = in[i];
        if (isnan(xn)) xn = 0.;

        // vowel morph modulation (follower and/or LFO, RESON-style combination)
        float morphNow = morphConst;
        if (envV_on)
            morphNow = (float) (_chan == 0 ? folV.detectL(envVbuf[i]) : folV.detectR(envVbuf[i]));
        if (lfoV_on)
            morphNow = envV_on ? morphNow * (float) lfoV->buf[i]
                               : vowelA + (float) lfoV->buf[i] * vowelRange;
        morphNow = Effect::limit(morphNow, 0.f, 4.f);

        float bwNow = bwConst;
        if (lfoB_on)
            bwNow = Effect::limit(bwA + (float) lfoB->buf[i] * bwRange, .25f, 4.f);

        if (vc != _lastVc || std::fabs(morphNow - _lastMorph) > .002f
            || std::fabs(bwNow - _lastBwScale) > .002f)
            updateFormants(vc, morphNow, bwNow);

        // wet modulation via follower (throttled dB -> linear conversion)
        float wetNow = wetConst;
        if (envW_on && !inactive) {
            const float wetDb = (float) (_chan == 0 ? folW.detectL(envWbuf[i]) : folW.detectR(envWbuf[i]));
            if (std::fabs(wetDb - _wetDbLast) > .1f) {
                _wetDbLast = wetDb;
                _wetLin = LOG2NORMALF(wetDb);
            }
            wetNow = _wetLin;
        }

        MYFLOAT vox = 0.;
        for (int f = 0; f < NFORM; f++) {
            MYFLOAT yn = _c1[f] * xn + _c2[f] * _y1[f] - _c3[f] * _y2[f];
            if (isnan(yn)) yn = 0.;
            _y2[f] = _y1[f];
            _y1[f] = yn;
            vox += yn * _fgain[f];
        }

        in[i] = xn * _smooth2 + vox * _smooth1;
        smwetdry(wetNow, dry);
    }
}
