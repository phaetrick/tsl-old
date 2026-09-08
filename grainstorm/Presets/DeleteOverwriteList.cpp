#include "view.h"
#include "RecyclerView.h"
#include "ButtonBase.h"
#include "keyboard.h"
#include <filesystem>
#include <IconsMaterialDesignReduced.h>
#include "app.h"
#include "tools/PlatformPaths.h"

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
			//if(indexhot.load() == index)
			button.render(c);

			//		c->drawString(ICON_MD_DELETE, width - iconWidth + xT, yT, font2, paint);
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

				if (tmp >= 0 && tmp < this->_values.size()) {
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

		void setActive(int index) {
			active.store(index);
		}

		int getActive() const {
			return active.load();
		}


		void acquire_slot() {
			waitToken = _STATE->waitNotify.begin_wait();
		}

		void reset_slot() {
			waitToken = {};
		}

		float heightEstimate(float maxHeight) override {
			borderSize = _STATE->textsize2 * 0.5;

			_itemView.computeHeight();
			rowHeight = itemheight = _itemView.height;

			int availableHeight = maxHeight - 2 * borderSize - lw;

			// Calculate how many FULL items fit
			int numFullItemsThatFit = availableHeight / itemheight;
			int totalBoxHeight = itemheight * _itemView._values.size();

			// Draw only the full items that fit
			if (_itemView._values.size() > numFullItemsThatFit)
				boxheight = numFullItemsThatFit * itemheight;
			else boxheight = itemheight * _itemView._values.size();
			auto finalheight = boxheight + 2 * borderSize + lw;
			return finalheight;
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

			/*

			_itemView.computeHeight();
			rowHeight = itemheight = _itemView.height;


			boxheight = itemheight * _itemView._values.size();

			if (boxheight > height - 2 * borderSize - lw) {
				maxoffset = -DISTANCE(boxheight, height - 2 * borderSize - lw);
			}
			else {
				maxoffset = 0;
			}
			*/

			_itemView.computeHeight();
			rowHeight = itemheight = _itemView.height;

			int availableHeight = height - 2 * borderSize - lw;

			// Calculate how many FULL items fit
			int numFullItemsThatFit = availableHeight / itemheight;
			int totalBoxHeight = itemheight * _itemView._values.size();

			// Draw only the full items that fit
			if (_itemView._values.size() > numFullItemsThatFit)
				boxheight = numFullItemsThatFit * itemheight;
			else boxheight = itemheight * _itemView._values.size();

			// Calculate scroll range based on total content vs drawable area
			if (totalBoxHeight > boxheight) {
				maxoffset = -(totalBoxHeight - boxheight);
			}
			else {
				maxoffset = 0;
			}

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
			if (!canvas)
				return;
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
					canvas->translate(lw2, sy);
					_itemView.render(canvas, i);
					canvas->restore();
				}
				sy += itemheight;
			}



			// Draw scrollbar
			const float maxoff = std::abs(maxoffset);

			if (maxoff) {
				// Update overscroll animation
				updateOverscroll(canvas);
			}

			if (maxoff > 0 && elapsed < 2.0) {
				float alpha = elapsed > 1.0 ? 255.f - 255.f * (elapsed - 1.) : 255.f;
				paint.setColor(SkColorSetA(mode == 1000 ? skcol::blue_transparent : skcol::fg, alpha));
				const float length = boxheight / (1.f + maxoff / boxheight);
				const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
				canvas->drawRect(
					SkRect::MakeXYWH(lw2 + boxwidth + borderSize - 2 * lw, boxoffsety + pos, 2 * lw, length),
					paint);
			}
			canvas->restore();
			// Border (match NumericalPopUp)
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
				if (xpos >= boxoffsetx && xpos < boxoffsetx + boxwidth && ypos >= boxoffsety &&
					ypos < boxoffsety + boxheight) {
					int pos = (int)((ypos - boxoffsety - offset) / itemheight);
					if (pos >= _itemView._values.size())
						pos = _itemView._values.size() - 1;
					mode = INSIDE;
					_itemView.cb(e, pos);
				}
				else {
					mode = OUTSIDE;
					_itemView.cb(e, -1);
				}
				totalmoved = 0;
				lastXpos = xpos;
				lastYpos = ypos;
				pointerid = e.pointer_id;

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
				if (pointerid == e.pointer_id) {
					if (mode == OUTSIDE) {
						if (waitToken.ticket) {
							_STATE->waitNotify.complete(waitToken);
						}
						timeLastAction.reset();
						redraw();
						return {};
					}
					else if (mode == INSIDE) {
						auto res = _itemView.onInput(e, -1);
						pointerid = -1;
						mode = UNTOUCHED;
						timeLastAction.reset();
						redraw();

						return res;
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
				if (pointerid == e.pointer_id) {
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
								_itemView.cb(e, -1);
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
					off += e.pointer_id * itemheight;

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
				if (e.pointer_id == VKEY_ESCAPE) {
					if (waitToken.ticket) {
						_STATE->waitNotify.complete(waitToken);
					}
				}
				break;
			}
			default:
				break;
			}
			return {};
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
			//	View::delRecursiveCB();
		}
		void addRecursiveDraw() override {
			timeLastAction.reset();
			//		View::addRecursiveDraw();
		}

		void delRecursiveDraw() override {
			_STATE->graphics.deleteWindow(windex);
			//View::delRecursiveDraw();
		}

	private:
		tsl::ThreadSignal::WaitToken waitToken{};
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
			setText(title == "Save Preset" ? "My Preset" : title == "Save Project" ? "My Project" : "My MIDIMapping");
			if (!vals.empty()) {
				listView.setValues(vals);
				childView_ = &listView;
			}
			else childView_ = nullptr;

		}
		void callback(const InputEvent& e) override {
			if (childView_ != nullptr && (e.action != ACTION_KEY_UP && e.action != ACTION_KEY_DOWN) && listView.isInside(e)) {

				auto val = listView.cb(e);
				if (val.index != -1) {
					val.nameFromUi = AlphaPopUp::textInput1.getText();
					if (func)func(val);
					wakeWaiter();
				}

			}
			else AlphaPopUp::callback(e);
		}
		void render(void* c) override {
			AlphaPopUp::render(c);

			if (childView_ != nullptr) {
				listView.render(c);
			}
		}
	protected:
		void delRecursiveCB() override {
			listView.delRecursiveCB();
			AlphaPopUp::delRecursiveCB();
		}
		void addRecursiveDraw() override {
			listView.addRecursiveDraw();
			AlphaPopUp::addRecursiveDraw();
		}

		void delRecursiveDraw() override {
			listView.delRecursiveDraw();
			AlphaPopUp::delRecursiveDraw();
		}

	private:
		bool isProject{};

		ScrollListView<DeleteOverwritetItem<T>, T> listView;
	};

}

