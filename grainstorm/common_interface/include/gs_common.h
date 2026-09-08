#pragma once
//
// Created by pr on 13.10.19.
//

#include <types.h>
#include <string_view>
#include <span>

static_assert(__cplusplus >= 201703L, "C++17 required");

// PITCHMAP2 (the multi-resolution A/B build of PITCHMAP) is compiled out.
// Set to 1 to bring it back: this one switch drops it from the MONO EFFECTS
// list, from the fx-power callback table, from the GUI page mapping and from
// pitchmap2.h/.cpp, so nothing else needs touching. SPACE_PITCHMAP2 stays in
// the _paramspaces enum either way -- removing it would shift every space id
// after it and break saved spaces.
#define GS_ENABLE_PITCHMAP2 0

inline constexpr std::span<const float> emptyFloats{};

template <size_t N>
constexpr std::span<const std::string_view>
makeStringSpan(const std::string_view(&arr)[N]) {
    return { arr, N };
}

template <size_t N>
constexpr std::span<const float>
makeFloatSpan(const float(&arr)[N]) {
    return { arr, N };
}

inline constexpr std::string_view lfospaces[] = { "MAIN", "EDIT", "RND"
#if defined(PLUGIN_MODE) || defined(OS_IOS)
,"SYNC"
#endif
};


inline constexpr std::string_view fmunitnames[] = { "UNIT1", "UNIT2", "UNIT3", "UNIT4", "UNIT5", "UNIT6" };
#define SHIMMER_MODES_NAMES { "SHIFT OFF", "SINGLE", "DUAL", "SINGLE REVERSE", "DUAL REVERSE" }
inline constexpr std::string_view shimmermodesnames[] = SHIMMER_MODES_NAMES;


inline constexpr std::string_view shimmerdarkmodenames[] = { "LOFI MODE", "NORMAL MODE" };

inline constexpr std::string_view shimmeralgnames[] = { "STEREO1", "STEREO2", "MONO" };

inline constexpr std::string_view grainmods1[] = {
        "GRANULATION", "GRANULATION2", "GRAINGEN", "BPM", "SEQUENCER1", "SEQUENCER2", "ARP",
        "RANDOM PITCH", "RESON", "RINGMOD", "REVERB", "PHASE VOCODER", "CROSS SYNTHESIS", "TAPE MODE"
#if defined PLUGIN_MODE || defined STANDALONE_MODE
        ,"GAIN/SYNC"
#endif
};

inline constexpr const float grainmod1values[] = { SPACE_GRAINSETTINGS, SPACE_GRAINSETTINGS2, SPACE_GRAINGEN, SPACE_BPM,SPACE_GRAINSEQUENCER2, SPACE_GRAIN_SEQUENCER, SPACE_ARP, SPACE_PITCH,
                                        SPACE_GRAIN_BP, SPACE_RM_GRAIN, SPACE_GRAINREVERB, SPACE_PV_MAIN, SPACE_CROSS_MAIN, SPACE_LOOPER
#if defined PLUGIN_MODE || defined STANDALONE_MODE
    , SPACE_GRAINGAIN
#endif
};

inline constexpr std::string_view grainmods2[] = {
        "GRANULATION", "GRANULATION2", "GRAINGEN", "BPM", "SEQUENCER1", "SEQUENCER2", "ARP", "RANDOM PITCH", "BUZZ", "MODAL",
        "VCO", "VCO2", "FILTER", "NOISE", "RESON", "RINGMOD",
        "WAVESET", "DRIVE", "DISPERSE", "VOWEL", "PLUCK",
        "REVERB", "PDETECT", "PHASE VOCODER", "CROSS SYNTHESIS", "TAPE MODE"
#if defined PLUGIN_MODE || defined STANDALONE_MODE
        ,"GAIN/SYNC"
#endif
};

inline constexpr const float grainmod2values[] = { SPACE_GRAINSETTINGS, SPACE_GRAINSETTINGS2, SPACE_GRAINGEN, SPACE_BPM, SPACE_GRAINSEQUENCER2, SPACE_GRAIN_SEQUENCER, SPACE_ARP,
                                        SPACE_PITCH, SPACE_GRAINPART,
                                        SPACE_GRAINMODAL, SPACE_GRAINVCO, SPACE_GRAINVCO2, SPACE_GRAINFILTER, NOISEGRAINEFFECT, SPACE_GRAIN_BP, SPACE_RM_GRAIN,
                                        // This is display order only. The SPACE_ ids themselves stay at the
                                        // end of ParameterSpaces - inserting mid-enum there would shift every
                                        // later space id and break saved spaces.
                                        SPACE_GRAINWAVESET, SPACE_GRAINDRIVE, SPACE_GRAINDISPERSE, SPACE_GRAINVOWEL, SPACE_GRAINPLUCK,
                                        SPACE_GRAINREVERB, SPACE_PDETECTGRAIN,  SPACE_PV_MAIN, SPACE_CROSS_MAIN, SPACE_LOOPER
#if defined PLUGIN_MODE || defined STANDALONE_MODE
    , SPACE_GRAINGAIN
#endif
};

