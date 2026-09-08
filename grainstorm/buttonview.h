#pragma once
#ifndef ButtonView_H
#define ButtonView_H

#include <cstdint>
#include <atomic>
#include <vector>
#include "types.h"
#include "grainstorm.h"
#include "view.h"
#include "textview.h"
#include "button.h"
#include "Input.h"
#include <SkPaint.h>
#include "app.h"

namespace tsl::graphics {
	template<typename T>
	class ButtonView : public View {
	public:
		ButtonView(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio,
			int32_t _alignment, int _orientation, int numelements, std::span<const std::string_view>(_names),
			std::span<const float>(_values), uint16_t _id,
			long _offset = 0,
			int32_t _multi = 1) : View(appState, _scalefactor, _aspect_ratio, _alignment, 10, false,
				"BV"), titleview{ appState, "BV", START_ALIGN } {
			id = _id;
			e.setup(_STATE, 0, id);
			_STATE->parameters[id].view = this;
			_STATE->parameters[id].type = ParameterType_enum;
			if (!_names.empty()) {
				if(!_values.empty())
				_STATE->parameters[id].values = _values.subspan(0, numelements);
				_STATE->parameters[id].names = _names.subspan(0, numelements);  // <-- store span, not pointer
			}
			if (_STATE->parameters[id].paramOffset != 0) {
				for (int i = 1; i < _STATE->parameters[_STATE->parameters[id].paramOffset].max; i++) {
					_STATE->parameters[id + i * _STATE->parameters[id].offsetFact].view = this;
				}
			}

			framed = false;
			orientation = _orientation;


			for (int32_t i = 0; i < numelements; i++) {
				T button(appState, _names[i].data());
				button.id = id;
				button.userdata = !_values.empty() ? (int)_values[i] : i;
				buttons.emplace_back(std::move(button));
			}
		}



		void init() override {
			int32_t titleheight = 0;

			if (height_title != 0)
				titleheight = (framed ? height - 2 * lw : height.load()) * .3333f;


			int32_t starty_elementsview = framed ? lw + titleheight : titleheight;
			int32_t height_elementsview = framed ? height - (int)titleheight - 3 * lw : height -
				(int)titleheight;
			int32_t stopy_elementsview = starty_elementsview + height_elementsview;

			if (titleheight != 0) {
				titleview.width = framed ? width - 2 * lw : width.load();
				titleview.height = framed ? titleheight - 2 * lw : titleheight;
				titleview.startx = startx + (framed ? lw : 0);
				titleview.stopx = titleview.startx + titleview.width;
				titleview.starty = starty + (framed ? lw : 0);
				titleview.stopy = titleview.starty + titleview.height;
			}

			if (orientation == HORIZONTAL) {
				int32_t max = 0;
				for (auto& b : buttons) {
					b.height = height_elementsview;
					b.computeWidth();
					max = b.width > max ? b.width.load() : max;
				}
				for (int32_t i = 0; i < buttons.size(); i++) {
					Button& button = buttons.at(i);
					const int32_t tmp = framed ? (width - 2 * lw) / buttons.size() : width /
						buttons.size();
					button.width = max > tmp ? tmp : max;
					button.startx = startx + DISTANCE(button.width, tmp) * .5f +
						(framed ? lw + i * tmp : i * tmp);
					button.stopx = button.startx + button.width;
					button.starty = starty + starty_elementsview;
					button.stopy = button.starty + button.height;
					if (framed) {
						button.padding = 10.f;
					}
					else {
						button.paddingleft = button.paddingright = 5.f;
					}
					button.computePadding();
					button.init();
				}
			}
			else {
				for (int32_t i = 0; i < buttons.size(); i++) {
					Button& button = buttons.at(i);
					button.width = framed ? (width - 2 * lw) : width.load();
					button.startx = startx + (framed ? lw
						: 0); //(bview->framed ? lw + i * button->width : i * button->width);
					button.stopx = button.startx + button.width;
					button.height = (int)(height_elementsview / (float)buttons.size());
					button.starty = (int)(starty + starty_elementsview +
						i * height_elementsview / (float)buttons.size());
					button.stopy = button.starty + button.height;
					if (framed) {
						button.padding = buttonPadding;
					}
					else {
						button.padding = buttonPadding;
					}
					button.computePadding();
					button.init();
				}
			}
		}

		void callback(const InputEvent& event) override {
			int32_t action = event.action;
			float xpos = event.x;
			float ypos = event.y;
			// static NodeQ *savednode = NULL;
			switch (action) {
			case ACTION_DOWN: {
				auto val = (int)e.getCurrentValue(_STATE);
				for (auto& temp : buttons) {
					if (xpos >= temp.startx && xpos < temp.stopx && ypos >= temp.starty &&
						ypos < temp.stopy && val != temp.userdata) {
						temp.setState(HOT);
						inputState.addPointer(event.pointer_id, xpos, ypos, pmode_t::WINPOINTER, temp.userdata);
						return;
					}
				}
				break;
			}
			case ACTION_MOVE: {
				if (auto pt = inputState.getById(event.pointer_id)) {
					if (pt->distanceTo(event.x, event.y) >
						_STATE->textsize2) {
						for (auto& b : buttons) {
							if (b.userdata == pt->target)
								b.setState(NORMAL);
						}
						inputState.removePointer(event.pointer_id);
					}
				}
				break;
			}

			case ACTION_UP: {
				if (auto pt = inputState.getById(event.pointer_id)) {
					for (auto& b : buttons) {
						b.setState(NORMAL, false);
					}
					e.value = pt->target;
					auto old = e.apply(_STATE, tsl::parameters::FromUi);
					inputState.clear();
					redraw();
					return;
				}
				break;
			}
			default:
				break;
			}

		}

		void render(void* context) override {
			auto* canvas = (SkCanvas*)context;
			flush(canvas);
			for (auto& button : buttons) {
				button.render(context);
			}
			if (height_title != 0) {
				titleview.render(canvas);
			}
			if (framed) {
				SkPaint paint;
				paint.setStrokeWidth(lw);
				paint.setAntiAlias(true);
				paint.setColor(skcol::fg);
				canvas->drawRect(
					SkRect::MakeXYWH(startx + 1, starty + 1, width - 2, height - 2),
					paint);

			}
		}

		void computeSize() override {
			int32_t max = 0;
			height = scalefactor * _STATE->windowHeight * 0.01f;

			for (auto& c : buttons) {
				c.height = height.load();
				c.computeWidth();
				max = c.width > max ? c.width.load() : max;
			}
			width = max * (buttons.size() + 1);
		}
		std::vector<T>& getButtons() { return buttons; }
		const std::vector<T>& getButtons() const { return buttons; }
		float buttonPadding{ 10.f };
	protected:
		void delRecursiveDraw() override {
			for (auto& b : buttons) {
				b.delRecursiveDraw();
			}
			visible_ = false;
			View::delRecursiveDraw();
		};

		void delRecursiveCB() override {
			inputState.clear();
			View::delRecursiveCB();
		};
		
		void addRecursiveDraw() override{
			auto tindex = _STATE->active_track.load();
			e.setup(_STATE, tindex, id);
			View::addRecursiveDraw();
		};

	private:
		bool framed{};
		float height_title{};
		const char* title{};
		int32_t orientation{};
		TitleView titleview;
		std::vector<T> buttons{};
		tsl::graphics::InputSystem::InputState inputState;
		tsl::parameters::Event e{};
	};
}
#endif