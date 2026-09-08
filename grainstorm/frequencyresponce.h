#pragma once
//
// Created by pr on 12.07.19.
//

#ifndef GRAINSTORM_FREQUENCYRESPONCE_H
#define GRAINSTORM_FREQUENCYRESPONCE_H


#include <cstdint>
#include "types.h"
#include <defines.h>
#include "SpectrumAnalyzerView.h"
#include <view.h>
#include <Input.h>
#include "SkPath.h"
#include <skia.h>
#include <EnterValue.h>
#include <app.h>

namespace tsl {

	namespace graphics {

		class EQ5RenderDummy : public View {
		public:
			EQ5RenderDummy(tsl::AppState* appState, View& par, int32_t paramOffset) : View(appState, WRAP, 0, END_ALIGN) {
			};

			void render(void* ctx) override {};

			void callback(const InputEvent& inputEvent) override {};

			void addRecursiveDraw()override {
			}
			void delRecursiveDraw()override {
			}
			void delRecursiveCB()override {
			}
		};


		class EQ5FrScale : public View {
		public:
			EQ5FrScale(tsl::AppState* appState) : View(appState, WRAP, 0, END_ALIGN) {}

			void render(void* ctx) override;
		};


		class EQ5DBScale : public View {
		public:
			EQ5DBScale(tsl::AppState* appState, std::vector<std::string>& names) : View(appState, VALUE_FROM_POINTER,
				VALUE_FROM_POINTER, START_ALIGN) {
				_names = names;
			}

			void render(void* ctx) override;

			void init() override {
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2 * .6f);
				float max = 0;
				for (auto& n : _names) {
					auto s = measureWidth(font, n.c_str());
					max = s > max ? s : max;
				}
				xoff = (width - max) * .5f;
			}

