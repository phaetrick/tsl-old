#include "logger.h"
#include "Input.h"
#include "tools/queuetsl.h"
#include "view.h"
#include "colours.h"
#include <app.h>
#include <SkStream.h>
#include <SkPath.h>
#include <SkFontMetrics.h>
#include <SkCanvas.h>
#include <SkPaint.h>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>


using namespace tsl::graphics;

const char* tsl::graphics::nums32[] = { "1", "2", "3", "4","5","6","7","8","9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32" };
std::vector<float> tsl::graphics::numvals32 = { 1, 2, 3, 4,5,6,7,8,9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };


View::View(View const& other) {
	// memcpy(this, (const void *) &other, sizeof(View));
	_appState = other._appState;
	name = other.name;
	parent = other.parent;
	id = other.id;
	width = other.width.load();
	height = other.height.load();
	startx = other.startx.load();
	starty = other.starty.load();
	stopx = other.stopx.load();
	stopy = other.stopy.load();
	pos_p = other.pos_p;
	x_pos_p = other.x_pos_p.load();
	y_pos_p = other.y_pos_p.load();
	active = other.active.load();
	prio = other.prio;
	perm = other.perm;
	overlap = other.overlap;
	scalefactor = other.scalefactor;
	alignment = other.alignment;
	aspect_ratio = other.aspect_ratio;
	padding = other.padding;
	paddingleft = other.paddingleft;
	paddingright = other.paddingright;
	paddingtop = other.paddingtop;
	paddingbottom = other.paddingbottom;
	bg = other.bg;
	size_reference = other.size_reference;
	size_reference_scale = other.size_reference_scale;
	viewport = other.viewport;
	userdata = other.userdata;
	visible_.store(other.visible_.load());
	innerAlignment = other.innerAlignment;
}



void View::textDisplayCentered(View* v, SkCanvas* canvas, SkPaint& paint, SkFont& font,
	const char* text, float scale, bool clip,
	SkTextEncoding textencoding) {
	auto fontsize = SkFloatToScalar(std::min(v->width, v->height) * scale);
	font.setSize(fontsize);
	SkRect bounds;
	float textwidth = font.measureText(text, strlen(text), textencoding, &bounds);
	if (clip) {
		SkPath path;
		path.addRect(0, 0, v->width, v->height);
		canvas->save();
		canvas->clipPath(path);
	}
	auto xpos = SkFloatToScalar(v->startx + v->width * .5f - bounds.centerX());
	auto ypos = SkFloatToScalar(v->starty + v->height * .5f - bounds.centerY());
	//SkScalar xpos = SkFloatToScalar(v->startx + v->width * .5f - textwidth * .5f);
	//SkScalar ypos = SkFloatToScalar(v->starty + v->height - (v->height - fontsize) * .5f);
	canvas->drawSimpleText(text, strlen(text), textencoding, xpos, ypos, font, paint);
	if (clip)
		canvas->restore();
};


void View::textDisplayCenteredFixed(View* v, SkCanvas* canvas, SkPaint& paint, SkFont& font,
	const char* text, float scale, bool clip) {
	canvas->save();
	canvas->translate(SkFloatToScalar(v->startx), SkFloatToScalar(v->starty));
	SkRect bounds;
	font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	if (clip) {
		SkPath path;
		path.addRect(0, 0, v->width, v->height);
		canvas->clipPath(path);
	}
	auto xpos = SkFloatToScalar(v->width * .5f - bounds.centerX());
	auto ypos = SkFloatToScalar(v->height * .5f - bounds.centerY());
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font, paint);
	canvas->restore();
};


void View::centerText(SkFont& font, View* v, const char* s, float& x, float& y) {
	SkRect bounds{};
	font.measureText(s, strlen(s), SkTextEncoding::kUTF8, &bounds);
	x = SkFloatToScalar(v->width * .5f - bounds.centerX());
	y = SkFloatToScalar(v->height * .5f - bounds.centerY());
};

void View::centerText(SkFont& font, float w, float h, const char* s, float& x, float& y) {
	View v(nullptr);
	v.width = w;
	v.height = h;
	centerText(font, &v, s, x, y);
};

void View::measureText(View* v, SkFont& font, const char* text, float scale, float* x, float* y,
	float* fontsize) {
	SkScalar textsize = 100;//std::min(v->height, v->width);
	font.setSize(textsize);
	SkRect bounds{};
	font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	float scaletmp = v->width / bounds.width();
	textsize *= scaletmp;
	font.setSize(textsize);
	font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	if (bounds.height() > v->height) {
		scaletmp = v->height / bounds.height();
		textsize *= scaletmp;
	}

	textsize *= scale;
	font.setSize(textsize);
	font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	*x = SkFloatToScalar(v->width * .5f - bounds.centerX());
	*y = SkFloatToScalar(v->height * .5f - bounds.centerY());
	//*x = SkFloatToScalar((v->width - bounds.width()) * .5f);
	//*y = SkFloatToScalar(v->height - ((v->height - bounds.height()) * .5f));
	*fontsize = textsize;
};


float View::measureTextFixed(float w, float h, SkFont& font, const char* text, float* x, float* y,
	float fontsize) {
	font.setSize(fontsize);
	SkRect bounds{};
	auto width = font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	*x = SkFloatToScalar((w - width) * .5f);
	*y = SkFloatToScalar(h - ((h - bounds.height()) * .5f));
	return width;
};

