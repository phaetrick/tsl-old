#pragma once
#ifndef TRACK_H
#define TRACK_H

#include <cstdint>
#include <atomic>
#include "defines.h"
#include "graingenerator.h"
#include "tools.h"
#include "Midi.h"
#include "vco.h"
#include "lfo.h"
#include "base.h"
#include <tools/queuetsl.h>
#include "random.h"
#include "sequencer.h"
#include "ffttools.h"
#include "correlation.h"
#include "SyncTargets.h"
#include "resample.h"
#include "gs_common.h"
#include "SpectrumAnalyzer.h"
#include <tools/AudioSemaphore.h>
#include <audio/Recording.h>
#include <Effects/gaintask.h>

namespace tsl {
    namespace graphics {
    class TrackButton;
    class Waveform;
    }
}
struct NodePV;

// CROSSLPCORDER tops out at 100.
#define CROSS_LPC_MAX_ORDER 100
// Largest entry in VOCODER_CHANNELS.
#define CROSS_VOC_MAX_BANDS 224

// Grain-magnitude frames the HARM/PERC separator keeps. The harmonic estimate is
// the per-bin median across these, so it needs to be odd and small: this is the
// only thing in the PV path that costs memory per track, and a longer window
// smears the harmonic estimate across read-position jumps rather than improving
// it (consecutive grains are not consecutive frames of one signal here).
#define PV_HPSS_FRAMES 5
// Widest PVHPSSWIDTH frequency median, in bins. Odd.
#define PV_HPSS_MAX_WIDTH 31

// Resonators SPECTRAL RES runs. The bank stops at Nyquist anyway, so this only
// bites on a low ROOT with a dense series -- 256 harmonics of 110 Hz already
// reach 28 kHz. Each one costs two doubles of state and about seven bins of
// work per grain.
#define PV_RES_MAX_PARTIALS 256

// PH CORRECTION III (dephase_tracked).
// How far, in bins, a peak may have moved since the previous grain and still
// count as the same partial. Wider than a mainlobe so vibrato and moderate
// glides track; narrow enough that a new partial appearing next to an old one
// is not mistaken for it and made to inherit its phase accumulator.
#define PV_PH3_TRACK_TOL 3.0
// Rectified-spectral-flux onset threshold, as a fraction of the previous
// frame's magnitude sum. TRANS maps 0..1 onto HI..LO, so turned down only a
// hard attack resets the phases and turned up almost any spectral movement does.
#define PV_PH3_FLUX_HI 1.20
#define PV_PH3_FLUX_LO 0.10
// Peak floor below the frame maximum, in dB, that PEAKS maps 0..1 onto. At the
// bottom only the few strongest partials are locked and everything else runs
// per-bin; at the top nearly every mainlobe in the frame gets its own region.
#define PV_PH3_FLOOR_LO 20.0
#define PV_PH3_FLOOR_HI 90.0
// Longest transient hold, in grains. The hold wants fft_size/step_length grains
// (until the attack has cleared the analysis window); the cap only bites at
// extreme stretch, where an uncapped hold would freeze the stretch for seconds.
#define PV_PH3_HOLD_MAX 16

// State carried across grains by CROSS_LPC. The all-pole tract filter has to
// run continuously: zeroing it every grain amputated the formant ringing (which
// is longer than the hop at any useful Q) and then overlap-added
// phase-incoherent copies of the same carrier, combing at the grain rate.
// Reflection coefficients rather than direct-form a[k], because they get
// interpolated across the grain and |k| < 1 is a stability condition that
// survives a linear blend -- interpolating a[k] has no such guarantee.
struct CrossLpcState {
    MYFLOAT kMod[CROSS_LPC_MAX_ORDER + 1]{}, kModInc[CROSS_LPC_MAX_ORDER + 1]{};
    MYFLOAT kCar[CROSS_LPC_MAX_ORDER + 1]{}, kCarInc[CROSS_LPC_MAX_ORDER + 1]{};
    MYFLOAT bMod[CROSS_LPC_MAX_ORDER + 2]{};   // all-pole lattice delays
    MYFLOAT bCar[CROSS_LPC_MAX_ORDER + 2]{};   // whitening lattice delays
    MYFLOAT exciteGain{1.}, exciteGainInc{};
    // The two filters' RMS gains, tracked apart and composed per grain. One
    // combined number bakes the whitener's prediction gain into the excitation,
    // so toggling WHITE steps the level; see cross_lpc.
    MYFLOAT tractGain{1.};                     // all-pole lattice, per unit excitation
    MYFLOAT whitenGain{1.};                    // whitening FIR, held while it is off
    int32_t runOrder{}, runCarOrder{};
    bool init{true};

