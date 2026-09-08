#pragma once
#include "types.h"
#include "defines.h"
#include <mutex>
#include "tools.h"
#include "FileWrapper.h"
#include "tools/RingBufferQueue.h"
#include <thread>
#include <vector>
#include <array>

#ifdef __ANDROID__
#define sampleTSL float

#include <jni.h>
#include <oboe/Oboe.h>

namespace tsl {
	// The engine's sample rate on Android. Fixed, not asked of the device.
	//
	// It has to be a constant because of ordering: android-main.cpp fills
	// _STATE->sr and then calls tsl::app::setup(), which sizes every table and
	// filter for that rate, and no stream exists until the first play(). There
	// is no point at which the engine could be told what rate the hardware
	// chose, so the hardware is told instead -- BuildStream asks for this rate
	// and enables Oboe's converter, and devices that do not run at 48k resample.
	//
	// Before this, OpenSLES opened at oboe::DefaultStreamValues::SampleRate --
	// a hardcoded 48000, because nothing ever assigned it -- while the engine
	// ran at whatever PROPERTY_OUTPUT_SAMPLE_RATE reported. On 44.1 kHz hardware
	// that is roughly two semitones sharp, with no setting to escape it.
	constexpr int kEngineSampleRate = 48000;
}

namespace PlayerBase {
	constexpr int NUM_REC_BUFFERS = 64;
	extern void synthFunc(tsl::AppState*, sampleTSL*, int numframes);
}
jint java_record_live(JNIEnv* env, jclass obj, int fd);

#else
#define sampleTSL double
namespace PlayerBase {
	constexpr int NUM_REC_BUFFERS = 64;
	extern void synthFunc(tsl::AppState*, sampleTSL** in, sampleTSL** out, int numframes, int channelMask);
	
};

#endif
namespace tsl {
	constexpr int audioBufferDefaultSize = 1024;
	template<typename T, size_t numSamples = audioBufferDefaultSize>
	struct AudioBuffer
	{
		std::array<T, numSamples> data{};
		size_t frames{};
		size_t channels{ 2 };
	};
}

using ToRecThreadQueue = tsl::RingBufferSPSCQueue<tsl::AudioBuffer<sampleTSL>*, 128>;

#ifdef __ANDROID__


namespace tsl {
	class Player : oboe::AudioStreamDataCallback, oboe::AudioStreamErrorCallback {
	public:
        oboe::Result BuildStream();


		void onErrorBeforeClose(oboe::AudioStream* audioStream, oboe::Result error) override {};

		void onErrorAfterClose(oboe::AudioStream* audioStream, oboe::Result error) override;

		oboe::DataCallbackResult
			onAudioReady(oboe::AudioStream* audioStream, void* audioData, int32_t numFrames) override;

#else

namespace tsl {
	class Player {
	public:
		void onAudioReady(sampleTSL** in, sampleTSL** out, int numframes, int channelMask);

#endif
        struct RecordingContext {
            tsl::AudioFileWrapper wrapper;
            std::atomic<bool> finished{};
        };
        int record_live_thread(const std::shared_ptr<RecordingContext>&);
        static std::shared_ptr<RecordingContext> setupRecording(tsl::AppState*, FILE* fd = nullptr, std::string message = "Live Recording ", uint16_t channels = 2, tsl::AudioFileWrapper::SampleFormat audioFormat = sizeof(sampleTSL) == 4 ? tsl::AudioFileWrapper::SAMPLE_FLOAT32 : tsl::AudioFileWrapper::SAMPLE_FLOAT64);
        void lock() { mtx.lock(); };
		void unlock() { mtx.unlock();}
        bool try_lock() { return mtx.try_lock(); }

		bool isPlaying();

		void init();

		int play();

		int stop();

		int pause();

		int flush();

		void recStop();

		std::atomic<bool> _isplaying{
#ifdef PLUGIN_MODE
			true
#endif
		};
		std::atomic<bool> isrecording{};
		long offset{};
		void* data{};
		int recformat{};
		std::atomic<unsigned long> recoff{};
		tsl::AppState *_STATE{};
		ToRecThreadQueue recQueue{ };
		int slot{};
#if defined(OS_IOS) && defined(STANDALONE_MODE)
		std::function<bool()> startAudioFunc;
		std::function<void()> stopAudioFunc;
		std::function<void(int)> setBufferSizeFunc;
		int preferredBufferSize{};
#endif
	private:
        std::recursive_mutex mtx{};
		
#ifdef __ANDROID__

		oboe::Result _play();

		oboe::Result _stop();

		oboe::Result _pause();

		oboe::Result _flush();

		std::shared_ptr<oboe::AudioStream> stream{};
#endif

	};
}