#define ARP_CYCLE_MODES { "UP", "DOWN", "UP/DOWN", "BOUNCE" }
inline constexpr std::string_view cycle_modes[] = ARP_CYCLE_MODES;


inline constexpr std::string_view lfoItems[] = { "PREGAIN", "PANNING", "SPEED", "GRAINSIZE", "DENSITY", "DEVIATION", "LOOP POS", "READ OFFSET",
                                 "WRITE OFFSET", "RND READ",
                                 "ENV CYCLES", "ENV PHASE", "ENV INTERPOLATE",
                                 "SEMITONES", "PITCH", "GLISS", "PITCH MIN", "PITCH MAX", "GRAINVCO1PW", "GRAINVCO2PW", "GRAINVCO3PW", "NOISE GRAIN GAIN",
                                 // Grain effects. A grain-side LFO is a per-grain sample-and-hold
                                 // (lfo->buf[step_point_grain]), so on the MIN/MAX effects it moves
                                 // the CENTRE of the range and MIN/MAX stays the per-grain spread.
                                 "GRAIN WAVESET AMT", "GRAIN DRIVE", "GRAIN DISPERSE FREQ",
                                 "GRAIN DISPERSE DEPTH", "GRAIN VOWEL", "GRAIN VOWEL BW",
                                 "GRAIN PLUCK TUNE",
                                 "PVFORMSTRETCH", "PVWARPFROMB",
                                 "PV PERC/HARM", "PV CONTRAST", "PV CONTRAST FLOOR", "PV SNAP AMOUNT", "PV SNAP ROOT",
                                 "PV RES ROOT", "PV RES DECAY", "PV RES MIX",
                                 "PV FREEZE", "PV FREEZE PROB", "PV FREEZE MIX",
                                 "CROSS INTERPOLATION",
                                 "CROSS TRANSPORT", "CROSS STACK TILT", "CROSS STACK PEAKS",
                                 "PITCH SHIFTER",
                                 "SSBMOD CARRIER", "PHASER BAND", "PHASER IV BAND",
                                 "PHASER IV DISTANCE", "FLANGER DELAY", "DELAY DELAY",
                                 "MDELAY DELAY1", "MDELAY DELAY2", "MDELAY DELAY3", "MDELAY DELAY4",
                                 "MDELAY DELAY5", "MDELAY DELAY6", "MDELAY DELAY7", "MDELAY DELAY8", "LOFI SR", "FM CPS", "FM INDEX", "NOISE GAIN",
                                 "RESON CF", "RESON Q", "VOX VOWEL", "VOX CPS", "VOX2 VOWEL", "VOX2 BW",
                                 "RETUNE CPS", "RETUNE AMOUNT",
                                 "SPECFILT MAG", "SPECFILT PHASE",
                                 "LFO1 RATE", "LFO2 RATE", "LFO3 RATE" };
inline constexpr const float lfoValues[] = { PREGAIN, GRAINPAN, SPEED, GRAINSIZE, DENSITY, DENSDEV, LOOP_POS, READOFFSET_LFO, WRITEOFFSET,
                                   RNDREAD, AWINCYLCES, ENVPHASE, GRAINENVINTERPOL, SEMITONES_LFO, PITCH, GRAINGLISS, PITCHMIN, PITCHMAX, GRAINVCO1PW, GRAINVCO2PW,
                                  GRAINVCO3PW, NOISEGRAINGAIN,
                                  // the pair destinations carry the MIN id (the effect derives the
                                  // spread from MAX - MIN itself)
                                  GRAINWSAMT, GRAINDRVMIN, GRAINDISPFMIN,
                                  GRAINDISPDEPTH, GRAINVOWMIN, GRAINVOWBW,
                                  GRAINPLKMIN,
                                  STRETCHCOEFF, PVSPECBOUNDBIN,
                                   PVHPSSMIX, PVCONTRAST, PVCONTRASTFLOOR, PVSNAPAMT, PVSNAPROOT,
                                   PVRESROOT, PVRESDECAY, PVRESMIX,
                                   PVFREEZEAMT, PVFREEZEPROB, PVFREEZEMIX,
                                   IPOL,
                                   CROSSTRANSAMT, CROSSSTACKTILT, CROSSSTACKN,
                                   PITCHSHIFTSHIFT, SSBMODRATE, PHASERBAND, PHASER4BAND,
                                   PHASER4SPACING, FLANGERDELAY, DELAYDEL, MDELAY1DEL,
                                  MDELAY2DEL, MDELAY3DEL, MDELAY4DEL, MDELAY5DEL, MDELAY6DEL,
                                  MDELAY7DEL, MDELAY8DEL, BCSR, FMCPS, FMI,NOISEMONOGAIN, BPCENTER, BPBW, VOXVOWEL, VOXFREQ, VOX2VOWEL, VOX2BW,
                                  PMAPCPS, PMAPAMT,
                                  SPECFILTMAGLPCUT2, SPECFILTFREQLPCUT2,
                                  LFO1CPS, LFO2CPS,
                                   LFO3CPS };
