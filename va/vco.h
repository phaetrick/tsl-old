#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <memory>
#include "defines.h"
#include "modal.h"
#include <tools/aligned_memalloc.h>

namespace tsl { struct AppState; }

// ── WaveTable ────────────────────────────────────────────────────────────────

// Samples are stored as float (half the memory of double); the DSP still reads them
// into MYFLOAT (double) for interpolation, and the build/FFT math stays double via a
// scratch buffer — see the compute* methods.
//
// STORAGE IS EXACTLY tableSize SAMPLES — there is no duplicated guard sample at the
// end, and the interpolator reaches two samples BACK and three FORWARD, so every tap
// must be taken through interpAt(), which masks each index individually. Never index
// data() directly with n±k. The guard used to cost 25% of all table memory and 50% at
// 8192: tableSize is a power of two, so `tableSize + 2` floats push every allocation
// just past a size class and the allocator rounds the whole thing up (16392 B →
// 20480, 32776 B → 49152). An exact power-of-two request wastes nothing.
class WaveTable : public tsl::AlignedVector<float> {
public:
    WaveTable() = default;

    void setWavetable(const MYFLOAT* in, int s) {
        type_ = -1;
        resize(s, 0);
        tableSize = s;
        flenSetup();
        for (int i = 0; i < s; i++) data()[i] = in[i];
    }

    void computeWavetable(int type, int nPartials);
    // Build a bandlimited table from an arbitrary harmonic spectrum (mags[1..nHarm]
    // are sine-phase harmonic amplitudes) — the wavetable equivalent of
    // calculateTable(), limited to nPartials so it stays alias-free per frequency.
    void computeWavetableFromHarmonics(const float* mags, int nHarm, int nPartials);
    // Phase-preserving version: re[i]/im[i] are the complex harmonics of a source
    // cycle of length srcLen (from a forward FFT), so the actual waveform shape
    // (not just magnitudes) is reproduced. Used for time-domain tables (fold, CZ).
    void computeWavetableFromComplex(const float* re, const float* im,
                                     int nSrcHarm, int nPartials, int srcLen);

    int32_t  nPart{};
    uint32_t lobits_32{}, fractMask_32{}, tableMask_32{}, tableSize{};
    MYFLOAT  pfrac_32{};
    uint64_t lobits_64{}, fractMask_64{}, tableMask_64{};
    MYFLOAT  pfrac_64{};

    // The one place a table sample is reconstructed between grid points: a 6-point,
    // 5th-order "optimal 2x" polynomial (Niemitalo). Every mipmap level is at least
    // 2x oversampled relative to its own harmonic content — which is exactly this
    // kernel's design point — so it reconstructs to about -115 dB where the old
    // linear read left -50 dB of inharmonic junk. `n` is the integer sample index,
    // `f` the fraction in [0,1) toward n+1; both wrap.
    MYFLOAT interpAt(uint32_t n, MYFLOAT f) const;

    // Read + interpolate at phs WITHOUT advancing (for morphing between two frames
    // at the same phase before the caller advances once).
    MYFLOAT read(uint32_t phs) const;
    MYFLOAT tick(uint32_t& phs, uint32_t frq) const;
    MYFLOAT tickpw(uint32_t& phs, uint32_t frq, MYFLOAT pw = .5) const;
    MYFLOAT tickpwm(uint32_t& phs, uint32_t frq, MYFLOAT pw = .5) const;
    MYFLOAT tickpw64(uint64_t& phs, uint64_t frq, MYFLOAT pw = .5) const;

private:
    int type_{};
    void calculateTable(int type);
    int  computeTableSize();
    void flenSetup();
};

// ── WaveTables ───────────────────────────────────────────────────────────────

class WaveTables : private std::vector<WaveTable> {
public:
    static constexpr int     harmonicStep = 1;
    static constexpr int     maxHarmonics = 4096;
    static constexpr int     nTables      = (maxHarmonics / harmonicStep) + 1;
    static constexpr MYFLOAT p_scl        = 0.5;
    static constexpr MYFLOAT p_min        = p_scl / (MYFLOAT)maxHarmonics;

    void setup(int type);
    // Same per-partial-count mipmap structure as setup(), but each level is built
    // from the given harmonic spectrum instead of an analytic waveform formula.
    void setupFromHarmonics(const float* mags, int nHarm);
    // Build the mipmap set from an arbitrary time-domain single cycle (length len,
    // power of 2): forward-FFT once, then each level keeps its complex harmonics —
    // preserves phase, so folds / phase-distortion read exactly.
    void setupFromWaveform(const float* cycle, int len);
    WaveTable& getTable(MYFLOAT sampleRate, MYFLOAT frequency);

private:
    void buildIndex(int32_t ntables);
    // partial count → mipmap level, as an INDEX into our own vector rather than a
    // pointer: 2 bytes per entry instead of 8 (this map is 4097 entries long and
    // exists per morph frame, so pointers cost 32 KB a frame — 1 MB per warped
    // wavetable set). Indices also mean the map no longer aliases our own storage,
    // which is what made this class unsafe to copy.
    std::vector<uint16_t> npartsIdx;
};

