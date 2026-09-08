#pragma once
//
// Created by pr on 29.01.22.
//


#include "defines.h"
#include <vector>
#include "tools.h"
#include "Input.h"
#include "logger.h"
#include "VelocityTracker.h"
#include "keyboard.h"
#include "view.h"
#include "skia.h"
#include "app.h"
#include <include/core/SkPath.h>


namespace tsl::graphics {

	class ScrollViewBase {
	protected:
		explicit ScrollViewBase(tsl::AppState* appState) : _appState2(appState) {
			timeLastAction.set0();
		};
		enum {
			UNTOUCHED = 0,
			MOVING = 1,
			INSIDE = 2,
			OUTSIDE = 3,
			SCROLLBAR = 4
		};
		tsl::AppState* _appState2{};
#ifdef PLATFORM_MOBILE
		static constexpr int numScrollers = 4;
		std::atomic<int> currentScroller{};
		Scroller scroller[numScrollers] = { {_appState2->mPpi, false},
										   {_appState2->mPpi, false},
										   {_appState2->mPpi, false},
										   {_appState2->mPpi, false} };
		VelocityTracker velocityTracker;
#endif // !PLATFORM_MOBILE


		int pointerid{ -1 };
		std::atomic<int> mode{ UNTOUCHED };
		float totalmoved{};
		std::atomic<float> offset{};
		float lastXpos{}, lastYpos{};
		tsl::AtomicTimer timeLastAction{};

		int boxoffsetx{}, boxoffsety{}, boxheight{}, boxwidth{}, itemheight{}, maxoffset{};
		int drawstartx{}, drawstarty{}, drawstopx{}, drawstopy{}, drawheight{}, drawwidth{};
		float glowStartX{}, glowWidth{};
		
		float _titleX{}, _titleY{};

		std::atomic<int> active{};
		// Overscroll state
		std::atomic<float> overscrollTop = 0.0f;
		std::atomic<float> overscrollBottom = 0.0f;
		const float maxOverscroll = 80.0f;

		// FIX: Keep a running animation timer incremented per frame, 
		// or use global steady_clock to avoid resetting with user actions.
		float animationTime = 0.0f;



		void drawOverscrollGlow(SkCanvas* canvas, bool isTop, float amount);

		void updateOverscroll(SkCanvas* c);
	};

	template<typename T, typename T2>
	class RecyclerView : public View, protected ScrollViewBase
	{
	public:
		explicit RecyclerView(tsl::AppState* appState, const std::vector<T2>& vals = {}, const char* title = "", View* par = nullptr) : View(appState, WRAP, 0, CENTER_ALIGN, 0), ScrollViewBase(_appState), _itemView(appState) {
			_itemView._values = vals;
			_title = title;
			_itemView.parent = this;
			parent = par;
		}

		void setValues(const std::vector<T2>& vals) {
			_itemView._values = vals;
		}


		void setTitle(const std::string& title) {
			_title = title;
		}