inline constexpr std::string_view lfoItems2[] = {
        "PREGAIN", "PANNING", "SPEED", "GRAINSIZE", "DENSITY", "DEVIATION", "LOOP POS", "READ OFFSET", "WRITE OFFSET", "RND READ",
        "ENV CYCLES", "ENV PHASE", "ENV INTERPOLATE", "SEMITONES", "PITCH", "GLISS", "PITCH MIN", "PITCH MAX","PVFORMSTRETCH", "PVWARPFROMB",
        "PV PERC/HARM", "PV CONTRAST", "PV CONTRAST FLOOR", "PV SNAP AMOUNT", "PV SNAP ROOT",
        "PV RES ROOT", "PV RES DECAY", "PV RES MIX",
        "PV FREEZE", "PV FREEZE PROB", "PV FREEZE MIX",
        "CROSS INTERPOLATION",
        "CROSS TRANSPORT", "CROSS STACK TILT", "CROSS STACK PEAKS",
        "SSBMOD CARRIER", "RESON CF", "RESON Q", "VOX VOWEL", "VOX CPS", "VOX2 VOWEL", "VOX2 BW",
        "RETUNE CPS", "RETUNE AMOUNT",
        "SPECFILT MAG", "SPECFILT PHASE",
        "LFO1 RATE",
        "LFO2 RATE", "LFO3 RATE" };
inline constexpr const float lfoValues2[] = { PREGAIN, GRAINPAN, SPEED, GRAINSIZE, DENSITY, DENSDEV, LOOP_POS, READOFFSET_LFO, WRITEOFFSET,
                                  RNDREAD, AWINCYLCES, ENVPHASE, GRAINENVINTERPOL, SEMITONES_LFO, PITCH, GRAINGLISS, PITCHMIN, PITCHMAX, STRETCHCOEFF, PVSPECBOUNDBIN,
        PVHPSSMIX, PVCONTRAST, PVCONTRASTFLOOR, PVSNAPAMT, PVSNAPROOT,
        PVRESROOT, PVRESDECAY, PVRESMIX,
        PVFREEZEAMT, PVFREEZEPROB, PVFREEZEMIX,
        IPOL,
        CROSSTRANSAMT, CROSSSTACKTILT, CROSSSTACKN,
        SSBMODRATE,BPCENTER, BPBW, VOXVOWEL, VOXFREQ, VOX2VOWEL, VOX2BW,
        PMAPCPS, PMAPAMT,
        SPECFILTMAGLPCUT2, SPECFILTFREQLPCUT2,
        LFO1CPS, LFO2CPS, LFO3CPS };

// The names are looked up positionally against the values (toString.cpp, and the
// toast in apply.cpp), so a name added without its value silently mislabels
// every destination after it.
static_assert(std::size(lfoItems) == std::size(lfoValues),
              "lfoItems and lfoValues must stay in step");
static_assert(std::size(lfoItems2) == std::size(lfoValues2),
              "lfoItems2 and lfoValues2 must stay in step");

// Direction of a follower's action on its target parameter (FOLLOWER*ADD).
// Formerly lived in EnvelopeDetector.h, alongside the class Follower replaced.
#define DETECT_ADD 0
#define DETECT_SUB 1

inline constexpr std::string_view env_detectoritems[] = {
        "PITCH SHIFTER", "SSBMOD CARRIER", "DELAY DELAY", "MDELAY DELAY1", "MDELAY DELAY2",
        "MDELAY DELAY3", "MDELAY DELAY4", "MDELAY DELAY5", "MDELAY DELAY6", "MDELAY DELAY7",
        "MDELAY DELAY8", "DISTORTION MIX", "LOFI MIX", "GENCREV MIX", "FM INDEX", "FM VIBRATE",
        "FM VIBAMOUNT", "FM WET", "BOWED POS", "NOISE MIX", "RESON CF", "MOOGLADDER", "CREVERB MIX", "CHORUS MIX", "VOX VOWEL", "VOX WET", "VOX2 VOWEL", "VOX2 WET",
        "SPECFILT MAG", "SPECFILT PHASE" };
