#pragma once
//
// Created by pr on 15.05.20.
//

#ifndef GRAINSTORM_ENVWIN2_H
#define GRAINSTORM_ENVWIN2_H


#include <cstdint>
#include "types.h"
#include "view.h"
#include "Input.h"
#include <EnterValue.h>
#include "grainstorm.h"
#if defined USE_IMGUI
#include <imgui.h>
#endif
namespace tsl::graphics {
    class LfoEnvEditor : public View {
    public:
        explicit LfoEnvEditor(tsl::AppState *);
        void render(void *ctx) override;

        void callback(const InputEvent &e) override;

        tsl::graphics::InputSystem::InputState inputstate;
        std::atomic<int> _activeSeg{-1};
        ValueView valueView;
        
        void addRecursiveDraw() override {
            auto tindex = _STATE->active_track.load();
            _DATA->tracks[tindex]->lfos[GASLFO]->store(LFOREDRAW, 1.0);
            if(drawBuf == nullptr)
                drawBuf = _STATE->pool.acquire<float>(LFO_TBL_SIZE);
        }
        
    private:
        float *drawBuf{nullptr};
    };


    class GrainEnvEditor : public View {
    public:
        explicit GrainEnvEditor(tsl::AppState*);
        void render(void *ctx) override;

        void callback(const InputEvent &e) override;

        tsl::graphics::InputSystem::InputState  inputstate;
        std::atomic<int> _activeSeg{-1};
        ValueView valueView;
        std::atomic<float> zoom{.8f}, pos{};
        void addRecursiveDraw() override {
            auto tindex = _STATE->active_track.load();
            _STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
        }
    };

    template<typename T>
    class EnvEditor : public View {
    public:
        EnvEditor(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment) : View(appState, _scalefactor,
                                                                                _aspect_ratio,
                                                                                _alignment, 10,
                                                                                true,
                                                                                "EnveditParent"), child(appState) {
            child.paddingtop = child.paddingbottom = 10.0f;
            child.parent = this;
        }


        void init() override {
            child.startx = startx.load();
            child.stopx = stopx.load();
            child.starty = starty.load();
            child.stopy = stopy.load();
            child.width = width.load();
            child.height = height.load();
            child.computePadding();
        }

        void render(void *context) override {
#if defined USE_IMGUI
            ImGui::SetNextWindowPos(ImVec2(startx - 2, starty), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(width + 4, height), ImGuiCond_Always);
            child.render(context);
            return;
#else
            auto* canvas = (SkCanvas*)context;
            canvas->save();
            child.render(context);

			canvas->restore();

#endif
        }

        void callback(const InputEvent &event) override {
            child.callback(event);
        }

        T child;
    protected:
        void delRecursiveCB() override {
            child._activeSeg.store(-1);
            child.inputstate.clear();
            View::delRecursiveCB();
        }
        void delRecursiveDraw() override {
            child.valueView.delRecursiveDraw();
            View::delRecursiveDraw();
        }
        void addRecursiveDraw() override {
            child.addRecursiveDraw();
            View::addRecursiveDraw();
		}
    };
}
#endif //GRAINSTORM_ENVWIN2_H
