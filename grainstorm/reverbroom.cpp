//
// Created by pr on 31.08.20.
//

#include "reverbroom.h"
#include "track.h"
#include "base.h"
#include "delay.h"

roomreverb::roomreverb(TRACK *t) : reverbbase(t), _smalll(_STATE->sr), _mediuml(_STATE->sr), _largel(_STATE->sr), _smallr(_STATE->sr, 1.01),
_mediumr(_STATE->sr, 1.01), _larger(_STATE->sr, 1.01) {
    //_mode = &_STATE->params[t->index][MODFMVOCWET];
    //_early = &_STATE->params[t->index][MODFMVOCDRY];
    _oldearly = *_early;
    _oldmode = *_mode;
    _bypass = &t->bypass[SPACE_REVERB1];
    _gain = &_STATE->params[t->index][REV1GAIN];
    _mix = &_STATE->params[t->index][REV1MIX];
    rev[0][0] = &_smalll;
    rev[0][1] = &_smallr;
    rev[1][0] = &_mediuml;
    rev[1][1] = &_mediumr;
    rev[2][0] = &_largel;
    rev[2][1] = &_larger;
    //     _earlyrefl.setSampleRate(_STATE->sr);
    _earlyrefl.loadPresetReflection(_oldearly);
}