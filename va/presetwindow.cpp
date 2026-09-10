#include "view.h"
#include "RecyclerView.h"
#include "ButtonBase.h"
#include "keyboard.h"
#include <filesystem>
#include <IconsMaterialDesignReduced.h>
#include "app.h"
#include "tools/PlatformPaths.h"
// Needed by the template body below, not just by the explicit instantiation at
// the bottom: Preset::removePreset is a non-dependent name and is looked up
// where the template is defined.
#include "preset.h"

namespace tsl::graphics {
	template<typename T>
	class DeleteOverwritetItem : public TextViewBase<T> {
	private:
		float x{}, y{};
		float xT{}, yT{}, iconWidth{};
		std::vector<std::string> displayStrings;
		IconButton button{ nullptr, ICON_MD_DELETE };
		std::atomic<bool> buttonHot{};

	public:
		enum Action : uint8_t {
			None = 0,
			Overwrite = 1,
			Delete = 2,
			New = 3,
			Canceled = 4,
		};
		struct CallbackResult {
			Action action{ Canceled };
			int index{ -1 };
			T item{};
			std::string nameFromUi{};
		};
		explicit DeleteOverwritetItem(tsl::AppState* appState) : TextViewBase<T>(appState) {};

		void render(SkCanvas* c, int32_t index) override {
			const auto& s = displayStrings.at(index);
			this->flush(c);
			const auto bh = buttonHot.load() && this->indexhot.load() == index;
			if (!bh) {
				TextViewBase<T>::render(c, index);
				button.state.store(NORMAL);
			}
			else button.state.store(HOT);
			SkFont font(this->_STATE->font_normal);
			SkPaint paint;
			paint.setColor(skcol::fg);
			font.setSize(this->_STATE->textsize2 * .9f);
			c->drawString(s.c_str(), this->textOffset, y, font, paint);
			button.render(c);
		}

		void computeWidth(int32_t index) override {
			const auto& s = this->_values.at(index);
			SkFont font(this->_STATE->font_normal);
			font.setSize(this->_STATE->textsize2 * .9);
			SkRect bounds{};
			this->centerText(font, this, s->name.c_str(), x, y);
			font.measureText(s->name.c_str(), s->name.size(),
				SkTextEncoding::kUTF8, &bounds);
			iconWidth = 1.25 * this->height;
			this->width = bounds.width() + iconWidth;
			auto w = this->measureTextFixed(iconWidth, this->height, this->_STATE->font_md, ICON_MD_DELETE, &xT, &yT,
				this->height.load() * .5);
		}

		void setup(tsl::AppState* _appState) {
			button._appState = _appState;
			button.startx = this->width.load() - this->height.load();
			button.width = this->height.load();
			button.stopx = button.startx.load() + button.width.load();
			button.height = this->height.load();
			button.stopy = this->height.load();
			button.textpadding = 40;
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * .9);
			for (auto& ptl : this->_values) {
				displayStrings.push_back(this->truncateText(font, ptl->name, button.startx - this->textOffset));
			}
			if (!this->_values.empty()) {
				this->centerText(font, this, this->_values.at(0)->name.c_str(), x, y);
			}
		}

