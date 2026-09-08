// Voltaic factory preset maker — bank 3 (soft synth pads / ambient)
// Leans on the wavetable oscillators + LFO→MORPH for movement instead of the
// filter-sweep cliché. Build: clang++ -std=c++17 -O2 presetmaker_pads.cpp -o pm_pads
// Run: ./pm_pads <output-dir>
//
// Conventions (see presetmaker.cpp / project memory):
//   VCO type: -1 sine, 4 tri, 0 saw, 2 pulse, 99 wavetable.
//   VCOxWTSEL: 0 BASIC 1 CLIMB 2 ODD 3 FORMANT 4 VOWEL 5 METAL 6 PULSE 7 FOLD
//              8 ORGAN 9 FIFTHS 10 GROWL 11 CHIME 12 DIGITAL 13 FM 14 COMB
//              15 SHAPER 16 SWEEP 17 HOLLOW 18 AIR 19 CZSAW 20 CZRES
//   VCOxWTPOS 0..1 morph base.  LFO dest 8/9/10 = MRPH1/2/3.
//   TUNE EG must be -1 (NONE) for static detune.  Unused VCO2 gain = -60.
//   Log params stored as 20*log10(x): LFO/chorus rate (Hz), CDELDEL (s), REV cut (Hz).

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "/Users/patrickropohl/programming/CPP-New/graphics/common_interface/include/types_voltaic.h"

struct PresetHeader { char header[11]{"TSLVASYNTH"}; int version{3}; int64_t date{}; int namelen{}; int numparams{}; };
static_assert(sizeof(PresetHeader) == 32, "header layout");
struct P { int num; float val; };
struct Def { const char* name; std::vector<P> vals; };
static float L(float x) { return 20.f * std::log10(x); }