#include <DynamicDialog.h>
using namespace tsl::graphics;

namespace {

	// Every file that belongs to one saved item: the preset/project/mapping file
	// itself plus its "-audio-N.flac" side files, which share its filename as a
	// prefix. Collected up front so the directory is never mutated while it is
	// walked, and so a file written *after* this call (the replacement) can never
	// be caught by the prefix match. Returns false only if storage was
	// unreadable; an empty result then means "nothing left to remove".
	bool collectItemFiles(const std::string& path, std::vector<std::string>& out) {
		if (path.empty()) return false;
#ifdef __ANDROID__
		out.push_back(path);
		return true;
#else
		const std::filesystem::path p(path);
		const auto prefix = p.filename().string();
		if (prefix.empty()) return false;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(p.parent_path(), ec))
			if (entry.path().filename().string().starts_with(prefix))
				out.push_back(entry.path().string());
		if (ec) {
			out.clear();
			return false;
		}
		return true;
#endif
	}

	// True only if every file is gone afterwards. Each removal gets its own
	// error_code: sharing one across the loop let a later success (an audio side
	// file) hide an earlier failure on the item itself, which is how a stale file
	// survived a "successful" replace and reappeared as a duplicate.
	bool removeItemFiles(const std::vector<std::string>& files, std::string& err) {
		bool ok = true;
		for (const auto& f : files) {
#ifdef __ANDROID__
			if (!tsl::app::deleteFromUri(f)) {
				ok = false;
				if (err.empty()) err = "permission denied";
			}
#else
			std::error_code ec;
			std::filesystem::remove(f, ec);
			if (ec) {
				ok = false;
				if (err.empty()) err = ec.message();
			}
#endif
		}
		return ok;
	}

}

