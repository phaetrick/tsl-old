//
// Created by pr on 03.11.21.
//

#include "distortion.h"
#include "track.h"
#include "grainstorm.h"
#include "app.h"


// The quantizer's level count for a fractional bit depth. BITS is smoothed in
// the bit domain, not here: fact is exponential in bits, so smoothing the level
// count instead puts almost the whole glide at the top - a 16 -> 4 jump
// measured 13.1 bits after 200 ms, i.e. inaudible for a second.
// Capped at the level count for the parameter's own 16-bit maximum: BITS is no
// longer rounded, so a degenerate value from host automation or a modulation
// source would otherwise reach exp2() and come back as inf, and inf/inf in the
// quantizer is a NaN.
static inline MYFLOAT bitsToFact(MYFLOAT bits) {
    return MIN(exp2(bits) * .5, 32768.);
}

// DRIVE (0..1 knob) as the tanh shaper's gain. Clamped first: pow() of a
// negative base with a fractional exponent is NaN, and DRIVE is a modulation
// destination.
static inline MYFLOAT driveToShaperGain(MYFLOAT drive) {
    drive = MAX(0., MIN(1., drive));
    return kDistDriveMin * pow(kDistDriveMax / kDistDriveMin, pow(drive, kDistDriveCurve));
}


BitCrusher::BitCrusher(TRACK *_track, int32_t _channel) : PolyPhaseResampler2x<double>(.1 + .9 * _STATE->params[_track->index][BCTONE].load()), Effect(_track, _channel, SPACE_BITCRUSHER,
                                                             MONOEFFECT), Balance(_STATE->sr) {
    samples = &_STATE->params[_track->index][BCSR];
    bits = &_STATE->params[_track->index][BCBITS];
    _mix = &_STATE->params[_track->index][BCMIX];
    _gain = &_STATE->params[_track->index][BCGAIN];
    _smooth2 = dbToLinear60(_gain->load());
    _bypass = &_track->bypass[SPACE_BITCRUSHER];
    _oldtone = *(_tone = &_STATE->params[_track->index][BCTONE]);
    xdc = ydc = 0;
    // 20 Hz. The blocker sits inside the oversampled loop, so it has to be
    // derived from sr * OVERSAMPLERATIO - the old fixed .995 was a 76 Hz
    // corner at 48 kHz and 153 Hz at 96 kHz, i.e. it ate the bass and moved
    // with the project rate.
    _dcb = exp(-TWOPI_P * 20. / (_STATE->sr * OVERSAMPLERATIO));
    _bitssm = bits->load();
    _lfo = &_track->lfo[BCSR];
}

void BitCrusher::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(_gain->load());
    MYFLOAT  mix;
    const bool bypass = (*_bypass || destroyRequested);
    if (bypass) {
        mix = 0;
    } else {
        mix = *_mix;
    }
    MYFLOAT *dst;
    int32_t n, c;

    const MYFLOAT nbits = bits->load();
    MYFLOAT _samples = _STATE->sr / LOG2NORMALF(samples->load()) * OVERSAMPLERATIO;

    LFO *lfo = *_lfo;
    bool lfo_on = lfo && lfo->power();
    MYFLOAT lfo_range = 0;
    MYFLOAT lfo_min = LOG2NORMALF(_STATE->parameters[BCSR].min);
    if (lfo_on) {
        MYFLOAT a = LOG2NORMALF(_STATE->controls[_track->index][BCSR].lfo_min.load());
        MYFLOAT b = LOG2NORMALF(_STATE->controls[_track->index][BCSR].lfo_max.load());
        lfo_range = b-a;
        lfo_min = a;
    }

    MYFLOAT tone = *_tone;
    if (tone != _oldtone) {
        _oldtone = tone;
        setCutOff(.1 + .9 *_oldtone);

    }

    auto &fol = _STATE->followerMap[_track->index].at(BCMIX);
    auto env_on = fol.prepare(_chan);
    auto ss = fol.source.load();
    MYFLOAT *envbuf = ss == _track->index ? in : _DATA->tracks[ss]->envf_buffer[_chan];

    UDD(ydc)

    for (n = 0; n < size; n++) {
        MYFLOAT sample[2];
         tickUp(in[n], sample);
        if (lfo_on) {
            _samples = (MYFLOAT) (_STATE->sr / (lfo_min +
                                                  lfo->buf[n] * lfo_range) * OVERSAMPLERATIO);
        }
        const MYFLOAT f = bitsToFact(paramSmooth(nbits, _bitssm));
        auto templ = bitreduction(samplereduction(sample[0], _samples), f);
        sample[0] = ydc = templ - xdc + _dcb * ydc;
        UDD(sample[0])
        xdc = templ;
        templ = bitreduction(samplereduction(sample[1], _samples), f);
        sample[1] = ydc = templ - xdc + _dcb * ydc;
        UDD(sample[1])
        xdc = templ;
        if (env_on) {
            MYFLOAT mmix =   _chan == 0 ? fol.detectL(envbuf[n]) : fol.detectR(envbuf[n]);
            if(bypass) mmix = _smooth1;
            auto mixsrc = 1. - mmix;
            in[n] = in[n] * mixsrc + tickBalance(tickDown(sample), in[n]) * mmix * _smooth2;
        }else{
            MYFLOAT mixsrc = 1. - _smooth1;
            in[n] = in[n] * mixsrc + tickBalance(tickDown(sample), in[n]) * _smooth1 * _smooth2;
        }
        smmixgain(mix, gain);
    }
}


