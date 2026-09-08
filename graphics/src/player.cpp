#include "defines.h"
#include "logger.h"
#include "player.h"
#include "tools.h"
#include "view.h"
#include "tools/queuetsl.h"
#include "app.h"
#include <cstring>
#include <utility>

#include "file.h"

std::shared_ptr<tsl::Player::RecordingContext>tsl::Player::setupRecording(tsl::AppState* _appState, FILE* fd, std::string message,  uint16_t channels, tsl::AudioFileWrapper::SampleFormat audioFormat) {
	auto w = std::make_shared<tsl::Player::RecordingContext>();
	if (fd != nullptr) {
		if (w->wrapper.init(fd, true) != 0) {
			message.append("Could not open output file.");
			showToast(_STATE, message.data());
			return nullptr;
		};

		std::vector<uint8_t> vec;
		vec.resize(tsl::app::icon_size);
		std::memcpy(vec.data(), tsl::app::icon_data, tsl::app::icon_size * sizeof(uint8_t));
		w->wrapper.set_artwork(vec, "image/png");
		w->wrapper.set_album(tsl::app::appName);
		w->wrapper.set_comment(tsl::app::recordingComment);

		int  recformat = _STATE->format;
		if (recformat == 0) {
			if (w->wrapper.init_audio(tsl::AudioFileWrapper::WAV_16BIT, (uint32_t)_STATE->sr, channels, audioFormat) != 0) {
				message.append("Could not init audio converter.");
				showToast(_STATE, message.data());
				return nullptr;
			}
		}
		else if (recformat == 10) {
			if (w->wrapper.init_audio(tsl::AudioFileWrapper::FLAC_16BIT, (uint32_t)_STATE->sr, channels, audioFormat) != 0)
			{
				message.append("Could not init audio converter.");
				showToast(_STATE, message.data());
				return nullptr;
			}
		}
		else if (recformat == 11) {
			if (w->wrapper.init_audio(tsl::AudioFileWrapper::FLAC_24BIT, (uint32_t)_STATE->sr, channels, audioFormat) != 0)
			{
				message.append("Could not init audio converter.");
				showToast(_STATE, message.data());
				return nullptr;
			}
		}
		else if (recformat == 12) {
			if (w->wrapper.init_audio(tsl::AudioFileWrapper::WAV_32FLOAT, (uint32_t)_STATE->sr, channels, audioFormat) != 0)
			{
				message.append("Could not init audio converter.");
				showToast(_STATE, message.data());
				return nullptr;
			}
		}
		else {
			if (w->wrapper.init_audio(tsl::AudioFileWrapper::MP3_LAME, (uint32_t)_STATE->sr, channels, audioFormat) != 0)
			{
				message.append("Could not init audio converter.");
				showToast(_STATE, message.data());
				return nullptr;
			}
		}

	}
	else {
		if (w->wrapper.init() != 0) {
			message.append("Could not open output.");
			showToast(_STATE, message.data());
			return nullptr;
		};
		if (w->wrapper.init_audio(tsl::AudioFileWrapper::RAW_16BIT, _STATE->sr, channels, audioFormat) != 0)
		{
			message.append("Could not init audio converter.");
			showToast(_STATE, message.data());
			return nullptr;
		}
	}
	return w;
};


