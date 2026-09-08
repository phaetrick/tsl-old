#include "preset.h"
#include "grainstorm.h"
#include "gui.h"
#include "defines.h"
#include "button.h"
#include "buttonview.h"
#include "tools.h"
#include "envelope.h"
#include <logger.h>
#include "synth.h"
#include "queue.h"
#include "view.h"
#include "Input.h"
#include <sstream>
#include <algorithm>
#include <dirent.h>
#include <logger.h>

struct PresetHeader {
    char header[11]{"TSLVASYNTH"};
    int version{4};
    int64_t date{};  // fixed-width: plain `long` is 4 bytes on Windows (LLP64) vs
                      // 8 bytes on macOS/iOS/Android (LP64) — would misalign every
                      // field after it when reading a preset saved on the other kind
                      // of platform. int64_t is 8 bytes everywhere.
    int namelen{};
    int numparams{};
};

// Old v1/v2 MIDI assignment struct — used only for skipping legacy bytes safely
struct LegacyMidiAssignment {
    uint8_t trackindex{};
    uint16_t miditarget{};
    double min{}, max{};
    uint8_t active_space_lfo{};
    uint8_t active_space_windows{};
    uint8_t active_space_envf{};
    uint8_t active_space_mdelay{};
    uint8_t active_space_granulation{};
    uint8_t active_space_fx{};
    uint8_t active_space_stereo_fx{};
    uint8_t active_space_cross{};
    uint8_t active_space_pv{};
    uint8_t active_space_main{};
    uint8_t dummy2{}, dummy3{}, dummy4{}, dummy5{};
    uint8_t dummy6{}, dummy7{}, dummy8{}, dummy9{}, dummy10{};
};
struct LegacyPresetMidiAssignment {
    uint8_t type{};
    uint8_t channel{};
    uint8_t num{};
    LegacyMidiAssignment midiAssignment{};
};

static const char *headername = "TSLVASYNTH";
static const int hasNotesCode = 13031974;
static const int hasMidiEventsCode = 20250629;

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <tools/PlatformPaths.h>
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#endif

bool Preset::contains(tsl::AppState* _appState, int64_t date) {
    for (auto& p : _DATA->presets)
        if (p.date == date)
            return true;
    return false;
}

bool Preset::get(tsl::AppState* _appState, Preset& preset, int64_t date) {
    for (auto& p : _DATA->presets) {
        if (p.date == date) {
            preset = p;
            return true;
        }
    }
    return false;
}

namespace fs = std::filesystem;

static bool readOnePreset(tsl::AppState* _appState, FILE* fd, const std::string& path) {
    PresetHeader header{};
    if (fread(&header, 1, sizeof(PresetHeader), fd) != sizeof(PresetHeader))
        return false;
    if (strcmp(header.header, headername) != 0)
        return false;
    if (Preset::contains(_appState, header.date))
        return false;
    // File-sourced counts drive allocations and reads below — a corrupt or truncated
    // file must fail here, not crash at startup. Bounds are generous: names are a few
    // dozen chars, and numparams can never legitimately exceed a future NUM_PARAMS by
    // more than a version's worth of growth.
    if (header.namelen < 0 || header.namelen > 4096 ||
        header.numparams < 0 || header.numparams > 65536)
        return false;

    std::vector<char> namebuf(header.namelen + 1, '\0');
    if (fread(namebuf.data(), header.namelen, 1, fd) != 1)
        return false;
    std::string n(namebuf.data());

    std::vector<Preset::PresetParam> vals;
    if (header.version == 1) {
        std::vector<float> tmp(header.numparams);
        if (fread(tmp.data(), sizeof(float), tmp.size(), fd) != (size_t)header.numparams)
            return false;
        // A v1 file wider than this build's parameter table would read initvalue past
        // the end of parameters[] — the extra entries have no meaning here anyway.
        const int nCompare = std::min((int)tmp.size(), (int)NUM_PARAMS);
        for (int i = 0; i < nCompare; i++) {
            if (tmp[i] != static_cast<float>(_STATE->parameters[i].initvalue))
                vals.push_back({i, tmp[i]});
        }
    } else {
        vals.resize(header.numparams);
        if (fread(vals.data(), sizeof(Preset::PresetParam), vals.size(), fd) != (size_t)header.numparams)
            return false;
        // Ids are applied as raw params[0] indices at load — drop anything outside
        // this build's table (same guard as unserializeState). This is also what makes
        // a preset from a NEWER build load cleanly: its unknown ids are skipped
        // instead of writing out of bounds. 0 is the reserved slot, never stored.
        vals.erase(std::remove_if(vals.begin(), vals.end(),
                                  [](const Preset::PresetParam& p) {
                                      return p.num <= 0 || p.num >= NUM_PARAMS;
                                  }),
                   vals.end());
    }

    // FILTEG's initvalue changed from 0 (EG1) to -1 (NONE) at version 4. Presets are
    // stored sparsely against initvalue, so a v<=3 file that simply left the filter
    // envelope alone has no FILTEG entry and would now load with it OFF — a silent
    // change to a patch the user already saved. Write the old default back in.
    if (header.version <= 3) {
        bool hasFiltEg = false;
        for (const auto& v : vals)
            if (v.num == FILTEG) { hasFiltEg = true; break; }
        if (!hasFiltEg)
            vals.push_back({(uint16_t)FILTEG, 0.f});
    }

    auto offset = ftell(fd);
    int num{};
    std::vector<VcoPreNote> notes;
    if (fread(&num, sizeof(int), 1, fd) == 1) {
        if (num == hasNotesCode) {
            if (fread(&num, sizeof(int), 1, fd) != 1)
                return false;
            if (num < 0 || num > 10000)   // same style of cap as the midi-events count
                return false;
            notes.resize(num);
            if (fread(notes.data(), sizeof(VcoPreNote), notes.size(), fd) != (size_t)num)
                return false;
        } else {
            fseek(fd, offset, SEEK_SET);
        }
    }

    // MIDI assignments
    std::vector<tsl::parameters::Event> midiEvents;
    if (header.version <= 2) {
        // Old format: drain remaining LegacyPresetMidiAssignment structs — convert to Events
        LegacyPresetMidiAssignment lma;
        while (fread(&lma, sizeof(LegacyPresetMidiAssignment), 1, fd) == 1) {
            if (lma.midiAssignment.miditarget == 0 || lma.midiAssignment.miditarget >= NUM_PARAMS)
                continue;
            tsl::parameters::Event e;
            e.setup(_STATE, 0, lma.midiAssignment.miditarget);
            auto& m = _STATE->parameters[lma.midiAssignment.miditarget];
            e.midiState.channel = lma.channel;
            e.midiState.num = lma.num;
            if (lma.type == 0) {
                e.midiState.type = ParameterType_double;
                auto& m = _STATE->parameters[lma.midiAssignment.miditarget];
                e.midiState.min = (uint16_t)(std::clamp(m.toNormalized(lma.midiAssignment.min), 0., 1.) * UINT16_MAX);
                e.midiState.max = (uint16_t)(std::clamp(m.toNormalized(lma.midiAssignment.max), 0., 1.) * UINT16_MAX);
            } else {
                e.midiState.type = ParameterType_bool;
                e.midiState.inc = 1;
            }
            midiEvents.push_back(e);
        }
    } else {
        // New format: optional hasMidiEventsCode marker + count + Events
        int marker{};
        auto markerOffset = ftell(fd);
        if (fread(&marker, sizeof(int), 1, fd) == 1 && marker == hasMidiEventsCode) {
            int count{};
            if (fread(&count, sizeof(int), 1, fd) == 1 && count > 0 && count < 10000) {
                for (int i = 0; i < count; i++) {
                    tsl::parameters::Event e{};
                    if (fread(&e, sizeof(tsl::parameters::Event), 1, fd) != 1) break;
                    midiEvents.push_back(e);
                }
            }
        } else {
            fseek(fd, markerOffset, SEEK_SET);
        }
    }

    _DATA->presets.push_back({n, path, header.date, false, vals, notes, midiEvents});
    return true;
}

