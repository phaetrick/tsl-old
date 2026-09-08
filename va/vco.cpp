#include "defines.h"
#include "ffttools.h"
#include "vco.h"
#include "gui.h"      // BuildOverlayScope
#include "grainstorm.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <memory>
#include <map>
#include <deque>
#include <array>
#include <atomic>

#define OSCBNK_PHSMAX   OSCBNK_PHSMAX_32
#define OSCBNK_PHSMSK   OSCBNK_PHSMSK_32
#define OSCBNK_PHS2INT  OSCBNK_PHS2INT_32

#define CHECKFFT(_M) \
    int32_t _index = (int32_t)std::log2(_M); \
    if (ffts[_index] == nullptr) ffts[_index] = std::make_unique<FFT>(_M); \
    auto fft = ffts[_index].get();

// Per-thread FFT plans: wavetable sets are now built on the UiTasks worker thread
// (see getWavetableSet/requestWavetableSet) while the GUI thread may build for the
// scope and the audio thread builds the shared non-WT tables — an FFT plan holds
// internal scratch, so a shared instance transformed from two threads corrupts. One
// lazy plan set per building thread keeps every transform independent.
static thread_local std::unique_ptr<FFT> ffts[NUMFFTS + 1];

// Mipmap density + size cap (memory: float storage + these ≈ 20x smaller than the
// old double / 1.05 / 16384 scheme). 2^(1/3) ratio = 3 tables/octave (~34 levels vs
// ~130); worst-case ~21% of the top harmonics missing at an octave-band bottom.
static constexpr double WT_MIPMAP_RATIO  = 1.2599210498948732;  // 2^(1/3)
// 4096. This was briefly 8192 to buy headroom for a LINEAR read; interpAt's 6-point
// kernel makes that unnecessary — measured, the same worst case is -60 dB with 4-point
// Hermite at 4096 (already better than linear at 8192) and about -113 dB with the
// 6-point one. Spending the bytes on table size is strictly worse value than spending
// a few ns on the interpolator, and the memory is wanted for morph frames instead.
static constexpr int    WT_MAX_TABLESIZE = 4096;                 // was WINDOW_SIZE (16384)

static void oscbnk_flen_setup(int32_t flen, uint32_t* mask, uint32_t* lobits, MYFLOAT* pfrac) {
    uint32_t n  = (uint32_t)flen;
    uint32_t fm = 1;
    *lobits = 0;
    while (n < OSCBNK_PHSMAX_32) { n <<= 1u; fm <<= 1u; (*lobits)++; }
    *pfrac = 1.0 / (MYFLOAT)fm;
    *mask  = fm - 1;
}

// ── WaveTable ────────────────────────────────────────────────────────────────

void WaveTable::computeWavetable(int type, int nPartials) {
    type_     = type;
    nPart     = nPartials;
    tableSize = computeTableSize();
    resize(tableSize, 0);
    flenSetup();
    calculateTable(type);
}

void WaveTable::computeWavetableFromHarmonics(const float* mags, int nHarm, int nPartials) {
    type_     = 1;   // interpolated table (not the sine special-case)
    nPart     = nPartials;
    tableSize = computeTableSize();
    resize(tableSize, 0);
    flenSetup();
    // Same spectrum→IFFT path as calculateTable, but the harmonic amplitudes come
    // from mags[] instead of a formula. Bandlimit to nPart (and nHarm) per level.
    const MYFLOAT scaleFac = .5 * (MYFLOAT)tableSize;
    CHECKFFT(tableSize);
    // the FFT scratch keeps its +2: the packed spectrum addresses bin tableSize/2 as
    // buf[tableSize] / buf[tableSize+1]. Only the stored table is exactly tableSize.
    std::vector<double> buf(tableSize + 2, 0.0);
    unsigned int maxh = (unsigned int)std::min({(int)(tableSize >> 1u), nPart, nHarm});
    for (unsigned int i = 1; i <= maxh; i++)
        buf[(i << 1u) + 1] = (MYFLOAT)mags[i] * scaleFac;   // sine-phase harmonic i
    buf[1] = buf[tableSize];
    buf[tableSize] = 0.0;
    fft->backward(buf.data());
    for (int j = 0; j < (int)tableSize; j++) data()[j] = (float)buf[j];
}

// Olli Niemitalo, "Polynomial Interpolators for High-Quality Resampling of
// Oversampled Audio" — Optimal 2x (6-point, 5th-order). Each of the six taps is
// masked on its own: the table has no guard sample, and n-2 / n+3 straddle the wrap
// at both ends of the cycle. Unsigned arithmetic makes (0 - 2) & mask land on
// tableSize-2, which is the sample we want.
MYFLOAT WaveTable::interpAt(uint32_t n, MYFLOAT f) const {
    const float* d = data();
    const uint32_t m = tableMask_32;
    const MYFLOAT ym2 = d[(n - 2u) & m], ym1 = d[(n - 1u) & m], y0 = d[n & m],
                  y1  = d[(n + 1u) & m], y2  = d[(n + 2u) & m], y3 = d[(n + 3u) & m];
    const MYFLOAT z = f - 0.5;
    const MYFLOAT e1 = y1 + y0,  o1 = y1 - y0;
    const MYFLOAT e2 = y2 + ym1, o2 = y2 - ym1;
    const MYFLOAT e3 = y3 + ym2, o3 = y3 - ym2;
    const MYFLOAT c0 = e1 *  0.40513396007145713 + e2 *  0.09251794438424393 + e3 * 0.00234806603570670;
    const MYFLOAT c1 = o1 *  0.28342806338906690 + o2 *  0.21703445007362035 + o3 * 0.01309294748731147;
    const MYFLOAT c2 = e1 * -0.19133771509021100 + e2 *  0.16187844487943592 + e3 * 0.02946017143111912;
    const MYFLOAT c3 = o1 * -0.16471626190395582 + o2 * -0.00154547203542499 + o3 * 0.03399271444851909;
    const MYFLOAT c4 = e1 *  0.03845798729588149 + e2 * -0.05712936104242644 + e3 * 0.01866750929921070;
    const MYFLOAT c5 = o1 *  0.04317950185225609 + o2 * -0.01802814255926417 + o3 * 0.00152170021558204;
    return ((((c5 * z + c4) * z + c3) * z + c2) * z + c1) * z + c0;
}

MYFLOAT WaveTable::tick(uint32_t& phs, const uint32_t frq) const {
    uint32_t n = (phs >> lobits_32) & tableMask_32;
    if (type_ == -1) {
        phs = (phs + frq) & OSCBNK_PHSMSK_32;
        return data()[n];
    }
    const MYFLOAT v = interpAt(n, (MYFLOAT)((int32_t)(phs & fractMask_32)) * pfrac_32);
    phs = (phs + frq) & OSCBNK_PHSMSK_32;
    return v;
}

MYFLOAT WaveTable::read(uint32_t phs) const {
    return interpAt((phs >> lobits_32) & tableMask_32,
                    (MYFLOAT)((int32_t)(phs & fractMask_32)) * pfrac_32);
}

MYFLOAT WaveTable::tickpw(uint32_t& phs, const uint32_t frq, const MYFLOAT pw) const {
    uint32_t phs2 = (phs + OSCBNK_PHS2INT_32(pw)) & OSCBNK_PHSMSK_32;
    const MYFLOAT smpl = interpAt((phs >> lobits_32) & tableMask_32,
                                  (MYFLOAT)((int32_t)(phs & fractMask_32)) * pfrac_32);
    const MYFLOAT v    = interpAt((phs2 >> lobits_32) & tableMask_32,
                                  (MYFLOAT)((int32_t)(phs2 & fractMask_32)) * pfrac_32);
    phs = (phs + frq) & OSCBNK_PHSMSK_32;
    MYFLOAT denom = pw - pw * pw;
    if (std::fabs(denom) < 1e-6) return 0.0;
    return (smpl - v) * (.25 / denom);
}

// Csound vco2 mode 1: PWM square as saw(phs) - saw(phs + fract(-pw)) plus a
// DC correction term, giving constant +-1 amplitude for every pulse width
// (unlike tickpw, whose .25/(pw-pw*pw) normalization only holds for the
// parabola-table triangle).
MYFLOAT WaveTable::tickpwm(uint32_t& phs, const uint32_t frq, const MYFLOAT pw) const {
    MYFLOAT f = -pw;
    f -= (MYFLOAT)((int32_t)f);
    if (f < 0.) f++;
    uint32_t phs2 = (phs + OSCBNK_PHS2INT_32(f)) & OSCBNK_PHSMSK_32;
    const MYFLOAT smpl = interpAt((phs >> lobits_32) & tableMask_32,
                                  (MYFLOAT)((int32_t)(phs & fractMask_32)) * pfrac_32);
    const MYFLOAT v    = interpAt((phs2 >> lobits_32) & tableMask_32,
                                  (MYFLOAT)((int32_t)(phs2 & fractMask_32)) * pfrac_32);
    phs = (phs + frq) & OSCBNK_PHSMSK_32;
    return smpl - v + (1.0 - 2.0 * f);
}

MYFLOAT WaveTable::tickpw64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw) const {
    // the masked index is < tableSize (<= 4096), so it always fits the 32-bit tap path
    const MYFLOAT smpl = interpAt((uint32_t)((phs >> lobits_64) & tableMask_64),
                                  (MYFLOAT)(phs & fractMask_64) * pfrac_64);
    if (type_ == 1 || type_ == -1) {
        phs = (phs + frq) & OSCBNK_PHSMSK_64;
        return smpl;
    }
    uint64_t phs2 = (phs + OSCBNK_PHS2INT_64(pw)) & OSCBNK_PHSMSK_64;
    const MYFLOAT v = interpAt((uint32_t)((phs2 >> lobits_64) & tableMask_64),
                               (MYFLOAT)(phs2 & fractMask_64) * pfrac_64);
    phs = (phs + frq) & OSCBNK_PHSMSK_64;
    MYFLOAT denom = pw - pw * pw;
    if (std::fabs(denom) < 1e-5) return 0.0;
    return (smpl - v) * (0.25 / denom);
}

void WaveTable::calculateTable(int type) {
    int32_t minh = (type >= 0) ? 1 : 0;
    MYFLOAT scaleFac = .5 * (MYFLOAT)tableSize;
    CHECKFFT(tableSize);
    std::vector<double> buf(tableSize + 2, 0.0);   // FFT scratch (double); table stores float

    switch (type) {
    case 0:  scaleFac *= (8. / (PI_F_P * PI_F_P)); break;
    case 1:  scaleFac *= (-2. / PI_F_P);            break;
    case 2:  scaleFac *= (-4. / PI_F_P);            break;
    case 10: scaleFac *= (-4. / (PI_F_P * PI_F_P)); break;
    case 11: break;
    default: break;
    }

    for (unsigned int i = minh; i <= (tableSize >> 1u); i++) {
        switch (type) {
        case 0:  // triangle
            if (i <= (unsigned)nPart)
                buf[(i << 1u) + 1] = (i & 1u ? ((i & 2u ? scaleFac : -scaleFac)
                    / ((MYFLOAT)i * (MYFLOAT)i)) : 0.0);
            break;
        case 1:  // sawtooth
            if (i <= (unsigned)nPart)
                buf[(i << 1u) + 1] = scaleFac / (MYFLOAT)i;
            break;
        case 2:  // square
            if (i <= (unsigned)nPart)
                buf[(i << 1u) + 1] = (i & 1u ? scaleFac / (MYFLOAT)i : 0.0);
            break;
        case 10: // 4*x*(1-x)
            if (i <= (unsigned)nPart)
                buf[i << 1u] = scaleFac / ((MYFLOAT)i * (MYFLOAT)i);
            break;
        case 11: // pulse
            if (i <= (unsigned)nPart)
                buf[i << 1u] = scaleFac;
            break;
        default:
            if (i > (unsigned)nPart)
                buf[(i << 1u) + 1] = buf[(i << 1u)] = 0.0;
            break;
        }
    }
    buf[1] = buf[tableSize];
    buf[tableSize] = 0.0;
    fft->backward(buf.data());
    for (int j = 0; j < (int)tableSize; j++) data()[j] = (float)buf[j];
}

void WaveTable::computeWavetableFromComplex(const float* re, const float* im,
                                            int nSrcHarm, int nPartials, int srcLen) {
    type_     = 1;
    nPart     = nPartials;
    tableSize = computeTableSize();
    resize(tableSize, 0);
    flenSetup();
    CHECKFFT(tableSize);
    std::vector<double> buf(tableSize + 2, 0.0);   // +2: FFT scratch, see above
    // scale keeps a harmonic's amplitude constant across mipmap sizes (source was
    // FFT'd at srcLen, this level IFFTs at tableSize).
    const MYFLOAT scale = (MYFLOAT)tableSize / (MYFLOAT)srcLen;
    unsigned int maxh = (unsigned int)std::min({(int)(tableSize >> 1u), nPartials, nSrcHarm});
    for (unsigned int i = 1; i <= maxh; i++) {
        buf[i << 1u]       = (MYFLOAT)re[i] * scale;   // real of harmonic i
        buf[(i << 1u) + 1] = (MYFLOAT)im[i] * scale;   // imag of harmonic i
    }
    buf[1] = buf[tableSize];   // pack Nyquist (matches calculateTable)
    buf[tableSize] = 0.0;
    fft->backward(buf.data());
    for (int j = 0; j < (int)tableSize; j++) data()[j] = (float)buf[j];
}

int WaveTable::computeTableSize() {
    int n;
    if      (nPart <= 1)    n = 1;
    else if (nPart <= 4)    n = 2;
    else if (nPart <= 16)   n = 4;
    else if (nPart <= 64)   n = 8;
    else if (nPart <= 256)  n = 16;
    else if (nPart <= 1024) n = 32;
    else                    n = 64;
    n *= 256;
    if (n > WT_MAX_TABLESIZE) n = WT_MAX_TABLESIZE;
    return n;
}

void WaveTable::flenSetup() {
    uint32_t n = tableSize;
    tableMask_32  = tableMask_64 = n - 1;
    lobits_32     = 0;
    fractMask_32  = 1;
    pfrac_32      = 0.0;
    while (n < OSCBNK_PHSMAX_32) {
        n <<= 1u;
        fractMask_32 <<= 1u;
        lobits_32++;
    }
    pfrac_32 = 1.0 / (MYFLOAT)fractMask_32;
    fractMask_32--;

    uint64_t n2 = tableSize;
    lobits_64    = 0;
    fractMask_64 = 1;
    pfrac_64     = 0.0;
    while (n2 < OSCBNK_PHSMAX_64) {
        n2 <<= 1u;
        fractMask_64 <<= 1u;
        lobits_64++;
    }
    pfrac_64 = 1.0 / (MYFLOAT)fractMask_64;
    fractMask_64--;
}

// ── WaveTables ───────────────────────────────────────────────────────────────

// The mipmap ladder: partial counts 0, 1, 2, ... stepping by WT_MIPMAP_RATIO once
// the ratio advances by more than one partial. Returns the level count and, if
// `out` is given, each level's partial count.
//
// `maxContent` is the highest harmonic the SOURCE can actually supply. Levels past
// it would be byte-identical copies of the last one — the source has nothing more
// to put in them — and getTable's index map already sends every lower frequency to
// the last level, so generating them is pure waste. A wavetable frame carries at
// most WT_CYCLE/2 - 1 harmonics, which made 7 of 34 levels (26% of the bytes)
// duplicates. The ladder always ENDS on a level that reaches maxContent, so nothing
// is dulled: only exact duplicates disappear.
static int32_t wtLadder(int32_t maxContent, std::vector<int32_t>* out) {
    const int32_t cap = std::min(maxContent, (int32_t)WaveTables::maxHarmonics);
    double  npart_f = 0.0;
    int32_t n       = 0;
    for (;;) {
        const int32_t npart = (int32_t)(npart_f + 0.5);
        if (out) out->push_back(npart);
        n++;
        if (npart >= cap) break;               // this level already holds everything
        const double x = npart_f * WT_MIPMAP_RATIO;
        npart_f = ((x - npart_f) < 1.0) ? npart_f + 1.0 : x;
    }
    return n;
}

// partial count → level index, for every count getTable can ask for. Levels beyond
// the ladder's end all map to its last entry.
void WaveTables::buildIndex(int32_t ntables) {
    npartsIdx.resize(maxHarmonics + 1);
    int32_t npart = 0, i = 0;
    do {
        npartsIdx[npart++] = (uint16_t)i;
        if (i < (ntables - 1) && npart >= data()[i + 1].nPart) i++;
    } while (npart <= maxHarmonics);
}

void WaveTables::setup(int type) {
    std::vector<int32_t> lad;
    // analytic waveforms are unbounded in harmonics — every level differs
    const int32_t ntables = wtLadder(maxHarmonics, &lad);
    resize(ntables);
    for (int32_t i = 0; i < ntables; i++) data()[i].computeWavetable(type, lad[i]);
    buildIndex(ntables);
}

void WaveTables::setupFromHarmonics(const float* mags, int nHarm) {
    std::vector<int32_t> lad;
    const int32_t ntables = wtLadder(nHarm, &lad);
    resize(ntables);
    for (int32_t i = 0; i < ntables; i++)
        data()[i].computeWavetableFromHarmonics(mags, nHarm, lad[i]);
    buildIndex(ntables);
}

void WaveTables::setupFromWaveform(const float* cycle, int len) {
    // forward-FFT the source cycle once → complex harmonics [DC, Nyq, re1,im1, ...]
    std::vector<double> src(cycle, cycle + len);
    { CHECKFFT(len); fft->forward(src.data()); }
    int nh = len / 2;
    std::vector<float> re(nh + 1, 0.f), im(nh + 1, 0.f);
    for (int i = 1; i < nh; i++) { re[i] = (float)src[2 * i]; im[i] = (float)src[2 * i + 1]; }

    std::vector<int32_t> lad;
    // the loop above fills harmonics 1..nh-1; bin nh (Nyquist) stays zero
    const int32_t ntables = wtLadder(nh - 1, &lad);
    resize(ntables);
    for (int32_t i = 0; i < ntables; i++)
        data()[i].computeWavetableFromComplex(re.data(), im.data(), nh, lad[i], len);
    buildIndex(ntables);
}

WaveTable& WaveTables::getTable(MYFLOAT sampleRate, MYFLOAT frequency) {
    MYFLOAT npart = std::fabs(frequency);
    if (npart < p_min) npart = p_min;
    int32_t k = (int32_t)(p_scl / npart);
    // p_min bounds this at maxHarmonics for any sane input; the clamp only exists so
    // a NaN/denormal frequency can't index out of the map into arbitrary memory.
    if ((uint32_t)k > (uint32_t)maxHarmonics) k = maxHarmonics;
    return data()[npartsIdx[k]];
}

// ── Shared table cache (read-only after init, process-lifetime) ──────────────
// PA uses modes: -1(sine), 0(sawtooth), 2(square/PWM), 4(ramp/tri)
// tnum→GS type:  0→1(saw), 1→10(4x(1-x)), 2→11(pulse), 3→2(square), 4→0(tri)
static constexpr int GS_TYPE[5] = {1, 10, 11, 2, 0};
static WaveTables    gTables[5];
static bool          gTablesReady[5]{};
static std::mutex    gTablesMtx;

static WaveTables* getSharedTables(int tnum) {
    if (tnum < 0 || tnum >= 5) return nullptr;
    if (!gTablesReady[tnum]) {
        std::lock_guard<std::mutex> lk(gTablesMtx);
        if (!gTablesReady[tnum]) {
            gTables[tnum].setup(GS_TYPE[tnum]);
            gTablesReady[tnum] = true;
        }
    }
    return &gTables[tnum];
}

// ── Shared wavetables (built lazily per table on first use) ──────────────────
// Built-in tables, each morphing across WT_FRAMES frames. All frames are defined
// as sine-phase harmonic spectra so they build through the existing bandlimited
// mipmap machinery (setupFromHarmonics) — alias-free at every pitch. Some (Metal,
// Chime) are harmonic approximations of their namesake (true inharmonic content
// isn't a single periodic cycle).
static constexpr int WT_FRAMES      = 8;   // default morph frames; see wtMorphFrames
static constexpr int WT_WARP_LEVELS = 4;   // baked warp-amount levels (interpolated at runtime)
static constexpr int WT_HARMONIC   = 512;
static constexpr int WT_NUM_TABLES = 27;  // ...FM2 PLUCK RESO BUZZ EPIANO STACK ; 19/20 time-domain
static constexpr int WT_CYCLE      = 2048; // source length for time-domain tables
static constexpr int WT_DIGITAL_DRAWS = 8; // DIGITAL's distinct random spectra (see case 12)

