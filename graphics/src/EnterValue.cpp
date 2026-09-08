#include <sstream>
#include <iomanip>
#include "EnterValue.h"
#include "app.h"

using namespace tsl::graphics;
void EnterValue::Task(tsl::AppState* state, tsl::parameters::Event& ev) {
	auto& param = state->parameters[ev.getDisplayParam()];
	if (param.name && !strcmp(param.name, "RATIO")) {
		showToast(state, "No direct input for this parameter.");
		return;
	}
	auto enterValue = state->enterValue.load();
	if (enterValue == nullptr) {
		state->enterValue.store(enterValue = std::make_shared<EnterValue>(state));
	}
	enterValue->deldraw();
	enterValue->delCB();
	enterValue->setup(ev);
	enterValue->init();
	enterValue->addDraw();
	enterValue->addCB();
	state->waitNotify.wait_for_signal(enterValue->token());
	enterValue->deldraw();
	enterValue->delCB();

}

void EnterValue::setup(tsl::parameters::Event& ev) {
	e = ev;
	
	setToken(_STATE->waitNotify.begin_wait());
	perm = true;
    auto& miditarget = _STATE->parameters[e.getDisplayParam()];
	auto min = miditarget.getMin(_STATE->sr);
	auto max = miditarget.getMax(_STATE->sr);
	auto actual = miditarget.toDisplay(_STATE->sr, e.getCurrentValue(_STATE));
	auto digits = miditarget.digits;
	_type = tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER
		| ((min < 0 || max < 0) ? tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_SIGNED : 0)
		| ((digits > 0) ? tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_DECIMAL : 0);


	std::stringstream tmp;

	tmp << miditarget.name;
	tmp << ": ";
	tmp << std::fixed << std::setprecision(digits) << min;
	tmp << " - ";
	tmp << std::fixed << std::setprecision(digits) << max << " " << miditarget.valuename;
	setTitle(tmp.str());
	std::stringstream tmp2;
	tmp2 << std::fixed << std::setprecision(digits) << actual;
	textInput1.setText(tmp2.str());
	textInput1.resetTimer();
}

int EnterValue::onEnter() {
	auto s = textInput1.getText();
	if (!TextInput::isValidNumber(s))return 0;

	auto value = std::stod(s);
	auto& miditarget = _STATE->parameters[e.getDisplayParam()];
	value = miditarget.fromDisplay(_STATE->sr, value);

	if (value < miditarget.min || value > miditarget.max) { return 0; }
	e.value = value;
	e.apply(_STATE, tsl::parameters::FromUi);
	return 1;
}


ValueView::ValueView(tsl::AppState* appState, View& par) : View(appState, WRAP, 0, CENTER_ALIGN, 0) , par_{ par } {
	ff = [&val = *this]() {
		auto& par = val.par_;
		auto _appState = par._appState;
		if (val.timer.elapsed() > 1.) {
			val.perm = false;
			_STATE->graphics.deleteWindow(val.windex);
			return;
		}
		if (par.id == 0 || par.id > NUM_PARAMS) {
			LOGE("EEEERRR");
			return;
		}

		auto& miditarget = _STATE->parameters[val.e.getDisplayParam()];
		//Knob *Knob = (Knob*) info->reference;
		float offset = miditarget.offset;

		const char* formatvalue = formatvalues[miditarget.digits];

		char text[100]{};
		const float value = miditarget.toDisplay(_STATE->sr, val.e.getCurrentValue(_STATE));

		if (!strcmp(miditarget.name, "RATIO")) {
			if (value == 0.)
				snprintf(text, 100, "%s", "BYPASS");
			else if (value == 1.0)
				snprintf(text, 100, "%s", "INF : 1");
			else
				snprintf(text, 100, "%.1f : 1", 1. / (1. - value));
			auto width = (strlen(text) + 2) * _STATE->maxCharWidtht2;
			float sx = par.startx + (par.width - width) * .5f;
			if (sx < 0)
				sx = 0;
			if (sx + width > _STATE->windowWidth)
				sx = _STATE->windowWidth - width;
			float sy = par.stopy;
			if (sy + _STATE->textsize1 > _STATE->windowHeight)
				sy = par.starty - _STATE->textsize1;

			auto c = _STATE->graphics.getCanvas(val.windex, sx, sy, width, _STATE->textsize1);
			if (!c)
				return;
			c->clear(skcol::bg);
			SkRect bounds{};

			int length = strlen(text);
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9);
			SkPaint paint;
			paint.setStrokeWidth(View::lw);
			paint.setAntiAlias(true);
			paint.setColor(skcol::text);
			font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);
			float xpos = (width - (bounds.width() + _STATE->maxCharWidtht2)) * .5f;
			//measureTextFixed(width - sxval, View::textsize1, View::font_normal, text, &sxval, &yy,
			//               info->fontsize);

			c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, _STATE->startyt2, font,
				paint);
			return;
		}
		else
			snprintf(text, 100, formatvalue, value);



		auto valueWidth = miditarget.getDisplayWidthValue(_STATE->sr, _STATE->maxCharWidtht2), nameWith = miditarget.getDisplayNameWidth(_STATE->maxCharWidtht2);
		auto width = valueWidth + nameWith + 2 * _STATE->maxCharWidtht2;

		float sx = par.startx + (par.width - width) * .5f;
		if (sx < 0)
			sx = 0;
		if (sx + width > _STATE->windowWidth)
			sx = _STATE->windowWidth - width;
		float sy = par.stopy;
		if (sy + _STATE->textsize1 > _STATE->windowHeight)
			sy = par.starty - _STATE->textsize1;

		auto c = _STATE->graphics.getCanvas(val.windex, sx, sy, width, _STATE->textsize1);
		if (!c)
			return;
		c->clear(skcol::bg);
		SkFont font(_STATE->font_normal);


		font.setSize(_STATE->textsize2 * .9);
		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::text);

		SkRect bounds{};

		int length = strlen(text);
		font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);
		float xpos = (width - (valueWidth + nameWith + _STATE->maxCharWidtht2)) * .5f;
		//measureTextFixed(width - sxval, View::textsize1, View::font_normal, text, &sxval, &yy,
		//               info->fontsize);

		c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos + valueWidth - bounds.width(), _STATE->startyt2, font,
			paint);


		if (miditarget.valuename != nullptr && miditarget.valuename[0] != ' ') {
			c->drawSimpleText(miditarget.valuename, strlen(miditarget.valuename),
				SkTextEncoding::kUTF8,
				xpos + valueWidth + _STATE->maxCharWidtht2, _STATE->startyt2,
				font, paint);
		}
		};
}

