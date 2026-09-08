#pragma once
// RecyclerView_port.h — adapted to your new View system (absolute bounds, parent-driven draw/callback)
//
// Notes:
// - No separate window management; draws into the provided SkCanvas within this->bounds.
// - Keeps your original template shape: RecyclerView<ItemViewT, ValueT> where ItemViewT implements
//     * void computeHeight();  // sets ItemViewT::height
//     * void computeWidth(int index); // sets ItemViewT::width for item index
//     * void render(SkCanvas* c, int index);
//     * int  cb(const InputEvent& e, int index); // item-tap feedback; pass -1 to cancel/clear
//     * std::vector<ValueT> _values;
//   This minimizes changes in your existing ItemView implementation(s).
// - Uses your Style (dp, colors) and absolute coordinates: startx/starty/stopx/stopy/bounds.
// - Input handling returns bool to indicate event consumption (fits newer callbacks).
// - Inertia: simple velocity-based scrolling (desktop wheel + touch drag). You can wire your VelocityTracker
//   if needed — hooks are left in guarded blocks.
#include "view2.h"
#include "Input.h"
#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include "include/core/SkPath.h"

namespace tsl::ui {

    class ScrollViewBase {
    protected:
        explicit ScrollViewBase(tsl::AppState* appState) : _appState2(appState) {};
        enum {
            UNTOUCHED = 0,
            MOVING = 1,
            INSIDE = 2,
            OUTSIDE = 3,
            SCROLLBAR = 4
        };
        tsl::AppState* _appState2{};
#ifdef PLATFORM_MOBILE
        static constexpr int numScrollers = 4;
        std::atomic<int> currentScroller{};
        Scroller scroller[numScrollers] = { {_appState2->mPpi, false},
                                           {_appState2->mPpi, false},
                                           {_appState2->mPpi, false},
                                           {_appState2->mPpi, false} };
        VelocityTracker velocityTracker;
#endif


        int pointerid{ -1 };
        std::atomic<int> mode{ UNTOUCHED };
        float totalmoved{};
        std::atomic<float> offset{};
        float lastXpos{}, lastYpos{};
        tsl::AtomicTimer timeLastAction{};

        int boxoffsetx{}, boxoffsety{}, boxheight{}, boxwidth{}, itemheight{}, maxoffset{};
        int drawstartx{}, drawstarty{}, drawstopx{}, drawstopy{}, drawheight{}, drawwidth{};
        float _titleX{}, _titleY{};

        std::atomic<int> active{};
    };

    // TItemView: provides per-row rendering/measurement; TValue: your data element type held by TItemView::_values
    // This class is intentionally very close to your original so you can drop it in with minimal edits.
    template<typename TItemView, typename TValue>
    class RecyclerView : public View, protected ScrollViewBase {
    public:
        explicit RecyclerView(tsl::AppState* appState,
            const std::vector<TValue>& vals = {},
            const char* title = "",
            View* par = nullptr)
            : View(appState, WRAP, 0, CENTER_ALIGN, 0)
            , ScrollViewBase(_appState)
            , _itemView(appState) {
            _itemView._values = vals;
            perm = true;            // matches your original intent
            _title = title;
            _itemView.parent = this;
            parent = par;
        }

        void setValues(const std::vector<TValue>& vals) { _itemView._values = vals; recompute(); }
        void setTitle(const std::string& title) { _title = title; }

        // --- Layout / sizing ---
        void computeSize() override { recompute(); }

