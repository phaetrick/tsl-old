#include "FloatingView.h"
#include "app.h"
using namespace tsl::graphics;

FloatingView::FloatingView(tsl::AppState* appState) : View(appState, WRAP, 0,
	CENTER_ALIGN, 0) {
}

FloatingView::FloatingView(tsl::AppState* appState, const char* t, bool isScroller_) : title{ t }, isScroller{ isScroller_ }, View(appState, WRAP, 0,
	CENTER_ALIGN, 0) {
}


void FloatingView::addRecursiveDraw() {
	View::addRecursiveDraw();
}

void FloatingView::delRecursiveDraw() {
	_STATE->graphics.deleteWindow(windex);
	View::delRecursiveDraw();
	visible_.notify_all();
}

void FloatingView::delRecursiveCB() {
	active.store(-1);
	View::delRecursiveCB();
}


void FloatingView::callback(const InputEvent& e) {
	auto xpos = e.x - x.load(), ypos = e.y - y.load();
	switch (e.action) {
	case ACTION_DOWN: {
		toForeground(windex);
		if (ypos < contentStartY) {
			active.store(xpos > width - rowHeight - lw2 ? -2 : -1);
		}
		else if (ypos < height) {
			active.store(0);
			cb(xpos - borderSize, ypos - contentStartY, ACTION_DOWN, e.pointer_id);
		}
		else active.store(-1);

		olde = e;
		redraw();
		break;
	}
	case ACTION_MOVE: {
		if (olde.pointer_id == e.pointer_id) {
			if (active == -1) {
				startx += (e.x - olde.x);
				if (startx > _STATE->graphics.windowWidth - _STATE->textsize1)
					startx = _STATE->graphics.windowWidth - _STATE->textsize1;
				stopx = startx + width;
				starty += (e.y - olde.y);
				if (starty > _STATE->graphics.windowHeight - _STATE->textsize1)
					starty = _STATE->graphics.windowHeight - _STATE->textsize1;
				stopy = starty + height;
				x = startx;
				y = starty;
				olde = e;
				redraw();
			}
			else if (!isScroller && spacing(olde.x, e.x, olde.y, e.y) > _STATE->textsize2) {
				active = -1;
				olde = e;
				cb(xpos - borderSize, ypos - contentStartY, ACTION_MOVE, e.pointer_id);
				redraw();
			}
			else if (active == 0) { cb(xpos - borderSize, ypos - contentStartY, ACTION_MOVE, e.pointer_id); }
		}
		break;
	}
	case ACTION_UP: {
		if (e.pointer_id == olde.pointer_id) {
			auto act = active.load();
			if (act == -2) {
				deldraw();
				/* was the non-locking delRecursiveCB(): this closes the popup from
				   the input thread while the draw thread walks queue_callback */
				delCB();
			}
			else if (act >= 0) {
				cb(xpos - borderSize, ypos - contentStartY, ACTION_UP, e.pointer_id);
				active.store(-1);
				redraw();
			}
		}
		break;
	}
	case ACTION_MOUSE_WHEEL: {
		cb(xpos - borderSize, ypos - contentStartY, ACTION_MOUSE_WHEEL, e.pointer_id);
		active.store(-1);
		redraw();
	}
	break;
	
	default:break;
	}
}


void FloatingView::render(void*) {

	auto c = _STATE->graphics.getCanvas(windex, x.load(), y.load(), width, height);
	if (!c)
		return;
	c->clear(skcol::bg);
	c->save();
	c->translate(borderSize + lw2, contentStartY);
	c->clipRect(SkRect::MakeXYWH(-borderSize, 0, contentWidth + 2 * borderSize, contentHeight));
	renderContent(c);
	c->restore();

	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::fg);
	auto act = active.load();
	// 1. Define dimensions based on the current bar height
	// This ensures the button is always a square based on the top bar's height
	float btnSide = rowHeight;
	SkRect btnRect = SkRect::MakeXYWH(width - btnSide - lw2, lw2, btnSide, btnSide);

	// 2. Background for active/hover state
	if (act == -2) {
		paint.setStyle(SkPaint::kFill_Style);
		paint.setColor(skcol::blue_transparent);
		c->drawRect(btnRect, paint);
	}

	// 3. Scale the "X" lines relative to the button size
	// Using 30% padding ensures the X looks good regardless of bar height
	float padding = btnSide * 0.30f;
	SkRect xArea = btnRect;
	xArea.inset(padding, padding);

	// 4. Configure Stroke
	paint.setColor(skcol::fg);
	paint.setStyle(SkPaint::kStroke_Style);

	// Scale line thickness: e.g., 10% of the bar height, but at least 1.0f
	float thickness = lw;
	paint.setStrokeWidth(thickness);
	paint.setAntiAlias(true);
	paint.setStrokeCap(SkPaint::kRound_Cap);

	// 5. Draw the lines
	c->drawLine(xArea.fLeft, xArea.fTop, xArea.fRight, xArea.fBottom, paint);
	c->drawLine(xArea.fRight, xArea.fTop, xArea.fLeft, xArea.fBottom, paint);
	if (title != nullptr) {
		SkPaint fpaint;
		fpaint.setAntiAlias(true);
		fpaint.setColor(skcol::fg);
		fpaint.setStrokeWidth(lw);
		fpaint.setStyle(SkPaint::kFill_Style);
		SkFont font(_STATE->font_normal);
		font.setEmbolden(true);
		font.setSize(_STATE->textsize2 * .9);
		c->drawSimpleText(title, strlen(title), SkTextEncoding::kUTF8, tx, ty, font, fpaint);
	}
	// Always reset to Fill so you don't break subsequent drawing code
	paint.setStyle(SkPaint::kFill_Style);
	paint.setStrokeWidth(1.0);
	paint.setStyle(SkPaint::kStroke_Style);
	c->drawRect(SkRect::MakeXYWH(0.5, 0.5, width - 1, height - 1), paint);
}

void FloatingView::setTitle(const char* t) {
	title = t;
}


void FloatingView::init() {
	//perm = true;
	if (_oldWindowWidth != _STATE->windowWidth || _oldWindowHeight != _STATE->windowHeight) {
		/*
		for (auto& s : names) {
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9);
			SkRect bounds{};
			font.measureText(s.c_str(), strlen(s.c_str()),
				SkTextEncoding::kUTF8, &bounds);
			if (width < bounds.width())width = bounds.width();
		*/
		_oldWindowWidth = _STATE->windowWidth;
		_oldWindowHeight = _STATE->windowHeight;
		rowHeight = _STATE->textsize1;
		borderSize = _STATE->textsize2 * 0.5;
		contentStartY = rowHeight + lw2;
		computeContent(_oldWindowWidth - 4 * borderSize - lw, _oldWindowHeight - contentStartY - 3 * borderSize - lw);
		width = contentWidth + 2 * borderSize + lw;
		height = contentHeight + contentStartY + borderSize + lw;
		startx = x = _STATE->windowWidth / 2 - width / 2;
		starty = y = _STATE->windowHeight / 2 - height / 2;
		stopx = startx + width;
		stopy = starty + height;
		if (title != nullptr) {
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9);
			SkRect bounds{};
			auto tw = font.measureText(title, strlen(title), SkTextEncoding::kUTF8, &bounds);
			ty = SkFloatToScalar(lw2 + rowHeight * .5f - bounds.centerY());
			tx = SkFloatToScalar(lw2 + borderSize + contentWidth * 0.5 - bounds.centerX());
		}
	}

}


