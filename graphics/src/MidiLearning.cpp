//
// Created by pr on 19.09.23.
//

#include "defines.h"
#include "logger.h"
#include "Midi.h"
#include "Input.h"
#include "view.h"
#include "MidiLearning.h"
#include "EnterValue.h"
#include "keyboard.h"
#include "app.h"
#include "tools/queuetsl.h"
#include <include/core/SkFontMetrics.h>
#include <iomanip>


using namespace tsl::midi;
using namespace tsl::graphics;

void MidiLearning::setup(int _id, int track, bool _isLFO, tsl::ThreadSignal::WaitToken waitToken_) {
    waitToken = waitToken_;
    tindex = track;
    eventReceived = false;
    hasFocus = true;
    perm = true;
    id = _id;
    tsl::parameters::Event e;;
    e.setup(_STATE, track, id);
    auto &m = _STATE->parameters[e.getDisplayParam()];
    char buffer[100]{};
    int pos = 0;
    pos += snprintf(buffer + pos, 100 - pos, "MidiLearning: ");
    pos = std::min(pos, (int)100 - 1);
    e.toString(_STATE, pos, buffer, 100, false);
    //title = "MidiLearning: ";
    //if (m.category) { title += m.category; title += " "; }
    //if (m.subcategory) { title += m.subcategory; title += " "; }
    //title += m.name;
    title = buffer;
    if (m.type == ParameterType_double) {
        _min.setText(m.toString(m.getMin(_STATE->sr)));
        _max.setText(m.toString(m.getMax(_STATE->sr)));
        type = m.inputType();
        isControl = true;
        _min.setHighLight(true);
        _max.setHighLight(true);
    } else isControl = false;

}

void MidiLearning::init() {
    width = _STATE->windowWidth;
    height = _STATE->windowHeight;
    startx = starty = 0;
    stopx = width.load();
    stopy = height.load();
    fs = _STATE->textsize2 * .9f;
    lh = _STATE->textsize2 * 2.0f;
    offset = (lh + fs) * .5f;
    SkFont font(_STATE->font_normal);
    font.setSize(fs);
    float x1, y1;
    float w1 = measureTextFixed(w, fs, font, title.c_str(), &x1, &y1, fs);
    float w2 = measureTextFixed(w, fs, font,
                                isControl ? "Waiting for MIDI \"Control Change\" Event..."
                                          : "Waiting for MIDI \"Note On\" Event...", &x1, &y1, fs);


    w = (w1 > w2 ? w1 : w2) + 2 * _STATE->textsize2;
    h = (isControl ? 13.5 : (_STATE->parameters[id].type == ParameterType_enum ? 11.5 : 9.5)) * _STATE->textsize2;
    if (_STATE->windowHeight >= _STATE->windowWidth) {
        x = (_STATE->windowWidth - w) * .5;
        y = _STATE->windowHeight * 0.25 - h * .5;
        keyboard.alignment = keyboard.BOTTOM;
    } else {
        x = _STATE->textsize2;
        y = (_STATE->windowHeight - h) * .5;
        keyboard.alignment = keyboard.RIGHT;
    }
    _min.height = _max.height = 2 * _STATE->textsize2;
    _min.width = _max.width = w * 0.5 - 2 * _STATE->textsize2;
    _min.startx = _max.startx = w * .5 + _STATE->textsize2;
    _min.stopx = _max.stopx = _min.startx + _min.width;
    _min.starty = 2.5 * _STATE->textsize2;
    _min.stopy = _min.starty + _min.height;
    _max.starty = _min.stopy.load();
    _max.stopy = _max.starty + _max.height;
    keyboard.init();
  }

