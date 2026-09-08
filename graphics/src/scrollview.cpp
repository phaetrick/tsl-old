//
// Created by pr on 08.10.20.
//

#include "scrollview.h"
#include "Input.h"
#include "logger.h"
#include "app.h"

using namespace tsl::graphics;

void ScrollBar::delRecursiveCB() {
    auto &queue = _STATE->queue_callback;
    queue.del(this);
    _pointerid = -1;
};

void ScrollView::init() {
    Layout::init();
    SkRect wRect{};
    if(orientation == HORIZONTAL){
        wRect.setXYWH(startx, starty, width, height - bar->height);
    } else
        wRect.setXYWH(startx, starty, width - bar->width, height);
    window->setViewPort(wRect, EVERYTHING);
	auto scale_ = func ? func() : scale;
    window->width.store(window->width.load() * scale_);
    window->stopx = window->startx + window->width;
    window->init();
}

void ScrollBar::render(void *ctx) {
    float offset = ((ScrollView *) parent)->offset.load();
    auto *canvas = (SkCanvas *) ctx;
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    flush(canvas);
    canvas->save();
    canvas->translate(startx, starty);
    paint.setColor(skcol::fg);
    if (((Layout*)parent)->orientation == HORIZONTAL) {
        float buttonwidth = height * 2;
        float rest = width - buttonwidth - lw;
        float xoff = rest * offset + lw2;

        canvas->drawLine(lw2, lw2, width-lw2, lw2, paint);
        canvas->drawLine(xoff, lw2, xoff, height - lw2, paint);
        canvas->drawLine(xoff + buttonwidth, lw2, xoff + buttonwidth, height - lw2, paint);
    }
    else{
        float buttonheight = width * 2;
        float rest = height - buttonheight - lw;
        float yoff = rest * offset + lw2;
        canvas->drawLine(lw2, lw2, lw2, height-lw2, paint);
        canvas->drawLine(lw2, yoff, width-lw2, yoff, paint);
        canvas->drawLine(lw2, yoff + buttonheight, width-lw2, yoff + buttonheight, paint);
    }
    canvas->restore();
}

static void recompute_positions_and_draw_(View *ssv, float offset){
    auto sv = (ScrollView*)ssv;
    


    auto v = sv->window;
    int startx = v->startx;
    int stopx = v->stopy;

    float orig = v->viewport.width();
    float offsetw = v->width - orig;
    float off = offsetw * offset;
    v->startx -= off;
    v->stopx = v->startx + v->width;
    v->init();
    v->startx = startx;
    v->stopx = stopx;
    v->redrawDirect();
    v->addRecursiveDraw();
}

void ScrollBar::callback(const InputEvent &event) {
    int action = event.action;
    float xpos = event.x;
    float ypos = event.y;
    float offset = ((ScrollView *) parent)->offset.load();

    int32_t pointerid = event.pointer_id;

    switch (action) {
        case ACTION_DOWN:
            lastx = xpos;
            lasty = ypos;
            _pointerid = pointerid;
            break;

        case ACTION_MOVE:
            if (_pointerid == pointerid) {
                offset += ((Layout*)parent)->orientation == HORIZONTAL ? (xpos - lastx) / (float) width : (ypos - lasty) / (float) height;
                if (offset < 0)
                    offset = 0;
                if (offset > 1.0)
                    offset = 1.0;
                ((ScrollView *) parent)->offset.store(offset);
                lastx = xpos;
                lasty = ypos;
                std::lock_guard lk(_STATE->queue_draw);
                _STATE->queue_draw.add(this, prio);
                    recompute_positions_and_draw_(parent, offset);
                
            }
            break;

        case ACTION_UP:
            if (_pointerid == pointerid) {
                if (timer.elapsed() > 0.3) {
                    timer.reset();
                } else {
                    offset = ((Layout*)parent)->orientation == HORIZONTAL ? (xpos - startx) / (float) width : (ypos - starty) / (float) height;
                    if (offset < 0)
                        offset = 0;
                    if (offset > 1.0)
                        offset = 1.0;
                    ((ScrollView *) parent)->offset.store(offset);
                    std::lock_guard lk(_STATE->queue_draw);
                    _STATE->queue_draw.add(this, prio);
                    recompute_positions_and_draw_(parent, offset);
                }
            }
            break;
        default:
            return;
    }
}

ScrollView::ScrollView(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, int _orientation, float _scale)
        : Layout(appState, _scalefactor, _aspect_ratio, _alignment) {
    orientation = _orientation;
     window = new HorizontalLayout{appState, WRAP, 0, CENTER_ALIGN};
     bar = new ScrollBar(appState);

    bar->size_reference = orientation == HORIZONTAL ? &_STATE->windowHeight : &_STATE->windowWidth;
    bar->size_reference_scale = 1. / 15. * .7f;
    addChild(window);
    addChild(bar);
    scale = _scale;
};