float View::measureWidth(SkFont& font, const char* text) {
	return font.measureText(text, strlen(text), SkTextEncoding::kUTF8);
}

float View::measureWidth(SkFont& font, std::vector<std::string>& text) {
	float width = 0;

	for (auto& s : text) {
		auto w = measureWidth(font, s.c_str());
		width = w > width ? w : width;
	}
	return width;
};

std::string View::truncateText(const SkFont& font, const std::string& text, float maxWidth) {
	float width = font.measureText(text.c_str(), text.size(), SkTextEncoding::kUTF8);
	if (width <= maxWidth)
		return text;

	const std::string ellipsis = "...";
	float ellipsisWidth = font.measureText(ellipsis.c_str(), ellipsis.size(), SkTextEncoding::kUTF8);
	float availWidth = maxWidth - ellipsisWidth;
	if (availWidth <= 0)
		return ellipsis;

	// binary search for the longest prefix that fits
	int lo = 0, hi = (int)text.size();
	while (lo < hi) {
		int mid = (lo + hi + 1) / 2;
		float w = font.measureText(text.c_str(), mid, SkTextEncoding::kUTF8);
		if (w <= availWidth)
			lo = mid;
		else
			hi = mid - 1;
	}

	return text.substr(0, lo) + ellipsis;
}


#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif

bool View::isInside(const InputEvent& e) const {
	return e.x >= startx && e.x < stopx && e.y >= starty && e.y < stopy;
}

void View::render(void*) {
	CANVAS
		flush(c, bg);
}

void View::flush(SkCanvas* c, const uint32_t col) const {
	SkPaint paint;
	paint.setColor(static_cast<SkColor>(col));
	paint.setAntiAlias(true);
	c->drawRect(SkRect::MakeXYWH(startx, starty, width, height), paint);
};

void View::drawRect(SkCanvas* c, const uint32_t col, float strokeWidth) const {
	SkPaint paint;
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStroke(strokeWidth);
	paint.setAntiAlias(true);
	paint.setColor(static_cast<SkColor>(col));
	const float lwd2 = strokeWidth * .5f;
	c->drawRect(SkRect::MakeXYWH(startx + lwd2, starty + lwd2, width - strokeWidth,
		height - strokeWidth), paint);
};

void View::borderWindow(SkCanvas* c, uint32_t col, float strokeWidth) const {
	SkPaint paint;
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStroke(strokeWidth);
	paint.setAntiAlias(true);
	paint.setColor(static_cast<SkColor>(col));
	const float lwd2 = strokeWidth * .5f;
	c->drawRect(SkRect::MakeXYWH(lwd2, lwd2, width - strokeWidth,
		height - strokeWidth), paint);
};


void View::redraw() {
	{
		std::lock_guard lk(_STATE->queue_draw);
		_STATE->queue_draw.add(this, prio);
	}
	visible_.store(true, std::memory_order_release);
}

void
View::redrawDirect() {
	_STATE->queue_draw.add(this, prio);
	visible_.store(true, std::memory_order_release); // Ensure visibility after adding to the queue
}

void View::addDraw() {
	std::lock_guard lk(_STATE->queue_draw);
		addRecursiveDraw();
	}

void View::deldraw() {
	std::lock_guard lk(_STATE->queue_draw);
	delRecursiveDraw();
}

void View::addCB() {
	std::lock_guard lk(_STATE->queue_callback);
	addRecursiveCB();
}

void View::delCB() {
	std::lock_guard lk(_STATE->queue_callback);
		delRecursiveCB();
}

void View::addRecursiveCB() {
	_STATE->queue_callback.add(this, prio);
}

void
View::delRecursiveCB() {
	_STATE->queue_callback.del(this);
	if(_STATE->mCurrentOver == this)
		_STATE->mCurrentOver = nullptr;
	_STATE->inputEventPool.invalidateViewEvents(this);
#ifdef PLUGIN_MODE
	auto tindex = _STATE->active_track.load();
	tsl::parameters::Event e;
	e.setup(_STATE, tindex, id);
	_STATE->EndParamChange(e);
#endif
}

void View::addRecursiveDraw() {
	_STATE->queue_draw.add(this, prio);
	visible_.store(true, std::memory_order_release); // Ensure visibility after adding to the queue
}

void
View::delRecursiveDraw() {
	_STATE->queue_draw.del(this);
	visible_.store(false, std::memory_order_release); // Ensure visibility after adding to the queue
}

void View::waitForDrawDetached() {
	// Wait for the draw queue to be empty before proceeding
	while (visible_.load()) {
		visible_.wait(true, std::memory_order_relaxed);
	}
}



int View::setViewPort(SkRect& o, bool everything) {
	viewport = o;
	return 1;
}

int Layout::setViewPort(SkRect& o, bool everything) {
	int elements_added = 0;

	switch (everything) {
	case LAST_ONLY:
		if (childs.empty()) {
			viewport = o;
			elements_added++;
		}
		else {
			for (auto temp : childs) {
				elements_added += temp->setViewPort(o, LAST_ONLY);
			}
		}
		break;

	case EVERYTHING:
		viewport = o;
		elements_added++;
		for (auto temp : childs) {
			elements_added += temp->setViewPort(o, EVERYTHING);
		}
		break;
	default:
		break;
	}
	return elements_added;
}