int tsl::Player::record_live_thread(const std::shared_ptr<RecordingContext>& w) {
	std::string message = "Live Recording: ";
	while(auto recItemOpt = recQueue.try_pop()) {
		auto *recItem = *recItemOpt;  // or recItemOpt.value()
		if (!recItem) continue;
		_STATE->pool.release(recItem);
	}


	recoff = 0;
	slot = _STATE->waitNotify.acquire_slot();
	isrecording.store(true, std::memory_order_release);
	//tprio(p, -19);
	{
		/* One acquisition for all three, mirroring the restore below. Two
		   reasons it must not be three separate ones: the render walk reads
		   `perm` under queue_draw every frame, so writing it bare here is a
		   data race rather than a stale read; and deldraw() + redraw() is a
		   RE-INSERT -- redraw() on a still-queued view is a no-op duplicate,
		   so the removal is what lets the re-add take. A walk landing between
		   them sees a half-applied button. */
		std::lock_guard lk(_STATE->queue_draw);
		_STATE->parameters[RECORDButton].view->delRecursiveDraw();
		_STATE->parameters[RECORDButton].view->perm = true;
		_STATE->parameters[RECORDButton].view->addRecursiveDraw();
	}
	long samplesWritten = 0;
	while (isrecording.load(std::memory_order_acquire)) {
		_STATE->waitNotify.wait_for_signal();
		if (_STATE->destroyRequested.load(std::memory_order_acquire))break;
		while (auto recItemOpt = recQueue.try_pop()) {
			auto *recItem = *recItemOpt;  // or recItemOpt.value()
			if (!recItem) continue;
        	if (w->wrapper.write(recItem->data.data(), sizeof(sampleTSL), recItem->frames * recItem->channels) != recItem->frames * recItem->channels) {
				LOGE("Error record. Disk full?");
				message.append("Write Error.");
				showToast(_STATE, message.data());
				isrecording.store(false, std::memory_order_release);
				_STATE->pool.release(recItem);
				break;
			}
			samplesWritten += recItem->frames;
			recoff = samplesWritten;

			_STATE->pool.release(recItem);
		}
		if (w->wrapper.get_mode() == tsl::FileWrapper::INTERNAL_BUFFER && samplesWritten >= _STATE->sr * 420)isrecording.store(false, std::memory_order_release);
	}

	while (auto recItemOpt = recQueue.try_pop()) {
		auto* recItem = *recItemOpt;  // or recItemOpt.value()
		if (!recItem) continue;
		_STATE->pool.release(recItem);
	}

	LOGI("Recording exit. Offset: %ld", samplesWritten);
	if(!_STATE->destroyRequested.load(std::memory_order_acquire))
	{
		std::lock_guard lk(_STATE->queue_draw);
		_STATE->parameters[RECORDButton].view->delRecursiveDraw();
		_STATE->parameters[RECORDButton].view->perm = false;
		_STATE->parameters[RECORDButton].view->addRecursiveDraw();
	}
	w->wrapper.close();
	w->finished.store(true, std::memory_order_release);
	
	return samplesWritten > 0 ? 0 : -1;
};


void
tsl::Player::recStop() {
	isrecording.store(false, std::memory_order_release);
	_STATE->waitNotify.wake_thread(slot);
}

bool tsl::Player::isPlaying() {
    return _isplaying.load(std::memory_order_acquire);
}

#ifdef __ANDROID__
#include <jni.h>
using namespace oboe;


jint java_record_live(JNIEnv* env, jclass obj, jint fd) {
    FILE *f = nullptr;
    f = fdopen(fd, "wb");
    if (f == nullptr){
        if(fd)close(fd);
        return -1;
    }
    auto w = tsl::Player::setupRecording(__STATE, f);
	if (w != nullptr)
		return (jint)__STATE->player.record_live_thread(w);
	else{
        fclose(f);
        return -1;
    }
};

static void ForegroundService(bool isPlaying) {
	ATTACH
	if (env && tsl::android::activityclass) {
		jmethodID mid = env->GetStaticMethodID(tsl::android::activityclass, "powerControl", "(Z)V");
		if (mid) {
			env->CallStaticVoidMethod(tsl::android::activityclass, mid, (jboolean)isPlaying);
		} else {
			env->ExceptionClear();
		}
	}
	DETACH
}






// Read a static int field that an app may or may not declare.
//
// GetStaticFieldID throws when the field is missing, and a pending exception
// makes the *next* JNI call fail, so the clear is not optional. Same pattern as
// the lowLatency field below.
static int optionalStaticInt(JNIEnv* env, jclass cls, const char* name, int fallback) {
	if (env == nullptr || cls == nullptr) return fallback;
	jfieldID fid = env->GetStaticFieldID(cls, name, "I");
	if (fid == nullptr) {
		env->ExceptionClear();
		return fallback;
	}
	return (int)env->GetStaticIntField(cls, fid);
}

// Capacity to ask for when the block size is left to the device: 160 ms, the
// same depth the 40 ms default gets from the n * 4 below. Auto picks the block
// size, not the amount of slack -- there is no reason for it to be shallower.
static constexpr int kAutoCapacityFrames = (tsl::kEngineSampleRate * 4) / 25;

