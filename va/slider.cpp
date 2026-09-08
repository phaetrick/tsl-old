#include <grainstorm.h>
#include <cstdlib>
#include <logger.h>
#include "view.h"
#include "slider.h"
#include <colours.h>
#include "infopanel.h"
#include "tools.h"
#include "setup.h"
#include "queue.h"
#include "EnterValue.h"
#include <SkPaint.h>
#include <include/core/SkFont.h>
#include "Input.h"
#include "gui.h"

using namespace tsl::graphics;


Slider::Slider(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, int _id, long _offset,
               int _multi, Layout *_parent) : View(appState, _scalefactor, _aspect_ratio, _alignment, 10,
                                                   false, "Slider") {
    id = _id;
    _STATE->parameters[id].view = this;
    _STATE->parameters[id].type = ParameterType_double;
    _STATE->parameters[id].paramOffset = _offset;
    _STATE->parameters[id].offsetFact = (_multi == 0 ? 1 : _multi);
    e.setup(_appState, 0, id);
    if (_parent != nullptr)
        _parent->addChild(this);
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
        int diff = (height - s) * .5f;
        starty += diff;
        stopy -= diff;
        height = s;
    }


    starty_slider = height * .5f;
    height_slider = View::lw;
    width_tv = width - width_slider;
    float x, y;
    //measureTextFixed(s->width_tv, h, _STATE->font_normal, _STATE->parameters[v->id].name, &x, &y, _STATE->textsize2 * .9f);
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
    const int trackIndex = 0;
    int cbo = 0;
    if (_appState->parameters[id].paramOffset)
        cbo = (int)(_appState->params[0][_appState->parameters[id].paramOffset].load());
    cbo = cbo * _appState->parameters[id].offsetFact;
    float reference = _appState->params[0][id + cbo].load();


    float percentage = DISTANCEF(reference, _STATE->parameters[id].min) /
                       DISTANCEF(_STATE->parameters[id].min, _STATE->parameters[id].max);
    float offset = startx_slider + width_slider * percentage;

    auto *canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    canvas->save();
    canvas->translate(startx, starty);
    paint.setColor(skcol::bg);
    canvas->drawLine(0, 1, 0, height - 1, paint);
    canvas->drawRect(
            SkRect::MakeXYWH(posx_text, (height - _STATE->textsize2) * .5f, width_text, _STATE->textsize2),
            paint);
    paint.setColor(skcol::blue);
    canvas->drawLine(startx_slider, starty_slider, offset, starty_slider, paint);
    paint.setColor(skcol::fg);
    canvas->drawLine(offset, starty_slider, stopx_slider, starty_slider, paint);
    canvas->drawLine(offset, 1, offset, height - 1, paint);

    if (tv_touched) {
        paint.setColor(skcol::bghot);
        canvas->drawRoundRect(SkRect::MakeXYWH(posx_text, (height - _STATE->textsize2) * .5f, width_text,
                                               _STATE->textsize2), _STATE->textsize2 * .1f,
                              _STATE->textsize2 * .1f, paint);
    }
    paint.setColor(_STATE->midilearning.load() ? skcol::midilearning
                                                                             :  (tv_touched ? skcol::fghot : skcol::fg));
    canvas->drawSimpleText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                           SkTextEncoding::kUTF8, posx_text, posy_text, font, paint);
    canvas->restore();
}

