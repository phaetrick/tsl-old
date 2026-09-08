#include "History.h"
#include "grainstorm.h"
#include "app.h"
#if defined __ANDROID__
#include "DecoderAndroid.h"
#elif defined _WIN32 || defined __APPLE__
#include "DecoderWindows.h"
#else
#endif
#include "waveform.h"
#include "button.h"

#include "infopanel.h"
#include "selector.h"

using namespace tsl::parameters;

constexpr auto minSnapSize = next_pow_2(512 * sizeof(Event) + 4 * 128);

constexpr int  tsl::parameters::supposeSnapSize(int bufsize) {
	return bufsize > minSnapSize ? next_pow_2(bufsize) : minSnapSize;
};

static size_t headerSize(unsigned char* snap) {
	int hdr;
	std::memcpy(&hdr, snap, sizeof(int));
	return hdr;
}

bool validateSnapshot(const std::vector<unsigned char>& snap) {
	if (snap.size() < sizeof(int))
		return false;

	int hdr;
	std::memcpy(&hdr, snap.data(), sizeof(int));

	// header must be within bounds
	if (hdr < (int)(sizeof(int) * 5) || hdr >(int)snap.size())
		return false;

	// walk names safely
	size_t off = sizeof(int);
	for (int i = 0; i < 4; i++) {
		if (off + sizeof(int) > snap.size())
			return false;

		int n;
		std::memcpy(&n, snap.data() + off, sizeof(int));

		if (n < 0)
			return false;

		if (off + sizeof(int) + n > snap.size())
			return false;

		off += sizeof(int) + n;
	}

	// header must match computed name section
	if (off != (size_t)hdr)
		return false;

	// events section must be aligned to Event size
	size_t evBytes = snap.size() - hdr;
	if (evBytes % sizeof(Event) != 0)
		return false;

	return true;
}



void debugPrint2(tsl::AppState* _appState, unsigned char* snapshot, int size) {
	LOGI("=== Snapshot debug ===");
	LOGI("total size: %zu  header size: %zu", size, headerSize(snapshot));

	size_t namePos = sizeof(int);
	for (int i = 0; i < 4; i++) {
		if (namePos + sizeof(int) > size) { LOGI("track %d: out of bounds", i); break; }
		int n;
		std::memcpy(&n, snapshot + namePos, sizeof(int));

		if (n > 0 && namePos + sizeof(int) + n <= size) {
			std::string name(reinterpret_cast<const char*>(snapshot + namePos + sizeof(int)), n);
			LOGI("track %d: '%s' (%d chars)", i, name.c_str(), n);
		}
		else {
			LOGI("track %d: empty", i);
		}
		namePos += sizeof(int) + n;
	}

	size_t hdr = headerSize(snapshot);
	size_t evBytes = size - hdr;
	size_t evCount = evBytes / sizeof(Event);
	LOGI("events: %zu", evCount);

	for (size_t i = 0; i < evCount; i++) {
		Event e;
		std::memcpy(&e, snapshot + hdr + i * sizeof(Event), sizeof(Event));
		char buf[100]{};
		int pos = 0;
		e.toString(_STATE, pos, buf, 100, true);
		LOGI("  [%zu] %s %s", i, _DATA->tracks[e.trackIndex]->name, buf);
	}
	LOGI("=== end snapshot ===");
}


#ifdef PLUGIN_MODE
void Snapshot::clear(int trackIndex) {
	std::lock_guard lk(*this);

	auto it = events_.begin();
	std::vector<Event> events;
	while (it != events_.end()) {
		if (it->trackIndex == trackIndex) {
			if (it->getPluginIndex(_STATE) >= 0) {
				auto e = std::move(*it);
				e.value = e.getDefaultValue(_STATE);
				e.toNormalized(_STATE);
				events.push_back(e);
			}
			it = events_.erase(it);
		}
		else ++it;
	}
	names_[trackIndex].clear();
	// Direct is a thread-safe queue push delivered in the plugin's OnIdle —
	// no UI-thread detour, so this also reaches the host with the editor closed.
	if (_STATE->InformHostOfParamChangeDirect)
		for (auto& e : events)
			_STATE->InformHostOfParamChangeDirect(e);
};