// Morph frames PER TABLE. Runtime morph crossfades the two neighbouring frames, and a
// crossfade only reconstructs the position in between when the thing being morphed
// FADES. Where the morph MOVES a narrow peak or a spectral edge, blending two frames
// gives you both features at once instead of one in between, and the error measures
// as large as the signal itself. Measured against a densely baked reference, at 8
// frames: SWEEP -0.0 dB, CZ RES -0.3, BUZZ -9.4, FORMANT -16.4, METAL -17.3, against
// ODD -28.9 / NOISE -31.5 for the fade-family tables. It converges only 6-12 dB per
// doubling, so density is spent only where the morph actually moves something.
//
// BASIC is the cheap one: its morph is exactly piecewise linear with a breakpoint at
// t = 0.5, which the 8-frame grid (t = k/7) steps straight over. Any grid containing
// 0.5 reproduces it exactly — 9 frames measures -151 dB against -23 dB at 8.
static int wtMorphFrames(int table) {
    switch (table) {
    case 16:            // SWEEP    — narrow peak sweeping a dense spectrum
    case 20:            // CZ RES   — windowed resonance, same shape
    case 24: return 33; // BUZZ     — moving spectral edge
    case 3:             // FORMANT  — moving formant over a buzz
    case 5:  return 17; // METAL    — sparse partials fanning outward
    case 0:  return 9;  // BASIC    — only needs t = 0.5 on the grid
    default: return WT_FRAMES;
    }
}

// deterministic hash → [0,1), for reproducible random spectra (NOISE/DIGITAL).
static double wtHash01(uint32_t a) {
    a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16;
    return (a & 0xffffffu) / (double)0x1000000u;
}

// Time-domain single cycle for the phase/random-phase tables (19 NOISE, 20 CZRES).
// The old warp-redundant time-domain tables (FOLD/CZSAW/SYNC/SATURATE/CRUSH) were
// replaced in place — those sounds live in the WARP dimension now (sine+FOLD warp,
// sine+BEND, saw+SYNC, sine+SAT, sine+CRUSH→BITS).
// t is a CONTINUOUS morph position, like wtFrameHarmonicsT — frames are baked across
// whatever [A,B] range the oscillator is set to, not a fixed 0..1 grid.
static void wtFrameWaveformT(int wt, double t, float* c, int L) {
    switch (wt) {
    case 19: { // Noise: dense spectrum with hashed random magnitudes AND random
               // phases (frozen across frames so the morph is smooth); the morph
               // sweeps the spectral tilt dark → bright. Random phase is why this
               // is time-domain — the sine-phase harmonic path would keep it buzzy.
        const int nh = 256;
        const double e = 1.6 - 1.3 * t;               // tilt: 1/n^1.6 → 1/n^0.3
        for (int i = 0; i < L; i++) c[i] = 0.f;
        for (int n = 1; n <= nh; n++) {
            double mag = (0.35 + 0.65 * wtHash01((uint32_t)n * 40503u + 999u))
                         / std::pow((double)n, e);
            double phi = 2.0 * PI_F_P * wtHash01((uint32_t)n * 2654435761u + 12345u);
            for (int i = 0; i < L; i++)
                c[i] += (float)(mag * std::sin(2.0 * PI_F_P * n * i / L + phi));
        }
        float peak = 1e-6f;
        for (int i = 0; i < L; i++) peak = std::max(peak, std::fabs(c[i]));
        for (int i = 0; i < L; i++) c[i] = (float)(0.6 * c[i] / peak);
    } break;
    default: { // 20: CZ resonant — falling-saw window × a resonant sine that sweeps up
        double res = 1.0 + t * 11.0;             // linear: moving resonance (see FORMANT)
        for (int i = 0; i < L; i++) {
            double p = (double)i / L;
            c[i] = (float)(0.6 * (1.0 - p) * std::sin(2.0 * PI_F_P * res * p));
        }
    } break;
    }
}
static inline bool wtIsTimeDomain(int wt) {
    return wt == 19 || wt == 20;
}

// Bessel J_n(x), series form — used for a genuine 2-op FM spectrum.
static double wtBesselJ(int n, double x) {
    const double half = x * 0.5;
    double fact = 1.0;
    for (int i = 2; i <= n; i++) fact *= i;
    double term = std::pow(half, n) / fact, sum = 0.0;
    for (int m = 0; m < 40; m++) {
        sum += term;
        term *= -(half * half) / ((m + 1.0) * (m + 1.0 + n));
    }
    return sum;
}

// Fill mags[1..nHarm] for wavetable `wt` at a CONTINUOUS morph position t in [0,1].
// The WT oscillator only ever asks for the WT_FRAMES discrete frames (via the
// wtFrameHarmonics wrapper below), but PADsynth bakes one arbitrary morph position
// per table, so it needs the recipes evaluated off the frame grid.
static void wtFrameHarmonicsT(int wt, double t, float* mags, int nHarm);

// The only way in from outside. PAD_BW_CAP and the PAD_TABLES diversity rule are both
// SOLVED from these recipes, so re-picking either needs the spectra a tool can read —
// and the numbers have to come from the same function the build uses, not from a copy
// of it in a scratch file that drifts. Time-domain tables (see wtIsTimeDomain) have no
// recipe and come back all-zero. tools/pad-tables is the consumer.
void wtHarmonicsAt(int wt, double t, float* mags, int nHarm) {
    wtFrameHarmonicsT(wt, t, mags, nHarm);
}

static void wtFrameHarmonicsT(int wt, double t, float* mags, int nHarm) {
    const double A = 2.0 / PI_F_P;   // fundamental ~= analytic saw level
    for (int i = 0; i <= nHarm; i++) mags[i] = 0.f;
    switch (wt) {
    case 0: // Basic: sine → saw → square
        for (int n = 1; n <= nHarm; n++) {
            double sine = (n == 1) ? 1.0 : 0.0, saw = 1.0 / n, sq = (n & 1) ? 1.0 / n : 0.0, v;
            if (t < 0.5) { double u = t * 2.0;       v = (1 - u) * sine + u * saw; }
            else         { double u = (t - 0.5) * 2; v = (1 - u) * saw  + u * sq;  }
            mags[n] = (float)(A * v);
        }
        break;
    case 1: { // Harmonic climb: add harmonics 1..H with a soft top edge.
              // H is GEOMETRIC in t, not linear: harmonic number is heard
              // logarithmically, so 1→32 spread linearly put 60% of the audible
              // change in the first morph step and almost none in the last.
              // Endpoints are unchanged; only the spacing between them moves.
        double H = std::pow(32.0, t);
        for (int n = 1; n <= nHarm; n++) {
            double edge = H - n + 1.0; edge = edge < 0 ? 0 : (edge > 1 ? 1 : edge);
            mags[n] = (float)(A / n * edge);
        }
    } break;
    case 2: // Odd harmonics: sine → square (hollow)
        for (int n = 1; n <= nHarm; n += 2)
            mags[n] = (float)(A / n * (n == 1 ? 1.0 : std::pow(t, (n - 1) * 0.5)));
        break;
    case 3: { // Formant: a resonant "wah" — a main formant peak sweeps up the spectrum
              // with a weaker second formant an octave-ish above it, over a thin buzz.
        double hc = 2.0 + t * 26.0;          // primary formant centre — LINEAR on purpose:
                                             // a narrow peak sweeping a dense spectrum
                                             // changes the shape evenly when it moves by
                                             // equal harmonic counts; geometric spacing
                                             // measured 1.9%/33.9% first-vs-last step.
        double hc2 = hc * 1.9;               // secondary formant tracks above
        double w1 = 2.0 + hc * 0.10;         // resonances broaden as they climb
        double w2 = 3.0 + hc2 * 0.10;
        for (int n = 1; n <= nHarm; n++) {
            double buzz = 0.06 / n;          // faint source so gaps aren't silent
            double f1 = std::exp(-((n - hc) * (n - hc)) / (2 * w1 * w1));
            double f2 = 0.45 * std::exp(-((n - hc2) * (n - hc2)) / (2 * w2 * w2));
            mags[n] = (float)(A * (buzz + (f1 + f2) / std::sqrt((double)n)));
        }
    } break;
    case 4: { // Vowel: morph ah→eh→ee→oh→oo through 3 formants (harmonic pos @ ~150Hz f0).
              // F1 dominates, F2/F3 progressively quieter and broader, over a thin glottal
              // buzz with a gentle spectral tilt so the upper formants stay present.
        static const double vf[5][3] = {{5,8,17},{4,12,16},{2,15,20},{4,6,16},{2,6,15}};
        static const double fa[3]    = {1.0, 0.55, 0.28};   // formant amplitudes
        double vp = t * 4.0; int v0 = (int)vp; if (v0 > 3) v0 = 3; double m = vp - v0;
        for (int n = 1; n <= nHarm; n++) {
            double e = 0;
            for (int k = 0; k < 3; k++) {
                double fc = vf[v0][k] + (vf[v0 + 1][k] - vf[v0][k]) * m;
                double w  = 1.2 + k * 0.9;                  // higher formants broader
                e += fa[k] * std::exp(-((n - fc) * (n - fc)) / (2 * w * w));
            }
            mags[n] = (float)(A * (0.05 / n + e / std::sqrt((double)n)));
        }
    } break;
    case 5: { // Metallic: a handful of sparse, narrow partials at bell-like positions that
              // spread apart as t rises — a periodic-table approximation of an inharmonic
              // clang (weak fundamental, bright upper partials, no dense harmonic series).
        static const double bp[6] = {1, 2, 2.8, 4.2, 5.4, 7.6};  // near-bell ratios
        static const double ba[6] = {0.35, 1.0, 0.7, 0.55, 0.4, 0.3};
        double spread = 1.0 + t * 2.2, w = 0.9;
        for (int k = 0; k < 6; k++) {
            double fc = bp[k] * spread;            // partials fan out with the morph
            for (int n = 1; n <= nHarm; n++)
                mags[n] += (float)(A * ba[k] * std::exp(-((n - fc) * (n - fc)) / (2 * w * w)));
        }
    } break;
    case 6: { // Pulse sweep: rectangular duty 0.5 (square) → thin, normalized to fundamental
        double d = 0.5 - 0.45 * t, s1 = std::sin(PI_F_P * d);
        for (int n = 1; n <= nHarm; n++)
            mags[n] = (float)(A * std::sin(n * PI_F_P * d) / (n * (std::fabs(s1) < 1e-6 ? 1e-6 : s1)));
    } break;
    case 7: { // Soft: sine → triangle → dark saw. The only 1/n² spectra in the set
              // (everything else is 1/n or brighter) — the mellow/bass table.
        if (t < 0.5) {           // sine → triangle: odd 1/n², fading in like ODD
            double u = t * 2.0;
            for (int n = 1; n <= nHarm; n += 2)
                mags[n] = (float)(A / ((double)n * n) * (n == 1 ? 1.0 : std::pow(u, (n - 1) * 0.5)));
        } else {                 // triangle → dark saw: even 1/n² harmonics fade in
            double u = (t - 0.5) * 2.0;
            for (int n = 1; n <= nHarm; n++)
                mags[n] = (float)(A / ((double)n * n) * ((n & 1) ? 1.0 : u));
        }
    } break;
    case 8: { // Organ: Hammond drawbar additive, upper drawbars swell in
        double amp[9] = {0, 1.0, 0.8, 0.2 + 0.6 * t, 0.15 + 0.5 * t, 0, 0.4 * t, 0, 0.3 * t};
        for (int n = 1; n <= 8; n++) mags[n] = (float)(A * amp[n]);
    } break;
    case 9: { // Fifths: a root SAW plus a full saw series a twelfth up (3×) swelling
              // in — power-chord saws, not sine drawbars (that's ORGAN's turf).
        for (int n = 1; n <= nHarm; n++) mags[n] = (float)(A / n);
        double g = 0.15 + 0.75 * t;
        for (int k = 1; 3 * k <= nHarm; k++) mags[3 * k] += (float)(A * g / k);
    } break;
    case 10: { // Growl: two formants sweeping apart over a saw (animated/aggressive)
               //
               // The formants ride at 1/sqrt(n) over a 1/n saw body — the same split
               // FORMANT (case 3) uses, and for the same reason. This used to put the
               // WHOLE expression under A/n, formants included, so the upper formant
               // arrived at h20 scaled by 1/20 and never reached the output: measured,
               // the spectral centroid moved by a factor of 1.45 across the entire
               // morph and ended LOWER than it started, against 14.63 for FORMANT doing
               // the same thing, and the two morph endpoints were 96% identical. The
               // table's headline feature was inaudible.
               //
               // The saw body stays at 1/n — that is what separates this from FORMANT,
               // whose source is a deliberately thin 0.06/n buzz. Its weight is NOT the
               // lever it looks like: swept 0.25 → 0.70 the centroid span only moves
               // 1.64 → 1.47, because the low harmonics outweigh a /sqrt(n) formant up
               // at h20 either way. 0.35 is the middle of that, and a 1.59 span is
               // normal for a table that keeps a body (ORGAN 1.75, FIFTHS 1.57);
               // FORMANT reaches 14.63 only by having almost no body at all.
        double h1 = 3.0 - t * 1.5, h2 = 4.0 + t * 16.0;   // linear: moving peak
        double w1 = 2.5, w2 = 2.5 + h2 * 0.10;            // the climbing one broadens
        for (int n = 1; n <= nHarm; n++) {
            double saw = 0.35 / n;
            double f1 = std::exp(-((n - h1) * (n - h1)) / (2 * w1 * w1));
            double f2 = std::exp(-((n - h2) * (n - h2)) / (2 * w2 * w2));
            mags[n] = (float)(A * (saw + (f1 + f2) / std::sqrt((double)n)));
        }
    } break;
    case 11: { // Chime: sparse octave-spaced harmonics (glassy bell)
        for (int k = 0, n = 1; n <= nHarm; k++, n <<= 1)
            mags[n] = (float)(A * std::pow(0.6, k) * (k == 0 ? 1.0 : 0.3 + 0.7 * t));
    } break;
    case 12: { // Digital: deterministic pseudo-random spectrum per frame (evolving).
               // The draw is keyed to the DISCRETE frame, not to t — interpolating two
               // independent random spectra just averages them toward flat, so the
               // morph steps between draws instead of sliding. The draw count is a
               // FIXED 8 rather than this table's frame count: it is the sound, not a
               // resolution, so giving DIGITAL more frames must not change it.
               //
               // THE FUNDAMENTAL IS NOT PART OF THE DRAW. It used to be — mags[1] was
               // A * wtHash01(...) like every other harmonic — so a draw whose hash at
               // n = 1 came out near zero lost the fundamental outright and the sound
               // jumped up an octave or more and back as the morph stepped past it.
               // Measured across all 27 recipes, this was the ONLY table besides the
               // old EPIANO whose fundamental collapses mid-morph and returns. A random
               // TIMBRE is the intent here; a random PITCH is not.
               //
               // DIG_FUND IS 0.40, NOT 1.0 — the level the old random draw AVERAGED,
               // not a full-strength fundamental. Pinning it at 1.0 fixes the null but
               // makes h1 four times h2 where it used to be twice, so every draw becomes
               // a fundamental-dominated near-saw and they all sound alike: measured,
               // t=0 vs t=1 similarity went to 0.99, i.e. the morph stops doing
               // anything. The whole point of this table is that consecutive draws
               // differ. Measured against the original (fund fraction 0.52/0.86, NULL,
               // reversal 0.76, t0-t1 similarity 0.90):
               //
               //   DIG_FUND 0.40  0.58/0.51  no null  rev 0.27  sim 0.96   <- chosen
               //            0.55  0.72/0.67  no null  rev 0.21  sim 0.97
               //            0.70  0.81/0.76  no null  rev 0.15  sim 0.98   too static
               //            1.00  0.89/0.87  no null  rev 0.09  sim 0.99   too static
               //
               // 0.40 keeps the original's spectral balance, removes the null, cuts the
               // reversal to a third, and happens to shrink the worst harmonic gap from
               // 6 to 3. Raising it trades the table's character for numbers that only
               // look better.
               //
               // DIG_FLOOR stops a single harmonic reaching exactly zero. It is a
               // smaller effect than DIG_FUND — sweeping it 0.00 to 0.25 moved nothing
               // by more than a hair — so it is there to keep draws comparable in
               // brightness, not as the fix for anything.
        static constexpr double DIG_FLOOR = 0.10, DIG_FUND = 0.40;
        const uint32_t fq = (uint32_t)std::lround(t * (WT_DIGITAL_DRAWS - 1));
        mags[1] = (float)(A * DIG_FUND);
        for (int n = 2; n <= nHarm; n++) {
            const double h = wtHash01((uint32_t)n * 2654435761u + fq * 40503u);
            mags[n] = (float)(A / n * (DIG_FLOOR + (1.0 - DIG_FLOOR) * h));
        }
    } break;
    case 13: { // FM: genuine 2-op FM spectrum (carrier=mod=1), index morphs the brightness
        double beta = t * 7.0;
        int hi = nHarm < 96 ? nHarm : 96;
        for (int n = 1; n <= hi; n++) mags[n] = (float)(A * wtBesselJ(n - 1, beta));
    } break;
    case 14: { // Comb: saw through resonant comb PEAKS whose spacing morphs (flangey/
               // metallic). Was |sin| notches — spectrally the same family as PULSE
               // (both |sin(nπd)|/n), so the notches became peaks to separate them.
        double period = 2.0 + t * 10.0;
        for (int n = 1; n <= nHarm; n++) {
            double peak = 0.5 + 0.5 * std::cos(2.0 * PI_F_P * n / period);
            mags[n] = (float)(A / n * (0.12 + 0.88 * peak * peak * peak));
        }
    } break;
    case 15: { // Shaper: even-harmonic emphasis (asymmetric/tube warmth, octave-up)
        mags[1] = (float)A;
        for (int n = 2; n <= nHarm; n++)
            mags[n] = (float)((n & 1) ? A / n * 0.15 * (1.0 - t * 0.5)
                                      : A / n * (0.3 + 0.9 * t));
    } break;
    case 16: { // Sweep: a single narrow partial sweeping up the spectrum (pure whistle)
               // over a faint 1/n floor — without the floor the top frames go SILENT
               // at high notes (partial beyond Nyquist, mipmap truncates everything).
        double h = 1.0 + t * 40.0, w = 1.2;      // linear: moving peak (see FORMANT)
        for (int n = 1; n <= nHarm; n++)
            mags[n] = (float)(A * (0.04 / n + std::exp(-((n - h) * (n - h)) / (2 * w * w))));
    } break;
    case 17: { // Hollow: only every 3rd harmonic, climbing (nasal/reedy)
        double H = 1.0 + t * 30.0;               // LINEAR: harmonics are 3 apart here, so
                                                 // a geometric H sat below 4 for two whole
                                                 // frames and nothing happened at all
        for (int n = 1; n <= nHarm; n++)
            if ((n - 1) % 3 == 0) {
                double edge = H - n + 1.0; edge = edge < 0 ? 0 : (edge > 1 ? 1 : edge);
                mags[n] = (float)(A / n * edge);
            }
    } break;
    case 18: { // Air: a wide high-frequency band, center rising (breathy/thin), over a
               // faint 1/n floor (same Nyquist-silence guard as SWEEP).
        double c = 15.0 + t * 35.0, w = 8.0;     // linear: moving band (see FORMANT)
        for (int n = 1; n <= nHarm; n++)
            mags[n] = (float)(A * (0.04 / n
                              + std::exp(-((n - c) * (n - c)) / (2 * w * w)) / std::sqrt((double)n)));
    } break;
    case 23: { // Reso: a saw through a resonant low-pass whose cutoff opens across the
               // morph — 24 dB/oct rolloff above the cutoff plus a resonant peak on it.
        double hc = 1.5 * std::pow(41.5 / 1.5, t);   // cutoff climbs GEOMETRICALLY — a
                                                     // filter sweep is heard in octaves
        double rw = 0.7 + hc * 0.06;             // resonance bandwidth widens with cutoff
        for (int n = 1; n <= nHarm; n++) {
            double lp   = 1.0 / (1.0 + std::pow(n / hc, 4.0));            // butterworth-ish
            double peak = 0.9 * std::exp(-((n - hc) * (n - hc)) / (2 * rw * rw));
            mags[n] = (float)(A / n * lp + A * peak / std::sqrt((double)n));
        }
    } break;
    case 24: { // Buzz: flat-spectrum bandlimited impulse train, brightness edge sweeping up
               // (all harmonics equal — brassier/reedier than Climb's 1/n slope). Loudness
               // held roughly constant by scaling with the active-harmonic count.
        double H = std::pow(46.0, t);            // geometric edge sweep
        double amp = A * 0.7 / std::sqrt(H);
        for (int n = 1; n <= nHarm; n++) {
            double edge = H - n + 1.0; edge = edge < 0 ? 0 : (edge > 1 ? 1 : edge);
            mags[n] = (float)(amp * edge);
        }
    } break;
    case 25: { // E-piano: TWO FM pairs summed — a ratio-1 BODY and a ratio-14 TINE.
               //
               // It used to be the tine pair alone: one modulator at ratio 14, so the
               // carrier was unmodulated and the only thing FM added was an isolated
               // cluster at h13/h15/h27/h29. Measured, that put 94-99.9% of the energy
               // in the fundamental over the lower half of the morph and never had more
               // than 3-11 non-zero partials out of 82, with nothing at all between h1
               // and h13. Ear-reported on both WT and PAD as "just a sine wave and then
               // some very high frequency jitter when morph moves" — which is exactly
               // what a bare carrier plus a detached, Bessel-modulated cluster is.
               //
               // A real E-piano is two operator pairs: a low-ratio one that makes the
               // body and a high-ratio one that makes the tine ping. The second half of
               // that idea — the tine being an ATTACK TRANSIENT under a fast envelope —
               // a static wavetable cannot express, so the tine is mixed in at a fixed
               // level instead of being enveloped.
               //
               // Both indices stay below 2.405, J0's first zero. The old single beta ran
               // to 3.2 and crossed it at t = 0.735, where the carrier vanished outright
               // and left only the cluster (and, because the recipe takes |J0|, bounced
               // back off zero rather than passing through).
        const int MB = 1, MT = 14;
        const double bb   = 0.6 + t * 1.4;   // body index  0.60 .. 2.00
        const double bt   = 0.3 + t * 1.7;   // tine index  0.30 .. 2.00
        const double tine = 0.5;             // tine level against the body
        for (int k = 0; k <= 10; k++) {
            const double ab = std::fabs(wtBesselJ(k, bb));
            const double at = std::fabs(wtBesselJ(k, bt)) * tine;
            if (k == 0) { mags[1] += (float)(A * (ab + at)); continue; }
            const int ub = 1 + k * MB, db = std::abs(1 - k * MB);
            if (ub >= 1 && ub <= nHarm) mags[ub] += (float)(A * ab / std::sqrt((double)ub));
            if (db >= 1 && db <= nHarm) mags[db] += (float)(A * ab / std::sqrt((double)db));
            const int ut = 1 + k * MT, dt = std::abs(1 - k * MT);
            if (ut >= 1 && ut <= nHarm) mags[ut] += (float)(A * at / std::sqrt((double)ut));
            if (dt >= 1 && dt <= nHarm) mags[dt] += (float)(A * at / std::sqrt((double)dt));
        }
    } break;
    case 21: { // FM2: genuine 2-op FM at mod ratio 3 (clangy DX-bass/bell territory) —
               // sits between FM (ratio 1, smooth) and EPIANO (ratio 14, tine).
        const int M = 3;
        double beta = 0.3 + t * 5.7;
        for (int k = 0; k <= 12; k++) {
            double a = std::fabs(wtBesselJ(k, beta));
            if (k == 0) { mags[1] += (float)(A * a); continue; }
            int up = 1 + k * M, dn = std::abs(1 - k * M);
            if (up >= 1 && up <= nHarm) mags[up] += (float)(A * a / std::sqrt((double)up));
            if (dn >= 1 && dn <= nHarm) mags[dn] += (float)(A * a / std::sqrt((double)dn));
        }
    } break;
    case 22: { // Pluck: ideal-string spectrum |sin(πnd)|/n^e — d is the pluck position
               // (comb tightening across the morph), the strong 1/n^~2 rolloff is the
               // string damping that keeps it dark (PULSE is the bright flat cousin).
        double d = 0.35 - 0.27 * t;              // pluck point moves toward the bridge
        double e = 2.2 - 0.7 * t;                // brightens slightly as it does
        for (int n = 1; n <= nHarm; n++) {
            double body = 0.15 * std::exp(-((n - 2.5) * (n - 2.5)) / (2 * 1.5 * 1.5));
            mags[n] = (float)(A * (std::fabs(std::sin(PI_F_P * n * d)) / std::pow((double)n, e) + body / n));
        }
    } break;
    case 26: { // Stack: root saw + octave saw + 2-octave saw swelling in sequence —
               // the dense octave-stack wave (saw series, distinct from ORGAN's
               // sine drawbars and FIFTHS' twelfth).
        double o1 = std::min(1.0, t * 2.0) * 0.85;        // octave swells first
        double o2 = std::max(0.0, t * 2.0 - 1.0) * 0.75;  // then the 2-octave
        double r  = 1.0 - 0.35 * t;                       // root fades as they rise
        for (int n = 1; n <= nHarm; n++) mags[n] = (float)(A * r / n);
        for (int k = 1; 2 * k <= nHarm; k++) mags[2 * k] += (float)(A * o1 / k);
        for (int k = 1; 4 * k <= nHarm; k++) mags[4 * k] += (float)(A * o2 / k);
    } break;
    default: // safe fallback: plain fundamental
        mags[1] = (float)A;
        break;
    }
}

