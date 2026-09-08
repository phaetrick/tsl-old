//
// Created by pr on 15.05.20.
//

#include "logger.h"
#include <include/core/SkPath.h>
#include "types.h"
#include "envwin2.h"
#include <view.h>
#include "lfo.h"
#include <Input.h>
#include "colours.h"

using namespace tsl::graphics;

LfoEnvEditor::LfoEnvEditor(tsl::AppState* appState) :View(appState), valueView(appState,
	[&val = valueView, &par = *this, &_act = _activeSeg]() {
		auto _appState = val._appState;
		if (val.timer.elapsed() > 1.) {
			val.perm = false;
			_STATE->graphics.deleteWindow(val.windex);
			return;
		}

		TRACK* track = _DATA->tracks[_STATE->active_track.load()];
		auto tindex = track->index;
		LFO* lfo = track->lfos[GASLFO];
		int32_t active = _act.load();
		if (active < 0 || active > 16)
			return;

		int32_t control_num = lfo->dest();
		if (control_num == ROTPOS || control_num == PHASER3CENTER)
			return;
		auto reference = _STATE->controls[tindex][control_num].lfo_min.load() +
			DISTANCEF(
				_STATE->controls[tindex][control_num].lfo_min.load(),
				_STATE->controls[tindex][control_num].lfo_max.load()) *
			lfo->gp(LFOENVY0 + active);
		auto& miditarget = _STATE->parameters[control_num];

		const char* test =
			"POS: 00 : 00 : 000X/ 00 : 00 : 000  LOOP: 00 : 00 : 000XLOAD: 100%";

		auto fontsize = _STATE->textsize2 * .9f;

		val.width = _STATE->textsize2;
		val.height = _STATE->textsize1;
		val.startx = val.starty = 0;


		float startx_pos1, starty_pos;//"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
		measureTextFixed(val.width, val.height, _STATE->font_normal, test, &startx_pos1, &starty_pos,
			fontsize);

		//Knob *Knob = (Knob*) info->reference;
		float offset = miditarget.offset;

		const char* formatvalue = ValueView::formatvalues[miditarget.digits];

		char text[100]{};

		const float value = miditarget.toDisplay(_appState->sr, reference);


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

		int32_t lenwithoutdigits = 1;
		int32_t x = std::max(std::abs(miditarget.getMin(_STATE->sr)), std::abs(miditarget.getMax(_STATE->sr)));
		while (x /= 10)
			lenwithoutdigits++;
		int32_t negone = (miditarget.getMin(_STATE->sr) < 0 || miditarget.getMax(_STATE->sr) < 0);

		int32_t len = lenwithoutdigits + negone + miditarget.digits;
		auto lenval = (int)(miditarget.valuename == nullptr || miditarget.valuename[0] == ' ') ? 0
			:
			strlen(miditarget.valuename);
		float width = (len + lenval + 4) * _STATE->maxCharWidtht2;


		float ms = 1000.f / powf(10, lfo->freq() * .05f) *
			lfo->gp(LFOENVX0 + active);
		float seconds = ms / 1000.f;
		float minutes = seconds / 60.f;
		float hours = minutes / 60.f;

		auto seconds_floorf = (unsigned int)floorf(seconds);
		auto msrest = (unsigned int)((seconds - seconds_floorf) * 1000.f);
		auto minutes_floorf = (unsigned int)floorf(minutes);
		auto secondsrest = (unsigned int)((minutes - minutes_floorf) * 60.f);
		auto hours_floorf = (unsigned int)floorf(hours);
		auto minutesrest = (unsigned int)((hours - hours_floorf) * 60.f);

		char output2[50];
		output2[0] = '\0';

		snprintf(output2, 50, "%02d : %02d : %03d", minutesrest, secondsrest, msrest);
		SkFont font(_STATE->font_normal);

		font.setSize(fontsize);
		width += measureWidth(font, "00 : 00 : 000 --") + _STATE->maxCharWidtht2;


		float sx = par.startx + (par.width - width) * .5f;
		if (sx < 0)
			sx = 0;
		else if (sx + width > _STATE->windowWidth)
			sx = _STATE->windowWidth - width;
		float sy = par.stopy;
		if (sy + _STATE->textsize1 > _STATE->windowHeight)
			sy = par.starty - _STATE->textsize1;

		auto c = _STATE->graphics.getCanvas(val.windex, sx, sy, width, _STATE->textsize1);
		if (!c)
			return;
		c->clear(skcol::bg);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::text);

		c->drawSimpleText(output2, strlen(output2),
			SkTextEncoding::kUTF8,
			_STATE->maxCharWidtht2 * .5, starty_pos,
			font, paint);

		float sxval = width;
		if (miditarget.valuename != nullptr && miditarget.valuename[0] != ' ') {
			sxval -= (strlen(miditarget.valuename) + 2) * _STATE->maxCharWidtht2;
			c->drawSimpleText(miditarget.valuename, strlen(miditarget.valuename),
				SkTextEncoding::kUTF8,
				sxval, starty_pos,
				font, paint);
		}

		c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, sxval - measureWidth(font, text) - _STATE->maxCharWidtht2, starty_pos, font,
			paint);


	}) {

}