		CallbackResult onInput(const InputEvent& e, int index) {
			if (e.action == ACTION_UP) {
				auto tmp = this->indexhot.load();
				TextViewBase<T>::cb(e, index);
				if (tmp >= 0 && tmp < (int)this->_values.size()) {
					CallbackResult res;
					res.index = tmp;
					if (!buttonHot.load()) res.action = Action::Overwrite;
					else  res.action = Action::Delete;
					res.item = this->_values.at(tmp);
					return res;
				}
				else return {};
			}
			TextViewBase<T>::cb(e, index);
			return {};
		}
		int cb(const InputEvent& e, int index) override {
			if (e.action == ACTION_DOWN) {
				buttonHot.store(e.x >= button.startx);
			}
			return TextViewBase<T>::cb(e, index);
		}
	};

	template<typename T>
	class SavePresetView;

	template<typename T, typename T2>
	class ScrollListView : public View, protected ScrollViewBase
	{
		friend SavePresetView<T2>;

	public:
		explicit ScrollListView(tsl::AppState* appState)
			: ScrollViewBase(appState), _itemView(appState) {
			_itemView.parent = this;
			_appState = appState;
		}

		void setValues(const std::vector<T2>& vals) {
			_itemView._values = vals;
		}

		void setActive(int index) { active.store(index); }
		int getActive() const { return active.load(); }
		void acquire_slot() { slot = _STATE->waitNotify.acquire_slot(); }
		void reset_slot() { slot = 1; }

		float heightEstimate(float maxHeight) override {
			borderSize = _STATE->textsize2 * 0.5;
			_itemView.computeHeight();
			rowHeight = itemheight = _itemView.height;
			int availableHeight = maxHeight - 2 * borderSize - lw;
			int numFullItemsThatFit = availableHeight / itemheight;
			int totalBoxHeight = itemheight * _itemView._values.size();
			if (_itemView._values.size() > (size_t)numFullItemsThatFit)
				boxheight = numFullItemsThatFit * itemheight;
			else boxheight = itemheight * _itemView._values.size();
			return boxheight + 2 * borderSize + lw;
		}

		void init() override {
			borderSize = _STATE->textsize2 * 0.5;
			boxwidth = width.load() - borderSize - lw;
			_itemView.textOffset = borderSize;
			boxoffsetx = lw2;
			boxoffsety = borderSize + lw2;
			drawwidth = width.load();
			drawstartx = startx;
			drawstopx = drawstartx + drawwidth;
			_itemView.startx = 0;
			_itemView.width = boxwidth;
			_itemView.stopx = _itemView.startx + _itemView.width;

			_itemView.computeHeight();
			rowHeight = itemheight = _itemView.height;
			int availableHeight = height - 2 * borderSize - lw;
			int numFullItemsThatFit = availableHeight / itemheight;
			int totalBoxHeight = itemheight * _itemView._values.size();
			if (_itemView._values.size() > (size_t)numFullItemsThatFit)
				boxheight = numFullItemsThatFit * itemheight;
			else boxheight = itemheight * _itemView._values.size();
			maxoffset = totalBoxHeight > boxheight ? -(totalBoxHeight - boxheight) : 0;

			auto finalheight = boxheight + 2 * borderSize + lw;
			if (finalheight < height) {
				auto diff = height - finalheight;
				height -= diff;
				starty += diff * .5;
				stopy -= diff * .5;
			}
			drawheight = height;
			boxoffsety = borderSize + lw2;
			drawstarty = starty;
			drawstopy = drawstarty + drawheight;
			_itemView.starty = 0;
			_itemView.stopy = _itemView.starty + _itemView.height;
			_itemView.init();
			_itemView.setup(_STATE);
			glowStartX = lw2;
			glowWidth = width.load() - lw;
		}

		void render(void* ctx) override {
			auto canvas = _STATE->graphics.getCanvas(windex, drawstartx, drawstarty, drawwidth, drawheight);
			canvas->clear(skcol::wbg);
			SkPath path;
			path.addRect(SkRect::MakeXYWH(lw2, boxoffsety, drawwidth - lw, boxheight));
			canvas->save();
			canvas->clipPath(path);
			SkPaint paint;
			paint.setColor(skcol::fg);
			paint.setStrokeWidth(lw);
			paint.setStyle(SkPaint::kFill_Style);
			paint.setAntiAlias(true);
#ifdef PLATFORM_MOBILE
			auto& s = scroller[currentScroller.load()];
			const auto elapsed = !s.isFinished() ? timeLastAction.elapsedReplace() : timeLastAction.elapsed();
			if (s.computeScrollOffset()) offset = s.getCurrY();
#else
			const auto elapsed = timeLastAction.elapsed();
#endif
			const float off = offset;
			float sy = boxoffsety + off;
			for (int i = 0; i < (int)_itemView._values.size(); i++) {
				if (sy >= boxoffsety - itemheight && sy - itemheight <= boxoffsety + boxheight) {
					canvas->save();
					canvas->translate(lw2, sy);
					_itemView.render(canvas, i);
					canvas->restore();
				}
				sy += itemheight;
			}
			const float maxoff = std::abs(maxoffset);
			if (maxoff) updateOverscroll(canvas);
			if (maxoff > 0 && elapsed < 2.0) {
				float alpha = elapsed > 1.0 ? 255.f - 255.f * (elapsed - 1.) : 255.f;
				paint.setColor(SkColorSetA(mode == 1000 ? skcol::blue_transparent : skcol::fg, alpha));
				const float length = boxheight / (1.f + maxoff / boxheight);
				const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
				canvas->drawRect(SkRect::MakeXYWH(lw2 + boxwidth + borderSize - 2 * lw, boxoffsety + pos, 2 * lw, length), paint);
			}
			canvas->restore();
			paint.setStyle(SkPaint::kStroke_Style);
			paint.setStrokeWidth(1.0f);
			paint.setColor(skcol::border);
			canvas->drawRect(SkRect::MakeXYWH(0.5f, 0.5f, (float)drawwidth - 1.0f, (float)drawheight - 1.0f), paint);
		}

		typename DeleteOverwritetItem<T2>::CallbackResult cb(const InputEvent& e) {
#ifdef PLATFORM_MOBILE
			velocityTracker.addMovement(e);
#endif
			auto xpos = e.x - drawstartx;
			auto ypos = e.y - drawstarty;
			switch (e.action) {
			case ACTION_DOWN: {
				if (xpos >= boxoffsetx && xpos < boxoffsetx + boxwidth && ypos >= boxoffsety && ypos < boxoffsety + boxheight) {
					int pos = (int)((ypos - boxoffsety - offset) / itemheight);
					if (pos >= (int)_itemView._values.size()) pos = (int)_itemView._values.size() - 1;
					mode = INSIDE;
					_itemView.cb(e, pos);
				} else {
					mode = OUTSIDE;
					_itemView.cb(e, -1);
				}
				totalmoved = 0;
				lastXpos = xpos; lastYpos = ypos;
				pointerid = e.pointer_id;
				overscrollTop = 0.0f; overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
				scroller[currentScroller.load()].forceFinished(true);
#endif
				timeLastAction.reset();
				redraw();
				break;
			}
			case ACTION_UP: {
				if (pointerid == e.pointer_id) {
					if (mode == OUTSIDE) {
						if (slot != -1) _STATE->waitNotify.wake_thread(slot);
						timeLastAction.reset(); redraw();
						return {};
					} else if (mode == INSIDE) {
						auto res = _itemView.onInput(e, -1);
						pointerid = -1; mode = UNTOUCHED;
						timeLastAction.reset(); redraw();
						return res;
					}
					if (maxoffset < 0) {
						float diffy = lastYpos - ypos;
						auto off = offset.load() - diffy;
						if (off < maxoffset) { overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll); off = maxoffset; }
						else if (off > 0) { overscrollTop = std::min(off * 0.3f, maxOverscroll); off = 0; }
						offset.store(off);
#ifdef PLATFORM_MOBILE
						float x, y;
						if (velocityTracker.getVelocity(pointerid, &x, &y) && (_STATE->mMinimumFlingVelocity <= 0 || std::abs(y) >= _STATE->mMinimumFlingVelocity)) {
							if (_STATE->mMaximumFlingVelocity <= 0 || std::abs(y) > _STATE->mMaximumFlingVelocity) y = ISNEG(y) ? -_STATE->mMaximumFlingVelocity : _STATE->mMaximumFlingVelocity;
							int cur = currentScroller.load();
							if (++cur >= numScrollers) cur = 0;
							scroller[cur].fling(0, off, 0, y, 0, 0, maxoffset, 0);
							currentScroller.store(cur);
						}
#endif
					}
					pointerid = -1; mode = UNTOUCHED;
				}
				timeLastAction.reset(); redraw();
				break;
			}
			case ACTION_MOVE: {
				if (pointerid == e.pointer_id) {
					if (mode == MOVING) {
						float diffy = lastYpos - ypos;
						float off = offset;
						off -= diffy;
						if (off < maxoffset) { overscrollBottom = std::max(overscrollBottom.load(), std::min((maxoffset - off) * 0.5f, maxOverscroll)); off = maxoffset; }
						else if (off > 0) { overscrollTop = std::max(overscrollTop.load(), std::min(off * 0.5f, maxOverscroll)); off = 0; }
						offset = off; lastXpos = xpos; lastYpos = ypos;
						timeLastAction.reset();
					} else if (mode == INSIDE || mode == OUTSIDE) {
						// lastXpos/lastYpos still hold the down point in this branch, so
						// this is displacement from the press; accumulating (+=) added the
						// full distance again on every move event and tripped the threshold
						// on a stationary finger, which then flung on release.
						totalmoved = spacing(xpos, lastXpos, ypos, lastYpos);
						if (totalmoved > _STATE->touchSlop()) {
							if (mode == INSIDE) { lastXpos = xpos; lastYpos = ypos; _itemView.cb(e, -1); if (maxoffset < 0) mode = MOVING; }
							else { mode = UNTOUCHED; pointerid = -1; }
							timeLastAction.reset(); redraw();
						}
					}
				}
				break;
			}
			case ACTION_MOUSE_WHEEL: {
				if (maxoffset < 0 && mode == UNTOUCHED) {
					float off = offset;
					off += e.pointer_id * itemheight;
					if (off < maxoffset) { overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll); off = maxoffset; }
					else if (off > 0) { overscrollTop = std::min(off * 0.3f, maxOverscroll); off = 0; }
					offset = off; timeLastAction.reset(); redraw();
				}
				break;
			}
			case ACTION_KEY_UP: {
				if (e.pointer_id == VKEY_ESCAPE) {
					if (slot != -1) _STATE->waitNotify.wake_thread(slot);
				}
				break;
			}
			default: break;
			}
			return {};
		}

		void delRecursiveCB() override {
			hasFocus.store(false); pointerid = -1; mode = UNTOUCHED; totalmoved = 0;
			overscrollTop = 0.0f; overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
			velocityTracker.clear();
#endif
		}
		void addRecursiveDraw() override { timeLastAction.reset(); }
		void delRecursiveDraw() override { _STATE->graphics.deleteWindow(windex); }

	private:
		int slot{ -1 };
		T _itemView;
		std::atomic<int> active{};
		float rowHeight{};
		float borderSize{};
		int windex{ -1 };
	};

	template<typename T>
	class SavePresetView : public tsl::graphics::AlphaPopUp {
	private:
		std::function<void(const typename DeleteOverwritetItem<T>::CallbackResult&)> func;
	public:
		explicit SavePresetView<T>(tsl::AppState* appState)
			: listView(appState), AlphaPopUp(appState) {
			onCompleteCallback = [this](const InputResult& r) {
				if (func) {
					typename DeleteOverwritetItem<T>::CallbackResult res{};
					if (r.confirmed) {
						res.action = DeleteOverwritetItem<T>::Action::New;
						res.nameFromUi = r.text;
						func(res);
						return 1;
					}
				}
				return r.confirmed == true ? 1 : 0;
			};
		}

		~SavePresetView<T>() override {
			View::deldraw();
			View::delCB();
		}

		void setup(std::vector<T>& vals, std::string t, std::function<void(const typename DeleteOverwritetItem<T>::CallbackResult&)> func_) {
			func = std::move(func_);
			setTitle(t);
			setText(title == "Save Preset" ? "My Preset" : "My Preset");
			if (!vals.empty()) { listView.setValues(vals); childView_ = &listView; }
			else childView_ = nullptr;
		}

		void callback(const InputEvent& e) override {
			if (childView_ != nullptr && (e.action != ACTION_KEY_UP && e.action != ACTION_KEY_DOWN) && listView.isInside(e)) {
				auto val = listView.cb(e);
				if (val.index != -1) {
					val.nameFromUi = AlphaPopUp::textInput1.getText();
					if (func) func(val);
					_STATE->waitNotify.wake_thread(waitSlotId);
				}
			} else AlphaPopUp::callback(e);
		}

		void render(void* c) override {
			AlphaPopUp::render(c);
			if (childView_ != nullptr) listView.render(c);
		}

	protected:
		void delRecursiveCB() override { listView.delRecursiveCB(); AlphaPopUp::delRecursiveCB(); }
		void addRecursiveDraw() override { listView.addRecursiveDraw(); AlphaPopUp::addRecursiveDraw(); }
		void delRecursiveDraw() override { listView.delRecursiveDraw(); AlphaPopUp::delRecursiveDraw(); }

	private:
		ScrollListView<DeleteOverwritetItem<T>, T> listView;
	};
}