void Slider::callback(const InputEvent &event) {
    int action = event.action;
    const float xpos = event.x - startx;
    const float ypos = event.y - starty;

    //float ypos = AMotionEvent_getY(event, view->touchid);
    auto &miditarget = _STATE->parameters[id];
    // Show the value only while dragging (like the knob), not on hover.
    auto showValue = [&]() {
        auto info = _DATA->views.infopanel;
        info->ref = &_appState->params[0][e.paramIndex];
        info->miditargetindex = id;
        info->setRenderFunc(VALUE);
    };

    int pointerid = event.pointer_id;

    switch (action) {
        case ACTION_DOWN: {
            e.setup(_appState, 0, id);
            if (xpos < width_slider) {
                inputstate.addPointer({pointerid, xpos, ypos, pmode_t::WINPOINTER, SLIDERVIEW});
                e.value = e.getCurrentValue(_STATE);
                showValue();
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
                    MYFLOAT val = e.getCurrentValue(_STATE) + (xpos - pt->xpos) / width_slider *
                                                              DISTANCEF(miditarget.min, miditarget.max);
                    pt->xpos = xpos;
                    showValue();
                    if (val < miditarget.min)
                        val = miditarget.min;
                    else if (val > miditarget.max)
                        val = miditarget.max;
                    e.value = val;
                    auto old = e.apply(_STATE, tsl::parameters::FromUi);
                    if (old.value != val)
                        redraw();
                } else if (pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    tv_touched = false;
                    redraw();
                    inputstate.removePointer(pointerid);
                }
            }


            /*

            for(auto &pt : inputstate.pointers){
                if(pt.id == pointerid ){
                    if(pt.mode == SLIDERVIEW) {
                        float val = *ref + (xpos - pt.xpos) / width_slider *
                                           DISTANCEF(miditarget.min, miditarget.max);
                        //reference += diff * (miditarget.max - miditarget.min);
                        pt.xpos = xpos;
                        if (val < miditarget.min)
                            val = miditarget.min;
                        else if (val > miditarget.max)
                            val = miditarget.max;
                        *ref = val;
                    }
                    else if(DISTANCEF(xpos, pt.xpos) > _STATE->textsize2){

                    }
                    redraw();
                }
            }*/
            break;
    }

    case ACTION_UP: {
        if (auto pt = inputstate.getById(pointerid)) {
            if (pt->target == SLIDERVIEW) {
                // Double-tap to default. Same idiom as Knob and PlusMinusControl:
                // elapsedReplace() is the gap between consecutive releases in SECONDS.
                // (This used to compare a nanosecond delta against SHORTCLICKTIME,
                // which is 300000 — a 0.3 ms window, so the reset never fired.)
                if (inputstate.timer.elapsedReplace() < .3) {
                    e.value = e.getDefaultValue(_STATE);
                    auto old = e.apply(_STATE, tsl::parameters::FromUi);
                    if (old.value != e.value)
                        redraw();
                }
#ifdef PLUGIN_MODE
                _STATE->EndParamChange(e);
#endif
            } else {
                tv_touched = false;
                redraw();
                auto ev = e;
                _STATE->UiTasksQueue.add_task([this, ev]() mutable { EnterValue::Task(_appState, ev); });
            }
            inputstate.removePointer(pointerid);
        }
        break;
    }
    default:
        return;
}

}

// ── SliderVert — vertical fader (label on top, vertical track) ────────────────

void SliderVert::init() {
    starty_slider = _STATE->textsize2 * 1.5f + lw;
    height_slider = height - starty_slider - 2 * lw;
    SkRect bounds{};
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    font.measureText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                     SkTextEncoding::kUTF8, &bounds);
    width_text = bounds.width();
    posy_text = SkFloatToScalar(_STATE->textsize2 - ((_STATE->textsize2 - bounds.height()) * .5f));
    posx_text = SkFloatToScalar((width - width_text) * .5f);
    const float s = _STATE->windowHeight * .05f;
    if (width >= s) { _widthBar = s - lw;    _startxBar = (width - _widthBar) * .5f; }
    else            { _startxBar = lw2;      _widthBar = width - lw; }
}

