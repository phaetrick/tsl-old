//
// Created by pr on 08.12.19.
//

#include <SkFont.h>
#include <SkPath.h>
#include "logger.h"
#include "envelope_window.h"
#include "view.h"
#include "plusminuscontrol.h"
#include "textview.h"
#include "button.h"
#include "defines.h"
#include "checkbox.h"
#include "synth.h"
#include "Input.h"
#include "lfo.h"
#include "grainstorm.h"
#include <array>
#include <string_view>

using namespace tsl::graphics;


void EnvelopeWindow::render(void* context) {
	auto tindex = _STATE->active_track.load();

	auto canvas = (SkCanvas*)context;

	flush(canvas);
	canvas->save();
	canvas->translate(startx, starty);

	SkPath path;
	path.addRect(SkRect::MakeXYWH(0, 0, width, height), SkPathDirection::kCCW);
	canvas->clipPath(path);
	path.reset();


	SkPaint paint;
	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);


	const auto* env_inner1 = _DATA->windows[(int)_STATE->params[tindex][ENVINNER].load()];
	const auto* env_outer1 = _DATA->windows[(int)_STATE->params[tindex][ENVOUTER].load()];

	const auto depth1 = _STATE->params[tindex][AOUTERDEPTH].load();
	const int cycles1 = (int)_STATE->params[tindex][AOUTERCYCLES].load();

	const int freq1 = 1;//*win->freq;


	const auto* env_inner2 = _DATA->windows[(int)_STATE->params[tindex][ENVINNER2].load()];
	const auto* env_outer2 = _DATA->windows[(int)_STATE->params[tindex][ENVOUTER2].load()];

	const auto depth2 = _STATE->params[tindex][AOUTERDEPTH2].load();
	const int cycles2 = (int)_STATE->params[tindex][AOUTERCYCLES2].load();

	const auto mix2 = _STATE->params[tindex][GRAINENVINTERPOL].load();
	const auto mix1 = 1. - mix2;


	{
		const int steps = width - 4 * lw;
		double env_frame1 = 0;
		const double step_length_inner1 = freq1 / ((double)steps);
		const double step_length_outer1 = cycles1 / ((double)steps - 1.);


		double env_frame2 = 0;
		const double step_length_inner2 = freq1 / ((double)steps);
		const double step_length_outer2 = cycles2 / ((double)steps - 1.);;


		double step_point_inner1 = 0;
		double step_point_outer1 = 0;
		double step_point_inner2 = 0;
		double step_point_outer2 = 0;

		float center = height * .5f;
		const float draw_height = center - 4 * lw;
		center -= lw2;

		paint.setColor(skcol::orange);
		path.moveTo(2 * lw, center - draw_height *
			(mix1 * env_inner1[(int)step_point_inner1] * (1. -
				env_outer1[(int)step_point_outer1] *
				depth1) + mix2 * env_inner2[(int)step_point_inner2] * (1. - env_outer2[(int)step_point_outer2] * depth2)));

		for (int step = 0; step < steps; step++) {
			path.lineTo(lw + lw + step, center - draw_height *
				(mix1 * env_inner1[PHS2INT(step_point_inner1)] * (1. -
					env_outer1[PHS2INT(step_point_outer1)] *
					depth1) + mix2 * env_inner2[PHS2INT(step_point_inner2)] * (1. -
						env_outer2[PHS2INT(step_point_outer2)] *
						depth2)));
			step_point_inner1 += step_length_inner1;
			step_point_outer1 += step_length_outer1;
			step_point_inner2 += step_length_inner2;
			step_point_outer2 += step_length_outer2;
		}
		canvas->drawPath(path, paint);
	}

