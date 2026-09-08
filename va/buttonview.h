#ifndef ButtonView_H
#define ButtonView_H

#include <cstdint>
#include <atomic>
#include <functional>
#include <vector>
#include <app.h>
#include "types.h"
#include "grainstorm.h"
#include "view.h"
#include "textview.h"
#include "button.h"
#include "Input.h"


namespace tsl {
    namespace graphics {

        template<typename T>
        class ButtonView : public View {
        public:
            ButtonView(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
                       int _alignment, int _orientation, std::vector<std::string> &_names,
                       std::vector<float> &_values, uint16_t _id,
                       long _offset = 0,
                       int _multi = 1) : View(appState, _scalefactor, _aspect_ratio, _alignment, 10, false,
                                              "BV"), titleview(appState, "BV", START_ALIGN) {
                id = _id;
                _STATE->parameters[id].view = this;
                _STATE->parameters[id].paramOffset = _offset;
                _STATE->parameters[id].offsetFact = (_multi == 0 ? 1 : _multi);
                framed = false;
                orientation = _orientation;
                for (int i = 0; i < _names.size(); i++) {
                    T button(appState, _names[i].c_str());
                    button.id = id;
                    button.userdata = _values.empty() ? i : (int) _values[i];
                    buttons.push_back(button);
                }
            }


            void init() {
                int titleheight = 0;
                if (height_title != 0)
                    titleheight = (framed ? height - 2 * lw : (int)height) * .3333f;
                int starty_elementsview = framed ? lw + titleheight : titleheight;
                int height_elementsview = framed ? height - (int) titleheight - 3 * lw : (int)height -
                                                                                         (int) titleheight;
                int stopy_elementsview = starty_elementsview + height_elementsview;
                if (titleheight != 0) {
                    titleview.width = framed ? width - 2 * lw : width.load();
                    titleview.height = framed ? titleheight - 2 * lw : titleheight;
                    titleview.startx = startx + (framed ? lw : 0);
                    titleview.stopx = titleview.startx + titleview.width;
                    titleview.starty = starty + (framed ? lw : 0);
                    titleview.stopy = titleview.starty + titleview.height;
                }
                if (orientation == HORIZONTAL) {
                    int max = 0;
                    for (auto &b :buttons) {
                        b.height = height_elementsview;
                        b.computeWidth();
                        max = b.width > max ? b.width.load() : max;
                    }
                    for (int i = 0; i < buttons.size(); i++) {
                        Button &button = buttons.at(i);
                        const int tmp = framed ? (width - 2 * lw) / buttons.size() : width /
                                                                                     buttons.size();
                        button.width = max > tmp ? tmp : max;
                        button.startx = startx + DISTANCE(button.width, tmp) * .5f +
                                        (framed ? lw + i * tmp : i * tmp);
                        button.stopx = button.startx + button.width;
                        button.starty = starty + starty_elementsview;
                        button.stopy = button.starty + button.height;
                        if (framed) {
                            button.padding = 10.f;
                        } else {
                            button.paddingleft = button.paddingright = 5.f;
                        }
                        button.computePadding();
                        button.init();
                    }
                } else {
                    for (int i = 0; i < buttons.size(); i++) {
                        Button &button = buttons.at(i);
                        button.width = framed ? (width - 2 * lw) : width.load();
                        button.startx = startx + (framed ? lw
                                                         : 0); //(bview->framed ? lw + i * button->width : i * button->width);
                        button.stopx = button.startx + button.width;
                        button.height = _STATE->textsize1;
                        button.starty = (int) (starty + starty_elementsview +
                                               i * _STATE->textsize1);
                        button.stopy = button.starty + button.height;
                        if (framed) {
                            button.padding = 10.f;
                        } else {
                            button.padding = 10.f;
                        }
                        button.computePadding();
                        button.init();
                    }
                }
            }

            void callback(const InputEvent &event) {
                int action = event.action;
                float xpos = event.x;
                float ypos = event.y;
                auto tindex = _STATE->active_track.load();
                int offset = 0;
                if (_STATE->parameters[id].paramOffset)
                    offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
                offset = offset * _STATE->parameters[id].offsetFact;
                int val = _STATE->params[tindex][id + offset].load();
                // static NodeQ *savednode = NULL;
                switch (action) {
                    case ACTION_DOWN:
                        for (auto &temp : buttons) {
                            if (xpos >= temp.startx && xpos < temp.stopx && ypos >= temp.starty &&
                                ypos < temp.stopy && val != temp.userdata) {
                                temp.setState(HOT);
                                inputstate.addPointer({event.pointer_id, xpos, ypos,
                                    tsl::graphics::InputSystem::WinState::WINPOINTER, temp.userdata});
                                return;
                            }
                        }
                        break;
                    case ACTION_MOVE: {
                        auto pt = inputstate.getById(event.pointer_id);
                        if (pt && pt->distanceTo(event.x, event.y) > _STATE->textsize2) {
                            for (auto &b : buttons) {
                                if (b.userdata == pt->target)
                                    b.setState(NORMAL);
                            }
                            inputstate.removePointer(event.pointer_id);
                        }
                        break;
                    }

                    case ACTION_UP: {
                        if (auto pt = inputstate.getById(event.pointer_id)) {
                            if (cb != nullptr)
                                cb(id, pt->target);
                            else
                                _STATE->params[tindex][id + offset].store(pt->target);
                            for (auto &b : buttons) {
                                b.setState(NORMAL, false);
                            }
                            inputstate.clear();
                            redraw();
                            return;
                        }
                        break;
                    }
                    default:
                        break;
                }

            }

            void render(void *context) {
                auto *canvas = (SkCanvas *) context;
                flush(canvas);

                for (auto &button : buttons) {
                    button.render(context);
                }
                if (height_title != 0) {
                    titleview.render(canvas);
                }
                if (framed) {
                    drawRect(canvas, skcol::fg);
                }
            }

            void computeSize() {
                int max = 0;
                height = scalefactor * _STATE->windowHeight * 0.01f;

                for (auto &c : buttons) {
                    c.height = height.load();
                    c.computeWidth();
                    max = c.width.load() > max ? c.width.load() : max;
                }
                width = max * (buttons.size() + 1);
            }

            void setCB(std::function<void(int id, int mode)> _cb) {
                cb = std::move(_cb);
            }

        protected:
            void delRecursiveDraw() {
                for (auto &b : buttons) {
                    b.delRecursiveDraw();
                }
                View::delRecursiveDraw();
                visible_ = false;
            };

            void delRecursiveCB() {
                inputstate.clear();
                View::delRecursiveCB();
            };
        private:
            bool framed{};
            float height_title{};
            const char *title{};
            int orientation{};
            TitleView titleview;
            std::vector<T> buttons{};
            tsl::graphics::InputSystem::InputState inputstate;
            std::function<void(int id, int mode)> cb;
        };
    }
}
#endif