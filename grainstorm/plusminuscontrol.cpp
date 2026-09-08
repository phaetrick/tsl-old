#include "logger.h"
#include <cstdlib>
#include <cstring>
#include <include/core/SkFont.h>
#include "plusminuscontrol.h"
#include "textview.h"
#include "defines.h"
#include "button.h"
#include "view.h"
#include "tools.h"
#include "grainstorm.h"
#include "colours.h"
#include "setup.h"
#include "track.h"
#include "synth.h"
#include "lfo.h"
#include "Input.h"
#include "envelope_window.h"
#include "EnterValue.h"
#include "app.h"

using namespace tsl::graphics;


void PlusMinusControlQuant::render(void *context) {
    auto *canvas = (SkCanvas *) context;
    flush(canvas);
    view_title.render(context);
    plus.render(context);
    minus.render(context);
    auto tindex = _STATE->active_track.load();
    char output[20];
    std::atomic<MYFLOAT> *ref;
    if (id == LFO1QUANT)
        ref = &_STATE->params[tindex][LFO1QUANT + GASLFO * LFONUMPARAMS];
    else if (id == GRAINQUANT)
        ref = &_STATE->params[tindex][GRAINQUANT];
    int32_t value = (int) (1. / (float) *ref);
    snprintf(output, 20, "1/%d", value);


    int32_t height_rect = (int) (height - view_title.height - plus.height);

    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::fg);

    float startxtext, startytext;

    measureTextFixed(width, height_rect, _STATE->font_normal, output,
                     &startxtext, &startytext, _STATE->textsize2);
    //measureText2(view->width, height_rect, view->_DATA->_STATE->font_normal, output, .33f, &startx, &starty, &fontsize);
    SkFont font(_STATE->font_normal);

    font.setSize(_STATE->textsize2);
    canvas->save();
    canvas->translate(startx, starty + view_title.height);
    canvas->drawSimpleText(output, strlen(output), SkTextEncoding::kUTF8, startxtext, startytext,
                           font,
                           paint);
    canvas->restore();
}

void PlusMinusControlQuant::callback(const InputEvent &event) {
    int32_t action = event.action;
    float xpos = event.x - startx;
    float ypos = event.y - starty;
    auto tindex = _STATE->active_track.load();
    int32_t pointerid = event.pointer_id;
    std::atomic<MYFLOAT> *ref = nullptr;
    if (id == LFO1QUANT)
        ref = &_STATE->params[tindex][LFO1QUANT + GASLFO * LFONUMPARAMS];
    else if (id == GRAINQUANT)
        ref = &_STATE->params[tindex][GRAINQUANT];

    switch (action) {
        case ACTION_DOWN:
            if (ypos < view_title.height) {
                which = TITLEView;
                view_title.setState(HOT);
                pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, TITLEView);

            } else if (ypos < height - plus.height)
                pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, VALUEView);
            else {
                if (xpos >= width * .5f) {
                    plus.setState(HOT);
                    pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, PLUSView);
                } else {
                    minus.setState(HOT);
                    pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, MINUSView);
                }
            }
            break;

        case ACTION_MOVE: {
            if (auto pt = pointers.getById(event.pointer_id)) {
                if (pt->target == PLUSView &&
                    pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    plus.setState(NORMAL);
                    pointers.removePointer(event.pointer_id);
                } else if (pt->target == MINUSView &&
                           pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    minus.setState(NORMAL);
                    pointers.removePointer(event.pointer_id);
                } else if (pt->target == TITLEView &&
                           pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    view_title.setState(NORMAL);
                    pointers.removePointer(event.pointer_id);
                } else if (pt->target == VALUEView &&
                           pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    pointers.removePointer(event.pointer_id);
                }
            }
            break;
        }
        case ACTION_UP: {
            if (auto pt = pointers.getById(event.pointer_id)) {
                if (pt->target == TITLEView) {
                    view_title.setState(NORMAL, false);
                    showToast(_STATE, "No direct input for this parameter.");
                    //ADD_WORK3(entervaluetasklfoquant, v);
                } else if (pt->target == VALUEView) {
                    if (pointers.timer.elapsedReplace() < .3) {
                        _DATA->tracks[tindex]->lfos[GASLFO]->store(LFOQUANT, 1.);
                    }
                } else if (pt->target == MINUSView) {
                    auto value = *ref * .5;
                    if (value < 0.0625)
                        value = 0.0625;
                    tsl::parameters::Event e;
                    e.setup(_STATE, tindex, id == LFO1QUANT ? LFO1QUANT + GASLFO * LFONUMPARAMS : id);
                    e.value = value;
                    e.apply(_STATE, tsl::parameters::FromUi);
                    minus.setState(NORMAL, false);
                } else if (pt->target == PLUSView) {
                    auto value = *ref * 2.;
                    if (value > 1)
                        value = 1;
                    tsl::parameters::Event e;
                    e.setup(_STATE, tindex, id == LFO1QUANT ? LFO1QUANT + GASLFO * LFONUMPARAMS : id);
                    e.value = value;
                    e.apply(_STATE, tsl::parameters::FromUi);
                    plus.setState(NORMAL, false);
                }
                redraw();
            }
            break;
        }
        default:
            break;
    }
}


