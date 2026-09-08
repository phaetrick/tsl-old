#include "defines.h"
#include "granulate.h"
#include "granulate_fft.h"
#include "granulate_loop.h"
#include "synth.h"
#include "grainstorm.h"
#include "track.h"
#include "envelope.h"
#include "infopanel.h"
#include "button.h"
#include "tools/aligned_memalloc.h"
#include "ffttools.h"
#include "tools/queuetsl.h"
#include "tools.h"
#include "view.h"
#include "vocoder.h"
#include "lfo.h"
#include "pv.h"
#include <cstring>
#include <chrono>
#include <thread>
#include <keyboard.h>
#include "player.h"
#include <views.h>
#include <BackView.h>
#include <tools/StackVector.h>
#include <tools/AdpfHint.h>


static inline int32_t irand(int a, int e);

// hintActive: the caller owns a live ADPF session for the thread this runs on,
// so the governor is already being told which deadline to hold and the
// generateLoad spin at the end must not also run. ChannelThread::run makes the
// same either/or choice for the worker threads; this parameter is how the
// coordinator gets to make it, since the spin lives down here and the session
// lives up in SynthThread::run.
static inline void synthFuncIntern(tsl::AppState* _appState, bool isMultiThreaded,
	bool hintActive = false);

#ifdef IS_MULTITHREADED

constexpr int32_t kLoadGenerationStepSizeNanos = 20000;
constexpr double kPercentageOfCallbackToUse = 0.8;
constexpr float kFilterCoefficient = 0.1;


static inline void generateLoad(int64_t durationNanos, double& mOpsPerNano) {
	int64_t currentTimeNanos = tsl::time::nanosecondsSinceEpoch();
	int64_t deadlineTimeNanos = currentTimeNanos + durationNanos;

	// opsPerStep gives us an estimated number of operations which need to be run to fully utilize
	// the CPU for a fixed amount of time (specified by kLoadGenerationStepSizeNanos).
	// After each step the opsPerStep value is re-calculated based on the actual time taken to
	// execute those operations.
	auto opsPerStep = (int)(mOpsPerNano * kLoadGenerationStepSizeNanos);
	int64_t stepDurationNanos = 0;
	int64_t previousTimeNanos = 0;

	while (currentTimeNanos <= deadlineTimeNanos) {

		for (int32_t i = 0; i < opsPerStep; i++) cpu_relax();

		previousTimeNanos = currentTimeNanos;
		currentTimeNanos = tsl::time::nanosecondsSinceEpoch();
		stepDurationNanos = currentTimeNanos - previousTimeNanos;

		// Calculate exponential moving average to smooth out values, this acts as a low pass filter.
		// @see https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
		auto measuredOpsPerNano = (double)opsPerStep / stepDurationNanos;
		mOpsPerNano =
			kFilterCoefficient * measuredOpsPerNano + (1.0 - kFilterCoefficient) * mOpsPerNano;
		opsPerStep = (int)(mOpsPerNano * kLoadGenerationStepSizeNanos);
	}
}


void tsl::synth::ChannelThread::run() {
	auto _appState = track->_appState;
#ifdef __ANDROID__
	double mOpsPerNano = 1;
	// Ask the governor to keep this thread fast, instead of buying it with
	// generateLoad below. See tools/AdpfHint.h; the spin remains the fallback
	// for devices that do not implement the hint.
	tsl::AdpfHint hint;
	bool hintTried = false;
#endif
	tprio(-19);
	auto& workerLatch = _STATE->data->workerLatch;
	while (true) {
		sem.acquire();
		if (threadShouldExit())
			break;
#ifdef __ANDROID__
		const int64_t numFramesAsNanos =
			(_STATE->currentBufSize * kPercentageOfCallbackToUse / _STATE->sr) *
			tsl::time::nanosPerSecond;
		// The hint wants the whole deadline, not the 0.8 of it the spin aims at.
		const int64_t blockNanos =
			int64_t((double(_STATE->currentBufSize) / _STATE->sr) * tsl::time::nanosPerSecond);
		// Opened on the first block, not at thread start: the session is bound to
		// a deadline, and the buffer size is not known until the audio callback
		// has asked for one.
		if (!hintTried) {
			hintTried = true;
			LOGD("ChannelThread %d/%d ADPF %s", track->number, channel,
				hint.startForCurrentThread(blockNanos) ? "session created" : "unavailable");
		}
		hint.setTarget(blockNanos);
		auto start = tsl::time::nanosecondsSinceEpoch();
#endif
		track->synth_func_tmp(track, channel, true);

		workerLatch.done();

#ifdef __ANDROID__
		const auto elapsed = tsl::time::nanosecondsSinceEpoch() - start;
		if (hint.active()) {
			// Reporting is the half that does the work: the governor adjusts by
			// comparing the actual duration against the target. A session that
			// is never reported to does nothing.
			hint.report(elapsed);
		}
		else if (_DATA->stabilize) {
			auto nanosleep = numFramesAsNanos - elapsed;

			if (nanosleep > 0)
				generateLoad(nanosleep, mOpsPerNano);
		}
#endif

	};
#ifdef __ANDROID__
	hint.stop();
#endif
	//	track->_STATE->data->cool_sem.incr(1);
	LOGD("ChannelThread Track: %d Channel: %d GoodBye", track->number, channel);
}

void tsl::synth::ChannelThread::stop() {
	{
		signalThreadShouldExit();

		sem.release();

	}
	waitForThreadToExit(-1);
}

#if defined __ANDROID__

void tsl::synth::SynthThread::stop() {
	if (_appState == nullptr)return;
	{
		_STATE->data->workerLatch.wait();
		for (int32_t j = 0; j < 4; j++) {
			for (int32_t i = 0; i < _STATE->channels; i++) {
				_STATE->data->channelThreads[j * _STATE->channels + i].stop();
			}
		}
		signalThreadShouldExit();

		_DATA->sem.release();

	}
	waitForThreadToExit(-1);
}

void tsl::synth::SynthThread::run() {
	if (_appState == nullptr)return;
	tprio(-19);
	// This thread runs the whole synth pass and dispatches the channel threads,
	// so it carries a deadline of its own and wants the same treatment. Unlike
	// ChannelThread it does not spin here — its spin is the generateLoad at the
	// tail of synthFuncIntern, which is why hint.active() has to be handed down
	// through the call rather than just consulted locally.
	tsl::AdpfHint hint;
	bool hintTried = false;
	while (true) {
		_STATE->data->sem.acquire();
		if (threadShouldExit())break;

		const int64_t blockNanos =
			int64_t((double(_STATE->currentBufSize) / _STATE->sr) * tsl::time::nanosPerSecond);
		if (!hintTried) {
			hintTried = true;
			LOGD("SynthThread ADPF %s",
				hint.startForCurrentThread(blockNanos) ? "session created" : "unavailable");
		}
		hint.setTarget(blockNanos);

		const auto start = tsl::time::nanosecondsSinceEpoch();
		synthFuncIntern(_STATE, true, hint.active());
		hint.report(tsl::time::nanosecondsSinceEpoch() - start);
	}
	hint.stop();
}

#endif
#endif


#if defined __ANDROID__
#if defined(__i386__) || defined(__x86_64__)
#define cpu_relax() asm volatile("rep; nop" ::: "memory");
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)

#elif defined(__arm__) || defined(__mips__)
#define cpu_relax() asm volatile("":::"memory")

#elif defined(__aarch64__)
#define cpu_relax() asm volatile("yield" ::: "memory")
#else
//#error "cpu_relax is not defined for this architecture"
#endif
#endif


