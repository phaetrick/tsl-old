//
// Created by pr on 23.04.21.
//

#ifndef GRAINSTORM_PIANOVIEW_H
#define GRAINSTORM_PIANOVIEW_H


#include "Input.h"
#include <logger.h>
#include <string>
#include <include/core/SkFont.h>

#define KEYSVISIBLE 15
#define TOTALKEYS 88
#define WHITEKEYS 63//52 + 11
#define BLACKKEYS 44//36 + 8
#define INITOFFSET 23.f / (float) (WHITEKEYS-KEYSVISIBLE)


namespace tsl {
    namespace graphics {
        class PianoBase {

        };

        class PianoKey : public View, public PianoBase {
        public:
            PianoKey() : View() {
                prio = 0;
            }

            virtual bool touchDown();

            virtual void touchUp();

            int num{};
            std::string name;
            char midinum{};
            float freq{};

            void delRecursiveDraw() override {
                View::delRecursiveDraw();
            }

        protected:
            std::atomic_bool pressed{};
        };

        class WhiteKey : public PianoKey {
        public:
            WhiteKey() : PianoKey() {
                prio = 1;
            }

            void init() override;

            bool touchDown() override {
                if (PianoKey::touchDown())
                    return true;
                redrawKeys[0]->redraw();
                if (redrawKeys[1] != redrawKeys[0])
                    redrawKeys[1]->redraw();
                return false;
            }

            void touchUp() override {
                PianoKey::touchUp();
                redrawKeys[0]->redraw();
                if (redrawKeys[1] != redrawKeys[0])
                    redrawKeys[1]->redraw();
            }

            void render(void *context) override;

            PianoKey *redrawKeys[2];
        private:
            float textx{}, texty{};
            bool drawText{};
        };

        class BlackKey : public PianoKey {
        public:

            void render(void *context) override;

        private:
        };


        class Piano : public View, public PianoBase {
        public:

            Piano();

            void init() override;

            void render(void *ctx) override;


