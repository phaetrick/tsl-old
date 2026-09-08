#include "ui/style.h"
#include "app.h"

#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkFontMgr.h"

#include "Input.h"

#if defined(OS_WIN)
#include <windows.h>
#include <include/ports/SkTypeface_win.h>
#else
#include <include/ports/SkFontMgr_empty.h>
#endif

#include <roboto-regular-reduced.h>
#include <MaterialIcons-Regular-Reduced.h>

namespace tsl::ui {

	Style::Style(tsl::AppState* state) : _appState(state) {
#if defined(OS_WIN)
		sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_DirectWrite();
#else
		sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_Custom_Empty();
#endif

		auto fontDataText = SkData::MakeWithoutCopy(roboto_reg_data, roboto_reg_size);
		auto fontDataIcon = SkData::MakeWithoutCopy(md_reduced_data, md_reduced_size);

		textTypeface = fontMgr->makeFromData(fontDataText);
		iconTypeface = fontMgr->makeFromData(fontDataIcon);

		rebuild();
	}

	void Style::rebuild() {
		float sf = _appState->graphics.scaleFactor;

		// Scale all sizes
		smallTextPx = 12.f * sf;
		normalTextPx = 14.f * sf;
		largeTextPx = 18.f * sf;
		titleTextPx = 24.f * sf;

		smallIconPx = 18.f * sf;
		normalIconPx = 24.f * sf;
		largeIconPx = 32.f * sf;

		// Setup text fonts
		fontTextSmall_.setTypeface(textTypeface);
		fontTextSmall_.setSize(smallTextPx);

		fontTextNormal_.setTypeface(textTypeface);
		fontTextNormal_.setSize(normalTextPx);

		fontTextLarge_.setTypeface(textTypeface);
		fontTextLarge_.setSize(largeTextPx);

		fontTextTitle_.setTypeface(textTypeface);
		fontTextTitle_.setSize(titleTextPx);

		// Setup icon fonts
		fontIconSmall_.setTypeface(iconTypeface);
		fontIconSmall_.setSize(smallIconPx);

		fontIconNormal_.setTypeface(iconTypeface);
		fontIconNormal_.setSize(normalIconPx);

		fontIconLarge_.setTypeface(iconTypeface);
		fontIconLarge_.setSize(largeIconPx);
	}

	int Style::dp(float units) const {
		return static_cast<int>(units * _appState->graphics.scaleFactor);
	}

	SkPaint Style::strokePaint() const {
		SkPaint p;
		p.setStyle(SkPaint::kStroke_Style);
		p.setStrokeWidth(dp(1.5f));
		return p;
	}

	SkPaint Style::fillPaint(SkColor color) const {
		SkPaint p;
		p.setStyle(SkPaint::kFill_Style);
		p.setColor(color);
		return p;
	}
	int Style::standardButtonHeight() const { return 36; };

}
