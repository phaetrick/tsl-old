//
// Created by pr on 13.11.17.
//
#include <cstdlib>
#include <cstring>
#include <string>
#include <logger.h>
#include <include/core/SkFont.h>
#include "selector.h"
#include "view.h"
#include "defines.h"
#include "button.h"
#include "textview.h"
#include "queue.h"
#include "grainstorm.h"
#include "tools.h"
#include "Input.h"
#include "skia.h"

using namespace tsl::graphics;

static constexpr float HOTANIMATIONTIMENS = 1e9f;

void Selector::redraw() {
    auto tindex = (_appState)->active_track.load();
    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;
    float val = (_appState)->params[tindex][id + offset].load();
    int a = values.empty() ? val : findIndex(val);
    if (a >= names.size())
        a = -1;
    popupview.active.store(a);
    popupview.lastmoved = tsl::time::nanosecondsSinceEpoch();
    View::redraw();
}


void PopUpWindow::init() {
    startx = 0;
    starty = 0;
    width = stopx = _STATE->windowWidth;
    height = stopy = _STATE->windowHeight;
    const float textsize = _STATE->textsize2;
    auto s = selector;
    float heightwin = 0;
    float widthwin = 0;
    itemheight = textsize * 2;
    SkFont font(_STATE->font_normal);
    font.setSize(itemheight);
    SkRect bounds;
    for (int i = 0; i < s->names.size(); i++) {
        font.measureText(s->names[i].c_str(), strlen(s->names[i].c_str()), SkTextEncoding::kUTF8,
                         &bounds);
        widthwin = bounds.width() > widthwin ? bounds.width() : widthwin;
    }
    font.setSize(textsize * .9f);
    font.measureText(s->names[0].c_str(), strlen(s->names[0].c_str()), SkTextEncoding::kUTF8,
                     &bounds);
    textoffset = textsize - bounds.centerY();

    float maxw = width * .9f;
    float maxh = height * .9f;
    float finalwidth = widthwin + itemheight + 2 * lw;
    if (finalwidth > maxw)
        finalwidth = maxw;

    drawwidth = finalwidth;
    drawstartx = selector->startx + (selector->width - drawwidth) * .5f;
    if (drawstartx < startx)
        drawstartx = startx;

    drawstopx = drawstartx + drawwidth;
    if (drawstopx > width) {
        drawstopx = width;
        drawstartx = drawstopx - drawwidth;
    }
    startxtext = lw + textsize;
    widthtext = drawwidth - 2 * lw - textsize;


    const float finalheight = textsize + itemheight * s->names.size() + 2 * lw;
    if (finalheight > maxh) {
        maxoffset = -DISTANCE(finalheight, maxh);
        drawheight = maxh;
    } else {
        maxoffset = 0;
        drawheight = finalheight;
    }
    drawstarty = (height - drawheight) * .5f;
    if (drawstarty < 0) drawstarty = 0;
    drawstopy = drawstarty + drawheight;
    if (drawstopy > height) drawstopy = height;
    startytext = textsize * .5f + lw;
    heighttext = drawheight - 2 * lw - textsize;
    startxrect = lw2;
    startyrect = lw2;
    widthrect = drawwidth - lw;
    heightrect = drawheight - lw;
    //setWindowPosition(s->windowindex, view->startx, view->starty);
}

#include <SkPath.h>
#include <string>

static constexpr float FLINGDECAY = .998f;   // per millisecond

// v0 = pt/s (points per second),
static inline float
currentOffset(int64_t t0, float x0, float v0, int64_t currenttime = tsl::time::nanosecondsSinceEpoch()) {
    const float d = FLINGDECAY;
    return x0 + (v0 >= 0 ? 1.f : -1.f) * std::abs(v0) * (pow(d, (currenttime - t0) / 1e6) - 1) /
                (1000 * log(d));
}

// Distance the momentum still has to travel from where it is now.
static inline float flingRemaining(int64_t t0, float v0, int64_t currenttime) {
    const float d = FLINGDECAY;
    return std::abs(v0) * pow(d, (currenttime - t0) / 1e6) / (-1000 * log(d));
}