#include <DynamicDialog.h>
using namespace tsl::graphics;

template<typename T>
void tsl::app::deleteOverwriteFunc(tsl::AppState* _appState, tsl::QueueUnsafe<T, 100>& q, std::string title, std::function<std::vector<T>()> getItems, std::function<bool(tsl::AppState*, std::string&)> saver) {
	// Seed ONCE, before the loop. This used to sit inside it, guarded by
	// q.empty() -- which meant deleting the last preset emptied the queue and the
	// next pass re-seeded from getItems(). That reads _DATA->presets, which is
	// only rebuilt from disk after this function returns, so the preset that had
	// just been deleted came straight back into the list. With one preset in the
	// bank, deleting it appeared to do nothing at all.
	{
		std::lock_guard lk(q);
		if (q.empty())
			for (auto& p : getItems()) q.push(p);
	}

	while (true) {
		std::vector<T> vals;
		{
			std::lock_guard lk(q);
			for (auto& pr : q) vals.push_back(pr);
		}

		SavePresetView<T> tt(_appState);
		auto slot = _STATE->waitNotify.acquire_slot();
		tt.setSlotId(slot);
		typename DeleteOverwritetItem<T>::CallbackResult res;
		tt.setup(vals, title, [&res](const auto& r) mutable { res = r; });
		tt.init();
		tt.addDraw();
		tt.addCB();

		_STATE->waitNotify.wait_for_signal();
		if (_STATE->destroyRequested.load()) return;

		if (res.action == DeleteOverwritetItem<T>::Canceled) break;

		std::string presetName{};
		T itemToReplace{};
		std::string fileToRemove{};
		if (res.item != nullptr) {
			if (res.action == DeleteOverwritetItem<T>::Delete || res.action == DeleteOverwritetItem<T>::Overwrite) {
				const bool overwrite = res.action == DeleteOverwritetItem<T>::Overwrite;
				tsl::graphics::DynamicDialog loginDialog(_STATE);
				std::string dlgTitle = overwrite ? "Replace '" : "Delete '";
				dlgTitle += res.item->name;
				dlgTitle += "'";
				// The replacement is named from the text field, never from the item
				// being replaced — spell that out before anything touches the disk.
				if (overwrite) {
					dlgTitle += " with '";
					dlgTitle += res.nameFromUi.empty() ? std::string("<no name>") : res.nameFromUi;
					dlgTitle += "'";
				}
				dlgTitle += "?";
				loginDialog.setTitle(dlgTitle);
				loginDialog.setButtonMode(DynamicDialog::ButtonMode::OK_CANCEL);
				auto slot2 = _STATE->waitNotify.acquire_slot();
				bool ack = false;
				loginDialog.onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
					ack = result.confirmed;
					_STATE->waitNotify.wake_thread(slot2);
				};
				loginDialog.init();
				loginDialog.addDraw();
				loginDialog.addCB();
				_STATE->waitNotify.wait_for_signal();
				loginDialog.delCB();
				loginDialog.deldraw();
				if (ack) {
					if (!overwrite) {
						// Not std::filesystem: on Android the path may be a
						// content:// URI in the user's own preset folder, which
						// has no filesystem entry to unlink.
						if (!::Preset::removePreset(res.item->path))
							showToast(_STATE, "Delete failed.");
						else {
							std::lock_guard lk(q);
							for (auto& p : q) { if (p == res.item) { q.del(p); break; } }
							showToast(_STATE, "OK");
						}
						continue;
					}
					// Replace: the old file is only removed AFTER the new preset was
					// written successfully — a failed save must not destroy the
					// preset it replaces.
					itemToReplace = res.item;
					fileToRemove  = res.item->path;
					presetName    = res.nameFromUi;
				} else continue;
			}
		} else if (res.action == DeleteOverwritetItem<T>::New) {
			presetName = res.nameFromUi;
		}

		if (!presetName.empty()) {
			tt.delCB();
			tt.deldraw();
			if (saver(_appState, presetName) && itemToReplace != nullptr) {
				if (::Preset::removePreset(fileToRemove)) {
					std::lock_guard lk(q);
					for (auto& p : q) { if (p == itemToReplace) { q.del(p); break; } }
				} else {
					// Keep the entry: its file is still there — dropping it here is
					// what let the stale item come back as a duplicate on rebuild.
					showToast(_STATE, "Saved, but the replaced preset could not be removed.");
				}
			}
			break;
		} else showToast(_STATE, "Name empty.");
	}
}

// PA instantiation
using PAPreset = std::shared_ptr<Preset::Preset>;
template void tsl::app::deleteOverwriteFunc<PAPreset>(
	tsl::AppState*,
	tsl::QueueUnsafe<PAPreset, 100>&,
	std::string,
	std::function<std::vector<PAPreset>()>,
	std::function<bool(tsl::AppState*, std::string&)>);
