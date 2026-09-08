#include "ui/ButtonBase2.h"
#include "ui/style.h"

#include "app.h"
#include <cstring>

using namespace tsl::ui;

Button::Button(tsl::AppState* app) : View(app) {
    height = style()->standardButtonHeight();
}
int Button::callback(tsl::graphics::InputEvent& e) {
    if (isDisabled) return 1;

    switch (e.action) {
    case tsl::graphics::ACTION_DOWN:
        isPressed = true;
        enqueueDraw();
        return 1;
        break;

    case tsl::graphics::ACTION_UP:
        if (isPressed) {
            isPressed = false;
            enqueueDraw();
            if (onClick) onClick();
            return 1;
        }
        break;

    case tsl::graphics::ACTION_MOVE:
        isPressed = false;
        enqueueDraw();
        return 1;
        break;
    }
}


SkColor Button::getEffectiveFgColor(bool active, bool hot) const {
#if defined HAS_MIDI
    if (_STATE->midilearning && id != PARAM_NOT_ASSIGNED &&
        _STATE->parameters[id].callback != nullptr) {
        return style()->colorMidiLearn;
    }
#endif
    if (isDisabled) return style()->colorDisabledFg;
    return active || hot ? style()->colorFgHot : style()->colorFg;
}


void Button::drawBackground(SkCanvas* canvas, SkPaint& paint, bool active, bool hot) {
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(getEffectiveBgColor(active, hot));
    float r = height * 0.2f;
    canvas->drawRoundRect(bounds, r, r, paint);
}

void Button::drawLabel(SkCanvas* canvas, SkFont& font, SkPaint& paint) {
    paint.setColor(getEffectiveFgColor(false, isPressed));
    paint.setAntiAlias(true);
    
    SkRect textBounds;
    font.measureText(label.c_str(), label.size(), SkTextEncoding::kUTF8, &textBounds);

    float x = bounds.centerX() - textBounds.centerX();
    float y = bounds.centerY() - textBounds.centerY();
    canvas->drawSimpleText(label.c_str(), label.size(), SkTextEncoding::kUTF8, x, y, font, paint);
}

void Button::render(SkCanvas* canvas) {
    SkPaint paint;
    SkFont font;

    canvas->save();
    canvas->translate(startX, startY);

    bool active = isActiveFunc ? isActiveFunc() : false;
    drawBackground(canvas, paint, active, isPressed);
    drawLabel(canvas, font, paint);

    canvas->restore();
}

// ------------------------------

void TextButton::render(SkCanvas* canvas) {
    Button::render(canvas);
}

// ------------------------------

void TextButtonFramed::render(SkCanvas* canvas) {
    SkPaint paint;
    SkFont font;

    canvas->save();
    canvas->translate(startX, startY);

    bool active = isActiveFunc ? isActiveFunc() : false;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(style()->dp(1));
    paint.setColor(getEffectiveFgColor(active, isPressed));
    float r = height * 0.2f;
    canvas->drawRoundRect(bounds.makeOffset(-startX, -startY), r, r, paint);

    drawLabel(canvas, font, paint);
    canvas->restore();
}

// ------------------------------

void IconButton::render(SkCanvas* canvas) {
    SkPaint paint;
    SkFont font(style()->fontIconNormal());

    canvas->save();
    canvas->translate(startX, startY);

    bool active = isActiveFunc ? isActiveFunc() : false;
    drawBackground(canvas, paint, active, isPressed);

    paint.setColor(getEffectiveFgColor(active, isPressed));
    paint.setAntiAlias(true);

    SkRect boundsText;
    font.measureText(label.c_str(), label.size(), SkTextEncoding::kUTF8, &boundsText);
    float x = width * 0.5f - boundsText.centerX();
    float y = height * 0.5f - boundsText.centerY();
    canvas->drawSimpleText(label.c_str(), label.size(), SkTextEncoding::kUTF8, x, y, font, paint);

    canvas->restore();
}