void Snapshot::clear() {
	std::lock_guard lk(*this);
	auto it = events_.begin();
	std::vector<Event> events;
	for (auto& e : events_) {
		if (e.getPluginIndex(_STATE) >= 0) {
			e.value = e.getDefaultValue(_STATE);
			e.toNormalized(_STATE);
			events.push_back(std::move(e));

		}
	}
	
	events_.clear();
	events_.reserve(1024);

	for (auto& n : names_)
		n.clear();
	if(_STATE->InformHostOfParamChangeDirect)
		for (auto& e : events)
			_STATE->InformHostOfParamChangeDirect(e);
};


#else
void Snapshot::clear(int trackIndex) {
	std::lock_guard lk(*this);
	auto it = events_.begin();
	while (it != events_.end()) {
		if (it->trackIndex == trackIndex) {
			it = events_.erase(it);
		}
		else ++it;
	}
	names_[trackIndex].clear();
};

void Snapshot::clear() {
	std::lock_guard lk(*this);
	events_.clear();
	events_.reserve(1024);
	for (auto& name : names_)
		name.clear();
}
#endif

void Snapshot::deserialize(unsigned char* in, int size, std::vector<Event>& events, std::string(&names)[4]) {
	// parse names
	//debugPrint2(in, size);
	size_t namePos = sizeof(int);
	for (int i = 0; i < 4; i++) {
		int len; std::memcpy(&len, in + namePos, sizeof(int)); namePos += sizeof(int);
		names[i] = len > 0 ? std::string((char*)(in + namePos), len) : "";
		namePos += len;
	}
	// parse events
	events.clear();
	size_t pos = headerSize(in);
	while (pos + sizeof(Event) <= size) {
		Event ev; std::memcpy(&ev, in + pos, sizeof(Event)); pos += sizeof(Event);
		if (ev.trackIndex < 4)
			events.push_back(ev);
	}
}
void Snapshot::deserialize(unsigned char* in, int size, std::vector<Event>& events, std::string& name, int trackIndex) {
	// parse names
	size_t namePos = sizeof(int);
	for (int i = 0; i < 4; i++) {
		int len; std::memcpy(&len, in + namePos, sizeof(int)); namePos += sizeof(int);
		if (i == trackIndex) {
			name = len > 0 ? std::string((char*)(in + namePos), len) : "";
		}
		namePos += len;
	}
	// parse events
	events.clear();
	size_t pos = headerSize(in);
	while (pos + sizeof(Event) <= size) {
		Event ev; std::memcpy(&ev, in + pos, sizeof(Event)); pos += sizeof(Event);
		if (ev.trackIndex == trackIndex)
			events.push_back(ev);
	}
}


