//
// Created by pr on 23.07.25.
//

#ifndef GRAINSTORM_PRESETDEF_H
#define GRAINSTORM_PRESETDEF_H

struct TRACK;

#include <vector>
#include <FileWrapper.h>
#include <audio/Recording.h>
#include "preset.h"

#include <cstdint>

inline constexpr const char* errorReadingPreset = "Error reading preset.";


enum ParameterTypePreset {
    NormalParam = 0,
    LFOmin,
    LFOmax
};

bool sortbysec(const std::pair<int, int> &a,
               const std::pair<int, int> &b);






struct PRESET_HEADER {
    char header[3]{};
    int32_t version{};
    char name[100]{};
    bool import_audio{};
    long date{};
    uint8_t num_track{};
    char audio_file_path[4096]{};
};

struct PresetHeader20 {
    PresetHeader20(const bool isProject = false) {
        strncpy(header, isProject ? "GPR" : "GSP", 3);
        date = static_cast<long>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
        version = 21;
    }

    char header[3];
    int32_t version;
    long date;
    long off;
    double off_start, off_stop, offset;
    int8_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    uint16_t env1, env2, env3, asgrenv, powertrack, distr;
    uint16_t numparams, numfx, numbypass, numlfo1targets, numlfo2targets, numlfo3targets, namelen, audiofilepathlen;
};

struct Preset17 {
    Preset17() { std::memset(this, 0, sizeof(Preset17)); }

    char header[3];
    int32_t version;
    char name[100];
    bool import_a;
    long date;
    uint8_t num_track;
    char audio_file_path[4096];
    int32_t numparameters;
    long off;
    long double off_start, off_stop, offset;
    int32_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    float fxvalues[4000];
    float lfomin[4000];
    float lfomax[4000];
    bool bypass[1000];
    bool fxpower[1000];
    int32_t q_pos[1000];
};

struct Preset18 {
    Preset18(const bool isProject = false) {
        std::memset(this, 0, sizeof(Preset18));
        strncpy(header, isProject ? "GPR" : "GSP", 3);
        version = 18;
        numparameters = NUM_PARAMETERS ;
        numfx = NUM_PARAMSPACES;
        date = static_cast<long>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
    }

    char header[3];
    int32_t version;
    char name[100];
    bool import_a;
    long date;
    uint8_t trackindex;
    char audio_file_path[4096];
    int32_t numparameters;
    long off;
    long double off_start, off_stop, offset;
    int32_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    int32_t numfx;
};

struct Preset19 {
    Preset19(const bool isProject = false) {
        std::memset(this, 0, sizeof(Preset19));
        strncpy(header, isProject ? "GPR" : "GSP", 3);
        version = 19;
        numparameters = NUM_PARAMETERS;
        numfx = NUM_PARAMSPACES;
        date = static_cast<long>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
    }

    char header[3];
    int32_t version;
    char name[100];
    bool import_a;
    long date;
    uint8_t tracki;
    char audio_file_path[4096];
    int32_t numparameters;
    long off;
    long double off_start, off_stop, offset;
    int32_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    int32_t numfx;
    int32_t lfo1targets, lfo2targets, lfo3targets;
};

struct PresetHeader2023 {
    PresetHeader2023(const bool isProject = false) {
        strncpy(header, isProject ? "GPR" : "GSP", 3);
        date = static_cast<long>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
        version = 22;
        namelen = audioPathLen = 0;
    }
    char header[3];
    int32_t version;
    int64_t date;
    int64_t off;
    uint16_t namelen, audioPathLen;
};

struct Preset20 {
    double off_start, off_stop, offset;
    int8_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    uint16_t env1, env2, env3, asgrenv, powertrack, distr;
    uint16_t numparams, numfx, numbypass, numlfo1targets, numlfo2targets, numlfo3targets;
    int64_t end;
};

struct Preset21 {
    double samplerate, off_start, off_stop, offset;
    int8_t playbackspeed_dir, bounce_type;
    float waveformzoom, waveformpos;
    uint16_t powertrack, distr;
    uint16_t numparams, numfx, numbypass, numlfo1targets, numlfo2targets, numlfo3targets, numfollowers;
    int64_t end;
};
 struct Preset22{
     size_t numEvents{};
     size_t end{};
 };

void load_preset17(TRACK *t, Preset17 *pr);
void load_preset18(TRACK *t, tsl::preset::PresetWrapper &ptl);
void load_preset19(TRACK *t, tsl::preset::PresetWrapper &ptl);
void load_preset20(TRACK *t, tsl::preset::PresetWrapper &ptl);
void load_preset21(TRACK *t, tsl::preset::PresetWrapper &ptl);
#endif //GRAINSTORM_PRESETDEF_H
