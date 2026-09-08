#include "knob.h"
#include "defines.h"
#include "textview.h"
#include "tools.h"
#include "view.h"
#include "colours.h"
#include "tools/queuetsl.h"
#include "Input.h"
#include "app.h"
#ifdef GRAINSTORM
#include "Follower.h"
#endif
#include <cinttypes>
#include <cstring>
#include <SkPath.h>

//#define MAX(x, y)               ((x) > (y) ? (x) : (y))
//#define MIN(x, y)               ((x) < (y) ? (x) : (y))
//#define ABS(x)                  (((x) < 0) ? -(x) : (x))

#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define NOTTOUCHED 0
#define KnobView 1
#define MINUSView 2
#define PLUSView 3
#define TITLEView 4

#define DEG2PERC (1.f / 360.f)


/**
 * @return The selected quadrant.
 */
static inline int getQuadrant(const float x, const float y) {
	if (x >= 0) {
		return y >= 0 ? 1 : 4;
	}
	else {
		return y >= 0 ? 2 : 3;
	}
}

const float RAD2DEGREE = PI_F_P / 180.f;
const float rad2deg = (180.f / PI_F_P);

inline float
getAngle(const float xTouch, const float yTouch, const float center_x, const float center_y) {
	const float x = xTouch - center_x;
	const float y = center_y - yTouch;
	const float length = hypot(x, y);
	if (length == 0)
		return 0;
	switch (getQuadrant(x, y)) {
	case 1:
		return asinf(y / length) * 180.f / PI_F_P;
	case 2:
		return 180 - asinf(y / length) * 180.f / PI_F_P;
	case 3:
		return 180 + (-1 * asinf(y / length) * 180 / PI_F_P);
	case 4:
		return 360 + asinf(y / length) * 180 / PI_F_P;
	default:
		return 0;
	}
}


static inline float
angle(const float x, const float y, const float center_x, const float center_y) {
	//LOGE("%g %g %g %g", x, y, center_x, center_y);
	float res = atan2(center_y - y, center_x - x) * rad2deg;
	return (res < 0) ? res + 360.f : res;
}

using namespace tsl::graphics;

// No provider by default: a host that never installs one gets exactly the old
// rendering, with a single null check per knob per frame.
Knob::ModRangeFn Knob::modRangeProvider = nullptr;

Knob::Knob(tsl::AppState* appState, const float _scalefactor,
	const int _aspect_ratio,
	const int _alignment, int _target, long _offset, int _offsetmulti, Layout* _parent)
	: View(appState,
		_scalefactor, _aspect_ratio, _alignment, 10, false), valueView(appState, *this), title{ appState,
	nullptr, START_ALIGN }, plus{ appState, "+" }, minus{ appState,  "-" }, surface(appState) {
	id = _target;
	e.setup(_STATE, 0, id);

	_STATE->parameters[id].view = this;
	surface.id = id;
	surface.knob = this;
	title.id = id;
	title.setText(_STATE->parameters[id].name
		? _STATE->parameters[id].name : "Title");
	if (_parent)
		_parent->addChild(this);
#ifdef POCKETANALOG
	padding = 5.;
#endif
	auto offset = _STATE->parameters[id].paramOffset;
	if (offset != 0) {
		auto fact = _STATE->parameters[id].offsetFact;
		for (int i = 1; i < _STATE->parameters[offset].max; i++) {
			_STATE->parameters[id + i * fact].view = this;
		}
	}
	
}

void Knob::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();

	e.setup(_STATE, tindex, id);
	View::addRecursiveDraw();
}

void Knob::delRecursiveDraw() {
	auto& queue = _STATE->queue_draw;
	valueView.delRecursiveDraw();
	queue.del(&minus);
	queue.del(&plus);
	queue.del(&surface);
	queue.del(&title);
	queue.del(this);
	plus.setState(NORMAL, false);
	minus.setState(NORMAL, false);
	title.setState(NORMAL, false);
	visible_ = false;
};

void Knob::delRecursiveCB() {
	View::delRecursiveCB();
	pointers.clear();
};

