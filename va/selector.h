//
// Created by pr on 13.11.17.
//

#ifndef GRAINSTORM_Selector_H
#define GRAINSTORM_Selector_H

#include <atomic>
#ifdef __ANDROID__
#include <jni.h>
#endif
#include <vector>
#include "types.h"
#include "view.h"
#include "button.h"
#include "textview.h"
#include "grainstorm.h"
#include "Input.h"
#include <SkFont.h>
#include <string>
#include <skia.h>
#include <app.h>

#define ONESECONDNS 1e9f
#define TWOSECONDNS 2e9f

#include "sequencer.h"
#include "preset.h"
#include "VelocityTracker.h"


    namespace tsl::graphics{
class Selector;
class PopUpWindow : public View {
    friend Selector;
public:
    PopUpWindow(tsl::AppState* appState = nullptr) : View(appState) {
        perm = true;
        prio = -1;  // always at _last in queue → checked first in input dispatch
    }

    void render(void *ctx);

    void callback(const InputEvent &e);

    void init();

    void setActive(int index) {
        active.store(index);
    }

    Selector *selector;
protected:
    void delRecursiveDraw() override{
        _STATE->graphics.deleteWindow(windowindex);
        View::delRecursiveDraw();
    };

    void delRecursiveCB() override{
        pointerid = -1;
        hot = -1;
        mode = UNTOUCHED;
        totalmoved = 0;
        // The fling state has to die with the gesture. v0 only ever reached 0 when
        // the momentum ran into one of the two edges, so a fling that came to rest
        // in the middle of the list left v0 != 0 behind - and render() then kept
        // re-deriving the offset from that ancient t0/x0 the next time the popup
        // was opened.
        v0 = 0;
        View::delRecursiveCB();
    };

private:
    enum {
        UNTOUCHED = 0,
        MOVING = 1,
        INSIDE = 2,
        OUTSIDE = 3
    };
    std::atomic<int64_t> timedown{tsl::time::nanosecondsSinceEpoch()};
    std::atomic<int64_t> lastmoved{timedown.load()};
    int pointerid{-1};
    std::atomic<int> mode{UNTOUCHED};
    float px, py, totalmoved{};
    float downx{}, downy{};   // press point: the tap/scroll test measures displacement
                              // from HERE, never accumulated path length

    std::atomic<int> active{};
    std::atomic<int> hot{-1};

    int windowindex{-1};
    std::atomic<float> offset{};
    std::atomic<float> v0{};
    std::atomic<int64_t> t0{};
    std::atomic<float> x0{};
    int64_t lastmovetime{};   // time of the last ACTION_MOVE, to reject a stale fling
#ifdef PLATFORM_MOBILE
    tsl::graphics::VelocityTracker velocityTracker;
#endif
    float startxrect{}, startyrect{}, widthrect{}, heightrect{}, startxtext{}, startytext{}, widthtext{}, heighttext{}, maxoffset{}, itemheight, textoffset;
    int drawstartx{}, drawstarty{}, drawstopx{}, drawstopy{}, drawheight{}, drawwidth{};
};

class Selector : public View {
public:

    static enum {
        Selector_LEFT = 0,
        Selector_RIGHT = 1,
        Selector_MIDDLE = 2
    };

    Selector(tsl::AppState* appState, float scalefactor, int aspect_ratio,
             int alignment, std::vector<std::string> &_names,
             std::vector<float> &_values, uint16_t id,
             long offset = 0,
             int offsetmulti = 1, int (*callback)(Selector *, int) = nullptr,
             const char *title = "Selector");

    void render(void *ctx) override = 0;

    void callback(const InputEvent &e) override = 0;

    void init() override = 0;

    void setActive(const char *name, bool docallback);

    void setActiveByIndex(int index, bool docallback);


    PopUpWindow popupview{};
    const char *titletext = nullptr;
    std::vector<std::string> names;
    std::vector<float> values;
    std::vector<std::string_view> nameViews;

    int (*cb)(Selector *, int){};

    tsl::graphics::InputSystem::InputState inputstate;

    void redraw() override;

    static int element_callback_new(Selector *s, int index) {
        auto* _appState = s->_appState;
        auto tindex = _STATE->active_track.load();
        int offset = 0;
        if (_STATE->parameters[s->id].paramOffset)
            offset = (int)(_STATE->params[tindex][_STATE->parameters[s->id].paramOffset].load());
        offset = offset * _STATE->parameters[s->id].offsetFact;
        tsl::parameters::Event e;
        e.setup(_appState, tindex, s->id + offset);
        e.value = index;
        e.apply(_appState, tsl::parameters::FromUi);
        return 0;
    }
protected:
    void delRecursiveDraw() override {
        popupview.delRecursiveDraw();
        View::delRecursiveDraw();
    };

    void delRecursiveCB() override {
        popupview.delRecursiveCB();
        inputstate.clear();
        View::delRecursiveCB();
    };

    int findIndex(float val) {
        auto it = find(values.begin(), values.end(), val);

        // If element was found
        if (it != values.end()) {

            // calculating the index
            // of K
            int index = it - values.begin();
            return index;
        } else {
            return 0;
        }
    }

};

template<typename T>
class Selector1 : public Selector {
public:
    Selector1(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
              int _alignment, std::vector<std::string> &_names,
              std::vector<float> &_values,
              uint16_t _id, long _offset = 0,
              int _offsetmulti = 1, int (*_callback)(Selector *, int) = nullptr)
            : Selector(appState, _scalefactor, _aspect_ratio, _alignment, _names, _values, _id,
                       _offset,
                       _offsetmulti, _callback),
              left(appState, reinterpret_cast<const char*>(ICON_MD_NAVIGATE_BEFORE)),
              right(appState, reinterpret_cast<const char*>(ICON_MD_NAVIGATE_NEXT)),
              textview(appState, "SelTitle", CENTER_ALIGN) {
        textview.id = _id;
    }

    void render(void *context) override{
        auto tindex = _STATE->active_track.load();
        flush((SkCanvas *) context);
        left.render(context);
        right.render(context);
        int offset = 0;
        if (_STATE->parameters[id].paramOffset)
            offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
        offset = offset * _STATE->parameters[id].offsetFact;
        float index = _STATE->params[tindex][id + offset].load();
        if (values != nullptr)
            index = findIndexFloat(values, index);
        textview.setText(names[(int) index]);
        textview.render(context);
        //renderpopup(view, nullptr);
    }

    void callback(const InputEvent &event) override{
        auto tindex = _STATE->active_track.load();
        int action = event.action;

        float xpos = event.x;
        float ypos = event.y;


        switch (action) {
            case ACTION_DOWN:
                if (!middleclick) {
                    if (xpos < startx + width * .5) {
                        inputstate.addPointer({event.pointer_id, xpos, ypos,
                            tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_LEFT});
                        left.setState(HOT);
                    } else if (xpos >= startx + width * .5) {
                        inputstate.addPointer({event.pointer_id, xpos, ypos,
                            tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_RIGHT});
                        right.setState(HOT);
                    }
                } else {
                    if (xpos < startx + width * .25) {
                        inputstate.addPointer({event.pointer_id, xpos, ypos,
                            tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_LEFT});
                        left.setState(HOT);
                    } else if (xpos >= stopx - width * .25) {
                        inputstate.addPointer({event.pointer_id, xpos, ypos,
                            tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_RIGHT});
                        right.setState(HOT);
                    } else {
                        inputstate.addPointer({event.pointer_id, xpos, ypos,
                            tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_MIDDLE});
                        textview.setState(HOT);
                    }
                }
                break;
            case ACTION_MOVE: {
                auto pt = inputstate.getById(event.pointer_id);
                if (pt && pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
                    if (pt->target == Selector_LEFT)
                        left.setState(NORMAL);
                    else if (pt->target == Selector_RIGHT)
                        right.setState(NORMAL);
                    else
                        textview.setState(NORMAL);
                    inputstate.removePointer(event.pointer_id);
                }
                break;
            }

            case ACTION_UP: {
                if (auto pt = inputstate.getById(event.pointer_id)) {
                    int offset = 0;
                    if (_STATE->parameters[id].paramOffset)
                        offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
                    offset = offset * _STATE->parameters[id].offsetFact;
                    float active = _STATE->params[tindex][id + offset].load();
                    if (values != nullptr)
                        active = findIndexFloat(values, active);

                    if (pt->target == Selector_LEFT) {
                        --active;
                        if (active < 0)
                            active = names.size() - 1;
                        popupview.setActive(active);
                        textview.setText(names[(int) active], true);
                        cb(this, values == nullptr ? active : values[(int) active]);
                        left.setState(NORMAL);
                    } else if (pt->target == Selector_RIGHT) {
                        ++active;
                        if (active >= names.size())
                            active = 0;
                        popupview.setActive(active);
                        textview.setText(names[(int) active], true);
                        cb(this, values == nullptr ? active : values[(int) active]);
                        right.setState(NORMAL);
                    } else {
                        popupview.init();
                        popupview.redraw();
                        popupview.addCB();
                        textview.setState(NORMAL);
                        left.setState(NORMAL);
                        right.setState(NORMAL);
                        inputstate.clear();
                        return;
                    }
                    inputstate.removePointer(event.pointer_id);
                }
                break;
            }
            default:
                break;
        }
    };

    void init() override{
        auto buttonwidth = height.load();
        left.width = buttonwidth;
        left.height = height.load();
        left.startx = startx.load();
        left.stopx = left.startx + left.width;
        left.starty = starty.load();
        left.stopy = left.starty + left.height;
        left.setState(NORMAL, false);

        right.width = buttonwidth;
        right.height = height.load();
        right.startx = startx + width - buttonwidth;
        right.stopx = right.startx + right.width;
        right.starty = starty.load();
        right.stopy = right.starty + right.height;
        right.setState(NORMAL, false);


        textview.width = width - 2 * buttonwidth;
        textview.height = _STATE->textsize2;
        textview.startx = startx + buttonwidth;
        textview.stopx = textview.startx + textview.width;
        textview.starty = starty + (height - _STATE->textsize2) * .5f;
        textview.stopy = textview.starty + textview.height;

        popupview.init();
    }

    void computeSize() override{
        int longest = 0;
        int len = 0;
        for (int i = 0; i < names.size(); i++)
            if (strlen(names[i]) > len) {
                len = strlen(names[i]);
                longest = i;
            }
        float h = height = scalefactor * _STATE->windowHeight * .01;

        if (((Layout *) parent)->orientation == HORIZONTAL) {

            SkRect bounds;
            SkFont font(_STATE->font_normal);
            font.setSize(_STATE->textsize2);
            font.measureText(names[longest], strlen(names[longest]), SkTextEncoding::kUTF8,
                             &bounds);
            float w = bounds.width();
            float _padding = (padding != 0 ? 100.f - 2 * padding : 100.f - paddingleft -
                                                                   paddingright) * .01f;
            w += 4.0 * height * _padding;
            width = w;
            startx = parent->startx + (parent->width - w) / 2;
            starty = startx + w;
        } else if (((Layout *) parent)->orientation == VERTICAL) {
            starty = parent->starty + (parent->height - h) / 2;
            stopy = starty + h;
            SkRect bounds;
            SkFont font(_STATE->font_normal);
            font.setSize(_STATE->textsize2);
            font.measureText(names[longest], strlen(names[longest]), SkTextEncoding::kUTF8,
                             &bounds);
            float w = bounds.width();
            float _padding =
                    (padding != 0 ? 100.f - 2 * padding : 100.f - paddingleft - paddingright) *
                    .01f;
            w += 3.0f * height;// * _padding;
            width = w;
        }
        // view->height = view->scalefactor * view->_STATE->windowHeight / 100.f;

    }

    bool middleclick = false;
protected:
    void delRecursiveDraw() override{
        auto &q = _STATE->queue_draw;
        std::lock_guard<std::recursive_mutex> lck(q);
        q.del(&left);
        left.state.store(NORMAL);
        q.del(&right);
        right.state.store(NORMAL);
        q.del(&textview);
        textview.setState(NORMAL, false);
        Selector::delRecursiveDraw();
    };
private:
    IconButton left, right;
    T textview;
};

//View *selector2_create(void *p, const char *name, const char *title, int prio, double scalefactor, uint8_t aspect_ratio,
//                      int alignment, const char **text1, const char **text2, const char **text3, const char **text4, int type, int (*callback)(Selector2 *s, const char *fieldname));

class Selector2 : public Selector {
public:
    Selector2(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
              int _alignment, std::vector<std::string> &_names,
              std::vector<float> &_values,
              uint16_t _id, long _offset = 0,
              int _offsetmulti = 1, const char *_title = nullptr,
              int (*_callback)(Selector *, int) = nullptr);

    void render(void *ctx) override;

    void callback(const InputEvent &e) override;

    void init() override;

    void computeHeight() override;

    bool disabled{};
protected:
    void delRecursiveDraw() override {
        hot.store(false);
        Selector::delRecursiveDraw();
    }

    TextToggle value;
    TextToggle title;
    std::atomic_bool hot{false};
};

class PresetSelector : public Selector2 {
public:
    PresetSelector(tsl::AppState* appState, float _scalefactor, int _aspect_ratio,
                   int _alignment, std::vector<std::string> &_names,
                   std::vector<float> &_values,
                   uint16_t _id, long _offset = 0,
                   int _offsetmulti = 1, const char *_title = nullptr,
                   int (*_callback)(Selector *, int) = nullptr) : Selector2(appState, _scalefactor,
                                                                            _aspect_ratio,
                                                                            _alignment, _names,
                                                                            _values, _id, 0, 1,
                                                                            "LOAD PRESET", &cb) {

    }

protected:
    void addRecursiveDraw() override {
        names.clear();
        values.clear();
        _STATE->params[0][id] = -1;

        // Budget against what the POPUP can show, measured with the real font.
        // PopUpWindow::init sizes itself to the longest name up to 90% of the window,
        // but this used to trim by character count against `width` — the width of the
        // little selector button, about 13 average characters. Names were chopped to
        // stumps and the popup then sized itself to the stumps, which is the empty
        // space visible to their right. An average-character estimate is wrong twice
        // over anyway: proportional fonts make it a guess, and `width` here is still
        // the value from the previous layout pass.
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * 2.f);          // itemheight, as the popup measures
        const float avail = _STATE->windowWidth * 0.9f - _STATE->textsize2 * 2.f - 4.f * lw;
        auto textWidth = [&](const std::string& t) {
            SkRect b;
            font.measureText(t.c_str(), t.size(), SkTextEncoding::kUTF8, &b);
            return b.width();
        };

        int i = 0;
        for (auto &s : _DATA->presets) {
            std::string tmp(s.name);
            if (avail > 0.f && textWidth(tmp) > avail) {
                while (tmp.size() > 1) {
                    tmp.pop_back();
                    if (textWidth(tmp + "..") <= avail) break;
                }
                tmp.append("..");
            }
            names.push_back(tmp);
            values.push_back(i++);
            if (s.date == _DATA->presetDate) {
                _STATE->params[0][id] = names.size() - 1;
            }
        }
        // The Param's names/values are spans over the members this just rebuilt —
        // republish them, or they dangle into the cleared vectors after the first
        // preset-list refresh.
        nameViews.clear();
        nameViews.reserve(names.size());
        for (const auto& n : names) nameViews.emplace_back(n);
        _STATE->parameters[id].names = nameViews;
        _STATE->parameters[id].values = values;
        popupview.init();
        View::addRecursiveDraw();
    }

private:
    static int cb(Selector *s, int preset) {
        auto* _appState = s->_appState;
        if(!_DATA->toAudioThreadQueue.push([_appState, preset](){
            sequencer::loadPreset(_appState, preset);
        })) {
            showToast(_STATE, "Too many tasks");
        }
        return 0;
    }
};
    }

#endif //GRAINSTORM_Selector_H


