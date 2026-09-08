#include "textview.h"
#include "logger.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
#include "view.h"
#include "tools.h"
#include "colours.h"
#include "app.h"
using namespace tsl::graphics;


TextView::TextView(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, const char *_text,
                   uint32_t _bg, uint32_t _fg, float _textpadding, bool _frame) : View(appState,
        _scalefactor, _aspect_ratio, _alignment, 10) {
    text = _text;
    textpadding = _textpadding;
    fg = _fg;
    bg = _bg;
    frame = _frame;
}

void TextView::computeWidth() {
    if (text == nullptr)
        width = 0;
    else {
        SkFont font(_STATE->font_normal);
        font.setSize(height);
        SkRect bounds{};
        int length = strlen(text);
        font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);
        width = bounds.width();
    }
}


void TextView::setText(const char *_text, bool _redraw) {
    text = _text;
    if (_redraw)
        redraw();
}

void TextView::setState(int state, bool _redraw) {
    if (state == HOT) {
        bg = skcol::bghot;
        fg = skcol::fghot;
    } else {
        bg = skcol::bg;
        fg = skcol::fg;
    }
    if (_redraw)
        redraw();
}


void TextView::render(void *context) {
    auto canvas = (SkCanvas *) context;
    int length = text != nullptr ? strlen(text) : 0;
    if(length == 0){
        flush(canvas);
        return;
    }
    float fontsize = height * (textpadding ? 1.0f - textpadding * 0.01f : 1.0f);

    SkFont font(_STATE->font_normal);
    font.setSize(fontsize);

    SkRect bounds{};
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    while (bounds.width() > (float) width) {
        --length;
        if (length <= 0)
            return;
        font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);
    }
    float xpos =
            textalignhoz == CENTER_ALIGN ? SkFloatToScalar((width - bounds.width()) * .5f) : (textalignhoz == START_ALIGN ? 0 : width - bounds.width());
    float ypos = textalignvert == CENTER_ALIGN ? SkFloatToScalar(
            height - ((height - bounds.height()) * .5f)) : 0;


    //measureText(v, getInstance()->font_normal, text, padding ? 1.0f - padding * 0.01f :  1.0f, &xpos, &ypos, &fontsize);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    flush(canvas);

    if (bg) {
        paint.setColor(bg);
        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx + xpos, starty + (height - fontsize) * .5f,
                                 bounds.width(), fontsize),
                fontsize * .1f, fontsize * .1f, paint);
    }
    bool midilearning =
#if defined HAS_MIDI
        _STATE->midilearning.load() && (_STATE->parameters[id].flags & Param::MidiParam);
#else
        false;
#endif
    if (midilearning)
        paint.setColor(skcol::midilearning);
    else
        paint.setColor(fg.load() != 0 ? fg.load() : skcol::text);

    canvas->drawSimpleText(text, length, SkTextEncoding::kUTF8, startx + xpos, starty + ypos,
                           font, paint);
    if (frame) {
        paint.setStyle(SkPaint::kStroke_Style);
        canvas->drawRect(
                SkRect::MakeXYWH(startx + 1, starty + 1, width - 2, height - 2),
                paint);

    }
}

void DropDownView::render(void *context) {
    float fontsize = height * (textpadding ? 1.0f - textpadding * 0.01f : 1.0f);

    SkFont font(_STATE->font_normal);
    font.setSize(fontsize);

    SkRect bounds{};
    int length = strlen(text);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    while (bounds.width() > (float) (width - height)) {
        --length;
        if (length <= 0)
            return;
        font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);
    }
    float xpos =
            textalignhoz == CENTER_ALIGN ? SkFloatToScalar(((width - _STATE->textsize2) - bounds.width()) * .5f) : 0;
    float ypos = textalignvert == CENTER_ALIGN ? SkFloatToScalar(
            height - ((height - bounds.height()) * .5f)) : 0;


    //measureText(v, getInstance()->font_normal, text, padding ? 1.0f - padding * 0.01f :  1.0f, &xpos, &ypos, &fontsize);
    auto canvas = (SkCanvas *) context;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    flush(canvas);

    if (bg) {
        paint.setColor(bg);
        canvas->drawRoundRect(
                SkRect::MakeXYWH(startx + lw2, starty + lw2,
                                 width - lw, height -lw),
                fontsize * .1f, fontsize * .1f, paint);
    }

    bool midilearning =
#if defined HAS_MIDI
        _STATE->midilearning.load() && (_STATE->parameters[id].flags & Param::MidiParam);
#else
        false;
