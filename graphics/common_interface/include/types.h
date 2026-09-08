#pragma once
//
// Created by pr on 17.09.23.
//
#pragma once
#ifndef _TYPES_H
#define _TYPES_H
template<typename T,int, int>
class WindowedSinc;
namespace tsl {
    struct AppState;
#ifdef HAS_GUI
    namespace graphics {
        class View;
        class Graphics;
        class EnterValue;
#ifdef GRAINSTORM
        class LoopPositions;
        class ListView;
#endif
#ifdef HAS_MIDI
        class MidiLearning;
#endif
    }
#endif
}
#ifdef GRAINSTORM
#include "types_grainstorm.h"
#define NUM_TRACKS 4
#elif defined POCKETANALOG
#include "types_pocketanalog.h"
#define NUM_TRACKS 1
#elif defined GENERATIVE
#include "types_generative.h"
#define NUM_TRACKS 1
#else

#endif


#endif //GRAINSTORM_TYPES_H
