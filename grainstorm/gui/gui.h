#pragma once

struct TRACK;
namespace tsl {
	struct AppState;
}

#include <cstdint>


bool gui_setup(tsl::AppState* _appState);

bool progress(double progress, const char *message);


#ifdef __ANDROID__
#include <jni.h>
void guiSetup(JNIEnv *env, jclass obj, jboolean fastrender, jint screenWidth, jint screenHeight);
#endif
