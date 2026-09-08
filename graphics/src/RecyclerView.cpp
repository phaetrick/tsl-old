#include "RecyclerView.h"
#include <SkGradientShader.h>
using namespace tsl::graphics;
void ScrollViewBase::drawOverscrollGlow(SkCanvas* canvas, bool isTop, float amount) {
	// 1. FIXED ALPHA MATH: 
	float alpha = std::min(amount / 10.0f, 1.0f);
	alpha = std::max(0.01f, std::min(alpha, 1.0f)); // Clamp floor

	// Scale height dynamically
	float height = 25.0f + (amount * 0.8f);

	SkRect glowRect;
	SkPoint pts[2];

	if (isTop) {
		// Rectangle starts at boxoffsety
		glowRect = SkRect::MakeXYWH(glowStartX, boxoffsety, glowWidth, height);

		// FIXED: Gradient must start at boxoffsety and move downwards by 'height'
		pts[0] = SkPoint::Make(glowStartX + glowWidth * 0.5f, boxoffsety);
		pts[1] = SkPoint::Make(glowStartX + glowWidth * 0.5f, boxoffsety + height);
	}
	else {
		float visualBottom = boxoffsety + boxheight;
		glowRect = SkRect::MakeXYWH(glowStartX, visualBottom - height, glowWidth, height);
		pts[0] = SkPoint::Make(glowStartX + glowWidth * 0.5f, visualBottom);
		pts[1] = SkPoint::Make(glowStartX + glowWidth * 0.5f, visualBottom - height);
	}

	// Boosted opacity colors
	SkColor colors[] = {
		SkColorSetA(skcol::blue_transparent, static_cast<int>(255 * alpha * 0.50f)),
		SkColorSetA(skcol::blue_transparent, static_cast<int>(255 * alpha * 0.25f)),
		SkColorSetA(skcol::blue_transparent, 0)
	};
	SkScalar pos[] = { 0.0f, 0.15f, 1.0f };

	SkPaint paint;
	paint.setShader(SkGradientShader::MakeLinear(
		pts, colors, pos, 3, SkTileMode::kClamp
	));
	paint.setStyle(SkPaint::kFill_Style);
	paint.setAntiAlias(true);

	canvas->drawRect(glowRect, paint);
}
void ScrollViewBase::updateOverscroll(SkCanvas* c) {
	auto ost = overscrollTop.load();
	auto osb = overscrollBottom.load();
	bool needsAnim = (ost > 0.0f || osb > 0.0f);

	if (needsAnim) {
		// Smooth out the decay rate so it takes ~300ms to pull back (Rubber-banding)
		if (ost > 0.0f) {
			drawOverscrollGlow(c, true, ost);
			ost *= 0.96f; // Relaxed decay rate for better retention
			if (ost < 0.005f) ost = 0.0f;
			overscrollTop.store(ost);
		}
		if (osb > 0.0f) {
			drawOverscrollGlow(c, false, osb);
			osb *= 0.96f;
			if (osb < 0.005f) osb = 0.0f;
			overscrollBottom.store(osb);
		}
	}
}