        // --- Rendering ---
        void render(SkCanvas* canvas) override {
            if (!canvas) return;
            SkAutoCanvasRestore acr(canvas, true);
            canvas->clipRect(bounds);

            SkPaint paint;
            canvas->clear(_STATE->style.color.bg_surface);
            paint.setAntiAlias(true);

            // Title
            if (!_title.empty()) {
                SkFont font(_STATE->style.fonts.normal);
                font.setSize(_STATE->style.dp(16));
                paint.setColor(_STATE->style.color.text_primary);
                paint.setEmbolden(true);
                canvas->drawString(_title.c_str(), (float)drawstartx + _STATE->style.dp(12),
                    (float)drawstarty + boxoffsetx + _titleY, font, paint);
            }

            // Scissor to list content rect
            {
                SkAutoCanvasRestore acr2(canvas, true);
                SkRect clip = SkRect::MakeLTRB((float)drawstartx + _STATE->style.dp(1),
                    (float)drawstarty + boxoffsety,
                    (float)drawstopx - _STATE->style.dp(1),
                    (float)drawstarty + boxoffsety + boxheight);
                canvas->clipRect(clip);

                const float off = offset.load();
                float sy = (float)drawstarty + boxoffsety + off;

                // First visible index
                int first = 0;
                if (itemheight > 0) {
                    int rel = (int)((sy - ((float)drawstarty + boxoffsety)) / (float)itemheight);
                    if (rel < 0) rel = 0; // clamp
                    first = rel;
                }

                // Draw all visible rows
                for (int i = first; i < (int)_itemView._values.size(); ++i) {
                    float rowTop = (float)drawstarty + boxoffsety + off + i * (float)itemheight;
                    if (rowTop >= (float)drawstarty + boxoffsety - itemheight &&
                        rowTop <= (float)drawstarty + boxoffsety + boxheight) {
                        SkAutoCanvasRestore acr3(canvas, true);
                        canvas->translate((float)drawstartx, rowTop);
                        _itemView.render(canvas, i);
                    }
                    if (rowTop > (float)drawstarty + boxoffsety + boxheight) break;
                }
            }

            // Scrollbar
            const int contentH = itemheight * (int)_itemView._values.size();
            const int visibleH = boxheight;
            if (contentH > visibleH) {
                float track = (float)visibleH;
                float ratio = track / (float)contentH;
                float thumb = std::max(_STATE->style.dp(24.f), ratio * track);
                float top = (float)drawstarty + boxoffsety + (-offset.load()) * ratio;
                SkRect r = SkRect::MakeLTRB((float)drawstopx - _STATE->style.dp(4), top,
                    (float)drawstopx - _STATE->style.dp(2), top + thumb);
                SkPaint p; p.setAntiAlias(true); p.setColor(_STATE->style.color.scrollbar);
                canvas->drawRoundRect(r, _STATE->style.dp(3), _STATE->style.dp(3), p);
            }

            // Border
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setColor(_STATE->style.color.border);
            paint.setStrokeWidth(1.f);
            canvas->drawRect(SkRect::MakeLTRB((float)drawstartx + 0.5f, (float)drawstarty + 0.5f,
                (float)drawstopx - 0.5f, (float)drawstopy - 0.5f), paint);
        }

