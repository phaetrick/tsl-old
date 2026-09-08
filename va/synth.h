#ifndef SYNTH_H
#define SYNTH_H
#include <vector>
#include <defines.h>
namespace tsl{
    struct AppState;
}
void synth_main_thread();
void onGotSampleRate(tsl::AppState* _appState, int sampleRate);
// Re-takes the per-block parameter snapshot every voice is built from. Call it on the
// audio thread after anything rewrites params[0] mid-block (see synth.cpp).
void refreshSynthParams(tsl::AppState* _appState);
#endif