// Floor for the block size Auto is allowed to settle on, in frames at the
// engine rate. 5 ms, which is the smallest block the buffer list above offers
// by hand -- Auto choosing the device's granularity is the point of it, but a
// block the user could not have selected deliberately is not a choice this code
// should make for them. An MMAP burst can be a single millisecond, and the
// engine's per-block cost does not shrink with the block.
static constexpr int kAutoMinCallbackFrames = tsl::kEngineSampleRate / 200;

// The buffer size choices, in frames at kEngineSampleRate. Index 0 is Auto.
//
// This table is the definition of the setting. Java stores nothing but the
// index into it, so a frame count never has to survive a trip through a
// preference file, and the rate these are counted at is the one right above --
// not a second copy of it living somewhere that nothing checks.
//
// MUST stay in step with BUFFER_LABELS in each app's MyApplication.java, which
// is the display side of the same list. Appending is safe; reordering or
// removing an entry silently changes what every stored index means.
static constexpr int kBufferChoiceFrames[] = {
	0,      // 0 -- Auto
	240,    // 1 --   5 ms
	480,    // 2 --  10 ms
	960,    // 3 --  20 ms
	1920,   // 4 --  40 ms  (default)
	3840,   // 5 --  80 ms
	7680,   // 6 -- 160 ms
};
static constexpr int kBufferChoiceCount =
	(int)(sizeof(kBufferChoiceFrames) / sizeof(kBufferChoiceFrames[0]));

