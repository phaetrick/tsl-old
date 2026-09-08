#pragma once
//
// PITCHMAP -- polyphonic pitch mapping, mono FX.
//
// Finds the pitched notes in whatever is playing, decides a target pitch for
// each one from a scale rooted on CPS, and moves each note -- all of its
// partials together, by one ratio -- to that target. A chord played into it
// comes out as a different chord; a whole mix comes out in a different key.
// Unpitched content (drums, breath, consonants) is left where it is unless
// PURIFY is turned up to remove it.
//
// This is deliberately a mono FX and not a PV algorithm. The PV path analyses
// grains, which arrive from jittered read positions and are therefore not
// consecutive frames: it can neither track a note across frames nor keep a
// shifted partial phase-coherent from one frame to the next, and both of those
// are what separate "pitch mapping" from "spectral snapping". Here the STFT is
// fixed -- 4096 samples, hop 1024, frames strictly consecutive -- so a bin's
// instantaneous frequency comes from the phase difference between frames (far
// more accurate than interpolating the magnitude peak), the detected notes can
// be tracked and hysteresis-locked over time, and the output phase can be
// integrated per bin. The price is 4096 samples of latency, compensated on the
// dry path by `delay`.
//
// SPECTRAL SNAP in the PV list is the same idea one level down: it snaps every
// peak to a grid *independently*, which breaks the harmonic ratios inside a
// note and is why it rings like a bell rather than transposing. The grouping
// step here is the whole difference.
//
// CPS + SCALE are the target set. MIDI-driven target chords are the intended
// next step: everything downstream of buildTargets() only asks "nearest allowed
// frequency", so held notes can replace the knob without touching the engine.
//

#ifndef GRAINSTORM_PITCHMAP_H
#define GRAINSTORM_PITCHMAP_H

#include <atomic>
#include <cstdint>
#include <tools/CircularBuffer.h>
#include "defines.h"
#include "base.h"
#include "DelayBase.h"
#include "ffttools.h"

// 4096 at 48 kHz is an 11.7 Hz bin and a 21 ms hop: fine enough to separate the
// partials of two voices a tone apart, short enough that the note tracker still
// follows a melody. Fixed rather than a parameter because the buffers and the
// FFT plan are sized in the constructor -- a size knob would mean tearing the
// effect down and rebuilding it through the fxpower factory.
#define PitchMapFFTSize 4096
#define PitchMapOverlap 4
#define PitchMapHop     (PitchMapFFTSize / PitchMapOverlap)
#define PitchMapNyq     (PitchMapFFTSize / 2)
// Harmonic-number lookup tables live at file scope in the .cpp, so their size
// cannot come from the class constant it has to agree with. The static_assert
// there ties the two together.
#define PitchMapMaxHarmTbl 17
// Notes mapped at once. ParameterInit uses it as the SOURCES maximum.
#define PitchMapMaxSources 6

class PitchMap : public Effect, private tsl::CircularBuffer<MYFLOAT> {
public:
    PitchMap(TRACK *t, int32_t chan);
    ~PitchMap() override;