// Fill mags[1..nHarm] for wavetable `wt`, frame f of N — the WT oscillator's view.
static void wtFrameHarmonics(int wt, int f, int N, float* mags, int nHarm) {
    wtFrameHarmonicsT(wt, (N > 1) ? (double)f / (N - 1) : 0.0, mags, nHarm);
}

// RING's carrier has to be an INTEGER multiple of the cycle or the ring-modulated
// table stops being periodic in L and the wrap point breaks, so its amount axis is a
// staircase however it is written. It used to be four OCTAVE steps (k = 1,2,4,8),
// which meant most A→B ranges baked duplicate levels — 0.6→0.7 gave k = 4,4,4,4 and
// did nothing at all across the whole range, and dragging A or B hauled a level over
// one of the three hard edges, which is what jumped. A linear ladder of eight integer
// carriers keeps BOTH endpoints exactly as they were (k = 1 at amt 0, k = 8 at amt 1)
// and fills in the 3, 5, 6, 7 that were missing. Intermediate amounts therefore sound
// different from before; the extremes do not.
static constexpr int WT_RING_KMAX = 8;
static inline int wtRingIndex(float amt) {
    const int i = (int)std::lround(amt * (WT_RING_KMAX - 1));
    return i < 0 ? 0 : (i > WT_RING_KMAX - 1 ? WT_RING_KMAX - 1 : i);
}
static inline int wtRingK(float amt) { return 1 + wtRingIndex(amt); }

// Read-phase warp — applied at BUILD time (see warpCycle) to bake the warp into
// the bandlimited tables, so it's alias-free. Applies to the wavetable read, not
// the accumulator, so pitch is unchanged: 1 SYNC (read the cycle faster and wrap,
// ratio to ×7 so it covers the range of the removed SYNC table), 2 BEND (Casio-CZ
// phase distortion toward a breakpoint — also the home of the removed CZ SAW table),
// 5 FORMANT (read faster and HOLD at the end instead of wrapping — shifts formants
// up without sync buzz; replaced baked CRUSH, whose grit couldn't survive the
// bandlimited build by design — BITS is the audible crush).
uint32_t warpPhase(uint32_t phs, int type, float amt) {
    if (type <= 0 || amt <= 0.f) return phs;
    const double p = (double)phs / (double)OSCBNK_PHSMAX_32;   // 0..1
    double wp;
    switch (type) {
    // Every driver below is GEOMETRIC in amt rather than arithmetic: sync ratio,
    // formant shift, breakpoint and step count are all heard logarithmically, so
    // linear spacing bunched the audible change at one end of the knob (measured:
    // SAT put 70% of its change in the first third, BITS 95% in the last).
    // Endpoints are identical to before — only the travel between them is evened out.
    case 1: wp = std::fmod(p * (1.0 + amt * 6.0), 1.0); break;                 // SYNC  1→7,
                                                                               // LINEAR: the
    // wrap count scales with the ratio, so shape change is already even; geometric
    // spacing measured 8.7% / 15.7% / 75.6% across the three steps.
    case 2: { const double d = 0.5 * std::pow(0.1, amt);                       // BEND (CZ) .5→.05
              wp = (p < d) ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d); } break;
    case 5: { wp = p * (1.0 + amt * 3.0);                                      // FORMANT 1→4,
                                                                               // linear, as SYNC
              if (wp > 1.0) wp = 1.0; } break;                                 // hold, don't wrap
    case 10: { const int steps = (int)std::lround(3.0 + (1.0 - amt) * 45.0);   // STEP 48→3,
                                                                               // linear measured
                                                                               // marginally better
               wp = std::floor(p * steps) / steps; } break;                    // synced to the cycle → alias-free
    default: return phs;   // amplitude-domain warps (RING/FOLD/SAT) leave phase alone
    }
    return ((uint32_t)(wp * (double)OSCBNK_PHSMAX_32)) & OSCBNK_PHSMSK;
}

// Amplitude-domain warp: reshapes the output sample value (rather than the read
// phase). 4 FOLD (overdriven wavefolder, reflect back into [-1,1]), 9 SAT (baked
// hard-clip drive). Like the phase warps these add harmonics and are only
// alias-free because they're baked through the bandlimited build.
MYFLOAT warpAmplitude(MYFLOAT x, int type, float amt) {
    if (amt <= 0.f) return x;
    if (type == 4) {                                   // FOLD
        double v = x * std::pow(5.0, amt);             // geometric drive 1→5
        while (v > 1.0 || v < -1.0) { if (v > 1.0) v = 2.0 - v; else v = -2.0 - v; }
        return v;
    }
    if (type == 9) {                                   // SAT (baked hard-clip drive)
        const double v = x * std::pow(16.0, amt);      // 0→+24 dB in EVEN dB steps; the
                                                       // old linear 1+15·amt spent 15 of
                                                       // those 24 dB in the first third
        return v > 1.0 ? 1.0 : (v < -1.0 ? -1.0 : v);  // baked+bandlimited → alias-free
    }
    return x;
}

// Bake a warp into one cycle: phase warps (SYNC/BEND/FORMANT/STEP) resample the
// read positions; amplitude warps (FOLD/SAT) reshape the values; RING (type 3,
// replaced ASYM — which measured 0.98 similar to BEND) multiplies by a sine at an
// integer multiple of the cycle so every harmonic gains ±k sidebands (metallic).
// k is octave-stepped per baked level; the runtime warp-amt interpolation
// crossfades adjacent levels, which stays smooth because each level is periodic.
static void warpCycle(const double* in, double* out, int L, int type, float amt) {
    if (type <= 0 || amt <= 0.f) { for (int i = 0; i < L; i++) out[i] = in[i]; return; }
    if (type == 3) {                                    // RING
        const int k = wtRingK(amt);
        for (int i = 0; i < L; i++)
            out[i] = 1.25 * in[i] * std::sin(2.0 * PI_F_P * k * i / L);
    } else if (type == 4 || type == 9) {                // amplitude domain (FOLD/SAT)
        for (int i = 0; i < L; i++) out[i] = warpAmplitude(in[i], type, amt);
    } else {                                            // phase domain: read in[] warped
        for (int i = 0; i < L; i++) {
            const uint32_t p  = (uint32_t)((double)i / L * (double)OSCBNK_PHSMAX_32) & OSCBNK_PHSMSK;
            const uint32_t wp = warpPhase(p, type, amt);
            const double pos  = (double)wp / (double)OSCBNK_PHSMAX_32 * L;
            int i0 = ((int)pos) % L; const double fr = pos - (int)pos;
            const int i1 = (i0 + 1) % L;
            out[i] = in[i0] + (in[i1] - in[i0]) * fr;
        }
    }
}

// A frame's base single cycle (length WT_CYCLE): time-domain tables give it directly;
// harmonic recipes are IFFT'd with setupFromHarmonics' amplitude convention, so a
// warp-0 harmonic frame is identical to the old direct build.
static void wtFrameCycleT(int table, double t, double* cycle, int L) {
    if (wtIsTimeDomain(table)) {
        std::vector<float> tmp(L, 0.f);
        wtFrameWaveformT(table, t, tmp.data(), L);
        for (int i = 0; i < L; i++) cycle[i] = tmp[i];
    } else {
        std::vector<float> mags(WT_HARMONIC + 1, 0.f);
        wtFrameHarmonicsT(table, t, mags.data(), WT_HARMONIC);
        CHECKFFT(L);
        std::vector<double> buf(L + 2, 0.0);
        const double scaleFac = 0.5 * L;
        const int maxh = std::min(L / 2, WT_HARMONIC);
        for (int k = 1; k <= maxh; k++) buf[(k << 1) + 1] = (double)mags[k] * scaleFac;
        buf[1] = buf[L]; buf[L] = 0.0;
        fft->backward(buf.data());
        for (int i = 0; i < L; i++) cycle[i] = buf[i];
    }
}

// Warp-type classes. Baked into the tables (alias-free): 1 SYNC/2 BEND/3 RING/4 FOLD/
// 5 FORMANT plus 9 SAT (hard-clip drive) and 10 STEP (cycle-synced lo-fi). Runtime
// waveshapers applied to the osc output (carry the broadband grit baking smooths
// away): 6 BITS/7 RATE/8 DRIVE. Runtime types use the clean (OFF) table set.
static inline bool wtWarpBaked(int t)   { return (t >= 1 && t <= 5) || t == 9 || t == 10; }
static inline bool wtWarpRuntime(int t) { return t >= 6 && t <= 8; }

// How many warp levels to bake for this type across this range. Most types are a
// smooth function of the amount, so a fixed few levels interpolate fine. RING is not:
// its carrier is an integer staircase (wtRingK), so it gets exactly ONE LEVEL PER
// DISTINCT CARRIER inside the range. No level is then a duplicate of its neighbour,
// and moving A or B moves the ENDS of the ladder instead of dragging a level across a
// hard edge — which is the whole of why it jumped. A range narrower than one carrier
// step collapses to a single level, because nothing changes across it.
static int wtWarpLevels(int warpType, const WtRange& r) {
    if (!wtWarpBaked(warpType))                  return 1;
    if (std::fabs(r.wB - r.wA) <= 1e-4f)         return 1;
    if (warpType == 3)                                          // RING
        return std::abs(wtRingIndex(r.wB) - wtRingIndex(r.wA)) + 1;
    return WT_WARP_LEVELS;
}

// Runtime-warp sample shaper (types 6-8). `hold`/`dph` carry the RATE
// sample-and-hold state across calls (unused by BITS/DRIVE). amt<=0 → identity.
static inline MYFLOAT runtimeWarpSample(int type, float amt, MYFLOAT x,
                                        MYFLOAT& hold, double& dph) {
    if (amt <= 0.f) { hold = x; return x; }
    switch (type) {
    case 6: { const double st = std::pow(32.0, 1.0 - amt); // BITS: 32 levels → 1, halving
              return std::round(x * st) / st; }            // per equal step (was linear,
                                                           // which hid 95% of the crush
                                                           // in the last third of the knob)
    case 7: { dph += std::pow(2.0, -(double)amt * 7.0);     // RATE: full → 1/128 rate
              if (dph >= 1.0) { dph -= 1.0; hold = x; }
              return hold; }
    case 8: { const double v = x * std::pow(16.0, (double)amt);  // DRIVE: 0→+24 dB, even dB
              return v > 1.0 ? 1.0 : (v < -1.0 ? -1.0 : v); }
    }
    return x;
}

// Build the 2D (morph × warp) set for one (table, warpType) ACROSS THE A→B RANGES.
//
// The frames are baked over exactly the span the oscillator will traverse, in that
// direction — B < A bakes backwards — so all of the resolution lands where it is
// used. That is what makes a narrow range sound right: eight frames stretched over a
// whole table crossfade badly wherever the morph moves a peak (SWEEP measured -0 dB
// of error), but the same eight frames inside a 20% window sit close enough together
// that the crossfade is fine. The runtime therefore feeds a NORMALISED 0..1 position
// (how far along the range modulation currently is), not the absolute value.
//
// A degenerate span collapses that axis to a single baked frame: nothing moves along
// it, so more would be identical copies.
static void buildWtSet(WavetableSet& set, int tableNum, int warpType, const WtRange& r) {
    const bool  mSpan  = std::fabs(r.mB - r.mA) > 1e-4f;
    const int   nMorph = mSpan ? wtMorphFrames(tableNum) : 1;
    const int   nWarp  = wtWarpLevels(warpType, r);
    set.nMorph = nMorph; set.nWarp = nWarp;
    set.frames.clear();
    set.frames.resize((size_t)nMorph * nWarp);
    std::vector<double> base(WT_CYCLE), warped(WT_CYCLE);
    std::vector<float>  cyc(WT_CYCLE);
    for (int m = 0; m < nMorph; m++) {
        const double mt = (nMorph > 1) ? (double)m / (nMorph - 1) : 0.0;
        wtFrameCycleT(tableNum, r.mA + mt * (r.mB - r.mA), base.data(), WT_CYCLE);
        for (int w = 0; w < nWarp; w++) {
            const double wt01 = (nWarp > 1) ? (double)w / (nWarp - 1) : 0.0;
            // an unwarped type still bakes at wA — with warp OFF that is 0 anyway
            const float amt = wtWarpBaked(warpType)
                            ? (float)(r.wA + wt01 * (r.wB - r.wA)) : 0.f;
            warpCycle(base.data(), warped.data(), WT_CYCLE, warpType, amt);
            for (int i = 0; i < WT_CYCLE; i++) cyc[i] = (float)warped[i];
            auto wtb = std::make_unique<WaveTables>();
            wtb->setupFromWaveform(cyc.data(), WT_CYCLE);
            set.frames[(size_t)m * nWarp + w] = std::move(wtb);
        }
    }
}

// ── Wavetable set cache & off-thread building ────────────────────────────────
// buildWtSet is far too heavy for the audio thread (up to 8×4 tables × ~34 mipmap
// FFTs). So: the audio thread only *looks up* a ready set (getWavetableSetIfReady)
// and, on a miss, requests an off-thread build (requestWavetableSet → UiTasks
// worker); it keeps its previous _wt as fallback until the build lands. The GUI/
// worker use the blocking getWavetableSet. Sets are shared via a weak cache and
// kept alive against note-off churn by a small strong LRU (the currently-selected
// sets), so releasing all notes no longer forces a rebuild on the next note.
// Since the frames are baked across the A→B ranges, the key carries those ranges too
// — it is no longer the bounded 27×16 space it used to be. Each end is quantised to
// WT_RANGE_STEPS so that dragging a fader produces a bounded number of distinct sets
// instead of one per pixel, and the SAME quantised values are what gets baked, so the
// runtime never normalises against a range the tables were not built for.
static constexpr int WT_RANGE_STEPS = 64;
static inline int   wtQ(float v)   { const int q = (int)std::lround(v * (WT_RANGE_STEPS - 1));
                                     return q < 0 ? 0 : (q > WT_RANGE_STEPS - 1 ? WT_RANGE_STEPS - 1 : q); }
static inline float wtDeQ(int q)   { return (float)q / (float)(WT_RANGE_STEPS - 1); }

static std::map<uint64_t, std::weak_ptr<WavetableSet>> gWtCache;   // dedup/sharing
static std::mutex gWtCacheMtx;                                // guards gWtCache + gWtWarm (tiny sections; never held during a build)
static constexpr int WT_WARM_MAX = 8;
static constexpr uint64_t WT_KEY_NONE = ~0ull;                // sentinel; every real key is smaller
static std::deque<std::pair<uint64_t, std::shared_ptr<WavetableSet>>> gWtWarm;  // strong LRU
static std::array<std::atomic<uint64_t>, 16> gWtInFlight{};   // keys building off-thread; lock-free

static inline uint64_t wtKey(int tableNum, int warpType, const WtRange& r) {
    if (tableNum < 0 || tableNum >= WT_NUM_TABLES) tableNum = 0;
    if (!wtWarpBaked(warpType)) warpType = 0;   // runtime/OFF warps share the clean table set
    return ((uint64_t)tableNum << 28) | ((uint64_t)warpType << 24)
         | ((uint64_t)wtQ(r.mA) << 18) | ((uint64_t)wtQ(r.mB) << 12)
         | ((uint64_t)wtQ(r.wA) <<  6) | ((uint64_t)wtQ(r.wB));
}

// The range actually baked, i.e. the key's quantised values — the runtime normalises
// against exactly this so the ends line up with the frames.
static inline WtRange wtQuantised(int warpType, const WtRange& r) {
    WtRange q{ wtDeQ(wtQ(r.mA)), wtDeQ(wtQ(r.mB)), wtDeQ(wtQ(r.wA)), wtDeQ(wtQ(r.wB)) };
    if (!wtWarpBaked(warpType)) { q.wA = q.wB = 0.f; }
    return q;
}

// Where the modulated absolute value sits along the baked span, 0..1. A collapsed
// span means that axis has a single baked frame, so there is nowhere to be but 0.
static inline MYFLOAT wtNormPos(MYFLOAT value, float a, float b) {
    const MYFLOAT span = (MYFLOAT)b - (MYFLOAT)a;
    if (std::fabs(span) < 1e-4) return 0.0;
    const MYFLOAT p = (value - (MYFLOAT)a) / span;
    return p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
}

static void wtInFlightInit() {   // one-time sentinel seed (key 0 is a valid key)
    static std::once_flag once;
    std::call_once(once, []{ for (auto& s : gWtInFlight) s.store(WT_KEY_NONE, std::memory_order_relaxed); });
}
static bool wtInFlightClaim(uint64_t key) {   // true if newly claimed (not already building)
    wtInFlightInit();
    for (auto& s : gWtInFlight) if (s.load(std::memory_order_acquire) == key) return false;
    for (auto& s : gWtInFlight) { uint64_t e = WT_KEY_NONE;
        if (s.compare_exchange_strong(e, key, std::memory_order_acq_rel)) return true; }
    return false;   // table full — skip; a later note retries
}
static void wtInFlightRelease(uint64_t key) {
    for (auto& s : gWtInFlight) { uint64_t k = key;
        if (s.compare_exchange_strong(k, WT_KEY_NONE, std::memory_order_acq_rel)) return; }
}

static void wtWarmPut(uint64_t key, const std::shared_ptr<WavetableSet>& sp) {   // caller holds gWtCacheMtx
    for (auto it = gWtWarm.begin(); it != gWtWarm.end(); ++it)
        if (it->first == key) { gWtWarm.erase(it); break; }
    gWtWarm.emplace_front(key, sp);
    while ((int)gWtWarm.size() > WT_WARM_MAX) gWtWarm.pop_back();
}

