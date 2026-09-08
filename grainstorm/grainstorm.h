#pragma once
#ifndef GRAINSTORM_H
#define GRAINSTORM_H

#include "defines.h"
#include "types.h"
#include "History.h"
#include "Recorder.h"
#include "Midi.h"
#include "callbacks_fxpower.h"
#include "tools/queuetsl.h"
#include "tools.h"
#include "track.h"
#include "ffttools.h"
#include "player.h"
#include "synth.h"
#include "frequencyresponce.h"
#include "tools/inplace_function.h"
#include "tools/RingBufferQueue.h"
#include <FloatingScrollView.h>
#include "Presets/preset.h"


using ToAudioThreadQueue = tsl::RingBufferMPSCQueue<stdext::inplace_function<void(), 64>, 64>;
using PlaybufQueue = tsl::RingBufferMPSCQueue<TRACK*, 4>;

#if defined IS_MULTITHREADED
#include "synth.h"
#endif

#ifdef LICENSE_CHECK_ENABLED
#include <Licensechecker.h>
#endif

class TrackSettings;
class ImportMapping;
struct MIDIHEADER2;
class ImportPreset;
class Syncing;
class InputRouting;
struct controlitem_t;
class SaveDummy;
struct PlayBufElement {
	MYFLOAT* l, * r;
};


namespace tsl {

    namespace graphics {
		template<typename T1, typename T2>
		class RecyclerView;
        class BackView;
		class FXView;
		class EffectOrderView;
	}
}
namespace tsl::graphics {
	class ButtonSpaceSwitch;
	class InfoPanel;
	class ButtonEnvelopeSwitch;
}

struct ViewS
{
	tsl::graphics::View* active_spaces_array[NUM_PARAMSPACES];
	tsl::graphics::View* spaces_fx[NUM_PARAMSPACES];
	tsl::graphics::ButtonSpaceSwitch* active_spaces_buttons[NUM_APPSPACE_CATEGORIES];
	tsl::graphics::View* toppanel_main;
	tsl::graphics::View* divider_bottom;
	tsl::graphics::View* meter;
	tsl::graphics::InfoPanel* infopanel;
	tsl::graphics::View* sidebar;
	tsl::graphics::View* controlpanel;
	tsl::graphics::View* toppanel_track;
	tsl::graphics::View* button;
	tsl::graphics::ButtonEnvelopeSwitch* button_analysis;
	tsl::graphics::ButtonEnvelopeSwitch* button_synthesis;
	tsl::graphics::View* divider_envelopes;
	tsl::graphics::View* divider_left2;
	tsl::graphics::View* divider_cpan_bottom;
	tsl::graphics::View* divider_toppanel;
	tsl::graphics::View* space_lfo1;
	tsl::graphics::View* IRSTComp;
	tsl::graphics::View* IRMonoComp;
	tsl::graphics::View* space_grainenv1, * space_grainenv2;
	tsl::graphics::View* mdelay_controlpanel2;
	tsl::graphics::View* env_win_lfo;
	tsl::graphics::View* tv_cross_dest;
	tsl::graphics::View* midilearnbutton;
	std::shared_ptr<tsl::graphics::View> decoderView;
	std::unique_ptr<tsl::graphics::View>recorderView;
	tsl::graphics::View* lfo_edit_root;
	tsl::graphics::View* lfo_title;
	tsl::graphics::View* grainfilterenv;
	tsl::graphics::View* lfo_rand_root;
	tsl::graphics::View* lfocontrolpanel;
	tsl::graphics::View* lfo_sync_root = nullptr;
};





