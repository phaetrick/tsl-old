#pragma once
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkColor.h"
#include <memory>

namespace tsl {
	class AppState;
}

namespace tsl::ui {

	class Style {
	public:
		explicit Style(tsl::AppState* state);

		void rebuild();

		// Fonts
		const SkFont& fontTextSmall()  const { return fontTextSmall_; }
		const SkFont& fontTextNormal() const { return fontTextNormal_; }
		const SkFont& fontTextLarge()  const { return fontTextLarge_; }
		const SkFont& fontTextTitle()  const { return fontTextTitle_; }

		const SkFont& fontIconSmall()  const { return fontIconSmall_; }
		const SkFont& fontIconNormal() const { return fontIconNormal_; }
		const SkFont& fontIconLarge()  const { return fontIconLarge_; }

		// Paint helpers
		SkPaint strokePaint() const;
		SkPaint fillPaint(SkColor color) const;

		// Scale helper
		int dp(float units) const;
		int standardButtonHeight() const;

		// Colors
		SkColor colorBg = SkColorSetRGB(0, 0, 0);
		SkColor colorSurface = SkColorSetRGB(33, 33, 33);
		SkColor colorFg = SkColorSetRGB(255, 255, 255);
		SkColor colorFgHot = SkColorSetRGB(3, 169, 244);      // Light Blue A400
		SkColor colorBgHot = SkColorSetRGB(48, 48, 48);        // Hover BG
		SkColor colorDisabledFg = SkColorSetARGB(128, 255, 255, 255);
		SkColor colorDisabledBg = SkColorSetARGB(64, 255, 255, 255);
		SkColor colorMidiLearn = SkColorSetRGB(255, 138, 101); // Deep Orange 300

	private:
		tsl::AppState* _appState = nullptr;

		sk_sp<SkTypeface> textTypeface;
		sk_sp<SkTypeface> iconTypeface;

		float smallTextPx{}, normalTextPx{}, largeTextPx{}, titleTextPx{};
		float smallIconPx{}, normalIconPx{}, largeIconPx{};

		SkFont fontTextSmall_;
		SkFont fontTextNormal_;
		SkFont fontTextLarge_;
		SkFont fontTextTitle_;

		SkFont fontIconSmall_;
		SkFont fontIconNormal_;
		SkFont fontIconLarge_;
	};
}