end:
	paint.setColor(skcol::grey);

	canvas->drawRect(
		SkRect::MakeXYWH(lw2, lw2, width - lw, height - lw),
		paint);//draw_frame2(canvas, view, paint);
	canvas->restore();

	//draw_frame2(canvas, view, paint);
}
#if defined USE_IMGUI
#include <imgui.h>
void LFOWindow::renderRnd(void* context) {
	auto tindex = _STATE->active_track.load();


	ImGui::SetNextWindowPos(ImVec2(startx - 2 * lw, starty - 2 * lw), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(width + 4 * lw, height + 4 * lw), ImGuiCond_Always);
	ImGui::GetStyle().AntiAliasedFill = true;
	ImGui::GetStyle().AntiAliasedLines = true;
	bool test = true;

	if (!ImGui::Begin("LFO Envelope", &test,
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
	float w = canvas_size.x - 2 * lw, h = canvas_size.y - 2 * lw;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::bg));
	draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y),
		IM_Colour(skcol::grey), 1);
	draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y),
		ImVec2(canvas_pos.x + canvas_size.x,
			canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::grey), 1);

	draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y),
		ImVec2(canvas_pos.x, canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::grey), 1);
	draw_list->AddLine(ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y),
		ImVec2(canvas_pos.x + canvas_size.x,
			canvas_pos.y + canvas_size.y),
		IM_Colour(skcol::grey), 1);
	//draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y +
	//                                                                         canvas_size.y));// clip lines within the canvas (if we resize it, etc.)

	// int32_t width = (int) (view->width - line_width), height = (int) (view->height - 2 * line_width);
	LFO* lfo = _DATA->tracks[tindex]->lfos[GASLFO];

	float phase = _STATE->params[tindex][LFO1PHS + lfo->index * LFONUMPARAMS].load();
	float dir = _STATE->params[tindex][LFO1DIR + lfo->index * LFONUMPARAMS].load();

	int32_t func = lfo->func();



	//long double step_length = ((size - 1) * (long double) freq + freq - 0.94) / ((long double) width - 1);


	float offset = h;
	h -= 3 * lw;

	float length_line = w * .1f;
	float hit = w * phase;
	float hithead = hit + (ISNEG(dir) ? length_line : 0);
	float hittail = hit - (ISNEG(dir) ? 0 : length_line);

	float step_length = lw / (float)(w - lw - lw);
	float env_frame = 0;

	for (float i = lw; i < w - lw; i += lw) {
		float end = env_frame + step_length;
		draw_list->AddLine(
			ImVec2(canvas_pos.x + lw + i,
				canvas_pos.y + offset - lfo->draw(func, env_frame, h)),
			ImVec2(canvas_pos.x + lw + lw + i, canvas_pos.y + offset - lfo->draw(func, end, h)),
			(i >= hittail && i <= hithead) ? IM_Colour(skcol::fg)
			: IM_Colour(skcol::orange),
			lw);
		env_frame = end;
	}

	// draw_list->AddRect(ImVec2(canvas_pos.x + lw, canvas_pos.y + lw), ImVec2(canvas_pos.x    + canvas_size.x - lw, canvas_pos.y + canvas_size.y - lw), IM_Colour(skcol::fg), 20, ImDrawCornerFlags_All, lw);

	ImGui::End();
}
#else
void LFOWindow::renderRnd(void* context) {
	SkCanvas* canvas = static_cast<SkCanvas*>(context);
	canvas->save();

	// Background and border
	SkPaint bg;
	bg.setStyle(SkPaint::kFill_Style);
	bg.setColor(skcol::bg);
	canvas->drawRect(SkRect::MakeXYWH(startx - 2 * lw, starty - 2 * lw, width + 4 * lw, height + 4 * lw), bg);
	canvas->clipRect(SkRect::MakeXYWH(startx - 2 * lw, starty - 2 * lw, width + 4 * lw, height + 4 * lw));

	SkPaint border;
	border.setStyle(SkPaint::kStroke_Style);
	border.setStrokeWidth(1.0f);
	border.setColor(skcol::grey);
	// Top, bottom, left, right
	canvas->drawLine(startx - 2 * lw, starty - 2 * lw, startx + width + 2 * lw, starty - 2 * lw, border);
	canvas->drawLine(startx - 2 * lw, starty + height + 2 * lw, startx + width + 2 * lw, starty + height + 2 * lw, border);
	canvas->drawLine(startx - 2 * lw, starty - 2 * lw, startx - 2 * lw, starty + height + 2 * lw, border);
	canvas->drawLine(startx + width + 2 * lw, starty - 2 * lw, startx + width + 2 * lw, starty + height + 2 * lw, border);

	// LFO params
	int tindex = _STATE->active_track.load();
	LFO* lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	float phase = _STATE->params[tindex][LFO1PHS + lfo->index * LFONUMPARAMS].load();
	float dir = _STATE->params[tindex][LFO1DIR + lfo->index * LFONUMPARAMS].load();
	int func = lfo->func();

	float w = width;
	float h0 = height;
	float offY = starty + h0; // baseline
	float hInset = h0 - 3 * lw;

	// Build path
	SkPath path;
	path.setFillType(SkPathFillType::kWinding);
	float length_line = w * 0.1f;
	float hit = w * phase;
	float hitH = hit + (dir < 0 ? length_line : 0);
	float hitT = hit - (dir < 0 ? 0 : length_line);

	float step = lw / (w - 2 * lw);
	float env = 0;
	for (float x = lw; x < w - lw; x += lw) {
		float nxt = env + step;
		float y0 = offY - lfo->draw(func, env, hInset);
		float y1 = offY - lfo->draw(func, nxt, hInset);
		path.moveTo(startx + x, y0);
		path.lineTo(startx + x + lw, y1);
		env = nxt;
	}

	// Stroke segments with color
	SkPaint segPaint;
	segPaint.setStyle(SkPaint::kStroke_Style);
	segPaint.setStrokeWidth(lw);
	segPaint.setAntiAlias(true);
	canvas->drawPath(path, segPaint);

	canvas->restore();
}
#endif

