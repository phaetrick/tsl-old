//
// Created by pr on 19.10.23.
//

#include "views.h"
#include "skia.h"
#include "app.h"
using namespace tsl::graphics;
#ifdef HAS_AUDIO
#ifndef AUDIO_NO_THREADS
void xrunview::render(void *){
    if (timer.elapsed()>1) {
        _STATE->graphics.deleteWindow(index);
        perm = false;
        return;
    }
    else perm = true;
    char text[30];
    snprintf(text, 30, "XRUN #%d", _STATE->xruncount.load());


    float w = _STATE->textsize2 * strlen(text + 1);

    SkRect bounds{};
    int length = strlen(text);

    SkFont font(_STATE->font_normal);
    font.setSize(_STATE->textsize2 * .9);
    font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

    float xpos = (w - bounds.width()) * .5f;
    float ypos = _STATE->textsize1 - (_STATE->textsize1 - bounds.height()) * .5f;





    auto c = _STATE->graphics.getCanvas(index,
                                               (_STATE->windowWidth - w) * .5f, _STATE->windowHeight * .75 - _STATE->textsize1 * .5, w, _STATE->textsize1);
    if (!c)
        return;

    c->clear(skcol::bg);

    SkPaint paint;
    paint.setStrokeWidth(View::lw);
    paint.setAntiAlias(true);

    paint.setColor(skcol::fg);

    c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font,
                      paint);
    paint.setStrokeWidth(1);
    paint.setStyle(SkPaint::kStroke_Style);
    c->drawRect(SkRect::MakeXYWH(0,0, w, _STATE->textsize1), paint);

}
#endif
#endif