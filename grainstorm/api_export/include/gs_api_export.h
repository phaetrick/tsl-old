#pragma once
#include "api_export.h"
#include <vector>
#include <string>

// C API - no name mangling, flat names
extern "C" {
    TSL_API_EXPORT  bool TSL_API_CALL TSL_save_preset(std::vector<unsigned char>& destinationBuf, std::string name, const bool isProject = true);
    TSL_API_EXPORT  bool TSL_API_CALL TSL_load_preset(unsigned char* data, int size, bool async = false);
    TSL_API_EXPORT  void TSL_API_CALL TSL_OnMidiMessage(uint8_t one, uint8_t two, uint8_t three);
    TSL_API_EXPORT  void TSL_API_CALL TSL_ChangeValue(int, double);
    TSL_API_EXPORT  void TSL_API_CALL TSL_SampleRateFromApp(double);
    TSL_API_EXPORT void TSL_API_CALL TSL_OnDrop(const char* str);
    TSL_API_EXPORT void TSL_API_CALL GS_Cleanup();

}