GrainEnvEditor::GrainEnvEditor(tsl::AppState* appState) : View(appState), valueView(appState,
	[&val = valueView, &par = *this, &_act = _activeSeg]() {
		auto _appState = val._appState;
		if (val.timer.elapsed() > 1.) {
			val.perm = false;
			_STATE->graphics.deleteWindow(val.windex);
			return;
		}

		auto tindex = _STATE->active_track.load();
		int32_t seg = _act.load();
		if (seg < 0 || seg > 8)
			return;

		float pos = _STATE->params[tindex][GRAINENVX0 + seg].load();
		float val1 = _STATE->params[tindex][GRAINENVY0 + seg].load();
		float ms = _STATE->params[tindex][GRAINSIZE].load() / (float)_STATE->sr * pos * 1000.f;
		float db = -60 + 60 * val1;

		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9);
		const char* test =
			" 1000.0 ms -60.0 dB ";

		float startx_pos1, starty_pos;//"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
		auto width = measureTextFixed(100, _STATE->textsize1, _STATE->font_normal, test, &startx_pos1, &starty_pos,
			_STATE->textsize2 * .9);
		float sx = par.startx + (par.width - width) * .5f;
		if (sx < 0)
			sx = 0;
		else if (sx + width > _STATE->windowWidth)
			sx = _STATE->windowWidth - width;
		float sy = par.stopy;
		if (sy + _STATE->textsize1 > _STATE->windowHeight)
			sy = par.starty - _STATE->textsize1;

		auto c = _STATE->graphics.getCanvas(val.windex, sx, sy, width, _STATE->textsize1);
		if (!c)
			return;
		c->clear(skcol::bg);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::text);

		char text[20];
		snprintf(text, 20, "%.1f dB ", db);
		c->drawSimpleText(text, strlen(text),
			SkTextEncoding::kUTF8,
			width - measureWidth(font, text), starty_pos,
			font, paint);
		auto offset = width - measureWidth(font, "  -60.0 dB ");
		snprintf(text, 20, "%.1f ms", ms);
		c->drawSimpleText(text, strlen(text),
			SkTextEncoding::kUTF8,
			offset - measureWidth(font, text), starty_pos,
			font, paint);
	}) {

}