void Preset::readPresets(tsl::AppState* _appState) {
    std::string dir = tsl::app::getStoragePath("presets");
    if (dir.empty()) return;
#ifdef __ANDROID__
    DIR* d = opendir(dir.c_str());
    if (!d) { LOGE("readPresets: cannot open dir %s", dir.c_str()); return; }
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_type != DT_REG) continue;
        std::string path = dir + "/" + ent->d_name;
        FILE* fd = fopen(path.c_str(), "rb");
        if (!fd) { LOGE("readPresets: cannot open %s", path.c_str()); continue; }
        readOnePreset(_appState, fd, path);
        fclose(fd);
    }
    closedir(d);
#else
    if (!fs::exists(dir)) return;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        FILE* fd = fopen(entry.path().string().c_str(), "rb");
        if (!fd) { LOGE("readPresets: cannot open %s", entry.path().string().c_str()); continue; }
        readOnePreset(_appState, fd, entry.path().string());
        fclose(fd);
    }
#endif
    if (!_DATA->presets.empty()) {
        std::sort(_DATA->presets.begin(), _DATA->presets.end(), [](const Preset& a, const Preset& b) {
            return a.date > b.date;
        });
    }
};

void Preset::setupDefault(tsl::AppState* _appState) {
    Preset preset{};
    std::vector<PresetParam> values;
    // From 1: index 0 is PARAM_NOT_ASSIGNED, the reserved slot no preset stores —
    // the same convention unserializeState and the file-read guard enforce.
    for (int i = 1; i < NUM_PARAMS; i++)
        values.push_back({i, static_cast<float>(_STATE->parameters[i].initvalue)});
    _DATA->presets.push_back({"Default", "null", LONG_MAX, true, values});
    _DATA->presetDate = LONG_MAX;
};

