#include "guisetup2.h"
// TabBarBrowser.h
#pragma once
#include "logger.h"     // your View / flags / Align / InputEvent
#include "ui/view2.h"     // your View / flags / Align / InputEvent
#include "ui/style.h"     // your View / flags / Align / InputEvent
#include "Input.h"
#include <SkPath.h>
#include <SkRRect.h>
#include <array>
#include <string>
#include <functional>

namespace tsl::ui {

    class TabBarBrowser : public View {
    public:
        explicit TabBarBrowser(tsl::AppState* app,
            std::function<void(int)> onChange = {})
            : View(app, FILL_WIDTH | FIXED_HEIGHT, Align::Start, Align::Start, 0, /*prio*/5, /*perm*/true),
            onChange_(std::move(onChange)) {
            height = dp(44);
            labels_ = { "1", "2", "3", "4" };
        }

        // Pass the rounded rect used by your track buttons container (same coords as this view)
        void setContainerRRect(const SkRRect& rr) {
            container_ = rr;
            haveContainer_ = true;
            redrawSafe();
        }

        void setLabels(const std::array<std::string, 4>& labels) { labels_ = labels; }

        int  activeIndex() const { return active_; }
        void setActive(int idx, bool notify = false) {
            idx = std::clamp(idx, 0, 3);
            if (active_ == idx) return;
            active_ = idx;
            redrawSafe();
            if (notify && onChange_) onChange_(active_);
        }

        // ---------------- Input ----------------
        int callback(tsl::graphics::InputEvent& e) override {
            switch (e.action) {
            case tsl::graphics::ACTION_DOWN: {
                pressed_ = hitTest(e.x, e.y);
                redrawSafe();
                return 1;
            }
            case tsl::graphics::ACTION_MOVE: {
                if (pressed_ >= 0 && pressed_ != hitTest(e.x, e.y)) {
                    pressed_ = -1;
                    redrawSafe();
                }
                return 1;
            }
            case tsl::graphics::ACTION_UP: {
                const int hit = hitTest(e.x, e.y);
                if (pressed_ == hit && hit >= 0) setActive(hit, /*notify*/true);
                pressed_ = -1;
                redrawSafe();
                return 1;
            }
            default: break;
            }
            return 0;
        }

