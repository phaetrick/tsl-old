#pragma once
#ifndef View_H
#define View_H

#include "types.h"
#include "colours.h"
#include "Input.h"
#include <SkFont.h>
#include <SkCanvas.h>
#include <SkRect.h>
#include <cstdint>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <array>
#include <utility>

using ScaleFactor = float;

constexpr ScaleFactor WRAP = ScaleFactor(0);
constexpr ScaleFactor SYM = ScaleFactor(-1);
constexpr ScaleFactor WRAP_CONTENT = ScaleFactor(-2);

enum {
    PERCENTAGE_FROM_MAIN_WINDOW,
    RATIO_FROM_MAIN_WINDOW,
    PERCENTAGE_FROM_PARENT_View,
    RATIO_FROM_PARENT_View,
    ABSOLUTE_VALUE,
    VALUE_FROM_POINTER,
    VIEW_COMPUTESIZE
};

enum {
    CENTER_ALIGN,
    START_ALIGN,
    END_ALIGN
};

constexpr bool NO_OVERLAP = false;
constexpr bool OVERLAP = true;
enum {
    LAST_ONLY,
    EVERYTHING
};

enum {
    HORIZONTAL, VERTICAL
};


namespace tsl::graphics {
    class Layout;

    struct InputEvent;

    extern const char *nums32[];
    extern std::vector<float> numvals32;

    class View {
        friend Layout;
    public:
        View() = default;

        View(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio, int _alignment,
             int _prio = 20,
             bool _perm = false,
             const char *_name = "View") : _appState(appState) {
            scalefactor = _scalefactor;
            aspect_ratio = _aspect_ratio;
            alignment = _alignment;
            prio = _prio;
            perm = _perm;
            name = _name;
        };

        View(tsl::AppState *appState) : View(appState, WRAP, PERCENTAGE_FROM_MAIN_WINDOW,
                                             CENTER_ALIGN) {

        };

        View(View const &other);


        View(tsl::AppState *appState, int w, int h) : _appState(appState), width(w), height(h) {}

        virtual ~View() = default;

        virtual void callback(const InputEvent &event) {};

        bool isInside(const InputEvent &e) const;

        virtual void init() {};

        virtual void computeSize() {};

        virtual void computeWidth();

        virtual void computeHeight();

        virtual float heightEstimate(float maxHeight) { return height.load(); };

        virtual int setViewPort(SkRect &, bool everything);

        virtual void redraw();

        virtual void addDraw();

        virtual void deldraw();

        virtual void addCB();

        virtual void delCB();

        void redrawDirect();

        void computePadding();


        virtual void render(void *);


        void flush(SkCanvas *c, uint32_t col = ::tsl::sk_colours::bg) const;

        void drawRect(SkCanvas *c, uint32_t color = skcol::border,
                      float strokeWidth = 1.0) const;

        void borderWindow(SkCanvas *, uint32_t col = skcol::border, float strokeWidth = 1.0) const;

        virtual void compileShaders() {};

       
        tsl::AppState* _appState{};

        uint32_t bg{::tsl::sk_colours::bg};
        uint32_t fg{::tsl::sk_colours::fg};

        const char *name = nullptr;
        View *parent{};
        
        int id = 0;
        std::atomic<int> startx{}, starty{}, stopx{}, stopy{}, width{}, height{}, x_pos_p{}, y_pos_p{};
        int pos_p = 0;
        int prio = 0;
        bool perm = false;
        bool overlap = 0;
        ScaleFactor scalefactor = 0;
        int alignment = CENTER_ALIGN;
        int innerAlignment = START_ALIGN;
        int aspect_ratio = 0;
        float padding = 0;
        float paddingleft = 0;
        float paddingright = 0;
        float paddingtop = 0;
        float paddingbottom = 0;
        int *size_reference = nullptr;
        float size_reference_scale = 0;
        SkRect viewport{};
        int userdata{};
        std::atomic_bool hovered{};
        std::atomic_bool clicked{};
        std::atomic_bool active{};
        std::atomic_bool hasFocus{};
        
