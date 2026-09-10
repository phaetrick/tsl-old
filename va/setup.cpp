#include <logger.h>
#include <cstdlib>
#include <SkStream.h>

#include "gui.h"
#include "button.h"
#include "setup.h"
#include "tools/aligned_memalloc.h"
#include "infopanel.h"
#include "defines.h"
#include "tools.h"
#include <file.h>
#include "preset.h"
#include "synth.h"
#include "view.h"
#include "knob.h"
#include <Midi.h>
#include "MidiReceiver.h"
#include "Input.h"
#include "grainstorm.h"
#include "callbacks_loop_controls.h"
#include "icon.h"
#include "sequencer.h"
#include "synth.h"
#include <app.h>
#include <settings.h>
#include <tools/PlatformPaths.h>
#include "envelope.h"

#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif

static void initparams(tsl::AppState* _appState) {

    _STATE->parameters[POSTGAIN].min = -60.f;
    // +6 rather than 0: the factory bank is loudness-solved (see preset.cpp), and
    // Pluck Strings needs +3 dB more than a 0 dB ceiling can give to reach the same
    // target as the other fourteen. Deliberately NOT grainstorm's -60..+60 — the
    // host rebuilds this parameter from min/max (IPlugEffect.cpp: internal =
    // min + (max-min)*norm), so widening the range reinterprets every automation
    // lane already recorded against it. At +6 an existing lane shifts 3 dB; at +60
    // the same lane would come back 30 dB hotter, straight into the +-0.99 clip.
    // initvalue stays -30 for the same reason: moving it re-levels every patch that
    // does not set POSTGAIN explicitly, Default included.
    _STATE->parameters[POSTGAIN].max = 6.f;
    _STATE->parameters[POSTGAIN].name = "GAIN";
    _STATE->parameters[POSTGAIN].valuename = "dB";
    _STATE->parameters[POSTGAIN].type = ParameterType_double;
    _STATE->parameters[POSTGAIN].digits = 0;
    _STATE->parameters[POSTGAIN].initvalue = -30.f;
    _STATE->parameters[POSTGAIN].progress = 1;

    // max here is the PAGE COUNT the EG knob view-aliasing loop consumes (knob.cpp
    // walks i = 1..max-1 registering EG1's controls for each later page), not a
    // value bound.
    _STATE->parameters[EGSPACE].max = 4;

    static const char* egCategories[] = { "EG1", "EG2", "EG3", "EG4" };
    for (int eg = 0; eg < 4; eg++) {
        const char* cat = egCategories[eg];
        const int base = EG1ATTACK + eg * 4;
        const bool isEg1 = eg == 0;

        auto& atk = _STATE->parameters[base + 0];
        atk.name = "ATTACK"; atk.category = cat;
        atk.min = 0; atk.max = 1; atk.initvalue = .5f;
        atk.valuename = ""; atk.type = ParameterType_double; atk.digits = 2; atk.progress = .01;
        if (isEg1) { atk.paramOffset = EGSPACE; atk.offsetFact = 4; }

        auto& dec = _STATE->parameters[base + 1];
        dec.name = "DECAY"; dec.category = cat;
        dec.min = 0; dec.max = 1; dec.initvalue = .5f;
        dec.valuename = " "; dec.type = ParameterType_double; dec.digits = 2; dec.progress = .01;
        if (isEg1) { dec.paramOffset = EGSPACE; dec.offsetFact = 4; }

        auto& sus = _STATE->parameters[base + 2];
        sus.name = "SUSTAIN"; sus.category = cat;
        sus.min = 0; sus.max = 1.f; sus.initvalue = .75f;
        sus.valuename = " "; sus.type = ParameterType_double; sus.digits = 2; sus.progress = .05;
        if (isEg1) { sus.paramOffset = EGSPACE; sus.offsetFact = 4; }

        auto& rel = _STATE->parameters[base + 3];
        rel.name = "RELEASE"; rel.category = cat;
        rel.min = 0; rel.max = 1; rel.initvalue = .5f;
        rel.valuename = " "; rel.type = ParameterType_double; rel.digits = 2; rel.progress = .01;
        if (isEg1) { rel.paramOffset = EGSPACE; rel.offsetFact = 4; }
    }

    // EG5 is set up but read by nothing in the engine — kept only so enum positions
    // hold; deliberately absent from midiParams and the host map (see the note there).
    _STATE->parameters[EG5ATTACK].min = 0;
    _STATE->parameters[EG5ATTACK].max = 1;
    _STATE->parameters[EG5ATTACK].name = "ATTACK";
    _STATE->parameters[EG5ATTACK].valuename = "";
    _STATE->parameters[EG5ATTACK].type = ParameterType_double;
    _STATE->parameters[EG5ATTACK].digits = 2;
    _STATE->parameters[EG5ATTACK].initvalue = .5f;
    _STATE->parameters[EG5ATTACK].progress = .01;

    _STATE->parameters[EG5DECAY].min = 0;
    _STATE->parameters[EG5DECAY].max = 1;
    _STATE->parameters[EG5DECAY].name = "DECAY";
    _STATE->parameters[EG5DECAY].valuename = " ";
    _STATE->parameters[EG5DECAY].type = ParameterType_double;
    _STATE->parameters[EG5DECAY].digits = 2;
    _STATE->parameters[EG5DECAY].initvalue = .5f;
    _STATE->parameters[EG5DECAY].progress = .01;

    _STATE->parameters[EG5RELEASE].min = 0;
    _STATE->parameters[EG5RELEASE].max = 1;
    _STATE->parameters[EG5RELEASE].name = "RELEASE";
    _STATE->parameters[EG5RELEASE].valuename = " ";
    _STATE->parameters[EG5RELEASE].type = ParameterType_double;
    _STATE->parameters[EG5RELEASE].digits = 2;
    _STATE->parameters[EG5RELEASE].initvalue = .5f;
    _STATE->parameters[EG5RELEASE].progress = .01;

    _STATE->parameters[EG5SUSTAIN].min = 0;
    _STATE->parameters[EG5SUSTAIN].max = 1.f;
    _STATE->parameters[EG5SUSTAIN].name = "SUSTAIN";
    _STATE->parameters[EG5SUSTAIN].valuename = " ";
    _STATE->parameters[EG5SUSTAIN].type = ParameterType_double;
    _STATE->parameters[EG5SUSTAIN].digits = 2;
    _STATE->parameters[EG5SUSTAIN].initvalue = .75f;
    _STATE->parameters[EG5SUSTAIN].progress = .05;

    _STATE->parameters[VCO1GAIN].min = -60.f;
    _STATE->parameters[VCO1GAIN].max = 60.f;
    _STATE->parameters[VCO1GAIN].name = "GAIN";
    _STATE->parameters[VCO1GAIN].category = "OSC1";
    _STATE->parameters[VCO1GAIN].valuename = "dB";
    _STATE->parameters[VCO1GAIN].type = ParameterType_double;
    _STATE->parameters[VCO1GAIN].digits = 0;
    _STATE->parameters[VCO1GAIN].initvalue = 0.f;
    _STATE->parameters[VCO1GAIN].progress = 1;

    _STATE->parameters[VCO1COARSE].min = 0.f;
    _STATE->parameters[VCO1COARSE].max = 36;
    _STATE->parameters[VCO1COARSE].name = "COARSE";
    _STATE->parameters[VCO1COARSE].category = "OSC1";
    _STATE->parameters[VCO1COARSE].valuename = "SEMITONES";
    _STATE->parameters[VCO1COARSE].type = ParameterType_double;
    _STATE->parameters[VCO1COARSE].digits = 2;
    _STATE->parameters[VCO1COARSE].initvalue = 0.f;
    _STATE->parameters[VCO1COARSE].progress = 1;

    _STATE->parameters[VCO1FINE].min = 0.f;
    _STATE->parameters[VCO1FINE].max = 20.f;
    _STATE->parameters[VCO1FINE].name = "FINE";
    _STATE->parameters[VCO1FINE].category = "OSC1";
    _STATE->parameters[VCO1FINE].valuename = "CENTS";
    _STATE->parameters[VCO1FINE].type = ParameterType_double;
    _STATE->parameters[VCO1FINE].digits = 2;
    _STATE->parameters[VCO1FINE].initvalue = 0.f;
    _STATE->parameters[VCO1FINE].progress = .01;

    _STATE->parameters[VCO1PW].min = 0.f;
    _STATE->parameters[VCO1PW].max = 1.f;
    _STATE->parameters[VCO1PW].name = "PW ";
    _STATE->parameters[VCO1PW].category = "OSC1";
    _STATE->parameters[VCO1PW].valuename = " ";
    _STATE->parameters[VCO1PW].type = ParameterType_double;
    _STATE->parameters[VCO1PW].digits = 2;
    _STATE->parameters[VCO1PW].initvalue = 0.f;
    _STATE->parameters[VCO1PW].progress = .01;

    _STATE->parameters[VCO2GAIN].min = -60.f;
    _STATE->parameters[VCO2GAIN].max = 60.f;
    _STATE->parameters[VCO2GAIN].name = "GAIN";
    _STATE->parameters[VCO2GAIN].category = "OSC2";
    _STATE->parameters[VCO2GAIN].valuename = "dB";
    _STATE->parameters[VCO2GAIN].type = ParameterType_double;
    _STATE->parameters[VCO2GAIN].digits = 0;
    _STATE->parameters[VCO2GAIN].initvalue = 0.f;
    _STATE->parameters[VCO2GAIN].progress = 1;

    _STATE->parameters[VCO2COARSE].min = 0.f;
    _STATE->parameters[VCO2COARSE].max = 36;
    _STATE->parameters[VCO2COARSE].name = "COARSE";
    _STATE->parameters[VCO2COARSE].category = "OSC2";
    _STATE->parameters[VCO2COARSE].valuename = "SEMITONES";
    _STATE->parameters[VCO2COARSE].type = ParameterType_double;
    _STATE->parameters[VCO2COARSE].digits = 2;
    _STATE->parameters[VCO2COARSE].initvalue = 0.f;
    _STATE->parameters[VCO2COARSE].progress = 1;

    _STATE->parameters[VCO2FINE].min = 0.f;
    _STATE->parameters[VCO2FINE].max = 20.f;
    _STATE->parameters[VCO2FINE].name = "FINE";
    _STATE->parameters[VCO2FINE].category = "OSC2";
    _STATE->parameters[VCO2FINE].valuename = "CENTS";
    _STATE->parameters[VCO2FINE].type = ParameterType_double;
    _STATE->parameters[VCO2FINE].digits = 2;
    _STATE->parameters[VCO2FINE].initvalue = 0.f;
    _STATE->parameters[VCO2FINE].progress = .01;

    _STATE->parameters[VCO2PW].min = 0.f;
    _STATE->parameters[VCO2PW].max = 1.f;
    _STATE->parameters[VCO2PW].name = "PW ";
    _STATE->parameters[VCO2PW].category = "OSC2";
    _STATE->parameters[VCO2PW].valuename = " ";
    _STATE->parameters[VCO2PW].type = ParameterType_double;
    _STATE->parameters[VCO2PW].digits = 2;
    _STATE->parameters[VCO2PW].initvalue = 0.f;
    _STATE->parameters[VCO2PW].progress = .01;

    _STATE->parameters[VCO3GAIN].min = -60.f;
    _STATE->parameters[VCO3GAIN].max = 60.f;
    _STATE->parameters[VCO3GAIN].name = "GAIN";
    _STATE->parameters[VCO3GAIN].category = "SUB";
    _STATE->parameters[VCO3GAIN].valuename = "dB";
    _STATE->parameters[VCO3GAIN].type = ParameterType_double;
    _STATE->parameters[VCO3GAIN].digits = 0;
    _STATE->parameters[VCO3GAIN].initvalue = 0.f;
    _STATE->parameters[VCO3GAIN].progress = 1;

    _STATE->parameters[VCO3COARSE].min = -5.f;
    _STATE->parameters[VCO3COARSE].max = 5.f;
    _STATE->parameters[VCO3COARSE].name = "COARSE";
    _STATE->parameters[VCO3COARSE].category = "SUB";
    _STATE->parameters[VCO3COARSE].valuename = "OCTAVES";
    _STATE->parameters[VCO3COARSE].type = ParameterType_double;
    _STATE->parameters[VCO3COARSE].digits = 2;
    _STATE->parameters[VCO3COARSE].initvalue = 0;
    _STATE->parameters[VCO3COARSE].progress = 1;

    _STATE->parameters[VCO3COARSEST].min = -60.f;
    _STATE->parameters[VCO3COARSEST].max = 60.f;
    _STATE->parameters[VCO3COARSEST].name = "COARSE";
    _STATE->parameters[VCO3COARSEST].category = "SUB";
    _STATE->parameters[VCO3COARSEST].valuename = "SEMITONES";
    _STATE->parameters[VCO3COARSEST].type = ParameterType_double;
    _STATE->parameters[VCO3COARSEST].digits = 0;
    _STATE->parameters[VCO3COARSEST].initvalue = 0.f;
    _STATE->parameters[VCO3COARSEST].progress = 1;
    // 0 semitones is in tune, so the fill runs out from centre — up for sharp, down
    // for flat. (OSC1/OSC2 COARSE are 0..36, unipolar, and keep their min anchor.)
    _STATE->parameters[VCO3COARSEST].flags |= Param::CentreFill;

    _STATE->parameters[VCO3FINE].min = 0.f;
    _STATE->parameters[VCO3FINE].max = 20.f;
    _STATE->parameters[VCO3FINE].name = "FINE";
    _STATE->parameters[VCO3FINE].category = "SUB";
    _STATE->parameters[VCO3FINE].valuename = "CENTS";
    _STATE->parameters[VCO3FINE].type = ParameterType_double;
    _STATE->parameters[VCO3FINE].digits = 2;
    _STATE->parameters[VCO3FINE].initvalue = 0.f;
    _STATE->parameters[VCO3FINE].progress = .01;

    _STATE->parameters[VCO3PW].min = 0.f;
    _STATE->parameters[VCO3PW].max = 1.f;
    _STATE->parameters[VCO3PW].name = "PW ";
    _STATE->parameters[VCO3PW].category = "SUB";
    _STATE->parameters[VCO3PW].valuename = " ";
    _STATE->parameters[VCO3PW].type = ParameterType_double;
    _STATE->parameters[VCO3PW].digits = 2;
    _STATE->parameters[VCO3PW].initvalue = 0.f;
    _STATE->parameters[VCO3PW].progress = .01;

    _STATE->parameters[FILTCENTER].min = LOG10D20(20.f);
    _STATE->parameters[FILTCENTER].max = LOG10D20(_STATE->sr * .5f);
    _STATE->parameters[FILTCENTER].category = "FILTER";
    _STATE->parameters[FILTCENTER].name = "CUT";
    _STATE->parameters[FILTCENTER].valuename = "Hz";
    _STATE->parameters[FILTCENTER].type = ParameterType_double;
    _STATE->parameters[FILTCENTER].digits = 0;
    _STATE->parameters[FILTCENTER].initvalue = LOG10D20(_STATE->sr * .5f);
    _STATE->parameters[FILTCENTER].progress = 1;
    _STATE->parameters[FILTCENTER].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[FILTRES].min = 0;
    _STATE->parameters[FILTRES].max = 1.f;
    _STATE->parameters[FILTRES].category = "FILTER";
    _STATE->parameters[FILTRES].name = "RES";
    _STATE->parameters[FILTRES].valuename = " ";
    _STATE->parameters[FILTRES].type = ParameterType_double;
    _STATE->parameters[FILTRES].digits = 2;
    _STATE->parameters[FILTRES].initvalue = 0.f;
    _STATE->parameters[FILTRES].progress = .01;

    // 0..6 match HuovilainenMoog::Mode (0=LP4 .. 6=NOTCH). 7..15 are the ZDF
    // filters from <audio/ZdfFilters.h>, dispatched in VcoNote::tickFilter:
    // LAD = 4-pole transistor ladder, SEM = 2-pole state variable, 303 =
    // 3-pole acid ladder. 7/8/9 are their low-pass forms, 10..15 the band and
    // high-pass taps. Appended at the end so existing presets keep their mode.
    // 0..6 are the original filter and are frozen — released presets index into this.
    // 7..16 never shipped, so they are grouped by family: ladder, state-variable, acid.
    // Keep in step with filtmodenames in gui.cpp — the host list and the GUI list have
    // to read the same. See the note there for why the first seven stay abbreviated.
    static const std::string_view filtModeNames[] = {"LP4", "LP2", "BP2", "BP4", "HP2", "HP4", "NOTCH",
                                                     "LADDER LP", "LADDER BP", "LADDER HP",
                                                     "SEM LP", "SEM BP", "SEM HP", "SEM NOTCH",
                                                     "303 LP", "303 BP", "303 HP",
                                                     "MODAL"};
    _STATE->parameters[FILT_MODE].category = "FILTER";
    _STATE->parameters[FILT_MODE].name = "MODE";
    _STATE->parameters[FILT_MODE].min = 0.f;
    _STATE->parameters[FILT_MODE].max = 17.f;
    _STATE->parameters[FILT_MODE].initvalue = 0.f; // LP4, matches legacy behaviour
    _STATE->parameters[FILT_MODE].type = ParameterType_enum;
    _STATE->parameters[FILT_MODE].names = filtModeNames;

    _STATE->parameters[NOISEGAIN].min = -60.f;
    _STATE->parameters[NOISEGAIN].max = 60.f;
    _STATE->parameters[NOISEGAIN].category = "NOISE";
    _STATE->parameters[NOISEGAIN].name = "GAIN";
    _STATE->parameters[NOISEGAIN].valuename = "dB";
    _STATE->parameters[NOISEGAIN].type = ParameterType_double;
    _STATE->parameters[NOISEGAIN].digits = 0;
    _STATE->parameters[NOISEGAIN].initvalue = 0.f;
    _STATE->parameters[NOISEGAIN].progress = 1;

    // values -1,0,1,2 map to NoiseSource::mode OFF/WHITE/PINK/BROWN. The .values
    // array is NOT optional here: to/fromNormalized and the MIDI-note stepper index
    // names/values, and without .values they treat the raw value as the index — a
    // min of -1 then shifts everything by one, so the host could never represent
    // OFF and every reload turned OFF into WHITE (audit finding H3).
    static const std::string_view noiseModeNames[] = {"OFF", "WHITE", "PINK", "BROWN"};
    static const std::vector<float> noiseModeValues = {-1, 0, 1, 2};
    _STATE->parameters[NOISEMODE].category = "NOISE";
    _STATE->parameters[NOISEMODE].name = "MODE";
    _STATE->parameters[NOISEMODE].min = -1.f;
    _STATE->parameters[NOISEMODE].max = 2.f;
    _STATE->parameters[NOISEMODE].initvalue = -1.f;
    _STATE->parameters[NOISEMODE].type = ParameterType_enum;
    _STATE->parameters[NOISEMODE].names = noiseModeNames;
    _STATE->parameters[NOISEMODE].values = noiseModeValues;

    // Gates the noise level. -1 = NONE means no envelope: the level holds for as long as
    // the voice lives, which is what a steady noise bed under an enveloped oscillator
    // wants. names/values are assigned with the other amp EGs further down.
    _STATE->parameters[NOISEEG].category = "NOISE";
    _STATE->parameters[NOISEEG].name = "AMP EG";
    _STATE->parameters[NOISEEG].min = -1.f;
    _STATE->parameters[NOISEEG].max = 3.f;
    _STATE->parameters[NOISEEG].initvalue = 0.f;
    _STATE->parameters[NOISEEG].type = ParameterType_enum;

    _STATE->parameters[VCO1TYPE].category = "OSC1"; _STATE->parameters[VCO1TYPE].name = "WAVE";
    _STATE->parameters[VCO1EG].category = "OSC1"; _STATE->parameters[VCO1EG].name = "AMP EG";
    _STATE->parameters[VCO1TUNEEG].category = "OSC1"; _STATE->parameters[VCO1TUNEEG].name = "TUNE EG";
    _STATE->parameters[VCO1SYNC].category = "OSC1"; _STATE->parameters[VCO1SYNC].name = "SYNC";
    _STATE->parameters[VCO2TYPE].category = "OSC2"; _STATE->parameters[VCO2TYPE].name = "WAVE";
    _STATE->parameters[VCO2EG].category = "OSC2"; _STATE->parameters[VCO2EG].name = "AMP EG";
    _STATE->parameters[VCO2TUNEEG].category = "OSC2"; _STATE->parameters[VCO2TUNEEG].name = "TUNE EG";
    _STATE->parameters[VCO2SYNC].category = "OSC2"; _STATE->parameters[VCO2SYNC].name = "SYNC";
    _STATE->parameters[VCO3TYPE].category = "SUB"; _STATE->parameters[VCO3TYPE].name = "WAVE";
    _STATE->parameters[VCO3EG].category = "SUB"; _STATE->parameters[VCO3EG].name = "AMP EG";
    _STATE->parameters[VCO3TUNEEG].category = "SUB"; _STATE->parameters[VCO3TUNEEG].name = "TUNE EG";
    _STATE->parameters[VCO3RINGMOD].category = "SUB"; _STATE->parameters[VCO3RINGMOD].name = "RINGMOD";
    _STATE->parameters[VCO3NORMAL].category = "SUB"; _STATE->parameters[VCO3NORMAL].name = "NORMAL";
    // SUB modulation routings (all independent toggles like NORMAL/RINGMOD):
    // AM = unipolar amplitude mod of the main pair, PM = phase-mod of VCO1/VCO2.
    _STATE->parameters[VCO3AM].category = "SUB"; _STATE->parameters[VCO3AM].name = "AM";
    _STATE->parameters[VCO3PM].category = "SUB"; _STATE->parameters[VCO3PM].name = "PM";
    _STATE->parameters[VCO3AMDEPTH].category = "SUB"; _STATE->parameters[VCO3AMDEPTH].name = "AM DEPTH";
    _STATE->parameters[VCO3AMDEPTH].min = 0.f; _STATE->parameters[VCO3AMDEPTH].max = 1.f;
    _STATE->parameters[VCO3AMDEPTH].type = ParameterType_double; _STATE->parameters[VCO3AMDEPTH].digits = 2;
    _STATE->parameters[VCO3AMDEPTH].initvalue = 0.f; _STATE->parameters[VCO3AMDEPTH].progress = .01;
    _STATE->parameters[VCO3AMDEPTH].valuename = " ";
    _STATE->parameters[VCO3PMDEPTH].category = "SUB"; _STATE->parameters[VCO3PMDEPTH].name = "PM DEPTH";
    _STATE->parameters[VCO3PMDEPTH].min = 0.f; _STATE->parameters[VCO3PMDEPTH].max = 1.f;
    _STATE->parameters[VCO3PMDEPTH].type = ParameterType_double; _STATE->parameters[VCO3PMDEPTH].digits = 2;
    _STATE->parameters[VCO3PMDEPTH].initvalue = 0.f; _STATE->parameters[VCO3PMDEPTH].progress = .01;
    _STATE->parameters[VCO3PMDEPTH].valuename = " ";
    _STATE->parameters[FILTEG].category = "FILTER";
    _STATE->parameters[FILTEG].name = "EG";
    _STATE->parameters[RESEG].category = "FILTER";
    _STATE->parameters[RESEG].name = "RES EG";
    _STATE->parameters[RESEG].initvalue = -1;
    // NONE, matching RESEG — a modulation ROUTE should default to off. It used to
    // default to EG1, which put a filter envelope on every patch that never asked
    // for one. The cutoff is built UP from the played note (gains[4] = filtBase +
    // rest*filtEgVal), so when the envelope bottoms out the cutoff sits on the
    // fundamental: at C4 that is merely dull, but at C2 the surviving fundamental
    // needs ~20 dB more level to sound as loud, so the note reads as silent.
    // Presets are sparse against initvalue, so this changes what a stored patch
    // means — readOnePreset migrates files written at version <= 3.
    _STATE->parameters[FILTEG].initvalue = -1;
    _STATE->parameters[REVPOW].name = "POWER";   _STATE->parameters[REVPOW].category = "REVERB";
    _STATE->parameters[CDELPOW].name = "POWER";   _STATE->parameters[CDELPOW].category = "DELAY";
    _STATE->parameters[ARP_MODE].category = "ARP";
    _STATE->parameters[ARP_MODE].name = "MODE";
    // Same shape as NOISEMODE and for the same reason: -1 (OFF) is only
    // representable with a .values array, and without one the host registered this
    // as a one-value param whose restore switched the arp ON (audit finding H4).
    // gui.cpp's selector lists must match this order exactly.
    static const std::string_view arpModeNames[] = {"OFF", "UP", "DOWN", "UP/DOWN", "BOUNCE"};
    static const std::vector<float> arpModeValues = {-1, 0, 1, 2, 3};
    _STATE->parameters[ARP_MODE].min = -1.f;
    _STATE->parameters[ARP_MODE].max = 3.f;
    _STATE->parameters[ARP_MODE].names = arpModeNames;
    _STATE->parameters[ARP_MODE].values = arpModeValues;

    static constexpr int enumParams[] = {
        VCO1TYPE, VCO1EG, VCO1TUNEEG, VCO1SYNC,
        VCO2TYPE, VCO2EG, VCO2TUNEEG, VCO2SYNC,
        VCO3TYPE, VCO3EG, VCO3TUNEEG,
        FILTEG, RESEG, ARP_MODE,
    };
    for (int id : enumParams)
        _STATE->parameters[id].type = ParameterType_enum;

    // 99 = wavetable mode, 98 = PADsynth (Vco::check special-cases both).
    // names required alongside values (see filtEgNames note below) so the IPlug
    // host param range is sized correctly and state save/restore round-trips.
#if PA_ENABLE_PAD
    static const std::vector<float> vcoTypeValues = {-1, 4, 0, 2, 99, 97, 98};
    static const std::string_view vcoTypeNames[] = {"SINE", "TRI", "SAW", "PULSE", "WT", "MODAL", "PAD"};
#else
    static const std::vector<float> vcoTypeValues = {-1, 4, 0, 2, 99, 97};
    static const std::string_view vcoTypeNames[] = {"SINE", "TRI", "SAW", "PULSE", "WT", "MODAL"};
#endif
    _STATE->parameters[VCO1TYPE].values = vcoTypeValues;
    _STATE->parameters[VCO1TYPE].names = vcoTypeNames;
    _STATE->parameters[VCO2TYPE].values = vcoTypeValues;
    _STATE->parameters[VCO2TYPE].names = vcoTypeNames;
    _STATE->parameters[VCO3TYPE].values = vcoTypeValues;
    _STATE->parameters[VCO3TYPE].names = vcoTypeNames;

    // Wavetable morph position (0..1), one per oscillator. Modulated per-sample in
    // synth.cpp (base + the osc's PW-EG source + LFO — repurposed, since a WT osc
    // has no pulse width).
    auto setupWtPos = [&](int id, const char* cat) {
        _STATE->parameters[id].category = cat;
        _STATE->parameters[id].subcategory = "MORPH";
        _STATE->parameters[id].name = "A";
        _STATE->parameters[id].min = 0.f; _STATE->parameters[id].max = 1.f;
        _STATE->parameters[id].type = ParameterType_double; _STATE->parameters[id].digits = 2;
        _STATE->parameters[id].initvalue = 0.f; _STATE->parameters[id].progress = .01;
        _STATE->parameters[id].valuename = " ";
    };
    setupWtPos(VCO1WTPOS, "OSC1");
    setupWtPos(VCO2WTPOS, "OSC2");
    setupWtPos(VCO3WTPOS, "SUB");

    // MORPH TO (= B): modulation moves MORPH (= A = WTPOS) toward this target. B<A
    // inverts the sweep. Default 1 = old behaviour (mod pushes morph toward full).
    auto setupMorphTo = [&](int id, const char* cat) {
        _STATE->parameters[id].category = cat;
        _STATE->parameters[id].subcategory = "MORPH";
        _STATE->parameters[id].name = "B";
        _STATE->parameters[id].min = 0.f; _STATE->parameters[id].max = 1.f;
        _STATE->parameters[id].type = ParameterType_double; _STATE->parameters[id].digits = 2;
        _STATE->parameters[id].initvalue = 1.f; _STATE->parameters[id].progress = .01;
        _STATE->parameters[id].valuename = " ";
    };
    setupMorphTo(VCO1MORPHTO, "OSC1");
    setupMorphTo(VCO2MORPHTO, "OSC2");
    setupMorphTo(VCO3MORPHTO, "SUB");

    // Wavetable selection (which built-in table), one per oscillator.
    // Slots 7/19/21/22/26 were FOLD/CZ SAW/SYNC/SATURATE/CRUSH — measured redundant
    // with the WARP dimension (sine+FOLD, sine+BEND, saw+SYNC, sine+SAT, BITS) and
    // replaced IN PLACE (enum positions are stored in presets — never renumber).
    //
    // INDEXED BY VALUE: entry i is the name of table i, so this stays in id order.
    // wtselnames in gui.cpp is a DISPLAY list with its own values array, grouped by
    // family for the user, and the two are deliberately in different orders. Do not
    // reorder this one to match — it would rename every table instead of moving it.
    static const std::string_view wtNames[] = {"BASIC", "CLIMB", "ODD", "FORMANT",
                                               "VOWEL", "METAL", "PULSE", "SOFT",
                                               "ORGAN", "FIFTHS", "GROWL", "CHIME",
                                               "DIGITAL", "FM", "COMB", "SHAPER",
                                               "SWEEP", "HOLLOW", "AIR", "NOISE", "CZ RES",
                                               "FM2", "PLUCK", "RESO", "BUZZ",
                                               "EPIANO", "STACK"};
    auto setupWtSel = [&](int id, const char* cat) {
        _STATE->parameters[id].category = cat;
        _STATE->parameters[id].name = "TABLE";
        _STATE->parameters[id].min = 0.f; _STATE->parameters[id].max = 26.f;
        _STATE->parameters[id].initvalue = 0.f;
        _STATE->parameters[id].type = ParameterType_enum;
        _STATE->parameters[id].names = wtNames;
    };
    setupWtSel(VCO1WTSEL, "OSC1");
    setupWtSel(VCO2WTSEL, "OSC2");
    setupWtSel(VCO3WTSEL, "SUB");

    // Modal resonator bank (osc type 97). All five are runtime controls — the bank
    // is 14 complex rotations with no table behind it, so turning any of these while
    // a note rings is legal and cheap. CHARACTER and POSITION restructure the bank,
    // so they take effect at the next note-on; DECAY / BRIGHT / HARDNESS are picked
    // up by the block-rate coefficient refresh and act immediately.
    static const std::string_view modalNames[] = {"DEEP", "STRING", "TINE",
                                                  "WOOD", "BELL", "GLASS"};
    auto setupModal = [&](int chId, int decId, int brtId, int hrdId, int posId, const char* cat) {
        _STATE->parameters[chId].category = cat;
        _STATE->parameters[chId].name = "BODY";
        _STATE->parameters[chId].min = 0.f; _STATE->parameters[chId].max = 5.f;
        _STATE->parameters[chId].initvalue = 4.f;   // BELL — the one that shows what this is
        _STATE->parameters[chId].type = ParameterType_enum;
        _STATE->parameters[chId].names = modalNames;

        // The fundamental's T60. Beware the bottom of this range: bandwidth is
        // ~2.2/T60 Hz, so 4 s is a 0.55 Hz skirt and 0.03 s is 73 Hz — the same knob
        // spans "bell" and "formant filter". Log curve so the short end is reachable.
        _STATE->parameters[decId].category = cat;
        _STATE->parameters[decId].name = "DECAY";
        // A Log10 param stores its value ALREADY ENCODED as 20*log10(seconds) —
        // refreshSynthParams decodes it with LOG2NORMALF (10^(v/20)) on the way into
        // synth_params. So min/max/initvalue must be wrapped in LOG10D20, exactly as
        // CDELDEL does. Writing raw seconds here silently rescales the whole knob:
        // 0.03..8 was read as 20*log10 values and decoded to a 1.00..2.51 s range, so
        // the top of the fader gave a 2.5 s tail instead of 8 s.
        _STATE->parameters[decId].min = LOG10D20(0.03f); _STATE->parameters[decId].max = LOG10D20(8.f);
        _STATE->parameters[decId].initvalue = LOG10D20(2.f);
        _STATE->parameters[decId].type = ParameterType_double;
        _STATE->parameters[decId].paramCurve = Param::ParamCurve::Log10;
        // Step is in the encoded (dB-like) domain, so 1 unit, not 0.01 s — same as
        // CDELDEL. The span is LOG10D20(0.03)..LOG10D20(8) = -30.5..18.1, ~49 steps.
        _STATE->parameters[decId].progress = 1.f; _STATE->parameters[decId].digits = 2;
        _STATE->parameters[decId].valuename = "s";

        // Tilts the initial partial amplitudes. Measured in the generative engine:
        // past ~0.15 here partial 8 lands at -50 dB and the whole thing goes muffled.
        _STATE->parameters[brtId].category = cat;
        _STATE->parameters[brtId].name = "BRIGHT";
        _STATE->parameters[brtId].min = 0.f; _STATE->parameters[brtId].max = 1.f;
        _STATE->parameters[brtId].initvalue = 0.5f;
        _STATE->parameters[brtId].type = ParameterType_double;
        _STATE->parameters[brtId].progress = 0.01f; _STATE->parameters[brtId].digits = 2;
        _STATE->parameters[brtId].valuename = "";

        // Contact time as a spectral tilt: a soft mallet never delivers energy to the
        // high modes. VEL_TO_AMP's sibling — velocity is added to this in synth.cpp,
        // so set it low and let the keyboard open the top up.
        _STATE->parameters[hrdId].category = cat;
        _STATE->parameters[hrdId].name = "MALLET";
        _STATE->parameters[hrdId].min = 0.f; _STATE->parameters[hrdId].max = 1.f;
        _STATE->parameters[hrdId].initvalue = 0.55f;
        _STATE->parameters[hrdId].type = ParameterType_double;
        _STATE->parameters[hrdId].progress = 0.01f; _STATE->parameters[hrdId].digits = 2;
        _STATE->parameters[hrdId].valuename = "";

        // A mode with a node at the strike point cannot be excited: |sin(pi*ratio*x)|.
        // 0.5 is dead centre, which nulls every even partial — that is why a
        // centre-struck bar sounds hollow, and it is worth having on a knob.
        _STATE->parameters[posId].category = cat;
        _STATE->parameters[posId].name = "STRIKE";
        _STATE->parameters[posId].min = 0.f; _STATE->parameters[posId].max = 0.5f;
        _STATE->parameters[posId].initvalue = 0.24f;
        _STATE->parameters[posId].type = ParameterType_double;
        _STATE->parameters[posId].progress = 0.01f; _STATE->parameters[posId].digits = 2;
        _STATE->parameters[posId].valuename = "";
    };
    setupModal(VCO1MODALCH, VCO1MODALDEC, VCO1MODALBRT, VCO1MODALHRD, VCO1MODALPOS, "OSC1");
    setupModal(VCO2MODALCH, VCO2MODALDEC, VCO2MODALBRT, VCO2MODALHRD, VCO2MODALPOS, "OSC2");
    setupModal(VCO3MODALCH, VCO3MODALDEC, VCO3MODALBRT, VCO3MODALHRD, VCO3MODALPOS, "SUB");

    // Modal FILTER (mode 17). Deliberately only two params: CUT is the bank's
    // fundamental and RESO is its decay, so everything already routed to the filter
    // — FILTEG, KEYTRACK, VEL/AT/MW, the LFOs — modulates the resonator for free.
    _STATE->parameters[FILTMODALBODY].category = "FILTER";
    _STATE->parameters[FILTMODALBODY].name = "BODY";
    _STATE->parameters[FILTMODALBODY].min = 0.f;
    _STATE->parameters[FILTMODALBODY].max = 5.f;
    // STRING by default, not BELL: the filter is usually fed the oscillators, and a
    // harmonic source can only excite modes that land on its harmonics. BELL and
    // GLASS are the ones to reach for once NOISE is in the mix.
    _STATE->parameters[FILTMODALBODY].initvalue = 1.f;
    _STATE->parameters[FILTMODALBODY].type = ParameterType_enum;
    _STATE->parameters[FILTMODALBODY].names = modalNames;

    _STATE->parameters[FILTMODALPOS].category = "FILTER";
    _STATE->parameters[FILTMODALPOS].name = "STRIKE";
    _STATE->parameters[FILTMODALPOS].min = 0.f;
    _STATE->parameters[FILTMODALPOS].max = 0.5f;
    _STATE->parameters[FILTMODALPOS].initvalue = 0.24f;
    _STATE->parameters[FILTMODALPOS].type = ParameterType_double;
    _STATE->parameters[FILTMODALPOS].progress = 0.01f;
    _STATE->parameters[FILTMODALPOS].digits = 2;
    _STATE->parameters[FILTMODALPOS].valuename = "";

#if PA_ENABLE_PAD
    // PADsynth. The table list is a curated subset of the 27 above — see PAD_TABLES
    // in vco.cpp for why the other 14 are not offered. These are all BUILD-time:
    // touching any of them rebuilds ~2.4 MB of tables on the UiTasks worker, so they
    // are design controls, not performance controls. MORPH is the modulatable one
    // and lives on VCOxWTPOS, shared with the WT oscillator.
    // INDEXED BY VALUE, so this list is in PAD_TABLES order and must stay that way —
    // entry i is the name of table i. Keep the CONTENTS in step with PAD_TABLES in
    // vco.cpp.
    //
    // It is deliberately NOT in the same ORDER as padselnames in gui.cpp, which is a
    // display list carrying its own values array and is sorted soft→hard for the user.
    // The two disagreeing is intended: do not "fix" it by reordering this one, which
    // would rename every table without moving it. The `max` below is a THIRD place the
    // count lives; shortening the list without it leaves the extra slots selectable and
    // reading past the end of every per-table array in vco.cpp.
    static const std::string_view padNames[] = {"BASIC", "SOFT", "BUZZ", "ODD",
                                                "VOWEL", "FORMANT", "ORGAN", "CHIME",
                                                "FIFTHS", "SHAPER", "FM", "EPIANO",
                                                "STACK", "METAL", "HOLLOW", "FM2",
                                                "GROWL", "AIR"};
    // Matches pwmodsrcnames in gui.cpp — the selector there reads its own copy.
    static const std::string_view padMorphEgNames[] = {"OFF", "EG1", "EG2", "EG3", "EG4"};
    auto setupPad = [&](int selId, int bwId, int bwsId, int strId, int seedId,
                        int posId, int mtoId, int megId, const char* cat) {
        _STATE->parameters[selId].category = cat;
        _STATE->parameters[selId].name = "PAD TBL";
        _STATE->parameters[selId].min = 0.f;
        _STATE->parameters[selId].max = (float)(sizeof(padNames) / sizeof(padNames[0]) - 1);
        _STATE->parameters[selId].initvalue = 0.f;
        _STATE->parameters[selId].type = ParameterType_enum;
        _STATE->parameters[selId].names = padNames;

        // Bandwidth is 0..1, NOT cents. The underlying smear is still in cents (which
        // is what makes it scale-invariant — a table transposed inside its key region
        // keeps its character), but the cents a given knob position means are chosen
        // per table by padBandwidthCents. Both ENDS are the same on every table — 0 is
        // clean, 1 is a full wash where the fundamental itself merges — and half travel
        // is where that particular table stops being a tone. See PAD_BW_NID in vco.cpp.
        _STATE->parameters[bwId].category = cat;
        _STATE->parameters[bwId].name = "BANDW";
        _STATE->parameters[bwId].min = 0.f; _STATE->parameters[bwId].max = 1.f;
        _STATE->parameters[bwId].initvalue = 0.35f;
        _STATE->parameters[bwId].type = ParameterType_double;
        _STATE->parameters[bwId].progress = 0.01f; _STATE->parameters[bwId].digits = 2;
        _STATE->parameters[bwId].valuename = "";

        // Growth exponent: 1 keeps the smear constant in cents up the series; below
        // 1 holds the top harmonics tight (string section), above 1 washes them out.
        _STATE->parameters[bwsId].category = cat;
        _STATE->parameters[bwsId].name = "BW SCL";
        _STATE->parameters[bwsId].min = 0.5f; _STATE->parameters[bwsId].max = 2.f;
        _STATE->parameters[bwsId].initvalue = 1.f;
        _STATE->parameters[bwsId].type = ParameterType_double;
        _STATE->parameters[bwsId].progress = 0.05f; _STATE->parameters[bwsId].digits = 2;
        _STATE->parameters[bwsId].valuename = "";

        _STATE->parameters[strId].category = cat;
        _STATE->parameters[strId].name = "STRETCH";
        _STATE->parameters[strId].min = 0.f; _STATE->parameters[strId].max = 1.f;
        _STATE->parameters[strId].initvalue = 0.f;
        _STATE->parameters[strId].type = ParameterType_double;
        _STATE->parameters[strId].progress = 0.01f; _STATE->parameters[strId].digits = 2;
        _STATE->parameters[strId].valuename = "";

        _STATE->parameters[seedId].category = cat;
        _STATE->parameters[seedId].name = "SEED";
        _STATE->parameters[seedId].min = 0.f; _STATE->parameters[seedId].max = 15.f;
        _STATE->parameters[seedId].initvalue = 0.f;
        _STATE->parameters[seedId].type = ParameterType_double;
        _STATE->parameters[seedId].progress = 1.f; _STATE->parameters[seedId].digits = 0;
        _STATE->parameters[seedId].valuename = " ";

        // PAD's OWN morph A/B and EG source — no longer the wavetable oscillator's
        // VCOxWTPOS / VCOxMORPHTO / VCOxPWMODSRC. Same shape as setupWtPos and
        // setupMorphTo so the two read identically; the subcategory differs so the host
        // sees distinct names.
        //
        // A and B are BUILD inputs here in a way they are not for WT: the four baked
        // morph levels span A→B, so moving either rebuilds the set (quantised through
        // padQuantMorph first). The modulated position between them rides pw.
        for (int end = 0; end < 2; end++) {
            const int id = end ? mtoId : posId;
            _STATE->parameters[id].category = cat;
            _STATE->parameters[id].subcategory = "PAD MORPH";
            _STATE->parameters[id].name = end ? "B" : "A";
            _STATE->parameters[id].min = 0.f; _STATE->parameters[id].max = 1.f;
            _STATE->parameters[id].type = ParameterType_double;
            _STATE->parameters[id].digits = 2; _STATE->parameters[id].progress = .01;
            _STATE->parameters[id].initvalue = end ? 1.f : 0.f;
            _STATE->parameters[id].valuename = " ";
        }
        // Deliberately NOT flagged NoAssignment, unlike the WT counterpart it replaced.
        // That flag excludes a parameter from presets AND from MIDI learn (preset.cpp,
        // EventHandler.cpp), so VCOxPWMODSRC — the WT morph EG source — is not saved
        // with a patch at all. Whether that is intended for WT is a separate question;
        // PAD's morph routing is part of the sound and is saved.
        _STATE->parameters[megId].category = cat;
        _STATE->parameters[megId].subcategory = "PAD MORPH";
        _STATE->parameters[megId].name = "EG";
        _STATE->parameters[megId].min = 0.f; _STATE->parameters[megId].max = 4.f;
        _STATE->parameters[megId].initvalue = 0.f;
        _STATE->parameters[megId].type = ParameterType_enum;
        _STATE->parameters[megId].names = padMorphEgNames;
    };
    setupPad(VCO1PADSEL, VCO1PADBW, VCO1PADBWSC, VCO1PADSTR, VCO1PADSEED,
             VCO1PADPOS, VCO1PADMTO, VCO1PADMEG, "OSC1");
    setupPad(VCO2PADSEL, VCO2PADBW, VCO2PADBWSC, VCO2PADSTR, VCO2PADSEED,
             VCO2PADPOS, VCO2PADMTO, VCO2PADMEG, "OSC2");
    setupPad(VCO3PADSEL, VCO3PADBW, VCO3PADBWSC, VCO3PADSTR, VCO3PADSEED,
             VCO3PADPOS, VCO3PADMTO, VCO3PADMEG, "SUB");
#endif // PA_ENABLE_PAD

    // Unison: per-oscillator detuned voice stacking. VOICES=1 is off (no extra cost).
    // DETUNE spreads the copies in cents, BLEND sets the side-voice level vs centre.
    auto setupUnison = [&](int voicesId, int detuneId, int blendId, const char* cat) {
        _STATE->parameters[voicesId].category = cat;
        _STATE->parameters[voicesId].name = "VOICES";
        _STATE->parameters[voicesId].min = 1.f; _STATE->parameters[voicesId].max = 7.f;
        _STATE->parameters[voicesId].initvalue = 1.f;
        _STATE->parameters[voicesId].type = ParameterType_double;
        _STATE->parameters[voicesId].progress = 1; _STATE->parameters[voicesId].digits = 0;
        _STATE->parameters[voicesId].valuename = " ";

        _STATE->parameters[detuneId].category = cat;
        _STATE->parameters[detuneId].name = "DETUNE";
        _STATE->parameters[detuneId].min = 0.f; _STATE->parameters[detuneId].max = 1.f;
        _STATE->parameters[detuneId].initvalue = 0.2f;
        _STATE->parameters[detuneId].type = ParameterType_double;
        _STATE->parameters[detuneId].digits = 2; _STATE->parameters[detuneId].progress = .01;
        _STATE->parameters[detuneId].valuename = " ";

        _STATE->parameters[blendId].category = cat;
        _STATE->parameters[blendId].name = "BLEND";
        _STATE->parameters[blendId].min = 0.f; _STATE->parameters[blendId].max = 1.f;
        _STATE->parameters[blendId].initvalue = 0.75f;
        _STATE->parameters[blendId].type = ParameterType_double;
        _STATE->parameters[blendId].digits = 2; _STATE->parameters[blendId].progress = .01;
        _STATE->parameters[blendId].valuename = " ";
    };
    setupUnison(VCO1UNIVOICES, VCO1UNIDETUNE, VCO1UNIBLEND, "OSC1");
    setupUnison(VCO2UNIVOICES, VCO2UNIDETUNE, VCO2UNIBLEND, "OSC2");
    setupUnison(VCO3UNIVOICES, VCO3UNIDETUNE, VCO3UNIBLEND, "SUB");

    // Warp: a runtime transform on the wavetable read phase (works on any table).
    // Slot 3 was ASYM (0.98 similar to BEND → replaced by RING, baked ring-mod);
    // slot 5 was CRUSH (baked bitcrush is bandlimited to silence by design; BITS is
    // the audible one → replaced by FORMANT, read-faster-and-hold). In-place only.
    static const std::string_view warpNames[] = {"OFF", "SYNC", "BEND", "RING", "FOLD", "FORMANT",
                                                  "BITS", "RATE", "DRIVE", "SAT", "STEP"};
    auto setupWarp = [&](int typeId, int amtId, int egId, int toId, const char* cat) {
        _STATE->parameters[typeId].category = cat;
        _STATE->parameters[typeId].name = "WARP";
        _STATE->parameters[typeId].min = 0.f; _STATE->parameters[typeId].max = 10.f;
        _STATE->parameters[typeId].initvalue = 0.f;
        _STATE->parameters[typeId].type = ParameterType_enum;
        _STATE->parameters[typeId].names = warpNames;

        _STATE->parameters[amtId].category = cat;
        _STATE->parameters[amtId].subcategory = "WARP";
        _STATE->parameters[amtId].name = "A";
        _STATE->parameters[amtId].min = 0.f; _STATE->parameters[amtId].max = 1.f;
        _STATE->parameters[amtId].initvalue = 0.f;
        _STATE->parameters[amtId].type = ParameterType_double;
        _STATE->parameters[amtId].digits = 2; _STATE->parameters[amtId].progress = .01;
        _STATE->parameters[amtId].valuename = " ";

        // EG source for WARP AMT (0 off, 1..4 = EG1..EG4). Type + names here, not
        // only in the GUI selector: without .type this defaulted to BOOL — the same
        // defect as the PW EG source (audit H5 class) — so MIDI learn refused it and
        // the host registered it as an OFF/ON switch.
        static const std::string_view warpEgNames[] = {"OFF", "EG1", "EG2", "EG3", "EG4"};
        _STATE->parameters[egId].category = cat;
        _STATE->parameters[egId].name = "WARP EG";
        _STATE->parameters[egId].min = 0.f; _STATE->parameters[egId].max = 4.f;
        _STATE->parameters[egId].initvalue = 0.f;
        _STATE->parameters[egId].type = ParameterType_enum;
        _STATE->parameters[egId].names = warpEgNames;
        // No NoAssignment, for the same reason as the PW EG source: it excludes the
        // parameter from presets, and which EG sweeps WARP is part of the patch.
        // Keeping it would also have left WARP EG as the only per-oscillator EG
        // selector that does not survive a save, now that PAD's morph EG is saved.

        // WARP TO (= B): modulation moves WARP AMT (= A) toward this target. B<A inverts
        // the sweep (warp decreases as the EG/LFO/AT rises). Default 1 = old behaviour.
        _STATE->parameters[toId].category = cat;
        _STATE->parameters[toId].subcategory = "WARP";
        _STATE->parameters[toId].name = "B";
        _STATE->parameters[toId].min = 0.f; _STATE->parameters[toId].max = 1.f;
        _STATE->parameters[toId].initvalue = 1.f;
        _STATE->parameters[toId].type = ParameterType_double;
        _STATE->parameters[toId].digits = 2; _STATE->parameters[toId].progress = .01;
        _STATE->parameters[toId].valuename = " ";
    };
    setupWarp(VCO1WARPTYPE, VCO1WARPAMT, VCO1WARPEG, VCO1WARPTO, "OSC1");
    setupWarp(VCO2WARPTYPE, VCO2WARPAMT, VCO2WARPEG, VCO2WARPTO, "OSC2");
    setupWarp(VCO3WARPTYPE, VCO3WARPAMT, VCO3WARPEG, VCO3WARPTO, "SUB");

    static const std::vector<float> filtegValues = {-1, 0, 1, 2, 3};
    // .names MUST accompany .values on these enums: the IPlug wrapper sizes the
    // host param range from std::size(names) (IPlugEffect.cpp), so with names
    // empty it registers InitInt(...,0,-1) — a broken range that corrupts the
    // value through fromNormalized() on every state save/restore (e.g. TUNE EG
    // NONE(-1) comes back as EG1). Conversion still uses .values (takes priority).
    static const std::string_view filtEgNames[] = {"NONE", "EG1", "EG2", "EG3", "EG4"};
    _STATE->parameters[FILTEG].values = filtegValues;
    _STATE->parameters[FILTEG].names = filtEgNames;
    _STATE->parameters[RESEG].values = filtegValues;
    _STATE->parameters[RESEG].names = filtEgNames;
    // NONE (-1) pins the pitch offset at full/constant strength (see filtEgVal/resEgVal
    // convention below) instead of having it ride a slow amp/filter envelope, which would
    // otherwise glide the detune in/out audibly. Default stays 0 (EG1) for legacy presets.
    _STATE->parameters[VCO1TUNEEG].values = filtegValues;
    _STATE->parameters[VCO1TUNEEG].names = filtEgNames;
    _STATE->parameters[VCO2TUNEEG].values = filtegValues;
    _STATE->parameters[VCO2TUNEEG].names = filtEgNames;
    _STATE->parameters[VCO3TUNEEG].values = filtegValues;
    _STATE->parameters[VCO3TUNEEG].names = filtEgNames;

    // The four amp EGs carry NONE for the same reason the TUNE EGs do: a source that
    // should sit at a constant level underneath an enveloped one has no business being
    // shaped by an envelope, and with LFO -> GAIN now routable an unenveloped source is
    // the only way to let the LFO alone shape a level. NONE means factor 1.0, matching
    // filtEgVal/resEgVal — see VcoNote::deriveEgRouting, which also covers the one thing
    // that does not generalise: VCO1EG and VCO2EG decide when a voice ENDS, so NONE
    // there needs a mortality fallback and a fade-out.
    //
    // Storage is by VALUE, so 0 still means EG1 and no stored patch changes meaning.
    // HOST AUTOMATION does shift: an enum's host integer is the index into this list, so
    // a DAW lane that wrote 0 for EG1 now reads NONE. Same one-off break the TUNE EGs
    // took when they gained NONE; nothing shipped is automating these.
    _STATE->parameters[VCO1EG].values = filtegValues;
    _STATE->parameters[VCO1EG].names = filtEgNames;
    _STATE->parameters[VCO2EG].values = filtegValues;
    _STATE->parameters[VCO2EG].names = filtEgNames;
    _STATE->parameters[VCO3EG].values = filtegValues;
    _STATE->parameters[VCO3EG].names = filtEgNames;
    _STATE->parameters[NOISEEG].values = filtegValues;
    _STATE->parameters[NOISEEG].names = filtEgNames;
    static const std::string_view syncNames[] = {"OFF", "ON"};
    _STATE->parameters[VCO1SYNC].names = syncNames;
    _STATE->parameters[VCO2SYNC].names = syncNames;

    _STATE->parameters[REV4GAIN].min = -60.f;
    _STATE->parameters[REV4GAIN].max = 60.f;
    _STATE->parameters[REV4GAIN].category = "REVERB"; _STATE->parameters[REV4GAIN].name = "GAIN";
    _STATE->parameters[REV4GAIN].valuename = "dB";
    _STATE->parameters[REV4GAIN].type = ParameterType_double;
    _STATE->parameters[REV4GAIN].digits = 0;
    _STATE->parameters[REV4GAIN].initvalue = 0.f;
    _STATE->parameters[REV4GAIN].progress = 1;

    _STATE->parameters[REV4MIX].min = 0.f;
    _STATE->parameters[REV4MIX].max = 1.f;
    _STATE->parameters[REV4MIX].category = "REVERB"; _STATE->parameters[REV4MIX].name = "MIX";
    _STATE->parameters[REV4MIX].valuename = " ";
    _STATE->parameters[REV4MIX].type = ParameterType_double;
    _STATE->parameters[REV4MIX].digits = 2;
    _STATE->parameters[REV4MIX].initvalue = .5f;
    _STATE->parameters[REV4MIX].progress = .01;

    _STATE->parameters[REV4REF].min = 0.f;
    _STATE->parameters[REV4REF].max = 1.f;
    _STATE->parameters[REV4REF].category = "REVERB"; _STATE->parameters[REV4REF].name = "FB";
    _STATE->parameters[REV4REF].valuename = " ";
    _STATE->parameters[REV4REF].type = ParameterType_double;
    _STATE->parameters[REV4REF].digits = 2;
    _STATE->parameters[REV4REF].initvalue = .5f;
    _STATE->parameters[REV4REF].progress = .01;

    _STATE->parameters[REV3GAIN].min = -60.f;
    _STATE->parameters[REV3GAIN].max = 60.f;
    _STATE->parameters[REV3GAIN].category = "REVERB"; _STATE->parameters[REV3GAIN].name = "GAIN";
    _STATE->parameters[REV3GAIN].valuename = "dB";
    _STATE->parameters[REV3GAIN].type = ParameterType_double;
    _STATE->parameters[REV3GAIN].digits = 0;
    _STATE->parameters[REV3GAIN].initvalue = 0.f;
    _STATE->parameters[REV3GAIN].progress = 1;

    _STATE->parameters[REV3MIX].min = 0.f;
    _STATE->parameters[REV3MIX].max = 1.f;
    _STATE->parameters[REV3MIX].category = "REVERB"; _STATE->parameters[REV3MIX].name = "MIX";
    _STATE->parameters[REV3MIX].valuename = " ";
    _STATE->parameters[REV3MIX].type = ParameterType_double;
    _STATE->parameters[REV3MIX].digits = 2;
    _STATE->parameters[REV3MIX].initvalue = .5f;
    _STATE->parameters[REV3MIX].progress = .01;

    _STATE->parameters[REV3REF].min = 0.f;
    _STATE->parameters[REV3REF].max = 1.f;
    _STATE->parameters[REV3REF].category = "REVERB"; _STATE->parameters[REV3REF].name = "FB";
    _STATE->parameters[REV3REF].valuename = " ";
    _STATE->parameters[REV3REF].type = ParameterType_double;
    _STATE->parameters[REV3REF].digits = 2;
    _STATE->parameters[REV3REF].initvalue = .5f;
    _STATE->parameters[REV3REF].progress = .01;

    _STATE->parameters[REV3DAMP].min = 0.f;
    _STATE->parameters[REV3DAMP].max = 1.f;
    _STATE->parameters[REV3DAMP].category = "REVERB"; _STATE->parameters[REV3DAMP].name = "DAMP";
    _STATE->parameters[REV3DAMP].valuename = " ";
    _STATE->parameters[REV3DAMP].type = ParameterType_double;
    _STATE->parameters[REV3DAMP].digits = 2;
    _STATE->parameters[REV3DAMP].initvalue = .5f;
    _STATE->parameters[REV3DAMP].progress = .01;

    _STATE->parameters[CDELFB].min = 0.f;
    _STATE->parameters[CDELFB].max = 1.f;
    _STATE->parameters[CDELFB].category = "DELAY"; _STATE->parameters[CDELFB].name = "FB";
    _STATE->parameters[CDELFB].valuename = " ";
    _STATE->parameters[CDELFB].type = ParameterType_double;
    _STATE->parameters[CDELFB].digits = 2;
    _STATE->parameters[CDELFB].initvalue = .5f;
    _STATE->parameters[CDELFB].progress = .01;

    _STATE->parameters[CDELDEL].min = LOG10D20(0.001f);
    _STATE->parameters[CDELDEL].max = LOG10D20(3.f);
    _STATE->parameters[CDELDEL].category = "DELAY"; _STATE->parameters[CDELDEL].name = "DELAY";
    _STATE->parameters[CDELDEL].valuename = "s";
    _STATE->parameters[CDELDEL].type = ParameterType_double;
    _STATE->parameters[CDELDEL].digits = 3;
    _STATE->parameters[CDELDEL].initvalue = LOG10D20(.25f);
    _STATE->parameters[CDELDEL].progress = 1;
    _STATE->parameters[CDELDEL].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[CDELMIX].min = 0.f;
    _STATE->parameters[CDELMIX].max = 1.f;
    _STATE->parameters[CDELMIX].category = "DELAY"; _STATE->parameters[CDELMIX].name = "MIX";
    _STATE->parameters[CDELMIX].valuename = " ";
    _STATE->parameters[CDELMIX].type = ParameterType_double;
    _STATE->parameters[CDELMIX].digits = 2;
    _STATE->parameters[CDELMIX].initvalue = .5f;
    _STATE->parameters[CDELMIX].progress = .01;

    _STATE->parameters[CDDELMODRATE].min = LOG10D20(0.001f);
    _STATE->parameters[CDDELMODRATE].max = LOG10D20(20.f);
    _STATE->parameters[CDDELMODRATE].name = "RATE";
    _STATE->parameters[CDDELMODRATE].valuename = "Hz";
    _STATE->parameters[CDDELMODRATE].type = ParameterType_double;
    _STATE->parameters[CDDELMODRATE].digits = 3;
    _STATE->parameters[CDDELMODRATE].initvalue = LOG10D20(.25f);
    _STATE->parameters[CDDELMODRATE].progress = 1;
    _STATE->parameters[CDDELMODRATE].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[CDDELMODDEPTH].min = 0.f;
    _STATE->parameters[CDDELMODDEPTH].max = 1.f;
    _STATE->parameters[CDDELMODDEPTH].name = "DEPTH";
    _STATE->parameters[CDDELMODDEPTH].valuename = " ";
    _STATE->parameters[CDDELMODDEPTH].type = ParameterType_double;
    _STATE->parameters[CDDELMODDEPTH].digits = 2;
    _STATE->parameters[CDDELMODDEPTH].initvalue = 0.f;
    _STATE->parameters[CDDELMODDEPTH].progress = .01;

    // ---- ensemble chorus ----------------------------------------------------
    // An FX unit like the phaser/delay/reverb: POWER carries no initvalue, so it
    // is off until a preset asks for it and the other 75 are untouched.
    _STATE->parameters[CHORUSPOW].type = ParameterType_bool;
    _STATE->parameters[CHORUSPOW].category = "CHORUS"; _STATE->parameters[CHORUSPOW].name = "POWER";

    _STATE->parameters[CHORUSMIX].min = 0.f;
    _STATE->parameters[CHORUSMIX].max = 1.f;
    _STATE->parameters[CHORUSMIX].category = "CHORUS";
    _STATE->parameters[CHORUSMIX].name = "MIX";
    _STATE->parameters[CHORUSMIX].valuename = " ";
    _STATE->parameters[CHORUSMIX].type = ParameterType_double;
    _STATE->parameters[CHORUSMIX].digits = 2;
    // Near fully wet by default. On a string machine the ensemble is not an effect
    // over the oscillators, it IS the voice — and the dry path is what makes the
    // wet sum comb against it and read as a phaser.
    _STATE->parameters[CHORUSMIX].initvalue = .9f;
    _STATE->parameters[CHORUSMIX].progress = .01;

    _STATE->parameters[CHORUSDEPTH].min = .2f;
    _STATE->parameters[CHORUSDEPTH].max = 4.f;
    _STATE->parameters[CHORUSDEPTH].category = "CHORUS";
    _STATE->parameters[CHORUSDEPTH].name = "DEPTH";
    _STATE->parameters[CHORUSDEPTH].valuename = "MS";
    _STATE->parameters[CHORUSDEPTH].type = ParameterType_double;
    _STATE->parameters[CHORUSDEPTH].digits = 2;
    _STATE->parameters[CHORUSDEPTH].initvalue = 1.f;
    _STATE->parameters[CHORUSDEPTH].progress = .01;

    _STATE->parameters[CHORUSRATE].min = .05f;
    _STATE->parameters[CHORUSRATE].max = 3.f;
    _STATE->parameters[CHORUSRATE].category = "CHORUS";
    _STATE->parameters[CHORUSRATE].name = "RATE";
    _STATE->parameters[CHORUSRATE].valuename = "HZ";
    _STATE->parameters[CHORUSRATE].type = ParameterType_double;
    _STATE->parameters[CHORUSRATE].digits = 2;
    _STATE->parameters[CHORUSRATE].initvalue = .58f;
    _STATE->parameters[CHORUSRATE].progress = .01;

    _STATE->parameters[JITTERCENTS].min = 0.f;
    _STATE->parameters[JITTERCENTS].max = 100.f;
    _STATE->parameters[JITTERCENTS].name = "JITTER";
    _STATE->parameters[JITTERCENTS].valuename = "CENTS";
    _STATE->parameters[JITTERCENTS].type = ParameterType_double;
    _STATE->parameters[JITTERCENTS].digits = 0;
    _STATE->parameters[JITTERCENTS].initvalue = 0;
    _STATE->parameters[JITTERCENTS].progress = 1;

    // JITTERA/JITTERB have no UI control by design, but they are MIDI-learnable and
    // host-automatable — they sit beside JITTERCENTS in midiParams.
    _STATE->parameters[JITTERA].min = LOG10D20(1.f);
    _STATE->parameters[JITTERA].max = LOG10D20(20.f);
    _STATE->parameters[JITTERA].name = "RATE A";
    _STATE->parameters[JITTERA].valuename = "Hz";
    _STATE->parameters[JITTERA].type = ParameterType_double;
    _STATE->parameters[JITTERA].digits = 2;
    _STATE->parameters[JITTERA].initvalue = LOG10D20(4.f);
    _STATE->parameters[JITTERA].progress = 1;
    _STATE->parameters[JITTERA].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[JITTERB].min = LOG10D20(1.f);
    _STATE->parameters[JITTERB].max = LOG10D20(20.f);
    _STATE->parameters[JITTERB].name = "RATE B";
    _STATE->parameters[JITTERB].valuename = "Hz";
    _STATE->parameters[JITTERB].type = ParameterType_double;
    _STATE->parameters[JITTERB].digits = 2;
    _STATE->parameters[JITTERB].initvalue = LOG10D20(8.f);
    _STATE->parameters[JITTERB].progress = 1;
    _STATE->parameters[JITTERB].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[RECORDButton].type = ParameterType_bool;
    _STATE->parameters[RECORDButton].name = "RECORD";

    _STATE->parameters[POWERButton].type = ParameterType_bool;
    _STATE->parameters[POWERButton].name = "POWER";

    _STATE->parameters[SETTINGSBUTTON].type = ParameterType_bool;
    _STATE->parameters[SETTINGSBUTTON].name = "SETTINGS";

    _STATE->parameters[ARP_STEPS].min = 1;
    _STATE->parameters[ARP_STEPS].max = 32;
    _STATE->parameters[ARP_STEPS].category = "ARP";
    _STATE->parameters[ARP_STEPS].name = "STEPS";
    _STATE->parameters[ARP_STEPS].valuename = " ";
    _STATE->parameters[ARP_STEPS].type = ParameterType_double;
    _STATE->parameters[ARP_STEPS].initvalue = 3;
    _STATE->parameters[ARP_STEPS].progress = 1;
    _STATE->parameters[ARP_STEPS].digits = 0;

    _STATE->parameters[LEARNMODE].category = "SEQUENCER";
    _STATE->parameters[LEARNMODE].name = "LEARN MODE";
    _STATE->parameters[LEARNMODE].initvalue = 0.f;
    _STATE->parameters[LEARNMODE].type = ParameterType_enum;
    static const std::vector<float> learnmodeVals = {0, 1};
    _STATE->parameters[LEARNMODE].values = learnmodeVals;
    _STATE->parameters[SEQ_STEPS].min = 1;
    _STATE->parameters[SEQ_STEPS].max = 132;
    _STATE->parameters[SEQ_STEPS].category = "SEQUENCER";
    _STATE->parameters[SEQ_STEPS].name = "STEPS";
    _STATE->parameters[SEQ_STEPS].valuename = " ";
    _STATE->parameters[SEQ_STEPS].type = ParameterType_double;
    _STATE->parameters[SEQ_STEPS].initvalue = 1;
    _STATE->parameters[SEQ_STEPS].progress = 1;
    _STATE->parameters[SEQ_STEPS].digits = 0;

    _STATE->parameters[SEQ_SWING].min = 0;
    _STATE->parameters[SEQ_SWING].max = 1;
    _STATE->parameters[SEQ_SWING].category = "SEQUENCER";
    _STATE->parameters[SEQ_SWING].name = "SWING";
    _STATE->parameters[SEQ_SWING].valuename = " ";
    _STATE->parameters[SEQ_SWING].type = ParameterType_double;
    _STATE->parameters[SEQ_SWING].initvalue = 0;
    _STATE->parameters[SEQ_SWING].progress = 0.05;
    _STATE->parameters[SEQ_SWING].digits = 2;

    _STATE->parameters[SEQ_GAIN].min = -60;
    _STATE->parameters[SEQ_GAIN].max = 60;
    _STATE->parameters[SEQ_GAIN].category = "SEQUENCER";
    _STATE->parameters[SEQ_GAIN].name = "GAIN";
    _STATE->parameters[SEQ_GAIN].valuename = "dB";
    _STATE->parameters[SEQ_GAIN].type = ParameterType_double;
    _STATE->parameters[SEQ_GAIN].initvalue = 0;
    _STATE->parameters[SEQ_GAIN].progress = 1;
    _STATE->parameters[SEQ_GAIN].digits = 0;

    _STATE->parameters[SEQ_SYNCDAW].category = "SEQUENCER";
    _STATE->parameters[SEQ_SYNCDAW].name = "SYNC DAW";
    _STATE->parameters[SEQ_SYNCDAW].type = ParameterType_bool;
    _STATE->parameters[SEQ_SYNCDAW].initvalue = 0;

    _STATE->parameters[ZERONOTE].category = "SEQUENCER";
    _STATE->parameters[ZERONOTE].name = "ZERO NOTE";
    _STATE->parameters[ZERONOTE].type = ParameterType_bool;
    _STATE->parameters[ZERONOTE].initvalue = 0;

    _STATE->parameters[CLEARTASKS].name = "CLEAR TASKS";
    _STATE->parameters[CLEARTASKS].type = ParameterType_bool;
    _STATE->parameters[CLEARTASKS].initvalue = 0;

    _STATE->parameters[WAITFORZERO].category = "SEQUENCER";
    _STATE->parameters[WAITFORZERO].name = "WAIT FOR ZERO";
    _STATE->parameters[WAITFORZERO].type = ParameterType_bool;
    _STATE->parameters[WAITFORZERO].initvalue = 0;

    _STATE->parameters[SEQLEARNING].category = "SEQUENCER";
    _STATE->parameters[SEQLEARNING].name = "LEARN";
    _STATE->parameters[SEQLEARNING].type = ParameterType_bool;
    _STATE->parameters[SEQLEARNING].initvalue = 0;

    _STATE->parameters[NOTESETTINGSFROMSYNTH].category = "SEQUENCER";
    _STATE->parameters[NOTESETTINGSFROMSYNTH].name = "REINIT NOTES";

    _STATE->parameters[NONOTES].category = "SEQUENCER";
    _STATE->parameters[NONOTES].name = "DONT IMPORT NOTES";
    _STATE->parameters[NONOTES].type = ParameterType_bool;
    _STATE->parameters[NONOTES].initvalue = 0;
    _STATE->parameters[NOTESETTINGSFROMSYNTH].type = ParameterType_bool;
    _STATE->parameters[NOTESETTINGSFROMSYNTH].initvalue = 0;

    _STATE->parameters[SYNTHRESET].name = "CLEAR SYNTH";
    _STATE->parameters[SYNTHRESET].type = ParameterType_bool;
    _STATE->parameters[SYNTHRESET].initvalue = 0;

    _STATE->parameters[ARPRESET].category = "SEQUENCER";
    _STATE->parameters[ARPRESET].name = "CLEAR";
    _STATE->parameters[ARPRESET].type = ParameterType_bool;
    _STATE->parameters[ARPRESET].initvalue = 0;

    static const std::string_view timeDivNames[] = {"1/64","1/32","1/16","1/8","1/4","1/2","1","2","4"};
    _STATE->parameters[SEQ_TIMEDIV].category = "SEQUENCER";
    _STATE->parameters[SEQ_TIMEDIV].name = "TIME DIV";
    _STATE->parameters[SEQ_TIMEDIV].type = ParameterType_enum;
    _STATE->parameters[SEQ_TIMEDIV].min = 0;
    _STATE->parameters[SEQ_TIMEDIV].max = 8;
    _STATE->parameters[SEQ_TIMEDIV].initvalue = 0;
    _STATE->parameters[SEQ_TIMEDIV].names = timeDivNames;

    auto setupRouteParam = [&](int id, const char* category, const char* name, double initval) {
        _STATE->parameters[id].category = category;
        _STATE->parameters[id].name = name;
        _STATE->parameters[id].type = ParameterType_double;
        _STATE->parameters[id].min = 0.0;
        _STATE->parameters[id].max = 1.0;
        _STATE->parameters[id].initvalue = initval;
        _STATE->parameters[id].digits = 2;
        _STATE->parameters[id].progress = 0.01;
        _STATE->parameters[id].valuename = " ";
        // MidiParam only. NoAssignment used to be set here too, which excluded every
        // modulation route from presets — and because unserializeState resets EVERY
        // parameter to its initvalue before applying the stored ones, the effect was
        // not "kept across loads" but "reset to default on every load". How far
        // velocity opens the filter, or aftertouch moves morph, is part of the patch.
        //
        // The two non-zero defaults below (KEYTRACK_TO_FILT 1.0, KEYTRACK_TO_DECAY
        // 0.35) still behave as their comments describe: presets are sparse id/value
        // pairs, so a file saved before this simply has no entry and picks the
        // initvalue up on load exactly as it did.
        _STATE->parameters[id].flags |= Param::MidiParam;
    };
    setupRouteParam(VEL_TO_FILT,    "VELOCITY",    "FILT",      0.0);
    setupRouteParam(VEL_TO_AMP,     "VELOCITY",    "AMP",       0.0);
    setupRouteParam(AT_TO_FILT,     "AFTERTOUCH",  "FILT",      0.0);
    setupRouteParam(AT_TO_AMP,      "AFTERTOUCH",  "AMP",       0.0);
    setupRouteParam(MW_TO_FILT,     "MOD WHEEL",   "FILT",      0.0);
    setupRouteParam(MW_TO_VIBRATO,  "MOD WHEEL",   "VIB",       0.0);
    setupRouteParam(AT_TO_VIBRATO,  "AFTERTOUCH",  "VIB",       0.0);
    // Defaults to FULL tracking, and that default is what preserves every existing
    // patch. The cutoff has always been built upward from the played note — 100%
    // keytrack, hardcoded into the shape of the expression — and this parameter now
    // scales that, so 1.0 IS the old behaviour. Presets are stored sparsely against
    // initvalue and none of them contains this id (it had no control until now), so
    // they all pick up 1.0 on load with no migration. Do NOT "tidy" this back to 0.0:
    // that would silently un-track every preset ever saved.
    setupRouteParam(KEYTRACK_TO_FILT, "KEYTRACK",  "FILT",      1.0);
    setupRouteParam(VEL_TO_RES,     "VELOCITY",    "RES",       0.0);
    setupRouteParam(AT_TO_RES,      "AFTERTOUCH",  "RES",       0.0);
    setupRouteParam(MW_TO_RES,      "MOD WHEEL",   "RES",       0.0);
    setupRouteParam(MW_TO_LFORATE,  "MOD WHEEL",   "LFORT",  0.0);
    setupRouteParam(AT_TO_LFORATE,  "AFTERTOUCH",  "LFORT",  0.0);
    setupRouteParam(MW_TO_LFODEPTH, "MOD WHEEL",   "LFODEP", 0.0);
    setupRouteParam(AT_TO_LFODEPTH, "AFTERTOUCH",  "LFODEP", 0.0);
    setupRouteParam(AT_TO_WARP,     "AFTERTOUCH",  "WARP",      0.0);
    setupRouteParam(AT_TO_MORPH,    "AFTERTOUCH",  "MORPH",     0.0);
    setupRouteParam(MW_TO_MORPH,    "MOD WHEEL",   "MORPH",     0.0);
    setupRouteParam(MW_TO_WARP,     "MOD WHEEL",   "WARP",      0.0);
    setupRouteParam(AT_TO_UNI,      "AFTERTOUCH",  "UNI",       0.0);
    setupRouteParam(MW_TO_UNI,      "MOD WHEEL",   "UNI",       0.0);
    // MODAL oscillator decay. AT/MW damp — pressing SHORTENS the tail, which is the
    // opposite direction to FILT/AMP and the same reasoning as the UNI routes: the
    // knob should be the undamped value and the route should only ever subtract.
    // KEYTRACK defaults to 0.35 rather than 0 because a struck instrument whose T60
    // is flat across the keyboard sounds wrong in the top octave — some of this is
    // the correct resting state, not an effect to be dialled in.
    setupRouteParam(AT_TO_DECAY,       "AFTERTOUCH", "DAMP",  0.0);
    setupRouteParam(MW_TO_DECAY,       "MOD WHEEL",  "DAMP",  0.0);
    setupRouteParam(KEYTRACK_TO_DECAY, "KEYTRACK",   "DECAY", 0.35);

    _STATE->parameters[ROUTESPACE].name = "ROUTESPACE";
    _STATE->parameters[ROUTESPACE].min = 0.f;
    _STATE->parameters[ROUTESPACE].max = 3.f;   // VEL / AT / MW / KEY
    _STATE->parameters[ROUTESPACE].initvalue = 0.f;
    _STATE->parameters[ROUTESPACE].flags |= Param::NoAssignment;

    auto setupVcoSpace = [&](int spaceId, int srcId, int depthId,
                              const char* spaceName, const char* srcName, const char* depthName,
                              const char* category) {
        _STATE->parameters[spaceId].name = spaceName;
        _STATE->parameters[spaceId].min = 0.f;
        _STATE->parameters[spaceId].max = 1.f;
        _STATE->parameters[spaceId].initvalue = 0.f;
        // spaceId KEEPS NoAssignment: it is the UI subsection index, not a sound
        // parameter, and never writing it to a preset is what makes it safe to add or
        // reorder subsections without leaving a saved index dangling (see gui.cpp).
        _STATE->parameters[spaceId].flags |= Param::NoAssignment;
        // srcId and depthId do NOT carry it. NoAssignment excludes a parameter from
        // presets (captureCurrentPreset / serializeState) while unserializeState still
        // resets EVERY parameter to its initvalue first — so these were silently reset
        // to OFF/0 on every preset load. A pulse-width sweep is part of the patch, not
        // a global performance setting. Dropping the flag also gives them undo history
        // (EventHandler.cpp).
        //
        // Backward compatible: presets are sparse id/value pairs, so an older file
        // simply has no entry and these land on their initvalue exactly as before.
        // .type on both is load-bearing: Param::type defaults to BOOL, and four
        // consumers dispatch on it — MIDI learn refused these, knob steppers
        // treated them as toggles, and the host registered the depth as an OFF/ON
        // switch that squashed any fractional value to 0/1 on every project
        // round-trip (audit finding H5).
        static const std::string_view pwSrcNames[] = {"OFF", "EG1", "EG2", "EG3", "EG4"};
        _STATE->parameters[srcId].category = category;
        _STATE->parameters[srcId].name = srcName;
        _STATE->parameters[srcId].min = 0.f;
        _STATE->parameters[srcId].max = 4.f;
        _STATE->parameters[srcId].initvalue = 0.f;
        _STATE->parameters[srcId].type = ParameterType_enum;
        _STATE->parameters[srcId].names = pwSrcNames;
        _STATE->parameters[depthId].category = category;
        _STATE->parameters[depthId].name = depthName;
        _STATE->parameters[depthId].min = 0.f;
        _STATE->parameters[depthId].max = 1.f;
        _STATE->parameters[depthId].initvalue = 0.f;
        _STATE->parameters[depthId].type = ParameterType_double;
        _STATE->parameters[depthId].progress = .01f;
        _STATE->parameters[depthId].digits = 2;
        _STATE->parameters[depthId].valuename = " ";
    };
    setupVcoSpace(VCO1SPACE, VCO1PWMODSRC, VCO1PWMODDEPTH, "VCO1SPACE", "PW EG", "PWM", "OSC1");
    setupVcoSpace(VCO2SPACE, VCO2PWMODSRC, VCO2PWMODDEPTH, "VCO2SPACE", "PW EG", "PWM", "OSC2");
    setupVcoSpace(VCO3SPACE, VCO3PWMODSRC, VCO3PWMODDEPTH, "VCO3SPACE", "PW EG", "PWM", "SUB");

    static const std::string_view lfoWaveNames[] = {"SIN", "TRI", "SAW", "SQR"};
    // STRIKE (17) is the MODAL filter's strike position. It is the only modal target
    // in this list on purpose: the filter's pitch and decay are already CUT and RES,
    // and on the struck oscillator everything except decay is an initial condition
    // that an LFO could not move once the note is ringing.
    // GAIN1/2/3 and NOISE (18..21) are the PER-SOURCE levels, and they are UNIPOLAR
    // ducks for the same reason AMP is: the GAIN knob has to stay the ceiling, or the
    // LFO peak is louder than the written level and the knob becomes the midpoint of a
    // sweep. Ordered so that dest 18+i is gains[i] in synth.cpp, noise included --
    // that array is already VCO1/VCO2/SUB/NOISE.
    static const std::string_view lfoDestNames[] = {"OFF", "PITCH", "FILT", "AMP", "PW1", "PW2", "PW3", "RES",
                                                    "MORPH1", "MORPH2", "MORPH3", "WARP1", "WARP2", "WARP3",
                                                    "UNI1", "UNI2", "UNI3", "STRIKE",
                                                    "GAIN1", "GAIN2", "GAIN3", "NOISE"};
    auto setupLfo = [&](int rateId, int depthId, int waveId, int destId, int phaseId,
                        const char* category) {
        _STATE->parameters[rateId].name = "CPS";
        _STATE->parameters[rateId].category = category;
        _STATE->parameters[rateId].min = LOG10D20(0.01f);
        _STATE->parameters[rateId].max = LOG10D20(20.f);
        _STATE->parameters[rateId].initvalue = LOG10D20(1.f);
        _STATE->parameters[rateId].type = ParameterType_double;
        _STATE->parameters[rateId].digits = 2;
        _STATE->parameters[rateId].valuename = "Hz";
        _STATE->parameters[rateId].paramCurve = Param::ParamCurve::Log10;

        // BIPOLAR: a negative depth INVERTS the LFO, which is what lets two sources be
        // crossfaded against each other rather than ducked together. Inversion, not a
        // half-cycle phase offset -- those are only the same thing for sine, triangle
        // and square. saw(p+0.5) = 2p, where the inverse is 1-2p, so a phase-shifted
        // pair of saws is NOT complementary and the crossfade would pump.
        // 0 is still the centre and still means no modulation, so no preset moves.
        _STATE->parameters[depthId].name = "DEPTH";
        _STATE->parameters[depthId].category = category;
        _STATE->parameters[depthId].min = -1.f;
        _STATE->parameters[depthId].max = 1.f;
        _STATE->parameters[depthId].initvalue = 0.f;
        _STATE->parameters[depthId].type = ParameterType_double;
        _STATE->parameters[depthId].digits = 2;
        _STATE->parameters[depthId].valuename = " ";
        // 0 is off and the sign only picks which half of the cycle ducks, so the
        // knob fill belongs either side of centre rather than growing from -1.
        _STATE->parameters[depthId].flags |= Param::CentreFill;

        _STATE->parameters[waveId].name = "WAVE";
        _STATE->parameters[waveId].category = category;
        _STATE->parameters[waveId].min = 0.f;
        _STATE->parameters[waveId].max = 3.f;
        _STATE->parameters[waveId].initvalue = 0.f;
        _STATE->parameters[waveId].type = ParameterType_enum;
        _STATE->parameters[waveId].names = lfoWaveNames;

        _STATE->parameters[destId].name = "DEST";
        _STATE->parameters[destId].category = category;
        // min stays 0 and names keeps its OFF row so a legacy 0 (old projects,
        // stray automation) is still in range and displayable on the host side;
        // the app's selector lists 1..21 only, init points the cursor at PITCH,
        // and foldLegacyLfoRoutes remaps a loaded 0 to 1. Old presets almost
        // never STORED 0 — sparse saves skip values equal to init, which was 0.
        _STATE->parameters[destId].min = 0.f;
        _STATE->parameters[destId].max = 21.f;
        _STATE->parameters[destId].initvalue = 1.f;
        _STATE->parameters[destId].type = ParameterType_enum;
        _STATE->parameters[destId].names = lfoDestNames;

        // Where in its cycle this LFO sits, as an offset applied when the waveform is
        // READ -- the accumulator itself is untouched. That is what keeps two LFOs at the
        // same rate bit-identical, and it means turning PHASE moves a note that is already
        // sounding instead of waiting for the next note-on.
        //
        // NOT the way to crossfade two sources: use a negative DEPTH. A half-cycle offset
        // equals inversion only for sine, triangle and square -- saw(p+0.5) = 2p where the
        // inverse is 1-2p. This is for quadrature and for deliberately offsetting one
        // route against another; on saw and square it also picks where a note starts.
        // 5 deg per step puts 90/180/270 on exact detents.
        _STATE->parameters[phaseId].name = "PHASE";
        _STATE->parameters[phaseId].category = category;
        _STATE->parameters[phaseId].min = 0.f;
        _STATE->parameters[phaseId].max = 360.f;
        _STATE->parameters[phaseId].initvalue = 0.f;
        _STATE->parameters[phaseId].type = ParameterType_double;
        _STATE->parameters[phaseId].digits = 0;
        _STATE->parameters[phaseId].progress = 5.f;
        _STATE->parameters[phaseId].valuename = "deg";
    };
    setupLfo(LFO1RATE, LFO1DEPTH, LFO1WAVE, LFO1DEST, LFO1PHASE, "LFO1");
    setupLfo(LFO2RATE, LFO2DEPTH, LFO2WAVE, LFO2DEST, LFO2PHASE, "LFO2");
    setupLfo(LFO3RATE, LFO3DEPTH, LFO3WAVE, LFO3DEST, LFO3PHASE, "LFO3");
    setupLfo(LFO4RATE, LFO4DEPTH, LFO4WAVE, LFO4DEST, LFO4PHASE, "LFO4");

    // The multi-dest matrix: one bipolar depth per (LFO, dest). Same shape as the
    // DEPTH knob above (which is a UI window onto the selected dest's slot — see
    // lfoWindowTick in synth.cpp). Named after the destination so the host's
    // automation list reads "LFO3: MORPH2" rather than a bare index.
    {
        static const char* lfoCats[4] = {"LFO1", "LFO2", "LFO3", "LFO4"};
        for (int n = 0; n < 4; n++) {
            for (int d = 1; d <= LFO_MD_NDEST; d++) {
                auto& p = _STATE->parameters[lfoMdId(n, d)];
                // string_view over a literal — .data() is null-terminated here
                p.name = lfoDestNames[d].data();
                p.category = lfoCats[n];
                p.min = -1.f;
                p.max = 1.f;
                p.initvalue = 0.f;
                p.type = ParameterType_double;
                p.digits = 2;
                p.valuename = " ";
                p.flags |= Param::CentreFill;
            }
        }
    }

    _STATE->parameters[FILT_CUT].min = 0.f;
    _STATE->parameters[FILT_CUT].max = 1.f;
    _STATE->parameters[FILT_CUT].category = "FILTER";
    _STATE->parameters[FILT_CUT].name = "CUT";
    _STATE->parameters[FILT_CUT].valuename = " ";
    _STATE->parameters[FILT_CUT].type = ParameterType_double;
    _STATE->parameters[FILT_CUT].digits = 2;
    _STATE->parameters[FILT_CUT].initvalue = 1.f;
    _STATE->parameters[FILT_CUT].progress = .01;

    _STATE->parameters[SEQ_BPM].min = 10;
    _STATE->parameters[SEQ_BPM].max = 960;
    _STATE->parameters[SEQ_BPM].category = "SEQUENCER";
    _STATE->parameters[SEQ_BPM].name = "BPM";
    _STATE->parameters[SEQ_BPM].valuename = "BPM";
    _STATE->parameters[SEQ_BPM].type = ParameterType_double;
    _STATE->parameters[SEQ_BPM].initvalue = 240;
    _STATE->parameters[SEQ_BPM].progress = 1;
    _STATE->parameters[SEQ_BPM].digits = 0;
    _STATE->parameters[SEQ_MODE].category = "SEQUENCER";
    _STATE->parameters[SEQ_MODE].initvalue = _STATE->parameters[ARP_MODE].initvalue = -1;

    for (int i = 0; i < NUM_ARP_STEPS; i++) {
        _STATE->parameters[ARPSTEP01 + i].min = -32;
        _STATE->parameters[ARPSTEP01 + i].max = 32;
        _STATE->parameters[ARPSTEP01 + i].name = tsl::graphics::nums32[i];
        _STATE->parameters[ARPSTEP01 + i].valuename = "Semitones";
        _STATE->parameters[ARPSTEP01 + i].type = ParameterType_double;
        _STATE->parameters[ARPSTEP01 + i].initvalue = 3;
        _STATE->parameters[ARPSTEP01 + i].progress = 1;
        _STATE->parameters[ARPSTEP01 + i].digits = 0;
        _STATE->parameters[ARPSTEP01 + i].category = "ARP STEP";
        // A step is an interval from the root, so 0 is the root itself: the fill
        // grows either side of centre instead of from -32 semitones.
        _STATE->parameters[ARPSTEP01 + i].flags |= Param::MidiParam | Param::CentreFill;
    }
    _STATE->parameters[ARPSTEP01].initvalue = 0;

    _STATE->parameters[PHASERRANGE].min = 0;
    _STATE->parameters[PHASERRANGE].max = 60;
    _STATE->parameters[PHASERRANGE].category = "PHASER"; _STATE->parameters[PHASERRANGE].name = "RANGE";
    _STATE->parameters[PHASERRANGE].valuename = "SEMITONES";
    _STATE->parameters[PHASERRANGE].type = ParameterType_double;
    _STATE->parameters[PHASERRANGE].initvalue = 12;
    _STATE->parameters[PHASERRANGE].progress = 1;
    _STATE->parameters[PHASERRANGE].digits = 2;

    _STATE->parameters[PHASERRATE].min = LOG10D20(0.01);
    _STATE->parameters[PHASERRATE].max = LOG10D20(20);
    _STATE->parameters[PHASERRATE].category = "PHASER"; _STATE->parameters[PHASERRATE].name = "RATE";
    _STATE->parameters[PHASERRATE].valuename = "Hz";
    _STATE->parameters[PHASERRATE].type = ParameterType_double;
    _STATE->parameters[PHASERRATE].initvalue = LOG10D20(0.25);
    _STATE->parameters[PHASERRATE].progress = 1;
    _STATE->parameters[PHASERRATE].digits = 2;
    _STATE->parameters[PHASERRATE].paramCurve = Param::ParamCurve::Log10;

    _STATE->parameters[PHASERFB].min = 0;
    _STATE->parameters[PHASERFB].max = 1;
    _STATE->parameters[PHASERFB].category = "PHASER"; _STATE->parameters[PHASERFB].name = "FB";
    _STATE->parameters[PHASERFB].valuename = "";
    _STATE->parameters[PHASERFB].type = ParameterType_double;
    _STATE->parameters[PHASERFB].initvalue = 0.5;
    _STATE->parameters[PHASERFB].progress = 0.05;
    _STATE->parameters[PHASERFB].digits = 2;

    _STATE->parameters[PHASERPOW].type = ParameterType_bool;
    _STATE->parameters[PHASERPOW].category = "PHASER"; _STATE->parameters[PHASERPOW].name = "POWER";

    static constexpr int midiParams[] = {
        POSTGAIN,
        EG1ATTACK, EG1DECAY, EG1SUSTAIN, EG1RELEASE,
        EG2ATTACK, EG2DECAY, EG2SUSTAIN, EG2RELEASE,
        EG3ATTACK, EG3DECAY, EG3SUSTAIN, EG3RELEASE,
        EG4ATTACK, EG4DECAY, EG4SUSTAIN, EG4RELEASE,
        // EG5 removed: fully set up but read by nothing (audit) — a learnable
        // knob that does nothing is worse than none.
        VCO1GAIN, VCO1COARSE, VCO1FINE, VCO1PW,
        VCO1TYPE, VCO1EG, VCO1TUNEEG, VCO1SYNC,
        VCO2GAIN, VCO2COARSE, VCO2FINE, VCO2PW,
        VCO2TYPE, VCO2EG, VCO2TUNEEG, VCO2SYNC,
        // VCO3COARSE removed: the SUB's live coarse param is VCO3COARSEST below.
        VCO3GAIN, VCO3FINE, VCO3PW,
        VCO3TYPE, VCO3EG, VCO3TUNEEG, VCO3RINGMOD, VCO3NORMAL, VCO3AM, VCO3PM, VCO3AMDEPTH, VCO3PMDEPTH,
        VCO1WTPOS, VCO2WTPOS, VCO3WTPOS,
        VCO1MORPHTO, VCO2MORPHTO, VCO3MORPHTO,
        VCO1WTSEL, VCO2WTSEL, VCO3WTSEL,
        VCO1MODALCH, VCO2MODALCH, VCO3MODALCH,
        VCO1MODALDEC, VCO2MODALDEC, VCO3MODALDEC,
        VCO1MODALBRT, VCO2MODALBRT, VCO3MODALBRT,
        VCO1MODALHRD, VCO2MODALHRD, VCO3MODALHRD,
        VCO1MODALPOS, VCO2MODALPOS, VCO3MODALPOS,
        FILTMODALBODY, FILTMODALPOS,
        KEYTRACK_TO_DECAY, AT_TO_DECAY, MW_TO_DECAY,
#if PA_ENABLE_PAD
        // PADBW/PADBWSC/PADSTR/PADSEED removed: replaced by the PAD_FIXED_*
        // constants in padParamsFromSnapshot — nothing reads them any more.
        VCO1PADSEL, VCO2PADSEL, VCO3PADSEL,
        VCO1PADPOS,  VCO2PADPOS,  VCO3PADPOS,
        VCO1PADMTO,  VCO2PADMTO,  VCO3PADMTO,
        VCO1PADMEG,  VCO2PADMEG,  VCO3PADMEG,
#endif
        VCO1UNIVOICES, VCO1UNIDETUNE, VCO1UNIBLEND,
        VCO2UNIVOICES, VCO2UNIDETUNE, VCO2UNIBLEND,
        VCO3UNIVOICES, VCO3UNIDETUNE, VCO3UNIBLEND,
        VCO1WARPTYPE, VCO1WARPAMT, VCO1WARPEG, VCO1WARPTO,
        VCO2WARPTYPE, VCO2WARPAMT, VCO2WARPEG, VCO2WARPTO,
        VCO3WARPTYPE, VCO3WARPAMT, VCO3WARPEG, VCO3WARPTO,
        // FILTCENTER removed (nothing reads it — FILT_CUT below is the live
        // cutoff); RESEG added — it was the one routing selector not learnable.
        FILTRES, FILTEG, RESEG, NOISEGAIN, NOISEMODE, NOISEEG,
        REV3GAIN, REV3MIX, REV3REF, REV3DAMP, REVPOW,
        // REV4GAIN/MIX/REF removed: only Progenitor1Dark (the REV3 family) is
        // instantiated in this build.
        CDELFB, CDELDEL, CDELMIX, CDDELMODRATE, CDDELMODDEPTH, CDELPOW,
        JITTERCENTS, JITTERA, JITTERB,
        SEQ_BPM, SEQ_SWING, SEQ_GAIN, ARP_MODE,
        PHASERRANGE, PHASERRATE, PHASERFB, PHASERPOW,
        AT_TO_VIBRATO, KEYTRACK_TO_FILT,
        VEL_TO_FILT, VEL_TO_AMP, AT_TO_FILT, AT_TO_AMP, MW_TO_FILT, MW_TO_VIBRATO,
        FILT_CUT,
        VCO1PWMODSRC, VCO1PWMODDEPTH,
        VCO2PWMODSRC, VCO2PWMODDEPTH,
        VCO3PWMODSRC, VCO3PWMODDEPTH,
        VCO3COARSEST,
        LFO1RATE, LFO1DEPTH, LFO1WAVE, LFO1DEST, LFO1PHASE,
        LFO2RATE, LFO2DEPTH, LFO2WAVE, LFO2DEST, LFO2PHASE,
        LFO3RATE, LFO3DEPTH, LFO3WAVE, LFO3DEST, LFO3PHASE,
        LFO4RATE, LFO4DEPTH, LFO4WAVE, LFO4DEST, LFO4PHASE,
        SEQ_STEPS, SEQ_SYNCDAW, SEQ_TIMEDIV, ARP_STEPS,   // SEQ_MODE removed: dead
        ZERONOTE, CLEARTASKS, WAITFORZERO, SEQLEARNING,
        NOTESETTINGSFROMSYNTH, SYNTHRESET, NONOTES, ARPRESET,
        LEARNMODE,
        POWERButton, SETTINGSBUTTON, RECORDButton
    };
    // UI AND TRANSIENT PARAMETERS ARE NEVER PART OF A PATCH.
    //
    // Param::NoAssignment keeps a parameter out of presets entirely — and because
    // unserializeState resets every parameter to its initvalue before applying the
    // stored ones, anything NOT flagged here that differs from its default at save
    // time gets baked into the preset and pushed back in on load. For this list that
    // ranges from cosmetic to actively wrong: a stored page index dangles if
    // subsections are ever added or reordered (the original reason VCOxSPACE carried
    // the flag), a stored SETTINGSBUTTON reopens the settings panel on load, a stored
    // POWERButton loads a silent instrument, and PRESETSELECT stores which preset is
    // selected inside a preset.
    //
    // Sound parameters must NOT be added here. That mistake is what made the EG
    // sources and every modulation route silently reset on load; see the note in
    // setupRouteParam.
    static constexpr int uiOnlyParams[] = {
        // page and subsection indices
        GUISPACE, GUISPACE2, EGSPACE, FXSPACE, FXSPACE1, LFOSPACE, ARPSPACE,
        // momentary buttons and panel toggles
        POWERButton, POWERTRACK, SETTINGSBUTTON, RECORDButton, MIDILEARNBUTTON,
        LOADPRESETBUTTON, SAVEPRESETBUTTON, SAVEPRESETBUTTON1, PRESETSELECT,
        // running readouts the sequencer/arp write for the GUI
        CURRENTNOTE, ARP_STEP,
        // cross-thread handshakes and one-shot triggers
        ZERONOTE, LEARNMODE, NONOTES, NOTESETTINGSFROMSYNTH, WAITFORZERO,
        CLEARTASKS, SEQLEARNING, ARPRESET, SYNTHRESET,
    };
    for (int id : uiOnlyParams)
        _STATE->parameters[id].flags |= Param::NoAssignment;

    for (int id : midiParams)
        _STATE->parameters[id].flags |= Param::MidiParam;

    for (int i = 0; i < NUM_PARAMS; i++)
        _STATE->params[0][i].store(_STATE->parameters[i].initvalue);
}


