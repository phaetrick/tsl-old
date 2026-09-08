#ifndef InfoPanel_H
#define InfoPanel_H

#include <ctime>
#include <atomic>
#include <defines.h>
#include "types.h"
#include "view.h"

namespace tsl { namespace parameters { struct Event; } }

#define REC 0
#define MIC 1
#define STAND 2
#define VOID 3
#define TEXT 4
#define VALUE 5
#define XRUN 6
#define PROG 7
#define DECODE 8
#define LFOEDITOR 9
#define ENVEDITOR 10
#define INFO_EQ5 11
#define INFO_ADSR 12
#define RENDERBUFFER 13

//#define KILL_DELAY_THREAD  if(track->thread_setinfo){signal(SIGUSR2, thread_exit_handler); pthread_kill(track->thread_setinfo, SIGUSR2); pthread_join(track->thread_setinfo, NULL); track->thread_setinfo = NULL;}
//#define START_DELAY_THREAD  if(!track->thread_setinfo) { pthread_attr_t tattr; pthread_attr_init(&tattr); pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE); pthread_create(&(track->thread_setinfo), &tattr, (void *) infopanel_delayed_set,track->infopanel); pthread_attr_destroy(&tattr);}

namespace tsl {
    namespace graphics {


        class InfoPanel : public View {
        public:
            InfoPanel(tsl::AppState* appState, float scalefactor, int32_t aspect_ratio, int alignment);

            std::atomic<int64_t> tstamp{};

            void render(void *ctx) override ;

            void setRenderFunc(int func);

            void setTimeStamp();

            void init() override ;

            bool active = false;
            std::atomic_int xruncount{};
            float fontsize{};
            std::atomic<MYFLOAT> *ref{};
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
            std::atomic_uint16_t miditargetindex{};
            std::atomic<void (*)(InfoPanel *infopanel, void *context)> renderfunc{};
            std::atomic<const char *> text{};
            std::atomic<char *> progress_text{};
            std::atomic<float> progress{0};
            std::atomic<long> dec_offset{0};
            int windowindex{-1};

            char textBuffer[101]{};
            void (*prevRenderfunc)(InfoPanel*, void*){};

            void prepareBuffer(tsl::parameters::Event& ev);
        };

        class LoadInfo : public View {
        public:
            LoadInfo(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment) : View(appState, _scalefactor,
                                                                                   _aspect_ratio,
                                                                                   _alignment, 10,
                                                                                   true) {
            }

            void render(void *ctx) override;

            void init() override;

        private:
            int windowindex{-1};
        };

    }
}

#endif