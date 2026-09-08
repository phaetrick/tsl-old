#include "textview.h"
#include "defines.h"
#include "tools.h"
#include "tools/queuetsl.h"
#include "Input.h"
#include "app.h"
#include <cstdlib>
#include <cstring>
#include <include/core/SkFont.h>
#include "PlusMinusControlBase.h"

using namespace tsl::graphics;

PlusMinusControl::PlusMinusControl(tsl::AppState* state, float _scalefactor,
	int _aspect_ratio, int _alignment, int _id, const char* _title)
	: View(state, _scalefactor, _aspect_ratio, _alignment, 10), view_title{ state,
nullptr, START_ALIGN
	}, plus{ state, "+" }
	, minus{ state, "-" }
{
	id = _id;
	title = _title;
	view_title.id = _id;
	const char* name =
		_id != PARAM_NOT_ASSIGNED ? _STATE->parameters[_id].name : (title !=
			nullptr
			? title
			: "no name");
	view_title.setText(name);
	id = _id;
	_STATE->parameters[id].view = this;
	e.setup(_STATE, 0, id);
	// plus.id = _id;
	// minus.id = _id;
}


PlusMinusControl::PlusMinusControl(tsl::AppState* appState, float _scalefactor,
	int _aspect_ratio, int _alignment, int _id, int _offset,
	int _multi, const char* _title) : View(appState, _scalefactor,
		_aspect_ratio, _alignment,
		10), view_title{ appState, nullptr, START_ALIGN }, plus{ appState, "+" }, minus{ appState, "-" }
{
	id = _id;
	_STATE->parameters[id].view = this;

	auto offset = _STATE->parameters[id].paramOffset;
	if (offset > 0) {
		auto fact = _STATE->parameters[id].offsetFact;
		for (int i = 1; i < _STATE->parameters[offset].max; i++) {
			_STATE->parameters[id + i * fact].view = this;
		}
	}
	title = _title;
	view_title.id = _id;
	const char* name =
		_id != PARAM_NOT_ASSIGNED ? _STATE->parameters[_id].name : (title !=
			nullptr
			? title
			: "no name");
	view_title.setText(name);
	e.setup(_STATE, 0, id);

	//  plus.id = _id;
	//  minus.id = _id;
}


void PlusMinusControl::delRecursiveDraw() {
	auto& queue = _STATE->queue_draw;
	queue.del(&minus);
	queue.del(&plus);
	queue.del(&view_title);
	queue.del(this);
	plus.setState(NORMAL, false);
	minus.setState(NORMAL, false);
	view_title.setState(NORMAL, false);
	visible_ = false;
};

void PlusMinusControl::delRecursiveCB() {
	View::delRecursiveCB();
	pointers.clear();
};


void PlusMinusControl::init() {
	const char* name = id != PARAM_NOT_ASSIGNED ? _STATE->parameters[id].name : (title !=
		nullptr
		? title
		: "no name");
	view_title.viewport = viewport;
	view_title.starty = starty.load();
	view_title.height = _STATE->textsize2;
	view_title.stopy = view_title.starty + view_title.height;
	view_title.width = width.load();
	view_title.startx = startx.load();
	view_title.stopx = view_title.startx + view_title.width;

	//pm->view_title->init(pm->view_title);

	minus.viewport = viewport;
	minus.height = (int)(_STATE->textsize1);
	minus.width = minus.height.load();
	if (minus.width > width * .5f)
		minus.width = width * .5f;
	minus.startx = startx + width * .5f - minus.width * 1.5f;
	if (minus.startx < startx)
		minus.startx = startx.load();
	minus.stopx = minus.startx + minus.width;
	minus.starty = starty + height - minus.height;
	minus.stopy = minus.starty + minus.height;
	minus.padding = .9f;
	minus.computePadding();

	plus.viewport = viewport;
	plus.height = minus.height.load();
	plus.width = minus.width.load();
	plus.startx = startx + width * .5f + plus.width * .5f;
	if (plus.startx + plus.width >= stopx)
		plus.startx = stopx - plus.width;
	plus.stopx = plus.startx + plus.width;
	plus.starty = starty + height - plus.height;
	plus.stopy = plus.starty + plus.height;
	plus.padding = .9;
	plus.computePadding();
}


