// Animated Spinner with Glassy Background Overlay
#pragma once
#include <view.h>

namespace tsl {
	class AppState;
	namespace graphics {
		
		class AnimatedSpinner : public View {
		private:
			// Animation state
			float rotation = 0.0f;
			std::chrono::steady_clock::time_point startTime;

			// Spinner properties
			float spinnerRadius = 30.0f;
			float strokeWidth = 4.0f;
			int numSegments = 12;
			float animationSpeed = 360.0f; // degrees per second

			// Colors
			SkColor primaryColor = SkColorSetARGB(255, 0, 122, 255);   // Blue
			SkColor secondaryColor = SkColorSetARGB(60, 0, 122, 255);  // Transparent blue
			SkColor glassyBg = SkColorSetARGB(120, 40, 40, 40);        // Semi-transparent dark

			// Window management
			int windowindex = -1;

		public:
			explicit AnimatedSpinner(tsl::AppState* appState)
				: View(appState, WRAP, 0, CENTER_ALIGN, 0) {
				perm = true; // Permanent overlay
				startTime = std::chrono::steady_clock::now();
			}

			void init() override;


			void render(void* c) override;

			void delRecursiveDraw() override;

			void addRecursiveDraw() override;

		private:


			void drawGlassyBackground(SkCanvas* canvas);

			void drawSpinner(SkCanvas* canvas);
		};


	} // namespace graphics
} // namespace tsl

