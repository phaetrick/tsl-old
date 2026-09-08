#pragma once
#ifndef SYNTH_H
#define SYNTH_H


#include <tools/threadtsl.h>
#include <tools/AudioSemaphore.h>
#include "tools.h"

class TRACK;
namespace tsl
{
    struct AppState;
}
namespace tsl::synth {
    void synth(int);
#ifdef IS_MULTITHREADED

    class ChannelThread : public tsl::Thread {
    public:
        ChannelThread(TRACK* t, int chan)  {
            track = t, channel = chan;
        };

        void run() override;

        void stop() override;

        TRACK *track{};
        int channel{};
        tsl::BinarySemaphore sem{};
    };
#if defined __ANDROID__
    class SynthThread : public tsl::Thread {
    public:
        explicit SynthThread(tsl::AppState* appState) : _appState(appState) {};
        void run() override;
        void stop() override;
		tsl::AppState* _appState{ nullptr };
    };
#endif
#endif
} 

#ifdef __ANDROID__
#include <jni.h>
int32_t java_record_loop(JNIEnv *env, jclass obj, jint fd, jlong);
#endif
#include <player.h>
int record_loop(TRACK* track, const std::shared_ptr<tsl::Player::RecordingContext>&);

#endif