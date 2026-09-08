#include <logger.h>
#include <SkPath.h>
#include <include/core/SkFont.h>
#include "button.h"
#include "grainstorm.h"
#include "defines.h"
#include "player.h"
#include "synth.h"
#include "infopanel.h"
#include "knob.h"
#include "gui.h"
#include "view.h"
#include "tools.h"
#include "button.h"
#include "meter.h"
#include "buttonview.h"
#include "preset.h"
#include "selector.h"
#include "queue.h"
#include "tools/aligned_memalloc.h"
#include "ffttools.h"
#include "textview.h"
#include "callbacks_loop_controls.h"
#include "Input.h"


#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif

using namespace tsl::graphics;

NormalButton::NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                           const char *_normal,
                           const char *_pressed, int32_t _id, long off, int mul) : Button(
        appState, _scalefactor, _aspect_ratio, _alignment, _normal, _pressed) {
    id = _id;
    if (id != PARAM_NOT_ASSIGNED) {
        _STATE->parameters[id].view = this;
        _STATE->parameters[id].type = ParameterType_bool;
    }
    if (id != PARAM_NOT_ASSIGNED) {
        _STATE->parameters[id].paramOffset = off;
        _STATE->parameters[id].offsetFact = (mul == 0 ? 1 : mul);
    }
}


void NormalButton::callback(const InputEvent &event) {
    auto tindex = _STATE->active_track.load();
    int action = event.action;
    switch (action) {
        case ACTION_DOWN:
            down(event);
            setState(HOT);
            break;

        case ACTION_MOVE:
            check(event);
            break;

        case ACTION_UP:
            if (pointerid == event.pointer_id) {
                pointerid = -1;
                setState(NORMAL);
                if (id != PARAM_NOT_ASSIGNED) {
                    tsl::parameters::Event e;
                    e.setup(_STATE, tindex, id);
                    e.value = e.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
                    e.applyFromExt(_STATE, tsl::parameters::FromUi);
                }
            }
            break;
        default:
            break;
    }
}


void NormalButton::render(void *context) {
    auto tindex = _STATE->active_track.load();
    auto canvas = (SkCanvas *) context;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_md);

    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;
    flush(canvas);

    const bool hot = state.load() == HOT;
    bool pressed = _STATE->params[tindex][id + offset].load() == 1.0;

    if (hot) {
        paint.setColor(skcol::blue_transparent);
        float sizerect = height > width ? width : height;
        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx + (width - sizerect) * .5f,
                                 starty + (height - sizerect) / 2, sizerect, sizerect),
                sizerect * .1f, sizerect * .1f, paint);
    }
    paint.setColor(
            _STATE->midilearning.load() && id != PARAM_NOT_ASSIGNED
                                                         ? skcol::midilearning
                                                         : pressed ? skcol::custom_green : skcol::fg);
    View::textDisplayCentered(this, canvas, paint, font, name_normal, iconScale, false);
}

void RecordButton::render(void *context) {
    if (!_STATE->player.isrecording.load()) {
        NormalButton::render(context);
        return;
    }
    auto canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setAntiAlias(true);

    const bool hot = state.load() == HOT;
    if (hot) {
        paint.setColor(skcol::blue_transparent);
        float sizerect = height > width ? width : height;
        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx + (width - sizerect) / 2,
                                 starty + (height - sizerect) / 2, sizerect, sizerect),
                sizerect * .1f, sizerect * .1f, paint);
    }
    SkFont font(_STATE->font_md);
    paint.setColor(skcol::red);
    textDisplayCentered(this, canvas, paint, font, name_normal, 1.0, false);

    TIME_P time{};
    time_convert(time, _STATE->sr, _STATE->player.recoff);
    char text[30];
    snprintf(text, 30, "%02d : %02d", time.m, time.s);
    float fontsize, sx, sy;
    measureText(this, _STATE->font_normal, "00 : 00", .9f, &sx, &sy, &fontsize);
    font.setSize(fontsize);
    paint.setColor(skcol::pressed);
    font = _STATE->font_normal;
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx + sx,
                           starty + sy, font, paint);
}

