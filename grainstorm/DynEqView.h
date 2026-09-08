//
// Created by pr on 09.08.26.
//
// Moved out of eq.h: this is a Skia GUI class and eq.h is a DSP header included
// by the audio path (track.cpp, preset.cpp, callbacks_fxpower.cpp), which do not
// otherwise need Skia.
//

#ifndef GRAINSTORM_DYNEQVIEW_H
#define GRAINSTORM_DYNEQVIEW_H

#include <view.h>
#include "app.h"
#include "grainstorm.h"
#include "colours.h"

namespace tsl::graphics {
    class DynEqView : public View {
    public:
        DynEqView(tsl::AppState *appState, MYFLOAT a, int32_t b, int c) : View(appState, a, b,
                                                                               c) {
            perm = true;
        }

        void render(void *ctx) override {
            auto tindex = _STATE->active_track.load();
            auto c = (SkCanvas *) ctx;
            if (_DATA->tracks[tindex]->fxpower[SPACE_DYNEQ5].load()) {
                flush(c);
                SkFont font(_STATE->font_normal);
                font.setSize(_STATE->textsize2 * .9f);
                SkPaint paint;
                paint.setAntiAlias(true);
                paint.setStrokeWidth(lw);
                const uint32_t cols[] = {skcol::blue_violet,
                                         skcol::blue, skcol::blue,
                                         skcol::blue,
                                         skcol::blue_violet};
                auto space = (int) _STATE->params[tindex][SPACEDYNEQ].load();
                for (int chan = 0; chan < _STATE->channels; chan++) {
                    auto env = _STATE->params[tindex][DYNEQ5ENV10 + space + chan * 5].load();
                    if (env != 0) env = LOG10D20(env);
                    else env = -12;
                    if (env > 12)
                        env = 12;
                    else if (env < -12)
                        env = -12;
                    env += 12;
                    env /= 24.f;
                    paint.setColor(cols[space]);
                    c->drawLine(startx + width * env,
                                starty + chan * (height / _STATE->channels),
                                startx + width * env,
                                starty + (chan + 1) * (height / _STATE->channels), paint);
                }
                if (_STATE->channels == 2) {
                    paint.setColor(skcol::bg);
                    c->drawLine(startx, height / 2 - lw2, stopx, height / 2 - lw2, paint);
                }
                drawRect(c);
            }
        }
    };
}

#endif //GRAINSTORM_DYNEQVIEW_H
