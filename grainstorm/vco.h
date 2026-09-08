#pragma once
//
// Created by pr on 04.06.19.
//

#ifndef GRAINSTORM_VCO_H
#define GRAINSTORM_VCO_H

#include <cstdint>
#include <atomic>
#include <cmath>
#include <vector>
#include <tools/CircularBuffer.h>
#include "defines.h"
#include "base.h"
#include "Biquad.h"
#include "DelayBase.h"
#include "ffttools.h"
class WaveTable : public tsl::AlignedVector<MYFLOAT> {
public:
    WaveTable() = default;
    WaveTable(int s) {
        type_ = -1;
        resize(s + 2, 0);
        tableSize = s;
        flenSetup();
    }
    void setWavetable(tsl::AlignedVector<MYFLOAT>& in, int s) {
        setWavetable(in.data(), s);
    }
    void setWavetable(const MYFLOAT* in, int s) {
        type_ = -1;
        resize(s + 2, 0);
        tableSize = s;
        flenSetup();
        for (int i = 0; i < s; i++) data()[i] = in[i];
        data()[s] = data()[0];
    }
    void computeWavetable(int type, int nPartials);
    void computeWavetable(int nPartials, tsl::AlignedVector<MYFLOAT>& in, uint32_t size);
    int32_t nPart{};              /* number of harmonic partials (may be zero) */
    uint32_t               /* parameters needed for reading the table,  */
        lobits_32{}, fractMask_32{}, tableMask_32, tableSize{};       /*   and interpolation                       */
    MYFLOAT pfrac_32{};
    uint64_t               /* parameters needed for reading the table,  */
        lobits_64{}, fractMask_64{}, tableMask_64;       /*   and interpolation                       */
    MYFLOAT pfrac_64{};

    
    MYFLOAT tickpw(MYFLOAT& phase, MYFLOAT frq, const MYFLOAT pw = .5) const;
    MYFLOAT tickpw64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw = .5)const;
    MYFLOAT tickpwm64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw = .5)const;
    MYFLOAT tickpwtri64(uint64_t& phs, const uint64_t frq, const MYFLOAT pw = .5)const;
    MYFLOAT tickpw(uint32_t& phase, uint32_t frq, const MYFLOAT pw = .5) const;
    MYFLOAT tick2(uint32_t& phase, MYFLOAT& oldpw, const uint32_t frq, const MYFLOAT pw = .5);
    MYFLOAT tick(uint32_t& phs, const uint32_t frq)const;
    MYFLOAT tick64(uint64_t& phs, const uint64_t frq)const;
    void debugTicks(int nTicks, float pw) const;
private:
    int type_{};
    // calculateTable() peak-normalizes, so the stored ramp is the ideal one divided
    // by its Gibbs peak (saw 1.178, 4x(1-x) 0.666). The PWM constructions below are
    // only valid on an un-normalized ramp, so they scale their reads back up by this
    // before differencing. Left at 1.0 for tables that were never normalized.
    MYFLOAT peakScale_{ 1.0 };

    void calculateTable(int type);

    int computeTableSize();

    void flenSetup();

    void generateSawtoothWaveform(double dutyCycle);
    void generateTriangleWaveform(double dutyCycle);
    void generatePulseWaveform(double pulseWidth);

};

class WaveTables : private std::vector<WaveTable> {
public:
    static constexpr int harmonicStep = 1;
    static constexpr int maxHarmonics = 4096;
    static constexpr int nTables = (maxHarmonics / harmonicStep) + 1;
    static constexpr MYFLOAT p_scl = 0.5;
    static constexpr MYFLOAT p_min = p_scl / (MYFLOAT)maxHarmonics;


    void setup(int type) {
        MYFLOAT npart_f = 0.0;
        int32_t ntables = 0;
        int32_t i = maxHarmonics;

        do {
            ntables++;
            double n = npart_f * 1.05;
            if ((n - npart_f) < 1.0)
                npart_f++;
            else
                npart_f = n;
        } while (npart_f <= (double)i);
        /* allocate memory for the table array ... */

        /* ... and all tables */
        npartsTable.resize(maxHarmonics + 1);
        resize(ntables);
        /* generate tables */
        npart_f = 0.0;
        i = 0;
        do {
            /* store number of partials, */
            int32_t npart = (int32_t)(npart_f + 0.5);
            data()[i].computeWavetable(type, npart);
            double n = npart_f * 1.05;
            if ((n - npart_f) < 1.0)
                npart_f++;
            else
                npart_f = n;
        } while (++i < ntables);


        int32_t npart = 0;
        i = 0;
        do {
            npartsTable[npart++] = &data()[i];
            if (i < (ntables - 1) && npart >= data()[i + 1].nPart) i++;
        } while (npart <= maxHarmonics);
    }

    void setup(tsl::AlignedVector<MYFLOAT>& in, int oSize) {
        MYFLOAT npart_f = 0.0;
        int32_t ntables = 0;
        int32_t i = maxHarmonics;

        do {
            ntables++;
            double n = npart_f * 1.05;
            if ((n - npart_f) < 1.0)
                npart_f++;
            else
                npart_f = n;
        } while (npart_f <= (double)i);
        /* allocate memory for the table array ... */

        /* ... and all tables */
        npartsTable.resize(maxHarmonics + 1);
        resize(ntables);
        /* generate tables */
        npart_f = 0.0;
        i = 0;
        do {
            /* store number of partials, */
            int32_t npart = (int32_t)(npart_f + 0.5);
            data()[i].computeWavetable(npart, in, oSize);
            double n = npart_f * 1.05;
            if ((n - npart_f) < 1.0)
                npart_f++;
            else
                npart_f = n;
        } while (++i < ntables);


        int32_t npart = 0;
        i = 0;
        do {
            npartsTable[npart++] = &data()[i];
            if (i < (ntables - 1) && npart >= data()[i + 1].nPart) i++;
        } while (npart <= maxHarmonics);
    }

