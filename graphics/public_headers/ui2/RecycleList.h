// RecycleList.h  (fits your View system)
#pragma once
#include "ui/view2.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include <functional>
#include <deque>
#include <optional>

namespace tsl::ui {
    // ----- ItemRenderer -----------------------------------------------------------
    // Minimal renderer for one row. You can make this a class or std::function bundle.
    // For variable heights, implement measureHeight() based on width and data.
    template<typename ItemT>
    struct ItemRenderer {
        // Required
        std::function<int(const ItemT&, int /*rowIndex*/, int /*availableWidth*/)> measureHeight;
        std::function<void(SkCanvas*, const ItemT&, const SkRect& /*rowRect*/, bool /*pressed*/)> draw;

        // Optional
        std::function<void(const ItemT&, int /*rowIndex*/)> onClick; // invoked on tap up inside
    };

    // ----- RecycleList ------------------------------------------------------------
    template<typename ItemT>
    class RecycleList : public View {
    public:
        struct Config {
            bool variableRowHeights = false;
            int  fixedRowHeightDp = 44;      // used if variableRowHeights=false
            int  dividerPx = 1;       // 0 = no divider
            bool showScrollBar = true;
            float friction = 0.90f;   // inertia
        };

    private:
        struct RecycledCell {
            int rowIndex = -1;            // logical row bound to this cell
            int y = 0;                    // top (screen coords)
            int h = 0;                    // height (px)
            bool pressed = false;         // touch feedback
            int pointerId = -1;           // active pointer

            // cache any paints/fonts/paths per cell if needed
        };

    public:
        RecycleList(tsl::AppState* s, const Config& cfg, ItemRenderer<ItemT> r)
            : View(s), config(cfg), renderer(std::move(r)) {
        }

        // Data API
        void setItems(std::vector<ItemT> items, bool keepScrollIfPossible = true) {
            items_ = std::move(items);
            if (!config.variableRowHeights) {
                fixedRowH_ = _STATE->style.dp((float)config.fixedRowHeightDp);
            }
            rebuildHeights();
            if (!keepScrollIfPossible) scrollY_ = 0;
            clampScroll();
            requestLayout();
        }

        void setRenderer(ItemRenderer<ItemT> r) {
            renderer = std::move(r);
            rebuildHeights();
            requestLayout();
        }

        // View overrides
        void init() override {
            // adopt full parent bounds by default; caller can override
            startx = 0; starty = 0;
            stopx = width = _STATE->graphics.windowWidth;
            stopy = height = _STATE->graphics.windowHeight;
            bounds = SkRect::MakeLTRB((float)startx, (float)starty, (float)stopx, (float)stopy);
        }

        void render(SkCanvas* c) override {
            if (items_.empty()) return;

            // clip to list rect
            SkAutoCanvasRestore acr(c, true);
            c->clipRect(bounds);

            const int viewH = height;
            const int viewTop = starty;
            const int viewBottom = starty + viewH;

            // apply inertia
            applyInertia();

            // figure visible range
            ensureLayoutComputed();
            int first = rowAtY(scrollY_);
            if (first < 0) first = 0;

            int yCursor = rowTop(first) - scrollY_ + starty;
            int i = first;

            // draw rows until we leave viewport
            while (i < (int)items_.size() && yCursor < viewBottom) {
                const int rh = rowHeight(i);
                SkRect r = SkRect::MakeLTRB((float)startx,
                    (float)yCursor,
                    (float)stopx,
                    (float)(yCursor + rh));
                RecycledCell* cell = bindCell(i, (int)r.top, rh);
                renderer.draw(c, items_[i], r, cell->pressed);

                // divider
                if (config.dividerPx > 0) {
                    SkPaint p;
                    p.setColor(_STATE->style.color.divider);
                    p.setStrokeWidth((float)config.dividerPx);
                    c->drawLine(r.left(), r.bottom() - 0.5f, r.right(), r.bottom() - 0.5f, p);
                }

                yCursor += rh;
                i++;
            }

            // Scrollbar
            if (config.showScrollBar) drawScrollbar(c);
        }

