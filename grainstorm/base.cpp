//
// Created by pr on 16.06.20.
//

#include "base.h"
#include "grainstorm.h"
#include "track.h"
#include "app.h"


int Effect::grainLfoIndex() const {
    return (int) _track->step_point_grain[_chan].load();
}


Effect::Effect(TRACK *t, int id, int type, const std::function<void()> &activate_,
               const std::function<void()> &deactivate_, int32_t chan) : _appState(t->_appState), smoothCoeff{exp(
DIGITAL_TC / (FXRELEASE * t->_STATE->sr * 0.001))} , _type(type), _id(id), _track{t},
                                                           _chan{chan}, _fadeconst{2. * _STATE->onedsr},
                                                           _fadeinc{_fadeconst}{


    if(activate_ != nullptr)
        activate = activate_;
    else{
        if(type == GRAINEFFECT)
        activate = [this](){_track->fx_queue_grain[_chan].push(shared_from_this());};
        else if(type == MONOEFFECT)
        activate = [this](){_track->fx_queue[_chan].push(shared_from_this());};
        else if(type == STEREOEFFECT)
            activate = [this](){_track->fx_queue_stereo.push(shared_from_this());};
    }
    if(deactivate_ != nullptr)
        deactivate = deactivate_;
    else{
        deactivate = [this](){destroyRequested = true;};
    }
}