    void compute(MYFLOAT *in, int32_t size) override;

protected:
    void onBufferReady(MYFLOAT *buf, int32_t size) override;

private:
    static constexpr int MAX_SOURCES = PitchMapMaxSources;
    // Spectral peaks kept per frame. The picker scans in ASCENDING bin order,
    // so when the budget runs out everything above the last peak goes
    // unanalysed -- at 200 a dense mix hit the ceiling mid-spectrum and a
    // claimed note's harmonics above it stayed at the original pitch. The
    // 5-point picker with its 3-bin stride cannot return more than ~680
    // maxima over 2046 bins, and the -60 dB gate keeps real frames well under
    // 512. If it ever does truncate, the loss is now benign: unanalysed bins
    // fall into the last region's non-moving part (PITCHMAP_SHIFT_W) and pass
    // through unmapped instead of being dragged by the topmost peak.
    static constexpr int MAX_PEAKS = 512;
    static constexpr int MAX_HARM = 16;     // harmonics the salience search scatters over
    // Harmonics a KNOWN note may claim. MAX_HARM bounds the cost of the search
    // and the octave ambiguity inside it; it has no business limiting a note
    // whose f0 is already established. At 16, a 233 Hz note stops owning its
    // partials at 3.7 kHz and everything above stays at the original pitch while
    // the rest of the note moves -- one note coming out as two pitches, which is
    // most of what "phase vocoder artefacts" means here. Measured: 89.6% of a
    // 24-harmonic note's peak mass claimed at 16, 100% at 40.
    static constexpr int CLAIM_HARM = 40;
    // ...but 40 harmonics is also 40x the fundamental of territory, and in a
    // mix that is everything the OTHER notes are made of. The measurement above
    // is a SOLO one: it says how much of a lone note's mass a wide net catches,
    // not what the net costs when something else is playing. So the width is
    // the solo figure only while one note is live and tapers as notes are
    // added -- see claimWidth().
    static constexpr int MIN_CLAIM_HARM = 12;
    static constexpr int CELLS_PER_OCT = 24;// f0 search grid: half a semitone
    static constexpr int MAX_CAND = 320;
    static constexpr int MAX_TARGETS = 192;
    // Peak-to-harmonic match window, as |f / (h*f0) - 1|. 35 cents: wide enough
    // for vibrato and for the inharmonicity of a real string, narrow enough that
    // a neighbouring semitone cannot be claimed by the wrong note.
    static constexpr MYFLOAT TOL_RATIO = .0204;
    // Only the low harmonics vote on a note's frequency, and the vote can only
    // move it this far per frame (40 cents). See the runaway note in the .cpp.
    static constexpr int VOTE_HARM = 8;
    static constexpr MYFLOAT MAX_STEP_OCT = .0333;

    void buildCandidates(MYFLOAT lo, MYFLOAT hi);
    void buildTargets(MYFLOAT cps, int scale);
    void arbitrateClaims();
    void scoreClaim(int32_t trk);
    MYFLOAT peakFree(int32_t p) const;
    void gatherClaim(MYFLOAT f0);
    void commitClaim(int32_t trk);
    void updateTracks(int wanted, MYFLOAT totalMass, bool transient);
    MYFLOAT nearestTarget(MYFLOAT f) const;

    FFT fft;
    delay<MYFLOAT> delay;

    MYFLOAT _win[PitchMapFFTSize]{};        // Hann, used on both ends of the frame
    MYFLOAT _spec[PitchMapFFTSize + 2]{};   // {DC, Nyq, re1, im1, ...}
    MYFLOAT _prevPhi[PitchMapNyq + 1]{};    // last frame's phase, for the freq estimate
    MYFLOAT _sumPhi[PitchMapNyq + 1]{};     // integrated output phase
    // Which frame last integrated _sumPhi at this bin. An accumulator is only
    // continuous if it (or an immediate neighbour -- a partial wobbling across a
    // bin boundary changes the peak bin) ran last frame; otherwise it is stale
    // garbage and the region would be rotated by it. See the seeding note in
    // the synthesis pass.
    int32_t _phiFrame[PitchMapNyq + 1]{};
    // Last frame's final claim map, per bin: which track owned the peak there
    // and at what harmonic number. Consulted by the continuity rescue in
    // arbitrateClaims; -1 = unowned (set in the constructor and rebuilt at the
    // end of every updateTracks).
    int8_t _prevOwn[PitchMapNyq + 1]{};
    int8_t _prevHrm[PitchMapNyq + 1]{};
    MYFLOAT _prevMag[PitchMapNyq + 1]{};    // last frame's magnitude, for the flux
    MYFLOAT _inc[PitchMapNyq + 1]{};        // this frame's onset increment
    MYFLOAT _mag[PitchMapNyq + 1]{};
    MYFLOAT _frq[PitchMapNyq + 1]{};        // instantaneous frequency, Hz
    MYFLOAT _logm[PitchMapNyq + 1]{};       // log magnitude (FORMANT scratch)
    MYFLOAT _env[PitchMapNyq + 1]{};        // log spectral envelope (FORMANT)
    MYFLOAT _outMag[PitchMapNyq + 1]{};     // magnitude already written per
                                            // output bin; collision arbitration
                                            // in the synthesis pass

