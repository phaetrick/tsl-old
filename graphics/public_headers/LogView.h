#pragma once
#include "view.h"
namespace tsl {
	namespace graphics {
		class LogView :public View{
		public:
			LogView(tsl::AppState*app) : View(app, WRAP, 0, CENTER_ALIGN, 0) {
				perm=true;
			}
			~LogView() { deldraw(); delCB(); }
			void render(void*) override;
			void callback(const InputEvent& event)override;
			void init() override;
		protected:
			void delRecursiveDraw() override;
		private:
			int windex{ -1 };
			bool firstRun{ true };
			int scrollOffset = 0;  // Current scroll position (in lines)
			int maxScrollOffset = 0; // Maximum allowed scroll offset
			std::vector<std::string> cachedWrappedLines; // Cache wrapped lines
			bool needsRecalc = true; // Flag to recalculate wrapped lines
			float lastY = 0;
			bool isDragging = false;
			bool buttonTouched = false;
			static constexpr float dragThreshold = 10.0f; // Minimum drag distance to prevent accidental scrolling
			bool hasDragged = false;
			bool autoScrollToBottom = true; // Auto-scroll to bottom when new messages arrive
            bool userHasScrolled = false; // Track if user has manually scrolled
			void drawScrollIndicator(SkCanvas* canvas, int w, int h, float lineSpacing);
			void drawScrollToBottomButton(SkCanvas* canvas, int w, int h);
		};
	}
}
