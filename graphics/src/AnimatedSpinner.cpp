#include "AnimatedSpinner.h"
#include "app.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkGradientShader.h"
#include "SkImageFilters.h"
#include <chrono>
#include <cmath>

using namespace tsl::graphics;

void AnimatedSpinner::init() {
	// Full screen overlay
	startx = 0;
	starty = 0;
	width = _STATE->windowWidth;
	height = _STATE->windowHeight;
	stopx = width.load();
	stopy = height.load();
	spinnerRadius = std::min(width.load(), height.load()) * 0.075f;
}


void AnimatedSpinner::render(void* c) {
	if (width != _STATE->windowWidth || height != _STATE->windowHeight) {
		init();
	}
	auto canvas = _STATE->graphics.getCanvas(windowindex, startx, starty, width, height);
	if (!canvas)
		return;
	canvas->clear(SK_ColorTRANSPARENT);
	// Update animation

	// Draw glassy background (much more subtle)
	drawGlassyBackground(canvas);

	// Draw spinner
	drawSpinner(canvas);
}

void AnimatedSpinner::delRecursiveDraw() {
	View::delRecursiveDraw();
	_STATE->graphics.deleteWindow(windowindex);
}

void AnimatedSpinner::addRecursiveDraw() {
	startTime = std::chrono::steady_clock::now();
	View::addRecursiveDraw();
}



void AnimatedSpinner::drawGlassyBackground(SkCanvas* canvas) {
	// Simple semi-transparent background with slight blur
	SkPaint glassPaint;
	glassPaint.setColor(SkColorSetARGB(200, 40, 40, 40));  // Light, transparent
	glassPaint.setStyle(SkPaint::kFill_Style);

	// Optional: Add a simple blur filter if available
	//sk_sp<SkImageFilter> blur = SkImageFilters::Blur(102.0f, 102.0f, nullptr);
	//glassPaint.setImageFilter(blur);

	canvas->drawRect(SkRect::MakeWH(width, height), glassPaint);
}
/*
void AnimatedSpinner::drawGlassyBackground(SkCanvas* canvas) {
	SkPaint glassPaint;
	glassPaint.setColor(SkColorSetARGB(50, 240, 245, 250));  // Subtle tinted glass
	canvas->drawRect(SkRect::MakeWH(width, height), glassPaint);
}
*/
void AnimatedSpinner::drawSpinner(SkCanvas* canvas) {
	// Calculate time once
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
	float seconds = elapsed.count() / 1000.0f;

	// Use same time for both rotation and spread
	rotation = fmod(seconds * animationSpeed, 360.0f);

	float centerX = width * 0.5f;
	float centerY = height * 0.5f;

	canvas->save();
	canvas->translate(centerX, centerY);
	canvas->rotate(rotation);  // Rotate the whole spinner continuously

	// Time-based spread using same seconds
	float timeInCycle = fmod(seconds, 2.0f);  // 2-second cycle
	float spreadAngle;
	if (timeInCycle < 1.0f) {
		spreadAngle = timeInCycle * 360.0f;
	}
	else {
		spreadAngle = (2.0f - timeInCycle) * 360.0f;
	}

	for (int i = 0; i < numSegments; i++) {
		float segmentAngle = (spreadAngle / numSegments) * i;
		float alpha = 1. - (float(i) / (numSegments - 1));

		SkPaint segmentPaint;
		segmentPaint.setStyle(SkPaint::kStroke_Style);
		segmentPaint.setStrokeCap(SkPaint::kRound_Cap);
		segmentPaint.setAntiAlias(true);
		segmentPaint.setStrokeWidth(strokeWidth);

		SkColor segmentColor = SkColorSetARGB(
			static_cast<U8CPU>(255 * alpha),
			SkColorGetR(primaryColor),
			SkColorGetG(primaryColor),
			SkColorGetB(primaryColor)
		);
		segmentPaint.setColor(segmentColor);

		canvas->save();
		canvas->rotate(-segmentAngle);

		float innerRadius = spinnerRadius * 0.6f;
		float outerRadius = spinnerRadius;
		canvas->drawLine(0, -innerRadius, 0, -outerRadius, segmentPaint);

		canvas->restore();
	}

	canvas->restore();
}