    int32_t _peakBin[MAX_PEAKS]{};
    MYFLOAT _peakFrq[MAX_PEAKS]{};
    MYFLOAT _peakMag[MAX_PEAKS]{};
    MYFLOAT _peakLog2[MAX_PEAKS]{};
    MYFLOAT _peakSin[MAX_PEAKS]{};          // sinusoidality; see the peak pass
    int32_t _peakSrc[MAX_PEAKS]{};          // owning note track, or -1
    int32_t _peakHrm[MAX_PEAKS]{};          // harmonic number within that track
    int32_t _peakGhost[MAX_PEAKS]{};        // synthesis-only bridge owner for a
    int32_t _peakGhostHrm[MAX_PEAKS]{};     // one-frame claim dropout, or -1
    int32_t _npeaks{};

    // Scratch for one claim attempt (gatherClaim -> commitClaim).
    int32_t _claimIdx[MAX_PEAKS]{};
    int32_t _claimHrm[MAX_PEAKS]{};
    int32_t _claimN{};
    MYFLOAT _claimF0{}, _claimMass{};
    MYFLOAT _claimB{-1.};                   // stretch fit from this claim, <0 = none
    bool _claimRoot{};
    int32_t _claimHarm{CLAIM_HARM};         // this frame's claim width

    MYFLOAT _candF0[MAX_CAND]{};
    MYFLOAT _sal[MAX_CAND]{};
    MYFLOAT _hits[MAX_CAND]{};
    int32_t _ncand{};
    MYFLOAT _candLo{-1.}, _candHi{-1.};
    MYFLOAT _log2lo{};

    MYFLOAT _targets[MAX_TARGETS]{};        // allowed output frequencies, ascending
    int32_t _ntargets{};
    MYFLOAT _targetCps{-1.};
    int32_t _targetScale{-1};

    // Note tracks. A detection is matched to one of these so that the mapping
    // decision survives across frames -- see the hysteresis note in the .cpp.
    MYFLOAT _trkF0[MAX_SOURCES]{};          // measured, drives the ratio
    MYFLOAT _trkF0m1[MAX_SOURCES]{};        // f0 one and two frames back; the
    MYFLOAT _trkF0m2[MAX_SOURCES]{};        // render pitch reads their median-3
    MYFLOAT _trkB[MAX_SOURCES]{};           // inharmonicity (stretch) coefficient
    MYFLOAT _trkDec[MAX_SOURCES]{};         // smoothed, drives the target choice
    MYFLOAT _trkRatio[MAX_SOURCES]{};
    MYFLOAT _trkTarget[MAX_SOURCES]{};
    int32_t _trkAge[MAX_SOURCES]{};
    int32_t _trkDied[MAX_SOURCES]{};        // frame the slot's occupant retired
    bool _trkLive[MAX_SOURCES]{};

    int32_t _lfoIdx{};                      // sample index the frame closed on
    int32_t _warm{};                        // frames since reset; see onBufferReady
    int32_t _frameNo{};                     // frame counter, for _phiFrame

    // Channel link: channel 0 leads, the others adopt its ratios so a stereo
    // mix gets ONE mapping instead of two disagreeing ones. See the registry
    // in the .cpp.
    bool _linkLeader{};
    int32_t _linkSlot{-1};

    std::atomic<MYFLOAT> *_cps{}, *_scale{}, *_amt{}, *_sources{}, *_purify{},
            *_formant{}, *_glide{}, *_lo{}, *_hi{}, *_dry{}, *_wet{};
    std::atomic<LFO *> *_lfo_cps{}, *_lfo_amt{};
};

#endif //GRAINSTORM_PITCHMAP_H
