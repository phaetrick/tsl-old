//
// Created by pr on 19.10.20.
//

#include <include/core/SkFont.h>
#include "sequencer.h"
#include "track.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "ControlItem.h"
#include "app.h"
#include "view.h"

using namespace tsl::parameters;
using namespace tsl::Syncing;

SyncResult grainsequencer::syncBySamples(double samples, std::vector<Event>& events) {
	auto _appState = track->_appState;
	SyncResult ret{};
	double steps = _STATE->params[parent->index][GRAINSEQSTEPS].load();
	double smplsperstep = samples / steps;
	double beats = _STATE->sr / smplsperstep;

	auto syncfactor = syncFact();

	while (beats > DENSITYMAX)
		beats /= syncfactor;
	while (beats < DENSITYMIN)
		beats *= syncfactor;

	if (_STATE->params[parent->index][DENSITY].exchange(beats) != beats) {
		Event e;
		e.trackIndex = parent->index;
		e.eventType = Eventtype::paramUpdate;
		e.paramIndex = DENSITY;
		e.value = beats;
		events.push_back(e);
		_STATE->toUiThreadQueue.try_push([_STATE, tindex = parent->index, v = _STATE->parameters[DENSITY].view] {
			if (_STATE->active_track.load() == tindex &&
				v->visible_) {
				v->redraw();
			}
			});
		ret |= SyncFlags::Ok;
		return ret;

	};

	ret |= SyncFlags::UnChanged;
	return ret;
}
/*
_STATE->params[track->index][ARP_CYCLESTEP].store(arpstep[1] = arpstep[0]);
_STATE->params[track->index][ARP_CYCLEDIR].store(arpcycledir[1] = arpcycledir[0]);
_STATE->params[track->index][SILENCECOUNT] = silencecount[0];
_STATE->params[track->index][GRAINSCOUNT] = grainscount[0];
*/
SyncResult grainsequencer::control(uint16_t todo, std::vector<Event>& events) {
	auto _appState = track->_appState;
	SyncResult ret = 0;
	switch (todo) {
	case STEPBACK:
		events.push_back(getState());
		_currentnote[0] = 0;
		grainscount[0] = grainscount[1] = grainstocompute[0];
		silencecount[0] = silencecount[1] = 0;
		arpstep[0] = arpstep[1] = 0;
		arpcycledir[0] = arpcycledir[1] = 1;
		events.push_back(track->graingen.getState());
		track->graingen.reset();
		ret |= SyncFlags::Ok;
		return ret;
	case STEPFORW:
		events.push_back(getState());
		grainscount[0] = grainscount[1] = grainstocompute[0];
		silencecount[0] = silencecount[1] = 0;
		_currentnote[0] = _stepsold[0] - 1;
		arpstep[0] = arpstep[1] = 0;
		arpcycledir[0] = arpcycledir[1] = 1;
		events.push_back(track->graingen.getState());
		track->graingen.reset();
		ret |= SyncFlags::Ok;
		return ret;
	case SLOWButton:
	case FASTButton: {
		double delay = _STATE->params[parent->index][DENSITY].load();
		// DENSITY is a rate in Hz: slower = fewer steps per second
		if (todo == SLOWButton) delay /= syncFact();
		else delay *= syncFact();
		if (delay < DENSITYMIN) {
			ret |= SyncFlags::MinReached;
			return ret;
		}
		else if (delay > DENSITYMAX) {
			ret |= SyncFlags::MaxReached;

			return ret;
		}
		if (_STATE->params[parent->index][DENSITY].exchange(delay) != delay) {
			Event e;
			e.trackIndex = parent->index;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = DENSITY;
			e.value = delay;
			events.push_back(e);
			_STATE->toUiThreadQueue.try_push([_STATE, tindex = parent->index, v = _STATE->parameters[DENSITY].view] {
				if (_STATE->active_track.load() == tindex &&
					v->visible_) {
					v->redraw();
				}
				});
			ret |= SyncFlags::Ok;
			return ret;
		}
		else {
			ret |= SyncFlags::UnChanged;
			return ret;
		}
	}
	case DIRButton: {
		auto x = _STATE->params[track->index][GRAINSEQDIR].load();
		auto des = x == 1.0 ? 0 : 1.0;
		while (!_STATE->params[track->index][GRAINSEQDIR].compare_exchange_weak(x, des,
			std::memory_order_release, std::memory_order_relaxed));
		Event e;
		e.trackIndex = parent->index;
		e.eventType = Eventtype::paramUpdate;
		e.paramIndex = GRAINSEQDIR;
		e.value = des;
		events.push_back(e);
		ret |= SyncFlags::Ok;
		return ret;
	}

	case STOPButton:
	case PLAYButton:
	default:
		ret |= SyncFlags::NotImplemented;
		return ret;
	}
}


