#include "ui/view2.h"
#include "ui/style.h"
#include "app.h"

using namespace tsl::ui;

void View::redrawSafe() {
	std::lock_guard lk(_STATE->queue_draw2);
	View::enqueueDraw();
};


void View::enqueueDrawSafe() {
	std::lock_guard lk(_STATE->queue_draw2);
	enqueueDraw();
};


void View::removeDrawSafe() {
	std::lock_guard lk(_STATE->queue_draw2);
	removeDraw();
};
void View::enqueueInputSafe() {
	std::lock_guard lk(_STATE->queue_callback2);
	enqueueInput();
};
void View::removeInputSafe() {
	std::lock_guard lk(_STATE->queue_callback2);
	removeInput();
};

void View::enqueueDraw() {
	if (doEnqueue()){
		_STATE->queue_draw2.add(this, prio);
		visible.store(true, std::memory_order_release);
	}
}

void View::removeDraw() {
	_STATE->queue_draw2.del(this);
	visible.store(false, std::memory_order_release);
}

void View::enqueueInput() {}

void View::removeInput() {}


int View::dp(float units) {
	return static_cast<int>(units * _appState->graphics.scaleFactor);
}

Style* View::style() const { return _appState ? &_appState->style : nullptr; }


using namespace tsl::ui;
int View::desiredWidth() const {
	if (flags & FIXED_WIDTH) return width;
	if (flags & SYM_WIDTH) return height;
	if (flags & FILL_WIDTH) return -1;
	if (flags & WRAP_CONTENT_WIDTH) return maxChildWidth();
	if ((flags & FRACTION_WIDTH_PARENT) && _parent) return int(_parent->width * fractionWidth);
	if ((flags & FRACTION_WIDTH_WINDOW) && _appState) return int(_appState->graphics.windowWidth * fractionWidth);
	return width;
}


