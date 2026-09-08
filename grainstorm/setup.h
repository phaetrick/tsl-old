#pragma once
#ifndef SETUP_H
#define SETUP_H
#include <types.h>
void first_setup();
void handler();
void crashHandler();
void initFailed();
void OnGotSampleRate(tsl::AppState*);
void cleanUp(tsl::AppState*);
void sampleRateFromApp(tsl::AppState*, double sr);
void startSnapshotDrainThread(tsl::AppState*);
void stopSnapshotDrainThread(tsl::AppState*);

#endif