void GrainEnvEditor::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();
	int32_t action = event.action;
	// float _xpos1 = event.x1;// - view->startx;
	// const float _ypos1 = event.y1;// - view->starty;
	float w = width * zoom.load();
	float h = height;

	float segsize = width * .2f;
	float segsized2 = segsize * .5f;
	float xpos = event.x - startx;
	float ypos = event.y - starty;
	int32_t pointerid = event.pointer_id;
	//LOGE("%f %f %f %f", _xpos, xpos, _ypos, ypos);
	const float mindist = 0;
	const int32_t nsegs = _STATE->params[tindex][GRAINNSEGS].load();
	switch (action) {
	case ACTION_DOWN: {
		float z = zoom.load(), p = pos.load();
		if (z < 1.0) {
			float points = width * z;
			float diff = (width - points) * .5f;
			for (int32_t i = 0; i <= nsegs; i++) {
				auto x = _STATE->params[tindex][GRAINENVX0 + i].load(), y = _STATE->params[tindex][GRAINENVY0 +
					i].load();
				if (xpos >
					diff + x * points - segsized2 &&
					xpos < diff + x * points + segsized2 &&
					ypos >(1.0 - y) * h -
					segsized2 &&
					ypos < (1.0 - y) * h + segsized2) {
					inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, i);
					_activeSeg = i;
					valueView.addDraw();
					_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
					return;
				}
			}
		}
		else {
			for (int32_t i = 0; i <= nsegs; i++) {
				const auto x = _STATE->params[tindex][GRAINENVX0 + i].load(), y = _STATE->params[tindex][
					GRAINENVY0 + i].load();
					if (xpos + width * p * z >
						x * w - segsized2 &&
						xpos + width * p * z < x * w + segsized2 &&
						ypos >(1.0 - y) * h -
						segsized2 &&
						ypos < (1.0 - y) * h + segsized2) {
						inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, i);
						_activeSeg = i;
						valueView.addDraw();
						_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
						return;
					}
			}
		}
		inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINDRAG, 0);
		inputstate.checkZoom();
		break;
	}
	case ACTION_MOVE: {
		if (auto pt = inputstate.getById(pointerid)) {
			if (pt->mode == pmode_t::WINPOINTER) {
				float diffx = (xpos - pt->xpos) / w;
				float diffy = (pt->ypos - ypos) / h;

				float val = _STATE->params[tindex][GRAINENVY0 + pt->target].load();
				//LOGE("%d %f %f", track->active_seg, val, diffy);
				val += diffy;
				if (val < 0)
					val = 0;
				if (val > 1.0f)
					val = 1.0f;
				_STATE->params[tindex][GRAINENVY0 + pt->target] = val;

				if (_STATE->params[tindex][GRAINJOIN].load() == 1.0) {
					if (pt->target == 0)
						_STATE->params[tindex][GRAINENVY0 +
						nsegs] = val;
					else if (pt->target == nsegs)
						_STATE->params[tindex][GRAINENVY0] = val;
				}

				if (pt->target != 0 &&
					pt->target != nsegs) {
					val = _STATE->params[tindex][GRAINENVX0 + pt->target];
					val += diffx;
					if (val < 0)
						val = 0;
					if (val > 1.0f)
						val = 1.0f;
					float prev, next;
					if (pt->target == 1)
						prev = 0.0f;
					else
						prev = _STATE->params[tindex][GRAINENVX0 + pt->target - 1].load();
					if (pt->target == nsegs - 1)
						next = 1.0f;
					else
						next = _STATE->params[tindex][GRAINENVX0 + pt->target + 1].load();
					//LOGE("Mindist %f n-p %f active %f val %f", mindist, next -prev, track->positions[active_seg], val);

					if (!((next - val) < 0 || (val - prev) < mindist))
						// val -= diffx;
						_STATE->params[tindex][GRAINENVX0 + pt->target] = val;
				}
				_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
				_STATE->params[tindex][RECOMPUTEGRAINENV3] = 1.0;
				_activeSeg = pt->target;
				valueView.addDraw();
			}
			else if (pt->mode == pmode_t::WINDRAG) {
				float z = zoom.load(), p = pos.load();

				//float diffy = (pt.ypos - ypos) / h;
				float diffx = (xpos - pt->xpos) / w;
				if (z >= 1.f) {
					float _pos = p - diffx;
					float max = 1 - 1 / z;
					pos = _pos < 0 ? 0 : (_pos > max ? max : _pos);
				}
				else
					pos = 0;
			}
			else if (pt->mode == pmode_t::WINZOOM) {
				auto theother = inputstate.getTheOther(pt);
				if (theother == nullptr) {
					pt->xpos = xpos;
					pt->ypos = ypos;
					break;
				}
				float xposold = pt->xpos;
				float yposold = pt->ypos;
				float olddist = spacing(xposold, theother->xpos, yposold, theother->ypos);
				float newdist = spacing(xpos, theother->xpos, ypos, theother->ypos);
				float _zoom = newdist / olddist * zoom.load();
				if (_zoom < .8f)
					_zoom = .8f;
				if (_zoom > 10.f)
					_zoom = 10.f;
				float diff = (1 - _zoom / zoom.load()) / _zoom;
				float newpos =
					pos.load() - diff * ((xposold + xpos) * .5f / width);
				float max = 1 - 1 / _zoom;
				pos = newpos < 0 ? 0 : (newpos > max ? max : newpos);
				zoom = _zoom;
			}
			pt->xpos = xpos;
			pt->ypos = ypos;
		}
		break;
	}
	case ACTION_UP: {
		bool doubleClick = false;
		if (inputstate.lastpointer == pointerid &&
			inputstate.timer.elapsedReplace() < .3) {
			doubleClick = true;

			auto positions = std::vector<std::pair<int, double>>();
			positions.reserve(nsegs);



			float inc = 1.f / (float)nsegs;
			float step = inc * _STATE->params[tindex][GRAINQUANT].load();

			for (int32_t i = 1; i < nsegs; i++) {
				float center = i * inc;
				float start = center - inc;
				float _pos = _STATE->params[tindex][GRAINENVX0 + i].load();
				while (start < center + inc) {
					float dist = DISTANCE(start, _pos);
					if (dist > 0 && dist < step) {
						auto val = start + (dist < step / 2 ? 0 : step);
						_STATE->params[tindex][GRAINENVX0 + i] = val;
						positions.push_back({ GRAINENVX0 + i,val });

						break;
					}
					start += step;
				}
			}
			if (positions.size() > 0) {
				_DATA->snapShot.add_task([this, tindex, ppos = std::move(positions)]() {
					auto groupId = _DATA->snapShot.nextGroupId();
					auto ev = tsl::parameters::Event::createTextEvent(tindex, "GRAINENV EDIT", 0, 0, groupId);
					std::lock_guard lk(_DATA->snapShot);
					_DATA->snapShot.addEvent(ev);
					for (auto pos : ppos) {
						auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, pos.first, pos.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
						ev.groupId = groupId;
						_DATA->snapShot.addEvent(ev);
					}
					ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RERENDERGRAINENV3, 1.0, 0, groupId);
					_DATA->snapShot.addEvent(ev);
					ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RECOMPUTEGRAINENV3, 1.0, 0, groupId); ;
					_DATA->snapShot.addEvent(ev);
					});
			}

			_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
		}
		if (auto pt = inputstate.getById(pointerid)) {
			if (pt->mode == pmode_t::WINPOINTER && pt->target == _activeSeg.load()) {
				_activeSeg = -1;
				if (!doubleClick && (pt->xposWhenCreated != xpos || pt->yposWhenCreated != ypos)) {
					auto poss = std::vector<std::pair<int, double>>();
					uint16_t xParam = GRAINENVX0 + pt->target;
					uint16_t yParam = GRAINENVY0 + pt->target;
					auto valX = _STATE->params[tindex][xParam].load();
					auto valY = _STATE->params[tindex][yParam].load();
					poss.push_back({ xParam, valX });
					poss.push_back({ yParam, valY });
					if (_STATE->params[tindex][GRAINJOIN].load() == 1.0) {
						if (pt->target == 0)
							poss.push_back({ GRAINENVY0 + nsegs, _STATE->params[tindex][GRAINENVY0 + nsegs].load() });
						else if (pt->target == nsegs)
							poss.push_back({ GRAINENVY0, _STATE->params[tindex][GRAINENVY0].load() });
					}
					_DATA->snapShot.add_task([this, tindex, pos = std::move(poss)]() {
						auto groupId = _DATA->snapShot.nextGroupId();
						auto ev = tsl::parameters::Event::createTextEvent(tindex, "GRAINENV2 EDIT", 0, 0, groupId);
						std::lock_guard lk(_DATA->snapShot);
						_DATA->snapShot.addEvent(ev);
						for (auto& p : pos) {
							auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, p.first, p.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
							_DATA->snapShot.addEvent(ev);
						}
						ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RECOMPUTEGRAINENV3, 1.0, 0, groupId, tsl::parameters::Event::History);
						_DATA->snapShot.addEvent(ev);
						ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RERENDERGRAINENV3, 1.0, 0, groupId, tsl::parameters::Event::History); ;
						_DATA->snapShot.addEvent(ev);
						});
				}



			}
			inputstate.removePointer(pointerid);
			inputstate.lastpointer = pointerid;
			inputstate.checkZoom();
			_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
		}

		break;
	}
	default:
		break;
	}
	//LOGE("%ld %f %f %f", AMotionEvent_getPointerCount(event), zoom.load(), pos.load(), olddist);
}