// Extract the display picture from a finished set. Runs on the worker, inside the
// build, so the render thread never does this: nf × POINTS iterations of four table
// reads and two interpolations, plus a normalise pass. It used to run on the render
// thread on every change, which is what this whole handover exists to stop.
//
// The runtime-warp post-pass is NOT applied — see the note on WtDisplayFrames.
static void extractDisplayFrames(const WavetableSet& ws, WtDisplayFrames& out) {
    const int nm = ws.nMorph, nw = ws.nWarp;
    out.n = 0;
    if (nm <= 0) return;
    // Tables carry DIFFERENT frame counts (wtMorphFrames) and some exceed what the
    // block can hold, so the slice count is capped and the range is resampled rather
    // than taking the first SLICES baked frames — otherwise a 33-frame table would
    // draw only part of itself while the caller maps the whole range across what it got.
    const int nf = nm < WtDisplayFrames::SLICES ? nm : WtDisplayFrames::SLICES;
    const int n  = WtDisplayFrames::POINTS;
    const MYFLOAT dispFreq = 30.0 / 44100.0;   // low freq → fullest mipmap

    for (int f = 0; f < nf; f++) {
        // t runs 0→1 across the stack, which IS the position along the baked range:
        // slice 0 is the current unmodulated sound (both axes at A), the last slice is
        // full modulation (both at B), and a B < A range was baked backwards so the
        // stack reads backwards with it.
        const float t = (nf > 1) ? (float)f / (nf - 1) : 0.f;

        int w0 = 0; float wf = 0.f;
        if (nw > 1) { float wp = t * (nw - 1); w0 = (int)wp; if (w0 > nw - 2) w0 = nw - 2; wf = wp - w0; }
        const int w1 = (nw > 1) ? w0 + 1 : w0;

        float fp = t * (nm - 1);
        int   m0 = (int)fp;
        if (m0 > nm - 2) m0 = nm > 1 ? nm - 2 : 0;
        const float mfrac = (nm > 1) ? fp - m0 : 0.f;
        const int   m1 = (nm > 1) ? m0 + 1 : m0;

        WaveTable& t00 = ws.at(m0, w0).getTable(44100.0, dispFreq);
        WaveTable& t01 = ws.at(m0, w1).getTable(44100.0, dispFreq);
        WaveTable& t10 = ws.at(m1, w0).getTable(44100.0, dispFreq);
        WaveTable& t11 = ws.at(m1, w1).getTable(44100.0, dispFreq);
        float* dst = out.f + (size_t)f * n;
        float mx = 1e-6f;
        for (int i = 0; i < n; i++) {
            // cell centres, for the same reason as getWavetableDisplay
            const uint32_t phs = (uint32_t)(((double)i + 0.5) / (double)n * (double)OSCBNK_PHSMAX_32) & OSCBNK_PHSMSK;
            const MYFLOAT a = t00.read(phs) + (t01.read(phs) - t00.read(phs)) * wf;
            const MYFLOAT b = t10.read(phs) + (t11.read(phs) - t10.read(phs)) * wf;
            const float v = (float)(a + (b - a) * mfrac);
            dst[i] = v;
            const float av = v < 0 ? -v : v;
            if (av > mx) mx = av;
        }
        const float g = 0.95f / mx;
        for (int i = 0; i < n; i++) dst[i] *= g;
    }
    out.n = nf;
}

// Blocking build — safe on any thread EXCEPT the audio thread. Heavy buildWtSet runs
// outside the lock so audio-thread lookups never wait on a build.
std::shared_ptr<WavetableSet> getWavetableSet(int tableNum, int warpType, const WtRange& range) {
    const uint64_t key = wtKey(tableNum, warpType, range);
    {
        std::lock_guard<std::mutex> lk(gWtCacheMtx);
        if (auto it = gWtCache.find(key); it != gWtCache.end())
            if (auto sp = it->second.lock()) { wtWarmPut(key, sp); return sp; }
    }
    const int t = (tableNum < 0 || tableNum >= WT_NUM_TABLES) ? 0 : tableNum;
    const int w = wtWarpBaked(warpType) ? warpType : 0;
    auto sp = std::make_shared<WavetableSet>();
    buildWtSet(*sp, t, w, wtQuantised(warpType, range));   // heavy — outside the lock
    {   // the picture, extracted once here rather than per change on the render thread
        auto d = std::make_shared<WtDisplayFrames>();
        extractDisplayFrames(*sp, *d);
        sp->display = std::move(d);
    }
    std::lock_guard<std::mutex> lk(gWtCacheMtx);
    if (auto it = gWtCache.find(key); it != gWtCache.end())
        if (auto ex = it->second.lock()) { wtWarmPut(key, ex); return ex; }   // lost a build race
    gWtCache[key] = sp;
    wtWarmPut(key, sp);
    return sp;
}

// Audio-thread lookup: a ready set or nullptr (never builds). Only tiny map/deque
// ops under the lock.
static std::shared_ptr<WavetableSet> getWavetableSetIfReady(int tableNum, int warpType,
                                                            const WtRange& range) {
    const uint64_t key = wtKey(tableNum, warpType, range);
    std::lock_guard<std::mutex> lk(gWtCacheMtx);
    for (auto& e : gWtWarm) if (e.first == key) return e.second;
    if (auto it = gWtCache.find(key); it != gWtCache.end())
        if (auto sp = it->second.lock()) { wtWarmPut(key, sp); return sp; }
    return nullptr;
}

// How many wavetable/PADsynth builds are running right now. Lock-free, so the GUI
// can poll it every frame to tell the user why the picture (or the sound) has not
// caught up yet. Counting rather than a bool: dragging a fader legitimately has
// several distinct keys in flight, and "3 building" is more honest than "busy".
int wtBuildsInFlight() {
    // MUST seed. gWtInFlight is zero-initialised and key 0 is a valid key, so the
    // sentinel cannot be 0 -- until wtInFlightInit() has run, all 16 slots read 0,
    // which is "not WT_KEY_NONE", and this returns 16 on a system where nothing has
    // ever been built. wtInFlightClaim() used to be the only thing that seeded it,
    // so any reader that ran before the first request read garbage.
    //
    // The wavetable display used to hide it: it requested a set (and so seeded)
    // earlier in the same render than it read this. Nothing guaranteed that ordering
    // for any other caller, and the build overlay -- which polled this cold on its
    // first frame -- sat there permanently because of it.
    //
    // NOTE: both counters currently have no callers. The display stopped polling when
    // it moved to the published-block handover, and the keyboard panel is push/pop.
    wtInFlightInit();
    int n = 0;
    for (auto& s : gWtInFlight)
        if (s.load(std::memory_order_relaxed) != WT_KEY_NONE) ++n;
    return n;
}

// Settle time before a wavetable build is allowed to start — the same mechanism
// PADsynth already has, and for the same reason.
//
// MORPH A/B and WARP A/B are part of the cache key, quantised to WT_RANGE_STEPS, so
// one drag of a fader mints up to 63 distinct keys. Nothing cancels a task once it is
// queued, so every value the fader passed through got its own build: the 16-slot
// UiTasks queue overflows, add_task starts failing, and the builds that do run are
// mostly for positions the fader already left. Worse, that queue is shared — the
// deallocation handoffs, preset saves and MIDI-learn all sit behind the backlog.
//
// "Only one build per key is ever in flight" was never coalescing. Waiting for the
// key to HOLD STILL is: a value swept past never reaches the queue at all.
//
// Shorter than PAD's 180 ms because a wavetable build is far cheaper and this delay
// is felt directly — it is the lag between letting go of a fader and hearing the
// result. 60 ms is below what reads as lag while still being several blocks longer
// than the fastest useful drag produces.
static constexpr int64_t WT_SETTLE_NS = 60'000'000;   // 60 ms
struct WtPend { std::atomic<uint64_t> key{WT_KEY_NONE}; std::atomic<int64_t> since{0}; };
static std::array<WtPend, 16> gWtPend{};

static void wtPendInit() {
    static std::once_flag once;
    std::call_once(once, []{ for (auto& s : gWtPend) s.key.store(WT_KEY_NONE, std::memory_order_relaxed); });
}

// true once this key has been asked for continuously for WT_SETTLE_NS.
static bool wtSettled(uint64_t key, int64_t now) {
    wtPendInit();
    for (auto& s : gWtPend)
        if (s.key.load(std::memory_order_acquire) == key)
            return (now - s.since.load(std::memory_order_relaxed)) >= WT_SETTLE_NS;
    for (auto& s : gWtPend) {                       // first sighting: start its clock
        uint64_t e = WT_KEY_NONE;
        if (s.key.compare_exchange_strong(e, key, std::memory_order_acq_rel)) {
            s.since.store(now, std::memory_order_relaxed);
            return false;
        }
    }
    WtPend* oldest = &gWtPend[0];                   // full: recycle the stalest slot
    for (auto& s : gWtPend)
        if (s.since.load(std::memory_order_relaxed) < oldest->since.load(std::memory_order_relaxed))
            oldest = &s;
    oldest->key.store(key, std::memory_order_release);
    oldest->since.store(now, std::memory_order_relaxed);
    return false;
}

// Request an off-thread build via the UiTasks worker (idempotent, lock-free on the
// caller side). No-op if it's already building; the blocking build inside dedups
// against anything already cached.
//
// `urgent` skips the settle wait. Note-on uses it: a note has to sound now, and its
// key is not a value something is sweeping through. The PADsynth twin of this function
// deliberately has no equivalent — see the note above requestPadSet for why it does not
// carry over.
static void requestWavetableSet(tsl::AppState* app, int tableNum, int warpType,
                                const WtRange& range, bool urgent = false) {
    if (!app) return;
    const uint64_t key = wtKey(tableNum, warpType, range);
    if (!urgent && !wtSettled(key, tsl::time::nanosecondsSinceEpoch())) return;
    // ALREADY BUILT? Then there is nothing to do. Without this the warm path queues a
    // task every block for the life of the patch: the settle passes (the key has been
    // stable for ages), the claim succeeds (the last build released its slot), and the
    // task runs, hits the cache and returns instantly. Harmless-looking, ~750 no-op
    // tasks a second per oscillator into the 16-slot shared queue -- and once anything
    // hangs its lifetime off "a build is running", it never stops running.
    if (getWavetableSetIfReady(tableNum, warpType, range)) return;
    if (!wtInFlightClaim(key)) return;
    if (!app->UiTasksQueue.add_task([tableNum, warpType, range, key, app]() {
            // Panel over the keyboard for as long as this build runs. Constructed
            // here, on the worker, NOT at the call site -- the per-block caller is
            // the audio thread, which must never touch queue_draw.
            tsl::app::BuildOverlayScope panel(app);
            getWavetableSet(tableNum, warpType, range);   // builds + warms the LRU
            wtInFlightRelease(key);
        }))
        wtInFlightRelease(key);   // queue full — let a later note re-request
}

// Warm-up entry point for the per-block path — see the note in vco.h. Deliberately
// the same call refreshWt() makes on a miss, so the key it warms is byte-identical
// to the one the oscillator will look up at note-on; warming a different key would
// silently do nothing (the display path does exactly that, asking for the full-range
// set, which is why the picture updates on a preset change while the sound does not).
void wtWarmRequest(tsl::AppState* app, int tableNum, int warpType, const WtRange& range) {
    requestWavetableSet(app, tableNum, warpType, range);
}

void getWavetableDisplay(int tableNum, float morph, int warpType, float warpAmt, float* out, int n) {
    for (int i = 0; i < n; i++) out[i] = 0.f;
    // absolute positions, so this asks for the full-range set and indexes into it
    auto ws = getWavetableSet(tableNum, warpType, WtRange{0.f, 1.f, 0.f, 1.f});
    const int nm = ws ? ws->nMorph : 0, nw = ws ? ws->nWarp : 0;
    if (nm <= 0) return;
    const MYFLOAT dispFreq = 30.0 / 44100.0;   // low freq → fullest mipmap
    float m = morph < 0.f ? 0.f : (morph > 1.f ? 1.f : morph);
    int f0 = 0; float mf = 0.f;
    if (nm > 1) { float fp = m * (nm - 1); f0 = (int)fp; if (f0 > nm - 2) f0 = nm - 2; mf = fp - f0; }
    const int f1 = (nm > 1) ? f0 + 1 : f0;
    float wa = warpAmt < 0.f ? 0.f : (warpAmt > 1.f ? 1.f : warpAmt);
    int w0 = 0; float wf = 0.f;
    if (nw > 1) { float wp = wa * (nw - 1); w0 = (int)wp; if (w0 > nw - 2) w0 = nw - 2; wf = wp - w0; }
    const int w1 = (nw > 1) ? w0 + 1 : w0;
    WaveTable& t00 = ws->at(f0, w0).getTable(44100.0, dispFreq);
    WaveTable& t01 = ws->at(f0, w1).getTable(44100.0, dispFreq);
    WaveTable& t10 = ws->at(f1, w0).getTable(44100.0, dispFreq);
    WaveTable& t11 = ws->at(f1, w1).getTable(44100.0, dispFreq);
    float mx = 1e-6f;
    for (int i = 0; i < n; i++) {
        // Sample the CENTRE of each display cell, not its left edge. Phase 0 is the
        // midpoint of the waveform's wrap discontinuity, and every sine-phase table
        // is exactly 0 there — so cell 0 used to land on the centre line while cell 1
        // was already at full amplitude, drawing a half-height vertical stub before
        // the waveform proper started (and a small hook back at the right edge).
        const uint32_t phs = (uint32_t)(((double)i + 0.5) / (double)n * (double)OSCBNK_PHSMAX_32) & OSCBNK_PHSMSK;
        const MYFLOAT s00 = t00.read(phs), s01 = t01.read(phs), s10 = t10.read(phs), s11 = t11.read(phs);
        const MYFLOAT a = s00 + (s01 - s00) * wf, b = s10 + (s11 - s10) * wf;
        const float v = (float)(a + (b - a) * mf);
        out[i] = v;
        const float av = v < 0 ? -v : v;
        if (av > mx) mx = av;
    }
    const float g = 0.95f / mx;   // peak-normalise so every table fills the display
    for (int i = 0; i < n; i++) out[i] *= g;
    // runtime warps aren't in the table set — shape the normalised display samples so
    // the scope shows BITS/RATE/DRIVE (state is local to this one-cycle pass)
    if (wtWarpRuntime(warpType) && warpAmt > 0.f) {
        MYFLOAT hold = 0; double dph = 0;
        for (int i = 0; i < n; i++)
            out[i] = (float)runtimeWarpSample(warpType, warpAmt, out[i], hold, dph);
    }
}

// ── Display handover ─────────────────────────────────────────────────────────
// SPSC, latest-wins, and deliberately not a general queue.
//
// THE PRODUCER MUST NEVER DESTROY A shared_ptr. It writes only into slots the
// consumer has already moved out of (guaranteed by the `w - r < CAP` test), so every
// slot it assigns to holds nullptr and the assignment cannot run a destructor. That
// is what keeps a delete — even a 7 KB one, even a malloc-lock — off the audio
// thread. If the ring is full the publish is dropped: the block being dropped is an
// intermediate selection the display would have replaced anyway, and the next block
// republishes because lastKey is only advanced on a successful publish.
namespace {
struct WtDispBox {
    static constexpr int CAP = 4;
    std::shared_ptr<const WtDisplayFrames> slot[CAP];
    std::atomic<unsigned> w{0}, r{0};
    uint64_t lastKey{WT_KEY_NONE};   // audio thread only
};
std::array<WtDispBox, WT_DISPLAY_OSCS> gWtDispBox{};
}

void wtPublishDisplay(int osc, int tableNum, int warpType, const WtRange& range) {
    if (osc < 0 || osc >= WT_DISPLAY_OSCS) return;
    auto& b = gWtDispBox[osc];
    const uint64_t key = wtKey(tableNum, warpType, range);
    if (key == b.lastKey) return;              // nothing moved: the display is current
    // Only now touch the cache. On a miss the set is still building; leave lastKey
    // alone so the next block tries again and the display catches up when it lands.
    auto ws = getWavetableSetIfReady(tableNum, warpType, range);
    if (!ws || !ws->display) return;
    const unsigned w = b.w.load(std::memory_order_relaxed);
    const unsigned r = b.r.load(std::memory_order_acquire);
    if (w - r >= WtDispBox::CAP) return;       // full — drop, and retry next block
    b.slot[w % WtDispBox::CAP] = ws->display;  // empty slot: copy only, no destructor
    b.w.store(w + 1, std::memory_order_release);
    b.lastKey = key;
}

// Live morph position. Separate from the frame mailbox because it is a different kind
// of value: one float that changes every block rather than a block that changes rarely,
// so there is nothing to queue — the newest simply replaces the old.
//
// `seq` is what says "a voice is still publishing". A held note at a standstill writes
// the same pos forever, so the value alone cannot distinguish sounding from silent; the
// counter can. Release/acquire on it also publishes the pos store that preceded it.
namespace {
struct WtMorphBox {
    std::atomic<float>    pos{0.f};
    std::atomic<uint32_t> seq{0};
    uint32_t lastSeq{0};      // render thread only
    int      idle{0};         // render thread only
};
std::array<WtMorphBox, WT_DISPLAY_OSCS> gWtMorph{};
// ~8 frames of silence before the trace goes. Long enough that a block boundary or a
// dropped frame never blinks it, short enough that it leaves with the note.
constexpr int WT_MORPH_IDLE_FRAMES = 8;
}

void wtPublishMorph(int osc, float pos01) {
    if (osc < 0 || osc >= WT_DISPLAY_OSCS) return;
    auto& b = gWtMorph[osc];
    b.pos.store(pos01 < 0.f ? 0.f : (pos01 > 1.f ? 1.f : pos01), std::memory_order_relaxed);
    b.seq.fetch_add(1, std::memory_order_release);
}

bool wtTakeMorph(int osc, float& pos01) {
    if (osc < 0 || osc >= WT_DISPLAY_OSCS) return false;
    auto& b = gWtMorph[osc];
    const uint32_t s = b.seq.load(std::memory_order_acquire);
    if (s == b.lastSeq) { if (b.idle < WT_MORPH_IDLE_FRAMES) ++b.idle; }
    else { b.lastSeq = s; b.idle = 0; }
    if (b.idle >= WT_MORPH_IDLE_FRAMES) return false;
    pos01 = b.pos.load(std::memory_order_relaxed);
    return true;
}

bool wtApplyRuntimeWarp(int warpType, float warpA, float warpB,
                        const WtDisplayFrames& in, float* out) {
    if (!wtWarpRuntime(warpType) || in.n <= 0) return false;
    if (warpA <= 0.f && warpB <= 0.f) return false;
    const int F = in.n, n = WtDisplayFrames::POINTS;
    for (int f = 0; f < F; f++) {
        const float  t   = (F > 1) ? (float)f / (F - 1) : 0.f;
        const float  amt = warpA + t * (warpB - warpA);
        const float* s   = in.f + (size_t)f * n;
        float*       dst = out  + (size_t)f * n;
        if (amt <= 0.f) { for (int i = 0; i < n; i++) dst[i] = s[i]; continue; }
        MYFLOAT hold = 0; double dph = 0;
        for (int i = 0; i < n; i++)
            dst[i] = (float)runtimeWarpSample(warpType, amt, s[i], hold, dph);
    }
    return true;
}

std::shared_ptr<const WtDisplayFrames> wtTakeDisplay(int osc) {
    if (osc < 0 || osc >= WT_DISPLAY_OSCS) return nullptr;
    auto& b = gWtDispBox[osc];
    std::shared_ptr<const WtDisplayFrames> latest;
    unsigned r = b.r.load(std::memory_order_relaxed);
    const unsigned w = b.w.load(std::memory_order_acquire);
    for (; r != w; ++r) {                      // drain to the newest, drop the rest
        latest = std::move(b.slot[r % WtDispBox::CAP]);
        b.r.store(r + 1, std::memory_order_release);
    }
    return latest;                             // null when nothing new was published
}

// ── PADsynth ─────────────────────────────────────────────────────────────────
// See the long note in vco.h for what this is and why the phase rule matters.

// The curated table subset: EVERY wavetable that passes rules 1-4 below. Nine of the
// 27 are excluded and only for cause — the VCF already does it live and better (CLIMB,
// RESO); BANDWIDTH smears the structure away (COMB, PULSE, PLUCK are comb-shaped, and a
// band is ~n·bwRatio harmonics wide, so above the low harmonics the comb dissolves —
// the table would sound like a different table depending on an unrelated knob); the
// feature MOVES with morph faster than the baked frames can follow (SWEEP, DIGITAL —
// rule 4); or there is no harmonic recipe at all (NOISE, CZ RES are time-domain).
//
// The first thirteen keep the numbering they have always had, so an index in a saved
// patch still means what it meant. The last five have never been offered before.
static constexpr int PAD_TABLES[] = {
    0,   // BASIC   — 1/n anchor, sine→saw→square
    7,   // SOFT    — the only 1/n² family, the dark end
    24,  // BUZZ    — flat spectrum, the bright end
    2,   // ODD     — odd harmonics only, hollow
    4,   // VOWEL   — 3 formants ah→eh→ee→oh→oo, the choir
    3,   // FORMANT — a formant pair sweeping 2→28 over almost no body
    8,   // ORGAN   — sparse drawbars, each becomes its own band
    11,  // CHIME   — octave-spaced 1,2,4,8,16, exact on the grid
    9,   // FIFTHS  — saw + saw a twelfth up, interval content STRETCH can't make
    15,  // SHAPER  — even-harmonic emphasis, the octave-up warmth
    13,  // FM      — Bessel amplitudes, genuinely non-monotonic
    25,  // EPIANO  — fundamental + sideband cluster at 14×
    26,  // STACK   — root + octave + two-octave saws swelling in sequence
    // ---- never offered before this point ----
    5,   // METAL   — the only RISING envelope: h2..h5 above the fundamental
    17,  // HOLLOW  — every 3rd harmonic, climbing; deep nulls with an h4 peak
    21,  // FM2     — alternating notches. SEE THE RULE 4 NOTE: 0.865, the one entry
         //           here below the 0.885 floor. Present to be auditioned, not because
         //           it passed.
    10,  // GROWL   — FORMANT's pair, but over a full 1/n saw body
    18,  // AIR     — a wide high band, centre h15→h50, breathy
};

