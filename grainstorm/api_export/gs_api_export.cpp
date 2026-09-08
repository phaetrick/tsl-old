#include "gs_api_export.h"
#ifdef _WIN32
#include "DecoderWindows.h"
#else
#endif

#include "MidiReceiver.h"
#include "preset.h"
#include "IPlugParamDefs.h"
#include "grainstorm.h"
#include <callbacks.h>
#include <setup.h>
#include <functional>

extern "C" {
    TSL_API_EXPORT void TSL_API_CALL GS_Cleanup() {
		auto app = tsl::getInstance();
		delete app->data;
    };

    TSL_API_EXPORT void TSL_API_CALL TSL_OnMidiMessage(uint8_t one, uint8_t two, uint8_t three) {
        onMidiMsg(tsl::getInstance(), one, two, three);
    };
    TSL_API_EXPORT void TSL_API_CALL TSL_OnDrop(const char* str) {
        filebrowsercallback(tsl::getInstance(), str);
    }
    TSL_API_EXPORT bool TSL_API_CALL TSL_save_preset(std::vector<unsigned char>& destinationBuf, std::string name, const bool isProject) {
        return save_preset(tsl::getInstance(), destinationBuf, name, isProject);
    };
    TSL_API_EXPORT bool TSL_API_CALL TSL_load_preset(unsigned char* data, int size, bool async) {
        return load_preset(tsl::getInstance(), data, size, async);
    }
    TSL_API_EXPORT void TSL_API_CALL TSL_ChangeValue(int paramIdx, double value) {
		auto _appState = tsl::getInstance();
        int index = paramIdx % tsl::iplug::paramCount;
        auto tindex = static_cast<int>(floor(paramIdx / tsl::iplug::paramCount));

        auto& p = tsl::iplug::dp[index];
        auto& par = _STATE->parameters[p.num];
        if (par.type == ParameterType_bool)
        {
            if (p.flags & tsl::iplug::FlagOneShot)
            {
                switch (p.num)
                {
                case STEPBACK:
                case STEPFORW:
                case STOPButton:
                case PLAYButton:
                case DIRButton:
                case SLOWButton:
                case FASTButton:
                    if (_DATA->inputdisabled.load(std::memory_order_acquire))
                        return;
                    else if (_STATE->player.isPlaying())
                    {
                        TRACK* distr = _DATA->tracks[(int)_STATE->params[index][DISTRSOURCE].load()];

                        _DATA->toAudioThreadQueue.try_push([distr, num = p.num]() { distr->controlAll(num); });
                        break;
                    }
                }
            }
            else
            {
                changeValue(_STATE, p.num, value != 0. ? 1. : 0., tindex);
            }
        }
        else if (par.type == ParameterType_double)
        {
            changeValue(_STATE, p.num, par.fromDisplay(_STATE->sr, value), tindex);
        }
        else if (par.type == ParameterType_enum)
        {
            changeValue(_STATE, p.num, par.fromDisplay(_STATE->sr, value), tindex);
        }
    };

    TSL_API_EXPORT void TSL_API_CALL TSL_SampleRateFromApp(double sr) {
		sampleRateFromApp(tsl::getInstance(), sr);
    };


}