void LFOWindow::render(void* context) {
	auto canvas = static_cast<SkCanvas*>(context);
	auto tindex = _STATE->active_track.load();
	auto track = _DATA->tracks[tindex];
	LFO& lfo = *_DATA->tracks[tindex]->lfos[GASLFO];

	auto old = lfo.drawBuf.exchange(nullptr, std::memory_order_acq_rel);
	if (old == nullptr) {
		if (firstDraw == true && (_STATE->params[tindex][POWERTRACK].load() == 0.0 || !lfo.power() || !_STATE->player.isPlaying())) {
			firstDraw = false;

			flush(canvas);
			SkPaint paint;
			paint.setAntiAlias(true);
			paint.setStrokeWidth(lw);
			paint.setStyle(SkPaint::kStroke_Style);

			paint.setColor(skcol::grey);

			canvas->drawRect(
				SkRect::MakeXYWH(startx + lw, starty + lw, width - 2 * lw, height - 2 * lw),
				paint);
		}
		return;
	}
	firstDraw = true;


	flush(canvas);
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setStrokeWidth(lw);
	paint.setStyle(SkPaint::kStroke_Style);

	canvas->save();
	canvas->translate(startx + 1.5 * lw,
		starty + 1.5 * lw);


	const float heightscaled = height - 3 * lw;
	const float widthscaled = width - 3 * lw;
	float multi = widthscaled / (float)LFO_TBL_SIZE;
	SkPath path;
	path.moveTo(0, heightscaled - heightscaled * old[0]);
	for (int32_t i = 1; i < LFO_TBL_SIZE; i++) {
		path.lineTo(i * multi, heightscaled - heightscaled * old[i]);
	}
	_STATE->pool.release(old);
	paint.setColor(skcol::orange);
	canvas->drawPath(path, paint);
	canvas->restore();
	paint.setColor(skcol::grey);

	canvas->drawRect(
		SkRect::MakeXYWH(startx + lw, starty + lw, width - 2 * lw, height - 2 * lw),
		paint);

}


void LFOWindow::addRecursiveDraw() {
	firstDraw = true;
	auto tindex = _STATE->active_track.load();
	auto lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	if (auto buf = lfo->drawBuf.exchange(nullptr, std::memory_order_acq_rel)) {
		_STATE->pool.release(buf);
	}
	lfo->updateRenderThread.store(true, std::memory_order_release);
	View::addRecursiveDraw();
}
void LFOWindow::delRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	auto lfo = _DATA->tracks[tindex]->lfos[GASLFO];
	lfo->updateRenderThread.store(false, std::memory_order_release);
	View::delRecursiveDraw();
}

#define LIN2LOG (x) log2(x)
#define LOG2LIN (x) pow(2, (x))


const float lowfr = 200.f;
const float highfr = 8000.f;
const float frrange = highfr - lowfr;
const float lowlog = LIN2SCALED2(lowfr);
const float highlog = LIN2SCALED2(highfr);
const float rangelog = LIN2SCALED2(frrange);

#define LIN2SCALEDSD2(x) powf(x, .25)
const float lowfrsd2 = 0.f;
const float highfrsd2 = 20000.f;
const float frrangesd2 = highfrsd2 - lowfrsd2;


inline constexpr const float editvalues[] = {
	0.f, 1.f, 2.f
};

inline constexpr std::string_view segnames[] = {
	"1", "2", "3", "4",
	"5", "6", "7", "8"
};

inline constexpr const float segvals[] = {
	1.f, 2.f, 3.f, 4.f,
	5.f, 6.f, 7.f, 8.f
};

inline constexpr std::string_view freqcharsvert[] = {
	"200", "320", "500",
	"800", "1K3", "2K",
	"3K2", "5K", "8K"
};

void VertScale::render(void* context) {

	auto* canvas = (SkCanvas*)context;
	flush(canvas);
	canvas->save();
	canvas->translate(startx, starty);
	float textsize = _STATE->textsize2 * .6f;
	float t2 = textsize * .5f;
	float t4 = t2 * .5f;
	SkPaint paint;

	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kFill_Style);
	paint.setColor(skcol::fg);

	SkFont skFont(_STATE->font_normal);
	skFont.setSize(textsize);
	SkRect bounds{};
	int i = 0;
	for (auto& freqChar : freqcharsvert)
	{
		const auto txt = std::string(freqChar);

		skFont.measureText(
			txt.data(),
			txt.size(),
			SkTextEncoding::kUTF8,
			&bounds
		);

		canvas->drawSimpleText(
			txt.data(),
			txt.size(),
			SkTextEncoding::kUTF8,
			width - bounds.width() - t4,
			t4 + height - height * (i * 0.125f),
			skFont,
			paint
		);
		i++;
	}


	canvas->restore();
}


