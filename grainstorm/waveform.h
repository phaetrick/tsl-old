#pragma once
#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <cstdint>
#include <sys/types.h>
#include <atomic>
#include <vector>
#include "defines.h"
#include <colours.h>
#include <view.h>
#include <tools/AtomicSharedPtr.h>
#include <audio/Recording.h>
#include <mutex>


#include <tools.h>

namespace tsl {
	struct AppState;
}

struct TRACK;

namespace tsl::graphics {
	enum WaveformMode : int32_t {
		UNTOUCHED = 0,
		WAVE = 1,
		OFF_START = 2,
		OFF_STOP = 3,
		OFF_GRAIN = 4,
		ZOOMWAVEFORM = 5
	};

	class Waveform2;

	struct pointer2 {
		int32_t id;
		float xpos;
		float ypos;
		int32_t mode;

		bool operator==(const pointer2 &other) const {
			return (id == other.id && xpos == other.xpos && ypos == other.ypos &&
					mode == other.mode);
		}
	};


	struct WAVE_S {
		explicit WAVE_S(tsl::AppState *appState):_appState(appState) {}
		~WAVE_S();
		void render(std::shared_ptr<tsl::Recording>&, int32_t orientation = VERTICAL);
		std::vector<std::array<int16_t, 4>> waveformarray;
		int32_t width{};
		int32_t height{};
		WAVE_S *next{};
		float zoom{};
		tsl::AppState* _appState;
		int channels{};
	};

	class Waveform : public View {
		friend Waveform2;
		friend WAVE_S;
	public:
		Waveform() = default;
		Waveform(tsl::AppState *, TRACK *track);
		void setup(std::shared_ptr<tsl::Recording>&);
	protected:
		

		struct WaveformRenderData {
			std::unique_ptr<WAVE_S> wave_s{};
			std::shared_ptr<tsl::Recording> filebuffer{};
			std::atomic<MYFLOAT> posStartLoop{}, offsetLoop{}, posStopLoop{};
			std::atomic<WaveformMode> last{ OFF_STOP };
		};
		
		TRACK* associatedtrack{};
		tsl::AtomicSharedPtr<WaveformRenderData> renderData{};

		int32_t orientation;

		void render(void* ctx) override;

		void init() override;

		void callback(const InputEvent &e) override;

		void render_surface_static();

		void render_surface_direct();

		void delRecursiveCB() override;

		tsl::time timer;
		//short *waveformarray;
		float markerwidth{};
		float markerheight{};
		float markerwidth_half{};
		float markerheight_half{};
        float width_triangle{},
                height_triangle{},
                height_rect{},
                corner_radius{};
		std::atomic<float> zoom_gap{};
		float olddist{1};
		tsl::graphics::InputSystem::InputState pointers;
		int startx{}, starty{}, stopx{}, stopy{}, width{}, height{};
		tsl::AppState* _appState{};
#ifdef USE_IMGUI
		std::vector<std::array<short, 4>> waveformarray;
#else
		int windex{-1};
#endif
	};



	class Waveform2 : public View {
	public:
		Waveform2(tsl::AppState*, float scalefactor, int32_t aspect_ratio, int alignment);

		void render(void* ctx) override;

		void callback(const InputEvent& e) override;

		void init() override;

		void delRecursiveCB() override;
	private:
		Waveform waveforms[4];
		int32_t orientation;
		//short *waveformarray;
		float markerwidth{};
		float markerheight{};
		float markerwidth_half{};
		float markerheight_half{};
		float width_triangle{},
			height_triangle{},
			height_rect{},
			corner_radius{};
		std::mutex mtx{};
		// Resolved at the FIRST finger-down, kept until the last finger lifts.
		// Re-resolving per event from active_track / LOOPER / DISTRSOURCE sent a
		// mid-gesture UP to a different Waveform than the DOWN -- stranding the
		// old one's pointer and its filebuffer isresizing/ismoving flags.
		Waveform* gestureTarget{};
		int32_t gesturePointers{};

	};

}
#endif
