//
// Created by pr on 29.12.17.
//

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "oscil.h"
#include "defines.h"
#include "types.h"
#include "synth.h"
#include "grainstorm.h"
#include "track.h"
#include "app.h"

RINGG::RINGG(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_RM_GRAIN, GRAINEFFECT) {
    _mix = &_STATE->params[track->index][GRAINRMMIX];
    _bypass = &track->bypass[SPACE_RM_GRAIN];
}

// RMS of the modulator ((1-w) + w*sin) so that MIX only changes character, not level.
static inline MYFLOAT rmMakeup(MYFLOAT w) {
    const MYFLOAT rms = std::sqrt((1.f - w) * (1.f - w) + w * w * .5f);
    return rms > 1e-6f ? 1.f / rms : 1.f;
}

void RINGG::setup() {
    inc = _STATE->onedsr * tsl::random::randomfloat(_STATE->params[_track->index][GRAINRMMIN].load(),
                                               _STATE->params[_track->index][GRAINRMMAX].load());
    // Random start phase: at phase 0 the carrier sits on a zero crossing, so short
    // grains at low rates degenerate into one amplitude hump, identical on every grain.
    phase = tsl::random::randomfloat(0.f, 1.f);
    if (_bypass->load() || destroyRequested)
        mix = 0.f;
    else mix = *_mix;
}

MYFLOAT GrainRingMod::tick(MYFLOAT in, int32_t offset, int size) {
    auto ret = in * ((1.f - mix) + _sine[PHS2INT(phase)] * mix) * makeup;
    phase += inc;
    if (phase >= 1.f)
        phase -= 1.f;
    return ret;
}


void GrainRingMod::prepare(TRACK *t, int32_t chan) {
    inc = t->_STATE->onedsr *
          tsl::random::randomfloat(t->_STATE->params[t->index][GRAINRMMIN].load(), t->_STATE->params[t->index][GRAINRMMAX].load());
    phase = tsl::random::randomfloat(0.f, 1.f);
    _sine = getsinewave();
    if (t->bypass[SPACE_RM_GRAIN])
        mix = 0.f;
    else mix = t->_STATE->params[t->index][GRAINRMMIX];
    makeup = rmMakeup(mix);
}

void RINGG::compute(MYFLOAT *in, int32_t size) {
    setup();
    //double freq = randmton(pow(10, *rm->freq_min * .05), pow(10, *rm->freq_max * .05));
    const MYFLOAT *sine = getsinewave();
    const MYFLOAT makeup = rmMakeup(_smooth1);
    for (int32_t n = 0; n < size; n++) {
        in[n] *= ((1.f - _smooth1) + sine[PHS2INT(phase)] * _smooth1) * makeup;
        sm1(mix);
        phase += inc;
        if (phase >= 1.f)
            phase -= 1.f;
    }
}


static inline MYFLOAT intpow1(MYFLOAT x, uint32_t n) /* Binary positive power function */
{
    MYFLOAT ans = 1.0f;
    while (n != 0) {
        if (n & 1u) ans = ans * x;
        n >>= 1u;
        x = x * x;
    }
    return ans;
}

void Partials::setup(int32_t size) {
    k = (int32_t) *kpartoffset;                   /* fix k and n  */
    int32_t nn;
    if ((nn = (int32_t) *kparts) < 0) nn = -nn;
    if (nn == 0) {              /* n must be > 0 */
        nn = 1;
    }
    km1 = k - 1;
    kpn = k + nn;
    kpnm1 = kpn - 1;
    MYFLOAT absr;
    if ((r = (MYFLOAT) *kr) != prvr || nn != prvn) {
        twor = r + r;
        rsqp1 = r * r + 1.0f;
        rtn = intpow1(r, nn);
        rtnp1 = rtn * r;
        if ((absr = std::abs(r)) > 0.999f && absr < 1.001f)
            rsumr = 1.0f / nn;
        else rsumr = (1.0f - absr) / (1.0f - std::abs(rtn));
        prvr = r;
        prvn = (int16_t) nn;
    }
    MYFLOAT gain = 1.0f;
    scal = rsumr * gain;
    MYFLOAT freq = LOG2NORMALF(*kfreq) * (cpsfact != nullptr ? *cpsfact : 1.0);
    inc = (int32_t) (prevfreq * phaseinc);
    incinc = (freq * *portamento - freq) * phaseinc / (MYFLOAT) size;
    prevfreq = freq;
}