static const float frequencypointshv[] = { .25f, .5f, .75f };

void EnvWin::renderVertLines(void* context) {
	auto* canvas = (SkCanvas*)context;
	//FLUSH2(canvas, v);

	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;

	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);
	paint.setColor(skcol::grey);

	SkPath path;

	for (int i = 1; i < ARRAY_LEN(freqcharsvert) - 1; i++) {
		float hh = height - height * i * .125f;
		path.moveTo(0, hh);
		path.lineTo(width, hh);
	}

	for (float point : frequencypointshv) {
		path.moveTo(width * point, 0);
		path.lineTo(width * point, height);
	}
	canvas->drawPath(path, paint);
	canvas->restore();
}

inline constexpr std::string_view  frequencycharsPV[] = { "DC", "25", "100", "250", "600", "1K5", "3K5", "8K5",
										 "NYQ" };


void HozScale::render(void* context) {
	auto canvas = (SkCanvas*)context;
	flush(canvas);
	canvas->save();
	canvas->translate(startx, starty);
	float textsize = _STATE->textsize2 * .6f;
	SkPaint paint;

	paint.setStrokeWidth(lw);
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kFill_Style);
	paint.setColor(skcol::fg);

	SkFont skFont(_STATE->font_normal);
	skFont.setSize(textsize);
	SkRect bounds{};

	float offy = (height - textsize) / 2.f;
	int i = 0;
	for (auto& freqChar : frequencycharsPV) {
		auto str = std::string(freqChar);
		skFont.measureText(str.data(), str.size(),
			SkTextEncoding::kUTF8, &bounds);

		canvas->drawSimpleText(str.data(), str.size(),
			SkTextEncoding::kUTF8,
			width * i * .125f -
			bounds.width() * .5f,
			offy,
			skFont, paint);
		i++;
	}

	canvas->restore();
}

void EnvWin::renderFrame(void* ctx) {
	auto canvas = (SkCanvas*)ctx;
	flush(canvas);

	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;

	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);
	paint.setColor(skcol::grey);

	SkPath path;


	path.moveTo(0, 0);
	path.lineTo(0, height);

	path.moveTo(width, 0);
	path.lineTo(width, height);

	path.moveTo(0, 0);
	path.lineTo(width, 0);

	path.moveTo(0, height);
	path.lineTo(width, height);

	canvas->drawPath(path, paint);
	canvas->restore();
}

void EnvWin::renderHozLines(void* context) {
	auto* canvas = (SkCanvas*)context;
	//FLUSH2(canvas, v);

	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;

	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);
	paint.setColor(skcol::grey);

	SkPath path;

	for (int32_t i = 1; i < ARRAY_LEN(frequencycharsPV) - 1; i++) {
		float ww = width * i * .125f;
		path.moveTo(ww, 0);
		path.lineTo(ww, height);
	}

	for (float point : frequencypointshv) {
		path.moveTo(0, height - height * point);
		path.lineTo(width, height - height * point);
	}
	canvas->drawPath(path, paint);
	canvas->restore();
}


void EnvWin::render(void* context) {
	auto tindex = _STATE->active_track.load();

	auto* canvas = (SkCanvas*)context;
	// FLUSH2(canvas, v);

	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;

	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);

	SkPath path;

	int offx = id;
	int offy = offx + OFFPOINTY;
	int nsegs = _STATE->params[tindex][offx + OFFNSEGS].load();
	int curve = _STATE->params[tindex][offx + OFFCURVE].load();
	int active = _activeSeg.load();


	MYFLOAT xx[32], yy[32];

	for (int i = 0; i < nsegs + 1; i++) {
		xx[i] = _STATE->params[tindex][offx + i].load();
		yy[i] = _STATE->params[tindex][offy + i].load();

	}

	auto* env = _STATE->pool.acquire<MYFLOAT>(ADSRSIZE);
	tsl::envelope::compute<MYFLOAT>[curve](env, ADSRSIZE, xx, yy, nsegs, true);

	float inc = 3 * (float)(ADSRSIZE - 1) / (width - 1);
	float offset = 0;
	paint.setColor(skcol::orange);

	path.reset();
	for (int i = 0; i < width - 3; i += 3) {
		path.moveTo(i, height - height * env[(int)offset]);
		offset += inc;
		if (offset > ADSRSIZE - 1)
			offset = ADSRSIZE - 1;
		path.lineTo(i + 3, height - height * env[(int)offset]);
	}
	_STATE->pool.release(env);
	canvas->drawPath(path, paint);


	paint.setStyle(SkPaint::kFill_Style);
	const float radius = _STATE->circleradius;

	for (int i = 0; i < nsegs + 1; i++) {
		paint.setColor(i == active ? skcol::grey : skcol::blue_transparent);
		canvas->drawCircle(xx[i] * width, height - yy[i] * height, radius,
			paint);
	}
	canvas->restore();
}


