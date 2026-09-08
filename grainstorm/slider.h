#pragma once
#ifndef Slider_H
#define Slider_H
#include <Input.h>
#include "EnterValue.h"
#include "params.h"

namespace tsl { namespace graphics {
    class Slider : public View {
    public:
        Slider(tsl::AppState *appState, float scalefactor, int32_t aspect_ratio, int alignment, int id, long offset = 0,
               int32_t multi = 1, Layout *parent = nullptr);

        virtual void init() override ;

        virtual void callback(const InputEvent &event) override ;

        virtual void render(void *ctx) override ;
        
        void addRecursiveDraw()override;

        enum {
            SLIDERVIEW = 0,
            TEXTVIEW = 1
        };
    protected:
        void delRecursiveCB() override {
            inputstate.clear();
            tv_touched = false;
            valueView.delRecursiveDraw();
            View::delRecursiveCB();
        };
        ValueView valueView;
        tsl::graphics::InputSystem::InputState inputstate;
        std::atomic<bool> tv_touched{};
        float height_slider = 0;
        float starty_slider = 0;
        float posx_text = 0, posy_text = 0;
        float width_text = 0;
		tsl::parameters::Event e{};
    private:
        float width_slider = 0;
        float startx_slider = 0;
        float stopx_slider = 0;
        float stopy_slider = 0;
        float abs_startx_slider = 0;
        float abs_stopx_slider = 0;
        float startx_tv = 0;
        float width_tv = 0;
    };

    class SliderVert : public Slider {
    public:
        SliderVert(tsl::AppState *appState, float scalefactor, int32_t aspect_ratio, int alignment, int id, long offset = 0,
                   int32_t multi = 1, Layout *parent = nullptr) : Slider(appState, scalefactor, aspect_ratio,
                                                                     alignment, id, offset, multi,
                                                                     parent) {};

        virtual void init() override ;

        virtual void callback(const InputEvent &event) override ;

        virtual void render(void *ctx) override ;

    private:
        float _startxBar{}, _widthBar{};
    };
} }

#endif