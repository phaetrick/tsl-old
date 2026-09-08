//
// PITCHMAP -- polyphonic pitch mapping. See pitchmap.h for what it is and why
// it is a mono FX rather than a PV algorithm.
//
// One frame is:
//
//   1. analyse   -- magnitude and *instantaneous frequency* per bin, the latter
//                   from the phase difference against the previous frame.
//   2. peaks     -- local maxima, each carrying the phase-derived frequency of
//                   its own bin, which is a far better estimate than anything
//                   the magnitude shape can give.
//   3. track     -- the notes already playing claim their own peaks FIRST, so a
//                   held note keeps its identity and its target rather than
//                   being re-decided 47 times a second.
//   4. detect    -- multi-f0 by harmonic salience with iterative cancellation,
//                   over what is left: find the f0 that explains the most peak
//                   energy, take its peaks away, repeat. Only finds notes that
//                   are new.
//   5. map       -- each note gets one ratio, target/f0, and every peak that
//                   belongs to it moves by that same ratio. This is the step
//                   that makes it a transposition rather than a snap.
//   6. synthesis -- each peak's region is translated rigidly and rotated as a
//                   unit, the rotation coming from an output phase integrated
//                   at the peak's new frequency. That is what keeps consecutive
//                   frames adding coherently after a shift.
//
// Order matters in 3/4: doing it the other way round is what made the first
// version jump in pitch. See the note above updateTracks.
//

#include <cmath>
#include <cstring>
#include "pitchmap.h"
#include "track.h"
#include "grainstorm.h"
#include "lfo.h"

// Harmonic-number tables. Both are hot inner-loop constants and both are exact
// as written, so nothing here needs a log or a sqrt per peak.
static constexpr MYFLOAT LOG2H[PitchMapMaxHarmTbl] = {
        0., 0., 1., 1.5849625007211562, 2., 2.321928094887362, 2.584962500721156,
        2.807354922057604, 3., 3.169925001442312, 3.321928094887362,
        3.4594316186372973, 3.584962500721156, 3.700439718141092,
        3.807354922057604, 3.9068905956085187, 4.};
static constexpr MYFLOAT INV_SQRT_H[PitchMapMaxHarmTbl] = {
        0., 1., .7071067811865476, .5773502691896258, .5, .4472135954999579,
        .4082482904638631, .3779644730092272, .35355339059327373,
        .3333333333333333, .31622776601683794, .30151134457776363,
        .2886751345948129, .2773500981126146, .2672612419124244,
        .25819888974716115, .25};
static_assert(PitchMapMaxHarmTbl == 17, "LOG2H/INV_SQRT_H cover harmonics 1..16");

// How much closer a competing scale step has to be, in octaves, before a note
// changes its mind: 40 cents. A note that sits exactly between two steps would
// otherwise flip on every frame and the output warbles between two pitches.
static constexpr MYFLOAT PITCHMAP_HYST_OCT = .0333;
// Frames a note track survives without claiming its peaks. Six frames is
// ~130 ms: long enough to ride out a few frames where a chord's inner voice is
// masked, short enough that a finished note releases its slot promptly.
static constexpr int PITCHMAP_TRACK_HOLD = 6;
// A claimed peak's region tiles out to the neighbour midpoints so that every
// bin lands somewhere -- but only the core of it MOVES. The mainlobe is what
// has to arrive intact (see the synthesis note); past it the region is the
// noise floor it happens to carry (see the PURIFY clamp), and the two boundary
// regions are unbounded -- the first runs down to DC and the last runs to
// Nyquist, so the topmost claimed peak dragged everything above it, air and
// cymbals included, by its own shift, and the lowest dragged the sub-bass.
// 16 bins is 8x the Hann mainlobe half-width; what an off-centre partial
// leaves beyond that is -76.5 dB of itself (measured, half-bin worst case),
// i.e. floor -- and floor does not transpose. Deep-vibrato high harmonics
// smear wider than this, but those fail the sinusoidality claim gate and are
// never shifted in the first place.
static constexpr int32_t PITCHMAP_SHIFT_W = 16;
// How much a peak that is only some other note's HIGH harmonic counts as
// available to a note being born. Ownership at h > VOTE_HARM is weak evidence:
// the peak is one of forty things that note is made of, and it may well be the
// FUNDAMENTAL of something else. Without this a solo bass with a 40-harmonic
// net owns every peak up to 4 kHz, so the salience search finds nothing
// unexplained and no second note can ever be created -- the first note simply
// keeps the whole spectrum. Weighted below a genuinely free peak so that real
// unexplained material still wins the argmax where there is any.
static constexpr MYFLOAT PITCHMAP_WEAK_OWN = .5;
// Half-width of the spectral envelope estimator, in hertz. Wide enough to smooth
// over the partials of a low note, narrow enough to keep a formant.
static constexpr MYFLOAT PITCHMAP_ENV_HZ = 400.;
// Hann on analysis times Hann on synthesis, overlap-added at hop N/4, sums to
// 3/2 exactly. Applied once, on the way out.
static constexpr MYFLOAT PITCHMAP_OLA_GAIN = 2. / 3.;
// A frame is a transient when more than half its magnitude is NEW -- spectral
// flux, sum(max(0, mag - prevMag)) / sum(mag). Measured on synthetic frames:
// drum bursts reach .69-.86, a second note entering at equal level only .38,
// +-50 cent vibrato .18, a steady tone 0. On a transient frame the salience
// search is skipped, so a drum hit cannot mint a "note" and get a chunk of
// itself pitch-shifted; a real new note is merely captured one frame (21 ms)
// later, with its attack passing through untouched. (Full-depth tremolo rises
// also cross the threshold, but a tremolo'd note is already tracked and the
// track holds through the null, so nothing is lost.)
static constexpr MYFLOAT PITCHMAP_FLUX_THR = .5;
// A claimed note whose ratio is within this of 1 (3 cents) is not shifted at
// all: its regions pass through bit-exact, phases untouched, and the phase
// accumulators are resynced to the input. Most of a song mapped to its own key
// is ALREADY on target, and integrating those notes' phases anyway makes them
// random-walk against their own window skirts and against the other channel --
// audible as phasiness on material that should have come through clean. A
// 3-cent shift rendered as 0 is inaudible; the phasiness was not.
static constexpr MYFLOAT PITCHMAP_IDENT_OCT = .0025;
// Above the identity gate the render frequency BLENDS from the partial's own
// measured position (times the ratio) onto its harmonic slot, fully locked by
// 48 cents of correction. Without the blend, a partial sitting 20 cents off
// its grid slot -- string stretch, a detuned unison stack -- snapped that far
// the instant its note crossed the identity gate, and a ratio hovering near
// the gate turned that into an audible shimmer. Fully remapped notes (a scale
// step or more) are past the zone and stay rigidly locked, which is where the
// lock's mono coherence and stereo pinning actually matter.
static constexpr MYFLOAT PITCHMAP_BLEND_OCT = .04;
// Sinusoidality: a resolved partial's phase-derived frequency is a PLATEAU
// across its mainlobe -- the bins next to the peak report the same frequency
// -- while a noise maximum's estimates scatter. s = the larger neighbour
// disagreement in bins. Measured (scratchpad sin_check.py): clean partials
// 0.00 even inside a dense mix, hat noise median .93 (10th pct .37), deep
// +-50 cent vibrato partials ~.6. So: claims at high harmonic numbers, where
// hat peaks fake "partials" of a bass or mid note, need s < .5 -- that is
// what actually removes hi-hats at full PURIFY. The f0 vote is not gated but
// smoothly DOWN-WEIGHTED by 1/(1+(s/.25)^2): a hat peak votes at 7%, a clean
// partial at 100%, and a vibrato'd note -- whose partials all smear alike --
// keeps its votes balanced and its tracking intact.
static constexpr MYFLOAT PITCHMAP_CLAIM_SIN = .5;
static constexpr MYFLOAT PITCHMAP_VOTE_SIN = .25;
// How far the full f0 vote may stray from a root-only (h <= 2) estimate before
// the root wins outright: 15 cents. See the anchor note in gatherClaim.
static constexpr MYFLOAT PITCHMAP_ROOT_ANCHOR_OCT = .0125;
// Half-width, in bins, of the part of a CLAIMED region that is really the
// partial. Regions run to the neighbour midpoints, and up where partials are
// 20 bins apart most of that is carried noise floor -- which is why hi-hats
// used to survive PURIFY 1 by riding inside the melody's own high-harmonic
// regions. Outside kp +- LOBE a claimed region purifies like the residual.
static constexpr int32_t PITCHMAP_LOBE = 4;
// Frames a live track must have gone unclaimed before a new candidate may
// steal its slot. Two frames (42 ms) undercut TRACK_HOLD badly enough that a
// chord voice masked by a hit could lose its slot and come back as a fresh
// note with a fresh decision; three is still fast enough for melody capture.
static constexpr int PITCHMAP_STEAL_AGE = 3;
// Portamento handoff. A track can never FOLLOW a melody step -- the claim
// window is 35 cents -- so every note change is a death and a rebirth, and a
// rebirth seeded exact snapped from the old mapping to the new target inside
// one hop: the "jumps in melodies". A new note that lands close (in time and
// in pitch) to its slot's previous occupant is treated as that melody
// continuing: it inherits the old RATIO and glides to its own target at the
// GLIDE rate. At GLIDE 0 the first update still lands exact, so the hard-snap
// character is preserved there. Genuinely new voices -- far in pitch, or
// arriving long after the slot emptied -- still seed exact (no scoop).
static constexpr int PITCHMAP_HANDOFF_FRAMES = 8;
static constexpr MYFLOAT PITCHMAP_HANDOFF_OCT = .5;
// From this PURIFY upward a shifted claimed partial is rendered as an exact
// Hann kernel at its FRACTIONAL target bin instead of an integer-translated
// region. Region translation has three defects that all peak in the low mids
// and all surface when PURIFY strips the residual that masked them: the shift
// quantizes to a whole bin (a glide crosses bin boundaries in jumps), the
// LOBE clamp truncates the moved region at +-4 bins (a spectral edge rings),
// and where regions nearly touch the moved core carries the neighbour's skirt
// rotated by the wrong phase. The kernel has none of these: measured against
// the same OLA pipeline (scratchpad kernel_check.py), levels exact to 0.00 dB
// where translation erred up to -1.9 dB, residual -53 dB vs -46 dB. It is the
// atom resynthesis rejected for the normal path -- correctly, because a
// kernel DISCARDS the noise the region carries with the partial -- but from
// mid PURIFY up discarding that noise is precisely what the knob is asking
// for. Below the threshold nothing changes.
static constexpr MYFLOAT PITCHMAP_KERNEL_PUR = .5;
static constexpr MYFLOAT PITCHMAP_KERNEL_W = 4.;
// Inharmonicity. A string's partials are STRETCHED: f_h = h*f0*(1 + B*h^2/2),
// and a piano's B (~1e-4..8e-4) puts the partials above h ~ 13 closer to the
// WRONG integer harmonic than the right one -- they were being claimed one or
// two slots high with a small negative deviation, and the harmonic lock then
// rendered them at that wrong slot: the 16th partial at 17*f0, +60 cents off,
// sustained. So each track carries an estimated B: claims are made against
// the STRETCHED comb (harmonic numbers refined iteratively), the f0 vote
// divides the stretch back out, and synthesis renders the stretched comb
// rigidly -- same coherence as before, but the piano keeps sounding like a
// piano instead of a detuned additive synth. The fit uses only POSITIVE
// deviations at h 4..14 (stretch is always sharp; a negative deviation up
// there IS a misassignment) -- with that rule the estimator recovers B to
// three digits across the whole piano range and every harmonic is claimed at
// the correct number (measured, scratchpad session); without it the
// misassigned claims poison the fit and it converges to zero.
static constexpr MYFLOAT PITCHMAP_B_MAX = 8e-4;
static constexpr int PITCHMAP_B_FIT_LO = 4;
static constexpr int PITCHMAP_B_FIT_HI = 14;
// Transient split. A note onset's energy lands inside the HELD notes' claimed
// regions, and moving it with them rotates and transposes the attack -- the
// classic phase-vocoder transient smudge, and on piano the difference between
// a piano and a barrel organ. On a flux-detected transient frame each moving
// region therefore carries only its SUSTAINED part (the magnitude it already
// had last frame), and the onset INCREMENT -- max(0, mag - prevMag) per bin,
// the hammer itself -- passes through at its original position and phase at
// full gain, at every PURIFY. Held voices are untouched: their level did not
// jump, so their increment is ~zero. The deliberate trade: drum onsets also
// ride through high PURIFY as one hop of increment.
//
// The grid lock is likewise capped: a partial renders at most this fraction
// of the way from its measured position onto the harmonic grid, so a sliver
// of the real micro-detune (piano triple-string unisons, choir spread)
// survives in the sustain instead of freezing into a static comb.
static constexpr MYFLOAT PITCHMAP_LOCK_MAX = .85;
// A region no wider than this (bins, midpoint to midpoint) renders as a
// kernel at EVERY purify. Down where harmonics sit ~10 bins apart the region
// IS the mainlobe: translation's differential shift opens gaps into real
// energy on the way up and overlap-collides mainlobes on the way down --
// measured both directions in kernel_check.py (levels exact vs up to -1.8 dB,
// residual ~5 dB cleaner). And with the region only a bin or two wider than
// the kernel, the carried noise the kernel would discard is not there to
// lose. 10 bins is an f0 of ~117 Hz at 48 k: the bass regime, exactly where
// translation geometrically runs out of resolution.
static constexpr int32_t PITCHMAP_KERNEL_NARROW = 10;

