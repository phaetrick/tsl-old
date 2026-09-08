#ifndef _PRESET_H_
#define _PRESET_H_

#include <utility>
#include "va_types.h"
#include <tools/queuetsl.h>
#include <params.h>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <cstdint>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace tsl { struct AppState; }

namespace Preset {

    struct PresetParam {
        int num{};
        float val{};
    };

    struct Preset {
        std::string name;
        std::string path;
        int64_t date{};
        bool isSystem{};
        std::vector<PresetParam> values;
        std::vector<VcoPreNote> notes;
        std::vector<tsl::parameters::Event> midiEvents;
    };

    void setupDefault(tsl::AppState* _appState);
    void setupFactory(tsl::AppState* _appState);
    void readPresets(tsl::AppState* _appState);
    bool contains(tsl::AppState* _appState, int64_t date);
    bool get(tsl::AppState* _appState, Preset& preset, int64_t date);
    void loadPreset(tsl::AppState* _appState, int num, tsl::FastQueue<VcoPreNote>& notes);
    void savePreset(tsl::AppState* _appState, std::vector<PresetParam> vals, std::vector<VcoPreNote> notes);
    std::vector<uint8_t> serializeCurrentState(tsl::AppState* _appState);
    void unserializeState(tsl::AppState* _appState, const uint8_t* data, size_t size);
};

#ifdef __ANDROID__
jobjectArray java_read_presets(JNIEnv* env, jclass thiz);
jint java_delete_preset(JNIEnv* env, jclass thiz, jlong date);
#endif

#endif