		private:
			std::vector<std::string> _names;
			float xoff{};
		};
		class EQ5RenderWindow : public View {
		public:
			EQ5RenderWindow(tsl::AppState* appState, View& par, int32_t paramOffset) : View(appState, WRAP, 0, END_ALIGN), editor(par), _paramOffset(paramOffset), valueView(appState, [&val = valueView, &par, &_act = _activeSeg]() {
				auto _appState = par._appState;
				if (val.timer.elapsed() > 1.) {
					val.perm = false;
					_STATE->graphics.deleteWindow(val.windex);
					return;
				}

				auto tindex = _STATE->active_track.load();
				int32_t active_seg = _act.load();
				if (active_seg < 0 || active_seg > 4)
					return;

				auto starty = par.stopy.load();
				auto height = _STATE->textsize1;
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2 * .9);
				float startx_pos1, starty_pos;//"Pos: 00 : 00 : 000   Dur: 00 : 00 : 000   Playdur: 00 : 00 : 000";
				auto width = measureTextFixed(100, height, _STATE->font_normal, " 20000 Hz / -24dB / Q = 1.00 ", &startx_pos1, &starty_pos,
					_STATE->textsize2 * .9);
				auto startx = par.startx + (par.width - width) * .5;


				auto asfx = GASFX;
				float freq = LOG2NORMAL(_STATE->params[tindex][(asfx == SPACE_EQ5 ? EQ5LOWCF : DYNEQ5LOWCF) +
					active_seg * 3].load());
				float gain = _STATE->params[tindex][(asfx == SPACE_EQ5 ? EQ5LOWGAIN : DYNEQ5LOWGAIN) +
					active_seg * 3].load();
				float q = LOG2NORMAL(_STATE->params[tindex][(asfx == SPACE_EQ5 ? EQ5QLOW : DYNEQ5QLOW) +
					active_seg * 3].load());


				auto canvas = _STATE->graphics.getCanvas(val.windex, startx, starty, width, height);
				if (!canvas)
					return;
				canvas->clear(skcol::bg);

				SkPaint paint;
				paint.setStrokeWidth(View::lw);
				paint.setAntiAlias(true);

				paint.setColor(skcol::fg);
				//                snprintf(text, 30, "%.0f Hz / %.0f dB / Q = %.2f", freq, gain, q);

				char text[30];
				snprintf(text, 30, "%.2f ", q);
				auto sx = width - measureWidth(font, text);
				canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, sx, starty_pos, font, paint);
				sx = width - measureWidth(font, "1.00 ");
				snprintf(text, 30, "%.0f dB   Q = ", gain);
				canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, sx - measureWidth(font, text), starty_pos, font, paint);
				sx = width - measureWidth(font, "-24 dB / Q = 1.00 ");
				snprintf(text, 30, "%.0f Hz   ", freq);
				canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, sx - measureWidth(font, text), starty_pos, font, paint);
				}) {
				_appState = par._appState;
			};

			void render(void* ctx) override;

			void callback(const InputEvent& inputEvent) override;

			void addRecursiveDraw()override {
				redrawNeeded.store(true, std::memory_order_release);
			}
			void delRecursiveDraw()override {
				valueView.delRecursiveDraw();
				_STATE->graphics.deleteWindow(windex);
				windex = -1;
			}
			void delRecursiveCB()override {
				inputstate.clear();
				_activeSeg = -1;
			}
			tsl::graphics::InputSystem::InputState inputstate;
			std::atomic<int> _activeSeg{ -1 };
			ValueView valueView;
		private:
			View& editor;
			int32_t _paramOffset;
			int windex{ -1 };
			std::atomic<bool> redrawNeeded{ true };
		};


		template<typename T = EQ5RenderWindow, int32_t PARAMOFFSET = EQ5LOWCF>
		class EQ5Editor : public HorizontalLayout {
		public:
			EQ5Editor(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment, std::atomic<bool>(&updateRenderThread)[4],
				std::atomic<double*>(&input)[4], std::vector<std::string>& dbs = dbpointstext) : HorizontalLayout(appState,
					_scalefactor, _aspect_ratio, _alignment) {
				_STATE->parameters[PARAMOFFSET].view = this;
				dummybottom = new VerticalLayout{ appState, VALUE_FROM_POINTER,
											VALUE_FROM_POINTER, END_ALIGN };
				dummytop = new View{ appState, VALUE_FROM_POINTER,
							   VALUE_FROM_POINTER, START_ALIGN };
				dummybottomleft = new View{ appState, VALUE_FROM_POINTER,
									  VALUE_FROM_POINTER, START_ALIGN };
				dummycenter = new VerticalLayout{ appState, WRAP, 0, CENTER_ALIGN };
				frscale = new EQ5FrScale{ appState };
				dbscale = new EQ5DBScale{ appState, dbs };
				window = new T{ appState, *this, PARAMOFFSET };
				window->overlap = true;
				prio = 10;
				addChild(dummybottom);
				dummybottom->size_reference = &_STATE->windowHeight;
				dummybottom->size_reference_scale = .7f / 15.;

				addChild(dummytop);
				dummytop->size_reference = &_STATE->windowHeight;
				dummytop->size_reference_scale = .7f / 15.;

				dummybottom->addChild(dummybottomleft);
				dummybottomleft->size_reference = &_STATE->windowHeight;
				dummybottomleft->size_reference_scale = .6f / 15.;


				dummybottom->addChild(frscale);

				addChild(dummycenter);
				dummycenter->addChild(dbscale);
				dbscale->size_reference = &_STATE->windowHeight;
				dbscale->size_reference_scale = .5f / 15.;
				dummycenter->addChild(window);
				spectrum = new SpectrumAnalyzerView{ appState, updateRenderThread, input };
				spectrum->paddingleft = spectrum->paddingright = window->paddingleft = window->paddingright = dummybottom->paddingleft = dummybottom->paddingright = dummytop->paddingleft = dummytop->paddingright = 1.25;
				spectrum->overlap = true;
				dummycenter->addChild(spectrum);
				perm = true;
			}


			void callback(const InputEvent& e) override {
				window->callback(e);
			}

			void render(void* ctx) override {
				auto c = (SkCanvas*)ctx;
				flush(c);
				c->save();
				SkPath path;
				path.addRect(SkRect::MakeXYWH(startx, starty, width, height));
				c->clipPath(path);
				spectrum->render(c);
				dbscale->render(c);
				frscale->render(c);
				window->render(c);
				c->restore();
			}

			void addRecursiveDraw() override {
				if (visible_)
					window->addRecursiveDraw();
				else {
					spectrum->activeTrack_ = _STATE->active_track.load();
					spectrum->updateRenderThread_[spectrum->activeTrack_] = true;
					if (auto buf = spectrum->input_[spectrum->activeTrack_].exchange(nullptr, std::memory_order_acq_rel)) {
						_STATE->pool.release(buf);
					}
					window->addRecursiveDraw();
					View::addRecursiveDraw();
				}
			}

			void delRecursiveDraw() override {
				spectrum->updateRenderThread_[spectrum->activeTrack_] = false;
				window->delRecursiveDraw();
				View::delRecursiveDraw();
			}

			void addRecursiveCB() override {
				View::addRecursiveCB();
			}

			void delRecursiveCB() override {
				window->delRecursiveCB();
				_STATE->queue_callback.del(this);
			}
		protected:
			VerticalLayout* dummybottom{};
			View* dummytop{};
			View* dummybottomleft{};
			VerticalLayout* dummycenter{};
			EQ5FrScale* frscale{};
			EQ5DBScale* dbscale{};
			T* window{};
			SpectrumAnalyzerView* spectrum{};

		private:
			static std::vector<std::string> dbpointstext;
		};
		template<typename T, int32_t PARAMOFFSET>
		std::vector<std::string>
			EQ5Editor<T, PARAMOFFSET>::dbpointstext = { "-24", "-18", "-12", " -6", "  0", "  6", " 12", " 18",
									   " 24" };
	}
}

