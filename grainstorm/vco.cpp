//
// Created by pr on 04.06.19.
//
/*
 static int32_t vco2set(CSOUND *csound, VCO2 *p)
{
    int32_t     mode, tnum;
    int32_t     tnums[8] = { 0, 0, 1, 2, 1, 3, 4, 5 };
    int32_t     modes[8] = { 0, 1, 2, 0, 0, 0, 0, 0 };
    MYFLT   x;
    uint32_t min_args;

    if (p->vco2_nr_table_arrays == NULL || p->vco2_tables == NULL) {
      STDOPCOD_GLOBALS  *pp = get_oscbnk_globals(csound);
      p->vco2_nr_table_arrays = &(pp->vco2_nr_table_arrays);
      p->vco2_tables = &(pp->vco2_tables);
    }

if (UNLIKELY(p->INOCOUNT > 6)) {
return csound->InitError(csound, Str("vco2: too many input arguments"));
}
mode = (int32_t) MYFLT2LONG(*(p->imode)) & 0x1F;
if (mode & 1) return OK;               // skip initialisation
// more checks
min_args = 2;
if ((mode & 14) == 2 || (mode & 14) == 4) min_args = 4;
if (mode & 16) min_args = 5;
if (UNLIKELY(p->INOCOUNT < min_args)) {
return csound->InitError(csound,
        Str("vco2: insufficient required arguments"));
}

//FIXME

//    if (UNLIKELY(p->XINCODE)) {
//      return csound->InitError(csound, Str("vco2: invalid argument type"));
//    }

// select table array and algorithm, according to waveform
tnum = tnums[(mode & 14) >> 1];
p->mode = modes[(mode & 14) >> 1];
// initialise tables if not done yet
if (tnum >= *(p->vco2_nr_table_arrays) ||
(*(p->vco2_tables))[tnum] == NULL) {
if (LIKELY(tnum < 5))
vco2_tables_create(csound, tnum, -1, NULL);
else {
return csound->InitError(csound, Str("vco2: table array not found for "
                                     "user defined waveform"));
}
}
#ifdef VCO2FT_USE_TABLE
p->nparts_tabl = (*(p->vco2_tables))[tnum]->nparts_tabl;
#else
// address of number of partials list (with offset for padding)
p->nparts = (*(p->vco2_tables))[tnum]->nparts
            + (*(p->vco2_tables))[tnum]->ntabl;
p->npart_old = p->nparts + ((*(p->vco2_tables))[tnum]->ntabl >> 1);
p->tables = (*(p->vco2_tables))[tnum]->tables;
#endif
// set misc. parameters
p->init_k = 1;
p->pm_enabled = (mode & 16 ? 1 : 0);
if ((mode & 16) || (p->INOCOUNT < 5))
p->phs = 0UL;
else {
x = *(p->kphs); x -= (MYFLT) ((int32) x);
p->phs = OSCBNK_PHS2INT(x);
}
p->f_scl = csound->onedsr;
x = (p->INOCOUNT < 6 ? FL(0.5) : *(p->inyx));
if (x < FL(0.001)) x = FL(0.001);
if (x > FL(0.5)) x = FL(0.5);
p->p_min = x / (MYFLT) VCO2_MAX_NPART;
p->p_scl = x;
return OK;
}

// ---- vco2 opcode (performance) ----

static int32_t vco2(CSOUND *csound, VCO2 *p)
{
    uint32_t offset = p->h.insdshead->ksmps_offset;
    uint32_t early  = p->h.insdshead->ksmps_no_end;
    uint32_t nn, nsmps = CS_KSMPS;
    int32_t      n;
    VCO2_TABLE      *tabl;
    uint32  phs, phs2, frq, frq2, lobits, mask;
#ifdef VCO2FT_USE_TABLE
    MYFLT   f, f1, npart, pfrac, v, *ftable, kamp, *ar;
    if (UNLIKELY(p->nparts_tabl == NULL)) {
#else
    MYFLT   f, f1, npart, *nparts, pfrac, v, *ftable, kamp, *ar;
    if (UNLIKELY(p->tables == NULL)) {
#endif
        return csound->PerfError(csound, &(p->h),
                                 Str("vco2: not initialised"));
    }
    // if 1st k-cycle, initialise now
    if (p->init_k) {
        p->init_k = 0;
        if (p->pm_enabled) {
            f = p->kphs_old = *(p->kphs); f -= (MYFLT) ((int32) f);
            p->phs = OSCBNK_PHS2INT(f);
        }
        if (p->mode) {
            p->kphs2_old = -(*(p->kpw));
            f = p->kphs2_old; f -= (MYFLT) ((int32) f);
            p->phs2 = (p->phs + OSCBNK_PHS2INT(f)) & OSCBNK_PHSMSK;
        }
    }
    ar = p->ar;
    if (UNLIKELY(offset)) memset(ar, '\0', offset*sizeof(MYFLT));
    if (UNLIKELY(early)) {
        nsmps -= early;
        memset(&ar[nsmps], '\0', early*sizeof(MYFLT));
    }
    // calculate frequency (including phase modulation)
    f = *(p->kcps) * p->f_scl;
    frq = OSCBNK_PHS2INT(f);
    if (p->pm_enabled) {
        f1 = (MYFLT) ((double) *(p->kphs) - (double) p->kphs_old)
             / (nsmps-offset);
        p->kphs_old = *(p->kphs);
        frq = (frq + OSCBNK_PHS2INT(f1)) & OSCBNK_PHSMSK;
        f += f1;
    }
    // find best table for current frequency
    npart = (MYFLT)fabs(f); if (npart < p->p_min) npart = p->p_min;
#ifdef VCO2FT_USE_TABLE
    tabl = p->nparts_tabl[(int32_t) (p->p_scl / npart)];
#else
    npart = p->p_scl / npart;
    nparts = p->npart_old;
    if (npart < *nparts) {
        do {
            nparts--; nn = 1;
            while (npart < *(nparts - nn)) {
                nparts = nparts - nn; nn <<= 1;
            }
        } while (nn > 1);
    }
    else if (npart >= *(nparts + 1)) {
        do {
            nparts++; nn = 1;
            while (npart >= *(nparts + nn + 1)) {
                nparts = nparts + nn; nn <<= 1;
            }
        } while (nn > 1);
    }
    p->npart_old = nparts;
    tabl = p->tables + (int32_t) (nparts - p->nparts);
#endif
    // copy object data to local variables
    kamp = *(p->kamp);
    phs = p->phs;
    lobits = tabl->lobits; mask = tabl->mask; pfrac = tabl->pfrac;
    ftable = tabl->ftable;

    if (!p->mode) {                   // - mode 0: simple table playback
        for (nn=offset; nn<nsmps; nn++) {
            n = phs >> lobits;
            v = ftable[n++];
            v += (ftable[n] - v) * (MYFLT) ((int32) (phs & mask)) * pfrac;
            phs = (phs + frq) & OSCBNK_PHSMSK;
            ar[nn] = v * kamp;
        }
    }
    else {
        v = -(*(p->kpw));                                 // pulse width
        f1 = (MYFLT) ((double) v - (double) p->kphs2_old) / (nsmps-offset);
        f = p->kphs2_old; f -= (MYFLT) ((int32) f); if (f < FL(0.0)) f++;
        p->kphs2_old = v;
        phs2 = p->phs2;
        frq2 = (frq + OSCBNK_PHS2INT(f1)) & OSCBNK_PHSMSK;
        if (p->mode == 1) {               // - mode 1: PWM -
            // DC correction offset
            f = FL(1.0) - FL(2.0) * f;
            f1 *= FL(-2.0);
            for (nn=offset; nn<nsmps; nn++) {
                n = phs >> lobits;
                v = ftable[n++];
                ar[nn] = v + (ftable[n] - v) * (MYFLT) ((int32) (phs & mask)) * pfrac;
                n = phs2 >> lobits;
                v = ftable[n++];
                v += (ftable[n] - v) * (MYFLT) ((int32) (phs2 & mask)) * pfrac;
                ar[nn] = (ar[nn] - v + f) * kamp;
                phs = (phs + frq) & OSCBNK_PHSMSK;
                phs2 = (phs2 + frq2) & OSCBNK_PHSMSK;
                f += f1;
            }
        }
        else {                            // - mode 2: saw / triangle ramp -
            for (nn=offset; nn<nsmps; nn++) {
                n = phs >> lobits;
                v = ftable[n++];
                ar[nn] = v + (ftable[n] - v) * (MYFLT) ((int32) (phs & mask)) * pfrac;
                n = phs2 >> lobits;
                v = ftable[n++];
                v += (ftable[n] - v) * (MYFLT) ((int32) (phs2 & mask)) * pfrac;
                ar[nn] = (ar[nn] - v) * (FL(0.25) / (f - f * f)) * kamp;
                phs = (phs + frq) & OSCBNK_PHSMSK;
                phs2 = (phs2 + frq2) & OSCBNK_PHSMSK;
                f += f1;
            }
        }
        p->phs2 = phs2;
    }
    // save oscillator phase
    p->phs = phs;
    return OK;
}

*/
#include "defines.h"
#include "ffttools.h"
#include "random.h"
#include "track.h"
#include "lfo.h"
#include "vco.h"
#include "grainstorm.h"
#include "track.h"
#include "app.h"

