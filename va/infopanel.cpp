#include "infopanel.h"
#include <logger.h>
#include <cstdlib>
#include <cstring>
#include <include/core/SkFont.h>
#include "grainstorm.h"
#include "knob.h"
#include "defines.h"
#include "view.h"
#include "tools.h"
#include "player.h"
#include <skia.h>
#include <app.h>

using namespace tsl::graphics;


static void render_mic(InfoPanel *infopanel, void *context);

void render_infopanel(InfoPanel *infopanel, void *context);

static void render_rec(InfoPanel *infopanel, void *context);

static void render_void(InfoPanel *infopanel, void *context);

static void render_text(InfoPanel *infopanel, void *context);
static void render_text_frombuffer(InfoPanel *infopanel, void *context);

static void render_value(InfoPanel *infopanel, void *context);

static void render_normal(InfoPanel *infopanel, void *context);

static void render_progress(InfoPanel *infopanel, void *context);

static void render_value_lfo_editor(InfoPanel *infopanel, void *context);

static void render_xrun(InfoPanel *infopanel, void *context);

InfoPanel::InfoPanel(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment) : View(appState,
                                                                                   _scalefactor,
                                                                                   _aspect_ratio,
                                                                                   _alignment, 10,
                                                                                   true,
                                                                                   "Infopanel") {
    renderfunc.store(render_normal);
    setTimeStamp();
}

static void render_void(InfoPanel *infopanel, void *context) {
}

void InfoPanel::render(void *context) {
    renderfunc.load()(this, context);
}


#include <SkCanvas.h>
#include "defines.h"
#include "grainstorm.h"
#include "view.h"
#include "tools.h"
#include "meter.h"


static const float values[] = {-120, -60, -40, -30, -25, -20, -15, -10, -6, -3, 0};


#define TOP_DRAW(x, y) ((x) > (y) ? (y) : (y) - DISTANCEF((x), (y)))


static void render_normal(InfoPanel *infopanel, void *context) {
}

static void render_rec(InfoPanel *info, void *context) {
    auto* _appState = info->_appState;
    size_t samples = _DATA->currentrecoff / _STATE->channels;

    auto *canvas = (SkCanvas *) context;
    info->flush(canvas);
    canvas->save();
    canvas->translate(info->startx, info->starty);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::fg);
    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);

    TIME_P t;
    time_convert(t, _STATE->sr, (long) samples);
    char text[20];
    snprintf(text, 20, "%02d : %02d", t.m, t.s);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, info->startx_rec,
                           info->starty_pos, font, paint);
    canvas->restore();
}

static void render_mic(InfoPanel *infopanel, void *context) {
}

static void render_progress(InfoPanel *info, void *context) {
    float progress = info->progress.load();
    long offset = info->dec_offset.load();
    float line_width = View::lw;

    auto *canvas = (SkCanvas *) context;
    info->flush(canvas);
    canvas->save();
    canvas->translate(info->startx, info->starty);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::blue_transparent);
    auto* _appState = info->_appState;
    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);

    canvas->drawRect(SkRect::MakeXYWH(0, SkDoubleToScalar(line_width),
                                      SkDoubleToScalar(info->width * progress), info->height),
                     paint);

    paint.setColor(skcol::fg);
    TIME_P t;
    time_convert(t, _STATE->sr, offset);
    char text[20];
    snprintf(text, 20, "%02d : %02d : %03d", t.m, t.s, t.ms);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, info->startx_rec,
                           info->starty_pos, font, paint);

    snprintf(text, 20, "%d%%", (int) (progress * 100));
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, 0, info->starty_pos, font,
                           paint);
    canvas->restore();
}

static void render_text(InfoPanel *info, void *context) {
    if (!info->text)
        return;
    const char *text = info->text;
    auto _appState = info->_appState;
    if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L) {
        _STATE->graphics.deleteWindow(info->windowindex);
        info->renderfunc = render_normal;
        return;
    }

    float width = _STATE->textsize2 * strlen(text + 1);

    SkRect bounds{};
    int length = strlen(text);

    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    float xpos = (width - bounds.width()) * .5f;
    float ypos = _STATE->textsize1 - (_STATE->textsize1 - bounds.height()) * .5f;





    SkCanvas *c = _STATE->graphics.getCanvas(info->windowindex,
                                          (_STATE->windowWidth - width) * .5f, _STATE->windowHeight - _STATE->windowHeight * .618 * .5 + _STATE->textsize1 * .5f, width, _STATE->textsize1);
    if (!c)
        return;

    c->clear(skcol::window_darktransp);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);

    paint.setColor(skcol::fg);

    c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font,
                      paint);
}

