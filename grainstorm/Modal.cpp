//
// Created by pr on 15.08.20.
//

#include "Modal.h"
#include "defines.h"
#include "tools.h"
#include "track.h"
#include "lpc.h"
#include "grainstorm.h"
#include "app.h"


const MYFLOAT modal_dahina[] = {1, 2.89, 4.95, 6.99, 8.01, 9.02, -1.};

const MYFLOAT modal_banyan[] = {1, 2.0, 3.01, 4.01, 4.69, 5.63, -1.};

const MYFLOAT modal_xylophon[] = {1, 3.932, 9.538, 16.688, 24.566, 31.147, -1.};

//;tibetan bowl (180mm)
const MYFLOAT modal_tibetan_bowl[] = {1, 2.77828, 5.18099, 8.16289, 11.66063, 15.63801, 19.99, -1.};

//;spinel sphere with diameter of 3.6675mm
const MYFLOAT modal_spinel_sphere[] = {1, 1.026513174725, 1.4224916858532, 1.4478690202098,
                                      1.4661959580455, 1.499452545408, 1.7891839345101,
                                      1.8768994627782, 1.9645945254541, 1.9786543873113,
                                      2.0334612432847, 2.1452852391916, 2.1561524686621,
                                      2.2533435661294, 2.2905090816065, 2.3331798413917,
                                      2.4567715528268, 2.4925556408289, 2.5661806088514,
                                      2.6055768738808, 2.6692760296751, 2.7140956766436,
                                      2.7543617293425, 2.7710411870043, -1.};

//;pot lid
const MYFLOAT modal_pot[] = {1, 3.2, 6.23, 6.27, 9.92, 14.15, -1.};


//tubular bell
const MYFLOAT modal_tubular_bell[] = {272. / 437., 538. / 437., 874. / 437., 1281. / 437.,
                                     1755. / 437.,
                                     2264. / 437., 2813. / 437., 3389. / 437., 4822. / 437.,
                                     5255. / 437.,
                                     -1.};

//;red cedar wood plate
const MYFLOAT modal_cedar_wood_plate[] = {1, 1.47, 2.09, 2.56, -1.};

//redwood wood plate
const MYFLOAT modal_redwood_wood_plate[] = {1, 1.47, 2.11, 2.57, -1.};

//douglas fir wood plate
const MYFLOAT modal_fir_wood_plate[] = {1, 1.42, 2.11, 2.47, -1.};

//uniform wooden bar
const MYFLOAT modal_uniform_wodden_bar[] = {1, 2.572, 4.644, 6.984, 9.723, 12, -1.};

//uniform aluminum bar
const MYFLOAT uniform_aluminium_bar[] = {1, 2.756, 5.423, 8.988, 13.448, 18.680, -1.};

//vibraphone 1
const MYFLOAT modal_vibraphone1[] = {1, 3.984, 10.668, 17.979, 23.679, 33.642, -1.};

//vibraphone 2
const MYFLOAT modal_vibraphone2[] = {1, 3.997, 9.469, 15.566, 20.863, 29.440, -1.};

//Chalandi plates
const MYFLOAT modal_chalandi_plates[] = {1, 1.72581, 5.80645, 7.41935, 13.91935, -1.};

//tibetan bowl (152 mm)
const MYFLOAT modal_tibetan_bowl_152[] = {1, 2.66242, 4.83757, 7.51592, 10.64012, 14.21019,
                                         18.14027, -1.};

//tibetan bowl (140 mm)
const MYFLOAT modal_tibetan_bowl_140[] = {1, 2.76515, 5.12121, 7.80681, 10.78409, -1.};

//wine glass
const MYFLOAT modal_wine_glass[] = {1, 2.32, 4.25, 6.63, 9.38, -1.};

//small handbell
const MYFLOAT modal_small_handbell[] = {1, 1.0019054878049, 1.7936737804878, 1.8009908536585,
                                       2.5201981707317, 2.5224085365854, 2.9907012195122,
                                       2.9940548780488, 3.7855182926829, 3.8061737804878,
                                       4.5689024390244, 4.5754573170732, 5.0296493902439,
                                       5.0455030487805, 6.0759908536585, 5.9094512195122,
                                       6.4124237804878, 6.4430640243902, 7.0826219512195,
                                       7.0923780487805, 7.3188262195122, 7.5551829268293, -1.};

