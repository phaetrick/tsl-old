#ifndef Slider_H
#define Slider_H

#include "Input.h"
#include "params.h"

namespace tsl {
    namespace graphics {

        // Horizontal slider (label + track). Writes through tsl::parameters::Event like
        // the knob does, so a drag reaches the host's automation and the info panel.
        // (It used to store into params[0] directly and skip all of that. Voltaic has no
        // undo History — that is grainstorm's Snapshot — but the host notification and
        // the mod-range invalidation in Event::apply are real and were being lost.)
        class Slider : public View {
        public:
            Slider(tsl::AppState* appState, float scalefactor, int aspect_ratio, int alignment, int id, long offset = 0,
                   int multi = 1, Layout *parent = nullptr);

            void init() override;

            void callback(const InputEvent &event) override;

            void render(void *ctx) override;

            enum {
                SLIDERVIEW = 0,
                TEXTVIEW = 1
            };
        protected:
            void delRecursiveCB() override {
                inputstate.clear();
                tv_touched = false;
                return View::delRecursiveCB();
            };
            float width_text = 0;
            float height_slider = 0;
            float width_slider = 0;
            float startx_slider = 0;
            float starty_slider = 0;
            float stopx_slider = 0;
            float stopy_slider = 0;
            float abs_startx_slider = 0;
            float abs_stopx_slider = 0;
            float startx_tv = 0;
            float width_tv = 0;
            float posx_text = 0, posy_text = 0;
            // Double-tap-to-default timing lives in inputstate.timer (seconds).
            tsl::graphics::InputSystem::InputState inputstate;
            // Re-resolved on every ACTION_DOWN: Event::setup folds the offset parameter
            // into paramIndex, so the id a gesture writes can move between gestures.
            tsl::parameters::Event e{};
            std::atomic<bool> tv_touched{};
        };

        // Vertical fader variant (label on top, vertical track). Geometry ported from
        // grainstorm's SliderVert; reuses Slider's simple param handling.
        class SliderVert : public Slider {
        public:
            SliderVert(tsl::AppState* appState, float scalefactor, int aspect_ratio, int alignment, int id,
                       long offset = 0, int multi = 1, Layout *parent = nullptr)
                : Slider(appState, scalefactor, aspect_ratio, alignment, id, offset, multi, parent) {}

            void init() override;
            void callback(const InputEvent &event) override;
            void render(void *ctx) override;

        private:
            float _startxBar = 0, _widthBar = 0;
        };
    }
}
#endif