void Snapshot::OnPresetLoaded(std::shared_ptr<CurrentSnapShot> oldsnap, std::shared_ptr<CurrentSnapShot> nsnap, std::vector<Event>& recs) {
	
	std::lock_guard lk(*this);

	auto recSize = recs.size();
	if (recSize & 1) {
		LOGE("unexpected odd number of recording events: %zu", recSize);
		return;
	}
	for (int i = 0; i < recSize; i += 2) {
		auto& old = recs[i];
		auto& newRec = recs[i + 1];
		auto tindex = old.trackIndex;
		if (tindex != newRec.trackIndex) {
			LOGE("Track mismatch for old and new recording events.");
			return;
		}
		PresetUndoRedo oldState{};
		deserialize(oldsnap->mem, oldsnap->size, oldState.events, oldState.fileName, tindex);
		oldState.fileEvent = old;
		PresetUndoRedo newState{};
		deserialize(nsnap->mem, nsnap->size, newState.events, newState.fileName, tindex);
		newState.fileEvent = newRec;
		if (oldState.fileName == newState.fileName && oldState.events.size() == newState.events.size()) {
			bool eventsMatch = true;
			for (int i = 0; i < oldState.events.size(); i++) {
				if (oldState.events[i] == newState.events[i] && oldState.events[i].value == newState.events[i].value) {
					continue;
				}
				else {
					eventsMatch = false;
					break;
				}
			}
			if (eventsMatch) {
				LOGE("No changes detected for track %d, skipping undo/redo entry.", tindex);
				continue;
			}
		}
		std::lock_guard<std::recursive_mutex> lock(presetMutex);
		presetUndoRedos[tindex].resize(posPreset[tindex]);
		presetUndoRedos[tindex].push_back(std::move(newState));
		presetUndoRedos[tindex].push_back(std::move(oldState));
		posPreset[tindex] = (int)presetUndoRedos[tindex].size();
		addEvent(Event::createEvent((uint8_t)tindex, Eventtype::Preset, 0, posPreset[tindex], 0, 0, Event::EventFlags::History | Event::EventFlags::ToWorkerThread));
	}
}

void Snapshot::loadFromPresetEvent(Event& event) {
	auto tindex = event.trackIndex;
	auto track = _DATA->tracks[tindex];


	std::lock_guard lk(*this);
	auto pos = (int)event.value;
	std::lock_guard<std::recursive_mutex> lock(presetMutex);
	if (pos > 0 && pos <= (int)presetUndoRedos[tindex].size()) {
		_DATA->isRestoringState.store(true);
		auto hasGainTask = event.flags & Event::ToWorkerThread && _STATE->player.isPlaying() && _STATE->params[tindex][POWERTRACK] == 1.0;
		if (hasGainTask) {
			track->gainTask.setTargetAutoWait(tsl::gaintask::GainDown);
			_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
			auto token = _STATE->waitNotify.begin_wait();
			_DATA->toAudioThreadQueue.try_push([this, track, token] {
				track->disabled = true;
				_STATE->waitNotify.complete(token);
				});
			_STATE->waitNotify.wait_for_signal(token);
		}




		while (auto e = queue.try_pop()) {
			e->flags &= ~tsl::parameters::Event::History;
			addEvent(std::move(*e));
		}
		clear(tindex);
		publish_();
		auto& state = presetUndoRedos[tindex][pos - 1];
		auto current = track->filebuffer.load();
		bool sameFile = current && current->numEdits == 0
			&& current->poolHandle == state.fileEvent.poolHandle
			&& state.fileEvent.poolHandle != tsl::NO_SOUND_PRESENT;
		tsl::parameters::WaveformState currentWaveformState{};
		if (sameFile) {
			auto recState = current->state.load();
			if (recState) currentWaveformState = recState->waveformState.load();
		}
		track->reset();
		track->setDefaults();

		auto isPower = [](Event& ev) {
			return ev.eventType == Power && (ev.subType == powerGrainFx || ev.subType == powerFx || ev.subType == powerStereoFx);
			};
		std::vector<Event> powerEvents;
		for (auto& e : state.events) {
			e.setFlag(Event::ToWorkerThread, false);
			e.setFlag(Event::ToAudioThread, false);
			e.setFlag(Event::Info, false);
			e.setFlag(Event::History, false);
			e.setFlag(Event::Redraw, false);
			if (isPower(e)) {
				powerEvents.push_back(e);
			}
			else  {
				auto ne = e;
				ne.fromNormalized(_STATE);
				ne.apply(_STATE, tsl::parameters::FromHistory);
			}
		}
		std::sort(powerEvents.begin(), powerEvents.begin() + powerEvents.size(), [](Event& a, Event& b) {return a.power.pos > b.power.pos; });

		for (auto& e : powerEvents)e.apply(_STATE, tsl::parameters::FromHistory);
		if (!sameFile) {
			auto e = state.fileEvent;
			e.setFlag(Event::ToAudioThread, false);
			e.setFlag(Event::Info, false);
			e.setFlag(Event::History, false);
			e.setFlag(Event::Redraw, false);
			e.apply(_STATE, tsl::parameters::FromHistory);
		} else {
			auto recState = current->state.load();
			if (track->waveform && recState && recState->waveformState.load() != currentWaveformState)
				track->waveform->setup(current);
		}
		if (hasGainTask) {
			auto token = _STATE->waitNotify.begin_wait();
			_DATA->toAudioThreadQueue.try_push([this, track, token] {
				track->disabled = false;
				for (int i = 0; i < _STATE->channels; i++)
					std::memset(track->ringbuffer[i], 0, _STATE->ringSize * sizeof(MYFLOAT));
				track->gainTask.setTargetAutoWait(tsl::gaintask::GainUp);
				_STATE->waitNotify.complete(token);
				});
			_STATE->waitNotify.wait_for_signal(token);
		}

		_STATE->toUiThreadQueue.try_push([this, tindex] {
			if (tindex == _STATE->active_track.load())
				tsl::graphics::TrackButton::func(_appState, tindex);
			});
		// See Snapshot::apply — must not depend on the UI queue draining.
		_DATA->isRestoringState.store(false);
	}
}