// A FIFTH RULE, AND IT IS ADVISORY, NOT A FILTER:
// SOME OF THESE TABLES ARE NEAR-DUPLICATES OF EACH OTHER, AND WHICH ONES IS MEASURED.
//
// The set is deliberately the FULL eligible list. It was cut to nine on 2026-08-16 and
// put back the same day, and the reason is worth keeping, because it is the reason this
// rule does not get to remove anything:
//
//   Ear-reported four times across one session: "no difference when I choose another
//   table in pad", "they all sound more or less the same". Three offline investigations
//   found nothing wrong — the selector, the cache, the build and the read were all
//   correct. The set was cut to nine on the strength of the numbers below. THE ACTUAL
//   CAUSE WAS OSC2 LEFT ON: VCO2GAIN's init is 0 dB, i.e. UNITY, so a saw was playing
//   over the pad and masking the table changes. The measurements were right; the thing
//   being listened to was not what anyone thought it was.
//
// So a table is removed for FAILING A RULE, never for scoring close to another one. The
// numbers below say which pairs are worth checking by ear, and nothing more.
//
// Distance is mean |dB| between two LOG spectra, each peak-normalised and floored 60 dB
// down, averaged over five matched morph positions (a table's sound is its whole sweep,
// not one frame). Calibrate any similarity metric on pairs whose answer you already
// know before trusting it: on this one, identical 0.00, saw vs square 14.91, sine vs
// saw 29.35.
//
// CLOSEST PAIRS IN THE FULL SET — check these first if two tables sound alike, but check
// what else is sounding first of all:
//
//     2.68  ORGAN / CHIME        4.27  CHIME / HOLLOW      5.02  SOFT / CHIME
//     3.55  ORGAN / FM           4.36  ODD / SOFT          5.16  HOLLOW / EPIANO
//     3.62  ORGAN / EPIANO       4.57  SOFT / HOLLOW       5.26  FM / HOLLOW
//     3.89  SOFT / ORGAN         4.65  CHIME / EPIANO      5.47  ODD / EPIANO
//     3.96  SOFT / EPIANO        4.84  HOLLOW / FM2        5.87  METAL / FM2
//     4.18  SOFT / FM            5.00  ORGAN / HOLLOW      5.98  ODD / HOLLOW
//
// ODD, ORGAN, HOLLOW, EPIANO, FM, SOFT and FIFTHS are one cluster of sparse or decaying
// spectra. If the set is ever cut for real, that is where to cut, and a greedy
// farthest-point subset has a cliff in it at nine:
//
//     size  8  9 10 11 12 13 ...
//     min  10.00  9.67  7.78  7.30  6.19  5.87   <- closest pair once that table is in
//
// THREE MEASUREMENT MISTAKES ARE BURIED HERE. Do not repeat any of them:
//   - Scoring the tables through the real oscillator at ONE pitch, harmonics 1..14.
//     That is the sound, which sounds like the right thing to measure, but it folds in
//     BANDWIDTH -- which is a KNOB, and worse, was itself miscalibrated for the very
//     tables being judged (see PAD_BW_CAP). Score the RECIPE; the knob is the user's.
//   - Cosine similarity of magnitude spectra, which scores SINE vs SAW at 0.974 because
//     the fundamental dominates the dot product. It called every table identical, and a
//     preset was changed on it and had to be reverted.
//   - A rectangular-window Goertzel over a ±0.45·f0 band, used to check whether the
//     BUILD was collapsing the tables. Its sidelobes fall at 6 dB/octave, so h1 leaked
//     into the h3 band at -10 dB and every table came back with its nulls filled in --
//     "the build destroys the difference", which was entirely the meter. Hann window and
//     ±0.2·f0 reads -60 in a true null. ALWAYS put a self-test in a spectral harness:
//     synthesise h1,2,4,8 only and confirm the gaps read -60 before believing anything.
//
// tools/pad-tables re-solves all of this from wtHarmonicsAt in one run -- the caps, the
// rule-4 fidelity, the distance matrix and the greedy curve. Everything below that is
// per-table MUST be re-solved with it, in the same order as PAD_TABLES. Getting that
// wrong is not cosmetic: METAL once shipped against SOFT's bandwidth cap, 8.6x the
// smear it can carry, i.e. a table with no tone left on most of its fader. The
// static_asserts under PAD_TABLE_TRIM catch a length mismatch; nothing can catch a
// stale VALUE, so re-solve, do not hand-edit.

// A FOURTH CURATION RULE, added after DIGITAL and SWEEP were measured unusable:
// THE FEATURE MUST NOT MOVE IN FREQUENCY WITH MORPH.
//
// padRead lerps MAGNITUDES between PAD_MORPH_LEVELS baked frames. That reproduces the
// true spectrum only for a table whose partials keep their position and merely change
// amplitude — the same assumption the bin-index+seed phase rule rests on. A table whose
// identity is a partial that MOVES cannot be interpolated that way at all: you get the
// old peak fading out while a new one at a different index fades in, so the pitch sags
// back before jumping to the next baked position.
//
// Measured as cosine similarity between the true spectrum and what padRead actually
// produces, worst case over the morph range, at C4:
//
//   SWEEP    0.039  — its peak walks h13→h41, i.e. ~9 harmonics between baked frames.
//                     Essentially orthogonal to what it should be, and the worst of all
//                     27 wavetables. Ear-reported as "sounds very high and folds back
//                     when morph moves". More frames barely help: 0.687 at 8 levels,
//                     0.928 at 12 — 50 MB/osc for one table.
//   DIGITAL  0.566  — WT_DIGITAL_DRAWS is 8 DISCRETE random draws, so every step is a
//                     dissolve between two uncorrelated spectra. More frames make it
//                     WORSE (0.838 at 6, 0.718 at 8, 0.648 at 12): you simply land on
//                     different draws. Unfixable by construction.
//   FM2      0.865  — alternating notches that shift index with the modulation ratio.
//                     Below BUZZ, so NOT ELIGIBLE. It was nonetheless added to the set
//                     on 2026-08-16 and removed the same day, because that pick was made
//                     on the spectra alone without re-running this test.
//   BUZZ     0.885  — a moving spectral edge, marginal. KEPT: it is the bright anchor,
//                     and it does improve with frames (0.953 at 8) if that ever changes.
//                     It is also the FLOOR — 0.885 is what "marginal but shipped" means,
//                     so a candidate below it is out.
//   the rest ≥0.94, most at 1.000 — amplitude-only, which is what this algorithm wants.
//
// A MOVING FEATURE IS NOT AUTOMATICALLY DISQUALIFIED, which is the non-obvious part:
// what decides it is the feature's WIDTH against how far it travels between two baked
// frames. AIR's band walks h15→h50, further than SWEEP's peak, and still measures 0.995
// — its σ is 8 harmonics, so consecutive frames overlap and the lerp lands inside the
// band. SWEEP is a single narrow partial and has nothing to overlap with. GROWL 0.999
// and FORMANT 0.969 pass for the same reason. So MEASURE the candidate; do not read the
// recipe and reject it for containing a `t` in a centre frequency.
//
// When auditing candidates, note that CZ RES and NOISE also score 1.000 and are NOT
// eligible — they are the two TIME-DOMAIN tables, so wtFrameHarmonicsT gives them no
// spectrum and the perfect score is an artifact.
static constexpr int PAD_NUM_TABLES = (int)(sizeof(PAD_TABLES) / sizeof(PAD_TABLES[0]));

int padTableCount() { return PAD_NUM_TABLES; }
int padTableToWt(int i) {
    const int wt = PAD_TABLES[(i < 0 || i >= PAD_NUM_TABLES) ? 0 : i];
    // buildPadRegion reads the spectrum through wtFrameHarmonicsT, which knows nothing
    // about the two TIME-DOMAIN tables (19 NOISE, 20 CZ RES) — the WT builder reaches
    // those via wtFrameCycleT's wtIsTimeDomain branch instead. Putting one in
    // PAD_TABLES would therefore not fail, it would silently produce a bare SINE, and
    // an audit that scores tables on their harmonics would rate that a perfect result.
    // Fall back to BASIC so the mistake is audible rather than invisible.
    return wtIsTimeDomain(wt) ? PAD_TABLES[0] : wt;
}

// Regions are HALF an octave, built at the TOP of the region so they are alias-free
// everywhere they play. Octave-wide regions were measured unusable: a region's
// harmonic count is fixed by its top note but used for the whole span below it, so
// the count HALVED across one semitone at every boundary — C7 had h10 (top at 20.9
// kHz), C7# had h5 (11.1 kHz). On most tables that is a brightness cliff, but on a
// table whose defining feature sits at a fixed harmonic index it removes the sound
// entirely: EPIAN's tine cluster is at h14, present at C6 and gone at C6#. Half
// octaves cut the worst-case error to sqrt(2), so a feature disappears within ~3
// semitones of where physics says it should rather than up to an octave early.
//
// Length is set by loop repetition, not by pitch: the table only has to outlast the
// beating inside a band, and bandwidth in Hz scales with the fundamental, so the
// beats get proportionally faster. ~8.4 beat periods per table at every region.
static const double PAD_REGION_F0[PAD_REGIONS] = {
      65.406,   92.499,  130.813,  184.997,  261.626,
     369.994,  523.251,  739.989, 1046.502, 1479.978,
    2093.005, 2959.955, 4186.009, 5919.911, 8372.018};
static const int PAD_REGION_LEN[PAD_REGIONS] = {
    1 << 18, 1 << 18, 1 << 17, 1 << 17, 1 << 16,
    1 << 16, 1 << 15, 1 << 15, 1 << 14, 1 << 14,
    1 << 14, 1 << 14, 1 << 14, 1 << 14, 1 << 14};

static constexpr double PAD_MORPH_LO   = 0.30;   // see the note in buildPadRegion
static constexpr double PAD_TARGET_RMS = 0.22;   // ~4.5σ peaks land just under 1.0
// Per-table loudness trim, as a linear factor on PAD_TARGET_RMS. Equal RMS is not
// equal loudness, so without this the tables span a few dB and switching table steps
// the level. These are MEASURED, not derived: each table is rendered offline through
// the real synthFunc and its loudness compared against the quietest (see the note above
// the trim in buildPadRegion for why the analytic version does not work). Re-measure if
// a table's recipe, its BANDWIDTH cap or the VCF default changes; order matches
// PAD_TABLES.
// Everything is trimmed DOWN to the quietest table rather than to the mean, so no entry
// is above 1. A boost would run into the peak guard at the bottom of buildPadRegion and
// be silently thrown away on exactly the tables that needed it. The whole oscillator is
// a dB or two quieter for it, which is what POSTGAIN is for.
static const double PAD_TABLE_TRIM[] = {
    0.9421,  // BASIC    -0.52 dB
    0.8480,  // SOFT     -1.43 dB
    0.9453,  // BUZZ     -0.49 dB
    0.7601,  // ODD      -2.38 dB
    0.8959,  // VOWEL    -0.96 dB
    0.7559,  // FORMANT  -2.43 dB
    0.8579,  // ORGAN    -1.33 dB
    0.7857,  // CHIME    -2.10 dB
    0.7327,  // FIFTHS   -2.70 dB
    0.8632,  // SHAPER   -1.28 dB
    1.0000,  // FM       +0.00 dB
    0.8010,  // EPIANO   -1.93 dB
    0.8874,  // STACK    -1.04 dB
    0.8486,  // METAL    -1.43 dB
    0.8191,  // HOLLOW   -1.73 dB
    0.9326,  // FM2      -0.61 dB
    0.8666,  // GROWL    -1.24 dB
    0.8294,  // AIR      -1.63 dB
};

// The three per-table columns and PAD_TABLES are one table split four ways, and they
// are indexed by the SAME pad index with no relation the compiler can see. Swapping a
// table and updating only some of them is exactly what happened on 2026-08-16, and the
// result was not a wrong number but a table with no tone. A length mismatch is the only
// half of that a machine can catch — so catch it, and treat it as a reminder that the
// VALUES have to be re-solved too (tools/pad-tables), which nothing here can check.
static_assert(sizeof(PAD_TABLE_TRIM) / sizeof(PAD_TABLE_TRIM[0])
              == sizeof(PAD_TABLES) / sizeof(PAD_TABLES[0]),
              "PAD_TABLE_TRIM must have one entry per PAD_TABLES entry, in the same "
              "order — re-measure it, do not pad it out");
// ALL EIGHTEEN RE-MEASURED 2026-08-16 at 44.1 kHz at the FIXED BANDW of 0.5 — the
// knob is gone (PAD_FIXED_BW in vco.h) and these are measured at that one value, so
// changing that constant means re-solving this column. Solved once before at the old
// 0.35 default, AFTER the BANDWIDTH fader became
// linear in the noise fraction — the default knob position now delivers roughly 40x
// the smear it used to, and a wider band is a LOUDER band here (ODD moved 2.4 dB), so
// the whole column had to be re-solved. Untrimmed spread 2.76 dB. Also re-solved once
// before that, AFTER PAD_BW_CAP was fixed —
// the cap sets how much of each table is smeared into a wash, so it moves the loudness,
// and trims measured against a stale cap are measuring the wrong sound. METAL is the
// worked example: 0.8064 against SOFT's cap, 0.8465 against its own.
// Averaged over C3 and C4 and three builds each, because PADsynth re-rolls its
// read offset per note. Rendered through the real synthFunc by linking libva.a, default
// patch with only the oscillator under test changed, INTEGRATED loudness over the
// sustain averaged across morph 0 / 0.5 / 1.0 and four notes at C3 and C4. Spread
// before trimming: 2.78 dB, CHIME quietest at -45.18 and FORMANT loudest at -42.39.
//
// Three things the harness has to get right, each of which silently produced a
// meaningless answer first time round:
//   - VCOxGAIN / NOISEGAIN are dB: 0.0 is UNITY, not off. Silencing the other sources
//     needs the parameter minimum, or the pad is measured under a saw and the noise.
//   - FILTCENTER is Log10-encoded Hz decoded by refreshSynthParams: 1.0 is a 1.1 Hz
//     cutoff. Use its initvalue for wide open.
//   - MAX-MOMENTARY IS THE WRONG STATISTIC HERE. padNoteOn re-rolls _padPhase per note,
//     so each note enters the random-phase table at a different offset and a 400 ms
//     window samples a lottery: same table, two runs, 1.25 dB apart. Integrated over
//     the sustain and averaged over several notes is repeatable to 0.05 dB.
// The figures average morph 0 / 0.5 / 1.0 because ONE trim has to serve the whole
// fader, not because either end is a bad place to measure. Measured spread per morph
// position: 2.62 dB at 0, 2.72 dB at 0.5, 1.86 dB at 1.0 — if anything the dark end
// separates the tables slightly MORE. (An earlier note here claimed morph 0 collapsed
// the spread into the noise; that was an artifact of the two harness bugs above, not
// of morph position, and it is wrong.)
static double padTableTrim(int i) {
    const int n = (int)(sizeof(PAD_TABLE_TRIM) / sizeof(PAD_TABLE_TRIM[0]));
    return PAD_TABLE_TRIM[(i < 0 || i >= n) ? 0 : i];
}

// BANDWIDTH knob (0..1) → the cents each table actually needs.
//
// The merge harmonic n ≈ 0.5/bwRat is the same for every table, but what merging
// DESTROYS is not: a table's identity dies when the smear reaches whichever harmonics
// carry its energy. A fixed cents knob is therefore unusable — on the old 5..150 ct
// one SWEEP (a single narrow partial at h27) was fully noise by 14% of the travel
// while SOFT and CHIME never got there at all.
//
// THE KNOB ENDS AT THE NOISE ONSET, and that point is MEASURED per table. Two earlier
// designs are buried here; both failed the same way, by anchoring on the wrong thing.
//
// WHAT DECIDES IT. The profile is an amplitude Gaussian exp(-x²), x = (f - n·f0)/bw,
// so the power spectrum around partial n is Gaussian with σ_f = bw/2, and its
// autocorrelation is exp(-2π²σ_f²τ²)·cos(2π·f_n·τ). At τ = 1/f0 with f_n = n·f0 the
// cosine is exactly 1, so the share of partial n's power still PERIODIC at the
// fundamental period is exp(-π²(bw_n/f0)²/2), with bw_n/f0 = bwRat·n^s. Everything
// else is heard as a wash, so
//
//     noise(bwRat) = Σ aₙ² (1 - exp(-π²(bwRat·n^s)²/2)) / Σ aₙ²
//
// The usable range lives around bwRat ~ 0.008, where 1-exp(-z) ≈ z, so this collapses
// to ONE number per table: noise ≈ (π²bwRat²/2)·⟨n^2s⟩. The knob's top is where the
// POWER-WEIGHTED RMS bandwidth, measured in partial spacings, reaches PAD_BW_K.
//
// That is why both earlier attempts were wrong: they anchored on where a table's
// IDENTITY lives, and what actually decides noise is where its ENERGY lives, weighted
// by n². For BASIC those differ by a factor of twenty — identity at h3, but rmsN 7.09
// against 165 available harmonics, and the cap 14.5 ct rather than 267.
//
//  - v1, a 13×5 matrix fitted to harmonic-to-inter-harmonic ratio. HNR reads falsely
//    tonal on sparse low-order spectra, so it walked SOFT/ODD/CHIME up to merge
//    harmonics of 1.15/1.17/1.56 — below the fundamental, i.e. rubble.
//  - v2, endpoints pinned at "clean" and "the fundamental itself merges", with the
//    identity harmonic at half travel. Ear-reported: "BASIC first 10% of slider
//    usable, rest noise." Correct — a full wash is not a destination anyone wants, so
//    spending 90% of the fader getting deeper into one wastes the control.
//  - v3, the cap above with a GEOMETRIC fader under it, cap/40 up to cap. The cap is
//    right — it is the one ear datum here — but the taper was wrong, and wrong in the
//    direction that hides the whole feature. Ear-reported 2026-08-16 as PADsynth
//    simply "not sounding too convincing".
//
//    The reasoning for geometric was "bandwidth is heard as a ratio, a linear sweep
//    would put every audible step in the last few percent". Half right: a fader linear
//    in bwRat is indeed bad. But the quantity actually heard is the NOISE FRACTION,
//    and that goes as bwRat SQUARED, so a 40x geometric sweep overcorrects hard —
//    noise(k)/noise(1) = 40^(2(k-1)), which is 48% at knob 0.9, 2.5% at 0.5 and 0.8%
//    at the 0.35 default. Half the effect lived in the top TENTH of the fader and the
//    default delivered a hundredth of it: bands narrower than half an FFT bin, which
//    buildPadRegion then clamps to exactly one bin, i.e. a pure sine per partial. A
//    PADsynth oscillator with no smear is just an additive one with random phases,
//    which is precisely what it sounded like.
//
// SO THE FADER IS LINEAR IN THE NOISE FRACTION, the quantity the whole calibration is
// built on: bwRat = cap·sqrt(knob) gives noise(k) = noise_max·k exactly, since
// noise ≈ (π²/2)(bwRat·rmsN)² and cap·rmsN is the constant K. Knob 0.5 is now half the
// available wash rather than a fortieth of it, and the top is untouched at the
// ear-set cap. Do not "restore" a geometric taper here without re-reading the two
// paragraphs above.
static constexpr float PAD_BW_K    = 0.05977f;  // RMS bandwidth, in partial spacings, at the cap

