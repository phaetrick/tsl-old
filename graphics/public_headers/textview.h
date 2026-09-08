#pragma once
#ifndef _TextView_H
#define _TextView_H

#include "types.h"
#include "colours.h"
#include "view.h"
#include <cstdint>

namespace tsl {
    namespace graphics {
        class TextView : public View {
        public:
            TextView(tsl::AppState* appState, const char *title = "T", uint32_t colfg = skcol::fg,
                     uint32_t colbg = skcol::bg) : View(appState) {
                text = title;
                fg = colfg;
                bg = colbg;
            };

            TextView(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, const char *_text,
                     uint32_t _bg = skcol::bg, uint32_t _fg = skcol::fg,
                     float _textpadding = .9, bool _frame = false);

            virtual void computeWidth()override ;

            virtual void render(void *ctx)override ;

            void setState(int state, bool redraw = true);

            void setText(const char *text, bool redraw = false);

            const char *text = nullptr;
            float textpadding = 0;
            bool frame = false;
            std::atomic<int> fg = 0, bg = 0;
            int textalignhoz = 0, textalignvert = 0;
        };

        class DropDownView : public TextView{
        public:
            DropDownView(tsl::AppState* appState, const char *text, int alignment, Layout *parent = nullptr);
            void computeWidth()override ;
            void render(void*)override ;
        };

        class TitleView : public TextView {
        public:
            TitleView(tsl::AppState* appState, const char *text, int alignment, Layout *parent = nullptr);
        };

#ifdef GRAINSTORM

        class PDectectTV : public TextView {
        public:
            PDectectTV(tsl::AppState* appState, int alignment, Layout *parent = nullptr);

            void render(void *ctx);

        private:
            int64_t _time;
        };

        class PDetectTVGrain : public TextView {
        public:
            PDetectTVGrain(tsl::AppState* appState, int alignment, Layout *parent = nullptr);

            void render(void *ctx);

        private:
            int64_t _time;
        };

#endif
    }
}

#endif