#ifdef __ANDROID__
#include "callbacks_loop_controls.h"
void java_set_hqresampling(JNIEnv *env, jclass obj, jboolean hqresampling) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) return;
    _DATA->hqresampling.store((bool) hqresampling);
}

// Called from the Java settings screen (Android UI thread). The selector only
// rebuilds its list in addRecursiveDraw, so push a rebuild like savePreset does;
// if the native views are torn down behind the settings Activity, the visible_
// guard skips it and the re-add on resume applies the flag instead.
void java_set_showfactory(JNIEnv *env, jclass obj, jboolean show) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) return;
    _DATA->showFactoryPresets.store((bool) show);
    _appState->toUiThreadQueue.try_push([_appState]() {
        auto* sel = _DATA->views.presetSelector;
        if (sel && sel->visible_) {
            sel->addRecursiveDraw();
            sel->redraw();
        }
    });
}

// The trial gate's verdict, from TrialGate.java's own thread. Arrives whenever
// it resolves -- possibly before the UI exists, possibly minutes into a capped
// session once the phone finds signal. The wall is a view, so raising one has to
// happen on the UI thread; setTrialState only records the state when there is no
// AppState yet, and startSessionCap reads it when the UI comes up.
void java_set_trial_state(JNIEnv *env, jclass obj, jint state) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) {
        tsl::app::setTrialState(nullptr, (int) state);
        return;
    }
    const int st = (int) state;
    _appState->toUiThreadQueue.try_push([_appState, st]() {
        tsl::app::setTrialState(_appState, st);
    });
}

