//
// VOX — mono-FX FOF vowel synthesis. See Vox.h.
//

#include <cmath>
#include "Vox.h"
#include "VoxFormants.h"
#include "track.h"
#include "grainstorm.h"

Vox::Vox(TRACK *t, int32_t chan) : Effect(t, chan, SPACE_VOX, MONOEFFECT) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_VOX];
    _freq = &_STATE->params[t->index][VOXFREQ];
    _vowel = &_STATE->params[t->index][VOXVOWEL];
    _voice = &_STATE->params[t->index][VOXVOICE];
    _att = &_STATE->params[t->index][VOXATT];
    _wet = &_STATE->params[t->index][VOXWET];
    _dry = &_STATE->params[t->index][VOXDRY];
    _follow = &_STATE->params[t->index][VOXFOLLOW];
    _hold = &_STATE->params[t->index][VOXHOLD];
    _pitchOut = &_STATE->params[t->index][PITCHDETECTFXTRACKOUT0 + chan];
    _gliss = &_STATE->params[t->index][VOXGLISS];
    _oct = &_STATE->params[t->index][VOXOCT];
    _lfo_vowel = &t->lfo[VOXVOWEL];
    _lfo_freq = &t->lfo[VOXFREQ];
    _sinewave = _DATA->eq["FULL SINE"].win;
    _mpidsr = -PI_F_P / (float) _STATE->sr;
}

void Vox::updateFormants(int vc, float morph) {
    _lastVc = vc;
    _lastMorph = morph;
    const int v0 = (int) morph;
    const int v1 = std::min(v0 + 1, 4);
    const float frac = morph - (float) v0;
    auto _appState = this->_appState;
    for (int f = 0; f < NFORM; f++) {
        const float freq = voxFormFreq[vc][v0][f] + frac * (voxFormFreq[vc][v1][f] - voxFormFreq[vc][v0][f]);
        const float bw = voxFormBw[vc][v0][f] + frac * (voxFormBw[vc][v1][f] - voxFormBw[vc][v0][f]);
        const float ampDb = voxFormAmpDb[vc][v0][f] + frac * (voxFormAmpDb[vc][v1][f] - voxFormAmpDb[vc][v0][f]);
        _phinc[f] = freq * (float) _STATE->onedsr;
        _decmult[f] = expf(bw * _mpidsr);
        _spawnGain[f] = powf(10.f, ampDb * .05f);
    }
}