double grainsequencer::getSamples() {
	auto _appState = track->_appState;

	// Inverse of syncBySamples: the sequencer is clocked by DENSITY (Hz),
	// so a full pass of all steps lasts steps * sr / DENSITY samples.
	return _STATE->sr * _STATE->params[parent->index][GRAINSEQSTEPS].load() /
		_STATE->params[parent->index][DENSITY].load();
}

Event grainsequencer::getState() {
	Event e{};
	e.trackIndex = track->index;
	e.eventType = Eventtype::paramUpdate;
	e.subType = EventSubtype::sequencerState;
	e.flags |= Event::ToAudioThread;

	SequencerState s{};
	s.grainscount = grainscount[0];
	s.silencecount = silencecount[0];
	s.currentnote = _currentnote[0];
	s.arpstep = arpstep[0];
	s.arpcycledir = arpcycledir[0];
	e.sequencerState = s;
	return e;
};

void grainsequencer::setState(Event& e) {
	auto &s = e.sequencerState;
	grainscount[0] = grainscount[1] = s.grainscount;
	silencecount[0] = silencecount[1] = s.silencecount;
	_currentnote[0] = _currentnote[1] = s.currentnote;
	arpstep[0] = arpstep[1] = s.arpstep;
	arpcycledir[0] = arpcycledir[1] = s.arpcycledir;
};


namespace tsl {
	namespace graphics {
		void SequenceTicker::render(void* ctx) {
			auto* canvas = (SkCanvas*)ctx;
			SkPaint paint;
			paint.setStrokeWidth(lw);
			paint.setAntiAlias(true);
			flush(canvas);
			canvas->save();
			canvas->translate(startx, starty);
			int32_t active = (int)_STATE->params[_STATE->active_track.load()][id].load();
			float wfield = width / (float)16;
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2);
			SkRect bounds{};
			for (int32_t i = 0; i < 16; i++) {
				paint.setColor(i == active ? skcol::custom_green : skcol::fg);
				font.measureText(nums32[i], i > 8 ? 2 : 1, SkTextEncoding::kUTF8, &bounds);
				canvas->drawSimpleText(nums32[i], i > 8 ? 2 : 1, SkTextEncoding::kUTF8,
					i * wfield + wfield * .5f - bounds.centerX(),
					height * .5f - bounds.centerY(),
					font, paint);
			}
			canvas->restore();
		};

		SequenceTicker::SequenceTicker(tsl::AppState* appState, int32_t _alignment, int _id, Layout* _parent) : View(appState,
			VALUE_FROM_POINTER, VALUE_FROM_POINTER,
			_alignment, 10, true) {
			id = _id;
			_STATE->parameters[id].view = this;
			size_reference = &_STATE->textsize2;
			if (_parent != nullptr)
				_parent->addChild(this);
			//v->size_reference_scale = .9f;
		}
	}
}


void grainsequencer::init(TRACK* t) {
	track = t;
	auto _appState = track->_appState;

	_steps = &_STATE->params[t->index][GRAINSEQSTEPS];
	_currentnotedisp = &_STATE->params[t->index][GRAINSEQACTIVE];
	_dir = &_STATE->params[t->index][GRAINSEQDIR];
	for (int32_t i = 0; i < 16; i++) {
		_size[i] = &_STATE->params[t->index][GRAINSEQSIZE01 + i];
		_gain[i] = &_STATE->params[t->index][GRAINSEQGAIN01 + i];
		_pitch[i] = &_STATE->params[t->index][GRAINSEQPITCH01 + i];
	}
	grainscount[0] = grainscount[1] = _STATE->params[track->index][GRAINS].load();
	pitchfacts[0].resize((int)_STATE->parameters[ARP_CYCLES].max + 1);
	pitchfacts[1].resize((int)_STATE->parameters[ARP_CYCLES].max + 1);
}