static std::mutex guard1;
static std::unique_ptr<FFT> ffts[NUMFFTS + 1];


constexpr int TABLE_SIZE = 2048;
constexpr int NUM_PW_TABLES = 16; // Number of pulse width variations
constexpr int HARMONIC_STEP = 16; // Harmonic step size
constexpr int HARMONIC_STEP_GRAINENV = 64; // Harmonic step size



#define CHECKFFT(_M) int32_t _index = (int) std::log2(_M); if(ffts[_index].get() == nullptr) ffts[_index] = std::make_unique<FFT>(_M);auto fft = ffts[_index].get();


// Adjusted phase calculations for different waveforms

double adjusted_phase_square2(double phase, double pw) {
    return (phase < pw) ? (phase / pw / 2.0) : (0.5 + (phase - pw) / (1.0 - pw) / 2.0);
}

double adjusted_phase_sawtooth2(double phase, double pw) {
    return (phase < pw) ? (phase / pw) : ((phase - pw) / (1.0 - pw));
}

double adjusted_phase_triangle2(double phase, double pw) {
    return (phase < pw) ? (phase / pw) : (1.0 - (phase - pw) / (1.0 - pw));
}

double adjusted_phase_square(double phase, double pw) {
    return phase / pw;
}

double adjusted_phase_sawtooth(double phase, double pw) {
    if (phase < pw) {
        return phase / pw;
    }
    else {
        return (phase - pw) / (1.0 - pw);
    }
}

double adjusted_phase_triangle(double phase, double pw) {
    if (phase < pw) {
        return phase / pw;
    }
    else {
        return 1.0 - ((phase - pw) / (1.0 - pw));
    }
}

tsl::AlignedVector<double> generateSawtoothWaveform(int tableSize, double dutyCycle) {
    tsl::AlignedVector<double> waveform(tableSize);
    int transitionPoint = static_cast<int>((1.0 - 0.5 * dutyCycle) * tableSize);

    for (int i = 0; i < transitionPoint; ++i) {
        waveform[i] = 2.0 * (i / static_cast<double>(transitionPoint)) - 1.0;
    }
    for (int i = transitionPoint; i < tableSize; ++i) {
        waveform[i] =
            2.0 * ((i - transitionPoint) / static_cast<double>(tableSize - transitionPoint)) -
            1.0;
    }

    return waveform;
}

tsl::AlignedVector<double> generateTriangleWaveform(int tableSize, double dutyCycle) {
    tsl::AlignedVector<double> waveform(tableSize);
    int upSamples = static_cast<int>((0.5 + 0.5 * dutyCycle) * tableSize);
    int downSamples = tableSize - upSamples;

    for (int i = 0; i < upSamples; ++i) {
        waveform[i] = 2.0 * (i / static_cast<double>(upSamples)) - 1.0;
    }
    for (int i = 0; i < downSamples; ++i) {
        waveform[upSamples + i] = 1.0 - 2.0 * (i / static_cast<double>(downSamples));
    }

    return waveform;
}

// Generate a pulse waveform for a given pulse width
tsl::AlignedVector<double> generatePulseWaveform(int tableSize, double pulseWidth) {
    tsl::AlignedVector<double> waveform(tableSize);
    int transitionPoint = static_cast<int>((0.5 + 0.4 * pulseWidth) * tableSize);
    for (int i = 0; i < tableSize; ++i) {
        waveform[i] = (i < transitionPoint) ? 1.0 : -1.0;
    }
    return waveform;
}


void generateCustomTable(std::vector<tsl::AlignedVector<double>>& wavetables,
    tsl::AlignedVector<MYFLOAT>& inn) {
    int maxHarmonics = WINDOW_SIZE / 8;
    int numTables = (maxHarmonics / HARMONIC_STEP_GRAINENV) + 1;
    wavetables.resize(numTables);
    auto baseWaveform = inn;
    FFT fft(WINDOW_SIZE);
    fft.forward(baseWaveform.data());
    baseWaveform[0] = baseWaveform[1] = 0.0;
    for (int i = 0; i < numTables; ++i) {
        wavetables[i] = baseWaveform;
        int harmonics = (i + 1) * HARMONIC_STEP;
        harmonics = std::min(harmonics, maxHarmonics);
        for (int k = harmonics; k < WINDOW_SIZE / 2; ++k) {
            wavetables[i][k * 2] = wavetables[i][k * 2 + 1] = 0.0;
        }
        fft.backward(wavetables[i].data());
        wavetables[i].data()[WINDOW_SIZE] = wavetables[i].data()[0];
    }
}

tsl::AlignedVector<double>&
getGrainWindow(std::vector<tsl::AlignedVector<double>>& wavetables, int grainsize, MYFLOAT cycles) {
    int harmonics = static_cast<int>(grainsize / (2 * cycles));
    int index = harmonics / HARMONIC_STEP_GRAINENV;
    return wavetables[std::min(index, static_cast<int>(wavetables.size() - 1))];
}


static std::vector<std::vector<tsl::AlignedVector<double>>> waveforms[3]{};


// Generate wavetables for pulse waveforms with different pulse widths and harmonics
std::vector<std::vector<tsl::AlignedVector<double>>> generateWavetables(int type) {
    int maxHarmonics = TABLE_SIZE / 2;
    int numTables = (maxHarmonics / HARMONIC_STEP) + 1;
    std::vector<std::vector<tsl::AlignedVector<double>>> wavetables(NUM_PW_TABLES,
        std::vector<tsl::AlignedVector<double>>(
            numTables,
            tsl::AlignedVector<double>(
                TABLE_SIZE)));
    FFT fft(TABLE_SIZE);
    for (int pw = 0; pw < NUM_PW_TABLES; ++pw) {
        double pulseWidth = static_cast<double>(pw) / (NUM_PW_TABLES - 1);
        auto baseWaveform =
            type == 0 ? generateTriangleWaveform(TABLE_SIZE, pulseWidth) : (type == 1
                ? generateSawtoothWaveform(
                    TABLE_SIZE, pulseWidth) : generatePulseWaveform(TABLE_SIZE,
                        pulseWidth));
        fft.forward(baseWaveform.data());
        baseWaveform[1] = 0.0;
        for (int i = 0; i < numTables; ++i) {
            int harmonics = (i + 1) * HARMONIC_STEP;
            harmonics = std::min(harmonics, maxHarmonics);

            auto spectrum = baseWaveform;
            for (int k = harmonics; k < TABLE_SIZE / 2; ++k) {
                spectrum[k * 2] = spectrum[k * 2 + 1] = 0.0;
            }
            fft.backward(spectrum.data());
            wavetables[pw][i] = spectrum;
        }
    }
    return wavetables;
}

constexpr double interpolate(double a, double b, double t) {
    return a + (b - a) * t;
}

void WaveTable::computeWavetable(int type, int nPartials) {
    type_ = type;
    nPart = nPartials;
    tableSize = computeTableSize();
    resize(tableSize + 2, 0);
    flenSetup();
    calculateTable(type);
    return;
}