#if defined USE_IMGUI
#include <imgui.h>
void LfoEnvEditor::render(void* context) {
	auto tindex = _STATE->active_track.load();
	LFO* lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	auto table = lfo->drawTable;

	const int32_t nsegs = lfo->segments();
	std::vector<MYFLOAT> xx(nsegs + 1), yy(nsegs + 1);
	for (int32_t i = 0; i <= nsegs; i++) {
		xx[i] = *(lfo->getpos0() + i);
		yy[i] = *(lfo->getval0() + i);
	}

	if (lfo->gp(LFOREDRAW) == 1.0) {
		lfo->store(LFOREDRAW, 0.f);
		tsl::envelope::compute[lfo->editfunc()](table, LFO_TBL_SIZE, xx.data(),
			yy.data(), nsegs, true);
	}
	//ImGui::SetNextWindowPos(ImVec2(view->startx - 2, view->starty), ImGuiCond_Always);
	//ImGui::SetNextWindowSize(ImVec2(view->width + 4, view->height), ImGuiCond_Always);
	//ImGui::GetStyle().AntiAliasedFill = true;
	//ImGui::GetStyle().AntiAliasedLines = true;
	bool test = true;

	if (!ImGui::Begin("LFO EDIT", &test,
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBringToFrontOnFocus)) {
		ImGui::End();
		return;
	}

	ImVec2 canvas_size = { (float)width,
						  (float)height };//ImGui::GetContentRegionAvail();        // Resize canvas to what's available
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
	ImVec2 canvas_pos = { (float)startx,
						 (float)starty };//ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->AddRectFilled(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::bg));

	float h = canvas_size.y;
	const float radius = _STATE->circleradius;

	const float zoom = lfo->zoom();
	const float pos = lfo->pos();

	if (zoom >= 1.0) {
		draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y),
			ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		float w = canvas_size.x * zoom;


		float inc = lw / w * (LFO_TBL_SIZE - 1);
		float step_point = (LFO_TBL_SIZE - 1) * pos;
		float length_line = w * .1f;
		float hit = w * lfo->phs();
		float hithead = hit + (ISNEG(lfo->dir()) ? length_line : 0);
		float hittail = hit - (ISNEG(lfo->dir()) ? 0 : length_line);


		for (float x = 0; x < canvas_size.x; x += lw) {
			float end = step_point + inc;
			if (end > LFO_TBL_SIZE - 1)
				end = LFO_TBL_SIZE - 1;
			draw_list->AddLine(
				ImVec2(canvas_pos.x + x, canvas_pos.y + h - table[(int)step_point] * h),
				ImVec2(canvas_pos.x + x + lw, canvas_pos.y + h - table[(int)end] * h),
				x >= hittail && x <= hithead ? IM_Colour(skcol::fg)
				: IM_Colour(skcol::orange),
				lw);
			step_point = end;
		}


		for (int32_t point = 0; point < nsegs + 1; point++) {
			draw_list->AddCircleFilled(
				ImVec2(canvas_pos.x + lw + w * xx[point] - w * pos,
					canvas_pos.y + h + lw - h * yy[point]),
				radius, IM_Colour(
					point == _activeSeg ? IM_COL32(127, 127, 127, 127)
					: skcol::blue_transparent));
		}
	}
	else {

		float points = canvas_size.x * zoom;
		float diff = (canvas_size.x - points) * .5f;

		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff, canvas_pos.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y + canvas_size.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff,
				canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y),
			ImVec2(canvas_pos.x + diff, canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + canvas_size.x - diff, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff,
				canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);


		float inc = lw / points * (LFO_TBL_SIZE - 1);
		float step_point = 0;
		float length_line = points * .1f;
		float hit = diff + points * lfo->phs();
		float hithead = hit + (ISNEG(lfo->dir()) ? length_line : 0);
		float hittail = hit - (ISNEG(lfo->dir()) ? 0 : length_line);

		for (float x = diff; x < canvas_size.x - diff; x += lw) {
			float end = step_point + inc;
			if (end > LFO_TBL_SIZE - 1)
				end = LFO_TBL_SIZE - 1;
			draw_list->AddLine(
				ImVec2(canvas_pos.x + x, canvas_pos.y + h - table[(int)step_point] * h),
				ImVec2(canvas_pos.x + x + lw, canvas_pos.y + h - table[(int)end] * h),
				x >= hittail && x <= hithead ? IM_Colour(skcol::fg)
				: IM_Colour(skcol::orange),
				lw);
			step_point = end;
		}

		for (int32_t point = 0; point < nsegs + 1; point++) {
			draw_list->AddCircleFilled(
				ImVec2(canvas_pos.x + diff + points * xx[point],
					canvas_pos.y + h - h * yy[point]),
				radius, IM_Colour(
					point == _activeSeg.load() ? IM_COL32(127, 127, 127, 127)
					: skcol::blue_transparent));
		}
	}


	ImGui::End();
}


