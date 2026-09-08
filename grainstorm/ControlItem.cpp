//
// Created by pr on 30.09.19.
//

#include "ControlItem.h"
#include "grainstorm.h"
#include <atomic>
#include <app.h>
#include "logger.h"
#include "params.h"
#include "history.h"

using namespace tsl::parameters;
using namespace tsl::Syncing;

SyncTarget::SyncTarget(tsl::AppState *_appState, const char *_syncName, TRACK *_parent, uint16_t syncing, uint16_t fact) :syncName(_syncName), parent(_parent), syncid(syncing), factid(fact) {
    _STATE->syncTargets.push_back(this);
};

static constexpr const char* returnValues[]{ "OK", "MIN REACHED", "MAX REACHED", "LOOP TOO SHORT", "NOT CHANGED",
                                  "OUT OF RANGE" };

void toString(int& pos, char buffer[], size_t bufferSize, const char* trackName, const char* name, SyncResult flags) {
    pos += snprintf(buffer + pos, bufferSize - pos, "%s %s", trackName, name);
    pos = std::min(pos, (int)bufferSize - 1);
    if (flags & SyncFlags::MinReached) {
        pos += snprintf(buffer + pos, bufferSize - pos, " MIN REACHED");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::MaxReached) {
        pos += snprintf(buffer + pos, bufferSize - pos, " MAX REACHED");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::LoopTooShort) {
        pos += snprintf(buffer + pos, bufferSize - pos, " EMPTY");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::NotImplemented) {
        pos += snprintf(buffer + pos, bufferSize - pos, " NOT IMPLEMENTED");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::OutOfRange) {
        pos += snprintf(buffer + pos, bufferSize - pos, " OUT OF RANGE");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::UnChanged) {
        pos += snprintf(buffer + pos, bufferSize - pos, " UNCHANGED");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    if (flags & SyncFlags::Ok) {
        pos += snprintf(buffer + pos, bufferSize - pos, " OK");
        pos = std::min(pos, (int)bufferSize - 1);
    }
    pos += snprintf(buffer + pos, bufferSize - pos, "\n");
    pos = std::min(pos, (int)bufferSize - 1);
    return;
}

void SyncTarget::controlAll(uint16_t which) {

    char str[2000]{};
    str[0] = '\0';
    std::vector<Event> ev;

    auto ret = control(which, ev);
    int pos = 0;
    if (!(ret & SyncFlags::Ok)) {        
        toString(pos, str, 2000, parent->name, syncName, ret);
    }
    else if (syncing()) {
        for (auto target : parent->_STATE->syncTargets) {
            if (target->syncing() && target != this) {
                ret = target->control(which, ev);
                if (!(ret & SyncFlags::NotImplemented))
                    toString(pos, str, 2000, target->parent->name, target->syncName, ret);
            }
        }
    }

    if (ev.size()>0) {
        // Label the undo/redo group with the initiating action ("DELAY VEL DOWN"),
        // not whichever member param happens to be applied. Appended last so its
        // info-panel update wins (group events are applied in order).
        auto ne = Event::createEvent(parent->index, Eventtype::TextEvent, which, 0.0, EventSubtype::syncEvent);
        ne.text = syncName;
        ev.push_back(ne);
        auto _appState = parent->_appState;
        
        _DATA->snapShot.add_task([_STATE, events = std::move(ev)] mutable {
            std::lock_guard lk(_DATA->snapShot);

            auto groupId = _DATA->snapShot.nextGroupId();
            for (auto& e : events) {
                e.groupId = groupId;
                e.flags = Event::EventFlags::Redraw | Event::EventFlags::ToAudioThread | Event::EventFlags::History;
                _DATA->snapShot.addEvent(e);
            }
            });
    }

    if (pos > 0) {
        showToast(parent->_appState, str);
    }
}

void SyncTarget::sync() {
    if (!syncing())
        return;

    auto smpls = getSamples();

    if (smpls == 0)
        return;


    char str[2000]{};

    std::vector<Event> ev;

    auto _appState = parent->_appState;

    int pos = 0;

    for (auto target : parent->_STATE->syncTargets) {
        if (target->syncing() && target != this) {
            auto ret = target->syncBySamples(smpls, ev);
            toString(pos, str, 2000, target->parent->name, target->syncName, ret);
        }
    }

    if (ev.size()>0) {
        // Label the undo/redo group with the initiating sync action.
        auto ne = Event::createEvent(parent->index, Eventtype::TextEvent, SYNCButton, 0.0, EventSubtype::syncEvent);
        ne.text = syncName;
        ev.push_back(ne);
        
        _DATA->snapShot.add_task([_STATE, events = std::move(ev)] mutable {
            std::lock_guard lk(_DATA->snapShot);

            auto groupId = _DATA->snapShot.nextGroupId();
            for (auto& e : events) {
                e.groupId = groupId;
                e.flags = Event::EventFlags::Redraw | Event::EventFlags::ToAudioThread | Event::EventFlags::History;
                _DATA->snapShot.addEvent(e);
            }
            });
    }


    if (pos>0) {
        showToast(parent->_appState, str);
    }
}

bool SyncTarget::syncing() const {
    return parent->_STATE->params[parent->index][syncid].load() == 1.0;
};

double SyncTarget::syncFact() const {
    return parent->_STATE->params[parent->index][factid].load();
};




