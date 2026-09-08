//
// Created by pr on 19.12.20.
//

#include "Input.h"
#ifndef ANDROID
#include "app.h"
#include "view.h"
#include "MidiLearning.h"

using namespace tsl::graphics;

int32_t tsl::graphics::callback_input(tsl::AppState* _appState, InputEvent& event) {

#if defined HAS_MIDI
    const bool midilearning = _STATE->midilearning.load() && !_STATE->midilearning_waiting.load();
#endif
    const float xx = event.x, yy = event.y;

    auto& q = _STATE->queue_callback;
    std::lock_guard lock(q);

    switch (event.action) {

    case(ACTION_DOWN): {
        auto node = q._last; /* Iterate from end to start for callbacks */

        while (node) {
            View* temp = node->data;
            if (temp != nullptr && temp->isInside(event) && (temp->viewport.width() == 0 ||
                (xx >= temp->viewport.x() &&
                    xx < temp->viewport.x() +
                    temp->viewport.width() &&
                    yy >= temp->viewport.y()
                    && yy < temp->viewport.y() +
                    temp->viewport.height()))) {

                event.v = temp;
                event.time = tsl::time::nanosecondsSinceEpoch();

                // Add to InputSystem with current coordinates
                _STATE->input_state.addPointer(
                    event.pointer_id,
                    xx, yy,
                    InputSystem::WinState::WINPOINTER,
                    0
                );

                // Add to event pool for active tracking
                auto* pooled_event = _STATE->inputEventPool.acquire();
                if (pooled_event) {
                    *pooled_event = event;
                    _STATE->inputEventPool.addActive(pooled_event);
                }

#if defined HAS_MIDI
                if (midilearning && (_STATE->parameters[temp->id].flags & Param::MidiParam)) {
                    _STATE->UiTasksQueue.add_task([_STATE, id = temp->id]{MidiLearning::wait(_STATE, _STATE->active_track.load(), id);});
                }
                else if (!midilearning || !(_STATE->parameters[temp->id].flags & Param::MidiParam))
                {
                    temp->callback(event);
                    return temp->id;
                }
#else
                temp->callback(event);
                return temp->id;
#endif
                break;
            }
            node = node->prev;
        }
        break;
    }

    case(ACTION_MOUSE_OVER): {
        auto node = q._last; /* Iterate from end to start for callbacks */
        while (node) {
            View* temp = node->data;
            if (temp != nullptr && temp->isInside(event) && (temp->viewport.width() == 0 ||
                (xx >= temp->viewport.x() &&
                    xx < temp->viewport.x() +
                    temp->viewport.width() &&
                    yy >= temp->viewport.y()
                    && yy < temp->viewport.y() +
                    temp->viewport.height()))) {

                event.v = temp;
                event.time = tsl::time::nanosecondsSinceEpoch();

                if (temp != _appState->mCurrentOver) {
                    if (_appState->mCurrentOver) {
                        event.action = ACTION_MOUSE_OUT;
                        _appState->mCurrentOver->callback(event);
                    }
                    _appState->mCurrentOver = temp;
                }

                if (_appState->mCurrentOver) {
                    event.action = ACTION_MOUSE_OVER;
                    _appState->mCurrentOver->callback(event);
                }

                return true;
            }
            node = node->prev;
        }
        break;
    }

    case(ACTION_MOUSE_WHEEL): {
        auto node = q._last; /* Iterate from end to start for callbacks */
        while (node) {
            View* temp = node->data;
            if (temp != nullptr && temp->isInside(event) && (temp->viewport.width() == 0 ||
                (xx >= temp->viewport.x() &&
                    xx < temp->viewport.x() +
                    temp->viewport.width() &&
                    yy >= temp->viewport.y()
                    && yy < temp->viewport.y() +
                    temp->viewport.height()))) {

                event.v = temp;
                event.time = tsl::time::nanosecondsSinceEpoch();

                if ((temp->id != PARAM_NOT_ASSIGNED && temp->id < NUM_PARAMETERS &&
                    !(_appState->parameters[temp->id].flags & Param::NoAssignment) &&
                    _appState->parameters[temp->id].progress != 0)
#ifdef GRAINSTORM
                    || temp->id == LFO1BOUNDA || temp->id == LFO1BOUNDB
#endif
                    ) {

                    auto tindex = _appState->active_track.load();
                    tsl::parameters::Event ev;
                    uint16_t targetId{};
                    ev.setup(_STATE, tindex, temp->id, &targetId);
                    
                    auto& miditarget = _STATE->parameters[targetId];
                    auto value = ev.getCurrentValue(_STATE) + miditarget.progress * event.pointer_id;
                    if (value > miditarget.max)
                        value = miditarget.max;
                    else if (value < miditarget.min)
                        value = miditarget.min;
                    ev.value = value;
                    ev.apply(_STATE, tsl::parameters::FromUi);
                    
                    
                }
                else {
                    temp->callback(event);
                }

                return true;
            }
            node = node->prev;
        }
        break;
    }

    case(ACTION_UP): {
        // CRITICAL FIX: Use the event coordinates directly, not the stored pointer coordinates
        // The event object contains the actual UP coordinates from the system

        // Get the associated view from the pooled event (not the pointer)
        auto* pooled_event = _STATE->inputEventPool.getActive(event.pointer_id);
        if (pooled_event && pooled_event->v) {
            // Use the current event coordinates, not the stored ones
            event.v = pooled_event->v;
            event.time = tsl::time::nanosecondsSinceEpoch();

#if defined HAS_MIDI
            if (!midilearning || !(_STATE->parameters[pooled_event->v->id].flags & Param::MidiParam))
#endif
                pooled_event->v->callback(event);
        }

        // Clean up: Remove from InputSystem and event pool
        _STATE->input_state.removePointer(event.pointer_id);
        _STATE->inputEventPool.removeActive(event.pointer_id);
        // If no more active pointers, clear everything to avoid stale state
        if (_STATE->inputEventPool.getActiveCount() == 0) {
            _STATE->input_state.clear();
        }
        break;
    }

    case(ACTION_MOVE): {
        // CRITICAL: Always update both InputSystem and event pool coordinates
        _STATE->input_state.updatePointer(event.pointer_id, xx, yy);
        _STATE->inputEventPool.updateActive(event.pointer_id, xx, yy, ACTION_MOVE);

        // Get the associated view from the pooled event
        auto* pooled_event = _STATE->inputEventPool.getActive(event.pointer_id);
        if (pooled_event && pooled_event->v) {
            event.v = pooled_event->v;
            event.action = ACTION_MOVE;
            event.time = tsl::time::nanosecondsSinceEpoch();

#if defined HAS_MIDI
            if (!midilearning || !(_STATE->parameters[pooled_event->v->id].flags & Param::MidiParam))
#endif
                pooled_event->v->callback(event);
        }
        break;
    }

    case(ACTION_MOUSE_OUT): {
        if (_STATE->mCurrentOver) {
            _STATE->mCurrentOver->callback(event);
            _STATE->mCurrentOver = nullptr;
        }
        break;
    }

    case(ACTION_KEY_DOWN):
    case(ACTION_KEY_UP): {
        if (event.pointer_id != VKEY_UNKNOWN) {
            auto node = q._last;

            while (node) {
                auto temp = node->data;
                if (temp != nullptr && temp->hasFocus.load()) {
                    temp->callback(event);
                    return 1;
                }
                node = node->prev;
            }
		}
        break;
    }
    }
    return 0;
}