    void reset() {
        for (int32_t i = 0; i <= CROSS_LPC_MAX_ORDER + 1; ++i)
            bMod[i] = bCar[i] = 0;
        for (int32_t i = 0; i <= CROSS_LPC_MAX_ORDER; ++i)
            kMod[i] = kModInc[i] = kCar[i] = kCarInc[i] = 0;
        exciteGain = 1.;
        exciteGainInc = 0;
        tractGain = 1.;
        whitenGain = 1.;
        runOrder = runCarOrder = 0;
        init = true;
    }
};

struct TRACK : SyncTarget {
    TRACK(DATA *, tsl::AppState* appState, const char* _name, int _index, const char* modulatorname);
    
    ~TRACK();
    
    bool getFilesCopy(std::vector<int16_t> files[], int32_t& offTmp);
    
    std::shared_ptr<tsl::Recording> getAudioCopy();
        
    void reset();

    tsl::parameters::Event setToInitState();
    
    void setDefaults();

    void setEnvfDest(int32_t envfindex, int target);

    void setLfoDest(int32_t lfoindex, int target);

    int number{};
    int index{};
    const char *name;
    const char *modulatorName;
    TRACK *destinationz{}, *lockerz{}, *source{};
    std::atomic<bool> updated_this_cycle{ false };
    bool disabled{};
    bool active_tmp{}, stopped_tmp{};
    bool cross_power_tmp{}, pv_power_tmp{}, doLoopPre{}, crossPowerPre{};
    int pv_func_tmp{}, cross_func_tmp{}, crossLPCOrderTmp{ 20 }, bounce_type_tmp{};
    double playbackDirTmp{ 1. };
    MYFLOAT speed_tmp{}, looppos_lfo_min_tmp{}, looppos_lfo_max_tmp{}, speed_lfo_min_tmp{}, speed_lfo_max_tmp{}, offset_tmp{}, off_start_tmp{}, off_stop_tmp{}, offset_cross_tmp{}, maxTmp[MAX_CHANNELS]{};
    // RMS of the (windowed) source grain fed into the cross-synthesis function,
    // measured locally in granulate just before crossfunc. This is the level
    // reference every cross algorithm normalises its output to, so they all come
    // out at the same loudness regardless of the algorithm's crest factor.
    MYFLOAT srcRmsTmp[MAX_CHANNELS]{};
    // Cross-synthesis output levelling (normalize_grain in pv.cpp). The two
    // energies are smoothed separately and the calibration gain is their ratio,
    // so the algorithms come out equally loud while every grain-to-grain level
    // move survives. Per channel: the ratio is a property of the algorithm, not
    // of the content, so the two channels agree without being linked. Reset on
    // an algorithm switch (synth.cpp) -- a gain calibrated for VOCODER is wrong
    // for CONVOLUTION, and the running-mean warm-up makes the new one converge
    // on its first grain.
    MYFLOAT crossRefMs[MAX_CHANNELS]{};   // smoothed target mean-square
    MYFLOAT crossOutMs[MAX_CHANNELS]{};   // smoothed raw-output mean-square
    int crossLvlN[MAX_CHANNELS]{};        // grains learned since reset
	LFO* speed_lfo_tmp{}, *looppos_lfo_tmp{};
#if defined(PLUGIN_MODE) || defined(OS_IOS)
    bool syncDawTmp{}, requestLoopPosTmp{};
	std::atomic<bool> requestLoopPos{};
	MYFLOAT loopsPerBarTmp{1};
#endif
    void (*synth_func_tmp)(TRACK *track, int channel, const bool isMultithreaded){};

    MYFLOAT grainsize_tmp{};
    std::atomic<MYFLOAT> play_dur{};
    std::atomic<int> offToInfo{};

