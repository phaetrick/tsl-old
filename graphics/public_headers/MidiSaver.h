#pragma once
//
// Created by pr on 24.01.19.
//

#ifndef GRAINSTORM_MIDISAVER_H
#define GRAINSTORM_MIDISAVER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
namespace tsl{
    struct AppState;
}
struct MIDIHEADER {
    char header[3]{};
    int version{};
    char name[101]{};
    int64_t date{};  // fixed-width: plain `long` is 4 bytes on Windows (LLP64) vs
    int64_t offset{}; // 8 bytes on macOS/iOS/Android (LP64), which would misalign
                       // this on-disk format across platforms.
};

struct MIDIHEADER2{
    MIDIHEADER h;
    std::string path;
    std::string name;
};

void loadMidiMapping(tsl::AppState* , const std::shared_ptr<MIDIHEADER2>& h);
void getMappings(std::vector<std::shared_ptr<MIDIHEADER2>> &presets);

int setdummy1(void *_p);
void save_midimapping(void *_p);
int saveMappingGotFileName(tsl::AppState* _appState, const std::string& name);

#ifdef __ANDROID__
#include <jni.h>
int read_midiheader(JNIEnv *env, jclass thiz, jstring, jobject obj);
jint save_midimapping_callback(JNIEnv *env, jclass thiz, jstring preset_name);
#else
void saveMidimapping(tsl::AppState* _appState);
#endif
#endif //GRAINSTORM_MIDISAVER_H