void GrainEnvEditor::render(void* context) {
	auto tindex = _STATE->active_track.load();
	const int32_t active_seg = _activeSeg.load();
	//ImGui::SetNextWindowPos(ImVec2(view->startx - 2, view->starty), ImGuiCond_Always);
	//ImGui::SetNextWindowSize(ImVec2(view->width + 4, view->height), ImGuiCond_Always);
	//ImGui::GetStyle().AntiAliasedFill = true;
	//ImGui::GetStyle().AntiAliasedLines = true;
	bool test = true;

	if (!ImGui::Begin(_DATA->tracks[tindex]->name, &test,
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBringToFrontOnFocus)) {
		ImGui::End();
		return;
	}

	ImVec2 canvas_size = { (float)width,
						  (float)height };//ImGui::GetContentRegionAvail();        // Resize canvas to what's available
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
	ImVec2 canvas_pos = { (float)startx,
						 (float)starty };//ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::bg));

	//draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y +
	//                                                                         canvas_size.y));// clip lines within the canvas (if we resize it, etc.)

	float h = canvas_size.y;
	const float radius = _STATE->circleradius;


	const int32_t nsegs = _STATE->params[tindex][GRAINNSEGS].load();
	std::vector<MYFLOAT> x(nsegs + 1), y(nsegs + 1);
	for (int32_t i = 0; i <= nsegs; i++) {
		x[i] = _STATE->params[tindex][GRAINENVX0 + i].load();
		y[i] = _STATE->params[tindex][GRAINENVY0 + i].load();
	}

	if (zoom.load() >= 1.0) {
		draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y),
			ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		float w = canvas_size.x * zoom;


		float inc = 1.f / w * (width - 1);
		float step_point = (width - 1) * pos;

		std::vector<MYFLOAT> table(width);


		tsl::envelope::compute[(int)_STATE->params[tindex][GRAINCURVE].load()](table.data(), width,
			x.data(),
			y.data(),
			nsegs,
			true);
		for (float x = 0; x < canvas_size.x; x++) {
			float end = step_point + inc;
			if (end > width - 1)
				end = width - 1;
			draw_list->AddLine(
				ImVec2(canvas_pos.x + x,
					canvas_pos.y + h - table[(int)step_point] * h),
				ImVec2(canvas_pos.x + x + 1, canvas_pos.y + h - table[(int)end] * h),
				IM_Colour(skcol::orange),
				lw);
			step_point = end;
		}

		for (int32_t point = 0; point <= nsegs; point++) {
			draw_list->AddCircleFilled(
				ImVec2(canvas_pos.x + w * x[point] - w * pos,
					canvas_pos.y + h - h * y[point]),
				radius, IM_Colour(
					point == active_seg ? IM_COL32(127, 127, 127, 127)
					: skcol::blue_transparent));
		}
	}
	else {

		int32_t points = canvas_size.x * zoom.load();
		int32_t diff = (canvas_size.x - points) / 2;
		int32_t inc = 1;
		int32_t step_point = 0;
		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff, canvas_pos.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y + canvas_size.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff,
				canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + diff, canvas_pos.y),
			ImVec2(canvas_pos.x + diff, canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);
		draw_list->AddLine(ImVec2(canvas_pos.x + canvas_size.x - diff, canvas_pos.y),
			ImVec2(canvas_pos.x + canvas_size.x - diff,
				canvas_pos.y + canvas_size.y),
			IM_Colour(skcol::grey), 1);


		std::vector<MYFLOAT> table(points);

		tsl::envelope::compute[(int)_STATE->params[tindex][GRAINCURVE].load()](table.data(), points,
			x.data(),
			y.data(),
			nsegs,
			true);
		for (float x = diff; x < canvas_size.x - diff; x++) {
			int32_t end = step_point + inc;
			if (end > points - 1)
				end = points - 1;
			draw_list->AddLine(
				ImVec2(canvas_pos.x + x,
					canvas_pos.y + h - table[(int)step_point] * h),
				ImVec2(canvas_pos.x + x + 1, canvas_pos.y + h - table[(int)end] * h),
				IM_Colour(skcol::orange),
				lw);
			step_point = end;
		}


		for (int32_t point = 0; point <= nsegs; point++) {
			draw_list->AddCircleFilled(
				ImVec2(canvas_pos.x + lw + diff + points * x[point],
					canvas_pos.y + h - h * y[point]),
				radius, IM_Colour(
					point == active_seg ? IM_COL32(127, 127, 127, 127)
					: skcol::blue_transparent));
		}
	}


	ImGui::End();
}
#else
void LfoEnvEditor::render(void* context) {
	int tindex = _STATE->active_track.load();

	LFO* lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	const int32_t nsegs = lfo->segments();

	// Load control points
	float xx[32], yy[32];
	for (int i = 0; i <= nsegs; i++) {
		xx[i] = *(lfo->getpos0() + i);
		yy[i] = *(lfo->getval0() + i);
	}

	if (lfo->redrawExchange()) {
		// Fetch LFO state
		tsl::envelope::compute<float>[lfo->editfunc()](drawBuf, LFO_TBL_SIZE,
			xx, yy, nsegs, true);

	}
	auto* canvas = static_cast<SkCanvas*>(context);
	canvas->clipRect(SkRect::MakeXYWH(parent->startx, parent->starty, parent->width, parent->height), SkClipOp::kIntersect);

	parent->flush(canvas);


	auto h = (float)height;
	float z = lfo->zoom();
	float pos = lfo->pos();
	float lw = this->lw;
	float hitHead, hitTail;

	// Build path for waveform
	SkPath path;
	path.setFillType(SkPathFillType::kWinding);
	SkPaint paintRec;
	paintRec.setStroke(1.0);
	paintRec.setAntiAlias(true);
	if (z >= 1.0f) {
		flush(canvas, skcol::wbg);
		paintRec.setStyle(SkPaint::kStroke_Style);
		paintRec.setColor(skcol::grey);
		canvas->drawLine(startx, starty, startx + width, starty, paintRec);
		canvas->drawLine(startx, starty + height, startx + width, starty + height, paintRec);

		float wZoom = width * z;
		float inc = lw / wZoom * (LFO_TBL_SIZE - 1);
		float sp = (LFO_TBL_SIZE - 1) * pos;
		for (float x = 0; x < width; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - drawBuf[int(sp)] * h;
			float y1 = starty + h - drawBuf[int(end)] * h;
			path.moveTo(startx + x, y0);
			path.lineTo(startx + x + lw, y1);
			sp = end;
		}
		// compute hit region
		float length_line = wZoom * .1f;
		float hit = wZoom * lfo->phs();
		hitHead = hit + (lfo->dir() < 0 ? length_line : 0);
		hitTail = hit - (lfo->dir() < 0 ? 0 : length_line);
	}
	else {
		int pts = int(width * z);
		int diff = (width - pts) / 2;
		paintRec.setStyle(SkPaint::kStroke_Style);
		paintRec.setColor(skcol::grey);
		canvas->drawRect(SkRect::MakeXYWH(startx + diff, starty, width - diff * 2, height), paintRec);
		paintRec.setStyle(SkPaint::kStroke_Style);
		paintRec.setColor(skcol::grey);
		canvas->drawRect(SkRect::MakeXYWH(startx + diff, starty, width - diff * 2, height), paintRec);

		float inc = lw / float(pts) * (LFO_TBL_SIZE - 1);
		float sp = 0;
		for (float x = diff; x < width - diff; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - drawBuf[int(sp)] * h;
			float y1 = starty + h - drawBuf[int(end)] * h;
			path.moveTo(startx + x, y0);
			path.lineTo(startx + x + lw, y1);
			sp = end;
		}
		// hit region
		float length_line = pts * .1f;
		float hit = diff + pts * lfo->phs();
		hitHead = hit + (lfo->dir() < 0 ? length_line : 0);
		hitTail = hit - (lfo->dir() < 0 ? 0 : length_line);
	}

	// Stroke path
	SkPaint linePaint;
	linePaint.setStyle(SkPaint::kStroke_Style);
	linePaint.setStrokeWidth(lw);
	linePaint.setAntiAlias(true);

	// Draw segments with conditional color
	// Skia doesn�t support per-segment color in a single path; draw in loop
	if (z >= 1.0f) {
		float wZoom = width * z;
		float sp = (LFO_TBL_SIZE - 1) * pos;
		float inc = lw / wZoom * (LFO_TBL_SIZE - 1);
		for (float x = 0; x < width; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - drawBuf[int(sp)] * h;
			float y1 = starty + h - drawBuf[int(end)] * h;
			bool isHit = x >= hitTail && x <= hitHead;
			linePaint.setColor(isHit ? skcol::fg : skcol::orange);
			canvas->drawLine(startx + x, y0,
				startx + x + lw, y1,
				linePaint);
			sp = end;
		}
	}
	else {
		int pts = int(width * z);
		int diff = (width - pts) / 2;
		float sp = 0;
		float inc = lw / float(pts) * (LFO_TBL_SIZE - 1);
		for (float x = diff; x < width - diff; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - drawBuf[int(sp)] * h;
			float y1 = starty + h - drawBuf[int(end)] * h;
			bool isHit = x >= hitTail && x <= hitHead;
			linePaint.setColor(isHit ? skcol::fg : skcol::orange);
			canvas->drawLine(startx + x, y0,
				startx + x + lw, y1,
				linePaint);
			sp = end;
		}
	}
	//_STATE->pool.release(table); // release table back to pool
	// Draw control circles
	SkPaint circle;
	circle.setStyle(SkPaint::kFill_Style);
	for (int i = 0; i <= nsegs; i++) {
		float cx, cy;
		if (z >= 1.0f)
			cx = startx + lw + (width * z) * xx[i] - (width * z) * pos;
		else {
			int pts = int(width * z);
			int diff = (width - pts) / 2;
			cx = startx + diff + pts * xx[i];
		}
		cy = starty + h + lw - h * yy[i];
		circle.setColor(i == _activeSeg ? SkColorSetARGB(127, 127, 127, 127) : skcol::blue_transparent);
		canvas->drawCircle(cx, cy, _STATE->circleradius, circle);
	}

}

