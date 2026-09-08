#include <cstdlib>
#include "logger.h"
#include "view.h"
#include "slider.h"
#include "colours.h"
#include "infopanel.h"
#include "tools.h"
#include "setup.h"
#include "track.h"
#include "EnterValue.h"
#include <SkPaint.h>
#include <include/core/SkFont.h>
#include <grainstorm.h>
#include "app.h"

using namespace tsl::graphics;

Slider::Slider(tsl::AppState *appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
               int _id, long _offset,
               int32_t _multi, Layout *_parent) : View(appState, _scalefactor, _aspect_ratio,
                                                       _alignment, 10,
                                                       false, "Slider"),
                                                  valueView(appState, *this) {
    id = _id;
    _STATE->parameters[id].view = this;
    _STATE->parameters[id].type = ParameterType_double;
    e.setup(appState, 0, id);
    if (_parent != nullptr)
        _parent->addChild(this);
    if (_STATE->parameters[id].paramOffset>0) {
        for (int i = 1; i < _STATE->parameters[_STATE->parameters[id].paramOffset].max; i++) {
            _STATE->parameters[id + i * _STATE->parameters[id].offsetFact].view = this;
        }
    }
}

void Slider::addRecursiveDraw() {
    auto tindex = _STATE->active_track.load();
    e.setup(_STATE, tindex, id);
    View::addRecursiveDraw();
}

void Slider::init() {
    startx_slider = 0;
    width_slider = width * .725f;
    stopx_slider = startx_slider + width_slider;
    abs_startx_slider = startx + startx_slider;
    abs_stopx_slider = abs_startx_slider + width_slider;

    const float t = _STATE->windowHeight * .066666f;
    const float s = _STATE->windowHeight * .05f;

    if (height >= t) {
        int32_t diff = (height - s) * .5f;
        starty += diff;
        stopy -= diff;
        height = s;
    }


    starty_slider = height * .5f;
    height_slider = lw;
    width_tv = width - width_slider;
    float x, y;
    //measureTextFixed(s->width_tv, h, _DATA->_STATE->font_normal, _STATE->parameters[v->id].name, &x, &y, _DATA->_STATE->textsize2 * .9f);
    SkRect bounds{};
    SkPaint paint;
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);

    font.measureText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                     SkTextEncoding::kUTF8, &bounds);
    width_text = bounds.width();
    //x = SkFloatToScalar((s->width_tv - s->width_text) * .5f);
    y = SkFloatToScalar(height - ((height - bounds.height()) * .5f));
    posx_text = width_slider + _STATE->textsize2 * .9f;
    posy_text = y;
};

void Slider::render(void *context) {
    auto reference = e.getCurrentValue(_STATE);


    auto percentage = DISTANCEF(reference, _STATE->parameters[id].min) /
                       DISTANCEF(_STATE->parameters[id].min, _STATE->parameters[id].max);
    float offset = startx_slider + width_slider * percentage;

    auto *canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    canvas->save();
    canvas->translate(startx, starty);
    paint.setColor(skcol::bg);
    canvas->drawLine(0, 1, 0, height - 1, paint);
    canvas->drawRect(
            SkRect::MakeXYWH(posx_text, (height - _STATE->textsize2) * .5f, width_text,
                             _STATE->textsize2),
            paint);
    paint.setColor(skcol::blue);
    canvas->drawLine(startx_slider, starty_slider, offset, starty_slider, paint);
    paint.setColor(skcol::fg);
    canvas->drawLine(offset, starty_slider, stopx_slider, starty_slider, paint);
    canvas->drawLine(offset, 1, offset, height - 1, paint);

    if (tv_touched) {
        paint.setColor(skcol::bghot);
        canvas->drawRoundRect(
                SkRect::MakeXYWH(posx_text, (height - _STATE->textsize2) * .5f, width_text,
                                 _STATE->textsize2), _STATE->textsize2 * .1f,
                _STATE->textsize2 * .1f, paint);
    }
    paint.setColor(_STATE->midilearning.load() && _STATE->parameters[id].flags & Param::MidiParam
                   ? skcol::midilearning
                   : (tv_touched
                      ? skcol::fghot
                      : skcol::fg));
    canvas->drawSimpleText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                           SkTextEncoding::kUTF8, posx_text, posy_text, font, paint);
    canvas->restore();
}