MYFLOAT WaveTable::tick(uint32_t& phs, const uint32_t frq) const {
    uint32_t n = (phs >> lobits_32) & tableMask_32;
    if (type_ == -1) {
        phs = (phs + frq) & OSCBNK_PHSMSK_32;
        return data()[n];

    }
    else {                     /* - mode 0: simple table playback - */
        MYFLOAT v = data()[n++];
        v += (data()[n] - v) * (MYFLOAT)((int32_t)(phs & fractMask_32)) * pfrac_32;
        phs = (phs + frq) & OSCBNK_PHSMSK_32;
        return v;
    }
}

MYFLOAT WaveTable::tickpw(MYFLOAT& phs, const MYFLOAT frq, const MYFLOAT pw) const {
    // 1. Normalize phase
    if (phs >= 1.0) phs -= floor(phs);
    else if (phs < 0.0) phs += ceil(fabs(phs));

    // 2. Sample first sawtooth
    MYFLOAT realIdx1 = phs * (MYFLOAT)tableSize;
    uint32_t i0 = static_cast<uint32_t>(realIdx1) & tableMask_32;
    MYFLOAT frac1 = realIdx1 - floor(realIdx1);
    MYFLOAT smpl = data()[i0] + frac1 * (data()[i0 + 1] - data()[i0]);

    if (type_ == 1 || type_ == -1) {
        phs += frq;
        return smpl;
    }

    // 3. Sample second sawtooth
    MYFLOAT phs2 = phs + pw;
    if (phs2 >= 1.0) phs2 -= 1.0;

    MYFLOAT realIdx2 = phs2 * (MYFLOAT)tableSize;
    uint32_t j0 = static_cast<uint32_t>(realIdx2) & tableMask_32;
    MYFLOAT frac2 = realIdx2 - floor(realIdx2);
    MYFLOAT v = data()[j0] + frac2 * (data()[j0 + 1] - data()[j0]);

    // 4. Advance phase
    phs += frq;

    // 5. PWM output
    MYFLOAT denom = pw - (pw * pw);
    if (fabs(denom) < 1e-6) return 0.0;
  
    return (smpl - v) * (0.25 / denom);
}

MYFLOAT WaveTable::tickpw(uint32_t& phs, const uint32_t frq,
    const MYFLOAT pw) const {
    uint32_t phs2 = (phs + OSCBNK_PHS2INT_32(pw)) & OSCBNK_PHSMSK_32;
    int32_t n = (phs >> lobits_32) & tableMask_32;
    MYFLOAT v = data()[n++];
    MYFLOAT smpl =
        v + (data()[n] - v) * (phs & fractMask_32) * pfrac_32;
    n = phs2 >> lobits_32;
    v = data()[n++];
    v += (data()[n] - v) * (phs2 & fractMask_32) * pfrac_32;
    phs = (phs + frq) & OSCBNK_PHSMSK_32;
    return (smpl - v) * (.25 / (pw - pw * pw));
}

// Plain interpolated read + advance, 64-bit phase. Identical to the path
// tickpw64 already takes for the saw/sine tables, but without the pointless
// second lookup — and without the x2 that differencing a symmetric table by
// half a cycle produces, since shifting one by .5 just negates it.
MYFLOAT WaveTable::tick64(uint64_t& phs, const uint64_t frq) const {
    uint64_t n = (phs >> lobits_64) & tableMask_64;
    MYFLOAT  f = (MYFLOAT)(phs & fractMask_64) * pfrac_64;
    MYFLOAT  smpl = data()[n] + f * (data()[n + 1] - data()[n]);
    phs = (phs + frq) & OSCBNK_PHSMSK_64;
    return smpl;
}

MYFLOAT WaveTable::tickpw64(uint64_t& phs, const uint64_t frq,
    const MYFLOAT pw) const {

    uint64_t n1 = (phs >> lobits_64) & tableMask_64;
    MYFLOAT f1 = (MYFLOAT)(phs & fractMask_64) * pfrac_64;
    MYFLOAT smpl = data()[n1] + f1 * (data()[n1 + 1] - data()[n1]);
    if (type_ == 1 || type_ == -1) {
        phs = (phs + frq) & OSCBNK_PHSMSK_64;
        return smpl;
    }
    uint64_t phs2 = (phs + OSCBNK_PHS2INT_64(pw)) & OSCBNK_PHSMSK_64;

    uint64_t n2 = (phs2 >> lobits_64) & tableMask_64;
    MYFLOAT f2 = (MYFLOAT)(phs2 & fractMask_64) * pfrac_64;
    MYFLOAT v = data()[n2] + f2 * (data()[n2 + 1] - data()[n2]);

    phs = (phs + frq) & OSCBNK_PHSMSK_64;

    MYFLOAT denom = pw - pw * pw;
    if (fabs(denom) < 1e-5) return 0.0;
    return (smpl - v) * (0.25 / denom);
}

// PWM by differencing a LINEAR RAMP table. Both constructions below undo the
// calculateTable() peak normalization first (* peakScale_), because the ideal
// ramp is what makes the algebra come out to a constant amplitude — the DC
// correction in tickpwm64 is an absolute constant, and .25/(pw-pw*pw) in
// tickpwtri64 is calibrated for a unit ramp.

// Csound vco2 mode 1: pulse = saw(phs) - saw(phs + fract(-pw)) + (1 - 2f),
// constant +-1 at every pulse width. Feed it the type 1 (SAW) table.
// tickpw64's .25/(pw-pw*pw) is NOT valid here: applied to the square table it
// ran the level from +-2 at pw .5 up to +-9.7 at pw .05.
MYFLOAT WaveTable::tickpwm64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw) const {
    MYFLOAT f = -pw;
    f -= (MYFLOAT)((int64_t)f);
    if (f < 0.) f++;

    uint64_t n1 = (phs >> lobits_64) & tableMask_64;
    MYFLOAT  f1 = (MYFLOAT)(phs & fractMask_64) * pfrac_64;
    MYFLOAT  smpl = data()[n1] + f1 * (data()[n1 + 1] - data()[n1]);

    uint64_t phs2 = (phs + OSCBNK_PHS2INT_64(f)) & OSCBNK_PHSMSK_64;
    uint64_t n2 = (phs2 >> lobits_64) & tableMask_64;
    MYFLOAT  f2 = (MYFLOAT)(phs2 & fractMask_64) * pfrac_64;
    MYFLOAT  v = data()[n2] + f2 * (data()[n2 + 1] - data()[n2]);

    phs = (phs + frq) & OSCBNK_PHSMSK_64;
    return (smpl - v) * peakScale_ + (1.0 - 2.0 * f);
}

// Csound vco2 mode 2: a constant-amplitude triangle whose skew follows pw,
// differenced from the type 10 (4x(1-x)) table. Same math as tickpw64 but in
// un-normalized units, so it lands at the same +-1 as tickpwm64.
MYFLOAT WaveTable::tickpwtri64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw) const {
    uint64_t n1 = (phs >> lobits_64) & tableMask_64;
    MYFLOAT  f1 = (MYFLOAT)(phs & fractMask_64) * pfrac_64;
    MYFLOAT  smpl = data()[n1] + f1 * (data()[n1 + 1] - data()[n1]);

    uint64_t phs2 = (phs + OSCBNK_PHS2INT_64(pw)) & OSCBNK_PHSMSK_64;
    uint64_t n2 = (phs2 >> lobits_64) & tableMask_64;
    MYFLOAT  f2 = (MYFLOAT)(phs2 & fractMask_64) * pfrac_64;
    MYFLOAT  v = data()[n2] + f2 * (data()[n2 + 1] - data()[n2]);

    phs = (phs + frq) & OSCBNK_PHSMSK_64;
    MYFLOAT denom = pw - pw * pw;
    if (fabs(denom) < 1e-5) return 0.0;
    return (smpl - v) * peakScale_ * (0.25 / denom);
}