// Optional clamp for resampling sanity
inline double Clamp(double v, double lo, double hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

static inline void checkwindows(TRACK* track) {
	auto _appState = track->_appState;
	auto& params = _appState->params[track->index];
	auto newval = params[BANDLIMITEDGRAINENV].load();
	if (newval != track->oldbandlimitedgrainenv) {
		track->oldbandlimitedgrainenv = newval;
		if (newval == 1.0) {
			if (track->grainenvcomputebandlimited[SPACE_GRAINENV1]) {
				track->grainenvcomputebandlimited[SPACE_GRAINENV1] = false;
				track->bandlimitedEnvtable[SPACE_GRAINENV1].setup(track->grainenv[SPACE_GRAINENV1],
					track->grainenv[SPACE_GRAINENV1].tableSize);
			}
			if (track->grainenvcomputebandlimited[SPACE_GRAINENV2]) {
				track->grainenvcomputebandlimited[SPACE_GRAINENV2] = false;
				track->bandlimitedEnvtable[SPACE_GRAINENV2].setup(track->grainenv[SPACE_GRAINENV2],
					track->grainenv[SPACE_GRAINENV2].tableSize);

			}
			if (track->grainenvcomputebandlimited[SPACE_GRAINENV3]) {
				track->grainenvcomputebandlimited[SPACE_GRAINENV3] = false;
				track->bandlimitedEnvtable[SPACE_GRAINENV3].setup(track->grainenv[SPACE_GRAINENV3],
					track->grainenv[SPACE_GRAINENV3].tableSize);
			}
		}
	}

	if (_STATE->params[track->index][RECOMPUTEGRAINENV1].exchange(0.0) == 1.0) {
		const auto awin_outer_env = _DATA->windows[(int)params[ENVOUTER].load()];
		const auto awin_inner_env = _DATA->windows[(int)params[ENVINNER].load()];
		auto awin_outer_cycles = (int)params[AOUTERCYCLES].load();
		auto awin_outer_depth = params[AOUTERDEPTH].load();
		double step_point = 0;

		for (int32_t i = 0; i < WINDOW_SIZE; i++) {
			track->grainenv[SPACE_GRAINENV1][i] =
				awin_inner_env[i] * (1. - awin_outer_env[(int)step_point] * awin_outer_depth);
			step_point += awin_outer_cycles;
			if (step_point >= WINDOW_SIZE)
				step_point -= WINDOW_SIZE;
		}
		track->grainenv[SPACE_GRAINENV1][WINDOW_SIZE] = track->grainenv[SPACE_GRAINENV1][0];

		if (track->oldbandlimitedgrainenv == 1.0) {
			track->bandlimitedEnvtable[SPACE_GRAINENV1].setup(track->grainenv[SPACE_GRAINENV1],
				track->grainenv[SPACE_GRAINENV1].tableSize);
		}
		else
			track->grainenvcomputebandlimited[SPACE_GRAINENV1] = true;
	}

	if (_STATE->params[track->index][RECOMPUTEGRAINENV2].exchange(0.0) == 1.0) {
		const auto awin_outer_env = _DATA->windows[(int)params[ENVOUTER2].load()];
		const auto awin_inner_env = _DATA->windows[(int)params[ENVINNER2].load()];
		int32_t awin_outer_cycles = (int)params[AOUTERCYCLES2].load();
		auto awin_outer_depth = params[AOUTERDEPTH2].load();
		double step_point = 0;

		for (int32_t i = 0; i < WINDOW_SIZE; i++) {
			track->grainenv[SPACE_GRAINENV2][i] =
				awin_inner_env[i] * (1. - awin_outer_env[(int)step_point] * awin_outer_depth);
			step_point += awin_outer_cycles;
			if (step_point >= WINDOW_SIZE)
				step_point -= WINDOW_SIZE;
		}
		track->grainenv[SPACE_GRAINENV2][WINDOW_SIZE] = track->grainenv[SPACE_GRAINENV2][0];

		if (track->oldbandlimitedgrainenv == 1.0) {
			track->bandlimitedEnvtable[SPACE_GRAINENV2].setup(track->grainenv[SPACE_GRAINENV2],
				track->grainenv[SPACE_GRAINENV2].tableSize);

		}
		else
			track->grainenvcomputebandlimited[SPACE_GRAINENV2] = true;
	}


	if (_STATE->params[track->index][RECOMPUTEGRAINENV3].exchange(0.0) == 1.0) {


		int32_t nsegs = params[GRAINNSEGS].load();


		MYFLOAT xx[32], yy[32];

		for (int32_t i = 0; i < nsegs + 1; i++) {
			xx[i] = params[GRAINENVX0 + i].load();
			auto v = params[GRAINENVY0 + i].load(); // 0..1

			auto db = -60.0 + v * 60.0;
			auto lin = std::pow(10.0, db / 20.0);

			// make -60 dB become exact 0
			lin = (lin - 0.001) / 0.999;

			yy[i] = std::max(0.0, lin);
			
		}

		tsl::envelope::compute<MYFLOAT>[(int)params[GRAINCURVE].load()](
			track->grainenv[SPACE_GRAINENV3].data(), WINDOW_SIZE,
			xx,
			yy, nsegs, true);
		track->grainenv[SPACE_GRAINENV3][WINDOW_SIZE] = track->grainenv[SPACE_GRAINENV3][0];

		if (track->oldbandlimitedgrainenv == 1.0) {
			track->bandlimitedEnvtable[SPACE_GRAINENV3].setup(track->grainenv[SPACE_GRAINENV3],
				track->grainenv[SPACE_GRAINENV3].tableSize);

		}
		else
			track->grainenvcomputebandlimited[SPACE_GRAINENV3] = true;
	}
	for (auto lfo : track->lfos) {
		lfo->reCompute();
	}
	for (int32_t i = 2; i >= 0; i--)
		track->lfos[i]->setUpBuffer();
}


static inline void setupTracks(tsl::AppState* _appState, tsl::StackVector<TRACK*, 4>& tracks) {

	for (auto track : _DATA->tracks) {
		track->active_tmp = _STATE->params[track->index][POWERTRACK].load() && ! track->disabled;
		if (track->active_tmp) {
			track->doLoopPre = track->fxpower[SPACE_LOOPER].load();
			track->crossPowerPre = track->fxpower[SPACE_CROSS_MAIN].load() &&
				!track->lockerz->fxpower[SPACE_CROSS_MAIN].load();
			track->pv_power_tmp = track->fxpower[SPACE_PV_MAIN].load();
			tracks.push_back(track);

		}
	}
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	const auto discontinuity = _DATA->hostTimeSnapshot.DetectTransportDiscontinuity();
#endif

	for (auto track : tracks) {
		auto source = _DATA->tracks[(int)_STATE->params[track->index][DISTRSOURCE].load()];
		track->source = (track->doLoopPre == source->doLoopPre) ? source : track;
		track->source->updated_this_cycle.store(false, std::memory_order_release);
		auto rec = track->source->filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (state) {
			track->offset_tmp = state->offset.load();
			track->off_start_tmp = state->off_start.load();
			track->off_stop_tmp = state->off_stop.load();
			track->bounce_type_tmp = state->bounceType.load();
			track->playbackDirTmp = state->playbackDir.load();
			track->currentState = state;
		}
		track->stopped_tmp = _STATE->params[track->source->index][TRACKSTOPPED].load() == 1.0;
		track->speed_tmp = std::max(0., LOG2NORMALF(
			_STATE->params[track->source->index][SPEED].load()
		) - SPEED_OFFSET);
		auto lfo_speed = track->source->lfo[SPEED].load();
		track->speed_lfo_tmp = lfo_speed && lfo_speed->power() ? lfo_speed : nullptr;
		if (track->speed_lfo_tmp) {
			track->speed_lfo_min_tmp =
				std::max(0., LOG2NORMALF(_STATE->controls[track->source->index][SPEED].lfo_min.load()) -
				SPEED_OFFSET);
			track->speed_lfo_max_tmp =
				std::max(0., LOG2NORMALF(_STATE->controls[track->source->index][SPEED].lfo_max.load()) -
				SPEED_OFFSET);
		}
		auto lfo_looppos = track->source->lfo[LOOP_POS].load();
		track->looppos_lfo_tmp = lfo_looppos && lfo_looppos->power() ? lfo_looppos : nullptr;
		if (track->looppos_lfo_tmp) {
			track->looppos_lfo_min_tmp =
				_STATE->controls[track->source->index][LOOP_POS].lfo_min.load();
			track->looppos_lfo_max_tmp =
				_STATE->controls[track->source->index][LOOP_POS].lfo_max.load();
		}
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		track->syncDawTmp = _STATE->params[track->source->index][LOOPSYNCDAWTRANSPORT].load() == 1.0;

		if (track->syncDawTmp) {
			track->loopsPerBarTmp = _STATE->parameters[LOOPSYNCDAWBEATS].toDisplay(_STATE->sr, _STATE->params[track->source->index][LOOPSYNCDAWBEATS].load());
			track->requestLoopPosTmp = discontinuity || track->source->requestLoopPos.exchange(false, std::memory_order_acq_rel);
		}

#endif
		track->graingen.prepare();
		track->grainsequencer.check();
		checkwindows(track);
		if (track->lockerz->crossPowerPre &&
			track->lockerz->active_tmp &&
			!track->lockerz->doLoopPre) {
			track->synth_func_tmp = compute_fft;
		}
		else
			track->synth_func_tmp = track->doLoopPre ? loop : granulate;
		track->cross_power_tmp = track->crossPowerPre && track->destinationz->active_tmp;
		track->grainsize_tmp =
			track->cross_power_tmp || track->pv_power_tmp
			? static_cast<int>(_STATE->params[track->index][FFT_SIZE].load())
			: static_cast<int>(_STATE->params[track->index][GRAINSIZE].load());
		if (track->cross_power_tmp) {
#ifdef IS_MULTITHREADED
			for (int32_t i = 0; i < _STATE->channels; i++) {
				track->cross_barrier[i].reset();
			}
#endif
			const int newcross = (int)_STATE->params[track->index][ASCROSS].load();
			if (newcross != track->cross_func_tmp) {
				// The LPC lattice carries filter state across grains, so
				// switching algorithm has to wipe it or the first grain back on
				// LPC rings from whatever the last LPC grain left behind.
				// The output-level calibration is per algorithm for the same
				// reason: the level CONVOLUTION needs is not the level VOCODER
				// needs, and carrying one into the other is an audible jump that
				// then slides back over a second.
				for (int32_t ch = 0; ch < MAX_CHANNELS; ch++) {
					track->crossLpc[ch].reset();
					track->crossRefMs[ch] = track->crossOutMs[ch] = 0;
					track->crossLvlN[ch] = 0;
					track->duckFill[ch] = true;
				}
			}
			track->cross_func_tmp = newcross;
			track->crossLPCOrderTmp = static_cast<int>(_STATE->params[track->index][CROSSLPCORDER].load());
			// if(track->grainsize_tmp == 256)track->crossLPCOrderTmp = 2;
			// else if(track->grainsize_tmp == 512)track->crossLPCOrderTmp = 4;
		}
		else
			// Same reason, for the gap while cross is switched off entirely --
			// the source can be a different sound by the time it comes back.
			for (int32_t ch = 0; ch < MAX_CHANNELS; ch++) {
				track->crossRefMs[ch] = track->crossOutMs[ch] = 0;
				track->crossLvlN[ch] = 0;
				track->duckFill[ch] = true;
			}
		if (track->pv_power_tmp) {
			const int newpv = (int)_STATE->params[track->index][ASPV].load();
			// HARM/PERC medians across the last few grains, so the frames it kept
			// while another algorithm was running describe a different sound.
			// Refill on the way back in rather than medianing across the switch.
			// SPECTRAL RES rings on across grains for as long as DECAY says, so
			// the same switch has to wipe its bank -- otherwise coming back lands
			// a tail belonging to whatever was playing before it.
			// PH CORRECTION III accumulates phase and keeps a peak list across
			// grains, so the same switch has to make it re-seed rather than
			// continue against a frame belonging to another algorithm.
			if (newpv != track->pv_func_tmp)
				for (int32_t ch = 0; ch < MAX_CHANNELS; ch++) {
					track->hpssFill[ch] = true;
					track->resFill[ch] = true;
					track->pv3Init[ch] = true;
					track->pv3Hold[ch] = 0;
					track->freezeFill[ch] = true;
				}
			track->pv_func_tmp = newpv;
		}
		else
			// Same reason, for the gap while PV is switched off entirely.
			for (int32_t ch = 0; ch < MAX_CHANNELS; ch++) {
				track->hpssFill[ch] = true;
				track->resFill[ch] = true;
				track->pv3Init[ch] = true;
				track->pv3Hold[ch] = 0;
				track->freezeFill[ch] = true;
			}

	}

	// Allocate the FFT plans a spectral grain will need here, in the once-per-
	// block prep, so the per-grain CHECKFFT never has to make_unique inside the
	// grain loop -- an allocation there lands on a worker (or the audio) thread
	// mid-grain and glitches. Runs as its own pass so every grainsize_tmp above
	// is already settled: a modulator sizes its transforms from its carrier's
	// (lockerz) grainsize, and the carrier may come later in the list.
	// CONVOLUTION and LPC run a zero-padded 2N transform, so warm that too, and
	// warm both sides -- the modulator's compute_fft needs the same sizes the
	// carrier's cross function does.
	for (auto track : tracks) {
		int32_t gs = 0;
		int crossfn = -1;
		if (track->synth_func_tmp == compute_fft) {
			gs = (int32_t)track->lockerz->grainsize_tmp;      // modulator side
			crossfn = track->lockerz->cross_func_tmp;
		}
		else if (track->cross_power_tmp || track->pv_power_tmp) {
			gs = (int32_t)track->grainsize_tmp;               // carrier side
			if (track->cross_power_tmp) crossfn = track->cross_func_tmp;
		}
		if (gs <= 0) continue;

		const bool needsDouble = crossfn == CROSS_CONV || crossfn == CROSS_LPC;
		const int32_t i1 = (int32_t)std::log2(gs);
		const int32_t i2 = (int32_t)std::log2(gs * 2);
		for (int32_t ch = 0; ch < _STATE->channels; ch++) {
			if (i1 >= 0 && i1 <= NUMFFTS && track->ffts[ch][i1] == nullptr)
				track->ffts[ch][i1] = std::make_unique<FFT>(gs);
			if (needsDouble && i2 >= 0 && i2 <= NUMFFTS && track->ffts[ch][i2] == nullptr)
				track->ffts[ch][i2] = std::make_unique<FFT>(gs * 2);
		}
	}
}

static inline void synthFuncIntern(tsl::AppState* _appState, bool isMultiThreaded,
	bool hintActive) {
	const auto numFrames = _STATE->currentBufSize;
#ifdef __ANDROID__

	const int64_t numFramesAsNanos =
		(numFrames * kPercentageOfCallbackToUse / _STATE->sr) *
		tsl::time::nanosPerSecond;
#endif


	const int32_t checkload = _STATE->sr / 4;



	int64_t start = tsl::time::nanosecondsSinceEpoch();
	{
			while (auto func = _DATA->toAudioThreadQueue.try_pop()) {
				(*func)();
			}
	}

	tsl::StackVector<TRACK*, 4> tracks;

	setupTracks(_appState, tracks);

#ifdef IS_MULTITHREADED
	if (isMultiThreaded) {
		auto count = tracks.size();
		auto channels = _STATE->channels;
		_DATA->workerLatch.reset(count * channels - 1);
		TRACK* done = nullptr;

		for (auto t : tracks) {
			if (!done) {
				done = t;
				for (int32_t i = 1; i < channels; i++)
					_STATE->data->channelThreads[t->index * MAX_CHANNELS + i].sem.release();
			}
			else {
				for (int32_t i = 0; i < channels; i++)
					_STATE->data->channelThreads[t->index * MAX_CHANNELS + i].sem.release();
			}
		}
		if (done) {
			done->synth_func_tmp(done, 0, true);
		}
		// Spin most of a block before parking. We only get here after running our
		// own slice, so the remaining wait is worker skew -- normally short, and
		// staying on the CPU for it is far cheaper than a futex round trip plus
		// the governor deciding this thread is idle. Parking is the escape hatch
		// for a worker that has genuinely been descheduled, not the normal path.
		_DATA->workerLatch.wait(
			int64_t((double(numFrames) / _STATE->sr) * tsl::time::nanosPerSecond * 0.75));
	}
	else {
#endif
		for (auto t : tracks) {
			if (t->synth_func_tmp != compute_fft)
				for (int i = _STATE->channels - 1; i >= 0; i--) {
					t->synth_func_tmp(t, i, false);
				}
		}
#ifdef IS_MULTITHREADED
	}
#endif
	_STATE->ringPos = (_STATE->ringPos + numFrames) & _STATE->ringMask;

	_DATA->offset += numFrames;
	auto diff = tsl::time::nanosecondsSinceEpoch() - start;
	_STATE->diffprint += diff;
	if (diff > _STATE->nanospersample *
		numFrames) {
		;// LOGE("XRUN %g", diff / (double)(_STATE->nanospersample * numFrames));
	}

	_STATE->bb += numFrames;

	if (_STATE->bb >= checkload) {
		_STATE->bb -= checkload;
		_STATE->buffer_fill = _STATE->diffprint;
		_STATE->diffprint = 0;
	}
#if defined IS_MULTITHREADED && defined __ANDROID__
	if (isMultiThreaded)
		_STATE->data->open.store(false);
#endif
#ifdef __ANDROID__
	// The hint and the spin are alternatives, never both: a caller with a live
	// ADPF session has already declared its deadline, so burning out the rest of
	// the block on top of that buys nothing and costs ~90% of a core (see
	// tools/AdpfHint.h for the measurement). Without a session -- API < 33, or a
	// device that does not implement it -- the spin is still the only thing
	// keeping the core from being downclocked between blocks, so it stays.
	if (!hintActive && _DATA->stabilize) {
		auto nanosleep = numFramesAsNanos - (tsl::time::nanosecondsSinceEpoch() - start);

		// thread_local, not a plain local: generateLoad refines this estimate as
		// it runs, and re-declaring it per block threw that away every time and
		// restarted from a guess of 1, which overshoots. ChannelThread::run keeps
		// its copy across blocks for the same reason. Per thread rather than
		// static because synthFuncIntern runs on the audio thread, on
		// SynthThread, and on record_loop's thread, which neither share a core
		// nor may race on it.
		static thread_local double sOpsPerNano = 1;

		if (nanosleep > 0)
			generateLoad(nanosleep, sOpsPerNano);
	}
#endif

}

#include <security/signature.h>

#ifndef __ANDROID__
inline bool isChannelConnected(int index, int mask) { return mask & (1 << index); }

void PlayerBase::synthFunc(tsl::AppState* _appState, sampleTSL** tin, sampleTSL** out, int frames, int channelMask) {

	_STATE->channelMask.store(channelMask);
	auto& rsOut = _STATE->rsOut;
	int outChans = 0;
	if (isChannelConnected(OUTPUT_ACTIVE_BIT_L, channelMask)) outChans++;
	if (isChannelConnected(OUTPUT_ACTIVE_BIT_R, channelMask)) outChans++;
	// The engine block size is driven by the INPUT resampler when there is one: rsIn
	// decides how many engine frames this host block turns into, and _STATE->inputPos
	// must advance by exactly that many or the input ring drops/repeats samples at the
	// block seam. outputSamplesCreated() replays the same phase arithmetic the input
	// loop below performs, so it predicts `produced` exactly and the size guard can
	// still run before anything is written.
#if defined DOES_INPUT_RESAMPLING
	auto numFrames = _STATE->rsIn.outputSamplesCreated(frames);
#else
	auto numFrames = rsOut.inputSamplesNeeded(frames);
#endif

	if (numFrames > _appState->maxBufSize) {
		if (auto info = _appState->data->views.infopanel) {
			info->setTimeStamp();
			info->text.store("BUFFERSIZE TOO BIG!");
			info->setRenderFunc(tsl::graphics::InfoPanel::RenderText);
		}
		for (int i = 0; i < frames; i++)
			for (int j = 0; j < outChans; j++) out[j][i] = 0;
		return;
	}



	MYFLOAT* inBuffers[MAX_CHANNELS];
	int isActive[MAX_CHANNELS];

	int bufIndex = 0;
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		if (isChannelConnected(i, channelMask)) {
			inBuffers[i] = tin[bufIndex];
			isActive[i] = 1;// _DATA->tracks[static_cast<int>(floor(i / 2))]->active_tmp ? 1 : 0;
			bufIndex++;
		}
		else {
			inBuffers[i] = nullptr;
			isActive[i] = 0;
		}
	}
#ifdef DOES_INPUT_RESAMPLING
	auto& rsIn = _STATE->rsIn;

	auto inputMask = _STATE->inputMask;
	auto inputPosInternal = (_STATE->inputPos + 1) & inputMask;
	int samplesRead = 0;
	int produced = 0;

	while (samplesRead < frames) {
		if (rsIn.isWriteNeeded()) {
			sampleTSL tmp[MAX_CHANNELS]{};
			for (int chan = 0; chan < MAX_CHANNELS; chan++) {
				if (inBuffers[chan] != nullptr) {
					tmp[chan] = inBuffers[chan][samplesRead];
				}
				else {
					tmp[chan] = 0.0;
				}
			}
			rsIn.writeNextFrame(tmp);
			samplesRead++;
		}
		else {
			MYFLOAT tmp[MAX_CHANNELS]{};
			rsIn.readNextFrame(tmp, isActive);
			if (inBuffers[1] == nullptr && inBuffers[0] != nullptr)
				tmp[1] = tmp[0];
			for (int chan = 0; chan < MAX_CHANNELS; chan++) {
				_STATE->inputBuf[chan][inputPosInternal] = tmp[chan];
			}
			inputPosInternal = (inputPosInternal + 1) & inputMask;
			produced++;
		}
	}
	while (!rsIn.isWriteNeeded()) {
		MYFLOAT tmp[MAX_CHANNELS]{};
		rsIn.readNextFrame(tmp, isActive);
		if (inBuffers[1] == nullptr && inBuffers[0] != nullptr)
			tmp[1] = tmp[0];
		for (int chan = 0; chan < MAX_CHANNELS; chan++) {
			_STATE->inputBuf[chan][inputPosInternal] = tmp[chan];
		}
		inputPosInternal = (inputPosInternal + 1) & inputMask;
		produced++;
	}
	// outputSamplesCreated() above is the same simulation this loop just ran, so these
	// always agree. Track the real count anyway -- inputPos advancing by anything other
	// than what was actually written is exactly the bug this fixes.
	numFrames = produced;

	if (false) {
		std::stringstream ss;
		ss << "I: " << frames << " O: " << numFrames << std::endl;
		static char buf11[4000];
		std::string s = ss.str();
		std::strcpy(buf11, s.c_str());
		if (auto info = _appState->data->views.infopanel) {
			info->setTimeStamp();
			info->text.store(&buf11[0]);
			info->setRenderFunc(tsl::graphics::InfoPanel::RenderText);
		}
	}

	if (_DATA->recorder._isRecording.load(std::memory_order_relaxed))
	{
		//static double phase = 0;
		int totalFrames = numFrames;
		auto inputPos = _STATE->inputPos & inputMask;

		while (totalFrames > 0)
		{
			auto recItem = static_cast<tsl::AudioBuffer<recSampleFormat>*>(_STATE->pool.acquire(sizeof(tsl::AudioBuffer<recSampleFormat>)));
			if (!recItem) {
				LOGE("%d frames dropped in recording", totalFrames);
				break;
			}

			int chunkFrames = std::min(totalFrames, tsl::audioBufferDefaultSize / MAX_CHANNELS);
			recItem->channels = MAX_CHANNELS;
			recItem->frames = chunkFrames;
			int idx = 0;

			for (int f = 0; f < chunkFrames; ++f)
			{
				// Calculate the sample once for this frame
				//float sampleValue = 0.9f * std::sin(TWOPI_P * phase * 440.0f);
				//int16_t sample16 = tsl::to16Bit(sampleValue);

				for (int ch = 0; ch < MAX_CHANNELS; ++ch)
				{
					recItem->data[idx++] = tsl::to16Bit(_STATE->inputBuf[ch][inputPos]);
				}
				//phase += _STATE->onedsr;
				//while (phase > 1.0)phase -= 1.0;

				inputPos = (inputPos + 1) & inputMask;
			}

			if (!_DATA->recorder.queue.try_push(recItem)) {
				LOGE("%d frames dropped in recording (queue full)", totalFrames);
				_STATE->pool.release(recItem);
				break;
			}
			else {
				_STATE->waitNotify.wake_thread(_DATA->recorder.slot);
				totalFrames -= chunkFrames;
			}
		}

	}
#endif

	_STATE->currentBufSize = numFrames;
	auto buf = _DATA->track1.grain_buffer[0];
	/*
		auto inBuf0 = _DATA->inBuf[0], inBuf1 = _DATA->inBuf[1];


	for (int smpl = 0; smpl<numFrames; smpl++) {
			buf[smpl * 2] = inBuf0[smpl];
			buf[smpl * 2 + 1] = inBuf1[smpl];
		}
		*/
#else

#include <views.h>

#if defined IS_MULTITHREADED && defined __ANDROID__

// Per-block decay of the concealed signal. 0.5 reaches -24 dB after four blocks
// (~21 ms at 256 frames / 48 kHz), so a run of drops fades out well before the
// ping-pong below could start sounding periodic.
constexpr float kConcealDecay = 0.5f;

// Fade-in owed to the first good block after a drop: ~1.3 ms at 48 kHz. Long
// enough to remove the step back up to full level, short enough not to be heard
// as a fade on the real audio it is applied to.
constexpr int32_t kConcealFadeSamples = 64;

// Fill a block the coordinator failed to deliver in time.
//
// Plays back the last mix that actually reached the device, ping-ponged in
// direction and under a decaying gain. Reversing on alternate drops is the
// whole trick: a reversed pass starts on the very sample the forward pass ended
// on, and vice versa, so every join is continuous in VALUE. Only the slope
// breaks, which is a far quieter artifact than an amplitude step -- and cutting
// to silence has two such steps at full amplitude, one into the hole and one
// back out of it.
//
// This does not reconstruct the missing audio; nothing can. It turns two hard
// artifacts into one soft one, and stops the hole being a hole.
static inline void concealBlock(tsl::AppState* _appState, sampleTSL* buf, int32_t numFrames) {
	const int32_t have = _DATA->concealFrames;
	if (have <= 0 || _DATA->concealBuf == nullptr) {
		// Nothing has been delivered yet -- a drop inside the first blocks after
		// start. Silence is genuinely all there is to play.
		memset(buf, 0, sizeof(sampleTSL) * numFrames * 2);
		return;
	}

	const float g0 = _DATA->concealGain;
	const float g1 = g0 * kConcealDecay;
	const bool rev = _DATA->concealReverse;
	const sampleTSL* src = _DATA->concealBuf;
	const float dg = (g1 - g0) / (float)numFrames;

	float g = g0;
	for (int32_t i = 0; i < numFrames; i++, g += dg) {
		// Ramping within the block as well as across them means even a single
		// isolated drop fades instead of holding flat and stepping at its end.
		// Beyond what we stored, hold the last frame: numFrames > have only
		// happens if the device grew the block mid-stream, and the gain is on
		// its way down regardless.
		const int32_t j = i < have ? i : have - 1;
		const int32_t s = (rev ? (have - 1 - j) : j) * 2;
		buf[i * 2]     = src[s]     * g;
		buf[i * 2 + 1] = src[s + 1] * g;
	}

	_DATA->concealGain = g1;
	_DATA->concealReverse = !rev;
	// The step back up to full level on recovery is the one seam concealment
	// cannot soften from this side, so charge it to the next good block.
	_DATA->concealFadeIn = kConcealFadeSamples;
}

// How many blocks after a stream start count as priming -- see the wait in
// synthFunc. Three is what the measurement showed; eight is that with room for
// a device that primes a deeper buffer, and is still far short of anything the
// listener could be hearing yet.
constexpr int32_t kPrimingBlocks = 8;

// Wait for the coordinator to finish the pass it is running, up to one block
// period. True means it finished and its four tracks are in playBufQueue, so
// the caller can carry on as if it had never been late.
static inline bool awaitCoordinator(tsl::AppState* _appState, int32_t numFrames) {
	const int64_t deadline = tsl::time::nanosecondsSinceEpoch()
		+ int64_t((double(numFrames) / _STATE->sr) * tsl::time::nanosPerSecond);
	while (_DATA->open.load(std::memory_order_acquire)) {
		if (tsl::time::nanosecondsSinceEpoch() >= deadline)
			return false;
		// Sleep rather than spin. The pass being waited on is running on
		// SynthThread and up to eight channel threads, and this is the highest
		// priority thread in the process -- spinning here takes a core away
		// from precisely the work that has to finish for the wait to end.
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}
	return true;
}

#endif

void PlayerBase::synthFunc(tsl::AppState * _appState, sampleTSL * buf, int32_t numFrames) {
	// Android does not strictly need this -- Player::_stop() calls
	// stream->close(), which blocks until this callback has returned and
	// guarantees no further ones -- but counting here means record_loop's drain
	// rests on our own invariant instead of on Oboe's close() semantics, and it
	// costs one uncontended atomic per block. The early return below is exactly
	// why it is RAII.
	DATA::AudioPassGuard pass(_DATA);
	// Priming window.
	//
	// At stream start the device does not ask for blocks at the block rate: it
	// asks for several back-to-back to fill the buffer it is about to play out
	// of. Measured on a 23100RN82L, 1920-frame callback, 8192-frame buffer:
	// callbacks 0, 1 and 2 arrive within 7 ms of each other, and only from
	// callback 3 does the ~40 ms cadence start.
	//
	// A pipeline that produces one block per callback cannot serve that. The
	// pass dispatched at the end of callback 0 is still running when 1 and 2
	// arrive, so both find `open` set and both drop -- on 10 launches out of 10,
	// which is the "one XRUN on every stream start" this fixes. Making the pass
	// faster does not help and was measured not to: a 7.8 ms pass still loses to
	// a 1 ms gap.
	//
	// So inside that window, wait for the pass instead of dropping it. The wait
	// is free there by construction -- the device only asks back-to-back because
	// it is filling a buffer it has not started playing from yet, so the block
	// lands late into a buffer with room for it instead of on time and silent.
	//
	// Deliberately NOT the steady-state behaviour. Once the cadence is real,
	// late is late: the buffer is being drained as fast as it is filled, waiting
	// only moves the hole to the next block, and dropping the pass whole and
	// concealing is the right answer. That is the path below.
	const int32_t sinceStart = _DATA->startBlocks.load(std::memory_order_relaxed);
	const bool priming = sinceStart < kPrimingBlocks;
	if (priming)
		_DATA->startBlocks.store(sinceStart + 1, std::memory_order_relaxed);

	bool late = _DATA->open.load();
	if (late && priming)
		late = !awaitCoordinator(_appState, numFrames);

	if (late) {
		// The coordinator did not finish this block in time. Reported through the
		// info panel's own XRUN display, which already exists for exactly this:
		// it shows "XRUN #n" and reverts to the normal panel a second later.
		//
		// A transient wants a transient display. The first attempt appended a
		// running "DROP: n" to the LOAD line instead, which was wrong twice --
		// the suffix overflowed the panel's fixed layout and was never visible
		// on device, and the non-zero count turned the LOAD figure red for the
		// rest of the session, so an idle engine at 2% read as saturated.
		if (auto info = _DATA->views.infopanel) {
			info->xruncount.fetch_add(1, std::memory_order_relaxed);
			info->setRenderFunc(tsl::graphics::InfoPanel::RenderXRun);
		}

		concealBlock(_appState, buf, numFrames);

		// Still drained, and deliberately. playBufQueue is depth 4 -- exactly one
		// pass of 4 tracks -- so there is nowhere to hold a finished pass while
		// the next one is produced, and consuming the partial set now would leave
		// the leftovers to corrupt the FOLLOWING block too. Dropping the pass
		// whole is the only option the queue depth allows.
		while (auto track_ = _DATA->playBufQueue.try_pop()) {}
		_STATE->peak[0] = 0.00001;
		_STATE->peak[1] = 0.00001;
		return;
	}
#endif

#ifdef IS_MULTITHREADED
	const bool isMultiThreaded = _DATA->isMultithreaded.load();
	if (!isMultiThreaded) {
#ifdef __ANDROID__
		// This runs on Oboe's callback thread, which player.cpp already handed to
		// setPerformanceHintEnabled(true). That thread therefore has a session
		// without owning an AdpfHint we could ask, and Oboe's call is a no-op
		// where the platform lacks ADPF -- so the platform probe is exactly the
		// answer to "did this thread actually get the treatment". True: skip the
		// spin, the governor is already being told. False: keep it, it is the
		// only help a pre-API-33 device gets.
		synthFuncIntern(_STATE, false, tsl::AdpfHint::platformHasAdpf());
#else
		synthFuncIntern(_STATE, false);
#endif
	}
#if !defined __ANDROID__
	else
		synthFuncIntern(_STATE, true);
#endif
#else if !defined __ANDROID__
	synthFuncIntern(_STATE, false);
#endif
#if defined DOES_INPUT_RESAMPLING
	_STATE->inputPos = (_STATE->inputPos + numFrames) & inputMask;
#endif
	auto end = buf + numFrames * 2;

	memset(buf, 0, sizeof(sampleTSL) * numFrames * 2);

	// float postgain, *smoothedgain;
	const auto smoothCoeff = _STATE->smoothCoeff;
	while (auto track_ = _DATA->playBufQueue.try_pop()) {
		auto track = track_.value();
		auto ll = track->out_buf[0];
		auto rr = track->out_buf[1];
		auto envl = track->envf_buffer[0];
		auto envr = track->envf_buffer[1];
		auto tmp = buf;
		auto& pgsmoothed = track->postgainsmoothed;
		auto postgain = dbToLinear60(_STATE->params[track->index][POSTGAIN].load());
		while (tmp < end) {
			*(tmp++) += static_cast<sampleTSL>((*envl++ = *(ll++)) * pgsmoothed);
			*(tmp++) += static_cast<sampleTSL>((*envr++ = *(rr++)) * pgsmoothed);
			pgsmoothed += smoothCoeff * (postgain - pgsmoothed);
		}
	}

#if defined IS_MULTITHREADED && defined __ANDROID__
	// Pay off the fade-in owed by a preceding drop, before the recording capture
	// below so the file matches what is heard. Real audio otherwise returns at
	// full level from whatever the concealment decayed to, and that step is a
	// click in its own right.
	if (_DATA->concealFadeIn > 0) {
		const int32_t n = _DATA->concealFadeIn < numFrames ? _DATA->concealFadeIn : numFrames;
		for (int32_t i = 0; i < n; i++) {
			const float g = (float)i / (float)n;
			buf[i * 2]     *= g;
			buf[i * 2 + 1] *= g;
		}
		_DATA->concealFadeIn = 0;
	}
	// A block is about to be delivered, so the next drop starts from full gain
	// and plays backwards -- backwards because that begins on the sample this
	// block ends on.
	_DATA->concealGain = 1.f;
	_DATA->concealReverse = true;
#endif

	// snapShot.queue is now drained by snapshotDrainThread (setup.cpp) every 50ms
	// to avoid calling notify_one() (syscall) from the real-time audio thread.

	if (_STATE->player.isrecording.load(std::memory_order_acquire)) {

		int totalSamples = numFrames * MAX_CHANNELS;
		int srcIndex = 0;

		auto tmp = buf;


		while (totalSamples > 0) {
			auto recItem = static_cast<tsl::AudioBuffer<sampleTSL> *>(_STATE->pool.acquire(
				sizeof(tsl::AudioBuffer<sampleTSL>)));
			if (!recItem) {
				//LOGE("Pool exhausted");
				break;
			}

			int chunkSamples = std::min(totalSamples, tsl::audioBufferDefaultSize);
			recItem->frames = chunkSamples / MAX_CHANNELS;
			recItem->channels = MAX_CHANNELS;

			int idx = 0;
			int frame = srcIndex / MAX_CHANNELS;
			for (int f = 0; f < recItem->frames; f++, frame++)
				for (int ch = 0; ch < MAX_CHANNELS; ch++)
					recItem->data[idx++] = *(tmp++);

			if (!_STATE->player.recQueue.try_push(recItem)) {
				LOGE("Queue full");
				_STATE->pool.release(recItem);
			}
			else {
				_STATE->waitNotify.wake_thread(_STATE->player.slot);
			}

			srcIndex += chunkSamples;
			totalSamples -= chunkSamples;
		}
	}


	sampleTSL peakright = 0.00001;
	sampleTSL peakleft = 0.00001;


#if defined IS_MULTITHREADED && defined __ANDROID__
	if (isMultiThreaded) {
		_STATE->data->open.store(true);
		_STATE->data->sem.release();
	}
#endif
#if defined __ANDROID__
	auto tmp = buf;
	while (tmp < end) {
		sampleTSL* l, * r;
		sampleTSL templ = ABS(*(l = tmp++));
		sampleTSL tempr = ABS(*(r = tmp++));
		peakleft = templ > peakleft ? templ : peakleft;
		peakright = tempr > peakright ? tempr : peakright;
		if (*l > 0.99)
			*l = .99;
		else if (*l < -.99)
			*l = -.99;
		if (*r > .99)
			*r = .99;
		else if (*r < -.99)
			*r = -.99;
	}
	_STATE->peak[0] = peakleft;
	_STATE->peak[1] = peakright;

#if defined IS_MULTITHREADED && defined __ANDROID__
	// Saved AFTER the clamp, so what a later drop replays is byte-for-byte what
	// the device actually received -- concealment that replayed unclamped audio
	// would be louder than the material it is standing in for. Guarded against a
	// callback larger than the ceiling Java promised; synthFunc reports that case
	// separately, and truncating here is better than running off the arena.
	if (_DATA->concealBuf != nullptr) {
		const int32_t n = numFrames < _STATE->maxBufSize ? numFrames : _STATE->maxBufSize;
		memcpy(_DATA->concealBuf, buf, sizeof(sampleTSL) * n * 2);
		_DATA->concealFrames = n;
	}
#endif
}

#else

	int framesWritten = 0;

	// Clip and meter the whole engine block up front. It used to happen as rsOut pulled
	// frames, which meant the peak meter only saw the frames rsOut happened to consume;
	// now every rendered frame is metered exactly once, including any that carry over.
	{
		auto p = buf;
		for (int i = 0; i < numFrames; i++) {
			auto l = p[0], r = p[1];
			auto templ = ABS(l);
			auto tempr = ABS(r);
			peakleft = templ > peakleft ? templ : peakleft;
			peakright = tempr > peakright ? tempr : peakright;
			if (l > 0.99)
				l = .99;
			else if (l < -.99)
				l = -.99;
			if (r > .99)
				r = .99;
			else if (r < -.99)
				r = -.99;
			*p++ = l;
			*p++ = r;
		}
	}

	// rsOut needs numFrames +- 1 engine frames to emit `frames` host frames. Take the
	// leftovers from last block first, then this block's, and keep whatever is unused.
#if defined DOES_INPUT_RESAMPLING
	auto& carry = _STATE->rsOutCarry;
	int carryCount = _STATE->rsOutCarryCount;
#else
	MYFLOAT carry[1][2]{};
	int carryCount = 0;
#endif
	int carryPos = 0, srcPos = 0;

	while (framesWritten < frames) {
		while (rsOut.isWriteNeeded()) {
			MYFLOAT midOut[2];
			if (carryPos < carryCount) {
				midOut[0] = carry[carryPos][0];
				midOut[1] = carry[carryPos][1];
				carryPos++;
			}
			else if (srcPos < numFrames) {
				midOut[0] = buf[srcPos * 2];
				midOut[1] = buf[srcPos * 2 + 1];
				srcPos++;
			}
			else {
				// Cannot happen: carry occupancy measured 1..3 over every host rate and
				// block size. Emitting silence beats reading past the block.
				midOut[0] = midOut[1] = 0;
			}
			rsOut.writeNextFrame(midOut);
		}
		MYFLOAT tmp[2];
		rsOut.readNextFrame(tmp);
		if (outChans == 1)out[0][framesWritten] = (tmp[0] + tmp[1]) * .5;
		else {
			out[0][framesWritten] = tmp[0];
			out[1][framesWritten] = tmp[1];
		}
		framesWritten++;
	}

#if defined DOES_INPUT_RESAMPLING
	// Compact whatever rsOut did not take into the front of the carry, oldest first.
	{
		int nc = 0;
		for (int i = carryPos; i < carryCount && nc < tsl::AppState::rsOutCarryMax; i++, nc++) {
			carry[nc][0] = carry[i][0];
			carry[nc][1] = carry[i][1];
		}
		for (int i = srcPos; i < numFrames && nc < tsl::AppState::rsOutCarryMax; i++, nc++) {
			carry[nc][0] = buf[i * 2];
			carry[nc][1] = buf[i * 2 + 1];
		}
		_STATE->rsOutCarryCount = nc;
	}
#endif

	_STATE->peak[0] = peakleft;
	_STATE->peak[1] = peakright;

}
#endif


