#pragma once
#include "types.h"
#include "tools.h"
#include "textview.h"
#include "ButtonBase.h"
#include "logger.h"
#include "defines.h"
#include "EnterValue.h"
#include "params.h"
#include "view.h"
#include <cstdint>
#include <atomic>
#include <deque>

namespace tsl { namespace graphics {

	class Knob;

	class KnobSurface : public View {
		friend Knob;
	public:
		KnobSurface(tsl::AppState* appState) : View(appState) {};
		void render(void *ctx);
		Knob *knob{};
	protected:
		float radius = 0;
		float radius_arrow = 0;
		float center_x = 0, center_y = 0;
	};

	class Knob : public View {
		friend KnobSurface;
	public:
		Knob(tsl::AppState *appState, const float scalefactor,
			 const int aspect_ratio,
			 const int alignment, int target, long offset = 0, int offsetmulti = 1,
			 Layout *parent = nullptr);

		virtual void callback(const InputEvent &event) override ;

		virtual void render(void *ctx) override ;

		virtual void init() override ;

		void setTitle(const char *_title) {
			title.setText(_title);
		}

		bool drawTitle{true}; // defined by user
		bool drawButtons{true};

		// Optional app-supplied hook for drawing a knob's modulation range.
		//
		// Given a parameter id, report where modulation can push it, in the
		// parameter's OWN value domain — not normalised. KnobSurface normalises the
		// endpoint exactly as it normalises the value, so the two cannot disagree
		// about the mapping. Return false when nothing modulates the parameter, which
		// is the common case and the default when no provider is installed.
		//
		// tslgraphics deliberately knows nothing about mod sources. The provider returns
		// the LOW and HIGH ends of the reachable range in the parameter's own domain and
		// the arc is drawn across that band, so whether a route attenuates, pushes, or
		// swings both ways falls out of where the two ends land — none of it has to be
		// expressed here. A one-directional source returns one end equal to the knob's
		// own value, which draws exactly the single stretch this used to draw.
		//
		// It carried a single endpoint until bipolar LFO routes arrived: those reach
		// both sides of the knob at once, which one float cannot describe.
		using ModRangeFn = bool (*)(tsl::AppState*, int paramId, float& loValue, float& hiValue);
		static ModRangeFn modRangeProvider;
	protected:
		float knob_center_x = 0;
		float knob_center_y = 0;

		float theta_old = 0;
		int old_quadrant = 0;
		TitleView title;
		PlusMinusButton plus;
		PlusMinusButton minus;
        tsl::graphics::InputSystem::InputState pointers;

		void delRecursiveDraw() override;
		void addRecursiveDraw() override ;

		void delRecursiveCB() override ;
		tsl::parameters::Event e{};
		ValueView valueView;
	private:
		KnobSurface surface;
	};
} 
}