// Hann window transform, W(0) = 1, signed: W(x) = sinc(x) / (1 - x^2).
static inline MYFLOAT pitchmapHannK(MYFLOAT x) {
    const MYFLOAT ax = std::abs(x);
    if (ax < 1e-9) return 1.;
    if (std::abs(ax - 1.) < 1e-9) return .5;
    const MYFLOAT px = (TWOPI_P * .5) * x;
    return std::sin(px) / (px * (1. - x * x));
}

// ---------------------------------------------------------------------------
// Channel link. One PitchMap runs per channel, each with its own detector, and
// two detectors on the two sides of a stereo mix WILL disagree -- a note a
// little more masked in one channel picks a different target there, and even
// when both agree the two ratios differ by the measurement noise. Both come
// out as image smear and phasiness. So channel 0 leads: after every frame it
// publishes its per-track (f0, ratio) pairs, and the other channels adopt the
// leader's ratio for any of their own tracks within PITCHMAP_LINK_OCT of a
// published f0. A note only one channel has -- a hard-panned voice -- finds no
// match and keeps its own decision.
//
// Slots are keyed by the TRACK pointer, NOT the track index, which collides
// between plugin instances sharing this static. Claimed in the constructor,
// released in the destructor; a follower trusts a slot only while the leader's
// frame counter stays close to its own (both channels consume the same sample
// stream, so the counters advance in lockstep, and a slot whose leader was
// destroyed stops advancing and is ignored). The channels can run on different
// threads; a follower reading mid-publish gets some ratios one frame older
// than others, which is harmless -- they are 21 ms apart, not wrong.
// ---------------------------------------------------------------------------
static constexpr int PITCHMAP_LINK_SLOTS = 16;
static constexpr int32_t PITCHMAP_LINK_SLACK = 8;
// 60 cents: generous against detune between the channels' estimates, still
// well under the semitone that would confuse two different notes.
static constexpr MYFLOAT PITCHMAP_LINK_OCT = .05;

namespace {
struct PitchMapLink {
    std::atomic<const void *> key{nullptr};
    std::atomic<int32_t> frame{INT32_MIN};
    std::atomic<MYFLOAT> f0[PitchMapMaxSources]{};
    std::atomic<MYFLOAT> ratio[PitchMapMaxSources]{};
};
}
static PitchMapLink g_pitchMapLink[PITCHMAP_LINK_SLOTS];

// Scale definitions, decoded positionally from PITCHMAP_SCALES in gs_common.h.
// UNISON (0) and HARMONIC (10) are not pitch-class sets and are handled
// separately in buildTargets.
static constexpr int PITCHMAP_NSCALES = 11;
static constexpr int8_t PITCHMAP_PC[PITCHMAP_NSCALES][12] = {
        {0},                                  // UNISON     (unused, special)
        {0},                                  // OCTAVES
        {0, 7},                               // FIFTHS
        {0, 4, 7},                            // MAJ TRIAD
        {0, 3, 7},                            // MIN TRIAD
        {0, 2, 4, 5, 7, 9, 11},               // MAJOR
        {0, 2, 3, 5, 7, 8, 10},               // MINOR
        {0, 3, 5, 7, 10},                     // MIN PENT
        {0, 2, 4, 6, 8, 10},                  // WHOLE TONE
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // CHROMATIC
        {0}};                                 // HARMONIC   (unused, special)
static constexpr int PITCHMAP_NPC[PITCHMAP_NSCALES] = {1, 1, 2, 3, 3, 7, 7, 5, 6, 12, 1};

PitchMap::PitchMap(TRACK *t, int32_t chan)
        : Effect(t, chan, SPACE_PITCHMAP, MONOEFFECT),
          fft(PitchMapFFTSize),
          CircularBuffer(PitchMapFFTSize, PitchMapOverlap) {
    auto _appState = t->_appState;
    _bypass = &t->bypass[SPACE_PITCHMAP];
    _cps = &_STATE->params[t->index][PMAPCPS];
    _scale = &_STATE->params[t->index][PMAPSCALE];
    _amt = &_STATE->params[t->index][PMAPAMT];
    _sources = &_STATE->params[t->index][PMAPSOURCES];
    _purify = &_STATE->params[t->index][PMAPPURIFY];
    _formant = &_STATE->params[t->index][PMAPFORMANT];
    _glide = &_STATE->params[t->index][PMAPGLIDE];
    _lo = &_STATE->params[t->index][PMAPLO];
    _hi = &_STATE->params[t->index][PMAPHI];
    _dry = &_STATE->params[t->index][PMAPDRY];
    _wet = &_STATE->params[t->index][PMAPWET];
    _lfo_cps = &t->lfo[PMAPCPS];
    _lfo_amt = &t->lfo[PMAPAMT];

    // Hann on both ends. The FFT class does no windowing of its own, which is
    // why it is used here rather than FFT3: FFT3's pair is Csound's pvoc kernel,
    // and its analysis half is very nearly rectangular -- with -13 dB sidelobes
    // the peak picker would spend the frame finding leakage rather than
    // partials. Hann on analysis and again on synthesis sums to exactly 3/2 at
    // 75% overlap, so the reciprocal goes on once, in the synthesis pass.
    for (int32_t i = 0; i < PitchMapFFTSize; ++i)
        _win[i] = .5 * (1. - std::cos(TWOPI_P * i / (MYFLOAT) PitchMapFFTSize));

    delay.setsize(getDelay());

    std::memset(_prevOwn, -1, sizeof(_prevOwn));

    _linkLeader = chan == 0;
    if (_linkLeader)
        for (int32_t i = 0; i < PITCHMAP_LINK_SLOTS; ++i) {
            const void *expect = nullptr;
            if (g_pitchMapLink[i].key.compare_exchange_strong(expect, t)) {
                _linkSlot = i;
                break;
            }
        }
}

