#pragma once

namespace tsl { struct AppState; }
struct TRACK;

namespace tsl::aigen {

// True when this build carries the text-to-sample engine (GS_AIGEN) and the
// model directory exists on disk.
bool available();

// "RECORD AI->TRACK": prompt popup on the modal thread, generation on the
// RecordingQueue, push onto `track` through the snapShot worker. Safe to call
// from the UI thread; returns immediately.
void recordToTrack(tsl::AppState* _appState, TRACK* track);

}
