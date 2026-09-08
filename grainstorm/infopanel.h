#pragma once

#include <ctime>
#include <atomic>
#include "types.h"
#include <defines.h>
#include "view.h"


namespace tsl { 
    namespace parameters {
        struct Event;
    }
    
    
    namespace graphics {
    
    class InfoPanel : public View {
    public:
        enum RenderFunc {
            RenderMic = 0,
            RenderStand,
            RenderVoid,
            RenderText,
            RenderParam,
            RenderXRun,
            RenderProg,
            RenderDecode,
            RenderEncode,
            RenderBuffer
        };

		using RenderFuncInfoPanel = void (InfoPanel* infopanel, void* context);

        InfoPanel(tsl::AppState *, float scalefactor, int32_t aspect_ratio, int alignment);

        std::atomic<uint64_t> tstamp{};

        void render(void *ctx) override ;

        void init() override ;

        void setRenderFunc(RenderFunc func);

        void setTimeStamp();

        void prepareBuffer(tsl::parameters::Event& ev);

        bool active = false;
        std::atomic_int32_t xruncount{};
        float fontsize{};
        std::atomic<std::atomic<MYFLOAT>*> ref{};
        float starty_pos = 0;
        float startx_pos1 = 0;
        float startx_pos2 = 0;
        float startx_pos3 = 0;
        float startx_pos4 = 0;
        float startx_mic = 0;
        float startx_rec = 0;
        float startx_decode = 0;
        float startx_render = 0;
        float startx_warning = 0;
        float startx_poweroff = 0;
        std::atomic<uint16_t> miditargetindex{0};
        std::atomic<RenderFuncInfoPanel*> renderfunc{};
        RenderFuncInfoPanel *prevRenderfunc{};
        std::atomic<const char *> text{"TRACK1"};
        std::atomic<const char *> text2{" ENCODING: "};
        std::atomic<float> progress{0};
        std::atomic<long> dec_offset{0};
        std::atomic<long> bytes{0};
        SkFont font;
        char textBuffer[101]{};
    };
} 
}
