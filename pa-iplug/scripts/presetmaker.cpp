// Voltaic factory preset maker — bank 2 (25 presets)
// Focus: soft pads for ambient/deep house + showcases for the new filter
// modes (LP4 LP2 BP2 BP4 HP2 HP4 NTCH).
//
// Build:  clang++ -std=c++17 -O2 presetmaker.cpp -o presetmaker
// Run:    ./presetmaker <output-dir>
//
// File format = preset.cpp v3:
//   PresetHeader{char[11] "TSLVASYNTH", int version=3, int64 date,
//                int namelen, int numparams}  (32 bytes)
//   name bytes, then numparams * {int32 paramnum, float value}
//
// Engine conventions (from CPP-New/va setup.cpp / synth.cpp):
//   VCO type: -1 sine, 4 tri, 0 saw, 2 pulse.  Gains in dB (unused osc: -60).
//   PW param 0..1 -> duty 0.5..0.05.
//   EG A/D/R 0..1 exponential (0=10ms, 0.5~330ms, 0.7~1.3s, 0.85~3.7s).
//   FILTEG/RESEG/TUNEEG: -1 off, 0..3 = EG1..EG4. TUNEEG defaults to EG1, so
//   any static detune MUST set TUNEEG=-1 or it rides the amp envelope.
//   LFO dest: 1 pitch(scales tune offset), 2 filt, 3 amp, 4/5/6 pw1-3, 7 res.
//   Rates/delays/cutoffs stored as 20*log10(x) (Hz / seconds).
//   FILT_MODE: 0 LP4, 1 LP2, 2 BP2, 3 BP4, 4 HP2, 5 HP4, 6 NTCH.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#define GRAINSTORM_TYPES_H_STANDALONE
#include "/Users/patrickropohl/programming/CPP-New/graphics/common_interface/include/types_voltaic.h"

struct PresetHeader {
    char header[11]{"TSLVASYNTH"};
    int version{3};
    int64_t date{};
    int namelen{};
    int numparams{};
};
static_assert(sizeof(PresetHeader) == 32, "header layout must match preset.cpp");

struct P { int num; float val; };
struct Def { const char* name; std::vector<P> vals; };

static float L(float x) { return 20.f * std::log10(x); }  // Hz/seconds -> stored log value