		void computeSize() override {
			startx = 0;
			starty = 0;
			width = stopx = _STATE->windowWidth;
			height = stopy = _STATE->windowHeight;
			const float textsize = _STATE->textsize2;

			_itemView.computeHeight();
			itemheight = _itemView.height;


			if (!_title.empty()) {
				SkFont font(_STATE->font_normal);
				font.setSize(textsize * .9f);
				SkRect bounds{};
				font.measureText(_title.c_str(), _title.size(), SkTextEncoding::kUTF8, &bounds);
				boxwidth = bounds.width();
			}
			boxwidth = parent != nullptr && parent->width > boxwidth ? parent->width.load() : boxwidth;

			for (int i = 0; i < _itemView._values.size(); i++) {
				_itemView.computeWidth(i);
				boxwidth = _itemView.width > boxwidth ? _itemView.width.load() : boxwidth;
			}

			_itemView.textOffset = textsize * .5f;
			boxoffsetx = _itemView.textOffset + lw2;
			boxoffsety = boxoffsetx + (!_title.empty() ? itemheight : 0);

			const float maxw = parent == nullptr ? width.load() * .9f : parent->width.load();

			if (boxwidth + 2 * boxoffsetx > maxw)
				boxwidth = maxw - 2 * boxoffsetx;

			drawwidth = boxwidth + 2 * boxoffsetx;

			drawstartx = parent == nullptr ? startx.load() + (width.load() - drawwidth) * .5f : parent->startx.load();
			if (drawstartx < startx)
				drawstartx = startx.load();

			drawstopx = drawstartx + drawwidth;
			if (drawstopx > width) {
				drawstopx = width;
				drawstartx = drawstopx - drawwidth;
			}

			_itemView.startx = lw2;
			_itemView.width = drawwidth - lw;
			_itemView.stopx = _itemView.startx + _itemView.width;

			boxheight = itemheight * _itemView._values.size();

			const float maxh = parent == nullptr ? height * .9f : DISTANCE(parent->stopy,
				_STATE->windowHeight -
				_STATE->textsize2);

			if (boxheight + boxoffsety + boxoffsetx > maxh) {
				maxoffset = -DISTANCE(boxheight + boxoffsety + boxoffsetx, maxh);
				drawheight = maxh;
				boxheight = maxh - boxoffsety - boxoffsetx;
			}
			else {
				maxoffset = 0;
				drawheight = boxheight + boxoffsety + boxoffsetx;
			}

			drawstarty = parent == nullptr ? starty + (height - drawheight) * .5f : parent->stopy - parent->height * .1f;
			drawstopy = drawstarty + drawheight;
			if (drawstopy > height) {
				drawstopy = height;
				drawstarty = drawstopy - drawheight;
			}

			_itemView.starty = 0;
			_itemView.stopy = _itemView.starty + _itemView.height;
			_itemView.init();

			if (!_title.empty()) {
				SkFont font(_STATE->font_normal);
				font.setSize(_STATE->textsize2 * .9f);
				centerText(font, boxwidth, itemheight, _title.c_str(), _titleX, _titleY);
			}
			glowStartX = 0;
			glowWidth = drawwidth;
		}

		void render(void* ctx) override {
			auto canvas = _STATE->graphics.getCanvas(windowindex, drawstartx,
				drawstarty, drawwidth, drawheight);
			if (canvas == nullptr)
				return;
			SkPaint paint;
			canvas->clear(skcol::wbg);
			paint.setColor(skcol::fg);
			paint.setStrokeWidth(lw);
			paint.setStyle(SkPaint::kFill_Style);
			paint.setAntiAlias(true);
			if (!_title.empty()) {
				auto& font = _STATE->font_normal;
				font.setSize(_STATE->textsize2 * .9f);
				font.setEmbolden(true);
				canvas->drawString(_title.c_str(), _STATE->textsize2 * .5f,
					boxoffsetx + _titleY, font, paint);
				font.setEmbolden(false);
			}

			canvas->save();
			SkPath path;
			path.addRect(SkRect::MakeXYWH(lw, boxoffsety, width - 2 * lw, boxheight));
			canvas->clipPath(path);

			// std::lock_guard lk(mutex);


#ifdef PLATFORM_MOBILE
			auto& s = scroller[currentScroller.load()];
			const auto elapsed = !s.isFinished() ? timeLastAction.elapsedReplace()
				: timeLastAction.elapsed();
			if (s.computeScrollOffset()) {
				;
				offset = s.getCurrY();
			}
#else
			const auto elapsed = timeLastAction.elapsed();
#endif
			const float off = offset;

			float sx = boxoffsetx;
			float sy = boxoffsety + off;

			for (int i = 0; i < _itemView._values.size(); i++) {
				if (sy >= boxoffsety - itemheight && sy - itemheight <= boxoffsety + boxheight) {
					canvas->save();
					canvas->translate(0, sy);
					_itemView.render(canvas, i);
					canvas->restore();
				}
				sy += itemheight;
			}

			const float maxoff = std::abs(maxoffset);

			if (maxoff) {
				// Update overscroll animation
				updateOverscroll(canvas);
			}

			if (maxoff > 0 && elapsed < 2.0) {
				float alpha =
					elapsed > 1.0 ? 255.f - 255.f * (elapsed - 1.) : 255.f;
				paint.setColor(
					SkColorSetA(mode == 1000 ? skcol::blue_transparent : skcol::fg, alpha));
				const float length = boxheight / (1.f + maxoff / boxheight);
				const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
				canvas->drawRect(
					SkRect::MakeXYWH(drawwidth - 2 * lw, boxoffsety + pos, 2 * lw, length),
					paint);
			}
			if (elapsed > 2.0 && mode != MOVING)perm = false;
			else perm = true;
			canvas->restore();
			paint.setStyle(SkPaint::kStroke_Style);
			paint.setColor(skcol::border);
			paint.setStrokeWidth(1.0f);
			canvas->drawRect(SkRect::MakeXYWH(0.5, 0.5, drawwidth - 1.0, drawheight - 1.0), paint);
		}