// Called from the Java worker that moved the preset folder. The bank has to be
// re-read from wherever it now lives; Preset::reload queues that onto the UI
// thread, since the deque belongs to it.
void java_reload_presets(JNIEnv *env, jclass obj) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) return;
    Preset::reload(_appState);
}

void java_set_value(JNIEnv *env, jclass obj, jboolean logarithmic, jboolean convms, jlong _view,
                    jlong _ref, jlong _refmin,
                    jlong _refmax, jdouble _value, jdouble min, jdouble max, jstring _valname);

void java_setup(JNIEnv *env, jclass obj, jint sr, jint bufsize, jint channels,
           jboolean hasMic, jboolean fastrendering);

void java_control(JNIEnv *env, jclass obj, jint _c) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) return;
    int c = (int) _c;
    if (c == 0) {
        tsl::parameters::Event e;
        e.setup(_STATE, 0, POWERButton);
        e.applyFromExt(_STATE, tsl::parameters::FromUi);
    }
}

void java_set_track_source(JNIEnv *env, jclass obj, jint source) {
}

void java_set_output_format(JNIEnv *env, jclass obj, jint format) {
    tsl::AppState* _appState = __STATE;
    if (_appState == nullptr) return;
    _STATE->format = (uint8_t) format;
}

void java_set_micrec_format(JNIEnv *env, jclass obj, jint format) {
}

