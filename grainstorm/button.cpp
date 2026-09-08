#include "logger.h"
#include <SkPath.h>
#include <include/core/SkFont.h>
#include "button.h"
#include "grainstorm.h"
#include "defines.h"
#include "colours.h"
#include "track.h"
#include "infopanel.h"
#include "gui/gui.h"
#include "view.h"
#include "tools.h"
#include "selector.h"
#include "tools/aligned_memalloc.h"
#include "Convolver.h"
#include "lfo.h"
#include <app.h>
#include <Input.h>
#include "waveform.h"

#ifdef PLUGIN_MODE
#include "IPlugParamDefs.h"
#endif

#ifdef __ANDROID__
#include "TrackSettings.h"
#endif


using namespace tsl::graphics;
NormalButton::NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
	const char* _normal,
	const char* _pressed, int32_t _id, long off, int mul) : Button(appState,
		_scalefactor, _aspect_ratio, _alignment, _normal, _pressed) {
	id = _id;
	e.setup(appState, 0, id);
	if (id != PARAM_NOT_ASSIGNED) {
		_STATE->parameters[id].view = this;
		_STATE->parameters[id].type = ParameterType_bool;
		auto offset = _STATE->parameters[id].paramOffset;
		if (offset > 0 && _STATE->parameters[offset].max > 0) {
			for (int i = 1; i < _STATE->parameters[offset].max; i++)
				_STATE->parameters[id + i * _STATE->parameters[id].offsetFact].view = this;
		}
	}

}

void NormalButton::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();

	e.setup(_STATE, tindex, id);
	View::addRecursiveDraw();
}


#ifdef PLUGIN_MODE
void NormalButton::delRecursiveCB() {
	auto& queue = _STATE->queue_callback;
	queue.del(this);
	pointerid = -1;
	if (auto tmpid = _STATE->parameters[id].pluginIndex >= 0) {
		auto tindex = _STATE->active_track.load();
		auto& pc = _STATE->parameters[id].paramChanging[tindex];
		int expected = pc.load(std::memory_order_acquire);
		while (expected > 0) {
			if (pc.compare_exchange_weak(expected, expected - 1,
				std::memory_order_acq_rel,
				std::memory_order_acquire)) {
				if (expected == 1) {
					// transitioned to 0, call EndParamChange
					if (id == BYPASSGRAINFX)      tmpid = tsl::iplug::BypassParams[GASGRAIN];
					else if (id == BYPASSFX)      tmpid = tsl::iplug::BypassParams[GASFX];
					else if (id == BYPASSSTEREOFX)tmpid = tsl::iplug::BypassParams[GASSTFX];
					else if (id == OFFGRAIN)      tmpid = tsl::iplug::PowerParams[GASGRAIN];
					else if (id == OFFFX)         tmpid = tsl::iplug::PowerParams[GASFX];
					else if (id == OFFSTEREOFX)   tmpid = tsl::iplug::PowerParams[GASSTFX];
					_STATE->EndParamChange(e);
				}
				break;
			}
		}
	}
}
#endif


void NormalButton::callback(const InputEvent& event) {

	auto tindex = _STATE->active_track.load();
	int32_t action = event.action;

	switch (action) {
	case ACTION_DOWN:
		Button::down(event);
		setState(HOT);
#ifdef PLUGIN_MODE
			_STATE->StartParamChange(e);
#endif
		break;

	case ACTION_MOVE:
		if (Button::check(event)) {
#ifdef PLUGIN_MODE
				_STATE->EndParamChange(e);
#endif			;
		}
		break;

	case ACTION_UP:
		if (pointerid == event.pointer_id) {
			pointerid = -1;
			setState(NORMAL);
#ifdef PLUGIN_MODE
				_STATE->EndParamChange(e);
#endif
			if (e.eventType == tsl::parameters::Eventtype::Power) {
				switch (e.subType) {
				case tsl::parameters::powerFx:
				case tsl::parameters::powerGrainFx:
				case tsl::parameters::powerStereoFx: {
					e.power.pow = std::bit_cast<tsl::parameters::PowerState>(e.getCurrentValue(_STATE)).pow == 1 ? 0 : 1;
				}
				break;
				default:
					e.value = e.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;
					break;
				}
			}
			else 
				e.value = e.getCurrentValue(_STATE) == 1.0 ? 0.0 : 1.0;

			e.applyFromExt(_STATE, tsl::parameters::FromUi);


		}
		break;
	default:
		break;
	}
}


