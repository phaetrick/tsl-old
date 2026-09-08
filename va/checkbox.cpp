//
// Created by pr on 01.10.19.
//

#include <logger.h>
#include <defines.h>
#include <view.h>
#include <tools.h>
#include <textview.h>
#include <include/core/SkPaint.h>
#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
#include <Input.h>
#include <app.h>
#include "callbacks_loop_controls.h"
#include "checkbox.h"
#include <fonts/IconsMaterialDesignReduced.h>
using namespace tsl::graphics;


void CheckBox::down(const InputEvent &e) {
    xpos = e.x;
    ypos = e.y;
    pointerid = e.pointer_id;
}


void CheckBoxView::render(void *context) {
    auto canvas = (SkCanvas *) context;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_md);
    auto tindex = _STATE->active_track.load();

    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;

    bool checked = _STATE->params[tindex][id + offset].load() == 1.0;
    float textsize = (height) < (2 * _STATE->textsize2 ) ? height.load() : 2 * _STATE->textsize2;
    if (textsize > width)
        textsize = width;
    font.setSize(textsize);
    flush(canvas);

    const bool hot = state.load() == HOT;

    if (hot) {
        paint.setColor(skcol::bghot);

        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx + (width - textsize) * .5f,
                                 starty + (height - textsize) * .5f,
                                 textsize, textsize),
                textsize * .1f, textsize * .1f, paint);
    }
    paint.setColor(hot ? skcol::fghot : skcol::fg);
    textDisplayCentered(this, canvas, paint, font,
                        checked ? reinterpret_cast<const char *>(ICON_MD_CHECK_BOX)
                                : reinterpret_cast<const char *>(ICON_MD_CHECK_BOX_OUTLINE_BLANK),
                        1.0, false);
}

void CheckBox::render(void *context) {
    auto canvas = (SkCanvas *) context;
    SkPath path;
    path.addRect(SkRect::MakeXYWH(startx, starty, width, height),
                 SkPathDirection::kCW);
    canvas->save();
    canvas->clipPath(path);
    tv.fg = _STATE->midilearning.load() ? skcol::midilearning : skcol::fg;
    tv.render(context);
    checkbox.render(context);
    canvas->restore();
}
/*
View *view_create(DATA *p, const char *name, int prio, bool perm, int orientation,
					  double scalefactor, uint8_t aspect_ratio, int alignment, void (*init)(View *view), void (*destroy)(View *view),
					  void (*callback)(View *view, AInputEvent *event), void (*render)(View *view, void *context), void (*flush)(View *view, unsigned int mode, Colour *colour));
*/

// View* textview_create(DATA *p, const char *text, float textpadding, bool frame, uint32_t bg, uint32_t fg, int prio, bool perm, int orientation, double scalefactor, uint8_t aspect_ratio, int alignment);
void CheckBox::callback(const InputEvent &event) {
    auto tindex = _STATE->active_track.load();
    int action = event.action;

    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;

    bool pressed = _STATE->params[tindex][id + offset].load() == 1.0;

    switch (action) {
        case ACTION_DOWN:
            down(event);
            checkbox.state = HOT;
            redraw();
            break;
        case ACTION_MOVE:
            if (spacing(xpos, event.x, ypos, event.y) > _STATE->textsize2) {
                pointerid = -1;
                checkbox.state = NORMAL;
                redraw();
            };
            break;
        case ACTION_UP:
            if (event.pointer_id == pointerid) {
                pointerid = -1;
                checkbox.state = NORMAL;
                redraw();
                tsl::parameters::Event e;
                e.setup(_STATE, tindex, id);
                e.value = e.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
                e.applyFromExt(_STATE, tsl::parameters::FromUi);
                break;
            }
        default:
            break;
    }
}

