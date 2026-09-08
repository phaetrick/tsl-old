#include <vector>
#include <grainstorm.h>
#include <tools/AtomicSharedPtr.h>
#include "audio/Recording.h"

using namespace tsl::parameters;

void tsl::Recording::loadPositions(int slot) {
	auto s = state.load();
	if (!s)return;
	SavedRecordingState sold(*s);

	std::vector<tsl::parameters::Event> vec1;

	auto tmp = std::make_shared<RecordingState>(*s);

	if (tmp->off_start.exchange(positions[slot][LoopStart]) != positions[slot][LoopStart]) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, positions[slot][LoopStart], EventSubtype::offStart, 0, 0));
	}

	if (tmp->off_stop.exchange(positions[slot][LoopStop]) != positions[slot][LoopStop]) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, positions[slot][LoopStop], EventSubtype::offStop, 0, 0));
	}

	if (tmp->offset.exchange(positions[slot][ReadPos]) != positions[slot][ReadPos]) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, positions[slot][ReadPos], EventSubtype::offset, 0, 0));
	}

	auto newWs = tsl::parameters::WaveformState{
		static_cast<float>(positions[slot][WaveformStartPosition]),
		static_cast<float>(positions[slot][WaveformZoom])
	};
	if (tmp->waveformState.exchange(newWs) != newWs) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, std::bit_cast<double>(newWs), EventSubtype::waveformView, 0, 0));
	}
	auto bt = static_cast<int>(positions[slot][BounceType]);

	if (tmp->bounceType.exchange(bt) != bt) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, bt, EventSubtype::bounceType, 0, 0));
	}
	if (tmp->playbackDir.exchange(positions[slot][PlaybackDirection]) != positions[slot][PlaybackDirection]) {
		vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, positions[slot][PlaybackDirection], EventSubtype::playbackDir, 0, 0));
	}

	if (vec1.size() > 0) {

		if (poolHandle < NO_SOUND_PRESENT) {
			auto& pd = _STATE->recordings[poolHandle];
			std::lock_guard lk(pd.mutex);
			if (pd.isValid()) {
				pd.loadUndoRedos[trackIndex].resize(pd.posLoad[trackIndex]);
				pd.loadUndoRedos[trackIndex].push_back(SavedRecordingState{ *tmp });
				pd.loadUndoRedos[trackIndex].push_back(sold);
				pd.posLoad[trackIndex] = pd.loadUndoRedos[trackIndex].size();
				vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, pd.posLoad[trackIndex], slot, 0, Event::EventFlags::History));
			}
		}

		state.store(tmp, std::memory_order_release);

		_DATA->snapShot.add_task([this, evUi = std::move(vec1)] mutable {
			std::lock_guard lk(_DATA->snapShot);

			for (auto& e : evUi) {
				_DATA->snapShot.addEvent(std::move(e));
			}
			});
	}
}

void tsl::Recording::savePositions(int slot) {
	auto s = state.load();
	if (!s)return;
	std::vector<tsl::parameters::Event> vec1;

	SavedRecordingState cur(*s);
	SavedRecordingState old(positions[slot]);
	for (int i = 0; i < numSavedPositions; i++) {
		if (cur[i] != old[i]) {
			if (i < WaveformStartPosition)
				vec1.push_back(Event::createEvent(trackIndex, Eventtype::paramUpdate, LOOPSTART0 + i + slot * WaveformStartPosition, cur[i], 0, 0, 0));
			else
				vec1.push_back(Event::createEvent(trackIndex, Eventtype::paramUpdate, WAVEFORMPOS01 + i - WaveformStartPosition + slot * 2, cur[i], 0, 0, 0));
		}
	}

	positions[slot] = cur;

	if (vec1.size() > 0) {
		if (poolHandle < NO_SOUND_PRESENT) {
			auto& pd = _STATE->recordings[poolHandle];
			std::lock_guard lk(pd.mutex);
			if (pd.isValid()) {
				pd.loadUndoRedos[trackIndex].resize(pd.posLoad[trackIndex]);
				pd.loadUndoRedos[trackIndex].push_back(cur);
				pd.loadUndoRedos[trackIndex].push_back(old);
				pd.posLoad[trackIndex] = pd.loadUndoRedos[trackIndex].size();
				vec1.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, pd.posLoad[trackIndex], LoopSave1 + slot, 0, Event::EventFlags::History));
			}
		}
		_DATA->snapShot.add_task([this, ev = std::move(vec1)] {
			std::lock_guard lk(_DATA->snapShot);
			for (auto& e : ev) {
				_DATA->snapShot.addEvent(e);
			}
			});
	}
}