inline constexpr const float env_detectorvalues[] = { PITCHSHIFTSHIFT, SSBMODRATE, DELAYDEL, MDELAY1DEL, MDELAY2DEL,
                                           MDELAY3DEL, MDELAY4DEL, MDELAY5DEL, MDELAY6DEL,
                                           MDELAY7DEL, MDELAY8DEL, DISTMIX, BCMIX,
                                           SPECDELMIX, FMI,
                                            FMVIBRATEA, FMVIBDEPTHA, FMWET, BOWEDPOS, NOISEMONOMIX, BPCENTER, MOOGCUT, CREVMIX, CHORUSMIX, VOXVOWEL, VOXWET, VOX2VOWEL, VOX2WET,
                                            SPECFILTMAGLPCUT2, SPECFILTFREQLPCUT2 };
inline constexpr std::string_view env_detectoritems2[] = {
        "SSBMOD CARRIER", "DISTORTION MIX", "NOISE MIX", "RESON CF", "CREVERB MIX", "CHORUS MIX", "VOX VOWEL", "VOX WET", "VOX2 VOWEL", "VOX2 WET",
        "SPECFILT MAG", "SPECFILT PHASE" };
inline constexpr const float env_detectorvalues2[] = { SSBMODRATE, DISTMIX, NOISEMONOMIX, BPCENTER, CREVMIX, CHORUSMIX, VOXVOWEL, VOXWET, VOX2VOWEL, VOX2WET,
                                                      SPECFILTMAGLPCUT2, SPECFILTFREQLPCUT2 };

// Same positional pairing as the LFO lists above, and track.cpp builds followerMap
// straight off env_detectorvalues -- a value without its name mislabels the menu,
// a name without its value builds no Follower and the effect's .at() throws.
static_assert(std::size(env_detectoritems) == std::size(env_detectorvalues),
              "env_detectoritems and env_detectorvalues must stay in step");
static_assert(std::size(env_detectoritems2) == std::size(env_detectorvalues2),
              "env_detectoritems2 and env_detectorvalues2 must stay in step");




inline constexpr std::string_view fxtypes1[] = {
        "SSB MOD", "DISTORTION", "VOCODER", "RESON", "BANDREJECT",
        "LP/HP", "EQ5", "CREVERB", "COMPRESSOR"
};
inline constexpr const float fxtypes1values[] = { SPACE_SSB, SPACE_DISTORT, SPACE_VOCODER, SPACE_BANDPASS,
                                       SPACE_BANDREJECT, SPACE_HPLP, SPACE_EQ5, SPACE_CONVOLVER,
                                       SPACE_COMPRESSION };


// fxtypes2/fxtypes2values are DISPLAY ORDER for the MONO EFFECTS selector, and
// nothing else: both consumers (findIndexFloat in fxOrder.cpp, the membership
// test above) are order-independent, and the processing chain order lives in
// track->fx_queue. So entries can be moved freely here -- but the two arrays
// are read positionally against each other, so move name and value together.
inline constexpr const float fxtypes2values[] = { SPACE_SHIFTER, SPACE_SSB, SPACE_PHASER,
                                       SPACE_PHASER4, SPACE_FLANGER, SPACE_DELAY, SPACE_MDELAY, SPACE_DISTORT,
                                       SPACE_BITCRUSHER,
                                       SPACE_SATURATOR, SPACE_VOCODER,
                                       SPACE_LPCVOCODER, /*SPACE_MODFMVOCODER,*/
                                       SPACE_SPECTRAL_FILTER, SPACE_SPECDEL2,
                                       SPACE_PVAMPS, SPACE_FM, SPACE_FM2, SPACE_BOWED, SPACE_VOX,
                                       NOISEMONOEFFECT,  SPACE_MODAL, SPACE_BANDPASS, SPACE_VOX2, SPACE_MOOGLADDER,
                                       SPACE_BANDREJECT,
                                       SPACE_HPLP, SPACE_EQ, SPACE_EQ5, SPACE_DYNEQ5, SPACE_CONVOLVER,
                                       SPACE_CREVERB2, SPACE_SPECDEL,
                                       SPACE_SPECDELALG,
                                       SPACE_COMPRESSION, SPACE_MULTICOMP,
                                       SPACE_PITCHMAP,
#if GS_ENABLE_PITCHMAP2
		SPACE_PITCHMAP2,
#endif
                                       SPACE_PDETECT
#if defined PLUGIN_MODE || defined STANDALONE_MODE
		, SPACE_INPUTGAIN
#endif
};