void NormalButton::render(void* context) {
	auto tindex = _STATE->active_track.load();
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);

	flush(canvas);

	const bool hot = state.load() == HOT;
	const bool pressed = e.getCurrentValue(_STATE) == 1.0;

	if (hot) {
		paint.setColor(skcol::bghot);
		float sizerect = height > width ? width : height;
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + (width - sizerect) * .5f,
				starty + (height - sizerect) / 2, sizerect, sizerect),
			sizerect * .1f, sizerect * .1f, paint);
	}
	bool greyedOut = false;
	switch (id)
	{
	case UNDOBUTTON:
		greyedOut = !_DATA->snapShot.hasUndos[tindex].load();
		
		break;
	case REDOBUTTON:
		greyedOut = !_DATA->snapShot.hasRedos[tindex].load();
			break;
#ifdef PLUGIN_MODE
	case STOPButton:
		//case STEPBACK:
		//case STEPFORW:
		//case SLOWButton:
		//case FASTButton:
		greyedOut = _STATE->params[tindex][LOOPSYNCDAWTRANSPORT] == 1.0;
		break;
	case LFOBACKW:
	case LFOFORW:
		//case LFOSLOW:
		//case LFOFAST:
	case LFOSTOP:
	case LFO1BACKW:
	case LFO1FORW:
		//case LFOSLOW:
		//case LFOFAST:
	case LFO1STOP:
		greyedOut = _STATE->params[tindex][LFO1SYNCDAWTRANSPORT + GASLFO * 2] == 1.0;
		break;
#endif
	default:
		break;
	}


	paint.setColor(
		_STATE->midilearning.load() && id != PARAM_NOT_ASSIGNED &&
		(_STATE->parameters[id].flags & Param::MidiParam) ? skcol::midilearning
		: (pressed ? (greyedOut
			? SkColorSetA(C_active, 125) : C_active)
			: (hot ? (greyedOut
				? skcol::grey : skcol::fghot)
				: (greyedOut
					? skcol::grey : skcol::fg))));

	textDisplayCentered(this, canvas, paint, font, name_normal, iconScale, false);
}


void TextButton2::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();
	int32_t action = event.action;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;

	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (pointerid == event.pointer_id) {
			pointerid = -1;
			if (func != nullptr)func();
			setState(NORMAL);
		}
		break;
	default:
		break;
	}
}

TextButton2::TextButton2(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
	int32_t _id, const std::function<void()>& f) : TextButtonFramed(appState, _scalefactor,
		_aspect_ratio,
		_alignment, appState->parameters[_id].name, appState->parameters[_id].name,
		appState->parameters[_id].name) {
	name_normal = _STATE->parameters[_id].name;
	name_pressed = _STATE->parameters[_id].name;
	prio = 10;
	id = _id;
	if (f != nullptr)
		func = f;
	else
		func = [=]() {auto tindex = _STATE->active_track.load();
	tsl::parameters::Event e;
	e.setup(_STATE, tindex, id);
	e.applyFromExt(_STATE, tsl::parameters::FromUi);
		};
};


void TextButton2::render(void* context) {
	float startx_pos, starty_pos, fontsize;

	auto tindex = _STATE->active_track.load();
	const char* text =
		_STATE->parameters[id].name;
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

	paint.setColor(_STATE->midilearning.load() && id != PARAM_NOT_ASSIGNED &&
		(_STATE->parameters[id].flags & Param::MidiParam) ? skcol::midilearning
		: skcol::pressed);
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx_pos,
		starty_pos,
		font, paint);
	if (state == HOT) {
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
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
			radius, radius, paint);
	}
	canvas->restore();
}

void MicButton::render(void* context) {

	auto tindex = _STATE->active_track.load();
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);

	//   bool pressed = _STATE->params[track->index][id + offset].load() == 1.0;

	// const bool pressed = state == PRESSED || active_track.load()->params[id] == 1.0;
	flush(canvas);

	const bool hot = state.load() == HOT;
#ifdef __ANDROID__

	bool pressed = _DATA->recorder._isRecording.load();

#else
	bool pressed{};
#endif
	if (hot) {
		paint.setColor(skcol::bghot);
		float sizerect = height > width ? width : height;
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + (width - sizerect) * .5f,
				starty + (height - sizerect) / 2, sizerect, sizerect),
			sizerect * .1f, sizerect * .1f, paint);
	}
	paint.setColor(
		_STATE->midilearning.load() && id != PARAM_NOT_ASSIGNED &&
		(_STATE->parameters[id].flags & Param::MidiParam) ? skcol::midilearning
		: (pressed ? C_active
			: (hot ? skcol::fghot
				: skcol::fg)));
	textDisplayCentered(this, canvas, paint, font, name_normal, iconScale, false);
}


