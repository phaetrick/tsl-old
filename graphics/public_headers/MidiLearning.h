#pragma once
//
// Created by pr on 19.09.23.
//

#ifndef POCKET_ANALOG_MIDILEARNING_H
#define POCKET_ANALOG_MIDILEARNING_H

#include "types.h"
#include "Input.h"
#include "keyboard.h"
#include "view.h"


namespace tsl{
    namespace graphics{

        class MidiLearning : public View{
        public:
            MidiLearning(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0), _min(appState), _max(appState), keyboard(appState) {};
            void setup(int _id, int track, bool _isLFO, tsl::ThreadSignal::WaitToken waitToken_);

            void init() override;

            void render(void *ctx) override;

            void callback(const InputEvent &e) override;


            static void wait(tsl::AppState* appState, int track, uint16_t);

            static void setEventReceived(tsl::AppState *);

            static MidiLearning obj;

            void addRecursiveDraw() override;

            void delRecursiveDraw() override;


        private:
            std::string title;
            std::atomic<bool> eventReceived{false};
            TextInput _min, _max;
            NumericalKeyboard keyboard;
            int x{}, y{}, w{}, h{};
            int index{-1};
            int tindex{};
            int type{};
            bool isControl{};

            int save(int track, MYFLOAT min, MYFLOAT max);
            tsl::ThreadSignal::WaitToken waitToken{};
            std::atomic<bool> increment{ true };
            std::atomic<int> hot{-1};
            int pointerid{ -1 };
            float xpos{ -1 }, ypos{ -1 };

            void down(const InputEvent& e) {
                xpos = e.x;
                ypos = e.y;
                pointerid = e.pointer_id;
            }
            float lh{}, fs{}, offset{};


            bool check(const InputEvent& e);

        };
    }
}



#endif //POCKET_ANALOG_MIDILEARNING_H
