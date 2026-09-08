//
// Created by pr on 12.07.19.
//

#include "logger.h"
#include <include/core/SkPath.h>
#include <include/core/SkFont.h>
#include "frequencyresponce.h"
#include "eq.h"
#include "Input.h"
#include "tools/StackVector.h"


static float freq_at_x(const float x, const float m0_width) {
	return 20.f * powf(1000.f, x / m0_width);
}

static float x_at_freq(const float f, const float m0_width) {
	return m0_width * logf(f / 20.f) / logf(1000.f);
}


namespace tsl::graphics {
	void EQ5RenderWindow::render(void* context) {
		auto tindex = _STATE->active_track.load();
		float vars[NUM_ElementS_EQVARS];
		for (int32_t i = 0; i < NUM_ElementS_EQVARS; i++)
			vars[i] = _STATE->params[tindex][_paramOffset + i].load();
		//_DATA->frequencyResponse->compute(_STATE->sr, &vars[0], Eq5::computeResponse);
		auto canvas = _appState->graphics.getCanvas(windex, 0, 0, editor.width, editor.height, false);
		if (!canvas) {
			LOGE("Failed to create waveform Skia surface");
			return;
		}
		auto rootCanvas = static_cast<SkCanvas*>(context);
		bool expected = true;
		if (redrawNeeded.compare_exchange_strong(expected, false)) {
			int x = startx - editor.startx, y = starty - editor.starty;
			FrequencyResponse::render(_appState, canvas, x, y, width, height, &vars[0], 5, _activeSeg.load());
		}
		canvas->getSurface()->draw(rootCanvas, editor.startx, editor.starty);
	}



	void EQ5DBScale::render(void* context) {
		auto* canvas = (SkCanvas*)context;
		SkPaint paint;
		paint.setStrokeWidth(lw);
		paint.setAntiAlias(true);
		paint.setStyle(SkPaint::kFill_Style);
		flush(canvas);
		canvas->save();
		canvas->translate(startx, starty);
		float textsize = _STATE->textsize2 * .6f;
		float t2 = textsize * .25;
		SkFont font(_STATE->font_normal);
		font.setSize(textsize);
		paint.setColor(skcol::fg);

		for (int32_t i = 0; i < _names.size(); i++) {
			canvas->drawSimpleText(_names[i].c_str(), strlen(_names[i].c_str()), SkTextEncoding::kUTF8,
				xoff,
				height - (i / (float)(_names.size() - 1) * height) +
				t2,
				font,
				paint);
		}

		canvas->restore();
	}


	void EQ5FrScale::render(void* context) {
		float w = width, h = height;
		auto* canvas = (SkCanvas*)context;
		SkPaint paint;
		paint.setStrokeWidth(lw);
		paint.setAntiAlias(true);
		paint.setStyle(SkPaint::kFill_Style);
		flush(canvas);
		canvas->save();
		canvas->translate(startx, starty);
		float textsize = _STATE->textsize2 * .6f;
		float t2 = textsize;// * .85f;
		SkFont font(_STATE->font_normal);
		font.setSize(textsize);
		paint.setColor(skcol::fg);
		const float* points = _DATA->frequencyResponse.points;
		for (int32_t i = 0; i < ARRAY_LEN(FrequencyResponse::frequencynames2); i++) {
			canvas->drawSimpleText(FrequencyResponse::frequencynames2[i],
				strlen(FrequencyResponse::frequencynames2[i]),
				SkTextEncoding::kUTF8,
				i * 0.1 * w - measureWidth(font, FrequencyResponse::frequencynames2[i]) * .5f, textsize,
				font, paint);
		}

		canvas->restore();
	}