oboe::Result  tsl::Player::BuildStream() {
	AudioStreamBuilder builder;

	// Requested sizes, kept for the log line after the stream is open -- the
	// point of that line is the gap between what was asked and what was given.
	int requestedFrames = 0;
	int deviceBurst = 0;
	bool wasAuto = false;

	builder.setSharingMode(oboe::SharingMode::Exclusive);
	builder.setDirection(oboe::Direction::Output);
	ATTACH
		// Performance mode, chosen by the app.
		//
		// Oboe documents this as determining "the latency, the power
		// consumption, and the level of protection from glitches" -- so
		// LowLatency is the least glitch-protected setting, and it is the right
		// one only when something is being played. Grainstorm and PocketAnalog
		// are instruments and want it; an app that generates its own music does
		// not, and for it the protection is worth more than the latency.
		//
		// The effect is not only advisory. In Oboe's OpenSLES
		// estimateNativeFramesPerBurst the branch that grows the burst to a 20 ms
		// high-latency size is gated on mPerformanceMode != LowLatency, so asking
		// for LowLatency is what stops Oboe reporting a larger burst.
		//
		// Absent field means LowLatency, so apps that do not declare it keep
		// exactly the behaviour they have always had. GetStaticFieldID throws
		// when the field is missing, hence the explicit clear -- leaving a
		// pending exception would make the next JNI call in this function fail.
		{
			auto perfMode = oboe::PerformanceMode::LowLatency;
			jfieldID fid = env->GetStaticFieldID(tsl::android::appclass, "lowLatency", "Z");
			if (fid != nullptr) {
				if (!env->GetStaticBooleanField(tsl::android::appclass, fid))
					perfMode = oboe::PerformanceMode::None;
			} else {
				env->ExceptionClear();
			}
			builder.setPerformanceMode(perfMode);
			LOGD("Requested perf mode: %s (lowLatency field %s)",
				perfMode == oboe::PerformanceMode::LowLatency ? "LowLatency" : "None",
				fid != nullptr ? "found" : "absent");
		}
		// Values Oboe cannot discover for itself on the OpenSLES path.
		//
		// Oboe ships FramesPerBurst = 192 and SampleRate = 48000 as placeholders
		// and documents that the app must pass the real ones down through JNI
		// (see DefaultStreamValues in Oboe's Definitions.h). Nothing here ever
		// did, so every OpenSLES stream opened at a hardcoded 48000 with a
		// 192-frame burst. AAudio takes both from the HAL and ignores these.
		//
		// SampleRate stays at the engine's rate on purpose. Setting it to the
		// device's rate instead would make the child stream open at, say, 44100,
		// fail AudioStreamBuilder::isCompatible against our 48000 request, and
		// pull Oboe's own resampler onto the audio thread. Leaving them equal
		// lets the SLES stream *be* the stream and hands the conversion to
		// AudioFlinger, off our thread.
		oboe::DefaultStreamValues::SampleRate = tsl::kEngineSampleRate;
		oboe::DefaultStreamValues::ChannelCount = 2;
		deviceBurst = optionalStaticInt(env, tsl::android::appclass, "deviceBurst", 0);
		if (deviceBurst > 0)
			oboe::DefaultStreamValues::FramesPerBurst = deviceBurst;
		else
			LOGE("deviceBurst not found on the app class -- check the -keep "
				"rules. OpenSLES will size its queue against Oboe's placeholder "
				"192, which is not this device's burst.");

		// Ask for the rate the engine was built for, and let Oboe convert if the
		// hardware disagrees. The conversion quality is not decoration: with it
		// left at None, QuirksManager::isAAudioMMapPossible rejects any rate
		// other than 48000, so naming a rate would cost the MMAP path.
		builder.setSampleRate(tsl::kEngineSampleRate);
		builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);

		builder.setChannelCount(2);
	builder.setFormat(AudioFormat::Float);
	builder.setDataCallback(this);

	// Buffer size, resolved from the stored index through the table above.
	//
	// audioBufIndex is what grainstorm and PocketAnalog declare. Index 0 is a
	// legitimate stored value (Auto), so "field absent" has to be a different
	// number -- hence the -1 fallback rather than 0. Apps that do not declare it
	// (generative) fall back to savedBufSize, which is a raw frame count and is
	// used as it stands.
	//
	// Out-of-range indices resolve to Auto rather than being clamped to an end
	// of the list: an index this code does not recognise came from a build that
	// offered a different list, and the safe reading of an unknown choice is
	// "let the device decide", not "take the largest" or "take the smallest".
	int n = 0;                  // frames at kEngineSampleRate; <= 0 means Auto
	const int idx = optionalStaticInt(env, tsl::android::appclass, "audioBufIndex", -1);
	if (idx >= 0) {
		n = (idx < kBufferChoiceCount) ? kBufferChoiceFrames[idx] : 0;
		if (idx >= kBufferChoiceCount)
			LOGE("audioBufIndex %d out of range (%d choices) -- using Auto",
				idx, kBufferChoiceCount);
	} else {
		n = optionalStaticInt(env, tsl::android::appclass, "savedBufSize", -1);
		// Neither field found. For generative that cannot happen (it declares
		// savedBufSize); for anything else it means the name did not survive to
		// the APK -- R8 renames a field that only native refers to unless
		// proguard-rules.pro lists it, and GetStaticFieldID then fails exactly
		// like an absent field. That once looked like the user having chosen
		// Auto, on every minified build, which is why this is loud.
		if (n == -1)
			LOGE("Neither audioBufIndex nor savedBufSize found on the app class "
				"-- check the -keep rules. Falling back to Auto.");
	}

	// Depth is requested separately from block size, and it is the depth that
	// keeps a late thread from underrunning.
	//
	// On OpenSLES 1.10 the two are genuinely separate knobs. From
	// AudioStreamOpenSLES::calculateOptimalBufferQueueLength:
	//
	//     queueLength = max(2, ceil(max(requestedCapacity, 2 * framesPerCallback)
	//                               / estimateNativeFramesPerBurst()))
	//     queueLength = min(queueLength, 8)
	//
	// and the queue that actually holds audio is queueLength buffers of
	// framesPerCallback frames each. Note what that means for the burst: the
	// queue length is capacity DIVIDED BY the burst, so reporting a truthful
	// (larger) burst SHORTENS the queue. Today, with FramesPerBurst left at
	// Oboe's placeholder 192, ceil() saturates at the cap of 8 and every stream
	// silently gets the deepest queue OpenSLES allows. Requesting the capacity
	// outright is what keeps that depth once the burst tells the truth --
	// otherwise this change would quietly halve the buffering on every device.
	//
	// On AAudio the request is the capacity, directly. This is also the line
	// that fixes the 4096-frames-into-a-4100-frame-capacity case: nothing ever
	// asked for capacity, so AAudio handed back the least that fit one block,
	// leaving four frames of slack behind the block in flight.
	if (n <= 0) {
		// Auto: the device's burst picks the block size, and it is REQUESTED,
		// not left unspecified.
		//
		// Oboe's advice is to leave it unspecified ("We encourage leaving this
		// unspecified in most cases"), but the same doc names the exception and
		// this engine is it: "if your application is, for example, doing FFTs or
		// other block oriented operations, then call this function to get the
		// sizes you need." Unspecified does not mean "the device picks a size
		// and keeps it" -- on AAudio it means the size is free to CHANGE from
		// one callback to the next. AAudio.h, on getFramesPerDataCallback:
		//
		//     AAUDIO_UNSPECIFIED indicates that the callback buffer size for
		//     this stream may vary from one dataProc callback to the next.
		//
		// The engine renders one block AHEAD: a callback mixes out_buf and then
		// dispatches the pass that fills it for next time, so how many frames
		// the next callback may consume is fixed at dispatch. Arrive with a
		// larger numFrames and the mix reads past what that pass wrote -- the
		// tail of an older block, played again. Arrive with a smaller one and
		// rendered frames are thrown away that the engine has already counted as
		// played: grain positions, LFO phases and the ring pointer all advanced
		// by the length that was rendered, so the discarded frames are never
		// made up. Either way it is a discontinuity, at every callback where the
		// size moves.
		//
		// That is why AAudio + Auto was the one combination of the four that
		// glitched. Both fixed paths set the size explicitly, and Oboe's
		// OpenSLES path resolves an unspecified size to the burst when it sizes
		// its queue buffers -- so the other three were already constant, and
		// asking for the burst here is what OpenSLES + Auto was doing anyway.
		//
		// Requested rather than probed with a throwaway stream: the burst is not
		// readable until a stream is open, and closing an exclusive MMAP stream
		// only to reopen it in the same breath risks coming back with nothing on
		// devices that hold the endpoint briefly. This keeps Auto to exactly one
		// open, the same shape as the fixed path.
		//
		// Java's burst is in device-rate frames while the callback size is at
		// the engine rate, so on a device that disagrees about the rate the
		// multiple is approximate. Fine: what is needed here is a size that does
		// not MOVE, and a resampled stream needs that most -- Oboe's resampler
		// makes the block wander by a frame at the seams on its own.
		const int burst = deviceBurst > 0
			? deviceBurst : oboe::DefaultStreamValues::FramesPerBurst;
		int cb = burst;
		while (cb > 0 && cb < kAutoMinCallbackFrames) cb += burst;
		if (cb <= 0) cb = kAutoMinCallbackFrames;
		builder.setFramesPerDataCallback(cb);
		builder.setBufferCapacityInFrames(kAutoCapacityFrames);
		n = cb;
		wasAuto = true;
	} else {
		builder.setFramesPerDataCallback(n);
		builder.setBufferCapacityInFrames(n * 4);
	}
	requestedFrames = n;

	builder.setErrorCallback(this);
	builder.setAudioApi(env->GetStaticBooleanField(tsl::android::appclass,
		env->GetStaticFieldID(tsl::android::appclass,
			"useAAudio", "Z"))
		? AudioApi::AAudio : AudioApi::OpenSLES);
	DETACH
		Result result = builder.openStream(stream);

	// Ask the platform to keep the audio callback thread fast (Oboe 1.10+).
	//
	// This is the modern replacement for oboe::StabilizedCallback, which held the
	// clocks up by burning the rest of every callback with cpu_relax. That works
	// -- keeping the core busy is exactly how it stops the governor downclocking
	// or migrating the thread to a little core -- but the heat is the mechanism,
	// not a side effect, and it costs roughly 90% of a core for as long as audio
	// plays. Measured on a Redmi 23100RN82L: 11 s of CPU per 12 s of wall clock
	// spinning, versus 1 s per 20 s with this enabled, for the same work.
	//
	// Here it asks instead: the thread's deadline and its actual durations go to
	// the power manager, which boosts and places it accordingly. Backed by ADPF
	// where the device implements it, and a documented no-op where it does not --
	// so it is safe unconditionally and needs no API-level guard. Oboe reads the
	// flag in its data callback, so setting it before start() is what counts.
	//
	// It covers Oboe's own callback thread and nothing else. An app that renders
	// on a worker (generative's RenderWorker) still needs its own hint session
	// for that thread; this does not reach it.
	if (result == Result::OK && stream != nullptr) {
		stream->setPerformanceHintEnabled(true);

		// The high-water mark, which is what latency and underruns actually
		// follow -- capacity is only the ceiling it may grow to.
		//
		// AAudio does not derive this from the callback size: it opens with a
		// default of a burst or two regardless of how big a block the callback
		// asks for, so a 1920-frame callback into a default buffer size can
		// never keep a full block queued and starves on every callback. That
		// reads as ragged, part-filled audio rather than clean xruns, because
		// the stream is short of data continuously rather than missing a
		// deadline occasionally.
		//
		// Two blocks in flight for a fixed size, four bursts when the device
		// chose the size. Under a FilterAudioStream this forwards to the child
		// and is therefore in device-rate frames -- a request for depth, not an
		// exact figure, so the unit slip is not worth correcting for.
		//
		// Auto takes the larger of the two rules rather than the burst one
		// alone. Now that Auto asks for a concrete size, and rounds it up to the
		// floor, that size can be several bursts -- and four bursts of
		// high-water would then be SHORTER than a single callback, which is the
		// starvation this whole section exists to prevent. Where Auto lands on
		// exactly one burst the old rule is still the larger of the two, so the
		// depth it used to get is unchanged.
		{
			const int burstGranted = stream->getFramesPerBurst();
			int target = requestedFrames * 2;
			if (wasAuto && target < burstGranted * 4) target = burstGranted * 4;
			if (target < burstGranted * 2) target = burstGranted * 2;
			auto sized = stream->setBufferSizeInFrames(target);
			if (!sized)
				LOGE("setBufferSizeInFrames(%d) failed: %s", target,
					convertToText(sized.error()));
		}

		// What was actually granted. Worth logging in full because under a
		// FilterAudioStream these numbers are not all in the same unit: the data
		// callback runs at the engine's rate, while getFramesPerBurst,
		// getBufferSizeInFrames and getBufferCapacityInFrames all pass straight
		// through to the child stream and are in device-rate frames.
		LOGI("Audio: %s, sr %d (asked %d), cb %d (asked %d, %s), burst %d, size %d/%d [device frames]",
			stream->getAudioApi() == AudioApi::AAudio ? "AAudio" : "OpenSLES",
			stream->getSampleRate(), tsl::kEngineSampleRate,
			stream->getFramesPerDataCallback(), requestedFrames,
			// "auto VARIES" is the state that glitches: the size request did
			// not take, so numFrames is free to change on every callback and
			// the one-block-ahead pipeline has nothing stable to render into.
			// It should be unreachable now; it is here so that if it ever
			// happens it can be read straight off a device log instead of
			// being diagnosed by ear.
			!wasAuto ? "fixed"
				: stream->getFramesPerDataCallback() > 0 ? "auto"
				: "auto VARIES",
			stream->getFramesPerBurst(),
			stream->getBufferSizeInFrames(), stream->getBufferCapacityInFrames());

		// Publish the real block size now, not on the first callback.
		//
		// currentBufSize otherwise sits at its 1024 default until onAudioReady
		// assigns it, and everything that derives a deadline from it during
		// startup gets that stale figure -- measured on a 23100RN82L,
		// SynthThread opened its ADPF session with a 20 ms target for what is
		// actually a 40 ms block.
		//
		// This does NOT account for the one dropped block seen at every launch:
		// that still happens with the size correct from the first pass, so its
		// cause is something else and remains open.
		//
		// getFramesPerDataCallback runs at the engine's rate, which is the unit
		// currentBufSize wants; the burst fallback covers a device that leaves
		// the callback size unspecified.
		{
			int cb = stream->getFramesPerDataCallback();
			if (cb <= 0) cb = stream->getFramesPerBurst();
			if (cb > 0) _STATE->currentBufSize = cb;
		}

		// A rate mismatch is silent and un-hearable as a fault -- the whole
		// piece just runs fast or slow, every decay and LFO off by one factor.
		// The engine cannot follow the stream (its tables were built during
		// setup, long before this), so there is nothing to do but say so.
		if (stream->getSampleRate() != tsl::kEngineSampleRate) {
			LOGE("Stream opened at %d but the engine runs at %d -- pitch will be wrong",
				stream->getSampleRate(), tsl::kEngineSampleRate);
			showToast(__STATE, "Audio device rate mismatch");
		}
	}
	return result;
};



