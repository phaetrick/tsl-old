#pragma once
//
// Created by pr on 15.11.17.
//
#ifndef GRAINSTORM_DECODER_H
#define GRAINSTORM_DECODER_H
#include "types.h"
#include "audio/Recording.h"
#include <string>
#include <memory>

#include <jni.h>
struct TRACK;
jint filebrowsercallback(JNIEnv *env, jclass thiz, jstring _filename);
jstring java_ffmpegerrortostring(JNIEnv*env, jclass obj, int32_t errorCode);
std::shared_ptr<tsl::Recording> dec(TRACK *track, std::string name, long gap, bool showMsg = true);
#endif