void grainsequencer::check() {
	auto _appState = track->_appState;

	_stepsold[0] = _stepsold[1] = (int)*_steps;
	_dirold = (int)*_dir;
	silencetocompute[0] = silencetocompute[1] = _STATE->params[track->index][SILENCE];
	grainstocompute[0] = grainstocompute[1] = _STATE->params[track->index][GRAINS];
#if defined(PLUGIN_MODE) || defined(OS_IOS)
	const bool syncDaw = _STATE->params[track->index][SEQSYNCDAWTRANSPORT].load() == 1.f;
	if (syncDaw && _DATA->hostTimeSnapshot.running && !_DATA->hostTimeSnapshot.wasRunning && _STATE->params[track->index][SEQSYNCDAWRESETONSTART].load() == 1.f)
	{
		if (_dirold > 0) {
			_currentnote[0] = 0;
			grainscount[0] = grainscount[1] = grainstocompute[0];
			silencecount[0] = silencecount[1] = 0;
			arpstep[0] = arpstep[1] = 0;
			arpcycledir[0] = arpcycledir[1] = 1;
			track->graingen.reset();
		}
		else {
			_currentnote[0] = _stepsold[0] - 1;
			grainscount[0] = grainscount[1] = grainstocompute[0];
			silencecount[0] = silencecount[1] = 0;
			arpstep[0] = arpstep[1] = 0;
			arpcycledir[0] = arpcycledir[1] = 1;
			track->graingen.reset();		
		};
	}
#endif

	_STATE->params[track->index][SILENCECOUNT] = silencecount[0];
	_STATE->params[track->index][GRAINSCOUNT] = grainscount[0];

	for (int32_t c = 0; c < _STATE->channels; c++)
		for (int32_t i = 0; i < _stepsold[0]; i++) {
			_sizeold[c][i] = *_size[i];
			_gainold[c][i] = dbToLinear60(*_gain[i]);
			_pitchold[c][i] = pow(2., *_pitch[i] / 12.);
		}
	_currentnote[1] = _currentnote[0];
	arp[0] = arp[1] = track->fxpower[SPACE_ARP].load();
	if (arp[0]) {
		oldarpcycles[1] = oldarpcycles[0];
		newarpcycles = (int)_STATE->params[track->index][ARP_CYCLES].load();
		auto interval = _STATE->params[track->index][ARP_INTERVAL].load();
		if (interval != oldinterval) {
			oldinterval = interval;
			MYFLOAT arpcyclefact = pow(2., interval / 12.);
			for (int32_t i = 0; i <= (int)_STATE->parameters[ARP_CYCLES].max; i++) {
				pitchfacts[0][i] = pitchfacts[1][i] = pow(arpcyclefact, i);
			}
		}
		_STATE->params[track->index][ARP_CYCLESTEP].store(arpstep[1] = arpstep[0]);
		_STATE->params[track->index][ARP_CYCLEDIR].store(arpcycledir[1] = arpcycledir[0]);
		oldarpcyclemode[1] = oldarpcyclemode[0];
		newarpcyclemode = (int)_STATE->params[track->index][ARP_CYCLEMODE].load();
		if (oldarpcyclemode[0] == 0)arpcycledir[0] = arpcycledir[1] = 1;
		else if (oldarpcyclemode[0] == 1)arpcycledir[0] = arpcycledir[1] = -1;
		else {
			if (arpstep[0] < 0)arpstep[0] = arpstep[1] = 0;
			else if (arpstep[0] > oldarpcycles[0]) arpstep[0] = arpstep[1] = oldarpcycles[0];
		}
	}
	_DATA->snapShot.queue.try_push(getState());
}

const  char *saveSeq[] = {"SEQUENCER SAVE1", "SEQUENCER SAVE2" , "SEQUENCER SAVE3" , "SEQUENCER SAVE4" };
const  char* loadSeq[] = { "SEQUENCER LOAD1", "SEQUENCER LOAD2" , "SEQUENCER LOAD3" , "SEQUENCER LOAD4" };