void SliderVert::render(void *context) {
    int cbo = 0;
    if (_appState->parameters[id].paramOffset)
        cbo = (int)(_appState->params[0][_appState->parameters[id].paramOffset].load());
    cbo = cbo * _appState->parameters[id].offsetFact;
    float reference = _appState->params[0][id + cbo].load();

    float percentage = DISTANCEF(reference, _STATE->parameters[id].min) /
                       DISTANCEF(_STATE->parameters[id].min, _STATE->parameters[id].max);
    float offset = height - lw * 1.5f - height_slider * percentage;

    // Where the fill starts and stops, from the same helper the knob ring uses, so
    // a bipolar fader would grow out from its centre instead of up from the floor.
    // No CentreFill parameter is a fader today, and without the flag fillLo is 0 —
    // which takes the `bottom` branch below and reproduces the original geometry
    // exactly. This exists so that putting a bipolar parameter on a fader later
    // cannot quietly reintroduce the half-full-at-off reading.
    float fillLo, fillHi;
    _STATE->parameters[id].fillRange(reference, fillLo, fillHi);
    const float offsetHi = height - lw * 1.5f - height_slider * fillHi;
    const float bottom   = fillLo > 0.f ? height - lw * 1.5f - height_slider * fillLo
                                        : height - lw;

    auto *canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    canvas->save();
    canvas->translate(startx, starty);
    // filled part (anchor → thumb), unfilled (top → thumb), then the thumb bar
    paint.setColor(skcol::blue);
    canvas->drawLine((width - lw) * .5f, offsetHi, (width - lw) * .5f, bottom, paint);
    paint.setColor(skcol::fg);
    canvas->drawLine((width - lw) * .5f, starty_slider, (width - lw) * .5f, offsetHi, paint);
    if (fillLo > 0.f)
        canvas->drawLine((width - lw) * .5f, bottom, (width - lw) * .5f, height - lw, paint);

    // Modulation range, same idea as the knob's ring: recolour the stretch of track
    // modulation can move the thumb across. This is not cosmetic parity with the
    // knob — MORPH, WARP and modal DECAY are faders, not knobs, so several of the
    // 22 LFO destinations (0 OFF .. 21 NOISE) can only ever be shown here.
    {
        // Both ends, because LFO routes are bipolar and the reachable band can sit on
        // either side of the thumb — drawing only to one endpoint would hide half of a
        // symmetrical sweep. Sources that only ever pull one way return lo == thumb or
        // hi == thumb, so those still render as the single stretch they always did.
        float lo, hi;
        if (tsl::app::modRangeFor2(_appState, id + cbo, lo, hi)) {
            const auto& mt = _STATE->parameters[id];
            const float range = (float)(mt.max - mt.min);
            if (range != 0.f) {
                auto trackY = [&](float v) {
                    float pct = (v - (float)mt.min) / range;
                    if (pct < 0.f) pct = 0.f; else if (pct > 1.f) pct = 1.f;
                    return height - lw * 1.5f - height_slider * pct;
                };
                const float yLo = trackY(lo), yHi = trackY(hi);
                if (std::fabs(yLo - yHi) > lw) {
                    paint.setColor(skcol::modarc);
                    canvas->drawLine((width - lw) * .5f, yLo, (width - lw) * .5f, yHi, paint);
                    paint.setColor(skcol::fg);
                }
            }
        }
    }

    canvas->drawLine(_startxBar, offset, _startxBar + _widthBar, offset, paint);

    if (tv_touched) {
        paint.setColor(skcol::bghot);
        canvas->drawRoundRect(SkRect::MakeXYWH(posx_text, 0, width_text, _STATE->textsize2),
                              _STATE->textsize2 * .1f, _STATE->textsize2 * .1f, paint);
    }
    paint.setColor(_STATE->midilearning.load() ? skcol::midilearning
                                               : (tv_touched ? skcol::fghot : skcol::fg));
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    canvas->drawSimpleText(_STATE->parameters[id].name, strlen(_STATE->parameters[id].name),
                           SkTextEncoding::kUTF8, posx_text, posy_text, font, paint);
    canvas->restore();
}

void SliderVert::callback(const InputEvent &event) {
    int action = event.action;
    const float xpos = event.x - startx;
    const float ypos = event.y - starty;
    auto &miditarget = _STATE->parameters[id];
    // Show the value in the info panel only while actually dragging (like the knob),
    // not on hover — set it in the SLIDERVIEW branches below, not up here.
    auto showValue = [&]() {
        auto info = _DATA->views.infopanel;
        info->ref = &_appState->params[0][e.paramIndex];
        info->miditargetindex = id;
        info->setRenderFunc(VALUE);
    };

    int pointerid = event.pointer_id;
    switch (action) {
        case ACTION_DOWN: {
            e.setup(_appState, 0, id);
            if (ypos >= starty_slider) {
                inputstate.addPointer({pointerid, xpos, ypos, pmode_t::WINPOINTER, SLIDERVIEW});
                e.value = e.getCurrentValue(_STATE);
                showValue();
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
                    // drag up = increase
                    MYFLOAT val = e.getCurrentValue(_STATE) + (pt->ypos - ypos) / height_slider *
                                                              DISTANCEF(miditarget.min, miditarget.max);
                    pt->ypos = ypos;
                    if (val < miditarget.min) val = miditarget.min;
                    else if (val > miditarget.max) val = miditarget.max;
                    e.value = val;
                    auto old = e.apply(_STATE, tsl::parameters::FromUi);
                    showValue();
                    if (old.value != val)
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
                    if (inputstate.timer.elapsedReplace() < .3) {   // double-tap → default
                        e.value = e.getDefaultValue(_STATE);
                        auto old = e.apply(_STATE, tsl::parameters::FromUi);
                        if (old.value != e.value)
                            redraw();
                    }
#ifdef PLUGIN_MODE
                    _STATE->EndParamChange(e);
#endif
                } else {
                    tv_touched = false;
                    redraw();
                    auto ev = e;
                    _STATE->UiTasksQueue.add_task([this, ev]() mutable { EnterValue::Task(_appState, ev); });
                }
                inputstate.removePointer(pointerid);
            }
            break;
        }
        default:
            return;
    }
}