void PopUpWindow::render(void *ctx) {
    SkCanvas *canvas = _STATE->graphics.getCanvas(windowindex, drawstartx,
                                               drawstarty, drawwidth, drawheight);
    if (canvas == nullptr)
        return;
    SkPaint paint;
    paint.setAntiAlias(true);

    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9f);
    // canvas->translate(startx, starty);
    //canvas->clear(skcol::transparent);
    paint.setColor(skcol::bg);
    canvas->drawRect(SkRect::MakeXYWH(0, 0, drawwidth, drawheight), paint);
    paint.setColor(skcol::fg);
    paint.setStrokeWidth(1);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawRect(SkRect::MakeXYWH(0, 0, drawwidth, drawheight),
                     paint);

    canvas->save();
    SkPath path;
    path.addRect(SkRect::MakeXYWH(lw + lw2, startytext, drawwidth - 3 * lw, heighttext));
    canvas->clipPath(path);

    const int64_t currenttime = tsl::time::nanosecondsSinceEpoch();

    float off;
#ifdef PLATFORM_MOBILE
    if (maxoffset < 0 && v0.load() != 0) {
        off = currentOffset(t0, x0, v0, currenttime);
        if (off > 0) {
            off = 0;
            v0 = 0;
        } else if (off < maxoffset) {
            off = maxoffset;
            v0 = 0;
        } else if (flingRemaining(t0, v0, currenttime) < .5f) {
            // Came to rest between the two edges. Without this the momentum is
            // never declared finished and every later frame - including the first
            // frame of the NEXT time this popup is opened - keeps recomputing the
            // offset from a t0 that is by then minutes old.
            v0 = 0;
        }
        offset.store(off);
    } else
        off = offset.load();
#else
    off = offset.load();
#endif

    float sx = startxtext;
    float sy = startytext + off + textoffset;
    int a = active.load();
    int h = hot.load();
    paint.setStyle(SkPaint::kFill_Style);

    for (int i = 0; i < selector->names.size(); i++) {
        if (sy >= startytext && sy - itemheight <= startytext + heighttext) {
            std::string tmp = selector->names[i];
            if (i == a) {
                tmp.append(" * ");
            }

            if (i == h) {
                const int64_t passed = currenttime - timedown;
                if (passed < HOTANIMATIONTIMENS) {
                    const float val = passed / HOTANIMATIONTIMENS * 100;
                    paint.setColor(SK_Colour(33, 150, 243, val));
                } else {
                    paint.setColor(skcol::blue_transparent);
                }
                canvas->drawRect(
                        SkRect::MakeXYWH(lw + lw, sy - textoffset, drawwidth - 4 * lw, itemheight),
                        paint);
                paint.setColor(skcol::fg);
            }
            canvas->drawSimpleText(tmp.data(), strlen(tmp.data()),
                                   SkTextEncoding::kUTF8, sx, sy, font, paint);
        }
        sy += itemheight;
    }

    const float maxoff = std::abs(maxoffset);
    if (maxoff > 0) {
        const int64_t passed = currenttime - lastmoved;
        if (passed < ONESECONDNS)
            paint.setColor(skcol::fg);
        else if (passed < TWOSECONDNS) {
            const uint8_t val = (1.f - (passed - ONESECONDNS) / ONESECONDNS) * 255;
            paint.setColor(SK_Colour(val, val, val, 255));
        } else {
            paint.setColor(skcol::bg);
        }
        const float length = heighttext / (1.f + maxoff / heighttext);
        const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (heighttext - length);
        canvas->drawRect(
                SkRect::MakeXYWH(startxtext + widthtext - 4 * lw, startytext + pos, 3 * lw, length),
                paint);
    }
    canvas->restore();
}

// A release only flings if the finger was still travelling when it left the glass.
// Anything older than this is a hold, no matter what the last samples said.
static constexpr int64_t FLINGIDLENS = 100000000;   // 100 ms

