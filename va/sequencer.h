//
// Created by pr on 03.10.23.
//

#ifndef POCKET_ANALOG_SEQUENCER_H
#define POCKET_ANALOG_SEQUENCER_H
#include <defines.h>
#include <vector>
#include "grainstorm.h"
#define NUM_ARP_STEPS 32
#include "synth.h"
namespace sequencer {
    extern LockFreeQueue<VCOPreEvent, 64> preQueue;
    void savePr(tsl::AppState*);
    void loadPreset(tsl::AppState*, int preset);
    void tick(tsl::AppState*, VcoPreNote**);
    void check(tsl::AppState*);
    void init(tsl::AppState*);
};

#endif //POCKET_ANALOG_SEQUENCER_H