#if defined(PLUGIN_MODE) || defined(OS_IOS)
struct HostTimeSnapshot {
	bool   running = false;
	bool   wasRunning = false;
	double bpm = 120.0;
	double ppqNow = 0.0;
	bool   loopEnabled = false;
	double loopStartPPQ = 0.0;
	double loopEndPPQ = 0.0;
	double beatsPerBar = 4;
	double lastPpqPos = 0.0;
	double triggerPPQ = 0.0;
	inline bool DetectTransportDiscontinuity()
	{
		// Transport start
		if (running && !wasRunning)
			return true;

		// Transport stop
		if (!running && wasRunning)
			return true;

		// Loop wrap or backwards seek
		if (ppqNow < lastPpqPos)
			return true;

		// Large forward jump (seek)
		const double delta = ppqNow - lastPpqPos;
		if (delta > 4.0) // > 1 bar @ 4/4
			return true;

		return false;
	}

};
#endif

struct DATA
{	
	DATA(tsl::AppState* appState):
		track1{ this, appState, "TRACK1", 0, "TRACK2" },
		track2{ this, appState, "TRACK2", 1, "TRACK3" },
		track3{ this, appState, "TRACK3", 2, "TRACK4" },
		track4{ this, appState, "TRACK4", 3, "TRACK1" },
		snapShot{ appState }
#if defined IS_MULTITHREADED
#if defined __ANDROID__
		, synthThread{ appState }
#endif
		,channelThreads{
			tsl::synth::ChannelThread{ &track1, 0 },
			tsl::synth::ChannelThread{ &track1, 1 },
			tsl::synth::ChannelThread{ &track2, 0 },
			tsl::synth::ChannelThread{ &track2, 1 },
			tsl::synth::ChannelThread{ &track3, 0 },
			tsl::synth::ChannelThread{ &track3, 1 },
			tsl::synth::ChannelThread{ &track4, 0 },
			tsl::synth::ChannelThread{ &track4, 1 }
	}
#endif
	{		};
	~DATA();
	tsl::AlignedVector<unsigned char> buf{};
	TRACK track1;
	TRACK track2;
	TRACK track3;
	TRACK track4;
	std::array<TRACK*, 4> tracks{ &track1, &track2, &track3, &track4 };
	callback_fx_power callbacks_fx_power[NUM_PARAMSPACES]{};
	ToAudioThreadQueue toAudioThreadQueue;
	std::atomic<bool> toAudioThreadQueueStop{};
	PlaybufQueue playBufQueue{};	
	tsl::parameters::Snapshot snapShot;
	int32_t maxgrainsize{};

	// Synth passes currently in flight, counted on every thread that can run
	// one. record_loop drains this to zero before it retunes currentBufSize and
	// ringPos and starts driving synthFuncIntern from the snapshot thread
	// itself; without it, an audio callback that is mid-pass keeps writing the
	// state record_loop just changed.
	//
	// Deliberately NOT under IS_MULTITHREADED: a single-threaded pass is just as
	// much in flight as a dispatched one.
	std::atomic<int32_t> audioPassesInFlight{};

