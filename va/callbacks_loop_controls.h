//
// Created by pr on 06.10.19.
//

#ifndef GRAINSTORM_CALLBACKS_LOOP_CONTROLS_H
#define GRAINSTORM_CALLBACKS_LOOP_CONTROLS_H


#include "types.h"
#include <view.h>
#include <app.h>
void playstatetask(void *);
// Runs the special (non-toggle) action bound to viewid — record/power/settings/
// midi-learn/clear-tasks/zero-note. Returns true if viewid was a special action
// (already fully handled), false for a plain param that should toggle normally.
// Invoked from Event::applyFromExt so it only fires on UI/MIDI gestures.
bool applySpecialAction(tsl::AppState* _appState, int viewid);
#endif //GRAINSTORM_CALLBACKS_LOOP_CONTROLS_H