		void callback(const InputEvent& event) override {
			int action = event.action;
			int _pointerid = event.pointer_id;
			float xpos = event.x - drawstartx;
			float ypos = event.y - drawstarty;
#ifdef PLATFORM_MOBILE
			velocityTracker.addMovement(event);
#endif
			switch (action) {
			case ACTION_DOWN: {
				if (xpos >= boxoffsetx && xpos < boxoffsetx + boxwidth && ypos >= boxoffsety &&
					ypos < boxoffsety + boxheight) {
					int pos = (int)((ypos - boxoffsety - offset) / itemheight);
					if (pos >= _itemView._values.size())
						pos = _itemView._values.size() - 1;
					mode = INSIDE;
					timeLastAction.reset();

					_itemView.cb(event, pos);
				}
				else {
					mode = OUTSIDE;
					_itemView.cb(event, -1);
				}
				totalmoved = 0;
				lastXpos = xpos;
				lastYpos = ypos;
				pointerid = _pointerid;
				// Stop overscroll animation
				overscrollTop = 0.0f;
				overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
				scroller[currentScroller.load()].forceFinished(true);
#endif
				redraw();
				break;
			}
			case ACTION_UP: {
				if (pointerid == _pointerid) {
					if (mode == OUTSIDE) {
						deldraw();
						delCB();
						if (waitToken.ticket) {
							_STATE->waitNotify.complete(waitToken);
						}
						return;
					}
					else if (mode == INSIDE) {
						auto res = _itemView.cb(event, -1);
						if (res == 1) {
							deldraw();
							delCB();
							if (parent != nullptr)
								parent->redraw();
							if (waitToken.ticket) {
								_STATE->waitNotify.complete(waitToken);
							}
						}
						else {
							timeLastAction.reset();
							redraw();
						}
						pointerid = -1;
						mode = UNTOUCHED;
						
						return;
					}
					if (maxoffset < 0) {
						float diffy = lastYpos - ypos;
						auto off = offset.load() - diffy;
						// Handle overscroll
						if (off < maxoffset) {
							overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll);
							off = maxoffset;
						}
						else if (off > 0) {
							overscrollTop = std::min(off * 0.3f, maxOverscroll);
							off = 0;
						}
						offset.store(off);
#ifdef PLATFORM_MOBILE
						float x, y;
						if (velocityTracker.getVelocity(pointerid, &x, &y) &&
							(_STATE->mMinimumFlingVelocity <= 0 ||
								std::abs(y) >= _STATE->mMinimumFlingVelocity)) {
							if (_STATE->mMaximumFlingVelocity <= 0 ||
								std::abs(y) > _STATE->mMaximumFlingVelocity)
								y = ISNEG(y) ? -_STATE->mMaximumFlingVelocity
								: _STATE->mMaximumFlingVelocity;
							int cur = currentScroller.load();
							if (++cur >= numScrollers)
								cur = 0;
							scroller[cur].fling(0, off, 0, y, 0, 0, maxoffset, 0);
							currentScroller.store(cur);
						}
#endif
					}
					pointerid = -1;
					mode = UNTOUCHED;
					timeLastAction.reset();
					redraw();
				}
				break;
			}
			case ACTION_MOVE: {
				if (pointerid == _pointerid) {
					if (mode == MOVING) {
						float diffy = lastYpos - ypos;
						float off = offset;
						off -= diffy;
						// Apply dampening overscroll accumulator
						if (off < maxoffset) {
							float overscroll = maxoffset - off;
							// Addively pile onto the value instead of setting a harsh minimum cap
							overscrollBottom = std::max(overscrollBottom.load(), std::min(overscroll * 0.5f, maxOverscroll));
							off = maxoffset;
						}
						else if (off > 0) {
							overscrollTop = std::max(overscrollTop.load(), std::min(off * 0.5f, maxOverscroll));
							off = 0;
						}
						
						offset = off;
						lastXpos = xpos;
						lastYpos = ypos;
						timeLastAction.reset();
					}
					else if (mode == INSIDE || mode == OUTSIDE) {
						// lastXpos/lastYpos still hold the down point here, so this is
						// displacement from the down point; accumulating (+=) would add the
						// full distance on every move event and break taps on devices with
						// high touch report rates
						totalmoved = spacing(xpos, lastXpos, ypos, lastYpos);
						if (totalmoved > _STATE->touchSlop()) {
							if (mode == INSIDE) {
								lastXpos = xpos;
								lastYpos = ypos;
								_itemView.cb(event, -1);
								if (maxoffset < 0)
									mode = MOVING;
							}
							else {
								mode = UNTOUCHED;
								pointerid = -1;
							}
							timeLastAction.reset();
							redraw();
						}
					}
				}
				break;
			}
			case ACTION_MOUSE_WHEEL: {
				if (maxoffset < 0 && mode == UNTOUCHED) {
					float off = offset;
					off += event.pointer_id * itemheight;
					// Handle overscroll for mouse wheel
					if (off < maxoffset) {
						overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll);
						off = maxoffset;
					}
					else if (off > 0) {
						overscrollTop = std::min(off * 0.3f, maxOverscroll);
						off = 0;
					}
					offset = off;
					timeLastAction.reset();
					redraw();
				}
				break;
			}
			case ACTION_KEY_UP: {
				if (event.pointer_id == VKEY_ESCAPE) {
					delCB();
					deldraw();
					if (waitToken.ticket) {
						_STATE->waitNotify.complete(waitToken);
					}
				}

				break;
			}