void EnvWin::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();

	int offx = id;
	int offy = offx + OFFPOINTY;
	int nsegs = _STATE->params[tindex][offx + OFFNSEGS].load();
	int curve = _STATE->params[tindex][offx + OFFCURVE].load();
	bool join = _STATE->params[tindex][offx + OFFJOINENDS].load() == 1.0;


	std::atomic<MYFLOAT>* xx[32], * yy[32];

	for (int i = 0; i < nsegs + 1; i++) {
		xx[i] = &_STATE->params[tindex][offx + i];
		yy[i] = &_STATE->params[tindex][offy + i];
	}

	const int action = event.action;
	const int pointerid = event.pointer_id;
	const float xpos = event.x - startx;
	const float ypos = event.y - starty;
	const float w = width;
	const float h = height;
	const float segsize = w * .1f;
	const float segsized2 = segsize * .5f;
	auto view = _STATE->parameters[id].view;
	switch (action) {
	case ACTION_DOWN: {
		for (int i = 0; i < nsegs + 1; i++) {
			const float x = xx[i]->load() * w;
			const float y = h - yy[i]->load() * h;
			if (xpos >
				x - segsized2 &&
				xpos < x + segsized2 &&
				ypos > y -
				segsized2 &&
				ypos < y + segsized2) {
				_activeSeg = i;
				inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINDRAG, i);
				view->redraw();
			}
		}
		break;
	}
	case ACTION_MOVE: {
		if (auto pt = inputstate.getById(pointerid)) {
			auto active_seg = pt->target;
			float val = xpos / w;
			if (active_seg == 0)
				val = 0;
			else if (active_seg == nsegs)
				val = 1;
			else if (val < xx[active_seg - 1]->load())
				val = xx[active_seg - 1]->load();
			else if (val > xx[active_seg + 1]->load())
				val = xx[active_seg + 1]->load();
			xx[active_seg]->store(val);

			val = (h - ypos) / h;
			if (val > 1.0)
				val = 1.0;
			if (val < 0)
				val = 0;

			yy[active_seg]->store(val);
			if (active_seg == 0 && join)
				yy[nsegs]->store(val);
			else if (active_seg == nsegs && join)
				yy[0]->store(val);
			for (int i = 0; i < _STATE->channels; i++)
				_STATE->params[tindex][offx + OFFRECOMP + i].store(1.0);
			_activeSeg.store(active_seg);
			view->redraw();
		}
		break;
	}
	case ACTION_UP: {

		if (auto pt = inputstate.getById(pointerid)) {
			if (pt->target == _activeSeg.load() && inputstate.size() == 1) {
				_activeSeg.store(-1);
				if (pt->xposWhenCreated != xpos || pt->yposWhenCreated != ypos) {
					auto poss = std::make_shared<std::vector<std::pair<int, double>>>();
					uint16_t xParam = offx + pt->target;
					uint16_t yParam = offy + pt->target;
					auto valX = _STATE->params[tindex][xParam].load();
					auto valY = _STATE->params[tindex][yParam].load();
					poss->push_back({ xParam, valX });
					poss->push_back({ yParam, valY });
					if (join) {
						if (pt->target == 0)
							poss->push_back({ offy + nsegs, _STATE->params[tindex][offy + nsegs].load() });
						else if (pt->target == nsegs)
							poss->push_back({ offy, _STATE->params[tindex][offy].load() });
					}
					_DATA->snapShot.add_task([this, tindex, pos = std::move(poss), offx]() {
						auto groupId = _DATA->snapShot.nextGroupId();
						std::lock_guard lk(_DATA->snapShot);
						for (auto& p : *pos) {
							auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, p.first, p.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
							_DATA->snapShot.addEvent(ev);
						}
						for (int i = 0; i < _STATE->channels; i++) {
							auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, offx + OFFRECOMP + i, 1.0, 0, groupId);
							_DATA->snapShot.addEvent(ev);
						}
						auto ev = tsl::parameters::Event::createRerenderEvent(tindex, id, groupId);
						_DATA->snapShot.addEvent(ev);
						ev = tsl::parameters::Event::createTextEvent(tindex, _STATE->parameters[id].name != nullptr ? _STATE->parameters[id].name : "ENV EDIT", 0, 0, groupId);
						_DATA->snapShot.addEvent(ev);
						});
				}
			}
			inputstate.removePointer(pointerid);
			view->redraw();
		}
	}
	default:
		break;
	}
}

void EnvelopeEditorWindow::callback(const InputEvent& event) {
	window->callback(event);
}