        // ---------------- Render ----------------
        void render(SkCanvas* c) override {
            const Style* st = style();
            SkAutoCanvasRestore acr(c, true);

            // Clip to the same rounded container as the track buttons (if provided).
            if (haveContainer_) {
                c->clipRRect(container_, SkClipOp::kIntersect, /*antiAlias*/true);
            }
            else {
                c->clipRect(bounds, SkClipOp::kIntersect, /*antiAlias*/true);
            }

            // Background of the strip (inside the rounded container)
            {
                SkPaint bg = st->fillPaint(st->colorSurface);
                bg.setAntiAlias(true);
                if (haveContainer_) c->drawRRect(container_, bg);
                else                c->drawRect(bounds, bg);
            }

            // Metrics
            const float W = (float)width;
            const float H = (float)height;
            const float slotW = W / 4.0f;
            const float os = (float)dp(6);    // active overhang up/down
            const float gap = (float)dp(2);    // baseline gap under active
            const float r0 = (float)dp(10);
            const float r = std::min({ r0, slotW * 0.35f, H * 0.9f }); // keep corners sane

            auto slotRect = [&](int i) {
                float x0 = (float)startX + i * slotW;
                float x1 = x0 + slotW;
                return SkRect::MakeLTRB(x0, (float)startY, x1, (float)stopY);
                };

            auto makeTabPath = [&](const SkRect& base, bool active) {
                SkRect rr = base;
                if (active) {
                    rr.fTop -= os; // extend up (clipped by container)
                    rr.fBottom += os; // tiny extend down to keep stroke continuous
                }
                else {
                    rr.fTop -= (float)dp(2);
                }

                SkPath p;
                const float x0 = rr.left(), x1 = rr.right();
                const float y0 = rr.top(), y1 = rr.bottom();

                p.moveTo(x0, y1);
                p.lineTo(x0, y0 + r);
                p.quadTo(x0, y0, x0 + r, y0);
                p.lineTo(x1 - r, y0);
                p.quadTo(x1, y0, x1, y0 + r);
                p.lineTo(x1, y1);
                p.close();
                return p;
                };

            // Colors
            const SkColor divider = SkColorSetA(st->colorFg, 60);
            const SkColor outline = SkColorSetA(st->colorFg, 100);
            const SkColor inactiveFill = SkColorSetA(st->colorFg, 24);
            const SkColor pressedFill = SkColorSetA(st->colorFg, 40);
            const SkColor activeFill = st->colorBgHot;

            SkPaint stroke = st->strokePaint();
            stroke.setStrokeWidth(1.0f);
            stroke.setAntiAlias(true);

            // Inactive tabs first
            for (int i = 0; i < 4; ++i) {
                if (i == active_) continue;
                SkRect rc = slotRect(i);
                rc.inset((float)dp(1), 0.0f); // prevent double seams

                SkPath p = makeTabPath(rc, /*active*/false);

                SkPaint fill = st->fillPaint((pressed_ == i) ? pressedFill : inactiveFill);
                fill.setAntiAlias(true);

                c->drawPath(p, fill);
                stroke.setColor(divider);
                c->drawPath(p, stroke);
            }

            // Active tab on top
            {
                SkRect rc = slotRect(active_);
                rc.inset((float)dp(1), 0.0f);
                SkPath p = makeTabPath(rc, /*active*/true);

                SkPaint fill = st->fillPaint(activeFill);
                fill.setAntiAlias(true);

                c->drawPath(p, fill);
                stroke.setColor(outline);
                c->drawPath(p, stroke);
            }

            // Pixel-aligned baseline with a small gap under the active tab
            {
                const float y = std::floor((float)stopY) - 0.5f;
                SkPaint base = st->strokePaint();
                base.setColor(divider);
                base.setStrokeWidth(1.0f);
                base.setAntiAlias(true);

                const float ax0 = (float)startX + active_ * slotW + gap;
                const float ax1 = (float)startX + (active_ + 1) * slotW - gap;

                if (ax0 > startX + 1)  c->drawLine((float)startX + 0.0f, y, ax0, y, base);
                if (ax1 < stopX - 1)   c->drawLine(ax1, y, (float)stopX + 0.0f, y, base);
            }

            // Labels
            {
                SkFont f = st->fontTextNormal();
                SkPaint tp = st->fillPaint(st->colorFg);
                tp.setAntiAlias(true);

                for (int i = 0; i < 4; ++i) {
                    SkRect rc = slotRect(i);
                    SkRect tb{};
                    const auto& txt = labels_[i];

                    SkPaint thisText = tp;
                    if (i == active_) {
                        thisText.setColor(st->colorFg);
                        f.setEmbolden(true);
                    }
                    else {
                        thisText.setColor(SkColorSetA(st->colorFg, 200));
                        f.setEmbolden(false);
                    }

                    f.measureText(txt.c_str(), txt.size(), SkTextEncoding::kUTF8, &tb);
                    const float cx = rc.centerX();
                    const float cy = rc.centerY() + (float)dp(5);
                    c->drawString(txt.c_str(), cx - tb.width() * 0.5f, cy, f, thisText);
                }
            }
        }

    private:
        // Map a point to tab index (−1 if outside)
        int hitTest(float x, float y) const {
            // Respect the rounded clip if provided
            if (haveContainer_) {
                if (!container_.contains(SkRect::MakeXYWH(x, y,1,1))) return -1;
            }
            else if (!bounds.contains(x, y)) {
                return -1;
            }
            const float slotW = (float)width / 4.0f;
            int idx = int((x - startX) / slotW);
            return (idx >= 0 && idx < 4) ? idx : -1;
        }

    private:
        std::function<void(int)> onChange_;
        std::array<std::string, 4> labels_{};
        int active_ = 0;
        int pressed_ = -1;

        // Rounded container to clip against (pass your track-buttons rounded rect here)
        SkRRect container_;
        bool    haveContainer_ = false;
    };

} // namespace tsl::ui

#include "app.h"                         // AppState with peaks/params
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkColor.h"
#include "include/core/SkRect.h"
#include "include/effects/SkGradientShader.h"
#include <algorithm>
#include <cmath>

namespace tsl::ui {

    enum class Orientation { Horizontal, Vertical };

    // ---- Meter math (always use our own impl; no macros) -------------------------
    namespace MeterMath {
        inline float ampToDb(float x) {
            x = std::max(1e-12f, std::fabs(x));
            return 20.0f * std::log10f(x);
        }
        inline float dbToNorm(float db) {
            // Map dBFS (−∞..+∞) → normalized [0..∞). Clamp at use-site.
            return std::pow(10.0f, db / 20.0f);
        }
    } // namespace MeterMath
    // -----------------------------------------------------------------------------

