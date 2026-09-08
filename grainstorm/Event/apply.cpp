#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "track.h"
#include "waveform.h"

using namespace tsl::parameters;

void tsl::parameters::redrawEvent(tsl::AppState* _appState, Event& e, uint16_t viewid) {
	if (e.flags & tsl::parameters::Event::Redraw || e.flags & tsl::parameters::Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, e, viewid] mutable {
			if (e.flags & tsl::parameters::Event::Redraw) {
				auto v = _STATE->parameters[viewid].view;
				if (v != nullptr && _STATE->active_track.load() == e.trackIndex && v->visible_)v->redraw();

			}
			if (e.flags & tsl::parameters::Event::Info) {
				auto info = static_cast<tsl::graphics::InfoPanel*>(_DATA->views.infopanel);
				info->prepareBuffer(e);
			}
			});
	}
}
void fxPow(tsl::AppState* _appState, Event& e, tsl::parameters::SenderFlags from) {
	auto t = _DATA->tracks[e.trackIndex];

	auto power = e.power.pow == 1.0;
	auto old = _DATA->callbacks_fx_power[e.paramIndex](_DATA->tracks[e.trackIndex], power);
	bool changed = old != power;
	if (changed) {
		if (power) {
			switch (e.subType) {
			case tsl::parameters::powerGrainFx:
				for (int i = 0; i < _STATE->channels; i++) {
					auto& q = t->fx_queue_grain[i];
					int move_result = q.move_element_new(e.paramIndex, e.power.pos);
				}
				e.power.pos = t->fx_queue_grain[0].pos(e.paramIndex);
				break;
			case tsl::parameters::powerFx:
				for (int i = 0; i < _STATE->channels; i++) {
					auto& q = t->fx_queue[i];
					int move_result = q.move_element_new(e.paramIndex, e.power.pos);
					e.power.pos = t->fx_queue[0].pos(e.paramIndex);
				}
				break;
			case tsl::parameters::powerStereoFx:
				t->fx_queue_stereo.move_element_new(e.paramIndex, e.power.pos);
				e.power.pos = t->fx_queue_stereo.pos(e.paramIndex);
				break;

			default:break;
			}
		}
		auto viewid = e.subType == tsl::parameters::EventSubtype::powerGrainFx ? OFFGRAIN : e.subType == tsl::parameters::EventSubtype::powerFx ? OFFFX : e.subType == tsl::parameters::EventSubtype::powerStereoFx ? OFFSTEREOFX : POWERTRACK;
		redrawEvent(_STATE, e, viewid);
		_STATE->onParamChange(e, from);
	}
}