void CheckBox::computeSize() {
    int textsize = _STATE->textsize2;

    SkPaint paint;
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_normal);
    font.setSize(textsize);
    SkRect bounds{};
    font.measureText(tv.text, strlen(tv.text), SkTextEncoding::kUTF8, &bounds);

    //while (bounds.width() > v->width || bounds.height() > v->height);
    switch (orientation) {
        case HORIZONTAL:
            checkbox.height = (parent->height) < (2 * textsize ) ? parent->height.load() : 2 * textsize;
            checkbox.width = checkbox.height.load();
            tv.width = bounds.width();
            tv.height = textsize;
            width = tv.width + checkbox.width + textsize * .5f;
            height = checkbox.height.load();
            break;
        case VERTICAL:
            tv.width = static_cast<Layout *>(parent)->childs.size() > 1 ? bounds.width()
                                                                        : parent->width.load();
            tv.height = textsize;

            //if(parent->orientation == VERTICAL)
            checkbox.height =
                    parent->height - tv.height < _STATE->knob_height_Knob ? parent->height - tv.height
                                                                  : _STATE->knob_height_Knob;
            checkbox.height = checkbox.height.load() * (1.f - paddingbottom * 0.01f);
            //if(parent->orientation == VERTICAL)
            //checker->width = parent->height;
            //else
            checkbox.width = checkbox.height.load();
            width = std::max(tv.width.load(), checkbox.width.load());
            height = tv.height + checkbox.height;
            break;
        default:
            break;

    }
}


void CheckBox::init() {
    int textsize = _STATE->textsize2;
    switch (orientation) {
        case HORIZONTAL:
            tv.startx = startx.load();
            tv.stopx = tv.startx + tv.width;
            tv.starty = starty + (height - tv.height) * .5f;
            tv.stopy = tv.starty + tv.height;
            checkbox.startx = tv.stopx + textsize * .5f;
            checkbox.stopx = checkbox.startx + checkbox.width;
            checkbox.starty = starty + (height - checkbox.height) * .5f;
            checkbox.stopy = checkbox.starty + checkbox.height;
            break;
        case VERTICAL:
            tv.starty = starty.load();
            tv.stopy = tv.starty + tv.height;
            tv.startx = startx + (width - tv.width) * .5f;
            tv.stopx = tv.startx + tv.width;
            checkbox.starty = tv.stopy.load();
            checkbox.stopy = checkbox.starty + checkbox.height;
            checkbox.startx = startx + (width - checkbox.width) * .5f;
            checkbox.stopx = checkbox.startx + checkbox.width;
            break;
        default:
            break;
    }
}

CheckBox::CheckBox(tsl::AppState* appState, int _orientation, int _alignment, uint16_t _id, long _offset, int _multi) : View(
        appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, _alignment, 10, false, "Checkbox"),
        tv(appState, _STATE->parameters[_id].name, CENTER_ALIGN),
        checkbox(appState) {
    orientation = _orientation;
    id = _id;
    _STATE->parameters[id].type = ParameterType_bool;
    _STATE->parameters[id].view = this;
    _STATE->parameters[id].paramOffset = _offset;
    _STATE->parameters[id].offsetFact = (_multi == 0 ? 1 : _multi);

    tv.setText(_STATE->parameters[id].name
               ? _STATE->parameters[id].name : "Title");
    tv.prio = 10;
    tv.alignment = START_ALIGN;
    tv.id = id;
    if (orientation == HORIZONTAL)
        tv.textalignhoz = START_ALIGN;
    checkbox.id = id;
}

CheckBoxWrapped::CheckBoxWrapped(tsl::AppState* appState, int al, uint16_t _id, long offset, int multi) : CheckBox(
        appState, HORIZONTAL, al, _id, offset, multi) {
    alignment = WRAP;
    aspect_ratio = 0;
}


void CheckBoxWrapped::init() {
    int textsize = _STATE->textsize2;
    checkbox.height = (parent->height) < (2 * _STATE->textsize2 ) ? parent->height.load() : 2 * _STATE->textsize2;
    checkbox.width = checkbox.height.load();
    checkbox.stopx = stopx.load();
    checkbox.startx = checkbox.stopx - checkbox.width;
    checkbox.starty = starty + height * .5f - checkbox.height * .5;
    checkbox.stopy = checkbox.starty + checkbox.height;
    tv.startx = startx.load();
    tv.stopx = stopx - checkbox.width;
    tv.width = tv.stopx - tv.startx;
    tv.height = (parent->height) < (_STATE->textsize2 ) ? parent->height.load() : _STATE->textsize2;
    tv.starty = starty + height * .5f - tv.height * .5;
    tv.stopy = tv.starty + tv.height;
}