            void callback(const InputEvent &event) override {
                int action = event.action;
                int _pointerid = event.pointer_id;
                float xpos = event.x;
                float ypos = event.y;
                switch (action) {
                    case ACTION_DOWN: {
                        for (auto &key : blackKeys) {
                            if (xpos >= key.startx && xpos < key.stopx && ypos >= key.starty &&
                                ypos < key.stopy) {
                                if (!key.touchDown())
                                    touched.push_back({_pointerid, &key});
                                return;
                            }
                        }
                        for (auto &key : whitekeys) {
                            if (xpos >= key.startx && xpos < key.stopx && ypos >= key.starty &&
                                ypos < key.stopy) {
                                if (!key.touchDown())
                                    touched.push_back({_pointerid, &key});
                                return;
                            }
                        }
                        break;
                    }
                    case ACTION_MOVE: {
                        auto it = touched.begin();
                        bool newcheck = false;
                        while (it != touched.end()) {
                            if (it->pointerid == _pointerid &&
                                (xpos < it->key->startx || xpos >= it->key->stopx ||
                                 ypos < it->key->starty || ypos >= it->key->stopy)) {
                                it->key->touchUp();
                                it = touched.erase(it);
                                newcheck = true;
                            } else ++it;
                        }
                        if (newcheck) {
                            for (auto &key : blackKeys) {
                                if (xpos >= key.startx && xpos < key.stopx && ypos >= key.starty &&
                                    ypos < key.stopy) {
                                    key.touchDown();
                                    touched.push_back({_pointerid, &key});
                                    return;
                                }
                            }
                            for (auto &key : whitekeys) {
                                if (xpos >= key.startx && xpos < key.stopx && ypos >= key.starty &&
                                    ypos < key.stopy) {
                                    key.touchDown();
                                    touched.push_back({_pointerid, &key});
                                    return;
                                }
                            }
                        }
                        break;
                    }
                    case ACTION_UP: {
                        auto it = touched.begin();
                        while (it != touched.end()) {
                            if (it->pointerid == _pointerid) {
                                it->key->touchUp();
                                it = touched.erase(it);
                            } else ++it;
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            WhiteKey whitekeys[WHITEKEYS];
            BlackKey blackKeys[BLACKKEYS];

            void delRecursiveDraw() override {
                for (auto &t : whitekeys)
                    t.delRecursiveDraw();
                for (auto &t : blackKeys)
                    t.delRecursiveDraw();
                View::delRecursiveDraw();
            }

            void delRecursiveCB() override {
                for (auto t : touched)
                    t.key->touchUp();
                touched.clear();
                View::delRecursiveCB();
            };;
            float oldoffset{INITOFFSET};
        private:
            bool firstrun{true};
            static struct PianoEvent {
                int pointerid{};
                PianoKey *key{};
            };
            std::deque<PianoEvent> touched;

            static constexpr int redrawKeys[WHITEKEYS][2] = {{0,  0},
                                                             {0,  0},
                                                             {1,  1},
                                                             {1,  2},
                                                             {2,  2},
                                                             {3,  3},
                                                             {3,  4},
                                                             {4,  5},
                                                             {5,  5},
                                                             {6,  6},
                                                             {6,  7},
                                                             {7,  7},
                                                             {8,  8},
                                                             {8,  9},
                                                             {9,  10},
                                                             {10, 10},
                                                             {11, 11},
                                                             {11, 12},
                                                             {12, 12},
                                                             {13, 13},
                                                             {13, 14},
                                                             {14, 15},
                                                             {15, 15},
                                                             {16, 16},
                                                             {16, 17},
                                                             {17, 17},
                                                             {18, 18},
                                                             {18, 19},
                                                             {19, 20},
                                                             {20, 20},
                                                             {21, 21},
                                                             {21, 22},
                                                             {22, 22},
                                                             {23, 23},
                                                             {23, 24},
                                                             {24, 25},
                                                             {25, 25},
                                                             {26, 26},
                                                             {26, 27},
                                                             {27, 27},
                                                             {28, 28},
                                                             {28, 29},
                                                             {29, 30},
                                                             {30, 30},
                                                             {31, 31},
                                                             {31, 32},
                                                             {32, 32},
                                                             {33, 33},
                                                             {33, 34},
                                                             {34, 35},
                                                             {35, 35},
                                                             {36, 36},
                                                             {36, 37},
                                                             {37, 37},
                                                             {38, 38},
                                                             {38, 39},
                                                             {39, 40},
                                                             {40, 40},
                                                             {41, 41},
                                                             {41, 42},
                                                             {42, 42},
                                                             {43, 43},
                                                             {43, 43}};
            static constexpr int blackhits[BLACKKEYS] = {0, 2, 3, 5, 6, 7, 9, 10, 12, 13, 14, 16,
                                                         17, 19,
                                                         20, 21, 23, 24, 26, 27, 28, 30, 31, 33, 34,
                                                         35, 37,
                                                         38, 40, 41, 42, 44, 45, 47, 48, 49, 51, 52,
                                                         54, 55,
                                                         56, 58, 59, 61};
        };


        class PianoScrollBar : public View, public PianoBase {
        public:
            PianoScrollBar() : View() {
                scalefactor = VALUE_FROM_POINTER;
                aspect_ratio = VALUE_FROM_POINTER;
                alignment = END_ALIGN;
                prio = 0;
                perm = false;
                size_reference_scale = .618f;
            };

            void render(void *ctx) override;

            void callback(const InputEvent &e) override;

            void delRecursiveCB() override;

        private:
            float lastx{}, lasty{};
            int32_t _pointerid{};
        };

        class PianoView : public VerticalLayout, public PianoBase {
        public:
            PianoView(tsl::AppState* appState, float scalefactor, int aspect_ratio, int alignment);
            ~PianoView() override;

            std::atomic<float> offset{INITOFFSET};

            void init() override {
                scrollBar._appState = _appState;
                scrollBar.size_reference = &_DATA->panelheight;
                piano._appState = _appState;
                for (auto& k : piano.whitekeys) k._appState = _appState;
                for (auto& k : piano.blackKeys) k._appState = _appState;
                viewPort.startx = startx.load();
                viewPort.width = width - _DATA->panelheight * .618 -
                                 lw;
                viewPort.stopx = viewPort.startx + viewPort.width;
                viewPort.starty = starty.load();
                viewPort.height = height.load();
                viewPort.stopy = viewPort.starty + viewPort.height;
                SkRect rect = SkRect::MakeXYWH(viewPort.startx, viewPort.starty, viewPort.width, viewPort.height);
                piano.viewport = rect;
                for (auto &k : piano.whitekeys) {
                    k.viewport = rect;
                }
                for (auto &k : piano.blackKeys) {
                    k.viewport = rect;
                }
                VerticalLayout::init();
            }

            View viewPort{};
            Piano piano{};
            PianoScrollBar scrollBar{};
            Divider divider{nullptr, END_ALIGN};
        };
    }
}


#endif //GRAINSTORM_PIANOVIEW_H