void RecordButton::render(void* context) {
	if (!_STATE->player.isrecording.load(std::memory_order_acquire)) {
		if (!_STATE->dofastrender.load()) {
			flush((SkCanvas*)context);
			return;
		}
		NormalButton::render(context);
		return;
	}
	auto canvas = (SkCanvas*)context;
	flush(canvas);
	SkPaint paint;
	paint.setAntiAlias(true);

	const bool hot = state.load() == HOT;
	if (hot) {
		paint.setColor(skcol::bghot);
		float sizerect = height > width ? width : height;
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + (width - sizerect) / 2,
				starty + (height - sizerect) / 2, sizerect, sizerect),
			sizerect * .1f, sizerect * .1f, paint);
	}
	SkFont font(_STATE->font_md);
	paint.setColor(skcol::red);
	textDisplayCentered(this, canvas, paint, font, name_normal, 1.0, false);

	TIME_P time{};
	time_convert(time, _STATE->sr, _STATE->player.recoff);
	char text[30];
	snprintf(text, 30, "%02d : %02d", time.m, time.s);
	float fontsize, sx, sy;
	measureText(this, _STATE->font_normal, "00 : 00", .9f, &sx, &sy, &fontsize);
	font.setSize(fontsize);
	paint.setColor(skcol::pressed);
	font = _STATE->font_normal;
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx + sx,
		starty + sy, font, paint);
}


void EnvelopeButton::render(void* context) {

	auto tindex = _STATE->active_track.load();

	float offsetrect = lw * .5f;


	int32_t offset1 = 0;
	if (_STATE->parameters[id].paramOffset)
		offset1 = (int)(_STATE->params[tindex][_STATE->parameters[id].paramOffset].load());
	int32_t offsetmulti = _STATE->parameters[id].offsetFact;
	offset1 = offset1 * offsetmulti;
	int32_t val = _STATE->params[tindex][id + offset1].load();
	bool active = userdata == val;

	if (userdata == LFO_USER) {
		auto canvas = (SkCanvas*)context;
		SkPaint paint;
		paint.setStrokeWidth(lw);
		paint.setAntiAlias(true);
		SkFont font(_STATE->font_md);
		flush(canvas);
		paint.setStyle(SkPaint::kStroke_Style);
		paint.setColor(skcol::fg);
		textDisplayCentered(this, canvas, paint, font, reinterpret_cast<const char*>(ICON_MD_EDIT),
			1.0, false);
		if (active || state == HOT) {
			canvas->drawRoundRect(
				SkRect::MakeXYWH(startx + offsetrect, starty + offsetrect,
					width - lw,
					height - lw),
				width * .1f, width * .1f, paint);
		}
		return;
	}
	else if (userdata == LFO_RND) {
		auto canvas = (SkCanvas*)context;
		SkPaint paint;
		paint.setStrokeWidth(lw);
		paint.setAntiAlias(true);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2);
		flush(canvas);
		//paint.setStyle(SkPaint::kStroke_Style);
		paint.setColor(skcol::fg);
		textDisplayCenteredFixed(this, canvas, paint, font, "RND", 1.0, false);
		if (active || state == HOT) {
			paint.setStyle(SkPaint::kStroke_Style);
			canvas->drawRoundRect(
				SkRect::MakeXYWH(startx + offsetrect, starty + offsetrect,
					width - lw,
					height - lw),
				width * .1f, width * .1f, paint);
		}
		return;
	}

	float mw = width / 5.f;
	float mw2 = mw / 2.f;

	float line_width = lw;
	float lh = line_width / 2;
	auto env = _DATA->eq[name_normal].win;

	int32_t size = WINDOW_SIZE;
	int32_t freq = 1;//*win->freq;

	long double env_frame = 0;
	float i = lh + mw2;
	long double step_length =
		((size - 1) * (long double)freq + freq - 0.94) / ((long double)width - 1 - mw);
	if (step_length >= size) {
		return;
	}
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(line_width);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	flush(canvas);
	canvas->save();
	canvas->translate(startx, starty);

	float draw_height = (height - 2 * line_width - mw);
	float offset = line_width + mw2;
	SkPath path;
	paint.setColor(skcol::fg);

	if (active || state == HOT) {
		float width_rect = width - lw;
		float height_rect = height - lw;
		float radius = height * .1f;
		float startx_rect = offsetrect;
		float starty_rect = offsetrect;
		path.addRoundRect({ startx_rect, starty_rect, width_rect,
						   height_rect }, radius, radius,
			SkPathDirection::kCCW);
	}

	path.moveTo(i, offset + draw_height * (1.f - env[(int)env_frame]));

	for (int32_t j = 0; j < width - mw - 3; j++) {
		env_frame += step_length;
		if (env_frame >= size)
			env_frame -= size;
		i++;
		path.lineTo(i, offset + draw_height * (1.f - env[(int)env_frame]));
		path.moveTo(i, offset + draw_height * (1.f - env[(int)env_frame]));

	}
	canvas->drawPath(path, paint);
	canvas->restore();
}

