#include "ui/knob2.h"
#include "ui/style.h"
#include "app.h"
#include <cmath>
#include <SkPath.h>
#include <algorithm>

using namespace tsl::ui;

constexpr float DEG2RAD = 3.1415926f / 180.f;
constexpr float ARC_SWEEP = 270.f;
constexpr float ARC_START = -225.f;

Knob::Knob(tsl::AppState* app, int paramId)
    : View(app), paramID(paramId) {
    height = dp(14 + 48 + 24);
    width = dp(48);
}

void Knob::init() {
    float y = startY;
    if (drawTitle)
        y += style()->fontTextSmall().getSize();

    arcRadius = dp(48) * 0.5f;
    arcRadiusArrow = arcRadius - dp(2);
    centerX = startX + width * 0.5f;
    centerY = y + arcRadius;
}

void Knob::render(SkCanvas* canvas) {
    auto& p = _STATE->parameters[paramID];
    auto& val = _STATE->params[_STATE->active_track.load()][paramID];
    float value = val.load();
    float percent = (value - p.min) / (p.max - p.min);

    canvas->save();
    canvas->translate(0, 0); // full window space

    if (drawTitle) drawTitleBar(canvas);
    drawArc(canvas);
    drawPointer(canvas, percent);
    if (drawButtons) drawButtonsBar(canvas);

    canvas->restore();
}

void Knob::drawTitleBar(SkCanvas* canvas) {
    SkPaint paint = style()->fillPaint(style()->colorFg);
    SkFont font(style()->fontTextSmall());

    SkRect boundsText;
    font.measureText(title.c_str(), title.size(), SkTextEncoding::kUTF8, &boundsText);
    float x = startX + dp(4);
    float y = startY + boundsText.height();

    canvas->drawSimpleText(title.c_str(), title.size(), SkTextEncoding::kUTF8, x, y, font, paint);
}

void Knob::drawArc(SkCanvas* canvas) {
    SkRect arcRect = SkRect::MakeXYWH(centerX - arcRadius, centerY - arcRadius, arcRadius * 2, arcRadius * 2);

    auto& p = _STATE->parameters[paramID];
    auto& val = _STATE->params[_STATE->active_track.load()][paramID];
    float percent = (val.load() - p.min) / (p.max - p.min);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(dp(2));

    paint.setColor(style()->colorMidiLearn);
    canvas->drawArc(arcRect, ARC_START, ARC_SWEEP * percent, false, paint);

    paint.setColor(style()->colorFg);
    canvas->drawArc(arcRect, ARC_START + ARC_SWEEP * percent, ARC_SWEEP * (1.f - percent), false, paint);
}

void Knob::drawPointer(SkCanvas* canvas, float percent) {
    float angle = (ARC_START + ARC_SWEEP * percent) * DEG2RAD;

    float tipX = centerX + std::cos(angle) * arcRadiusArrow;
    float tipY = centerY + std::sin(angle) * arcRadiusArrow;

    float baseOffset = dp(6);
    float sideAngle = DEG2RAD * 5;

    float x1 = centerX + std::cos(angle - sideAngle) * (arcRadiusArrow - baseOffset);
    float y1 = centerY + std::sin(angle - sideAngle) * (arcRadiusArrow - baseOffset);
    float x2 = centerX + std::cos(angle + sideAngle) * (arcRadiusArrow - baseOffset);
    float y2 = centerY + std::sin(angle + sideAngle) * (arcRadiusArrow - baseOffset);

    SkPath path;
    path.moveTo(x1, y1);
    path.lineTo(tipX, tipY);
    path.lineTo(x2, y2);
    path.close();

    SkPaint paint = style()->strokePaint();
    paint.setColor(style()->colorFg);
    canvas->drawPath(path, paint);
}

void Knob::drawButtonsBar(SkCanvas* canvas) {
    SkFont font(style()->fontTextNormal());
    SkPaint paint = style()->fillPaint(style()->colorFg);

    float bh = dp(24);
    float by = stopY - bh;

    canvas->drawSimpleText("-", 1, SkTextEncoding::kUTF8, startX + dp(6), by + bh - dp(6), font, paint);
    canvas->drawSimpleText("+", 1, SkTextEncoding::kUTF8, stopX - dp(18), by + bh - dp(6), font, paint);
}

int Knob::callback(tsl::graphics::InputEvent& e) {
    switch (e.action) {
    case tsl::graphics::ACTION_DOWN: handleTouchDown(e.x, e.y, e.pointer_id); break;
    case tsl::graphics::ACTION_MOVE: handleTouchMove(e.x, e.y, e.pointer_id); break;
    case tsl::graphics::ACTION_UP:   handleTouchUp(e.x, e.y, e.pointer_id); break;
    }
    return 1;
}

void Knob::handleTouchDown(float x, float y, int pointerId) {
    if (drawTitle && y < startY + style()->fontTextSmall().getSize()) {
        touch = { pointerId, TITLE, x, y };
    }
    else if (drawButtons && y > stopY - dp(24)) {
        touch = { pointerId, (x < startX + width / 2 ? MINUS : PLUS), x, y };
    }
    else {
        touch = { pointerId, ARC, x, y };
        lastAngle = getAngle(x, y);
        lastQuadrant = getQuadrant(x, y);
    }
}

void Knob::handleTouchMove(float x, float y, int pointerId) {
    if (pointerId != touch.pointerId || touch.target != ARC) return;

    float newAngle = getAngle(x, y);
    int quadrant = getQuadrant(x, y);
    if ((quadrant == 1 && lastQuadrant == 4) || (quadrant == 4 && lastQuadrant == 1)) return;

    float delta = (lastAngle - newAngle) / 360.f;
    adjustValue(delta);

    lastAngle = newAngle;
    lastQuadrant = quadrant;
}

void Knob::handleTouchUp(float x, float y, int pointerId) {
    if (pointerId != touch.pointerId) return;

    auto& p = _STATE->parameters[paramID];
    auto& val = _STATE->params[_STATE->active_track.load()][paramID];

    switch (touch.target) {
    case TITLE:
        // valueView.redraw(); // (if implemented)
        break;
    case MINUS:
        adjustValue(-p.progress);
        break;
    case PLUS:
        adjustValue(+p.progress);
        break;
    case ARC:
        if (std::abs(touch.downX - x) < dp(4) && std::abs(touch.downY - y) < dp(4))
            val.store(p.initvalue);
        break;
    default:
        break;
    }

    touch = {};
    enqueueDraw();
}

float Knob::getAngle(float x, float y) const {
    float dx = x - centerX;
    float dy = y - centerY;
    float len = std::hypot(dx, dy);
    if (len == 0.f) return 0.f;
    return std::atan2(dy, dx) * 180.f / 3.1415926f + 180.f;
}

int Knob::getQuadrant(float x, float y) const {
    float dx = x - centerX;
    float dy = centerY - y;
    if (dx >= 0) return dy >= 0 ? 1 : 4;
    return dy >= 0 ? 2 : 3;
}

void Knob::adjustValue(float delta) {
    auto& p = _STATE->parameters[paramID];
    auto& val = _STATE->params[_STATE->active_track.load()][paramID];
    auto value = val.load() + delta * (p.max - p.min);
    value = std::clamp(value, p.min, p.max);
    val.store(value);
    enqueueDraw();
}