// bwRat at knob 1: the measured noise onset, at bwScale 1, solved exactly (geometric
// bisection on the expression above, not the linearised form — that overestimates on
// spectra with a few very high partials, where 1-exp() has saturated rather than
// staying linear). Every entry sits at the SAME 1.29% noise fraction, and 1.29% is
// itself the one ear datum in the whole table: it is where BASIC was reported to stop
// being usable. Taken at the brightest morph position each recipe reaches, since one
// cap has to hold across the whole fader.
//
// TO RETUNE BY EAR: these scale together. If everything crosses over too early, the
// calibration point moved — scale the whole column. If ONE table is wrong, that
// table's recipe is brighter or darker than its neighbours think, and only its entry
// moves. Reference region is C4 (nHarm 82); see the pitch note below.
//
// ORDER MATCHES PAD_TABLES, AND A STALE ENTRY IS NOT A COSMETIC BUG. Three tables were
// swapped into PAD_TABLES on 2026-08-16 without re-solving this column, so each ran on
// its predecessor's cap: METAL inherited SOFT's 0.04294 against its own 0.00502, i.e.
// 8.6x the smear it can carry. SOFT is 1/n² with nothing above h2 to blur and METAL is
// the opposite, a RISING series -- so METAL had no tone left above about a third of the
// BANDWIDTH fader, and was ear-reported the same day as "no sound at all when choosing
// pad". Re-solve with tools/pad-tables whenever PAD_TABLES changes.
static const float PAD_BW_CAP[18] = {
    0.00849f,  // BASIC    14.6 ct   rmsN  7.05
    0.04294f,  // SOFT     72.8 ct   rmsN  1.23  — 1/n², almost nothing above h2 to smear
    0.00191f,  // BUZZ      3.3 ct   rmsN 26.99  — flat series, every harmonic counts
    0.01114f,  // ODD      19.2 ct   rmsN  5.78
    0.00753f,  // VOWEL    13.0 ct   rmsN  6.87
    0.00157f,  // FORMANT   2.7 ct   rmsN 32.80  — formant pair sweeps to h28
    0.01688f,  // ORGAN    29.0 ct   rmsN  3.06  — sparse low drawbars
    0.01582f,  // CHIME    27.2 ct   rmsN  4.15
    0.00526f,  // FIFTHS    9.1 ct   rmsN 10.43
    0.01018f,  // SHAPER   17.5 ct   rmsN  6.17
    0.00914f,  // FM       15.8 ct   rmsN  5.62
    0.02069f,  // EPIANO   35.5 ct   rmsN  2.85  — re-solved when the recipe gained a body
    0.00457f,  // STACK     7.9 ct   rmsN 11.81
    0.00502f,  // METAL     8.7 ct   rmsN 10.26  — NOT SOFT's 0.04294; see above
    0.01829f,  // HOLLOW   31.4 ct   rmsN  3.15
    0.00592f,  // FM2      10.2 ct   rmsN  8.72
    0.00681f,  // GROWL    11.7 ct   rmsN  7.71  — FORMANT's pair over a 1/n body
    0.00104f,  // AIR       1.8 ct   rmsN 49.44  — a band at h15..h50 and nothing below
};
static_assert(sizeof(PAD_BW_CAP) / sizeof(PAD_BW_CAP[0])
              == sizeof(PAD_TABLES) / sizeof(PAD_TABLES[0]),
              "PAD_BW_CAP must have one entry per PAD_TABLES entry, in the same order — "
              "re-solve it with tools/pad-tables, do not pad it out");

// KNOWN, NOT FIXED: the cap depends on PITCH, because rmsN counts the harmonics that
// fit under Nyquist. BASIC measures 10 ct at C3 and 40 ct at C7 — a 4× drift across
// the keyboard, and these constants are the C4 value. Closing it means resolving the
// bandwidth per region inside buildPadRegion instead of once in padParamsFromSnapshot,
// which moves the cache key off cents. Tables whose energy is at fixed low harmonics
// (SOFT, ORGAN, FM) barely drift at all; the bright ones drift most.
float padBandwidthCents(int table, float knob, float bwScale) {
    const int n = (int)(sizeof(PAD_BW_CAP) / sizeof(PAD_BW_CAP[0]));
    const double cap1 = PAD_BW_CAP[(table < 0 || table >= n) ? 0 : table];
    const float  k    = knob < 0.f ? 0.f : (knob > 1.f ? 1.f : knob);
    const double s    = bwScale < 0.1f ? 0.1 : (double)bwScale;
    // BW SCL moves the cap, it does not move the knob under it. From the linearised
    // criterion, cap(s) = K/rmsN^s, and rmsN = K/cap(1) — so cap(s) = K^(1-s)·cap(1)^s
    // with no need to carry rmsN separately. At s = 1 this is exactly the measured
    // constant; above it the smear grows faster up the series so less is affordable,
    // below it more. That keeps BANDWIDTH and BW SCL independent instead of trading
    // against each other.
    //
    // RESIDUAL, measured: this step assumes ⟨n^2s⟩ ≈ ⟨n²⟩^s, exact only for a
    // concentrated spectrum, so the tables drift apart again away from s = 1. At the
    // knob's top the noise fraction spans 1.29–1.30% at s = 1 (i.e. exact), 0.74–1.50%
    // at s = 0.5 and 0.95–3.93% at s = 2. Closing that needs a second constant per
    // table, or the spectrum at runtime; it is bounded and away from the default.
    const double cap = std::pow((double)PAD_BW_K, 1.0 - s) * std::pow(cap1, s);
    // LINEAR IN THE NOISE FRACTION — see the v3 note above for the taper this
    // replaced and why. noise ≈ (π²/2)(bwRat·rmsN)² and cap·rmsN = PAD_BW_K, so
    // bwRat = cap·sqrt(k) makes noise(k) = noise_max·k exactly: knob 0.5 is half the
    // available wash, and knob 0 is genuinely clean rather than cap/40.
    const double r = cap * std::sqrt((double)k);
    return (float)(1200.0 * std::log2(1.0 + r));
}

// Coarser than WT's 64: a pad set is 16.75 MB and ~150 ms to build against a
// wavetable set's far cheaper rebuild, so the grid is sized to keep a fader drag to a
// handful of builds rather than a smooth one. 16 steps is ~6.7% of the morph span,
// which is well inside one baked level's width and therefore inaudible as stepping.
static constexpr int PAD_RANGE_STEPS = 16;
float padQuantMorph(float v) {
    int q = (int)std::lround(v * (PAD_RANGE_STEPS - 1));
    q = q < 0 ? 0 : (q > PAD_RANGE_STEPS - 1 ? PAD_RANGE_STEPS - 1 : q);
    return (float)q / (float)(PAD_RANGE_STEPS - 1);
}

int padRegionFor(double freq) {
    // The tolerance is not cosmetic. PAD_REGION_F0 holds equal-tempered pitches
    // truncated to three decimals, so a note sitting exactly ON a region top --
    // every C and F# -- computes a hair ABOVE its own literal and falls into the
    // next region up, which is built as much as a half octave higher and therefore
    // culls harmonics it did not need to. Measured on EPIANO: MIDI 90, 96 and 102
    // each lost their tine cluster a semitone before the note above them did.
    // 1.0002 is ~0.35 cents: three orders of magnitude above the truncation error,
    // three orders below a semitone, so it can never capture the next note.
    for (int r = 0; r < PAD_REGIONS; r++) if (freq <= PAD_REGION_F0[r] * 1.0002) return r;
    return PAD_REGIONS - 1;
}

// Local FFT plans. CHECKFFT indexes ffts[(int)log2(M)] into an array sized
// NUMFFTS+1 == 16, so asking it for 2^18 is an out-of-bounds write — and NUMFFTS
// lives in a header shared with grainstorm. Keyed by actual size instead.
static FFT* padFFT(int n) {
    static thread_local std::map<int, std::unique_ptr<FFT>> plans;
    auto it = plans.find(n);
    if (it != plans.end()) return it->second.get();
    return plans.emplace(n, std::make_unique<FFT>(n)).first->second.get();
}

// BIN INDEX AND SEED ONLY. Adding morph, bandwidth or table here would silently
// turn every crossfade in the read path into a dissolve — see vco.h.
static inline double padBinPhase(uint32_t bin, uint32_t seed) {
    return 2.0 * PI_F_P * wtHash01(bin * 2654435761u + seed * 0x9E3779B9u);
}

static void buildPadRegion(PadRegion& reg, const PadParams& p, int r, double sr) {
    const int N = PAD_REGION_LEN[r], half = N / 2;
    reg.len   = N;
    reg.f0    = PAD_REGION_F0[r];
    reg.shift = 64 - (int)std::lround(std::log2((double)N));

    const double binHz  = sr / (double)N;
    const double nyq    = sr * 0.45;      // headroom so a bend up doesn't fold
    const double bwRat  = std::pow(2.0, (double)p.bandwidth / 1200.0) - 1.0;
    const double strB   = (double)p.stretch * 0.002;   // piano-ish inharmonicity
    int nHarm = (int)(nyq / reg.f0);
    if (nHarm > WT_HARMONIC) nHarm = WT_HARMONIC;
    if (nHarm < 1) nHarm = 1;

    const int wt = padTableToWt(p.table);
    std::vector<float>  mags(WT_HARMONIC + 1, 0.f);
    std::vector<double> amp(half + 1, 0.0);
    std::vector<double> buf(N + 2, 0.0);
    FFT* fft = padFFT(N);

    for (int m = 0; m < PAD_MORPH_LEVELS; m++) {
        // MORPH covers recipe t in [PAD_MORPH_LO, 1], not [0, 1]. Every recipe starts
        // from a sine at t=0 because WT sweeps UP from there, so the bottom third of
        // the range is degenerate here: measured, BASIC / SOFT / BUZZ / ODD / FM /
        // EPIAN / CHIME are all within 0.999 similarity of each other at t=0 -- seven
        // of thirteen tables indistinguishable. A near-sine is not what a PADsynth
        // oscillator is for, so the fader is mapped onto the part of each recipe that
        // actually differs. Set PAD_MORPH_LO to 0 for the literal WT morph range.
        // The levels span the oscillator's own MORPH A→B, not a fixed range. PAD_MORPH_LO
        // stays as the KNOB→recipe mapping (so A=0 is still the useful end of the recipe
        // rather than a sine), but the four bakes are now spread across A→B, which is what
        // gives a narrow morph its resolution instead of parking it between two frames.
        const double u  = (PAD_MORPH_LEVELS > 1) ? (double)m / (PAD_MORPH_LEVELS - 1) : 0.0;
        const double kn = (double)p.mA + ((double)p.mB - (double)p.mA) * u;
        const double t  = PAD_MORPH_LO + (1.0 - PAD_MORPH_LO) * kn;
        // Ask for one octave MORE than the region can hold, so the fold below can see
        // whatever is about to be cut off.
        const int fullHarm = std::min(nHarm * 2, WT_HARMONIC);
        wtFrameHarmonicsT(wt, t, mags.data(), fullHarm);

        // Octave-fold a FEATURE that sits just above the region's harmonic limit
        // rather than dropping it. Without this, a table whose identity lives at a
        // fixed harmonic index loses that identity outright at a region edge: EPIANO
        // is `1 +- k*14`, so in the 2093 Hz region (nHarm 10) every sideband is culled
        // and all that is left is the k=0 body -- measured, it became a literal sine
        // from MIDI 91 up (centroid/f0 1.03, against 7.8 a semitone below).
        //
        // Only spectra that RISE into the cut are folded. A decaying series (BASIC
        // 1/n, SOFT 1/n^2) has nothing but tail up there, and a flat one (BUZZ) is
        // equal to its own edge, so neither folds and both keep the alias-free
        // brightness rolloff they should have. Energies add because these bins are
        // given random phases anyway.
        // CLAMP to the top representable partial, do NOT fold down by octaves.
        //
        // This used to do `int d = n; while (d > nHarm) d >>= 1;`. Two things were
        // wrong with that, both measured:
        //
        //  - Halving a harmonic INDEX is an octave only when the index is even. h27
        //    folds to h13, but an octave below h27 is h13.5 — so the partial lands 65
        //    cents flat, by a different amount for every n (h25 +71, h29 +61, h31 +57).
        //    A cluster folded that way is detuned into inharmonicity.
        //  - Worse for any feature that MOVES with morph. As the feature crosses the
        //    region limit its fold depth increments and the pitch drops an octave, so
        //    the trajectory is a sawtooth, not a climb. Traced on SWEEP in the 1046 Hz
        //    region, dominant partial against morph: 13 16 19 **10** 12 13 15 16 17 19
        //    20 — it climbs, drops an octave, and climbs again. Ear-reported as "sweep
        //    and epiano sound very high and they seem to fold back when morph moves".
        //    Higher up it stops sweeping altogether (2093 Hz: 6 8 9 10 1 1 1 1 1 1 1).
        //
        // Octave folding on an integer harmonic grid cannot avoid either problem — an
        // octave below an odd harmonic is not on the grid, and the fold depth has to
        // step somewhere. Clamping has neither: the energy stays (which is the whole
        // point — culling is what turned EPIANO into a literal sine above MIDI 91), it
        // stays in tune because nHarm is a real harmonic, and a rising feature tops out
        // instead of dropping. A sweep that runs out of room plateaus, which is what
        // the physical limit actually sounds like.
        {
            double edgeRef = 0.0;
            for (int n = std::max(1, nHarm - 3); n <= nHarm; n++)
                edgeRef = std::max(edgeRef, (double)mags[n]);
            for (int n = nHarm + 1; n <= fullHarm; n++) {
                const double a = mags[n];
                mags[n] = 0.f;
                if (a <= 1e-7 || a <= edgeRef * 1.2) continue;   // tail, not a feature
                mags[nHarm] = (float)std::sqrt((double)mags[nHarm] * mags[nHarm] + a * a);
            }
        }

        std::fill(amp.begin(), amp.end(), 0.0);
        for (int n = 1; n <= nHarm; n++) {
            const double a = mags[n];
            if (a <= 1e-7) continue;
            const double fn = reg.f0 * n * std::sqrt(1.0 + strB * (double)n * (double)n);
            if (fn >= nyq) break;
            // Nasca's law: bandwidth is proportional to the FUNDAMENTAL times
            // n^bwScale, so bwScale == 1 keeps the smear constant in cents across
            // the whole series (and therefore under transposition).
            double bw = bwRat * reg.f0 * std::pow((double)n, (double)p.bwScale);
            if (bw < binHz * 0.5) bw = binHz * 0.5;   // never narrower than one bin
            int i0 = (int)std::floor((fn - 3.0 * bw) / binHz);
            int i1 = (int)std::ceil ((fn + 3.0 * bw) / binHz);
            if (i0 < 1) i0 = 1;
            if (i1 > half - 1) i1 = half - 1;
            const double inv = 1.0 / bw;
            // Profile normalisation is 1/SQRT(bw), not Nasca's 1/bw. His preserves
            // each harmonic's amplitude SUM, but these bins have random phases, so
            // what survives into the waveform is the L2 norm: sum of squares over a
            // band goes as 1/bw, making a harmonic's actual RMS a[n]/sqrt(bw), i.e.
            // a[n]/sqrt(n) at bwScale 1. That is a hidden -3 dB/octave tilt on top of
            // every source spectrum -- measured, it put 95% of a SQUARE's energy in
            // the fundamental (a real square is 84%) and collapsed all 13 tables
            // toward "sine plus a trace". 1/sqrt(bw) makes the band's energy
            // independent of its width, so the table's spectrum is reproduced as
            // written and BANDWIDTH/BW SCL control smear only, never tilt.
            const double nrm = 1.0 / std::sqrt(bw);
            for (int i = i0; i <= i1; i++) {
                const double x = ((double)i * binHz - fn) * inv;
                amp[i] += a * std::exp(-x * x) * nrm;
            }
        }

        std::fill(buf.begin(), buf.end(), 0.0);
        for (int i = 1; i < half; i++) {
            if (amp[i] == 0.0) continue;
            const double ph = padBinPhase((uint32_t)i, (uint32_t)p.seed);
            buf[i << 1]       = amp[i] * std::cos(ph);
            buf[(i << 1) + 1] = amp[i] * std::sin(ph);
        }
        buf[0] = 0.0; buf[1] = 0.0;   // DC and Nyquist (packed layout, as elsewhere)
        fft->backward(buf.data());

        // RMS, not peak: a random-phase signal's peak is a lucky draw, so peak
        // normalisation would jump the level between regions and morph levels.
        double sum = 0.0;
        for (int i = 0; i < N; i++) sum += buf[i] * buf[i];
        const double rms = std::sqrt(sum / (double)N);

        // ...but equal RMS is not equal loudness: measured across the 13 tables at
        // identical settings the spread was 3.3 dB, which is a step you hear when
        // switching table. PAD_TABLE_TRIM corrects it.
        //
        // An analytic correction was tried first and REJECTED by measurement: weight
        // each table's own spectrum by the BS.1770 curve and divide it out. It made
        // the spread WORSE, 3.3 dB -> 6.4 dB, because it models the loudness of the
        // oscillator while what reaches the ear has been through the VCF and the
        // crest-factor peak guard below -- the bright tables it attenuated were
        // already the quiet ones. Do not re-derive it from the spectrum; the number
        // that matters is only observable at the output.
        double g = (rms > 1e-12) ? (PAD_TARGET_RMS * padTableTrim(p.table) / rms) : 0.0;
        double pk = 0.0;
        for (int i = 0; i < N; i++) { const double v = std::fabs(buf[i]) * g; if (v > pk) pk = v; }
        if (pk > 0.99) g *= 0.99 / pk;

        reg.lvl[m].resize(N + 1, 0.f);
        for (int i = 0; i < N; i++) reg.lvl[m].data()[i] = (float)(buf[i] * g);
        reg.lvl[m].data()[N] = reg.lvl[m].data()[0];   // guard: the read never wraps
    }
}

// ── pad set cache ────────────────────────────────────────────────────────────
// Unlike wtKey's bounded 27×16 space, the pad key has three continuous axes, so this
// never gets a hit on a setting you have been to before — it holds a freshly built set
// alive between the worker finishing and the audio thread picking it up, and then
// holds the CURRENT set for each oscillator against the per-block warm loop.
//
// That second job is why order matters: with one slot per VCO there is no slack, so
// eviction must drop the set nobody is asking for. See padWarmFind.
static std::mutex gPadMtx;
static std::deque<std::shared_ptr<PadSet>> gPadWarm;
static double gPadSr = 0.0;
static constexpr int PAD_WARM_MAX = 3;   // one per VCO

static std::array<std::atomic<uint32_t>, 8> gPadInFlight{};   // 0 = empty

// The grid padKey hashes the continuous fields on. padSnapParams puts the fields
// THEMSELVES on it, so the two ways of asking "same set?" cannot disagree.
static constexpr float PAD_Q_BW = 100.f, PAD_Q_BWS = 1000.f, PAD_Q_STR = 10000.f;

// Snap the continuous fields onto the key's own grid. This is not cosmetic: padKey
// quantises, operator== does not, so without it two params can hash IDENTICALLY while
// comparing UNEQUAL — and the two are used for different questions. A knob that drifts
// by less than one quantum after a build (smoothing, a mod source, any float jitter)
// then leaves getPadSetIfReady unable to see the set that was just built for it, while
// the in-flight claim still dedups on the matching hash. The block loop asks again,
// the worker builds a near-identical 16.75 MB duplicate, and that is a second ~150 ms
// build — and a second panel over the keyboard — from a setting the user did not touch.
//
// One spurious build, then it settles, because the duplicate does match afterwards.
// Snapping at the single construction site is what makes the two agree by
// construction; both sides derive from the snapped value, so even if a round-trip
// landed a quantum out they would land there together.
void padSnapParams(PadParams& p) {
    p.bandwidth = (float)std::lround(p.bandwidth * PAD_Q_BW)  / PAD_Q_BW;
    p.bwScale   = (float)std::lround(p.bwScale   * PAD_Q_BWS) / PAD_Q_BWS;
    p.stretch   = (float)std::lround(p.stretch   * PAD_Q_STR) / PAD_Q_STR;
    // mA/mB are already exact: padQuantMorph puts them on a 16-step grid before they
    // ever get here, and that grid is far coarser than the 1e4 the key hashes them on.
}

static uint32_t padKey(const PadParams& p) {
    uint32_t h = 2166136261u;
    auto mix = [&h](uint32_t v) { h ^= v; h *= 16777619u; };
    mix((uint32_t)p.table);
    mix((uint32_t)std::lround(p.bandwidth * PAD_Q_BW));
    mix((uint32_t)std::lround(p.bwScale * PAD_Q_BWS));
    mix((uint32_t)std::lround(p.stretch * PAD_Q_STR));
    mix((uint32_t)p.seed);
    mix((uint32_t)std::lround(p.mA * 10000.f));
    mix((uint32_t)std::lround(p.mB * 10000.f));
    return h ? h : 1u;
}
int padBuildsInFlight() {
    int n = 0;
    for (auto& s : gPadInFlight)
        if (s.load(std::memory_order_relaxed) != 0) ++n;
    return n;
}

static bool padInFlightClaim(uint32_t key) {
    for (auto& s : gPadInFlight) if (s.load(std::memory_order_acquire) == key) return false;
    for (auto& s : gPadInFlight) { uint32_t e = 0; if (s.compare_exchange_strong(e, key, std::memory_order_acq_rel)) return true; }
    return false;   // saturated — a later block retries
}
static void padInFlightRelease(uint32_t key) {
    for (auto& s : gPadInFlight) { uint32_t k = key; if (s.compare_exchange_strong(k, 0u, std::memory_order_acq_rel)) return; }
}

// Find a resident set AND mark it as used. The touch is the whole point: the list is
// exactly as deep as there are oscillators, so eviction has no slack, and it used to
// be ordered by BUILD time (push_front on build, and nothing ever re-ordered it). A
// fresh build therefore dropped whichever set had been built longest ago — which is
// very often one an oscillator is still asking for every block. warmPadSets found it
// missing on the next block and queued a rebuild of a set that had just been thrown
// away: another ~150 ms and 16.75 MB, and another panel over the keyboard, with no
// user input at all.
//
// Reported as "I change some setting, it renders and applies, and afterwards there is
// sometimes a short second render without hitting anything". The "sometimes" is which
// oscillator you touched: modelled against the block loop with all three on PAD,
// moving OSC1's knob costs 0 spurious rebuilds, OSC2's costs 1 and OSC3's costs 2,
// because OSC1's old set is already the oldest and is the right thing to evict.
//
// Touch-on-hit makes the victim the least recently WANTED set, which is the stale key
// nobody asked for — 0 spurious rebuilds from any oscillator, and no extra memory.
// It is what wtWarmPut has always done for the wavetable list.
//
// Caller holds gPadMtx.
static std::shared_ptr<PadSet> padWarmFind(const PadParams& p) {
    for (auto it = gPadWarm.begin(); it != gPadWarm.end(); ++it) {
        if (!((*it)->params == p)) continue;
        auto sp = *it;
        if (it != gPadWarm.begin()) { gPadWarm.erase(it); gPadWarm.push_front(sp); }
        return sp;
    }
    return nullptr;
}

