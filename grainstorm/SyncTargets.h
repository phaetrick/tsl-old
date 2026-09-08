#pragma once
//
// Created by pr on 10.11.23.
//

#ifndef GRAINSTORM_SYNCTARGETS_H
#define GRAINSTORM_SYNCTARGETS_H


#include "ControlItem.h"

struct DelaySyncTarget : SyncTarget{
    DelaySyncTarget(tsl::AppState *appState, const char *name, TRACK *_parent, int32_t _sync, int _fact, int _delayid) : SyncTarget(appState, name, _parent,  _sync, _fact){
        delayid = _delayid;
    }
    tsl::Syncing::SyncResult syncBySamples(double, std::vector<tsl::parameters::Event> &)override;
    double getSamples()override;
    tsl::Syncing::SyncResult control(uint16_t which, std::vector<tsl::parameters::Event>&)override;
    int32_t delayid{};
};

struct BPMSyncTarget : SyncTarget{
    BPMSyncTarget(tsl::AppState *appState, const char *name, TRACK *_parent, int32_t _sync, int _fact) : SyncTarget(appState, name, _parent,  _sync, _fact){
    }
    tsl::Syncing::SyncResult syncBySamples(double, std::vector<tsl::parameters::Event>&)override;
    double getSamples()override;
    tsl::Syncing::SyncResult control(uint16_t which, std::vector<tsl::parameters::Event>&)override;
    static constexpr double minBPM = 0.06;
    static constexpr double maxBPM = 60 * 500;


};

#endif //GRAINSTORM_SYNCTARGETS_H