// Helper function to get active pointers for a specific view
std::vector<InputSystem::Pointer*> getActivePointersForView(tsl::AppState* state, View* view) {
    std::vector<InputSystem::Pointer*> result;
    const auto& active_pointers = state->inputEventPool.getActivePointers();
    size_t active_count = state->inputEventPool.getActiveCount();

    for (size_t i = 0; i < active_count; ++i) {
        if (active_pointers[i] && active_pointers[i]->v == view) {
            auto* pointer = state->input_state.getById(active_pointers[i]->pointer_id);
            if (pointer) {
                result.push_back(pointer);
            }
        }
    }
    return result;
}

// Helper function to get all active touch points (for multi-touch gestures)
size_t getActiveTouchCount(tsl::AppState* state) {
    return state->input_state.numPointersWindow();
}

// Helper function for zoom gesture detection
bool isZoomGestureActive(tsl::AppState* state) {
    state->input_state.checkZoom();
    return state->input_state.numPointersWindow() >= 2;
}

// Helper function to get zoom center and distance
std::pair<std::pair<float, float>, float> getZoomInfo(tsl::AppState* state) {
    auto [center_x, center_y] = state->input_state.getZoomCenter();
    float distance = state->input_state.getZoomDistance();
    return { {center_x, center_y}, distance };
}
#endif