#endif
    if (midilearning)
        paint.setColor(skcol::midilearning);
    else
        paint.setColor(fg.load() != 0 ? fg.load() : skcol::text);

    canvas->drawSimpleText(text, length, SkTextEncoding::kUTF8, startx + xpos, starty + ypos,
                           font, paint);

    SkPath path;
    path.moveTo(stopx - height + ypos + lw2, starty +ypos - bounds.height() + lw2);
    path.lineTo(stopx - ypos - lw2, starty + ypos- bounds.height()+lw2);
    path.lineTo(stopx - height /2, starty+ height - ypos + bounds.height() - lw2);
    path.lineTo(stopx - height + ypos + lw2, starty +ypos - bounds.height() + lw2);
    paint.setStyle(SkPaint::kFill_Style);

    canvas->drawPath(path, paint);
}


TitleView::TitleView(tsl::AppState* appState, const char *_text, int _alignment, Layout *_parent) : TextView(appState,
        VALUE_FROM_POINTER, VALUE_FROM_POINTER, _alignment, _text, skcol::bg,
        skcol::fg, .9) {
    size_reference = &_STATE->textsize2;
    textpadding = 10.f;
    if (_parent != nullptr)
        _parent->addChild(this);
};
DropDownView::DropDownView(tsl::AppState* appState, const char *_text, int _alignment, Layout *_parent) : TextView(appState,
        VALUE_FROM_POINTER, VALUE_FROM_POINTER, _alignment, _text, skcol::bg,
        skcol::fg, .9) {
    size_reference = &_STATE->textsize2;
    textpadding = 10.f;
    if (_parent != nullptr)
        _parent->addChild(this);
};

void DropDownView::computeWidth() {
    TextView::computeWidth();
    width += _STATE->textsize2;
}

#ifdef GRAINSTORM

PDectectTV::PDectectTV(tsl::AppState* appState, int _alignment, Layout *_parent) : TextView(appState, VALUE_FROM_POINTER,
                                                                   VALUE_FROM_POINTER,
                                                                   _alignment, "PDetectTV",
                                                                   skcol::bg,
                                                                   skcol::fg, .9f) {
    size_reference = &_STATE->textsize2;
    if (_parent != nullptr)
        _parent->addChild(this);
    _time = tsl::time::nanosecondsSinceEpoch();
    perm = true;

};

PDetectTVGrain::PDetectTVGrain(tsl::AppState* appState, int _alignment, Layout *_parent) : TextView(appState, VALUE_FROM_POINTER,
                                                                           VALUE_FROM_POINTER,
                                                                           _alignment,
                                                                           "PDetectTVGrain",
                                                                           skcol::bg,
                                                                           skcol::fg,
                                                                           .9f) {
    size_reference = &_STATE->textsize2;
    if (_parent != nullptr)
        _parent->addChild(this);
    _time = tsl::time::nanosecondsSinceEpoch();
    perm = true;
};

static const uint64_t timeout = 1e+9 / 4;


void PDectectTV::render(void *context) {
    auto t2 = tsl::time::nanosecondsSinceEpoch();
    if (DISTANCE(t2, _time) > timeout)
        _time = t2;
    else return;
    auto tindex = _STATE->active_track.load();
    auto canvas = (SkCanvas *) context;
    flush(canvas, skcol::bg);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    float fontsize = _STATE->textsize2;
    SkFont font(_STATE->font_normal);

    font.setSize(fontsize);
    paint.setColor(skcol::text);
    std::ostringstream ss;
    ss << (int) _STATE->params[tindex][PITCHDETECTFXTRACKOUT0].load();
    if (_STATE->channels == 2)
        ss << " / " << (int) _STATE->params[tindex][PITCHDETECTFXTRACKOUT0].load();
    ss << " Hz";
    std::string s(ss.str());
    textDisplayCenteredFixed(this, canvas, paint, font, s.c_str(), 1.0, 0);
    //canvas->drawText(s.c_str(), s.size(), startx, starty, paint);
}

void PDetectTVGrain::render(void *context) {
    auto t2 = tsl::time::nanosecondsSinceEpoch();
    if (DISTANCE(t2, _time) > timeout)
        _time = t2;
    else return;
    auto tindex = _STATE->active_track.load();
    auto canvas = (SkCanvas *) context;
    flush(canvas, skcol::bg);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    float fontsize = _STATE->textsize2;
    SkFont font(_STATE->font_normal);

    font.setSize(fontsize);
    paint.setColor(skcol::text);
    std::ostringstream ss;
    ss << (int) _STATE->params[tindex][PITCHDETECTGRAINFXTRACKOUT0].load();
    if (_STATE->channels == 2)
        ss << " / " << (int) _STATE->params[tindex][PITCHDETECTGRAINFXTRACKOUT1].load();
    ss << " Hz";
    std::string s(ss.str());
    textDisplayCenteredFixed(this, canvas, paint, font, s.c_str(), 1.0, 0);
    //canvas->drawText(s.c_str(), s.size(), startx, starty, paint);
}
#endif