			default:
				break;
			}
		}

		void setActive(int index) {
			active.store(index);
		}

		int getActive() {
			return active.load();
		}

		std::function<bool(int)> isActiveFunc = nullptr;

		void addRecursiveDraw() override {
			timeLastAction.reset();
			View::addRecursiveDraw();
		}

		void delRecursiveDraw() override {
			_STATE->graphics.deleteWindow(windowindex);
			View::delRecursiveDraw();
		};

		void addRecursiveCB() override {
			hasFocus.store(true);
			View::addRecursiveCB();
		}

		void delRecursiveCB()override {
			hasFocus.store(false);
			pointerid = -1;
			mode = UNTOUCHED;
			totalmoved = 0;
			overscrollTop = 0.0f;
			overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
			velocityTracker.clear();
#endif // !PLATFORM_DESKTOP
			View::delRecursiveCB();
		};
		void acquire_slot() {
			waitToken = _STATE->waitNotify.begin_wait();
		}
		void reset_slot() {
			waitToken = {};
		}
		tsl::ThreadSignal::WaitToken token() const { return waitToken; }
	private:
		int windowindex{ -1 };
		tsl::ThreadSignal::WaitToken waitToken{};
		T _itemView;
		std::string _title;
		std::atomic<int> active{};
	};

	template<typename T>
	class TextViewBase : public View {
	public:
		explicit TextViewBase<T>(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0) {};
		virtual void computeWidth(int index) = 0;

		void computeHeight() override {
			height = _STATE->textsize1;//2 * DATA::_STATE->textsize2;
		}
		virtual void render2(SkCanvas* c, int index) {
			if (index == indexhot) {
				auto elapsed = timer.elapsed();
				SkPaint paint;
				paint.setColor(SkColorSetA(skcol::blue_transparent,
					elapsed < .5 ? 200.f * elapsed : 100.f));
				c->drawRect(
					SkRect::MakeXYWH(startx, 0, width, height),
					paint);
			}
		};
		virtual void render(SkCanvas* c, int index) {
			if (index == indexhot) {
				SkPaint paint;
				paint.setColor(skcol::blue_transparent);
				c->drawRect(
					SkRect::MakeXYWH(startx, 0, width, height),
					paint);
			}
		};

		virtual int cb(const tsl::graphics::InputEvent& event, int index) {
			switch (event.action) {
			case ACTION_DOWN:
				timer.reset();
				indexhot = index;
				break;
			case ACTION_UP:
				indexhot = -1;
				break;
			case ACTION_MOVE:
				timer.reset();
				indexhot = index;
				break;
			}
			return 1;
		};

		std::vector<T> _values;
		int textOffset{};
	protected:
		tsl::AtomicTimer timer{};
		std::atomic<int> indexhot{ -1 };
	};

}
