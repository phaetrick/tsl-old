//
// Created by pr on 19.09.23.
//

#include "ButtonBase.h"
#include "Input.h"
#include "app.h"
#include "logger.h"
#include "app.h"

using namespace tsl::graphics;
bool Button::check(const InputEvent& e) {
	if (pointerid == e.pointer_id && spacing(xpos, e.x, ypos, e.y) > _STATE->textsize2) {
		pointerid = -1;
		setState(NORMAL);
		return true;
	}
	else return false;
}

void Button::delRecursiveDraw() {
	auto& queue = _STATE->queue_draw;
	queue.del(this);
	state = NORMAL;
	visible_ = false;
};

void Button::delRecursiveCB() {
	View::delRecursiveCB();
	pointerid = -1;
};



void Button::setStateRedrawParent(int _state, bool _redraw) {
	state = _state;
	if (_redraw) {
		parent->addDraw();
	}
}


void Button::setState(int _state, bool _redraw) {
	state = _state;
	if (_redraw)
		redraw();
}

void TextButtonFramed::render(void* context) {
	float startx_pos, starty_pos, fontsize;

	const char* text =
		name_normal;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	flush(canvas);
	auto w = measureTextFixed(width, height, _STATE->font_normal, text, &startx_pos, &starty_pos,
		_STATE->textsize2 * .9f);
	font.setSize(_STATE->textsize2 * .9f);
	canvas->save();
	canvas->translate(startx, starty);

	tsl::parameters::Event ev;
	auto tindex = _STATE->active_track.load();
	ev.setup(_STATE, tindex, id);
	auto active = ev.getCurrentValue(_STATE) == userdata;

	if (active || state == HOT) {
		w += _STATE->textsize2 * 1.8;
		if (w >= width)
			w = width - 1;
		float width_rect = w - lw;
		float height_rect = height - lw;
		float radius = height / 10.f;
		float startx_rect = (width - width_rect) * .5f + lw / 2;
		float starty_rect = lw / 2;
		paint.setStyle(SkPaint::kFill_Style);
		paint.setColor(skcol::blue_transparent);
		if (drawRoundedRect)
			canvas->drawRoundRect(
				SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
				radius, radius, paint);
		else
			canvas->drawRect(SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect), paint);
	}
#ifdef HAS_MIDI
	const bool ml = (_STATE->parameters[id].flags & Param::MidiParam) && _STATE->midilearning.load();
#else
	// AppState::midilearning only exists behind HAS_MIDI. Apps with no MIDI
	// (Generative) still draw buttons, so the highlight folds to a constant
	// false and the compiler drops the branch.
	constexpr bool ml = false;
#endif
	paint.setColor(ml ? skcol::orange :skcol::fg);
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx_pos,
		starty_pos,
		font, paint);

	canvas->restore();
}

void TextButtonFramed::computeWidth() {
	if (name_normal == nullptr)
		width = 0;
	else {
		int ts = _STATE->textsize2;
		SkFont font(_STATE->font_normal);
		font.setSize(ts);
		SkRect bounds{};
		int length = strlen(name_normal);
		font.measureText(name_normal, length, SkTextEncoding::kUTF8, &bounds);
		width = bounds.width() + ts;
	}
}

void TextButton::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();
	int action = event.action;
	tsl::parameters::Event ev;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
#ifdef PLUGIN_MODE
		ev.setup(_STATE, tindex, id);
		_STATE->StartParamChange(ev);
#endif

		break;

	case ACTION_MOVE:
		if (check(event)) {
#ifdef PLUGIN_MODE
			ev.setup(_STATE, tindex, id);
			_STATE->EndParamChange(ev);
#endif
			;
		}
		;
		break;

	case ACTION_UP:
		if (pointerid == event.pointer_id) {
			pointerid = -1;
			setState(NORMAL);
#ifdef PLUGIN_MODE
			ev.setup(_STATE, tindex, id);
			_STATE->EndParamChange(ev);
#endif
			if (id != PARAM_NOT_ASSIGNED) {
				int offset = 0;
				if (_STATE->parameters[id].paramOffset)
					offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
				offset = offset * _STATE->parameters[id].offsetFact;
				auto des = _STATE->params[tindex][id + offset].load() == 1.0 ? 0.0 : 1.0;
				auto e = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, id + offset, des);
				e.applyFromExt(_STATE, tsl::parameters::FromUi);
			}

		}
		break;
	default:
		break;
	}

}