	void EQ5RenderWindow::callback(const InputEvent& event) {
		const int32_t action = event.action;
		const int32_t pointerid = event.pointer_id;

		auto tindex = _STATE->active_track.load();
		std::atomic<MYFLOAT>* vars[NUM_ElementS_EQVARS];
		double defaults[NUM_ElementS_EQVARS];
		for (int32_t i = 0; i < NUM_ElementS_EQVARS; i++) {
			vars[i] = &_STATE->params[tindex][_paramOffset + i];
			defaults[i] = _STATE->parameters[_paramOffset + i].initvalue;
		}
		const float xpos = event.x - startx;
		const float ypos = event.y - starty;

		//LOGE("%f %f %f %f", _xpos, xpos, _ypos, ypos);
		const float w = width;

		//LOGE("%f", freq_at_x(xpos, width));
		const float h = height;
		const float segsize = w * .1f;
		const float segsized2 = segsize * .5f;


		switch (action) {
		case ACTION_DOWN: {
			for (int32_t i = 0; i < 5; i++) {
				float val = LOG2NORMALF(vars[i * 3]->load());
				if (val < 20.f)
					val = 20.f;
				else if (val > 20000.f)
					val = 20000.f;
				float x = x_at_freq(val, w);
				float y =
					h - ((float)vars[i * 3 + 1]->load() + FrequencyResponse::eq5rangedb2) /
					FrequencyResponse::eq5rangedb * h;
				if (xpos >
					x - segsized2 &&
					xpos < x + segsized2 &&
					ypos > y -
					segsized2 &&
					ypos < y + segsized2) {
					_activeSeg = i;
					tsl::graphics::InputSystem::Pointer pointer(pointerid, xpos, ypos, tsl::graphics::InputSystem::WinState::WINDRAG, i);
					pointer.target = i;
					inputstate.addPointer(pointer);
					valueView.addDraw();
					redrawNeeded.store(true, std::memory_order_release);
					return;
				}
			}
			auto pt = inputstate.nearest(xpos, ypos);
			if (pt != nullptr) {
				tsl::graphics::InputSystem::Pointer pointer(pointerid, xpos, ypos, tsl::graphics::InputSystem::WinState::WINZOOM, pt->target);
				inputstate.addPointer(pointer);
				redrawNeeded.store(true, std::memory_order_release);
			}
			break;
		}
		case ACTION_MOVE: {
			for (auto& pt : inputstate) {
				if (pt.id == pointerid) {

					int32_t active_seg = pt.target;
					if (pt.mode == tsl::graphics::InputSystem::WinState::WINDRAG) {
						float val = -FrequencyResponse::eq5rangedb2 +
							FrequencyResponse::eq5rangedb * (h - ypos) / h;
						if (val >= FrequencyResponse::eq5rangedb2)
							val = FrequencyResponse::eq5rangedb2;
						else if (val <= -FrequencyResponse::eq5rangedb2)
							val = -FrequencyResponse::eq5rangedb2;
						if (vars[active_seg * 3 + 1] != nullptr)
							vars[active_seg * 3 + 1]->store(val);
						if (xpos < 0)
							val = 0;
						else if (xpos > w)
							val = w;
						else val = xpos;
						val = freq_at_x(val, w);
						if (vars[active_seg * 3] != nullptr)
							vars[active_seg * 3]->store(LOG10D20(val));
					}
					else {
						float diffx = (xpos - pt.xpos) / w;
						//float diffy = (pt.ypos - ypos) / h;
						if (diffx != 0) {
							double qval = LOG2NORMAL(vars[active_seg * 3 + 2]->load());
							qval += diffx;
							if (qval < 0.01f)
								qval = 0.01f;
							else if (qval > 10.f)
								qval = 10.f;
							vars[active_seg * 3 + 2]->store(LOG10D20(qval));
						}
					}
					pt.xpos = xpos;
					pt.ypos = ypos;
					_activeSeg = active_seg;
					valueView.addDraw();
					redrawNeeded.store(true, std::memory_order_release);

				}
			}
			break;
		}
		case ACTION_UP: {
			int32_t active_seg = _activeSeg;
			auto p = inputstate.getById(pointerid);
			// p is null when the DOWN missed every handle: the pointer is captured
			// at the editor, so the UP still lands here with nothing in inputstate.
			// _activeSeg can survive such a gesture (a Q-pinch whose drag finger
			// lifts first leaves it set), so without this guard a miss-and-release
			// dereferenced null below -- and a quick miss-tap took the double-click
			// branch and reset a band that was never touched.
			if (p != nullptr && !(active_seg < 0 || active_seg > 4)) {
				bool doubleClick = false;
				std::vector<tsl::parameters::Event> events;

				if (inputstate.lastpointer == pointerid && inputstate.timer.elapsedReplace() < .3) {
					vars[active_seg * 3]->store(defaults[active_seg * 3]);
					vars[active_seg * 3 + 1]->store(defaults[active_seg * 3 + 1]);
					vars[active_seg * 3 + 2]->store(defaults[active_seg * 3 + 2]);
					// defaults[] is indexed 0..NUM_ElementS_EQVARS-1, NOT by absolute
					// param id - _paramOffset belongs on the event's param id only.
					events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3, defaults[active_seg * 3], 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
					events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3 + 1, defaults[active_seg * 3 + 1], 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
					events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3 + 2, defaults[active_seg * 3 + 2], 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
					doubleClick = true;
				}
				if (!doubleClick) {
					if (p->mode == tsl::graphics::InputSystem::WinState::WINDRAG) {
						if (p->xposWhenCreated != xpos) {
							events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3, _STATE->params[tindex][_paramOffset + active_seg * 3].load(), 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
						}
						if (p->yposWhenCreated != ypos) {
							events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3 + 1, _STATE->params[tindex][_paramOffset + active_seg * 3 + 1].load(), 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
						}
					}
					else if (p->mode == tsl::graphics::InputSystem::WinState::WINZOOM && (p->xposWhenCreated != xpos || p->yposWhenCreated != ypos)) {
						events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, _paramOffset + active_seg * 3 + 2, _STATE->params[tindex][_paramOffset + active_seg * 3 + 2].load(), 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
					}
				}
				events.push_back(tsl::parameters::Event::createRerenderEvent(tindex, _paramOffset));

				if (events.size() > 0) {
					_DATA->snapShot.add_task([this, ev = std::move(events)] () mutable {
						auto groupId = _DATA->snapShot.nextGroupId();
						auto ee = tsl::parameters::Event::createTextEvent(0, "EQ5 EDIT", 0, 0, groupId);
						std::lock_guard lk(_DATA->snapShot);
						_DATA->snapShot.addEvent(ee);
						for (auto& e : ev) {
							e.groupId = groupId;
							_DATA->snapShot.addEvent(e);
						}
						});
				}

			}




			inputstate.lastpointer = pointerid;
			if (p && p->mode != tsl::graphics::InputSystem::WinState::WINZOOM && _activeSeg.load() == p->target)
				_activeSeg.store(-1);
			inputstate.removePointer(pointerid);
			redrawNeeded.store(true, std::memory_order_release);
			break;
		}

		default:
			break;
		}
		//LOGE("%ld %f %f %f", AMotionEvent_getPointerCount(event), track->zoom.load(), track->pos.load(), olddist);
	}
}
#include "track.h"


