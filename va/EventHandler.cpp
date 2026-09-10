#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "gui.h"
#include "callbacks_loop_controls.h"
#include <cstring>
#include <cstdio>

using namespace tsl::parameters;

// Global OOM handler — registered by std::set_new_handler in android-main.cpp.
// (Added originally for the speex resampler's alloc loops; that resampler is gone,
// this is still the process-wide handler.)
void handler() { abort(); }

// Recording comment string required by player.cpp. Display metadata only, and it
// sits next to the album tag (tsl::app::appName), so the two must agree.
const char* tsl::app::recordingComment = "Made with Voltaic";

// ── Event special members ────────────────────────────────────────────────────

Event::~Event() {}

Event::Event(const Event& other) {
    std::memcpy(this, &other, sizeof(Event));
}

Event& Event::operator=(const Event& other) {
    if (this != &other)
        std::memcpy(this, &other, sizeof(Event));
    return *this;
}

Event::Event(Event&& other) noexcept {
    std::memcpy(this, &other, sizeof(Event));
}

Event& Event::operator=(Event&& other) noexcept {
    if (this != &other)
        std::memcpy(this, &other, sizeof(Event));
    return *this;
}

// ── setup ────────────────────────────────────────────────────────────────────

void Event::setup(tsl::AppState* _appState, int tindex, uint16_t id, uint16_t* displayParam, std::atomic<MYFLOAT>** ref) {
    if (id == PARAM_NOT_ASSIGNED) {
        paramIndex = id;
        eventType = Eventtype::NoParam;
        return;
    }

    flags = Redraw;
    if (!(_STATE->parameters[id].flags & Param::NoAssignment) && !(_STATE->parameters[id].flags & Param::NoValue))
        flags |= History;

    groupId = 0;
    trackIndex = tindex;

    // A handful of controls run a side effect instead of toggling a param value
    // (record / power / settings / midi-learn / clear-tasks / zero-note). Tag them
    // so apply() routes them to their action on a UI/MIDI gesture. Preset load and
    // history apply with a non-FromUi sender, so they only carry the value through.
    switch (id) {
    case POWERButton:
    case SETTINGSBUTTON:
    case RECORDButton:
    case MIDILEARNBUTTON:
    case CLEARTASKS:
    case ZERONOTE:
        paramIndex = id;
        eventType = Eventtype::SpecialAction;
        if (displayParam) *displayParam = id;
        if (ref) *ref = &_STATE->params[tindex][id];
        return;
    default:
        break;
    }

    eventType = Eventtype::paramUpdate;

    if (_STATE->parameters[id].paramOffset) {
        int offset = _STATE->params[tindex][_STATE->parameters[id].paramOffset].load();
        offset *= _STATE->parameters[id].offsetFact;
        paramIndex = id + offset;
    } else {
        paramIndex = id;
    }

    if (_STATE->parameters[id].flags & Param::HasOnChange) flags |= DoOnChange;
    if (displayParam) *displayParam = id;
    if (ref) *ref = &_STATE->params[tindex][paramIndex];
}

// ── getDefaultValue ──────────────────────────────────────────────────────────

MYFLOAT Event::getDefaultValue(tsl::AppState* _appState) const {
    return _STATE->parameters[paramIndex].initvalue;
}

// ── getPluginIndex ───────────────────────────────────────────────────────────

#ifdef PLUGIN_MODE
#include <IPlugParamDefs.h>
int Event::getPluginIndex(tsl::AppState* _appState) const {
    if (eventType == Eventtype::paramUpdate)
        return _STATE->parameters[paramIndex].pluginIndex + trackIndex * tsl::iplug::paramCount;
    return -1;
}
#endif

// ── getCurrentValue ──────────────────────────────────────────────────────────

MYFLOAT Event::getCurrentValue(tsl::AppState* _appState) const {
    switch (eventType) {
    case Eventtype::SpecialAction:
    case Eventtype::paramUpdate:
    default:
        return _STATE->params[trackIndex][paramIndex].load();
    }
}

// ── getDisplayParam ──────────────────────────────────────────────────────────

uint16_t Event::getDisplayParam() const {
    switch (eventType) {
    case paramUpdate:
    case SpecialAction:
        return paramIndex;
    default:
        return 0;
    }
}

// ── redrawEvent ──────────────────────────────────────────────────────────────

void tsl::parameters::redrawEvent(tsl::AppState* _appState, Event& e, uint16_t viewid) {
    if (e.flags & tsl::parameters::Event::Redraw) {
        _STATE->toUiThreadQueue.try_push([_STATE, e, viewid]() mutable {
            auto v = _STATE->parameters[viewid].view;
            if (v != nullptr && v->visible_) v->redraw();
        });
    }
    if (e.flags & tsl::parameters::Event::Info) {
        _STATE->toUiThreadQueue.try_push([_STATE, e]() mutable {
            auto info = static_cast<tsl::graphics::InfoPanel*>(_DATA->views.infopanel);
            info->prepareBuffer(const_cast<Event&>(e));
        });
    }
}

// ── afterChange ──────────────────────────────────────────────────────────────

void Event::afterChange(tsl::AppState* _appState) {
    // PA has no params with HasAfterChange — nothing to do
}

