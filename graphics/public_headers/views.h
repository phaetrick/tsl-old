#pragma once
//
// Created by pr on 19.10.23.
//

#ifndef POCKET_ANALOG_VIEWS_H
#define POCKET_ANALOG_VIEWS_H

#include "view.h"

namespace tsl {
    namespace graphics {
#ifdef HAS_AUDIO
        class xrunview : public View {
        public:
            void render(void *) override;

        private:
            tsl::AtomicTimer timer;
        protected:
            void addRecursiveDraw() override {
                timer.reset();
                View::addRecursiveDraw();
            }

        private:
            int index{-1};

        };
#endif
    }
}

#endif //POCKET_ANALOG_VIEWS_H