Event Event::apply(tsl::AppState* _appState, tsl::parameters::SenderFlags from
){

	switch (eventType) {
	case LoopFromExt: {
		redrawEvent(_appState, *this, 0);

		auto rec = _DATA->tracks[trackIndex]->filebuffer.load();
		auto oldstate = rec  ? rec->state.load() : nullptr;
		auto e = *this;
		
		if (!oldstate) return e;
		auto newstate = *oldstate;
		switch (paramIndex) {
		case LOOPSTARTFROMEXT: {
			auto newpos = newstate.off_stop.load() * value;
			if (newstate.off_start.exchange(newpos) != newpos) {
				std::vector<Event> vec;
				vec.push_back(tsl::parameters::Event::createEvent(trackIndex, tsl::parameters::Eventtype::Recording, rec->poolHandle, newpos, tsl::parameters::EventSubtype::offStart, 0, 0));
				if (newstate.offset < newpos) {
					newstate.offset.store(newpos);
					vec.push_back(tsl::parameters::Event::createEvent(trackIndex, tsl::parameters::Eventtype::Recording, rec->poolHandle, newpos, tsl::parameters::EventSubtype::offset, 0, 0));
				}
				_DATA->snapShot.add_task([_STATE, v = std::move(vec)] {
					for (auto& e : v) {
						_DATA->snapShot.addEvent(std::move(e));
					}
					});
			}
			else return e;
			break;
		}
			
		case LOOPSTOPFROMEXT: {
			auto newpos = newstate.off_start.load() + (rec->off - newstate.off_start.load()) * value;
			if (newstate.off_stop.exchange(newpos) != newpos) {
				std::vector<Event> vec;
				vec.push_back(tsl::parameters::Event::createEvent(trackIndex, tsl::parameters::Eventtype::Recording, rec->poolHandle, newpos, tsl::parameters::EventSubtype::offStop, 0, 0));

				if (newstate.offset > newpos) {
					newstate.offset.store(newpos);
					vec.push_back(tsl::parameters::Event::createEvent(trackIndex, tsl::parameters::Eventtype::Recording, rec->poolHandle, newpos, tsl::parameters::EventSubtype::offset, 0, 0));
				}
				_DATA->snapShot.add_task([_STATE, v = std::move(vec)] {
					std::lock_guard lk(_DATA->snapShot);
					for (auto& e : v) {
						_DATA->snapShot.addEvent(std::move(e));
					}
					});
			}
			else return e;
			break;
		}

		case LOOPPOSFROMEXT:{
			auto newpos = newstate.off_start.load()  + (newstate.off_stop.load() - newstate.off_start.load()) * value;
			if (newstate.offset.exchange(newpos) != newpos) {
				auto e = tsl::parameters::Event::createEvent(trackIndex, tsl::parameters::Eventtype::Recording, rec->poolHandle, newpos, tsl::parameters::EventSubtype::offset, 0, 0);
				_DATA->snapShot.add_task([_STATE, e = std::move(e)] {
						_DATA->snapShot.addEvent(std::move(e));
					});
			}
			else return e;
			break;
		}
		default: break;
		}
		rec->state.store(std::make_shared<tsl::RecordingState>(newstate), std::memory_order_release);

	}
		break;
	case Preset: {
		redrawEvent(_appState, *this, 0);
		_DATA->snapShot.loadFromPresetEvent(*this);
		auto e = *this;
		auto iv = static_cast<uint32_t>(value);
		e.value = iv % 2 == 0 ? iv - 1 : iv + 1;
		return e;
	}

			   break;
	case Eventtype::FxOrder: {
		redrawEvent(_appState, *this, 0);
		auto track = _DATA->tracks[trackIndex];
		if ((flags & Event::ToWorkerThread) && _STATE->player.isPlaying() && _STATE->params[trackIndex][POWERTRACK].load() == 1.0) {
			track->gainTask.setTargetAutoWait(gaintask::GainDown);
			auto token = _STATE->waitNotify.begin_wait();
			_DATA->toAudioThreadQueue.try_push([=] {
				switch (subType) {
				case powerGrainFx: {
					for (int i = 0; i < _STATE->channels; i++) {
						track->fx_queue_grain[i].move_element_new(paramIndex, fxOrder.newPos);
					}
				}
								 break;
				case powerFx: {
					for (int i = 0; i < _STATE->channels; i++) {
						track->fx_queue[i].move_element_new(paramIndex, fxOrder.newPos);
					}
				}
							break;
				case powerStereoFx: {
					track->fx_queue_stereo.move_element_new(paramIndex, fxOrder.newPos);

				}
				default: break;

				}
				track->gainTask.setTarget(-120, 0);
				_STATE->waitNotify.complete(token);
				});
			_STATE->waitNotify.wait_for_signal(token);
		}
		else {
			switch (subType) {
			case powerGrainFx: {
				for (int i = 0; i < _STATE->channels; i++) {
					track->fx_queue_grain[i].move_element_new(paramIndex, fxOrder.newPos);
				}
			}
							 break;
			case powerFx: {
				for (int i = 0; i < _STATE->channels; i++) {
					track->fx_queue[i].move_element_new(paramIndex, fxOrder.newPos);
				}
			}
						break;
			case powerStereoFx: {
				track->fx_queue_stereo.move_element_new(paramIndex, fxOrder.newPos);

			}
			default: break;

			}
		}

		auto e = *this;
		std::swap(e.fxOrder.oldPos, e.fxOrder.newPos);

		auto powerEvent = Event::createEvent(trackIndex, Power, paramIndex, std::bit_cast<double>(PowerState{ fxOrder.newPos, 1 }), subType, 0, 0);
		_DATA->snapShot.addEvent(powerEvent);

		return e;
	}
						   break;
	case Eventtype::Recording:
		if (poolHandle == INVALID_POOL_HANDLE) {
			redrawEvent(_appState, *this, 0);
			return *this;
		}
		switch (subType) {
		case recordingChange: {
			if (poolHandle == NO_SOUND_PRESENT) {
				if (flags & tsl::parameters::Event::Info) {
					auto info = static_cast<tsl::graphics::InfoPanel*>(_DATA->views.infopanel);
					info->prepareBuffer(*this);
				}
				std::shared_ptr<tsl::Recording> rec{};
				auto old = _DATA->tracks[trackIndex]->loadAudio(rec);
				old.setFlag(Event::History, false);
				old.setFlag(Event::Info, true);
				_DATA->tracks[trackIndex]->waveform->setup(rec);
				return old;
			}
			auto& pd = _STATE->recordings[poolHandle];
			std::lock_guard lk(pd.mutex);
			if (!pd.isValid()) {
				redrawEvent(_appState, *this, 0);
				return *this;
			}
			auto reverseDiff = pd.diff.reverse();
			auto oldrec = _DATA->tracks[trackIndex]->filebuffer.load();
			auto newrec = tsl::Recording::makeFromSwap(_STATE, oldrec, poolHandle);
			redrawEvent(_appState, *this, 0);

			if (newrec) {
				auto old = _DATA->tracks[trackIndex]->loadAudio(newrec, reverseDiff);
				_STATE->WorkerQueue.add_task([_STATE, newrec, tindex = trackIndex] mutable {_DATA->tracks[tindex]->waveform->setup(newrec); });
				old.setFlag(Event::History, false);
				old.setFlag(Event::Info, true);
				return old;
			}
			return *this;
		}

		default: {
			auto rec = _DATA->tracks[trackIndex]->filebuffer.load();
			auto state = rec ? rec->state.load() : nullptr;
			if (poolHandle == NO_SOUND_PRESENT || !state || rec->poolHandle != poolHandle) {
				redrawEvent(_appState, *this, 0);
				return *this;
			}

			auto e = *this;

			auto makestatenew = [&]() {
				return std::make_shared<RecordingState>(*state);
				};

			switch (subType) {
			case LoopLoad1:
			case LoopLoad2:
			case LoopLoad3:
			case LoopLoad4:
			case LoopLoad5:
			case LoopLoad6:
			case LoopLoad7:
			case LoopLoad8:
			case LoopSave1:
			case LoopSave2:
			case LoopSave3:
			case LoopSave4:
			case LoopSave5:
			case LoopSave6:
			case LoopSave7:
			case LoopSave8: {
				redrawEvent(_appState, *this, 0);

				if (poolHandle < NO_SOUND_PRESENT) {
					auto rec = _DATA->tracks[trackIndex]->filebuffer.load();
					auto state = rec && rec->poolHandle == poolHandle ? rec->state.load() : nullptr;
					auto e = *this;
					if (!state) return e;
					auto& pd = _STATE->recordings[poolHandle];
					std::lock_guard lk(pd.mutex);
					if (pd.isValid() && pd.loadUndoRedos[trackIndex].size() >= (int)value && value > 0) {
						auto& s = pd.loadUndoRedos[trackIndex].at((int)value - 1);
						std::vector<Event> sideEffects;
						switch (subType) {
						case LoopLoad1:
						case LoopLoad2:
						case LoopLoad3:
						case LoopLoad4:
						case LoopLoad5:
						case LoopLoad6:
						case LoopLoad7:
						case LoopLoad8: {
							SavedRecordingState defaultState{};
							defaultState[WaveformZoom] = 1.0;
							defaultState[PlaybackDirection] = 1.0;
							auto oldptr = rec->state.load();

							SavedRecordingState old = oldptr ? SavedRecordingState(*oldptr) : defaultState;

							rec->state.store(std::make_shared<RecordingState>(s), std::memory_order_release);

							if (old[LoopStart] != s[LoopStart])
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, s[LoopStart], EventSubtype::offStart, 0, 0));
							if (old[LoopStop] != s[LoopStop])
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, s[LoopStop], EventSubtype::offStop, 0, 0));
							if (old[ReadPos] != s[ReadPos])
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, s[ReadPos], EventSubtype::offset, 0, 0));
							auto newWs = tsl::parameters::WaveformState{ static_cast<float>(s[WaveformStartPosition]), static_cast<float>(s[WaveformZoom]) };
							auto oldWs = tsl::parameters::WaveformState{ static_cast<float>(old[WaveformStartPosition]), static_cast<float>(old[WaveformZoom]) };
							if (oldWs != newWs)
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, std::bit_cast<double>(newWs), EventSubtype::waveformView, 0, 0));
							if (old[BounceType] != s[BounceType])
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, s[BounceType], EventSubtype::bounceType, 0, 0));
							if (old[PlaybackDirection] != s[PlaybackDirection])
								sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::Recording, poolHandle, s[PlaybackDirection], EventSubtype::playbackDir, 0, 0));

						}
									  break;
						case LoopSave1:
						case LoopSave2:
						case LoopSave3:
						case LoopSave4:
						case LoopSave5:
						case LoopSave6:
						case LoopSave7:
						case LoopSave8: {
							auto slot = subType - LoopSave1;
							auto& old = rec->positions[slot];
							for (int i = 0; i < numSavedPositions; i++) {
								if (s[i] != old[i]) {
									if (i < WaveformStartPosition)
										sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::paramUpdate, LOOPSTART0 + i + slot * WaveformStartPosition, s[i], 0, 0, 0));
									else
										sideEffects.push_back(Event::createEvent(trackIndex, Eventtype::paramUpdate, WAVEFORMPOS01 + i - WaveformStartPosition + slot * 2, s[i], 0, 0, 0));
								}
							}
							old = s;
						}
						default: break;
						}

						if (sideEffects.size() > 0) {
							std::lock_guard lk(_DATA->snapShot);

							for (auto& e : sideEffects)
								_DATA->snapShot.addEvent(std::move(e));
						}
						e.value = value - 1;
						return e;
					}
				}
				return *this;
				break;
			}
			case offsetFromUi:
			case offStartFromUi:
			case offStopFromUi: {
				redrawEvent(_appState, *this, 0);
				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				if (pd.isValid() && pd.waveformUndoRedos[trackIndex].size() >= (int)value && value > 0) {
					auto& s = pd.waveformUndoRedos[trackIndex].at((int)value - 1);
					auto current = makestatenew();
					std::vector<Event> ev;

					switch (subType) {
					case offsetFromUi:
						if (current->offset.exchange(s.offset) != s.offset)
							ev.push_back(createEvent(trackIndex, Eventtype::Recording, poolHandle, s.offset, EventSubtype::offset, 0, 0));
						break;

					case offStopFromUi:
					case offStartFromUi: {
						if (subType == offStopFromUi && current->off_stop.exchange(s.loopBound) != s.loopBound)
							ev.push_back(createEvent(trackIndex, Eventtype::Recording, poolHandle, s.loopBound, EventSubtype::offStop, 0, 0));
						else if (subType == offStartFromUi && current->off_start.exchange(s.loopBound) != s.loopBound)
							ev.push_back(createEvent(trackIndex, Eventtype::Recording, poolHandle, s.loopBound, EventSubtype::offStart, 0, 0));

						if (s.offsetChanged) {
							if (current->offset.exchange(s.offset) != s.offset)
								ev.push_back(createEvent(trackIndex, Eventtype::Recording, poolHandle, s.offset, EventSubtype::offset, 0, 0));
							if (current->playbackDir.exchange(s.playbackDir) != s.playbackDir)
								ev.push_back(createEvent(trackIndex, Eventtype::Recording, poolHandle, s.playbackDir, EventSubtype::playbackDir, 0, 0));
						}
						break;
					}
					default: break;
					}

					rec->state.store(current, std::memory_order_release);
					if (!ev.empty()) {
						std::lock_guard lk(_DATA->snapShot);

						for (auto& e : ev)
							_DATA->snapShot.addEvent(e);
					}
					e.value = value - 1;
					return e;
				}
				break;
			}
			case playbackDir: {
				double newDir = value > 0 ? 1.0 : -1.0;
				auto statenew = makestatenew();
				if ((e.value = statenew->playbackDir.exchange(newDir)) != newDir) {
					rec->state.store(statenew, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			case bounceType: {
				auto statenew = makestatenew();
				if ((e.value = statenew->bounceType.exchange(value)) != value) {
					rec->state.store(statenew, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			case offStop: {
				auto statenew = makestatenew();
				if (statenew->off_stop.exchange(value) != value) {
					rec->state.store(statenew, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			case offStart: {
				auto statenew = makestatenew();
				if (statenew->off_start.exchange(value) != value) {
					rec->state.store(statenew, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			case offset: {
				auto statenew = makestatenew();
				if ((e.value = statenew->offset.exchange(value)) != value) {
					rec->state.store(statenew, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			case waveformView: {
				auto ws = state->waveformState.load();
				if (ws.startPos != waveformState.startPos || ws.zoom != waveformState.zoom) {
					e.waveformState = ws;  // store old for undo
					state->waveformState.store(waveformState, std::memory_order_release);
					redrawEvent(_STATE, *this, 0);
					rec->dorendering.store(true);
					_STATE->onParamChange(*this, from);
				}
				break;
			}
			default: break;
			}

			return e;
		}
		}
		break;


	case Eventtype::SpecialAction: {
		_STATE->params[trackIndex][paramIndex].store(value);
		return *this;
	}

	case Eventtype::Rerender: {

		auto view = _STATE->parameters[paramIndex].view;

		if (getFlag(Event::Redraw) && view != nullptr)
			_STATE->toUiThreadQueue.try_push([_STATE, view, event = *this] mutable {
			if (view->visible_ && _STATE->active_track.load() == event.trackIndex)
				view->addDraw();
				});

		return *this;
	}

	case Eventtype::TextEvent: {

		_STATE->toUiThreadQueue.try_push([_STATE, event = *this] mutable {
			auto info = static_cast<tsl::graphics::InfoPanel*>(_DATA->views.infopanel);
			info->prepareBuffer(event);
			});

		return *this;

	}

	case tsl::parameters::Eventtype::Power: {

		switch (subType) {

		case tsl::parameters::EventSubtype::powerGrainFx:

		case tsl::parameters::EventSubtype::powerFx:

		case tsl::parameters::EventSubtype::powerStereoFx: {

			if ((flags & Event::ToWorkerThread) && _STATE->player.isPlaying()) {
				_DATA->toAudioThreadQueue.try_push([_STATE, e = *this] mutable {
					fxPow(_STATE, e, tsl::parameters::None);
					});
			}
			else {
				fxPow(_STATE, *this, tsl::parameters::FromHistory);
			}

			auto e = *this;
			e.power.pow = this->power.pow == 1 ? 0 : 1;
			return e;


		}

		case tsl::parameters::EventSubtype::powerTrack: {

			if (flags & Event::ToWorkerThread) {
				if (value == 0.0) {
					_DATA->tracks[trackIndex]->gainTask.setTargetAutoWait(tsl::gaintask::GainDown);
				}
				else {
					if (_STATE->player.isPlaying()) {
						_DATA->toAudioThreadQueue.try_push([_STATE, tindex = trackIndex] {
							for (int i = 0; i < _STATE->channels; i++)
								std::memset(_DATA->tracks[tindex]->ringbuffer[i], 0, _STATE->ringSize * sizeof(MYFLOAT));
							_DATA->tracks[tindex]->gainTask.setTargetAutoWait(tsl::gaintask::GainUp);
							});
					}
					else {
						for (int i = 0; i < _STATE->channels; i++)
							std::memset(_DATA->tracks[trackIndex]->ringbuffer[i], 0, _STATE->ringSize * sizeof(MYFLOAT));
						_DATA->tracks[trackIndex]->gainTask.setTargetAutoWait(tsl::gaintask::GainUp);

					}
				}
			}

			bool changed = _STATE->params[trackIndex][POWERTRACK].exchange(value) != value;
			if (changed) {
				auto viewid = POWERTRACK;
				redrawEvent(_STATE, *this, viewid);
				_STATE->onParamChange(*this, from);
			}

			auto e = *this;
			e.value = value == 1.0 ? 0.0 : 1.0;
			return e;

		}

		case tsl::parameters::EventSubtype::bypassGrainFx:

		case tsl::parameters::EventSubtype::bypassFx:

		case tsl::parameters::EventSubtype::bypassStereoFx:

		case tsl::parameters::EventSubtype::followerPower: {

			bool changed = false;

			if (subType == tsl::parameters::EventSubtype::followerPower) {
				auto& fol = _STATE->followerMap[trackIndex].at(paramIndex);
				auto ret = fol.envpower.exchange(value);
				changed = ret != value;
			}
			else {
				if (_DATA->callbacks_fx_power[paramIndex] == nullptr) changed = false;
				else {
					auto ret = _DATA->tracks[trackIndex]->bypass[paramIndex].exchange(value);
					changed = ret != value;
				}
			}

			if (changed) {
				auto viewid = subType == tsl::parameters::EventSubtype::bypassGrainFx ? BYPASSGRAINFX : subType == tsl::parameters::EventSubtype::bypassFx ? BYPASSFX : subType == tsl::parameters::EventSubtype::bypassStereoFx ? BYPASSSTEREOFX : FOLLOWERDESTPOWER;
				redrawEvent(_STATE, *this, viewid);
				_STATE->onParamChange(*this, from);
			}
			auto e = *this;
			if (changed) e.value = value == 1.0 ? 0.0 : 1.0;
			return e;
		}

		default: break;

		}

	}

	case lfoDest: {

		auto lfo = _DATA->tracks[trackIndex]->lfos[subType];
		auto& atomicLfo = _DATA->tracks[trackIndex]->lfo[paramIndex];
		MYFLOAT ret = 1.0;
		if (value == 1.0) {
			LFO* expected = nullptr;
			if (!atomicLfo.compare_exchange_strong(expected, lfo)) {
				// expected now holds the current value
				// different lfo already assigned
				if (expected != lfo && flags & Event::Redraw) {
					char s[200];
					snprintf(s, sizeof(s), "%s already has %s as target.", expected->name,
						lfoItems[findIndexFloat(lfoValues, paramIndex)].data());
					showToast(_STATE, s);
				}
			}
			else ret = 0.0;
		}
		else {
			LFO* expected = lfo;
			if (!atomicLfo.compare_exchange_strong(expected, nullptr)) {
			}
			else ret = 1.0;
		}
		if (ret != value) {
			redrawEvent(_STATE, *this, LFODESTPOWER);
			_STATE->onParamChange(*this, from);
		}
		auto e = *this;
		e.value = ret;
		return e;
	}

	case paramUpdate:
		switch (subType) {
		case EventSubtype::sequencerState: {
			auto& s = _DATA->tracks[trackIndex]->grainsequencer;
			auto cur = s.getState();
			if (cur.sequencerState != this->sequencerState) {
				s.setState(*this);
				_STATE->onParamChange(*this, from);
			}
			auto e = *this;
			e.sequencerState = cur.sequencerState;
			return e;
		}

		case EventSubtype::grainGenState: {
			auto& s = _DATA->tracks[trackIndex]->graingen;
			auto cur = s.getState();
			if (cur.graingenState != this->graingenState) {
				s.setState(*this);
				_STATE->onParamChange(*this, from);
			}
			auto e = *this;
			e.graingenState = cur.graingenState;
			return e;
		}
		}


	default:

		if (flags & DoOnChange) {
			auto e = *this;

			e.value = onChange(_STATE);
			return e;
		}

		auto& param = _STATE->parameters[paramIndex];
		std::atomic<MYFLOAT>* ref{};
		uint16_t drawId{};
		switch (eventType) {

#ifdef GRAINSTORM

		case lfoMin:
			ref = &_STATE->controls[trackIndex][paramIndex].lfo_min;
			drawId = LFO1BOUNDA;
			break;

		case lfoMax:
			ref = &_STATE->controls[trackIndex][paramIndex].lfo_max;
			drawId = LFO1BOUNDB;
			break;

		case Follower:

			switch (subType) {

			case followerMin:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).min;
				drawId = FOLLOWERBOUNDA;
				break;

			case followerMax:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).max;
				drawId = FOLLOWERBOUNDB;
				break;

			case followerGain:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).gain;
				drawId = FOLLOWERGAIN;
				break;

			case followerAtt:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).att;
				drawId = FOLLOWERATT;
				break;

			case followerDec:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).rel;
				drawId = FOLLOWERREL;
				break;

			case followerSidechain:
				ref = &_STATE->followerMap[trackIndex].at(paramIndex).source;
				drawId = FOLLOWERSRC;
				break;

			default:
				break;

			}

			break;
#endif

		default: {

			drawId = paramIndex;
			ref = &_STATE->params[trackIndex][paramIndex];
			break;

		}
		}
		auto e = *this;
		e.value = ref->exchange(value);

		if (e.value != value) {
			_STATE->onParamChange(*this, from);
			redrawEvent(_STATE, *this, drawId);
			if (_STATE->parameters[paramIndex].flags & Param::HasAfterChange)
				afterChange(_STATE);
		}

		return e;
	}

}