std::shared_ptr<CurrentSnapShot> Snapshot::get(int trackIndex) {
	std::lock_guard lk(*this);
	// serialize events_ + names_ into CurrentSnapShot here
	size_t nameBytes = sizeof(int); // header size int
	for (int i = 0; i < 4; i++) {
		if (i == trackIndex)
			nameBytes += sizeof(int) + names_[i].size();
		else
			nameBytes += sizeof(int);
	}

	size_t eventBytes = 0;
	for (auto& e : events_)if (e.trackIndex == trackIndex)eventBytes += sizeof(Event);
	size_t total = nameBytes + eventBytes;

	auto snap = std::make_shared<CurrentSnapShot>(_STATE, total);
	if (!snap->mem) return  nullptr;

	uint8_t* p = snap->mem;

	// write header size
	int hdr = (int)nameBytes;
	std::memcpy(p, &hdr, sizeof(int)); p += sizeof(int);

	// write names
	for (int i = 0; i < 4; i++) {
		int n = i == trackIndex ? (int)names_[i].size() : 0;
		std::memcpy(p, &n, sizeof(int)); p += sizeof(int);
		if (n > 0) {
			std::memcpy(p, names_[i].data(), n); p += n;
		}
	}

	// write events

	for (auto& e : events_) {
		if (e.trackIndex == trackIndex) {
			std::memcpy(p, &e, sizeof(Event));
			p += sizeof(Event);
		}
	}
};

CurrentSnapShot::~CurrentSnapShot() {
	if (mem != nullptr) {
		_STATE->pool.release(mem);
	}
}

CurrentSnapShot::CurrentSnapShot(tsl::AppState* appState, int s)
	: _appState(appState), size(s), mem(nullptr) {
	mem = _STATE->pool.acquire<unsigned char>(supposeSnapSize(s));
}