void tsl::Player::onErrorAfterClose(AudioStream* audioStream, Result error) {
	if (error == Result::ErrorDisconnected) {
		auto _appState = __STATE;
        _isplaying.store(false, std::memory_order_release);
		_STATE->parameters[POWERButton].view->redraw();
		// Handle stream restart on a separate thread
		//std::function<void(void)> restartStream = std::bind(&PlayAudioEngine::restartStream, this);
		//mStreamRestartThread = new std::thread(restartStream);
	}
}

oboe::DataCallbackResult
tsl::Player::onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) {

	_STATE->currentBufSize = numFrames;

	auto bfqbuffer = (sampleTSL*)audioData;

	PlayerBase::synthFunc(_STATE, bfqbuffer, numFrames);
    /*
    if (isrecording.load(std::memory_order_acquire)) {
         buf.resize(numFrames*2);
        for (int i = 0; i < numFrames; i++) {
            buf[i * 2] = bfqbuffer[i * 2];
            buf[i * 2 + 1] =  bfqbuffer[i * 2 + 1];
        }
        if (!toRecThreadQueue.push(std::move(buf))) {
            LOGE("Failed to Push Queue Full?");
        }
        else {
            recsem.release();
            recoff += numFrames;
        }
    }
*/
	return oboe::DataCallbackResult::Continue;
}