/*
MYFLOAT
WaveTable::tick2(MYFLOAT &phase, MYFLOAT &oldpw, const MYFLOAT frq, const MYFLOAT pw)  {
    MYFLOAT v = -pw;                               // pulse width
    MYFLOAT f1 = (v - oldpw);
    MYFLOAT f = oldpw;
    f -= (MYFLOAT) ((int32_t) f);
    if (f < 0.) f++;
    oldpw = v;
    auto phs2 = phase + f1;
    while (phs2 > 1.)phs2 -= 1.;
    auto floorphase = static_cast<int32_t>(floor(phase));
    auto frac = phase - floorphase;
    v = data()[floorphase++];
    MYFLOAT smpl =
            v + (data()[floorphase] - v) * frac;
    floorphase = static_cast<int32_t>(floor(phs2));
    frac = phs2 - floorphase;
    v = data()[floorphase++];
    v += (data()[floorphase] - v) * frac;
    phase += frq;
    while (phase >= 1.)phase -= 1;
    return (smpl - v) * (.25 / (f - f * f));



    //
    MYFLOAT lookup;
    if(type_==0)lookup = adjusted_phase_triangle2(phss, pw);
    else if(type_==1) lookup = adjusted_phase_sawtooth2(phss, pw);
    else lookup = adjusted_phase_square2(phss, pw);
    lookup *= tableSize;
    int floorPhase1 = static_cast<int>(lookup);
    auto smpl = data()[floorPhase1];
    phss += LOG2NORMAL(_STATE->params[0][GRAINVCOCPS].load())/_STATE->sr;
    if(phss>=1.)phss-=1.;
    return smpl;
     //
}
*/

void WaveTable::computeWavetable(int nPartials, tsl::AlignedVector<MYFLOAT>& in, uint32_t origSize) {
    type_ = -1;
    nPart = nPartials;
    tableSize = computeTableSize();
    resize(tableSize + 2, 0);
    flenSetup();

    for (int32_t j = 0; j < tableSize; j++) {
        double pos = (double)j * origSize / tableSize;
        int32_t i0 = (int32_t)pos;
        int32_t i1 = i0 + 1 < origSize ? i0 + 1 : i0;
        double frac = pos - i0;
        data()[j] = in[i0] + frac * (in[i1] - in[i0]);
    }
    //int index = (int) LOG2(tableSize);

    CHECKFFT(tableSize);
    fft->forward(data());
    data()[1] = 0.0;
    for (int k = 0; k < tableSize / 2; ++k) {
        if (k > nPartials)
            data()[(k << 1u)] = data()[(k << 1u) + 1] = 0.0;
    }
    fft->backward(data());
    data()[tableSize] = data()[0];
}

double triangle_wave_fourier_series_coeff(int k) {
    if (k % 2 == 0) {
        return 0;
    }
    else {
        return (8.0 / (PI_P * PI_P)) * (pow(-1, (k - 1) / 2)) / (k * k);
    }
}

double square_wave_fourier_series_coeff(int k) {
    return (k % 2 == 0) ? 0.0 : (4.0 / (PI_P * k));
}

double sawtooth_wave_fourier_series_coeff(int k) {
    return (k % 2 == 0) ? 0.0 : (2.0 / (PI_P * k)) * (k % 2 == 0 ? 0 : 1);
}


void WaveTable::calculateTable(int type) {
    /* allocate memory for FFT */
    int32_t minh;
    if (type >= 0) {                        /* no DC offset for   */
        minh = 1;
    }
    else
        minh = 0;
    MYFLOAT scaleFac = .5 * (MYFLOAT)tableSize;
    CHECKFFT(tableSize);
    
    std::fill(begin(), end(), 0.0);
    
    switch (type) {
    case 0:
        scaleFac *= (8. / (PI_F_P * PI_F_P));
        break;
    case 1:
        scaleFac *= (-2. / PI_F_P);
        break;
    case 2:
        scaleFac *= (-4. / PI_F_P);
        break;
    case 10:
        scaleFac *= (-4. / (PI_F_P * PI_F_P));
        break;
    case 11:
        break;
    default:
        break;
    }
    /* calculate FFT of the requested waveform */
    for (unsigned int i = minh; i <= (tableSize >> 1u); i++) {

        switch (type) {
        case 0:                                   /* triangle */
            if (i <= nPart)
                data()[(i << 1u) + 1] = (i & 1u ? ((i & 2u ? scaleFac : (-scaleFac))
                    / ((MYFLOAT)i * (MYFLOAT)i))
                    : 0.0);
            break;
        case 1:                                   /* sawtooth */
            if (i <= nPart)
                data()[(i << 1u) + 1] = scaleFac / (MYFLOAT)i;
            break;
        case 2:                                   /* square */
            if (i <= nPart)
                data()[(i << 1u) + 1] = (i & 1u ? (scaleFac / (MYFLOAT)i) : 0.0);
            break;
        case 10:                                   /* 4 * x * (1 - x) */
            if (i <= nPart)
                data()[i << 1u] = scaleFac / ((MYFLOAT)i * (MYFLOAT)i);
            break;
        case 11:                                   /* pulse */
            if (i <= nPart)
                data()[i << 1u] = scaleFac;
            break;
        default:                                  /* user defined */
            if (i > nPart) {
                data()[(i << 1u) + 1] = data()[(i << 1u)] = 0.;
            }
            break;

        }
    }
    /* inverse FFT */
    data()[1] = data()[tableSize];
	data()[tableSize] = 0.0;
    fft->backward(data()); 
    MYFLOAT peak = *std::max_element(data(), data() + tableSize,
        [](MYFLOAT a, MYFLOAT b) { return std::abs(a) < std::abs(b); });
    peakScale_ = 1.0;
    if (std::abs(peak) > 0.0) {
        peakScale_ = std::abs(peak);
        for (int i = 0; i <= tableSize; i++) data()[i] /= std::abs(peak);
    }
    data()[tableSize] = data()[0];
}

int WaveTable::computeTableSize() {
    int n;
    if (nPart <= 1)
        n = 1;
    else if (nPart <= 4)
        n = 2;
    else if (nPart <= 16)
        n = 4;
    else if (nPart <= 64)
        n = 8;
    else if (nPart <= 256)
        n = 16;
    else if (nPart <= 1024)
        n = 32;
    else
        n = 64;
    /* set table size according to min and max value */
    n *= 256;
    if (n > WINDOW_SIZE) n = WINDOW_SIZE;
    return n;
}

void WaveTable::flenSetup() {
    uint32_t n = tableSize;
	tableMask_32 = tableMask_64 = n - 1;
    lobits_32 = 0UL;
    fractMask_32 = 1UL;
    pfrac_32 = 0.0;
    while (n < OSCBNK_PHSMAX_32) {
        n <<= 1u;
        fractMask_32 <<= 1u;
        lobits_32++;
    }
    pfrac_32 = 1.0 / (MYFLOAT)fractMask_32;
    fractMask_32--;
    
    uint64_t n2 = tableSize;
    lobits_64 = 0UL;
    fractMask_64 = 1UL;
    pfrac_64 = 0.0;
    while (n2 < OSCBNK_PHSMAX_64) {
        n2 <<= 1u;
        fractMask_64 <<= 1u;
        lobits_64++;
    }
    pfrac_64 = 1.0 / (MYFLOAT)fractMask_64;
    fractMask_64--;
}

void WaveTable::debugTicks(int nTicks, float pw) const {
    uint32_t phs = 0;
    uint32_t frq = OSCBNK_PHS2INT_32(3000.0 / 44100.0);
    uint32_t pw_int = OSCBNK_PHS2INT_32(pw);
   
    printf("tick | phs        | n1   | f1       | smpl      | n2   | f2       | v         | smpl-v    | out\n");
    printf("-----|------------|------|----------|-----------|------|----------|-----------|-----------|----------\n");

    for (int i = 0; i < nTicks; i++) {
        uint32_t phs2 = (phs + pw_int) & OSCBNK_PHSMSK_32;

        uint32_t n1 = (phs >> lobits_32) & tableMask_32;
        MYFLOAT f1 = (MYFLOAT)(phs & fractMask_32) * pfrac_32;
        MYFLOAT smpl = data()[n1] + f1 * (data()[n1 + 1] - data()[n1]);

        uint32_t n2 = (phs2 >> lobits_32) & tableMask_32;
        MYFLOAT f2 = (MYFLOAT)(phs2 & fractMask_32) * pfrac_32;
        MYFLOAT v = data()[n2] + f2 * (data()[n2 + 1] - data()[n2]);

        MYFLOAT denom = pw - pw * pw;
        MYFLOAT out = (fabs(denom) < 1e-5f) ? 0.0f : (smpl - v) * (0.25f / denom);

        printf("%4d | %10u | %4u | %8.6f | %9.4f | %4u | %8.6f | %9.4f | %9.4f | %9.4f\n",
            i, phs, n1, f1, smpl, n2, f2, v, smpl - v, out);

        phs = (phs + frq) & OSCBNK_PHSMSK_32;
    }
}