void Snapshot::publish_() {
	std::lock_guard lk(*this);

	// serialize events_ + names_ into CurrentSnapShot here
	size_t nameBytes = sizeof(int); // header size int
	for (int i = 0; i < 4; i++)
		nameBytes += sizeof(int) + names_[i].size();

	size_t total = nameBytes + events_.size() * sizeof(Event);

	auto snap = std::make_shared<CurrentSnapShot>(_STATE, total);
	if (!snap->mem) return;

	uint8_t* p = snap->mem;

	// write header size
	int hdr = (int)nameBytes;
	std::memcpy(p, &hdr, sizeof(int)); p += sizeof(int);

	// write names
	for (int i = 0; i < 4; i++) {
		int n = (int)names_[i].size();
		std::memcpy(p, &n, sizeof(int)); p += sizeof(int);
		if (n > 0) {
			std::memcpy(p, names_[i].data(), n); p += n;
		}
	}

	// write events
	if (!events_.empty()) {
		std::memcpy(p, events_.data(), events_.size() * sizeof(Event));
		auto* ep = reinterpret_cast<Event*>(p);
		for (size_t i = 0; i < events_.size(); ++i) {
			ep[i].toNormalized(_STATE);
			// flags and groupId are live bookkeeping (Redraw/Info/History/FromDaw,
			// undo grouping); they describe how an event was delivered, not what
			// the state is. Every reader of a serialized snapshot overwrites them
			// -- Snapshot::apply assigns flags outright, loadFromPresetEvent
			// clears each one -- so carrying them made save -> restore -> save
			// produce different bytes for identical state, which reads to a host
			// as "the project changed the moment you opened it".
			ep[i].flags = 0;
			ep[i].groupId = 0;
		}
	}

	currentSnapShot.store(snap, std::memory_order_release);
}

Snapshot::Snapshot(tsl::AppState* appState) : tsl::graphics::FloatingView{ appState }{
	_appState = appState;
	_STATE->onParamChange = [this](Event& e, tsl::parameters::SenderFlags from) {
		if(from == SenderFlags::FromHistory){
			addEvent(e);
#ifdef PLUGIN_MODE
			if (!(e.flags & tsl::parameters::Event::FromDaw) && _STATE->InformHostOfParamChange) {
				auto ne = e;
				ne.toNormalized(_STATE);
				_STATE->InformHostOfParamChange(ne, from);
			}
#endif
		}
		else {
			add_task([this, e, from] {
				addEvent(e);
#ifdef PLUGIN_MODE
				if (!(e.flags & tsl::parameters::Event::FromDaw) && _STATE->InformHostOfParamChange) {
					auto ne = e;
					ne.toNormalized(_STATE);
					_STATE->InformHostOfParamChange(ne, from);
				}
#endif

				});
		}

		};
	historyTimer.reset();
	clear();
}



bool Snapshot::load(unsigned char* in, int size, bool fromDaw) {

	add_task([this, in, size, fromDaw]() mutable {
		// reset

		// deserialize into logical state
		deserialize(in, size, events_, names_);
		auto cs = std::make_shared<CurrentSnapShot>(_STATE, in, size);
		apply(_STATE, cs, events_, names_, fromDaw);

		});
	return true;
}


