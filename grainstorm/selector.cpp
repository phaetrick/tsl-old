//
// Created by pr on 13.11.17.
//
#include <cstdlib>
#include <cstring>
#include <string>
#include "logger.h"
#include <include/core/SkFont.h>
#include "selector.h"
#include "defines.h"
#include "colours.h"
#include "tools.h"
#include "Input.h"

using namespace tsl::graphics;

#include <SkPath.h>
#include <string>
#include <keyboard.h>


int32_t Selector::cb(int index) {
	e.value = index;
	e.apply(_STATE, tsl::parameters::FromUi);
	return 0;
}

Selector::Selector(
	tsl::AppState* appState,
	float _scalefactor,
	int32_t _aspect_ratio,
	int32_t _alignment,
	int _numelements,
	std::span<const std::string_view> _names,
	std::span<const float> _values,
	uint16_t _id,
	int64_t _offset,
	int32_t _offsetmulti,
	int (*_cb)(Selector*, int),
	const char* title)
	: View(appState, _scalefactor, _aspect_ratio, _alignment,
		10, false, "SelectorI")
{
	id = _id;
	e.setup(_STATE, 0, id);
	numelements = _numelements;
	names.reserve(_names.size());
	for (auto sv : _names)
		names.emplace_back(sv);  // guarantees null-terminated

	// Copy values (required but may be different size)
	values.assign(_values.begin(), _values.end());

	_STATE->parameters[id].view = this;
	if(_STATE->parameters[id].values.empty())
	_STATE->parameters[id].values = values;
	if (_STATE->parameters[id].names.empty())
		_STATE->parameters[id].names = _names;

	if (_STATE->parameters[id].paramOffset != 0) {
		for (int i = 1; i < _STATE->parameters[_STATE->parameters[id].paramOffset].max; i++) {
			_STATE->parameters[id + i * _STATE->parameters[id].offsetFact].view = this;
		}
	}

	if (title != nullptr) titletext = title;
	else if (_STATE->parameters[id].name != nullptr)
		titletext = _STATE->parameters[id].name;
	else titletext = "Selector";

	
	rv = std::make_unique<
		RecyclerView<SelectorTextView, std::string>
	>(appState, names, "", this);
}

#include "grainstorm.h"
#include <SkCanvas.h>

void WaveformChooser::render(void* context) {

	float line_width = 5;
	float lh = line_width / 2;
	auto env = _DATA->eq[text].win;
	if (env == nullptr) {
		showToast(_STATE, text);
		return;
	}
	const int32_t size = WINDOW_SIZE;

	auto* canvas = (SkCanvas*)context;

	flush(canvas);


	float step_length = (float)size / ((float)width - line_width);
	if (step_length >= size) {
		return;
	}
	SkPath path;
	SkPaint paint;
	paint.setStrokeWidth(line_width);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setColor(skcol::fg);

	float i = lh;
	float env_frame = 0;

	while (i < width) {
		path.moveTo(i, height * (1.f - env[(int)env_frame]));
		env_frame += step_length;
		if (env_frame >= size)
			env_frame -= size;
		if (++i == width - lh)
			break;
		path.lineTo(i, height * (1.f - env[(int)env_frame]));
	}

	canvas->save();
	canvas->translate(startx, starty);
	canvas->drawPath(path, paint);
	canvas->restore();
}

void Selector::setActive(const char* name, bool docallback) {
	if (!docallback)return;
	for (int i = 0; i < numelements; i++) {
		if (!strcmp(names[i].c_str(), name)) {
			e.value = values.empty() ? i : (int)values[i];
			e.apply(_STATE, tsl::parameters::FromUi);
			//	cb(this, values.empty() ? i : (int)values[i]);
		}
	}
}


void Selector::setActiveByIndex(int32_t index, bool docallback) {
	if (index < numelements && docallback) {
		e.value = !values.empty() ? values[index] : index;
		e.apply(_STATE, tsl::parameters::FromUi);
		//	cb(this, values.empty() ? i : (int)values[i]);
		//cb(this, !values.empty() ? values[index] : index);
	}//   _DATA->queue_render->add(_DATA->queue_render, view, view->name, nullptr, view->prio, false);
}

Selector2::Selector2(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio,
	int32_t _alignment, int _numelements, std::span<const std::string_view> _names,
	std::span<const float> _values,
	uint16_t _id, int64_t _offset,
	int32_t _offsetmulti, const char* _title, int (*_callback)(Selector*, int))
	: Selector(appState, _scalefactor, _aspect_ratio, _alignment, _numelements, _names, _values, _id,
		_offset,
		_offsetmulti, _callback, _title),  // <- SWAP: _callback first, then _title
	value{ appState, "SelTitle", CENTER_ALIGN },
	title{ appState, "SelTitle", CENTER_ALIGN }
{
	const char* tit = _title == nullptr ? _STATE->parameters[_id].name : _title;
	if (tit == nullptr)
		tit = "TITLE";
	title.setText(tit);
	innerAlignment = title.textalignhoz = START_ALIGN;
	title.id = id;
	//value.id = id;
}