void MidiLearning::render(void *ctx) {

    if (width != _STATE->windowWidth || height != _STATE->windowHeight) init();
    auto c = _STATE->graphics.getCanvas(index, x, y, w, h);
    if (!c)
        return;
    c->clear(skcol::wbg);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(skcol::fg);
    
    float w1 = 0;
    SkFont font(_STATE->font_normal);
    font.setSize(fs);
    SkFontMetrics metrics{};
    font.getMetrics(&metrics);
    float x1, y1;
    w1 = measureTextFixed(w, fs, font, title.c_str(), &x1, &y1, fs);
    paint.setColor(skcol::text);
    float multi = .25f;
    c->drawSimpleText(title.c_str(), title.size(), SkTextEncoding::kUTF8, x1,
                      multi * lh + offset, font,
                      paint);
    multi += 1.0;
    if (isControl) {
        c->drawSimpleText("Min:", strlen("Min:"), SkTextEncoding::kUTF8, w / 4 - 2 * fs,
                          multi * lh + offset, font,
                          paint);
        multi += 1.f;
        c->drawSimpleText("Max:", strlen("Max:"), SkTextEncoding::kUTF8, w / 4 - 2 * fs,
                          multi * lh + offset, font,
                          paint);
        multi += 1.f;
        _min.render(c);
        _max.render(c);
    }
    else if (_STATE->parameters[id].type == ParameterType_enum) {
        multi += 0.25;
        const auto h = hot.load();
        const auto incactive = increment.load();
        float width_rect = 4 * fs;
        float height_rect = lh;
        float radius = lh / 10.f;
        float startx_rect = w/2 * incactive + (w/2 - width_rect) * .5f + lw / 2;
        float starty_rect = multi * lh + lw / 2 + offset - lh/2 - fs/2;
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor(skcol::blue_transparent);
        c->drawRoundRect(
                SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
                radius, radius, paint);
        if (h != -1 && h != incactive) {
            startx_rect = w / 2 * h + (w / 2 - width_rect) * .5f + lw / 2;
            c->drawRoundRect(
                SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
                radius, radius, paint);
        }
        paint.setColor(skcol::fg);

        const char* dec = "--"; const char* inc = "++";
        centerText(font, w / 2, lh, dec, x1, y1);
        c->drawSimpleText(dec, strlen(dec), SkTextEncoding::kUTF8, x1,
            multi * lh + offset, font,
            paint);

        centerText(font, w / 2, lh, inc, x1, y1);

        c->drawSimpleText(inc, strlen(inc), SkTextEncoding::kUTF8, w / 2 + x1,
            multi * lh + offset, font,
            paint);
        multi += 1.25f;
    }
    else 
        multi += 0.5;

    std::string m;
    if (!eventReceived) {
        if (isControl) {
            m = "Waiting for MIDI \"Control Change\" Event...";
        } else
            m = "Waiting for MIDI \"Note On\" Event...";

    } else {
        auto channel = (int8_t) (_STATE->activemidicommand[0] & STATUS_CHANNEL_MASK);
        auto control = (int8_t)_STATE->activemidicommand[1];
        m = "Event Received. Channel: ";
        m += std::to_string(channel);
        m += " Num: ";
        m += std::to_string(control);
    }

    w1 = measureTextFixed(w, fs, font, m.c_str(), &x1, &y1, fs);
    c->drawSimpleText(m.c_str(), m.size(), SkTextEncoding::kUTF8, x1,
                      multi * lh + offset, font,
                      paint);
    multi += 1.5f;
    w1 = measureTextFixed(w / 2, fs, font, "CANCEL", &x1, &y1, fs);
    c->drawSimpleText("CANCEL", strlen("CANCEL"), SkTextEncoding::kUTF8, x1,
                      multi * lh + offset, font,
                      paint);
    if (eventReceived) {
        w1 = measureTextFixed(w / 2, fs, font, "SAVE", &x1, &y1, fs);
        c->drawSimpleText("SAVE", strlen("SAVE"), SkTextEncoding::kUTF8, w / 2 + x1,
                          multi * lh + offset, font,
                          paint);
    }
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1.0);
    paint.setColor(skcol::border);
    c->drawRect(SkRect::MakeXYWH(0.5, 0.5, w-1, h-1), paint);
}

