#pragma once
#ifndef KNOB2_H
#define KNOB2_H

#include "view2.h"
#include <string>
#include <atomic>

namespace tsl::ui {

    class Knob : public View {
    public:
        Knob(tsl::AppState* app, int paramId);

        void render(SkCanvas* ctx) override;
        int callback(tsl::graphics::InputEvent& e) override;
        void init() override;

        void setTitle(const std::string& txt) { title = txt; }

        bool drawTitle = true;
        bool drawButtons = true;

    private:
        std::string title;
        int paramID = 0;

        float arcRadius = 0.0f;
        float arcRadiusArrow = 0.0f;
        float centerX = 0.0f;
        float centerY = 0.0f;

        float lastAngle = 0.0f;
        int lastQuadrant = 0;

        enum TouchTarget { NONE_, TITLE, MINUS, PLUS, ARC };
        struct TouchState {
            int pointerId = -1;
            TouchTarget target = NONE_;
            float downX = 0, downY = 0;
        };
        TouchState touch;

        void drawArc(SkCanvas*);
        void drawPointer(SkCanvas*, float percent);
        void drawTitleBar(SkCanvas*);
        void drawButtonsBar(SkCanvas*);

        float getAngle(float x, float y) const;
        int getQuadrant(float x, float y) const;

        void adjustValue(float delta);
        void handleTouchDown(float x, float y, int pointerId);
        void handleTouchMove(float x, float y, int pointerId);
        void handleTouchUp(float x, float y, int pointerId);
    };

} // namespace tsl::ui

#endif
