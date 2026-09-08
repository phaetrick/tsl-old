#pragma once
#include "defines.h"
#include "FloatingView.h"
#include "RecyclerView.h"


namespace tsl::graphics {
	
	template<typename T, typename T2>
	class FloatingScrollView : public FloatingView, protected ScrollViewBase
	{
	public:
		explicit FloatingScrollView(tsl::AppState* appState, const std::vector<T2>& vals = {}, const char* title = "")
			: FloatingView(appState, title, true), ScrollViewBase(_appState), _itemView(appState) {
			_itemView._values = vals;
			_itemView.parent = this;
		}

		void setValues(const std::vector<T2>& vals) {
			_itemView._values = vals;
		}

		void setTitle(const char *title) {
			FloatingView::setTitle(title);
		}

		
		void setActive(int index) {
			active.store(index);
		}

		int getActive() {
			return active.load();
		}

		
		void acquire_slot() {
			waitToken = _STATE->waitNotify.begin_wait();
		}

		void reset_slot() {
			waitToken = {};
		}
		tsl::ThreadSignal::WaitToken token() const { return waitToken; }

	protected:
		void computeContent(int maxWidth, int maxHeight) override {
			auto mH = maxHeight;
			_itemView.computeHeight();
			itemheight = _itemView.height;

			for (int i = 0; i < _itemView._values.size(); i++) {
				_itemView.computeWidth(i);
				boxwidth = std::max(boxwidth, _itemView.width.load());
			}

			boxwidth = std::min(boxwidth, maxWidth);

			_itemView.textOffset = 0;
			boxoffsetx = -borderSize;
			boxoffsety = 0;

			drawwidth = boxwidth + 2 * borderSize;
			drawstartx = -borderSize;
			drawstopx = drawstartx + drawwidth;

			_itemView.startx = -borderSize;
			_itemView.width = boxwidth + 2 * borderSize;
			_itemView.stopx = _itemView.startx + _itemView.width;

			boxheight = itemheight * _itemView._values.size();

			if (boxheight > mH) {
				maxoffset = -DISTANCE(boxheight, mH);
				drawheight = mH;
				boxheight = mH;
			}
			else {
				maxoffset = 0;
				drawheight = boxheight;
			}

			drawstarty = 0;
			drawstopy = drawstarty + drawheight;

			_itemView.starty = 0;
			_itemView.stopy = _itemView.starty + _itemView.height;
			_itemView.init();

			contentWidth = boxwidth;
			contentHeight = drawheight;

			glowStartX = - borderSize;
			glowWidth = drawwidth;
		}

		void renderContent(void* ctx) override {
			auto canvas = static_cast<SkCanvas*>(ctx);
			SkPaint paint;
			canvas->clear(skcol::wbg);
			paint.setColor(skcol::fg);
			paint.setStrokeWidth(lw);
			paint.setStyle(SkPaint::kFill_Style);
			paint.setAntiAlias(true);

#ifdef PLATFORM_MOBILE
			auto& s = scroller[currentScroller.load()];
			const auto elapsed = !s.isFinished() ? timeLastAction.elapsedReplace()
				: timeLastAction.elapsed();
			if (s.computeScrollOffset()) {
				offset = s.getCurrY();
			}
#else
			const auto elapsed = timeLastAction.elapsed();
#endif

			

			const float off = offset;
			float sx = boxoffsetx;
			float sy = boxoffsety + off;

			// Draw items
			for (int i = 0; i < _itemView._values.size(); i++) {
				if (sy >= boxoffsety - itemheight && sy - itemheight <= boxoffsety + boxheight) {
					canvas->save();
					canvas->translate(0, sy);
					_itemView.render(canvas, i);
					canvas->restore();
				}
				sy += itemheight;
			}

			

			// Draw scrollbar
			const float maxoff = std::abs(maxoffset);
			
			if (maxoff){
				// Update overscroll animation
				updateOverscroll(canvas);
			}
			
			if (maxoff > 0 && elapsed < 2.0) {
				float alpha = elapsed > 1.0 ? 255.f - 255.f * (elapsed - 1.) : 255.f;
				paint.setColor(SkColorSetA(mode == 1000 ? skcol::blue_transparent : skcol::fg, alpha));
				const float length = boxheight / (1.f + maxoff / boxheight);
				const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
				canvas->drawRect(
					SkRect::MakeXYWH(boxwidth + borderSize - 2 * lw, boxoffsety + pos, 2 * lw, length),
					paint);
			}
			if (elapsed > 2.0 && mode != MOVING)perm = false;
			else perm = true;
		}

		int cb(float xpos, float ypos, int action, int pid) override {
			InputEvent event{ this, action, pid, xpos, ypos };
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
					_itemView.cb(event, pos);
				}
				else {
					mode = OUTSIDE;
					_itemView.cb(event, -1);
				}
				totalmoved = 0;
				lastXpos = xpos;
				lastYpos = ypos;
				pointerid = pid;

				// Stop overscroll animation
				overscrollTop = 0.0f;
				overscrollBottom = 0.0f;

#ifdef PLATFORM_MOBILE
				scroller[currentScroller.load()].forceFinished(true);
#endif
				timeLastAction.reset();
				redraw();
				break;
			}
			case ACTION_UP: {
				if (pointerid == pid) {
					if (mode == OUTSIDE) {
						if (waitToken.ticket) {
							_STATE->waitNotify.complete(waitToken);
						}
						timeLastAction.reset();
						redraw();
						return 0;
					}
					else if (mode == INSIDE) {
						auto res = _itemView.cb(event, -1);
						if (res == 1) {
							if (waitToken.ticket) {
								_STATE->waitNotify.complete(waitToken);
							}
						}
						pointerid = -1;
						mode = UNTOUCHED;
						timeLastAction.reset();
						redraw();

						return 0;
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
				}
				timeLastAction.reset();
				redraw();

				break;
			}
			case ACTION_MOVE: {
				if (pointerid == pid) {
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
					if (waitToken.ticket) {
						_STATE->waitNotify.complete(waitToken);
					}
				}
				break;
			}
			default:
				break;
			}
			return 0;
		}

		void delRecursiveCB() override {
			hasFocus.store(false);
			pointerid = -1;
			mode = UNTOUCHED;
			totalmoved = 0;
			overscrollTop = 0.0f;
			overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
			velocityTracker.clear();
#endif
			FloatingView::delRecursiveCB();
		}
		void addRecursiveDraw() override {
			timeLastAction.reset();
			FloatingView::addRecursiveDraw();
		}

	private:
		tsl::ThreadSignal::WaitToken waitToken{};
		T _itemView;
		std::atomic<int> active{};

		
};
}