MYFLOAT Partials::tick() {
    auto denom = rsqp1 - twor * ft[phase & lenmask];
    auto num = ft[phase * k & lenmask]
               - r * ft[phase * km1 & lenmask]
               - rtn * ft[phase * kpn & lenmask]
               + rtnp1 * ft[phase * kpnm1 & lenmask];
    MYFLOAT out;
    if (denom > 0.0002f || denom < -0.0002f) {
        out = last = num / denom * scal;
    } else if (last < 0)
        out = last = -1.f;
    else
        out = last = 1.f;
    //if (afreq.initialized)
    //    inc = (int32_t) ((prevfreq + prevfreq * afreq.Tick()) * sicvt);
    phase += inc;
    phase &= lenmask;

    inc += incinc;
    return out;
}

void Partials::compute(MYFLOAT *out, int32_t size) {
    const MYFLOAT gain = 1.0;
    for (int32_t i = 0; i < size; i++) {
        //if (ampcod)
        //    scal = rsumr * ampp[i];
        auto denom = rsqp1 - twor * ft[phase & lenmask];
        auto num = ft[phase * k & lenmask]
                   - r * ft[phase * km1 & lenmask]
                   - rtn * ft[phase * kpn & lenmask]
                   + rtnp1 * ft[phase * kpnm1 & lenmask];
        if (denom > 0.0002f || denom < -0.0002f) {
            out[i] = last = num / denom * scal;
        } else if (last < 0)
            out[i] = last = -gain;
        else
            out[i] = last = gain;
        //if (afreq.initialized)
        //    inc = (int32_t) ((prevfreq + prevfreq * afreq.Tick()) * sicvt);
        phase += inc;
        phase &= lenmask;

        inc += incinc;
    }
}

template<typename Iterator>
void Partials::compute2(Iterator start, Iterator end) {
    const MYFLOAT gain = 1.0;
    while (start != end) {
        //if (ampcod)
        //    scal = rsumr * ampp[i];
        MYFLOAT denom = rsqp1 - twor * ft[phase & lenmask];
        MYFLOAT num = ft[phase * k & lenmask]
                      - r * ft[phase * km1 & lenmask]
                      - rtn * ft[phase * kpn & lenmask]
                      + rtnp1 * ft[phase * kpnm1 & lenmask];
        if (denom > 0.0002f || denom < -0.0002f) {
            *start = last = num / denom * scal;
        } else if (last < 0)
            *start = last = -gain;
        else
            *start = last = gain;
        //if (afreq.initialized)
        //    inc = (int32_t) ((prevfreq + prevfreq * afreq.Tick()) * sicvt);
        phase += inc;
        phase &= lenmask;

        inc += incinc;
        ++start;
    }
}

GrainPart::GrainPart(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_GRAINPART, GRAINEFFECT),
                                               partials(getcosinewave(), _STATE->sr, &__freq,
                                                        &_STATE->params[track->index][GRAINPARTNUMPART],
                                                        &_STATE->params[track->index][GRAINPARTMULTI],
                                                        &_STATE->params[track->index][GRAINPARTOFFSET],
                                                        &track->pitchfact[_chan],
                                                        &track->glissfact[_chan]) {
    _bypass = &track->bypass[SPACE_GRAINPART];
    prevfreq[chan * track->index] = _STATE->params[track->index][GRAINPARTFREQ].load();
    //prevphase = _DATA->offset * _STATE->onedsr;
    //prevsteppoint = _DATA->offset;
};


void GrainPart::setup(int32_t size, const MYFLOAT *in) {
    if (_bypass->load() || destroyRequested) {
        dry = 1.f;
        wet = 0.f;
    } else {
        dry = LOG2NORMALF(_STATE->params[_track->index][GRAINPARTDRY]);
        wet = LOG2NORMALF(_STATE->params[_track->index][GRAINPARTWET]);
    }
/*    long double step_point = _DATA->offset + _track->step_point_grain[_chan];
    long double diff = step_point - prevsteppoint[_chan * _track->index];
    prevphase[_chan * _track->index] += diff * LOG2NORMAL(prevfreq[_chan * _track->index]) * _track->pitchfact[_chan] * _STATE->onedsr;
    prevsteppoint[_chan * _track->index] = step_point;
*/

    if (_STATE->params[_track->index][GRAINPARTFOLLOW] == 1) {
        if (in != nullptr) {
            MYFLOAT maxx = 0;
            for (int32_t i = 0; i < size; i++)
                maxx = maxx > in[i] ? maxx : in[i];
            gg = maxx;
        } else gg = 1.f;
        if (_STATE->params[_track->index][GRAINPARTHOLD] != 1.0) {
            prevfreq[_chan * _track->index] = __freq = LOG10D20(
                    _STATE->params[_track->index][PITCHDETECTGRAINFXTRACKOUT0 +
                                   _chan].load());
        } else __freq = prevfreq[_chan * _track->index];
    } else {
        prevfreq[_chan * _track->index] = __freq = _STATE->params[_track->index][GRAINPARTFREQ].load();
        gg = 1.0;
    }

    partials.setup(size);

    /*  if (false) {
          partials.SetPhase(fmod(prevphase[_chan * _track->index], 1.0));
      } else
      */    partials.SetPhase(0);

}