DISTORT::DISTORT(TRACK *track, int32_t channel) : PolyPhaseResampler2x<double>(.1 + .9 * _STATE->params[track->index][DISTTONE].load()), Effect(track, channel, SPACE_DISTORT, MONOEFFECT), Balance(_STATE->sr) {
    _bypass = &track->bypass[SPACE_DISTORT];
    _postgain = &_STATE->params[track->index][DISTGAIN];
    _smooth2 = dbToLinear60(_postgain->load());
    _mix = &_STATE->params[track->index][DISTMIX];
    _cut_off = &_STATE->params[track->index][DISTTONE];
    _cut_off_prev = *_cut_off;
    _drive = &_STATE->params[track->index][DISTDRIVE];
    _ksm = driveToShaperGain(_drive->load());
    _dcb = exp(-TWOPI_P * 20. / _STATE->sr);    // 20 Hz, was a fixed .995 (38 Hz at 48k, 76 Hz at 96k)
}

void DISTORT::compute(MYFLOAT *in, int32_t size) {
    const MYFLOAT gain = dbToLinear60(*_postgain);
    const bool bypass = (*_bypass || destroyRequested);
    MYFLOAT mix = bypass ? 0 : _mix->load();

    auto cc = _cut_off->load();
    if (_cut_off_prev != cc) {
        _cut_off_prev = cc;
        setCutOff(.1 + .9 *_cut_off_prev);
    }
    // DRIVE is a 0..1 knob; k is the shaper gain it maps onto, smoothed per
    // sample below. The old parameter was 0..60 dB and reached the shaper as
    // LOG2NORMAL(dB) * 0.0002 * 30000, i.e. k = 6 .. 6000.
    const MYFLOAT k = driveToShaperGain(_drive->load());
    // Output trim, was -20000. * .5 / 30000. with the 2 of 2*tanh() folded in
    // here. The minus inverted the whole wet path, so around MIX 0.5 the
    // balanced wet cancelled the dry instead of blending with it.
    const MYFLOAT postgain = 2. * 20000. * .5 / 30000.;

    auto &fol = _STATE->followerMap[_track->index].at(DISTMIX);
    auto src = fol.source.load();
    MYFLOAT *envbuf = src == _track->index ? in : _DATA->tracks[src]->envf_buffer[_chan];
    auto env_on = fol.prepare(_chan);


    for (int32_t i = 0; i < size; i++) {
        MYFLOAT tmp[2];
        tickUp(in[i], tmp);
        // (exp(a) - exp(-a)) / cosh(a) is exactly 2*tanh(a) - but the expanded
        // form (Csound distort1, IV - Dec 28 2002) overflows to inf/inf = NaN
        // for |a| > 709, and UDD then rewrote the sample to 0. At the old DRIVE
        // 60 dB that hit every sample above 0.12, punching holes in the waveform.
        const MYFLOAT ks = paramSmooth(k, _ksm);
        MYFLOAT y = tanh(tmp[0] * ks) * postgain;
        UDD(y)
        tmp[0] = y;
        y = tanh(tmp[1] * ks) * postgain;
        UDD(y)
        tmp[1] = y;

        auto temp = tickDown(tmp);
        _yt = temp - _xt + _dcb * _yt;
        _xt = temp;
        if (env_on) {
            auto mmix = _chan == 0 ? fol.detectL(envbuf[i]) : fol.detectR(envbuf[i]);
            if(bypass)
                mmix = _smooth1;
            in[i] = in[i] * (1. - mmix) + tickBalance(_yt, in[i]) * mmix * _smooth2;
        }
        else
        in[i] = in[i] * (1. - _smooth1) + tickBalance(_yt, in[i]) * _smooth1 * _smooth2;
        smmixgain(mix, gain);

    }
    UDD(_yt);
}