const MYFLOAT *modal_rations[] = {modal_dahina, modal_banyan,
                                 modal_xylophon, /*modal_spinel_sphere,*/
                                 modal_pot, modal_tubular_bell,
                                 modal_uniform_wodden_bar,
                                 uniform_aluminium_bar,
                                 modal_vibraphone1, modal_vibraphone2, modal_chalandi_plates,
                                 modal_tibetan_bowl,
                                 modal_tibetan_bowl_152, modal_tibetan_bowl_140, modal_wine_glass/*,
                                 modal_small_handbell*/, modal_cedar_wood_plate,
                                 modal_redwood_wood_plate,
                                 modal_fir_wood_plate};


inline int32_t arraylen(const MYFLOAT *arr) {
    int32_t i = 0;
    while (*(arr + i) != -1.)
        i++;
    return i;
}

void GrainModal::prepare(TRACK *_track, int32_t _chan) {
    auto _appState = _track->_appState;
    const int32_t oldmode = _STATE->params[_track->index][GRAINMODALMODE];
    numChannels = arraylen((MYFLOAT *) modal_rations[(int) oldmode]);
    ingain = LOG2NORMALF(_STATE->params[_track->index][GRAINMODALINGAIN]);
    pregain = LOG2NORMALF(_STATE->params[_track->index][GRAINMODALPREGAIN]);
    if (_track->bypass[SPACE_GRAINMODAL]) {
        dry = 1.f;
        wet = 0.f;
    } else {
        dry = LOG2NORMALF(_STATE->params[_track->index][GRAINMODALDRY]);
        wet = LOG2NORMALF(_STATE->params[_track->index][GRAINMODALWET]) / (MYFLOAT) numChannels;
    }
    const bool f = _STATE->params[_track->index][GRAINMODALFOLLOW] == 1.0;
    MYFLOAT freq;
    if (f) {
        const bool h = _STATE->params[_track->index][GRAINMODALHOLD] == 1.0;
        freq = ((h || !_track->fxpower[SPACE_PDETECTGRAIN]) ? _STATE->params[_track->index][GRAINMODALCPSOLD0 +
                                                                             _chan]
                                                            : _STATE->params[_track->index][
                        PITCHDETECTGRAINFXTRACKOUT0 + _chan]);
    } else freq = LOG2NORMALF(_STATE->params[_track->index][GRAINMODALFREQ]);
    _STATE->params[_track->index][GRAINMODALCPSOLD0 + _chan] = freq;
    freq *= _track->pitchfact[_chan];
    const MYFLOAT q = _STATE->params[_track->index][GRAINMODALQ];
    for (int32_t i = 0; i < numChannels; i++) {
        MYFLOAT fr2 = freq * modal_rations[(int) oldmode][i];
        if (fr2 > 14000)
            fr2 = 14000;
        else if (fr2 < 20.)
            fr2 = 20.;
        filters[i].reset();
        filters[i].setup(_STATE->sr, fr2, q);
    }
};

MYFLOAT GrainModal::tick(MYFLOAT in, int32_t offset, int) {
    MYFLOAT inn = in * ingain;
    if (offset == 0) inn += pregain;
    MYFLOAT smpl{};
    for (int32_t i = 0; i < numChannels; i++)
        smpl += filters[i].tick(inn);
    return smpl * wet + in * dry;
};

void StReson::prepare(MYFLOAT sr) {
    auto size = (int32_t) (sr / 20);   /* size of delay line */
    delaybuf.resize(size, 0);
}

void StReson::setup(MYFLOAT sr, MYFLOAT freq) {
    if (freq < 20.f) freq = 20.f;   /* lowest freq is 20 Hz */
    MYFLOAT tdelay = sr / freq;
    auto delay = (int32_t) (tdelay - 0.5); /* comb delay */
    auto fracdelay = tdelay - (delay + 0.5); /* fractional delay */
    vdt = (int) delaybuf.size() - delay;       /* set the var delay */
    a = (1.0 - fracdelay) / (1.0 + fracdelay);   /* set the all-pass gain */
}