void Snapshot::apply(tsl::AppState* _appState, std::shared_ptr<CurrentSnapShot> cs, std::vector<Event>& events, std::string(&names)[4], bool fromDaw) {
	std::lock_guard lk(_DATA->snapShot);
	_DATA->isRestoringState.store(true);

	double offStart[4]{}, offStop[4]{}, offset[4]{}, off[4]{}, bounceType[4]{}, playDir[4]{ 1,1,1,1 };
	WaveformState waveformView[4]{};

	std::vector<std::shared_ptr<tsl::Recording>> recs;

	for (auto& ev : events) {
		if (ev.eventType == Eventtype::Recording) {
			if (ev.subType == EventSubtype::off)       off[ev.trackIndex] = ev.value;
			else if (ev.subType == EventSubtype::offStart)  offStart[ev.trackIndex] = ev.value;
			else if (ev.subType == EventSubtype::offStop)   offStop[ev.trackIndex] = ev.value;
			else if (ev.subType == EventSubtype::offset)    offset[ev.trackIndex] = ev.value;
			else if (ev.subType == EventSubtype::waveformView) waveformView[ev.trackIndex] = ev.waveformState;
			else if (ev.subType == EventSubtype::bounceType) bounceType[ev.trackIndex] = ev.value;
			else if (ev.subType == EventSubtype::playbackDir) playDir[ev.trackIndex] = ev.value;
		}
	}
	while (auto e = _DATA->snapShot.queue.try_pop()) {
		e->flags &= ~tsl::parameters::Event::History;
		_DATA->snapShot.addEvent(*e);
	}

	auto oldSnap = _DATA->snapShot.get();
	std::vector<Event> oldRecs{};
	tsl::parameters::WaveformState prevWaveformState[4]{};
	bool checkWaveform[4]{};
	bool reregisterRecording[4]{};
	for (int i = 0; i < 4; i++) {
		auto track = _DATA->tracks[i];
		oldRecs.push_back(std::move(track->currentAudioToEvent()));
		if (names[i].empty() || off[i] == 0 || names[i] == presetHasAudio) {
			if (track->filebuffer.load()) reregisterRecording[i] = true;
			continue;
		}
		if (off[i] >= _STATE->sr * 420) off[i] = _STATE->sr * 420;
		auto current = track->filebuffer.load();
		if (current && current->fileName == names[i] && current->off == off[i]) {
			auto recState = current->state.load();
			if (recState) { prevWaveformState[i] = recState->waveformState.load(); checkWaveform[i] = true; }
			continue;
		}
		auto rec = dec(track, names[i].c_str(), off[i]);
		if (rec && rec->off > 0) {
			auto state = rec->state.load();
			if (state) {
				state->waveformState = waveformView[i];
				state->off_start = offStart[i];
				state->off_stop = offStop[i];
				state->offset = offset[i];
				state->bounceType = bounceType[i];
				state->playbackDir = playDir[i];
			}
			rec->trackIndex = i;
			recs.push_back(std::move(rec));
		}
	}
	std::vector<TRACK*> gainTracks;

	if (_STATE->player.isPlaying()) {
		for (auto t : _DATA->tracks)if (_STATE->params[t->index][POWERTRACK].load() == 1.0) {
			gainTracks.push_back(t);
			t->gainTask.setTarget(tsl::gaintask::GainDown, false);
		}
	}
	if (!gainTracks.empty()) {

		_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
		// The old wake_thread here fired after the wait had already returned —
		// it had no waiter and only poisoned the slot.
		_DATA->toAudioThreadQueue.try_push([_STATE, gainTracks] {
			for (auto t : gainTracks)t->disabled = true;
			});
	}

	auto isPower = [](Event& ev) {
		return ev.eventType == Power && (ev.subType == powerGrainFx || ev.subType == powerFx || ev.subType == powerStereoFx);
		};
	auto in = cs->mem;
	auto size = cs->size;
	for (auto t : _DATA->tracks) { t->reset(); t->setDefaults(); }
	_DATA->snapShot.clear();
	for (int i = 0; i < 4; i++)
		if (reregisterRecording[i]) _DATA->snapShot.addRecording(i, _DATA->tracks[i]->filebuffer.load());
	// parse events
	size_t pos = headerSize(in);
	std::vector<Event> powerEvents;
	while (pos + sizeof(Event) <= size) {
		Event ev; std::memcpy(&ev, in + pos, sizeof(Event)); pos += sizeof(Event);
		ev.flags = fromDaw ? Event::FromDaw : 0;
		if (isPower(ev)) {
			powerEvents.push_back(ev);
		}
		else if (ev.eventType != Eventtype::Recording) {
			auto ne = ev;
			ne.fromNormalized(_STATE);
			ne.apply(_STATE, tsl::parameters::FromHistory);
		}
	}
	std::sort(powerEvents.begin(), powerEvents.begin() + powerEvents.size(), [](Event& a, Event& b) {return a.power.pos > b.power.pos; });

	for (auto& e : powerEvents) {
		if (fromDaw) e.setFlag(Event::FromDaw, true);
		e.apply(_STATE, tsl::parameters::FromHistory);
	}

	for (auto& rec : recs) {
		if (rec) {
			for (int i = 0; i < 8; i++) {
				rec->positions[i][tsl::LoopStart] = _STATE->params[rec->trackIndex][LOOPSTART0 + i * 5].load();
				rec->positions[i][tsl::LoopStop] = _STATE->params[rec->trackIndex][LOOPSTOP0 + i * 5].load();
				rec->positions[i][tsl::ReadPos] = _STATE->params[rec->trackIndex][LOOPPOS0 + i * 5].load();
				rec->positions[i][tsl::BounceType] = _STATE->params[rec->trackIndex][LOOPTYPE0 + i * 5].load();
				rec->positions[i][tsl::PlaybackDirection] = _STATE->params[rec->trackIndex][LOOPDIR0 + i * 5].load();
				rec->positions[i][tsl::WaveformStartPosition] = _STATE->params[rec->trackIndex][WAVEFORMPOS01 + i * 2].load();
				rec->positions[i][tsl::WaveformZoom] = _STATE->params[rec->trackIndex][WAVEFORMZOOM01 + i * 2].load();
			}
			auto newLength = rec->off;
			auto oldLength = off[rec->trackIndex];
			if (newLength != oldLength) {
				rec->off = oldLength; //adjust assumes off isnt updated yet
				rec->adjustPositions(oldLength, newLength - oldLength);
				rec->off = newLength;
			}
			auto track = _DATA->tracks[rec->trackIndex];
			track->loadAudio(rec);
			track->waveform->setup(rec);
		}
	}

	for (int i = 0; i < 4; i++) {
		if (!checkWaveform[i]) continue;
		auto track = _DATA->tracks[i];
		auto current = track->filebuffer.load();
		if (!current || !track->waveform) continue;
		auto recState = current->state.load();
		if (recState && prevWaveformState[i] != waveformView[i]) {
			recState->waveformState.store(waveformView[i]);
			track->waveform->setup(current);
		}
	}

	_DATA->snapShot.publish_();

	// A host-initiated restore (project load / host preset switch) must not be
	// undoable: skip the preset undo/redo entry. Only user preset loads get one.
	if (!fromDaw) {
		std::vector<tsl::parameters::Event> fileEvents;
		for (int i = 0; i < 4; i++) {
			fileEvents.push_back(std::move(oldRecs[i]));
			fileEvents.push_back(std::move(_DATA->tracks[i]->currentAudioToEvent()));
		}

		auto newSnap = _DATA->snapShot.get();
		_DATA->snapShot.OnPresetLoaded(oldSnap, newSnap, fileEvents);
	}

	if (!gainTracks.empty()) {

		auto token = _STATE->waitNotify.begin_wait();
		_DATA->toAudioThreadQueue.try_push([_STATE, token, gainTracks] {
			for (auto t : gainTracks) {
				t->disabled = false;
				t->gainTask.setTarget(tsl::gaintask::GainUp, false);
			}
			_STATE->waitNotify.complete(token);
			});
		_STATE->waitNotify.wait_for_signal(token, tsl::gainTaskFadeMs);
	}

	_STATE->toUiThreadQueue.try_push([_STATE] {
		tsl::graphics::TrackButton::func(_appState, _STATE->active_track.load());
		});
	// Clear synchronously: the UI queue only drains while the editor renders, so a
	// restore with the editor closed would otherwise leave the flag stuck true —
	// host automation ignored and SerializeState frozen on the old snapshot.
	_DATA->isRestoringState.store(false);
}

std::shared_ptr<CurrentSnapShot> Snapshot::get() {
	if (_DATA->isRestoringState.load()) {
		return currentSnapShot.load(std::memory_order_acquire);
	}
	std::lock_guard lk(*this);
	publish_();
	return currentSnapShot.load(std::memory_order_acquire);
}

void Snapshot::getEvents(std::vector<Event>& events, int tindex) {
	// Must be a NAMED guard. `std::lock_guard(*this);` is an unnamed temporary:
	// it locked and unlocked on the same line, so this walked events_ with no
	// lock at all while another thread could erase/clear/reserve it out from
	// under the range-for. Save Project reaches here once per track.
	std::lock_guard lk(*this);
	for (auto& e : events_) { if (e.trackIndex == tindex)events.push_back(e); }
}