void GrainEnvEditor::render(void* context) {
	auto tindex = _STATE->active_track.load();

	if (_STATE->params[tindex][RERENDERGRAINENV3].exchange(0.0, std::memory_order_acq_rel) == 0.0)return;

	auto table = _STATE->pool.acquire<double>(LFO_TBL_SIZE);
	if (!table) {
		LOGE("Pool returned nullptr!");
		return; // allocation failed, skip rendering
	}
	SkCanvas* canvas = static_cast<SkCanvas*>(context);
	canvas->clipRect(SkRect::MakeXYWH(parent->startx, parent->starty, parent->width, parent->height), SkClipOp::kIntersect);

	parent->flush(canvas);

	// Fetch state
	int activeSeg = _activeSeg.load();
	int32_t nsegs = _STATE->params[tindex][GRAINNSEGS].load();
	float h = (float)height;
	float w = (float)width;
	float z = zoom.load();
	float pos = this->pos; // assume pos member
	float lw = this->lw;   // line width
	float radius = (float)_STATE->circleradius;

	// Load envelope points
	double ex[32], ey[32];
	for (int i = 0; i <= nsegs; i++) {
		ex[i] = _STATE->params[tindex][GRAINENVX0 + i].load();
		ey[i] = _STATE->params[tindex][GRAINENVY0 + i].load();
	}

	// Compute table
	//int points = (z >= 1.0f) ? width.load() : width * z;
	auto curve = (int)_STATE->params[tindex][GRAINCURVE].load();
	tsl::envelope::compute<MYFLOAT>[curve](
		table, LFO_TBL_SIZE, ex, ey, nsegs, true);

	// Build path
	SkPath path;
	path.setFillType(SkPathFillType::kWinding);
	SkPaint paintRec;
	paintRec.setStroke(1.0);
	paintRec.setAntiAlias(true);



	if (z >= 1.0f) {
		flush(canvas, skcol::wbg);
		paintRec.setColor(skcol::grey);
		paintRec.setStyle(SkPaint::kStroke_Style);
		canvas->drawLine(startx, starty, startx + width, starty, paintRec);
		canvas->drawLine(startx, starty + height, startx + width, starty + height, paintRec);

		float wZoom = width * z;
		float inc = lw / wZoom * (LFO_TBL_SIZE - 1);
		float sp = (LFO_TBL_SIZE - 1) * pos;
		for (float x = 0; x < width; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - table[int(sp)] * h;
			float y1 = starty + h - table[int(end)] * h;
			path.moveTo(startx + x, y0);
			path.lineTo(startx + x + lw, y1);
			sp = end;
		}

	}
	else {
		int pts = int(width * z);
		int diff = (width - pts) / 2;
		paintRec.setStyle(SkPaint::kStroke_Style);
		paintRec.setColor(skcol::grey);
		canvas->drawRect(SkRect::MakeXYWH(startx + diff, starty, width - diff * 2, height), paintRec);
		paintRec.setStyle(SkPaint::kStroke_Style);
		paintRec.setColor(skcol::grey);
		canvas->drawRect(SkRect::MakeXYWH(startx + diff, starty, width - diff * 2, height), paintRec);

		float inc = lw / float(pts) * (LFO_TBL_SIZE - 1);
		float sp = 0;
		for (float x = diff; x < width - diff; x += lw) {
			float end = sp + inc;
			if (end > LFO_TBL_SIZE - 1) end = LFO_TBL_SIZE - 1;
			float y0 = starty + h - table[int(sp)] * h;
			float y1 = starty + h - table[int(end)] * h;
			path.moveTo(startx + x, y0);
			path.lineTo(startx + x + lw, y1);
			sp = end;
		}
	}
	_STATE->pool.release(table); // release table back to pool
	// Stroke path
	SkPaint paint;
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);
	paint.setColor(skcol::orange);
	paint.setAntiAlias(true);
	canvas->drawPath(path, paint);

	// Draw circles
	SkPaint circleFill;
	circleFill.setStyle(SkPaint::kFill_Style);
	for (int i = 0; i <= nsegs; i++) {
		float cx, cy;
		if (z >= 1.0f) { cx = startx + w * z * ex[i] - w * z * pos; if (cx < startx - radius)continue; }
		else { int diff = (width - width * z) / 2; cx = startx + diff + width * z * ex[i]; }
		cy = starty + h - h * ey[i];
		circleFill.setColor(i == activeSeg ? SkColorSetARGB(127, 127, 127, 127) : skcol::blue_transparent);
		canvas->drawCircle(cx, cy, radius, circleFill);
	}

}
#endif // USE_IMGUI