float goertzel_mag(int32_t numSamples, float TARGET_FREQUENCY, float SAMPLING_RATE, float* data) {
	int32_t i;
	float floatnumSamples;
	float omega, sine, cosine, coeff, q0, q1, q2, magnitude, real, imag;

	float scalingFactor = numSamples / 2.0;

	floatnumSamples = (float)numSamples;
	float k = 0.5 + ((floatnumSamples * TARGET_FREQUENCY) / SAMPLING_RATE);
	omega = (2.0 * PI_P * k) / floatnumSamples;
	sine = sin(omega);
	cosine = cos(omega);
	coeff = 2.0 * cosine;
	q0 = 0;
	q1 = 0;
	q2 = 0;

	for (i = 0; i < numSamples; i++) {
		q0 = coeff * q1 - q2 + data[i];
		q2 = q1;
		q1 = q0;
	}

	// calculate the real and imaginary results
	// scaling appropriately
	real = (q1 - q2 * cosine);
	imag = (q2 * sine);

	magnitude = sqrtf(real * real + imag * imag);
	return magnitude;
}

float
goertzel_mag_non_integer(int32_t numSamples, float TARGET_FREQUENCY, float SAMPLING_RATE, float* data) {
	int32_t i;
	float pikterm = TWOPI_F_P * (TARGET_FREQUENCY / SAMPLING_RATE);
	float cospikterm2 = cosf(pikterm) * 2.f;
	float sine = -sin(pikterm);
	float cosine = cos(pikterm);
	float s0 = 0;
	float s1 = 0;
	float s2 = 0;

	int32_t end = numSamples - 1;
	for (i = 0; i < end; i++) {
		//float multiplier = 0.5f * (1 - cosf(TWOPI_F_P*i/(float) end));
		s0 = data[i] + cospikterm2 * s1 - s2;
		s2 = s1;
		s1 = s0;
	}
	s0 = data[end] + cospikterm2 * s1 - s2;
	std::complex<float> res(s0 - s1 * cosine, s1 * sine);
	std::complex<float> finalfact(cos(pikterm * (numSamples - 1)),
		-sin(pikterm * (numSamples - 1)));
	return std::abs(res * finalfact);
}