void MidilearnButton::render(void* context) {

	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);

	float offset = lw * .5f;

	if (state == NORMAL) {
		flush(canvas, _STATE->midilearning.load() ? skcol::midilearning : skcol::bg);
	}
	else if (state == HOT) {
		flush(canvas, skcol::bghot);
	}
	canvas->save();
	canvas->translate(startx, starty);
	paint.setColor(skcol::fg);
	canvas->drawLine(0, offset, width, offset, paint);
	canvas->restore();
}



void MidilearnButton::callback(const InputEvent& event) {

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	int32_t action = event.action;


	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			_STATE->midilearning.store(!_STATE->midilearning.load());
			setState(NORMAL);
			tsl::graphics::TrackButton::func(_appState, _STATE->active_track.load());

#if defined __ANDROID__
			midiLearningEvent(_STATE);
#endif           
		}
		break;
	default:
		break;
	}
}


void SyncMidiButton::render(void* context) {

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	auto tindex = track->index;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);
	flush(canvas);

	const bool hot = state.load() == HOT;

	if (hot) {
		paint.setColor(skcol::bghot);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx, starty, width, height),
			height * .1f, height * .1f, paint);
		//text_display_centered(view, canvas, &paint, b->name_normal, 1.0, false);
	}
	paint.setColor(
		track->lfos[GASLFO]->syncmidi()
		? skcol::custom_green : (hot ? skcol::fghot : skcol::fg));

	float r = width > height ? height * .5f : width * .5f;
	r *= .9;
	float cx = width * .5f;
	float cy = height * .5f;
	canvas->save();
	canvas->translate(startx, starty);
	SkPath path;
	path.addCircle(cx, cy, r);
	canvas->clipPath(path);
	paint.setStyle(SkPaint::kFill_Style);
	canvas->drawCircle(cx, cy, r, paint);
	paint.setColor(hot ? skcol::bghot : skcol::bg);
	float rcircles = r * .65f;
	float wrect = r / 2.f;
	float wrectd2 = wrect / 2.f;
	for (int32_t i = 360; i >= 180; i -= 45) {
		canvas->drawCircle(cx + cosf(SkDegreesToRadians(i)) * rcircles,
			cy + sinf(SkDegreesToRadians(i)) * rcircles,
			rcircles / 4.f, paint);
	}
	//canvas->drawCircle(view->width / 2. + cosf(SkDegreesToRadians(i)) *rcircles, view->height / 2. + sinf(SkDegreesToRadians(i)) * rcircles, rcircles/5.f, paint);

	canvas->drawRect(
		SkRect::MakeXYWH(cx - wrectd2, cy + r - wrect, wrect, wrect),
		paint);
	canvas->restore();
	//text_display_centered(view, canvas, &paint, b->name_pressed, 1.0, false);
}


void SyncMidiButton::callback(const InputEvent& event) {

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	auto tindex = track->index;
	int32_t action = event.action;

	LFO* lfo = track->lfos[GASLFO];
	char string[50];
	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;

	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			lfo->store(LFOSYNCMIDI, !lfo->syncmidi());
			{
				std::lock_guard lk(_DATA->midilfoqueue);
				if (lfo->syncmidi())
					_DATA->midilfoqueue.add(lfo);
				else
					_DATA->midilfoqueue.del(lfo);
			}
			if (lfo->syncmidi())
				snprintf(string, 50, "External MIDICLOCK activated for %s.",
					lfo->name);
			else
				snprintf(string, 50, "External MIDICLOCK deactivated for %s.", lfo->name);
			showToast(_STATE, string);
			setState(NORMAL);
		}
		break;
	default:
		break;
	}
}

