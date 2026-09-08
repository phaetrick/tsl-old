#pragma once
#include <deque>
#include "view.h"
#include "Input.h"
#include "RecyclerView.h"

struct TRACK;
namespace tsl {
	struct AppState;
	namespace graphics {
		class EffectOrderView : public View, protected ScrollViewBase {
		public:
			explicit EffectOrderView(tsl::AppState* appState);

			void computeSize() override;
			void callback(const InputEvent& event) override;

			void render(void* ctx) override;
			static int activate(TRACK* track);

		protected:
			void addRecursiveDraw() override;

			void delRecursiveDraw() override;
			void addRecursiveCB() override;

			void delRecursiveCB() override;
			void reset();
			std::deque<std::pair<std::string_view, int>> grainFX, monoFX, stereoFX;
		private:
			struct bla {
				bla(const char* name_, int32_t q_, int qpos_, int id_, float y_) : name(name_), q(q_),
					qpos(qpos_), id(id_),
					y(y_) {
				};
				const char* name{};
				int32_t q;
				int32_t qpos{};
				int32_t id;
				float y{};
			};

			std::atomic<bla> activeFx{ bla(nullptr, -1, 0, 0, 0.f) };
			tsl::AtomicTimer timer;
			static constexpr const char* titles[3] = { "GRAIN FX", "MONO FX", "STEREO FX" };
			int windowindex{ -1 };
		};

		class FXView : public TextViewBase<std::pair<std::string, int>> {
		public:
			explicit FXView(tsl::AppState* appState);
			void render(SkCanvas* c, int32_t index) override;

			void computeWidth(int32_t index) override;

			int32_t cb(const InputEvent& e, int index) override;

		private:
		};


	}

}

