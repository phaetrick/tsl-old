//
// Created by pr on 24.01.19.
//

#include <logger.h>
#include "MidiSaver.h"
#include "grainstorm.h"
#include <jni.h>
#include <Midi.h>
#include <app.h>
#include <tools/PlatformPaths.h>
#include <sstream>

using namespace tsl::midi;

struct MIDIHEADER {
    char header[3]{};
    int version{};
    char name[101]{};
    int64_t date{};  // fixed-width: plain `long` is 4 bytes on Windows (LLP64) vs
    int64_t offset{}; // 8 bytes on macOS/iOS/Android (LP64), which would misalign
                       // this on-disk format across platforms.
};


struct MIDIMAPPING1 {
    char header[3]{};
    int version{};
    char name[101]{};
    int64_t date{};
    int64_t offset{};
    MidiAssignment midiassignmentscontrol[NUM_MIDICHANNELS][NUM_MIDI_CONTROL]{};
    MidiAssignment midiassignmentsnote[NUM_MIDICHANNELS][NUM_MIDI_NOTEON]{};
};


int read_midiheader(JNIEnv *env, jclass thiz, jstring file, jobject obj) {
    MIDIHEADER pr;
    const char *temp = (char *) env->GetStringUTFChars(file, 0);
    if (temp == nullptr)
        return 0;
    std::ostringstream ss;
    ss << temp;
    env->ReleaseStringUTFChars(file, temp);

    std::string s(ss.str());

    const char *filename = s.c_str();
    FILE *fd = fopen(filename, "rb");
    if (!file) {
        LOGE("READ HEADER: Could not open input file.");
        return 0;
    }
    size_t bytes = 3 + sizeof(int);
    if (fread(&pr, 1, bytes, fd) != bytes) {
        LOGE("Error reading header");
        fclose(fd);
        return 0;
    }
    if (strcmp(pr.header, "GMM")) {
        LOGE("No preset file");
        fclose(fd);
        return 0;
    }
    rewind(fd);
    if (fread(&pr, 1, sizeof(MIDIHEADER), fd) != sizeof(MIDIHEADER)) {
        LOGE("Error reading preset");
        fclose(fd);
        return 0;
    }
    fclose(fd);
    jclass clazz = env->GetObjectClass(obj);
    if (0 == clazz) {
        LOGE("GetObjectClass returned 0.");
        return 0;
    }
    //jfieldID fid = (*env)->GetFieldID(env, clazz, "version", "I");
    //(*env)->SetIntField(env, obj, fid, pr->version);
    jfieldID fid = env->GetFieldID(clazz, "time", "J");
    env->SetLongField(obj, fid, pr.version >= 2 ? pr.date : 0);
    fid = env->GetFieldID(clazz, "version", "I");
    env->SetIntField(obj, fid, pr.version);
    fid = env->GetFieldID(clazz, "name", "Ljava/lang/String;");
    auto estr = (jstring) env->NewStringUTF(pr.name);
    env->SetObjectField(obj, fid, estr);
    return 1;
}

jint save_midimapping_callback(JNIEnv *env, jclass thiz, jstring _name, jstring _path) {
    MIDIMAPPING1 midimapping;
    strncpy(midimapping.header, "GMM", 3);
    midimapping.version = 3;
    struct timespec spec{};
    clock_gettime(CLOCK_REALTIME, &spec);
    midimapping.date = spec.tv_sec;
    jboolean isCopy;
    const char *name = env->GetStringUTFChars(_name, &isCopy);
    LOGI("save_midimapping_callback preset name: %s", name);
    snprintf(midimapping.name, 100, "%s", name);
    env->ReleaseStringUTFChars(_name, name);
    midimapping.offset =
            (NUM_MIDICHANNELS * NUM_MIDI_CONTROL + NUM_MIDICHANNELS * NUM_MIDI_NOTEON) *
            sizeof(MidiAssignment);
    memcpy(midimapping.midiassignmentscontrol, __STATE->midiassignmentscontrol,
           NUM_MIDICHANNELS * NUM_MIDI_CONTROL * sizeof(MidiAssignment));
    memcpy(midimapping.midiassignmentsnote, __STATE->midiassignmentsnote,
           NUM_MIDICHANNELS * NUM_MIDI_NOTEON * sizeof(MidiAssignment));

    char filename[4096];
    if (nullptr == _path) {
        //struct timespec time;
        //clock_gettime(CLOCK_REALTIME, &time);
        auto preset_dir = tsl::app::getStoragePath("midimappings");
        snprintf(filename, 4096, "%s/%lld", preset_dir.c_str(), tsl::time::millisecondsSinceEpoch());
    } else {
        const char *path = env->GetStringUTFChars(_path, &isCopy);
        snprintf(filename, 4096, "%s", path);
        env->ReleaseStringUTFChars(_path, path);
    }
    FILE *fd = fopen(filename, "wb");
    if (!fd) {
        LOGE("save_midimapping_callback: Could not open output file.");
        showToast( "Save midimapping error: Could not open output file.");
        return -1;
    }
    if (fwrite(&midimapping, 1, sizeof(MIDIMAPPING1), fd) != sizeof(MIDIMAPPING1)) {
        LOGE("Error writing midimapping");
        showToast( "Save midimapping: Write error.");
        fclose(fd);
        return -1;
    }
    LOGI("midimapping saved as: %s", filename);
    fclose(fd);
    return 0;
}

jint load_midimapping_callback(JNIEnv *env, jclass thiz, jint version, jstring _presetpath) {

    jboolean isCopy;
    const char *presetpath = env->GetStringUTFChars(_presetpath, &isCopy);
    FILE *fd = fopen(presetpath, "rb");
    if (!fd) {
        LOGE("load_midimapping_callback: Could not open preset file.");
        env->ReleaseStringUTFChars(_presetpath, presetpath);
        return -1;
    }
    env->ReleaseStringUTFChars(_presetpath, presetpath);

    LOGI("midimappingcallback v%d presetpath : %s", version, presetpath);

    std::unique_ptr<MIDIMAPPING1> pr = std::unique_ptr<MIDIMAPPING1>(new MIDIMAPPING1);
    if (fread(pr.get(), 1, sizeof(MIDIMAPPING1), fd) != sizeof(MIDIMAPPING1)) {
        LOGE("Error reading midimapping");
        fclose(fd);
        return -1;
    }
    __STATE->mutex_midi.lock();

    memcpy(__STATE->midiassignmentsnote, pr.get()->midiassignmentsnote,
           NUM_MIDICHANNELS * NUM_MIDI_NOTEON * sizeof(MidiAssignment));
    memcpy(__STATE->midiassignmentscontrol, pr.get()->midiassignmentscontrol,
           NUM_MIDICHANNELS * NUM_MIDI_CONTROL * sizeof(MidiAssignment));

    __STATE->mutex_midi.unlock();

    showToast( "Midimapping imported.");
    return 0;
}