typedef std::complex<float> dcomp;


dcomp divide(dcomp x, dcomp y) {
	dcomp z;
	z.real((x.real() * y.real() + x.imag() * y.imag()) /
		(y.real() * y.real() + y.imag() * y.imag()));
	z.imag((x.imag() * y.real() - x.real() * y.imag()) /
		(y.real() * y.real() + y.imag() * y.imag()));
	return z;
}

float computeresponse(const float* a, const float* b, float fr, float sr, int32_t stages) {

	const float w = fr / sr * TWOPI_F_P;
	/*
		const double z1 = cos(w);
		const double z2 = cos(w*2);
		double r = 1;
		for (int32_t i = 0; i < stages; i++) {
			double num = (a[i * 3] + a[i * 3 + 1] * z1+ z2 * a[i * 3 + 2]);
			double denum = (1. - b[i * 3 + 1] * z1 - z2 * b[i * 3 + 2]);
			r *= (num / denum);
		}

		return (float) r;
	*/


	const std::complex<float> z = exp(w * -J);
	const std::complex<float> z2 = z * z;

	std::complex<float> res = 1;

	for (int i = 0; i < stages; i++) {
		std::complex<float> num = (a[i * 3] + a[i * 3 + 1] * z + z2 * a[i * 3 + 2]);
		std::complex<float> denum = (1.f - b[i * 3 + 1] * z - z2 * b[i * 3 + 2]);
		res *= (num / denum);
	}


	return (float)std::abs(res);

}
// --- Your existing computeresponse function (from previous discussions) ---
// This function needs to be available to use the biquad coeffs.
// Re-adding it here for a self-contained example.
#define J std::complex<float>(0.0f, 1.0f) // The imaginary unit

float computeresponse2(const float* a_coeffs_num, const float* b_coeffs_den, float fr, float sr, int32_t stages) {
	const float w = fr / sr * TWOPI_P;
	const std::complex<float> z_inv = exp(-J * w);
	const std::complex<float> z_inv_2 = z_inv * z_inv;
	std::complex<float> total_response = 1.0f;

	for (int32_t i = 0; i < stages; i++) {
		// a_coeffs_num are actually the numerator (b0, b1, b2)
		float current_b0 = a_coeffs_num[i * 3];
		float current_b1 = a_coeffs_num[i * 3 + 1];
		float current_b2 = a_coeffs_num[i * 3 + 2];
		// b_coeffs_den are actually the denominator (a1, a2)
		float current_a1 = b_coeffs_den[i * 3 + 1]; // Assuming a1 at index 0, a2 at index 1
		float current_a2 = b_coeffs_den[i * 3 + 2];

		std::complex<float> num_stage = current_b0 + current_b1 * z_inv + current_b2 * z_inv_2;
		std::complex<float> denum_stage = 1.0f + current_a1 * z_inv + current_a2 * z_inv_2; // Corrected: + for a1, a2

		if (std::abs(denum_stage) < std::numeric_limits<float>::epsilon()) {
			return 1000.;
		}
		total_response *= (num_stage / denum_stage);
	}
	return std::abs(total_response);
}