jstring java_ofl(JNIEnv *env, jclass obj);
#include "MidiReceiver.h"
#include <MidiSaver.h>

JNINativeMethod tsl::android::methodTable[] = {
        {"java_record_live",       "(I)I",                  (void *) java_record_live},
        {"java_receive_midievent", "(BBB)V",                (void *) java_receive_midievent},
        {"save_midimapping_callback", "(Ljava/lang/String;)I", (void *) save_midimapping_callback},
        {"java_set_output_format", "(I)V",                  (void *) java_set_output_format},
        {"java_set_micrec_format", "(I)V",                  (void *) java_set_micrec_format},
        // hqresampling was declared+called in Java but never in this table — an
        // UnsatisfiedLinkError lying in wait behind its (currently hidden) pref.
        {"java_set_hqresampling",  "(Z)V",                  (void *) java_set_hqresampling},
        {"java_set_showfactory",   "(Z)V",                  (void *) java_set_showfactory},
        {"java_reload_presets",    "()V",                   (void *) java_reload_presets},
        {"java_set_trial_state",   "(I)V",                  (void *) java_set_trial_state},
        {"java_control",           "(I)V",                  (void *) java_control},
        {"java_ofl",               "()Ljava/lang/String;",  (void *) java_ofl},
        {"guiSetup",               "(ZII)V",                (void *) guiSetup},
};
int tsl::android::methodTableSize =
        sizeof(tsl::android::methodTable) / sizeof(tsl::android::methodTable[0]);