static void render_dec(InfoPanel *info, void *context) {
    auto* _appState = info->_appState;
    long samples = info->dec_offset.load();

    TIME_P t;
    time_convert(t, _STATE->sr, (long) samples);
    char text[20];
    snprintf(text, 20, "%02d : %02d", t.m, t.s);
    auto *canvas = (SkCanvas *) context;
    info->flush(canvas);
    canvas->save();
    canvas->translate(info->startx, info->starty);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);

    paint.setAntiAlias(true);

    paint.setColor(skcol::fg);
    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);

    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, info->startx_rec,
                           info->starty_pos, font, paint);
    canvas->restore();
}

static void render_value_lfo_editor(InfoPanel *info, void *context) {

}

static void render_value_adsr(InfoPanel *info, void *context) {
    auto* _appState = info->_appState;
    if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L) {
        info->renderfunc = render_normal;
        info->perm = false;
        return;
    }
    else info->perm = true;

    auto tindex = _STATE->active_track.load();
    int active_seg = _DATA->activeSeg.load();
    if (active_seg < 0 || active_seg > 4)
        return;

    char text[30];
    snprintf(text, 30, "%.0f dB", _STATE->params[tindex][info->miditargetindex + active_seg].load());


    auto *canvas = (SkCanvas *) context;
    info->flush(canvas);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);

    paint.setColor(skcol::fg);

    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);

    View::textDisplayCenteredFixed(info, canvas, paint, font, text, 1.0, false);
}

static void render_xrun(InfoPanel *info, void *context) {
    auto _appState = info->_appState;
    if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L) {
        _STATE->graphics.deleteWindow(info->windowindex);
        info->renderfunc = render_normal;
        info->perm = false;
        return;
    }
    else info->perm = true;
    char text[30];
    snprintf(text, 30, "XRUN #%d", info->xruncount.load());


    float width = _STATE->textsize2 * strlen(text + 1);

    SkRect bounds{};
    int length = strlen(text);

    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    float xpos = (width - bounds.width()) * .5f;
    float ypos = _STATE->textsize1 - (_STATE->textsize1 - bounds.height()) * .5f;





    SkCanvas *c = _STATE->graphics.getCanvas(info->windowindex,
                                          (_STATE->windowWidth - width) * .5f, (_STATE->windowHeight - _STATE->textsize1) * .5, width, _STATE->textsize1);
    if (!c)
        return;

    c->clear(skcol::window_darktransp);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);

    paint.setColor(skcol::fg);

    c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font,
                      paint);

}
void InfoPanel::init(){
    const char *test =
            "POS: 00 : 00 : 000X/ 00 : 00 : 000  LOOP: 00 : 00 : 000XLOAD: 100%";

    fontsize = _STATE->textsize2 * .9f;
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2);

    SkRect bounds{};
    font.measureText(text, strlen(test), SkTextEncoding::kUTF8, &bounds);

    width = bounds.width() + _STATE->textsize2;
    height = _STATE->textsize1;
    startx = starty = 0;

    //"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
    measureTextFixed(width, height, _STATE->font_normal, test, &startx_pos1, &starty_pos, fontsize);
    font.setSize(fontsize);
    const char *test2 = "POS: 00 : 00 : 000X";
    font.measureText(test2, strlen(test2), SkTextEncoding::kUTF8, &bounds);
    startx_pos2 = startx_pos1 + bounds.width();
    const char *test3 = "/ 00 : 00 : 000  LOOP: 00 : 00 : 000X";
    font.measureText(test3, strlen(test3), SkTextEncoding::kUTF8, &bounds);
    startx_pos3 = startx_pos2 + bounds.width();
    //info->starty_pos = mid;

    test = "00 : 00";
    font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
    startx_rec = width / 2 - bounds.width() / 2;

    test = "Microphone recording.";
    font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
    startx_mic = width / 2 - bounds.width() / 2;

    test = "DECODING...";
    font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
    startx_decode = width / 2 - bounds.width() / 2;

    test = "RENDERING WAVEFORM...";
    font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
    startx_render = width / 2 - bounds.width() / 2;

    test = "Buffer underrun!";
    font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
    startx_warning = width / 2 - bounds.width() / 2;
}