// touchSlop() is 8dp, but it is only as trustworthy as mPpi, and mPpi comes from the
// platform: a JNI static field on Android, UIScreen.scale on iOS, GetScreenScale() on
// the desktop. If any of those never lands, mPpi stays at its 160 default and the slop
// collapses to 8 *pixels* - which a resting finger crosses on a dense screen, turning
// every tap into a drag. The window is measured in the same coordinate space, so 1% of
// its height is an independent lower bound on "this was meant as a swipe".
static inline float touchslop(tsl::AppState *_appState) {
    return std::max(_appState->touchSlop(), _appState->windowHeight * .01f);
}

void PopUpWindow::callback(const InputEvent &event) {
    auto s = selector;
    int action = event.action;
    int _pointerid = event.pointer_id;
    float xpos = event.x - drawstartx;
    float ypos = event.y - drawstarty;
    float _startytext = startytext;
    float _stopytext = _startytext + heighttext;
    float _startxtext = startxtext;
    float _stopxtext = _startxtext + widthtext;

    switch (action) {
        case ACTION_DOWN: {
            if (xpos >= _startxtext && xpos < _stopxtext && ypos >= _startytext &&
                ypos < _stopytext) {
                int pos = (int) ((ypos - startytext - offset) / itemheight);
                if (pos >= s->names.size())
                    pos = s->names.size() - 1;
                hot = pos;
                mode = INSIDE;
                timedown = tsl::time::nanosecondsSinceEpoch();
            } else {
                mode = OUTSIDE;
                hot = -1;
            }
            totalmoved = 0;
            v0 = 0;
            px = downx = xpos;
            py = downy = ypos;
            pointerid = _pointerid;
            lastmovetime = event.time;
#ifdef PLATFORM_MOBILE
            velocityTracker.addMovement(event);   // clears this pointer's history
#endif
            break;
        }
        case ACTION_UP: {
            if (pointerid == _pointerid) {
                if (mode == INSIDE || mode == OUTSIDE) {
                    if (mode == INSIDE) {
                        s->cb(s, s->values.empty() ? hot.load() : s->values[hot.load()]);
                        s->redraw();
                    }
                    deldraw();
                    delCB();
                    hot = -1;
                }
#ifdef PLATFORM_MOBILE
                // Only a drag can fling, and only with a velocity measured over the
                // last few samples. The old code derived it from (release point -
                // last direction change) / (release time - last direction change),
                // which is not a velocity at all: a still finger changes direction on
                // every jitter sample, so lastdirchange was always a few ms old and
                // any pixel of noise came out as hundreds of pixels per second. That
                // is the movement that started when the finger was lifted.
                else if (mode == MOVING && maxoffset < 0) {
                    const int64_t now = tsl::time::nanosecondsSinceEpoch();
                    float vx = 0, vy = 0;
                    if (now - lastmovetime > FLINGIDLENS)
                        vy = 0;                       // finger came to rest before lift-off
                    else if (!velocityTracker.getVelocity(_pointerid, &vx, &vy))
                        vy = 0;

                    // 8dp/s is the smallest gesture worth animating; below that the
                    // list just stays where the finger left it.
                    const float minv = std::max(_STATE->mMinimumFlingVelocity,
                                                touchslop(_appState) * 8.f);
                    const float maxv = _STATE->mMaximumFlingVelocity;
                    if (std::abs(vy) < minv)
                        vy = 0;
                    else if (maxv > 0 && std::abs(vy) > maxv)
                        vy = vy < 0 ? -maxv : maxv;

                    v0 = vy;
                    t0 = now;
                    x0 = offset.load();
                }
#endif
                totalmoved = 0;
                mode = UNTOUCHED;
                // Unconditionally: the MOVING branch used to leave the dead pointer id
                // in place, so the popup stayed owned by a finger that was gone.
                pointerid = -1;
            }
            break;
        }
        case ACTION_MOVE: {
            if (pointerid == _pointerid) {
                const int64_t currenttime = tsl::time::nanosecondsSinceEpoch();
                lastmovetime = event.time;
#ifdef PLATFORM_MOBILE
                velocityTracker.addMovement(event);
#endif
                int m = mode.load();
                if (m == MOVING) {
                    lastmoved = currenttime;
                // Displacement from the PRESS POINT, not accumulated path length.
                // px/py advance on every move, so `totalmoved += spacing(..,px,py)`
                // summed the sensor jitter of a perfectly still finger until it
                // crossed the threshold: the hold silently became a drag and the
                // release then fired a small fling. Slop is a physical distance,
                // hence touchslop() (density-derived) rather than a font size.
                } else if (m == INSIDE) {
                    totalmoved = spacing(xpos, downx, ypos, downy);
                    if (totalmoved > touchslop(_appState)) {
                        mode = m = MOVING;
                        lastmoved = currenttime;
                        hot = -1;
                        // Scrolling starts HERE, not at the press point: leave py on
                        // this sample so the first drag step is 0 and the list does
                        // not jump by the slop distance the moment it is exceeded.
                        py = ypos;
                    }
                } else if (m == OUTSIDE) {
                    totalmoved = spacing(xpos, downx, ypos, downy);
                    if (totalmoved > touchslop(_appState)) {
                        mode = UNTOUCHED;
                        pointerid = -1;
                    }
                }

                // Below the slop the list must not move at all - dragging it by the
                // sensor noise of a resting finger is what made a plain tap look like
                // the start of a scroll.
                if (m == MOVING && maxoffset < 0) {
                    float off = offset.load() - (py - ypos);
                    if (off < maxoffset)
                        off = maxoffset;
                    else if (off > 0)
                        off = 0;
                    offset.store(off);
                }
                px = xpos;
                py = ypos;
            }
            break;
        }
        case ACTION_MOUSE_WHEEL: {
            if (maxoffset < 0 && mode == UNTOUCHED) {
                float off = offset.load();
                off += event.pointer_id * itemheight;
                if (off < maxoffset)
                    off = maxoffset;
                else if (off > 0)
                    off = 0;
                offset.store(off);
                lastmoved = tsl::time::nanosecondsSinceEpoch();
            }
            break;
        }
        default:
            break;
    }
}