        // --- Input ---
        bool callback(InputEvent& event) override {
            const int action = event.action;
            const int _pid = event.pointerId; // unified naming (your new InputEvent uses pointerId)
            float xpos = event.x - (float)drawstartx;
            float ypos = event.y - (float)drawstarty;

            switch (action) {
            case ACTION_DOWN: {
                if (insideList(xpos, ypos)) {
                    int pos = (int)((ypos - boxoffsety - offset.load()) / (float)itemheight);
                    pos = std::clamp(pos, 0, (int)_itemView._values.size() - 1);
                    mode.store(INSIDE);
                    _itemView.cb(event, pos);
                }
                else {
                    mode.store(OUTSIDE);
                    _itemView.cb(event, -1);
                }
                totalmoved = 0.f;
                lastXpos = xpos; lastYpos = ypos; pointerid = _pid;
                inertial_ = false; velocityY_ = 0.f;
                return true;
            }
            case ACTION_UP: {
                if (pointerid == _pid) {
                    if (mode.load() == OUTSIDE) {
                        _itemView.cb(event, -1);
                        mode.store(UNTOUCHED);
                        pointerid = -1;
                        return true;
                    }
                    else if (mode.load() == INSIDE) {
                        int res = _itemView.cb(event, -1);
                        (void)res; // your item cb may signal close; parent may react via onClick
                        pointerid = -1; mode.store(UNTOUCHED);
                        // start inertia if we have velocity
                        if (std::abs(velocityY_) > 0.1f) inertial_ = true;
                        return true;
                    }

                    // Release drag
                    pointerid = -1;
                    mode.store(UNTOUCHED);
                    if (std::abs(velocityY_) > 0.1f) inertial_ = true;
                    return true;
                }
                break;
            }
            case ACTION_MOVE: {
                if (pointerid == _pid) {
                    if (mode.load() == MOVING) {
                        float diffy = lastYpos - ypos;
                        float off = offset.load();
                        off -= diffy;
                        off = std::clamp(off, (float)maxoffset, 0.f);
                        offset.store(off);
                        lastXpos = xpos; lastYpos = ypos;
                        velocityY_ = 0.7f * velocityY_ + 0.3f * (-diffy);
                        requestRedraw_();
                        return true;
                    }
                    else if (mode.load() == INSIDE || mode.load() == OUTSIDE) {
                        totalmoved += std::hypot(xpos - lastXpos, ypos - lastYpos);
                        if (totalmoved > _STATE->style.dp(10)) { // threshold
                            if (mode.load() == INSIDE) {
                                lastXpos = xpos; lastYpos = ypos;
                                _itemView.cb(event, -1); // cancel press state
                                if (maxoffset < 0) mode.store(MOVING);
                            }
                            else {
                                mode.store(UNTOUCHED); pointerid = -1;
                            }
                            return true;
                        }
                    }
                }
                break;
            }
            case ACTION_MOUSE_WHEEL: {
                if (mode.load() == UNTOUCHED && maxoffset < 0) {
                    float step = (float)itemheight * (event.pointerId == 0 ? 1.f : (float)event.pointerId);
                    float off = offset.load();
                    off += -step; // typical wheel: down = positive delta -> scroll content up
                    offset.store(std::clamp(off, (float)maxoffset, 0.f));
                    inertial_ = false; velocityY_ = 0.f;
                    requestRedraw_();
                    return true;
                }
                break;
            }
            case ACTION_KEY_UP: {
                if (event.pointerId == VKEY_ESCAPE) {
                    _itemView.cb(event, -1);
                    requestRedraw_();
                    return true;
                }
                break;
            }
            default: break;
            }
            return false;
        }

        void setActive(int index) { active.store(index); }
        int  getActive() const { return active.load(); }

        std::function<bool(int)> isActiveFunc = nullptr;

    private:
        // --- helpers ---
        void recompute() {
            // Fill view rect if parent not placing us explicitly
            if (parent == nullptr) {
                startx = 0; starty = 0;
                stopx = _STATE->graphics.windowWidth; stopy = _STATE->graphics.windowHeight;
            }
            width = stopx - startx; height = stopy - starty;
            bounds = SkRect::MakeLTRB((float)startx, (float)starty, (float)stopx, (float)stopy);

            const float textsize = _STATE->style.dp(18);

            _itemView.computeHeight();
            itemheight = _itemView.height;

            // Title width
            boxwidth = 0;
            if (!_title.empty()) {
                SkFont font(_STATE->style.fonts.normal);
                font.setSize(textsize * .9f);
                SkRect tb{}; font.measureText(_title.c_str(), _title.size(), SkTextEncoding::kUTF8, &tb);
                boxwidth = (int)std::round(tb.width());
            }

            // Item width
            for (int i = 0; i < (int)_itemView._values.size(); ++i) {
                _itemView.computeWidth(i);
                boxwidth = std::max(boxwidth, _itemView.width.load());
            }

            _itemView.textOffset = (int)std::round(textsize * .5f);
            boxoffsetx = _itemView.textOffset + lw2;
            boxoffsety = boxoffsetx + (!_title.empty() ? itemheight : 0);

            const float maxw = parent == nullptr ? (float)width * .9f : (float)parent->width.load();
            if ((float)boxwidth + 2.f * (float)boxoffsetx > maxw)
                boxwidth = (int)std::floor(maxw - 2.f * (float)boxoffsetx);

            drawwidth = boxwidth + 2 * boxoffsetx;

            drawstartx = (parent == nullptr) ? (startx + (width - drawwidth) / 2) : parent->startx.load();
            drawstartx = std::max(drawstartx, startx.load());
            drawstopx = drawstartx + drawwidth;
            if (drawstopx > stopx) { drawstopx = stopx; drawstartx = drawstopx - drawwidth; }

            _itemView.startx = lw2; // local inside our sub-rect (we translate by drawstartx in render)
            _itemView.width = drawwidth - lw;
            _itemView.stopx = _itemView.startx + _itemView.width;

            const int contentH = itemheight * (int)_itemView._values.size();
            const float maxh = (parent == nullptr)
                ? (float)height * .9f
                : (float)std::abs(parent->stopy - (_STATE->graphics.windowHeight - _STATE->style.dp(18)));

            if ((float)contentH + boxoffsety + boxoffsetx > maxh) {
                maxoffset = -(int)std::floor(((float)contentH + boxoffsety + boxoffsetx) - maxh);
                drawheight = (int)std::floor(maxh);
                boxheight = drawheight - boxoffsety - boxoffsetx;
            }
            else {
                maxoffset = 0;
                drawheight = (int)std::floor((float)contentH + boxoffsety + boxoffsetx);
                boxheight = contentH;
            }

            drawstarty = (parent == nullptr) ? (starty + (height - drawheight) / 2)
                : (parent->stopy - parent->height * .1f);
            drawstopy = drawstarty + drawheight;
            if (drawstopy > stopy) { drawstopy = stopy; drawstarty = drawstopy - drawheight; }

            _itemView.starty = 0;
            _itemView.stopy = _itemView.starty + _itemView.height;
            _itemView.init();

            if (!_title.empty()) {
                SkFont font(_STATE->style.fonts.normal);
                font.setSize(_STATE->style.dp(16));
                centerText(font, (float)boxwidth, (float)itemheight, _title.c_str(), _titleX, _titleY);
            }

            // Clamp current offset into valid range when content changes
            offset.store(std::clamp(offset.load(), (float)maxoffset, 0.f));
        }