int View::desiredHeight() const {
	if (flags & FIXED_HEIGHT) return height;
	if (flags & SYM_HEIGHT) return width;
	if (flags & FILL_HEIGHT) return -1;
	if (flags & WRAP_CONTENT_HEIGHT) return maxChildHeight();
	if ((flags & FRACTION_HEIGHT_PARENT) && _parent) return int(_parent->height * fractionHeight);
	if ((flags & FRACTION_HEIGHT_WINDOW) && _appState) return int(_appState->graphics.windowHeight * fractionHeight);
	return height;
}
void Layout::initLayout(Axis axis) {
	if (width == 0) width = _appState->graphics.windowWidth;
	if (height == 0) height = _appState->graphics.windowHeight;

	std::vector<View*> groupStart, groupCenter, groupEnd;
	std::unordered_set<int> overlapGroups;

	// --- Step 1: Group children ---
	for (View* child : children) {
		child->_parent = this;
		if (child->overlap && overlapGroups.count(child->overlap)) continue;
		if (child->overlap) overlapGroups.insert(child->overlap);

		auto align = (axis == Axis::Vertical) ? child->alignV : child->alignH;
		switch (align) {
		case Align::Start:  groupStart.push_back(child); break;
		case Align::Center: groupCenter.push_back(child); break;
		case Align::End:    groupEnd.push_back(child); break;
		}
	}

	const int available = (axis == Axis::Vertical ? height : width);

	auto rawSize = [&](const View* c) {
		return (axis == Axis::Vertical) ? c->desiredHeight() : c->desiredWidth();
		};

	auto groupTotalSize = [&](const std::vector<View*>& group) {
		int sum = 0;
		for (const View* c : group) {
			int s = rawSize(c);
			if (s >= 0) sum += s;
		}
		if (!group.empty()) sum += spacing * int(group.size() - 1);
		return sum;
		};

	// --- Step 2: Compute total size and overflow scale ---
	const int sizeStart = groupTotalSize(groupStart);
	const int sizeCenter = groupTotalSize(groupCenter);
	const int sizeEnd = groupTotalSize(groupEnd);
	const int fullSize = sizeStart + sizeCenter + sizeEnd;

	float scale = 1.0f;
	if (fullSize > available && fullSize > 0) {
		scale = float(available) / float(fullSize);
	}

	// --- Step 3: Count FILL views ---
	auto countFill = [&](const std::vector<View*>& group) {
		int count = 0;
		for (const View* c : group) {
			if ((axis == Axis::Vertical && (c->flags & FILL_HEIGHT)) ||
				(axis == Axis::Horizontal && (c->flags & FILL_WIDTH)))
				++count;
		}
		return count;
		};

	const int fillStartCount = countFill(groupStart);
	const int fillEndCount = countFill(groupEnd);

	// --- Step 4: Compute remaining space for FILL views or gaps ---
	int remStart = 0, remEnd = 0;
	if (!groupCenter.empty()) {
		int used = int((sizeStart + sizeCenter + sizeEnd) * scale);
		int remaining = available - used;
		remStart = remaining / 2;
		remEnd = remaining - remStart;
	}
	else {
		int used = int((sizeStart + sizeEnd) * scale);
		int remaining = available - used;
		int totalFill = fillStartCount + fillEndCount;
		if (totalFill > 0) {
			remStart = (remaining * fillStartCount) / totalFill;
			remEnd = remaining - remStart;
		}
	}

	// --- Step 5: Place group ---
	auto placeGroup = [&](std::vector<View*>& group, int startPos, int rem, int fillCount) {
		int pos = startPos;
		for (View* c : group) {
			int w = c->desiredWidth();
			int h = c->desiredHeight();

			if (axis == Axis::Vertical) {
				if (h < 0 && (c->flags & FILL_HEIGHT) && fillCount > 0)
					h = (rem - spacing * (fillCount - 1)) / fillCount;
				else
					h = int(h * scale);  // <--- SCALE applied here
			}
			else {
				if (w < 0 && (c->flags & FILL_WIDTH) && fillCount > 0)
					w = (rem - spacing * (fillCount - 1)) / fillCount;
				else
					w = int(w * scale);  // <--- SCALE applied here
			}

			if (w < 0) w = width;
			if (h < 0) h = height;
			if (c->flags & SYM_WIDTH)  w = h;
			if (c->flags & SYM_HEIGHT) h = w;

			if (axis == Axis::Vertical) {
				c->relY = pos;
				c->relX = computeAlignH(c, w);
			}
			else {
				c->relX = pos;
				c->relY = computeAlignV(c, h);
			}
			c->width = w;
			c->height = h;
			c->updateBounds();

			// Overlap propagation
			if (c->overlap != 0) {
				for (View* other : children) {
					if (other != c && other->overlap == c->overlap) {
						other->relX = c->relX;
						other->relY = c->relY;
						other->width = c->width;
						other->height = c->height;
						other->updateBounds();
					}
				}
			}
			pos += (axis == Axis::Vertical ? c->height : c->width) + spacing;
		}
		};

	// --- Step 6: Execute placement ---
	int posStart = -scrollOffset;
	int posCenter = posStart + int(sizeStart * scale) + remStart;
	int posEnd = available - int(sizeEnd * scale) -scrollOffset;

	placeGroup(groupStart, posStart, remStart, fillStartCount);
	placeGroup(groupCenter, posCenter, 0, 0);
	placeGroup(groupEnd, posEnd, remEnd, fillEndCount);

	// --- Step 7: Initialize children ---
	for (View* c : children)
		c->init();
}

int Layout::computeAlignH(const View* v, int w) const {
	switch (v->alignH) {
	case Align::Start:  return 0;
	case Align::Center: return (width - w) / 2;
	case Align::End:    return width - w;
	}
	return 0;
}

int Layout::computeAlignV(const View* v, int h) const {
	switch (v->alignV) {
	case Align::Start:  return 0;
	case Align::Center: return (height - h) / 2;
	case Align::End:    return height - h;
	}
	return 0;
}