        bool callback(InputEvent& e) override {
            if (items_.empty()) return false;

            switch (e.action) {
            case InputEvent::DOWN: return onPointerDown(e);
            case InputEvent::MOVE: return onPointerMove(e);
            case InputEvent::UP:   return onPointerUp(e);
            case InputEvent::SCROLL: return onWheel(e);
            default: break;
            }
            return false;
        }

        // Programmatic scroll
        void scrollToY(int y) {
            scrollY_ = y;
            clampScroll();
            requestLayout();
        }

    private:
        // ----- Layout / measurement -----
        void rebuildHeights() {
            prefixHeights_.clear();
            prefixHeights_.reserve(items_.size() + 1);
            prefixHeights_.push_back(0);
            int acc = 0;
            const int availW = width;
            const int fixedH = fixedRowH_;

            for (size_t i = 0; i < items_.size(); ++i) {
                int h = config.variableRowHeights
                    ? renderer.measureHeight(items_[i], (int)i, availW)
                    : fixedH;
                if (h < 1) h = 1;
                acc += h;
                prefixHeights_.push_back(acc);
            }
            totalContentH_ = acc + (config.dividerPx > 0 ? (int)items_.size() * config.dividerPx : 0);
        }

        void ensureLayoutComputed() {
            if (prefixHeights_.empty() && !items_.empty()) {
                rebuildHeights();
            }
        }

        int rowHeight(int idx) const {
            int h = prefixHeights_[idx + 1] - prefixHeights_[idx];
            // divider is drawn over the row bottom; don’t add to height
            return h;
        }

        int rowTop(int idx) const {
            return prefixHeights_[idx];
        }

        int rowAtY(int contentY) const {
            // binary search in prefix sums
            int lo = 0, hi = (int)items_.size();
            while (lo < hi) {
                int mid = (lo + hi) >> 1;
                if (prefixHeights_[mid + 1] <= contentY) lo = mid + 1;
                else hi = mid;
            }
            return (lo < (int)items_.size()) ? lo : (int)items_.size() - 1;
        }

        void clampScroll() {
            const int maxScroll = std::max(0, totalContentH_ - height);
            if (scrollY_ < 0) scrollY_ = 0;
            if (scrollY_ > maxScroll) scrollY_ = maxScroll;
        }

        void requestLayout() {
            // Your render system—parent decides enqueue; here we can trigger a redraw flag
            // If you have a queue or invalidation, call it here.
        }

        // ----- Recycler / binding -----
        RecycledCell* bindCell(int rowIndex, int y, int h) {
            // try to find existing
            for (auto* c : active_) {
                if (c->rowIndex == rowIndex) {
                    c->y = y; c->h = h; return c;
                }
            }
            // reuse from pool
            RecycledCell* cell = nullptr;
            if (!pool_.empty()) {
                cell = pool_.back(); pool_.pop_back();
            }
            else {
                cell = new RecycledCell();
                owned_.emplace_back(cell);
            }
            cell->rowIndex = rowIndex;
            cell->y = y;
            cell->h = h;
            cell->pressed = false;
            active_.push_back(cell);
            // NOTE: we don’t unbind here—compaction happens per frame before drawing
            return cell;
        }

        void recycleAllOutsideViewport() {
            // compact active_: keep those intersecting viewport
            const int viewTop = starty;
            const int viewBottom = starty + height;
            std::vector<RecycledCell*> keep;
            keep.reserve(active_.size());
            for (auto* c : active_) {
                const int top = c->y;
                const int bottom = c->y + c->h;
                if (bottom < viewTop || top > viewBottom) {
                    c->rowIndex = -1;
                    c->pressed = false;
                    c->pointerId = -1;
                    pool_.push_back(c);
                }
                else {
                    keep.push_back(c);
                }
            }
            active_.swap(keep);
        }