    class MeterView : public View {
    public:
        explicit MeterView(tsl::AppState* app,
            Orientation o = Orientation::Horizontal)
            : View(app, FILL_WIDTH | FIXED_HEIGHT, Align::Start, Align::Start, 0, /*prio*/10, /*perm*/true),
            orient_(o)
        {
            if (orient_ == Orientation::Horizontal) {
                height = dp(28);                 // two rows (L/R)
            }
            else {
                // vertical “bar” meter: narrow fixed width; parent controls height
                flags = FIXED_WIDTH | FILL_HEIGHT;
                width = dp(18) * 2 + dp(6);     // two columns (L/R) + gap
                height = 0;                     // layout sets height
            }
        }

        void setOrientation(Orientation o) {
            orient_ = o;
            if (orient_ == Orientation::Horizontal) {
                flags = WRAP_WIDTH | FIXED_HEIGHT;
                height = dp(28);
            }
            else {
                flags = FIXED_WIDTH | FILL_HEIGHT;
                width = dp(18) * 2 + dp(6);
            }
        }

        void resetPeaks() {
            leftSaved_ = rightSaved_ = -130.f;
            peakLeft_ = peakRight_ = -135.f;
            dirL_ = dirR_ = -1;
            clipHoldL_ = clipHoldR_ = 0;
        }

        void render(SkCanvas* c) override {
            if (!c) return;
            const Style* st = style();
            SkAutoCanvasRestore acr(c, true);
            c->clipRect(bounds);

            // ---- Read & consume newest peaks from AppState (like original) ----
            const int tindex = _appState->active_track.load();
            float left = 1.0;// _appState->peak[0];
            float right = _appState->peak[1];
            _appState->peak[0] = _appState->peak[1] = 1e-5f;

            left = std::clamp(left, -2.0f, 2.0f);
            right = std::clamp(right, -2.0f, 2.0f);
            const float leftDb = MeterMath::ampToDb(left);
            const float rightDb = MeterMath::ampToDb(right);

            // Clip LEDs hold (~600 ms)
            const int holdFrames = std::max(1, (int)std::round(0.6f * std::max(1.0f, _appState->actual_framerate.load())));
            if (leftDb > 0.f) clipHoldL_ = holdFrames; else if (clipHoldL_ > 0) --clipHoldL_;
            if (rightDb > 0.f) clipHoldR_ = holdFrames; else if (clipHoldR_ > 0) --clipHoldR_;

            // Peak-hold style smoothing/decay
            const float decPerFrame = 20.0f / std::max(1.0f, _appState->actual_framerate.load());
            if (leftDb > leftSaved_) { leftSaved_ = leftDb; if (dirL_ == -1) dirL_ = 1; }
            else { if (dirL_ == 1) { peakLeft_ = leftSaved_; dirL_ = -1; } leftSaved_ -= decPerFrame; }
            if (leftSaved_ < -130.f) leftSaved_ = -130.f;

            if (rightDb > rightSaved_) { rightSaved_ = rightDb; if (dirR_ == -1) dirR_ = 1; }
            else { if (dirR_ == 1) { peakRight_ = rightSaved_; dirR_ = -1; } rightSaved_ -= decPerFrame; }
            if (rightSaved_ < -130.f) rightSaved_ = -130.f;

            // Background
            c->drawRect(bounds, st->fillPaint(st->colorSurface));

            if (orient_ == Orientation::Horizontal) {
                drawHorizontal(c, st, tindex);
            }
            else {
                drawVertical(c, st, tindex);
            }
        }