const char* pointer_names[] = { "L", "1", "2", "3", "H" };

using namespace tsl::graphics;

void FrequencyResponse::render(tsl::AppState* _appState, SkCanvas* canvas, int startx, int starty, int width, int height, float* vars, int32_t numpointers, int activeSeg) {
	canvas->clear(SK_ColorTRANSPARENT);
	canvas->save();
	canvas->translate(startx, starty);
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setStyle(SkPaint::kStroke_Style);
	SkPath path;
	paint.setStrokeWidth(2.0);
	paint.setColor(skcol::grey);

	path.moveTo(0, 0);
	path.lineTo(0, height);

	path.moveTo(width, 0);
	path.lineTo(width, height);

	for (auto point : dbpoints) {
		path.moveTo(0, height / 2 - height * (point / 48.f));
		path.lineTo(width, height / 2 - height * (point / 48.f));
	}


	for (auto point : frequencypoints) {
		path.moveTo(point * width, 0);
		path.lineTo(point * width, height);
		//  LOGE("%f", points[i]);
	}
	canvas->drawPath(path, paint);
	const float textsize = _STATE->textsize2 * .9f;
	const float t2 = textsize * .33f;
	SkFont font(_STATE->font_normal);
	font.setSize(textsize);
	paint.setStyle(SkPaint::kFill_Style);

	const float radius = _STATE->circleradius;

	if (vars != nullptr && numpointers != 0) {
		for (int32_t i = 0; i < numpointers; i++) {
			float val = LOG2NORMALF(vars[i * 3]);
			if (val < 20.f)
				val = 20.f;
			else if (val > 20000.f)
				val = 20000.f;
			float x = x_at_freq(val, width);
			float y = (vars[i * 3 + 1] + eq5rangedb2) / eq5rangedb;
			paint.setColor(i == activeSeg ? skcol::grey : (i == 0 || i == numpointers - 1
				? skcol::blue_violet_transparent
				: skcol::blue_transparent));
			canvas->drawCircle(x, height - y * height, radius, paint);
			paint.setColor(skcol::fg);
			// paint.setStyle(SkPaint::kStroke_Style);
			canvas->drawSimpleText(pointer_names[i], strlen(pointer_names[i]),
				SkTextEncoding::kUTF8, x - t2,
				height - y * height + t2, font, paint);

		}
	}

	path.reset();
	path.addRect(SkRect::MakeXYWH(1, -height, width - 1, height * 3));
	canvas->clipPath(path);
	path.reset();
	paint.setStrokeWidth(View::lw);
	paint.setColor(skcol::orange);
	paint.setStyle(SkPaint::kStroke_Style);

	float lw = View::lw;
	float sr = _STATE->sr;
	float a[15], b[15];
	Eq5::getCoeffs(&a[0], &b[0], sr, vars);

	double freq = freq_at_x(0, width);
	double mag = computeresponse(&a[0], &b[0], freq, sr, 5);
	float h = (LOG10D20F(mag) + eq5rangedb2) / eq5rangedb;
	path.moveTo(0,
		height - h * height);
	for (int32_t i = lw; i < width; i += lw) {
		freq = freq_at_x(i, width);
		mag = computeresponse(&a[0], &b[0], freq, sr, 5);
		h = (LOG10D20F(mag) + eq5rangedb2) / eq5rangedb;
		path.lineTo(i, height - h * height);
	}

	canvas->drawPath(path, paint);
	canvas->restore();
}
