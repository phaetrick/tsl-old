//
// Created by pr on 23.04.21.
//
#include "grainstorm.h"
#include "pianoview.h"
#include <Midi.h>
#include <app.h>
#include <logger.h>
using namespace tsl::graphics;
using namespace tsl::midi;


bool PianoKey::touchDown() {
    if(pressed.exchange(true))
        return true;
    redraw();
        _DATA->preQueue.push({tsl::midi::MidiNotes::midiToFreq(midinum), 127, STATUS_NOTE_ON, tsl::time::nanosecondsSinceEpoch(), (uint8_t)midinum});
    return false;
}

void PianoKey::touchUp() {
    pressed = false;
    // A note-off must never be lost to a full queue — see DATA::lostNoteOffs.
    if (!_DATA->preQueue.push({tsl::midi::MidiNotes::midiToFreq(midinum), 0, STATUS_NOTE_OFF, tsl::time::nanosecondsSinceEpoch(), (uint8_t)midinum}))
        _DATA->lostNoteOffs[(midinum >> 6) & 1].fetch_or(1ull << (midinum & 63), std::memory_order_release);
    redraw();
}

void WhiteKey::init() {
    if (name.find("c") != std::string::npos) {
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2);

        SkRect bounds{};
        int length = strlen(name.c_str());
        font.measureText(name.c_str(), length, SkTextEncoding::kUTF8, &bounds);

        textx = SkFloatToScalar((width - bounds.width()) * .5f);
        texty = SkFloatToScalar(height - bounds.height() * .5f);
        drawText = true;
    }
}

void WhiteKey::render(void *context) {
    auto canvas = (SkCanvas*) context;

    flush(canvas, pressed ? skcol::orange : skcol::fg);
    if (drawText) {
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2);
        int length = strlen(name.c_str());
        SkPaint paint;
        paint.setStrokeWidth(lw);
        paint.setAntiAlias(true);
        paint.setColor(skcol::bg);
        canvas->drawSimpleText(name.c_str(), length, SkTextEncoding::kUTF8,
                               startx + textx,
                               starty + texty, font, paint);
    }
    drawRect(canvas, skcol::bg,3);
}

void BlackKey::render(void *context) {
    auto canvas = (SkCanvas*) context;

    const bool p = pressed.load();

    flush(canvas, p ? skcol::orange : skcol::bg);
    if (p)
        drawRect(canvas, skcol::bg,3);
}

Piano::Piano() : View() {

    for (int i = 0; i < WHITEKEYS; i++) {
        whitekeys[i].num = i;
        whitekeys[i].redrawKeys[1] = &blackKeys[redrawKeys[i][0]];
        whitekeys[i].redrawKeys[0] = &blackKeys[redrawKeys[i][1]];
    }
    for (int i = 0; i < BLACKKEYS; i++) {
        blackKeys[i].num = i;
    }

    int poswhite = 0, posblack = 0;
    for (int midinote = 21; midinote < 128; midinote++) {
        std::string s = MidiNotes::midiToNoteName(midinote);
        if (s.find("s") != std::string::npos) {
            blackKeys[posblack].midinum = midinote;
            blackKeys[posblack].name = s;
            blackKeys[posblack].freq = MidiNotes::midiToFreq(midinote);
            posblack++;
        } else {
            whitekeys[poswhite].midinum = midinote;
            whitekeys[poswhite].name = s;
            whitekeys[poswhite].freq = MidiNotes::midiToFreq(midinote);
            poswhite++;
        }
    }
}

PianoView::PianoView(tsl::AppState* appState, float scalefactor, int aspect_ratio, int alignment) : VerticalLayout(
        appState, scalefactor,
        aspect_ratio,
        alignment) {
    addChild(&scrollBar);
    addChild(&divider);
    addChild(&piano);
    prio = 0;
};

PianoView::~PianoView() {
    childs.clear();
}


void PianoScrollBar::delRecursiveCB() {
    _pointerid = -1;
    View::delRecursiveCB();
};

void PianoScrollBar::render(void *ctx) {
    auto canvas = (SkCanvas*) ctx;
    float lh = lw * .5f;
    float offset = 1.f - ((PianoView *) parent)->offset.load();
    float buttonheight = height * .15f;
    float rest = height - buttonheight - lw;
    float yoff = rest * offset + lh;

    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    flush(canvas);
    canvas->save();
    canvas->translate(startx, starty);
    paint.setColor(skcol::fg);
    //canvas->drawLine(0, height - lh, width, height - lh, paint);
    canvas->drawLine(lw, yoff, width-lw, yoff, paint);
    canvas->drawLine(lw, yoff + buttonheight, width - lw, yoff + buttonheight, paint);
    //drawFrame(canvas);

    //canvas->drawLine(lh, view->height-lh, view->width-lh, view->height-lh, paint);
    // canvas->drawRect(SkRect::MakeXYWH(xoff, lw, buttonwidth, view->height - lw), paint);
    canvas->restore();
}