// ── TextToggle ───────────────────────────────────────────────────────────────

TextToggle::TextToggle(tsl::AppState* appState, const char *text, int32_t _id)
	: TextButtonFramed(appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, text, text) {
	size_reference = &_STATE->textsize1;
	padding = 10.f;
	id = _id;
	if (id != PARAM_NOT_ASSIGNED) {
		_STATE->parameters[id].view = this;

		if (_STATE->parameters[id].paramOffset) {
			for (int i = 1; i < _STATE->parameters[_STATE->parameters[id].paramOffset].max; i++) {
				_STATE->parameters[id + i * _STATE->parameters[id].offsetFact].view = this;
			}
		}
	}
}

void TextToggle::callback(const InputEvent& event) {
	int action = event.action;
#ifdef GRAINSTORM
	int tindex = _STATE->active_track.load();
#else
	int tindex = 0;
#endif
	tsl::parameters::Event ev;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
#ifdef PLUGIN_MODE
		ev.setup(_STATE, tindex, id);
		_STATE->StartParamChange(ev);
#endif
		break;

	case ACTION_MOVE:
		if (check(event)) {
#ifdef PLUGIN_MODE
			ev.setup(_STATE, tindex, id);
			_STATE->EndParamChange(ev);
#endif
		}
		break;

	case ACTION_UP:
		if (pointerid == event.pointer_id) {
			pointerid = -1;
			setState(NORMAL, true);
			ev.setup(_STATE, tindex, id);

#ifdef PLUGIN_MODE
			_STATE->EndParamChange(ev);
#endif
			if (id != PARAM_NOT_ASSIGNED) {
				// setup() tags the event type per param (plain toggle, or a special
				// action like power/record/midi-learn), so apply() routes it correctly.
				ev.value = ev.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
				ev.applyFromExt(_STATE, tsl::parameters::FromUi);
			}
		}
		break;
	default:
		break;
	}
}

void TextToggle::render(void* context) {
	float startx_pos, starty_pos, fontsize;
	const char* text = name_normal;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	auto w = measureTextFixed(width, height, _STATE->font_normal, text, &startx_pos, &starty_pos,
		_STATE->textsize2 * .9f);
	if (w > width - _STATE->maxCharWidtht2)
		startx_pos = _STATE->maxCharWidtht2 * .5;
	font.setSize(_STATE->textsize2 * .9f);

#ifdef GRAINSTORM
	int tindex = _STATE->active_track.load();
#else
	int tindex = 0;
#endif
	int offset = 0;
	if (_STATE->parameters[id].paramOffset)
		offset = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
	offset = offset * _STATE->parameters[id].offsetFact;
	bool active = _STATE->params[tindex][id + offset].load() == 1.0;
	flush(canvas);

	if (active || state == HOT)
		flush(canvas, skcol::blue_transparent);

	canvas->save();
	canvas->translate(startx, starty);
#ifdef HAS_MIDI
	const bool ml = (_STATE->parameters[id].flags & Param::MidiParam) && _STATE->midilearning.load();
#else
	// AppState::midilearning only exists behind HAS_MIDI. Apps with no MIDI
	// (Generative) still draw buttons, so the highlight folds to a constant
	// false and the compiler drops the branch.
	constexpr bool ml = false;
#endif
	paint.setColor(ml || midiHighlight ? skcol::midilearning : skcol::fg);
	canvas->clipRect(SkRect::MakeXYWH(_STATE->maxCharWidtht2 * .5, 0, width - lw, height));
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
		innerAlignment == START_ALIGN ? _STATE->maxCharWidtht2 * .5 : startx_pos, starty_pos,
		font, paint);
	canvas->restore();
}

void TextToggle::computeWidth() {
	if (name_normal == nullptr)
		width = 0;
	else {
		int ts = _STATE->textsize2;
		SkFont font(_STATE->font_normal);
		font.setSize(ts);
		SkRect bounds{};
		int length = strlen(name_normal);
		font.measureText(name_normal, length, SkTextEncoding::kUTF8, &bounds);
		if (bounds.width() + ts > width)
			width = bounds.width() + ts;
	}
}


