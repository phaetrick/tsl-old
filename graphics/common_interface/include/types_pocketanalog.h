#pragma once
//
// Created by pr on 23.11.17.
//

#ifndef GRAINSTORM_TYPES_H
#define GRAINSTORM_TYPES_H

#define PA_ENABLE_PAD 1

enum paramternumber {
    PARAM_NOT_ASSIGNED,
    POWERButton,
    POWERTRACK,
    SETTINGSBUTTON,
    POSTGAIN,
    GUISPACE2,
    EGSPACE,
    VCO1GAIN,
    VCO1EG,
    VCO1TUNEEG,
    VCO1TYPE,
    VCO1FINE,
    VCO1COARSE,
    VCO1PW,
    VCO2GAIN,
    VCO2EG,
    VCO2TUNEEG,
    VCO2TYPE,
    VCO2FINE,
    VCO2COARSE,
    VCO2PW,
    VCO3GAIN,
    VCO3EG,
    VCO3TUNEEG,
    VCO3TYPE,
    VCO3FINE,
    VCO3COARSE,
    VCO3PW,
    VCO3RINGMOD,
    VCO3NORMAL,
    EG1ATTACK,
    EG1DECAY,
    EG1SUSTAIN,
    EG1RELEASE,
    EG2ATTACK,
    EG2DECAY,
    EG2SUSTAIN,
    EG2RELEASE,
    EG3ATTACK,
    EG3DECAY,
    EG3SUSTAIN,
    EG3RELEASE,
    EG4ATTACK,
    EG4DECAY,
    EG4SUSTAIN,
    EG4RELEASE,
    EG5ATTACK,
    EG5DECAY,
    EG5SUSTAIN,
    EG5RELEASE,
    FILTGAIN,
    FILTEG,
    FILTCENTER,
    FILTRES,
    NOISEMODE,
    NOISEGAIN,
    NOISEEG,
    VCO1SYNC,
    VCO2SYNC,
    VCO3SYNC,
    LOADPRESETBUTTON,
    SAVEPRESETBUTTON1,
    FXSPACE1,
    REVPOW,
    REV4LPCUT,
    REV4HPCUT,
    REV3MIX,
    REV3HPCUT,
    REV3LPCUT,
    REV3PREDELAY,
    REV4GAIN,
    REV4MIX,
    REV4REF,
    REV3DAMP,
    REV3REF,
    REV3GAIN,
    REV4PREDELAY,
    CDELPOW,
    CDELDEL,
    CDELFB,
    CDELMODE,
    CDELMIX,
    CDDELMODRATE,
    CDDELMODDEPTH,
    JITTERCENTS,
    JITTERA,
    JITTERB,
    SEQ_BPM,
    SEQ_MODE,
    SEQ_SWING,
    SEQ_STEPS,
    ARP_STEPS,
    ARP_MODE,
    ARPSTEP01,
    ARPSTEP02,
    ARPSTEP03,
    ARPSTEP04,
    ARPSTEP05,
    ARPSTEP06,
    ARPSTEP07,
    ARPSTEP08,
    ARPSTEP09,
    ARPSTEP10,
    ARPSTEP11,
    ARPSTEP12,
    ARPSTEP13,
    ARPSTEP14,
    ARPSTEP15,
    ARPSTEP16,
    ARPSTEP17,
    ARPSTEP18,
    ARPSTEP19,
    ARPSTEP20,
    ARPSTEP21,
    ARPSTEP22,
    ARPSTEP23,
    ARPSTEP24,
    ARPSTEP25,
    ARPSTEP26,
    ARPSTEP27,
    ARPSTEP28,
    ARPSTEP29,
    ARPSTEP30,
    ARPSTEP31,
    ARPSTEP32,
    SEQ_GAIN,
    PHASERPOW,
    PHASERRANGE,
    PHASERFB,
    PHASERRATE,
    NUM_PARAMS1,
    ZERONOTE,
    LEARNMODE,
    NONOTES,
    NOTESETTINGSFROMSYNTH,
    CURRENTNOTE,
    ARP_STEP,
    CLEARTASKS,
    SAVEPRESETBUTTON,
    FXSPACE,
    WAITFORZERO,
    GUISPACE,
    SEQLEARNING,
    ARPRESET,
    SYNTHRESET,
    ARPSPACE,
    RECORDButton,
    MIDILEARNBUTTON,
    SEQ_SYNCDAW,
    SEQ_TIMEDIV,
    PRESETSELECT,
    VEL_TO_FILT,
    VEL_TO_AMP,
    AT_TO_FILT,
    AT_TO_AMP,
    MW_TO_FILT,
    MW_TO_VIBRATO,
    FILT_CUT,
    AT_TO_VIBRATO,
    KEYTRACK_TO_FILT,
    ROUTESPACE,
    VCO1SPACE,
    VCO2SPACE,
    VCO3SPACE,
    VCO1PWMODSRC,
    VCO2PWMODSRC,
    VCO3PWMODSRC,
    VCO1PWMODDEPTH,
    VCO2PWMODDEPTH,
    VCO3PWMODDEPTH,
    VCO3COARSEST,
    LFO1RATE,
    LFO1DEPTH,
    LFO1WAVE,
    LFO1DEST,
    LFO2RATE,
    LFO2DEPTH,
    LFO2WAVE,
    LFO2DEST,
    LFOSPACE,
    VEL_TO_RES,
    AT_TO_RES,
    MW_TO_RES,
    RESEG,
    MW_TO_LFORATE,
    AT_TO_LFORATE,
    MW_TO_LFODEPTH,
    AT_TO_LFODEPTH,
    FILT_MODE,
    VCO3AM,
    VCO3AMDEPTH,
    VCO3PM,
    VCO3PMDEPTH,
    VCO1WTPOS,
    VCO2WTPOS,
    VCO3WTPOS,
    VCO1WTSEL,
    VCO2WTSEL,
    VCO3WTSEL,
    VCO1UNIVOICES,
    VCO1UNIDETUNE,
    VCO1UNIBLEND,
    VCO2UNIVOICES,
    VCO2UNIDETUNE,
    VCO2UNIBLEND,
    VCO3UNIVOICES,
    VCO3UNIDETUNE,
    VCO3UNIBLEND,
    VCO1WARPTYPE,
    VCO1WARPAMT,
    VCO2WARPTYPE,
    VCO2WARPAMT,
    VCO3WARPTYPE,
    VCO3WARPAMT,
    VCO1WARPEG,
    VCO2WARPEG,
    VCO3WARPEG,
    AT_TO_WARP,
    AT_TO_MORPH,
    MW_TO_MORPH,
    MW_TO_WARP,
    VCO1WARPTO,   // warp modulation target: modulation moves WARP AMT from AMT(=A) toward TO(=B); B<A inverts
    VCO2WARPTO,
    VCO3WARPTO,
    VCO1MORPHTO,  // morph modulation target: modulation moves MORPH from WTPOS(=A) toward TO(=B); B<A inverts
    VCO2MORPHTO,
    VCO3MORPHTO,
    AT_TO_UNI,    // aftertouch / mod wheel widen the unison detune of all three oscs
    MW_TO_UNI,
    // Modal resonator bank (osc type 97). Unlike PAD these are all runtime — the
    // bank is 14 complex rotations, there is nothing to build and nothing to cache,
    // so every one of them can be turned while a note rings. They sit BEFORE
    // NUM_PARAMS deliberately: the block ships live and gets #if'd out later if it
    // has to be parked, which is the opposite of PAD's situation.
    VCO1MODALCH,  VCO2MODALCH,  VCO3MODALCH,    // character: DEEP..GLASS
    VCO1MODALDEC, VCO2MODALDEC, VCO3MODALDEC,   // fundamental T60, seconds
    VCO1MODALBRT, VCO2MODALBRT, VCO3MODALBRT,   // partial amplitude tilt
    VCO1MODALHRD, VCO2MODALHRD, VCO3MODALHRD,   // mallet hardness (velocity adds to this)
    VCO1MODALPOS, VCO2MODALPOS, VCO3MODALPOS,   // strike position, 0..0.5
    // Modal bank as a FILTER (mode 17). Only two ids, because CUT and RESO already
    // carry the bank's pitch and decay — with FILTEG / VEL / AT / MW / LFO routing
    // attached to them for free.
    FILTMODALBODY,   // character: DEEP..GLASS
    FILTMODALPOS,    // strike position — sets which modes the input can drive
    // Decay routing for the STRUCK oscillator, one route covering all three oscs
    // (the AT_TO_UNI idiom). The filter path deliberately gets none of these: its
    // decay IS RESO, which already carries EG / VEL / AT / MW / LFO.
    //
    // T60 is also the ONLY modal parameter that can move mid-ring. r appears solely
    // in the next rotation step, so rewriting it lengthens or shortens the tail from
    // that sample forward without touching the state vector's magnitude. BRIGHT,
    // MALLET and STRIKE are initial conditions that excite() has already consumed,
    // so modulating them on a struck bank is a no-op until the next note-on — which
    // is why they get no routes rather than routes that quietly do nothing.
    KEYTRACK_TO_DECAY,  // high notes die faster, as every struck instrument does
    AT_TO_DECAY,        // hand damping: pressing SHORTENS the tail
    MW_TO_DECAY,
    // PADsynth (osc type 98) build-time params — changing any of these rebuilds the
    // oscillator's table set off-thread. The runtime MORPH position is NOT here: it
    // reuses VCOxWTPOS / VCOxMORPHTO and their existing EG/LFO/AT/MW routing.
    // With PA_ENABLE_PAD 0 these live PAST NUM_PARAMS and must never be read from
    // the params / parameters / synth_params arrays — they are outside them.
#if !PA_ENABLE_PAD
    NUM_PARAMS,
#endif
    VCO1PADSEL,   VCO2PADSEL,   VCO3PADSEL,     // curated PADsynth table
    VCO1PADBW,    VCO2PADBW,    VCO3PADBW,      // bandwidth, cents
    VCO1PADBWSC,  VCO2PADBWSC,  VCO3PADBWSC,    // bandwidth growth vs harmonic index
    VCO1PADSTR,   VCO2PADSTR,   VCO3PADSTR,     // inharmonic stretch
    VCO1PADSEED,  VCO2PADSEED,  VCO3PADSEED,    // phase seed (re-roll the ensemble)
    // PAD's OWN morph controls. These used to be VCOxWTPOS / VCOxMORPHTO /
    // VCOxPWMODSRC, shared with the wavetable oscillator so PAD inherited its
    // EG/LFO/AT/MW routing for free. Sharing meant one set of morph settings for two
    // very different oscillators on the same slot: switching an osc between WT and PAD
    // carried the morph range across, and a patch could not hold a sensible morph for
    // each. Split. The AT/MW/LFO ROUTE DEPTHS stay shared — those are global "how far
    // does aftertouch move morph" controls, not per-oscillator-type settings.
    VCO1PADPOS,   VCO2PADPOS,   VCO3PADPOS,     // MORPH A  (was VCOxWTPOS)
    VCO1PADMTO,   VCO2PADMTO,   VCO3PADMTO,     // MORPH B  (was VCOxMORPHTO)
    VCO1PADMEG,   VCO2PADMEG,   VCO3PADMEG,     // morph EG source (was VCOxPWMODSRC)
    // LFO start phase, degrees. Appended at the very END because ids are positions:
    // putting them anywhere earlier would shift the PAD block and silently repoint every
    // user preset that stores one. They therefore inherit the PAD block's caveat above —
    // with PA_ENABLE_PAD 0 these fall past NUM_PARAMS and would have to be #if'd out too.
    LFO1PHASE,    LFO2PHASE,
#if PA_ENABLE_PAD
    NUM_PARAMS
#endif
};
static constexpr int NUM_PARAMETERS = NUM_PARAMS;

#endif //GRAINSTORM_TYPES_H