void EnvelopeEditorWindow::render(void* cr) {
	auto* c = (SkCanvas*)cr;
	flush(c);
	c->save();
	SkPath path;
	path.addRect(SkRect::MakeXYWH(startx, starty, width, height));
	c->clipPath(path);
	//render_dbscale(frequencyresponse->dbscale, context);
	window->renderFrame(cr);
	if (orient == VERTICAL) {
		vertscale->render(cr);
		hozscale->flush(c);
		window->renderVertLines(cr);
	}
	else {
		hozscale->render(cr);
		vertscale->flush(c);
		window->renderHozLines(cr);
	}
	window->render(cr);
	c->restore();
}

EnvelopeEditorWindow::EnvelopeEditorWindow(tsl::AppState* appState, float
	_scalefactor, int
	_aspect_ratio, int
	_alignment,
	int
	_id, int
	_orient) : HorizontalLayout(appState, _scalefactor,
		_aspect_ratio,
		_alignment) {
	dummybottom = new VerticalLayout{ appState, VALUE_FROM_POINTER,
							   VALUE_FROM_POINTER, END_ALIGN };
	dummybottomleft = new View{ appState, VALUE_FROM_POINTER,
						 VALUE_FROM_POINTER, START_ALIGN };
	filterfrequencytop = new View{ appState, VALUE_FROM_POINTER,
							VALUE_FROM_POINTER, START_ALIGN };
	filterfrequencycenter = new VerticalLayout{ appState, WRAP,
										 0, CENTER_ALIGN };
	window = new EnvWin{ appState };
	vertscale = new VertScale{ appState };
	hozscale = new HozScale{ appState };

	prio = 10;
	id = _id;
	_STATE->parameters[id].view = this;
	paddingleft = paddingright = 2.5f;
	orient = _orient;
	addChild(dummybottom);
	dummybottom->size_reference = &_STATE->windowHeight;
	dummybottom->size_reference_scale = .7f / 15.;

	dummybottomleft->size_reference = &_STATE->windowHeight;
	dummybottomleft->size_reference_scale = .5f / 15.;
	dummybottom->addChild(dummybottomleft);
	dummybottom->addChild(hozscale);
	hozscale->id = id;

	filterfrequencytop->size_reference = &_STATE->windowHeight;
	filterfrequencytop->size_reference_scale = .7f / 15.;
	addChild(filterfrequencytop);
	addChild(filterfrequencycenter);

	vertscale->size_reference = &_STATE->windowHeight;
	vertscale->size_reference_scale = .5f / 15.;
	filterfrequencycenter->addChild(vertscale);
	vertscale->id = id;

	window->id = id;
	filterfrequencycenter->addChild(window);

	/*filterfrequencycenter->paddingleft = */filterfrequencycenter->paddingright = /*dummybottom->paddingleft = */dummybottom->paddingright = /*filterfrequencyto_DATA->paddingleft = */filterfrequencytop->paddingright = 5.f;
}

EnvelopeWindowControls::EnvelopeWindowControls(tsl::AppState* appState, float
	_scalefactor, int
	_aspect_ratio,
	int
	_alignment,
	int
	_id) : VerticalLayout(appState, _scalefactor,
		_aspect_ratio, _alignment) {
	id = _id;

	selnsegs = new Selector1<tsl::graphics::TitleView>(appState, WRAP, 0,
		CENTER_ALIGN,
		ARRAY_LEN(segnames), makeStringSpan(segnames),
		makeFloatSpan(segvals), _id + OFFNSEGS);
	selwaveform = new Selector1<tsl::graphics::WaveformChooser>(appState, WRAP, 0,
		CENTER_ALIGN,
		ARRAY_LEN(editorcurvenames),
		makeStringSpan(editorcurvenames),
		makeFloatSpan(editvalues), _id + OFFCURVE);
	_STATE->parameters[id + OFFCURVE].name = "CURVE";

	dummycontrolsenveditor4 = new HorizontalLayout{ appState, WRAP, 0, CENTER_ALIGN };
	dummycb = new HorizontalLayout{ appState, WRAP, 0, CENTER_ALIGN };
	cb = new CheckBox(appState, HORIZONTAL, CENTER_ALIGN,
		_id + OFFJOINENDS);
	cb->userdata = _id;


	addChild(selnsegs);
	selnsegs->userdata = _id;

	addChild(dummycontrolsenveditor4);
	//dummycontrolsenveditor4->size_reference = &_DATA->_STATE->textsize2;

	dummycontrolsenveditor4->addChild(selwaveform);
	selwaveform->userdata = _id;

	addChild(dummycb);
	dummycb->paddingbottom = dummycb->paddingtop = 10.f;
	dummycb->addChild(cb);
	_STATE->parameters[id + OFFJOINENDS].view = cb;
	//_STATE->parameters[id + OFFJOINENDS].onChange = callback_joinendsbut;
}