// The A→B spans MORPH and WARP AMT sweep under modulation (synth.cpp computes
// A + mod*(B-A)). The tables are BAKED across exactly these spans, in this direction
// — B < A bakes backwards — so every frame lands inside the range that is actually
// used and the runtime feeds a normalised 0..1 position along it rather than an
// absolute value. Narrowing a range therefore buys resolution instead of wasting it.
struct WtRange {
    float mA = 0.f, mB = 1.f;   // morph
    float wA = 0.f, wB = 1.f;   // warp amount
    bool operator==(const WtRange& o) const {
        return mA == o.mA && mB == o.mB && wA == o.wA && wB == o.wB;
    }
};

// A morphable wavetable as a 2D grid: nMorph morph frames × nWarp baked warp levels
// (warp is rendered into the bandlimited tables at build time → alias-free; WARP AMT
// interpolates the warp axis at runtime). frames[m*nWarp + w]. Built once per
// (table, warpType) and shared read-only via shared_ptr (active-slot: freed when no
// oscillator references it).
// The picture of a set: one normalised cycle per slice, walking the baked A→B range
// front to back. ~7 KB against a set that is megabytes, and it is extracted ONCE at
// build time on the worker — the render thread never interpolates a table again, it
// draws this.
//
// Runtime warps (wtWarpRuntime) are deliberately NOT applied here. Those types share
// the clean, unwarped set (wtKey folds them to warp 0 and wtQuantised zeroes their
// range), so the warp amount is not part of the key and cannot be baked. The display
// applies that one post-pass live, which is a cheap per-sample shaper rather than the
// four table reads and two interpolations per sample that extraction costs.
struct WtDisplayFrames {
    static constexpr int SLICES = 16, POINTS = 110;
    int   n{0};                            // slices actually filled
    float f[SLICES * POINTS]{};
};

struct WavetableSet {
    std::vector<std::unique_ptr<WaveTables>> frames;
    int nMorph = 0, nWarp = 0;
    // Built with the set, never mutated afterwards, so it can be handed to the render
    // thread by pointer with no synchronisation beyond the handover itself.
    std::shared_ptr<const WtDisplayFrames> display;
    WaveTables& at(int m, int w) const { return *frames[m * nWarp + w]; }
    int nFrames() const { return nMorph; }
};

// ── Display handover: audio thread → render thread ───────────────────────────
// One mailbox per oscillator (0..2). The audio thread publishes the frame block of
// whatever set it has actually selected; the render thread drains to the newest and
// draws it. What crosses is a shared_ptr to the small block above, NEVER the set —
// so the worst a reference drop can cost is a 7 KB delete, and even that cannot land
// on the audio thread: the producer only ever writes slots the consumer has already
// emptied (see the ring in vco.cpp).
//
// This replaces the render thread doing its own cache lookup. That lookup took
// gWtCacheMtx — the same mutex the worker holds while inserting a finished set — and
// it asked with the display's own key, which is why the picture could disagree with
// the sound. Now there is one selection, made in one place, and the display is told.
static constexpr int WT_DISPLAY_OSCS = 3;
// The mailboxes carry twice that: slots [0, WT_DISPLAY_OSCS) are the WT display per
// oscillator, slots [WT_DISPLAY_OSCS, VA_DISPLAY_SLOTS) the PAD display per
// oscillator. Separate slots rather than shared ones because an oscillator's WT and
// PAD pages are both built and both remember their last picture — one slot per
// (osc, page) is what lets each page keep its own.
static constexpr int VA_DISPLAY_SLOTS = WT_DISPLAY_OSCS * 2;

// Audio thread. Cheap no-op unless the key moved since the last successful publish;
// on a move it looks up the ready set and publishes its frames. Retries every block
// while the set is still building, so the display catches up when it lands.
void wtPublishDisplay(int osc, int tableNum, int warpType, const WtRange& range);

// Render thread. Newest published block, or nullptr if nothing new since last call.
std::shared_ptr<const WtDisplayFrames> wtTakeDisplay(int osc);

// Where modulation currently has this oscillator, as 0..1 along the A→B span — 0 is A
// (the front slice), 1 is B (the back one), so it drops straight into the display's own
// t. Published by the audio thread once per block per sounding voice; last writer wins,
// which is that note when one is held and the last-served voice in a chord.
//
// A position rather than a range: the stack already IS the range, so what a knob's blue
// value arc corresponds to here is where in that range the sound actually is.
void wtPublishMorph(int osc, float pos01);

