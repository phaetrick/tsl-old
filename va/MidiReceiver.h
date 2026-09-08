//
// Created by pr on 18.01.19.
//

#ifndef GRAINSTORM_MIDIRECEIVER_H
#define GRAINSTORM_MIDIRECEIVER_H

#ifdef __ANDROID__
#include <jni.h>
void java_receive_midievent(JNIEnv *env, jclass obj, jbyte one, jbyte two, jbyte three);
#else
#include <cstdint>
namespace tsl { class AppState; }
extern void onMidiMsg(tsl::AppState* _appState, uint8_t one, uint8_t two, uint8_t three);
#endif

#endif //GRAINSTORM_MIDIRECEIVER_H