// =============================================================================
// 40 baked factory presets, initialized at startup like Default. Dates run
// LONG_MAX-1 downward so the date-descending sort places them directly below
// Default and above every user preset; isSystem=true keeps them out of the
// save/overwrite list and undeletable. loadPreset resets all params to
// initvalue first, so each preset lists only its deviations.
//
// No effects and no sequencer anywhere in the bank, by design: these exist to
// demonstrate the synthesis engine itself, so nothing is propped up by reverb.
//
// ORDER HERE IS THE UI ORDER. The dates below count down from LONG_MAX-1 in
// declaration order and the selector sorts date-descending, so these sections are
// exactly what the user scrolls past. Grouped by what a preset SOUNDS like rather
// than by which engine feature it happens to demonstrate:
//   4 pads / 3 keys & organs / 4 bells & mallets / 3 plucks / 4 leads / 4 bass /
//   2 winds & voices / 2 textures & fx = 26, the shipping selection.
// Cut from 42 for release: one preset per engine feature, no two demonstrating the
// same trick. Everything removed is in git history (see the commit that cut it) if a
// slot ever frees up. The remaining losses are the FOLD, STEP and SAT warps.
// Two of the 42 (Chime Glass Pad, Ring Mod Bell) are kept from the old PocketAnalog
// file-based banks rather than written for this one - see their own comments.
// Moving a preset between sections therefore also moves its date, and a DAW project
// saved earlier stored the date it was on. The SOUND still restores correctly - the
// project stores every parameter value, not a reference - but the selector will
// highlight whichever preset now holds that date. So reorder deliberately, not
// casually, once this ships.
//
// Design rules held throughout (verified against setup.cpp, synth.cpp, vco.h):
//   * NO effects. REVPOW / CDELPOW / PHASERPOW are never written, and none of them
//     has an initvalue, so all three stay off. Every preset is the raw voice.
//   * NO sequencer / arp: no notes, no SEQ_* / ARP_*.
//   * Presets are SPARSE against initvalue. All three VCOxGAIN init at 0 dB (unity)
//     and all three VCOxTYPE init at 0 (SAW), so VCO2GAIN MUST be written -60 when
//     VCO2 is unused, or every patch has a stray saw in it.
//   * VCOxTUNEEG = -1 wherever a static FINE/COARSE detune is set, otherwise the
//     detune rides EG1 (TUNEEG inits at 0 = EG1) and glides in with the note.
//   * FILTEG also inits at 0 (= EG1), so a patch that wants no filter envelope has
//     to write FILTEG = -1 explicitly. "Filter open" below means
//     {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1} — LP4 at the top of its range with the
//     envelope off. That is a real statement (this voice is unfiltered), unlike an
//     LP2 parked at cutoff 0.9, which is a bypass wearing a filter's name.
//   * WARP is a wavetable-path transform (Vco::aimWt / ensureWt), so it is only ever
//     set on VCOxTYPE 99. It is a silent no-op on SINE/TRI/SAW/PULSE/MODAL/PAD.
//   * PM is skipped whenever the carrier is WT/PAD/MODAL (synth.cpp, `wtCarrier`),
//     so the PM patch uses a plain SINE carrier. This is the same trap that left the
//     old PM Growl Bass a plain saw. PM depth also has no EG of its own — VCO3's gain
//     and amp EG do NOT feed the PM path — so a PM patch's movement must come from
//     somewhere else (here, the filter).
//   * RING / AM depth, unlike PM, IS scaled by VCO3's amp EG (RingModFast::tick gets
//     egtmp[egs[2]] * gains[2]), so VCO3EG is the way to make a ring fade.
//   * A MODAL oscillator is still gated by the amp EG (g0 = egtmp[egs[0]]*gains[0]),
//     so struck patches run EG1 ATTACK 0 / SUSTAIN 1 and let the bank's own DECAY be
//     the sound; RELEASE only sets how fast note-off damps the ring.
//   * Voice lifetime is adsr[egs[0]] and adsr[egs[1]] (VcoNote::isDead), i.e. the two
//     main oscillators' amp EGs — which is why the noise-only patches still set EG1.
//   * POLYPHONY IS 10 (SynthQueue(10)), and a note arriving on a full pool now STEALS
//     a voice instead of being dropped — see the long note on SynthQueue::stealVoice.
//     Until 2026-08-16 it was dropped SILENTLY, which made amp-EG RELEASE a polyphony
//     budget rather than a taste control: at RELEASE .85 (~3.7 s) a preset exhausted
//     all ten slots within seconds of normal playing and every further note was
//     silent. That is why the presets below keep RELEASE <= ~.7. It is no longer a
//     reason to, but the constraint has not vanished, only softened: ten voices is
//     still ten voices, and a long release now means old notes are CUT SHORT under
//     fast playing (quietest and already-released first) rather than new notes going
//     missing. A pad meant to be held in thick chords still should not ask for a
//     release it cannot afford.
//   * FILT_MODE 17 (MODAL filter): CUT is the bank's fundamental (3 octaves below the
//     keytracked note at CUT 0), RES is its T60 across 0.03..0.5 s. KEYTRACK_TO_FILT 0
//     pins the body so the notes move through a fixed resonator.
//   * A DRIVEN modal bank has ~30 dB of insertion loss on broadband input — measured
//     31.3 dB (Glass Body) and 29.5 dB (Struck String) — because a resonator only
//     captures what lands inside its skirts, and MODAL_DRIVEN_TRIM is a further -26 dB.
//     The exciter carries the makeup: NOISEGAIN runs -60..+60 and a noise-fed mode-17
//     patch wants it WELL POSITIVE, not attenuated. Both presets first shipped with the
//     noise turned DOWN and were near-inaudible. Tonal input gets through ~33 dB better
//     than broadband, but only where a harmonic happens to land on a partial, so it is
//     note-dependent and not a substitute for driving the thing properly.
//   * VCO1/VCO2 COARSE is 0..36 semitones and FINE 0..20 cents, both UP only. Only
//     VCO3COARSEST is signed (-60..+60).
//
// EG1 DECAY only shapes the segment between peak and SUSTAIN, so on the high-sustain
// pads it is nearly inaudible and sits at 0.5. On the 14 presets with SUSTAIN <= 0.6
// it is the main shape, and there it is spread 0.28..0.70 on purpose. Do not "tidy"
// those toward each other — that clustering is what made the old bank's envelopes
// interchangeable.
//
// Enums:
//   VCO type   -1 SINE  4 TRI  0 SAW  2 PULSE  99 WT  97 MODAL  98 PAD
//   FILT_MODE   0 LP4  1 LP2  2 BP2  3 BP4  4 HP2  5 HP4  6 NOTCH
//               7 LAD LP  8 LAD BP  9 LAD HP  10 SEM LP  11 SEM BP  12 SEM HP
//               13 SEM NOTCH  14 303 LP  15 303 BP  16 303 HP  17 MODAL
//   WT table    0 BASIC 1 CLIMB 2 ODD 3 FORMANT 4 VOWEL 5 METAL 6 PULSE 7 SOFT
//               8 ORGAN 9 FIFTHS 10 GROWL 11 CHIME 12 DIGITAL 13 FM 14 COMB
//               15 SHAPER 16 SWEEP 17 HOLLOW 18 AIR 19 NOISE 20 CZ RES 21 FM2
//               22 PLUCK 23 RESO 24 BUZZ 25 EPIANO 26 STACK
//   PAD table   0 BASIC 1 SOFT 2 BUZZ 3 ODD 4 VOWEL 5 FORMANT 6 ORGAN 7 CHIME
//               8 FIFTHS 9 SHAPER 10 FM 11 EPIANO 12 STACK 13 METAL 14 HOLLOW
//               15 FM2 16 GROWL 17 AIR
//               NOT the WT numbering above — PAD offers a curated subset, so the two
//               lists disagree on every index. This line is a FOURTH copy of that
//               subset (with PAD_TABLES in vco.cpp, padNames in setup.cpp and
//               padselnames in gui.cpp) and it has been stale before; re-read
//               PAD_TABLES rather than trusting it.
//   WARP        0 OFF 1 SYNC 2 BEND 3 RING 4 FOLD 5 FORMANT 6 BITS 7 RATE
//               8 DRIVE 9 SAT 10 STEP
//   MODAL body  0 DEEP 1 STRING 2 TINE 3 WOOD 4 BELL 5 GLASS
//   LFO wave    0 SIN 1 TRI 2 SAW 3 SQR
//   LFO dest    1 PITCH 2 FILT 3 AMP 4-6 PW1-3 7 RES 8-10 MORPH1-3
//               11-13 WARP1-3 14-16 UNI1-3 17 STRIKE 18-20 GAIN1-3 21 NOISE
//   NOISEMODE  -1 OFF 0 WHITE 1 PINK 2 BROWN
//   VCOxEG / NOISEEG (the amp EGs)  -1 NONE, 0..3 = EG1..EG4. NONE holds the source at
//               its GAIN for the life of the voice; on VCO1/VCO2 it also gives up that
//               source's vote on when the note ends (VcoNote::deriveEgRouting).
//   FILTEG / RESEG / VCOxTUNEEG   -1 NONE, 0..3 = EG1..EG4
//   VCOxWARPEG / VCOxPADMEG        0 OFF, 1..4 = EG1..EG4
//   VCOxPWMODSRC (= WT morph EG)   0 LFO, 1..4 = EG1..EG4
//
// THE BANDPASS AND HIGHPASS MODES ARE FINE. Measured against a saw at CUT .2, the
// ZDF bandpasses sit 17-20 dB below their own lowpass (LADDER BP -50.4 vs LADDER LP
// -30.6, SEM BP -44.6 vs SEM LP -27.8) and lose about 20 dB more across the whole
// cutoff range. That is what a bandpass on a harmonic source does - it is lossy, not
// broken, and POSTGAIN covers it. The classic BP2/BP4 barely lose anything at all.
//   This is on record because it has now been misdiagnosed TWICE, both times from a
// broken measurement: once as a ZDF gain bug, and once as "BP/HP only work on flat
// input, the usable range is the bottom .02 of the knob". The second reading came
// from a harness whose oscillators were not oscillating (see the cps note below) -
// a bandpass rejects DC almost completely, so every bandpass preset read as silent
// while every lowpass preset read normal. Four presets were rewritten off the back
// of that before the harness was checked, and the rewrite has been reverted.
//
// POSTGAIN IS SOLVED, NOT GUESSED - do not hand-tune it. Every value below comes
// from the offline harness, iterated to convergence so that a FOUR-NOTE CHORD at
// velocity 110 reads -27 LUFS (BS.1770 max-momentary), measured at C3 and C4.
// All 26 land inside 1.9 dB, -26.0 to -27.9; worst sample peak is 0.537.
//
// Level on the CHORD, not on one note. Nobody plays one note, four voices sum ~6 dB
// above one, and polyphony here is 10. Levelled on single notes instead, 21 of the
// 40 presets hit the 0.99 hard clip the moment a chord was played.
//
// Four ways this measurement has been got wrong, each of which produced confident,
// self-consistent, entirely fictional numbers:
//   * VCOPreEvent::cps IS A FREQUENCY IN HZ despite the name - the engine does
//     _cps = _freq * onedsr itself. Pre-dividing by the sample rate runs every
//     oscillator at 0.005 Hz, so the render is an amplitude envelope with no audio
//     in it: a DC ramp that never crosses zero. It still has a plausible RMS, which
//     is how it survived two whole gain solves. A K-weighted meter catches it at
//     once (the highpass eats DC), and so does printing min/max of the buffer.
//   * Plain RMS instead of K-weighted loudness, at a target 7 dB hotter than this
//     bank's -27 LUFS standard.
//   * Mean RMS over a window running past note-off makes every pluck and bell read
//     ~20 dB quiet, and the solve then pins them to POSTGAIN +6.
//   * Note-on on a live noteNum RETRIGGERS that voice rather than building a new
//     one, keeping the old oscillator setup while picking up the new POSTGAIN. Send
//     All Sound Off (CC 120) before every measured note.
// =============================================================================

