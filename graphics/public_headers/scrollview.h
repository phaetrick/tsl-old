#pragma once
//
// Created by pr on 08.10.20.
//

#ifndef GRAINSTORM_SCROLLView_H
#define GRAINSTORM_SCROLLView_H

#include "view.h"

namespace tsl::graphics {
    class ScrollBar : public View {
    public:
        ScrollBar(tsl::AppState* appState) : View(appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, END_ALIGN, 10, false) {};

        void render(void *ctx) override;

        void callback(const InputEvent &e) override;

        void delRecursiveCB() override;

    private:
        float lastx{}, lasty{};
        int32_t _pointerid{};
        tsl::time timer;
    };

    class ScrollView : public Layout {
    public:
        ScrollView(tsl::AppState* appState, float scalefactor, int aspect_ratio, int alignment, int orientation, float _scale = 1.0);
        ~ScrollView() override{};
        void init()override;

        HorizontalLayout *window{};
        ScrollBar *bar{};
        float scale{};
        std::atomic<float> offset{};
        std::function<float()> func{};
    };
}
#endif //GRAINSTORM_SCROLLView_H