// ── LFO window/write-through ─────────────────────────────────────────────────
// The DEST/DEPTH pair of each LFO is a WINDOW onto the multi-dest matrix: DEST is
// the cursor, DEPTH shows and edits matrix[DEST]. Hooked HERE — Event::apply is
// the one funnel every change source shares (knob, selector, MIDI learn, host
// automation, undo) — and deliberately NOT on the audio thread: the first version
// lived in synthFunc and the window went dead the moment audio wasn't processing
// (ear-reported: switching DEST stopped re-seating the DEPTH knob).
//   DEST moved  -> window refreshed from the newly selected slot + depth knob
//                  repaint. Plain stores, no Event: browsing must neither edit
//                  the patch nor spam host automation.
//   DEPTH moved -> written through into the selected slot. Keeps old DAW
//                  automation lanes on LFOnDEPTH working: they edit whatever
//                  route the stored DEST points at.
static void lfoWindowHook(tsl::AppState* _appState, int pid, MYFLOAT v) {
    static const int destIds[4]  = {LFO1DEST,  LFO2DEST,  LFO3DEST,  LFO4DEST};
    static const int depthIds[4] = {LFO1DEPTH, LFO2DEPTH, LFO3DEPTH, LFO4DEPTH};
    for (int n = 0; n < 4; n++) {
        if (pid == destIds[n]) {
            const int dest = (int)v;
            const MYFLOAT w = (dest >= 1 && dest <= LFO_MD_NDEST)
                              ? _STATE->params[0][lfoMdId(n, dest)].load() : 0.;
            _STATE->params[0][depthIds[n]].store(w);
            const int did = depthIds[n];
            _STATE->toUiThreadQueue.try_push([_STATE, did]() {
                auto* kv = _STATE->parameters[did].view;
                if (kv != nullptr && kv->visible_) kv->redraw();
            });
            return;
        }
        if (pid == depthIds[n]) {
            const int dest = (int)_STATE->params[0][destIds[n]].load();
            if (dest >= 1 && dest <= LFO_MD_NDEST)
                _STATE->params[0][lfoMdId(n, dest)].store(v);
            return;
        }
    }
}

// ── apply ────────────────────────────────────────────────────────────────────

Event Event::apply(tsl::AppState* _appState, tsl::parameters::SenderFlags from) {
    // Special actions (tagged in setup) run their side effect on a direct UI gesture.
    // Preset load / history apply with other senders, so the action is skipped and the
    // param value is simply left as stored — mirroring grainstorm's SpecialAction path.
    if (eventType == Eventtype::SpecialAction) {
        if (from == tsl::parameters::FromUi)
            applySpecialAction(_STATE, paramIndex);
        return *this;
    }
    auto& param = _STATE->parameters[paramIndex];
    auto* ref = &_STATE->params[trackIndex][paramIndex];
    auto e = *this;
    e.value = ref->exchange(value);
    if (e.value != value) {
        if (_STATE->onParamChange) _STATE->onParamChange(*this, from);
        redrawEvent(_STATE, *this, paramIndex);
        // redrawEvent only repaints the param's own view. A mod source changes the
        // range drawn on OTHER knobs without moving their values, so those have to
        // be invalidated separately or they keep showing the previous range.
        if (tsl::app::isModSource(paramIndex))
            tsl::app::redrawModTargets(_STATE);
        lfoWindowHook(_STATE, paramIndex, value);
        if (param.flags & Param::HasAfterChange)
            afterChange(_STATE);
    }
    return e;
}

// ── applyFromExt ─────────────────────────────────────────────────────────────

void Event::applyFromExt(tsl::AppState* _appState, tsl::parameters::SenderFlags from) {
    apply(_STATE, from);
}

// ── toString ─────────────────────────────────────────────────────────────────

void Event::toString(tsl::AppState* _appState, int& pos, char buffer[], size_t bufferSize, bool renderValue) {
    auto& miditarget = _STATE->parameters[paramIndex];
    if (miditarget.category) {
        pos += snprintf(buffer + pos, bufferSize - pos, "%s ", miditarget.category);
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (miditarget.subcategory) {
        pos += snprintf(buffer + pos, bufferSize - pos, "%s ", miditarget.subcategory);
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (miditarget.name) {
        pos += snprintf(buffer + pos, bufferSize - pos, "%s", miditarget.name);
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (renderValue) {
        auto display = miditarget.toDisplay(_STATE->sr, value);
        if (miditarget.type == ParameterType_enum && (int)display < (int)miditarget.names.size()) {
            pos += snprintf(buffer + pos, bufferSize - pos, ": %s", std::string(miditarget.names[(int)display]).c_str());
        } else if (miditarget.type == ParameterType_bool) {
            if (!miditarget.names.empty() && (int)display < (int)miditarget.names.size())
                pos += snprintf(buffer + pos, bufferSize - pos, ": %s", std::string(miditarget.names[(int)display]).c_str());
            else
                pos += snprintf(buffer + pos, bufferSize - pos, ": %s", display == 0. ? "OFF" : "ON");
        } else {
            pos += snprintf(buffer + pos, bufferSize - pos, ": %.*f", miditarget.digits, display);
        }
        pos = std::min(pos, (int)bufferSize - 1);
    }
}