void Preset::setupFactory(tsl::AppState* _appState) {
    // Four presets use the PADsynth oscillator. With PA_ENABLE_PAD 0 the VCOxPAD*
    // ids live PAST NUM_PARAMS and writing them would run off the params array.
    static_assert(PA_ENABLE_PAD, "factory bank uses PAD presets; see types_pocketanalog.h");
    // Log10 params store 20*log10(x): LFO and jitter rates in Hz, MODAL DECAY in s.
    auto Lg = [](float x) { return 20.f * std::log10(x); };
    struct FP { const char* name; std::vector<PresetParam> vals; };
    const FP bank[] = {
    // ---------------------------------------------------------------- PADS ---

    { "Air Choir", {        // PADsynth VOWEL; the movement is a slow MORPH EG, not an LFO.
                            // The morph EG here is AUDIBLE - measured as mean |dB| difference
                            // of the log spectrum early vs late on a held note: 3.87 dB against
                            // a 1.8 dB floor from the PAD's own random-phase read drifting, on a
                            // scale where SINE vs SAW is 5.41 dB. An earlier pass moved this to
                            // FORMANT believing VOWEL's morph was inert; that came from a metric
                            // (cosine similarity of magnitude spectra) which scored SINE vs SAW
                            // at 0.974 and so could not tell anything apart. Do not re-measure
                            // timbre change that way - calibrate on known pairs first.
        {VCO1TYPE,98},{VCO1GAIN,0},{VCO1EG,0},
        // BANDW / BW SCL / STRETCH / SEED are fixed constants now (PAD_FIXED_BW in
        // vco.h), so this preset no longer sets them. It IS revoiced by that: it used
        // to ask for 2.5 cents of smear and now gets VOWEL's half-cap, ~9 cents.
        {VCO1PADSEL,4},   // 4 = VOWEL
        {VCO1PADPOS,.15f},{VCO1PADMTO,.85f},{VCO1PADMEG,3},
        {EG3ATTACK,.8f},{EG3DECAY,.6f},{EG3SUSTAIN,1},{EG3RELEASE,.7f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.62f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,.62f},{EG1DECAY,.55f},{EG1SUSTAIN,.9f},{EG1RELEASE,.68f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-18} } },

    { "PWM Strings", {      // no wavetable anywhere: two pulses, two LFOs on pulse width
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.45f},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-2},{VCO2PW,.55f},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {LFO1RATE,Lg(.16f)},{LFO1DEPTH,.6f},{LFO1WAVE,0},{LFO1DEST,4},
        {LFO2RATE,Lg(.23f)},{LFO2DEPTH,.6f},{LFO2WAVE,1},{LFO2DEST,5},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-12},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.5f},{FILTRES,.15f},{FILTEG,-1},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.7f},
        {POSTGAIN,-30} } },

    { "Sub Drone", {        // AM two octaves down, with the filter taken OFF the keyboard
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-10},{VCO2FINE,4},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3AM,1},{VCO3AMDEPTH,.45f},{VCO3TYPE,-1},{VCO3COARSEST,-24},{VCO3GAIN,-2},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,10},{FILT_CUT,.5f},{FILTRES,.2f},{FILTEG,-1},
        {KEYTRACK_TO_FILT,0},
        {EG1ATTACK,.8f},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.7f},
        {POSTGAIN,-29} } },

    { "Chime Glass Pad", {  // kept from the old PocketAnalog bank. Two CHIME wavetables a
                            // tenth apart with a slow sine LFO walking OSC1's morph; the
                            // shimmer is JITTERCENTS, not a chorus.
                            // The original leaned on REVERB (REVPOW on, both mixes at .5).
                            // Dropped here because this bank is deliberately effect-free.
                            // The old restore recipe — {REVPOW,1},{REV3MIX,.5f},{REV4MIX,.5f},
                            // {REV3REF,.6f},{REV4REF,.6f},{REV3LPCUT,78.06f},{REV4LPCUT,78.06f}
                            // — is HISTORICAL: only Progenitor1Dark is instantiated in this
                            // build and it reads none of the REV4/LPCUT ids, so of that list
                            // only REVPOW, REV3MIX and REV3REF still do anything.
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,11},{VCO1WTPOS,.5f},
        {VCO2TYPE,99},{VCO2GAIN,-4},{VCO2WTSEL,11},{VCO2WTPOS,.7f},{VCO2FINE,10},
        {VCO2EG,0},{VCO2TUNEEG,-1},
        {LFO1RATE,Lg(.09f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,8},
        {FILT_MODE,1},{FILT_CUT,.65f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.75f},
        {JITTERCENTS,9},
        {POSTGAIN,-26} } },

    // ------------------------------------------------------- KEYS & ORGANS ---

    { "Tonewheel", {        // ORGAN table, filter open, percussive EG2 click on the upper sine
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,8},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO2TYPE,-1},{VCO2GAIN,-4},{VCO2COARSE,19},{VCO2EG,1},{VCO2TUNEEG,-1},
        {EG2ATTACK,0},{EG2DECAY,.22f},{EG2SUSTAIN,0},{EG2RELEASE,.05f},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.05f},
        {POSTGAIN,-30} } },

    { "Tremolo Rhodes", {   // EPIANO with a SQUARE LFO on AMP — gated tremolo, no chorus
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,25},{VCO1WTPOS,.3f},{VCO1MORPHTO,.85f},
        {VCO1PWMODSRC,2},
        {EG2ATTACK,0},{EG2DECAY,.4f},{EG2SUSTAIN,.15f},{EG2RELEASE,.3f},
        {LFO1RATE,Lg(5.5f)},{LFO1DEPTH,.4f},{LFO1WAVE,3},{LFO1DEST,3},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.48f},{EG1SUSTAIN,.4f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.45f},{MW_TO_LFODEPTH,.6f},
        {POSTGAIN,-17} } },

    { "Rate Crush Keys", {  // RATE warp opening back up on EG2 — lofi that resolves as it decays
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,15},{VCO1WTPOS,.35f},{VCO1MORPHTO,.35f},
        {VCO1WARPTYPE,7},{VCO1WARPAMT,.5f},{VCO1WARPTO,.08f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.45f},{EG2SUSTAIN,.2f},{EG2RELEASE,.3f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.55f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.36f},{EG1SUSTAIN,.45f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.4f},
        {POSTGAIN,-16} } },

    // ----------------------------------------------------- BELLS & MALLETS ---

    { "Bell Choir", {       // two engines at once: a struck MODAL bell over a sustained PAD
        {VCO1TYPE,97},{VCO1GAIN,-2},{VCO1EG,0},
        {VCO1MODALCH,4},{VCO1MODALDEC,Lg(2.2f)},{VCO1MODALBRT,.6f},
        {VCO1MODALHRD,.4f},{VCO1MODALPOS,.16f},
        {VCO2TYPE,98},{VCO2GAIN,-9},{VCO2EG,2},
        // Likewise revoiced: 4.3 cents before, CHIME's half-cap ~19 now.
        {VCO2PADSEL,7},{VCO2PADPOS,.3f},{VCO2PADMTO,.3f}, // 7 = CHIME
        {EG3ATTACK,.55f},{EG3DECAY,.5f},{EG3SUSTAIN,.8f},{EG3RELEASE,.5f},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.6f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-18} } },

    { "Ring Mod Bell", {    // kept from the old PocketAnalog bank, and it needed nothing
                            // dropped - no effects in it at all. Two SINES ring-modulated a
                            // fifth plus 15 cents apart: the detune is what makes the clang
                            // beat instead of sitting still. Filter EG on EG1 with SUSTAIN 0,
                            // so the strike closes as it decays.
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3RINGMOD,1},{VCO3TYPE,-1},{VCO3GAIN,0},{VCO3COARSEST,7},{VCO3FINE,15},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_CUT,.9f},{FILTRES,.2f},{FILTEG,0},
        {EG1ATTACK,0},{EG1DECAY,.55f},{EG1SUSTAIN,0},{EG1RELEASE,.2f},
        {POSTGAIN,-9} } },

    { "Deep Gong", {        // MODAL DEEP at the top of the decay range, struck off-centre
        {VCO1TYPE,97},{VCO1GAIN,0},{VCO1EG,0},
        {VCO1MODALCH,0},{VCO1MODALDEC,Lg(5.f)},{VCO1MODALBRT,.45f},
        {VCO1MODALHRD,.5f},{VCO1MODALPOS,.08f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.62f},
        {VEL_TO_AMP,.5f},{AT_TO_DECAY,.5f},{KEYTRACK_TO_DECAY,.25f},
        {POSTGAIN,-18} } },


    { "Metal Scrape", {     // RING warp INSIDE the oscillator riding EG2, ring mod OUTSIDE it.
                            // A static RING amount is a constant clang — it has to decay.
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,5},{VCO1WTPOS,.55f},{VCO1MORPHTO,.55f},
        {VCO1WARPTYPE,3},{VCO1WARPAMT,.25f},{VCO1WARPTO,.06f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.5f},{EG2SUSTAIN,.1f},{EG2RELEASE,.35f},
        {VCO2GAIN,-60},
        {VCO3RINGMOD,1},{VCO3TYPE,-1},{VCO3COARSEST,23},{VCO3GAIN,-3},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,13},{FILT_CUT,.55f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.7f},{EG1SUSTAIN,.3f},{EG1RELEASE,.4f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-16} } },

    { "Glass Body", {       // MODAL FILTER as the pad: a fixed resonator that the notes pass
                            // through, rather than a resonator that is struck. KEYTRACK_TO_FILT 0
                            // is what makes it a BODY - the bank stays put while the notes move
                            // under it, the way a real instrument's body does. LFO on STRIKE (17)
                            // moves where it is excited. One of only two presets using mode 17.
                            //   PINK NOISE, NOT WHITE, and that is the whole fix. Ear-reported
                            // as too noisy; it measured spectral flatness .0060, second-noisiest
                            // in the bank, on WHITE noise -- the hissiest source available.
                            //   TURNING THE NOISE DOWN IS THE WRONG FIX HERE, and was tried
                            // first: the broadband excitation is LOAD-BEARING. With
                            // KEYTRACK_TO_FILT 0 the body is fixed, so it only sounds where the
                            // source has energy at its modes -- noise always does, a harmonic
                            // oscillator only sometimes. Dropping NOISEGAIN +6/-10 to -12/-2 took
                            // flatness to .0005 but blew the level spread across C2..C6 from
                            // 12.0 dB to 30.7, i.e. a preset that is fine at C4 and inaudible two
                            // octaves away. Even a gentle -6/-4 still left 27.0.
                            //   Changing the noise COLOUR keeps the excitation broadband and
                            // removes the hiss: pink measures flatness .0000 and spread 8.0 dB,
                            // BETTER on both counts than the white original. Brown (mode 2) is
                            // 5.3 dB and equally clean but pulls the centroid down to a thud;
                            // pink keeps the top the glass needs.
        {VCO1TYPE,0},{VCO1GAIN,-10},{VCO1EG,0},
        {VCO2GAIN,-60},
        {NOISEMODE,1},{NOISEGAIN,6},{NOISEEG,0},
        {FILT_MODE,17},{FILTMODALBODY,5},{FILTMODALPOS,.12f},
        {FILT_CUT,.5f},{FILTRES,.7f},{FILTEG,-1},
        {KEYTRACK_TO_FILT,0},
        {LFO1RATE,Lg(.06f)},{LFO1DEPTH,.5f},{LFO1WAVE,1},{LFO1DEST,17},
        {EG1ATTACK,.45f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {AT_TO_FILT,.35f},
        {POSTGAIN,-8} } },

    // -------------------------------------------------------------- PLUCKS ---

    { "Analog Pluck", {     // the textbook one: pulse, LP4, fast filter EG, sustain 0
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.4f},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-10},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.3f},{FILTRES,.28f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.32f},{EG2SUSTAIN,.15f},{EG2RELEASE,.15f},
        {EG1ATTACK,0},{EG1DECAY,.38f},{EG1SUSTAIN,0},{EG1RELEASE,.2f},
        {VEL_TO_FILT,.5f},{VEL_TO_AMP,.4f},
        {POSTGAIN,-12} } },

    { "Bit Pluck", {        // BITS warp collapsing on EG2 — a digital decay, no delay involved
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,12},{VCO1WTPOS,.45f},{VCO1MORPHTO,.45f},
        {VCO1WARPTYPE,6},{VCO1WARPAMT,.65f},{VCO1WARPTO,.05f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.4f},{EG2SUSTAIN,0},{EG2RELEASE,.2f},
        {VCO2GAIN,-60},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.28f},{EG1SUSTAIN,0},{EG1RELEASE,.25f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-8} } },


    { "Struck String", {    // the other MODAL FILTER preset, and the opposite use of it: a 60 ms
                            // noise burst on EG2 excites the bank and then gets out of the way.
                            // No oscillator at all - the pitch is entirely the resonator.
        {VCO1GAIN,-60},{VCO2GAIN,-60},
        {NOISEMODE,0},{NOISEGAIN,10},{NOISEEG,1},
        {EG2ATTACK,0},{EG2DECAY,.06f},{EG2SUSTAIN,0},{EG2RELEASE,.04f},
        {FILT_MODE,17},{FILTMODALBODY,1},{FILTMODALPOS,.2f},
        {FILT_CUT,.6f},{FILTRES,.75f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.5f},
        {VEL_TO_AMP,.5f},
        {POSTGAIN,-9} } },

    // --------------------------------------------------------------- LEADS ---

    { "Wide Saw Lead", {    // a 7-voice unison stack with nothing behind it — the raw width
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO1UNIVOICES,7},{VCO1UNIDETUNE,.42f},{VCO1UNIBLEND,.8f},
        {VCO2GAIN,-60},
        {FILT_MODE,7},{FILT_CUT,.4f},{FILTRES,.2f},{FILTEG,1},
        {EG2ATTACK,.02f},{EG2DECAY,.5f},{EG2SUSTAIN,.45f},{EG2RELEASE,.3f},
        {EG1ATTACK,.04f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.3f},
        {JITTERCENTS,5},{MW_TO_UNI,.6f},{VEL_TO_FILT,.4f},
        {POSTGAIN,-23} } },

    { "Sync Sweep Lead", {  // real hard sync: VCO1SYNC + COARSE, EG3 sweeps the slave pitch
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},{VCO1SYNC,1},{VCO1COARSE,19},{VCO1TUNEEG,2},
        {EG3ATTACK,0},{EG3DECAY,.55f},{EG3SUSTAIN,.2f},{EG3RELEASE,.3f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,7},{FILT_CUT,.62f},{FILTRES,.15f},{FILTEG,-1},
        {EG1ATTACK,.03f},{EG1DECAY,.45f},{EG1SUSTAIN,.75f},{EG1RELEASE,.2f},
        {VEL_TO_AMP,.35f},{MW_TO_VIBRATO,.4f},
        {POSTGAIN,-22} } },

    { "303 Scream", {       // 303 HIGHPASS with a resonance envelope — the counterpart to Acid
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.25f},{VCO1EG,0},
        {VCO2GAIN,-60},
        {FILT_MODE,16},{FILT_CUT,.12f},{FILTRES,.45f},{FILTEG,1},
        {RESEG,1},
        {EG2ATTACK,0},{EG2DECAY,.42f},{EG2SUSTAIN,.16f},{EG2RELEASE,.25f},
        {EG1ATTACK,0},{EG1DECAY,.6f},{EG1SUSTAIN,.6f},{EG1RELEASE,.15f},
        {VEL_TO_FILT,.5f},{VEL_TO_RES,.35f},
        {POSTGAIN,-21} } },

    { "Buzz Reed", {        // BEND warp: the read phase leans and the harmonics tilt with it
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,24},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO1WARPTYPE,2},{VCO1WARPAMT,.08f},{VCO1WARPTO,.32f},{VCO1WARPEG,1},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,2},{FILT_CUT,.2f},{FILTRES,.45f},{FILTEG,-1},
        {EG1ATTACK,.12f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.25f},
        {AT_TO_WARP,.5f},
        {POSTGAIN,-20} } },

    // ---------------------------------------------------------------- BASS ---

    { "PM Sine Bass", {     // real PM — a SINE carrier, the only kind PM actually reaches.
                            // PM depth has no EG of its own, so the filter carries the motion.
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3PM,1},{VCO3PMDEPTH,.55f},{VCO3TYPE,-1},{VCO3COARSEST,12},{VCO3GAIN,0},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.4f},{FILTRES,.12f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.34f},{EG2SUSTAIN,.18f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,.55f},{EG1RELEASE,.12f},
        {VEL_TO_AMP,.4f},
        {POSTGAIN,-23} } },

    { "Reso Ladder Bass", { // pulse width moving on EG2 while the ladder rides the same shape
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.15f},{VCO1EG,0},
        {VCO1PWMODSRC,2},{VCO1PWMODDEPTH,.5f},
        {EG2ATTACK,0},{EG2DECAY,.3f},{EG2SUSTAIN,.17f},{EG2RELEASE,.15f},
        {VCO2GAIN,-60},
        {FILT_MODE,7},{FILT_CUT,.22f},{FILTRES,.5f},{FILTEG,1},
        {EG1ATTACK,0},{EG1DECAY,.33f},{EG1SUSTAIN,.5f},{EG1RELEASE,.1f},
        {VEL_TO_FILT,.5f},
        {POSTGAIN,-23} } },

    { "Drive Bass", {       // DRIVE warp for the dirt — distortion inside the oscillator
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,0},{VCO1WTPOS,.2f},{VCO1MORPHTO,.2f},
        {VCO1WARPTYPE,8},{VCO1WARPAMT,.55f},{VCO1WARPTO,.55f},
        {VCO2GAIN,-60},
        {NOISEMODE,2},{NOISEGAIN,-38},{NOISEEG,0},
        {FILT_MODE,14},{FILT_CUT,.26f},{FILTRES,.35f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.28f},{EG2SUSTAIN,.15f},{EG2RELEASE,.15f},
        {EG1ATTACK,0},{EG1DECAY,.52f},{EG1SUSTAIN,.5f},{EG1RELEASE,.12f},
        {VEL_TO_FILT,.5f},{VEL_TO_AMP,.3f},
        {POSTGAIN,-19} } },

    { "AM Bass", {          // amplitude modulation from the sub, a fourth down
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3AM,1},{VCO3AMDEPTH,.7f},{VCO3TYPE,-1},{VCO3COARSEST,-5},{VCO3GAIN,-1},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,10},{FILT_CUT,.3f},{FILTRES,.25f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.35f},{EG2SUSTAIN,.15f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.4f},{EG1SUSTAIN,.55f},{EG1RELEASE,.12f},
        {VEL_TO_FILT,.45f},
        {POSTGAIN,-24} } },

    // ------------------------------------------------------ WINDS & VOICES ---

    { "Breath Flute", {     // brown noise as breath, gated by its own EG3, over HOLLOW
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,17},{VCO1WTPOS,.35f},{VCO1MORPHTO,.35f},
        {VCO1FINE,3},{VCO1TUNEEG,-1},
        {VCO2GAIN,-60},
        {NOISEMODE,2},{NOISEGAIN,-14},{NOISEEG,2},
        {EG3ATTACK,.1f},{EG3DECAY,.3f},{EG3SUSTAIN,.25f},{EG3RELEASE,.2f},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.22f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.25f},
        {MW_TO_VIBRATO,.5f},{JITTERCENTS,7},
        {POSTGAIN,-22} } },

    { "Formant Vox", {      // FORMANT warp on LFO1 and MORPH on LFO2 — two independent movers
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,4},{VCO1WTPOS,.15f},{VCO1MORPHTO,.8f},
        {VCO1WARPTYPE,5},{VCO1WARPAMT,.06f},{VCO1WARPTO,.28f},
        {LFO1RATE,Lg(.35f)},{LFO1DEPTH,.55f},{LFO1WAVE,1},{LFO1DEST,11},
        {LFO2RATE,Lg(.22f)},{LFO2DEPTH,.5f},{LFO2WAVE,0},{LFO2DEST,8},
        {VCO2GAIN,-60},
        {FILT_MODE,11},{FILT_CUT,.2f},{FILTRES,.5f},{FILTEG,-1},
        {EG1ATTACK,.3f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.4f},
        {MW_TO_LFODEPTH,.5f},
        {POSTGAIN,-16} } },

    // ------------------------------------------------------- TEXTURES & FX ---

    { "Wind Sweep", {       // noise only, through a bandpass - the one place a bandpass belongs,
                            // since noise is flat and any placement has something to pass. Noise
                            // has no pitch of its own, so the RESONANT BAND is the pitch, and
                            // KEYTRACK is left at its default 1.0.
                            //   THE 4-POLE BAND IS THE POINT, and so is the resonance. SEM BP and
                            // 303 BP were re-measured here and are far worse (flatness .090 and
                            // .492 against BP4's .014), so a 2-pole band still cannot get there.
                            //   RES .95, NOT .85. An earlier note here claimed .85 was a measured
                            // optimum and that BP4 "gets wider again past it - 1.09 octaves at
                            // .92, 1.23 at 1.0". THAT WAS WRONG, and wrong because the LFO was
                            // left running through the measurement: LFO1 sweeps this filter over
                            // most of its range, so an average taken across the sweep measures the
                            // SWEEP, not the band, and a more resonant filter follows the sweep
                            // more visibly. Defeat the LFO and the band narrows monotonically with
                            // resonance, as a 4-pole bandpass must:
                            //     RES    .75   .85   .90   .92   .95   .98
                            //     oct    0.48  0.39  0.32  0.28  0.24  0.17
                            //     flat  .0061 .0038 .0026 .0020 .0015 .0008
                            // Ear-reported at .85 as "still too much noise", which the numbers
                            // agree with. .95 cuts flatness from .0137 to .0050 with the LFO live.
                            // Stopping short of .98 leaves room before self-oscillation, where the
                            // band stops being wind and becomes a whistle.
                            //   CUT IS THE WHOLE BALANCE HERE, because the filter LFO's offset
                            // is scaled by rest = (0.5 - base) * cut. At CUT 0 the band sits
                            // exactly on the played note - perfect tracking - but rest is 0, so
                            // the LFO is completely dead. Big CUT gives a big sweep that ignores
                            // the keyboard. CUT .03 with a full-depth LFO is the useful corner:
                            // the FLOOR of the sweep tracks (174 Hz at C2 to 1143 Hz at C6) and
                            // it still sweeps ~8x up from there. Measured, not guessed.
                            //   TRIANGLE, not saw: the saw only ever rose and then jumped back,
                            // which is exactly why it read as one endlessly rising noise.
        {VCO1GAIN,-60},{VCO2GAIN,-60},
        {NOISEMODE,0},{NOISEGAIN,-2},{NOISEEG,0},
        {FILT_MODE,3},{FILT_CUT,.03f},{FILTRES,.95f},{FILTEG,-1},
        {LFO1RATE,Lg(.18f)},{LFO1DEPTH,1},{LFO1WAVE,1},{LFO1DEST,2},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.7f},
        {MW_TO_FILT,.5f},
        {POSTGAIN,-37} } },

    };

    int64_t date = LONG_MAX - 1;
    for (auto& p : bank)
        _DATA->presets.push_back({p.name, "null", date--, true, p.vals});
};

