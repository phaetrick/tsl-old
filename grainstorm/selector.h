#pragma once
//
// Created by pr on 13.11.17.
//

#ifndef GRAINSTORM_Selector_H
#define GRAINSTORM_Selector_H

#include <atomic>
#include <vector>
#include "types.h"
#include "textview.h"
#include "Input.h"
#include "skia.h"
#include "ButtonBase.h"
#include "RecyclerView.h"
#include <IconsMaterialDesignReduced.h>

#include <SkFont.h>
#include <SkPath.h>
#include <map>
#include <span>
#include <string_view>
#include "view.h"

namespace tsl::graphics {
	class Selector;

	class WaveformChooser : public TitleView {
	public:
		WaveformChooser(tsl::AppState* appState, const char* text, int32_t alignment, Layout* _parent = nullptr) : TitleView(appState,
			text,
			alignment,
			_parent) {
		};

		void render(void* ctx) override;
	};




	class Selector : public View {
	public:
		Selector(tsl::AppState* appState) : View(appState) {};

		class SelectorTextView : public TextViewBase<std::string> {
		public:
			SelectorTextView(tsl::AppState* appState) : TextViewBase<std::string>(appState) {};

			void computeWidth(int32_t index) override {
				const auto& s = _values.at(index);
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2 * .9);
				SkRect bounds{};
				font.measureText(s.c_str(), s.size(),
					SkTextEncoding::kUTF8, &bounds);
				width = bounds.width();
				if (index == 0) {
					float x;
					centerText(font, this, s.c_str(), x, y);
				}
			}

			virtual int32_t cb(const InputEvent& event, int index) override {
				if (event.action == ACTION_UP) {
					auto tmp = indexhot.load();
					if (tmp >= 0) {
						auto s = (Selector*)parent->parent;
						s->cb(s->values.empty() ? tmp : s->values[tmp]);
					}
					else return 0;
				}
				return TextViewBase::cb(event, index);

			}

			virtual void render(SkCanvas* c, int32_t index) override {
				flush(c);
				TextViewBase::render(c, index);
				auto& font = _STATE->font_normal;
				SkPaint paint;
				const bool isActive =
					((RecyclerView<SelectorTextView, std::string> *) parent)->isActiveFunc !=
					nullptr
					? ((RecyclerView<SelectorTextView, std::string> *) parent)->isActiveFunc(
						index) : false;
				const bool ml = (_STATE->parameters[id].flags & Param::MidiParam) && _STATE->midilearning.load();
				if(ml)
				paint.setColor(skcol::midilearning);
				else
				paint.setColor(isActive ? skcol::active : skcol::fg);
				const auto& str = _values.at(index);
				font.setSize(_STATE->textsize2 * .9f);
				c->drawString(str.c_str(), _STATE->textsize2 * .5f, y, font, paint);
			};
		

		private:
			float y{};
		};

	
		enum {
			Selector_LEFT = 0,
			Selector_RIGHT = 1,
			Selector_MIDDLE = 2
		};


		Selector(tsl::AppState* appState,
			float scalefactor,
			int32_t aspect_ratio,
			int32_t alignment,
			int numelements_,
			std::span<const std::string_view> names,
			std::span<const float> values, 
			uint16_t id,
			int64_t offset = 0,
			int32_t offsetmulti = 1,
			int (*callback)(Selector*, int) = nullptr,
			const char* title = nullptr);

		
		virtual void render(void* ctx) override = 0;

		virtual void callback(const InputEvent& e) override = 0;

		virtual void init() override = 0;

		void setActive(const char* name, bool docallback);

		void setActiveByIndex(int32_t index, bool docallback);


		std::unique_ptr<RecyclerView<SelectorTextView, std::string>> rv;

		std::vector<std::string> names;   // owning, null-terminated
		std::vector<float>       values;  // owning copy
		std::string titletext;
		
		int32_t numelements{};

		int32_t cb(int);

		tsl::graphics::InputSystem::InputState inputstate;

	protected:
		virtual void delRecursiveDraw() override {
			rv->delRecursiveDraw();
			View::delRecursiveDraw();
		};

		void delRecursiveCB() override {
			rv->delRecursiveCB();
			inputstate.clear();
			View::delRecursiveCB();
		};