void JoinEndsButton::render(void* context) {

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	auto tindex = track->index;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);
	bool joinends = false;
	if (id == SPACE_LFOS)
		joinends = track->lfos[GASLFO]->joinends();
	else if (id == SPACE_ENVELOPES)
		joinends = _STATE->params[track->index][GRAINJOIN].load() == 1.0;
	flush(canvas);

	const bool hot = state == HOT;

	if (hot) {
		paint.setColor(skcol::bghot);

		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx + (width - height * .9f) * .5f,
				starty + (height - height * .9f) * .5f,
				height * .9f, height * .9f),
			height * .1f, height * .1f, paint);
	}
	paint.setColor(hot ? skcol::fghot : skcol::fg);

	textDisplayCentered(this, canvas, paint, font,
		joinends ? reinterpret_cast<const char*>(ICON_MD_CHECK_BOX)
		: reinterpret_cast<const char*>(ICON_MD_CHECK_BOX_OUTLINE_BLANK),
		1.0, false);

}


void JoinEndsButton::callback(const InputEvent& event) {

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	auto tindex = track->index;
	int32_t action = event.action;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			if (id == SPACE_LFOS) {
				track->lfos[GASLFO]->store(LFOJOIN, !track->lfos[GASLFO]->joinends());
				if (track->lfos[GASLFO]->joinends()) {
					track->lfos[GASLFO]->store(LFOENVY0 + track->lfos[GASLFO]->segments(),
						track->lfos[GASLFO]->gp(LFOENVY0));
				};
				track->lfos[GASLFO]->store(LFORECOMPUTE, 1.0);
			}
			else if (id == SPACE_ENVELOPES) {
				_STATE->params[track->index][GRAINJOIN].store(!_STATE->params[track->index][GRAINJOIN].load());
				if (_STATE->params[track->index][GRAINJOIN].load()) {
					_STATE->params[track->index][GRAINENVY0 +
						(int)_STATE->params[track->index][GRAINNSEGS].load()].store(
							_STATE->params[track->index][GRAINENVY0].load());
				};
				_STATE->params[track->index][RECOMPUTEGRAINENV2].store(1.0);
			}
			setState(NORMAL);
		}
		break;
	default:
		break;
	}
}

void BypassOffButton::render(void* context) {
	TRACK* track = _DATA->tracks[_STATE->active_track.load(std::memory_order_acquire)];
	auto tindex = track->index;
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_md);
	flush(canvas);

	if (state == DISABLED) {
		return;
	}

	bool active = false;
	bool greyedOut = false;
	switch (id) {
	case POWERButton:
		active = _STATE->player._isplaying.load(std::memory_order_acquire);
		break;
	case POWERTRACK:
		active = _STATE->params[track->index][POWERTRACK].load(std::memory_order_acquire);
		break;
	case OFFGRAIN:
		active = track->fxpower[GASGRAIN].load(std::memory_order_acquire);
		break;
	case OFFFX:
		active = track->fxpower[GASFX].load(std::memory_order_acquire);
		break;
	case OFFSTEREOFX:
		active = track->fxpower[GASSTFX].load(std::memory_order_acquire);
		break;
	case BYPASSGRAINFX:
		active = !track->bypass[GASGRAIN].load(std::memory_order_acquire);
		break;
	case BYPASSFX:
		active = !track->bypass[GASFX].load(std::memory_order_acquire);
		break;
	case BYPASSSTEREOFX:
		active = !track->bypass[GASSTFX].load(std::memory_order_acquire);
		break;
	case PLAYButton:
		active = _STATE->params[track->index][TRACKSTOPPED].load(std::memory_order_acquire) == 0.0;
#ifdef PLUGIN_MODE
		greyedOut = _STATE->params[track->index][LOOPSYNCDAWTRANSPORT].load(std::memory_order_acquire) == 1.0;
#endif
		break;
	case LFOPLAY:
	case LFO1PLAY:
		active = _STATE->params[track->index][LFO1STOPPED + GASLFO * LFONUMPARAMS].load(std::memory_order_acquire) == 0.0;
#ifdef PLUGIN_MODE
		greyedOut = _STATE->params[track->index][LFO1SYNCDAWTRANSPORT + GASLFO * 2].load(std::memory_order_acquire) == 1.0;
#endif
		break;

	default:
		break;
	}

	bool midilearning = _STATE->midilearning.load(std::memory_order_acquire);
	const bool hot = state.load() == HOT;
	if (hot) {
		paint.setColor(skcol::bghot);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx, starty, width, height),
			height * .1f, height * .1f, paint);
	}
	if (midilearning)
		paint.setColor(skcol::midilearning);
	else if (active)
		paint.setColor(
			greyedOut
			? SkColorSetA(skcol::custom_green, 110)
			: skcol::custom_green
		);
	else
		paint.setColor(hot ? (greyedOut
			? SkColorSetA(skcol::fghot, 110) : skcol::fghot
			) : (greyedOut ? SkColorSetA(skcol::fg, 110) : skcol::fg
				));


	textDisplayCentered(this, canvas, paint, font,
		active ? name_pressed
		: name_normal,
		1.0, false);
}