class FrequencyResponse {
public:

	static constexpr float eq5rangedb = 48.f;
	static constexpr float eq5rangedb2 = eq5rangedb / 2.f;
	static constexpr float frresponsedrawoffset = 20.f;
	static constexpr float frresponsedrawend = 20000.f;
	static constexpr float frresponsefrequencyrange = frresponsedrawend - frresponsedrawoffset;

	static constexpr float frequencypoints[] = { 25.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f,
												4000.f, 10000.f };
	static constexpr float frequencypoints2[] = { 20.f, 39.9f, 79.62f, 158.86f, 316.97f, 632.455f,
												 1261.91f, 2517.85f, 5023.77f, 10023.744, 20000.0 };
	static constexpr const char* frequencynames2[] = { "20", "40", "80", "160", "320", "640", "1K2", "2K5",
												"5K", "10K", "20K" };

	static constexpr float dbpoints[] = { -24.f, -18.f, -12.f, -6.f, 0.0f, 6.f, 12.f, 18.f, 24.f };

	static constexpr float normcoeff = 0.01162629691f;

	explicit FrequencyResponse(int size) {
		M = size;
		NYQ = M / 2;
		scale = 1.f / (float)M;
		for (int32_t i = 0; i < ARRAY_LEN(frequencypoints); i++)
			points[i] = (LOG10D20F(frequencypoints[i] - frresponsedrawoffset)) * normcoeff;
	}

	static void
		render(tsl::AppState* _appState, SkCanvas* canvas, int startx, int starty, int width, int height, float* vars = nullptr, int32_t numpoints = 0, int activeSeq = 0);

	float scale;
	uint32_t M, NYQ;
	float points[ARRAY_LEN(frequencypoints)]{};
};


#endif //GRAINSTORM_FREQUENCYRESPONCE_H
