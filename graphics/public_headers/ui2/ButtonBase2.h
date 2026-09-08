#pragma once
#include "view2.h"
#include "Input.h"

#include <string>
#include <atomic>

namespace tsl::ui {

    class Button : public View {
    public:
        Button(tsl::AppState* app);

        std::string label;
        std::function<void()> onClick = nullptr;
        std::function<bool()> isActiveFunc = nullptr;
        std::function<void()> setActiveFunc = nullptr;

        std::atomic<bool> isPressed{ false };
        std::atomic<bool> isDisabled{ false };

        int callback(tsl::graphics::InputEvent& e) override;
        void render(SkCanvas* ctx) override;

    protected:
        virtual SkColor getEffectiveFgColor(bool active, bool hot) const;
        virtual SkColor getEffectiveBgColor(bool active, bool hot) const;
        virtual void drawBackground(SkCanvas* canvas, SkPaint& paint, bool active, bool hot);
        virtual void drawLabel(SkCanvas* canvas, SkFont& font, SkPaint& paint);
    };

    // ------------------------------

    class TextButton : public Button {
    public:
        using Button::Button;

        void render(SkCanvas*) override;
    };

    // ------------------------------

    class TextButtonFramed : public Button {
    public:
        using Button::Button;

        void render(SkCanvas*) override;
    };

    // ------------------------------

    class IconButton : public Button {
    public:
        using Button::Button;

        IconButton(tsl::AppState* app, const std::string& iconChar)
            : Button(app) {
            label = iconChar;
            flags |= SYM_WIDTH;
        }

        void render(SkCanvas*) override;
        int desiredWidth() const override { return desiredHeight(); }
    };


} // namespace tsl::ui