void PianoScrollBar::callback(const InputEvent &event) {
    int action = event.action;
    float ypos = event.y;
    float offset = ((PianoView *) parent)->offset.load();
    int32_t pointerid = event.pointer_id;

    switch (action) {
        case ACTION_DOWN:
            lasty = ypos;
            _pointerid = pointerid;
            break;

        case ACTION_MOVE:
            if (_pointerid == pointerid) {
                offset -= (ypos - lasty) / height;
                if (offset < 0)
                    offset = 0;
                if (offset > 1.0)
                    offset = 1.0;
                ((PianoView *) parent)->offset.store(offset);
                lasty = ypos;
                redraw();
                ((PianoView *) parent)->piano.redraw();
            }
            break;

        case ACTION_UP:
            if (_pointerid == pointerid) {
                // Double-tap → jump so the grabber lands centred on the tap.
                // Detection is the Knob idiom: elapsedReplace() is the gap between
                // consecutive releases in seconds. The mapping is the inverse of
                // render()'s yoff = rest * (1 - offset) + lw * .5f.
                if (timer.elapsedReplace() < .3) {
                    float buttonheight = height * .15f;
                    float rest = height - buttonheight - lw;
                    float visual = (ypos - starty - lw * .5f - buttonheight * .5f) / rest;
                    if (visual < 0.f)
                        visual = 0.f;
                    else if (visual > 1.f)
                        visual = 1.f;
                    ((PianoView *) parent)->offset.store(1.f - visual);
                    redraw();
                    ((PianoView *) parent)->piano.redraw();
                }
            }
            break;
        default:
            return;
    }
}

void Piano::init() {
    const float keywidth = viewport.width() / 15.f;//width / (float) WHITEKEYS;
    width = keywidth * WHITEKEYS;

    float orig = viewport.width();
    float offsetw = width - orig;
    float off = offsetw * oldoffset;
    startx = parent->startx - off;
    stopx = startx + width;
    for (int i = 0; i < WHITEKEYS; i++) {
        whitekeys[i].width = keywidth;
        whitekeys[i].height = height.load();
        whitekeys[i].startx = startx + i * keywidth + lw2;
        whitekeys[i].stopx = whitekeys[i].startx + keywidth;
        whitekeys[i].starty = starty.load();
        whitekeys[i].stopy = stopy.load();
        whitekeys[i].init();
    }
    float blackheight = height / 1.618f;
    float blackwidth = keywidth / 1.618;
    float offset = keywidth -  blackwidth * .5f;

    int i = 0;
    for (auto hit : blackhits) {
        blackKeys[i].width = blackwidth;
        blackKeys[i].height = blackheight;
        blackKeys[i].startx = startx + offset + hit * keywidth + lw2;
        blackKeys[i].stopx = blackKeys[i].startx + blackwidth;
        blackKeys[i].starty = starty.load();
        blackKeys[i].stopy = blackKeys[i].starty + blackheight;
        i++;
    }
}


void Piano::render(void *ctx) {
    float offset = ((PianoView *) parent)->offset.load();
    if (offset != oldoffset) {
        oldoffset = offset;
        init();
    }
    std::lock_guard<std::recursive_mutex> lk(_STATE->queue_draw);
    // && , not || . With || the test is true for every key on the board (a key
    // is essentially always either right of the left edge or left of the right
    // one), so it culled nothing and all 63 + 44 keys were queued every frame
    // although only ~15 are on screen.
    //
    // That mattered because queue_draw holds 250 views and add() SILENTLY drops
    // on full -- View::redraw ignores the return and still marks the view
    // visible. The piano alone was taking 107 of the 250, and Piano::render
    // queues whites before blacks, so the keys that lost the race were always
    // the black ones: "the keyboard sometimes doesn't draw its black keys".
    for (auto &w : whitekeys)
        if (w.stopx >= parent->startx && w.startx < parent->stopx)
            w.redraw();
    for (auto &w : blackKeys)
        if (w.stopx >= parent->startx && w.startx < parent->stopx)
            w.redraw();
    // Silent truncation is what made this hard to see. Say so if it ever
    // happens again -- the queue being full is never normal.
    if (_STATE->queue_draw.isFull())
        LOGE("Piano::render: draw queue full (%d views) - keys were dropped",
             _STATE->queue_draw.size());
}