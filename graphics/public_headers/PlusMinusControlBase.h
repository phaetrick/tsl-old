#pragma once
#ifndef PLUS_MINUS_CONTROL_BASE_H
#define PLUS_MINUS_CONTROL_BASE_H

#include "defines.h"
#include "types.h"
#include "view.h"
#include "textview.h"
#include "ButtonBase.h"
#include "knob.h"
#include <stdint.h>
#include <atomic>

#define VALUEView 0
#define MINUSView 1
#define PLUSView 2
#define TITLEView 3


    namespace tsl::graphics{

        class PlusMinusControl : public View {
        public:
            PlusMinusControl(tsl::AppState *, float scalefactor,
                             int aspect_ratio, int alignment, int id, const char *title = nullptr);

            PlusMinusControl(tsl::AppState *, float scalefactor,
                             int aspect_ratio, int alignment, int id, int offset, int multi = 1,
                             const char *title = nullptr);

            void init() override ;

            void render(void *ctx) override;

            void callback(const InputEvent &ev) override;

        protected:
            void delRecursiveDraw() override;
            void addRecursiveDraw() override;

            void delRecursiveCB() override ;

            int which{};
            TitleView view_title;
            PlusMinusButton plus;
            PlusMinusButton minus;
            bool pressed = false;
            const char *title = nullptr;
            tsl::graphics::InputSystem::InputState pointers;;
        private: 
            tsl::parameters::Event e{};
        };

    }

#endif