static const char *formatvalues[6] = {"%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f"};

static void render_value(InfoPanel *info, void *context) {
    auto* _appState = info->_appState;
    if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L ||
        info->miditargetindex == PARAM_NOT_ASSIGNED) {
        _STATE->graphics.deleteWindow(info->windowindex);
        info->renderfunc = render_normal;
        info->perm = false;
        return;
    }else info->perm = true;

    auto &miditarget = info->miditargetindex > NUM_PARAMS ? _STATE->parameters[0]
                                                          : _STATE->parameters[info->miditargetindex];

    float reference = info->ref != nullptr ? (*info->ref).load() : 0;
    float offset = miditarget.offset;

    const char *formatvalue = formatvalues[miditarget.digits];

    char text[100];

    bool isLog = (miditarget.paramCurve == Param::ParamCurve::Log10);
    const float value = isLog ? LOG2NORMALF(reference) - offset : reference - offset;

    if (miditarget.getFlag(Param::ConvertMs) && miditarget.valuename) {
        const float samplesperms = _STATE->sr / 1000.f;
        float value = reference / samplesperms;
        snprintf(text, 100, "%.1f", value);
    } else if (!strcmp(miditarget.name, "RATIO")) {
        if (reference == 0.)
            snprintf(text, 100, "%s", "BYPASS");
        else if (reference == 1.0)
            snprintf(text, 100, "%s", "INF : 1");
        else
            snprintf(text, 100, "%.1f : 1", 1. / (1. - reference));

    } else
        snprintf(text, 100, formatvalue, value);

    int lenwithoutdigits = 1;
    float max = isLog ? LOG2NORMALF(miditarget.max) : miditarget.max;
    float min = isLog ? LOG2NORMALF(miditarget.min) : miditarget.min;

    int x = std::max(std::abs(min), std::abs(max));
    while ( x /= 10 )
        lenwithoutdigits++;
    int negone = (min < 0 || max < 0);

    int len = lenwithoutdigits + negone + miditarget.digits;
    int lenval = (miditarget.valuename == nullptr ? 0 :
                  strlen(miditarget.valuename));
    float width = (len + lenval + 4) * _STATE->maxCharWidtht2;
    View *view = miditarget.view;
    float sx, sy;
    if (view != nullptr) {
        sx = miditarget.view->startx + miditarget.view->width * .5f - width * .5f;
        if (sx < 0)
            sx = 0;
        else if (sx + width > _STATE->windowWidth)
            sx = _STATE->windowWidth - width;
        sy = miditarget.view->stopy;
    } else {
        sx = (_STATE->windowWidth - width);
        sy = (_STATE->windowWidth - width);
    }

    SkCanvas *c = _STATE->graphics.getCanvas(info->windowindex, sx, sy, width, _STATE->textsize1);
    if (!c)
        return;
    c->clear(skcol::window_darktransp);
    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize);
    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::text);


    const char *valuename = miditarget.valuename ? miditarget.valuename : " ";
    float sxval = width - (strlen(valuename) + 2) * _STATE->maxCharWidtht2;
    c->drawSimpleText(valuename, strlen(valuename), SkTextEncoding::kUTF8, sxval, info->starty_pos,
                      font, paint);

    SkRect bounds{};
    int length = strlen(text);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    float xpos = (sxval - bounds.width()) * .5f;
    //measureTextFixed(width - sxval, _STATE->textsize1, _STATE->font_normal, text, &sxval, &yy,
      //               info->fontsize);

    c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, info->starty_pos, font,
                      paint);


    // info->
    //       centerText(c, text
    //);
}