void
Layout::addRecursiveDraw() {
	if (childs.empty()) {
		auto& queue = _STATE->queue_draw;
		queue.add(this, prio);
		visible_.store(true, std::memory_order_release);
	}
	else {
		for (const auto& temp : childs) {
			if (temp->overlap)
				continue;
			temp->addRecursiveDraw();
		}
	}

}

void
Layout::delRecursiveDraw() {
	auto& queue = _STATE->queue_draw;
	if (childs.empty()) {
		queue.del(this);
		visible_.store(false, std::memory_order_release);
	}
	else {
		for (const auto& temp : childs) {
			temp->delRecursiveDraw();
		}
        queue.del(this);
        visible_.store(false, std::memory_order_release);
	}
}


void
Layout::addRecursiveCB() {
	auto& queue = _STATE->queue_callback;
	if (childs.empty()) {
		queue.add(this, prio);
	}
	else {
		for (const auto& temp : childs) {
			if (temp->overlap)
				continue;
			temp->addRecursiveCB();
		}
	}
}

void
Layout::delRecursiveCB() {
	auto& queue = _STATE->queue_callback;
	if (childs.empty()) {
	}
	else {
		for (const auto& temp : childs) {
			temp->delRecursiveCB();
		}
        queue.del(this);
    }
}

void View::computePadding() {
	if (padding != 0.f) {
		float top = height * padding / 100.f;
		height -= top * 2;
		starty += top;
		stopy -= top;
		float left = width * padding / 100.f;
		width -= left * 2;
		startx += left;
		stopx -= left;
	}
	if (paddingleft != 0.f && paddingright == 0.f) {
		float left = width * paddingleft / 100.f;
		width -= left;
		startx += left;
	}
	else if (paddingright != 0.f && paddingleft == 0.f) {
		float right = width * paddingright / 100.f;
		width -= right;
		stopx -= right;
	}
	else if (paddingleft != 0.f && paddingright != 0.f) {
		float left = width * paddingleft / 100.f;
		startx += left;
		float right = width * paddingright / 100.f;
		stopx -= right;
		width -= right;
		width -= left;
	}
	if (paddingtop != 0.f && paddingbottom == 0.f) {
		float top = height * paddingtop / 100.f;
		height -= top;
		starty += top;
	}
	else if (paddingbottom != 0.f && paddingtop == 0.f) {
		float bottom = height * paddingbottom / 100.f;
		height -= bottom;
		stopy -= bottom;
	}
	else if (paddingtop != 0.f && paddingbottom != 0.f) {
		float top = height * paddingtop / 100.f;
		starty += top;
		float bottom = height * paddingbottom / 100.f;
		stopy -= bottom;
		height -= top;
		height -= bottom;
	}
}

Divider::Divider(tsl::AppState* appState, int
	_alignment, Layout* _parent) : View(appState) {
	name = "div";
	prio = 10;


	scalefactor = VIEW_COMPUTESIZE;
	alignment = _alignment;
	aspect_ratio = VIEW_COMPUTESIZE;
	bg = skcol::fg;
	if (_parent != nullptr)
		_parent->addChild(this);
}

void Divider::computeWidth() {
#ifdef PLATFORM_DESKTOP
	width = 1.0;
#else
	width = lw;
#endif
}

void Divider::computeHeight() {
#ifdef PLATFORM_DESKTOP
	height = 1.0;
#else
	height = lw;
#endif
}


Layout::~Layout() {
	for (auto& c : childs) {
		delete c;
	}
}

void View::computeHeight() {
	if (scalefactor == WRAP_CONTENT) {
		LOGE("Warning: WRAP_CONTENT set for non Layout");
		height = 2;
	}
	else if (scalefactor == SYM) {
		height = width.load();
	}
	else if (scalefactor == WRAP) {
		height = 0;
	}
	else if (scalefactor > 0) {
		if (aspect_ratio == PERCENTAGE_FROM_MAIN_WINDOW)
			height = (int)(_STATE->windowHeight * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_MAIN_WINDOW)
			height = (int)(_STATE->windowHeight / scalefactor);
		else if (aspect_ratio == PERCENTAGE_FROM_PARENT_View)
			height = (int)(parent->height * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_PARENT_View)
			height = (int)(parent->height / scalefactor);
		else if (aspect_ratio == ABSOLUTE_VALUE)
			height = (int)scalefactor;
		else if (aspect_ratio == VALUE_FROM_POINTER && size_reference)
			height = *size_reference *
			(size_reference_scale ? size_reference_scale : 1.0f);
		else if (aspect_ratio == VIEW_COMPUTESIZE)
			computeSize();
	}
	//    LOGE("View COMPUTED HEIGHT for %s : %d", name, height);
}