void Preset::loadPreset(tsl::AppState* _appState, int num, tsl::FastQueue<VcoPreNote>& notes) {
    if (num >= (int)_DATA->presets.size())
        return;
    Preset& preset = _DATA->presets.at(num);

    for (int i = 0; i < NUM_PARAMS; i++)
        _STATE->params[0][i].store(static_cast<float>(_STATE->parameters[i].initvalue));
    for (auto& val : preset.values)
        _STATE->params[0][val.num].store(val.val);
    _DATA->presetDate = preset.date;

    std::lock_guard lk(_STATE->mutex_midi);
    for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
        for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
            _STATE->midicontrolevents[ch][i] = tsl::parameters::Event{};
            _STATE->midicontrolevents[ch][i].eventType = tsl::parameters::Eventtype::NoParam;
        }
        for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
            _STATE->midinoteevents[ch][i] = tsl::parameters::Event{};
            _STATE->midinoteevents[ch][i].eventType = tsl::parameters::Eventtype::NoParam;
        }
    }
    for (const auto& e : preset.midiEvents) {
        const int ch = e.midiState.channel;
        const int num = e.midiState.num;
        if (e.midiState.type == ParameterType_double) {
            if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_CONTROL)
                _STATE->midicontrolevents[ch][num] = e;
        } else {
            if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_NOTEON)
                _STATE->midinoteevents[ch][num] = e;
        }
    }

    notes.flush();
    for (auto& n : preset.notes)
        if (auto* p = notes.push()) *p = n;

}