void KnobSurface::render(void* context) {
	auto miditargetid = knob->e.getDisplayParam();
	auto& miditarget = _STATE->parameters[miditargetid];
	auto* canvas = (SkCanvas*)context;
	flush(canvas);
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	canvas->save();
	canvas->translate(startx, starty);

	const auto currentValue = knob->e.getCurrentValue(_STATE);
	float percentage =
		DISTANCEF(currentValue, miditarget.min) / DISTANCEF(miditarget.min, miditarget.max);

	float startx_rect = center_x - radius;
	float starty_rect = center_y - radius;

	// The value fill's two ENDS come from Param::fillRange: the min end of the ring
	// for an ordinary parameter, the zero position for a CentreFill one, so a
	// bipolar control sitting at its neutral centre draws an empty ring rather than
	// a half-full one. The pointer further down is unaffected — it marks where the
	// value IS, which has nothing to do with where the fill starts.
	float fillLo, fillHi;
	miditarget.fillRange(currentValue, fillLo, fillHi);

	paint.setColor(skcol::blue);
	if (fillHi > fillLo)
		canvas->drawArc(SkRect::MakeXYWH(startx_rect, starty_rect, radius * 2, radius * 2),
			-225 + fillLo * 270, (fillHi - fillLo) * 270, false, paint);

	paint.setColor(skcol::fg);
	if (fillLo > 0.f)
		canvas->drawArc(SkRect::MakeXYWH(startx_rect, starty_rect, radius * 2, radius * 2),
			-225, fillLo * 270, false, paint);
	canvas->drawArc(SkRect::MakeXYWH(startx_rect, starty_rect, radius * 2, radius * 2),
		-225 + fillHi * 270, 270 - fillHi * 270, false, paint);

	// Modulation range — the stretch of the ring that modulation can move the value
	// across, recoloured in place. The sweep is signed, so an attenuating route
	// paints backwards over the blue fill and a pushing one paints forwards over the
	// grey remainder, with no branch here; this code never learns which convention a
	// parameter uses, only where the travel ends.
	//
	// Recolouring the ring rather than adding a second concentric arc is not a
	// stylistic choice: radius is (min(w,h) - lw) / 2, so the ring's outer edge is
	// already flush with the surface and the arrow tip sits 2*lw2 inside it. There
	// is no room on either side for a separate ring that would not either clip or
	// collide with the pointer.
	if (Knob::modRangeProvider) {
		float lo, hi;
		if (Knob::modRangeProvider(_appState, miditargetid, lo, hi)) {
			const float span = miditarget.max - miditarget.min;
			if (span != 0.f) {
				auto pct = [&](float v) {
					float p = ((MYFLOAT)v - miditarget.min) / span;
					return p < 0.f ? 0.f : (p > 1.f ? 1.f : p);
				};
				const float loPct = pct(lo), hiPct = pct(hi);
				const float sweep = (hiPct - loPct) * 270.f;
				// Below about a third of a degree the segment is shorter than the
				// stroke is wide and renders as a smudge, so drop it entirely.
				if (sweep > .35f) {
					paint.setColor(skcol::modarc);
					canvas->drawArc(SkRect::MakeXYWH(startx_rect, starty_rect, radius * 2, radius * 2),
						-225.f + loPct * 270.f, sweep, false, paint);
				}
			}
		}
	}

	paint.setColor(skcol::fg);
	float pos = (-225.f + 270.f * percentage) * RAD2DEGREE;
	float end_x = center_x + cos(pos) * radius_arrow;
	float end_y = center_y + sin(pos) * radius_arrow;

	float arrow_length = radius_arrow / 5 + lw;
	float arrow_degrees = 5.f * RAD2DEGREE;

	float x1 = center_x + cos(pos - arrow_degrees) * (radius_arrow - arrow_length);
	float y1 = center_y + sin(pos - arrow_degrees) * (radius_arrow - arrow_length);
	float x2 = center_x + cos(pos + arrow_degrees) * (radius_arrow - arrow_length);
	float y2 = center_y + sin(pos + arrow_degrees) * (radius_arrow - arrow_length);

	SkPath path;


	path.moveTo(x1, y1);
	path.lineTo(end_x, end_y);
	path.moveTo(end_x, end_y);
	path.lineTo(x2, y2);
	path.moveTo(x2, y2);
	path.lineTo(x1, y1);

	canvas->drawPath(path, paint);
	canvas->restore();
}