Result tsl::Player::_play() {
	Result result;
	StreamState state = StreamState::Uninitialized;
	if (stream != nullptr)
		state = stream->getState();
	if (state == StreamState::Closed || state == StreamState::Uninitialized ||
		state == StreamState::Disconnected) {
		if (stream != nullptr && state != StreamState::Closed &&
			state != StreamState::Uninitialized)
			stream->close();
		if ((result = BuildStream()) != Result::OK) {
			showToast(__STATE, convertToText(result));
			return result;
		}
		LOGI("New Stream built.");
	}
	result = stream->start();
	if (result != Result::OK) {
		showToast(__STATE, convertToText(result));
		return result;
	}
	_isplaying.store(true, std::memory_order_release);

	return Result::OK;
}

Result tsl::Player::_stop() {
	Result result;
	StreamState state = StreamState::Uninitialized;
	if (stream != nullptr)
		state = stream->getState();
	if (state == StreamState::Closed || state == StreamState::Uninitialized) {
		_isplaying.store(false, std::memory_order_release);
		return Result::OK;
	}

	if (stream != nullptr) {
		result = stream->stop();
		if (result != Result::OK)
			showToast(__STATE, convertToText(result));
		result = stream->close();
		if (result != Result::OK) {
			showToast(__STATE, convertToText(result));
			return result;
		}
		LOGI("Stream closed.");
	}
	_isplaying.store(false, std::memory_order_release);

	return Result::OK;
}