    private:
        // ---------------- HORIZONTAL (two rows: top=L, bottom=R) ----------------
        void drawHorizontal(SkCanvas* c, const Style* st, int tindex) {
            const float W = (float)width;
            const float H = (float)height;
            const float rowH = H * 0.5f;

            // Reserve a small right gutter (as your original did)
            const float rightGutter = W * 0.05908f;
            const float usableW = W - rightGutter;

            // Gradient (left→right), warms near 0 dB
            SkPoint pts[2] = { { (float)startX, (float)startY }, { (float)stopX, (float)startY } };
            const SkColor accent = st->colorFgHot;
            const SkColor warm = SkColorSetRGB(255, 120, 80);
            SkColor  colors[4] = {
                SkColorSetA(accent,  64),
                SkColorSetA(accent, 160),
                SkColorSetA(accent, 220),
                SkColorSetA(warm,   240)
            };
            SkScalar stops[4] = { 0.0f, 0.60f, 0.85f, 1.0f };
            SkPaint grad; grad.setAntiAlias(true);
            grad.setShader(SkGradientShader::MakeLinear(pts, colors, stops, 4, SkTileMode::kClamp));

            auto pxLen = [&](float db) {
                float px = usableW * MeterMath::dbToNorm(db);
                if (px < 0.f) px = 0.f;
                if (px > W)   px = W;
                return px;
                };

            const float pxL = pxLen(leftSaved_);
            const float pxR = pxLen(rightSaved_);

            // Bars
            c->drawRect(SkRect::MakeLTRB((float)startX, (float)startY,
                (float)startX + pxL, (float)startY + rowH), grad);
            c->drawRect(SkRect::MakeLTRB((float)startX, (float)startY + rowH,
                (float)startX + pxR, (float)stopY), grad);

            // Ticks at −12/−6/−3/0
            const std::array<float, 4> ticksDb{ -12.f, -6.f, -3.f, 0.f };
            SkPaint tickP = st->strokePaint();
            tickP.setColor(SkColorSetA(st->colorFg, 60));
            tickP.setStrokeWidth(1.0f);
            for (float db : ticksDb) {
                const float x = (float)startX + usableW * MeterMath::dbToNorm(db);
                c->drawLine(x, (float)startY, x, (float)stopY, tickP);
            }

            // Midline
            SkPaint midP = st->strokePaint();
            midP.setColor(SkColorSetA(st->colorFg, 70));
            midP.setStrokeWidth(1.0f);
            c->drawLine((float)startX, (float)startY + rowH, (float)stopX, (float)startY + rowH, midP);

            // Peak lines (accent)
            SkPaint peakP = st->strokePaint(); peakP.setColor(st->colorFgHot); peakP.setStrokeWidth(1.0f);
            const float xLP = (float)startX + usableW * MeterMath::dbToNorm(peakLeft_);
            const float xRP = (float)startX + usableW * MeterMath::dbToNorm(peakRight_);
            if (peakLeft_ > -135.f) c->drawLine(xLP, (float)startY, xLP, (float)startY + rowH, peakP);
            if (peakRight_ > -135.f) c->drawLine(xRP, (float)startY + rowH, xRP, (float)stopY, peakP);

            // Clip LEDs (tiny dots at right)
            const float ledR = (float)dp(3);
            const float ledX = (float)stopX - (float)dp(8);
            const float ledYTop = (float)startY + (float)dp(6);
            const float ledYBot = (float)stopY - (float)dp(6);
            drawLED(c, st, ledX, ledYTop, clipHoldL_);
            drawLED(c, st, ledX, ledYBot, clipHoldR_);

            // Dynamics overlays
            drawDynamicsHorizontal(c, st, tindex, usableW);
        }

