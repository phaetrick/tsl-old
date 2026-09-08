#pragma once
//
// Created by pr on 30.09.19.
//

#ifndef GRAINSTORM_CONTROLITEM_H
#define GRAINSTORM_CONTROLITEM_H


#include <atomic>
#include <vector>
#include <cstdint>
#include "params.h"
struct TRACK;

namespace tsl {
    struct AppState;
}
namespace tsl::Syncing {
    using SyncResult = uint32_t;
    enum SyncFlags : uint32_t {
        Ok = 1<<0,
        MinReached = 1 << 1,
        MaxReached = 1 << 2,
        LoopTooShort = 1 << 3,
        UnChanged = 1 << 4,
        OutOfRange = 1 << 5,
        NotImplemented = 1 << 6
    };
    
};

struct SyncTarget{
    SyncTarget(tsl::AppState*, const char *_syncName, TRACK *_parent, uint16_t syncingid, uint16_t factid);
    virtual tsl::Syncing::SyncResult syncBySamples(double, std::vector<tsl::parameters::Event>&) = 0;
    virtual double getSamples() = 0;
    virtual tsl::Syncing::SyncResult control(uint16_t which, std::vector <tsl::parameters::Event>&) = 0;
    void controlAll(uint16_t which);
    [[nodiscard]] bool syncing() const;
    [[nodiscard]] double syncFact() const;
    void sync();
    TRACK *parent{};
    uint16_t syncid, factid{};
    const char *syncName;
};



#endif //GRAINSTORM_CONTROLITEM_H