inline constexpr std::string_view fxtypes2[] = {
        "PITCH SHIFTER", "SSB MOD", "PHASER", "PHASER IV", "FLANGER",
        "DELAY", "MULTI DELAY", "DISTORTION",
        "LOFI", "SATURATOR", "VOCODER", "LPCVOCODER", /*"MODFMVOCODER",*/ "SPECTRAL FILTER",
        "SPECTRAL DELAY", "PVAMPS", "FM", "FM2", "BOWED", "VOX",
        "NOISE", "MODAL", "RESON", "VOX2", "MOOGLADDER", "BANDREJECT",
        "LP/HP", "EQ", "EQ5", "EQ5DYN", "CREVERB", "CREVERB2", "GENCREVERB", "GENCREVERB ALG", "COMPRESSOR", "MULTICOMP",
        "RETUNE",
#if GS_ENABLE_PITCHMAP2
		"RETUNE2",
#endif
        "PDETECT"
#if defined PLUGIN_MODE || defined STANDALONE_MODE
		, "INPUT GAIN"
#endif
};

static_assert(sizeof(fxtypes2) / sizeof(fxtypes2[0])
              == sizeof(fxtypes2values) / sizeof(fxtypes2values[0]),
        "fxtypes2 and fxtypes2values are read positionally against each other -- "
        "an entry moved or added on one side only mislabels every effect after it");

inline constexpr const float reverbtypevalues[] = { SPACE_CHORUS, SPACE_PINGPONG,
                                         SPACE_REVERB1, SPACE_REVERB2, SPACE_REVERB3,
                                         SPACE_REVERB4, SPACE_REVERB5, SPACE_REVERB6, SPACE_MODALREV, SPACE_STC,
                                         SPACE_LIMITER, SPACE_CLIPPER, SPACE_DCS, SPACE_MONOSTEREO, SPACE_SPECTRUM };
inline constexpr std::string_view reverbtypes[] = {
    /*"BASS", */"CHORUS", "PPDELAY",
    "REVERB1",
    "REVERB2", "REVERB3", "REVERB4", "REVERB5", "REVERB6", "REVERB7", "COMPRESSOR", "LIMITER", "CLIPPER", "DC BLOCKER", "STEREOWIDTH", "SPECTRUM"
};


inline constexpr const float reverbtypevalues2[] = { SPACE_CHORUS, SPACE_REVERB3, SPACE_REVERB6, SPACE_MONOSTEREO };
inline constexpr std::string_view reverbtypes2[] = {
        "CHORUS", "REVERB3", "REVERB6", "STEREOWIDTH" };



inline constexpr std::string_view voxvoicenames[] = { "BASS", "TENOR", "CTENOR", "ALTO", "SOPRANO" };

/* CREVERB IR level normalisation. RAW is the historical behaviour (the only
   scaling is the fixed CONVMYFLT), which makes GAIN mean something different
   for every IR; PEAK equalises the loudest tap, RMS equalises energy - the
   one that keeps the wet level steady when you swap reverb tails. Order IS
   the CREVNORM value. */
inline constexpr std::string_view crevnormmodes[] = { "RAW", "PEAK", "RMS" };

/* WAVESET mode. Order IS the GRAINWSMODE value - see GrainWaveset.cpp for
   what AMT means in each one. */
inline constexpr std::string_view grainwavesetmodes[] = { "REPEAT", "OMIT", "REVERSE", "NORMALIZE",
                                                          "SHUFFLE", "SINE" };

/* Per-grain waveshaper. Order IS the GRAINDRVTYPE value. ASYM and FOLD
   generate DC, which the grain envelope would turn into a thump, so both run
   through the effect's DC blocker. */
inline constexpr std::string_view graindrivetypes[] = { "SOFT", "HARD", "FOLD", "ASYM", "CHEBY" };

inline constexpr std::string_view env_follower_types[] = { "PEAK", "MS" };
inline constexpr std::string_view env_follower_modes[] = { "ADD", "SUB" };
inline constexpr std::string_view mdelaymodes[] = { "PARALLEL", "SERIES" };
inline constexpr const float envfollowermodesvalues[] = { 0, 1 };

inline constexpr std::string_view envfollowers[] = { "FOLLOWER1", "FOLLOWER2", "FOLLOWER3" };

inline constexpr std::string_view lfos[] = { "LFO1", "LFO2", "LFO3" };

inline constexpr std::string_view lfo_envelopes_names[] = { "SINE LFO", "TRIANGLE", "SAW", "PULSE", "USER", "RND" };

inline constexpr std::string_view syncfactornames[] = { "2", "3", "5", "7" };
inline constexpr const float syncfactors[] = { 2, 3, 5, 7 };

inline constexpr const float vcoModes[] = { -1, 0, 1, 2 };
inline constexpr const float bass_modes[] = { 12, 0, 10 };
#define VCO_WAVEFORMS {"SINE", "TRI", "SAW", "PULSE"}
inline constexpr std::string_view vcoWaveforms[] = VCO_WAVEFORMS;

#define SSBMODECHARS { "UPPER SIDEBAND", "LOWER SIDEBAND", "DOUBLE SIDEBAND" }
inline constexpr std::string_view ssbmodeschars[] = SSBMODECHARS;

