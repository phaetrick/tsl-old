#pragma once
//
// Created by pr on 29.01.22.
//

#ifndef GRAINSTORM_TRACKSETTINGS_H
#define GRAINSTORM_TRACKSETTINGS_H

class TRACK;
#include <view.h>
#include <FloatingView.h>

void showTrackSettings(TRACK* t);
void midiLearningEvent(tsl::AppState*);
void showSettings(tsl::AppState* _appState);

namespace tsl::graphics {
	class LoopPositions : public FloatingView
	{
	public:
		LoopPositions() = default;
		LoopPositions(tsl::AppState* appState);
		static void show(tsl::AppState* _appState, int32_t t);
		static void midiLearningEvent(tsl::AppState* _appState);
	protected:
		int cb(float xpos, float ypos, int action, int) override;
		void renderContent(void* c)override;
		void computeContent(int maxWidth, int maxHeight);



	private:
		float y{}, x{};
		float downX{}, downY{};
		std::atomic<int> hot{ -1 };
		int columns{}, rows{};
		float columnWidth{};
		tsl::AtomicTimer timer;
	};


} // namespace tsl::graphics



#endif //GRAINSTORM_TRACKSETTINGS_H