    WaveTable& getTable(MYFLOAT sampleRate, MYFLOAT frequency);
    WaveTable& getGrainTable(MYFLOAT grainSize, MYFLOAT cycles, MYFLOAT sampleRate, uint64_t& size, uint64_t& lobits, MYFLOAT& pfrac);
private:
    std::vector<WaveTable*> npartsTable;
};

#include "moogladder.h"
#include "oscil.h"
class GrainVCO : public Effect {
public:
    GrainVCO(const GrainVCO& o) : GrainVCO(o._track, o._chan) {
    }
    GrainVCO(TRACK* track_, int32_t channel);
    void prepare(const MYFLOAT* in = nullptr, int32_t size = 0);
    MYFLOAT tick(MYFLOAT in, int32_t offset, int size);
    void compute(MYFLOAT* in, int32_t size);
private:
    struct tmpVCO {
        WaveTable* vco{};
        MYFLOAT gain{}, pw{};
        uint64_t phase{}, frq{}, inc{};
        // Which read the table needs, decoupled from the UI waveform now that
        // PULSE and TRI read ramp tables rather than their own spectra.
        enum Read { PLAIN, PULSE, TRI } read{ PLAIN };
    };
    std::vector<tmpVCO> tmpvcos;
    MYFLOAT _wet{}, _dry{};
    GrainFilter grainFilter;
    bool dovcf{};
    WaveTable sinewave;
};

class GrainVco : public GrainEffect {
public:
    void prepare(TRACK* track, int32_t chan);
    MYFLOAT tick(MYFLOAT in, int32_t offset, int size) override;
private:
    struct tmpVCO {
        WaveTables* vco{};
        MYFLOAT gain{}, pw{}, cps{}, inc{};
    };
    std::vector<tmpVCO> tmpvcos;
    MYFLOAT _wet{}, _dry{};
    GrainFilt grainFilter;
    WaveTables vco[3];
    bool dovcf{};
};


#define PVAmpsOlap 256          /* hop size, not an overlap factor */
#define PVAmpsFFTSize 1024
#define PVAmpsFFTSizeD2 (PVAmpsFFTSize / 2)
#define PVBlurMaxDelay 5
#define PVAmpsChannels 20
// How far a partial may move between two hops and still be counted as the same
// one, in BINS rather than as a frequency ratio. A ratio does not work at both
// ends: at bin 2 the estimator's own jitter is several percent, while at 20
// slots the spectrum is dense enough that a fifth's worth of tolerance lets a
// slot whose partial died grab its neighbour's -- adjacent harmonics of a
// 220 Hz tone are only 5 % apart. Two bins covers vibrato and portamento
// everywhere: even an octave glide in 100 ms moves under a bin per hop.
#define PVAmpsTrackTol 2.0


class PVAmps : public Effect, private tsl::CircularBuffer<MYFLOAT> {
public:
    PVAmps(TRACK* track, int32_t chan);

    void compute(MYFLOAT* in, int32_t size) override;

    void onBufferReady(MYFLOAT* buf, int32_t size) override;

    FFT3 fft;
    std::atomic<MYFLOAT>* range, * bounda, * boundb, * _dry, * _wet, * mode, * sm, * phasemode,
        * routing;
    //MYFLOAT win[PVBlurFFTSize];
    MYFLOAT win[PVAmpsFFTSize]{};
    MYFLOAT amps[PVAmpsFFTSize / 2];
    short indices[PVAmpsFFTSize / 2];
    tsl::complex<MYFLOAT> bins[PVAmpsFFTSize / 2];
    //std::atomic<MYFLOAT> pw{};
    Tone tone[PVAmpsChannels];
    MYFLOAT prevsm{};
    // One tracked partial per oscillator slot: the frequency it followed on the
    // previous hop (0 = idle) and its running phase. Slots used to be handed out
    // in ascending bin order, so a partial appearing below the others shifted
    // every slot by one and SMOOTH glided each oscillator across a partial it had
    // nothing to do with.
    MYFLOAT slotfreq[PVAmpsChannels]{};
    MYFLOAT slotphase[PVAmpsChannels]{};
    // A slot whose partial disappears holds its pitch and releases rather than
    // stopping dead, at the rate SMOOTH already sets. This is where the held,
    // drifting pitches come from: the original code never silenced a slot, it
    // just kept re-reading the previous hop's stale bin.
    MYFLOAT slotamp[PVAmpsChannels]{};
    // Bin magnitude -> output amplitude, derived in the constructor from the
    // windows actually in use so every waveform lands at the same level.
    MYFLOAT oscgain{ 1. };

    template<typename T>
    static void compute_hanning(T* w, const int32_t size, const T scale = 1.) {
        for (int32_t i = 0; i < size; i++)
            w[i] = 0.5 * (1. - cos(TWOPI_F_P * i / (size))) * scale;
    }
    delay<MYFLOAT> delay;
};


#endif 

//GRAINSTORM_VCO_H