void ValueView::delRecursiveDraw() {
	View::delRecursiveDraw();
	_STATE->graphics.deleteWindow(windex);
}


void ValueView2::render(void*) {
	if (timer.elapsed() > 1.) {
		perm = false;
		_STATE->graphics.deleteWindow(windex);
		return;
	}
	if (par.id == 0 || par.id > NUM_PARAMS) {
		LOGE("EEEERRR");
		return;
	}

	auto& miditarget = _STATE->parameters[e.getDisplayParam()];

	//Knob *Knob = (Knob*) info->reference;
	float offset = miditarget.offset;

	const char* formatvalue = formatvalues[miditarget.digits];

	char text[100]{};

	const float value = miditarget.toDisplay(_STATE->sr, e.getCurrentValue(_STATE));


	if (!strcmp(miditarget.name, "RATIO")) {
		if (value == 0.)
			snprintf(text, 100, "%s", "BYPASS");
		else if (value == 1.0)
			snprintf(text, 100, "%s", "INF : 1");
		else
			snprintf(text, 100, "%.1f : 1", 1. / (1. - value));

	}
	else
		snprintf(text, 100, formatvalue, value);

	int lenwithoutdigits = 1;
	int x = std::max(std::abs(miditarget.getMin(_STATE->sr)), std::abs(miditarget.getMax(_STATE->sr)));
	while (x /= 10)
		lenwithoutdigits++;
	int negone = (miditarget.getMin(_STATE->sr) < 0 || miditarget.getMax(_STATE->sr) < 0);

	int len = lenwithoutdigits + negone + miditarget.digits;
	auto lenval = (int)(miditarget.valuename == nullptr || miditarget.valuename[0] == ' ') ? 0
		:
		strlen(miditarget.valuename);
	float width = (len + lenval + 4) * _STATE->maxCharWidtht2;
	float sx = par.startx + par.width * .5f - width * .5f;
	if (sx < 0)
		sx = 0;
	else if (sx + width > _STATE->windowWidth)
		sx = _STATE->windowWidth - width;
	float sy = par.stopy;
	if (sy + _STATE->textsize1 > _STATE->windowHeight)
		sy = par.starty - _STATE->textsize1;

	auto c = _STATE->graphics.getCanvas(windex, sx, sy, width, _STATE->textsize1);
	if (!c)
		return;
	c->clear(skcol::bg);
	SkFont font(_STATE->font_normal);

	font.setSize(_STATE->textsize2 * .9);
	SkPaint paint;
	paint.setStrokeWidth(View::lw);
	paint.setAntiAlias(true);
	paint.setColor(skcol::text);


	float sxval = width;
	if (miditarget.valuename != nullptr && miditarget.valuename[0] != ' ') {
		sxval -= (strlen(miditarget.valuename) + 2) * _STATE->maxCharWidtht2;
		c->drawSimpleText(miditarget.valuename, strlen(miditarget.valuename),
			SkTextEncoding::kUTF8,
			sxval, _STATE->startyt2,
			font, paint);
	}

	SkRect bounds{};
	int length = strlen(text);
	font.measureText(text, length, SkTextEncoding::kUTF8, &bounds);

	float xpos = (sxval - bounds.width()) * .5f;
	//measureTextFixed(width - sxval, View::textsize1, View::font_normal, text, &sxval, &yy,
	//               info->fontsize);

	c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, xpos, _STATE->startyt2, font,
		paint);
}
void ValueView2::delRecursiveDraw() {
	View::delRecursiveDraw();
	_STATE->graphics.deleteWindow(windex);
}
void ValueView2::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	e.setup(_STATE, tindex, par.id);
	timer.reset();
	perm = true;
	View::addRecursiveDraw();
}
void ValueView::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	e.setup(_STATE, tindex, par_.id);
	timer.reset();
	perm = true;
	View::addRecursiveDraw();
}