        // ----- Scrollbar -----
        void drawScrollbar(SkCanvas* c) {
            if (totalContentH_ <= height) return;
            float track = (float)height;
            float ratio = track / (float)totalContentH_;
            float thumb = std::max(24.0f, ratio * track);
            float top = (float)scrollY_ * ratio + starty;
            SkRect r = SkRect::MakeLTRB((float)stopx - _STATE->style.dp(4.0f),
                top,
                (float)stopx - _STATE->style.dp(2.0f),
                top + thumb);
            SkPaint p;
            p.setColor(_STATE->style.color.scrollbar);
            p.setAntiAlias(true);
            c->drawRoundRect(r, _STATE->style.dp(3.0f), _STATE->style.dp(3.0f), p);
        }

        // ----- Input handling -----
        bool onPointerDown(InputEvent& e) {
            if (!bounds.contains(SkPoint::Make(e.x, e.y))) return false;
            dragging_ = false;
            lastY_ = e.y;
            velocityY_ = 0.0f;

            int contentY = scrollY_ + (e.y - starty);
            int idx = rowAtY(contentY);
            if (idx >= 0 && idx < (int)items_.size()) {
                // mark pressed cell
                for (auto* c : active_) c->pressed = false;
                pressedIndex_ = idx;
                // ensure we have a cell for feedback now
                const int top = rowTop(idx) - scrollY_ + starty;
                bindCell(idx, top, rowHeight(idx))->pressed = true;
            }
            return true;
        }

        bool onPointerMove(InputEvent& e) {
            if (!bounds.contains(SkPoint::Make(e.x, e.y))) return false;

            int dy = e.y - lastY_;
            if (std::abs(dy) > _STATE->style.dp(3.0f)) {
                dragging_ = true;
                if (pressedIndex_.has_value()) {
                    // cancel press if we start scrolling
                    for (auto* c : active_) c->pressed = false;
                    pressedIndex_.reset();
                }
            }
            scrollY_ -= dy; // invert to make drag natural
            clampScroll();
            lastY_ = e.y;
            velocityY_ = 0.7f * velocityY_ + 0.3f * (-dy); // simple low-pass
            recycleAllOutsideViewport();
            requestLayout();
            return true;
        }

        bool onPointerUp(InputEvent& e) {
            if (!bounds.contains(SkPoint::Make(e.x, e.y))) {
                pressedIndex_.reset();
                return false;
            }
            // click?
            if (pressedIndex_.has_value() && !dragging_) {
                int idx = *pressedIndex_;
                if (renderer.onClick) renderer.onClick(items_[idx], idx);
            }
            // clear press & start inertia
            for (auto* c : active_) c->pressed = false;
            pressedIndex_.reset();
            inertial_ = true;
            recycleAllOutsideViewport();
            requestLayout();
            return true;
        }

        bool onWheel(InputEvent& e) {
            // assume e.scrollDeltaY positive = scroll down
            scrollY_ += (int)(-e.scrollDeltaY * _STATE->style.dp(20.0f));
            clampScroll();
            inertial_ = false;
            velocityY_ = 0.0f;
            recycleAllOutsideViewport();
            requestLayout();
            return true;
        }

        void applyInertia() {
            if (!inertial_) return;
            velocityY_ *= config.friction;
            if (std::abs(velocityY_) < 0.1f) {
                inertial_ = false;
                velocityY_ = 0.0f;
                return;
            }
            scrollY_ += (int)velocityY_;
            clampScroll();
            recycleAllOutsideViewport();
        }

    private:
        Config config;
        ItemRenderer<ItemT> renderer;

        std::vector<ItemT> items_;
        std::vector<int> prefixHeights_; // prefix sums of row heights
        int totalContentH_ = 0;

        // recycling
        std::vector<RecycledCell*> active_;
        std::vector<RecycledCell*> pool_;
        std::vector<std::unique_ptr<RecycledCell>> owned_;

        // scroll state
        int scrollY_ = 0;
        int fixedRowH_ = 0;
        int lastY_ = 0;
        float velocityY_ = 0.0f;
        bool inertial_ = false;
        bool dragging_ = false;
        std::optional<int> pressedIndex_;
    };
}

