//
// Created by pr on 18.01.19.
//

#include <keyboard.h>
#include <logger.h>
#include "defines.h"
#include "tools.h"
#include "grainstorm.h"
#include <view.h>
#include "button.h"
#include <queue.h>
#include <Input.h>
#include <Midi.h>
#include <app.h>
#include <MidiLearning.h>
#include "MidiReceiver.h"
#include "callbacks_loop_controls.h"

using namespace tsl::midi;

#ifdef __ANDROID__
void java_receive_midievent(JNIEnv *env, jclass obj, jbyte _one, jbyte _two, jbyte _three) {
    auto one = (uint8_t)_one;
    auto two = (uint8_t)_two;
    auto three = (uint8_t)_three;
    tsl::AppState* _appState = __STATE;
#else
void onMidiMsg(tsl::AppState* _appState, uint8_t one, uint8_t two, uint8_t three) {
#endif
    if (!_appState) return;

    auto command = (unsigned char)(one & STATUS_COMMAND_MASK);

    switch (command) {
        case STATUS_NOTE_ON:
            if (_STATE->midilearning_waiting &&
                _STATE->midiLearning.load() != nullptr && (int8_t)three > 0) {
                auto ml = _STATE->midiLearning.load();
                if (!ml || (_STATE->parameters[ml->id].type != ParameterType_bool && _STATE->parameters[ml->id].type != ParameterType_enum))
                    break;
                _STATE->activemidicommand[0] = one;
                _STATE->activemidicommand[1] = two;
                tsl::graphics::MidiLearning::setEventReceived(_STATE);
            } else {
                auto channel = (one & STATUS_CHANNEL_MASK);
                if ((int8_t)three > 0) {
                    auto& e = _STATE->midinoteevents[channel][two];
                    if (e.eventType != tsl::parameters::Eventtype::NoParam) {
                        auto ne = e;
                        auto& m = _STATE->parameters[ne.getDisplayParam()];
                        if (m.type == ParameterType_bool) {
                            // A note mapped to a special action runs it; otherwise toggle.
                            if (applySpecialAction(_STATE, ne.paramIndex)) return;
                            ne.value = ne.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
                            ne.flags |= tsl::parameters::Event::Redraw | tsl::parameters::Event::Info;
                            ne.applyFromExt(_STATE, tsl::parameters::None);
                            return;
                        } else {
                            MYFLOAT numChoices = !m.values.empty() ? (MYFLOAT)m.values.size()
                                               : !m.names.empty()  ? (MYFLOAT)m.names.size()
                                               : m.max + 1;
                            ne.value = m.toDisplay(_STATE->sr, ne.getCurrentValue(_STATE)) + (e.midiState.inc == 1 ? 1 : -1);
                            if (ne.value >= numChoices) ne.value = 0;
                            else if (ne.value < 0) ne.value = numChoices - 1;
                            ne.value = m.fromDisplay(_STATE->sr, ne.value);
                        }
                        ne.flags |= tsl::parameters::Event::Redraw | tsl::parameters::Event::Info;
                        ne.applyFromExt(_STATE, tsl::parameters::None);
                    } else {
                        _DATA->preQueue.push(
                            {tsl::midi::MidiNotes::midiToFreq(two), static_cast<MYFLOAT>(three),
                             STATUS_NOTE_ON,
                             tsl::time::nanosecondsSinceEpoch(), two});
                    }
                } else {
                    // A note-off must never be lost to a full queue — latch it for
                    // play2 to apply after its drain (see DATA::lostNoteOffs).
                    if (!_DATA->preQueue.push(
                            {tsl::midi::MidiNotes::midiToFreq(two), static_cast<MYFLOAT>(three),
                             STATUS_NOTE_OFF,
                             tsl::time::nanosecondsSinceEpoch(), two}))
                        _DATA->lostNoteOffs[(two >> 6) & 1].fetch_or(1ull << (two & 63),
                                                                     std::memory_order_release);
                }
            }
            break;

        case STATUS_NOTE_OFF:
            if (!_DATA->preQueue.push(
                    {tsl::midi::MidiNotes::midiToFreq(two), static_cast<MYFLOAT>(three),
                     STATUS_NOTE_OFF,
                     tsl::time::nanosecondsSinceEpoch(), two}))
                _DATA->lostNoteOffs[(two >> 6) & 1].fetch_or(1ull << (two & 63),
                                                             std::memory_order_release);
            break;

        case STATUS_POLYPHONIC_AFTERTOUCH:
            _DATA->preQueue.push({0, (MYFLOAT)three, STATUS_POLYPHONIC_AFTERTOUCH, 0, two});
            break;

        // Channel pressure (0xD0) is what most keyboards actually send — it used to
        // fall into default: and every AFTERTOUCH route was dead for them. It is a
        // TWO-byte message, so the pressure is `two`, not `three`. play2 fans it out
        // to every live voice; a poly-AT message arriving later for one note simply
        // overwrites that voice's snapshot, so poly wins per note when both stream.
        case STATUS_CHANNEL_PRESSURE:
            _DATA->preQueue.push({0, (MYFLOAT)two, STATUS_CHANNEL_PRESSURE, 0, 0});
            break;

        case STATUS_CONTROL_CHANGE:
            if (two == 1) { // mod wheel
                _DATA->modwheel.store((float)three, std::memory_order_relaxed);
                break;
            }
            if (two == 120 || two == 123) { // All Sound Off / All Notes Off
                // The recovery command must survive even the burst that filled the
                // queue; the latch degrades a lost 120 to a graceful release-all.
                if (!_DATA->preQueue.push({0, (MYFLOAT)three, STATUS_CONTROL_CHANGE, 0, two}))
                    _DATA->lostAllOff.store(true, std::memory_order_release);
                break;
            }
            if (_STATE->midilearning_waiting && _STATE->midiLearning.load() != nullptr) {
                auto ml = _STATE->midiLearning.load();
                if (!ml || _STATE->parameters[ml->id].type != ParameterType_double)
                    break;
                _STATE->activemidicommand[0] = one;
                _STATE->activemidicommand[1] = two;
                tsl::graphics::MidiLearning::setEventReceived(_STATE);
            } else {
                auto channel = (one & STATUS_CHANNEL_MASK);
                auto e = _STATE->midicontrolevents[channel][two];
                if (e.eventType == tsl::parameters::Eventtype::NoParam)
                    break;
                MYFLOAT value =
                    (e.midiState.min +
                    DISTANCE(e.midiState.min, e.midiState.max) * (MIDIDIVISOR * three)) * (1. / (double)UINT16_MAX);
                auto& m = _STATE->parameters[e.getDisplayParam()];
                value = m.fromNormalized(value);
                e.flags |= (tsl::parameters::Event::History | tsl::parameters::Event::Info | tsl::parameters::Event::Redraw);
                e.value = value;
                e.apply(_STATE, tsl::parameters::None);
            }
            break;


        default:
            break;
    }
}