void TextButton::render(void* context) {
	float startx_pos, starty_pos, fontsize;

	const char* text =
		name_normal;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	flush(canvas);
	measureTextFixed(width, height, _STATE->font_normal, text, &startx_pos, &starty_pos,
		_STATE->textsize2 * .9f);
	font.setSize(_STATE->textsize2 * .9f);
	canvas->save();
	canvas->translate(startx, starty);
	tsl::parameters::Event ev;
	auto tindex = _STATE->active_track.load();
	ev.setup(_STATE, tindex, id);
	auto active = ev.getCurrentValue(_STATE) == 1.0;
    
	if (active || state == HOT) {
		float width_rect = width - lw;
		float height_rect = height - lw;
		float radius = height / 10.f;
		float startx_rect = lw / 2;
		float starty_rect = lw / 2;
		paint.setColor(skcol::bghot);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
			radius, radius, paint);

	}
#ifdef HAS_MIDI
	const bool ml = (_STATE->parameters[id].flags & Param::MidiParam) && _STATE->midilearning.load();
#else
	// AppState::midilearning only exists behind HAS_MIDI. Apps with no MIDI
	// (Generative) still draw buttons, so the highlight folds to a constant
	// false and the compiler drops the branch.
	constexpr bool ml = false;
#endif
	if (ml)
		paint.setColor(skcol::midilearning);
	else
	paint.setColor(active || state == HOT ? skcol::fghot : skcol::fg);
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx_pos,
		starty_pos,
		font, paint);

	canvas->restore();
}

void IconButton::render(void* context) {

	const char* text = name_normal;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);
	float fontsize = textpadding ? height - height * textpadding * .01f
		: height.load();

	font.setSize(fontsize);
	SkRect bounds;
	float textwidth = font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	SkScalar xpos = SkFloatToScalar(startx + width * .5f - bounds.centerX());
	SkScalar ypos = SkFloatToScalar(starty + height * .5f - bounds.centerY());
#if defined HAS_MIDI
	const bool midilearning = _STATE->midilearning && id != PARAM_NOT_ASSIGNED &&
		(_STATE->parameters[id].flags & Param::MidiParam);
#endif
	flush(canvas);

	const bool hot = state.load() == HOT;

	if (hot) {
		paint.setColor(skcol::bghot);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + width * .5f - height * .5f,
				starty, height, height), height * .1f,
			height * .1f, paint);
	}
	paint.setColor(
#if defined HAS_MIDI
		midilearning ? skcol::midilearning :
#endif

		(hot ? skcol::fghot : skcol::fg));
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font, paint);
}


void PlusMinusButton::render(void* context) {

	const char* text = name_normal;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	float fontsize = textpadding ? height - height * textpadding * .01f
		: height.load();

	font.setSize(fontsize);
	SkRect bounds;
	float textwidth = font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	SkScalar xpos = SkFloatToScalar(startx + width * .5f - bounds.centerX());
	SkScalar ypos = SkFloatToScalar(starty + height * .5f - bounds.centerY());
#if defined HAS_MIDI
	const bool midilearning = _STATE->midilearning && id != PARAM_NOT_ASSIGNED &&
		(_STATE->parameters[id].flags & Param::MidiParam);
#endif
	flush(canvas);

	const bool hot = state.load() == HOT;

	if (hot) {
		paint.setColor(skcol::bghot);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + width * .5f - height * .5f,
				starty, height, height), height * .1f,
			height * .1f, paint);
	}
	paint.setColor(
#if defined HAS_MIDI       
		midilearning ? skcol::midilearning :
#endif
		(hot ? skcol::fghot : skcol::fg));
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font, paint);
}

TextButton::TextButton(tsl::AppState* appState, float _scalefactor, int _aspect_ratio, int _alignment, const char* text,
	int _id,
	int offset, int offsetmulti) : TextButtonFramed(appState, _scalefactor,
		_aspect_ratio,
		_alignment, text, text,
		text) {
	name_normal = text;
	name_pressed = text;
	prio = 10;
	id = _id;

	if (_STATE->parameters[id].paramOffset) {
		auto fact = _STATE->parameters[id].offsetFact;
		for (int i = 1; i < _STATE->parameters[_STATE->parameters[id].paramOffset].max; i++)
			_STATE->parameters[id + i * fact].view = this;
	}

};