void Knob::init() {
	int height_knob = height;
	surface.viewport = viewport;
	surface.startx = startx.load();
	surface.width = width.load();
	surface.stopx = surface.startx + surface.width;
	surface.starty = starty.load();
	surface.height = height.load();
	surface.stopy = surface.starty + surface.height;

	if (drawTitle) {
		int titleheight = _STATE->textsize2;
		height_knob -= titleheight;
		title.viewport = viewport;
		title.starty = starty.load();
		title.height = titleheight;
		title.stopy = title.starty + title.height;
		title.width = width.load();
		title.startx = startx.load();
		title.stopx = title.startx + title.width;
		surface.height -= titleheight;
		surface.starty += titleheight;
	}

	if (drawButtons) {
		auto buttonheight = _STATE->textsize1;
		height_knob -= buttonheight;
		surface.height -= buttonheight;
		surface.stopy -= buttonheight;

		minus.viewport = viewport;
		minus.height = buttonheight;
		minus.width = minus.height.load();
		if (minus.width > width * .5f)
			minus.width = width * .5f;
		minus.startx = startx + width * .5f - minus.width * 1.5f;
		if (minus.startx < startx)
			minus.startx = startx.load();
		minus.stopx = minus.startx + minus.width;
		minus.starty = starty + height - buttonheight;
		minus.stopy = minus.starty + minus.height;
		minus.padding = .9f;
		minus.computePadding();

		plus.viewport = viewport;
		plus.width = minus.width.load();
		plus.height = minus.height.load();
		plus.startx = startx + width * .5f + plus.width * .5f;
		if (plus.startx + plus.width >= stopx)
			plus.startx = stopx - plus.width;
		plus.stopx = plus.startx + plus.width;
		plus.starty = starty + height - buttonheight;
		plus.stopy = plus.starty + plus.height;
		plus.padding = .9f;
		plus.computePadding();
	}

	float w = surface.width, h = surface.height;
	float radius = (MIN(w, h) - lw) * .5f;
	float radius_arrow = radius - lw2 - lw2;


	float center_x = w * .5f;
	float center_y = radius * 2 >= h ? h * .5f : h * .5f + radius * 0.146446609f;

	surface.center_x = center_x, surface.center_y = center_y, surface.radius = radius, surface.radius_arrow = radius_arrow;
	knob_center_x = surface.startx + center_x;
	knob_center_y = surface.starty + center_y;
}

void Knob::callback(const InputEvent& event) {
	
	auto& miditarget = _STATE->parameters[e.getDisplayParam()];

	const int32_t pointerid = event.pointer_id;
	int action = event.action;
	float xpos = event.x;
	float ypos = event.y;

	switch (action) {

	case ACTION_DOWN:
		if (drawTitle && ypos < title.stopy) {
			title.setState(HOT);
			pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, TITLEView);
			e.value = e.getCurrentValue(_STATE);
		}
		else if (drawButtons && ypos >= surface.stopy) {
			auto old = e.value = e.getCurrentValue(_STATE);
			if (xpos < minus.stopx) {
				if (old > miditarget.min) {
					minus.setState(HOT);
					pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, MINUSView);
					valueView.addDraw();
				}
			}
			else {
				if (old < miditarget.max) {
					plus.setState(HOT);
					pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, PLUSView);
					valueView.addDraw();
				}
			}
		}
		else if (ypos >= surface.starty) {
			e.value = e.getCurrentValue(_STATE);
			//double theta = getTheta(Knob->radius, xpos, ypos, Knob->Knob_center_x, Knob->Knob_center_y);
			old_quadrant = getQuadrant(xpos - knob_center_x,
				knob_center_y - ypos);
			theta_old = getAngle(xpos, ypos, knob_center_x,
				knob_center_y);
			pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, KnobView);
			valueView.addDraw();
#ifdef PLUGIN_MODE
				_STATE->StartParamChange(e);
