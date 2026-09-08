#pragma once
//
// Created by pr on 19.10.20.
//

#ifndef GRAINSTORM_SEQUENCER_H
#define GRAINSTORM_SEQUENCER_H
#include <atomic>
#include <defines.h>
#include "view.h"
#include "ControlItem.h"
#include <tools/RingBufferQueue.h>
struct TRACK;

namespace tsl::graphics {
    class SequenceTicker : public View {
    public:
        SequenceTicker(tsl::AppState*, int32_t alignment, int id, Layout *parent = nullptr);
        void render(void *ctx)override ;
};     }


class grainsequencer : public SyncTarget{
public:
    grainsequencer(tsl::AppState *appState, const char *n, TRACK *t, int32_t a, int b) : _appState(appState), SyncTarget(appState, n, t, a, b){};
    void init(TRACK *t);
    void check();
    void tick (int32_t channel, MYFLOAT &gain, MYFLOAT &pitch, MYFLOAT &size);
    tsl::Syncing::SyncResult syncBySamples(double, std::vector<tsl::parameters::Event>&) override;
    double getSamples() override;
    tsl::Syncing::SyncResult control(uint16_t which, std::vector<tsl::parameters::Event>&) override;
    void pushLoad(int32_t which);
    void pushSave(int32_t which);
    void setState(int32_t steppoint, int cycledir, int grains, int silence){
        arpstep[0] = arpstep[1] = steppoint;
        arpcycledir[0] = arpcycledir[1] = cycledir;
        grainscount[0] = grainscount[1] = grains;
        silencecount[0] = silencecount[1] = silence;
    }
    
    tsl::parameters::Event getState();
    void setState(tsl::parameters::Event&);


    void arpReset(){
        arpstep[0] = arpstep[1] = 0;
        arpcycledir[0] = arpcycledir[1] = 1;
    }

    enum LoadParams{
        SEQSTEPS = 0,
        SEQGRAINS,
        SEQGRAINSCOUNT,
        SEQSILENCE,
        SEQSILENCECOUNT,
        SEQMODE,
        SEQINT,
        SEQARPCYCLES,
        SEQARPMODE,
        SEQARPPOW,
        NUM_LOADPARAMS
    };
private:
    tsl::AppState* _appState;
    tsl::RingBufferMPSCQueue<int,8>loadQueue[2]{}, saveQueue{};
    std::atomic<MYFLOAT> *_pitch[16]{}, *_gain[16]{}, *_size[16]{}, *_steps{}, *_currentnotedisp{}, *_dir{};
    MYFLOAT _pitchold[2][16]{}, _gainold[2][16]{}, _sizeold[2][16]{};
    int32_t  _stepsold[2]{}, _dirold{};
    int32_t _currentnote[2]{};
    TRACK *track{};
    int32_t grainscount[2]{}, silencecount[2]{}, grainstocompute[2]{}, silencetocompute[2]{};
    bool arp[2]{};
    int32_t arpstep[2]{}, oldarpcycles[2]{}, newarpcycles{}, oldarpcyclemode[2]{}, newarpcyclemode{},arpcycledir[2]{1,1};
    MYFLOAT oldinterval{-1};
    std::vector<MYFLOAT>pitchfacts[2]{};
};


#endif //GRAINSTORM_SEQUENCER_H
