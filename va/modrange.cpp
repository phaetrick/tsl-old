// Modulation-range reporting for the knob UI.
//
// Answers one question for tslgraphics: given a target parameter, where can
// modulation take it? The answer is returned in the parameter's own value domain
// and the knob draws a signed arc from its current position to that endpoint, so
// nothing here has to say whether a route attenuates or pushes — the direction
// falls out of which side of the knob the endpoint lands on.
//
// IMPORTANT: every endpoint below has to agree with what synth.cpp's per-block
// tick() actually does, or the arc lies. The three shapes are factored into
// attenFloor/pushSum/lfoFloor so the two files can only ever disagree about which
// sources feed a target, never about the arithmetic. If you add a route or an LFO
// destination, add it here too.
//
// Deliberately NOT covered:
//   - KEYTRACK (both routes) and VEL. All three depend on something that is not a
//     parameter — the played note, the struck velocity — so there is no static
//     endpoint to draw. VEL_TO_FILT and VEL_TO_RES DO appear, because they attenuate
//     the same product as AT/MW and a resting velocity of 0 is a real reachable end.
//   - AMP. The AT and VEL amp routes have no control of their own to draw on;
//     voice gain is not a parameter.
//   - PITCH (the LFO route). The LFO's vibrato is an additive offset scaled by
//     LFO_PITCH_SEMITONES, not a modulation of any parameter, so there is no knob
//     whose range it widens. It deliberately no longer touches FINE / COARSE.
//     The TUNE EGs DO appear on COARSE/FINE below — those scale the knob itself.
//
// LFO ROUTES ARE BIPOLAR (synth.cpp, `lfoBi`), so an LFO target has TWO endpoints —
// one either side of the knob — while the VEL/AT/MW routes still have one. Hence
// modRangeFor2 returning both; the knob and the slider each draw the band between
// them. A one-directional source returns one end equal to the knob's own value, so
// it renders as the single stretch it always did.

#include "gui.h"
#include "knob.h"
#include "view.h"
#include "defines.h"
#include <app.h>
#include <algorithm>
#include <utility>
#include <cmath>

namespace {

inline float pval(tsl::AppState* s, int id) {
    return (float)s->params[0][id].load();
}

// Product of the floor factors of a set of attenuating routes. Each contributes
// (1 - amount), exactly as the factors multiply in the audio block, so the result
// is the fraction of the knob that survives with every source at rest.
inline float attenFloor(tsl::AppState* s, std::initializer_list<int> routes) {
    float f = 1.f;
    for (int id : routes) f *= 1.f - pval(s, id);
    return f;
}

// Sum of a set of pushing routes, clamped the way the audio block clamps them.
inline float pushSum(tsl::AppState* s, std::initializer_list<int> routes) {
    float a = 0.f;
    for (int id : routes) a += pval(s, id);
    return a > 1.f ? 1.f : a;
}

// Floor factor contributed by whichever LFOs are pointed at `dest`. Returns 1
// when neither is, and multiplies when both are — same as the audio block.
// |depth|: the sign only decides WHICH half of the cycle ducks, so a route reaches the
// same floor either way. Without the fabs a negative depth would report a floor above 1
// and the arc would claim the knob can be exceeded.
inline float lfoFloor(tsl::AppState* s, int dest) {
    float f = 1.f;
    for (int n = 0; n < 4; n++)
        f *= 1.f - std::fabs(pval(s, lfoMdId(n, dest)));
    return f;
}

inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// Total LFO depth aimed at `dest`, summed over both LFOs exactly as synth.cpp's
// lfoBi does. This is the half-width of the bipolar excursion, in the target's own
// normalised domain.
// |depth| again, and summed rather than cancelled: two LFOs on one bipolar dest with
// opposite signs still reach |d1|+|d2| at the extremes, since their phases are
// independent. Signing the sum here would draw a zero-width arc over a route that
// audibly moves.
inline float lfoDepth(tsl::AppState* s, int dest) {
    float d = 0.f;
    for (int n = 0; n < 4; n++)
        d += std::fabs(pval(s, lfoMdId(n, dest)));
    return d;
}

// Ends of the A->B sweep position `t`, matching applyMorph / applyWarpMod: with an
// EG assigned the EG covers 0..1 and the LFO offsets around it; with no EG the rest
// position depends on whether an LFO actually feeds the dest — centred at 0.5 with
// A/B as its extremes if one does, resting at 0 (= the fader's own value) if none
// does. Clamped to 0..1 — the declared span. Mirrors the lfoFeeds rule in synth.cpp.
inline void sweepT(tsl::AppState* s, int egSrcId, int lfoDest, float push,
                   float& tlo, float& thi) {
    const bool hasEg = (int)pval(s, egSrcId) != 0;
    const float d = lfoDepth(s, lfoDest);
    const float rest = hasEg ? 0.f : (d > 0.f ? 0.5f : 0.f);
    tlo = clamp01(rest - d);
    thi = clamp01((hasEg ? 1.f : rest) + d + push);
}

inline void order(float& lo, float& hi) { if (lo > hi) std::swap(lo, hi); }

// The three oscillators are contiguous for every per-osc parameter used here, so
// one index serves all of them.
inline int oscOf(int pid, int first) { return pid - first; }

}  // namespace