// Returns whether the preset file was actually written — deleteOverwriteFunc only
// removes a replaced preset's old file after a successful write.
static bool savePresetToFile(tsl::AppState* _appState, const std::string& presetname,
                             const std::vector<Preset::PresetParam>& vals,
                             const std::vector<VcoPreNote>& notes) {
    PresetHeader header;
    strcpy(header.header, headername);
    struct timespec spec{};
    clock_gettime(CLOCK_REALTIME, &spec);
    header.date = spec.tv_sec;
    header.namelen = (int)presetname.size();
    header.numparams = (int)vals.size();

    std::string dir = tsl::app::getStoragePath("presets");
    if (dir.empty()) { showToast(_appState, "Save Preset: No storage path."); return false; }
    if (!fs::exists(dir)) fs::create_directories(dir);

    std::stringstream filename;
    filename << dir << "/" << tsl::time::nanosecondsSinceEpoch();

    FILE* fd = fopen(filename.str().c_str(), "wb");
    if (!fd) { showToast(_appState, "Save Preset: Could not open output file."); return false; }

    bool ok = fwrite(&header, 1, sizeof(PresetHeader), fd) == sizeof(PresetHeader)
           && fwrite(presetname.data(), 1, presetname.size(), fd) == presetname.size()
           && fwrite(vals.data(), sizeof(Preset::PresetParam), vals.size(), fd) == vals.size();

    if (ok && !notes.empty()) {
        int s = (int)notes.size();
        ok = fwrite(&hasNotesCode, sizeof(int), 1, fd) == 1
          && fwrite(&s, sizeof(int), 1, fd) == 1
          && fwrite(notes.data(), sizeof(VcoPreNote), s, fd) == (size_t)s;
    }

    if (ok) {
        std::vector<tsl::parameters::Event> midiEvents;
        std::lock_guard lk(_STATE->mutex_midi);
        for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
            for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
                const auto& e = _STATE->midicontrolevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) midiEvents.push_back(e);
            }
            for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
                const auto& e = _STATE->midinoteevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) midiEvents.push_back(e);
            }
        }
        if (!midiEvents.empty()) {
            int marker = hasMidiEventsCode;
            int count = (int)midiEvents.size();
            ok = fwrite(&marker, sizeof(int), 1, fd) == 1
              && fwrite(&count, sizeof(int), 1, fd) == 1
              && fwrite(midiEvents.data(), sizeof(tsl::parameters::Event), count, fd) == (size_t)count;
        }
    }

    fclose(fd);
    if (!ok) { std::remove(filename.str().c_str()); showToast(_appState, "Save Preset: Write error."); return false; }

    _DATA->presets.push_back({presetname, filename.str(), header.date, false,
                               std::vector<Preset::PresetParam>(vals),
                               std::vector<VcoPreNote>(notes)});
    std::sort(_DATA->presets.begin(), _DATA->presets.end(),
              [](const ::Preset::Preset& a, const ::Preset::Preset& b) { return a.date > b.date; });
    showToast(_appState, "Preset Saved.");
    return true;
}

