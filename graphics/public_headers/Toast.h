#pragma once
//
// Created by pr on 25.01.22.
//

#ifndef GRAINSTORM_TOAST_H
#define GRAINSTORM_TOAST_H


#include "view.h"
#include "skia.h"

namespace tsl::graphics {
    class Toast : public View {
    public:
        Toast(tsl::AppState* s) { _appState = s; perm = true; };

        void render(void *ctx) override ;
        void prepare(const std::string& text, const float seconds);
        void delRecursiveDraw() override;
    private:
        struct ToastLine {
            std::string string;
            float y{};
        };
        uint64_t _time{}, _ns{};
        float x{};
        int windowindex{-1}, logoindex{-1};
        std::vector<ToastLine> lines;
        float fontsize{};
        //sk_sp<SkImage> logo;
    };

    void showToast3(tsl::AppState*, const std::string text, const float seconds = 2.5f);
}

#endif //GRAINSTORM_TOAST_H