std::shared_ptr<PadSet> getPadSet(const PadParams& p, double sampleRate) {
    {
        std::lock_guard<std::mutex> lk(gPadMtx);
        if (sampleRate != gPadSr) { gPadWarm.clear(); gPadSr = sampleRate; }
        if (auto sp = padWarmFind(p)) return sp;
    }
    auto sp = std::make_shared<PadSet>();
    sp->params = p;
    for (int r = 0; r < PAD_REGIONS; r++) buildPadRegion(sp->region[r], p, r, sampleRate);  // heavy, outside the lock
    std::lock_guard<std::mutex> lk(gPadMtx);
    if (auto e = padWarmFind(p)) return e;   // lost a build race
    gPadWarm.push_front(sp);
    while ((int)gPadWarm.size() > PAD_WARM_MAX) gPadWarm.pop_back();
    return sp;
}

static std::shared_ptr<PadSet> getPadSetIfReady(const PadParams& p, double sr) {
    std::lock_guard<std::mutex> lk(gPadMtx);
    if (sr != gPadSr) return nullptr;
    return padWarmFind(p);
}

// Any resident set built from the SAME TABLE, newest first. Only used to stop a fresh
// voice being silent: a voice returned to the pool has its _pad released (a set is
// 16.75 MB, so a finished voice must not pin one), which means the "keep the previous
// set playing" fallback in refreshPad protects a voice that is ALREADY sounding but
// can do nothing for the next note-on. Until the wanted build lands, that note would
// read from nullptr and be silent -- for every note, not just one.
//
// Same table only. Falling back to whatever happens to be resident would play a
// different waveform entirely, which is worse than the wrong smear.
//
// Deliberately does NOT go through padWarmFind: this is a stopgap, not a want. The set
// it returns is by definition NOT the one that was asked for, and marking it as used
// would let a set nobody actually wants outlive one that is being requested every
// block — the same eviction mistake padWarmFind exists to fix, in reverse.
static std::shared_ptr<PadSet> getPadSetSameTable(int table, double sr) {
    std::lock_guard<std::mutex> lk(gPadMtx);
    if (sr != gPadSr) return nullptr;
    for (auto& sp : gPadWarm) if (sp->params.table == table) return sp;
    return nullptr;
}

// Coalescing is implicit: a knob drag changes the params every block, but only one
// build per distinct key is ever in flight, and intermediate keys the worker never
// reached are simply never requested again once the knob moves on.
static void requestPadSet(tsl::AppState* app, const PadParams& p, double sr);

// Warm-up entry point for the per-block path: PADsynth builds take ~150 ms, so if
// the first request only went out at note-on the first note would be silent. The
// block loop calls this whether or not any voice is running.
void padWarmRequest(tsl::AppState* app, const PadParams& p, double sr) { requestPadSet(app, p, sr); }

// Settle time before a pad build is allowed to start. Dragging a fader changes the
// params every block, and the OLD claim: "coalescing is implicit, intermediate keys
// are simply never requested again once the knob moves on" -- was wrong. Nothing
// cancels a task once it is on the queue, so every value the fader passed through
// still got built: up to 8 full 16.75 MB builds (gPadInFlight has 8 slots) queued
// ahead of the one actually wanted, with a 16.75 MB free interleaved between each as
// the 3-deep warm list evicts. Measured in the app as up to ten seconds of silence.
//
// Waiting for the key to hold still is what makes coalescing real: a value the fader
// swept past never reaches the queue at all.
static constexpr int64_t PAD_SETTLE_NS = 180'000'000;   // 180 ms
struct PadPend { std::atomic<uint32_t> key{0}; std::atomic<int64_t> since{0}; };
static std::array<PadPend, 8> gPadPend{};

static bool padSettled(uint32_t key, int64_t now) {
    for (auto& s : gPadPend)
        if (s.key.load(std::memory_order_acquire) == key)
            return (now - s.since.load(std::memory_order_relaxed)) >= PAD_SETTLE_NS;
    for (auto& s : gPadPend) {                       // first sighting: start its clock
        uint32_t e = 0;
        if (s.key.compare_exchange_strong(e, key, std::memory_order_acq_rel)) {
            s.since.store(now, std::memory_order_relaxed);
            return false;
        }
    }
    PadPend* oldest = &gPadPend[0];                  // full: recycle the stalest slot
    for (auto& s : gPadPend)
        if (s.since.load(std::memory_order_relaxed) < oldest->since.load(std::memory_order_relaxed))
            oldest = &s;
    oldest->key.store(key, std::memory_order_release);
    oldest->since.store(now, std::memory_order_relaxed);
    return false;
}

// NO `urgent` BYPASS HERE, AND THAT IS DELIBERATE — do not add one for symmetry with
// requestWavetableSet, where note-on skips the settle. Three reasons it does not carry
// over:
//
//   - It would not rescue the note. A pad build is ~150 ms, so the first note sounds
//     on the wrong set with or without the bypass; skipping the wait only makes the
//     right set arrive earlier, still long after the attack. What actually covers that
//     note is the getPadSetSameTable fallback in refreshPad, which needs no bypass.
//   - It costs far more here. Each build is ~16.75 MB. Playing while turning a knob
//     would queue one per transient value passed through — the exact failure the
//     settle exists to prevent, in its expensive form.
//   - The case barely arises. warmPadSets runs every block from the moment a PAD
//     oscillator is selected, so the set is normally built long before anything is
//     played. The only window where a bypass would matter is the couple of hundred ms
//     after a knob change, which is precisely when a speculative build is least wanted.
//
// (Historical note, so the asymmetry does not read as an oversight to undo: it WAS
// one. The wavetable settle was modelled on this function and gained the bypass to
// fix its own note-on case; the reasoning above is why it was left unmatched.)
static void requestPadSet(tsl::AppState* app, const PadParams& p, double sr) {
    if (!app) return;
    const uint32_t key = padKey(p);
    if (!padSettled(key, tsl::time::nanosecondsSinceEpoch())) return;
    if (getPadSetIfReady(p, sr)) return;   // already built — see requestWavetableSet
    if (!padInFlightClaim(key)) return;
    if (!app->UiTasksQueue.add_task([p, sr, key, app]() {
            tsl::app::BuildOverlayScope panel(app);   // see requestWavetableSet
            getPadSet(p, sr);
            padInFlightRelease(key);
        }))
        padInFlightRelease(key);
}

// ── Vco ──────────────────────────────────────────────────────────────────────

void Vco::check(int mode) {
    if (_oldmode == mode) return;
    _oldmode = mode;

    if (mode == 99) {   // wavetable
        _mode = 3;
        ensureWt(/*immediate=*/true);   // default set until selectWavetable() picks one
        return;
    }

    if (mode == 98) {   // PADsynth
        _mode = 4;
        if (!_pad) refreshPad(/*immediate=*/true);   // default until setPad()
        return;
    }

    if (mode == 97) {   // modal resonator bank
        _mode = 5;
        _modal.init((double)_appState->sr);
        _modalCounter = 0;
        return;
    }

    if (mode == -1) {
        _mode  = -1;
        _table = _appState->data->sinewave;
        uint32_t mask, lobits;
        MYFLOAT  pfrac;
        oscbnk_flen_setup(WINDOW_SIZE, &mask, &lobits, &pfrac);
        _mask   = mask;
        _lobits = lobits;
        _pfrac  = pfrac;
        return;
    }

    // Map PA mode int → tnum + vco_mode
    // mode: 0→saw/simple, 2→saw/PWM, 4→ramp, 6→pulse, 8→4x(1-x), 10→square, 12→triangle
    static const int tnums[8] = {0, 0, 1, 2, 1, 3, 4, 5};
    static const int vmodes[8] = {0, 1, 2, 0, 0, 0, 0, 0};

    int32_t mt = (int32_t)mode & 0x1F;
    if (mt & 1u) return;
    int idx  = (mt & 14u) >> 1;
    int tnum = tnums[idx];
    _mode    = vmodes[idx];
    _tables  = getSharedTables(tnum);
}

MYFLOAT Vco::tick(const MYFLOAT cps, const MYFLOAT gain, const MYFLOAT pw, const uint32_t pmPhase) {
    _frq = OSCBNK_PHS2INT(cps);
    if (_mode == 5) {   // modal: no phase accumulator at all, the bank IS the state
        // cps is NORMALISED (cycles per sample) — the bank is keyed in real Hz, so
        // this conversion is mandatory. Getting it wrong is the PADsynth trap:
        // the result is near-DC, which reads as "no sound" rather than as a pitch bug.
        if (--_modalCounter <= 0) {
            _modal.retune(std::fabs(cps) * (double)_appState->sr);
            _modalCounter = MODAL_RETUNE_INTERVAL;
        }
        return _modal.tick() * gain;
    }
    if (_mode == -1) {
        // read at phs+pmPhase, but advance the accumulator by _frq only
        int32_t n = (int32_t)((phs + pmPhase) & OSCBNK_PHSMSK) >> (int32_t)_lobits;
        phs = (phs + _frq) & OSCBNK_PHSMSK;
        return _table[n] * gain;
    }
    if (_mode == 3) {   // wavetable: pw carries the morph position (0..1)
        const WavetableSet* ws = _wt.get();
        const int nm = ws ? ws->nMorph : 0;
        if (nm <= 0) { phs = (phs + _frq) & OSCBNK_PHSMSK; return 0.0; }
        const int nw = ws->nWarp;
        const MYFLOAT sr = (MYFLOAT)_appState->sr;
        const MYFLOAT f  = std::fabs(cps);
        const uint32_t rphs = (phs + pmPhase) & OSCBNK_PHSMSK;   // warp is baked → no runtime warpPhase
        // pw is the ABSOLUTE modulated morph; the frames span _bakedRange, so index by
        // how far along that span we are (which is just the modulation depth).
        const MYFLOAT morph = wtNormPos(pw, _bakedRange.mA, _bakedRange.mB);
        int f0 = 0; MYFLOAT mf = 0.0;
        if (nm > 1) { MYFLOAT fp = morph * (nm - 1); f0 = (int)fp; if (f0 > nm - 2) f0 = nm - 2; mf = fp - f0; }
        const int f1 = (nm > 1) ? f0 + 1 : f0;
        MYFLOAT out;
        if (nw == 1) {                                   // no warp: morph interp only
            const MYFLOAT s0 = ws->at(f0, 0).getTable(sr, f).read(rphs);
            out = (nm > 1) ? s0 + (ws->at(f1, 0).getTable(sr, f).read(rphs) - s0) * mf : s0;
        } else {                                         // bilinear morph × warp
            const MYFLOAT wa = wtNormPos(_warpAmt, _bakedRange.wA, _bakedRange.wB);
            MYFLOAT wp = wa * (nw - 1); int w0 = (int)wp; if (w0 > nw - 2) w0 = nw - 2;
            const MYFLOAT wf = wp - w0; const int w1 = w0 + 1;
            const MYFLOAT s00 = ws->at(f0, w0).getTable(sr, f).read(rphs);
            const MYFLOAT s01 = ws->at(f0, w1).getTable(sr, f).read(rphs);
            const MYFLOAT a = s00 + (s01 - s00) * wf;
            MYFLOAT b = a;
            if (nm > 1) {
                const MYFLOAT s10 = ws->at(f1, w0).getTable(sr, f).read(rphs);
                const MYFLOAT s11 = ws->at(f1, w1).getTable(sr, f).read(rphs);
                b = s10 + (s11 - s10) * wf;
            }
            out = a + (b - a) * mf;
        }
        if (wtWarpRuntime(_warpType)) out = applyRuntimeWarp(out);
        phs = (phs + _frq) & OSCBNK_PHSMSK;
        return out * gain * swapFadeTick();
    }
    if (_mode == 4) {   // PADsynth: a seconds-long sample, read with its own accumulator
        const MYFLOAT out = padRead(std::fabs(cps), 1, pw);   // pw carries morph, as in mode 3
        // pmPhase is deliberately ignored: offsetting the read of a five-second
        // sample jumps to unrelated audio rather than shifting a waveform. Keep the
        // single-cycle accumulator ticking so a sync source downstream still works.
        //
        // NOTE: no swapFadeTick() here, unlike mode 3 and unlike the unison path below.
        // It made no difference while this was the only reachable PAD path and the swap
        // duck was gated off (SWAP_DUCK_ENABLED), but the two PAD paths are BOTH live
        // now that unison stacks, so whoever re-enables the duck has to pick one.
        phs = (phs + _frq) & OSCBNK_PHSMSK;
        return out * gain;
    }
    if (!_tables) return 0.0;
    WaveTable& t = _tables->getTable((MYFLOAT)_appState->sr, std::fabs(cps));
    // Move to the phase-mod read position, let the table tick read+advance from
    // there, then overwrite the accumulator with a clean +_frq step so PM only
    // shifts the readout and never drifts the pitch (pmPhase==0 is a no-op).
    const uint32_t saved = phs;
    phs = (phs + pmPhase) & OSCBNK_PHSMSK;
    MYFLOAT out;
    if (_mode == 0)
        out = t.tick(phs, _frq) * gain;
    else if (_mode == 1)  // PWM square: DC-corrected constant-amplitude formula
        out = t.tickpwm(phs, _frq, pw) * gain;
    else  // mode 2 (saw/tri ramp): 1-pw keeps the fract(-pw) offset convention
        out = t.tickpw(phs, _frq, 1.0 - pw) * gain;
    phs = (saved + _frq) & OSCBNK_PHSMSK;
    return out;
}

void Vco::tickOS2(const MYFLOAT cps, const MYFLOAT gain, const MYFLOAT pw,
                  const uint32_t pm0, const uint32_t pm1, MYFLOAT& s0, MYFLOAT& s1) {
    _frq = OSCBNK_PHS2INT(cps);
    const uint32_t half = _frq >> 1u;   // per-subsample increment (2x oversampling)
    if (_mode == -1) {
        uint32_t r0 = (phs + pm0) & OSCBNK_PHSMSK;
        s0 = _table[(int32_t)r0 >> (int32_t)_lobits] * gain;
        phs = (phs + half) & OSCBNK_PHSMSK;
        uint32_t r1 = (phs + pm1) & OSCBNK_PHSMSK;
        s1 = _table[(int32_t)r1 >> (int32_t)_lobits] * gain;
        phs = (phs + (_frq - half)) & OSCBNK_PHSMSK;
        return;
    }
    if (_mode == 5) {   // modal: no phase accumulator, so there is nothing for PM to
                        // offset. synth.cpp's wtCarrier guard should keep us out of
                        // here entirely; this branch exists so that if the guard is
                        // ever loosened the result is a wrong sound, not a null deref.
        s0 = tick(cps, gain, pw);
        s1 = s0;
        return;
    }
    if (_mode == 4) {   // PADsynth: PM is meaningless on a multi-second sample, and
                        // the oversampled path exists only to keep PM sidebands from
                        // folding. Produce two half-step samples, ignore pm0/pm1.
        const MYFLOAT f = std::fabs(cps);
        // Two samples out of this call, so the duck advances twice — otherwise the
        // fade would run at half speed on the oversampled path only.
        s0 = padRead(f, 2, pw) * gain * swapFadeTick();
        s1 = padRead(f, 2, pw) * gain * swapFadeTick();
        phs = (phs + _frq) & OSCBNK_PHSMSK;
        return;
    }
    if (!_tables) { s0 = s1 = 0.0; return; }
    // table chosen for the real fundamental — the base waveform stays bandlimited
    // to 1x Nyquist, leaving headroom for the PM sidebands up to 2x Nyquist.
    WaveTable& t = _tables->getTable((MYFLOAT)_appState->sr, std::fabs(cps));
    // read-only lookups at the PM-offset positions (frq passed to the table tick
    // is irrelevant here — we overwrite phs afterwards to the clean +half step).
    uint32_t saved = phs;
    phs = (phs + pm0) & OSCBNK_PHSMSK;
    if      (_mode == 0) s0 = t.tick(phs, half) * gain;
    else if (_mode == 1) s0 = t.tickpwm(phs, half, pw) * gain;
    else                 s0 = t.tickpw(phs, half, 1.0 - pw) * gain;
    phs = (saved + half) & OSCBNK_PHSMSK;

    saved = phs;
    phs = (phs + pm1) & OSCBNK_PHSMSK;
    if      (_mode == 0) s1 = t.tick(phs, half) * gain;
    else if (_mode == 1) s1 = t.tickpwm(phs, half, pw) * gain;
    else                 s1 = t.tickpw(phs, half, 1.0 - pw) * gain;
    phs = (saved + (_frq - half)) & OSCBNK_PHSMSK;
}

MYFLOAT Vco::tickUnison(const MYFLOAT cps, const MYFLOAT gain, const MYFLOAT pw,
                        const VcoUnison& u) {
    const int n = u.n < 1 ? 1 : (u.n > UNISON_MAX ? UNISON_MAX : u.n);
    _frq = OSCBNK_PHS2INT(cps);   // centre increment, kept for sync/reset on voice 0
    MYFLOAT acc = 0.0;
    // accumulator for unison voice k (0 = primary phs, rest = _uphs[])
    #define UNI_PHASE(k) (*((k) == 0 ? &phs : &_uphs[(k) - 1]))

    if (_mode == -1) {   // sine
        for (int k = 0; k < n; k++) {
            uint32_t& ph = UNI_PHASE(k);
            const uint32_t frq = OSCBNK_PHS2INT(cps * u.ratio[k]);
            const int32_t idx = (int32_t)(ph & OSCBNK_PHSMSK) >> (int32_t)_lobits;
            ph = (ph + frq) & OSCBNK_PHSMSK;
            acc += _table[idx] * u.gain[k];
        }
        return acc * gain;
    }
    if (_mode == 5) {    // modal
        // Also not stacked: a unison copy means a second 14-partial bank, and the
        // per-partial amplitude scatter already varies the timbre note to note.
        // Detuned copies of a struck resonator beat against each other rather than
        // thickening, which is the opposite of what the knob promises.
        return tick(cps, gain, pw);
    }
    if (_mode == 4) {    // PADsynth
        // Set up once per sample rather than once per copy: the region latch, the rate
        // and the morph-level pick are the same for every copy, so padPrep hoists them
        // and each copy is then just two interpolated reads and an add.
        //
        // DETUNE is what makes this worth doing; a pure offset would not be. Summing
        // copies of a random-phase table at the SAME rate only adds level: the segments
        // are decorrelated by construction, so the sum has the same spectrum and the
        // same texture, just sqrt(n) louder. The ratio is what makes copies beat
        // against each other — and that beating is coherent and periodic, where
        // BANDWIDTH's is a random-phase wash, so the two thicken in different ways
        // rather than duplicating each other.
        //
        PadTap t;
        if (!padPrep(std::fabs(cps), 1, pw, t)) {
            phs = (phs + _frq) & OSCBNK_PHSMSK;
            return 0.0;
        }
        for (int k = 0; k < n; k++) {
            uint64_t& ph = (k == 0) ? _padPhase : _upadPhase[k - 1];
            // inc is ~2^46..2^51, so scaling it through a double keeps every bit that
            // matters (53-bit mantissa) — the detune ratios are within a few cents of 1.
            const uint64_t inc = (k == 0) ? t.inc
                                          : (uint64_t)((double)t.inc * (double)u.ratio[k]);
            acc += padTapRead(t, ph, inc) * u.gain[k];
        }
        phs = (phs + _frq) & OSCBNK_PHSMSK;
        return acc * gain * swapFadeTick();
    }
    if (_mode == 3) {    // wavetable: pw carries the morph position (0..1)
        const WavetableSet* ws = _wt.get();
        const int nm = ws ? ws->nMorph : 0;
        if (nm <= 0) {
            for (int k = 0; k < n; k++) {
                uint32_t& ph = UNI_PHASE(k);
                ph = (ph + OSCBNK_PHS2INT(cps * u.ratio[k])) & OSCBNK_PHSMSK;
            }
            return 0.0;
        }
        const int nw = ws->nWarp;
        const MYFLOAT sr = (MYFLOAT)_appState->sr;
        const MYFLOAT cf = std::fabs(cps);
        const MYFLOAT morph = wtNormPos(pw, _bakedRange.mA, _bakedRange.mB);   // as in tick()
        int f0 = 0; MYFLOAT mf = 0.0;
        if (nm > 1) { const MYFLOAT fp = morph * (nm - 1); f0 = (int)fp; if (f0 > nm - 2) f0 = nm - 2; mf = fp - f0; }
        const int f1 = (nm > 1) ? f0 + 1 : f0;
        int w0 = 0, w1 = 0; MYFLOAT wf = 0.0;
        if (nw > 1) {
            const MYFLOAT wa = wtNormPos(_warpAmt, _bakedRange.wA, _bakedRange.wB);
            MYFLOAT wp = wa * (nw - 1); w0 = (int)wp; if (w0 > nw - 2) w0 = nw - 2; wf = wp - w0; w1 = w0 + 1;
        }
        // fetch the (up to) 4 corner tables once at the centre frequency (warp baked),
        // reuse for all detuned copies; each copy reads at its own phase (bilinear).
        WaveTable& t00 = ws->at(f0, w0).getTable(sr, cf);
        WaveTable& t01 = ws->at(f0, w1).getTable(sr, cf);
        WaveTable& t10 = ws->at(f1, w0).getTable(sr, cf);
        WaveTable& t11 = ws->at(f1, w1).getTable(sr, cf);
        for (int k = 0; k < n; k++) {
            uint32_t& ph = UNI_PHASE(k);
            const uint32_t frq = OSCBNK_PHS2INT(cps * u.ratio[k]);
            const MYFLOAT r00 = t00.read(ph), r01 = t01.read(ph), r10 = t10.read(ph), r11 = t11.read(ph);
            const MYFLOAT a = r00 + (r01 - r00) * wf, b = r10 + (r11 - r10) * wf;
            ph = (ph + frq) & OSCBNK_PHSMSK;
            acc += (a + (b - a) * mf) * u.gain[k];
        }
        if (wtWarpRuntime(_warpType)) acc = applyRuntimeWarp(acc);
        return acc * gain * swapFadeTick();
    }
    if (!_tables) return 0.0;
    WaveTable& t = _tables->getTable((MYFLOAT)_appState->sr, std::fabs(cps));
    for (int k = 0; k < n; k++) {
        uint32_t& ph = UNI_PHASE(k);
        const uint32_t frq = OSCBNK_PHS2INT(cps * u.ratio[k]);
        MYFLOAT out;
        if      (_mode == 0) out = t.tick(ph, frq);
        else if (_mode == 1) out = t.tickpwm(ph, frq, pw);
        else                 out = t.tickpw(ph, frq, 1.0 - pw);
        acc += out * u.gain[k];
    }
    return acc * gain;
}
#undef UNI_PHASE