TrackButton::TrackButton(tsl::AppState* appState, float _scalefactor, int32_t _aspectratio, int _alignment, int _trackindex)
	: trackindex(_trackindex), Button(appState, _scalefactor, _aspectratio, _alignment, "1", "1") {
}

void TrackButton::render(void* context) {

	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	const float w = 2.0;
	SkPath path;
	//path.addRect(SkRect::MakeXYWH(view->startx, view->starty, view->width, button->state == PRESSED ? view->height + 3 : view->height),
	//             SkPath::kCW_Direction);
	canvas->save();
	// canvas->clipPath(path);
	float off = width / 4.f;

	const bool _active = _STATE->active_track.load() == trackindex;
	if (state == NORMAL) {
		if (!_active) {
			float fontsize = height > width ? width * .6f : height * .6f;
			font.setSize(fontsize);
			SkRect bounds{};
			font.measureText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, &bounds);
			auto xpos = SkFloatToScalar(width * .5f - bounds.centerX());
			auto ypos = SkFloatToScalar(height * .5f - bounds.centerY());
			flush(canvas, skcol::transparent);
			canvas->translate(startx, starty);
			paint.setColor(skcol::bg);
			canvas->drawRect(
				SkRect::MakeXYWH(off + lw2, 0, width - 2 * off - lw, height - off - lw),
				paint);
			paint.setColor(skcol::text);
			canvas->drawSimpleText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, xpos, ypos,
				font,
				paint);
		}
		else {
			float fontsize = height > width ? width * .7f : height * .7f;
			font.setSize(fontsize);
			SkRect bounds{};
			font.measureText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, &bounds);
			auto xpos = SkFloatToScalar(width * .5f - bounds.centerX());
			auto ypos = SkFloatToScalar(height * .5f - bounds.centerY());
			canvas->translate(startx, starty);

			paint.setColor((SkColor)skcol::bg);

			paint.setStyle(SkPaint::kFill_Style);
			canvas->drawRect(
				SkRect::MakeXYWH(-off + lw2, 0, width + 2 * off - lw2, height + lw),
				paint);
			paint.setColor(skcol::fg);
			canvas->drawSimpleText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, xpos, ypos,
				font,
				paint);
			paint.setStyle(SkPaint::kStroke_Style);
			if (trackindex == 0) {
				path.moveTo(-lw2, lw2);
				path.lineTo(width - off - lw2, lw2);
			}
			else {
				path.moveTo(-startx, height + lw2);
				path.lineTo(lw2 - off, height + lw2);
				path.conicTo(lw2, height + lw2, lw2, height - off + lw2, w);
				path.lineTo(lw2, lw2 + off);
				path.conicTo(lw2, lw2, lw2 + off, lw2, w);

				path.lineTo(width - off - lw2, lw2);
			}
			path.conicTo(width - lw2, lw2, width - lw2, lw2 + off, w);
			path.lineTo(width - lw2, height + lw2 - off);
			path.conicTo(width + lw2, height + lw2, width + off + lw2, height + lw2, w);
			path.lineTo(_STATE->windowWidth, height + lw2);
			canvas->drawPath(path, paint);
		}
	}
	else if (state == HOT) {
		float fontsize = height > width ? width * .7f : height * .7f;
		font.setSize(fontsize);
		SkRect bounds{};
		font.measureText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, &bounds);
		auto xpos = SkFloatToScalar(width * .5f - bounds.centerX());
		auto ypos = SkFloatToScalar(height * .5f - bounds.centerY());
		flush(canvas, skcol::transparent);
		canvas->translate(startx, starty);
		paint.setColor(skcol::text);
		canvas->drawSimpleText(&tnames[trackindex], 1, SkTextEncoding::kUTF8, xpos, ypos, font,
			paint);
	}
	canvas->restore();
}

void TrackButton::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();

	if (tindex == trackindex)
		return;

	auto action = event.action;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;
	case ACTION_UP: {
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			state = NORMAL;
			func(_appState, trackindex);
		}
		break;
	}
	default:
		break;
	}
}