void grainsequencer::tick(int32_t channel, MYFLOAT& gain, MYFLOAT& pitch, MYFLOAT& size) {
	auto _appState = track->_appState;
	auto apply = [this](std::vector<Event>&vec, uint16_t dest, MYFLOAT val) {
		if (track->_STATE->params[track->index][dest].exchange(val) != val) {
			Event e;
			e.trackIndex = track->index;
			e.eventType = Eventtype::paramUpdate;
			e.paramIndex = dest;
			e.value = val;
			e.flags = Event::NoInfo | Event::History;
			vec.push_back(e);
		}
	};
	if (channel == 0) {
		while (auto from_ = saveQueue.try_pop()) {
			{
				std::vector<Event> events{};				
				auto from = from_.value();
				auto groupId = _DATA->snapShot.nextGroupId();
				//events.push_back(Event::createEvent(track->index, Eventtype::paramUpdate, SAVESEQUENCE1 + from, 0, 0, groupId, Event::Info | Event::History));
				events.push_back(Event::createTextEvent(track->index, saveSeq[from]));
				for (int32_t i = 0; i < 16; i++) {
					apply(events, GRAINSEQ0GAIN01 + from * 48 + i, _STATE->params[track->index][GRAINSEQGAIN01 + i].load());
					apply(events, GRAINSEQ0PITCH01 + from * 48 + i, _STATE->params[track->index][GRAINSEQPITCH01 + i].load());
					apply(events, GRAINSEQ0SIZE01 + from * 48 + i, _STATE->params[track->index][GRAINSEQSIZE01 + i].load());
					apply(events, GRAINSEQ0PITCH01 + from * 48 + i, _STATE->params[track->index][GRAINSEQPITCH01 + i].load());
				}
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS + SEQSTEPS, _stepsold[channel]);
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS + SEQGRAINS, grainstocompute[channel]);
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS +
					SEQSILENCE, silencetocompute[channel]);
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS + SEQMODE,0);
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS + SEQINT,_STATE->params[track->index][ARP_INTERVAL].load());
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS +
					SEQARPCYCLES, arp[0] ? oldarpcycles[channel]
						: _STATE->params[track->index][ARP_CYCLES].load());
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS +
					SEQARPMODE, arp[0] ? oldarpcyclemode[channel]
						: _STATE->params[track->index][ARP_CYCLEMODE].load());
				apply(events, SEQSAVE01 + from * NUM_LOADPARAMS + SEQARPPOW, arp[channel]);
				auto cur = _currentnote[channel];
				auto gc = grainscount[channel];
				auto sil = silencecount[channel];
				auto astep = arpstep[channel];
				auto adir = arpcycledir[channel];

				while (true) {
					if (cur == 0) {
						apply(events, SEQSAVE01 + from * NUM_LOADPARAMS +
							SEQGRAINSCOUNT, gc);
						apply(events, SEQSAVE01 + from * NUM_LOADPARAMS +
							SEQSILENCECOUNT, sil);
						break;
					}
					if (sil-- > 0) {
					}
					else {
						if (--gc <= 0) {
							sil = silencetocompute[channel];
							gc = grainstocompute[channel];
						}
					}

					int32_t stepforward;

					if (arp[channel]) {
						astep += adir;
						switch (oldarpcyclemode[channel]) {
						case (0):
							if (arpstep[channel] > oldarpcycles[channel]) {
								if (oldarpcyclemode[channel] == 1) {
									astep = oldarpcycles[channel];
									adir = -1;
								}
								else {
									astep = 0;
								}
								stepforward = 1;
							}
							else stepforward = 0;
							break;
						case (1):
							if (astep < 0) {
								if (oldarpcyclemode[channel] == 1) {
									astep = oldarpcycles[channel];
								}
								else {
									astep = 0;
									adir = 1;
								}
								stepforward = 1;
							}
							else stepforward = 0;
							break;
						case (2):
							if (astep > oldarpcycles[channel]) {
								astep = oldarpcycles[channel];
								adir = -1;
								stepforward = 0;
							}
							else if (astep < 0) {
								stepforward = 1;
								if (oldarpcyclemode[channel] == 1)
									astep = oldarpcycles[channel];
								else {
									adir = 1;
									astep = 0;
								}
							}
							else stepforward = 0;
							break;
						default:
							if (astep == oldarpcycles[channel]) {
								adir = -1;
								stepforward = 0;
							}
							else if (astep == 0 && adir == -1) {
								stepforward = 1;
								if (oldarpcyclemode[channel] == 1)
									astep = oldarpcycles[channel];
								else
									adir = 1;
							}
							else stepforward = 0;
							break;

						}
					}
					else stepforward = 1;

					cur += _dirold * stepforward;
					if (cur >= _stepsold[channel]) {
						cur = 0;
					}
					else if (cur < 0)
						cur = _stepsold[channel] - 1;
				}
				if (events.size() > 1) {
					for (auto& e : events)e.groupId = groupId;
					_DATA->snapShot.add_task([_STATE, ev = std::move(events)] {
						std::lock_guard lk(_DATA->snapShot);

						for (auto &e : ev)_DATA->snapShot.addEvent(e);
						});
				}
				
			}
		}
	}
	if (silencecount[channel]-- > 0) {
		gain = 0;
	}
	else {
		gain = _gainold[channel][_currentnote[channel]];
		pitch = _pitchold[channel][_currentnote[channel]];
		size = _sizeold[channel][_currentnote[channel]];
		if (--grainscount[channel] <= 0) {
			silencecount[channel] = silencetocompute[channel];
			grainscount[channel] = grainstocompute[channel];
		}
	}

	if (_currentnote[channel] == 0) {
		if (auto from_ = loadQueue[channel].try_pop()) {
			auto from = from_.value();
			auto oldState = getState();
			for (int32_t i = 0; i < _stepsold[channel]; i++) {
				_sizeold[channel][i] = _STATE->params[track->index][GRAINSEQ0SIZE01 + from * 48 +
					i].load();
				_gainold[channel][i] = dbToLinear60(
					_STATE->params[track->index][GRAINSEQ0GAIN01 + from * 48 + i].load());
				_pitchold[channel][i] = pow(2.,
					_STATE->params[track->index][GRAINSEQ0PITCH01 +
					from * 48 + i].load() /
					12.);
			}
			silencecount[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQSILENCECOUNT].load();
			grainscount[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQGRAINSCOUNT].load();
			grainstocompute[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQGRAINS].load();
			silencetocompute[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQSILENCE].load();
			_stepsold[channel] = _STATE->params[track->index][SEQSAVE01 + from * NUM_LOADPARAMS +
				SEQSTEPS].load();

			arp[channel] = _STATE->params[track->index][SEQSAVE01 + from * NUM_LOADPARAMS +
				SEQARPPOW].load();
			oldarpcycles[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQARPCYCLES].load();
			oldarpcyclemode[channel] = _STATE->params[track->index][SEQSAVE01 +
				from * NUM_LOADPARAMS +
				SEQARPMODE].load();
			auto interval = _STATE->params[track->index][SEQSAVE01 + from * NUM_LOADPARAMS +
				SEQINT].load();
			if (interval != oldinterval) {
				oldinterval = interval;
				MYFLOAT arpcyclefact = pow(2., interval / 12.);
				for (int32_t z = 0; z <= (int)_STATE->parameters[ARP_CYCLES].max; z++) {
					pitchfacts[channel][z] = pow(arpcyclefact, z);
				}
			}
			if (oldarpcyclemode[channel] == 0)arpcycledir[channel] = 1;
			else if (oldarpcyclemode[channel] == 1) arpcycledir[channel] = -1;
			else {
				if (arpstep[channel] < 0)arpstep[channel] = 0;
				else if (arpstep[channel] > oldarpcycles[channel])
					arpstep[channel] = oldarpcycles[channel];
			}

			if (channel == 0) {
				std::vector<Event> events{};
				//events.push_back(Event::createEvent(track->index, Eventtype::paramUpdate, SAVESEQUENCE1 + from, 0, 0, groupId, Event::Info | Event::History));
				events.push_back(Event::createTextEvent(track->index, loadSeq[from]));
				auto newState = getState();
				if (oldState.sequencerState != newState.sequencerState)events.push_back(newState);

				for (int32_t i = 0; i < 16; i++) {
					apply(events, GRAINSEQSIZE01 + i, _STATE->params[track->index][GRAINSEQ0SIZE01 + from * 48 +
						i].load());
					apply(events, GRAINSEQGAIN01 + i, _STATE->params[track->index][GRAINSEQ0GAIN01 + from * 48 +
						i].load());
					apply(events, GRAINSEQPITCH01 +i, _STATE->params[track->index][GRAINSEQ0PITCH01 + from * 48 +
						i].load());
				}
				apply(events, GRAINSEQSTEPS, _stepsold[channel]);
				apply(events, GRAINS, grainstocompute[channel]);
				apply(events, SILENCE,silencetocompute[channel]);
				apply(events, ARP_CYCLES, oldarpcycles[channel]);
				apply(events, ARP_CYCLEMODE, oldarpcyclemode[channel]);
				apply(events, ARP_INTERVAL, interval);

				if (track->fxpower[SPACE_ARP].exchange(arp[channel]) != arp[channel]) {
					Event e;
					e.paramIndex = SPACE_ARP;
					e.trackIndex = track->index;
					e.eventType = Eventtype::Power;
					e.subType = EventSubtype::powerGrainFx;
					e.value = arp[channel];
					events.push_back(e);
				}
				if (events.size() > 1) {
					auto groupId = _DATA->snapShot.nextGroupId();
					for (auto& e : events) {
						e.groupId = groupId;
						e.flags = Event::Redraw | Event::History | Event::NoInfo | Event::ToAudioThread;
					}
					_DATA->snapShot.add_task([_STATE, ev = std::move(events)] {
						std::lock_guard lk(_DATA->snapShot);

						for (auto &e : ev)_DATA->snapShot.addEvent(e);
						});
				}
				_STATE->toUiThreadQueue.try_push([_STATE, tindex = track->index] {
					if (_STATE->active_track.load() == tindex) {
						if (_STATE->parameters[GRAINSEQSTEPS].view->visible_) {
							_STATE->parameters[GRAINSEQSTEPS].view->redraw();
							_STATE->parameters[GRAINS].view->redraw();
							_STATE->parameters[SILENCE].view->redraw();

						}
						else if (_STATE->parameters[ARP_CYCLES].view->visible_) {
							_STATE->parameters[ARP_CYCLES].view->redraw();
							_STATE->parameters[ARP_CYCLEMODE].view->redraw();
							_STATE->parameters[ARP_INTERVAL].view->redraw();
							_STATE->parameters[OFFGRAIN].view->redraw();
							_STATE->parameters[BYPASSGRAINFX].view->redraw();
						}
					}
					});
			}

		}
	}


	int32_t stepforward;
	if (arp[channel]) {
		pitch *= pitchfacts[channel][arpstep[channel]];
		arpstep[channel] += arpcycledir[channel];
		switch (oldarpcyclemode[channel]) {
		case (0):
			if (arpstep[channel] > oldarpcycles[channel]) {
				oldarpcyclemode[channel] = newarpcyclemode;
				oldarpcycles[channel] = newarpcycles;
				if (oldarpcyclemode[channel] == 1) {
					arpstep[channel] = oldarpcycles[channel];
					arpcycledir[channel] = -1;
				}
				else {
					arpstep[channel] = 0;
				}
				stepforward = 1;
			}
			else stepforward = 0;
			break;
		case (1):
			if (arpstep[channel] < 0) {
				oldarpcyclemode[channel] = newarpcyclemode;
				oldarpcycles[channel] = newarpcycles;
				if (oldarpcyclemode[channel] == 1) {
					arpstep[channel] = oldarpcycles[channel];
				}
				else {
					arpstep[channel] = 0;
					arpcycledir[channel] = 1;
				}
				stepforward = 1;
			}
			else stepforward = 0;
			break;
		case (2):
			if (arpstep[channel] > oldarpcycles[channel]) {
				arpstep[channel] = oldarpcycles[channel];
				arpcycledir[channel] = -1;
				stepforward = 0;
			}
			else if (arpstep[channel] < 0) {
				stepforward = 1;
				oldarpcyclemode[channel] = newarpcyclemode;
				oldarpcycles[channel] = newarpcycles;
				if (oldarpcyclemode[channel] == 1)
					arpstep[channel] = oldarpcycles[channel];
				else {
					arpcycledir[channel] = 1;
					arpstep[channel] = 0;
				}
			}
			else stepforward = 0;
			break;
		default:
			if (arpstep[channel] == oldarpcycles[channel]) {
				arpcycledir[channel] = -1;
				stepforward = 0;
			}
			else if (arpstep[channel] == 0 && arpcycledir[channel] == -1) {
				stepforward = 1;
				oldarpcyclemode[channel] = newarpcyclemode;
				oldarpcycles[channel] = newarpcycles;
				if (oldarpcyclemode[channel] == 1)
					arpstep[channel] = oldarpcycles[channel];
				else
					arpcycledir[channel] = 1;
			}
			else stepforward = 0;
			break;

		}
	}
	else stepforward = 1;

	if (channel == 0)
		*_currentnotedisp = _currentnote[channel];
	_currentnote[channel] += _dirold * stepforward;
	if (_currentnote[channel] >= _stepsold[channel]) {
		_currentnote[channel] = 0;
	}
	else if (_currentnote[channel] < 0)
		_currentnote[channel] = _stepsold[channel] - 1;
};

void grainsequencer::pushLoad(int32_t which) {
	auto _appState = track->_appState;

	for (int32_t i = 0; i < _STATE->channels; i++)
		loadQueue[i].try_push(which);
}

void grainsequencer::pushSave(int32_t which) {
	saveQueue.try_push(which);
}