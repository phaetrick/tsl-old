#pragma once
//
// Created by pr on 19.03.24.
//

#ifndef GRAINSTORM_EDITOR_H
#define GRAINSTORM_EDITOR_H

#include <string>
#include <view.h>
#include <Input.h>
#include <atomic>
#include <memory>
#include <tools/AtomicSharedPtr.h>
#include <FloatingView.h>

struct TRACK;

namespace tsl {
	struct AppState;

	namespace graphics {

		class ListView : public FloatingView
		{
		public:
			ListView() = default;
			ListView(tsl::AppState* appState, int, int);
			void setUp(int, int);
			static void show(tsl::AppState* appState, tsl::AtomicSharedPtr<ListView>*, int min, int max);
			static void midiLearningEvent(tsl::AtomicSharedPtr<ListView>&);
		protected:
			int cb(float xpos, float ypos, int action, int)override;
			void renderContent(void*)override;
			void computeContent(int maxWidth, int maxHeight) override;
		private:
			float ypos{}, xpos{};
			float downX{}, downY{};
			std::atomic<int> hot{ -1 };
			int min_{}, max_{};
			tsl::AtomicTimer timer;
		};

	}
}


void loopToTrack(tsl::AppState *_appState, int32_t trackid, int alg);
extern void fadeinout(tsl::AppState*, int32_t trackid, ParameterNum alg);


#ifdef __ANDROID__
#include <jni.h>

extern int java_save_loop(JNIEnv *, jobject, jint, jlong);
#else
extern int32_t save_loop(tsl::AppState* _appState, TRACK* track);
#endif
#endif //GRAINSTORM_EDITOR_H
