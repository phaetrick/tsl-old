#pragma once
//
// Created by pr on 08.12.19.
//

#ifndef GRAINSTORM_ENVELOPE_WINDOW_H
#define GRAINSTORM_ENVELOPE_WINDOW_H
#include <types.h>
#include <Input.h>

#include "checkbox.h"
#include "selector.h"
#include <view.h>
#include <EnterValue.h>
#include <cstdint>

#define GRAINENV_SEGMENTS 8

namespace tsl::graphics {
    class EnvelopeWindow : public View {
    public:
        EnvelopeWindow(tsl::AppState* appState, float _scalefactor, int32_t _aspectratio, int _alignment) : View(appState, _scalefactor,
                                                                                    _aspectratio,
                                                                                    _alignment, 10,
                                                                                    false,
                                                                                    "EnvelopeWindow") {

        }

        void render(void *context) override ;

    };

    class LFOWindow : public View {
    public:
        LFOWindow(tsl::AppState* appState, float _scalefactor, int32_t _aspectratio, int _alignment) : View(appState, _scalefactor,
                                                                               _aspectratio,
                                                                               _alignment, 10, true,
                                                                               "LFOWindow") {
        }

        void render(void *context) override;

        void renderRnd(void *c);

	protected:
        void addRecursiveDraw() override;
        void delRecursiveDraw() override;
    private:
        bool firstDraw = false;
    };

    class BetaPDFView : public View {
    public:
        BetaPDFView(tsl::AppState* appState, float _scalefactor, int32_t _aspectratio, int _alignment, int alpha, int beta,
                    int32_t offset) : View(appState, _scalefactor,
                                       _aspectratio,
                                       _alignment, 10, true,
                                       "BetaPDFView") {
            _alpha = alpha;
            _beta = beta;
            _offset = offset;
        }

        void render(void *context) override;

        void redraw() override {
            _prevA = _prevB = -1.f;
            View::redraw();
        }
    protected:
        void addRecursiveDraw() override{
            _prevA = _prevB = -1;
            View::addRecursiveDraw();
        }

    private:
        int32_t _alpha, _beta, _offset;
        float _prevA{}, _prevB{};
    };
}



#define OFFPOINTY 9
#define OFFNSEGS (9 * 2)
#define OFFCURVE (OFFNSEGS + 1)
#define OFFREDRAW (OFFCURVE + 1)
#define OFFRECOMP (OFFREDRAW + 1)
#define OFFJOINENDS (OFFRECOMP + 2)
#define OFFQUANT (OFFJOINENDS + 1)
#define MAX_SEGS 8
#define MAX_POINTERS MAX_SEGS + 1

namespace tsl { namespace graphics {
    class VertScale : public View {
    public:
        VertScale(tsl::AppState* appState) : View(appState, VALUE_FROM_POINTER,
                           VALUE_FROM_POINTER, START_ALIGN) {};

        void render(void *ctx) override ;
    };


    class HozScale : public View {
    public:
        HozScale(tsl::AppState* appState) : View(appState, WRAP, 0, END_ALIGN) {
        }

        void render(void *ctx) override ;
    };

    class EnvWin : public View {
    public:
        virtual void callback(const InputEvent &e) override ;

        EnvWin(tsl::AppState* appState) : View(appState, WRAP,
                        0, END_ALIGN) {};

        void renderHozLines(void *ctx);

        void renderVertLines(void *ctx);

        void renderFrame(void *ctx);

        virtual void render(void *ctx) override ;

        tsl::graphics::InputSystem::InputState inputstate;
        std::atomic<int> _activeSeg{-1};
    };

    class EnvelopeEditorWindow : public HorizontalLayout {
    public:
        EnvelopeEditorWindow(tsl::AppState* appState, float scalefactor, int32_t aspect_ratio, int alignment,
                             int32_t baseid, int orientation = 1);
        void render(void *ctx) override ;

        void callback(const InputEvent &e) override ;

        template<typename T>
        static T freq_at_y(const T y, const T height = 1.) {
            return 200. * pow(40., y / height);
        }

        template<typename T>
        static T y_at_freq(const T f, const T height = 1.) {
            return height * log(f / 200.) / log(40.);
        }

        template<typename T>
        static T freq_at_x(const T x, const T width = 1.) {
            return 20 * pow(1001., x / width) - 20.;
        }

        template<typename T>
        static T x_at_freq(const T f, const T width = 1.) {
            return width * log((f + 20.) / 20.) / log(1001.);
        }

    protected:
        void addRecursiveDraw() override {
            View::addRecursiveDraw();
        }

        void delRecursiveDraw() override {
            View::delRecursiveDraw();
        }

        void addRecursiveCB() override {
            View::addRecursiveCB();
        }

        void delRecursiveCB() override {
            window->inputstate.clear();
            window->_activeSeg = -1;
            View::delRecursiveCB();
        }

        VerticalLayout *dummybottom;
        View *dummybottomleft;
        View *filterfrequencytop;
        VerticalLayout *filterfrequencycenter;
        EnvWin *window;
        VertScale *vertscale;
        HozScale *hozscale;
        int32_t orient{};
    };

    class EnvelopeWindowControls : public VerticalLayout {
    public:
        EnvelopeWindowControls(tsl::AppState *appState, float scalefactor, int32_t aspect_ratio, int alignment,
                               int32_t baseid);

    private:
        Selector1<TitleView> *selnsegs;
        Selector1<WaveformChooser> *selwaveform;
        HorizontalLayout *dummycontrolsenveditor4{};
        HorizontalLayout *dummycb{};
        CheckBox *cb;
    };


    class ADSRWin : public EnvWin {
    public:
        ADSRWin(tsl::AppState* appState);
        void callback(const InputEvent &e) override ;

        void render(void *ctx) override ;
        ValueView valueView;
    };

    class ADSRWindow : public VerticalLayout {
    public:
        ADSRWindow(tsl::AppState* appState, float scalefactor, int32_t aspect_ratio, int alignment,
                   int32_t baseid);

        void render(void *ctx) override ;

        void callback(const InputEvent &e) override ;

    protected:
        void addRecursiveDraw() override {
            View::addRecursiveDraw();
        }

        void delRecursiveDraw() override {
            window->valueView.delRecursiveDraw();
            View::delRecursiveDraw();
        }

        void addRecursiveCB() override {
            View::addRecursiveCB();
        }

        void delRecursiveCB() override {
            window->inputstate.clear();
            window->_activeSeg = -1;
            View::delRecursiveCB();
        }

    private:
        ADSRWin *window;
    };
} }
#endif //GRAINSTORM_ENVELOPE_WINDOW_H
