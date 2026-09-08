//
// Created by pr on 24.01.19.
//

#ifndef GRAINSTORM_MIDISAVER_H
#define GRAINSTORM_MIDISAVER_H

#include <jni.h>

int setdummy1(void *_p);
int read_midiheader(JNIEnv *env, jclass thiz, jstring file, jobject obj);
void save_midimapping(void *_p);
void load_midimapping(void *_p);
jint save_midimapping_callback(JNIEnv *env, jclass thiz, jstring preset_name, jstring _path);
jint load_midimapping_callback(JNIEnv *env, jclass thiz, jint version, jstring _presetpath);


#endif //GRAINSTORM_MIDISAVER_H