        // ---------------- VERTICAL (two columns: left & right) -------------------
        void drawVertical(SkCanvas* c, const Style* st, int tindex) {
            const float W = (float)width;
            const float H = (float)height;

            // two columns with a gap
            const float gap = (float)dp(6);
            const float colW = (W - gap) * 0.5f;
            const float xL0 = (float)startX;
            const float xL1 = xL0 + colW;
            const float xR0 = xL1 + gap;
            const float xR1 = xR0 + colW;

            // Reserve a small top gutter (mirror of horizontal’s right gutter concept)
            const float topGutter = H * 0.05908f; // ~5.9%
            const float usableH = H - topGutter;

            // Gradient bottom→top (warms near top)
            SkPoint pts[2] = { { (float)startX, (float)stopY }, { (float)startX, (float)startY } };
            const SkColor accent = st->colorFgHot;
            const SkColor warm = SkColorSetRGB(255, 120, 80);
            SkColor  colors[4] = {
                SkColorSetA(accent,  64),
                SkColorSetA(accent, 160),
                SkColorSetA(accent, 220),
                SkColorSetA(warm,   240)
            };
            SkScalar stops[4] = { 0.0f, 0.60f, 0.85f, 1.0f };
            SkPaint grad; grad.setAntiAlias(true);
            grad.setShader(SkGradientShader::MakeLinear(pts, colors, stops, 4, SkTileMode::kClamp));

            auto pyLen = [&](float db) {
                float ny = usableH * MeterMath::dbToNorm(db); // 0..usableH
                if (ny < 0.f) ny = 0.f;
                if (ny > H)   ny = H;
                return ny;
                };

            const float nyL = pyLen(leftSaved_);
            const float nyR = pyLen(rightSaved_);

            // Bars (bottom-up)
            c->drawRect(SkRect::MakeLTRB(xL0, (float)stopY - nyL, xL1, (float)stopY), grad);
            c->drawRect(SkRect::MakeLTRB(xR0, (float)stopY - nyR, xR1, (float)stopY), grad);

            // Ticks at −12/−6/−3/0 as horizontal lines
            const std::array<float, 4> ticksDb{ -12.f, -6.f, -3.f, 0.f };
            SkPaint tickP = st->strokePaint();
            tickP.setColor(SkColorSetA(st->colorFg, 60));
            tickP.setStrokeWidth(1.0f);
            for (float db : ticksDb) {
                const float y = (float)stopY - usableH * MeterMath::dbToNorm(db);
                c->drawLine((float)startX, y, (float)stopX, y, tickP);
            }

            // Peak caps (small horizontal lines)
            SkPaint peakP = st->strokePaint(); peakP.setColor(st->colorFgHot); peakP.setStrokeWidth(1.0f);
            const float yLP = (float)stopY - usableH * MeterMath::dbToNorm(peakLeft_);
            const float yRP = (float)stopY - usableH * MeterMath::dbToNorm(peakRight_);
            if (peakLeft_ > -135.f) c->drawLine(xL0, yLP, xL1, yLP, peakP);
            if (peakRight_ > -135.f) c->drawLine(xR0, yRP, xR1, yRP, peakP);

            // Clip LEDs (tiny dots at top of each column)
            const float ledR = (float)dp(3);
            drawLED(c, st, xL1 - dp(6), (float)startY + dp(8), clipHoldL_);
            drawLED(c, st, xR1 - dp(6), (float)startY + dp(8), clipHoldR_);

            // Dynamics overlays (mapped to Y)
            drawDynamicsVertical(c, st, tindex, usableH, xL0, xL1, xR0, xR1);
        }

        // ---------------- Dynamics overlays (Compression / STC) ------------------
        void drawDynamicsHorizontal(SkCanvas* c, const Style* st, int tindex, float usableW) {
#ifdef SPACE_COMPRESSION
            if (_DATA->tracks[tindex]->fxpower[SPACE_COMPRESSION]) {
                const float H = (float)height;
                const float rowH = H * 0.5f;
                SkPaint compP = st->strokePaint();
                compP.setColor(SkColorSetA(SkColorSetRGB(255, 152, 0), 200));
                compP.setStrokeWidth(1.0f);

                for (int32_t i = 0; i < _appState->channels; ++i) {
                    const float grDb = -_appState->params[tindex][COMPGAIN0 + i].load();
                    const float x = (float)startX + usableW * MeterMath::dbToNorm(grDb);
                    const float y0 = (float)startY + i * rowH;
                    const float y1 = (float)startY + (i + 1) * rowH;
                    c->drawLine(x, y0, x, y1, compP);
                }
                const float thrX = (float)startX + usableW * MeterMath::dbToNorm(_appState->params[tindex][COMPTHR].load());
                c->drawLine(thrX, (float)startY, thrX, (float)stopY, compP);
            }
#endif
#ifdef SPACE_STC
            if (_DATA->tracks[tindex]->fxpower[SPACE_STC].load()) {
                const float H = (float)height;
                const float rowH = H * 0.5f;
                SkPaint stcP = st->strokePaint();
                stcP.setColor(SkColorSetA(SkColorSetRGB(3, 169, 244), 200));
                stcP.setStrokeWidth(1.0f);

                float gainDb = -_appState->params[tindex][STC_CURRENTGAIN].load();
                float x = (float)startX + usableW * MeterMath::dbToNorm(gainDb);
                for (int32_t i = 0; i < _appState->channels; ++i) {
                    const float y0 = (float)startY + i * rowH;
                    const float y1 = (float)startY + (i + 1) * rowH;
                    c->drawLine(x, y0, x, y1, stcP);
                }
                const float thrX = (float)startX + usableW * MeterMath::dbToNorm(_appState->params[tindex][STCOMPTHR].load());
                c->drawLine(thrX, (float)startY, thrX, (float)stopY, stcP);
            }
#endif
        }