void View::computeWidth() {
	if (scalefactor == WRAP_CONTENT) {
		LOGE("Warning: WRAP_CONTENT set for non Layout");
		width = 2;
	}
	else if (scalefactor == SYM) {
		width = height.load();
	}
	else if (scalefactor == WRAP) {
		width = 0;
	}
	else if (scalefactor > 0) {
		if (aspect_ratio == PERCENTAGE_FROM_MAIN_WINDOW)
			width = (int)(_STATE->windowWidth * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_MAIN_WINDOW)
			width = (int)(_STATE->windowWidth / scalefactor);
		else if (aspect_ratio == PERCENTAGE_FROM_PARENT_View)
			width = (int)(parent->width * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_PARENT_View)
			width = (int)(parent->width / scalefactor);
		else if (aspect_ratio == ABSOLUTE_VALUE)
			width = (int)scalefactor;
		else if (aspect_ratio == VALUE_FROM_POINTER && size_reference)
			width = *size_reference *
			(size_reference_scale ? size_reference_scale : 1.0f);
		else if (aspect_ratio == VIEW_COMPUTESIZE)
			computeSize();
	}

	//  LOGE("COMPUTED WIDTH for %s : %d", name, width);
}


void Layout::computeHeight() {
	if (scalefactor == WRAP_CONTENT) {
		int tmpheight = 0;
		for (auto v : childs) {
			v->computeHeight();
			if (v->height == 0)
				v->height = 2;
			tmpheight += v->height;
		}
		if (tmpheight == 0) {
			LOGE("Warning: s: %s view_compute_height(%s) height: %d Setting to 2",
				scalefactor == WRAP_CONTENT ? "WRAP_CONTENT" : "Unknown",
				name, height.load());
			tmpheight = 2;
		}
		height = tmpheight;
	}
	else if (scalefactor == SYM) {
		height = width.load();
	}
	else if (scalefactor == WRAP) {
		height = 0;
	}
	else if (scalefactor > 0) {
		if (aspect_ratio == PERCENTAGE_FROM_MAIN_WINDOW)
			height = (int)(_STATE->windowHeight * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_MAIN_WINDOW)
			height = (int)(_STATE->windowHeight / scalefactor);
		else if (aspect_ratio == PERCENTAGE_FROM_PARENT_View)
			height = (int)(parent->height * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_PARENT_View)
			height = (int)(parent->height / scalefactor);
		else if (aspect_ratio == ABSOLUTE_VALUE)
			height = (int)scalefactor;
		else if (aspect_ratio == VALUE_FROM_POINTER && size_reference)
			height = *size_reference *
			(size_reference_scale ? size_reference_scale : 1.0f);
		else if (aspect_ratio == VIEW_COMPUTESIZE)
			computeSize();
	}
	// LOGE("Lyout COMPUTED HEIGHT for %s : %d", name, height);
}

void Layout::computeWidth() {
	if (scalefactor == WRAP_CONTENT) {
		int tmpwidth = 0;
		for (auto v : childs) {
			v->computeWidth();
			if (v->width == 0)
				v->width = 2;
			tmpwidth += v->width;
		}
		if (tmpwidth == 0) {
			LOGE("Warning: s: %s view_compute_width(%s) width: %d Setting to 2",
				scalefactor == WRAP_CONTENT ? "WRAP_CONTENT" : "Unknown",
				name, tmpwidth);
			tmpwidth = 2;
		}
		width = tmpwidth;
	}
	else if (scalefactor == SYM) {
		width = height.load();
	}
	else if (scalefactor == WRAP) {
		width = 0;
	}
	else if (scalefactor > 0) {
		if (aspect_ratio == PERCENTAGE_FROM_MAIN_WINDOW)
			width = (int)(_STATE->windowWidth * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_MAIN_WINDOW)
			width = (int)(_STATE->windowWidth / scalefactor);
		else if (aspect_ratio == PERCENTAGE_FROM_PARENT_View)
			width = (int)(parent->width * .01 * scalefactor);
		else if (aspect_ratio == RATIO_FROM_PARENT_View)
			width = (int)(parent->width / scalefactor);
		else if (aspect_ratio == ABSOLUTE_VALUE)
			width = (int)scalefactor;
		else if (aspect_ratio == VALUE_FROM_POINTER && size_reference)
			width = *size_reference *
			(size_reference_scale ? size_reference_scale : 1.0f);
		else if (aspect_ratio == VIEW_COMPUTESIZE)
			computeSize();
	}
}


