#pragma once
#include "defines.h"
namespace tsl { struct AppState; }

struct VCOPreEvent {
    double cps;
    MYFLOAT keyvel;
    int type;
    int64_t time;
    uint8_t noteNum{};
};

struct VcoNote;

// LAYOUT IS FROZEN. Sequencer notes are fwrite'd raw into preset files
// (`fwrite(notes.data(), sizeof(VcoPreNote), ...)` in preset.cpp) and read back the
// same way, with no version tag and no migration — so changing the size, the order or
// the type of ANY member here silently reinterprets every preset ever saved with
// notes in it.
//
// A per-note value added later therefore cannot go in this struct. Read it live from
// synth_params in VcoNote::operator= instead; WTPOS, NOISEMODE, VCO3AM and the
// resonance EG source all do exactly that, each with a comment saying why.
struct VcoPreNote {
    VcoPreNote() = default;
    void init(tsl::AppState* appState, MYFLOAT freq, int64_t time, MYFLOAT vel = 127.f, uint8_t noteNum = 0);
    MYFLOAT _freq{}, _finalfreq{}, _tune1{}, _tune2{}, _tunenormal{}, _gain{}, _velnorm{1.f}, _jittera{}, _jitterb{}, _jittercents{}, egvals[16]{}, phaserrange{}, phaserfb{}, phaserrate{};
    float _atSnapshot{};
    bool _suppressAT{false};   // frozen placeholder, never read anywhere — layout
                               // padding only; do not repurpose without a
                               // preset-format version bump
    uint8_t _noteNum{};
    int64_t _time{}, _finaltime{};
    MYFLOAT _pws[3]{};
    MYFLOAT gains[5]{};
    // EIGHT, not nine: VCO1/2/3 EG, NOISEEG, FILTEG, VCO1/2/3 TUNEEG. VcoNote::egs
    // has a ninth slot for the resonance EG, which is why this looks short — that one
    // is read live at note adoption. Do not grow this array; see the note above.
    int egs[8]{};
    MYFLOAT _filtres{};
    int waveform[3]{};
    bool _doringmod{};
    bool _donormal{};
    bool _phaseractive{};
    bool _sync[3]{};
};

struct SeqPreEvent {
    VCOPreEvent event;
    int currentnote;
};