void WaveTable::generateSawtoothWaveform(double dutyCycle) {
    int transitionPoint = static_cast<int>(dutyCycle * tableSize);

    for (int i = 0; i < transitionPoint; ++i) {
        data()[i] = 2.0 * (i / static_cast<double>(transitionPoint)) - 1.0;
    }
    for (int i = transitionPoint; i < size(); ++i) {
        data()[i] =
            2.0 *
            ((i - transitionPoint) / static_cast<double>(tableSize - transitionPoint)) -
            1.0;
    }
}

void WaveTable::generateTriangleWaveform(double dutyCycle) {
    int upSamples = static_cast<int>(dutyCycle * tableSize);
    int downSamples = tableSize - upSamples;

    for (int i = 0; i < upSamples; ++i) {
        data()[i] = 2.0 * (i / static_cast<double>(upSamples)) - 1.0;
    }
    for (int i = 0; i < downSamples; ++i) {
        data()[upSamples + i] = 1.0 - 2.0 * (i / static_cast<double>(downSamples));
    }
}

// Generate a pulse waveform for a given pulse width
void WaveTable::generatePulseWaveform(double pulseWidth) {
    int transitionPoint = static_cast<int>(pulseWidth * tableSize);
    for (int i = 0; i < tableSize; ++i) {
        data()[i] = (i < transitionPoint) ? 1.0 : -1.0;
    }
}

WaveTable& WaveTables::getTable(MYFLOAT sampleRate, MYFLOAT frequency) {
    /* find best table for current frequency */
    MYFLOAT npart = fabs(frequency / sampleRate) * 1.05;
    if (npart < p_min) npart = p_min;
    return  *npartsTable[(int32_t)(p_scl / npart)];
}
WaveTable& WaveTables::getGrainTable(MYFLOAT grainSize, MYFLOAT cycles, MYFLOAT sampleRate, uint64_t& size, uint64_t& lobits, MYFLOAT& pfrac) {
    MYFLOAT env_freq = cycles * (sampleRate / grainSize);
    auto& ftable1 = getTable(sampleRate, env_freq);
    size = ftable1.tableSize;
    lobits = ftable1.lobits_64;
    pfrac = ftable1.pfrac_64;
    return ftable1;
}
static WaveTables osc[3];
// PWM is produced by differencing a LINEAR RAMP table, never the target shape
// itself: PULSE differences the saw (osc[1], via tickpwm64) and TRI differences
// 4x(1-x) (oscpar, via tickpw64, whose .25/(pw-pw*pw) is exactly that table's
// normalizer). osc[0]/osc[2] stay the plain tri/square spectra for PVAmps.
static WaveTables oscpar;
static bool setupDone{};
// PVAmps resynthesises its SINE mode from this instead of an inverse FFT, so all
// four waveforms share one code path. No mipmap needed: a sine has no partials
// to alias. Shared, because at MYFLOAT = double the table is 128 KB.
static WaveTable pvampsSine;
static bool pvampsSineDone{};

GrainVCO::GrainVCO(TRACK* track, int
    chan) : Effect(track, chan, SPACE_GRAINVCO, GRAINEFFECT),
    grainFilter(track, _chan) {
    _bypass = &track->bypass[SPACE_GRAINVCO];
    _STATE->params[_track->index][GRAINFILTERRECOMPUTE1 + chan].store(1.0);
    std::lock_guard<std::mutex> lk(guard1);

    if (!setupDone) {
        setupDone = true;
        osc[0].setup(0);
        osc[1].setup(1);
        osc[2].setup(2);
        oscpar.setup(10);   // 4x(1-x), the ramp TRI's PWM is differenced from
        uint64_t phs = 0;
        uint64_t frq = OSCBNK_PHS2INT_64(3000.0f / 48000.0f);
       
    }
    sinewave.setWavetable(getsinewave(), WINDOW_SIZE);
}

void GrainVCO::prepare(const MYFLOAT* in, int32_t size) {
    
    
    if (*_bypass || destroyRequested) {
        _dry = 1.;
        _wet = 0.;
    }
    else {
        _dry = LOG2NORMALF(_STATE->params[_track->index][GRAINVCODRY].load());
        _wet = LOG2NORMALF(_STATE->params[_track->index][GRAINVCOWET].load());
    }

    MYFLOAT _freq;
    MYFLOAT gg = 1.;
    if (_STATE->params[_track->index][GRAINVCOFOLLOW] == 1) {
        _freq = _DATA->prevcps[_track->index * _chan];
        if (in != nullptr) {
            MYFLOAT maxx = 0;
            for (int32_t i = 0; i < size; i++)
                maxx = maxx > in[i] ? maxx : in[i];
            gg = maxx;
        }
        if (_STATE->params[_track->index][GRAINVCOHOLD] != 1.0) {
            _DATA->prevcps[_track->index * _chan] = _freq = _STATE->params[_track->index][
                PITCHDETECTGRAINFXTRACKOUT0 + _chan];
        }
    }
    else _freq = LOG2NORMALF(_STATE->params[_track->index][GRAINVCOCPS].load());

    _freq *= _track->pitchfact[_chan];

    _freq *= (_chan == 0 ? pow(2, -_STATE->params[_track->index][GRAINVCODETLR].load() / 2400.)
        : pow(2, _STATE->params[_track->index][GRAINVCODETLR].load() / 2400.));

    if (_freq > 8000.)
        _freq = 8000.;
    else if (_freq < 18.)
        _freq = 18.;


    auto offset = static_cast<uint64_t>(_DATA->offset + _track->step_point_grain[_chan]);

    const bool phasereset = _STATE->params[_track->index][GRAINVCOPHRESET] == 1.0;
    tmpvcos.clear();
    for (int32_t i = 0; i < 3; i++) {
        if (_STATE->params[_track->index][GRAINVCO1POW + i] == 1) {
            const auto det = pow(2, _STATE->params[_track->index][GRAINVCO1DET + i].load() /
                1200.);
            const auto cpsvco = _freq * det;
            auto gliss = cpsvco * _track->glissfact[_chan];
            if (gliss > 8000.)
                gliss = 8000.;
            else if (gliss < 18.)
                gliss = 18.;
            //  gliss *= _STATE->onedsr;
            auto glissinc = (gliss - cpsvco) / (MYFLOAT)size;

            auto vcophase = phasereset ? 0. : fmod(cpsvco * offset * _STATE->onedsr, 1.0);;
            MYFLOAT pw;
            auto lfo = _track->lfo[GRAINVCO1PW + i].load();
            if (lfo && lfo->power()) {
                MYFLOAT a = lfo->min(GRAINVCO1PW + i);
                MYFLOAT b = lfo->max(GRAINVCO1PW + i);
                MYFLOAT range = b - a;
                MYFLOAT start = a;
                pw = 0.5 - .45 * (start +
                    lfo->buf[(int)_track->step_point_grain[_chan].load()] *
                    range);
            }
            else {
                pw = 0.5 - .45 * _STATE->params[_track->index][GRAINVCO1PW + i].load();
            }
            auto type = static_cast<int>(_STATE->params[_track->index][GRAINVCO0WAVEFORM +
                i].load());
            // UI: -1 SINE, 0 TRI, 1 SAW, 2 PULSE. TRI and PULSE are synthesized by
            // differencing a ramp table (see oscpar above), so the table they read
            // is not the one their index names.
            tmpVCO::Read read = tmpVCO::PLAIN;
            if (type == 0) read = tmpVCO::TRI;
            else if (type == 2) read = tmpVCO::PULSE;
            auto& wt = type == -1 ? sinewave
                : (read == tmpVCO::TRI   ? oscpar.getTable(_STATE->sr, cpsvco)
                :  read == tmpVCO::PULSE ? osc[1].getTable(_STATE->sr, cpsvco)
                :                          osc[type].getTable(_STATE->sr, cpsvco));

            /*
            osc[i].setWaveForm(static_cast<int>(_STATE->params[_track->index][GRAINVCO0WAVEFORM +
                                                           i].load()));

            osc[i].setPhase(vcophase);
            osc[i].setFrequency(cpsvco);
            */
            //            MYFLOAT gain{}, pw{}, cps{}, inc{}, oldpw{}, phase{};

            tmpvcos.emplace_back(tmpVCO{ &wt, LOG2NORMALF(_STATE->params[_track->index][
                                                                 GRAINVCO1AMP + i].load()) * gg,
                                        pw, OSCBNK_PHS2INT_64(vcophase),
                                        OSCBNK_PHS2INT_64(cpsvco * _STATE->onedsr),
                                        OSCBNK_PHS2INT_64(glissinc * _STATE->onedsr),
                                        read });
        }
    }
    dovcf = _STATE->params[_track->index][GRAINVCOVCF].load() == 1.;
    if (dovcf)
        grainFilter.start();
}