void Slider::callback(const InputEvent &event) {
    int32_t action = event.action;
    const float xpos = event.x - startx;
    const float ypos = event.y - starty;

    
    auto &miditarget = _STATE->parameters[e.getDisplayParam()];
    
    int32_t pointerid = event.pointer_id;

    switch (action) {
        case ACTION_DOWN: {
            if (xpos < width_slider) {
                inputstate.addPointer({pointerid, xpos, ypos, pmode_t::WINPOINTER, SLIDERVIEW});
                valueView.addDraw();
                e.value = e.getCurrentValue(_STATE);
#ifdef PLUGIN_MODE
                    _STATE->StartParamChange(e);
#endif

            } else {
                inputstate.addPointer({pointerid, xpos, ypos, pmode_t::WINPOINTER, TEXTVIEW});
                tv_touched = true;
                redraw();
            }
            break;

        }
        case ACTION_MOVE: {
            if (auto pt = inputstate.getById(pointerid)) {
                if (pt->target == SLIDERVIEW) {
                    auto val = e.getCurrentValue(_STATE) + (xpos - pt->xpos) / width_slider *
                                      DISTANCEF(miditarget.min, miditarget.max);
                    //reference += diff * (miditarget.max - miditarget.min);
                    pt->xpos = xpos;
                    if (val < miditarget.min)
                        val = miditarget.min;
                    else if (val > miditarget.max)
                        val = miditarget.max;
                    e.value = val;
                    auto oe = e.apply(_STATE, tsl::parameters::FromUi);
                    if(oe.value != val){
                    redraw();
                    valueView.addDraw();
                    }
                    
                } else if (pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    tv_touched = false;
                    redraw();
                    inputstate.removePointer(pointerid);
                }
            }
            break;
        }

        case ACTION_UP: {
            if (auto pt = inputstate.getById(pointerid)) {
                if (pt->target == SLIDERVIEW) {
                    uint64_t time = tsl::time::nanosecondsSinceEpoch();
                    if (inputstate.timer.elapsedReplace() < .3) {
						e.value = miditarget.initvalue;
						auto old = e.apply(_STATE, tsl::parameters::FromUi);
                        if (old.value != miditarget.initvalue) {
                            redraw();
                            valueView.addDraw();
                        }
  
                    }
                    
#ifdef PLUGIN_MODE
                        _STATE->EndParamChange(e);
              
#endif
                    
                } else {
                    tv_touched = false;
                    redraw();
                    _STATE->UiTasksQueue.add_task([this] ()mutable {
                        EnterValue::Task(_appState, e);

                        });
                        
                }
                inputstate.removePointer(pointerid);
            }
            break;
        }
        default:
            return;
    }
}


void SliderVert::init() {
    starty_slider = _STATE->textsize2 * 1.5f + lw;
    height_slider = height - starty_slider - 2 * lw;
    SkRect bounds{};
    SkPaint paint;
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    font.measureText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                     SkTextEncoding::kUTF8, &bounds);
    width_text = bounds.width();
    posy_text = SkFloatToScalar(_STATE->textsize2 - ((_STATE->textsize2 - bounds.height()) * .5f));
    posx_text = SkFloatToScalar((width - width_text) * .5f);
    const float s = _STATE->windowHeight * .05f;
    if (width >= s) {
        _widthBar = s - lw;
        _startxBar = (width - _widthBar) * .5f;
    } else {
        _startxBar = lw2;
        _widthBar = width - lw;
    }
};