const char *tsl::android::AppClassPath = "me/rocks/pocketanalog/MyApplication";
const char *tsl::android::ActivityClassPath = "me/rocks/pocketanalog/MainActivity";

#endif // __ANDROID__

// Both the album tag on exported recordings (player.cpp's set_album) and the
// DESKTOP settings directory (Application Support/<name>, APPDATA\<name>) derive
// from this. Safe to rename only because no desktop build has ever shipped; on a
// published platform this would silently strand every user's settings in the old
// directory. Android does not use it for storage at all — its root comes from
// getExternalFilesDir, i.e. from the frozen applicationId.
const char* tsl::app::appName = "Voltaic";
#ifdef LICENSE_CHECK_ENABLED
// Local credential-store key only: LicenseChecker passes it straight to
// OSCredentialStore (the OS keychain service name) and nothing else. It is NOT
// the product identity on the wire — the server keys off the separate
// compile-time `productNumber` in lc.cpp, which is unchanged.
//
// Renaming therefore costs one re-login on machines that already hold stored
// credentials, and nothing more. Safe here because LICENSE_CHECK_ENABLED is set
// only for the desktop app and the plugin formats (never Android, which uses
// Play Billing), and no desktop build has shipped.
const char* tsl::app::lsName = "voltaic";
#endif