Selector::Selector(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
                   int _alignment, std::vector<std::string> &_names,
                   std::vector<float> &_values,
                   uint16_t _id, long _offset, int _offsetmulti, int (*_cb)(Selector *, int),
                   const char *title) : View(appState, _scalefactor, _aspect_ratio, _alignment, 10, false,
                                             "SelectorI") {
    id = _id;
    _STATE->parameters[id].view = this;
    _STATE->parameters[id].paramOffset = _offset;
    _STATE->parameters[id].offsetFact = (_offsetmulti == 0 ? 1 : _offsetmulti);
    names = _names;
    values = _values;
    // Publish the GUI lists into the Param ONLY when setup.cpp defined none of its
    // own: a Param that already carries names/values keeps them — the setup lists
    // are canonical and value-indexed, while a GUI list may be display-permuted
    // (TABLE / PAD TBL) or padded for layout, so indexing it by raw value named the
    // wrong entry in the info toasts. When the GUI is the sole authority, names and
    // values are published AS A PAIR: toDisplay then resolves a raw value through
    // the values list to the matching name, which is correct even for permuted or
    // sparse (negative, 97/98/99) value sets. Both Param fields are non-owning
    // spans over this view's members, so they stay valid for the GUI's lifetime;
    // PresetSelector republites after every rebuild for the same reason.
    if (_STATE->parameters[id].names.empty() && _STATE->parameters[id].values.empty()) {
        if (!_values.empty()) {
            _STATE->parameters[id].max = (MYFLOAT)(_values.size() - 1);
            _STATE->parameters[id].values = values;
        }
        nameViews.reserve(names.size());
        for (const auto& n : names) nameViews.emplace_back(n);
        _STATE->parameters[id].names = nameViews;
    }
    titletext = _STATE->parameters[id].name == nullptr ? title
                                                               : _STATE->parameters[id].name;
    cb = _cb ? _cb : element_callback_new;
    popupview._appState = _appState;
    popupview.scalefactor = scalefactor;
    popupview.alignment = alignment;
    popupview.aspect_ratio = aspect_ratio;
    popupview.selector = this;
}

