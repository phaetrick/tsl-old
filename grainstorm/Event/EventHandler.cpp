#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "track.h"
#include "waveform.h"

using namespace tsl::parameters;


Event::~Event() {
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange && poolHandle < NO_SOUND_PRESENT) {
		auto& r = _STATE->recordings[poolHandle];
		std::lock_guard lk(r.mutex);
		r.decRefCount();
	}
}

Event::Event(const Event& other) {

	std::memcpy(this, &other, sizeof(Event));
	if (eventType == Recording && subType == EventSubtype::recordingChange && poolHandle < NO_SOUND_PRESENT) {
		auto& r = _STATE->recordings[poolHandle];
		std::lock_guard lk(r.mutex);
		r.incRefCount();
	}
}

Event& Event::operator=(const Event& other) {
	if (this == &other) return *this;
	// decrement old before overwriting
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange && poolHandle < NO_SOUND_PRESENT) {
		auto& r = _STATE->recordings[poolHandle];
		std::lock_guard lk(r.mutex);
		r.decRefCount();
	}
	std::memcpy(this, &other, sizeof(Event));
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange && poolHandle < NO_SOUND_PRESENT) {
		auto& r = _STATE->recordings[poolHandle];
		std::lock_guard lk(r.mutex);
		r.incRefCount();
	}
	return *this;
}

Event::Event(Event&& other) noexcept {
	std::memcpy(this, &other, sizeof(Event));
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange)
		other.poolHandle = NO_SOUND_PRESENT;
}

Event& Event::operator=(Event&& other) noexcept {
	if (this == &other) return *this;
	// decrement old
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange && poolHandle < NO_SOUND_PRESENT) {
		auto& r = _STATE->recordings[poolHandle];
		std::lock_guard lk(r.mutex);
		r.decRefCount();
	}
	std::memcpy(this, &other, sizeof(Event));
	if (eventType == Eventtype::Recording && subType == EventSubtype::recordingChange)
		other.poolHandle = NO_SOUND_PRESENT;
	return *this;
}