int main(int argc, char** argv) {
    std::string outdir = argc > 1 ? argv[1] : "presets_out";

    // dates newer than bank 1 (1783644581..605) so the new bank sorts on top
    const int64_t dateBase = 1784100000;
    const long long fileBase = 1784100000000000000LL;

    std::vector<Def> presets = {

    // ───────────────────────── PADS / AMBIENT ─────────────────────────

    { "Deep House Pad", {   // soft warm chord pad, LP4, gentle filter EG
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-2},{VCO2FINE,9},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-10},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.42f},{FILTRES,.18f},{FILTEG,1},
        {EG1ATTACK,.45f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.6f},
        {EG2ATTACK,.4f},{EG2DECAY,.55f},{EG2SUSTAIN,.55f},{EG2RELEASE,.6f},
        {MW_TO_FILT,.4f},{JITTERCENTS,8},
        {REVPOW,1},{REV3MIX,.3f},{REV4MIX,.3f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},{REV3REF,.55f},{REV4REF,.55f},
        {POSTGAIN,-24} } },

    { "Glass PWM Pad", {    // LP2 airy, slow LFO sweeps PW toward thin
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.25f},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-4},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {LFO1RATE,L(.15f)},{LFO1DEPTH,.45f},{LFO1WAVE,0},{LFO1DEST,4},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {REVPOW,1},{REV3MIX,.35f},{REV4MIX,.35f},{REV3LPCUT,L(8000)},{REV4LPCUT,L(8000)},{REV3REF,.6f},{REV4REF,.6f},
        {POSTGAIN,-24} } },

    { "Airy BP Pad", {      // BP2 breathy band + pink noise air
        {VCO1TYPE,4},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-5},{VCO2PW,.3f},{VCO2FINE,6},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {NOISEMODE,1},{NOISEGAIN,-28},{NOISEEG,0},
        {FILT_MODE,2},{FILT_CUT,.5f},{FILTRES,.25f},{FILTEG,-1},
        {LFO1RATE,L(.1f)},{LFO1DEPTH,.35f},{LFO1WAVE,0},{LFO1DEST,2},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.65f},
        {REVPOW,1},{REV3MIX,.4f},{REV4MIX,.4f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},{REV3REF,.6f},{REV4REF,.6f},
        {POSTGAIN,-22} } },

    { "Hollow Notch Pad", { // NTCH drifting through the spectrum = built-in phaser
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-3},{VCO2FINE,11},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,6},{FILT_CUT,.45f},{FILTRES,.3f},{FILTEG,-1},
        {LFO1RATE,L(.08f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,2},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.65f},
        {JITTERCENTS,6},
        {REVPOW,1},{REV3MIX,.3f},{REV4MIX,.3f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},
        {POSTGAIN,-22} } },

    { "Evolving PWM Pad", { // two pulses, two LFOs on their widths, never static
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.2f},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-3},{VCO2PW,.45f},{VCO2FINE,8},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,4},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {LFO1RATE,L(.11f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,4},
        {LFO2RATE,L(.17f)},{LFO2DEPTH,.4f},{LFO2WAVE,0},{LFO2DEST,5},
        {FILT_MODE,0},{FILT_CUT,.5f},{FILTRES,.15f},{FILTEG,-1},
        {MW_TO_FILT,.5f},
        {EG1ATTACK,.65f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.72f},
        {REVPOW,1},{REV3MIX,.35f},{REV4MIX,.35f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},{REV3REF,.6f},{REV4REF,.6f},
        {POSTGAIN,-25} } },

    { "Sine Bloom Pad", {   // filter blooms open ~2s after the note starts
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-8},{VCO2FINE,5},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-6},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.55f},{FILTRES,.2f},{FILTEG,1},
        {EG2ATTACK,.75f},{EG2DECAY,.6f},{EG2SUSTAIN,.6f},{EG2RELEASE,.7f},
        {EG1ATTACK,.5f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.68f},
        {REVPOW,1},{REV3MIX,.4f},{REV4MIX,.4f},{REV3LPCUT,L(6000)},{REV4LPCUT,L(6000)},{REV3REF,.65f},{REV4REF,.65f},
        {POSTGAIN,-22} } },

    { "Cold Drift Ambient", { // detuned tris, pitch-LFO wobbles the detune, drift
        {VCO1TYPE,4},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-2},{VCO2FINE,14},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {JITTERCENTS,15},
        {LFO1RATE,L(.07f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,1},
        {LFO2RATE,L(.05f)},{LFO2DEPTH,.3f},{LFO2WAVE,0},{LFO2DEST,2},
        {FILT_MODE,1},{FILT_CUT,.5f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.7f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.75f},
        {REVPOW,1},{REV3MIX,.5f},{REV4MIX,.5f},{REV3LPCUT,L(6000)},{REV4LPCUT,L(6000)},{REV3REF,.7f},{REV4REF,.7f},
        {POSTGAIN,-22} } },

    { "Underwater BP Pad", { // BP4 narrow band swimming slowly + dubby echo
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-4},{VCO2PW,.3f},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,3},{FILT_CUT,.32f},{FILTRES,.35f},{FILTEG,-1},
        {LFO1RATE,L(.12f)},{LFO1DEPTH,.45f},{LFO1WAVE,0},{LFO1DEST,2},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {CDELPOW,1},{CDELDEL,L(.42f)},{CDELFB,.35f},{CDELMIX,.25f},
        {REVPOW,1},{REV3MIX,.35f},{REV4MIX,.35f},{REV3LPCUT,L(5000)},{REV4LPCUT,L(5000)},
        {POSTGAIN,-20} } },

    { "Choir Pad", {        // narrow pulse through BP2 ~ vocal formant, PW shimmer
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.55f},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-5},{VCO2FINE,6},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,2},{FILT_CUT,.55f},{FILTRES,.3f},{FILTEG,-1},
        {LFO1RATE,L(.2f)},{LFO1DEPTH,.3f},{LFO1WAVE,0},{LFO1DEST,4},
        {JITTERCENTS,10},
        {EG1ATTACK,.62f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.68f},
        {REVPOW,1},{REV3MIX,.45f},{REV4MIX,.45f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},{REV3REF,.65f},{REV4REF,.65f},
        {POSTGAIN,-22} } },

    { "Dark Bed Drone", {   // low LP4 bed + brown noise floor, very slow swell
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-3},{VCO2FINE,10},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {NOISEMODE,2},{NOISEGAIN,-20},{NOISEEG,0},
        {FILT_MODE,0},{FILT_CUT,.22f},{FILTRES,.25f},{FILTEG,-1},
        {LFO1RATE,L(.06f)},{LFO1DEPTH,.35f},{LFO1WAVE,0},{LFO1DEST,2},
        {EG1ATTACK,.8f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.8f},
        {REVPOW,1},{REV3MIX,.45f},{REV4MIX,.45f},{REV3LPCUT,L(5000)},{REV4LPCUT,L(5000)},{REV3REF,.7f},{REV4REF,.7f},
        {POSTGAIN,-20} } },

    // ───────────────────────── DEEP HOUSE / HOUSE ─────────────────────────

    { "Deep House Stab", {  // short chordal stab, filter EG bite, velocity plays it
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-3},{VCO2PW,.3f},{VCO2FINE,6},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.45f},{FILTRES,.3f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.3f},{EG2SUSTAIN,.15f},{EG2RELEASE,.25f},
        {EG1ATTACK,0},{EG1DECAY,.42f},{EG1SUSTAIN,.25f},{EG1RELEASE,.3f},
        {VEL_TO_FILT,.4f},{VEL_TO_AMP,.3f},
        {CDELPOW,1},{CDELDEL,L(.32f)},{CDELFB,.3f},{CDELMIX,.22f},
        {POSTGAIN,-22} } },

    { "M1 House Bass", {    // organ-bass: sine + sub + quiet pulse, LP2, tight
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-7},{VCO2PW,.35f},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-4},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.1f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.25f},{EG2SUSTAIN,.4f},{EG2RELEASE,.15f},
        {EG1ATTACK,0},{EG1DECAY,.45f},{EG1SUSTAIN,.5f},{EG1RELEASE,.12f},
        {VEL_TO_AMP,.4f},
        {POSTGAIN,-18} } },

    { "Warm House Keys", {  // EP-ish keys with real chorus (modulated short delay)
        {VCO1TYPE,4},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-6},{VCO2PW,.4f},{VCO2FINE,5},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,1},{FILT_CUT,.55f},{FILTRES,.12f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.5f},{EG2SUSTAIN,.35f},{EG2RELEASE,.3f},
        {EG1ATTACK,0},{EG1DECAY,.55f},{EG1SUSTAIN,.5f},{EG1RELEASE,.35f},
        {VEL_TO_FILT,.35f},{VEL_TO_AMP,.35f},
        {CDELPOW,1},{CDELDEL,L(.018f)},{CDDELMODRATE,L(.9f)},{CDDELMODDEPTH,.35f},{CDELFB,.1f},{CDELMIX,.4f},
        {POSTGAIN,-20} } },

    { "Deep Sub Pluck", {   // house bassline: sine weight + saw grit, fast filter pluck
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-14},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,0},{FILT_CUT,.35f},{FILTRES,.2f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.28f},{EG2SUSTAIN,.1f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,.45f},{EG1RELEASE,.15f},
        {VEL_TO_FILT,.5f},
        {POSTGAIN,-16} } },

    { "Garage Skip Stab", { // HP2 thins the lows out of a detuned saw chord — UKG skip
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-2},{VCO2FINE,8},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,4},{FILT_CUT,.25f},{FILTRES,.25f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.3f},{EG1SUSTAIN,.12f},{EG1RELEASE,.18f},
        {VEL_TO_AMP,.5f},
        {CDELPOW,1},{CDELDEL,L(.166f)},{CDELFB,.35f},{CDELMIX,.3f},
        {POSTGAIN,-20} } },

    { "Night Drive Pluck", { // PWM pluck, EG2 drives width+filter+res together
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.3f},{VCO1EG,0},
        {VCO1PWMODSRC,2},{VCO1PWMODDEPTH,.4f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,0},{FILT_CUT,.4f},{FILTRES,.45f},{FILTEG,1},{RESEG,1},
        {EG2ATTACK,0},{EG2DECAY,.32f},{EG2SUSTAIN,.05f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,.3f},{EG1RELEASE,.25f},
        {VEL_TO_FILT,.45f},
        {CDELPOW,1},{CDELDEL,L(.375f)},{CDELFB,.4f},{CDELMIX,.3f},
        {POSTGAIN,-20} } },

    // ─────────────────── FILTER SHOWCASES / CHARACTER ───────────────────

    { "BP Talk Pluck", {    // BP4 + resonance + falling band = talky "yah"
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,3},{FILT_CUT,.5f},{FILTRES,.5f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.35f},{EG2SUSTAIN,.1f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,.35f},{EG1RELEASE,.2f},
        {VEL_TO_FILT,.5f},
        {POSTGAIN,-18} } },

    { "HP Air Lead", {      // HP4 keeps only the top — glassy thin lead
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-3},{VCO2FINE,9},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,5},{FILT_CUT,.3f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,.15f},{EG1DECAY,.4f},{EG1SUSTAIN,.75f},{EG1RELEASE,.3f},
        {MW_TO_VIBRATO,.4f},{JITTERCENTS,10},
        {CDELPOW,1},{CDELDEL,L(.25f)},{CDELFB,.3f},{CDELMIX,.25f},
        {POSTGAIN,-20} } },

    { "Notch Sweep Keys", { // NTCH + audible-rate-ish LFO = phaser keys, no phaser used
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.3f},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-4},{VCO2FINE,5},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,6},{FILT_CUT,.5f},{FILTRES,.2f},{FILTEG,-1},
        {LFO1RATE,L(.35f)},{LFO1DEPTH,.55f},{LFO1WAVE,0},{LFO1DEST,2},
        {EG1ATTACK,.05f},{EG1DECAY,.5f},{EG1SUSTAIN,.6f},{EG1RELEASE,.4f},
        {POSTGAIN,-20} } },

    { "Brass Swell", {      // filter swells in after the note like an analog brass section
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-2},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,0},{VCO3COARSEST,-12},{VCO3GAIN,-10},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.55f},{FILTRES,.15f},{FILTEG,1},
        {EG2ATTACK,.45f},{EG2DECAY,.4f},{EG2SUSTAIN,.7f},{EG2RELEASE,.3f},
        {EG1ATTACK,.12f},{EG1DECAY,.4f},{EG1SUSTAIN,.8f},{EG1RELEASE,.3f},
        {AT_TO_FILT,.4f},{JITTERCENTS,12},
        {POSTGAIN,-22} } },

    { "Solina Strings", {   // ensemble: detuned saws + chorus, HP2 just trims mud
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-2},{VCO2FINE,12},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,4},{FILT_CUT,.12f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.45f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.55f},
        {CDELPOW,1},{CDELDEL,L(.02f)},{CDDELMODRATE,L(.7f)},{CDDELMODDEPTH,.45f},{CDELFB,.15f},{CDELMIX,.5f},
        {REVPOW,1},{REV3MIX,.25f},{REV4MIX,.25f},{REV3LPCUT,L(8000)},{REV4LPCUT,L(8000)},
        {POSTGAIN,-22} } },

    { "Gamelan Bell", {     // soft inharmonic ring-mod bell through BP2, long tail
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3NORMAL,0},{VCO3RINGMOD,1},{VCO3TYPE,-1},{VCO3COARSEST,17},{VCO3FINE,8},{VCO3GAIN,0},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,2},{FILT_CUT,.6f},{FILTRES,.2f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.6f},{EG1SUSTAIN,0},{EG1RELEASE,.55f},
        {REVPOW,1},{REV3MIX,.45f},{REV4MIX,.45f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},{REV3REF,.65f},{REV4REF,.65f},
        {POSTGAIN,-18} } },

    { "Soft Sync Lead", {   // windowed sync, tri wave, EG3 gently sweeps the sync pitch
        {VCO1TYPE,4},{VCO1GAIN,0},{VCO1SYNC,1},{VCO1COARSE,7},{VCO1EG,0},{VCO1TUNEEG,2},
        {EG3ATTACK,.3f},{EG3DECAY,.6f},{EG3SUSTAIN,.55f},{EG3RELEASE,.5f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3RINGMOD,0},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.5f},{FILTRES,.1f},{FILTEG,-1},
        {JITTERCENTS,8},
        {EG1ATTACK,.2f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.35f},
        {REVPOW,1},{REV3MIX,.3f},{REV4MIX,.3f},{REV3LPCUT,L(7000)},{REV4LPCUT,L(7000)},
        {POSTGAIN,-20} } },

    { "Tape Flute", {       // breathy sine flute, tape-drift jitter, pink air
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,4},{VCO2GAIN,-10},{VCO2FINE,4},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {NOISEMODE,1},{NOISEGAIN,-26},{NOISEEG,0},
        {FILT_MODE,1},{FILT_CUT,.55f},{FILTRES,.05f},{FILTEG,-1},
        {JITTERCENTS,14},
        {EG1ATTACK,.25f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.3f},
        {CDELPOW,1},{CDELDEL,L(.3f)},{CDELFB,.25f},{CDELMIX,.2f},
        {POSTGAIN,-18} } },

    { "Dub Chord Echo", {   // dubby echo stab: dark LP4, long feedback tail
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.25f},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-3},{VCO2PW,.4f},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},{VCO3RINGMOD,0},
        {FILT_MODE,0},{FILT_CUT,.33f},{FILTRES,.35f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.3f},{EG2SUSTAIN,.1f},{EG2RELEASE,.25f},
        {EG1ATTACK,0},{EG1DECAY,.4f},{EG1SUSTAIN,.2f},{EG1RELEASE,.3f},
        {CDELPOW,1},{CDELDEL,L(.45f)},{CDELFB,.55f},{CDELMIX,.42f},
        {REVPOW,1},{REV3MIX,.2f},{REV4MIX,.2f},{REV3LPCUT,L(6000)},{REV4LPCUT,L(6000)},
        {POSTGAIN,-22} } },
    };

    int idx = 0;
    for (auto& def : presets) {
        PresetHeader h;
        h.date = dateBase + idx;
        h.namelen = (int)strlen(def.name);
        h.numparams = (int)def.vals.size();

        char path[512];
        snprintf(path, sizeof(path), "%s/%lld", outdir.c_str(), fileBase + idx);
        FILE* fd = fopen(path, "wb");
        if (!fd) { fprintf(stderr, "cannot open %s\n", path); return 1; }
        fwrite(&h, 1, sizeof(h), fd);
        fwrite(def.name, 1, h.namelen, fd);
        for (auto& p : def.vals) {
            int32_t n = p.num; float v = p.val;
            fwrite(&n, sizeof(n), 1, fd);
            fwrite(&v, sizeof(v), 1, fd);
        }
        fclose(fd);
        printf("%s  <- \"%s\" (%d params)\n", path, def.name, h.numparams);
        idx++;
    }
    printf("%d presets written.\n", idx);
    return 0;
}
