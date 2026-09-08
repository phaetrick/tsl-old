#pragma once
//
// Generative — parameter enum.
//

#ifndef GENERATIVE_TYPES_H
#define GENERATIVE_TYPES_H

struct SpeexResamplerState_;
typedef struct SpeexResamplerState_ SpeexResamplerState;

// Intentionally near-empty: the generative engine exposes nothing yet.
// The two entries below are not padding — shared tslgraphics sources reference
// them unconditionally, so the enum cannot compile without them:
//   PARAM_NOT_ASSIGNED — Input.cpp, ButtonBase.cpp, PlusMinusControlBase.cpp,
//                        MidiLearning.cpp, ui2/ButtonBase2.cpp, IPlugParamDefs.h
//   POWERButton        — Player::play()/stop()/pause() redraw parameters[POWERButton].view
//   RECORDButton       — Player recording start/stop does the same
// Nothing is bound to either yet, so .view is null; do not call
// player.play()/stop() or start recording until a widget owns them.
//
// New params go at the end, before NUM_PARAMS.
enum paramternumber {
    PARAM_NOT_ASSIGNED,
    POWERButton,
    RECORDButton,
    NUM_PARAMS
};

static constexpr int NUM_PARAMETERS = NUM_PARAMS;

#endif //GENERATIVE_TYPES_H
