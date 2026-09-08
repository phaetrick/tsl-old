#include "infopanel.h"
#include "logger.h"
#include <cstdlib>
#include <cstring>
#include <include/core/SkFont.h>
#include <iomanip>
#include "grainstorm.h"
#include "knob.h"
#include "defines.h"
#include "view.h"
#include "tools.h"
#include "track.h"
#include "colours.h"
#include "lfo.h"
#include "app.h"
#include "History.h"
#include "IPlugParamDefs.h"

#ifdef __ANDROID__

#include "player.h"
#include "Recorder.h"
#endif

#ifdef PLATFORM_MOBILE
const char* emptyText = "PRESS EJECT OR MIC TO LOAD SOUNDS";
#elif defined STANDALONE_MODE
const char* emptyText = "DROP FILES OR PRESS MIC TO LOAD SOUNDS";
#elif defined PLUGIN_MODE
const char* emptyText = "DROP FILES TO LOAD SOUNDS";
#endif


namespace tsl::graphics {

	static void render_mic(InfoPanel* infopanel, void* context);

	void render_infopanel(InfoPanel* infopanel, void* context);

	static void render_void(InfoPanel* infopanel, void* context);

	static void render_text(InfoPanel* infopanel, void* context);

	static void render_value(InfoPanel* infopanel, void* context);

	static void render_normal(InfoPanel* infopanel, void* context);

	static void render_progress(InfoPanel* infopanel, void* context);

	static void render_xrun(InfoPanel* infopanel, void* context);

	InfoPanel::InfoPanel(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment) : View(appState,
		_scalefactor,
		_aspect_ratio,
		_alignment,
		10, true,
		"Infopanel") {
		renderfunc.store(render_normal);
		setTimeStamp();
		font = _STATE->font_normal;
	}

	void InfoPanel::init() {
		const char* test =
			"POS: 00 : 00 : 000X/ 00 : 00 : 000  LOOP: 00 : 00 : 000XLOAD: 100%";

		//"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
		measureText(this, _STATE->font_normal, test, 1.0f, &startx_pos1, &starty_pos, &fontsize);
		if (fontsize > _STATE->textsize2 * .9f)
			fontsize = _STATE->textsize2 * .9f;
		font.setSize(fontsize);
		const char* test2 = "POS: 00 : 00 : 000X";
		SkRect bounds{};
		startx_pos1 = fontsize * .5;
		font.measureText(test2, strlen(test2), SkTextEncoding::kUTF8, &bounds);
		startx_pos2 = startx_pos1 + bounds.width();
		const char* test3 = "/ 00 : 00 : 000  LOOP: 00 : 00 : 000X";
		font.measureText(test3, strlen(test3), SkTextEncoding::kUTF8, &bounds);
		startx_pos3 = startx_pos2 + bounds.width();
		// info->starty_pos = mid;

		test = "POWER OFFX";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_poweroff = width - bounds.width();

		test = "00 : 00";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_rec = (width - bounds.width()) * .5f;

		test = "Microphone recording.";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_mic = (width - bounds.width()) * .5f;

		test = "DECODING...";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_decode = (width - bounds.width()) * .5f;

		test = "RENDERING WAVEFORM...";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_render = (width - bounds.width()) * .5f;

		test = "Buffer underrun!";
		font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		startx_warning = (width - bounds.width()) * .5f;

		test = "TRACK1 ENCODING:   0.00";
		startx_pos4 = font.measureText(test, strlen(test), SkTextEncoding::kUTF8, &bounds);
		textBuffer[100] = '\0';
	}

	static void render_void(InfoPanel* infopanel, void* context) {
	}

	void InfoPanel::render(void* context) {
		renderfunc.load()(this, context);
	}

	static void render_normal(InfoPanel* infopanel, void* context) {
		auto _appState = infopanel->_appState;
		TRACK* track = _DATA->tracks[_STATE->params[_STATE->active_track.load()][DISTRSOURCE].load()];
		auto* canvas = (SkCanvas*)context;
		infopanel->flush(canvas);
		canvas->save();
		canvas->translate(infopanel->startx, infopanel->starty);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::fg);
		auto& font(infopanel->font);

