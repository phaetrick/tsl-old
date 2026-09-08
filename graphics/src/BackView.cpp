//
// Created by pr on 06.10.25.
//
#include "BackView.h"

#include <utility>
#include "app.h"
#include "keyboard.h"

using namespace tsl::graphics;

BackView::BackView(tsl::AppState *appState, std::function<void()> cb) : View(appState, 0, 0, 0),
                                                                        cb_(std::move(cb)) {
    prio = 0;
    hasFocus = true;
    addCB();
}

BackView::~BackView() {
    delCB();
}

void BackView::callback(const InputEvent &e) {
    if (e.action == ACTION_DOWN) {
        _state = 1;
        xpos = e.x;
        ypos = e.y;
        pointerid = e.pointer_id;
    } else if (e.action == ACTION_MOVE && pointerid == e.pointer_id &&
               spacing(xpos, e.x, ypos, e.y) > _STATE->textsize2) {
        _state = 0;
    } else if ((e.action == ACTION_UP && _state == 1 && pointerid == e.pointer_id) ||
               (e.action == ACTION_KEY_UP && e.pointer_id == VKEY_ESCAPE)) {
        cb_();
    }

}

void BackView::addRecursiveCB() {
    stopx = width = _STATE->windowWidth;
    stopy = height = _STATE->windowHeight;
    View::addRecursiveCB();
}

void BackView::delRecursiveCB() {
    _state = 0;
    pointerid = -1;
    xpos = ypos = -1;
    View::delRecursiveCB();
}