    std::atomic<MYFLOAT> step_point_grain[MAX_CHANNELS]{};
    std::atomic<LFO *> lfo[NUM_PARAMS]{};
#ifdef IS_MULTITHREADED
    tsl::CrossBarrier cross_barrier[2];
    tsl::AudioSemaphore sem_cross[2];
#endif
    double stepLengthTmp[2]{};
    bool silenceTmp[2]{};
    tsl::graphics::TrackButton *button{};
    tsl::graphics::Waveform *waveform{};
    float xpos{}, ypos{};
    std::atomic<float> zoom{};
    std::atomic<float> pos{};
    TIME_P time_file{};
    TIME_P time_play_dur{};
    grainsequencer grainsequencer;
    BPMSyncTarget bpmSyncer;
    EffectQueue<ARRAY_LEN(grainmods2)> fx_queue_grain[2];
    EffectQueue<ARRAY_LEN(fxtypes2)> fx_queue[2];
    EffectQueue<ARRAY_LEN(reverbtypes)> fx_queue_stereo{};
    tsl::AudioSemaphore stereosem;
    int32_t fd{};
    MYFLOAT pitchfact[MAX_CHANNELS]{1, 1};
    MYFLOAT glissfact[MAX_CHANNELS]{1, 1};
    std::shared_ptr<tsl::RecordingState> currentState{};
    tsl::AtomicSharedPtr<tsl::Recording> filebuffer;
    tsl::AtomicSharedPtr<tsl::Recording> filebufferold;
    MYFLOAT *ringbuffer[MAX_CHANNELS]{};
    MYFLOAT *grain_buffer[MAX_CHANNELS]{};
    MYFLOAT *lfobuffer[3]{};
    MYFLOAT *fft_out[MAX_CHANNELS]{};
    MYFLOAT *fft_help1[MAX_CHANNELS]{};
    MYFLOAT *fft_help2[MAX_CHANNELS]{};
    MYFLOAT *envf_buffer[MAX_CHANNELS]{};
    MYFLOAT *out_buf[MAX_CHANNELS]{};
    std::unique_ptr<FFT> ffts[MAX_CHANNELS][NUMFFTS + 1];
    int *warp_table[MAX_CHANNELS]{};
    LFO lfo1;
    LFO lfo2;
    LFO lfo3;
    LFO *lfos[3]{&lfo1, &lfo2, &lfo3};

    std::atomic<MYFLOAT> tmpoffset[MAX_CHANNELS]{};
    std::atomic<double> bypass[NUM_PARAMSPACES]{};
    std::atomic<double> fxpower[NUM_PARAMSPACES]{};

    int current_nodes_pv[2]{};
    int pv_peaks[MAX_CHANNELS][2][MAX_FFT_SIZE + 1]{};
    int pv_nprevpeaks[MAX_CHANNELS]{};
    NodePV *nodes_pv[MAX_CHANNELS][2][MAX_FFT_SIZE + 1]{};
    bool pvLockedInit[2] = {true, true};
    // Set whenever the phase vocoder skips a grain (speed 0 or 1). The next
    // grain it does process starts its phase accumulator from the measured
    // phase instead of running it on from a frame that may be seconds old.
    bool pvInit[2] = {true, true};

    // PH CORRECTION III. Peaks are kept as fractional bin positions (parabolic
    // refinement) alongside the integer bin they were found at, ping-ponged on
    // the same current_nodes_pv index as nodes_pv so a frame's peaks always
    // travel with the magnitudes and phases they were picked from. Sized to the
    // half-spectrum: peak picking advances at least 3 bins, so it cannot fill
    // even a third of this, but the bound is what matters.
    MYFLOAT *pv3Peak[MAX_CHANNELS][2]{};
    int32_t *pv3PeakBin[MAX_CHANNELS][2]{};
    int pv3NPeaks[MAX_CHANNELS][2]{};
    bool pv3Init[MAX_CHANNELS]{true, true};
    // Grains left in the current transient hold (phase advances at the natural
    // rate while an attack is inside the analysis window; see dephase_tracked).
    int pv3Hold[MAX_CHANNELS]{};

    // Per-band vocoder followers, and the band count they were sized for.
    MYFLOAT vocBandGain[MAX_CHANNELS][CROSS_VOC_MAX_BANDS]{};
    int vocBands[MAX_CHANNELS]{};
    CrossLpcState crossLpc[MAX_CHANNELS];

