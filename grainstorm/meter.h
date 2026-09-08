#pragma once
#ifndef METER_H
#define METER_H
#include "view.h"
namespace tsl::graphics {
	class Meter : public View {
	public:
		Meter(tsl::AppState* appState, float _scalefactor, int32_t _aspect, int _align) : View(appState, _scalefactor, _aspect, _align, 10, true, "Meter") {};
		void render(void* ctx) override;
	private:
		float leftsaved{ -135 };
		float peakleft{ -135 };
		int32_t dir{ 1 };
		float rightsaved{ -135 };
		float peakright{ -135 };
		int32_t dirright{ 1 };
	};
}


#endif