        std::atomic_bool visible_{};

#ifdef PLATFORM_DESKTOP
        static constexpr float lw{ 2.0 };
        static constexpr float lw2{ 1.0 };
#else
        static constexpr float lw{ 3.0 };
        static constexpr float lw2{ 1.5 };
#endif
        
        void setClicked(bool clicked_, bool redraw_ = true) {
            clicked.store(clicked_);
            if (redraw_)
                redraw();
        }

        void setHovered(bool hovered_, bool redraw_ = true) {
            hovered.store(hovered_);
            if (redraw_)
                redraw();
        }

        void setActive(bool active_, bool redraw_ = true) {
            active.store(active_);
            if (redraw_)
                redraw();
        }


        void waitForDrawDetached();

        void addDrawCB() {
            addDraw();
            addCB();
        };

        void delDrawCB() {
            deldraw();
            delCB();
        };

        static constexpr bool isInside(float px, float py, float x, float y, float w, float h) {
            return px >= x && px < x + w && py >= y && py < y + h;
        }

        static float distance(float x1, float x2, float y1, float y2) {
            return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        }

        static void textDisplayCentered(View *v, SkCanvas *canvas, SkPaint &paint, SkFont &font,
                                        const char *text, float scale = 1.f, bool clip = false,
                                        SkTextEncoding textencoding = SkTextEncoding::kUTF8);

        static void
        textDisplayCenteredFixed(View *v, SkCanvas *canvas, SkPaint &paint, SkFont &font,
                                 const char *text, float scale = 1.f, bool clip = false);

        static void centerText(SkFont &font, View *v, const char *s, float &x, float &y);

        static void
        centerText(SkFont &font, float w, float h, const char *s, float &x, float &y);

        static void
        measureText(View *v, SkFont &, const char *text, float scale, float *x, float *y,
                    float *fontsize);


        static float
        measureTextFixed(float w, float h, SkFont &, const char *text, float *x, float *y,
                         float fontsize);

        static float measureWidth(SkFont &, std::vector<std::string> &text);

        static float measureWidth(SkFont &, const char *text);

        static std::string truncateText(const SkFont& font, const std::string& text, float maxWidth);

        // Takes queue_callback then queue_draw. Safe from a touch callback (the
        // input thread already holds queue_callback, so that half is recursive).
        // NOT safe from addRecursiveDraw()/delRecursiveDraw(), which run with
        // queue_draw already held -- that inverts the app-wide callback -> draw
        // order. Use the two halves below there instead.
        void toForeground(int windex = -10000);

        // Halves of toForeground(), each taking no lock of its own. Call only
        // from a context that already holds the matching queue:
        //   ...CBDirect()   -- queue_callback (addRecursiveCB / delRecursiveCB)
        //   ...DrawDirect() -- queue_draw     (addRecursiveDraw / delRecursiveDraw)
        void toForegroundCBDirect();
        void toForegroundDrawDirect(int windex);

        virtual void delRecursiveCB();

        virtual void addRecursiveCB();

        virtual void delRecursiveDraw();

        virtual void addRecursiveDraw();
    };

    class Divider : public View {
    public:
        Divider(tsl::AppState *, int alignment, Layout *layout = nullptr);

    protected:
        void computeWidth() override;

        void computeHeight() override;

    private:
    };

    class Layout : public View {
    public:
        Layout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio, int _alignment,
               bool _overlap = false) : View(appState, _scalefactor, _aspect_ratio, _alignment) {
            overlap = _overlap;
        };