void MidiLearning::callback(const tsl::graphics::InputEvent &e) {
    if (isControl && (_min.hasFocus || _max.hasFocus)) {
        int ret = keyboard.cb(e);
        if (ret != -1) {
            if (ret == -2) {
                auto e2 = e;
                e2.action = tsl::graphics::ACTION_KEY_DOWN;
                if (_min.hasFocus)
                    _min.callback(e2);
                else if (_max.hasFocus)
                    _max.callback(e2);
            } else {
                if (ret == tsl::graphics::VKEY_RETURN) {
                    _min.hasFocus = false;
                    _max.hasFocus = false;
                } else if (ret == tsl::graphics::VKEY_ESCAPE) {
                    delCB();
                    deldraw();
                } else {
                    auto e2 = e;
                    e2.pointer_id = ret;
                    e2.action = tsl::graphics::ACTION_KEY_UP;
                    if (_min.hasFocus)
                        _min.callback(e2);
                    else if (_max.hasFocus)
                        _max.callback(e2);
                }
            }
            return;
        }
    }
    if (e.action == tsl::graphics::ACTION_DOWN) {
        if (e.x > x && e.x < x + w && e.y > y + h - 3 * _STATE->textsize2 && e.y < y + h) {
            if (e.x < x + w / 2) {
                _STATE->waitNotify.complete(waitToken);
            } else if (eventReceived) {
                std::string min, max;
                if (isControl) {
                    min = _min.getText(), max = _max.getText();
                    if (!tsl::graphics::TextInput::isValidNumber(min) ||
                        !tsl::graphics::TextInput::isValidNumber(max))
                        return;
                }
                if (!save(tindex, isControl ? std::stod(min) : 0, isControl ? std::stod(max) : 0)) {
                    showToast(_STATE, "OK");
                    _STATE->waitNotify.complete(waitToken);
                }
            }
        } else if (isControl) {
            float xx = e.x - x, yy = e.y - y;

            if (yy >= _min.starty && yy < _min.stopy) {
                _min.hasFocus = true;
                _max.hasFocus = false;
                keyboard.addDraw();
//tsl::graphics::TextInput::showSoftInput(type);
            } else if (yy >= _max.starty && yy < _max.stopy) {
                _max.hasFocus = true;
                _min.hasFocus = false;
                keyboard.addDraw();
//tsl::graphics::TextInput::showSoftInput(type);
            } else {
                _min.hasFocus = _max.hasFocus = false;
                keyboard.deldraw();
            }
        }
        else if (_STATE->parameters[id].type == ParameterType_enum &&
            e.y > y + 1.5 * lh + lw / 2 + offset - lh / 2 - fs / 2 &&
            e.y < y + 2.5 * lh + lw / 2 + offset - lh / 2 - fs / 2) {

            // rect 1: centered at w/4, width 4*fs
            const float rect1_cx = x + w / 4;
            const float rect2_cx = x + 3 * w / 4;
            const float half_w = 2 * fs;  // half of 4*fs

            if (e.x > rect1_cx - half_w && e.x < rect1_cx + half_w) {
                down(e);
                hot.store(0);
            }
            else if (e.x > rect2_cx - half_w && e.x < rect2_cx + half_w) {
                down(e);
                hot.store(1);
            }
        }
    }else if (e.action == tsl::graphics::ACTION_KEY_UP) {
        if (e.pointer_id == tsl::graphics::VKEY_RETURN) {
            _min.hasFocus = false;
            _max.hasFocus = false;
        } else if (e.pointer_id == tsl::graphics::VKEY_ESCAPE) {
            _STATE->waitNotify.complete(waitToken);
        } else {
            if (_min.hasFocus)
                _min.callback(e);
            else if (_max.hasFocus)
                _max.callback(e);
            
        }
    }
    else if(e.action == tsl::graphics::ACTION_UP && _STATE->parameters[id].type == ParameterType_enum && e.pointer_id == pointerid) {
        auto h = hot.load();
        if (h == 0)increment.store(false);
        else if (h == 1)increment.store(true);
    }
    else if (e.action == tsl::graphics::ACTION_MOVE && _STATE->parameters[id].type == ParameterType_enum) {
        if(check(e))hot.store(-1);
    }
}
bool MidiLearning::check(const InputEvent& e) {
    if (pointerid == e.pointer_id && spacing(xpos, e.x, ypos, e.y) > _STATE->textsize2) {
        pointerid = -1;
        return true;
    }
    else return false;
}

