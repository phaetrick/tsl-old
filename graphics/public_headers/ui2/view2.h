#pragma once
#include "Input.h"
#include "queuetsl.h"
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <functional>
#include <cstdint>
#include <include/core/SkRect.h>
#include <SkCanvas.h>

namespace tsl
{
	struct AppState;
}
namespace tsl::ui {

	class Style;
	
	enum class Axis { Vertical, Horizontal };

	enum class Align {
		Start,
		Center,
		End
	};

	enum ViewFlags : uint32_t {
		WRAP_WIDTH = 1 << 0,
		WRAP_HEIGHT = 1 << 1,
		FILL_WIDTH = 1 << 2,
		FILL_HEIGHT = 1 << 3,
		SYM_WIDTH = 1 << 4,
		SYM_HEIGHT = 1 << 5,
		FIXED_WIDTH = 1 << 6,
		FIXED_HEIGHT = 1 << 7,
		FRACTION_WIDTH_PARENT = 1 << 8,
		FRACTION_HEIGHT_PARENT = 1 << 9,
		FRACTION_WIDTH_WINDOW = 1 << 10,
		FRACTION_HEIGHT_WINDOW = 1 << 11,
		WRAP_CONTENT_WIDTH = 1 << 12,
		WRAP_CONTENT_HEIGHT = 1 << 13,
	};

	class View {
	public:
		View() = default;

		// Minimal ctor (just sets _appState, everything else default)
		explicit View(tsl::AppState* app) : _appState(app) {}

		// Convenience: construct with app and size flags
		View(tsl::AppState* app,
			uint32_t flags,
			Align hAlign = Align::Start,
			Align vAlign = Align::Start,
			int overlapGroup = 0,
			int priority = 20,
			bool permanent = false)
			: _appState(app), flags(flags), alignH(hAlign), alignV(vAlign),
			overlap(overlapGroup), prio(priority), perm(permanent) {
		}

		// Absolute-bounds ctor
		View(tsl::AppState* app,
			int sx, int sy, int ex, int ey,
			Align hAlign = Align::Start,
			Align vAlign = Align::Start,
			int overlapGroup = 0,
			int priority = 20,
			bool permament = false)
			: _appState(app), alignH(hAlign), alignV(vAlign),
			overlap(overlapGroup), prio(priority), perm(permament)
		{
			setBounds(sx, sy, ex, ey);
		}
		//   // Fluent setters for absolute-bounds systems (return *this for chaining)
		View& setBounds(int sx, int sy, int ex, int ey) {
			startX = sx; startY = sy; stopX = ex; stopY = ey;
			width = ex - sx; height = ey - sy;
			bounds = SkRect::MakeLTRB((float)startX, (float)startY, (float)stopX, (float)stopY);
			return *this;
		}

		View& alignCenter() { alignH = Align::Center; alignV = Align::Center; return *this; }
		View& alignStart() { alignH = Align::Start;  alignV = Align::Start;  return *this; }
		View& alignEnd() { alignH = Align::End;    alignV = Align::End;    return *this; }


		int id = 0;
		int relX = 0, relY = 0, width = 0, height = 0;

		// Absolute screen-space (relative to window)
		int startX = 0, startY = 0, stopX = 0, stopY = 0;
		SkRect bounds;

		Align alignH = Align::Start;
		Align alignV = Align::Start;
		int overlap = 0;
		int prio = 0;
		uint32_t flags = WRAP_WIDTH | WRAP_HEIGHT;
		float fractionWidth = 1.0f;
		float fractionHeight = 1.0f;
		std::atomic<bool> visible{ false };
		std::vector<View*> children;
		std::function<bool()> doEnqueue = [] { return true; };

		bool perm = false;

		virtual int callback(tsl::graphics::InputEvent& event) { return 0; }	
		virtual void render(SkCanvas*) {};
		virtual void init() {}

		virtual int desiredWidth() const;

		virtual int desiredHeight() const;

		int maxChildWidth() const {
			int maxW = 0;
			for (const View* c : children)
				maxW = std::max(maxW, c->desiredWidth());
			return maxW;
		}

		int maxChildHeight() const {
			int maxH = 0;
			for (const View* c : children)
				maxH = std::max(maxH, c->desiredHeight());
			return maxH;
		}
		
		void enqueueDrawSafe();
		
		void removeDrawSafe();
		
		void enqueueInputSafe();
		
		void removeInputSafe();
		
		void redrawSafe();

		virtual void enqueueDraw();

		virtual void removeDraw();

		virtual void enqueueInput();
		virtual void removeInput(); 

		void updateBounds() {
			startX = (hasParent() ? _parent->startX : 0) + relX;
			startY = (hasParent() ? _parent->startY : 0) + relY;
			stopX = startX + width;
			stopY = startY + height;
			bounds.setXYWH((float)startX, (float)startY, (float)width, (float)height);
		}

		void addChild(View* v) {
			v->_parent = this;
			children.push_back(v);
		}

		int dp(float units);

		Style* style() const;

		tsl::AppState* _appState = nullptr;
		View* _parent = nullptr;

		inline bool hasParent() const { return _parent != nullptr; }
	};

	// -----------------------------
	class Layout : public View {
	public:
		using View::View;  // Inherit all constructors from A

		void enqueueDraw() override {
			for (auto* child : children) {
				if (child->doEnqueue())
					child->enqueueDraw();
			}
		}
		void removeDraw() override {
			for (auto* child : children)
				child->removeDraw();
			View::removeDraw();
		}
		void enqueueInput() override {
			for (auto* child : children) {
				if (child->doEnqueue())
					child->enqueueInput();
			}
		}
		void removeInput() override {
			for (auto* child : children)
				child->removeInput();
			View::removeInput();
		}

	protected:
		void initLayout(Axis axis);
		int computeAlignH(const View* v, int w) const;
		int computeAlignV(const View* v, int h) const;
		int scrollOffset = 0, spacing = 0;
	};

	class LayoutVertical : public Layout {
	public:
		using Layout::Layout;  // Inherit all constructors from A
		void init() override {
			initLayout(Axis::Vertical);
		}
	};

	class LayoutHorizontal : public Layout {
	public:
		using Layout::Layout;  // Inherit all constructors from A

		void init() override {
			initLayout(Axis::Horizontal);
		}
	};

} // namespace tsl::ui