#include <views.h>

static void handler3(int sig) {
    ;
}

#include <security/signature.h>
#include <random>
#include <chrono>

#if defined(_WIN32)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({ 0x27, 0x9F, 0xEC, 0x7E, 0x29, 0x05, 0xFB, 0x82,
    0xAB, 0x6C, 0x35, 0x4B, 0xC3, 0x19, 0x8E, 0xC4,
    0x1B, 0xD7, 0x2F, 0x43, 0x08, 0xAD, 0x3D, 0x72,
    0x79, 0x37, 0xD8, 0xC4, 0xC7, 0xB8, 0xE4, 0x78 });
#elif defined(__APPLE__)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({ 0xED, 0x4E, 0xF4, 0x80, 0x6A, 0x30, 0xF1, 0x6B,
    0x3B, 0x23, 0xD2, 0x13, 0x29, 0x6D, 0x62, 0x39,
    0x3C, 0x84, 0xB3, 0x89, 0x86, 0xD7, 0x54, 0xD4,
    0x85, 0xB9, 0x05, 0xD3, 0xDD, 0x1E, 0x51, 0x5F });
#elif defined(__ANDROID__)
static constexpr auto GOLDEN_CERT = tsl::security::make_secure<32>({
    0x4f, 0xc6, 0x69, 0xb4, 0xe7, 0xe4, 0x52, 0xb3,
    0x8e, 0xb2, 0x10, 0x7f, 0xda, 0x1a, 0x33, 0x1c,
    0xf3, 0x79, 0xf0, 0x64, 0x1a, 0x48, 0x06, 0x6a,
    0x30, 0x14, 0xbf, 0x66, 0x17, 0x6c, 0x93, 0x93 });