struct trackinfo_t {
	double offset;
	double dir;
	MYFLOAT pregain;
};

int32_t record_loop(TRACK* track, const std::shared_ptr<tsl::Player::RecordingContext>& w) {
	auto _appState = track->_appState;
	bool active = false;
	for (int i = 0; i < 4; i++) {
		if (_STATE->params[i][POWERTRACK].load()) {
			active = true;
			break;
		}
	}
	if (!active) {
		showToast(_STATE, "Record Loop: No tracks active.");
		return -1;
	}

	const bool nogranulation = track->fxpower[SPACE_LOOPER].load();
	TRACK* source = nogranulation ? track
		: _DATA->tracks[(int)_STATE->params[track->index][DISTRSOURCE].load()];
	long offset = 0;

	auto playbackspeed =
		std::max(0., LOG2NORMALF(_STATE->params[source->index][SPEED].load()) - SPEED_OFFSET);
	if (nogranulation) {
		playbackspeed = std::clamp(playbackspeed, .1, 10.);
	}

	std::string message = track->name;
	message.append(" Record Loop: ");

	std::shared_ptr<tsl::Recording> rec[4]{};
	for (auto ttt : _DATA->tracks)rec[ttt->index] = ttt->filebuffer.load();
	auto state = rec[source->index] != nullptr ? rec[source->index]->state.load() : nullptr;
	auto loopSamples = state != nullptr ? state->off_stop.load() - state->off_start.load() : 0;

	int32_t diff = playbackspeed != 0 ?
		static_cast<int>(loopSamples /
			abs(playbackspeed)) *
		(state ? (state->bounceType.load() == NO_BOUNCE ? 1 : 2) : 1) : static_cast<int>(_STATE->sr) *
		7 *
		60;
	if (diff == 0) {
		message.append("Empty Track.");
		showToast(_STATE, message.data());
		return -1;
	}
	double minutes = diff / _STATE->sr / 60.;
	if (minutes > (w->wrapper.get_mode() != tsl::FileWrapper::INTERNAL_BUFFER ? 60. : 7.)) {
		message.append(w->wrapper.get_mode() != tsl::FileWrapper::INTERNAL_BUFFER
			? "Loop exceeds 60 minutes." : "Loop exceeds 7 minutes.");
		showToast(_STATE, message.data());
	}

	if (playbackspeed == 0) {
		std::string tmp = message;
		tmp.append("Playbackspeed is zero. Recording 10 minutes.");
		showToast(_STATE, tmp.data());
	}

	
	bool isplaying{};
	{
		std::lock_guard lock(_STATE->player);
		isplaying = _STATE->player._isplaying.load();
		if (isplaying){
			for (auto t : _DATA->tracks) {
				t->gainTask.setTarget(tsl::gaintask::GainDown, false);
			}
			_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);

			_STATE->player.stop();
		}
	}

	// Drain every in-flight synth pass before touching currentBufSize, ringPos
	// or the ringbuffers below, and before driving synthFuncIntern from this
	// thread.
	//
	// This replaced workerLatch.wait(), which was the wrong barrier twice over.
	// The latch reaches zero in the MIDDLE of a pass -- the caller still has
	// ringPos, _DATA->offset and the entire mixing tail to run -- so a callback
	// could still be writing state we are about to reset. And on Android it does
	// not cover SynthThread at all: the last callback before the stop released
	// the coordinator (open.store(true); sem.release()) and stream->close()
	// knows nothing about that thread, so the coordinator can be anywhere inside
	// synthFuncIntern when stop() returns. It was also under IS_MULTITHREADED,
	// though a single-threaded pass races exactly the same way.
	//
	// The fence is the other half of AudioPassGuard's -- see grainstorm.h. It
	// must sit between Player::stop()'s _isplaying store and the load below, or
	// a callback that has just passed the _isplaying check but not yet raised
	// the counter is invisible to us and runs a full pass alongside record_loop.
	//
	// Terminates because no new RENDERING pass can start once we get here:
	// desktop/plugin has _isplaying false (read under the guard in
	// IPlugEffect::ProcessBlock), and Android has the stream closed, so nothing
	// sets open again either. The guard there brackets the whole of ProcessBlock,
	// so a non-playing callback still blips the count while it copies its
	// passthrough buffer -- sub-microsecond against a multi-millisecond block, so
	// a 1 ms poll lands on it essentially never, and if it does the only cost is
	// one more poll. It cannot spin: that branch touches nothing we are about to
	// change.
	std::atomic_thread_fence(std::memory_order_seq_cst);
	while (_DATA->audioPassesInFlight.load(std::memory_order_acquire) > 0
#if defined IS_MULTITHREADED && defined __ANDROID__
		|| _DATA->open.load(std::memory_order_acquire)
#endif
		)
		_STATE->waitNotify.sleep_for(1);


	while (auto t = _DATA->playBufQueue.try_pop()) {}
	tsl::StackVector<TRACK*, 4> tracks;
	setupTracks(_appState, tracks);
	if (tracks.empty()) {
		showToast(_STATE, "Record Loop: No tracks active.");
		if (isplaying)
			_STATE->player.play();
		return -1;
	}
	int32_t returnvalue = 0;

	
	

	_DATA->inputdisabled.store(true, std::memory_order_release);
	const int32_t bufsize = _STATE->maxBufSize;
	auto bufsizetmp = _STATE->currentBufSize;
	_STATE->currentBufSize = bufsize;
	int32_t samples_written = 0;
	int32_t samples_to_process = diff;

	auto grainsize = track->lockerz->fxpower[SPACE_CROSS_MAIN].load() ||
		track->fxpower[SPACE_CROSS_MAIN].load() ||
		track->fxpower[SPACE_PV_MAIN].load()
		? _STATE->params[track->index][FFT_SIZE].load()
		: _STATE->params[track->index][GRAINSIZE].load();
	int32_t samplesrest = nogranulation ? 0 : (int)(grainsize);


	std::vector<std::pair<TRACK*, trackinfo_t>> saved;
	std::vector<std::pair<LFO*, float>> lfosaved;

	for (auto t : tracks) {
		for (auto lfo : t->lfos)
			lfosaved.emplace_back(lfo, lfo->phs());
		auto state = rec[source->index] != nullptr ? rec[source->index]->state.load() : nullptr;
		auto offs = state != nullptr ? state->offset.load() : 0.;
		auto playDir = state != nullptr ? state->playbackDir.load() : 1.;
		saved.emplace_back(t, trackinfo_t{ offs, playDir,
										  _STATE->params[t->index][PREGAIN].load() });
		std::memset(t->ringbuffer[0], 0, sizeof(MYFLOAT) * _STATE->ringSize);
		std::memset(t->ringbuffer[1], 0, sizeof(MYFLOAT) * _STATE->ringSize);
		t->gainTask.setTarget(tsl::gaintask::GainUp, false);
	}

	//LOGE("Count %d", count);
	auto tindex = track->index;
	if (playbackspeed > 0) {
		tsl::parameters::Event e;
		if ISNEG(state ? state->playbackDir.load() : 1.) {
			e.setup(_STATE, tindex, STEPFORW);
			e.applyFromExt(_STATE, tsl::parameters::FromHistory);
		}
		else {
			e.setup(_STATE, tindex, STEPBACK);
			e.applyFromExt(_STATE, tsl::parameters::FromHistory);
		}
	}
	auto info = _DATA->views.infopanel;

	if (info) {
		info->progress = 0.0;
		info->dec_offset = 0.0;
		info->bytes = 0;
		info->text.store(track->name);
		info->text2.store(" RENDERING");
		info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
	}
	//_DATA->queue_render->add(_DATA->queue_render, button, button->name, nullptr, button->prio, true);
