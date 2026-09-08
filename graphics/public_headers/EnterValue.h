#pragma once
#ifndef GRAINSTORM_ENTERVALUE_H
#define GRAINSTORM_ENTERVALUE_H
#include "defines.h"
#include "types.h"
#include "keyboard.h"
#include "params.h"


namespace tsl {
	namespace graphics {
		class EnterValue : public NumericalPopUp {
		public:
			EnterValue(tsl::AppState* state) : NumericalPopUp(state) {};
			static void Task(tsl::AppState* state, tsl::parameters::Event&);

			void setup(tsl::parameters::Event&);

			virtual int onEnter() override;
		private:
			tsl::parameters::Event e{};
		};

		class ValueView : public View {
		public:
			ValueView(tsl::AppState* appState, View& par);
			ValueView(tsl::AppState* appState, std::function<void()>_f) : View(appState, WRAP, 0, CENTER_ALIGN, 0), ff{ _f }, par_{ *this } {
			};
			void render(void*) override {
				ff();
			}
			static constexpr const char* formatvalues[6] = { "%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f" };

			void addRecursiveDraw()override;
			void delRecursiveDraw() override;
			View& par_;
			std::function<void()> ff;
			tsl::time timer;
			int windex{ -1 };
			tsl::parameters::Event e{};
		};

		class ValueView2 : public View {
		public:
			ValueView2(tsl::AppState* appState, View& par_) : View(appState, WRAP, 0, CENTER_ALIGN, 0), par(par_){
			};

			void render(void*)override;

			static constexpr const char* formatvalues[6] = { "%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f" };

			void addRecursiveDraw() override;
			void delRecursiveDraw() override;
		private:
			View& par;
			int windex{ -1 };
			tsl::time timer{};
			tsl::parameters::Event e{};

		};

	}
}

#endif //GRAINSTORM_ENTERVALUE_H