// Render thread. False when nothing has published for a while — no voice is sounding,
// so there is no position to draw and the trace should be absent rather than parked.
bool wtTakeMorph(int osc, float& pos01);

// The one part of the picture that cannot be baked: runtime warps share the clean set
// (wtKey folds them to warp 0), so their amount never reaches the key and no republish
// happens when it moves. Applies that shaper across the block, slice k at amount
// warpA + t*(warpB-warpA), writing SLICES*POINTS floats into `out`.
//
// Returns false and writes NOTHING when this warp type is baked or the amount is zero
// — draw the published block directly. Cheap next to extraction: one shaper call per
// sample against four table reads plus two interpolations.
bool wtApplyRuntimeWarp(int warpType, float warpA, float warpB,
                        const WtDisplayFrames& in, float* out);

// Get (build lazily, cache by table + warp type + the quantised A→B ranges) the baked
// 2D wavetable set. BLOCKING build — GUI/worker threads only, never the audio thread
// (the audio path uses the off-thread request mechanism in vco.cpp instead).
std::shared_ptr<WavetableSet> getWavetableSet(int tableNum, int warpType,
                                              const WtRange& range);

// Ask for an off-thread build if this set isn't cached yet, WITHOUT an oscillator
// having to want it first. Cheap and idempotent (the request dedups against anything
// already in flight), so it is safe to call every block from the audio thread — the
// padWarmRequest contract, for wavetables.
//
// This exists because refreshWt() only ever runs from a live oscillator, so the first
// request for a set used to happen AT note-on, one note too late. On that miss the
// voice keeps whatever set it is already holding, so the first note after a preset
// change sounded on the outgoing table (or the default) and only switched when the
// build landed, mid-note. See warmWtSets in synth.cpp.
void wtWarmRequest(tsl::AppState* app, int tableNum, int warpType, const WtRange& range);

// Number of wavetable / PADsynth builds currently running on the worker. Lock-free
// (a scan of a small atomic array), so the GUI may call it every frame. Used to tell
// the user that a table is being computed rather than leaving the display looking
// stale or the sound looking unresponsive.
int wtBuildsInFlight();
int padBuildsInFlight();

// Fill out[0..n-1] with one cycle of built-in wavetable `tableNum` at morph position
// `morph` (0..1), with warp (type/amt) applied, peak-normalised for display. Builds
// the table lazily if needed, so this may be called from the GUI thread.
void getWavetableDisplay(int tableNum, float morph, int warpType, float warpAmt, float* out, int n);

// The 3D stack view lives in WtDisplayFrames (above), extracted at build time and
// handed to the render thread through wtPublishDisplay / wtTakeDisplay. MORPH and
// WARP AMT are each an A/B pair defining the range modulation sweeps (synth.cpp's
// applyMorph/applyWarpMod compute A + mod*(B-A)), so the stack walks BOTH ranges
// together: slice k sits at morph = mA + t*(mB-mA) and warp amount = wA + t*(wB-wA)
// for t = k/(count-1). It therefore shows the actual sequence of waveforms the
// modulation will produce, and runs backwards when B < A. Slice 0 is the current
// unmodulated sound (both axes at A). Each slice is peak-normalised on its own so
// quiet ones stay visible, matching getWavetableDisplay's convention.

// ── PADsynth (Nasca) ─────────────────────────────────────────────────────────
//
// A PADsynth table is a multi-SECOND sample, not a single cycle. Each harmonic is
// smeared into a band of bins, every bin gets a random phase, and the whole
// spectrum is inverse-FFT'd into one long seamless loop. The slow beating between
// the bins inside a band is the sound — that's what makes it an ensemble rather
// than a wavetable. Two consequences drive everything below:
//
//  - It needs key REGIONS, like a sampler multisample, not a mipmap. Each table is
//    built full-bandwidth for the TOP of the region it serves, so transposing up
//    would fold; regions are latched at note-on so a held note never switches.
//
//  - Every bin's phase is derived from the BIN INDEX AND SEED ALONE — never from
//    morph, bandwidth, table or region. Any two sets sharing a seed are therefore
//    phase-aligned bin-for-bin, so crossfading them collapses to
//        a·(Ma·e^iφ) + b·(Mb·e^iφ) = (a·Ma + b·Mb)·e^iφ
//    an exact linear interpolation of the magnitude spectrum. That is a real
//    spectral morph. Seed the phase from anything else and the same crossfade
//    becomes a dissolve between two decorrelated noises, audible as both textures
//    playing at once through the middle of the fade.

// Baked morph positions, interpolated at runtime → MORPH stays a mod destination.
static constexpr int PAD_MORPH_LEVELS = 4;
static constexpr int PAD_REGIONS      = 15;