		void addRecursiveDraw() override {
			e.setup(_STATE, _STATE->active_track.load(), id);
			View::addRecursiveDraw();
		};
		tsl::parameters::Event e{};

	};

	template<typename T>
	class Selector1 : public Selector {
	public:
		Selector1(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio,
			int32_t _alignment, int _numelements, std::span<const std::string_view> _names,
			std::span<const float> _values,
			uint16_t _id, int64_t _offset = 0,
			int32_t _offsetmulti = 1, int (*_callback)(Selector*, int) = nullptr)
			: Selector(appState, _scalefactor, _aspect_ratio, _alignment, _numelements, _names,
				_values,
				_id,
				_offset,
				_offsetmulti, _callback), left{ appState,
 reinterpret_cast<const char*>(ICON_MD_NAVIGATE_BEFORE)
				}, right{ appState,
	reinterpret_cast<const char*>(ICON_MD_NAVIGATE_NEXT) },
			textview{ appState, "SelTitle", CENTER_ALIGN } {
			textview.id = id;
		}





		template <size_t N, size_t M>
		Selector1(tsl::AppState* appState,
			float _scalefactor,
			int32_t _aspect_ratio,
			int32_t _alignment,
			int _numelements,
			const std::array<std::string_view, N>& names,
			const std::array<float, M>& values,
			uint16_t _id,
			int64_t _offset = 0,
			int32_t _offsetmulti = 1,
			int (*_callback)(Selector*, int) = nullptr)
			: Selector(appState,
				_scalefactor,
				_aspect_ratio,
				_alignment,
				numelements,
				std::span{ names },
				std::span{ values },
				_id,
				_offset,
				_offsetmulti,
				_callback), left{ appState,
 reinterpret_cast<const char*>(ICON_MD_NAVIGATE_BEFORE)
				}, right{ appState,
	reinterpret_cast<const char*>(ICON_MD_NAVIGATE_NEXT) },
			textview{ appState, "SelTitle", CENTER_ALIGN } {
			static_assert(N > 0, "Selector2: empty names array");
			textview.id = id;
		}


		void render(void* context) override {
			flush((SkCanvas*)context);
			left.render(context);
			right.render(context);
			auto index = static_cast<int>(e.getCurrentValue(_STATE));

			if (!values.empty())
				index = findIndexFloat(values.data(), index);
			if(index < names.size())
			textview.setText(names[(int)index].c_str());
			textview.render(context);
			//renderpopup(view, nullptr);
		}

		void callback(const InputEvent& event) override {
			int32_t action = event.action;

			float xpos = event.x;
			float ypos = event.y;


			switch (action) {
			case ACTION_DOWN:
				if (!middleclick) {
					if (xpos < startx + width * .5) {
						inputstate.addPointer(
							{ event.pointer_id, xpos, ypos, tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_LEFT });
						left.setState(HOT);
					}
					else if (xpos >= startx + width * .5) {
						inputstate.addPointer(
							{ event.pointer_id, xpos, ypos, tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_RIGHT });
						right.setState(HOT);
					}
				}
				else {
					if (xpos < startx + width * .25) {
						inputstate.addPointer(
							{ event.pointer_id, xpos, ypos, tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_LEFT });
						left.setState(HOT);
					}
					else if (xpos >= stopx - width * .25) {
						inputstate.addPointer(
							{ event.pointer_id, xpos, ypos, tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_RIGHT });
						right.setState(HOT);
					}
					else {
						inputstate.addPointer(
							{ event.pointer_id, xpos, ypos, tsl::graphics::InputSystem::WinState::WINPOINTER, Selector_MIDDLE });
						textview.setState(HOT);
					}
				}
#if defined PLUGIN_MODE
				_STATE->StartParamChange(e);
#endif
				break;
			case ACTION_MOVE: {
				auto pt = inputstate.getById(event.pointer_id);
				if (pt && pt->distanceTo(xpos, ypos) > _STATE->textsize2)
				{
					if (pt->target == Selector_LEFT)
						left.setState(NORMAL);
					else if (pt->target == Selector_RIGHT)
						right.setState(NORMAL);
					else
						textview.setState(NORMAL);
					inputstate.removePointer(event.pointer_id);
#if defined PLUGIN_MODE
					_STATE->EndParamChange(e);
#endif

				}
				break;
			}

			case ACTION_UP: {
				if (auto pt = inputstate.getById(event.pointer_id)) {
					auto active = static_cast<int>(e.getCurrentValue(_STATE));
					if (!values.empty())
						active = findIndexFloat(values.data(), active);

					if (pt->target == Selector_LEFT) {
						--active;
						if (active < 0)
							active = numelements - 1;
						rv->setActive(active);
						if (active < names.size())
						    textview.setText(names[active].c_str(), true);
						cb(!values.empty() ? values[active] : active);
						left.setState(NORMAL);
#if defined PLUGIN_MODE
						_STATE->EndParamChange(e);
#endif

					}
					else if (pt->target == Selector_RIGHT) {
						++active;
						if (active >= numelements)
							active = 0;
						rv->setActive(active);
						if(active < names.size())
						textview.setText(names[(int)active].c_str(), true);
						cb(!values.empty() ? values[(int)active] : active);
						right.setState(NORMAL);
#if defined PLUGIN_MODE
						_STATE->EndParamChange(e);
#endif
					}
					else {
						rv->addDraw();
						rv->addCB();
						textview.setState(NORMAL);
						left.setState(NORMAL);
						right.setState(NORMAL);
						inputstate.clear();
#if defined PLUGIN_MODE
						_STATE->EndParamChange(e);
#endif
						return;
					}
					inputstate.removePointer(event.pointer_id);

				}
				break;
			}
			case ACTION_MOUSE_WHEEL: {
				if (inputstate.empty()) {
					auto active = static_cast<int>(e.getCurrentValue(_STATE));
					if (!values.empty())
						active = findIndexFloat(values.data(), active);
					active += event.pointer_id;
					if (active < 0)
						active = numelements - 1;
					else if (active >= numelements)
						active = 0;
					rv->setActive(active);
					if (active < names.size())
					textview.setText(names[active].c_str(), true);
					cb(!values.empty() ? values[(int)active] : active);
				}
				break;
			}
			default:
				break;
			}
		};

		void init()  override {
			std::vector<std::string> tmp;
			for (int32_t i = 0; i < numelements; i++)
				tmp.emplace_back(names[i]);
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9f);
			float w = measureWidth(font, tmp);
			float buttonwidth = height;

			w += buttonwidth;
			if (w + 2 * buttonwidth > width)
				w = width - 2 * buttonwidth;
			if (innerAlignment == CENTER_ALIGN)
				startx = startx + (width - (w + 2 * buttonwidth)) * .5f;
			else if (innerAlignment == END_ALIGN) {
				startx = stopx - (w + 2 * buttonwidth);
			}
			width = (w + 2 * buttonwidth);
			stopx = startx + width;
			left.width = buttonwidth;
			left.height = height.load();
			left.startx = startx.load();
			left.stopx = left.startx + left.width;
			left.starty = starty.load();
			left.stopy = left.starty + left.height;
			left.setState(NORMAL, false);

			right.width = buttonwidth;
			right.height = height.load();
			right.startx = startx + width - buttonwidth;
			right.stopx = right.startx + right.width;
			right.starty = starty.load();
			right.stopy = right.starty + right.height;
			right.setState(NORMAL, false);


			textview.width = width - 2 * buttonwidth;
			textview.height = _STATE->textsize2;
			textview.startx = startx + buttonwidth;
			textview.stopx = startx - buttonwidth;
			textview.starty = starty + (height - _STATE->textsize2) * .5f;
			textview.stopy = textview.starty + textview.height;

			rv->computeSize();

		}

		void computeSize()  override {
			int32_t longest = 0;
			int32_t len = 0;
			for (int32_t i = 0; i < numelements; ++i) {
				if (names[i].size() > len) {
					len = names[i].size();
					longest = i;
				}
			}
			float h = height = scalefactor * _STATE->windowHeight * .01;

			if (((Layout*)parent)->orientation == HORIZONTAL) {

				SkRect bounds;
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2);
				font.measureText(names[longest].c_str(), names[longest].size(), SkTextEncoding::kUTF8,
					&bounds);
				float w = bounds.width();
				float _padding = (padding != 0 ? 100.f - 2 * padding : 100.f - paddingleft -
					paddingright) * .01f;
				w += 4.0 * height * _padding;
				if (w > parent->width) w = parent->width;
				width = w;
				startx = parent->startx + (parent->width - w) * 0.5;
				stopx = startx + w;
			}
			else if (((Layout*)parent)->orientation == VERTICAL) {
				starty = parent->starty + (parent->height - h) / 2;
				stopy = starty + h;
				SkRect bounds;
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2);
				font.measureText(names[longest].c_str(), names[longest].size(), SkTextEncoding::kUTF8,
					&bounds);
				float w = bounds.width();
				float _padding =
					(padding != 0 ? 100.f - 2 * padding : 100.f - paddingleft -
						paddingright) *
					.01f;
				w += 3.0f * height;// * _padding;
				width = w;

			}
			// view->height = view->scalefactor * view->p->height / 100.f;

		}

		bool middleclick = false;
	protected:
		void delRecursiveDraw()  override {
			auto& q = _STATE->queue_draw;
			q.del(&left);
			left.state.store(NORMAL);
			q.del(&right);
			right.state.store(NORMAL);
			q.del(&textview);
			textview.setState(NORMAL, false);
			Selector::delRecursiveDraw();
		};
	private:
		IconButton left, right;
		T textview;
	};

	//View *selector2_create(void *p, const char *name, const char *title, int32_t prio, double scalefactor, uint8_t aspect_ratio,
	//                      int32_t alignment, const char **text1, const char **text2, const char **text3, const char **text4, int type, int (*callback)(Selector2 *s, const char *fieldname));

	class Selector2 : public Selector {
	public:
		Selector2(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio,
			int32_t _alignment, int _numelements, std::span<const std::string_view> names,
			std::span<const float> values,
			uint16_t _id, int64_t _offset = 0,
			int32_t _offsetmulti = 1, const char* _title = nullptr,
			int32_t(*_callback)(Selector*, int) = nullptr);

		
		void render(void* ctx) override;

		void callback(const InputEvent& e) override;

		void init() override;

		bool disabled{};
	protected:
		void delRecursiveDraw()  override {
			hot.store(false);
			Selector::delRecursiveDraw();
		}

	private:
		TitleView value;
		DropDownView title;
		std::atomic_bool hot{ false };
	};
}


#endif //GRAINSTORM_Selector_H