ADSRWin::ADSRWin(tsl::AppState* appState) : EnvWin(appState), valueView(appState, [&val = valueView, &par = *this, &_act = _activeSeg]() {
	auto _appState = val._appState;
	if (val.timer.elapsed() > 1.) {
		val.perm = false;
		_STATE->graphics.deleteWindow(val.windex);
		return;
	}


	auto tindex = _STATE->active_track.load();
	auto active_seg = _act.load();
	if (active_seg < 0 || active_seg > 4)
		return;

	float startx_pos1, starty_pos;//"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
	auto width = measureTextFixed(100, _STATE->textsize1, _STATE->font_normal, " -60 dB ", &startx_pos1, &starty_pos,
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

	char text[30];
	snprintf(text, 30, "%.0f dB ", _STATE->params[tindex][par.id + 5 + active_seg].load());
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9);
	c->drawSimpleText(text, strlen(text),
		SkTextEncoding::kUTF8,
		width - measureWidth(font, text), starty_pos,
		font, paint);
	}) {

}


void ADSRWin::callback(const InputEvent& event) {
	auto tindex = _STATE->active_track.load();

	int offx = id;
	int offy = offx + 5;
	int nsegs = 4; //_STATE->params[track->index][offx + OFFNSEGS].load();
	bool join = false; //_STATE->params[track->index][offx + OFFJOINENDS].load() == 1.0;


	std::atomic<MYFLOAT>* xx[5], * yy[5];

	MYFLOAT yarr[5];

	for (int i = 0; i < nsegs + 1; i++) {
		xx[i] = &_STATE->params[tindex][offx + i];
		yarr[i] = 1 + _STATE->params[tindex][offy + i].load() / 60.;
		yy[i] = &_STATE->params[tindex][offy + i];
	}

	const auto action = event.action;
	const float xpos = event.x - startx;
	const float ypos = event.y - starty;
	const auto pointerid = event.pointer_id;
	const float w = width;
	const float h = height;
	const float segsize = w * .1f;
	const float segsized2 = segsize * .5f;



	switch (action) {
	case ACTION_DOWN: {
		for (int i = 0; i < nsegs + 1; i++) {
			const float x = xx[i]->load() * w;
			const float y = h - yarr[i] * h;
			if (xpos >
				x - segsized2 &&
				xpos < x + segsized2 &&
				ypos > y -
				segsized2 &&
				ypos < y + segsized2) {
				_activeSeg = i;
				_STATE->parameters[id].view->redraw();
				inputstate.addPointer(pointerid, xpos, ypos, pmode_t::WINDRAG, i);
				valueView.addDraw();
			}
		}
		break;
	}
	case ACTION_MOVE: {
		if (auto pt = inputstate.getById(pointerid)) {
			auto active_seg = pt->target;
			float val = xpos / w;
			if (active_seg == 0)
				val = 0;
			else if (active_seg == nsegs)
				val = 1;
			else if (val < xx[active_seg - 1]->load())
				val = xx[active_seg - 1]->load();
			else if (val > xx[active_seg + 1]->load())
				val = xx[active_seg + 1]->load();
			xx[active_seg]->store(val);

			val = (h - ypos) / h;
			if (val > 1.0)
				val = 1.0;
			if (val < 0)
				val = 0;

			val = val * 60. - 60.;
			yy[active_seg]->store(val);
			if (active_seg == 0 && join)
				yy[nsegs]->store(val);
			else if (active_seg == nsegs && join)
				yy[0]->store(val);
			_activeSeg = active_seg;
			_STATE->parameters[id].view->redraw();
			valueView.addDraw();
		}
		break;
	}
	case ACTION_UP: {
		if (auto pt = inputstate.getById(pointerid)) {
			if (pt->target == _activeSeg.load() && inputstate.size() == 1) {
				if (pt->xposWhenCreated != xpos || pt->yposWhenCreated != ypos) {
					auto poss = std::vector<std::pair<int, double>>();
					uint16_t xParam = offx + pt->target;
					uint16_t yParam = offy + pt->target;
					auto valX = _STATE->params[tindex][xParam].load();
					auto valY = _STATE->params[tindex][yParam].load();
					poss.push_back({ xParam, valX });
					poss.push_back({ yParam, valY });
					_DATA->snapShot.add_task([this, tindex, pos = std::move(poss)]() {
						auto groupId = _DATA->snapShot.nextGroupId();
						auto ev = tsl::parameters::Event::createTextEvent(tindex, "ENV EDIT", 0, 0, groupId);
						std::lock_guard lk(_DATA->snapShot);
						_DATA->snapShot.addEvent(ev);
						for (auto& p : pos) {
							auto ev = tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, p.first, p.second, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo);
							_DATA->snapShot.addEvent(ev);
						}
						ev = tsl::parameters::Event::createRerenderEvent(tindex, id, groupId);
						_DATA->snapShot.addEvent(ev);
						});
				}
				_activeSeg.store(-1);
			}
			_STATE->parameters[id].view->redraw();
			inputstate.removePointer(pointerid);;
		}
		break;
	}
	default:
		break;
	}
	//LOGE("%ld %f %f %f", AMotionEvent_getPointerCount(event), track->zoom.load(), track->pos.load(), olddist);
}