struct PadParams {
    int   table     = 0;      // index into the curated PADsynth table list
    float bandwidth = 40.f;   // cents
    float bwScale   = 1.f;    // bandwidth growth exponent vs harmonic index
    float stretch   = 0.f;    // inharmonicity, 0 = perfectly harmonic
    int   seed      = 0;
    // MORPH A→B, exactly as WtRange does it for the wavetable oscillator: the baked
    // levels span THIS range rather than a fixed one, so a narrow A→B gets the full
    // resolution instead of landing between two coarse frames. Always passed through
    // padQuantMorph() before it gets here — these are part of the cache key.
    float mA = 0.f, mB = 1.f;
    bool operator==(const PadParams& o) const {
        // Keep this in step with padKey() in vco.cpp — a field compared here but not
        // hashed there would make two different sets share an in-flight claim. And
        // every PadParams must have been through padSnapParams(), or this exact compare
        // and padKey's quantised one disagree; see the note above padSnapParams.
        return table == o.table && bandwidth == o.bandwidth
            && bwScale == o.bwScale && stretch == o.stretch && seed == o.seed
            && mA == o.mA && mB == o.mB;
    }
    bool operator!=(const PadParams& o) const { return !(*this == o); }
};

// One key region: PAD_MORPH_LEVELS phase-aligned tables of `len` samples (+1 guard
// sample holding a copy of [0] so the interpolating read never has to wrap).
struct PadRegion {
    tsl::AlignedVector<float> lvl[PAD_MORPH_LEVELS];
    int      len   = 0;
    int      shift = 0;       // 64-bit phase → sample index
    double   f0    = 0.0;     // build fundamental (top of the region)
};

struct PadSet {
    PadRegion region[PAD_REGIONS];
    PadParams params;
    // The stack view, same block type and conventions as WavetableSet::display:
    // SLICES windows walking the baked mA→mB span front to back, each peak-
    // normalised. Extracted ONCE at build time on the worker (from the middle-C
    // region, two fundamental periods per slice, levels lerped exactly as padPrep
    // does at runtime), so the render thread never touches the 16.75 MB tables.
    std::shared_ptr<const WtDisplayFrames> display;
};

// Region index for a playing frequency. Latched at note-on — see the header note.
int padRegionFor(double freq);

// BANDWIDTH knob (0..1) → cents, per table. The knob is uniform; the cents behind it
// are not, because each table stops being a tone at a different smear width. Takes
// bwScale so the merge point can be made independent of it — see PAD_BW_NID.
float padBandwidthCents(int table, float knob, float bwScale);

// ── the four fixed build inputs ──────────────────────────────────────────────────
// BANDWIDTH, BW SCL, STRETCH and SEED are no longer controls. Removed from the UI on
// 2026-08-16: all four are BUILD inputs, so touching any of them costs a ~150 ms,
// 16.75 MB set rebuild and a panel over the keyboard, and between them they gave this
// oscillator six faders of which only MORPH A/B respond in real time. PAD is now "pick
// a table, move the morph", and the whole rebuild-on-a-knob-drag apparatus — the
// settle timer, the coalescing, the eviction order — has only MORPH A/B left to fire
// on.
//
// THE PARAMETERS THEMSELVES STILL EXIST (VCOxPADBW / PADBWSC / PADSTR / PADSEED) and
// must not be deleted: a parameter id IS its position, so removing one renumbers every
// id below it and silently repoints every saved preset. They are simply never read.
// padParamsFromSnapshot in synth.cpp is the single site that used to read them.
//
//   BANDW  0.5  — the fader is linear in the noise fraction (see padBandwidthCents),
//                 so this is exactly half the wash each table can carry before it
//                 stops being a tone; per-table in cents, because the cap is.
//   BW SCL 1.0  — Nasca's law: the smear stays constant in CENTS up the series, so a
//                 table keeps its character under transposition. Also the only value
//                 at which the per-table cap is exact — the linearisation drifts to
//                 0.74–1.50% noise at s=0.5 and 0.95–3.93% at s=2, against a flat
//                 1.29% at s=1.
//   STRETCH 0   — perfectly harmonic. Inharmonicity is a per-sound choice, and with no
//                 control it would be one choice made once for every sound.
//   SEED   0    — only re-rolls the random phases; no character attaches to it.
//
// To retune BANDW by ear, change this one number — but PAD_TABLE_TRIM is measured AT
// this value and has to be re-solved with it.
static constexpr float PAD_FIXED_BW      = 0.5f;
static constexpr float PAD_FIXED_BWSCALE = 1.0f;
static constexpr float PAD_FIXED_STRETCH = 0.0f;
static constexpr int   PAD_FIXED_SEED    = 0;

