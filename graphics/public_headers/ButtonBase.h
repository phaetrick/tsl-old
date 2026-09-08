#pragma once
//
// Created by pr on 19.09.23.
//

#ifndef GRAINSTORM_BUTTONBASE_H
#define GRAINSTORM_BUTTONBASE_H

#include <cstring>
#include "defines.h"
#include "view.h"
#include "Input.h"

namespace tsl {
    namespace graphics {
        class Button : public View {
        public:
            Button(tsl::AppState* appState): View(appState) { prio = 10; };

            Button(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment,
                   const char *_normal = nullptr,
                   const char *_pressed = nullptr, const char *_name = "Button") : View(appState,
                    _scalefactor,
                    _aspect_ratio,
                    _alignment, 10,
                    false, _name) {
                name_normal = _normal;
                name_pressed = _pressed;
                prio = 10;
            };
#ifndef USE_CHAR_FOR_CHAR8_T
 
            Button(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, const char8_t _normal[4],
                   const char8_t _pressed[4], const char *_name = "Button") : View(appState, _scalefactor,
                                                                                   _aspect_ratio,
                                                                                   _alignment, 10,
                                                                                   false, _name) {
                name_normal = reinterpret_cast<const char *>(_normal);
                name_pressed = reinterpret_cast<const char *>(_pressed);
                prio = 10;
            };
#endif
            Button(const Button &other) : View(other) {
                state.store(other.state.load());
                name_normal = other.name_normal;
                name_pressed = other.name_pressed;
                textpadding = other.textpadding;
                imagebutton = other.imagebutton;
                cb = other.cb;
                C_active = other.C_active;
                iconScale = other.iconScale;
            }

            void setState(int state, bool redraw = true);

            void setStateRedrawParent(int state, bool redraw = true);

            virtual void render(void *ctx) override = 0;

            virtual void callback(const InputEvent &e) override {};
            
            std::atomic<int> state{};
            const char *name_normal{};
            const char *name_pressed{};
            float textpadding{};
            bool imagebutton{};

            void (*cb)(View *){};

            uint32_t C_active{tsl::sk_colours::active};
            float iconScale{1.0f};
            int pointerid{-1};
            float xpos{-1}, ypos{-1};

            void down(const InputEvent& e) {
                xpos = e.x;
                ypos = e.y;
                pointerid = e.pointer_id;
            }


            bool check(const InputEvent& e);
            
            virtual void delRecursiveDraw() override;

            virtual void delRecursiveCB() override;
        };

        class TextButtonFramed : public Button {
        public:
            TextButtonFramed(tsl::AppState* appState, const char *text) : Button(appState) {
                name_normal = text;
                name_pressed = text;
                prio = 10;
            };

            TextButtonFramed(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment,
                             const char *_normal = nullptr,
                             const char *_pressed = nullptr, const char *_name = "Button") : Button(appState,
                    _scalefactor,
                    _aspect_ratio,
                    _alignment, _normal,
                    _pressed, _name) {
                prio = 10;
            }

            virtual void render(void *) override;

            void computeWidth() override;
            bool drawRoundedRect{true};
        };

        class TextButton : public TextButtonFramed {
        public:
            TextButton(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, const char* text,
                int _id,
                int offset, int offsetmulti = 0);

            void render(void *) override;

            void callback(const InputEvent& e) override;

        };

        // Self-sizing text button bound to a bool param (toggles on release via the
        // Event API). Sizing/alignment mirrors PA's former TextButton2:
        // VALUE_FROM_POINTER + size_reference=&textsize1, width from text.
        class TextToggle : public TextButtonFramed {
        public:
            TextToggle(tsl::AppState* appState, const char *text, int32_t id = 0);

            void render(void *) override;

            void callback(const InputEvent& e) override;

            void computeWidth() override;

            bool midiHighlight{};
        };

        class PlusMinusButton : public Button {
        public:
            PlusMinusButton(tsl::AppState* appState, const char *_text) : Button(appState) {
                name_normal = _text;
            }

            PlusMinusButton(tsl::AppState* appState, const char *_text, int _id) : Button(appState) {
                name = "PMButton";
                name_normal = _text;
                id = _id;
            };

            void render(void *ctx) override;
        };

        class IconButton : public Button {
        public:
            IconButton(tsl::AppState* appState, const char *_text) : Button(appState) {
                name_normal = _text;
            }

            void render(void *ctx) override;
        };

    }
}

#endif //GRAINSTORM_BUTTONBASE_H