        void drawDynamicsVertical(SkCanvas* c, const Style* st, int tindex, float usableH,
            float xL0, float xL1, float xR0, float xR1) {
#ifdef SPACE_COMPRESSION
            if (_DATA->tracks[tindex]->fxpower[SPACE_COMPRESSION]) {
                SkPaint compP = st->strokePaint();
                compP.setColor(SkColorSetA(SkColorSetRGB(255, 152, 0), 200));
                compP.setStrokeWidth(1.0f);

                // per-channel (L then R)
                for (int32_t i = 0; i < _appState->channels; ++i) {
                    const float grDb = -_appState->params[tindex][COMPGAIN0 + i].load();
                    const float y = (float)stopY - usableH * MeterMath::dbToNorm(grDb);

                    // draw across each column’s width for that channel
                    if (i == 0) c->drawLine(xL0, y, xL1, y, compP);
                    else        c->drawLine(xR0, y, xR1, y, compP);
                }
                const float thrY = (float)stopY - usableH * MeterMath::dbToNorm(_appState->params[tindex][COMPTHR].load());
                c->drawLine((float)startX, thrY, (float)stopX, thrY, compP);
            }
#endif
#ifdef SPACE_STC
            if (_DATA->tracks[tindex]->fxpower[SPACE_STC].load()) {
                SkPaint stcP = st->strokePaint();
                stcP.setColor(SkColorSetA(SkColorSetRGB(3, 169, 244), 200));
                stcP.setStrokeWidth(1.0f);

                float gainDb = -_appState->params[tindex][STC_CURRENTGAIN].load();
                float y = (float)stopY - usableH * MeterMath::dbToNorm(gainDb);
                c->drawLine(xL0, y, xL1, y, stcP);
                c->drawLine(xR0, y, xR1, y, stcP);

                const float thrY = (float)stopY - usableH * MeterMath::dbToNorm(_appState->params[tindex][STCOMPTHR].load());
                c->drawLine((float)startX, thrY, (float)stopX, thrY, stcP);
            }
#endif
        }

        // Clip LED helper
        void drawLED(SkCanvas* c, const Style* st, float cx, float cy, int hold) {
            SkColor on = SkColorSetRGB(244, 67, 54);            // red 500
            SkColor off = SkColorSetA(st->colorFg, 60);
            SkPaint p = st->fillPaint(hold > 0 ? on : off);
            c->drawCircle(cx, cy, (float)dp(3), p);
        }

    private:
        Orientation orient_;

        // smoothed values + peak/hold (dB)
        float leftSaved_ = -130.f;
        float rightSaved_ = -130.f;
        float peakLeft_ = -135.f;
        float peakRight_ = -135.f;
        int   dirL_ = -1;
        int   dirR_ = -1;

        // clip LEDs hold counters (frames)
        int clipHoldL_ = 0;
        int clipHoldR_ = 0;
    };

} // namespace tsl::ui

namespace tsl::ui {

    LayoutVertical* createTestUI(tsl::AppState* app) {
        // Root layout: fills entire window vertically
        auto* root = new LayoutVertical(app);

        // --- Top: Track Tabs (like browser tabs)
        auto* tabBar = new TabBarBrowser(app,
            
            [](int idx) {
                // TODO: hook into active track change
                printf("Track %d selected\n", idx + 1);
            });
        tabBar->height = app->style.standardButtonHeight();
        root->addChild(tabBar);

        // --- Center: RecyclerView test
        auto* recycler = new View(app,
            FILL_WIDTH | FILL_HEIGHT,
            Align::Start,
            Align::Start);
        root->addChild(recycler);

        // --- Bottom: Horizontal Meter
        auto* meter = new MeterView(app);
        meter->height = app->style.dp(40);
        root->addChild(meter);

        return root;
    }

} // namespace tsl::ui
#ifdef NEW_UI
void tsl::app::guiSetup2(tsl::AppState* app) {
    app->rootwin2 = std::unique_ptr<tsl::ui::Layout>(tsl::ui::createTestUI(app));
}
void tsl::app::setup_main_window2(tsl::AppState* _appState) {
    {
        std::scoped_lock lk(_STATE->queue_draw, _STATE->queue_callback);
        _appState->rootwin2->enqueueDraw();
    }
}
#endif