// Snap a MORPH endpoint to the cache-key grid. Every caller that fills PadParams::mA
// or mB must go through this, or dragging the fader asks for a different 16.75 MB set
// on every pixel.
float padQuantMorph(float v);

// Put the continuous fields on the same grid padKey() hashes them on. MUST be called
// on every PadParams before it is used to look one up — see the note in vco.cpp.
void padSnapParams(PadParams& p);

// Ask for an off-thread build if this set isn't cached yet. Cheap and idempotent —
// safe to call every block from the audio thread.
void padWarmRequest(tsl::AppState* app, const PadParams& p, double sampleRate);

// The PAD counterpart of wtPublishDisplay, publishing PadSet::display into mailbox
// slot WT_DISPLAY_OSCS + osc. Same contract: audio thread, keyed on padKey so it is
// a cheap no-op until the params move, retried every block while the set builds.
void padPublishDisplay(int osc, const PadParams& p, double sampleRate);

// Build lazily / fetch from cache. BLOCKING — worker threads only, never audio.
std::shared_ptr<PadSet> getPadSet(const PadParams& p, double sampleRate);

// A wavetable's harmonic recipe at a continuous morph position, mags[1..nHarm]. This
// is PADsynth's whole input, and both PAD_BW_CAP and the PAD_TABLES set are solved
// from it — see the curation rules in vco.cpp. Exposed so a tool can re-solve them
// against the real recipes rather than a copy that drifts. Time-domain tables give
// zeros; they have no recipe, which is why they are not eligible for PAD_TABLES.
void wtHarmonicsAt(int wt, double t, float* mags, int nHarm);

// Number of curated PADsynth tables, and the map into the 27 built-in wavetables.
int padTableCount();
int padTableToWt(int padIndex);

// ── Modal resonator bank (oscillator type 97) ────────────────────────────────
//
// The cheap opposite of PADsynth: no table, no build, no cache, no key regions,
// no warm-up. The whole oscillator is 14 complex rotations (see modal.h), so the
// only state that survives a note is the ringing itself.
//
// DECAY is the one parameter here that can move while a note rings, and the split
// is structural rather than cautious. T60 sets r, which appears solely in the next
// rotation step, so rewriting it lengthens or shortens the tail from that sample
// forward with the state vector's magnitude untouched — the same property that
// makes retune() click-free (measured: a full-octave retune of a ringing bank
// changes max |dy| by 1.74x, i.e. exactly the frequency ratio, not a step).
//
// CHARACTER restructures the bank. BRIGHT, MALLET and STRIKE are initial
// conditions, consumed by excite() at note-on and thereafter living in the state
// vector: changing them mid-ring is not risky, it simply does nothing until the
// next strike — which is also the only moment at which a real strike can change.
// That is why DECAY has AT / MW / KEYTRACK routes and the others have none.
struct ModalOscParams {
    int    character = 4;      // index into kModalCharacters
    double decaySec  = 2.0;    // fundamental T60
    double bright    = 0.5;
    double hardness  = 0.55;   // synth.cpp adds velocity on top of this
    double position  = 0.24;
    bool operator==(const ModalOscParams& o) const {
        return character == o.character && decaySec == o.decaySec && bright == o.bright
            && hardness == o.hardness && position == o.position;
    }
    bool operator!=(const ModalOscParams& o) const { return !(*this == o); }
};

// How often tick() rewrites the bank's rotation coefficients. 14 sin/cos per 64
// samples is ~0.2% of the per-sample cost of the bank itself, and 64 samples at
// 48 kHz is 1.3 ms — far below the ~20 ms where pitch modulation starts to sound
// stepped.
static constexpr int MODAL_RETUNE_INTERVAL = 64;

// Unison configuration, computed once per block by synth.cpp and passed to
// Vco::tickUnison. `n` detuned copies, each a frequency multiplier `ratio` and a
// pre-normalised `gain` (blend + level compensation already baked in).
static constexpr int UNISON_MAX = 7;
struct VcoUnison {
    int   n = 1;
    float ratio[UNISON_MAX] = {1.f};
    float gain[UNISON_MAX]  = {1.f};
};

// PADsynth stacks to the same depth as everything else. It was held at 1 for a long
// time on the theory that a copy reading a megabyte-per-level region would turn one
// sequential stream into "dozens of scattered reads per sample" — MEASURED AND WRONG.
// Each copy is its own sequential walk, and a few sequential streams are exactly what
// a prefetcher is for: on arm64, 4 copies cost 3.2 ns/sample against 1.0 for one, and
// a 2^18 region measures the same as a 2^14 one. Do not re-impose a PAD-specific cap
// without measuring on the target first; the numbers above are a Mac.