void Selector2::render(void* context) {
	auto* canvas = (SkCanvas*)context;
	flush(canvas);
	auto tindex = _STATE->active_track.load();
	int32_t offset = 0;
	if (_STATE->parameters[id].paramOffset)
		offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load() * _STATE->parameters[id].offsetFact);

	auto _val = static_cast<int>(_STATE->params[tindex][id + offset].load());

	int index = values.empty() ?  _val : findIndexFloat(values.data(), _val);
	
	// Safety check before indexing into names
	if (index >= 0 && index < names.size()) {
		value.setText(names[index].data(), false);  // Use .data() not .c_str() for string_view
	}

	if (hot) {
		title.setState(HOT, false);
	}
	else {
		title.setState(NORMAL, false);
	}

	title.render(context);
	value.render(context);
}

void Selector2::init() {
	std::vector<std::string> tmp;
	for (int32_t i = 0; i < numelements; i++)
		tmp.emplace_back(names[i]);
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9f);
	float w = measureWidth(font, tmp);
	if (!titletext.empty()) {
		float wtmp = measureWidth(font, titletext.c_str()) + _STATE->textsize2;
		w = w > wtmp ? w : wtmp;
	}
	w += _STATE->textsize2;
	if (w > width)
		w = width;
	if (innerAlignment == CENTER_ALIGN)
		startx = startx + (width - w) * .5f;
	else if (innerAlignment == END_ALIGN) {
		startx = stopx - w;
	}
	width = w;
	stopx = startx + width;



	float sy = starty;
	if (!titletext.empty()) {
		title.startx = startx.load();
		title.width = width.load();
		title.stopx = title.startx + title.width;
		//    title.starty = starty + sy + (h - _STATE->textsize2) * .5f;
		title.starty = sy;
		title.height = _STATE->textsize2;
		title.stopy = title.starty + title.height;
		if (title.stopy > stopy) {
			title.stopy = stopy.load();
			title.height = height.load();
		}
		sy += title.height;
	}

	if (title.height) {
		sy += _STATE->textsize2 * .5f;
		if (sy > stopy)
			sy -= DISTANCE(stopy, sy);
	}

	value.width = width.load();
	value.startx = startx.load();
	value.stopx = value.startx + value.width;
	//    value.starty = starty + sy + (h - _STATE->textsize2) * .5f;
	value.starty = sy;
	value.height = _STATE->textsize2;
	value.stopy = value.starty + value.height;
	if (value.stopy > stopy) {
		value.height -= DISTANCE(value.stopy, stopy);
		value.stopy = value.starty + value.height;
	}
	sy += value.height;
	//sy +=2*lw;
	height = sy - starty;
	stopy = starty + height;
	rv->computeSize();
}

void Selector2::callback(const InputEvent& event) {
	if (disabled)
		return;
	int32_t action = event.action;

	switch (action) {
	case ACTION_DOWN:
		hot = true;
		inputstate.addPointer(event.pointer_id, event.x - startx,
			event.y - starty,pmode_t::WINPOINTER,
			0);
		redraw();
#if defined PLUGIN_MODE
		_STATE->StartParamChange(e);
#endif

		break;
	case ACTION_MOVE: {
		if(auto pt=inputstate.getById(event.pointer_id)) {
			if (pt->distanceTo(event.x - startx, event.y - starty) >
				_STATE->textsize2) {
				inputstate.removePointer(event.pointer_id);
				hot = false;
				redraw();
#if defined PLUGIN_MODE
				_STATE->EndParamChange(e);
#endif
			}
		}
		break;
	}
	case ACTION_UP: {
			if (auto pt = inputstate.getById(event.pointer_id)){
#if defined PLUGIN_MODE
				_STATE->EndParamChange(e);
#endif
				rv->redraw();
				rv->addCB();
				hot = false;
				redraw();
				inputstate.clear();
				return;
			}
		break;
	}


	case ACTION_MOUSE_WHEEL: {
		if (inputstate.empty()) {
			auto tindex = _STATE->active_track.load();
			int32_t offset = 0;
			if (_STATE->parameters[id].paramOffset)
				offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load() * _STATE->parameters[id].offsetFact);

			auto active = static_cast<int>(_STATE->params[tindex][id + offset].load());
			if (!values.empty())
				active = findIndexFloat(values.data(), active);
			active += event.pointer_id;
			if (active < 0)
				active = numelements - 1;
			else if (active >= numelements)
				active = 0;
#if defined PLUGIN_MODE
			_STATE->StartParamChange(e);
#endif
			tsl::parameters::Event ev;
			ev.setup(_STATE, tindex, id + offset);
			ev.value = !values.empty() ? values[active] : active;
			ev.apply(_STATE, tsl::parameters::FromUi);
#if defined PLUGIN_MODE
			_STATE->EndParamChange(e);
#endif
			redraw();
		}
		break;
	}
	default:
		break;
	}

};