inline constexpr std::string_view lfonames[] = { "LFO1", "LFO2", "LFO3" };

#define OSCBANKPHASENAMES { "KEEP PHASE", "RUNNING PHASE", "RANDOM PHASE" }
inline constexpr std::string_view oscbankphasenames[] = OSCBANKPHASENAMES;
#define FMINSTNAMES { "MODFM", "RHODEY", "WURLEY", "VIOLIN", "FLUTE", "BELL", "METAL"}
inline constexpr std::string_view fminstnames[] = FMINSTNAMES;
inline constexpr const float fminstvalues[] = { FM_MODFM, /*FM_STRINGS1,
                                      FM_STRINGS2, FM_STRINGS3, FM_BRASS1, FM_BRASS2, FM_BRASS3,
                                      FM_TAMBOURA, */FM_RHODES, FM_WURLIE, /*FM_B3,*/
                                      FM_VIOLIN, /*FMVIOLIN2, */FM_FLUTE, FM_BELL,/* FM_TUB, FM_GONG,*/
                                      FM_METAL/*, FM_KALIMBA, FM_GLIDE*/ };
#define MODAL_NAMES {"DAHINA", "BANYAN", "XYLOPHON", /*"SPHERE", */"POT", "TUB BELL", "WOODEN BAR", "ALUMINIUM BAR", "VIBRAPHONE1", "VIBRAPHONE2", "CHALANDI PLATES", "TIBETAN BOWL1", "TIBETAN BOWL2", "TIBETAN BOWL3", "WINE GLASS", "WOOD1", "WOOD2", "WOOD3"/*, "HAND BELL"*/}
inline constexpr std::string_view modal_names[] = MODAL_NAMES;

// MODAL REVERB algorithm selector. 1..9 are the original random-bank
// algorithms; 10..15 are the content-adaptive bank and exist only with
// GS_MODALREV_ADAPTIVE. This table is also the parameter's enum range
// (IPlugEffect uses std::size(par.names) - 1), so it must stay in sync with
// ModalReverb's _nummodes/_oversvals/_shifts.
// 10..12 = FFT partial tracker retunes the bank; 13..15 = adaptive-oscillator
// (Hopf/PLL) pool, sample-accurate tracking, pool sizes 128/256/512.
inline constexpr std::string_view modalrevmodes[] = { " 1 ", " 2 ", " 3 ", " 4 ", " 5 ", " 6 ",
                                                     " 7 ", " 8 ", " 9 "
#if GS_MODALREV_ADAPTIVE
                                                     , "10 ", "11 ", "12 ",
                                                     "13 ", "14 ", "15 "
#endif
};

#define ENVELOPE_NAMES {"RECTANGULAR", "BLACKMAN", "HANNING", "KAISER", "SINE", "FULL SINE", "SAW", "FULL SAW", "TRIANGLE", "FULL TRIANGLE", "PULSE", "FULL PULSE", "FLATTOP", "FLATTOP2", "TUKEY2", "TUKEY3", "TUKEY4"}
inline constexpr std::string_view envelopesnames[] = ENVELOPE_NAMES;

#define CROSS_TYPES { "POLAR FORM", "CEPSTRUM", "CEPSTRUM WHITE", "INTERPOLATION", "VOCODER", "CONVOLUTION", "LPC", "TRANSPORT", "PARTIAL STACK", "SPECTRAL DUCK" }
inline constexpr std::string_view cross_types[] = CROSS_TYPES;

#define FFT_SIZES { "256", "512", "1024", "2048", "4096", "8192", "16384" }
inline constexpr std::string_view fft_sizes[] = FFT_SIZES;
inline constexpr const float fft_size_values[] = { 256, 512, 1024, 2048, 4096, 8192, 16384 };




#define VOCODER_CHANNELS { "2", "3", "4", "5", "6", "7", "8", "9", "10", "19", "28", "56", "112", "224" }
inline constexpr std::string_view vocoder_channels[] = VOCODER_CHANNELS;

inline constexpr const float vocoder_channels_values[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 19, 28,
                                         56, 112, 224 };

#define CROSS_MAG_TYPES { "SRC", "MOD", "ADD", "SUB", "MULTIPLY", "MIN", "MAX" }
inline constexpr std::string_view cross_mag_types[] = CROSS_MAG_TYPES;
// Must stay in step with enum cross_phase_type in types_grainstorm.h. SUB and
// MULTIPLY are implemented in cross_mag_phase() but had no name here, so they
// could never be selected.
#define CROSS_PHASE_TYPES { "SRC", "MOD", "ADD", "SUB", "MULTIPLY" }
inline constexpr std::string_view cross_phase_types[] = CROSS_PHASE_TYPES;