// ── Vco — compatibility wrapper used by synth.cpp ───────────────────────────
// Preserves the check(mode) / tick(cps,gain,pw) / phs API.
// Internally backed by WaveTables for bandlimited synthesis.
//
// PA mode integers:
//   -1 : sine (reads DATA->sinewave)
//    0 : sawtooth
//    2 : square/PWM
//    4 : sawtooth/triangle/ramp
//   97 : modal resonator bank
//   98 : PADsynth
//   99 : wavetable

class Vco {
public:
    void    reset();
    void    rev();
    // pmPhase: transient phase offset added to the table read only (for phase
    // modulation) — does NOT advance the accumulator, so pitch stays stable.
    MYFLOAT tick(MYFLOAT cps, MYFLOAT gain, MYFLOAT pw = .5f, uint32_t pmPhase = 0);
    // 2x-oversampled tick for phase modulation: produces two subsamples read at
    // phs+pm0 and phs+pm1 while advancing the accumulator by exactly one _frq.
    // Generating the PM at 2x (then decimating in the caller) keeps the sidebands
    // that PM throws above Nyquist from folding back as aliasing.
    void    tickOS2(MYFLOAT cps, MYFLOAT gain, MYFLOAT pw,
                    uint32_t pm0, uint32_t pm1, MYFLOAT& s0, MYFLOAT& s1);
    // Unison tick: sum `u.n` detuned copies of the current waveform. Voice 0 uses
    // the primary accumulator `phs` (so sync/reset still act on it); the rest use
    // _uphs[]. The table is fetched once at the centre frequency and reused for all
    // copies (detune is small, so they share the same bandlimited mipmap). PM is not
    // applied here — the caller uses the plain tick()/tickOS2() path when PM is on.
    MYFLOAT tickUnison(MYFLOAT cps, MYFLOAT gain, MYFLOAT pw, const VcoUnison& u);
    // Spread the extra unison accumulators across the cycle on note-on so the copies
    // don't start phase-aligned (avoids an initial comb notch / build-up transient).
    void    spreadUnison(int n);
    void    check(int mode);
    void    selectWavetable(int n);   // pick which built-in wavetable (WT mode)
    // Note-on: set everything the cache key is made of at once, then look up ONCE.
    // Doing it through selectWavetable + setWarp + setRange instead would look up
    // after each, and the intermediate combinations (new table with the previous
    // note's range, and vice versa) are keys no one will ever read — each one costing
    // a wasted off-thread build and an eviction from the small warm LRU.
    void    aimWt(int tableNum, int warpType, float warpAmt, const WtRange& r) {
        _tableNum = tableNum; _warpType = warpType; _warpAmt = warpAmt; _range = r;
        ensureWt(/*immediate=*/true);   // note-on: nothing is sounding yet
    }
    // Drop this oscillator's references to the shared wavetable/PADsynth sets,
    // with the destructor handed to the UiTasks worker so the deallocation never
    // lands on the audio thread. Call when a voice goes back to the pool.
    void    releaseTables();
    // PADsynth: these are the BUILD-time params — changing any of them triggers an
    // off-thread rebuild. The runtime morph position rides the `pw` argument of
    // tick()/tickUnison() exactly as it does for WT, so it stays per-sample ramped
    // and fully modulatable without any extra plumbing.
    void    setPad(const PadParams& p);
    // Note-on: unlatch the key region and re-randomise the read offset.
    void    padNoteOn();
    // Modal (type 97). DECAY is applied to the live bank; the rest only stash and
    // are picked up by the next modalNoteOn(). See the note above ModalOscParams.
    void    setModal(const ModalOscParams& p) {
        if (p.decaySec != _modalParams.decaySec) _modal.setDecay(p.decaySec);
        _modalParams = p;
    }
    // freqHz is REAL Hz here, not the normalised cps that tick() takes.
    void    modalNoteOn(double freqHz, double attackSec, double velocity, uint32_t seed);
    // WARP: type selects which baked warp set; amt is the absolute modulated amount,
    // normalised against the baked range at read time.
    void    setWarp(int type, float amt) { _warpAmt = amt; _warpType = type; ensureWt(); }
    // The A→B spans the tables are baked over. Called per block from synth.cpp; the
    // quantised range is what keys the set, so a fader drag costs a bounded number of
    // builds rather than one per pixel.
    void    setRange(const WtRange& r)   { _range = r; ensureWt(); }
    // Which built-in table. ALSO called per block, and that is newer than it looks:
    // _tableNum used to be written only by aimWt() and selectWavetable(), both of
    // which run at note-on, so turning TABLE did nothing at all to a sounding voice
    // while WARP TYPE (via setWarp) changed under your fingers. The swap fade is what
    // makes carrying it per block reasonable — without the duck, replacing the tables
    // mid-note is the step this whole mechanism exists to avoid.
    void    setTable(int n)              { _tableNum = n; ensureWt(); }
    const WtRange& range() const { return _range; }