void TrackButton::func(tsl::AppState* _appState, int target) {
	auto tindex = _STATE->active_track.load();
	int asold = GASMAIN;
	int asnew = _appState->params[target][AS].load();
	{
		std::lock_guard lk(_STATE->queue_callback);
		_DATA->views.active_spaces_array[asold]->delRecursiveCB();
	}
	{
		std::lock_guard lk(_STATE->queue_draw);
		_DATA->views.active_spaces_array[asold]->delRecursiveDraw();
		_STATE->active_track.store(target);
		_DATA->views.controlpanel->redrawDirect();
		_DATA->views.controlpanel->addRecursiveDraw();
		_DATA->views.toppanel_main->redrawDirect();
		_DATA->views.toppanel_main->addRecursiveDraw();
		_DATA->views.toppanel_track->redrawDirect();
		_DATA->views.toppanel_track->addRecursiveDraw();
		_DATA->views.active_spaces_buttons[asold]->redrawDirect();
		_DATA->views.active_spaces_buttons[asnew]->redrawDirect();
		_DATA->views.active_spaces_array[asnew]->redrawDirect();
		_DATA->views.active_spaces_array[asnew]->addRecursiveDraw();
	}
	{
		std::lock_guard lk(_STATE->queue_callback);
		_DATA->views.active_spaces_array[asnew]->addRecursiveCB();
	}
}
/*
#include <mutextsl.h>
#include <mutex>
#include <thread>
void test() {
	std::thread t1, t2;
	tsl::recursive_mutex m1;
	std::recursive_mutex m2;
	std::atomic<bool> done{ false };
	int bla = 0;
	MEASUSEINIT
		MEASURESTART
		t1 = std::thread([&]() {
		for (int i = 0; i < 1000000; i++) {
			std::lock_guard lk(m2);
			bla++;
		}
			});
	t2 = std::thread([&]() {
		for (int i = 0; i < 1000000; i++) {
			std::lock_guard lk(m2);
			bla++;
		}
		});
	t1.join();
	t2.join();
	MEASURESTOP
		LOGE("1 %d", bla);
	bla = 0;
	MEASURESTART
		t1 = std::thread([&]() {
		for (int i = 0; i < 1000000; i++) {
			std::lock_guard lk(m1);
			bla++;
		}
			});
	t2 = std::thread([&]() {
		for (int i = 0; i < 1000000; i++) {
			std::lock_guard lk(m1);
			bla++;
		}
		});

	t1.join();
	t2.join();
	MEASURESTOP
		LOGE("2 %d", bla);

}
*/


ButtonSpaceSwitch::ButtonSpaceSwitch(tsl::AppState* appState, float sc, int32_t as, int al, int space_) : Button(appState, sc, as, al), space{ space_ } {
	_DATA->views.active_spaces_buttons[space] = this;
};
void ButtonSpaceSwitch::callback(const InputEvent& event) {
	ViewS& views = _DATA->views;
	auto tindex = _STATE->active_track.load();
	int32_t action = event.action;
	auto asold = GASMAIN;
	if (asold == space) {
		return;
	}
	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			state = NORMAL;
			/* The two add/delRecursiveCB calls below used to run with no lock at
			   all: only queue_draw was guarded, and ACTION_UP reaches this from
			   the Android input thread while the draw thread walks the same
			   intrusive list. Take both queues together for the whole swap. */
			std::scoped_lock lk(_STATE->queue_callback, _STATE->queue_draw);
			views.active_spaces_array[asold]->delRecursiveCB();
			_DATA->views.active_spaces_array[asold]->delRecursiveDraw();
			_STATE->params[tindex][AS].store(space);
			_DATA->views.active_spaces_buttons[asold]->redrawDirect();
			_DATA->views.active_spaces_buttons[space]->redrawDirect();
			_DATA->views.active_spaces_array[space]->redrawDirect();
			_DATA->views.active_spaces_array[space]->addRecursiveDraw();
			views.active_spaces_array[space]->addRecursiveCB();
		}
		break;
	default:
		break;
	}
}



void ButtonSpaceSwitch::render(void* context) {

	auto tindex = _STATE->active_track.load();
	auto canvas = (SkCanvas*)context;
	flush(canvas, skcol::bg);
	flush(canvas,
		state == HOT ? skcol::blue_transparent : (GASMAIN == space ? skcol::blue_transparent
			: skcol::bg));

	SkPaint paint;
	paint.setAntiAlias(true);
	char bla[10];
	snprintf(bla, 10, "%d", pos_p + 1);
	paint.setAntiAlias(true);
	paint.setStrokeWidth(lw);
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2);
	paint.setColor(skcol::fg);
	textDisplayCenteredFixed(this, canvas, paint, font, bla, 1.0, false);
}