void ButtonViewButton::render(void *context) {
    float startx_pos, starty_pos, fontsize;
    auto tindex = _STATE->active_track.load();
    const char *text =
            name_normal;
    auto canvas = (SkCanvas *) context;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_normal);
    auto w = measureTextFixed(width, height, _STATE->font_normal, text, &startx_pos, &starty_pos,
                              _STATE->textsize2 * .9f);
    if(w>width - _STATE->maxCharWidtht2)
        startx_pos = _STATE->maxCharWidtht2 * .5;
    font.setSize(_STATE->textsize2 * .9f);
    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[0][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;
    int val = _STATE->params[0][id + offset].load();
    bool active = val == userdata;
    flush(canvas);

    if (active || state == HOT)
        flush(canvas, skcol::blue_transparent);

    canvas->save();
    canvas->translate(startx, starty);
    paint.setColor(skcol::fg);
    SkPath path;
    path.addRect(SkRect::MakeXYWH(_STATE->maxCharWidtht2 * .5,0,width - lw,height));
    canvas->clipPath(path);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,  startx_pos, starty_pos,
                           font, paint);
    canvas->restore();
}

void PermButton::render(void *context) {
    float startx_pos, starty_pos, fontsize;
    const char *text =
            name_normal;
    auto canvas = (SkCanvas *) context;
    flush(canvas);

    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_normal);
    auto w = measureTextFixed(width, height, _STATE->font_normal, text, &startx_pos, &starty_pos,
                              _STATE->textsize2 * .9f);
    if(w>width - _STATE->maxCharWidtht2)
        startx_pos = _STATE->maxCharWidtht2 * .5;
    font.setSize(_STATE->textsize2 * .9f);

    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[0][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;
    int val = _STATE->params[0][id + offset].load();
    bool active = val == 1.;
    perm = active;
    flush(canvas);
    if (active || state == HOT)
        flush(canvas, skcol::blue_transparent);
    canvas->save();
    canvas->translate(startx, starty);
    const bool ml = (_STATE->parameters[id].flags & Param::MidiParam) && _STATE->midilearning.load();
    paint.setColor(ml ? skcol::midilearning : skcol::fg);
    SkPath path;
    path.addRect(SkRect::MakeXYWH(_STATE->maxCharWidtht2 * .5,0,width - lw,height));
    canvas->clipPath(path);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, innerAlignment == START_ALIGN ? _STATE->maxCharWidtht2 * .5 : startx_pos, starty_pos,
                           font, paint);
    canvas->restore();
}


void ButtonViewButton::computeWidth() {
    if (name_normal == nullptr)
        width = 0;
    else {
        int ts = _STATE->textsize2;
        SkFont font(_STATE->font_normal);
        font.setSize(ts);
        SkRect bounds{};
        int length = strlen(name_normal);
        font.measureText(name_normal, length, SkTextEncoding::kUTF8, &bounds);
        if (bounds.width() + ts > width)
            width = bounds.width() + ts;
    }
}

void BypassOffButton::render(void *context) {
    auto canvas = (SkCanvas *) context;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_md);
    flush(canvas);
    if (state == DISABLED) {
        return;
    }
    bool active = false;

    switch (id) {
        case POWERButton:
            active = _STATE->player._isplaying.load();
            break;
        case POWERTRACK:
            active = _STATE->params[0][POWERTRACK].load() == 1.0;
            break;
        default:
            active = _STATE->params[0][id].load() == 1.0;
            break;
    }

    bool midilearning = _STATE->midilearning.load();
    const bool hot = state.load() == HOT;
    if (hot) {
        paint.setColor(skcol::blue_transparent);
        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx, starty, width, height),
                height * .1f, height * .1f, paint);
    }
    if (midilearning)
        paint.setColor(skcol::midilearning);
    else if (active)
        paint.setColor(skcol::custom_green);
    else
        paint.setColor(skcol::fg);


    textDisplayCentered(this, canvas, paint, font,
                        active ? name_pressed
                               : name_normal,
                        1.0, false);
}