    // Keep asking until the set we HOLD is the one we WANT. This has to be level-
    // triggered, not edge-triggered: a miss only queues an off-thread build, so if
    // this fired once per change the oscillator would keep the old tables forever and
    // the build would land unused. That is exactly what happened when the A→B range
    // entered the cache key — every fader nudge minted a key that could not possibly
    // be ready yet, so warp type and warp A/B stopped changing the sound at all. Costs
    // one small map lookup per block only while a rebuild is outstanding.
    //
    // A new set is taken by a voice that is already sounding — turning TABLE or WARP
    // has to be audible on a held note, and latching it to note-on made a repeated
    // press on the SAME key keep the old tables (retrigger reuses the voice and never
    // reaches the note-on path). What removes the lurch is the fade in refreshWt(),
    // not refusing the swap.
    // `immediate` means "adopt now, no duck". Note-on passes it: phs is zeroed and the
    // amp envelope restarts there, so there is no running signal for a table swap to
    // step. Routing note-on through the fade instead made a new note START on the old
    // table and only change 4 ms in — and if the note ended, or the voice was reaped,
    // before the duck reached the bottom, cancelSwapFade() threw the pending set away
    // and the new table was never adopted at all. That is "changing the table does
    // nothing", and it is why this argument exists.
    void    ensureWt(bool immediate = false) {
        if (_mode != 3) return;                       // WT oscillators only
        if (!_wt || _bakedTable != _tableNum || _bakedType != _warpType
                 || !(_bakedRange == _range))
            refreshWt(immediate);
    }

    tsl::AppState* _appState{};
    uint32_t phs{};
    uint32_t _uphs[UNISON_MAX - 1]{};  // extra unison accumulators (voices 1..n-1)
    uint32_t _frq{};   // last phase increment, kept for sub-sample hard sync in reset()
    int      _dir{1};

    void refreshWt(bool immediate = false);   // (re)fetch the set for table+warp type
    void adoptWt(std::shared_ptr<WavetableSet> sp, const WtRange& r, int table, int type);

    // ── Swap fade ────────────────────────────────────────────────────────────
    // Duck to zero, swap, come back. ~4 ms each way: long enough that the ramp is
    // inaudible as a ramp, short enough that it reads as an articulation rather than
    // a dropout. Restarting mid-fade is safe — it keeps the current gain and only
    // reverses direction, so a fast series of table changes never steps.
    static constexpr MYFLOAT SWAP_FADE_SEC = 0.004;

    // Duck on live structural changes: OFF.
    //
    // The mechanism works — it just does not work RELIABLY, and an intermittent table
    // change is worse than an instant one with a small step. With this false a live
    // TABLE / WARP TYPE change is adopted the moment the set is ready, which is what
    // the oscillator did before the duck existed and is the behaviour that was never
    // in doubt. The duck stays compiled and the P/S/D/A counters stay wired, so
    // turning this back on is a one-character change once the counters say which
    // stage stalls. Do not flip it back without reading them first.
    static constexpr bool SWAP_DUCK_ENABLED = false;

    // Out of line: needs the sample rate, and AppState is only forward-declared here.
    void beginSwapFade();
    // Abandon a fade and anything queued behind it, leaving the gain wide open. For
    // voice recycling ONLY — it steps _swapGain, so calling it on a sounding voice is
    // the click the fade exists to prevent. A pooled voice that was mid-fade when its
    // note ended would otherwise wake up still ducking and, at the bottom, overwrite
    // the set the new note just correctly adopted with the previous note's pending
    // one — which reads as "changing the table does nothing", permanently, because
    // _bakedTable then never matches _tableNum again.
    void cancelSwapFade();

    // One sample of the duck. Returns the multiplier for this sample and performs the
    // swap at the bottom, where the signal is silent and a table change cannot produce
    // a step. The idle test is inline and is one compare; the ramp itself is out of
    // line because the swap needs handOffRelease/adoptWt, which live in vco.cpp.
    MYFLOAT swapFadeTick() {
        if (_swapGain >= 1. && !_swapFadingOut) return 1.;
        return swapFadeStep();
    }
    MYFLOAT swapFadeStep();
    // Runtime waveshaping warps (types 6-8: BITS/RATE/DRIVE) applied to the osc
    // output instead of baked into the table — keeps the broadband grit an
    // alias-free precomputed table can't carry. No-op for baked/OFF warp types.
    MYFLOAT applyRuntimeWarp(MYFLOAT x);

