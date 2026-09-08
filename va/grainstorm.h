#ifndef GRAINSTORM_H
#define GRAINSTORM_H

#include <atomic>
#include <vector>
#include <thread>
#include "types.h"
#include "defines.h"
#include "Queue.h"
#include "tools.h"
#include "player.h"
#include "infopanel.h"
#include <params.h>
#include <app.h>

#ifdef IS_SINGLETON
extern tsl::AppState* __STATE;
#endif

namespace tsl{namespace graphics{
    class PresetSelector;
}}

struct ViewS {
    tsl::graphics::View *root_window;
    tsl::graphics::View *toppanel_main;
    tsl::graphics::InfoPanel *infopanel;
    tsl::graphics::View *loadInfo;
    tsl::graphics::View *controlpanel;
    tsl::graphics::View *button;
    tsl::graphics::View *midilearnbutton;
    tsl::graphics::View *decoderView;
    tsl::graphics::View *recorderView;
    tsl::graphics::View *presetSelector;
};

#include "va_types.h"
#include "preset.h"
#include <envelope.h>
using ToAudioThreadQueue = LockFreeQueue<std::function<void()>, 4>;

struct DATA {
    tsl::AppState* _appState{};
    explicit DATA(tsl::AppState* s);

    uint64_t offset{};
    std::atomic<bool> pressed{};
    double time{};
    std::atomic<uint32_t> currentrecoff{};
    double sinewave[WINDOW_SIZE+2]{};
#ifndef AUDIO_NO_THREADS
    std::thread synth_main_thread;
#endif
    ViewS views{};
    int panelheight{};
    std::atomic<bool> hqresampling{};
    bool startPoweredOn{};
    uint32_t mask{};
    uint32_t lobits{};
    double pfrac{};
    std::atomic<int> activeSeg{-1};
    // MPSC: pushed from the MIDI delivery thread (JNI on Android, the host's audio
    // thread in the plugin) AND the UI thread (on-screen piano, ZERONOTE), drained by
    // play2 on the audio thread. 64 deep: poly aftertouch shares this queue at one
    // push per pressure message, so a 16-slot queue left almost no headroom for the
    // note-offs of a dense chord inside one host block — and a lost note-off is a
    // stuck note. The latches below catch the events that must survive even a full
    // queue; everything else (a note-on, a pressure update) is honest to drop.
    MpscQueue<VCOPreEvent, 64> preQueue{};
    // Set by a producer when preQueue.push() of a NOTE_OFF fails, one bit per MIDI
    // note; play2 applies and clears them right after its drain, so the worst case is
    // a release arriving one block late instead of never. lostAllOff is the same
    // latch for CC 120/123 — All Sound/Notes Off travels through the same queue, so
    // without this the recovery command could itself be the dropped event.
    std::atomic<uint64_t> lostNoteOffs[2]{};
    std::atomic<bool> lostAllOff{false};
    std::atomic<float> modwheel{0.f};
    std::atomic<float> lastPolyAT{0.f};
    int64_t presetDate{};
    ToAudioThreadQueue toAudioThreadQueue{};
    std::atomic<bool> integrity_failed{false};

    // sequencer state (was file-static in sequencer.cpp)
    int seq_currentnote{}, seq_stepforward{}, seq_arpstep{}, seq_oldarpcycles{},
        seq_newarpcycles{}, seq_oldarpcyclemode{}, seq_newarpcyclemode{},
        seq_arpcycledir{1}, seq_count{}, seq_nextcount{}, seq_stepsold{},
        seq_swingstep{}, seq_learnmode{};
    MYFLOAT seq_swingval[2]{1, 1};
    std::vector<MYFLOAT> seq_pitchfacts;
    bool seq_waitforzero{}, seq_arp{}, seq_oldarp{}, seq_notesettingsfromsynth{};
    std::atomic<bool> seq_backw{};
    tsl::FastQueue<SeqPreEvent> seq_prequeue{250};
    tsl::FastQueue<VcoPreNote> seq_noteon{132};
    tsl::AlignedVector<VcoPreNote> seq_notes;
    VcoPreNote seq_dummynote;

    // preset state (was file-static in preset.cpp)
    std::deque<Preset::Preset> presets;

    // synth state (was file/function-level static in synth.cpp)
    tsl::envelope::Window<MYFLOAT> synth_tukey{WINDOW_SIZE, tsl::envelope::wtTUKEY, .5};
    tsl::envelope::Window<MYFLOAT> synth_hann{WINDOW_SIZE, tsl::envelope::wtHANNING};
    MYFLOAT onedtwosr{1. / (2. * 48000.)};
    MYFLOAT* tmpbuf{};
    MYFLOAT synth_params[NUM_PARAMS]{};
    unsigned int currentoff{};
    int synth_bb{};
    int64_t synth_diffprint{};
    MYFLOAT synth_smoothed{};
    // CDELPOW/REVPOW on-off crossfade (0=fully dry/bypassed, 1=fully wet), so
    // toggling doesn't hard-cut a live tail or slam a stale one back in.
    MYFLOAT synth_delayFade{}, synth_reverbFade{};
    bool synth_delayNeedsReset{}, synth_reverbNeedsReset{};
    tsl::AlignedVector<MYFLOAT> synth_dryl, synth_dryr;
    tsl::AlignedVector<MYFLOAT> synth_bufl, synth_bufr;
    // complex synth objects — class definitions live in synth.cpp, initialized lazily in synthFunc
    std::vector<MYFLOAT> coeffsUp, coeffsDown;
    std::unique_ptr<void, void(*)(void*)> synthQueue_obj{nullptr, [](void*){}};
    std::unique_ptr<void, void(*)(void*)> progenitor1Dark_obj{nullptr, [](void*){}};
    std::unique_ptr<void, void(*)(void*)> delayEffect_obj{nullptr, [](void*){}};
    bool isRunningAsPlugin{};
    std::atomic_bool isRestoringState{};

    struct HostTimeSnapshot {
        bool   running = false;
        bool   wasRunning = false;
        double bpm = 120.0;
        double ppqNow = 0.0;
        double beatsPerBar = 4.0;
        double lastPpqPos = 0.0;
        bool   loopEnabled = false;
        double loopStartPPQ = 0.0;
        double loopEndPPQ = 0.0;
        double triggerPPQ = 0.0;
    } hostTimeSnapshot{};
};


#endif