void PlusMinusControl::callback(const InputEvent& event) {
	int action = event.action;
	float xpos = event.x - startx;
	float ypos = event.y - starty;

	auto& miditarget = _STATE->parameters[e.getDisplayParam()];

	int32_t pointerid = event.pointer_id;

	switch (action) {
	case ACTION_DOWN:
		if (ypos < view_title.height) {
			which = TITLEView;
			view_title.setState(HOT);
			pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, TITLEView);

		}
		else if (ypos < height - plus.height)
			pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, VALUEView);
		else {
			auto old = e.getCurrentValue(_STATE);
			if (xpos >= width * .5f) {
				if (old < miditarget.max) {
					plus.setState(HOT);
					pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, PLUSView);
				}
			}
			else {
				if (old > miditarget.min) {
					minus.setState(HOT);
					pointers.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, MINUSView);
				}
			}
		}
		break;

	case ACTION_MOVE: {
		auto pt = pointers.getById(pointerid);
		if (pt && pt->distanceTo(xpos, ypos) > _STATE->textsize2) {
			if (pt->target == PLUSView)
				plus.setState(NORMAL);
			else if (pt->target == MINUSView)
				minus.setState(NORMAL);
			else if (pt->target == TITLEView)
				view_title.setState(NORMAL);

			pointers.removePointer(pointerid);

		}
		break;
	}

	case ACTION_UP: {
		auto onChange = [this](MYFLOAT value) {
			e.value = value;
			e.apply(_STATE, tsl::parameters::FromUi);
		};

		if (auto pt = pointers.getById(pointerid)) {
			if (pt->target == TITLEView) {
				view_title.setState(NORMAL, false);
				_STATE->UiTasksQueue.add_task([this] () mutable{
					EnterValue::Task(_appState, e); });
			}
			else if (pt->target == VALUEView) {
				if (pointers.timer.elapsedReplace() < .3) {
					onChange(miditarget.initvalue);
				}
			}
			else if (pt->target == MINUSView) {
				minus.setState(NORMAL, false);
				auto value = e.getCurrentValue(_STATE) - miditarget.progress;
				if (value < miditarget.min)
					value = miditarget.min;
				onChange(value);
			}

			else if (pt->target == PLUSView) {
				plus.setState(NORMAL, false);
				auto value = e.getCurrentValue(_STATE) + miditarget.progress;
				if (value > miditarget.max)
					value = miditarget.max;
				onChange(value);
			}
			pointers.removePointer(pointerid);
			redraw();
		}
		break;
	}
	default:
		break;
	}
}

void PlusMinusControl::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	e.setup(_STATE, tindex, id);
	View::addRecursiveDraw();
}

void PlusMinusControl::render(void* context) {
	auto canvas = (SkCanvas*)context;
	flush(canvas);
	view_title.render(context);
	plus.render(context);
	minus.render(context);
	float value = e.getCurrentValue(_STATE);//view->_STATE->parameters[view->id].reference[view->_STATE->active_track.load()->index]->load();
	
	char output[20]{};

	int decimals = _STATE->parameters[e.getDisplayParam()].digits;
	if (decimals == 0)
		snprintf(output, 20, "%.0f", value);
	else if (decimals == 1)
		snprintf(output, 20, "%.1f", value);
	else if (decimals == 2)
		snprintf(output, 20, "%.2f", value);
	else
		snprintf(output, 20, "%.3f", value);


	float height_rect = (height - view_title.height -
		plus.height);

	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	paint.setColor(skcol::fg);
	float startxtext, startytext;
	measureTextFixed(width, height_rect, _STATE->font_normal, output, &startxtext, &startytext,
		_STATE->textsize2);
	//measureText2(view->width, height_rect, view->DATA::font_normal, output, .33f, &startx, &starty,
	//             &fontsize);
	SkFont font(_STATE->font_normal);

	font.setSize(_STATE->textsize2);
	canvas->save();
	canvas->translate(startx, starty + view_title.height);
	canvas->drawSimpleText(output, strlen(output), SkTextEncoding::kUTF8, startxtext, startytext,
		font,
		paint);
	canvas->restore();
}