#include "synth.h"
#include <SkCanvas.h>
#include <SkPath.h>

void Selector::setActive(const char *name, bool docallback) {
    int count = 0;
    for (int i = 0; i < names.size(); i++) {
        if (!strcmp(names[i].c_str(), name)) {
            if (docallback)
                cb(this, values.empty() ? (int) i : (int) values[i]);
        }
        count++;
    }
}


void Selector::setActiveByIndex(int index, bool docallback) {
    if (index >= names.size())
        return;
    if (docallback) {
        cb(this, values.empty() ? index : values[index]);
    }//   p->queue_render->add(p->queue_render, view, view->name, nullptr, view->prio, false);
}

Selector2::Selector2(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
                     int _alignment, std::vector<std::string> &_names,
                     std::vector<float> &_values,
                     uint16_t _id, long _offset,
                     int _offsetmulti, const char *_title, int (*_callback)(Selector *, int))
        : Selector(appState, _scalefactor, _aspect_ratio, _alignment, _names, _values, _id,
                   _offset,
                   _offsetmulti, _callback, _title),
          value(appState, "SelTitle", PARAM_NOT_ASSIGNED),
          title(appState, "SelTitle", PARAM_NOT_ASSIGNED) {
    const char *tit = _title == nullptr ? _STATE->parameters[_id].name : _title;
    if (tit == nullptr)
        tit = "TITLE";
    title.name_normal = tit;
}


void Selector2::render(void *context) {
    auto *canvas = (SkCanvas *) context;
    flush(canvas);

    auto tindex = (_appState)->active_track.load();
    int offset = 0;
    if (_STATE->parameters[id].paramOffset)
        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
    offset = offset * _STATE->parameters[id].offsetFact;
    float _val = (_appState)->params[tindex][id + offset].load();

    int index;
    if (!values.empty())
        index = findIndex(_val);
    else
        index = (int) _val;

    value.name_normal = names[index].c_str();

    const bool ml = _STATE->midilearning.load() && (_STATE->parameters[id].flags & Param::MidiParam);
    value.midiHighlight = ml;
    title.midiHighlight = ml;

    title.setState(hot ? HOT : NORMAL, false);

    title.render(context);
    value.render(context);
}


void Selector2::init() {
    value.height = title.height = _STATE->textsize1;
    value.width = title.width = width.load();
    value.startx = title.startx = startx.load();
    value.stopx = title.stopx = stopx.load();
    title.starty = starty.load();
    title.stopy = title.starty + title.height;
    value.starty = title.stopy.load();
    value.stopy = value.starty + value.height;

    value.computePadding();
    value.init();
    title.computePadding();
    title.init();
    popupview.init();
}

void Selector2::computeHeight() {
    height = _STATE->textsize1* 2.f;
}

void Selector2::callback(const InputEvent &event){
    if (disabled)
        return;
    int action = event.action;


    switch (action) {
        case ACTION_DOWN:
            hot = true;
            inputstate.addPointer({event.pointer_id, event.x - startx, event.y - starty,
                tsl::graphics::InputSystem::WinState::WINPOINTER, 0});
            redraw();
            break;
        case ACTION_MOVE: {
            auto pt = inputstate.getById(event.pointer_id);
            if (pt && pt->distanceTo(event.x - startx, event.y - starty) > _STATE->textsize2) {
                inputstate.removePointer(event.pointer_id);
                hot = false;
                redraw();
            }
            break;
        }
        case ACTION_UP: {
            if (inputstate.getById(event.pointer_id)) {
                popupview.init();
                popupview.redraw();
                popupview.addCB();
                hot = false;
                redraw();
                inputstate.clear();
                return;
            }
            break;
        }
        default:
            break;
    }
};