	// Raised BEFORE the caller reads _isplaying, dropped when the pass is fully
	// over -- not when the workers finish, which is the middle of the pass.
	//
	// The pairing with record_loop's drain is a Dekker handshake, so both sides
	// need a seq_cst fence between their store and their load or both can miss:
	// we raise the counter then read _isplaying, record_loop clears _isplaying
	// then reads the counter. At least one must see the other, so a pass can
	// neither start unnoticed nor be waited on forever. Player::stop() stores
	// _isplaying with release rather than seq_cst, which is why the barrier is a
	// fence here rather than an ordering on the flag itself.
	//
	// RAII so every early return in the caller still drops the count.
	struct AudioPassGuard {
		explicit AudioPassGuard(DATA* d) : _d(d) {
			_d->audioPassesInFlight.fetch_add(1, std::memory_order_seq_cst);
			std::atomic_thread_fence(std::memory_order_seq_cst);
		}
		~AudioPassGuard() {
			// Release: record_loop acquires on the drain, so everything this
			// pass wrote is visible to it before it touches the same state.
			_d->audioPassesInFlight.fetch_sub(1, std::memory_order_release);
		}
		AudioPassGuard(const AudioPassGuard&) = delete;
		AudioPassGuard& operator=(const AudioPassGuard&) = delete;
	private:
		DATA* _d;
	};

#if defined IS_MULTITHREADED
	tsl::WorkerLatch workerLatch;
	std::atomic<bool> isMultithreaded{ true };
	tsl::synth::ChannelThread channelThreads[8];
#if defined __ANDROID__
	tsl::BinarySemaphore sem;
	tsl::synth::SynthThread synthThread;
	// Doubles as the coordinator's in-flight flag for record_loop's drain: the
	// callback sets it true BEFORE it releases sem, and synthFuncIntern clears
	// it once the pass has finished touching shared state. That ordering is what
	// makes it a correct hand-off -- the counter above cannot cover SynthThread,
	// because the callback's guard drops just after it releases the coordinator,
	// leaving a window where nothing is counted but a pass is about to start.
	// Braces are load-bearing under the drain: an indeterminate true would hang.
	std::atomic<bool> open{};
	// ON by default, deliberately.
	//
	// The generateLoad fallback that keeps the governor from downclocking a
	// worker between blocks. record_loop clears it on entry and sets it on exit,
	// so false-by-default left the spin dead until the user's first RECORD LOOP
	// and permanently on afterwards -- an asymmetry with no defensible reading.
	// True from launch makes the flag mean one thing for the whole session.
	//
	// On API 33+ this costs nothing: synthFuncIntern and ChannelThread::run both
	// skip the spin once an ADPF session is live, and every device that matters
	// now opens one. Verified on a 23100RN82L -- LOAD reads the same with the
	// flag true as with it false, because the spin never runs.
	//
	// Where it is NOT free is a device without ADPF (API < 33, or one that does
	// not implement it): eight channel threads then spin out 80% of every block
	// from launch, holding the cores the dispatcher needs, and the pass itself
	// gets slower. That cost does not show up in LOAD directly -- diff is taken
	// before the spin -- so reading the code will not catch it and only a
	// pre-33 device will. If one is ever reported as running hot from startup,
	// this flag is the first thing to try.
	bool stabilize{ true };

	// Concealment for a block the coordinator failed to deliver in time (the
	// `open` path in PlayerBase::synthFunc). Holds the last stereo mix that
	// actually reached the device, so the drop can fade that out instead of
	// cutting straight to silence -- a hard cut has two full-amplitude steps,
	// one into the hole and one out of it, and both are broadband clicks.
	//
	// MT-only by construction: `open` is set solely under `if (isMultiThreaded)`,
	// so nothing here is reachable on the single-threaded path.
	//
	// concealBuf is arena-allocated in setupBuffers (maxBufSize * 2, interleaved),
	// so the audio thread never allocates.
	sampleTSL* concealBuf{};
	int32_t concealFrames{};        // frames stored; 0 = nothing delivered yet
	float   concealGain{ 1.f };     // decays across consecutive drops
	bool    concealReverse{ true };  // ping-pong direction; true = play backwards
	int32_t concealFadeIn{};        // samples of fade-in owed to the next good block

	// Blocks seen since the stream started, saturating at the priming window --
	// see the wait in PlayerBase::synthFunc. Reset by onPlayerStop, which runs
	// after stream->close() and so cannot race a callback. A stream that goes
	// away without a stop (onErrorAfterClose on a device disconnect) leaves it
	// saturated, and the next start gets the old drop-and-conceal behaviour for
	// its priming burst -- one clean audible cost on a path that has just had a
	// device yanked out of it.
	std::atomic<int32_t> startBlocks{};

	bool hasmic{};
	int32_t micrecformat{};
	bool startPoweredOn{};
#endif
	bool isRunningAsPlugin{};
	std::atomic<bool> isRestoringState{};
	alignas(ALIGN) uint64_t offset{};
//	std::atomic<bool> pressed{};
	int64_t bufsize_track{};