void Preset::savePreset(tsl::AppState* _appState, std::vector<PresetParam> vals, std::vector<VcoPreNote> notes) {
    using PAPreset = std::shared_ptr<::Preset::Preset>;
    tsl::QueueUnsafe<PAPreset, 100> queue;
    auto savedVals  = std::make_shared<std::vector<PresetParam>>(std::move(vals));
    auto savedNotes = std::make_shared<std::vector<VcoPreNote>>(std::move(notes));

    tsl::app::deleteOverwriteFunc<PAPreset>(
        _appState, queue, "Save Preset",
        [_appState]() -> std::vector<PAPreset> {
            std::vector<PAPreset> items;
            for (auto& p : _DATA->presets)
                if (!p.isSystem)
                    items.push_back(std::make_shared<::Preset::Preset>(p));
            return items;
        },
        [_appState, savedVals, savedNotes](tsl::AppState* app, std::string& name) {
            return savePresetToFile(app, name, *savedVals, *savedNotes);
        });

    // Refresh preset list (handles any deletions that happened in the window).
    // This must rebuild it exactly as startup does -- setupDefault, then setupFactory,
    // then readPresets (setup.cpp calls the first two, gui.cpp the third). Omitting
    // setupFactory here dropped the entire baked bank from the selector the moment the
    // user saved anything, leaving only Default and their own presets, and it looks
    // like the presets "suddenly disappeared" because nothing is logged and the next
    // launch puts them all back.
    long currentDate = _DATA->presetDate;
    _DATA->presets.clear();
    setupDefault(_appState);
    setupFactory(_appState);
    readPresets(_appState);
    _DATA->presetDate = currentDate;

    // Refresh the preset selector UI
    _appState->toUiThreadQueue.try_push([_appState]() {
        auto* sel = _DATA->views.presetSelector;
        if (sel && sel->visible_) {
            sel->addRecursiveDraw();
            sel->redraw();
        }
    });
}

