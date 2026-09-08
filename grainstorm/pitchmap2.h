#pragma once
//
// PITCHMAP2 -- the multi-resolution build of PITCHMAP, as a separate effect so
// the two can be A/B'd on the same material with the same knobs. It reads the
// SAME PMAP* parameters; only the space id (and so bypass/fx slot) differs.
//
// PITCHMAP renders everything from one 4096/1024 STFT: 11.7 Hz bins, which the
// low end needs, paid for with 85 ms windows, which the high end suffers --
// every shifted harmonic up top carries 85 ms of smear, and that smear is a
// large part of what reads as "PV artefact" on moving notes and re-attacks.
//
// Here analysis-for-DECISIONS and synthesis are split. All decisions -- peaks,
// tracking, arbitration, targets, ratios, identity/lock, channel link -- run
// exactly as in PITCHMAP, on the 4096 layer, so the two effects make identical
// choices and an A/B compares synthesis alone. Synthesis is three parallel
// full-band STFTs, each rendering one spectral share of the SAME input:
//
//   L0  4096/1024   below ~1.5 kHz   85 ms window, 11.7 Hz bins
//   L1  2048/512    ~1.5..4 kHz      43 ms window, 23.4 Hz bins
//   L2  1024/256    above ~4 kHz     21 ms window, 46.9 Hz bins
//
// Parallel full-band STFTs, NOT a filter band-split: crossover filters would
// hand each layer a partial-amplitude copy of every seam partial plus filter
// phase, and a note spans all bands anyway so the note model has to be global
// regardless. The shares are complementary raised-cosine masks applied to the
// SOURCE magnitudes at synthesis; they sum to exactly 1, so unshifted content
// -- residual, identity-path notes, the warm-up frames -- reconstructs
// bit-exactly by construction, whatever mix of paths the three layers take.
//
// What keeps the layers from becoming their own artefact (the reason this is
// a careful design and not just three PitchMaps):
//
//   1. One brain. Sub-layers create nothing and decide nothing: they arbitrate
//      their own peaks against a per-L0-frame SNAPSHOT of the track state and
//      render with the snapshot's ratios. No second opinion to disagree with.
//   2. Aligned latencies. Each layer's OLA latency is its FFT size; L1/L2 are
//      delayed by 2048/3072 so all three sit at exactly 4096 -- the same
//      reported latency as PITCHMAP, and the identity sum stays sample-exact.
//   3. Harmonic-locked coherence. A seam partial is rendered by TWO layers at
//      once (its mask shares). Both render it at the SAME frequency -- hn *
//      renderF0 * ratio from the same snapshot -- so once seeded, both copies
//      advance identically and their relative phase is CONSTANT, not beating.
//   4. Common-frame seeding. That constant is forced to ~zero: a sub-layer may
//      SEED a phase accumulator only on a frame that closed on the same sample
//      as the L0 frame (hops nest: 256 and 512 divide 1024). Both copies then
//      seed from their own input phase at the same instant -- two readings of
//      the same waveform -- and stay in phase for the life of the note. On the
//      frames between, an unseeded region renders unshifted for one subframe
//      (the attack passing intact, which is the house rule at onsets anyway).
//
// The honest trade, measured before building (see the layer table): a layer
// only resolves partials spaced wider than its ~4-bin mainlobe, so notes with
// f0 below ~94 Hz have unresolvable harmonics above the L1 seam, and notes
// below ~188 Hz above the L2 seam. Those partials fail sinusoidality, stay
// unclaimed, and do not move with their note. The seams are placed so that is
// rare: male vocals (f0 110+) stay resolved through 4 kHz, and what a 60-90 Hz
// bass carries above 1.5 kHz is weak; above 4 kHz low-f0 energy is sibilance
// and cymbals, which residual treatment is CORRECT for. Deep-bass-heavy dense
// material is where PITCHMAP should win the A/B; moving notes and re-attacks
// up top are where this should.
//

#ifndef GRAINSTORM_PITCHMAP2_H
#define GRAINSTORM_PITCHMAP2_H

#include "gs_common.h"      // GS_ENABLE_PITCHMAP2

#if GS_ENABLE_PITCHMAP2

#include <atomic>
#include <vector>
#include <tools/CircularBuffer.h>
#include "defines.h"
#include "base.h"
#include "DelayBase.h"
#include "ffttools.h"
#include "pitchmap.h"   // PitchMapMaxSources / PitchMapMaxHarmTbl -- the shared
                        // PMAPSOURCES maximum and harmonic-table size

#define PM2FFT0    4096
#define PM2OVERLAP 4
#define PM2HOP0    (PM2FFT0 / PM2OVERLAP)
#define PM2NYQ0    (PM2FFT0 / 2)