void Vox::compute(MYFLOAT *in, int32_t size) {
    auto _appState = this->_appState;
    // RESON-style: never stop processing on bypass/power-off — drive the wet/dry
    // targets to (0,1) instead, so the smoother completes the fade and smwetdry
    // can set readyToDestroy for the reaper.
    const bool inactive = *_bypass == 1.0 || destroyRequested;

    // fundamental
    float cps;
    if (_follow->load() == 1.0) {
        if (_hold->load() != 1.0)
            _lastCps = (float) LOG10D20(_pitchOut->load());
        cps = _lastCps;
    } else {
        cps = LOG2NORMALF(_freq->load());
        _lastCps = cps;
    }
    cps = Effect::limit(cps, 20.f, 2000.f);
    int32_t period = std::max((int32_t) ((float) _STATE->sr / cps), 16);

    const int vc = Effect::limit((int) _voice->load(), 0, 4);
    const float morphConst = Effect::limit((float) _vowel->load(), 0.f, 4.f);

    // VOWEL: envelope follower (grainstorm follower system, like RESON CF)
    auto &folV = _STATE->followerMap[_track->index].at(VOXVOWEL);
    auto srcV = folV.source.load();
    MYFLOAT *envVbuf = srcV == _track->index ? in : _DATA->tracks[srcV]->envf_buffer[_chan];
    const bool envV_on = folV.prepare(_chan);

    // VOWEL: LFO
    LFO *lfoV = *_lfo_vowel;
    const bool lfoV_on = lfoV && lfoV->power();
    float vowelA = 0.f, vowelRange = 0.f;
    if (lfoV_on) {
        vowelA = (float) _STATE->controls[_track->index][VOXVOWEL].lfo_min.load();
        vowelRange = (float) _STATE->controls[_track->index][VOXVOWEL].lfo_max.load() - vowelA;
    }

    // CPS: LFO (bounds live in the log domain like the param itself)
    LFO *lfoF = *_lfo_freq;
    const bool lfoF_on = lfoF && lfoF->power();
    float cpsA = 0.f, cpsRange = 0.f;
    if (lfoF_on) {
        cpsA = LOG2NORMALF(_STATE->controls[_track->index][VOXFREQ].lfo_min.load());
        cpsRange = (float) LOG2NORMALF(_STATE->controls[_track->index][VOXFREQ].lfo_max.load()) - cpsA;
    }

    // WET: envelope follower
    auto &folW = _STATE->followerMap[_track->index].at(VOXWET);
    auto srcW = folW.source.load();
    MYFLOAT *envWbuf = srcW == _track->index ? in : _DATA->tracks[srcW]->envf_buffer[_chan];
    const bool envW_on = folW.prepare(_chan);

    if (vc != _lastVc || std::fabs(morphConst - _lastMorph) > .002f)
        updateFormants(vc, morphConst);

    const float attFrac = Effect::limit((float) _att->load(), 0.f, 1.f);
    const float glissFact = powf(2.f, (float) _gliss->load() / 12.f);
    const float koct = Effect::limit((float) _oct->load(), 0.f, 2.f);
    const float wetConst = inactive ? 0.f : LOG2NORMALF(_wet->load());
    const float dry = inactive ? 1.f : LOG2NORMALF(_dry->load());

    for (int32_t i = 0; i < size; i++) {
        const MYFLOAT xn = in[i];

        // vowel morph modulation (follower and/or LFO, RESON-style combination)
        float morphNow = morphConst;
        if (envV_on)
            morphNow = (float) (_chan == 0 ? folV.detectL(envVbuf[i]) : folV.detectR(envVbuf[i]));
        if (lfoV_on)
            morphNow = envV_on ? morphNow * (float) lfoV->buf[i]
                               : vowelA + (float) lfoV->buf[i] * vowelRange;
        morphNow = Effect::limit(morphNow, 0.f, 4.f);
        if (vc != _lastVc || std::fabs(morphNow - _lastMorph) > .002f)
            updateFormants(vc, morphNow);

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

        // spawn a new burst every fundamental period
        if (--_spawnCounter <= 0) {
            float cpsNow = cps;
            if (lfoF_on)
                cpsNow = Effect::limit(cpsA + (float) lfoF->buf[i] * cpsRange, 20.f, 2000.f);
            period = std::max((int32_t) ((float) _STATE->sr / cpsNow), 16);
            _spawnCounter = period;

            // octaviation: attenuate odd-numbered bursts (koct 0..1 fades them
            // for an octave drop; 1..2 additionally fades every second
            // remaining burst for the next octave)
            const uint32_t n = _burstIndex++;
            float burstAmp = 1.f;
            for (int b = 0; b < 2; b++) {
                const float t = Effect::limit(koct - (float) b, 0.f, 1.f);
                if (t > 0.f && ((n >> b) & 1u))
                    burstAmp *= 1.f - t;
            }
            if (burstAmp > 1e-4f) {
                int slot = 0;
                int32_t oldest = -1;
                for (int e = 0; e < MAXEXC; e++) {
                    if (!_exc[e].active) { slot = e; break; }
                    if (_exc[e].age > oldest) { oldest = _exc[e].age; slot = e; }
                }
                auto &x = _exc[slot];
                x.active = true;
                x.age = 0;
                x.amp = burstAmp;
                x.attLen = std::max((int32_t) ((float) period * attFrac), 8);
                x.glissLen = 4 * period;
                for (int f = 0; f < NFORM; f++) {
                    x.phs[f] = 0.f;
                    x.phinc[f] = _phinc[f];
                    x.glissinc[f] = (_phinc[f] * glissFact - _phinc[f]) / (float) x.glissLen;
                    x.expamp[f] = _spawnGain[f];
                    x.decmult[f] = _decmult[f];
                }
            }
        }

        float vox = 0.f;
        for (int e = 0; e < MAXEXC; e++) {
            auto &x = _exc[e];
            if (!x.active) continue;
            // raised-cosine attack, then the bandwidth exponential carries the decay
            const float env = x.age < x.attLen
                ? .5f + .5f * _sinewave[PHS2INT(.75f + .5f * (float) x.age / (float) x.attLen)]
                : 1.f;
            float sum = 0.f;
            const bool glide = x.age < x.glissLen;
            for (int f = 0; f < NFORM; f++) {
                sum += _sinewave[PHS2INT(x.phs[f])] * x.expamp[f];
                x.expamp[f] *= x.decmult[f];
                x.phs[f] += x.phinc[f];
                if (glide)
                    x.phinc[f] += x.glissinc[f];
                if (x.phs[f] >= 1.f)
                    x.phs[f] -= 1.f;
            }
            vox += sum * env * x.amp;
            ++x.age;
            // retire once the slowest-decaying formants are inaudible
            if (x.age > x.attLen && x.expamp[0] < 1e-5f && x.expamp[1] < 1e-5f)
                x.active = false;
        }

        in[i] = xn * _smooth2 + vox * _smooth1;
        smwetdry(wetNow, dry);
    }
}