		char text[100];
		text[0] = '\0';
		auto rec = track->filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		auto off = state == nullptr ? 0 : rec->off;
		if (off > 0) {
			TIME_P t;
			time_convert(t, _STATE->sr, (long)state->offset);

			snprintf(text, 100,
				//"Pos: %02d : %02d : %03d   Dur: %02d : %02d : %03d   Playdur: %02d : %02d : %03d ",
				"POS: %02d : %02d : %03d",
				t.m, t.s, t.ms);

			canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
				infopanel->startx_pos1,
				infopanel->starty_pos, font, paint);

			if (track->time_play_dur.h > 8760)
				snprintf(text, 100, "/ %02d : %02d : %03d  LOOP: %dY",
					track->time_file.m,
					track->time_file.s,
					track->time_file.ms, track->time_play_dur.h / 8760);
			else {
				if (track->time_play_dur.h > 0)
					snprintf(text, 100, "/ %02d : %02d : %03d  LOOP: %dH",
						track->time_file.m,
						track->time_file.s,
						track->time_file.ms,
						track->time_play_dur.h);
				else
					snprintf(text, 100, "/ %02d : %02d : %03d  LOOP: %02d : %02d : %03d",
						track->time_file.m,
						track->time_file.s,
						track->time_file.ms, track->time_play_dur.m,
						track->time_play_dur.s,
						track->time_play_dur.ms);
			}

			canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
				infopanel->startx_pos2,
				infopanel->starty_pos, font, paint);
		}
		else {
			snprintf(text, 100, "%s", emptyText);
			canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
				infopanel->startx_pos1,
				infopanel->starty_pos, font, paint);
		}
		if (_STATE->player._isplaying.load()) {
			int32_t buffer_fill = (int)(_STATE->buffer_fill / 2500000.);
			// Colour tracks the LOAD figure and nothing else. Dropped blocks are
			// reported by render_xrun instead, which is transient by design --
			// tying the colour to a running drop count meant one drop at startup
			// left the readout red for the whole session while it displayed 2%.
			if (buffer_fill >= 100)
				paint.setColor(skcol::red);
			else if (buffer_fill >= 80.)
				paint.setColor(skcol::yellow);
			snprintf(text, 100, "%s%d%%", "LOAD: ", buffer_fill);
			canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
				infopanel->startx_pos3,
				infopanel->starty_pos, font, paint);
		}
		else {
			auto drawShort = off > 0 && infopanel->startx_poweroff < infopanel->startx_pos3;
			if (drawShort)
				snprintf(text, 100, "%s", "POW OFF");
			else
				snprintf(text, 100, "%s", "POWER OFF");
			canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
				drawShort ? infopanel->startx_pos3 : infopanel->startx_poweroff,
				infopanel->starty_pos, font, paint);
		}
		canvas->restore();
	}

	

	static void render_mic(InfoPanel* infopanel, void* context) {
#ifdef __ANDROID__
		auto* _appState = infopanel->_appState;
		auto samples = infopanel->dec_offset.load();
		if (samples <= 0)
			return;

		auto* canvas = (SkCanvas*)context;
		infopanel->flush(canvas);
		canvas->save();
		canvas->translate(infopanel->startx, infopanel->starty);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::fg);
		auto& font = infopanel->font;

		TIME_P t{};
		time_convert(t, _STATE->sr, (long)samples);
		char text[20];
		snprintf(text, 20, "%02d : %02d", t.m, t.s);
		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, infopanel->startx_rec,
			infopanel->starty_pos, font, paint);
		canvas->restore();