int main(int argc, char** argv) {
    std::string outdir = argc > 1 ? argv[1] : "presets_out";
    const int64_t dateBase = 1784600000;
    const long long fileBase = 1784600000000000000LL;

    // shared soft-pad tail: gentle reverb + drift + slow amp env
    auto REV = [](float mix, float cut) -> std::vector<P> {
        return {{REVPOW,1},{REV3MIX,mix},{REV4MIX,mix},{REV3LPCUT,L(cut)},{REV4LPCUT,L(cut)},{REV3REF,.6f},{REV4REF,.6f}};
    };
    auto add = [](std::vector<P>& v, std::vector<P> more){ for (auto& p:more) v.push_back(p); };

    std::vector<Def> presets;

    { std::vector<P> v = { // 1 — evolving two-WT dream pad, LFO morphs both layers
        {VCO1TYPE,99},{VCO1WTSEL,0},{VCO1WTPOS,.7f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,0},{VCO2WTPOS,.55f},{VCO2GAIN,-2},{VCO2FINE,9},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.55f},{FILTRES,.1f},{FILTEG,-1},
        {LFO1RATE,L(.13f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,8},
        {LFO2RATE,L(.09f)},{LFO2DEPTH,.45f},{LFO2WAVE,0},{LFO2DEST,9},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {JITTERCENTS,8},{POSTGAIN,-24} };
      add(v, REV(.35f,7000)); presets.push_back({"Morph Dream Pad", v}); }

    { std::vector<P> v = { // 2 — vowel choir pad, LFO sweeps the vowel
        {VCO1TYPE,99},{VCO1WTSEL,4},{VCO1WTPOS,.5f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,4},{VCO2WTPOS,.65f},{VCO2GAIN,-3},{VCO2FINE,6},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},
        {FILT_MODE,0},{FILT_CUT,.6f},{FILTRES,.12f},{FILTEG,-1},
        {LFO1RATE,L(.1f)},{LFO1DEPTH,.55f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.68f},
        {JITTERCENTS,10},{POSTGAIN,-23} };
      add(v, REV(.45f,7000)); presets.push_back({"Vowel Choir Pad", v}); }

    { std::vector<P> v = { // 3 — breathy air pad + sub
        {VCO1TYPE,99},{VCO1WTSEL,18},{VCO1WTPOS,.4f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,-1},{VCO2GAIN,-6},{VCO2FINE,4},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.05f},{FILTEG,-1},
        {LFO1RATE,L(.07f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.68f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.75f},
        {JITTERCENTS,12},{POSTGAIN,-23} };
      add(v, REV(.45f,8000)); presets.push_back({"Glass Air Pad", v}); }

    { std::vector<P> v = { // 4 — CZ resonant pad, warm with slow morph
        {VCO1TYPE,99},{VCO1WTSEL,20},{VCO1WTPOS,.45f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,20},{VCO2WTPOS,.3f},{VCO2GAIN,-3},{VCO2FINE,8},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.5f},{FILTRES,.12f},{FILTEG,-1},
        {LFO1RATE,L(.12f)},{LFO1DEPTH,.45f},{LFO1WAVE,0},{LFO1DEST,8},
        {LFO2RATE,L(.15f)},{LFO2DEPTH,.4f},{LFO2WAVE,0},{LFO2DEST,9},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {JITTERCENTS,8},{POSTGAIN,-24} };
      add(v, REV(.35f,6500)); presets.push_back({"CZ Resonant Pad", v}); }

    { std::vector<P> v = { // 5 — warm organ pad with chorus
        {VCO1TYPE,99},{VCO1WTSEL,8},{VCO1WTPOS,.6f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,8},{VCO2WTPOS,.4f},{VCO2GAIN,-3},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},
        {FILT_MODE,0},{FILT_CUT,.6f},{FILTRES,.08f},{FILTEG,-1},
        {LFO1RATE,L(.1f)},{LFO1DEPTH,.35f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.4f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.55f},
        {CDELPOW,1},{CDELDEL,L(.02f)},{CDDELMODRATE,L(.7f)},{CDDELMODDEPTH,.45f},{CDELFB,.12f},{CDELMIX,.4f},
        {POSTGAIN,-22} };
      add(v, REV(.25f,8000)); presets.push_back({"Warm Organ Pad", v}); }

    { std::vector<P> v = { // 6 — glassy chime pad, slow attack, big reverb
        {VCO1TYPE,99},{VCO1WTSEL,11},{VCO1WTPOS,.5f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,11},{VCO2WTPOS,.7f},{VCO2GAIN,-4},{VCO2FINE,10},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,0},
        {FILT_MODE,1},{FILT_CUT,.65f},{FILTRES,.08f},{FILTEG,-1},
        {LFO1RATE,L(.09f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.75f},
        {JITTERCENTS,9},{POSTGAIN,-24} };
      add(v, REV(.5f,8000)); presets.push_back({"Chime Glass Pad", v}); }

    { std::vector<P> v = { // 7 — formant sweep pad, evolving vocal-ish
        {VCO1TYPE,99},{VCO1WTSEL,3},{VCO1WTPOS,.3f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,0},{VCO2GAIN,-5},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.55f},{FILTRES,.15f},{FILTEG,-1},
        {LFO1RATE,L(.08f)},{LFO1DEPTH,.6f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.58f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.68f},
        {JITTERCENTS,8},{POSTGAIN,-23} };
      add(v, REV(.4f,7000)); presets.push_back({"Formant Sweep Pad", v}); }

    { std::vector<P> v = { // 8 — deep ambient wash, very slow, dark
        {VCO1TYPE,99},{VCO1WTSEL,0},{VCO1WTPOS,.3f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,0},{VCO2WTPOS,.2f},{VCO2GAIN,-3},{VCO2FINE,11},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-6},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.4f},{FILTRES,.12f},{FILTEG,-1},
        {LFO1RATE,L(.05f)},{LFO1DEPTH,.4f},{LFO1WAVE,0},{LFO1DEST,8},
        {LFO2RATE,L(.04f)},{LFO2DEPTH,.35f},{LFO2WAVE,0},{LFO2DEST,9},
        {EG1ATTACK,.75f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.82f},
        {JITTERCENTS,14},{POSTGAIN,-23} };
      add(v, REV(.55f,6000)); presets.push_back({"Deep Ambient Wash", v}); }

    { std::vector<P> v = { // 9 — FM glass EP pad (low index), soft digital
        {VCO1TYPE,99},{VCO1WTSEL,13},{VCO1WTPOS,.35f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,-1},{VCO2GAIN,-8},{VCO2FINE,5},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.06f},{FILTEG,-1},
        {LFO1RATE,L(.11f)},{LFO1DEPTH,.4f},{LFO1WAVE,0},{LFO1DEST,8},
        {EG1ATTACK,.5f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.66f},
        {JITTERCENTS,7},{POSTGAIN,-23} };
      add(v, REV(.4f,8000)); presets.push_back({"FM Glass Pad", v}); }

    { std::vector<P> v = { // 10 — digital drift pad, evolving textures
        {VCO1TYPE,99},{VCO1WTSEL,12},{VCO1WTPOS,.4f},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2TYPE,99},{VCO2WTSEL,12},{VCO2WTPOS,.55f},{VCO2GAIN,-4},{VCO2FINE,8},{VCO2EG,0},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.5f},{FILTRES,.1f},{FILTEG,-1},
        {LFO1RATE,L(.13f)},{LFO1DEPTH,.55f},{LFO1WAVE,0},{LFO1DEST,8},
        {LFO2RATE,L(.17f)},{LFO2DEPTH,.5f},{LFO2WAVE,0},{LFO2DEST,9},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.7f},
        {JITTERCENTS,9},{POSTGAIN,-24} };
      add(v, REV(.4f,7000)); presets.push_back({"Digital Drift Pad", v}); }

    int idx = 0;
    for (auto& def : presets) {
        PresetHeader h; h.date = dateBase + idx; h.namelen = (int)strlen(def.name); h.numparams = (int)def.vals.size();
        char path[512]; snprintf(path, sizeof(path), "%s/%lld", outdir.c_str(), fileBase + idx);
        FILE* fd = fopen(path, "wb");
        if (!fd) { fprintf(stderr, "cannot open %s\n", path); return 1; }
        fwrite(&h, 1, sizeof(h), fd);
        fwrite(def.name, 1, h.namelen, fd);
        for (auto& p : def.vals) { int32_t n = p.num; float v = p.val; fwrite(&n, sizeof(n), 1, fd); fwrite(&v, sizeof(v), 1, fd); }
        fclose(fd);
        printf("%s  <- \"%s\" (%d params)\n", path, def.name, h.numparams);
        idx++;
    }
    printf("%d pad presets written.\n", idx);
    return 0;
}