#endif

static void startIntegrityThread(tsl::AppState* _appState) {
    _STATE->integrityThread = std::thread([_appState]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(240, 480);
        _STATE->waitNotify.wait_for_signal(dist(gen) * 1000);
        if (_STATE->destroyRequested.load()) return;
        uint8_t computed[32];
#if defined(_WIN32)
        for (auto& b : computed) b = (uint8_t)(rand() % 256);
#else
        arc4random_buf(computed, 32);
#endif
        tsl::security::getCertHash(computed);
        uint8_t stack_hash[32];
        GOLDEN_CERT.reveal(stack_hash);
        auto deg = tsl::security::getDegradation(computed, stack_hash);
        memset(computed, 0, 32);
        memset(stack_hash, 0, 32);
        if (deg != 0)
            _DATA->integrity_failed.store(true);
    });
}

#ifndef __ANDROID__
static bool sigverify() {
#if defined(_WIN32) || defined(__APPLE__)
    return tsl::security::verifyMySignature2();
#else
    return true;
#endif
}
#endif

#ifdef IS_SINGLETON

void tsl::app::createInstance() {
    auto _appState = __STATE = new tsl::AppState;
    _appState->data = new DATA(_appState);
}

void tsl::app::setup(tsl::AppState* _appState) {
#else
tsl::AppState* tsl::app::setup(bool /*uisRunningAsPlugin*/) {
    auto* _appState = new tsl::AppState;
    _appState->data = new DATA(_appState);
    _STATE->onDestroy_ = [_appState]() { delete _appState->data; };
#endif
    _STATE->onDestroy_ = [_appState]() { delete _appState->data; };
    _appState->graphics.appDimension = tsl::graphics::LandScape;


    tsl::envelope::GenerateWindow(_DATA->sinewave, WINDOW_SIZE, tsl::envelope::SINE_FULL);
    _DATA->sinewave[WINDOW_SIZE] = _DATA->sinewave[0];
#ifdef __ANDROID__
    _STATE->player.init();
#endif
    _STATE->onedsr = 1. / _STATE->sr;
    _STATE->nanospersample = tsl::time::nanosPerSecond / _STATE->sr;
    _STATE->ksr = _STATE->sr / 64.;
    _STATE->onedksr = 1. / _STATE->ksr;
    _STATE->pidsr = PI_P / _STATE->sr;
    _STATE->twopidsr = TWOPI_P / _STATE->sr;
    LOGI("Samplerate = %g, Bufsize = %d", _STATE->sr, _STATE->currentBufSize);

    _DATA->currentrecoff = 0;

    initparams(_appState);

    sequencer::init(_appState);
    onGotSampleRate(_appState, _STATE->sr);

#ifdef __ANDROID__
    ATTACH
    if (env) {
        jclass appclazz = tsl::android::appclass;
        jclass actclazz = tsl::android::activityclass;

        if (appclazz) {
            jfieldID fid = env->GetStaticFieldID(appclazz, "startPoweredOn", "Z");
            if (fid) {
                _DATA->startPoweredOn = (bool)env->GetStaticBooleanField(appclazz, fid);
            } else {
                env->ExceptionClear();
            }

            fid = env->GetStaticFieldID(appclazz, "showFactoryPresets", "Z");
            if (fid) {
                _DATA->showFactoryPresets.store((bool)env->GetStaticBooleanField(appclazz, fid));
            } else {
                env->ExceptionClear();
            }
        }

        if (actclazz) {
            mid = env->GetStaticMethodID(actclazz, "getInstance2", "()Ljava/lang/Object;");
            if (mid) {
                jobject instance = env->CallStaticObjectMethod(actclazz, mid);
                if (instance) {
                    jmethodID midFormat = env->GetMethodID(env->GetObjectClass(instance), "getOutputFormat", "()I");
                    if (midFormat) {
                        _STATE->format = env->CallIntMethod(instance, midFormat);
                    } else {
                        env->ExceptionClear();
                    }
                }
            } else {
                env->ExceptionClear();
            }
        }
    }
    DETACH
#else
    {
        // Desktop mirror of the JNI reads above. Settings2 fires intChangeCallback
        // only on Set(), and the settings view itself is built lazily on first
        // open, so persisted values must be applied here or a restart forgets
        // them until the user visits Settings. Defaults match the XML in
        // callbacks_loop_controls.cpp.
        tsl::settings::SettingsManager mgr{ tsl::app::getStoragePath(tsl::app::appName) };
        _STATE->format  = (uint8_t)mgr.Get("audio_format", 0);
        _STATE->askName = mgr.Get("ask_name", false);
        _DATA->hqresampling.store(mgr.Get("hq_resampling", false));
        _DATA->showFactoryPresets.store(mgr.Get("show_factory", true));
    }
#endif
    Preset::setupDefault(_appState);
    Preset::setupFactory(_appState);
#ifdef RELEASEBUILD
    startIntegrityThread(_appState);
#endif

    LOGI("Ready.");
#ifndef IS_SINGLETON
    return _appState;
#endif
}

void cleanUp(tsl::AppState* _appState) {
    {
        std::lock_guard lk(_STATE->queue_draw);
        std::lock_guard lk2(_STATE->queue_callback);
        _appState->queue_draw.reset();
        _appState->queue_callback.reset();
        _appState->graphics.reset();
    }
    delete _appState;
}

void sampleRateFromApp(tsl::AppState* _appState, double sr) {
    _STATE->rsOut.init(_STATE->sr / sr);
    _STATE->srHost = sr;
#if defined DOES_INPUT_RESAMPLING
    _STATE->rsIn.init(sr / _STATE->sr);
#endif
    onGotSampleRate(_appState, (int)sr);
    LOGI("Samplerate from app: %g", sr);
}


/* --------------------------------- ABOUT -------------------------------------
Original Author: Adam Yaxley
Website: https://github.com/adamyaxley
License: See end of file
Obfuscate
Guaranteed compile-time string literal obfuscation library for C++14
Usage:
Pass string literals into the AY_OBFUSCATE macro to obfuscate them at compile
time. AY_OBFUSCATE returns a reference to an ay::obfuscated_data object with the
following traits:
	- Guaranteed obfuscation of string
	The passed string is encrypted with a simple XOR cipher at compile-time to
	prevent it being viewable in the binary image
	- Global lifetime
	The actual instantiation of the ay::obfuscated_data takes place inside a
	lambda as a function level static
	- Implicitly convertable to a char*
	This means that you can pass it directly into functions that would normally
	take a char* or a const char*
Example:
const char* obfuscated_string = AY_OBFUSCATE("Hello World");
std::cout << obfuscated_string << std::endl;
----------------------------------------------------------------------------- */

#ifndef AY_OBFUSCATE_DEFAULT_KEY
#define AY_OBFUSCATE_DEFAULT_KEY ay::generate_key(__LINE__)
#endif

namespace ay {
    using size_type = unsigned long long;
    using key_type = unsigned long long;

    constexpr key_type generate_key(key_type seed) {
        key_type key = seed;
        key ^= (key >> 33);
        key *= 0xff51afd7ed558ccd;
        key ^= (key >> 33);
        key *= 0xc4ceb9fe1a85ec53;
        key ^= (key >> 33);
        key |= 0x0101010101010101ull;
        return key;
    }

    constexpr void cipher(char *data, size_type size, key_type key) {
        for (size_type i = 0; i < size; i++) {
            data[i] ^= char(key >> ((i % 8) * 8));
        }
    }

    template<size_type N, key_type KEY>
    class obfuscator {
    public:
        constexpr obfuscator(const char *data) {
            for (size_type i = 0; i < N; i++) {
                m_data[i] = data[i];
            }
            cipher(m_data, N, KEY);
        }

        constexpr const char *data() const { return &m_data[0]; }
        constexpr size_type size() const { return N; }
        constexpr key_type key() const { return KEY; }

    private:
        char m_data[N]{};
    };

    template<size_type N, key_type KEY>
    class obfuscated_data {
    public:
        obfuscated_data(const obfuscator<N, KEY> &obfuscator) {
            for (size_type i = 0; i < N; i++) {
                m_data[i] = obfuscator.data()[i];
            }
        }

        ~obfuscated_data() {
            for (size_type i = 0; i < N; i++) {
                m_data[i] = 0;
            }
        }

        operator char *() {
            decrypt();
            return m_data;
        }

        void decrypt() {
            if (m_encrypted) {
                cipher(m_data, N, KEY);
                m_encrypted = false;
            }
        }

        void encrypt() {
            if (!m_encrypted) {
                cipher(m_data, N, KEY);
                m_encrypted = true;
            }
        }

        bool is_encrypted() const { return m_encrypted; }

    private:
        char m_data[N];
        bool m_encrypted{true};
    };

    template<size_type N, key_type KEY = AY_OBFUSCATE_DEFAULT_KEY>
    constexpr auto make_obfuscator(const char(&data)[N]) {
        return obfuscator<N, KEY>(data);
    }
}

#define AY_OBFUSCATE(data) AY_OBFUSCATE_KEY(data, AY_OBFUSCATE_DEFAULT_KEY)

#define AY_OBFUSCATE_KEY(data, key) \
    []() -> ay::obfuscated_data<sizeof(data)/sizeof(data[0]), key>& { \
        static_assert(sizeof(decltype(key)) == sizeof(ay::key_type), "key must be a 64 bit unsigned integer"); \
        static_assert((key) >= (1ull << 56), "key must span all 8 bytes"); \
        constexpr auto n = sizeof(data)/sizeof(data[0]); \
        constexpr auto obfuscator = ay::make_obfuscator<n, key>(data); \
        static auto obfuscated_data = ay::obfuscated_data<n, key>(obfuscator); \
        return obfuscated_data; \
    }()

#ifdef __ANDROID__
jstring java_ofl(JNIEnv *env, jclass obj) {
    return env->NewStringUTF(AY_OBFUSCATE(
                                     "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsVqqDTu/wkUr/tw8TaDtIz62NiT76gYQe1toi+e4LdBxrQm/PZGvuMam5jdPuZBCs8qeN3dYywpGAOxG2ksEnfNPP5Z1ZqLMD0y00iC1BevdZa/4ZG5/np0Sg1J7RbSFa9tEifKBPEnhCYWRvtbFoGr9P8qvQb6e3HZCReXxA7sU7frh1fMOXLD2UvOkmFGxr9/FDfytWiPbfe4LsJdOwCpgzWEqnrqJWJpjWLKMBXil/MPF2mpt/uinRdH5i8FQilgGReOTblzZUzgQAKtSY5z9gn6vGQ/wEV6+CxhWTk5MwRLBvlmu47GK2IHliGK0hOjLZOiW6j6H3cb30kPd+wIDAQAB"));
}
#endif