#endif
	}
	
	static void render_encode(InfoPanel* info, void* context) {
		auto _appState = info->_appState;
		float progress = info->progress.load();
		float line_width = View::lw;

		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);
		canvas->save();
		canvas->translate(info->startx, info->starty);

		SkPaint paint;
		paint.setStrokeWidth(line_width);
		paint.setAntiAlias(true);
		paint.setColor(skcol::blue_transparent);
		auto& font = info->font;

		if (progress >= 0)
			canvas->drawRect(SkRect::MakeXYWH(0, SkDoubleToScalar(line_width),
				SkDoubleToScalar(info->width * progress),
				info->height),
				paint);

		paint.setColor(skcol::fg);

		auto mbs = info->dec_offset.load() / (1024. * 1024.);
		std::stringstream ss;

		ss << info->text.load() << info->text2.load();
		canvas->drawSimpleText(ss.str().c_str(), ss.str().size(), SkTextEncoding::kUTF8, (info->width - info->startx_pos4) * .5, info->starty_pos,
			font,
			paint);
		std::stringstream ss2;
		ss2 << std::fixed << std::setprecision(1) << mbs;
		auto w2 = tsl::graphics::InfoPanel::measureWidth(font, ss2.str().c_str());
		canvas->drawSimpleText(ss2.str().c_str(), ss2.str().size(), SkTextEncoding::kUTF8, (info->width + info->startx_pos4) * .5 - w2, info->starty_pos,
			font,
			paint);
		canvas->drawSimpleText(" MB", 3, SkTextEncoding::kUTF8, (info->width + info->startx_pos4) * .5, info->starty_pos,
			font,
			paint);
		canvas->restore();
	}

	static void render_progress(InfoPanel* info, void* context) {
		auto _appState = info->_appState;
		float progress = info->progress.load();
		long offset = info->dec_offset.load();
		float line_width = View::lw;

		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);
		canvas->save();
		canvas->translate(info->startx, info->starty);

		SkPaint paint;
		paint.setStrokeWidth(line_width);
		paint.setAntiAlias(true);
		paint.setColor(skcol::blue_transparent);
		auto& font = info->font;

		canvas->drawRect(SkRect::MakeXYWH(0, SkDoubleToScalar(line_width),
			SkDoubleToScalar(info->width * progress),
			info->height),
			paint);

		paint.setColor(skcol::fg);

		TIME_P t{};
		time_convert(t, _STATE->sr, offset);
		char text[20];
		std::stringstream ss;

		ss << info->text.load() << info->text2.load();
		canvas->drawSimpleText(ss.str().c_str(), ss.str().size(), SkTextEncoding::kUTF8, info->startx_pos1, info->starty_pos,
			font,
			paint);

		snprintf(text, 20, "%02d : %02d : %03d", t.m, t.s, t.ms);
		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, info->startx_rec,
			info->starty_pos, font, paint);

		std::stringstream ss2;
		auto mbs = info->bytes.load() / (1024. * 1024.);

		ss2 << std::fixed << std::setprecision(1) << mbs;
		auto w2 = tsl::graphics::InfoPanel::measureWidth(font, ss2.str().c_str());
		auto syy = info->width - info->startx_pos1 - tsl::graphics::InfoPanel::measureWidth(font, " MB");
		canvas->drawSimpleText(ss2.str().c_str(), ss2.str().size(), SkTextEncoding::kUTF8, syy - w2, info->starty_pos,
			font,
			paint);
		canvas->drawSimpleText(" MB", 3, SkTextEncoding::kUTF8, syy, info->starty_pos,
			font,
			paint);

		/*
		snprintf(text, 20, "%d%%", (int) (progress * 100));
		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, 0, info->starty_pos,
							   font,
							   paint);
							   */
		canvas->restore();
	}

	static void render_text(InfoPanel* info, void* context) {
		auto text = info->text.load();
		if (!text)
			return;
		if (!strcmp(text, "Buffersize too big.") && tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L) {
			info->renderfunc = render_normal;
			return;
		}
		auto _appState = info->_appState;

		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);
		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::fg);
		auto& font = info->font;

		// canvas->drawText(text, strlen(text), info->startx_rec, info->starty_pos, paint);
		View::textDisplayCenteredFixed(info, canvas, paint, font, text, 1.0, false);
	}

	static void render_text_frombuffer(InfoPanel* info, void* context) {
		auto len = strlen(info->textBuffer);
		if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L ||
			!len || len == 101) {
			info->renderfunc = info->prevRenderfunc != nullptr ? info->prevRenderfunc : render_normal;
			return;
		}

		auto _appState = info->_appState;

		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);
		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);
		paint.setColor(skcol::fg);
		auto& font = info->font;

		// canvas->drawText(text, strlen(text), info->startx_rec, info->starty_pos, paint);
		View::textDisplayCenteredFixed(info, canvas, paint, font, info->textBuffer, 1.0, false);
	}


	static void render_dec(InfoPanel* info, void* context) {
		auto _appState = info->_appState;
		long samples = info->dec_offset.load();

		TIME_P t{};
		time_convert(t, _STATE->sr, (long)samples);
		char text[20];
		snprintf(text, 20, "%02d : %02d", t.m, t.s);
		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);
		canvas->save();
		canvas->translate(info->startx, info->starty);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);

		paint.setAntiAlias(true);

		paint.setColor(skcol::fg);
		auto& font = info->font;

		canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, info->startx_rec,
			info->starty_pos, font, paint);
		canvas->restore();
	}

	static void render_xrun(InfoPanel* info, void* context) {

		if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L) {
			info->renderfunc = render_normal;
			return;
		}
		auto _appState = info->_appState;

		char text[30];
		snprintf(text, 30, "XRUN #%d", info->xruncount.load());
		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);

		paint.setColor(skcol::fg);

		auto& font = info->font;

		View::textDisplayCenteredFixed(info, canvas, paint, font, text, 1.0, false);
	}

	static void render_value(InfoPanel* info, void* context) {
		if (tsl::time::nanosecondsSinceEpoch() - info->tstamp.load() > 1e+9L ||
			info->miditargetindex == PARAM_NOT_ASSIGNED) {
			info->renderfunc = render_normal;
			return;
		}
		auto _appState = info->_appState;

		auto& miditarget = info->miditargetindex > NUM_PARAMS ? _STATE->parameters[0]
			: _STATE->parameters[info->miditargetindex];

		auto ref = info->ref.load();
		auto reference = ref != nullptr ? ref->load() : 0;
		auto& buffer = info->textBuffer;
		int pos = 0;

		if (miditarget.name)
			pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%s: ", miditarget.name);
		pos = std::min(pos, (int)sizeof(buffer) - 1);
		if (!strcmp(miditarget.name, "RATIO")) {
			if (reference == 0.)
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, "BYPASS");
			else if (reference == 1.0)
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, "INF : 1");
			else
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%.1f : 1", 1. / (1. - reference));
			pos = std::min(pos, (int)sizeof(buffer) - 1);
		}
		else {
			auto display = miditarget.toDisplay(_appState->sr, reference);
			pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%.*f", miditarget.digits, display);
			pos = std::min(pos, (int)sizeof(buffer) - 1);
			if (miditarget.valuename) {
				pos += snprintf(buffer + pos, sizeof(buffer) - pos, " %s", miditarget.valuename);
				pos = std::min(pos, (int)sizeof(buffer) - 1);
			}
		}
		auto* canvas = (SkCanvas*)context;
		info->flush(canvas);

		SkPaint paint;
		paint.setStrokeWidth(View::lw);
		paint.setAntiAlias(true);

		paint.setColor(skcol::fg);

		auto& font = info->font;

		View::textDisplayCenteredFixed(info, canvas, paint, font, buffer, 1.0,
			false);
	}

	void InfoPanel::setRenderFunc(RenderFunc func) {
		switch (func) {
		
		case RenderMic:
			// info->active = true;
			renderfunc = render_mic;
			break;
		case RenderStand:
			// info->active = false;
			renderfunc = render_normal;
			break;
		case RenderVoid:
			renderfunc = render_void;
			break;
		case RenderText:
			renderfunc = render_text;
			break;
		case RenderParam:
			setTimeStamp();
			renderfunc = render_value;
			break;
		case RenderProg:
			renderfunc = render_progress;
			break;
		case RenderDecode:
			renderfunc = render_dec;
			break;
		case RenderXRun:
			setTimeStamp();
			renderfunc = render_xrun;
			break;
		case RenderEncode:
			renderfunc = render_encode;
			break;
		case RenderBuffer:
			setTimeStamp();
			renderfunc = render_text_frombuffer;
		default:
			break;
		}
	}

	void InfoPanel::setTimeStamp() {
		tstamp = tsl::time::nanosecondsSinceEpoch();
	}


	void InfoPanel::prepareBuffer(tsl::parameters::Event& ev) {
		const auto tindex = _STATE->active_track.load();

		const bool renderTrackName = ev.trackIndex != tindex;

		auto& buffer = textBuffer;
		int pos = 0;
		if (renderTrackName) {
			auto trackname = tracknames[ev.trackIndex].data();
			pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%s ", trackname);
		}
		ev.toString(_STATE, pos, buffer, 100, true);
		auto func = renderfunc.load();
		if (func != render_text_frombuffer)
			prevRenderfunc = func;
		setRenderFunc(RenderBuffer);

	}


}