#ifdef __ANDROID__
	_DATA->stabilize = false;
#endif

	_STATE->params[track->index][RECLOOPButton].store(1.0);
	if (tindex == _STATE->active_track.load() && GASMAIN == SPACE_WAVEFORM &&
		_STATE->dofastrender.load()) {
		_STATE->parameters[RECLOOPButton].view->redraw();
	}
	_STATE->ringPos = 0;
	std::atomic_bool abort = false;
	tsl::graphics::BackView bv(_STATE, [&abort]() { abort = true; });

	auto writebuf = _STATE->pool.acquire<unsigned char>(bufsize * 2 * sizeof(sampleTSL));
	if (writebuf == nullptr) {
		message.append("Not enough memory to record loop.");
		showToast(_STATE, message.data());
		returnvalue = -1;
	}
	else {
		for (int32_t i = 0; i < samples_to_process + samplesrest; i += bufsize) {
			if (abort.load() || _STATE->destroyRequested.load())
				break;
			if (i >= samples_to_process) {
				for (auto t : tracks) {
					_STATE->params[t->index][PREGAIN].store(_STATE->parameters[PREGAIN].min);
				}
			}
#ifdef IS_MULTITHREADED
			synthFuncIntern(_STATE, true);
#else
			synthFuncIntern(_STATE, false);
#endif
			// float postgain, *smoothedgain;
			std::memset(writebuf, 0, sizeof(sampleTSL) * bufsize * 2);

			while (auto track1_ = _DATA->playBufQueue.try_pop()) {
				auto track1 = track1_.value();
				auto ll = track1->out_buf[0];
				auto rr = track1->out_buf[1];
				auto envl = track1->envf_buffer[0];
				auto envr = track1->envf_buffer[1];
				auto& pgsmoothed = track1->postgainsmoothed;
				auto postgain = dbToLinear60(_STATE->params[track1->index][POSTGAIN].load());
				auto tmp = (sampleTSL*)writebuf;
				auto end = tmp + bufsize * 2;
				while (tmp < end) {
					*(tmp++) += static_cast<sampleTSL>((*envl++ = *(ll++)) * pgsmoothed);
					*(tmp++) += static_cast<sampleTSL>((*envr++ = *(rr++)) * pgsmoothed);
					pgsmoothed += _STATE->smoothCoeff * (postgain - pgsmoothed);
				}
			}


			MYFLOAT peakright = 0.00001;
			MYFLOAT peakleft = 0.00001;
			auto tmp = (sampleTSL*)writebuf;
			auto end = tmp + bufsize * 2;
			int z = 0;
			while (tmp < end) {
				sampleTSL* l, * r;
				sampleTSL templ = ABS(*(l = tmp++));
				sampleTSL tempr = ABS(*(r = tmp++));
				peakleft = templ > peakleft ? templ : peakleft;
				peakright = tempr > peakright ? tempr : peakright;

				if (*l > 0.99)
					*l = .99;
				else if (*l < -.99)
					*l = -.99;
				if (*r > .99)
					*r = .99;
				else if (*r < -.99)
					*r = -.99;
			}


			_STATE->peak[0] = peakleft;
			_STATE->peak[1] = peakright;

			if (info) {
				info->dec_offset.fetch_add(static_cast<long>(bufsize));
				info->bytes = w->wrapper.tell();
			}
			offset += bufsize;
			if (nogranulation && offset > diff) {
				long rest = bufsize - (offset % diff);
				offset -= bufsize;
				offset += rest;
				LOGD("No granulation aborting. Processed: %ld/ %ld", offset, diff);
				if (rest &&
					w->wrapper.write(writebuf, sizeof(sampleTSL), rest * 2) != rest * 2) {
					message.append("Write error. Disk full?");
					showToast(_STATE, message.data());
					returnvalue = -1;
					break;
				}
				break;
			}
			else if (w->wrapper.write(writebuf, sizeof(sampleTSL), bufsize * 2) !=
				bufsize * 2) {
				message.append("Write error. Disk full?");
				showToast(_STATE, message.data());
				returnvalue = -1;
				break;
			}

			samples_written += bufsize;
			if (info) info->progress = i / (double)(samples_to_process + samplesrest);
			//LOGE("%d", (int) (samples_written / samples_per_tick));
		}
		_STATE->pool.release(writebuf);
	}

	_STATE->currentBufSize = bufsizetmp;

	if (_DATA->abort_loop_record) {
		_DATA->abort_loop_record = false;
		returnvalue = 0;
	}

	for (auto t : saved) {

		if (rec[t.first->index] != nullptr) {
			auto s = rec[t.first->index]->state.load();
			if (s) {
				s->offset.store(t.second.offset);
				s->playbackDir.store(t.second.dir);
			}


		}
		_STATE->params[t.first->index][PREGAIN].store(t.second.pregain);
	}

	for (auto pp : lfosaved)
		pp.first->store(LFOPHS, pp.second);


	bv.delCB();

	if (auto infofin = _DATA->views.infopanel) infofin->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);

	_STATE->ringPos = 0;

	for (auto t : _DATA->tracks) {
		t->gainTask.setTarget(tsl::gaintask::GainUp, false);
		std::memset(t->ringbuffer[0], 0, sizeof(MYFLOAT) * _STATE->ringSize);
		std::memset(t->ringbuffer[1], 0, sizeof(MYFLOAT) * _STATE->ringSize);
		//for(int32_t chan =0;chan<_STATE->channels;chan++)
		//  t->mutexes[chan].unlock();
	}