template<typename T>
void tsl::app::deleteOverwriteFunc(tsl::AppState* _appState, tsl::QueueUnsafe<T, 100>& q, std::string title, std::function <std::vector<T>()>getItems, std::function<bool(tsl::AppState*, std::string&)>saver) {
	while (true) {
		std::vector <T> vals;
		{
			std::lock_guard lk(q);
			int i = 0;
			if (q.empty()) {
				vals = getItems();
				for (auto& p : vals)
					q.push(p);
			}
			else
				for (auto& pr : q)vals.push_back(pr);
		}

		// res must be declared before tt: tt's callback writes into it by
		// reference and is only unregistered by ~SavePresetView, so a res
		// declared later would already be destroyed while input still lands.
		typename DeleteOverwritetItem<T>::CallbackResult res;
		SavePresetView<T> tt(_appState);
		tt.setToken(_STATE->waitNotify.begin_wait());
		tt.setup(vals, title, [&res](const auto& r)mutable {
			res = r; });
		tt.init();
		tt.addDraw();
		tt.addCB();

		_STATE->waitNotify.wait_for_signal(tt.token());
		if (_STATE->destroyRequested.load())return;
		if (res.action == DeleteOverwritetItem<T>::Canceled) {
			//showToast(_STATE, "Canceled.");
			break;
		}
		std::string presetName{};
		T itemToReplace{};
		std::vector<std::string> filesToRemove;
		if (res.item != nullptr) {
			if ((res.action == DeleteOverwritetItem<T>::Delete || res.action == DeleteOverwritetItem<T>::Overwrite)) {
				const bool overwrite = res.action == DeleteOverwritetItem<T>::Overwrite;
				tsl::graphics::DynamicDialog loginDialog(_STATE);
				std::string dlgTitle = overwrite ? "Replace " : "Delete ";
				dlgTitle += "'";
				dlgTitle += res.item->name;
				dlgTitle += "'";
				// The replacement is named from the text field, never from the item
				// being replaced. Spell that out here -- it used to stay invisible
				// until the file had already been overwritten with the default name.
				if (overwrite) {
					dlgTitle += " with '";
					dlgTitle += res.nameFromUi.empty() ? std::string("<no name>") : res.nameFromUi;
					dlgTitle += "'";
				}
				dlgTitle += "?";
				loginDialog.setTitle(dlgTitle);
				loginDialog.setButtonMode(DynamicDialog::ButtonMode::OK_CANCEL);
				auto token = _STATE->waitNotify.begin_wait();
				bool ack = false;
				loginDialog.onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
					ack = result.confirmed;
					_STATE->waitNotify.complete(token); // unblocks
					};

				loginDialog.init();
				loginDialog.addDraw();
				loginDialog.addCB();
				_STATE->waitNotify.wait_for_signal(token); // blocks until user interacts

				//if (_STATE->destroyRequested.load())return;
				loginDialog.delCB();
				loginDialog.deldraw();
				if (!ack) continue;

				if (!overwrite) {
					std::vector<std::string> files;
					std::string err;
					if (!collectItemFiles(res.item->path, files) || !removeItemFiles(files, err)) {
						auto s = std::string("Delete failed: ") +
							(err.empty() ? std::string("could not access storage") : err);
						showToast(_STATE, s.c_str());
					}
					else {
						std::lock_guard lk(q);
						q.del(res.item);
						showToast(_STATE, "OK");
					}
					continue;
				}

				// Snapshot the old item's files now, while the replacement has not
				// been written yet. Removing them only after a successful write
				// means a failed save no longer destroys the project it replaces.
				if (!collectItemFiles(res.item->path, filesToRemove)) {
					showToast(_STATE, "Replace failed: could not access storage.");
					continue;
				}
				itemToReplace = res.item;
				presetName = res.nameFromUi;
			}
		}
		else if (res.action == DeleteOverwritetItem<T>::New)presetName = res.nameFromUi;
		if (!presetName.empty()) {
			tt.delCB();
			tt.deldraw();

			// The write can be long -- Save Project FLAC-encodes every track's audio
			// -- and this runs on the modal thread (UiTasksQueue). Doing it here
			// blocks every other dialog (EDITOR, RECORD, knob text entry, ...) for
			// the duration: taps appear to do nothing, then every queued dialog opens
			// at once when the encode finishes. Hand it to WorkerQueue -- the thread
			// the preset LOAD path already uses -- and let this one go straight back
			// to serving dialogs.
			//
			// Write and replace-cleanup move together on purpose: saver()'s bool is
			// the gate that stops a failed write from deleting the item it was
			// replacing, so splitting them would reintroduce exactly the data loss the
			// collect-before-write order above exists to prevent.
			//
			// The state goes behind one shared_ptr because MPSCWorker's Task is a
			// 64-byte inplace_function: capturing saver + name + item + file list +
			// queue field by field does not fit.
			struct Commit {
				std::function<bool(tsl::AppState*, std::string&)> saver;
				std::string               name;
				T                         item{};
				std::vector<std::string>  files;
				tsl::QueueUnsafe<T, 100>* q{};
			};
			auto commit = std::make_shared<Commit>();
			commit->saver = saver;
			commit->name  = presetName;
			commit->item  = itemToReplace;
			commit->files = std::move(filesToRemove);
			commit->q     = &q;   // an AppState member, outlives the task

			if (!_STATE->WorkerQueue.add_task([_appState, commit] {
				if (commit->saver(_appState, commit->name) && commit->item != nullptr) {
					std::string err;
					if (removeItemFiles(commit->files, err)) {
						std::lock_guard lk(*commit->q);
						commit->q->del(commit->item);
					}
					else {
						// Keep the entry: its file is still on disk, and dropping it
						// here is what let the stale item come back as a duplicate the
						// next time the list was rebuilt from storage.
						auto s = std::string("Saved, but the replaced file could not be removed: ") + err;
						showToast(_STATE, s.c_str());
					}
				}
				}))
				showToast(_STATE, "Busy, try again.");
			break;
		}
		else showToast(_STATE, "Name empty.");
	}
}

#include "preset.h"

using OT = std::shared_ptr<tsl::preset::PresetWrapper>;

template void tsl::app::deleteOverwriteFunc<OT>(
	tsl::AppState*,
	tsl::QueueUnsafe<OT, 100>&,
	std::string,
	std::function<std::vector<OT>()>,
	std::function<bool(tsl::AppState*, std::string&)>);

#include "MidiSaver.h"

using OT2 = std::shared_ptr<MIDIHEADER2>;
template void tsl::app::deleteOverwriteFunc<OT2>(
	tsl::AppState*,
	tsl::QueueUnsafe<OT2, 100>&,
	std::string,
	std::function<std::vector<OT2>()>,
	std::function<bool(tsl::AppState*, std::string&)>);