void GrainVCO::compute(MYFLOAT* in, int32_t size) {
    prepare(in, size);
    for (int32_t i = 0; i < size; i++) {

        MYFLOAT smpl = 0;
        for (auto& vco1 : tmpvcos) {
            //smpl += vco1.vco->tick(vco1.cps + i * vco1.inc, vco1.gain, vco1.pw);
            // vco1.vco->setFrequency(vco1.cps + i * vco1.inc);
//            smpl += vco1.vco->tick(vco1.cps + i * vco1.inc, vco1.pw) * vco1.gain;
            const uint64_t f = vco1.frq + i * vco1.inc;
            MYFLOAT s;
            if (vco1.read == tmpVCO::PULSE)
                s = vco1.vco->tickpwm64(vco1.phase, f, vco1.pw);
            else if (vco1.read == tmpVCO::TRI)
                // 1-pw keeps the fract(-pw) skew convention shared with PA; the
                // .25/(pw-pw*pw) denominator is unchanged by it, since
                // (1-pw)-(1-pw)^2 == pw-pw^2.
                s = vco1.vco->tickpwtri64(vco1.phase, f, 1.0 - vco1.pw);
            else
                s = vco1.vco->tickpw64(vco1.phase, f, vco1.pw);
            smpl += s * vco1.gain;
        }
        if (dovcf)
            smpl = grainFilter.tick(smpl, i, size);
        in[i] = (in[i] * _smooth2 + smpl * _smooth1);
        smwetdry(_wet, _dry);
    }
}

MYFLOAT GrainVCO::tick(MYFLOAT in, int32_t offset, int size) {
    MYFLOAT smpl = 0;
    for (auto& vco1 : tmpvcos) {
        ;//smpl += vco1.vco->tick(vco1.gain, vco1.pw);
    }
    if (dovcf)
        smpl = grainFilter.tick(smpl, offset, size);
    smpl = (in * _smooth2 + smpl * _smooth1);
    smwetdry(_wet, _dry);
    return smpl;
}

void GrainVco::prepare(TRACK* t, int32_t chan) {
    if (t->bypass[SPACE_GRAINVCO]) {
        _dry = 1.f;
        _wet = 0.f;
    }
    else {
        _dry = LOG2NORMALF(t->_STATE->params[t->index][GRAINVCODRY].load());
        _wet = LOG2NORMALF(t->_STATE->params[t->index][GRAINVCOWET].load());
    }

    MYFLOAT _freq;
    MYFLOAT gg = 1.;
    if (t->_STATE->params[t->index][GRAINVCOFOLLOW] == 1) {
        _freq = t->_DATA->prevcps[t->index * chan];
        if (t->_STATE->params[t->index][GRAINVCOHOLD] != 1.0) {
            t->_DATA->prevcps[t->index * chan] = _freq = t->_STATE->params[t->index][
                PITCHDETECTGRAINFXTRACKOUT0 + chan];
        }
    }
    else _freq = LOG2NORMALF(t->_STATE->params[t->index][GRAINVCOCPS]);

    _freq *= t->pitchfact[chan];

    _freq *= (chan == 0 ? pow(2, -t->_STATE->params[t->index][GRAINVCODETLR].load() / 2400.)
        : pow(
            2, t->_STATE->params[t->index][GRAINVCODETLR].load() / 2400.));

    if (_freq > 8000.)
        _freq = 8000.;
    else if (_freq < 18.)
        _freq = 18.;

    uint64_t offset = t->_DATA->offset + t->step_point_grain[chan];

    const bool phasereset = t->_STATE->params[t->index][GRAINVCOPHRESET] == 1.0f;
    tmpvcos.clear();
    for (int32_t i = 0; i < 3; i++) {
        if (t->_STATE->params[t->index][GRAINVCO1POW + i] == 1) {
            MYFLOAT cpsvco =
                _freq *
                pow(2, t->_STATE->params[t->index][GRAINVCO1DET + i].load() / 1200.) *
                t->_STATE->onedsr;
            uint64_t vcophase = phasereset ? 0 : (offset * OSCBNK_PHS2INT_64(cpsvco)) &
                OSCBNK_PHSMSK_64;
            MYFLOAT pw;
            auto mode = (int)t->_STATE->params[t->index][GRAINVCO1TYPE + i].load();
            if (mode == 2 || mode == 4) {
                auto lfo = t->lfo[GRAINVCO1PW + i].load();
                if (lfo && lfo->power()) {
                    MYFLOAT a = lfo->min(GRAINVCO1PW + i);
                    MYFLOAT b = lfo->max(GRAINVCO1PW + i);
                    MYFLOAT range = DISTANCEF(a, b);
                    MYFLOAT start = std::min(a, b);
                    pw = start + lfo->buf[(int)t->step_point_grain[chan].load()] * range;
                }
                else {
                    pw = t->_STATE->params[t->index][GRAINVCO1PW + i].load();
                }

            }
            else pw = .5;
            // vco[i].check(mode);
            //vco[i].setPhase(vcophase);
            tmpvcos.emplace_back(tmpVCO{ &vco[i], LOG2NORMALF(
                                                         t->_STATE->params[t->index][
                                                                 GRAINVCO1AMP +
                                                                 i].load()) *
                                                 gg, pw, cpsvco, 0 });
        }
    }
    dovcf = t->_STATE->params[t->index][GRAINVCOVCF].load() == 1.;
    if (dovcf)
        grainFilter.prepare(t, chan);
}

MYFLOAT GrainVco::tick(MYFLOAT in, int32_t offset, int size) {
    MYFLOAT smpl = 0;
    for (auto& vco1 : tmpvcos) {
        ;//smpl += vco1.vco->tick(vco1.gain, vco1.pw);;
    }
    if (dovcf)
        smpl = grainFilter.tick(smpl, offset, size);
    smpl = (in * _dry + smpl * _wet);
    return smpl;
}