Result tsl::Player::_pause() {
	Result result;
	StreamState state = StreamState::Uninitialized;
	if (stream != nullptr)
		state = stream->getState();
	if (state == StreamState::Closed || state == StreamState::Uninitialized ||
		state == StreamState::Disconnected) {
		if (stream != nullptr && state != StreamState::Closed)
			stream->close();
		if ((result = BuildStream()) != Result::OK) {
			showToast(__STATE, convertToText(result));
			return result;
		}
	}
	result = stream->pause(1000 * kNanosPerMillisecond);
	if (result != Result::OK) {
		showToast(__STATE, convertToText(result));
		return result;
	}
	_isplaying.store(false, std::memory_order_release);

	return Result::OK;
}

Result tsl::Player::_flush() {
	Result result;
	StreamState state = StreamState::Uninitialized;
	if (stream != nullptr)
		state = stream->getState();
	if (state == StreamState::Closed || state == StreamState::Uninitialized ||
		state == StreamState::Disconnected) {
		if (stream != nullptr && state != StreamState::Closed)
			stream->close();
		if ((result = BuildStream()) != Result::OK) {
			showToast(__STATE, convertToText(result));
			return result;
		}
	}
	return stream->flush(1000 * kNanosPerMillisecond);
}


int tsl::Player::flush() {
	std::lock_guard lock(mtx);
	auto res = _flush();
	return res == Result::OK ? 0 : 1;
}