#define PV_TYPES { "PHASE CORRECTION", "PH CORRECTION II", "FORMANT STRETCH", "RANDOM PHASE", "ZERO PHASE", "OSC BANK", "FREQ WARP", "HARM/PERC", "CONTRAST", "SPECTRAL SNAP", "SPECTRAL RES", "SPECTRAL FREEZE" }
inline constexpr std::string_view pv_types[] = PV_TYPES;

// SPECTRAL DUCK direction. DUCK passes the carrier where the modulator is
// quiet (spectral sidechain); KEY is the inverse (carrier passes only where
// the modulator is loud).
#define CROSS_DUCK_MODES { "DUCK", "KEY" }
inline constexpr std::string_view cross_duck_modes[] = CROSS_DUCK_MODES;

// HARM/PERC soft-mask hardness. The exponent is applied to the ratio of the two
// median estimates, so keeping it to 1/2/4 makes it plain multiplication rather
// than a pow() per bin; BINARY assigns each bin wholly to one side.
#define PV_HPSS_MASK_TYPES { "SOFT", "MEDIUM", "HARD", "BINARY" }
inline constexpr std::string_view pv_hpss_mask_types[] = PV_HPSS_MASK_TYPES;

// SPECTRAL SNAP target grids. HARMONIC snaps each peak to an integer multiple of
// ROOT (so a noisy source turns into one pitched tone); the others snap to a
// tuning grid anchored on ROOT.
#define PV_SNAP_MODES { "HARMONIC", "OCTAVES", "FIFTHS", "WHOLE TONE", "CHROMATIC" }
inline constexpr std::string_view pv_snap_modes[] = PV_SNAP_MODES;

// SPECTRAL RES partial series. These pick where the bank's resonators sit, all
// anchored on ROOT: HARMONIC is a string or a tube, ODD a clarinet or a square,
// and the tuning-grid entries turn the bank into a chord or a tuned reverb
// rather than one note. INHARM stretches whichever series is chosen.
#define PV_RES_MODES { "HARMONIC", "ODD", "OCTAVES", "FIFTHS", "CHROMATIC" }
inline constexpr std::string_view pv_res_modes[] = PV_RES_MODES;

// PITCHMAP target sets. Every detected note is moved to the nearest member of
// the set, so the set is what the material gets re-harmonised into. All of them
// except UNISON and HARMONIC repeat in every octave, which is what keeps a
// mapped note in its own register instead of collapsing the arrangement onto a
// single pitch.
//   UNISON   -- the CPS frequency itself and nothing else: every note becomes
//               the same note, the drone/monophoniser setting.
//   HARMONIC -- integer multiples of CPS, not octave-periodic: the material is
//               forced onto one harmonic series, which is the setting that
//               makes unpitched sources pitched.
// The rest are ordinary pitch-class sets rooted on CPS. pitchmap.cpp decodes
// this list positionally.
#define PITCHMAP_SCALES { "UNISON", "OCTAVES", "FIFTHS", "MAJ TRIAD", "MIN TRIAD", "MAJOR", "MINOR", "MIN PENT", "WHOLE TONE", "CHROMATIC", "HARMONIC" }
inline constexpr std::string_view pitchmap_scales[] = PITCHMAP_SCALES;

// SPECTRAL DELAY (GENCREVERB) algorithms. Order IS the SPECDELMODE value and
// must match SpectralDelay's enum in Convolver.h (static_asserts there) and
// specdel_subspaces below. Ids 4.. repurpose the dead experimental-generator
// ids, which were never selectable in the UI.
inline constexpr std::string_view specdelmodes[] = { "RAMP UP", "RAMP DOWN",
        "REVERB1", "REVERB2", "FEEDBACK", "STEPPED", "RIPPLE", "OCTAVE",
        "DRIFT", "BARBERPOLE", "TIDE", "ADAPTIVE", "LONGVERB", "PUREVERB" };

// LONGVERB and PUREVERB are parked, not production-ready. They stay at the
// end of the list so the enum and the static_asserts keep holding, but the
// SPECDELMODE parameter range and the UI selector stop short of them: an
// enum parameter's range comes from names.size(), so capping the span here
// hides them from host automation too.
inline constexpr int specdelmodes_shown = (int)std::size(specdelmodes) - 2;

// Sub-space view shown under the algorithm selector in the second SPECTRAL
// DELAY space, per algorithm. RAMP UP and RAMP DOWN share the SHAPE view, so
// this is a map, not a base offset.
inline constexpr const int specdel_subspaces[] = {
        SPACE_SPECDELSUB_RAMP, SPACE_SPECDELSUB_RAMP, SPACE_SPECDELSUB_RAND,
        SPACE_SPECDELSUB_GAUSS, SPACE_SPECDELSUB_FB, SPACE_SPECDELSUB_STEP,
        SPACE_SPECDELSUB_RIPPLE, SPACE_SPECDELSUB_OCT, SPACE_SPECDELSUB_DRIFT,
        SPACE_SPECDELSUB_BARBER, SPACE_SPECDELSUB_TIDE, SPACE_SPECDELSUB_ADAPT,
        SPACE_SPECDELSUB_VERB, SPACE_SPECDELSUB_PUREVERB };