void Layout::initHorizontal() {
	if (childs.empty())
		return;
	/*
		if(DEBUG) {
			LOGE("Hoz %s->init nchilds: %d H: %d W: %d sax: %d sox %d say: %d soy: %d",
				 name, childs.size(), height, width, (int) startx, (int) stopx, (int) starty,
				 (int) stopy);
		}
	*/
	std::vector<View*> te, tew, ce, cew, be, bew;

	for (auto temp : childs) {
		temp->_appState = _appState;
		temp->width = width.load();
		temp->parent = this;
		temp->startx = startx.load();
		temp->stopx = stopx.load();
		temp->computeHeight();
		/*
			  if(DEBUG)
			  LOGE("Hoz childs: %s w: %d h: %d sax: %g stopx: %g", temp->name, temp->width,
				   temp->height, temp->startx, temp->stopx);
	  */
		switch (temp->alignment) {

		case CENTER_ALIGN:
			if (temp->scalefactor != WRAP) {
				ce.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				cew.push_back(temp);
			}
			break;

		case START_ALIGN:
			if (temp->scalefactor != WRAP) {
				te.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				tew.push_back(temp);
			}
			break;

		case END_ALIGN:
			if (temp->scalefactor != WRAP) {
				be.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				bew.push_back(temp);
			}
			break;
		default:
			break;
		}
	}
	float resttop = height;
	float restbottom = height;
	float restcenter = height;
	float startycenter = starty;
	float stopycenter = stopy;
	float startytop = starty;
	float stopytop = stopy;
	float startybottom = starty;
	float stopybottom = stopy;


	/*
	 * Compute size of center
	 */

	float sizecenter = 0;
	if (!ce.empty()) {
		int overlapscenter = 0;
		float saved_size_center = 0;

		for (auto v : ce) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapscenter == 0)) {
				saved_size_center = sizecenter;
				sizecenter += v->height;
			}
			else if (v->overlap == OVERLAP &&
				(overlapscenter > 0 && saved_size_center + v->height > sizecenter))
				sizecenter = saved_size_center + v->height;
			if (v->overlap != OVERLAP)
				overlapscenter = 0;
			if (v->overlap == OVERLAP)
				overlapscenter++;
		}
		startycenter = starty + height / 2 - sizecenter / 2;
		stopycenter = starty + height / 2 + sizecenter / 2;
		resttop = height / 2 - sizecenter / 2;
		restbottom = height / 2 - sizecenter / 2;
		startybottom = stopycenter;
	}
	//LOGE("%s size_center: %d starty: %g stopy: %g", name, sizecenter, startycenter,
	//     stopycenter);


	/*
	 * Compute size of top
	 */
	float sizetop = 0;
	if (!te.empty()) {

		int overlapstop = 0;
		float saved_height_top = 0;

		for (auto v : te) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapstop == 0)) {
				saved_height_top = sizetop;
				sizetop += v->height;
			}
			else if (v->overlap == OVERLAP &&
				(overlapstop > 0 && saved_height_top + v->height > sizetop))
				sizetop = saved_height_top + v->height;
			if (v->overlap != OVERLAP)
				overlapstop = 0;
			if (v->overlap == OVERLAP)
				overlapstop++;
		}
		resttop -= sizetop;
		stopytop = sizetop;
	}

	//    LOGE("%s size_top: %d starty: %g stopy: %g", name, sizetop, startytop,
	//         stopytop);


		/*
		 * Compute size of bottom
		 */
	float sizebottom = 0;

	if (!be.empty()) {
		int overlapsbottom = 0;
		float saved_height_bottom = 0;

		for (auto v : be) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsbottom == 0)) {
				saved_height_bottom = sizebottom;
				sizebottom += v->height;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsbottom > 0 && saved_height_bottom + v->height > sizetop))
				sizebottom = saved_height_bottom + v->height;
			if (v->overlap != OVERLAP)
				overlapsbottom = 0;
			if (v->overlap == OVERLAP)
				overlapsbottom++;
		}
		restbottom -= sizebottom;
		startybottom = stopybottom - sizebottom;
	}

	//    LOGE("%s size_bottom: %d starty: %g stopy: %g", name, sizebottom, startybottom,
	//         stopybottom);

	if (ce.empty()) {
		restcenter -= (sizetop + sizebottom);
		resttop -= sizebottom;
		restbottom -= sizetop;
		startycenter += sizetop;
		startybottom += sizetop;
		stopycenter -= sizebottom;
	}
	else {
		float savedstartycenter = startycenter;
		int overlapscenter = 0;

		for (auto v : ce) {
			v->starty = startycenter;
			v->stopy = v->starty + v->height;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapscenter == 0)) {
				savedstartycenter = startycenter;
			}
			else if (v->overlap == OVERLAP &&
				(overlapscenter > 0 && savedstartycenter + v->height > startycenter))
				startycenter = savedstartycenter + v->height;
			if (v->overlap != OVERLAP) {
				overlapscenter = 0;
				startycenter += v->height;
			}
			if (v->overlap == OVERLAP)
				overlapscenter++;
			/*
					  if(DEBUG)
						 LOGE("Hoz ce %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
				  */
		}
	}

	if (!te.empty()) {

		int overlapstop = 0;
		float savedstartytop = startytop;
		for (auto v : te) {
			v->starty = startytop;
			v->stopy = v->starty + v->height;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapstop == 0)) {
				savedstartytop = startytop;
			}
			else if (v->overlap == OVERLAP &&
				(overlapstop > 0 && savedstartytop + v->height > startytop))
				startytop = savedstartytop + v->height;
			if (v->overlap != OVERLAP) {
				overlapstop = 0;
				startytop += v->height;
			}
			if (v->overlap == OVERLAP)
				overlapstop++;
			/*
			 if(DEBUG)
			   LOGE("HOZ te %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
 */
		}
	}

	if (!be.empty()) {
		int overlapsbottom = 0;
		float savedstopybottom = stopybottom;
		for (auto v : be) {
			v->stopy = stopybottom;
			v->starty = v->stopy - v->height;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsbottom == 0)) {
				savedstopybottom = stopybottom;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsbottom > 0 && savedstopybottom - v->height < stopybottom))
				stopybottom = savedstopybottom - v->height;
			if (v->overlap != OVERLAP) {
				overlapsbottom = 0;
				stopybottom -= v->height;
			}
			if (v->overlap == OVERLAP)
				overlapsbottom++;
			/*
					  if(DEBUG)
						  LOGE("HOZ be %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
				  */
		}
	}

	if (!tew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : tew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (resttop > 0)
			size = resttop / (float)num_to_count;
		else size = num_to_count * 2;
		//LOGE("%g %d %g", resttop, height, size);
		for (int i = 0; i < tew.size(); i++) {
			if (i == tew.size() - 1) {
				auto v = tew[i];
				v->starty = startytop;
				//v->height = (int) floor(size);
				v->height = (int)floorf(resttop);
				startytop += v->height;
				v->stopy = startytop;
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = tew[i];
			v->starty = startytop;
			v->height = (int)size;
			v->stopy = v->starty + v->height;
			if (v->overlap == NO_OVERLAP) {
				startytop += size;
				resttop -= size;
			}
			/*    if(DEBUG)
					LOGE("HOZ tw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
			*/
		}
	}

	if (!cew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : cew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (restcenter > 0)
			size = restcenter / (float)num_to_count;
		else size = num_to_count * 2;
		for (int i = 0; i < cew.size(); i++) {

			if (i == cew.size() - 1) {

				auto temp = cew[i];
				temp->starty = startycenter;
				//temp->height = (int) floor(size);
				temp->height = (int)floorf(restcenter);
				temp->stopy = temp->starty + temp->height;
				//LOGE("%d %d %g %d %g", sizecenter, height, size, temp->height, temp->starty);
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = cew[i];
			v->starty = startycenter;
			v->height = (int)size;
			v->stopy = v->starty + v->height;
			if (v->overlap == NO_OVERLAP) {
				startycenter += size;
				restcenter -= size;
			}
			// LOGE("%d %d %g %d %g", sizecenter, height, size, temp->height, temp->starty);
			/* if(DEBUG)
				 LOGE("HOZ cw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		 */
		}
	}

	if (!bew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : bew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (sizebottom > 0)
			size = sizebottom / (float)num_to_count;
		else size = num_to_count * 2;
		//LOGE("%g %d %g", resttop, height, size);
		for (int i = 0; i < bew.size(); i++) {

			if (i == bew.size() - 1) {
				auto temp = bew[i];
				temp->starty = startybottom;
				//temp->height = (int) floor(size);
				temp->height = (int)floor(restbottom);
				startybottom += temp->height;
				temp->stopy = startybottom;
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = bew[i];
			v->starty = startybottom;
			v->height = (int)size;
			v->stopy = v->starty + v->height;
			if (v->overlap == NO_OVERLAP) {
				startybottom += size;
				restbottom -= size;
			}
			/*
			if(DEBUG)
				LOGE("HOZ bw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		*/
		}
	}
	int pos = 0;
	for (auto temp : childs) {
		temp->computePadding();
		temp->pos_p = pos;
		temp->x_pos_p = temp->startx - startx;
		temp->y_pos_p = temp->starty - starty;
		temp->init();
		pos++;

		//LOGE("%s: %s->init H: %d W: %d sax: %d sox %d say: %d soy: %d", name, temp->name, temp->height, temp->width, temp->startx, temp->stopx, temp->starty, temp->stopy);
		/*
		if (DEBUG)
			LOGE("%s: %s->init H: %d W: %d sax: %d sox %d say: %d soy: %d", name,
				 temp->name, temp->height, temp->width, (int) temp->startx,
				 (int) temp->stopx,
				 (int) temp->starty,
				 (int) temp->stopy);*/
	}
}
void Layout::initVertical() {
	if (childs.empty())
		return;
	if (DEBUG)
		LOGE("vert init: %s %d %d %d %d", name, width.load(),
			height.load(), startx.load(), stopx.load());
	std::vector<View*> le, lew, ce, cew, re, rew;

	for (auto temp : childs) {
		temp->_appState = _appState;
		temp->parent = this;
		temp->height = height.load();
		temp->starty = starty.load();
		temp->stopy = stopy.load();
		temp->computeWidth();
		if (DEBUG)
			LOGE("View NORMAL: %s w: %f h: %f sax: %f stopx: %f", temp->name, temp->width.load(),
				temp->height.load(), temp->startx.load(), temp->stopx.load());

		switch (temp->alignment) {

		case CENTER_ALIGN:
			if (temp->scalefactor != WRAP) {
				ce.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				cew.push_back(temp);
			}
			break;

		case START_ALIGN:
			if (temp->scalefactor != WRAP) {
				le.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				lew.push_back(temp);
			}
			break;

		case END_ALIGN:
			if (temp->scalefactor != WRAP) {
				re.push_back(temp);
			}
			else if (temp->scalefactor == WRAP) {
				rew.push_back(temp);
			}
			break;
		default:
			break;
		}
	}
	float restleft = width;
	float restright = width;
	float restcenter = width;
	float startxright = startx;
	float stopxright = stopx;
	float startxcenter = startx;
	float stopxcenter = stopx;
	float startxleft = startx;
	float stopxleft = stopx;


	/*
	 * Compute size of center
	 */
	float sizecenter = 0;
	if (!ce.empty()) {
		int overlapscenter = 0;
		float saved_size_center = 0;

		for (auto v : ce) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapscenter == 0)) {
				saved_size_center = sizecenter;
				sizecenter += v->width;
			}
			else if (v->overlap == OVERLAP &&
				(overlapscenter > 0 && saved_size_center + v->width > sizecenter))
				sizecenter = saved_size_center + v->width;
			if (v->overlap != OVERLAP)
				overlapscenter = 0;
			if (v->overlap == OVERLAP)
				overlapscenter++;
		}
		startxcenter = startx + width / 2 - sizecenter / 2;
		stopxcenter = startx + width / 2 + sizecenter / 2;
		restleft = width / 2 - sizecenter / 2;
		restright = width / 2 - sizecenter / 2;
		stopxleft = startxcenter;
		startxright = stopxcenter;
	}
	//LOGE("%s size_center: %d starty: %g stopy: %g", name, sizecenter, startxcenter,
	//     stopxcenter);


	/*
	 * Compute size of left
	 */
	float sizeleft = 0;
	if (!le.empty()) {

		int overlapsleft = 0;
		float saved_width_left = 0;

		for (auto v : le) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsleft == 0)) {
				saved_width_left = sizeleft;
				sizeleft += v->width;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsleft > 0 && saved_width_left + v->width > sizeleft))
				sizeleft = saved_width_left + v->width;
			if (v->overlap != OVERLAP)
				overlapsleft = 0;
			if (v->overlap == OVERLAP)
				overlapsleft++;
		}
		stopxleft = sizeleft;
		restleft = restleft - sizeleft;
	}

	//LOGE("%s size_top: %d startx: %g stopx: %g", name, sizeleft, startxleft,
	//     stopxleft);


	/*
	 * Compute size of right
	 */
	float sizeright = 0;

	if (!re.empty()) {
		int overlapsright = 0;
		float saved_width_right = 0;

		for (auto v : re) {
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsright == 0)) {
				saved_width_right = sizeright;
				sizeright += v->width;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsright > 0 && saved_width_right + v->width > sizeright))
				sizeright = saved_width_right + v->width;
			if (v->overlap != OVERLAP)
				overlapsright = 0;
			if (v->overlap == OVERLAP)
				overlapsright++;
		}
		startxright = stopxright - sizeright;
		restright = restright - sizeright;
	}

	//LOGE("%s size_right: %d startx: %g stopx: %g", name, sizeright, startxright,
	//     stopxright);

	if (ce.empty()) {
		restcenter = restcenter - sizeleft - sizeright;
		restleft = restleft - sizeright;
		restright = restright - sizeleft;
		startxcenter += sizeleft;
		startxright += sizeleft;
		stopxcenter -= sizeright;
	}
	else {
		float savedstartxcenter = startxcenter;
		int overlapscenter = 0;

		for (auto v : ce) {
			v->startx = startxcenter;
			v->stopx = v->startx + v->width;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapscenter == 0)) {
				savedstartxcenter = startxcenter;
			}
			else if (v->overlap == OVERLAP &&
				(overlapscenter > 0 && savedstartxcenter + v->width > startxcenter))
				startxcenter = savedstartxcenter + v->width;
			if (v->overlap != OVERLAP) {
				overlapscenter = 0;
				startxcenter += v->width;
			}
			if (v->overlap == OVERLAP)
				overlapscenter++;
			//LOGE("ce %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		}
	}

	if (!le.empty()) {

		int overlapsleft = 0;
		float savedstartxleft = startxleft;
		for (auto v : le) {
			v->startx = startxleft;
			v->stopx = v->startx + v->width;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsleft == 0)) {
				savedstartxleft = startxleft;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsleft > 0 && savedstartxleft + v->width > startxleft))
				startxleft = savedstartxleft + v->width;
			if (v->overlap != OVERLAP) {
				overlapsleft = 0;
				startxleft += v->width;
			}
			if (v->overlap == OVERLAP)
				overlapsleft++;

			//LOGE("le %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);

		}
	}

	if (!re.empty()) {
		int overlapsright = 0;
		float savedstopxright = stopxright;
		for (auto v : re) {
			v->stopx = stopxright;
			v->startx = v->stopx - v->width;
			if (v->overlap != OVERLAP || (v->overlap == OVERLAP && overlapsright == 0)) {
				savedstopxright = stopxright;
			}
			else if (v->overlap == OVERLAP &&
				(overlapsright > 0 && savedstopxright - v->width < stopxright))
				stopxright = savedstopxright - v->width;
			if (v->overlap != OVERLAP) {
				overlapsright = 0;
				stopxright -= v->width;
			}
			if (v->overlap == OVERLAP)
				overlapsright++;
			//LOGE("re %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		}
	}

	if (!lew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : lew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (restleft > 0)
			size = restleft / (float)num_to_count;
		else size = num_to_count * 2;
		//LOGE("%g %d %g", resttop, height, size);
		for (int i = 0; i < lew.size(); i++) {
			if (i == lew.size() - 1) {
				auto v = lew[i];
				v->startx = startxleft;
				//v->width = (int) floor(size);
				v->width = (int)floor(restleft);
				startxleft += v->width;
				v->stopx = startxleft;
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = lew[i];
			v->startx = startxleft;
			v->width = (int)size;
			v->stopx = v->startx + v->width;
			if (v->overlap == NO_OVERLAP) {
				startxleft += size;
				restleft -= size;
			}
			//LOGE("lw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		}
	}

	if (!rew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : rew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (sizeright > 0)
			size = sizeright / (float)num_to_count;
		else size = num_to_count * 2;
		//LOGE("%g %d %g", resttop, height, size);
		for (int i = 0; i < rew.size(); i++) {

			if (i == rew.size() - 1) {
				auto temp = rew[i];
				temp->startx = startxright;
				//temp->width = (int) floor(size);
				temp->width = (int)floorf(restright);
				startxright += temp->width;
				temp->stopx = startxright;
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = rew[i];
			v->startx = startxright;
			v->width = (int)size;
			v->stopx = v->startx + v->width;
			if (v->overlap == NO_OVERLAP) {
				startxright += size;
				restright -= size;
			}
			//LOGE("bw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		}
	}

	if (!cew.empty()) {
		int num_to_count = 0;
		int overlappers = 0;
		for (auto v : cew) {
			if (v->overlap == OVERLAP && overlappers == 0) {
				overlappers++;
				num_to_count++;
			}
			else if (v->overlap == NO_OVERLAP)
				num_to_count++;

		}
		float size = 0;
		if (restcenter > 0)
			size = restcenter / (float)num_to_count;
		else size = num_to_count * 2;
		for (int i = 0; i < cew.size(); i++) {
			if (i == cew.size() - 1) {

				auto temp = cew[i];
				temp->startx = startxcenter;
				//temp->width = (int) floor(size);
				temp->width = (int)floorf(restcenter);
				temp->stopx = temp->startx + temp->width;
				//LOGE("%d %d %g %d %g", sizecenter, height, size, temp->height, temp->starty);
				// LOGE("%s %g %g", temp->name, temp->stopy, stopy);
				break;
			}
			auto v = cew[i];
			v->startx = startxcenter;
			v->width = (int)size;
			v->stopx = v->startx + v->width;
			if (v->overlap == NO_OVERLAP) {
				startxcenter += size;
				restcenter -= size;
			}
			//LOGE("cw %s w%d h%d sx%g sox%g sy%g soy%g", v->name, v->width, v->height, v->startx, v->stopx, v->starty, v->stopy);
		}
	}

	//compute final width
	if (scalefactor != WRAP_CONTENT) {
		int final_width = 0;
		bool overlap = false;
		for (auto temp : childs) {
			final_width += !temp->overlap || (temp->overlap && !overlap) ? temp->width.load() : 0;
			if (temp->overlap)
				overlap = true;
		}
		if (final_width > width) {
			std::deque<View*> deque;
			for (auto tmp : childs) {
				if (tmp->alignment == START_ALIGN)
					deque.push_back(tmp);
			}

			for (auto tmp : childs) {
				if (tmp->alignment == CENTER_ALIGN)
					deque.push_back(tmp);
			}

			auto pos = deque.end();

			for (auto tmp : childs) {
				if (tmp->alignment == END_ALIGN)
					deque.insert(pos, tmp);
			}


			float scalefactor = (float)width / (float)final_width;
			int tempstart = startx;
			bool ol = false;
			for (auto it = deque.begin(); it != deque.end(); it++) {
				it.operator*()->startx = tempstart;
				it.operator*()->width = (int)std::roundf(
					(float)it.operator*()->width * scalefactor);
				it.operator*()->padding *= scalefactor;
				it.operator*()->paddingleft *= scalefactor;
				it.operator*()->paddingright *= scalefactor;
				it.operator*()->paddingtop *= scalefactor;
				it.operator*()->paddingbottom *= scalefactor;
				it.operator*()->stopx = it.operator*()->startx + it.operator*()->width;
				if (!ol)
					tempstart += it.operator*()->width;
				ol = it.operator*()->overlap == OVERLAP &&
					((it + 1) == deque.end() || (it + 1).operator*()->overlap == OVERLAP);
			}
		}
	}


	for (int i = 0; i < childs.size(); i++) {
		auto temp = childs.at(i);
		temp->pos_p = i;
		temp->computePadding();
		temp->x_pos_p = temp->startx - startx;
		temp->y_pos_p = temp->starty - starty;
		temp->init();
	}
}
// Body of the queue_callback half. Caller must hold queue_callback.
void View::toForegroundCBDirect() {
	auto& queue = _STATE->queue_callback;
	if (queue._last && queue._last->data != this) {
		queue.del(this);
		queue.add(this, 0);
	}
}

// Body of the queue_draw half. Caller must hold queue_draw.
void View::toForegroundDrawDirect(int windex) {
	auto& d = _STATE->graphics.windows;
	auto it = std::find_if(d.begin(), d.end(),
		[windex](const std::shared_ptr<window>& obj) {
			return obj->index == windex;
		});

	if (it != d.end()) {
		// Move the found shared_ptr to the end
		auto foundObjectPtr = *it; // Copy the shared_ptr
		d.erase(it); // Remove from the current position
		d.push_back(foundObjectPtr); // Insert at the end

	}
}

void View::toForeground(int windex) {
	{
		{
			std::lock_guard lk(_STATE->queue_callback);
			toForegroundCBDirect();
		}

		{
			std::lock_guard lk(_STATE->queue_draw);
			toForegroundDrawDirect(windex);
		}

	}
}