	alignas(64) MYFLOAT envelopes[19][WINDOW_SIZE + 8]{};
	alignas(64) MYFLOAT pan[2][TBLSIZE2]{};
	MYFLOAT* windows[ARRAY_LEN(envelopesnames)]{};
	std::map<std::string, tsl::envelope::WindowDescriptor> eq{};

	int32_t flanger_min_samples{};
	int32_t flanger_max_samples{};
	int32_t flanger_range_samples{};
	int32_t min_delay_samples{}, max_delay_samples{};
	uint64_t mask{};
	uint64_t lobits{};
	MYFLOAT pfrac{};
	MYFLOAT* blackmanwin{}, * hanningwin{};

//	double time{};
	
	tsl::QueueUnsafe<LFO*, 16> midilfoqueue{};
	bool midiclockactive{ true };
	bool midiclockreset{};

	tsl::random::Random random{};
	tsl::graphics::Waveform* waveform{};
	FrequencyResponse frequencyResponse{ 1024 };
	MYFLOAT grainVCOPrevCps[8]{ LOG10D20F(220.f) };
	std::atomic<bool> decodingStop{};
    MYFLOAT prevcps[8]{LOG10D20F(220.), LOG10D20F(220.),LOG10D20F(220.),LOG10D20F(220.),LOG10D20F(220.),LOG10D20F(220.),LOG10D20F(220.),LOG10D20F(220.)};
	std::atomic<bool> updateRenderThreadeq5[4]{}, updateRenderThreadeq5dyn[4]{}, updateRenderThreadSpectrum[4]{};
	std::atomic<double*> inputeq5[4]{}, inputeq5dyn[4]{}, inputspectrum[4]{};

#if defined(PLUGIN_MODE) || defined(OS_IOS)
	HostTimeSnapshot hostTimeSnapshot{};
#endif
#endif
	Recorder recorder{};
//	std::atomic<uint32_t> currentrecoff{};
	std::atomic<bool> pauseplayback{};
	ViewS views{};
	std::atomic_bool abort_loop_record{};
	std::atomic_bool inputdisabled{};
	std::shared_ptr<std::vector<SaveDummy>> presetToSave{};
	bool saveAudioWithPreset{ true };
	bool micPausePlayback{ true };
	tsl::AtomicSharedPtr<tsl::graphics::EffectOrderView> effectsOrderView;
	tsl::AtomicSharedPtr<tsl::graphics::RecyclerView<tsl::graphics::FXView, std::pair<std::string, int>>> fxView;
	tsl::AtomicSharedPtr<tsl::graphics::RecyclerView<InputRouting, std::string>> inputRouting;
	tsl::AtomicSharedPtr<tsl::graphics::RecyclerView<Syncing, controlitem_t>> showSyncing;
	tsl::AtomicSharedPtr<tsl::graphics::FloatingScrollView<TrackSettings, std::string >> trackSettings;
	tsl::AtomicSharedPtr<tsl::graphics::RecyclerView<TrackSettings, std::string >> generalSettings;
	tsl::AtomicSharedPtr<tsl::graphics::LoopPositions> showPosWindows[4];
	tsl::AtomicSharedPtr<tsl::graphics::ListView> editorView{}, recordView{};
	tsl::AtomicSharedPtr<tsl::graphics::BackView> backView{};
	tsl::QueueUnsafe<std::shared_ptr<tsl::preset::PresetWrapper>, 100> presets{};
	tsl::QueueUnsafe<std::shared_ptr<tsl::preset::PresetWrapper>, 100> projects{};

#ifdef LICENSE_CHECK_ENABLED
	tsl::LicenseChecker licenseChecker{ "localhost", false };
	bool licenseValid = false;
	bool licenseChecked = false;
#endif
};

const char* exceptionStr();

MYFLOAT* getsinewave();

void setsinewave(MYFLOAT* s);

MYFLOAT* getcosinewave();

void setcosinewave(MYFLOAT* s);

MYFLOAT* getfulltri();

void setfulltri(MYFLOAT* s);

#endif