void GrainPart::compute(MYFLOAT *in, int32_t size) {
    setup(size, in);
    for (int32_t i = 0; i < size; i++) {
        in[i] = in[i] * _smooth2 + partials.tick() * _smooth1 * gg;
        smwetdry(wet, dry);
    }
}

MYFLOAT GrainPart::tick(MYFLOAT in) {
    auto out = in * _smooth2 + partials.tick() * _smooth1 * gg;
    smwetdry(wet, dry);
    return out;
}

MYFLOAT GrainPart::prevfreq[8]/*, GrainPart::prevphase[8], GrainPart::prevsteppoint[8]*/;


void GrainBuzz::prepare(TRACK *track, int32_t chan, int grainsize) {
    auto _appState = track->_appState;
    if (track->bypass[SPACE_GRAINPART]) {
        dry = 1.f;
        wet = 0.f;
    } else {
        dry = LOG2NORMALF(_STATE->params[track->index][GRAINPARTDRY]);
        wet = LOG2NORMALF(_STATE->params[track->index][GRAINPARTWET]);
    }
/*    long double step_point = _DATA->offset + _track->step_point_grain[_chan];
    long double diff = step_point - prevsteppoint[_chan * _track->index];
    prevphase[_chan * _track->index] += diff * LOG2NORMAL(prevfreq[_chan * _track->index]) * _track->pitchfact[_chan] * _STATE->onedsr;
    prevsteppoint[_chan * _track->index] = step_point;
*/

    MYFLOAT cps;
    if (_STATE->params[track->index][GRAINPARTFOLLOW] == 1) {
        if (_STATE->params[track->index][GRAINPARTHOLD] != 1.0) {
            cps = LOG10D20(_STATE->params[track->index][PITCHDETECTGRAINFXTRACKOUT0 + chan].load());
        } else cps = _STATE->params[track->index][GRAINBUZZCPSOLD0 + chan];
    } else {
        cps = LOG2NORMALF(_STATE->params[track->index][GRAINPARTFREQ].load());
    }

    _STATE->params[track->index][GRAINBUZZCPSOLD0 + chan] = cps;

    cps *= track->pitchfact[chan];

    if (cps < 18.)
        cps = 18.;
    else if (cps > 5000.)
        cps = 5000.;

    auto cpsgliss = cps * track->glissfact[chan];
    if (cpsgliss < 18.)
        cpsgliss = 18.;
    else if (cpsgliss > 5000.f)
        cpsgliss = 5000.f;

    ft = getcosinewave();
    last = 1.0f;
    k = (int) _STATE->params[track->index][GRAINPARTOFFSET];                   /* fix k and n  */
    auto nn = (int) _STATE->params[track->index][GRAINPARTNUMPART];
    km1 = k - 1;
    kpn = k + nn;
    kpnm1 = kpn - 1;
    r = _STATE->params[track->index][GRAINPARTMULTI];
    twor = r + r;
    rsqp1 = r * r + 1.0f;
    rtn = intpow1(r, nn);
    rtnp1 = rtn * r;
    MYFLOAT absr;
    if ((absr = std::abs(r)) > 0.999f && absr < 1.001f)
        scal = 1.0f / nn;
    else scal = (1.0f - absr) / (1.0f - std::abs(rtn));

    inc = cps * WINDOW_SIZE * _STATE->onedsr;
    incinc = (cpsgliss - cps) * WINDOW_SIZE * _STATE->onedsr / (MYFLOAT) grainsize;
    SetPhase(0);
}


MYFLOAT GrainBuzz::tick(MYFLOAT in, int, int) {
    auto denom = rsqp1 - twor * ft[phase & lenmask];
    auto num = ft[phase * k & lenmask]
               - r * ft[phase * km1 & lenmask]
               - rtn * ft[phase * kpn & lenmask]
               + rtnp1 * ft[phase * kpnm1 & lenmask];
    MYFLOAT out;
    if (denom > 0.0002f || denom < -0.0002f) {
        out = last = num / denom * scal;
    } else if (last < 0)
        out = last = -1.f;
    else
        out = last = 1.f;
    //if (afreq.initialized)
    //    inc = (int32_t) ((prevfreq + prevfreq * afreq.Tick()) * sicvt);
    phase += inc;
    phase &= lenmask;

    return in * dry + out * wet;
}