void MidiLearning::wait(tsl::AppState *appState, int track, uint16_t id) {

    Param &miditarget = appState->parameters[id];
    if (id == PARAM_NOT_ASSIGNED)
        return;
    auto midiLearning = appState->midiLearning.load();
    
    if (midiLearning != nullptr) {
        midiLearning->deldraw();
        midiLearning->delCB();
    }
    else {
        midiLearning = std::make_shared<MidiLearning>(appState);
        appState->midiLearning = midiLearning;
    }

    auto token = appState->waitNotify.begin_wait();
    midiLearning->setup(id, track, false, token);
    midiLearning->init();
    midiLearning->addDraw();
    midiLearning->addCB();
    appState->waitNotify.wait_for_signal(token);
    midiLearning->deldraw();
    midiLearning->delCB();
}

void MidiLearning::setEventReceived(tsl::AppState *appState) {
    auto obj1 = appState->midiLearning.load();
    if (obj1 != nullptr){
        obj1->eventReceived = true;
    }
}

void MidiLearning::addRecursiveDraw() {
    _STATE->midilearning_waiting = true;
    increment.store(true);
    hot = -1;
    View::addRecursiveDraw();
}

void MidiLearning::delRecursiveDraw() {
    keyboard.delRecursiveDraw();
    _STATE->graphics.deleteWindow(index);
    View::delRecursiveDraw();
    _STATE->midilearning_waiting = false;
};

int MidiLearning::save(int tindex, MYFLOAT min, MYFLOAT max) {
    if (id == PARAM_NOT_ASSIGNED)
        return 1;
    tsl::parameters::Event e;
    e.setup(_STATE, tindex, id);
    auto& m = _STATE->parameters[e.getDisplayParam()];


    MYFLOAT mintoset{}, maxtoset{};
    if (m.type == ParameterType_double) {
        auto bounda = min;
        auto boundb = max;
        if (bounda == boundb) {
            showToast(_STATE, "Invalid Range.");
            return 1;
        }
        if (bounda > boundb) {
            std::swap(bounda, boundb);
        }
        
        mintoset = m.fromDisplay(_STATE->sr, bounda);
        maxtoset = m.fromDisplay(_STATE->sr, boundb);
        LOGE("MidiLearning out of range.\n"
            "mintoset=%.9f min=%.9f\n"
            "maxtoset=%.9f max=%.9f",
            mintoset, m.min,
            maxtoset, m.max);

        if (mintoset < m.min || mintoset > m.max || maxtoset < m.min || maxtoset > m.max) {
            showToast(_STATE, "Invalid Range.");
            return 1;
        }
    }


    auto channel = (int8_t) (_STATE->activemidicommand[0] & STATUS_CHANNEL_MASK);
    auto control = (int8_t)_STATE->activemidicommand[1];
    e.midiState.type = m.type;
    e.midiState.channel = channel;
    e.midiState.num = control;
    
    if (m.type == ParameterType_double) {
        e.midiState.min = m.toNormalized(mintoset) * UINT16_MAX;
        e.midiState.max = m.toNormalized(maxtoset) * UINT16_MAX;
        _STATE->midicontrolevents[channel][control] = e;
    } else {
        if (_STATE->parameters[id].type == ParameterType_enum)e.midiState.inc = increment.load() ? 1 : -1;
        _STATE->midinoteevents[channel][control] = e;
    }
    return 0;
}