//
// Created by pr on 27.02.21.
//

#include "moogladder.h"
#include "defines.h"
#include "track.h"
#include "envelope_window.h"

GrainFilter::GrainFilter(TRACK *track, int32_t channel) : Effect(track, channel, SPACE_GRAINFILTER, GRAINEFFECT) {
//int32_t index = track->index;
    res = &_STATE->params[track->index][GRAINFILTERRES];
    _mix = &_STATE->params[track->index][GRAINFILTERMIX];
    _gain = &_STATE->params[track->index][GRAINFILTERGAIN];
    _smooth2 = dbToLinear60(*_gain);
    //static const char *filtertypes[] = {"VCF", "RESLP", "RESON", "VCF2", "DLAD", "DLAD2", "ImpMoog", "KMoog", "MTMoog", "MDSPMoog", "RKMoog", "SimplMoog", "StilsonMoog"};

    updateenv = &_STATE->params[track->index][GRAINFILTERRECOMPUTE1 + channel];
    _bypass = &track->bypass[SPACE_GRAINFILTER];
    type = &_STATE->params[track->index][GRAINFILTERTYPE];
    _env = track->grainfilttable[channel];
}

void GrainFilter::start(){
    if (updateenv->load() == 1.0 || _init) {
        updateenv->store(0.0);
        _init = false;
        renderenv(_track, GRAINFILTERENVX0, _env);
        for (int32_t smpl = 0; smpl < TBLSIZE3; smpl++) {
            auto i = tsl::graphics::EnvelopeEditorWindow::freq_at_y(_env[smpl]);
            if (i >= FILTER_CUT_MAX)
                i = FILTER_CUT_MAX - 1;
            else if (i < 1)
                i = 1;
            _env[smpl] = i * _STATE->onedsr;
        }
    }
    type_ = (int) type->load();
    q_ = res->load();
    filter[type_]->reset();
    gain = dbToLinear60(*_gain);
    if (*_bypass || destroyRequested) {
        mix = 0;
    } else {
        mix = 1.f;
    }
}

MYFLOAT GrainFilter::tick(MYFLOAT in, int32_t offset, int grainsize){
    if(offset %64 == 0)
        filter[type_]->setParams(_env[PHS2INT3(offset / (MYFLOAT) grainsize)], q_);
    smmixgain(mix, gain);
    return in * (1.f - mix) + (MYFLOAT)filter[type_]->tick(in) * mix * gain;
}

void GrainFilter::compute(MYFLOAT *in, int32_t size) {
/*
    for (int32_t i = 0; i < size; i++) {
        in[i] = filter[type_]->tick(in[i], _env[PHS2INT(i / (MYFLOAT) size)], q) * gain;
    }
*/
    start();
    for(int32_t i=0;i<size;i++){
        if(i %64 == 0)
            filter[type_]->setParams((double)_env[PHS2INT3(i / (MYFLOAT) size)], (double)q_);
        in[i]=  in[i] * (1.f - mix) + (MYFLOAT)filter[type_]->tick(in[i]) * mix * gain;
        smmixgain(mix, gain);
    }
}


void GrainFilter::renderenv(TRACK *track, int32_t miditarget, MYFLOAT *env, int size) {
    auto _appState = track->_appState;
    const int32_t nsegs = _STATE->params[track->index][miditarget +
                                    OFFNSEGS].load();
    MYFLOAT x[32], y[32];

    for (int32_t i = 0; i <= nsegs; i++) {
        x[i] = _STATE->params[track->index][miditarget + i].load();
        y[i] = _STATE->params[track->index][
                miditarget +
                OFFPOINTY + i].load();
    }


    tsl::envelope::compute<MYFLOAT>[(int) _STATE->params[track->index][miditarget + OFFCURVE].load()](env, size,
                                                                         x,
                                                                         y,
                                                                         nsegs,
                                                                         true);
}


void GrainFilt::prepare(TRACK *t, int32_t chan){
_env = t->grainfilttable[chan];
auto _appState = t->_appState;
    auto recompute = _STATE->params[t->index][GRAINFILTERRECOMPUTE1 + chan].exchange(0.f);
    if (recompute == 1.0) {
        GrainFilter::renderenv(t, GRAINFILTERENVX0, _env);
        for (int32_t smpl = 0; smpl < TBLSIZE3; smpl++) {
            auto i = tsl::graphics::EnvelopeEditorWindow::freq_at_y(_env[smpl]);
            if (i >= FILTER_CUT_MAX)
                i = FILTER_CUT_MAX - 1;
            else if (i < 1)
                i = 1;
            _env[smpl] = i * _STATE->onedsr;
        }
    }
    type_ = (int) _STATE->params[t->index][GRAINFILTERTYPE].load();
    q_ = _STATE->params[t->index][GRAINFILTERRES];
    filter[type_]->reset();
    gain = dbToLinear60(_STATE->params[t->index][GRAINFILTERGAIN]);
    if (t->bypass[SPACE_GRAINFILTER]) {
        mix = 0;
    } else {
        mix = 1.f;
    }
}

MYFLOAT GrainFilt::tick(MYFLOAT in, int32_t offset, int grainsize){
    if(offset %64 == 0)
        filter[type_]->setParams(_env[PHS2INT3(offset / (MYFLOAT) grainsize)], q_);
    return in * (1.f - mix) + filter[type_]->tick(in) * mix * gain;
}
