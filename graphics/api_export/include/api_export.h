#pragma once

// DLL Export/Import macro
#ifdef TSL_API_EXPORTS
#if defined(_WIN32)
#define TSL_API_EXPORT __declspec(dllexport)
#else
#define TSL_API_EXPORT __attribute__((visibility("default")))
#endif
#else
#if defined(_WIN32)
#define TSL_API_EXPORT __declspec(dllimport)
#else
#define TSL_API_EXPORT
#endif
#endif

// Calling convention
#if defined(_WIN32)
#define TSL_API_CALL __cdecl
#else
#define TSL_API_CALL
#endif

#include "InputEvent.h"
#include "params.h"

// C API - no name mangling, flat names
extern "C" {
    TSL_API_EXPORT  void TSL_API_CALL  TSL_ProcessAudio(double** inputs, double** outputs, int numSamples, int mask);
    TSL_API_EXPORT  void TSL_API_CALL  TSL_Setup();
    TSL_API_EXPORT  void TSL_API_CALL  TSL_Cleanup();
    TSL_API_EXPORT  void TSL_API_CALL  TSL_OnViewInitialized();
    TSL_API_EXPORT  void TSL_API_CALL  TSL_OnViewDestroyed();
    TSL_API_EXPORT  void TSL_API_CALL  TSL_DrawResize(int w, int h);
    TSL_API_EXPORT  void TSL_API_CALL  TSL_BeginFrame(int w, int h);
    TSL_API_EXPORT  void TSL_API_CALL  TSL_EndFrame();
    TSL_API_EXPORT  bool TSL_API_CALL  TSL_CallbackInput(tsl::graphics::InputEvent& ev);
    TSL_API_EXPORT  void TSL_API_CALL  TSL_Inc(int);
    TSL_API_EXPORT  int TSL_API_CALL  TSL_Get();
    TSL_API_EXPORT  Param& TSL_API_CALL  TSL_GetParam(int num);
    TSL_API_EXPORT  double TSL_API_CALL  TSL_GetSampleRate();
#ifdef PLUGIN_MODE
    TSL_API_EXPORT  void TSL_API_CALL  TSL_SetInformHostOfParamChange(std::function<void(int tindex, int idx, double normalizedValue)>);
    TSL_API_EXPORT  void TSL_API_CALL  TSL_SetBeginEndInformHostOfParamChange(std::function<void(int tindex, int idx, bool end)>);
#endif
}

namespace tsl
{
	struct AppState;
    AppState* getInstance();
}