PVAmps::PVAmps(TRACK* track, int32_t
    chan) : Effect(track, chan, SPACE_PVAMPS, MONOEFFECT),
    fft(_STATE->sr, PVAmpsFFTSize,
        PVAmpsFFTSize / PVAmpsOlap),
    CircularBuffer(PVAmpsFFTSize,
        PVAmpsFFTSize / PVAmpsOlap) {
    range = &_STATE->params[track->index][PVAMPSRANGE];
    bounda = &_STATE->params[track->index][PVAMPSBOUNDA];
    boundb = &_STATE->params[track->index][PVAMPSBOUNDB];
    phasemode = &_STATE->params[track->index][PVAMPSPHASE];
    routing = &_STATE->params[track->index][PVAMPSROUTE];

    _dry = &_STATE->params[track->index][PVAMPSDRY];
    _wet = &_STATE->params[track->index][PVAMPSWET];
    _bypass = &track->bypass[SPACE_PVAMPS];
    mode = &_STATE->params[track->index][PVAMPSVCOWAVEFORM2];
    compute_hanning(win, PVAmpsFFTSize);
    sm = &_STATE->params[track->index][PVAMPSSMOOTH2];
    // Give the smoothers usable coefficients here rather than on the first frame
    // that sees SMOOTH change: at c1 = c2 = 0 a Tone returns 0 for every input,
    // and SetState() divides by c2.
    prevsm = sm->load();
    for (auto& f : tone) {
        f.init(1.);
        f.Setup(SMOOTH2POLE(prevsm));
    }
    delay.setsize(getDelay());

    // Every waveform, SINE included, is resynthesised through the oscillator
    // bank, so one gain law serves all four: a partial of amplitude A analysed
    // through win * fft.awin peaks at A/2 * sum(win * awin), and overlap-adding
    // the Hann synthesis window at this hop sums to sum(win) / hop. SINE used to
    // go out through fft.backward instead and landed ~12 dB below the others.
    MYFLOAT suma = 0., sumw = 0.;
    for (int32_t i = 0; i < PVAmpsFFTSize; i++) {
        suma += win[i] * fft.awin[i];
        sumw += win[i];
    }
    oscgain = (2. / suma) * ((MYFLOAT)PVAmpsOlap / sumw);

    std::lock_guard<std::mutex> lk(guard1);
    if (!setupDone) {
        setupDone = true;
        osc[0].setup(0);
        osc[1].setup(1);
        osc[2].setup(2);
        oscpar.setup(10);
    }
    if (!pvampsSineDone) {
        pvampsSineDone = true;
        pvampsSine.setWavetable(getsinewave(), WINDOW_SIZE);
    }
};