void SliderVert::render(void *context) {
    float reference = e.getCurrentValue(_STATE);


    float percentage = DISTANCEF(reference, _STATE->parameters[id].min) /
                       DISTANCEF(_STATE->parameters[id].min, _STATE->parameters[id].max);
    float offset = height - lw * 1.5 - height_slider * percentage;

    auto *canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    canvas->save();
    canvas->translate(startx, starty);
/*
    paint.setColor(_DATA->C_bg);
    canvas->drawRect(
            SkRect::MakeXYWH(0, 0, width, _DATA->_STATE->textsize2),
            paint);
*/
    paint.setColor(skcol::blue);
    canvas->drawLine((width - lw) * .5f, offset, (width - lw) * .5f, height - lw, paint);
    paint.setColor(skcol::fg);
    canvas->drawLine((width - lw) * .5f, starty_slider, (width - lw) * .5f, offset, paint);
    canvas->drawLine(_startxBar, offset, _startxBar + _widthBar, offset, paint);

    if (tv_touched) {
        paint.setColor(skcol::bghot);
        canvas->drawRoundRect(SkRect::MakeXYWH(posx_text, 0 /*posytext*/, width_text,
                                               _STATE->textsize2), _STATE->textsize2 * .1f,
                              _STATE->textsize2 * .1f, paint);
    }
    paint.setColor(_STATE->midilearning.load() && _STATE->parameters[id].flags & Param::MidiParam
                   ? skcol::midilearning
                   : (tv_touched
                      ? skcol::fghot
                      : skcol::fg));


    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    canvas->drawSimpleText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                           SkTextEncoding::kUTF8, posx_text, posy_text, font, paint);
    canvas->restore();
}

void SliderVert::callback(const InputEvent &event) {
    int32_t action = event.action;
    const float xpos = event.x - startx;
    const float ypos = event.y - starty;

   
    int32_t pointerid = event.pointer_id;

    switch (action) {
        case ACTION_DOWN: {
            if (ypos >= starty_slider) {
                inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, SLIDERVIEW);
                e.value = e.getCurrentValue(_STATE);
#ifdef PLUGIN_MODE
                    _STATE->StartParamChange(e);
#endif
            } else {
                inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, TEXTVIEW);
                tv_touched = true;
                redraw();
            }
            break;
        }
        case ACTION_MOVE: {
            if (auto pt = inputstate.getById(pointerid)) {
                if (pt->target == SLIDERVIEW) {
                    auto& miditarget = _STATE->parameters[e.getDisplayParam()];
                    float val = e.getCurrentValue(_STATE) + (pt->ypos - ypos) / height_slider *
                                       DISTANCEF(miditarget.min, miditarget.max);
                    //reference += diff * (miditarget.max - miditarget.min);
                    pt->ypos = ypos;
                    if (val < miditarget.min)
                        val = miditarget.min;
                    else if (val > miditarget.max)
                        val = miditarget.max;
                    e.value = val;
					auto old = e.apply(_STATE, tsl::parameters::FromUi);
                    if(old.value != val)
                        redraw();

                } else if (pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    tv_touched = false;
                    redraw();
                    inputstate.removePointer(pointerid);
                }
            }
            break;
        }

        case ACTION_UP: {
            if (auto pt = inputstate.getById(pointerid)) {
                if (pt->target == SLIDERVIEW) {
                    auto& param = _STATE->parameters[e.getDisplayParam()];
                    uint64_t time = tsl::time::nanosecondsSinceEpoch();
                    if (inputstate.timer.elapsedReplace() < .3) {
						e.value = param.initvalue;

                        auto old = e.apply(_STATE, tsl::parameters::FromUi);
                        if (old.value != param.initvalue) {
                            redraw();
                            valueView.addDraw();
                        }

                    }
#ifdef PLUGIN_MODE
                        _STATE->EndParamChange(e);
#endif

                }
                else {
                    tv_touched = false;
                    redraw();
                    _STATE->UiTasksQueue.add_task([this] () mutable {
                        EnterValue::Task(_appState, e);

                        });

                }
                inputstate.removePointer(pointerid);
            }
            break;
        }
        default:
            return;
    }
}