PitchMap::~PitchMap() {
    if (_linkSlot >= 0) {
        g_pitchMapLink[_linkSlot].frame.store(INT32_MIN, std::memory_order_relaxed);
        g_pitchMapLink[_linkSlot].key.store(nullptr, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// f0 search grid, log-spaced at half a semitone. Peaks are scattered onto it by
// index arithmetic rather than compared candidate by candidate: a peak at
// frequency f votes for the candidate at log2(f) - log2(h) for every harmonic
// number h, which turns an O(candidates * peaks) search into O(peaks * 16) and
// takes every log out of the inner loop.
// ---------------------------------------------------------------------------
void PitchMap::buildCandidates(MYFLOAT lo, MYFLOAT hi) {
    _candLo = lo;
    _candHi = hi;
    _log2lo = std::log2(lo);
    _ncand = (int32_t) std::ceil(std::log2(hi / lo) * CELLS_PER_OCT) + 1;
    if (_ncand > MAX_CAND) _ncand = MAX_CAND;
    if (_ncand < 2) _ncand = 2;
    for (int32_t i = 0; i < _ncand; ++i)
        _candF0[i] = lo * std::exp2((MYFLOAT) i / (MYFLOAT) CELLS_PER_OCT);
}

// ---------------------------------------------------------------------------
// The allowed output frequencies. Everything downstream only ever asks for the
// nearest member of this list, which is the seam MIDI plugs into later: held
// notes build the list instead of CPS and SCALE, and nothing else changes.
//
// The pitch-class sets repeat in every octave on purpose. Mapping to the
// nearest step *in any octave* keeps a bassline in the bass and a melody where
// it was; a single-octave target set would fold the whole arrangement into one
// register. UNISON and HARMONIC are the two deliberate exceptions.
// ---------------------------------------------------------------------------
void PitchMap::buildTargets(MYFLOAT cps, int scale) {
    _targetCps = cps;
    _targetScale = scale;
    _ntargets = 0;
    if (scale < 0 || scale >= PITCHMAP_NSCALES) scale = 1;

    const MYFLOAT top = _appState->sr * .45;

    if (scale == 0) {                       // UNISON: one frequency, no octaves
        _targets[_ntargets++] = cps;
        return;
    }
    if (scale == PITCHMAP_NSCALES - 1) {    // HARMONIC: integer multiples of CPS
        for (int n = 1; n <= MAX_TARGETS && _ntargets < MAX_TARGETS; ++n) {
            const MYFLOAT f = cps * n;
            if (f > top) break;
            _targets[_ntargets++] = f;
        }
        if (_ntargets == 0) _targets[_ntargets++] = cps;
        return;
    }

    const int npc = PITCHMAP_NPC[scale];
    const int oct0 = (int) std::floor(std::log2(25. / cps));
    const int oct1 = (int) std::ceil(std::log2(top / cps));
    for (int o = oct0; o <= oct1 && _ntargets < MAX_TARGETS; ++o) {
        const MYFLOAT base = cps * std::exp2((MYFLOAT) o);
        for (int i = 0; i < npc && _ntargets < MAX_TARGETS; ++i) {
            const MYFLOAT f = base * std::exp2(PITCHMAP_PC[scale][i] / 12.);
            if (f < 20. || f > top) continue;
            _targets[_ntargets++] = f;
        }
    }
    if (_ntargets == 0) _targets[_ntargets++] = cps;
}

MYFLOAT PitchMap::nearestTarget(MYFLOAT f) const {
    if (_ntargets == 1) return _targets[0];
    int lo = 0, hi = _ntargets - 1;
    while (lo < hi) {                       // first target >= f
        const int mid = (lo + hi) >> 1;
        if (_targets[mid] < f) lo = mid + 1; else hi = mid;
    }
    if (lo == 0) return _targets[0];
    const MYFLOAT a = _targets[lo - 1], b = _targets[lo];
    // Nearest in pitch, not in hertz: compare the two ratios directly, which is
    // the same comparison as |log(f/a)| vs |log(b/f)| without the logs.
    return (f / a) <= (b / f) ? a : b;
}

// How available peak p is to a note being born. Free peaks count fully; a peak
// held only as another note's high harmonic counts partially; a peak that is
// another note's low harmonic is not available at all.
MYFLOAT PitchMap::peakFree(int32_t p) const {
    if (_peakSrc[p] < 0) return 1.;
    return _peakHrm[p] > VOTE_HARM ? PITCHMAP_WEAK_OWN : 0.;
}

// ---------------------------------------------------------------------------
// Decide, for every peak at once, which live note owns it.
//
// This replaces a greedy first-come pass in which each track in turn swept up
// every peak inside its own claim windows and later tracks saw only what was
// left. Two notes in a musical relationship share partials -- an octave shares
// ALL of them, a fifth shares every second one -- so that pass handed the
// shared partials to whichever track happened to run first, and the run order
// was track age, which in steady state is a tie broken by slot index. Which of
// two notes kept its own harmonics was therefore arbitrary, and it changed
// whenever the ages diverged.
//
// What it sounded like: at the octave the upper note lost even its fundamental,
// failed the root test, retired after TRACK_HOLD frames, and could not be
// rebuilt because the search only looks at unclaimed peaks -- the two notes
// collapsed into one and moved together. At the fifth the upper note kept its
// odd harmonics and lost its even ones, stayed alive on a half comb, and was
// rendered at ITS ratio while the stolen half was rendered at the lower note's
// -- one instrument coming out at two pitches at once. That is the mess, and it
// is why single notes were always clean: with one track there is nothing to
// arbitrate.
//
// The rule is that the lower harmonic number wins. A peak that is one note's
// 2nd harmonic belongs to it far more strongly than it belongs to another
// note's 37th, because a low harmonic is most of what a note IS and a high one
// is one of forty things it happens to contain. Deviation from the exact slot
// only breaks ties at equal harmonic number. This subsumes protecting
// fundamentals as a special case -- h = 1 outranks every other claim there can
// be, so a note's own root can no longer be taken from it by anything.
//
// It does not separate an exact octave, and nothing working from magnitude and
// frequency alone can: the upper note's partials ARE a subset of the lower's,
// and the spectrum holds no evidence of which note put the energy there. What
// the rule guarantees is that the upper note wins every peak it shares (its h
// is half the lower note's throughout), so it keeps its identity and its root
// instead of being erased. The lower note is left with its odd harmonics --
// audibly thinner, but present, tracked, and moving as one piece.
// ---------------------------------------------------------------------------
void PitchMap::arbitrateClaims() {
    for (int32_t p = 0; p < _npeaks; ++p) {
        _peakSrc[p] = -1;
        _peakHrm[p] = 0;
    }
    for (int32_t p = 0; p < _npeaks; ++p) {
        const int32_t kb = _peakBin[p];
        int32_t bestT = -1, bestH = 0;
        bool bestInc = false;
        MYFLOAT bestD = 0.;
        for (int t = 0; t < MAX_SOURCES; ++t) {
            if (!_trkLive[t] || !(_trkF0[t] > 0.)) continue;
            const MYFLOAT h = _peakFrq[p] / _trkF0[t];
            const MYFLOAT B = _trkB[t];
            // Harmonic number against the STRETCHED comb: h/(1 + B*hn^2/2)
            // is only computable once hn is known, so iterate -- three passes
            // settle it across the whole piano range (measured).
            int32_t hn = (int32_t) (h + .5);
            if (B > 0.)
                for (int it = 0; it < 3; ++it) {
                    const int32_t r =
                        (int32_t) (h / (1. + .5 * B * (MYFLOAT) (hn * hn)) + .5);
                    if (r == hn) break;
                    hn = r < 1 ? 1 : r;
                }
            if (hn < 1 || hn > _claimHarm) continue;
            // Incumbency: this track held this bin (+-1) at this harmonic
            // (+-1) last frame. A claim that exists is worth more than a claim
            // that might: without hysteresis, a partial sitting near a window
            // edge -- or contested between two tracks whose scores are nearly
            // equal -- flipped in and out of its claim every frame or two, and
            // at PURIFY 1 an unclaimed frame renders as SILENCE, so the flip
            // is a 21 ms amplitude strobe across the note's partials. That is
            // the low-mid "wobble": the low mids are where claims are contested.
            bool inc = false;
            for (int32_t b = kb - 1; b <= kb + 1 && !inc; ++b)
                if (b >= 1 && b < PitchMapNyq && _prevOwn[b] == t &&
                    std::abs((int32_t) _prevHrm[b] - hn) <= 1)
                    inc = true;
            MYFLOAT tol = .35 / (MYFLOAT) hn;
            if (tol > TOL_RATIO) tol = TOL_RATIO;
            // The window itself gets hysteresis: an incumbent's partial that
            // drifts a shade past the edge stays claimed rather than blinking.
            if (inc) tol *= 1.25;
            const MYFLOAT e = (MYFLOAT) hn * (1. + .5 * B * (MYFLOAT) (hn * hn));
            const MYFLOAT dev = h / e - 1.;
            if (std::abs(dev) > tol) continue;
            // Above the voting harmonics a claim still has to LOOK like a
            // partial -- see PITCHMAP_CLAIM_SIN and the hi-hat case. But the
            // threshold was measured on hats vs CLEAN partials, and a
            // vibrato'd or onset-blurred partial smears to s ~ .6: hard-gating
            // it sent everything above h = 8 down the RESIDUAL path at the
            // ORIGINAL pitch while h 1..8 moved -- one note coming out at two
            // pitches at once, which is most of what "smearing" meant. So the
            // hard gate applies to FIRST-TIME claims only: a peak this track
            // already held at (nearly) this harmonic last frame stays
            // claimable regardless of s. A real partial sits at a stable
            // harmonic slot frame after frame; hat maxima land on fresh bins
            // every frame and have no history to invoke.
            if (hn > VOTE_HARM && _peakSin[p] > PITCHMAP_CLAIM_SIN && !inc)
                continue;
            const MYFLOAT ad = std::abs(dev) / tol;
            // Lower harmonic number still wins outright; at equal harmonic
            // number the incumbent wins -- a contested peak whose two scores
            // are nearly equal must not change hands on measurement noise,
            // because each handover renders it at the OTHER ratio for a frame.
            // Deviation only decides when incumbency ties too.
            if (bestT < 0 || hn < bestH ||
                (hn == bestH && ((inc && !bestInc) ||
                                 (inc == bestInc && ad < bestD)))) {
                bestT = t;
                bestH = hn;
                bestInc = inc;
                bestD = ad;
            }
        }
        if (bestT >= 0) {
            _peakSrc[p] = bestT;
            _peakHrm[p] = bestH;
        }
    }
}

// The claim statistics for one track, read back off the arbitrated assignment.
// Same quantities gatherClaim computes, and the same biweight/sinusoidality
// vote -- the only difference is that the peaks were chosen by arbitration
// rather than by sweeping, so this cannot depend on the order tracks run in.
void PitchMap::scoreClaim(int32_t trk) {
    _claimN = 0;
    _claimMass = 0.;
    _claimRoot = false;
    _claimF0 = 0.;
    _claimB = -1.;
    const MYFLOAT f0 = _trkF0[trk];
    if (!(f0 > 0.)) return;
    const MYFLOAT B = _trkB[trk];

    MYFLOAT num = 0., den = 0., rnum = 0., rden = 0., fnum = 0., fden = 0.;
    for (int32_t p = 0; p < _npeaks; ++p) {
        if (_peakSrc[p] != trk) continue;
        const int32_t hn = _peakHrm[p];
        _claimIdx[_claimN] = p;
        _claimHrm[_claimN] = hn;
        ++_claimN;
        _claimMass += _peakMag[p];
        if (hn <= 2) _claimRoot = true;
        // The stretch fit, over the mid harmonics. dev0 is deviation from the
        // UNSTRETCHED slot -- the raw material the model explains. Stretch is
        // always sharp, so a clearly negative dev0 up here is a misassignment
        // and would poison the fit (see the constant note).
        if (hn >= PITCHMAP_B_FIT_LO && hn <= PITCHMAP_B_FIT_HI) {
            MYFLOAT tol = .35 / (MYFLOAT) hn;
            if (tol > TOL_RATIO) tol = TOL_RATIO;
            const MYFLOAT dev0 = _peakFrq[p] / ((MYFLOAT) hn * f0) - 1.;
            if (dev0 > -.25 * tol) {
                const MYFLOAT sn = _peakSin[p] * (1. / PITCHMAP_VOTE_SIN);
                const MYFLOAT wf = _peakMag[p] / (1. + sn * sn);
                const MYFLOAT x = .5 * (MYFLOAT) (hn * hn);
                fnum += wf * x * dev0;
                fden += wf * x * x;
            }
        }
        if (hn <= VOTE_HARM) {
            MYFLOAT tol = .35 / (MYFLOAT) hn;
            if (tol > TOL_RATIO) tol = TOL_RATIO;
            // The stretch divided back out, so the vote estimates the true f0
            // and the low harmonics agree with the root instead of dragging
            // the mean sharp.
            const MYFLOAT e = (MYFLOAT) hn * (1. + .5 * B * (MYFLOAT) (hn * hn));
            const MYFLOAT dev = _peakFrq[p] / (e * f0) - 1.;
            const MYFLOAT q = dev / tol;
            const MYFLOAT bw = (1. - q * q) * (1. - q * q);
            const MYFLOAT sn = _peakSin[p] * (1. / PITCHMAP_VOTE_SIN);
            const MYFLOAT wgt = _peakMag[p] * bw / (1. + sn * sn);
            num += wgt * _peakFrq[p] / e;
            den += wgt;
            if (hn <= 2) {
                rnum += wgt * _peakFrq[p] / e;
                rden += wgt;
            }
        }
    }
    if (fden > 0.) _claimB = fnum / fden;
    if (den > 0.) _claimF0 = num / den;
    // Same root anchor as gatherClaim -- see the note there.
    if (rden > 0. && _claimF0 > 0.) {
        const MYFLOAT rf = rnum / rden;
        if (rf > 0. && std::abs(std::log2(_claimF0 / rf)) > PITCHMAP_ROOT_ANCHOR_OCT)
            _claimF0 = rf;
    }
}

// ---------------------------------------------------------------------------
// Gather the unassigned peaks that fit f0's harmonic series, without committing
// them. Also computes what the claim says about f0, how much magnitude it
// accounts for, and whether the note's own root is among them.
//
// Still used for a CANDIDATE -- a note that does not exist yet and so took no
// part in arbitration. A candidate may take a peak that is free, and may take
// one held as another note's high harmonic if it needs it as a LOW one, which
// is the same lower-harmonic-wins rule arbitration applies; the peak is
// reassigned properly on the next frame once the candidate is a track.
//
// The 35-cent window is a RELATIVE one, and consecutive harmonics are only 1/h
// apart in relative terms. Above h ~ 24 the window is wider than the spacing
// between harmonics, so every peak matches *some* harmonic number and the f0
// estimate -- a mean over f/h -- can be dragged anywhere. Capping the window at
// a third of the spacing keeps a peak matchable by at most one harmonic.
//
// Only the low harmonics vote on the frequency. A high partial that is claimed
// still moves with the note, but a mis-numbered one at h = 30 would shift the
// mean by a semitone, and since the next frame claims against the moved f0 that
// error compounds -- measured, a 233 Hz tone walked to 215 Hz and stayed there.
// ---------------------------------------------------------------------------
void PitchMap::gatherClaim(MYFLOAT f0) {
    _claimN = 0;
    _claimMass = 0.;
    _claimRoot = false;
    _claimF0 = 0.;
    if (!(f0 > 0.)) return;

    MYFLOAT num = 0., den = 0., rnum = 0., rden = 0.;
    for (int32_t p = 0; p < _npeaks; ++p) {
        const MYFLOAT avail = peakFree(p);
        if (avail <= 0.) continue;
        const MYFLOAT h = _peakFrq[p] / f0;
        const int32_t hn = (int32_t) (h + .5);
        if (hn < 1 || hn > _claimHarm) continue;
        // A weakly-held peak may be taken only as a LOW harmonic -- that is the
        // stronger claim, and it is the same rule arbitration uses. Taking it
        // as another high harmonic would swap one weak owner for another.
        if (avail < 1. && hn > VOTE_HARM) continue;
        MYFLOAT tol = .35 / (MYFLOAT) hn;
        if (tol > TOL_RATIO) tol = TOL_RATIO;
        const MYFLOAT dev = h / (MYFLOAT) hn - 1.;
        if (std::abs(dev) > tol) continue;
        // Above the voting harmonics a claim needs to LOOK like a partial.
        // Hi-hat noise is dense enough in frequency that some of it always
        // lands inside the claim window of a bass or mid note's high
        // harmonics, and a claimed hat peak drags its whole region through
        // PURIFY at full gain. See PITCHMAP_CLAIM_SIN. (A candidate has no
        // claim history, so unlike arbitration there is no continuity rescue
        // here -- a first-time claim meets the full gate.)
        if (hn > VOTE_HARM && _peakSin[p] > PITCHMAP_CLAIM_SIN) continue;

        _claimIdx[_claimN] = p;
        _claimHrm[_claimN] = hn;
        ++_claimN;
        _claimMass += _peakMag[p];
        if (hn <= 2) _claimRoot = true;
        if (hn <= VOTE_HARM) {
            // The f0 vote is a weighted mean, and a weighted mean is only as
            // good as its worst voter: when another instrument onsets, its
            // peaks land inside these windows, out-shout the melody's own
            // partials, and drag the f0 -- which, with harmonic-locked
            // synthesis, bends the WHOLE rendered note until the glide
            // recovers. Two defenses, both smooth: a biweight kernel over the
            // window (a foreign peak sits at a random spot in the window and
            // usually near an edge; our own partial sits at the centre), and
            // the sinusoidality down-weight (see PITCHMAP_VOTE_SIN).
            const MYFLOAT q = dev / tol;
            const MYFLOAT bw = (1. - q * q) * (1. - q * q);
            const MYFLOAT sn = _peakSin[p] * (1. / PITCHMAP_VOTE_SIN);
            const MYFLOAT wgt = _peakMag[p] * bw / (1. + sn * sn);
            num += wgt * _peakFrq[p] / (MYFLOAT) hn;
            den += wgt;
            if (hn <= 2) {
                rnum += wgt * _peakFrq[p] / (MYFLOAT) hn;
                rden += wgt;
            }
        }
    }
    if (den > 0.) _claimF0 = num / den;
    // Root anchor. The biweight and sinusoidality defenses handle a SINGLE
    // frame's contamination, but another note parked inside these windows
    // drags the mean for as long as it sounds -- too long for the render
    // median to hide, and with harmonic-locked synthesis a dragged f0 bends
    // the whole rendered note. The h <= 2 peaks are the one part of the vote
    // a foreign note almost cannot touch (its own energy would have to land
    // inside their 35-cent window), so when the full vote strays more than
    // 15 cents from what the root says, the vote is contaminated and the
    // root wins outright. On a heavily stretched string this trusts the root
    // over the sharp upper partials -- which is also the perceived pitch.
    if (rden > 0. && _claimF0 > 0.) {
        const MYFLOAT rf = rnum / rden;
        if (rf > 0. && std::abs(std::log2(_claimF0 / rf)) > PITCHMAP_ROOT_ANCHOR_OCT)
            _claimF0 = rf;
    }
}

void PitchMap::commitClaim(int32_t trk) {
    for (int32_t i = 0; i < _claimN; ++i) {
        _peakSrc[_claimIdx[i]] = trk;
        _peakHrm[_claimIdx[i]] = _claimHrm[i];
    }
}

// ---------------------------------------------------------------------------
// Note tracking, then multi-f0 for whatever is left over.
//
// **A live note claims its own peaks before the search runs.** This is the
// difference between a pitch mapper and a spectral effect that re-decides
// everything 47 times a second. The first version took the ratio from this
// frame's detection alone, so a frame where the search missed a note dropped
// that note back to its original pitch for 21 ms, and a frame where it re-found
// it re-decided the target from scratch. Measured on a tone over a noise floor,
// the applied ratio flipped by a full 200 cents on 21 of 43 frames -- audible
// as exactly the "jumps in pitch" this is here to avoid. With tracking: zero.
//
// The cost of persistence is that a wrong note is now also persistent, so a
// track has to keep proving itself: enough claimed peaks, enough of the frame's
// magnitude (a lower bar than creation -- it is easier to keep a note than to
// start one), and its own root still present. That last test is what kills a
// track that locked onto a subharmonic, which otherwise claims plenty of peaks
// and defends the wrong f0 indefinitely.
//
// The salience search below only has to find notes that are NEW.
//
// The 1/sqrt(h) harmonic weighting keeps that search off subharmonics without
// any octave heuristic. A candidate an octave below a real f0 explains exactly
// the same peaks, each one harmonic number doubled, so it collects 1/sqrt(2) =
// 0.71 of the salience; a candidate an octave above only sees the even
// harmonics, which is about the same fraction. The true f0 wins on arithmetic.
// ---------------------------------------------------------------------------
void PitchMap::updateTracks(int wanted, MYFLOAT totalMass, bool transient) {
    if (wanted > MAX_SOURCES) wanted = MAX_SOURCES;

    // ---- 1. live notes own their peaks, decided for all of them at once ----
    // The claim width is the solo measurement only while one note is live.
    // Every further note narrows it: reaching across the other notes is exactly
    // what a wide net does, and the wider it is the further it reaches. At two
    // notes this is 20 harmonics, at three 13, and it floors at MIN_CLAIM_HARM.
    int nlive0 = 0;
    for (int i = 0; i < MAX_SOURCES; ++i) {
        if (!_trkLive[i]) continue;
        if (!(_trkF0[i] > 0.)) { _trkLive[i] = false; continue; }
        ++nlive0;
    }
    _claimHarm = nlive0 <= 1 ? CLAIM_HARM : CLAIM_HARM / nlive0;
    if (_claimHarm < MIN_CLAIM_HARM) _claimHarm = MIN_CLAIM_HARM;

    arbitrateClaims();

    // Run order no longer decides anything -- arbitration already did. Age is
    // still what retires a track and what the steal below picks on.
    bool claimed[MAX_SOURCES] = {};         // committed peaks THIS frame
    for (int t = 0; t < MAX_SOURCES; ++t) {
        if (!_trkLive[t]) continue;
        scoreClaim(t);
        // Two different bars. MAPPING the peaks only needs the root to still be
        // there: a note that dips below the mass gate for a frame -- masked by
        // a hit, or just quiet -- used to drop its peaks back to the ORIGINAL
        // pitch for 21 ms and warble between mapped and unmapped. Its peaks
        // keep moving with the track for as long as the note is audibly there.
        // TRUSTING the claim -- letting it vote on f0 and reset the age -- keeps
        // the full gate, so a corpse track still cannot defend itself and
        // retires on schedule.
        const bool solid = _claimN >= 2 && _claimMass >= totalMass * .05 && _claimRoot;
        if (_claimN >= 1 && _claimRoot) {
            claimed[t] = true;
        } else {
            // Arbitration hands peaks out before the root test runs, so a track
            // that turns out to have no root gives them back. Otherwise it sits
            // on material it does not own for the TRACK_HOLD frames it takes to
            // retire, and hides that material from the salience search over
            // exactly the frames a new note needs it.
            for (int32_t p = 0; p < _npeaks; ++p)
                if (_peakSrc[p] == t) {
                    _peakSrc[p] = -1;
                    _peakHrm[p] = 0;
                }
        }
        if (solid) {
            // No f0 update on a transient frame: an onset's energy is all
            // over these windows and even the down-weighted vote is not worth
            // trusting for the 21 ms it takes the spectrum to settle. The
            // note's own pitch does not move in that time; a dragged f0 does.
            if (!transient && _claimF0 > 0.) {
                // A real note does not move 40 cents in 21 ms. Clamping the
                // update is the second half of the anti-runaway: even a claim
                // that went wrong can only nudge the track, and the root test
                // above then retires it rather than letting it walk away.
                MYFLOAT step = std::log2(_claimF0 / _trkF0[t]);
                if (step > MAX_STEP_OCT) step = MAX_STEP_OCT;
                else if (step < -MAX_STEP_OCT) step = -MAX_STEP_OCT;
                if (std::isfinite(step)) _trkF0[t] *= std::exp2(step);
                // Stretch coefficient follows the fit at the same cadence.
                if (_claimB >= 0. && std::isfinite(_claimB)) {
                    MYFLOAT b = _claimB;
                    if (b > PITCHMAP_B_MAX) b = PITCHMAP_B_MAX;
                    _trkB[t] += .25 * (b - _trkB[t]);
                }
            }
            _trkAge[t] = 0;
        } else ++_trkAge[t];
    }

    // ---- 2. salience search, for notes that are new ----
    // Not on a transient frame: a drum hit is broadband, and broadband energy
    // always contains an f0 that "explains" some of it. See PITCHMAP_FLUX_THR.
    while (!transient && _ncand >= 2) {
        int nlive = 0, stale = 0;
        for (int i = 0; i < MAX_SOURCES; ++i)
            if (_trkLive[i]) {
                ++nlive;
                if (!claimed[i] && _trkAge[i] >= PITCHMAP_STEAL_AGE) ++stale;
            }
        // Room for a note either as a free slot under the SOURCES budget or by
        // stealing a stale track (see below). No room, no search.
        if (nlive >= wanted && stale == 0) break;
        // Unexplained mass, counting a peak held only as some other note's high
        // harmonic at PITCHMAP_WEAK_OWN. It appears in the salience sum at the
        // same weight, so the creation gate below stays proportionate.
        MYFLOAT rest = 0.;
        int32_t nrest = 0;
        for (int32_t p = 0; p < _npeaks; ++p) {
            const MYFLOAT av = peakFree(p);
            if (av <= 0.) continue;
            rest += _peakMag[p] * av;
            ++nrest;
        }
        if (nrest < 2) break;

        std::memset(_sal, 0, _ncand * sizeof(MYFLOAT));
        std::memset(_hits, 0, _ncand * sizeof(MYFLOAT));
        for (int32_t p = 0; p < _npeaks; ++p) {
            const MYFLOAT av = peakFree(p);
            if (av <= 0.) continue;
            const MYFLOAT lp = _peakLog2[p];
            for (int h = 1; h <= MAX_HARM; ++h) {
                const MYFLOAT x = (lp - LOG2H[h] - _log2lo) * CELLS_PER_OCT;
                if (x < -.5 || x > (MYFLOAT) _ncand - .5) continue;
                const int i0 = (int) std::floor(x);
                const MYFLOAT fr = x - (MYFLOAT) i0;
                const MYFLOAT w = _peakMag[p] * av * INV_SQRT_H[h];
                if (i0 >= 0) {
                    _sal[i0] += w * (1. - fr);
                    _hits[i0] += 1. - fr;
                }
                if (i0 + 1 < _ncand) {
                    _sal[i0 + 1] += w * fr;
                    _hits[i0 + 1] += fr;
                }
            }
        }

        int best = -1;
        MYFLOAT bestSal = 0.;
        // Two partials minimum: one peak on its own is a candidate for every
        // subharmonic of itself and carries no evidence of a pitch.
        for (int32_t c = 0; c < _ncand; ++c)
            if (_hits[c] >= 1.5 && _sal[c] > bestSal) {
                bestSal = _sal[c];
                best = c;
            }
        // A new note has to account for a real share of what is still
        // unexplained. Without this, noise always yields a "note": some
        // candidate always collects two accidental hits, its region then gets
        // shifted, and holes appear in a spectrum that had no pitch in it.
        if (best < 0 || bestSal < rest * .10) break;

        // Virtual-fundamental guard, applied ONCE, here at creation -- it is a
        // discrete decision, and re-taking it every frame would flip octaves.
        // A major triad is 4:5:6 of a note two octaves below it, so the greatest
        // common fundamental explains more peaks than any of the notes actually
        // played and wins the argmax: a C major triad resolves to one C two
        // octaves down and the three notes then move together instead of being
        // mapped apart. If nothing is playing at f0 itself and the octave above
        // explains nearly as much, the octave above is the note. A bass note
        // whose fundamental is merely weak still has *a* peak there.
        for (int guard = 0; guard < 3; ++guard) {
            const MYFLOAT f0t = _candF0[best];
            bool has1 = false;
            for (int32_t p = 0; p < _npeaks && !has1; ++p)
                if (peakFree(p) > 0. && std::abs(_peakFrq[p] / f0t - 1.) < TOL_RATIO)
                    has1 = true;
            if (has1) break;
            const int up = best + CELLS_PER_OCT;
            if (up >= _ncand || _sal[up] < _sal[best] * .5) break;
            best = up;
        }

        MYFLOAT f0 = _candF0[best];
        if (best > 0 && best < _ncand - 1) {
            const MYFLOAT a = _sal[best - 1], b = _sal[best], c2 = _sal[best + 1];
            const MYFLOAT den = a - 2. * b + c2;
            MYFLOAT d = den != 0. ? .5 * (a - c2) / den : 0.;
            if (d > .5) d = .5; else if (d < -.5) d = -.5;
            if (std::isfinite(d)) f0 *= std::exp2(d / (MYFLOAT) CELLS_PER_OCT);
        }

        int free = -1;
        if (nlive < wanted) {
            // Prefer the most recently vacated slot: if this candidate is the
            // slot's old note moved a step, the handoff below can pick up its
            // ratio.
            for (int i = 0; i < MAX_SOURCES; ++i)
                if (!_trkLive[i] && (free < 0 || _trkDied[i] > _trkDied[free]))
                    free = i;
        } else {
            // Steal. On a note change the old note's track claims nothing but
            // sits on its slot for TRACK_HOLD frames -- and with SOURCES set
            // low, that is 130 ms of the NEW note passing unmapped and then
            // snapping in late, which is what made fast melodies chaotic. A
            // candidate that passed every creation gate may take over the
            // stalest track that committed no peaks this frame; a track still
            // claiming -- even weakly -- is never stolen, so a briefly masked
            // chord voice keeps its slot.
            for (int i = 0; i < MAX_SOURCES; ++i)
                if (_trkLive[i] && !claimed[i] && _trkAge[i] >= PITCHMAP_STEAL_AGE &&
                    (free < 0 || _trkAge[i] > _trkAge[free]))
                    free = i;
        }
        if (free < 0) break;

        gatherClaim(f0);
        // _claimF0 is a mean over the low harmonics only, so it is zero when the
        // candidate claimed nothing below h = 8. A note has to be anchored by a
        // low harmonic; without this the track is born with f0 = 0 and every
        // later claim divides by it.
        if (_claimN < 2 || !(_claimF0 > 0.)) break;
        // Portamento handoff -- see PITCHMAP_HANDOFF_FRAMES. Read the slot's
        // old state BEFORE overwriting it: a steal is a death happening right
        // now, a free slot carries the frame its occupant retired on.
        MYFLOAT inherit = 0.;
        {
            const int32_t died = _trkLive[free] ? _frameNo : _trkDied[free];
            if (_trkRatio[free] > 0. && std::isfinite(_trkRatio[free]) &&
                _trkF0[free] > 0. && _frameNo - died <= PITCHMAP_HANDOFF_FRAMES &&
                std::abs(std::log2(_claimF0 / _trkF0[free])) < PITCHMAP_HANDOFF_OCT)
                inherit = _trkRatio[free];
        }
        commitClaim(free);
        _trkLive[free] = true;
        _trkF0[free] = _claimF0;
        // The median history starts at the creation f0, not at whatever the
        // slot's previous note left behind.
        _trkF0m1[free] = _trkF0m2[free] = _claimF0;
        _trkB[free] = 0.;       // stretch is learned, not inherited
        _trkDec[free] = _claimF0;
        _trkTarget[free] = 0.;
        _trkRatio[free] = inherit;  // 0 = sentinel: first mapped frame lands
                                // exactly, no glide up from unity (that would
                                // scoop); inherited = glide from the previous
                                // note's mapping to this note's target
        _trkAge[free] = 0;
        claimed[free] = true;   // the loop head recounts live and stale
    }

    // ---- 3. retire ----
    for (int i = 0; i < MAX_SOURCES; ++i)
        if (_trkLive[i] && _trkAge[i] > PITCHMAP_TRACK_HOLD) {
            _trkLive[i] = false;
            _trkDied[i] = _frameNo;     // the handoff window starts here
        }

    // ---- 3.5 ghost bridge ----
    // A peak that fails its claim for a single frame -- gate flutter, a
    // beating neighbour stealing its local maximum for one hop -- otherwise
    // renders that frame as RESIDUAL: silence at PURIFY 1, the original pitch
    // at PURIFY 0. Either way the partial strobes at the hop rate. So an
    // unclaimed peak whose bin (+-1) was claimed last frame by a still-live
    // track keeps rendering with that owner for this one frame -- synthesis
    // only: it is excluded from the claim map below, so a ghost cannot chain
    // into a second frame without the track genuinely reclaiming the peak,
    // and it takes no part in votes, mass gates, or the salience search.
    for (int32_t p = 0; p < _npeaks; ++p) {
        _peakGhost[p] = -1;
        if (_peakSrc[p] >= 0) continue;
        const int32_t kb = _peakBin[p];
        for (int32_t b = kb - 1; b <= kb + 1; ++b) {
            if (b < 1 || b >= PitchMapNyq) continue;
            const int32_t t = _prevOwn[b];
            if (t >= 0 && _trkLive[t] && _trkRatio[t] > 0.) {
                _peakGhost[p] = t;
                _peakGhostHrm[p] = _prevHrm[b];
                break;
            }
        }
    }

    // ---- 4. remember the claim map ----
    // Next frame's arbitration consults this for the continuity rescue.
    // Rebuilt from the FINAL assignment, so a stolen slot's map is already
    // the new note's claims and the old note cannot be continued by accident.
    std::memset(_prevOwn, -1, sizeof(_prevOwn));
    std::memset(_prevHrm, 0, sizeof(_prevHrm));
    for (int32_t p = 0; p < _npeaks; ++p)
        if (_peakSrc[p] >= 0) {
            _prevOwn[_peakBin[p]] = (int8_t) _peakSrc[p];
            _prevHrm[_peakBin[p]] = (int8_t) _peakHrm[p];
        }
}

void PitchMap::onBufferReady(MYFLOAT *buf, int32_t size) {
    auto _appState = this->_appState;
    const MYFLOAT sr = _STATE->sr;
    const MYFLOAT binHz = sr / (MYFLOAT) PitchMapFFTSize;
    const MYFLOAT nyqHz = sr * .5;

    // ---- parameters, once per frame -------------------------------------
    // CPS is stored as 20*log10(f) and the LFO interpolates in that stored
    // domain, so a swept root moves linearly in pitch and the mapping plays a
    // melody rather than crawling through the bottom octave. Same storage and
    // reasoning as PVSNAPROOT.
    MYFLOAT cpsStored = _cps->load();
    LFO *lfoC = _lfo_cps->load();
    if (lfoC && lfoC->power()) {
        const auto a = _STATE->controls[_track->index][PMAPCPS].lfo_min.load();
        const auto b = _STATE->controls[_track->index][PMAPCPS].lfo_max.load();
        cpsStored = a + lfoC->buf[_lfoIdx] * (b - a);
    }
    const MYFLOAT cps = std::pow(10., cpsStored * .05);

    MYFLOAT amt = _amt->load();
    LFO *lfoA = _lfo_amt->load();
    if (lfoA && lfoA->power()) {
        const auto a = _STATE->controls[_track->index][PMAPAMT].lfo_min.load();
        const auto b = _STATE->controls[_track->index][PMAPAMT].lfo_max.load();
        amt = a + lfoA->buf[_lfoIdx] * (b - a);
    }
    if (amt < 0.) amt = 0.; else if (amt > 1.) amt = 1.;

    const int scale = Effect::limit((int) _scale->load(), 0, PITCHMAP_NSCALES - 1);
    const int wanted = Effect::limit((int) _sources->load(), 1, MAX_SOURCES);
    MYFLOAT purify = Effect::limit((MYFLOAT) _purify->load(), 0., 1.);
    const MYFLOAT fa = Effect::limit((MYFLOAT) _formant->load(), 0., 1.);
    const MYFLOAT glide = Effect::limit((MYFLOAT) _glide->load(), 0., 1.);
    MYFLOAT lo = std::pow(10., _lo->load() * .05);
    MYFLOAT hi = std::pow(10., _hi->load() * .05);
    if (lo < 20.) lo = 20.;
    if (hi > nyqHz * .5) hi = nyqHz * .5;
    if (hi < lo * 1.2) hi = lo * 1.2;

    if (lo != _candLo || hi != _candHi) buildCandidates(lo, hi);
    if (cps != _targetCps || scale != _targetScale) buildTargets(cps, scale);

    // ---- 1. analysis -----------------------------------------------------
    for (int32_t i = 0; i < PitchMapFFTSize; ++i) buf[i] *= _win[i];
    fft.forward(buf, _spec);

    // Phase advance one hop of a bin-centre sinusoid. What the measured advance
    // exceeds this by, unwrapped, is how far the partial actually sits from the
    // bin centre -- and that is the whole frequency estimate.
    const MYFLOAT expected = TWOPI_P * (MYFLOAT) PitchMapHop / (MYFLOAT) PitchMapFFTSize;
    const MYFLOAT devToHz = sr / (TWOPI_P * (MYFLOAT) PitchMapHop);

    MYFLOAT frameMax = 0., flux = 0., magSum = 0.;
    _mag[0] = _frq[0] = 0.;
    for (int32_t k = 1; k < PitchMapNyq; ++k) {
        const MYFLOAT re = _spec[k * 2], im = _spec[k * 2 + 1];
        const MYFLOAT m = std::sqrt(re * re + im * im);
        _mag[k] = m;
        if (m > frameMax) frameMax = m;
        magSum += m;
        const MYFLOAT dm = m - _prevMag[k];
        if (dm > 0.) flux += dm;
        _inc[k] = dm > 0. ? dm : 0.;
        _prevMag[k] = m;
        const MYFLOAT phi = std::atan2(im, re);
        const MYFLOAT dev = princarg(phi - _prevPhi[k] - expected * (MYFLOAT) k);
        _prevPhi[k] = phi;
        MYFLOAT f = (MYFLOAT) k * binHz + dev * devToHz;
        if (f < 0.) f = 0.;
        _frq[k] = f;
    }
    const bool transient = magSum > 0. && flux > magSum * PITCHMAP_FLUX_THR;
    ++_frameNo;

    // ---- 2. peaks --------------------------------------------------------
    // -60 dB below the frame peak: below that a local maximum is the window's
    // own sidelobe skirt or noise, and feeding it to the salience search only
    // adds votes for pitches nobody plays.
    _npeaks = 0;
    const MYFLOAT peakThr = frameMax * .001;
    for (int32_t k = 2; k < PitchMapNyq - 2 && _npeaks < MAX_PEAKS;) {
        const MYFLOAT m = _mag[k];
        if (m > peakThr && m > _mag[k - 1] && m > _mag[k - 2] &&
            m > _mag[k + 1] && m > _mag[k + 2]) {
            const MYFLOAT f = _frq[k];
            if (f > 10.) {
                _peakBin[_npeaks] = k;
                _peakMag[_npeaks] = m;
                _peakFrq[_npeaks] = f;
                _peakLog2[_npeaks] = std::log2(f);
                // Sinusoidality: how far the neighbouring bins' frequency
                // estimates disagree with the peak's, in bins. A resolved
                // partial's mainlobe is a plateau (0.00 measured, even in a
                // mix); noise scatters (~1). See PITCHMAP_CLAIM_SIN.
                const MYFLOAT d1 = std::abs(_frq[k - 1] - f);
                const MYFLOAT d2 = std::abs(_frq[k + 1] - f);
                _peakSin[_npeaks] = (d1 > d2 ? d1 : d2) / binHz;
                _peakSrc[_npeaks] = -1;
                ++_npeaks;
            }
            k += 3;
        } else ++k;
    }

    // ---- 3./4. track the notes ------------------------------------------
    // The first frames have no previous frame to difference against, and the
    // input buffer is still filling, so their frequency estimates are noise.
    // The old per-frame design shrugged that off; a tracker cannot -- a note
    // created from a garbage frame is then claimed and defended indefinitely.
    // Measured: a clean 233 Hz tone locked to 109.9 Hz on frame 0 and held it.
    if (_warm <= PitchMapOverlap) {
        ++_warm;
        for (int32_t k = 1; k < PitchMapNyq; ++k) {
            _spec[k * 2] = _mag[k] * std::cos(_prevPhi[k]);
            _spec[k * 2 + 1] = _mag[k] * std::sin(_prevPhi[k]);
        }
        _spec[0] = _spec[1] = 0.;
        fft.backward(_spec, buf);
        for (int32_t i = 0; i < PitchMapFFTSize; ++i) {
            buf[i] *= _win[i] * PITCHMAP_OLA_GAIN;
            UDD(buf[i])
        }
        return;
    }

    MYFLOAT totalMass = 0.;
    for (int32_t p = 0; p < _npeaks; ++p) totalMass += _peakMag[p];
    updateTracks(wanted, totalMass, transient);

    // GLIDE is the weight on history in the frequency a note's *target* is
    // decided from. The ratio is always built from the frequency measured this
    // frame, so the partials land exactly on the target even while the decision
    // frequency is still catching up -- smoothing is for stability of the
    // decision, never for the size of the shift.
    const MYFLOAT follow = 1. - glide * .9;

    // The rendered pitch below is f0 x ratio, and only the ratio is smoothed.
    // At GLIDE 0 (follow 1) the two f0 terms cancel exactly -- but at any
    // other setting a share of each frame's f0 measurement error reaches the
    // OUTPUT PITCH instantly and is only corrected at the glide rate: a vote
    // nudged by a neighbouring onset became an audible wobble on a note that
    // was just sitting there, and at the 20th harmonic a 5-cent flutter is a
    // whole bin of region movement per frame, i.e. smear up top. So the
    // decision and render paths read a 3-point median of the track's f0:
    // single-frame outliers vanish (at one frame of extra decision latency),
    // while real vibrato is smooth at this hop and passes through. Tracking
    // itself still runs on the raw _trkF0 -- claiming must follow the note,
    // not a delayed copy of it.
    MYFLOAT renderF0[MAX_SOURCES];
    for (int t = 0; t < MAX_SOURCES; ++t) renderF0[t] = _trkF0[t];

    for (int t = 0; t < MAX_SOURCES; ++t) {
        if (!_trkLive[t] || !(_trkF0[t] > 0.)) continue;
        const MYFLOAT f0a = _trkF0[t], f0b = _trkF0m1[t], f0c = _trkF0m2[t];
        MYFLOAT fmed = std::max(std::min(f0a, f0b),
                                std::min(std::max(f0a, f0b), f0c));
        if (!(fmed > 0.) || !std::isfinite(fmed)) fmed = f0a;
        _trkF0m2[t] = f0b;
        _trkF0m1[t] = f0a;
        renderF0[t] = fmed;
        if (_trkDec[t] > 0.) {
            const MYFLOAT step = std::log2(fmed / _trkDec[t]);
            if (std::isfinite(step)) _trkDec[t] *= std::exp2(follow * step);
        } else _trkDec[t] = fmed;

        const MYFLOAT tgt = nearestTarget(_trkDec[t]);
        if (!(_trkTarget[t] > 0.)) _trkTarget[t] = tgt;
        else if (tgt != _trkTarget[t]) {
            // A note sitting between two scale steps would otherwise flip on
            // every frame -- 233 Hz is exactly half way between A and B, and
            // measurement noise alone flipped the applied ratio by 200 cents on
            // half the frames. The new step has to be clearly closer, not just
            // closer.
            const MYFLOAT dNew = std::abs(std::log2(_trkDec[t] / tgt));
            const MYFLOAT dOld = std::abs(std::log2(_trkDec[t] / _trkTarget[t]));
            if (dNew < dOld - PITCHMAP_HYST_OCT) _trkTarget[t] = tgt;
        }

        // The same fmed the render path uses, so the noise terms cancel at
        // every GLIDE setting, not only at 0.
        MYFLOAT r = _trkTarget[t] / fmed;
        if (!(r > 0.) || !std::isfinite(r)) r = 1.;
        // AMOUNT interpolates in the log domain: half way is half the interval,
        // not half the hertz.
        if (amt < 1.) r = std::exp2(amt * std::log2(r));
        // GLIDE is a retune speed, applied to the ratio itself. Instantaneous
        // r = target / measured-f0 puts the output at EXACTLY the target every
        // frame, which flattens vibrato dead and makes a target change a full
        // scale-step jump inside one 21 ms hop -- the "hard autotune" sound.
        // Smoothing r in the log domain fixes both at once: the inverse-vibrato
        // wiggle in r is averaged away so the player's vibrato survives around
        // the target, and a target change becomes a glide instead of a snap.
        // At GLIDE 0 follow is 1 and this reduces to the old exact lock. The
        // first mapped frame is seeded exact (see the creation sentinel) so a
        // new note lands ON pitch rather than scooping up from unity.
        if (!(_trkRatio[t] > 0.) || !std::isfinite(_trkRatio[t]))
            _trkRatio[t] = r;
        else {
            const MYFLOAT rstep = std::log2(r / _trkRatio[t]);
            if (std::isfinite(rstep)) _trkRatio[t] *= std::exp2(follow * rstep);
        }
    }

    // ---- channel link -----------------------------------------------------
    // Leader publishes, followers adopt the leader's (f0, ratio) for any of
    // their own tracks near a published f0. Adopting the f0 as well as the
    // ratio matters because synthesis is harmonic-locked below: with both
    // adopted, the two channels render a shared note's partials at IDENTICAL
    // frequencies, so the stereo image of a shifted note holds still instead
    // of slowly rotating. The adopted f0 only feeds synthesis -- the track's
    // own _trkF0 keeps driving claiming and tracking, so a follower whose
    // leader disappears just falls back to its own estimate.
    if (_linkSlot >= 0) {
        auto &L = g_pitchMapLink[_linkSlot];
        for (int t = 0; t < MAX_SOURCES; ++t) {
            const bool ok = _trkLive[t] && _trkF0[t] > 0. && _trkRatio[t] > 0.;
            // Publish the RENDER (median) f0: followers adopt it as their own
            // render frequency, and both channels must render the identical
            // value or the image rotates.
            L.f0[t].store(ok ? renderF0[t] : 0., std::memory_order_relaxed);
            L.ratio[t].store(ok ? _trkRatio[t] : 1., std::memory_order_relaxed);
        }
        L.frame.store(_frameNo, std::memory_order_release);
    } else if (!_linkLeader) {
        for (int32_t s = 0; s < PITCHMAP_LINK_SLOTS; ++s) {
            auto &L = g_pitchMapLink[s];
            if (L.key.load(std::memory_order_acquire) != (const void *) _track)
                continue;
            const int32_t lf = L.frame.load(std::memory_order_acquire);
            if (lf == INT32_MIN || std::abs(lf - _frameNo) > PITCHMAP_LINK_SLACK)
                continue;   // leader gone or resetting; keep own decisions
            for (int t = 0; t < MAX_SOURCES; ++t) {
                if (!_trkLive[t] || !(_trkF0[t] > 0.) || !(_trkRatio[t] > 0.))
                    continue;
                int best = -1;
                MYFLOAT bestD = PITCHMAP_LINK_OCT;
                for (int i = 0; i < MAX_SOURCES; ++i) {
                    const MYFLOAT lf0 = L.f0[i].load(std::memory_order_relaxed);
                    if (!(lf0 > 0.)) continue;
                    const MYFLOAT d = std::abs(std::log2(lf0 / _trkF0[t]));
                    if (d < bestD) {
                        bestD = d;
                        best = i;
                    }
                }
                if (best >= 0) {
                    const MYFLOAT lr = L.ratio[best].load(std::memory_order_relaxed);
                    const MYFLOAT lf0 = L.f0[best].load(std::memory_order_relaxed);
                    if (lr > 0. && std::isfinite(lr)) _trkRatio[t] = lr;
                    if (lf0 > 0. && std::isfinite(lf0)) renderF0[t] = lf0;
                }
            }
            break;
        }
    }

    // In-scale notes pass through untouched; just-off-scale notes ease onto
    // the harmonic grid. Computed once per track, used per peak in synthesis:
    // trkIdent picks the bit-exact path, trkLock is the grid morph weight --
    // 0 at the identity gate (render at the measured position), 1 from
    // PITCHMAP_BLEND_OCT up (render on the grid), smoothstepped between.
    bool trkIdent[MAX_SOURCES];
    MYFLOAT trkLock[MAX_SOURCES];
    for (int t = 0; t < MAX_SOURCES; ++t) {
        MYFLOAT d = _trkLive[t] && _trkRatio[t] > 0.
                    ? std::abs(std::log2(_trkRatio[t])) : 1.;
        if (!std::isfinite(d)) d = 1.;
        trkIdent[t] = _trkLive[t] && _trkRatio[t] > 0. && d < PITCHMAP_IDENT_OCT;
        MYFLOAT w = (d - PITCHMAP_IDENT_OCT)
                    / (PITCHMAP_BLEND_OCT - PITCHMAP_IDENT_OCT);
        if (w < 0.) w = 0.; else if (w > 1.) w = 1.;
        trkLock[t] = PITCHMAP_LOCK_MAX * w * w * (3. - 2. * w);
    }

    // Two notes mapped onto the SAME target render two combs a few cents
    // apart -- each is target/its-own-f0 with its own noise and vibrato --
    // and they beat against each other: a slow wobble that PURIFY lays bare
    // by stripping the residual that used to mask it. When both tracks are
    // fully grid-locked (and AMT is 1, so the render pitch IS the target),
    // the later track adopts the earlier one's render base, making the two
    // combs frequency-identical: they fuse into one note instead of beating.
    // In the blend zone or at partial AMT the render pitches genuinely
    // differ, so nothing is fused there.
    if (amt >= 1.)
        for (int a = 0; a < MAX_SOURCES; ++a) {
            if (!_trkLive[a] || !(_trkRatio[a] > 0.) ||
                trkLock[a] < PITCHMAP_LOCK_MAX) continue;
            for (int b = a + 1; b < MAX_SOURCES; ++b) {
                if (!_trkLive[b] || !(_trkRatio[b] > 0.) ||
                    trkLock[b] < PITCHMAP_LOCK_MAX) continue;
                if (_trkTarget[b] == _trkTarget[a])
                    renderF0[b] = renderF0[a] * _trkRatio[a] / _trkRatio[b];
            }
        }

    // ---- spectral envelope, for FORMANT ----------------------------------
    // A box mean of the log magnitude. Moving a partial from k to j and scaling
    // it by env(j)/env(k) leaves the envelope where it was, so the source keeps
    // its formants and its character instead of being transposed bodily.
    if (fa > 0.) {
        for (int32_t k = 0; k <= PitchMapNyq; ++k)
            _logm[k] = std::log(_mag[k] + 1e-12);
        const int32_t L = (int32_t) (PITCHMAP_ENV_HZ / binHz);
        MYFLOAT run = 0.;
        int32_t cnt = 0;
        for (int32_t k = 0; k <= L && k <= PitchMapNyq; ++k) { run += _logm[k]; ++cnt; }
        for (int32_t k = 0; k <= PitchMapNyq; ++k) {
            _env[k] = run / (MYFLOAT) cnt;
            const int32_t add = k + L + 1, drop = k - L;
            if (add <= PitchMapNyq) { run += _logm[add]; ++cnt; }
            if (drop >= 0) { run -= _logm[drop]; --cnt; }
        }
    }

    // ---- 5./6. map and resynthesise --------------------------------------
    // Each peak's region -- the bins out to the midpoint between it and its
    // neighbours -- is translated rigidly and rotated as a unit. The region is
    // the analysis window's mainlobe, and it has to arrive intact: mapping each
    // bin individually to the bin nearest its own frequency estimate collapses
    // the whole mainlobe into one bin, which sums magnitudes that were never
    // meant to add (measured +2.2 dB on tonal material, and correct on noise,
    // so it is a level error that follows the material) and smears the partial
    // on the way back out.
    //
    // Phase locking is the other half. The peak's output phase is integrated at
    // the peak's *new* frequency, which is what makes consecutive frames add
    // coherently after a shift, and every other bin in the region is rotated by
    // the same amount -- so the phase relationships inside the mainlobe, which
    // are what make it a mainlobe, survive the move. The accumulator is indexed
    // by the peak's *input* bin: a held note stays in the same input bin from
    // frame to frame, while the bin it is written to moves with the ratio.
    const MYFLOAT resGain = 1. - purify;
    const MYFLOAT phInc = TWOPI_P * (MYFLOAT) PitchMapHop / sr;
    std::memset(_spec, 0, PitchMapFFTSize * sizeof(MYFLOAT));
    std::memset(_outMag, 0, sizeof(_outMag));

    if (_npeaks == 0) {
        // Nothing resolved as a peak: the whole frame is residual, so it passes
        // as it came in, at the residual gain -- PURIFY applies here too, or a
        // peakless noise frame would pop back to full level.
        if (resGain > 0.)
            for (int32_t k = 1; k < PitchMapNyq; ++k) {
                _spec[k * 2] = resGain * _mag[k] * std::cos(_prevPhi[k]);
                _spec[k * 2 + 1] = resGain * _mag[k] * std::sin(_prevPhi[k]);
            }
    }

    for (int32_t p = 0; p < _npeaks; ++p) {
        int32_t src = _peakSrc[p];
        int32_t hrm = _peakHrm[p];
        // One-frame dropout: keep rendering with last frame's owner.
        if (src < 0 && _peakGhost[p] >= 0) {
            src = _peakGhost[p];
            hrm = _peakGhostHrm[p];
        }
        const int32_t kp = _peakBin[p];
        int32_t shift = 0;
        MYFLOAT rot = 0.;
        MYFLOAT ft = 0., ph = 0.;       // set on the shifted-claim path only
        if (src >= 0 && trkIdent[src]) {
            // The note is already on target. Pass its regions bit-exact --
            // shift 0, rotation 0 -- and resync the accumulator to the true
            // phase, so the drift is zeroed for the moment the ratio departs
            // from 1 again. Without this, in-scale notes (most of a song in
            // its own key) random-walk in phase against their own skirts and
            // against the other channel: the residual phasiness of v3.
            _sumPhi[kp] = _prevPhi[kp];
            _phiFrame[kp] = _frameNo;
        } else if (src >= 0) {
            // Harmonic-locked: the partial renders at its exact harmonic slot
            // on the note's output pitch, NOT at its own measured frequency
            // times the ratio. Each partial's phase-derived estimate carries
            // its own per-frame noise, and integrating every partial's phase
            // on its own noisy estimate lets the harmonics of one note random-
            // walk apart -- the waveform never repeats and the note sounds
            // phasey even in mono. hn * f0 moves the whole stack as one rigid
            // comb: f0's noise moves every partial COHERENTLY (vibrato-like,
            // not phasey), and what noise survives the ratio glide mostly
            // cancels since ratio = target / smoothed-f0. The price is that a
            // partial's real deviation from the grid (up to the 35-cent claim
            // window -- string inharmonicity, detuned unison stacks) is
            // flattened onto it: a "purified" rendition, which is the point
            // of this effect. Flattened GRADUALLY, though: trkLock morphs the
            // render position from the measured frequency onto the grid slot
            // across the blend zone, so a note easing away from the identity
            // gate does not snap its partials by their grid deviation all at
            // once -- that snap was audible as a shimmer.
            const MYFLOAT w = trkLock[src];
            // The stretched slot, not the exact-integer one: the whole
            // stretched comb moves rigidly, and the piano keeps its stretch.
            ft = (MYFLOAT) hrm * renderF0[src]
                 * (1. + .5 * _trkB[src] * (MYFLOAT) (hrm * hrm));
            if (w < 1.)
                ft = std::exp2(_peakLog2[p] + w * (std::log2(ft) - _peakLog2[p]));
            ft *= _trkRatio[src];
            if (!(ft > 0.) || ft >= nyqHz) continue;    // moved out of the band
            shift = (int32_t) (ft / binHz + .5) - kp;
            // The accumulator is only continuous if it integrated LAST frame.
            // At a note onset it holds whatever was left there minutes ago, and
            // rotating the attack by garbage is a smeared onset; and a partial
            // wobbling across a bin boundary moves kp by one, landing on a
            // neighbour's equally stale slot every few frames -- audible as
            // phasiness on perfectly steady notes. So: continue from this bin
            // if it ran last frame, else from a neighbour that did (the wobble
            // case), else seed output phase = input phase, which makes the
            // first mapped frame of a region phase-exact with the input and
            // hands the attack through intact.
            int32_t ks = -1;
            if (_phiFrame[kp] == _frameNo - 1) ks = kp;
            else if (kp > 1 && _phiFrame[kp - 1] == _frameNo - 1) ks = kp - 1;
            else if (kp < PitchMapNyq - 1 && _phiFrame[kp + 1] == _frameNo - 1) ks = kp + 1;
            ph = ks < 0 ? _prevPhi[kp]
                        : std::fmod(_sumPhi[ks] + phInc * ft, TWOPI_P);
            _sumPhi[kp] = ph;
            _phiFrame[kp] = _frameNo;
            rot = ph - _prevPhi[kp];
        }
        // A residual peak is not a note: it neither moves nor is re-phased. Its
        // phase-derived frequency is meaningless -- noise scatters over the
        // whole +-2 bin unambiguous range -- so shifting its region by that
        // estimate would punch holes in a spectrum that had no pitch in it.
        const MYFLOAT gain = src >= 0 ? 1. : resGain;
        if (gain <= 0.) continue;

        int32_t b1 = p == 0 ? 1 : ((_peakBin[p - 1] + kp) >> 1) + 1;
        int32_t b2 = p == _npeaks - 1 ? PitchMapNyq
                                      : ((_peakBin[p + 1] + kp) >> 1) + 1;
        if (b1 < 1) b1 = 1;
        if (b2 > PitchMapNyq) b2 = PitchMapNyq;

        // Kernel mode -- see PITCHMAP_KERNEL_PUR and PITCHMAP_KERNEL_NARROW.
        // Only a claimed peak actually being shifted has ft set; identity and
        // residual paths keep their bit-exact region behaviour at every PURIFY.
        const bool kern = ft > 0. && (purify >= PITCHMAP_KERNEL_PUR ||
                                      b2 - b1 <= PITCHMAP_KERNEL_NARROW);

        for (int32_t i = b1; i < b2; ++i) {
            // Only the core of the region moves and rotates; the floor it
            // carries beyond that stays put with its own phase, exactly like
            // residual. A no-op on the identity and residual paths, where
            // shift and rot are already zero. See PITCHMAP_SHIFT_W.
            const bool core = i >= kp - PITCHMAP_SHIFT_W && i <= kp + PITCHMAP_SHIFT_W;
            // In kernel mode the core's content is not translated at all: the
            // partial is resynthesized below and the noise the core carries is
            // discarded, which is what this much PURIFY means.
            if (kern && core) continue;
            const int32_t j = core ? i + shift : i;
            if (j < 1 || j >= PitchMapNyq) continue;
            MYFLOAT m = _mag[i] * gain;
            // On a transient frame a MOVING region carries only its sustained
            // part; the onset increment passes unshifted in the transient
            // pass below. See the PITCHMAP_LOCK_MAX comment block.
            if (transient && core && (shift != 0 || rot != 0.))
                m -= _inc[i] * gain;
            // A claimed region is only PARTIAL near its peak; out where the
            // partials are 20 bins apart, the rest of the region is the noise
            // floor it happens to carry -- hi-hats used to ride through
            // PURIFY 1 inside exactly these bins. Outside the mainlobe the
            // region purifies like the residual. At PURIFY 0 nothing changes.
            if (src >= 0 && resGain < 1. &&
                (i < kp - PITCHMAP_LOBE || i > kp + PITCHMAP_LOBE))
                m *= resGain;
            if (m <= 0.) continue;
            if (fa > 0. && j != i) {
                MYFLOAT d = _env[j] - _env[i];
                if (d > 3.) d = 3.; else if (d < -3.) d = -3.;
                m *= std::exp(fa * d);
            }
            // Collision arbitration. Before any shifting, the regions tile the
            // spectrum, so every output bin is written exactly once -- but a
            // shifted core lands wherever its ratio sends it, on top of another
            // note's core, a neighbour region's skirt, or the residual. Complex-
            // adding two mainlobes whose phases come from UNRELATED accumulators
            // produces deep notches that move every frame -- a gurgle that is
            // worst exactly where regions are dense and shifts differ, i.e. the
            // low mids of a mix. Masking says the ear keeps the louder of two
            // components at the same frequency anyway, so the louder write WINS
            // the bin outright instead of interfering. No collision, no change:
            // at PURIFY 0 with everything in scale (all shifts 0) the tiling is
            // collision-free and reconstruction stays bit-exact.
            if (m <= _outMag[j]) continue;
            _outMag[j] = m;
            const MYFLOAT a = core ? _prevPhi[i] + rot : _prevPhi[i];
            _spec[j * 2] = m * std::cos(a);
            _spec[j * 2 + 1] = m * std::sin(a);
        }

        if (kern) {
            // The partial itself: an analytic Hann mainlobe at the FRACTIONAL
            // target bin c, per-bin phase ph - pi*(j - c) (the large-N Hann
            // transform's linear phase), amplitude corrected for the input's
            // own fractional offset (the peak bin under-reads an off-centre
            // partial by up to 1.4 dB). Verified against this exact OLA in
            // scratchpad/kernel_check.py before being written.
            const MYFLOAT c = ft / binHz;
            MYFLOAT di = _peakFrq[p] / binHz - (MYFLOAT) kp;
            if (di > .6) di = .6; else if (di < -.6) di = -.6;
            MYFLOAT A = _peakMag[p] / pitchmapHannK(di);
            // Transient split, kernel form: only the sustained share moves.
            if (transient && _mag[kp] > 0.) {
                MYFLOAT s = (_mag[kp] - _inc[kp]) / _mag[kp];
                if (s < 0.) s = 0.;
                A *= s;
            }
            if (fa > 0.) {
                const int32_t jc = (int32_t) (c + .5);
                if (jc >= 1 && jc < PitchMapNyq) {
                    MYFLOAT d = _env[jc] - _env[kp];
                    if (d > 3.) d = 3.; else if (d < -3.) d = -3.;
                    A *= std::exp(fa * d);
                }
            }
            const int32_t j0 = (int32_t) std::ceil(c - PITCHMAP_KERNEL_W);
            const int32_t j1 = (int32_t) std::floor(c + PITCHMAP_KERNEL_W);
            for (int32_t j = j0; j <= j1; ++j) {
                if (j < 1 || j >= PitchMapNyq) continue;
                const MYFLOAT w = pitchmapHannK((MYFLOAT) j - c);
                const MYFLOAT m = A * std::abs(w);
                if (m <= _outMag[j]) continue;
                _outMag[j] = m;
                const MYFLOAT a = ph - (TWOPI_P * .5) * ((MYFLOAT) j - c)
                                  + (w < 0. ? TWOPI_P * .5 : 0.);
                _spec[j * 2] = m * std::cos(a);
                _spec[j * 2 + 1] = m * std::sin(a);
            }
        }
    }

    // ---- transient pass-through ------------------------------------------
    // The onset increments withheld from the moving regions above, written at
    // their ORIGINAL position and phase, at full gain regardless of PURIFY:
    // the hammer arrives as it was played, the sustain arrives mapped.
    if (transient)
        for (int32_t k = 1; k < PitchMapNyq; ++k) {
            const MYFLOAT m = _inc[k];
            if (m <= _outMag[k]) continue;
            _outMag[k] = m;
            _spec[k * 2] = m * std::cos(_prevPhi[k]);
            _spec[k * 2 + 1] = m * std::sin(_prevPhi[k]);
        }
    _spec[0] = _spec[1] = 0.;

    fft.backward(_spec, buf);
    for (int32_t i = 0; i < PitchMapFFTSize; ++i) {
        buf[i] *= _win[i] * PITCHMAP_OLA_GAIN;
        UDD(buf[i])
    }
}

void PitchMap::compute(MYFLOAT *in, int32_t size) {
    auto _appState = this->_appState;
    MYFLOAT wet, dry;
    // Never stop running: the OLA state and the dry delay have to keep advancing
    // or the effect resumes with a stale frame. Bypass drives the wet/dry
    // targets instead and lets smwetdry complete the fade for the reaper.
    if (_bypass->load() || destroyRequested) {
        dry = 1.;
        wet = 0.;
    } else {
        dry = LOG2NORMALF(_dry->load());
        wet = LOG2NORMALF(_wet->load());
    }
    for (int32_t i = 0; i < size; i++) {
        _lfoIdx = i;
        MYFLOAT x = in[i];
        if (std::isnan(x)) x = 0.;
        const MYFLOAT d = delay.process(x);
        in[i] = _smooth2 * d + _smooth1 * _tick(x);
        smwetdry(wet, dry);
    }
}