void PVAmps::onBufferReady(MYFLOAT* buf, int32_t size) {
    int32_t nslots = (int32_t)range->load();
    if (nslots < 1) nslots = 1;
    if (nslots > PVAmpsChannels) nslots = PVAmpsChannels;

    const MYFLOAT smooth = sm->load();
    if (smooth != prevsm) {
        prevsm = smooth;
        for (auto& filt : tone)
            filt.Setup(SMOOTH2POLE(smooth));
    }

    // A Hann analysis window on top of fft.awin, which alpha = 1 leaves all but
    // rectangular: peak picking and the parabolic interpolation below both need
    // -31 dB sidelobes rather than -13 dB to tell a partial from a skirt.
    for (int32_t i = 0; i < size; i++)
        buf[i] *= win[i];
    fft.forwardPolar(buf, buf);

    // buf holds |DC|, |Nyquist|, then (magnitude, phase) per bin. Bin 0 is never
    // picked as a partial -- kmin keeps it out -- but it has to keep its real
    // magnitude, because bin 1 is compared against it below. Zeroing it made
    // bin 1 a local maximum on every hop with any low-frequency energy, and its
    // interpolated amplitude then blew up against the zero (see below). buf[1]
    // is the Nyquist magnitude, not bin 0's phase, so it does not survive here.
    bins[0].r = buf[0];
    bins[0].i = 0.;
    for (int32_t i = 1; i < PVAmpsFFTSizeD2; i++) {
        bins[i].r = buf[i * 2];
        bins[i].i = buf[i * 2 + 1];
    }

    // CUT A/CUT B, in bins: size * f/sr IS the bin number. The old code fed that
    // straight into a loop stepping the interleaved (magnitude, phase) array, so
    // both cutoffs landed an octave low and CUT B at its default of sr/2 silently
    // discarded everything above sr/4.
    MYFLOAT ba = pow(10, bounda->load() * .05) / _STATE->sr;
    MYFLOAT bb = pow(10, boundb->load() * .05) / _STATE->sr;

    if (ba > bb) {
        auto tmp = ba;
        ba = bb;
        bb = tmp;
    }

    int32_t kmin = (int32_t)ceil(size * ba);
    int32_t kmax = (int32_t)(size * bb);
    if (kmin < 1) kmin = 1;
    if (kmax > PVAmpsFFTSizeD2 - 2)     // k + 1 has to stay inside bins[]
        kmax = PVAmpsFFTSizeD2 - 2;

    const bool classic = routing->load() < .5;

    // TRACKED collects local maxima, not merely loud bins: one strong partial
    // leaks into its neighbours, so ranking bins by magnitude alone spends slots
    // on the skirts of a peak another slot is already playing.
    //
    // CLASSIC keeps every bin in the band, which is how the effect always chose,
    // and that difference is the whole character. Measured on a 220 Hz tone with
    // eight harmonics and CHANNELS at 20: local maxima find exactly 8 peaks, so
    // 8 oscillators sound and 12 sit silent, while taking the loudest 20 bins
    // covers those same 8 partials with 20 oscillators -- the extra 12 land on
    // adjacent bins of a partial already sounding and beat against it. That
    // thickness is what the routing switch alone could not bring back, because
    // with a stable peak set both routings hand the same partials to the same
    // slots and sound identical.
    int32_t ncand = 0;
    for (int32_t k = kmin; k <= kmax; k++) {
        const MYFLOAT m = bins[k].r;
        if (m <= 0.)
            continue;
        if (!classic && !(m > bins[k - 1].r && m >= bins[k + 1].r))
            continue;
        amps[ncand] = m;
        indices[ncand++] = (short)k;
    }

    // The loudest nslots of them (nslots <= 6, so a scan beats a sort)
    MYFLOAT pfreq[PVAmpsChannels], pamp[PVAmpsChannels], pphase[PVAmpsChannels];
    int32_t npeak = 0;
    while (npeak < nslots) {
        int32_t best = -1;
        for (int32_t c = 0; c < ncand; c++)
            if (amps[c] >= 0. && (best < 0 || amps[c] > amps[best]))
                best = c;
        if (best < 0)
            break;
        const int32_t k = indices[best];
        amps[best] = -1.;                   // taken
        // Parabolic interpolation over log magnitude: sub-bin frequency, without
        // which every partial is quantised to the bin grid -- 46.9 Hz steps at
        // this size and rate -- which is what made the resynthesis step rather
        // than glide. The neighbours are floored at -60 dB of the peak first, so
        // a near-empty bin cannot drag the vertex around. CLASSIC stays on the
        // grid: at d = 0 every line below collapses to the raw bin values the
        // original used, so the two share one extraction.
        MYFLOAT d = 0.;
        if (!classic) {
            const MYFLOAT flr = bins[k].r * 1e-3;
            const MYFLOAT la = log(bins[k - 1].r > flr ? bins[k - 1].r : flr);
            const MYFLOAT lb = log(bins[k].r);
            const MYFLOAT lc = log(bins[k + 1].r > flr ? bins[k + 1].r : flr);
            const MYFLOAT den = la - 2. * lb + lc;
            d = den < 0. ? .5 * (la - lc) / den : 0.;
            if (d > .5) d = .5;
            else if (d < -.5) d = -.5;
        }
        pfreq[npeak] = ((MYFLOAT)k + d) / (MYFLOAT)PVAmpsFFTSize;
        // Amplitude from the main-lobe shape, not from the parabola: a partial d
        // off bin centre reads mag * sinc(d)/(1-d^2), so the true peak is that
        // much higher, and the correction is bounded to 1.42 dB. The textbook
        // log-parabola amplitude is unbounded against a near-empty neighbour --
        // it made bin 1 500x too loud, the +50 dB the effect used to add.
        MYFLOAT wlobe = 1.;
        if (d != 0.) {
            const MYFLOAT pd = PI_P * d;
            wlobe = (sin(pd) / pd) / (1. - d * d);
        }
        pamp[npeak] = bins[k].r / wlobe;
        // The analysis window is symmetric about (M-1)/2, not about 0, so a bin
        // the partial sits d away from carries a linear-phase term of pi*d --
        // half a cycle at the worst case. Left in, a partial landing between two
        // bins alternates which of them wins from hop to hop, the two phases
        // differ by pi, and the overlap-add half cancels it: 3 dB down and
        // amplitude modulated. In cycles rather than radians here, so d/2.
        pphase[npeak] = bins[k].i * (MYFLOAT)(1. / TWOPI_P) - .5 * d;
        npeak++;
    }

    int32_t owner[PVAmpsChannels];
    bool claimed[PVAmpsChannels]{};
    for (int32_t s = 0; s < PVAmpsChannels; s++)
        owner[s] = -1;

    if (classic) {
        // The routing the effect always had: peaks in ascending frequency order,
        // peak i to slot i, no identity from one hop to the next. A slot is
        // whatever the spectrum hands it, and because nothing is ever reseeded
        // the frequency smoother slurs between unrelated partials -- that slur is
        // the smear the effect is built on. Peaks arrive loudest-first from the
        // selection above, so sort them back into frequency order (<= 20 of them).
        for (int32_t i = 1; i < npeak; i++) {
            const MYFLOAT f = pfreq[i], a = pamp[i], ph = pphase[i];
            int32_t j = i - 1;
            while (j >= 0 && pfreq[j] > f) {
                pfreq[j + 1] = pfreq[j];
                pamp[j + 1] = pamp[j];
                pphase[j + 1] = pphase[j];
                j--;
            }
            pfreq[j + 1] = f;
            pamp[j + 1] = a;
            pphase[j + 1] = ph;
        }
        for (int32_t s = 0; s < npeak; s++)
            owner[s] = s;
    }
    else {
        // Hand each peak to the slot that was already following it, so a slot
        // keeps its partial and SMOOTH glides along that partial instead of
        // across two unrelated ones every time the set of peaks changes. Peaks
        // come loudest first and claim in that order: when the spectrum is
        // denser than the bank, the partials that matter keep their oscillator.
        for (int32_t p = 0; p < npeak; p++) {
            int32_t best = -1;
            MYFLOAT bestdist = PVAmpsTrackTol;
            for (int32_t s = 0; s < nslots; s++) {
                if (owner[s] >= 0 || slotfreq[s] <= 0.)
                    continue;
                const MYFLOAT dist = fabs(pfreq[p] - slotfreq[s]) * (MYFLOAT)PVAmpsFFTSize;
                if (dist < bestdist) {
                    bestdist = dist;
                    best = s;
                }
            }
            if (best >= 0) {
                owner[best] = p;
                claimed[p] = true;
            }
        }
        // Whatever is left is a partial nobody was tracking. A silent slot takes
        // it outright and starts its smoother there, having nothing to glide
        // from. If every slot is still sounding -- which at long SMOOTH settings
        // is most of the time -- the quietest one is taken over and left to
        // GLIDE in, keeping its phase, so the smear still happens, but when the
        // bank is full rather than on every hop by accident.
        for (int32_t p = 0; p < npeak; p++) {
            if (claimed[p])
                continue;
            int32_t idle = -1, quietest = -1;
            for (int32_t s = 0; s < nslots; s++) {
                if (owner[s] >= 0)
                    continue;
                if (slotamp[s] <= 0.) {
                    idle = s;
                    break;
                }
                if (quietest < 0 || slotamp[s] < slotamp[quietest])
                    quietest = s;
            }
            const int32_t s = idle >= 0 ? idle : quietest;
            if (s < 0)
                break;                      // every slot is spoken for
            owner[s] = p;
            claimed[p] = true;
            if (idle >= 0) {
                tone[s].SetState(pfreq[p]);
                slotphase[s] = 0.;
            }
        }
    }

    int32_t _mode = static_cast<int>(mode->load());
    if (_mode > 2) _mode = 2;
    const auto phsmode = static_cast<int>(phasemode->load());

    memset(buf, 0, sizeof(MYFLOAT) * size);

    // A slot that lost its partial holds the pitch and decays at the pole SMOOTH
    // already computed for the frequency smoother -- one Tone tick is one hop, so
    // c2 is directly the per-hop release. That puts the whole range under the one
    // knob: ~20 ms at SMOOTH max (partials stop with the peak, the tracking
    // behaviour), ~370 ms at the default, ~12 s at SMOOTH min, where the bank
    // fills with sustained tones and new partials have to steal a slot to get in.
    const MYFLOAT release = tone[0].c2;

    for (int32_t s = 0; s < nslots; s++) {
        const int32_t p = owner[s];
        MYFLOAT target;
        if (p >= 0) {
            target = pfreq[p];
            slotamp[s] = pamp[p];
        }
        else if (slotfreq[s] <= 0.) {
            slotamp[s] = 0.;                // never had a partial yet
            continue;
        }
        else if (classic) {
            // The original held its pitch by leaving a stale bin in indices[]
            // and then reading that bin's CURRENT magnitude, so a held partial
            // faded out on its own as the spectrum moved away from it. Keep
            // that: it is a more musical decay than a fixed release, and it is
            // what the old sound actually was.
            int32_t k = (int32_t)(slotfreq[s] * (MYFLOAT)PVAmpsFFTSize + .5);
            if (k < 1) k = 1;
            else if (k > PVAmpsFFTSizeD2 - 1) k = PVAmpsFFTSizeD2 - 1;
            slotamp[s] = bins[k].r;
            target = slotfreq[s];
        }
        else {
            if (slotamp[s] <= 0.) {
                slotfreq[s] = 0.;           // silent: free for the next partial
                continue;
            }
            slotamp[s] *= release;
            if (slotamp[s] < 1e-6) {
                slotamp[s] = 0.;
                slotfreq[s] = 0.;
                continue;
            }
            target = slotfreq[s];           // hold the pitch it had
        }

        MYFLOAT freq = tone[s].tickn(target);
        if (freq < 0.) freq = 0.;
        else if (freq > .49) freq = .49;

        MYFLOAT phase;
        if (phsmode == 0) {
            // Analysis phase. std::abs() used to fold the negative half of every
            // phase onto the positive one, which is not phase coherence. A slot
            // in its release has no new analysis phase, and restarting it from 0
            // would click, so it carries on from its own running phase.
            phase = p >= 0 ? pphase[p] - floor(pphase[p]) : slotphase[s];
        }
        else if (phsmode == 1)
            phase = slotphase[s];           // free-running, continuous per partial
        else if (phsmode == 2)
            phase = tsl::random::randomfloat(0., 1.);
        else
            phase = 0.;

        // No PW here — resynthesis wants the plain waveform. tickpw64's
        // default pw of .5 was differencing TRI and PULSE against themselves
        // half a cycle away, doubling them, while SAW took the early return
        // and stayed at 1x: a 6 dB step that appeared only on 2 of 3 shapes.
        const WaveTable& wt = _mode < 0 ? pvampsSine : osc[_mode].getTable(1., freq);
        auto phs = static_cast<uint64_t>(OSCBNK_PHS2INT_64(phase));
        const auto frq = OSCBNK_PHS2INT_64(freq);
        const MYFLOAT g = slotamp[s] * oscgain;
        for (int32_t i = 0; i < size; i++)
            buf[i] += wt.tick64(phs, frq) * win[i] * g;

        // Track on the detected frequency, not the smoothed one: at long SMOOTH
        // settings the oscillator lags the partial by tens of hops, and matching
        // against that lagged value would put the partial outside its own slot's
        // tolerance and hand it to a different oscillator.
        slotfreq[s] = target;
        slotphase[s] += freq * (MYFLOAT)PVAmpsOlap;     // advance by one hop
        slotphase[s] -= floor(slotphase[s]);
    }
}

void PVAmps::compute(MYFLOAT* in, int32_t size) {
    //_compute(in, helpbuf.data(), size);
    MYFLOAT wet, dry;
    if (_bypass->load() || destroyRequested) {
        dry = 1.;
        wet = 0.;
    }
    else {
        dry = LOG2NORMALF(*_dry);
        wet = LOG2NORMALF(*_wet);
    }
    for (int32_t i = 0; i < size; i++) {
        in[i] = _smooth2 * delay.process(in[i]) + _smooth1 * _tick(in[i]);
        smwetdry(wet, dry);
    }
};