namespace tsl { namespace app {

bool modRangeFor2(tsl::AppState* s, int pid, float& lo, float& hi) {
    switch (pid) {

    // ---- filter cutoff ----
    // synth.cpp: gains[4] = base + rest*(egVelAtMw + lfoOff), rest = (0.5-base)*CUT.
    // In the knob's own domain that is CUT*(product + lfoOff), so the LFO's excursion
    // is NOT scaled by the attenuating floor — it is added to it.
    case FILT_CUT: {
        const float knob = pval(s, FILT_CUT);
        float floor = attenFloor(s, {VEL_TO_FILT, AT_TO_FILT, MW_TO_FILT});
        if ((int)pval(s, FILTEG) != -1) floor = 0.f;   // an assigned EG opens from nothing
        const float d = lfoDepth(s, 2);
        lo = clamp01(knob * (floor - d));
        hi = clamp01(knob * (1.f + d));
        return d > 0.f || floor < 1.f;
    }

    // ---- resonance: same shape, its own EG ----
    case FILTRES: {
        const float knob = pval(s, FILTRES);
        float floor = attenFloor(s, {VEL_TO_RES, AT_TO_RES, MW_TO_RES});
        if ((int)pval(s, RESEG) != -1) floor = 0.f;
        const float d = lfoDepth(s, 7);
        lo = clamp01(knob * floor - d);
        hi = clamp01(knob + d);
        return d > 0.f || floor < 1.f;
    }

    // ---- unison detune: LFO is bipolar, AT/MW still only widen ----
    case VCO1UNIDETUNE: case VCO2UNIDETUNE: case VCO3UNIDETUNE: {
        const int o = (pid == VCO1UNIDETUNE) ? 0 : (pid == VCO2UNIDETUNE) ? 1 : 2;
        const float knob = pval(s, pid);
        const float d    = lfoDepth(s, 14 + o);
        const float push = pushSum(s, {AT_TO_UNI, MW_TO_UNI});
        lo = clamp01(knob - d);
        float h = clamp01(knob + d);
        hi = clamp01(h + push * (1.f - h));
        return d > 0.f || push > 0.f;
    }

    // ---- tune: a TUNE EG scales the knob's own contribution 0..1, so the reach-
    // able range spans 0..knob (either side of 0 for a negative knob) — the same
    // shape FILTEG gives CUT. synth.cpp: _jit = cps * 2^(_tune * tuneEgVal + ...).
    case VCO1COARSE: case VCO1FINE:
    case VCO2COARSE: case VCO2FINE:
    case VCO3COARSEST: case VCO3FINE: {
        const int eg = (pid == VCO1COARSE || pid == VCO1FINE) ? VCO1TUNEEG
                     : (pid == VCO2COARSE || pid == VCO2FINE) ? VCO2TUNEEG
                     : VCO3TUNEEG;
        if ((int)pval(s, eg) == -1) return false;   // NONE: the knob is static
        const float v = pval(s, pid);
        lo = std::min(0.f, v); hi = std::max(0.f, v);
        return v != 0.f;
    }

    // ---- pulse width: EG walks the knob toward PWMDEPTH^0.85 (in knob units —
    // the control is inverted vs the DSP value, pw = 0.5 - 0.45*v, and the target
    // pw 0.5 - depth*0.45 maps back to exactly depth). The LFO adds in the knob's
    // own domain (synth.cpp adds lfoOff*0.45 to pw = lfoOff in knob units). ----
    case VCO1PW: case VCO2PW: case VCO3PW: {
        // NB: the three PW ids are NOT contiguous (they interleave with the other
        // per-osc params), so no oscOf here.
        const int o = pid == VCO1PW ? 0 : pid == VCO2PW ? 1 : 2;
        const bool hasEg = (int)pval(s, VCO1PWMODSRC + o) != 0;
        const float d = lfoDepth(s, 4 + o);
        const float v = pval(s, pid);
        const float target = hasEg ? std::pow(pval(s, VCO1PWMODDEPTH + o), 0.85f) : v;
        lo = clamp01(std::min(v, target) - d);
        hi = clamp01(std::max(v, target) + d);
        return d > 0.f || (hasEg && target != v);
    }

#if PA_ENABLE_PAD
    // ---- PAD morph: same route set as WT morph (dests 8..10, AT/MW push, its own
    // EG in VCOxPADMEG), over PADPOS(A) -> PADMTO(B) ----
    case VCO1PADPOS: case VCO2PADPOS: case VCO3PADPOS: {
        const int o = oscOf(pid, VCO1PADPOS);
        const float a = pval(s, pid), b = pval(s, VCO1PADMTO + o);
        float tlo, thi;
        sweepT(s, VCO1PADMEG + o, 8 + o, pushSum(s, {AT_TO_MORPH, MW_TO_MORPH}), tlo, thi);
        lo = clamp01(a + tlo * (b - a));
        hi = clamp01(a + thi * (b - a));
        order(lo, hi);
        return thi > tlo && b != a;
    }
#endif

    // ---- wavetable morph: position along WTPOS(A) -> MORPHTO(B) ----
    case VCO1WTPOS: case VCO2WTPOS: case VCO3WTPOS: {
        const int o = oscOf(pid, VCO1WTPOS);
        const float a = pval(s, pid), b = pval(s, VCO1MORPHTO + o);
        float tlo, thi;
        sweepT(s, VCO1PWMODSRC + o, 8 + o, pushSum(s, {AT_TO_MORPH, MW_TO_MORPH}), tlo, thi);
        lo = clamp01(a + tlo * (b - a));
        hi = clamp01(a + thi * (b - a));
        order(lo, hi);                       // B < A inverts the sweep
        return thi > tlo && b != a;
    }

    // ---- warp amount: WARPAMT(A) -> WARPTO(B) ----
    case VCO1WARPAMT: case VCO2WARPAMT: case VCO3WARPAMT: {
        const int o = (pid - VCO1WARPAMT) / 2;
        const float a = pval(s, pid), b = pval(s, VCO1WARPTO + o);
        float tlo, thi;
        sweepT(s, VCO1WARPEG + o, 11 + o, pushSum(s, {AT_TO_WARP, MW_TO_WARP}), tlo, thi);
        lo = clamp01(a + tlo * (b - a));
        hi = clamp01(a + thi * (b - a));
        order(lo, hi);
        return thi > tlo && b != a;
    }

    // ---- modal strike: bipolar, scaled by the parameter's own 0..0.5 range ----
    case FILTMODALPOS: {
        const float knob = pval(s, pid);
        const float d = lfoDepth(s, 17) * .5f;
        lo = std::max(0.f,  knob - d);
        hi = std::min(.5f,  knob + d);
        return d > 0.f;
    }

    // ---- one-sided targets: the LFO does not reach these, so lo == hi == endpoint ----
    case LFO1DEPTH: case LFO2DEPTH: case LFO3DEPTH: case LFO4DEPTH: {
        // MW/AT pull the depth toward ZERO, which is below the knob for a positive depth
        // and above it for a negative one, so the two ends have to be ordered rather
        // than assumed. Ranges are drawn lo..hi and a swapped pair draws nothing.
        const float floor = attenFloor(s, {MW_TO_LFODEPTH, AT_TO_LFODEPTH});
        const float v = pval(s, pid), atten = v * floor;
        lo = std::min(v, atten); hi = std::max(v, atten);
        return floor < 1.f;
    }
    case LFO1RATE: case LFO2RATE: case LFO3RATE: case LFO4RATE: {
        const float amt = pushSum(s, {MW_TO_LFORATE, AT_TO_LFORATE});
        const float knob = pval(s, pid);
        lo = knob; hi = knob + ((float)s->parameters[pid].max - knob) * amt;
        return amt > 0.f;
    }
    case JITTERCENTS: {
        const float floor = attenFloor(s, {AT_TO_VIBRATO, MW_TO_VIBRATO});
        lo = pval(s, JITTERCENTS) * floor; hi = pval(s, JITTERCENTS);
        return floor < 1.f;
    }
    case VCO1MODALDEC: case VCO2MODALDEC: case VCO3MODALDEC: {
        const float damp = (1.f - pval(s, AT_TO_DECAY)) * (1.f - pval(s, MW_TO_DECAY));
        if (damp >= 1.f) return false;
        hi = pval(s, pid);
        lo = hi + 20.f * std::log10(damp < 1e-4f ? 1e-4f : damp);
        return true;
    }

    // ---- per-source gain: the LFO only ducks it, nothing pushes it ----
    // Same dB-domain shape as MODALDEC above. dest 18+i is gains[i] in synth.cpp, so
    // the ids line up VCO1/VCO2/SUB/NOISE. ONE endpoint, below the knob: these routes
    // are unipolar, unlike every other LFO destination here, because the GAIN knob is
    // the ceiling — see the note on lfoDuck in synth.cpp. A depth-1 duck reaches
    // actual silence, so the arc runs to the parameter's own floor — the closest
    // honest position the ring has (a fixed knob-relative floor understated the
    // sanctioned gate on any knob above 0 dB). The 1e-7 only guards log10(0).
    case VCO1GAIN: case VCO2GAIN: case VCO3GAIN: case NOISEGAIN: {
        const int dest = pid == VCO1GAIN ? 18 : pid == VCO2GAIN ? 19
                       : pid == VCO3GAIN ? 20 : 21;
        const float floor = lfoFloor(s, dest);
        if (floor >= 1.f) return false;
        hi = pval(s, pid);
        lo = std::max((float)s->parameters[pid].min,
                      hi + 20.f * std::log10(std::max(floor, 1e-7f)));
        return true;
    }

    default:
        return false;
    }
}

// ── invalidation ─────────────────────────────────────────────────────────────
//
// A knob only redraws when its own value changes, so without this a mod range
// goes stale the moment you touch the thing that defines it. That is not a corner
// case: CUT EG sits directly beside the CUT knob on the FILT page, and the LFO's
// DEST and DEPTH sit beside each other. Change either and the ring would keep
// showing the previous range until you left the page and came back.
//
// Rather than a reverse map from every source to the targets it feeds, redraw all
// the targets whenever any source moves. Only the handful currently on screen do
// any work (the rest fail the visible_ test), and sources move far less often than
// values do. A wrong entry here can only cost a redundant repaint, whereas a
// missing edge in a reverse map would be an invisible staleness bug.

static const int kModTargets[] = {
    FILT_CUT, FILTRES, FILTMODALPOS, JITTERCENTS,
    VCO1GAIN, VCO2GAIN, VCO3GAIN, NOISEGAIN,
    VCO1UNIDETUNE, VCO2UNIDETUNE, VCO3UNIDETUNE,
    VCO1WTPOS, VCO2WTPOS, VCO3WTPOS,
    VCO1WARPAMT, VCO2WARPAMT, VCO3WARPAMT,
    VCO1MODALDEC, VCO2MODALDEC, VCO3MODALDEC,
    LFO1DEPTH, LFO2DEPTH, LFO3DEPTH, LFO4DEPTH,
    LFO1RATE, LFO2RATE, LFO3RATE, LFO4RATE,
    VCO1COARSE, VCO1FINE, VCO2COARSE, VCO2FINE, VCO3COARSEST, VCO3FINE,
    VCO1PW, VCO2PW, VCO3PW,
#if PA_ENABLE_PAD
    VCO1PADPOS, VCO2PADPOS, VCO3PADPOS,
#endif
};

// Every parameter that can change some knob's mod range. The AMP / VIBRATO /
// DECAY / KEYTRACK routes are listed even though modRangeFor does not draw them
// yet: they cost nothing here, and including them means adding a target later is
// a one-line change to kModTargets rather than a hunt for the missing source.
bool isModSource(int pid) {
    // The whole multi-dest matrix feeds mod ranges; a contiguous id block beats 84
    // case labels.
    if (pid >= LFO_MD_FIRST && pid <= LFO_MD_LAST) return true;
    switch (pid) {
    case VEL_TO_FILT: case VEL_TO_AMP: case VEL_TO_RES:
    case AT_TO_FILT:  case AT_TO_AMP:  case AT_TO_RES:  case AT_TO_VIBRATO:
    case AT_TO_LFORATE: case AT_TO_LFODEPTH:
    case AT_TO_WARP:  case AT_TO_MORPH: case AT_TO_UNI: case AT_TO_DECAY:
    case MW_TO_FILT:  case MW_TO_VIBRATO: case MW_TO_RES:
    case MW_TO_LFORATE: case MW_TO_LFODEPTH:
    case MW_TO_MORPH: case MW_TO_WARP:  case MW_TO_UNI: case MW_TO_DECAY:
    case KEYTRACK_TO_FILT: case KEYTRACK_TO_DECAY:
    case LFO1DEST: case LFO1DEPTH: case LFO2DEST: case LFO2DEPTH:
    case LFO3DEST: case LFO3DEPTH: case LFO4DEST: case LFO4DEPTH:
    case FILTEG: case RESEG:
    case VCO1PWMODSRC: case VCO2PWMODSRC: case VCO3PWMODSRC:
    case VCO1PWMODDEPTH: case VCO2PWMODDEPTH: case VCO3PWMODDEPTH:
    case VCO1WARPEG:   case VCO2WARPEG:   case VCO3WARPEG:
    case VCO1MORPHTO:  case VCO2MORPHTO:  case VCO3MORPHTO:
    case VCO1WARPTO:   case VCO2WARPTO:   case VCO3WARPTO:
    case VCO1TUNEEG:   case VCO2TUNEEG:   case VCO3TUNEEG:
#if PA_ENABLE_PAD
    case VCO1PADMEG:   case VCO2PADMEG:   case VCO3PADMEG:
    case VCO1PADMTO:   case VCO2PADMTO:   case VCO3PADMTO:
#endif
        return true;
    default:
        return false;
    }
}

void redrawModTargets(tsl::AppState* _appState) {
    // Marshalled the same way redrawEvent does it: apply() can be reached from the
    // midi and audio threads, and a View may only be touched on the UI thread.
    _STATE->toUiThreadQueue.try_push([_appState]() mutable {
        for (int pid : kModTargets) {
            auto v = _STATE->parameters[pid].view;
            if (v != nullptr && v->visible_) v->redraw();
        }
    });
}

}}  // namespace tsl::app