    // HARM/PERC magnitude history: PV_HPSS_FRAMES frames of MAX_FFT_SIZE/2 bins,
    // used as a ring. hpssSize is the half-spectrum the ring currently holds, so
    // an FFT SIZE change refills instead of medianing across two layouts;
    // hpssFill is set on an algorithm switch for the same reason.
    MYFLOAT *hpssHist[MAX_CHANNELS]{};
    int hpssPos[MAX_CHANNELS]{};
    int hpssSize[MAX_CHANNELS]{};
    bool hpssFill[MAX_CHANNELS]{true, true};

    // SPECTRAL RES: one complex accumulator per resonator, interleaved re/im.
    // Indexed by partial number and held in hertz, not in bins, so a change of
    // FFT SIZE (or of ROOT, which is the point of putting an LFO on it) leaves
    // the ringing partials intact instead of invalidating the bank. resFill
    // wipes it when the algorithm is switched away from, so coming back does
    // not ring on whatever the last selection left behind.
    MYFLOAT resState[MAX_CHANNELS][2 * PV_RES_MAX_PARTIALS]{};
    bool resFill[MAX_CHANNELS]{true, true};

    // SPECTRAL FREEZE: per-bin held magnitude, per-output-hop phase advance,
    // synthesis phase accumulator, and the previous grain's analysis phase (for
    // the heterodyne frequency estimate). Per-bin state, so an FFT SIZE change
    // re-seeds (freezeSize, like hpssSize) and an algorithm switch does too
    // (freezeFill, like hpssFill).
    MYFLOAT *freezeMag[MAX_CHANNELS]{};
    MYFLOAT *freezeFreq[MAX_CHANNELS]{};
    MYFLOAT *freezePsi[MAX_CHANNELS]{};
    MYFLOAT *freezePrevPhi[MAX_CHANNELS]{};
    int freezeSize[MAX_CHANNELS]{};
    bool freezeFill[MAX_CHANNELS]{true, true};

    // SPECTRAL DUCK: per-bin smoothed gate gain across grains. Same
    // re-seed rules as the freeze state.
    MYFLOAT *duckGain[MAX_CHANNELS]{};
    int duckSize[MAX_CHANNELS]{};
    bool duckFill[MAX_CHANNELS]{true, true};

    VCO *vco[MAX_CHANNELS]{};
    /*
    float filterenv[WINDOW_SIZE]{};

    float specdel2env[WINDOW_SIZE]{};

    float specfiltenv[WINDOW_SIZE]{};
*/
    bool grainenvcomputebandlimited[NUM_GRAINENVS]{};
    MYFLOAT oldbandlimitedgrainenv{};
    WaveTables bandlimitedEnvtable[NUM_GRAINENVS];
   
    WaveTable grainenv[NUM_GRAINENVS]{WINDOW_SIZE, WINDOW_SIZE, WINDOW_SIZE};
    MYFLOAT xdc[MAX_CHANNELS]{}, ydc[MAX_CHANNELS]{};

    MYFLOAT *pitchdetectoutbuf[MAX_CHANNELS]{};

    tsl::Syncing::SyncResult control(uint16_t todo, std::vector<tsl::parameters::Event>&)override;

    double getSamples()override;

    tsl::Syncing::SyncResult syncBySamples(double samples, std::vector<tsl::parameters::Event>&)override;

    DelaySyncTarget delaySyncTargets[10];
    std::unique_ptr<CrossCorrelation<MYFLOAT>> corrWSOLA[MAX_CHANNELS];
    //grainqueue grainqueue[2];
    MYFLOAT postgainsmoothed{1.0}, pregrainSmoothed{};

    void init();

    void OnGotSampleRate();

    static void setupTracks(tsl::AppState *_state);

    MYFLOAT grainfilttable[2][TBLSIZE3]{};
    std::shared_ptr<Effect> pitchDetectGrain[2]{};
    graingenerator graingen;

    tsl::parameters::Event loadAudio(std::shared_ptr<tsl::Recording>&);
    tsl::parameters::Event loadAudio(std::shared_ptr<tsl::Recording>&, tsl::RecordingDiff &);
    tsl::parameters::Event currentAudioToEvent();
    tsl::AppState *_appState;
    std::unique_ptr<PolyPhaseResampler<MYFLOAT, 32, 256>> rst[2]{};
    void computeLoopTime();
    double getLoopSamples();
    
	SpectrumAnalyzer spectrumAnalyzer;
    tsl::gaintask gainTask;
};


#endif