void tsl::Player::init() {

	__STATE->smoothCoeff = (1. / (0.0033 * __STATE->sr + 1));
	stream = nullptr;
	_isplaying = false;

	Result result = BuildStream();
	if (result != Result::OK) {
		LOGE("Failed to open stream. Error: %s", convertToText(result));
		return;
	}
	LOGD("Frames per burst %d", stream->getFramesPerBurst());
	// Perf mode and buffer are logged as the stream REPORTS them, not as
	// requested: every one of these can be refused, and a request that was
	// silently downgraded looks identical to one that was honoured.
	LOGD("API: %s Mode: %s Perf: %s Buffer: %d/%d frames",
		stream->getAudioApi() == AudioApi::AAudio ? "AAudio" : "OpenSLES",
		stream->getSharingMode() == SharingMode::Exclusive ? "Exclusive" : "NonExclusive",
		stream->getPerformanceMode() == PerformanceMode::LowLatency ? "LowLatency"
			: stream->getPerformanceMode() == PerformanceMode::None ? "None" : "Other",
		stream->getBufferSizeInFrames(), stream->getBufferCapacityInFrames());
}



int tsl::Player::play() {
	std::lock_guard lock(mtx);
	Result result = _play();
	_STATE->parameters[POWERButton].view->redraw();
	ForegroundService(true);
	if (_STATE->onPlayerStart) _STATE->onPlayerStart();
	return result != Result::OK ? 1 : 0;
}

int tsl::Player::stop() {
	std::lock_guard lock(mtx);
	Result result = Player::_stop();
	_STATE->parameters[POWERButton].view->redraw();
	ForegroundService(false);
	if (_STATE->onPlayerStop) _STATE->onPlayerStop();
	return result != Result::OK ? 1 : 0;
}

int tsl::Player::pause() {
	std::lock_guard lock(mtx);
	Result result = _pause();
	_STATE->parameters[POWERButton].view->redraw();
	return result != Result::OK ? 1 : 0;
}

#else

void tsl::Player::onAudioReady(sampleTSL** in, sampleTSL** out, int numFrames, int channelMask)
{
	PlayerBase::synthFunc(_STATE, in, out, numFrames, channelMask);
}
#if defined(OS_IOS) && defined(STANDALONE_MODE)
int tsl::Player::play() {
	std::lock_guard lock(mtx);
	if (tsl::app::isRunningAsAppExtension()) {
		_isplaying.store(true, std::memory_order_release);
		_STATE->parameters[POWERButton].view->redraw();
		return 0;
	}
	if (startAudioFunc()){
		_isplaying.store(true, std::memory_order_release);
		_STATE->parameters[POWERButton].view->redraw();
		if (_STATE->onPlayerStart) _STATE->onPlayerStart();
	}
	return 0;
}

int tsl::Player::stop() {
	std::lock_guard lock(mtx);
	if (tsl::app::isRunningAsAppExtension()) {
		_isplaying.store(false, std::memory_order_release);
		_STATE->parameters[POWERButton].view->redraw();
		return 0;
	}
	stopAudioFunc();
	_isplaying.store(false, std::memory_order_release);
	_STATE->parameters[POWERButton].view->redraw();
	if (_STATE->onPlayerStop) _STATE->onPlayerStop();
	return 0;
}

#else
int tsl::Player::play() {
	std::lock_guard lock(mtx);
	_isplaying.store(true, std::memory_order_release);
	_STATE->parameters[POWERButton].view->redraw();
	if (_STATE->onPlayerStart) _STATE->onPlayerStart();
	return 0;
}

int tsl::Player::stop() {
	std::lock_guard lock(mtx);
	_isplaying.store(false, std::memory_order_release);
	_STATE->parameters[POWERButton].view->redraw();
	if (_STATE->onPlayerStop) _STATE->onPlayerStop();
	return 0;
}
#endif

int tsl::Player::flush() {
	return 0;
}

#endif