void ADSRWin::render(void* context) {
	auto tindex = _STATE->active_track.load();
	auto* canvas = (SkCanvas*)context;
	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;

	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(lw);

	SkPath path;
	int offx = id;


	int offy = offx + 5;
	int nsegs = 4;//_STATE->params[track->index][offx + OFFNSEGS].load();
	int curve = 0;//_STATE->params[track->index][offx + OFFCURVE].load();


	float xx[5], yy[5];

	for (int i = 0; i < nsegs + 1; i++) {
		xx[i] = _STATE->params[tindex][offx + i].load();
		auto val = _STATE->params[tindex][offy + i].load();
		if (val < -60.) {
			val = -60.;
			_STATE->params[tindex][offy + i].store(val);
		}
		yy[i] = 1.f + val / 60.f;
	}

	auto env = _STATE->pool.acquire<float>(ADSRSIZE);
	tsl::envelope::compute<float>[curve](env, ADSRSIZE, xx, yy, nsegs, true);

	float inc = 3 * (float)(ADSRSIZE - 1) / (float)(width - 1);
	float offset = 0;
	paint.setColor(skcol::orange);

	path.reset();
	for (int i = 0; i < width - 3; i += 3) {
		path.moveTo(i, height - height * env[(int)offset]);
		offset += inc;
		if (offset > ADSRSIZE - 1)
			offset = ADSRSIZE - 1;
		path.lineTo(i + 3, height - height * env[(int)offset]);
	}

	_STATE->pool.release(env);

	canvas->drawPath(path, paint);


	auto active = _activeSeg.load();


	paint.setStyle(SkPaint::kFill_Style);


	const float radius = _STATE->circleradius;

	for (int i = 0; i < nsegs + 1; i++) {
		paint.setColor(i == active ? skcol::grey : skcol::blue_transparent);
		canvas->drawCircle(xx[i] * width, height - yy[i] * height, radius, paint);
	}


	canvas->restore();
}


void ADSRWindow::render(void* cr) {
	auto* c = (SkCanvas*)cr;
	flush(c);
	c->save();
	SkPath path;
	path.addRect(SkRect::MakeXYWH(startx, starty, width, height));
	c->clipPath(path);
	window->renderFrame(cr);
	window->render(cr);
	c->restore();
}


void ADSRWindow::callback(const InputEvent& event) {
	window->callback(event);
}


ADSRWindow::ADSRWindow(tsl::AppState* appState, float
	_scalefactor, int
	_aspect_ratio, int
	_alignment,
	int
	_id) : VerticalLayout(appState, _scalefactor, _aspect_ratio, _alignment) {

	prio = 10;
	id = _id;
	_STATE->parameters[id].view = this;
	window = new ADSRWin(appState);
	window->padding = 10.f;
	window->id = id;
	addChild(window);
}

void BetaPDFView::render(void* context) {
	auto tindex = _STATE->active_track.load();
	int32_t offset = _STATE->params[tindex][_offset].load();
	float prevA = _STATE->params[tindex][_alpha + offset].load();
	float prevB = _STATE->params[tindex][_beta + offset].load();
	if (_prevA == prevA && _prevB == prevB)
		return;
	_prevA = prevA;
	_prevB = prevB;

	auto canvas = static_cast<SkCanvas*>(context);
	flush(canvas);
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::fg);
	paint.setStrokeWidth(lw);
	paint.setStyle(SkPaint::kStroke_Style);

	paint.setColor(skcol::grey);
	paint.setStrokeWidth(lw);
	canvas->drawRect(
		SkRect::MakeXYWH(startx + 1, starty + 1, width - 2, height - 2),
		paint);

	canvas->save();
	canvas->translate(startx + 1.5 * lw,
		starty + 1.5 * lw);


	const float heightscaled = height - 3 * lw;
	const float widthscaled = width - 3 * lw;
	float multi = widthscaled / (float)width;
	paint.setColor(skcol::orange);
	const float alpha = LOG2NORMALF(_prevA);
	const float beta = LOG2NORMALF(_prevB);
	float p0 = heightscaled - heightscaled * tsl::random::Random::betapdf2(0, alpha, beta);
	for (int i = 1; i < width; i++) {
		auto tmpi = (float)i;
		float p1 =
			heightscaled - heightscaled * tsl::random::Random::betapdf2(tmpi / (float)width, alpha, beta);
		if (p1 >= 0 && p0 >= 0)
			canvas->drawLine(SkPoint::Make((tmpi - 1.f) * multi, p0),
				SkPoint::Make(tmpi * multi, p1), paint);
		p0 = p1;
	}
	canvas->restore();
}