void ButtonEnvelopeSwitch::render(void* context) {

	auto tindex = _STATE->active_track.load();
	auto canvas = (SkCanvas*)context;
	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	float fontsize = _STATE->textsize2;
	const char* text =
		name_normal;
	font.setSize(fontsize);
	SkRect bounds;
	float textwidth = font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	SkScalar xpos = SkFloatToScalar(startx + width * .5f - bounds.centerX());
	SkScalar ypos = SkFloatToScalar(starty + height * .5f - bounds.centerY());
	flush(canvas, skcol::bg);
	flush(canvas,
		state == HOT ? skcol::blue_transparent : (GASENV == type ? skcol::blue_transparent
			: skcol::bg));

	paint.setColor(skcol::fg);
	canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, ypos, font, paint);
}


void ButtonEnvelopeSwitch::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();
	int32_t action = event.action;

	int32_t current = GASENV;

	if (current == type)
		return;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;
	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			pointerid = -1;
			setState(NORMAL, false);
			_STATE->params[tindex][ASENV] = type;

			/* Same defect as ButtonSpaceSwitch: the add/delRecursiveCB pairs below
			   ran unlocked, the queue_draw guard only started further down. One
			   scoped_lock over the whole swap instead. */
			std::scoped_lock lk(_STATE->queue_callback, _STATE->queue_draw);

			if (type == SPACE_GRAINENV1) {
				_DATA->views.space_grainenv2->delRecursiveCB();
				_DATA->views.space_grainenv1->addRecursiveCB();

			}
			else {
				_DATA->views.space_grainenv1->delRecursiveCB();
				_DATA->views.space_grainenv2->addRecursiveCB();

			}
			_DATA->views.button_analysis->addRecursiveDraw();
			_DATA->views.button_synthesis->addRecursiveDraw();
			if (type == SPACE_GRAINENV1) {
				_DATA->views.space_grainenv2->delRecursiveDraw();
				_DATA->views.space_grainenv1->redrawDirect();
				_DATA->views.space_grainenv1->addRecursiveDraw();
			}
			else {
				_DATA->views.space_grainenv1->delRecursiveDraw();
				_DATA->views.space_grainenv2->redrawDirect();
				_DATA->views.space_grainenv2->addRecursiveDraw();
			}
		}
		break;
	default:
		return;
	}
}


void ButtonEnvelopeSwitch::func(tsl::AppState* _appState) const {

	auto tindex = _STATE->active_track.load();
	if (type == SPACE_GRAINENV1) {
		_STATE->params[tindex][ASENV] = SPACE_GRAINENV1;
		_DATA->views.space_grainenv2->delCB();
		_DATA->views.space_grainenv2->deldraw();
		_DATA->views.space_grainenv1->redraw();
		_DATA->views.space_grainenv1->addDraw();
		_DATA->views.space_grainenv1->addCB();
	}
	else {
		_STATE->params[tindex][ASENV] = SPACE_GRAINENV2;
		_DATA->views.space_grainenv1->delCB();
		_DATA->views.space_grainenv1->deldraw();
		_DATA->views.space_grainenv2->redraw();
		_DATA->views.space_grainenv2->addDraw();
		_DATA->views.space_grainenv2->addCB();
	}
	_DATA->views.button_analysis->redraw();
	_DATA->views.button_synthesis->redraw();

}







void ConvolverButton::callback(const InputEvent& event) {

	int32_t action = event.action;

	switch (action) {
	case ACTION_DOWN:
		down(event);
		setState(HOT);
		break;
	case ACTION_MOVE:
		check(event);
		break;

	case ACTION_UP:
		if (event.pointer_id == pointerid) {
			auto track = _STATE->active_track.load();
			_DATA->toAudioThreadQueue.try_push([this, track]() {
				Convolver::loadIR(_DATA->tracks[track], _DATA->tracks[trackindex]);
				});
			pointerid = -1;
			setState(NORMAL);
		}
		break;
	default:
		break;
	}
}

void ConvolverButton::render(void* context) {
	float startx_pos, starty_pos, fontsize;

	const char* text = _DATA->tracks[trackindex]->name;
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

	if (state == HOT) {
		paint.setColor(skcol::pressed);
		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx_pos,
			starty_pos,
			font, paint);
		float width_rect = width - lw;
		float height_rect = height - lw;
		float radius = height / 10.f;
		float startx_rect = lw / 2;
		float starty_rect = lw / 2;
		paint.setStyle(SkPaint::kStroke_Style);
		canvas->drawRoundRect(
			SkRect::MakeXYWH(startx_rect, starty_rect, width_rect, height_rect),
			radius, radius, paint);
	}
	else if (state == NORMAL) {
		paint.setColor(skcol::text);
		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startx_pos,
			starty_pos,
			font, paint);
	}
	canvas->restore();
}