class PitchMap2 : public Effect, private tsl::CircularBuffer<MYFLOAT> {
public:
    PitchMap2(TRACK *t, int32_t chan);
    ~PitchMap2() override;

    void compute(MYFLOAT *in, int32_t size) override;

protected:
    void onBufferReady(MYFLOAT *buf, int32_t size) override;

private:
    static constexpr int MAX_SOURCES = PitchMapMaxSources;
    // Same peak budget as PITCHMAP; the sub-layers have fewer bins and cannot
    // come close to it.
    static constexpr int MAX_PEAKS = 512;
    static constexpr int MAX_HARM = 16;
    static constexpr int CLAIM_HARM = 40;
    static constexpr int MIN_CLAIM_HARM = 12;
    static constexpr int CELLS_PER_OCT = 24;
    static constexpr int MAX_CAND = 320;
    static constexpr int MAX_TARGETS = 192;
    static constexpr MYFLOAT TOL_RATIO = .0204;
    static constexpr int VOTE_HARM = 8;
    static constexpr MYFLOAT MAX_STEP_OCT = .0333;

    // One synthesis layer below the decision layer: its own STFT state, its
    // own accumulators, its own mask share. Decisions come in via _snap.
    struct Sub {
        const int32_t fftSize, hop, nyq;
        FFT fft;
        std::vector<MYFLOAT> win, spec, prevPhi, sumPhi, mag, frq, logm, env, mask;
        std::vector<MYFLOAT> outMag;    // collision arbitration, per frame
        std::vector<MYFLOAT> prevMag, inc;  // transient split, per layer
        std::vector<int32_t> phiFrame;
        // Last subframe's final claim map, for the incumbency/ghost logic --
        // same per-bin scheme as the L0 layer's _prevOwn/_prevHrm.
        std::vector<int8_t> prevOwn, prevHrm;
        int32_t peakBin[MAX_PEAKS]{};
        MYFLOAT peakFrq[MAX_PEAKS]{};
        MYFLOAT peakMag[MAX_PEAKS]{};
        MYFLOAT peakLog2[MAX_PEAKS]{};
        MYFLOAT peakSin[MAX_PEAKS]{};
        int32_t peakSrc[MAX_PEAKS]{};
        int32_t peakHrm[MAX_PEAKS]{};
        int32_t peakGhost[MAX_PEAKS]{};
        int32_t peakGhostHrm[MAX_PEAKS]{};
        int32_t npeaks{};
        int32_t frameNo{};
        int32_t warm{};
        // Which L0 frame this layer last saw -- a subframe that observes a NEW
        // L0 frame number closed on the same sample as that L0 frame (L0 ticks
        // first within the sample), and only such "common" subframes may seed.
        int32_t lastL0{-1};
        Sub(int32_t n, int32_t h);
    };

    // Forwards the circular-buffer callback of one sub-layer to subFrame().
    class SubBuf : private tsl::CircularBuffer<MYFLOAT> {
    public:
        SubBuf(PitchMap2 *o, Sub *s)
                : CircularBuffer(s->fftSize, (float) s->fftSize / (float) s->hop),
                  _o(o), _s(s) {}
        inline MYFLOAT tick(MYFLOAT in) { return _tick(in); }
    protected:
        void onBufferReady(MYFLOAT *buf, int size) override { _o->subFrame(*_s, buf); }
    private:
        PitchMap2 *_o;
        Sub *_s;
    };

    // The per-L0-frame decision snapshot the sub-layers render from. Written
    // once per L0 frame after the ratios/ident/lock are settled, read by the
    // sub-layer frames until the next one; all on the one audio thread.
    struct RenderSnap {
        bool live[MAX_SOURCES]{};
        MYFLOAT f0[MAX_SOURCES]{};        // measured -- drives arbitration
        MYFLOAT renderF0[MAX_SOURCES]{};  // link-adopted -- drives the slots
        MYFLOAT B[MAX_SOURCES]{};         // inharmonicity (stretch)
        MYFLOAT ratio[MAX_SOURCES]{};
        bool ident[MAX_SOURCES]{};
        MYFLOAT lock[MAX_SOURCES]{};
        int32_t claimHarm{CLAIM_HARM};
        MYFLOAT resGain{1.}, fa{0.};
        bool transient{};               // L0 flux verdict, for the split
    };

    void buildCandidates(MYFLOAT lo, MYFLOAT hi);
    void buildTargets(MYFLOAT cps, int scale);
    void arbitrateClaims();
    void scoreClaim(int32_t trk);
    MYFLOAT peakFree(int32_t p) const;
    void gatherClaim(MYFLOAT f0);
    void commitClaim(int32_t trk);
    void updateTracks(int wanted, MYFLOAT totalMass, bool transient);
    MYFLOAT nearestTarget(MYFLOAT f) const;
    void buildMasks(MYFLOAT sr);
    void subFrame(Sub &S, MYFLOAT *buf);

