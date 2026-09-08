//
// Created by pr on 01.10.19.
//

#ifndef GRAINSTORM_CheckBox_H
#define GRAINSTORM_CheckBox_H


#include <atomic>
#include <cstring>
#include <defines.h>
#include <types.h>
#include <view.h>

namespace tsl {
    namespace graphics {
        class CheckBoxView : public View {
        public:
            CheckBoxView(tsl::AppState* appState) : View(appState, 3., RATIO_FROM_PARENT_View, CENTER_ALIGN, 10, false, "CBV") {};

            virtual void render(void *ctx);

            std::atomic<int> state{};
        };

        class CheckBox : public View {
        public:
            CheckBox(tsl::AppState* appState, int orientation, int alignment, uint16_t id, long offset = 0, int multi = 0);

            virtual void init();

            void callback(const InputEvent &e);

            void render(void *ctx);

            virtual void computeSize();

            CheckBoxView checkbox;
            TitleView tv;
            int orientation;
        protected:
            void delRecursiveCB() {
                checkbox.state = NORMAL;
                pointerid = -1;
                View::delRecursiveCB();
            };
            int pointerid{-1};
            float xpos{-1}, ypos{-1};

            void down(const InputEvent &e);
        };


        class CheckBoxWrapped : public CheckBox {
        public:
            CheckBoxWrapped(tsl::AppState* appState, int alignment, uint16_t id, long offset = 0, int multi = 0);

            void init();

            void computeSize() {};
        };
    }
}
#endif //GRAINSTORM_CheckBox_H