        Layout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio, int _alignment,
               Layout *par,
               bool _overlap = false) : View(appState, _scalefactor, _aspect_ratio, _alignment) {
            overlap = _overlap;
            par->addChild(this);
        };

        ~Layout() override;

        void init() override {
            if (orientation == VERTICAL)
                initVertical();
            else
                initHorizontal();
        }


        void callback(const InputEvent &event) override {
            // Capture per pointer: MOVE/UP go to the child that took the DOWN,
            // matching how both dispatchers route to this Layout itself. The old
            // re-hit-test per event meant a drag leaving the child's bounds
            // never delivered its UP (stuck HOT state) and a neighbour received
            // an UP it had no DOWN for.
            if (event.action == ACTION_MOVE || event.action == ACTION_UP) {
                for (auto &c: captured_) {
                    if (c.first == event.pointer_id && c.second != nullptr) {
                        View *target = c.second;
                        if (event.action == ACTION_UP)
                            c.second = nullptr;
                        target->callback(event);
                        return;
                    }
                }
            }
            float xpos = event.x;
            float ypos = event.y;
            for (auto temp: childs) {
                if (xpos >= temp->startx && xpos < temp->stopx && ypos >= temp->starty
                    && ypos < temp->stopy) {
                    if (event.action == ACTION_DOWN) {
                        auto *slot = &captured_[0];
                        for (auto &c: captured_) {
                            if (c.second == nullptr) { slot = &c; break; }
                        }
                        // The OS reuses ids, so a stale capture under this id
                        // (its UP never arrived) must be reclaimed, not shadowed.
                        for (auto &c: captured_) {
                            if (c.second != nullptr && c.first == event.pointer_id) { slot = &c; break; }
                        }
                        *slot = { event.pointer_id, temp };
                    }
                    temp->callback(event);
                    break;
                }
            }
        }

        void computeWidth() override;

        void computeHeight() override;

        void addChild(View *child) { childs.push_back(child); }

        int setViewPort(SkRect &, bool everything) override;

        int orientation{};

        std::vector<View *> childs;

        // Pointer-id -> child captured at ACTION_DOWN; see callback(). Raw
        // pointers are safe on the same terms as childs itself: children live
        // as long as the layout.
        std::array<std::pair<int, View *>, 8> captured_{};

        void delRecursiveDraw() override;

        void addRecursiveDraw() override;

        void delRecursiveCB() override;

        void addRecursiveCB() override;


    protected:
        void initHorizontal();

        void initVertical();


    private:

    };

    class HorizontalLayout
            : public Layout {
    public:
        HorizontalLayout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio,
                         int _alignment,
                         bool _overlap = false) : Layout(appState, _scalefactor, _aspect_ratio,
                                                         _alignment,
                                                         _overlap) {
            orientation = HORIZONTAL;
            name = "HozLayout";
        };

        HorizontalLayout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio,
                         int _alignment,
                         Layout *par, bool _overlap = false) : Layout(appState, _scalefactor,
                                                                      _aspect_ratio,
                                                                      _alignment,
                                                                      par, _overlap) {
            orientation = HORIZONTAL;
            name = "HozLayout";
        };
    private:
    };

    class VerticalLayout
            : public Layout {
    public:
        VerticalLayout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio,
                       int _alignment,
                       bool _overlap = false) : Layout(appState, _scalefactor, _aspect_ratio,
                                                       _alignment,
                                                       _overlap) {
            orientation = VERTICAL;
            name = "VertLayout";
        }

        VerticalLayout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio,
                       int _alignment, Layout *par,
                       bool _overlap = false) : Layout(appState, _scalefactor, _aspect_ratio,
                                                       _alignment,
                                                       par, _overlap) {
            orientation = VERTICAL;
            name = "VertLayout";
        }

    private:
    };


    class DynamicLayout : public Layout {
    public:
        DynamicLayout(tsl::AppState *appState, ScaleFactor _scalefactor, int _aspect_ratio,
                      int _alignment,
                      bool _overlap = false) : Layout(appState, _scalefactor, _aspect_ratio,
                                                      _alignment,
                                                      _overlap) {

        };

        void init() override {
            if (width > height) {
                initVertical();
            } else {
                initHorizontal();
            }
        }

    private:
    };


}

#endif