#ifdef __ANDROID__
	_DATA->stabilize = true;
#endif
	_STATE->params[track->index][RECLOOPButton].store(0.0);
	if (_STATE->active_track.load() == tindex && GASMAIN == SPACE_WAVEFORM &&
		_STATE->dofastrender.load()) {
		_STATE->parameters[RECLOOPButton].view->redraw();
	}
	_DATA->inputdisabled.store(false, std::memory_order_release);

	if (isplaying) _STATE->player.play();


	return returnvalue;
}

#ifdef __ANDROID__

#include <oboe/Oboe.h>

int32_t java_record_loop(JNIEnv* env, jclass obj, jint fd, jlong _track) {
	FILE* f = nullptr;
	f = fdopen(fd, "wb");
	if (f == nullptr) {
		if (fd)close(fd);
		return -1;
	}
	auto w = tsl::Player::setupRecording(__STATE, f, "Loop Recording ");
	if (w == nullptr) {
		if (fd)close(fd);
		return -1;
	}
	if (record_loop((TRACK*)_track, w) == 0 && w->wrapper.size() != 0) {
		w->wrapper.close();
		w->finished.store(true, std::memory_order_release);
		return 0;
	}
	else {
		w->wrapper.close();
		w->finished.store(true, std::memory_order_release);
		return -1;
	}
}

#endif