void Vco::spreadUnison(int n) {
    if (n < 2) return;
    if (n > UNISON_MAX) n = UNISON_MAX;
    // even spread across the phase range so the copies don't start aligned
    for (int k = 1; k < n; k++)
        _uphs[k - 1] = (uint32_t)((double)k / (double)n * (double)OSCBNK_PHSMAX_32)
                       & OSCBNK_PHSMSK;
    // PADsynth copies get the same treatment on their own 64-bit accumulators, but
    // spread RELATIVE to this voice's read offset rather than from zero. padNoteOn has
    // already re-rolled _padPhase by the time this runs (see the note-on order in
    // synth.cpp), so keeping the offset preserves the per-voice randomisation while
    // still putting the copies as far apart inside the table as they can get. Starting
    // them at absolute positions would make every voice's copies land on the same few
    // segments, which is the correlation padNoteOn exists to break.
    for (int k = 1; k < n; k++)
        _upadPhase[k - 1] = _padPhase
                          + (uint64_t)((double)k / (double)n * 18446744073709551616.0);
}

void Vco::selectWavetable(int n) {
    _tableNum = n;
    ensureWt(/*immediate=*/true);   // note-on path; retries until the set is in hand
}

// Let go of a shared set WITHOUT risking the deallocation here. Dropping the last
// reference to a WavetableSet runs ~1000 frees (a PadSet is worse), which must never
// happen on the audio thread — and the warm LRU is only 8 deep, so "something else
// still holds it" is a likely accident, not a guarantee. Hand a reference to the
// UiTasks worker and let the destructor run there.
//
// The copy is deliberate: the closure is built before add_task can report failure,
// so capturing by move would leave the destructor to fire right here on the full-
// queue path — the exact thing being avoided. Copying means a failed enqueue just
// drops back to the caller's own reference and changes nothing.
template <class T>
static bool handOffRelease(tsl::AppState* app, std::shared_ptr<T>& p) {
    if (!p || !app) return false;
    auto ref = p;   // refcount bump only; no allocation
    if (!app->UiTasksQueue.add_task([r = std::move(ref)]() mutable { r.reset(); }))
        return false;
    p.reset();      // the worker holds a reference, so this can't reach zero
    return true;
}

// Give up this oscillator's claim on the shared table sets. Called when a voice is
// returned to the pool: FastQueue recycles VcoNote objects and only wipes them in
// _alloc() on the NEXT note-on, so without this a finished voice keeps its set
// resident until its pool slot happens to be reused — 10 voices x 3 oscillators can
// pin ten different sets at ~12 MB each — and that wipe would then run the whole
// deallocation inside the note-on callback.
void Vco::releaseTables() {
    cancelSwapFade();
    handOffRelease(_appState, _wt);
    handOffRelease(_appState, _pad);
}

void Vco::cancelSwapFade() {
    // Hand the pendings off rather than reset() them: this voice may hold the last
    // reference, and dropping it here would run ~1000 frees on the audio thread.
    if (_wtPending)  handOffRelease(_appState, _wtPending);
    if (_padPending) handOffRelease(_appState, _padPending);
    _wtPending.reset();
    _padPending.reset();
    _pendingTable = _pendingType = -1;
    _swapGain      = 1.;
    _swapInc       = 0.;
    _swapFadingOut = false;
}

void Vco::refreshWt(bool immediate) {
    // Audio thread: never build here. Use a ready set if we have one; otherwise
    // request an off-thread build and keep the previous _wt as fallback (the osc
    // keeps sounding on the old table until the new set lands a few ms later).
    // _bakedRange only advances with _wt, so while a rebuild is pending the reads
    // stay normalised against the range actually in hand.
    if (auto sp = getWavetableSetIfReady(_tableNum, _warpType, _range)) {
        // Structural change (different table or warp type) on a voice that is already
        // sounding: hand it to the fade instead of swapping here. swapFadeTick() takes
        // the output down to zero, performs the swap at the bottom and brings it back.
        // Everything else — the first set, and range-only changes, whose consecutive
        // bakes differ by a fraction of a frame — is taken immediately.
        if (SWAP_DUCK_ENABLED && !immediate && _wt &&
            (_bakedTable != _tableNum || _bakedType != _warpType)) {
            _wtPending    = std::move(sp);
            _pendingRange = _range;
            _pendingTable = _tableNum;
            _pendingType  = _warpType;
            beginSwapFade();
            return;
        }
        // Taking a set right now supersedes whatever the fade was queued to apply;
        // leaving it in place would let the bottom of the duck overwrite this one.
        if (_wtPending) { handOffRelease(_appState, _wtPending); _wtPending.reset(); }
        adoptWt(std::move(sp), _range, _tableNum, _warpType);
        // A note-on takes its set outright, so any duck left over from the previous
        // note is meaningless — and leaving it running would fade the new note in from
        // silence for no reason. Safe to step the gain here only because immediate is
        // exclusively the note-on path.
        if (immediate) { _swapGain = 1.; _swapInc = 0.; _swapFadingOut = false; }
        return;
    }
    // Note-on is urgent: it must sound now, and its key is a destination rather than
    // a value a fader is sweeping through, so there is nothing to coalesce away.
    requestWavetableSet(_appState, _tableNum, _warpType, _range, /*urgent=*/immediate);
}

void Vco::beginSwapFade() {
    const MYFLOAT sr = _appState ? (MYFLOAT)_appState->sr : 48000.;
    const MYFLOAT n  = SWAP_FADE_SEC * sr;
    _swapInc = n > 1. ? 1. / n : 1.;
    _swapFadingOut = true;
}

MYFLOAT Vco::swapFadeStep() {
    if (_swapFadingOut) {
        _swapGain -= _swapInc;
        if (_swapGain <= 0.) {
            _swapGain = 0.;
            _swapFadingOut = false;
            // Bottom of the duck — the only place a structural swap happens. The
            // output is silent here, so replacing the tables under the phase
            // accumulator cannot produce a step however different they are.
            if (_wtPending)
                adoptWt(std::move(_wtPending), _pendingRange, _pendingTable, _pendingType);
            if (_padPending) {
                handOffRelease(_appState, _pad);
                _pad = std::move(_padPending);
            }
        }
    } else {
        _swapGain += _swapInc;
        if (_swapGain > 1.) _swapGain = 1.;
    }
    return _swapGain;
}

// Take a set as the one this oscillator is sounding. Split out because both the
// immediate path above and the fade's bottom-of-the-duck swap need it.
void Vco::adoptWt(std::shared_ptr<WavetableSet> sp, const WtRange& r, int table, int type) {
    handOffRelease(_appState, _wt);   // outgoing set: not freed on this thread
    _wt = std::move(sp);
    // Normalise against the range as REQUESTED, not as quantised. synth.cpp builds
    // the absolute value from the raw params, so raw is what puts an unmodulated
    // oscillator exactly on frame 0 — which is also the display's front slice.
    // Against the quantised range instead, zero modulation landed a few percent
    // along the span, and on a warp whose levels are hard steps (RING) that is
    // enough to put the sound and the picture either side of a carrier change.
    _bakedRange = r;
    _bakedTable = table;
    _bakedType  = type;   // ensureWt() stops retrying once these all match
}

// One PADsynth sample at frequency `f`, advancing the accumulator by 1/div of a
// step (div == 2 for the oversampled path). Returns 0 while a rebuild is pending.

// ── per-voice drift ──────────────────────────────────────────────────────────────
// WHY THIS EXISTS. A PADsynth table is FROZEN: the partial amplitudes are baked and
// every voice reads the same samples, so two notes at one pitch are the same waveform
// at a different offset, and a held note has no life of its own. Ear-reported
// 2026-08-16, once BANDWIDTH was audible at all, as sounding like "a bit of reverb on
// it" — which is exactly right, and exactly the problem: a fixed diffuse tail rather
// than something that moves.
//
// The fix is the cheapest one available and also what a real ensemble does: give each
// VOICE its own slow, tiny, zero-mean wander in playback rate, so voices beat against
// each other and a held note keeps moving. A counter and a multiply per sample, no
// memory and no rebuild — against the alternative of baking a third dimension into the
// set, which multiplies a 16.75 MB set by however many levels that dimension gets.
//
// TEN CENTS, AND THE FIRST TRY AT THREE WAS MEASURED USELESS. The thing this competes
// with is the BANDWIDTH smear, which is itself ~10 cents wide at the fundamental, so
// the partial's own phase already wanders. Measured as the standard deviation of the
// fundamental's instantaneous frequency over a held note, drift depth against total
// wander: none 4.87 ct, 3 ct -> 5.10, 10 ct -> 6.94, 25 ct -> 13.15. Subtracting the
// band's own contribution in quadrature, THREE CENTS ADDS 1.5 -- a 5% increase in a
// quantity that was already there, i.e. nothing. Ten adds 4.9, comparable to the band
// itself and a 43% increase in total wander, which is the point where it is doing
// something rather than decorating.
//
// Still slow and still zero-mean (targets symmetric about 0, note starts at 0), so
// nothing is detuned on average; ten cents peak at under 1 Hz is gentle ensemble wow,
// not vibrato. If this turns out to be inaudible by ear even at 10, DELETE IT rather
// than shipping a random walk that measurably does nothing.
//
// Two measurement traps here, both of which produced a confident wrong answer first:
//   - Envelope AC energy is BLIND to pitch modulation, and pitch is most of what this
//     does. It read flat at 3, 10 and 25 cents alike and nearly said "no effect".
//   - A phase-difference frequency estimator saturates at |df| < 1/(2*hop). At the
//     250 ms hop first used that ceiling is 2 Hz = 13.2 cents at C4, and every
//     condition INCLUDING NO DRIFT came back as exactly +-13. Use a 50 ms hop.
//
// Slow on purpose: a new target every ~0.9 s smoothed by a ~0.45 s one-pole puts all
// the motion below about 1 Hz. Faster becomes vibrato, which is a control the player
// should own rather than something the oscillator does behind their back.
static constexpr double PAD_DRIFT_CENTS = 10.0;   // peak deviation, measured
static constexpr double PAD_DRIFT_SEG_S = 0.9;    // seconds between targets
static constexpr double PAD_DRIFT_TAU_S = 0.45;   // one-pole smoothing

MYFLOAT Vco::padDriftTick(double step) {
    const double sr = _appState ? (double)_appState->sr : 48000.0;
    _padDriftLeft -= step;
    if (_padDriftLeft <= 0.0) {
        _padDriftRng = _padDriftRng * 1664525u + 1013904223u;   // LCG; the top bits
        _padDriftTarget = (MYFLOAT)((double)(_padDriftRng >> 8) / 8388608.0 - 1.0);
        _padDriftLeft += PAD_DRIFT_SEG_S * sr;
    }
    // tau is many samples, so 1-exp(-x) ≈ x. Scaled by `step` so the oversampled path,
    // which calls this twice per sample with step 0.5, walks at the same rate in real
    // time as the plain one — the same correction t.inc makes for the accumulator.
    _padDrift += (MYFLOAT)((_padDriftTarget - _padDrift) * (step / (PAD_DRIFT_TAU_S * sr)));
    return (MYFLOAT)std::pow(2.0, (double)_padDrift * PAD_DRIFT_CENTS / 1200.0);
}

// Shared per-sample setup. Split out of padRead so a unison stack pays for it ONCE
// per sample rather than once per copy — the region latch, the rate conversion and the
// morph-level pick are identical for every copy, and only the accumulator differs.
bool Vco::padPrep(MYFLOAT cps, int div, MYFLOAT morph, PadTap& t) {
    const PadSet* ps = _pad.get();
    if (!ps) return false;
    // cps is NORMALISED frequency (cycles per sample, 0.5 == Nyquist) — that is the
    // convention everywhere in this file, see OSCBNK_PHS2INT and getTable's p_scl/f.
    // The regions are keyed by real Hz, so convert once here.
    const double fHz = (double)cps * (_appState ? (double)_appState->sr : 48000.0);
    if (!_padRegionLatched) { _padRegion = padRegionFor(fHz); _padRegionLatched = true; }
    const PadRegion& rg = ps->region[_padRegion];
    if (rg.len <= 0) return false;

    // Playback rate = wanted pitch / the pitch this region was built at. The uint64
    // accumulator spans the whole table, so its natural overflow IS the loop point —
    // the table is seamless by construction (every bin holds an exact integer number
    // of cycles over its length).
    // The per-voice drift rides the RATE, which is the whole of it: reading the table
    // a shade fast or slow is a shade sharp or flat, and since the table is a
    // multi-second render rather than one cycle, it also walks the voice through a
    // different part of the beating pattern. One call per sample — padPrep is hoisted
    // out of the unison loop, so a stack of copies shares one walk rather than
    // advancing it once per copy.
    const double   rate = fHz / rg.f0 * (double)padDriftTick(1.0 / (double)(div > 1 ? div : 1));
    const uint64_t inc  = (uint64_t)(rate / (double)rg.len * 18446744073709551616.0);

    // Both levels share the same bin phases, so this lerp is an exact linear
    // interpolation of the magnitude spectrum — a spectral morph, not a dissolve.
    // Normalise against the range this SET was baked with, not the one currently
    // requested: while a rebuild is in flight the old set is still sounding, and
    // measuring the position against a range it was not built for would slide the
    // morph out from under the note. Same reasoning as _bakedRange in the WT path.
    const MYFLOAT mc = wtNormPos(morph, ps->params.mA, ps->params.mB);
    MYFLOAT mp = mc * (PAD_MORPH_LEVELS - 1);
    int m0 = (int)mp;
    if (m0 > PAD_MORPH_LEVELS - 2) m0 = PAD_MORPH_LEVELS - 2;
    if (m0 < 0) m0 = 0;

    t.la    = rg.lvl[m0].data();
    t.lb    = rg.lvl[m0 + 1].data();
    t.mf    = mp - m0;
    t.inc   = (div > 1) ? (inc / (uint64_t)div) : inc;
    t.shift = rg.shift;
    return true;
}

// One copy: interpolate within each morph level at this accumulator, blend, advance.
// `inc` is passed rather than taken from the tap so a unison copy can walk at its own
// detuned rate off the same shared setup.
inline MYFLOAT Vco::padTapRead(const PadTap& t, uint64_t& phase, uint64_t inc) {
    const uint32_t idx  = (uint32_t)(phase >> t.shift);
    const uint64_t fmsk = (1ull << t.shift) - 1ull;
    const MYFLOAT  fr   = (MYFLOAT)((double)(phase & fmsk) / (double)(1ull << t.shift));
    const MYFLOAT  sa   = t.la[idx] + (t.la[idx + 1] - t.la[idx]) * fr;
    const MYFLOAT  sb   = t.lb[idx] + (t.lb[idx + 1] - t.lb[idx]) * fr;
    phase += inc;
    return sa + (sb - sa) * t.mf;
}

MYFLOAT Vco::padRead(MYFLOAT cps, int div, MYFLOAT morph) {
    PadTap t;
    if (!padPrep(cps, div, morph, t)) return 0.0;
    return padTapRead(t, _padPhase, t.inc);
}

void Vco::setPad(const PadParams& p) {
    _padParams = p;
    // Compare against what the HELD set was built from, not just against the wanted
    // params. A rebuild takes ~150 ms, so the refreshPad() that follows a param
    // change almost always finds nothing ready and keeps the old set; if the retry
    // condition were "params changed" it would already be false by the next block
    // and the finished table would never be picked up at all. Cheap: a field compare
    // per block, and the lock inside refreshPad is only taken while one is pending.
    if (!_pad || _pad->params != _padParams) refreshPad();
}

void Vco::refreshPad(bool immediate) {
    // Audio thread: never build here. Keep the previous set playing until the new
    // one lands, exactly as refreshWt does — a pad rebuild is far slower than a
    // wavetable one, so dropping to silence would be very audible.
    const double sr = _appState ? (double)_appState->sr : 48000.0;
    if (auto sp = getPadSetIfReady(_padParams, sr)) {
        // Same rule as the wavetable path: a voice that is already sounding gets the
        // new set through the fade, so the ~150 ms design change lands as a duck rather
        // than as a jump in the middle of a seconds-long sample read.
        if (SWAP_DUCK_ENABLED && !immediate && _pad) { _padPending = std::move(sp); beginSwapFade(); return; }
        if (_padPending) { handOffRelease(_appState, _padPending); _padPending.reset(); }
        handOffRelease(_appState, _pad);
        _pad = std::move(sp);
        if (immediate) { _swapGain = 1.; _swapInc = 0.; _swapFadingOut = false; }
        return;
    }
    // Nothing holding: sound the same table at whatever settings are resident rather
    // than nothing at all. Only when _pad is empty -- a voice that already holds a set
    // keeps it, which is both closer to what was asked for and free of any switch.
    if (!_pad) {
        if (auto any = getPadSetSameTable(_padParams.table, sr)) _pad = std::move(any);
    }
    requestPadSet(_appState, _padParams, sr);
}

void Vco::padNoteOn() {
    refreshPad(/*immediate=*/true);   // nothing sounding yet — take the newest set
    _padRegionLatched = false;
    // Random start offset per voice. Without it every voice enters the table at
    // sample 0, so unison copies and retriggered notes are phase-correlated and the
    // texture collapses toward a point source — the exact thing the random phases
    // are there to prevent.
    static std::atomic<uint32_t> ctr{0x9E3779B9u};
    const uint32_t a = ctr.fetch_add(0x9E3779B9u, std::memory_order_relaxed);
    _padPhase = ((uint64_t)(uint32_t)(wtHash01(a) * 4294967040.0) << 32)
              ^  (uint64_t)(uint32_t)(wtHash01(a ^ 0x85EBCA6Bu) * 4294967040.0);
    // Seed the drift off the same counter so every voice wanders differently — two
    // voices sharing a walk would stay locked together, which is the thing this is
    // here to prevent. Starting at 0 with no time left makes the note begin exactly in
    // tune and pick its first target on the next sample.
    _padDriftRng    = a ^ 0xB5297A4Du;
    _padDrift       = 0.;
    _padDriftTarget = 0.;
    _padDriftLeft   = 0.;
}

void Vco::modalNoteOn(double freqHz, double attackSec, double velocity, uint32_t seed) {
    // Velocity opens the top of the spectrum rather than just turning the volume up:
    // a harder strike is a shorter contact time, so it delivers energy to modes a
    // soft one never reaches. This is what replaced the generative engine's random
    // +-7% gesture wobble — the engine had no velocity to work with, PA does, and a
    // player's dynamics are a far better source of that variation than an rng.
    const double hardness = std::clamp(_modalParams.hardness + 0.45 * (velocity - 0.5),
                                       0.0, 1.0);
    _modal.configure(_modalParams.character, freqHz, _modalParams.decaySec,
                     _modalParams.bright, hardness, _modalParams.position, seed);
    _modal.excite(attackSec * _modal.attackScale());
    _modalCounter = MODAL_RETUNE_INTERVAL;
}

MYFLOAT Vco::applyRuntimeWarp(MYFLOAT x) {
    return runtimeWarpSample(_warpType, _warpAmt, x, _decHold, _decPhase);
}

void Vco::reset() {
    // hard sync: keep the sub-sample remainder so the sync edge isn't
    // quantized to the sample grid
    phs %= (_frq > 0 ? _frq : 1);
}

void Vco::rev() {
    _dir *= -1;
}