#endif
		}

		break;

	case ACTION_MOVE: {
		if (auto pt = pointers.getById(pointerid)) {
			if (pt->target == KnobView) {
				MYFLOAT value;
#ifdef PLATFORM_DESKTOP
				value = e.getCurrentValue(_STATE) + (pt->ypos - ypos) / _STATE->windowHeight * DISTANCEF(miditarget.min, miditarget.max);
				pt->ypos = ypos;
#else
				float currentAngle = getAngle(xpos, ypos, knob_center_x,
					knob_center_y);
				int quadrant = getQuadrant(xpos - knob_center_x,
					knob_center_y - ypos);
				if ((quadrant == 1 && old_quadrant == 4) ||
					(quadrant == 4 && old_quadrant == 1)) {
					value = e.getCurrentValue(_STATE);
				}
				else
					value = e.getCurrentValue(_STATE) +
					((theta_old - currentAngle) * DEG2PERC) *
					DISTANCEF(miditarget.min, miditarget.max);
				//LOGE("%d %d %g %g %g", quadrant, Knob->old_quadrant, currentAngle, Knob->theta_old - currentAngle, (Knob->theta_old - currentAngle) * DEG2PERC);

				theta_old = currentAngle;
				old_quadrant = quadrant;
#endif
				if (value < miditarget.min)
					value = miditarget.min;
				if (value > miditarget.max)
					value = miditarget.max;
				e.value = value;
				
				auto old = e.apply(_STATE, tsl::parameters::FromUi);
				if (old.value != value)
				{
					valueView.addDraw();
					surface.redraw();

				}
			}
			else if (pt->target == PLUSView &&
				pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
				plus.setState(NORMAL);
				pointers.removePointer(pointerid);

			}
			else if (pt->target == MINUSView &&
				pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
				minus.setState(NORMAL);
				pointers.removePointer(pointerid);
			}
			else if (pt->target == TITLEView &&
				pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
				title.setState(NORMAL);
				pointers.removePointer(pointerid);
			}
		}
		break;
	}
	case ACTION_UP: {
		if (auto pt = pointers.getById(pointerid)) {
			if (pt->target == TITLEView) {
				title.setState(NORMAL);
				_STATE->UiTasksQueue.add_task([this] () mutable {EnterValue::Task(_appState, e); });
			}
			else if (pt->target == MINUSView) {
				minus.setState(NORMAL);
				auto value = e.getCurrentValue(_STATE);
				if (value > miditarget.min) {
					value -= miditarget.progress;
					if (value < miditarget.min)
						value = miditarget.min;
					e.value = value;
					auto ret = e.apply(_STATE, tsl::parameters::FromUi);
					if (ret.value != value) {
						surface.redraw();
						valueView.addDraw();
					}
				}
			}
			else if (pt->target == PLUSView) {
				plus.setState(NORMAL);
				auto value = e.getCurrentValue(_STATE);
				if (value < miditarget.max) {
					value += miditarget.progress;
					if (value > miditarget.max)
						value = miditarget.max;
					e.value = value;
					auto ret = e.apply(_STATE, tsl::parameters::FromUi);
					if (ret.value != value) {
						surface.redraw();
						valueView.addDraw();
					}
				}
			}
			else if (pt->target == KnobView) {
				auto elapsed = pointers.timer.elapsedReplace();
				if (elapsed < .3) {
					e.value = miditarget.initvalue;
					auto old = e.apply(_STATE, tsl::parameters::FromUi);
					if (old.value != miditarget.initvalue) {
						surface.redraw();
					}
				}
#ifdef PLUGIN_MODE
					_STATE->EndParamChange(e);
#endif
				pointers.removePointer(pointerid);
			}
		}
		break;
	}
	default:
		break;
	}
}
	void Knob::render(void* context) {
#ifdef GRAINSTORM
		if (id == LFO1BOUNDA || id == LFO1BOUNDB) {
			auto tindex = _STATE->active_track.load();
			int control_num = _STATE->params[tindex][LFO1DEST + GASLFO * LFONUMPARAMS].load();
			if (control_num == ROTPOS || control_num == PHASER3CENTER)
				return;
		}
#endif
		if (drawTitle != 0)
			title.render(context);

		if (drawButtons != 0) {
			plus.render(context);
			minus.render(context);
		}
		surface.render(context);
	}