    FFT fft;
    delay<MYFLOAT> _dly1, _dly2;    // sub-layer alignment to 4096 total
    // Declared AFTER _dly1/_dly2: this member shadows the template name in
    // class scope, so nothing of type delay<> can be declared below it.
    delay<MYFLOAT> delay;

    Sub _s1, _s2;
    SubBuf _sb1, _sb2;
    std::vector<MYFLOAT> _mask0;    // L0's share, on L0's bin grid
    MYFLOAT _maskSr{-1.};
    RenderSnap _snap;

    MYFLOAT _win[PM2FFT0]{};
    MYFLOAT _spec[PM2FFT0 + 2]{};
    MYFLOAT _prevPhi[PM2NYQ0 + 1]{};
    MYFLOAT _sumPhi[PM2NYQ0 + 1]{};
    int32_t _phiFrame[PM2NYQ0 + 1]{};
    MYFLOAT _prevMag[PM2NYQ0 + 1]{};
    MYFLOAT _inc[PM2NYQ0 + 1]{};            // this frame's onset increment
    MYFLOAT _mag[PM2NYQ0 + 1]{};
    MYFLOAT _frq[PM2NYQ0 + 1]{};
    MYFLOAT _logm[PM2NYQ0 + 1]{};
    MYFLOAT _env[PM2NYQ0 + 1]{};
    MYFLOAT _outMag[PM2NYQ0 + 1]{};         // collision arbitration (L0)
    // Last frame's final claim map (L0): owner and harmonic number per bin,
    // -1 = unowned. Set in the constructor, rebuilt at end of updateTracks.
    int8_t _prevOwn[PM2NYQ0 + 1]{};
    int8_t _prevHrm[PM2NYQ0 + 1]{};

    int32_t _peakBin[MAX_PEAKS]{};
    MYFLOAT _peakFrq[MAX_PEAKS]{};
    MYFLOAT _peakMag[MAX_PEAKS]{};
    MYFLOAT _peakLog2[MAX_PEAKS]{};
    MYFLOAT _peakSin[MAX_PEAKS]{};
    int32_t _peakSrc[MAX_PEAKS]{};
    int32_t _peakHrm[MAX_PEAKS]{};
    int32_t _peakGhost[MAX_PEAKS]{};        // synthesis-only one-frame bridge
    int32_t _peakGhostHrm[MAX_PEAKS]{};
    int32_t _npeaks{};

    int32_t _claimIdx[MAX_PEAKS]{};
    int32_t _claimHrm[MAX_PEAKS]{};
    int32_t _claimN{};
    MYFLOAT _claimF0{}, _claimMass{};
    MYFLOAT _claimB{-1.};                   // stretch fit from this claim, <0 = none
    bool _claimRoot{};
    int32_t _claimHarm{CLAIM_HARM};

    MYFLOAT _candF0[MAX_CAND]{};
    MYFLOAT _sal[MAX_CAND]{};
    MYFLOAT _hits[MAX_CAND]{};
    int32_t _ncand{};
    MYFLOAT _candLo{-1.}, _candHi{-1.};
    MYFLOAT _log2lo{};

    MYFLOAT _targets[MAX_TARGETS]{};
    int32_t _ntargets{};
    MYFLOAT _targetCps{-1.};
    int32_t _targetScale{-1};

    MYFLOAT _trkF0[MAX_SOURCES]{};
    MYFLOAT _trkF0m1[MAX_SOURCES]{};        // f0 one and two frames back; the
    MYFLOAT _trkF0m2[MAX_SOURCES]{};        // render pitch reads their median-3
    MYFLOAT _trkB[MAX_SOURCES]{};           // inharmonicity (stretch) coefficient
    MYFLOAT _trkDec[MAX_SOURCES]{};
    MYFLOAT _trkRatio[MAX_SOURCES]{};
    MYFLOAT _trkTarget[MAX_SOURCES]{};
    int32_t _trkAge[MAX_SOURCES]{};
    int32_t _trkDied[MAX_SOURCES]{};        // frame the slot's occupant retired
    bool _trkLive[MAX_SOURCES]{};

    int32_t _lfoIdx{};
    int32_t _warm{};
    int32_t _frameNo{};

    bool _linkLeader{};
    int32_t _linkSlot{-1};

    std::atomic<MYFLOAT> *_cps{}, *_scale{}, *_amt{}, *_sources{}, *_purify{},
            *_formant{}, *_glide{}, *_lo{}, *_hi{}, *_dry{}, *_wet{};
    std::atomic<LFO *> *_lfo_cps{}, *_lfo_amt{};
};

#endif //GS_ENABLE_PITCHMAP2

#endif //GRAINSTORM_PITCHMAP2_H