void Mode::setup(MYFLOAT sr, MYFLOAT freq, MYFLOAT q) {
    MYFLOAT kfreq = freq * TWOPI_F_P;
    MYFLOAT kalpha = (sr / kfreq);
    MYFLOAT kbeta = kalpha * kalpha;
    d = 0.5f * kalpha;
    MYFLOAT _q = freq * (.1 + .9 * q);
    a0 = 1.f / (kbeta + d / _q);
    a1 = a0 * (1.f - 2.f * kbeta);
    a2 = a0 * (kbeta - d / _q);
};


void Modal::compute(MYFLOAT *in, int32_t size) {
    prepare(_track, _chan);
    if (destroyRequested) {
        dry = 1.f;
        wet = 0.f;
    }
    for (int32_t i = 0; i < size; i++) {
        in[i] = in[i] *_smooth2 + tick(in[i], 0, 0) * _smooth1;
        smwetdry(wet, dry);
    }
}

ModalEffect::ModalEffect(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_MODAL, MONOEFFECT) {
    track = _track;
    dry = &_STATE->params[track->index][MODALDRY];
    wet = &_STATE->params[track->index][MODALWET];
    hold = &_STATE->params[track->index][MODALHOLD];
    _follow = &_STATE->params[track->index][MODALFOLLOW];
    _bypass = &track->bypass[SPACE_MODAL];
    mode = &_STATE->params[track->index][MODALMODE];
    center = &_STATE->params[track->index][MODALFREQ];
    oldfreq = *center;
    q = &_STATE->params[track->index][MODALQ];
    oldmode = *mode;
    _cpsold = &_STATE->params[track->index][MODALCPSOLD0 + chan];
    _pdetectout = track->pitchdetectoutbuf[chan];
}


void ModalEffect::check(MYFLOAT cps, MYFLOAT q_, MYFLOAT mode_) {
    if (oldfreq != cps || oldmode != mode_ || oldq != q_) {
        oldfreq = cps;
        oldmode = mode_;
        oldq = q_;
        numChannels = arraylen((MYFLOAT *) modal_rations[(int) oldmode]);
        scale = 1.f / (MYFLOAT) numChannels;
        for (int32_t i = 0; i < numChannels; i++) {
            auto fr2 = cps * modal_rations[(int) oldmode][i];
            if (fr2 > 14000)
                fr2 = 14000;
            else if (fr2 < 20.)
                fr2 = 20.;
            filters[i].setup(_STATE->sr, fr2, q_);
        }
    }
    _cpsold->store(cps);
}

void ModalEffect::compute(MYFLOAT *in, int32_t size) {
    MYFLOAT _dry, _wet;
    if (*_bypass || destroyRequested) {
        _dry = 1.f;
        _wet = 0.f;
    } else {
        _dry = LOG2NORMALF(*dry);
        _wet = LOG2NORMALF(*wet);
    }
    const bool f = _follow->load() == 1.0;
    const bool h = hold->load() == 1.0;
    const MYFLOAT m = *mode;
    const MYFLOAT c = *center;
    const MYFLOAT qq = q->load();
    if (!f) {
        check(h ? _cpsold->load() : LOG2NORMALF(*center), qq, m);
    }

    const bool pdetecton = _track->fxpower[SPACE_PDETECT].load();
    for (int32_t i = 0; i < size; i++) {
        if (f && (pdetectindex++ % 64) == 0) {
            check((h || !pdetecton) ? _cpsold->load() : _pdetectout[i], qq, m);
        }
        MYFLOAT smpl = 0;
        for (int32_t chan = 0; chan < numChannels; chan++) {
            smpl += filters[chan].tick(in[i]);
        }
        in[i] = in[i] * _smooth2 + smpl * _smooth1 * scale;
        smwetdry(_wet, _dry);
    }
}