    WaveTables*   _tables{};
    std::shared_ptr<WavetableSet> _wt;   // baked 2D wavetable set (mode 3), active-slot
    int           _tableNum{0};   // selected built-in table
    int           _warpType{0};   // baked: 0 off,1 sync,2 bend,3 asym,4 fold,5 crush,9 sat,10 step; runtime: 6 bits,7 rate,8 drive
    float         _warpAmt{0.f};  // baked: interp across warp levels; runtime: shaper amount
    WtRange       _range{};       // A→B spans we WANT baked
    // What _wt actually holds. Reads normalise against _bakedRange, never _range, so a
    // pending rebuild keeps sounding right instead of indexing a span it wasn't built
    // for; ensureWt() compares all three to know whether it is still waiting.
    WtRange       _bakedRange{};
    int           _bakedTable{-1};
    int           _bakedType{-1};
    // ── Swap fade ────────────────────────────────────────────────────────────
    // A table or warp-type change replaces the waveform under a running phase
    // accumulator, which steps the output. Rather than refuse the swap (which makes
    // the control inert on held notes) or accept the step, the oscillator ducks its
    // own output to zero, swaps at the bottom, and comes back up. Range-only changes
    // do NOT fade: consecutive bakes of the same table differ by a fraction of a
    // frame, so the step is already below audibility and a duck would be the louder
    // artefact of the two.
    //
    // Held here rather than in the read path so it costs one multiply per sample and
    // no second table lookup — a true crossfade would have to read both sets for the
    // duration, doubling the hottest loop in the oscillator for 8 ms.
    std::shared_ptr<WavetableSet> _wtPending;   // built, waiting for the fade to reach 0
    std::shared_ptr<PadSet>       _padPending;
    WtRange       _pendingRange{};
    int           _pendingTable{-1}, _pendingType{-1};
    MYFLOAT       _swapGain{1};      // 1 = idle/open, ramps to 0 and back on a swap
    MYFLOAT       _swapInc{0};       // per-sample step; sign is the direction
    bool          _swapFadingOut{false};
    MYFLOAT       _decHold{0};    // RATE (runtime warp) sample-and-hold value
    double        _decPhase{0};   // RATE decimation phase accumulator

    // PADsynth (mode 4). The table is seconds long, so it needs its own 64-bit
    // accumulator spanning the whole sample rather than the 31-bit single-cycle one.
    void      refreshPad(bool immediate = false);
    MYFLOAT   padRead(MYFLOAT cps, int div, MYFLOAT morph);   // cps = cycles/sample

    // Everything a PADsynth read needs that is the SAME for every unison copy: which
    // two morph levels to blend, how far apart the table's samples are, and the centre
    // increment. Hoisting it out of the copy loop is what keeps unison affordable —
    // the per-copy work is then just two interpolated reads and an add.
    struct PadTap {
        const float* la{nullptr};   // the two morph levels to blend between
        const float* lb{nullptr};
        MYFLOAT      mf{0};         // blend between them
        uint64_t     inc{0};        // centre increment, before the per-copy ratio
        int          shift{0};      // 64 - log2(len): phase → sample index
    };
    bool      padPrep(MYFLOAT cps, int div, MYFLOAT morph, PadTap& t);
    static inline MYFLOAT padTapRead(const PadTap& t, uint64_t& phase, uint64_t inc);

    // Advance the per-voice PADsynth drift by `step` samples and return the factor to
    // multiply the read rate by. See PAD_DRIFT_CENTS in vco.cpp for what it is for.
    MYFLOAT padDriftTick(double step);

    std::shared_ptr<PadSet> _pad;
    PadParams _padParams;
    uint64_t  _padPhase{};
    // Per-voice drift state. Zero at note-on so a note STARTS in tune and wanders
    // from there; _padDriftLeft at 0 makes the first tick pick a target immediately.
    MYFLOAT   _padDrift{0.}, _padDriftTarget{0.};
    double    _padDriftLeft{0.};    // samples until the next target
    uint32_t  _padDriftRng{1u};
    // Extra PADsynth unison accumulators. Sixty-four bit and separate from _uphs[]
    // because they index a whole multi-second sample, not a single cycle.
    uint64_t  _upadPhase[UNISON_MAX - 1]{};
    int       _padRegion{0};      // latched at note-on; never changes while held
    bool      _padRegionLatched{false};
    // Modal bank (mode 5). No shared_ptr and no cache: unlike _wt / _pad this is
    // per-oscillator state, ~500 bytes, and costs nothing to construct.
    pa::ModalBank   _modal;
    ModalOscParams  _modalParams;
    int             _modalCounter{0};   // samples since the last retune()

    int         _mode{-3};    // -3=uninit, -1=sine, 0=simple, 1=PWM, 2=ramp, 3=wavetable,
                              //  4=PADsynth, 5=modal
    int         _oldmode{-3};
    // sine-mode parameters (mode -1 only)
    uint32_t    _lobits{};
    uint32_t    _mask{};
    MYFLOAT     _pfrac{};
    MYFLOAT*    _table{};
};