#ifdef __ANDROID__
jobjectArray java_read_presets(JNIEnv* env, jclass thiz) {
    tsl::AppState* _appState = __STATE;
    if (!_appState) return nullptr;

    jclass clazz = env->FindClass("me/rocks/pocketanalog/Presetitem");
    if (!clazz) { env->ExceptionClear(); return nullptr; }

    jmethodID mid = env->GetMethodID(clazz, "<init>", "()V");
    jfieldID idtime    = env->GetFieldID(clazz, "time",     "J");
    jfieldID idversion = env->GetFieldID(clazz, "version",  "I");
    jfieldID idname    = env->GetFieldID(clazz, "name",     "Ljava/lang/String;");
    jfieldID iddel     = env->GetFieldID(clazz, "isSystem", "Z");
    jfieldID idpath    = env->GetFieldID(clazz, "path",     "Ljava/lang/String;");

    if (!mid || !idtime || !idversion || !idname || !iddel || !idpath) {
        env->ExceptionClear();
        return nullptr;
    }

    int system = 0;
    for (auto& p : _DATA->presets) if (p.isSystem) system++;

    auto size = (int)_DATA->presets.size() - system;
    auto ret = (jobjectArray)env->NewObjectArray(size, clazz, nullptr);
    if (!ret) return nullptr;

    int j = 0;
    for (auto& p : _DATA->presets) {
        if (p.isSystem) continue;
        jobject obj = env->NewObject(clazz, mid);
        if (obj == nullptr) { LOGE("ERROR!"); continue; }
        env->SetLongField(obj, idtime, p.date);
        env->SetIntField(obj, idversion, 1);
        env->SetBooleanField(obj, iddel, p.isSystem);
        auto estr = (jstring)env->NewStringUTF(p.name.c_str());
        env->SetObjectField(obj, idname, estr);
        estr = (jstring)env->NewStringUTF(p.path.c_str());
        env->SetObjectField(obj, idpath, estr);
        env->SetObjectArrayElement(ret, j++, obj);
    }
    return ret;
}

#endif // __ANDROID__

static Preset::Preset captureCurrentPreset(tsl::AppState* _appState) {
    Preset::Preset p;
    p.date = _DATA->presetDate;

    for (int i = 0; i < NUM_PARAMS; i++) {
        float val = _STATE->params[0][i].load();
        if (val != static_cast<float>(_STATE->parameters[i].initvalue) &&
            !_STATE->parameters[i].getFlag(Param::NoAssignment))
            p.values.push_back({i, val});
    }
    for (auto* n = _DATA->seq_noteon._first; n; n = n->next)
        p.notes.push_back(n->data);
    {
        std::lock_guard lk(_STATE->mutex_midi);
        for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
            for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
                const auto& e = _STATE->midicontrolevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) p.midiEvents.push_back(e);
            }
            for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
                const auto& e = _STATE->midinoteevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) p.midiEvents.push_back(e);
            }
        }
    }
    return p;
}

static std::vector<uint8_t> presetToBytes(const Preset::Preset& p) {
    std::vector<uint8_t> buf;
    auto append = [&](const void* data, size_t n) {
        const auto* b = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), b, b + n);
    };

    PresetHeader header;
    strcpy(header.header, headername);
    header.date = p.date;
    header.namelen = (int)p.name.size();
    header.numparams = (int)p.values.size();
    append(&header, sizeof(header));
    if (!p.name.empty())
        append(p.name.data(), p.name.size());
    if (!p.values.empty())
        append(p.values.data(), p.values.size() * sizeof(Preset::PresetParam));
    if (!p.notes.empty()) {
        int s = (int)p.notes.size();
        append(&hasNotesCode, sizeof(int));
        append(&s, sizeof(int));
        append(p.notes.data(), p.notes.size() * sizeof(VcoPreNote));
    }
    if (!p.midiEvents.empty()) {
        int marker = hasMidiEventsCode;
        int count = (int)p.midiEvents.size();
        append(&marker, sizeof(int));
        append(&count, sizeof(int));
        append(p.midiEvents.data(), p.midiEvents.size() * sizeof(tsl::parameters::Event));
    }
    return buf;
}

static bool presetFromBytes(const uint8_t* data, size_t size, Preset::Preset& p) {
    size_t pos = 0;
    auto read = [&](void* dst, size_t n) -> bool {
        if (pos + n > size) return false;
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    };

    PresetHeader header{};
    if (!read(&header, sizeof(header))) return false;
    if (strcmp(header.header, headername) != 0) return false;

    p.date = header.date;
    if (header.namelen > 0) {
        std::vector<char> namebuf(header.namelen + 1, '\0');
        if (!read(namebuf.data(), header.namelen)) return false;
        p.name = namebuf.data();
    }

    p.values.resize(header.numparams);
    if (header.numparams > 0 && !read(p.values.data(), p.values.size() * sizeof(Preset::PresetParam)))
        return false;

    {
        int marker{};
        size_t saved = pos;
        if (read(&marker, sizeof(int)) && marker == hasNotesCode) {
            int count{};
            if (read(&count, sizeof(int)) && count > 0 && count < 10000) {
                p.notes.resize(count);
                if (!read(p.notes.data(), count * sizeof(VcoPreNote))) p.notes.clear();
            }
        } else {
            pos = saved;
        }
    }
    {
        int marker{};
        size_t saved = pos;
        if (read(&marker, sizeof(int)) && marker == hasMidiEventsCode) {
            int count{};
            if (read(&count, sizeof(int)) && count > 0 && count < 10000) {
                for (int i = 0; i < count; i++) {
                    tsl::parameters::Event e{};
                    if (!read(&e, sizeof(tsl::parameters::Event))) break;
                    p.midiEvents.push_back(e);
                }
            }
        } else {
            pos = saved;
        }
    }
    return true;
}

std::vector<uint8_t> Preset::serializeCurrentState(tsl::AppState* _appState) {
    // Only serialize params — notes/MIDI events contain raw pointers, unsafe across sessions
    std::vector<uint8_t> buf;
    auto append = [&](const void* d, size_t n) {
        const auto* b = static_cast<const uint8_t*>(d);
        buf.insert(buf.end(), b, b + n);
    };
    for (int i = 0; i < NUM_PARAMS; i++) {
        float val = _STATE->params[0][i].load();
        if (val != static_cast<float>(_STATE->parameters[i].initvalue) &&
            !_STATE->parameters[i].getFlag(Param::NoAssignment)) {
            PresetParam pp{(uint16_t)i, val};
            append(&pp, sizeof(pp));
        }
    }
    return buf;
}

void Preset::unserializeState(tsl::AppState* _appState, const uint8_t* data, size_t size) {
    // Reset all params to init, then apply saved values
    for (int i = 0; i < NUM_PARAMS; i++)
        _STATE->params[0][i].store(static_cast<float>(_STATE->parameters[i].initvalue));

    const size_t stride = sizeof(PresetParam);
    for (size_t pos = 0; pos + stride <= size; pos += stride) {
        PresetParam pp{};
        memcpy(&pp, data + pos, stride);
        if (pp.num > 0 && pp.num < NUM_PARAMS &&
            !_STATE->parameters[pp.num].getFlag(Param::NoAssignment))
            _STATE->params[0][pp.num].store(pp.val);
    }

    // (notes and MIDI learn not restored — unsafe to serialize raw structs)
    // Apply restored params to running state
    for (int i = 1; i < NUM_PARAMS; i++) {
        tsl::parameters::Event e;
        e.setup(_STATE, 0, i);
        e.value = _STATE->params[0][i].load();
        e.flags |= tsl::parameters::Event::FromDaw;
        e.applyFromExt(_STATE, tsl::parameters::None);
    }
}

#ifdef __ANDROID__
jint java_delete_preset(JNIEnv* env, jclass thiz, jlong date) {
    tsl::AppState* _appState = __STATE;
    auto it = _DATA->presets.begin();
    while (it != _DATA->presets.end()) {
        if (!it->isSystem && it->date == (int64_t)date) {
            int success = std::remove(it->path.c_str());
            if (!success)
                it = _DATA->presets.erase(it);
            return success;
        } else ++it;
    }
    return 1;
}
#endif // __ANDROID__