static const char* lfoUpdateNames[] = {
	"LFO1 EDIT",
	"LFO2 EDIT",
	"LFO3 EDIT"
};


void LfoEnvEditor::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();
	LFO* lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	const int32_t action = event.action;
	const int32_t pointerid = event.pointer_id;

	const float xpos = event.x - startx;
	const float ypos = event.y - starty;

	//LOGE("%f %f %f %f", _xpos, xpos, _ypos, ypos);
	const float w = width * lfo->zoom();
	const float h = height;
	const float segsize = w * .1f;
	const float segsized2 = segsize * .5f;
	const int32_t segments = lfo->segments();
	const float mindist = 0;

	switch (action) {
	case ACTION_DOWN: {
		const float zoom = lfo->zoom();
		const float position = lfo->pos();
		if (zoom < 1.0) {
			float points = width * zoom;
			float diff = (width - points) / 2;
			for (int32_t i = 0; i <= segments; i++) {
				float pos = *(lfo->getpos0() + i);
				float val = *(lfo->getval0() + i);
				if (xpos >
					diff + pos * points - segsized2 &&
					xpos < diff + pos * points + segsized2 &&
					ypos >(1.0 - val) * h -
					segsized2 &&
					ypos < (1.0 - val) * h + segsized2) {
					_activeSeg = i;
					valueView.addDraw();
					inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, i);
					lfo->store(LFOREDRAW, 1.0);
					return;
				}
			}
		}
		else {
			for (int32_t i = 0; i <= segments; i++) {
				float pos = *(lfo->getpos0() + i);
				float val = *(lfo->getval0() + i);
				if (xpos + width * position * zoom >
					pos * w - segsized2 &&
					xpos +
					width * position * zoom< pos * w + segsized2 &&
					ypos >(1.0 - val) * h -
					segsized2 &&
					ypos < (1.0 - val) * h + segsized2) {
					_activeSeg = i;
					valueView.addDraw();
					inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINPOINTER, i);
					lfo->store(LFOREDRAW, 1.0);
					return;
				}
			}
		}
		inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINDRAG, 0);
		inputstate.checkZoom();
		break;
	}
	case ACTION_MOVE: {
		if (auto pt = inputstate.getById(pointerid)) {
			if (pt->mode == pmode_t::WINPOINTER) {
				int32_t active_seg = pt->target;
				float diffx = (xpos - pt->xpos) / w;
				float diffy = (pt->ypos - ypos) / h;
				float val = val = *(lfo->getval0() + active_seg);
				//LOGE("%d %f %f", lfo->active_seg, val, diffy);
				val += diffy;
				val = std::clamp(val, 0.0f, 1.0f);
				lfo->store(LFOENVY0 + active_seg, val);
				if (lfo->joinends()) {
					if (active_seg == 0)
						lfo->store(LFOENVY0 + segments, val);
					else if (active_seg == segments)
						lfo->store(LFOENVY0, val);
				}

				if (active_seg != 0 && active_seg != segments) {
					val = *(lfo->getpos0() + active_seg);
					val += diffx;
					val = std::clamp(val, 0.0f, 1.0f);
					float prev, next;
					if (active_seg == 1)
						prev = 0.0f;
					else
						prev = *(lfo->getpos0() + active_seg - 1);
					if (active_seg == segments - 1)
						next = 1.0f;
					else
						next = *(lfo->getpos0() + active_seg + 1);
					if (!((next - val) < 0 || (val - prev) < mindist))
						lfo->store(LFOENVX0 + active_seg, val);
				}
				lfo->store(LFORECOMPUTE, 1.0);
				lfo->store(LFOREDRAW, 1.0);
				_activeSeg = pt->target;
				valueView.addDraw();
			}
			else if (pt->mode == pmode_t::WINDRAG) {
				//float diffy = (pt.ypos - ypos) / h;
				float diffx = (xpos - pt->xpos) / w;
				if (lfo->zoom() >= 1.f) {
					float pos = lfo->pos() - diffx;
					float max = 1 - 1 / lfo->zoom();
					lfo->store(LFOPOS, pos < 0 ? 0 : (pos > max ? max : pos));
				}
				else
					lfo->store(LFOPOS, 0);
			}
			else if (pt->mode == pmode_t::WINZOOM) {
				auto theother = inputstate.getTheOther(pt);
				if (theother == nullptr) {
					pt->xpos = xpos;
					pt->ypos = ypos;
					break;
				}
				float xposold = pt->xpos;
				float yposold = pt->ypos;
				float olddist = theother->distanceTo(xposold, yposold);
				float newdist = theother->distanceTo(xpos, ypos);
				const float zoom = lfo->zoom();
				float _zoom = newdist / olddist * zoom;
				if (_zoom < .8f)
					_zoom = .8f;
				if (_zoom > 10.f)
					_zoom = 10.f;
				float diff = (1 - _zoom / zoom) / _zoom;
				float newpos =
					lfo->pos() - diff * ((xposold + xpos) * .5f / width);
				float max = 1 - 1 / _zoom;
				lfo->store(LFOPOS, newpos < 0 ? 0 : (newpos > max ? max : newpos));
				lfo->store(LFOZOOM, _zoom);
			}
			pt->xpos = xpos;
			pt->ypos = ypos;
		}
		break;
	}
	case ACTION_UP: {
		bool doubleClick = false;
		if (inputstate.lastpointer == pointerid &&
			inputstate.timer.elapsedReplace() < .3) {
			doubleClick = true;
			float inc = 1.f / (float)segments;
			float step = inc * lfo->quant();
			auto positions = std::vector<std::pair<int, double>>();
			positions.reserve(segments);
			for (int32_t i = 1; i < segments; i++) {
				float center = i * inc;
				float start = center - inc;
				auto pos = (lfo->getpos0() + i);
				while (start < center + inc) {
					float dist = DISTANCE(start, *pos);
					if (dist > 0 && dist < step) {
						auto val = start + (dist < step / 2 ? 0 : step);
						*(pos) = val;
						positions.push_back({ LFO1ENVX0 + i + lfo->index * LFONUMPARAMS,val });
						break;
					}
					start += step;
				}
			}

			if (positions.size() > 0) {

				_DATA->snapShot.add_task([this, lfo, tindex = lfo->track->index, ppos = std::move(positions)]() {
					auto groupId = _DATA->snapShot.nextGroupId();
					auto ev = tsl::parameters::Event::createTextEvent(tindex, lfoUpdateNames[lfo->index], 0, 0, groupId);
					std::lock_guard lk(_DATA->snapShot);
					_DATA->snapShot.addEvent(ev);
					for (auto pos : ppos) {
						auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, pos.first, pos.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
						_DATA->snapShot.addEvent(ev);
					}
					ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1REDRAW + lfo->index * LFONUMPARAMS, 1.0, 0, groupId);
					_DATA->snapShot.addEvent(ev);
					ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1RECOMPUTE + lfo->index * LFONUMPARAMS, 1.0, 0, groupId); ;
					_DATA->snapShot.addEvent(ev);
					});
			}
			lfo->store(LFORECOMPUTE, 1.0);
			lfo->store(LFOREDRAW, 1.0);
		}

		if (auto pt = inputstate.getById(pointerid)) {
			inputstate.lastpointer = pointerid;

			if (pt->mode == pmode_t::WINPOINTER && pt->target == _activeSeg.load()) {
				_activeSeg = -1;
				if (!doubleClick && (pt->xposWhenCreated != xpos || pt->yposWhenCreated != ypos)) {
					auto poss = std::vector<std::pair<int, double>>();
					uint16_t xParam = LFO1ENVX0 + pt->target + lfo->index * LFONUMPARAMS;
					uint16_t yParam = LFO1ENVY0 + pt->target + lfo->index * LFONUMPARAMS;
					auto valX = _STATE->params[tindex][xParam].load();
					auto valY = _STATE->params[tindex][yParam].load();
					poss.push_back({ xParam, valX });
					poss.push_back({ yParam, valY });
					if (lfo->joinends()) {
						if (pt->target == 0)
							poss.push_back({ LFO1ENVY0 + segments + lfo->index * LFONUMPARAMS, _STATE->params[tindex][LFO1ENVY0 + segments + lfo->index * LFONUMPARAMS].load() });
						else if (pt->target == segments)
							poss.push_back({ LFO1ENVY0 + lfo->index * LFONUMPARAMS, _STATE->params[tindex][LFO1ENVY0 + lfo->index * LFONUMPARAMS].load() });
					}
					_DATA->snapShot.add_task([this, lfo, tindex, pos = std::move(poss)]() {
						auto groupId = _DATA->snapShot.nextGroupId();
						auto ev = tsl::parameters::Event::createTextEvent(tindex, lfoUpdateNames[lfo->index], 0, 0, groupId);
						std::lock_guard lk(_DATA->snapShot);
						_DATA->snapShot.addEvent(ev);
						for (auto& p : pos) {
							auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, p.first, p.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
							_DATA->snapShot.addEvent(ev);
						}
						ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1REDRAW + lfo->index * LFONUMPARAMS, 1.0, 0, groupId);
						_DATA->snapShot.addEvent(ev);
						ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1RECOMPUTE + lfo->index * LFONUMPARAMS, 1.0, 0, groupId); ;
						_DATA->snapShot.addEvent(ev);
						});
				}

			}

			inputstate.removePointer(pointerid);
			inputstate.checkZoom();
			lfo->store(LFOREDRAW, 1.0);
		}
		break;
	}
	default:
		break;
	}
}
