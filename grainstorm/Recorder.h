#pragma once
//
// Created by pr on 29.03.18.
//

#ifndef GRAINSTORM_RECORDER_H
#define GRAINSTORM_RECORDER_H

using recSampleFormat = short;

#include "defines.h"

#include <mutex>
#include <cstddef>
#include <cstdio>
#include <atomic>
#include <vector>
#include "view.h"

#if defined PLUGIN_MODE || defined STANDALONE_MODE
#include "tools/WaitNotify.h"
#include "tools/LockFreeQueue.h"
#include <player.h>
#endif


#ifdef __ANDROID__
#include <jni.h>
jlong java_recorder_callback(JNIEnv* env, jclass obj, jlong track, jint bufsize);


#include <oboe/Oboe.h>
using namespace oboe;

class OboeRecorder : public AudioStreamErrorCallback {
public:
	void onErrorBeforeClose(AudioStream* audioStream, Result error) override;
	void onErrorAfterClose(AudioStream* audioStream, Result error) override;
};
#else

#include <cstdint>
#include <type_traits>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <new>

#endif

#define RECORDER_START 0
#define RECORDER_STOP 1


struct TRACK;

void startrecfunc(TRACK* track);

namespace tsl::graphics {
	class RecorderView : public View {
	public:
		explicit RecorderView(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN) {
			prio = 0;
			perm = true;
		}

		void init() override;

		void render(void* ctx) override;

		void callback(const InputEvent& e) override;

	protected:
		float lastx{}, lasty{};
		int32_t dragid{ -1 };
#ifndef USE_IMGUI

		void delRecursiveDraw() override;

	private:
		int windex{};

#endif
	};
}


struct Recorder {
	explicit Recorder() = default;
    long Record(TRACK* t, int32_t bufsize);
	long offset{};
	std::atomic<bool> _isRecording{};
	size_t gap{};
	uint32_t count{};
	TRACK* _track{};
	std::string message{};
	std::vector<recSampleFormat> filebuffer[MAX_CHANNELS]{};
	std::atomic<int> fileBufferSize{};
    std::mutex mutex;
	int slot{};
#ifdef __ANDROID__
	OboeRecorder oboeRecorder;
	int32_t fd{};
	int32_t recformat{};
#else
	tsl::RingBufferSPSCQueue<tsl::AudioBuffer<recSampleFormat>*, 128> queue;
#endif
};

#endif //GRAINSTORM_RECORDER_H