        inline bool insideList(float x, float y) const {
            return (x >= boxoffsetx && x < boxoffsetx + boxwidth &&
                y >= boxoffsety && y < boxoffsety + boxheight);
        }

        void requestRedraw_() {
            // Hook if you maintain an explicit invalidation queue.
            // For now this is a placeholder to keep semantics.
        }

        // Simple per-frame inertia integrator — call from your app tick if you have one.
    public:
        void applyInertiaStep() {
            if (!inertial_) return;
            velocityY_ *= friction_;
            if (std::abs(velocityY_) < 0.05f) { inertial_ = false; velocityY_ = 0.f; return; }
            float off = offset.load() + velocityY_;
            off = std::clamp(off, (float)maxoffset, 0.f);
            offset.store(off);
            requestRedraw_();
        }

    private:
        // State for inertia
        float velocityY_ = 0.f;
        bool  inertial_ = false;
        float friction_ = 0.92f; // tune to taste

    private:
        TItemView _itemView;
        std::string _title;
    };

    // --- Optional helper base, ported to new View style ---------------------------------------------
    // You can keep using your existing TextViewBase-derived item views with minimal changes.

    template<typename T>
    class TextViewBase : public View {
    public:
        explicit TextViewBase(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0) {}
        virtual void computeWidth(int index) = 0;

        void computeHeight() override {
            height = (int)_STATE->style.dp(20); // use style dp scaling
        }

        virtual void render(SkCanvas* c, int index) {
            if (index == indexhot) {
                auto elapsed = timer.elapsed();
                SkPaint paint; paint.setAntiAlias(true);
                paint.setColor(SkColorSetA(_STATE->style.color.primary, (uint8_t)(std::min(1.0, elapsed) * 96)));
                c->drawRect(SkRect::MakeXYWH((float)startx, 0, (float)width, (float)height), paint);
            }
        }

        virtual int cb(const tsl::graphics::InputEvent& event, int index) {
            switch (event.action) {
            case ACTION_DOWN: timer.reset(); indexhot = index; break;
            case ACTION_UP:   indexhot = -1; break;
            case ACTION_MOVE: timer.reset(); indexhot = index; break;
            }
            return 1;
        }

        std::vector<T> _values; // your data
        int textOffset{};
    protected:
        tsl::AtomicTimer timer{};
        std::atomic<int> indexhot{ -1 };
    };

} // namespace tsl::graphics
