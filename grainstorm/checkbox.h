#pragma once
//
// Created by pr on 01.10.19.
//

#ifndef GRAINSTORM_CheckBox_H
#define GRAINSTORM_CheckBox_H


#include <atomic>
#include <cstring>
#include <defines.h>
#include "types.h"
#include "view.h"
#include "textview.h"

namespace tsl::graphics {
    class CheckBoxView : public View {
    public:
        CheckBoxView(tsl::AppState* appState) : View(appState, 3., RATIO_FROM_PARENT_View,
                              START_ALIGN, 10) {};

        virtual void render(void *ctx) override ;

        std::atomic<int> state{};
    };

    class CheckBox : public View {
    public:
        CheckBox(tsl::AppState* appState, int32_t orientation, int alignment, uint16_t id, long offset = 0, int multi = 1);

        virtual void init() override ;

        void callback(const InputEvent &e) override ;

        void render(void *ctx) override ;

        virtual void computeSize() override ;

        CheckBoxView checkbox;
        TitleView tv;
        int32_t orientation;
    protected:
        void delRecursiveCB() override {
            checkbox.state = NORMAL;
            pointerid = -1;
            View::delRecursiveCB();
        };
        int32_t pointerid{-1};
        float xpos{-1}, ypos{-1};

        void down(const InputEvent &e);
    };


    class CheckBoxWrapped : public CheckBox {
    public:
        CheckBoxWrapped(tsl::AppState* appState, int32_t alignment, uint16_t id, long offset = 0, int multi = 0);

        void init() override ;

        void computeSize() override {};
    };
}
#endif //GRAINSTORM_CheckBox_H