void InfoPanel::setRenderFunc(int func) {

    switch (func) {
        case REC:
            //info->active = true;
            renderfunc = render_rec;
            break;
        case MIC:
            //info->active = true;
            renderfunc = render_mic;
            break;
        case STAND:
            //info->active = false;
            renderfunc = render_normal;
            break;
        case VOID:
            renderfunc = render_void;
            break;
        case TEXT:
            renderfunc = render_text;
            break;
        case VALUE:
            setTimeStamp();
            renderfunc = render_value;
            break;
        case PROG:
            renderfunc = render_progress;
            break;
        case DECODE:
            renderfunc = render_dec;
            break;
        case LFOEDITOR:
            setTimeStamp();

            renderfunc = render_value_lfo_editor;
            break;
        case XRUN:
            setTimeStamp();
            renderfunc = render_xrun;
            break;
        case INFO_ADSR:
            setTimeStamp();
            renderfunc = render_value_adsr;
            break;
        case RENDERBUFFER:
            setTimeStamp();
            renderfunc = render_text_frombuffer;
            break;
        default:
            break;
    }
}

void InfoPanel::setTimeStamp() {
    tstamp = tsl::time::nanosecondsSinceEpoch();
    redraw();
}


void LoadInfo::init() {
    float ts = _STATE->textsize2;
    width = 4 * ts + ts;
    height = ts;
    startx = starty = 0;
}

void LoadInfo::render(void *ctx) {

    if (!_STATE->player._isplaying.load()) {
        _STATE->graphics.deleteWindow(windowindex);
        perm = false;
        return;
    }
    else perm = true;
    char text[10];
    int buffer_fill = (int) (_STATE->buffer_fill / 2500000.);
    uint32_t col = skcol::text;
    if (buffer_fill >= 100)
        col = skcol::red;
    else if (buffer_fill >= 80.)
        col = skcol::yellow;


    snprintf(text, 10, "%d%%", buffer_fill);

    SkCanvas *canvas = _STATE->graphics.getCanvas(windowindex,
                                               0,
                                               _STATE->windowHeight * 0.618f, width, height);
    canvas->clear(skcol::bg);
    SkPaint(paint);
    paint.setAntiAlias(true);
    paint.setColor(skcol::text);
    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .75);
    textDisplayCentered(this, canvas, paint, font, text);
    drawRect(canvas, skcol::fg, 3.0);
}

static void render_text_frombuffer(InfoPanel *info, void *context) {
    auto _appState = info->_appState;
    if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 2e+9L) {
        _STATE->graphics.deleteWindow(info->windowindex);
        auto prev = info->prevRenderfunc;
        info->renderfunc = prev ? prev : render_normal;
        return;
    }
    const char* text = info->textBuffer;
    if (!text[0]) return;

    float width = _STATE->textsize2 * strlen(text);
    int length = strlen(text);
    SkRect bounds{};
    SkFont font(_STATE->font_normal);
    font.setSize(info->fontsize ? info->fontsize : _STATE->textsize2);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    float xpos = (width - bounds.width()) * .5f;
    float ypos = _STATE->textsize1 - (_STATE->textsize1 - bounds.height()) * .5f;

    SkCanvas *c = _STATE->graphics.getCanvas(info->windowindex,
                                             (_STATE->windowWidth - width) * .5f,
                                             _STATE->windowHeight - _STATE->windowHeight * .618f * .5f + _STATE->textsize1 * .5f,
                                             width, _STATE->textsize1);
    c->clear(skcol::window_darktransp);
    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);
    paint.setColor(skcol::text);
    c->drawSimpleText(text, length, SkTextEncoding::kUTF8, xpos, ypos, font, paint);
    SkPaint borderPaint;
    borderPaint.setStyle(SkPaint::kStroke_Style);
    borderPaint.setStrokeWidth(3.0f);
    borderPaint.setColor(skcol::fg);
    c->drawRect(SkRect::MakeWH(width, _STATE->textsize1), borderPaint);
}

void InfoPanel::prepareBuffer(tsl::parameters::Event& ev) {
    int pos = 0;
    ev.toString(_appState, pos, textBuffer, sizeof(textBuffer), true);
    auto func = renderfunc.load();
    if (func != render_text_frombuffer)
        prevRenderfunc = func;
    setRenderFunc(RENDERBUFFER);
}