static_assert(std::size(specdel_subspaces) == std::size(specdelmodes),
        "every SPECTRAL DELAY algorithm needs a sub-space view");


#define VCF_TYPES {"VCF", "RESLP", "RESON", "VCF2", "DLAD"/*, "DLAD2", "VCF3", "ImpMoog", "KMoog", "MTMoog", "MDSPMoog", "RKMoog", "SimplMoog"*/}
inline constexpr std::string_view filtertypes[] = VCF_TYPES;


inline constexpr std::string_view tracknames[] = { "TRACK1", "TRACK2", "TRACK3", "TRACK4" };

inline constexpr std::string_view mdelaynames[] = { "DELAY1", "DELAY2", "DELAY3", "DELAY4", "DELAY5", "DELAY6", "DELAY7", "DELAY8" };
#define NOISE_TYPES {"WHITE", "PINK", "BROWN"}

#define GRAINGEN_MODES { "RND1", "RND2", "BOUNCE", "SPLINE", "FOLLOW", "CHIMES", "FIGURE", "GLISS" }
inline constexpr std::string_view graingen_types[] = GRAINGEN_MODES;
inline constexpr std::string_view graingen_figure_shapes[] = { "ANY", "RUN", "ARP", "ZIGZAG", "CASCADE" };
inline constexpr std::string_view graingen_gliss_modes[] = { "WANDER", "UP", "DOWN", "ARCH", "FALL", "RISSET UP", "RISSET DN" };

inline constexpr std::string_view editorcurvenames[] = { "POLYNOM", "LINE", "RECT" };

inline constexpr std::string_view editfuncs[] = {
        "SINE LFO",
        "TRIANGLE",
        "PULSE"
};

inline constexpr std::string_view oscnames[]{
        "KEEP",
        "RUNNING",
        "RANDOM",
        "ZERO"
};

// PVAMPS slot routing. CLASSIC hands peak i to slot i in ascending frequency
// order with no identity between hops, so the frequency smoother slurs across
// whatever lands in a slot -- that smear is the original character. TRACKED
// gives each slot the partial it was already following.
inline constexpr std::string_view pvampsroutes[] = { "CLASSIC", "TRACKED" };

inline constexpr std::string_view noise_types[] = { "WHITE", "PINK", "BROWN" };
inline constexpr std::string_view dyns[] = { "LOW", "PEAK1", "PEAK2", "PEAK3", "HIGH" };
inline constexpr std::string_view dyns2[] = { "BELOW", "ABOVE" };
inline constexpr std::string_view mods[] = { "UPPER", "LOWER", "DUAL" };
inline constexpr std::string_view comps[] = { "BAND1", "BAND2", "BAND3" };
inline constexpr std::string_view lforandcurve[] = { "POLY", "LIN", "RECT"/*, "SPLINE"*/ };

inline constexpr std::string_view lforanddistr[] = { "NONE", "BETA", "X+=R", "X+=V+=R",
                                                    "X+=V+=A+=R" };
inline constexpr const char* noSoundLoaded = "No sounddata present";
inline constexpr const char* presetHasAudio = "presethasaudio";

// DISTORTION DRIVE. The knob is a plain 0..1 (preset version 22 on) and maps
// onto the gain k of the tanh shaper in DISTORT::compute:
//   k = kDistDriveMin * (kDistDriveMax / kDistDriveMin)^(knob^kDistDriveCurve)
//
// The exponent is what makes the knob usable. tickBalance holds the output
// level equal to the dry, so the only thing DRIVE changes is harmonic content -
// and THD against log(k) is a sigmoid that tops out at 48.3% (an ideal square).
// A straight exponential map therefore has a dead bottom and a flat top: on the
// first 0..1 revision, a -18 dBFS input reached 1% THD only at knob 0.32 and
// 30% at 0.78, so three quarters of the travel did nothing and the last quarter
// did everything. kDistDriveCurve < 1 pushes the knob through the inaudible
// low-k region quickly and spends the travel where the sound changes; these
// three values are a least-squares fit of THD to a straight line across -12,
// -18 and -24 dBFS inputs, subject to knob 0 staying clean on a hot signal.
inline constexpr double kDistDriveMin = 0.5;
inline constexpr double kDistDriveMax = 180.;
inline constexpr double kDistDriveCurve = 0.6;
// Knob position reproducing the old 0 dB dB-parameter default, i.e. k = 6:
// (log(6 / kDistDriveMin) / log(kDistDriveMax / kDistDriveMin))^(1/kDistDriveCurve).
inline constexpr double kDistDriveInit = 0.238;