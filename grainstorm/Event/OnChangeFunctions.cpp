#include "types.h"
#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "track.h"
#include "button.h"
#include "Buttonbase.h"
#include "InfoPanel.h"

using namespace tsl::parameters;
using namespace tsl::graphics;

#define tindex e.trackIndex

MYFLOAT onChangeGrainJoin(tsl::AppState* _appState, Event& e) {
	auto old = _STATE->params[e.trackIndex][GRAINJOIN].exchange(e.value);
	if (old == e.value)return old;

	auto track = _DATA->tracks[tindex];
	std::vector<tsl::parameters::Event> toUpdate{};
	toUpdate.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, GRAINJOIN, e.value, 0, 0, Event::History | Event::Redraw));
	if (e.value == 1.0) {
		auto nsegs = (int)_STATE->params[track->index][GRAINNSEGS].load();
		auto dest = _STATE->params[track->index][GRAINENVY0].load();
		if (_STATE->params[track->index][GRAINENVY0 + nsegs].exchange(dest) != dest) {
			toUpdate.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, GRAINENVY0 + nsegs, dest, 0, 0, Event::History | Event::NoInfo));
			toUpdate.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RECOMPUTEGRAINENV3, 1.0));
			toUpdate.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RERENDERGRAINENV3, 1.0));
			_STATE->params[track->index][RECOMPUTEGRAINENV3] = 1.0;
			_STATE->params[track->index][RERENDERGRAINENV3] = 1.0;
		}
	};
	std::lock_guard lk(_DATA->snapShot);

	auto groupId = _DATA->snapShot.nextGroupId();
	for (auto& ev : toUpdate) {
		ev.groupId = groupId;
		_DATA->snapShot.addEvent(ev);
	}
	return old;
}

MYFLOAT onChangeGrainNsegs(tsl::AppState* _appState, Event& e) {
	auto segments = e.value;
	auto old = _STATE->params[tindex][GRAINNSEGS].exchange(segments);
	if (old == segments)return old;


	std::vector<tsl::parameters::Event> events{};
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, GRAINNSEGS, segments, 0, 0, Event::History | Event::Redraw));

	float inc = 1.f / (float)segments;
	float start = 0;

	for (int32_t i = 0; i <= segments; i++) {
		if (_STATE->params[tindex][GRAINENVX0 + i].exchange(start) != start) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, GRAINENVX0 + i, start, 0, 0, tsl::parameters::Event::History | Event::NoInfo));
		}
		start += inc;
	}
	if (_STATE->params[tindex][GRAINJOIN].load()) {
		auto val = _STATE->params[tindex][GRAINENVY0].load();
		if (_STATE->params[tindex][GRAINENVY0 + (int)segments].exchange(val) != val)
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, GRAINENVY0 + (int)segments, val, 0, 0, tsl::parameters::Event::History | Event::NoInfo));
	};

	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RECOMPUTEGRAINENV3, 1.0));
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, RERENDERGRAINENV3, 1.0));
	std::lock_guard lk(_DATA->snapShot);

	auto groupId = _DATA->snapShot.nextGroupId();
	for (auto& ev : events) {
		ev.groupId = groupId;
		_DATA->snapShot.addEvent(ev);
	}
	_STATE->params[tindex][RECOMPUTEGRAINENV3] = 1.0;
	_STATE->params[tindex][RERENDERGRAINENV3] = 1.0;
	return old;
}

MYFLOAT onChangeLfoJoinEnds(tsl::AppState* _appState, Event& e) {
	auto old = _STATE->params[tindex][e.paramIndex].exchange(e.value);
	if (old == e.value)return old;

	auto track = _DATA->tracks[tindex];
	auto aslfo = (e.paramIndex - LFO1JOIN) / LFONUMPARAMS;

	LFO* lfo = track->lfos[aslfo];
	std::vector<tsl::parameters::Event> events{};

	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, LFO1JOIN + aslfo * LFONUMPARAMS, e.value, 0, 0, Event::History | Event::Redraw));


	if (e.value == 1.0) {
		auto val = lfo->gp(LFOENVY0);
		auto nsegs = lfo->segments();
		if (_STATE->params[track->index][LFO1ENVY0 + nsegs + lfo->index * LFONUMPARAMS].exchange(val) != val) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, LFO1ENVY0 + nsegs + lfo->index * LFONUMPARAMS, val, 0, 0, tsl::parameters::Event::History | Event::NoInfo));
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1REDRAW + lfo->index * LFONUMPARAMS, 1.0));
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1RECOMPUTE + lfo->index * LFONUMPARAMS, 1.0));
			lfo->store(LFORECOMPUTE, 1.0);
			lfo->store(LFOREDRAW, 1.0);
		}
	};
	std::lock_guard lk(_DATA->snapShot);

	auto groupId = _DATA->snapShot.nextGroupId();
	for (auto& ev : events) {
		ev.groupId = groupId;
		_DATA->snapShot.addEvent(ev);
	}
	return old;
}

MYFLOAT onChangeSpaceGrainGen(tsl::AppState* _appState, Event& e) {
	auto space = (int)e.value;
	auto old = (int)_STATE->params[tindex][ASGRAINGEN].exchange(space);
	if (old == e.value)return old;
	_STATE->onParamChange(e, tsl::parameters::None);
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, old, space, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASGRAINGEN].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[SPACE_GRAINGEN1 + old]->delRecursiveCB();
					_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)e.value]->addRecursiveCB();
				}
				_STATE->parameters[ASGRAINGEN].view->redraw();
				_DATA->views.spaces_fx[SPACE_GRAINGEN1 + old]->delRecursiveDraw();
				_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)e.value]->redrawDirect();
				_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)e.value]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return old;
}

MYFLOAT onChangeLfoNumSegs(tsl::AppState* _appState, Event& e) {
	auto segments = e.value;
	auto track = _DATA->tracks[tindex];
	auto aslfo = (e.paramIndex - LFO1NSEGS) / LFONUMPARAMS;

	LFO* lfo = track->lfos[aslfo];
	auto old = _STATE->params[tindex][LFO1NSEGS + aslfo * LFONUMPARAMS].exchange(segments);
	if (old == segments)return old;
	float inc = 1.f / (float)segments;
	float start = 0;
	std::vector<tsl::parameters::Event> events{};
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, LFO1NSEGS + lfo->index * LFONUMPARAMS, segments, 0, 0, Event::History | Event::Redraw));

	for (int32_t i = 0; i <= segments; i++) {
		if (_STATE->params[tindex][LFO1ENVX0 + i + lfo->index * LFONUMPARAMS].exchange(start) != start) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, LFO1ENVX0 + i + lfo->index * LFONUMPARAMS, start, 0, 0, tsl::parameters::Event::History | Event::NoInfo));
		}
		start += inc;
	}
	if (lfo->joinends()) {
		auto val = lfo->gp(LFOENVY0);
		if (_STATE->params[tindex][LFO1ENVY0 + (int)segments + lfo->index * LFONUMPARAMS].exchange(val) != val) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, LFO1ENVY0 + segments + lfo->index * LFONUMPARAMS, val, 0, 0, tsl::parameters::Event::History | Event::NoInfo));
		}
	};
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1RECOMPUTE + lfo->index * LFONUMPARAMS, 1.0));
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, LFO1REDRAW + lfo->index * LFONUMPARAMS, 1.0));

	std::lock_guard lk(_DATA->snapShot);

	auto groupId = _DATA->snapShot.nextGroupId();
	for (auto& ev : events) {
		ev.groupId = groupId;
		_DATA->snapShot.addEvent(ev);
	}
	lfo->store(LFORECOMPUTE, 1.0);
	lfo->store(LFOREDRAW, 1.0);
	return old;
}

MYFLOAT onChangeSpaceCross(tsl::AppState* _appState, Event& e) {
	auto space = (int)e.value;
	auto old = (int)_STATE->params[tindex][ASCROSS].exchange(space);

	if (old == e.value) return old;
	_STATE->onParamChange(e, tsl::parameters::None);
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, old, space, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASCROSS].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[SPACE_CROSS_POLAR + old]->delRecursiveCB();
					_DATA->views.spaces_fx[SPACE_CROSS_POLAR + space]->addRecursiveCB();
				}
				/* same guard as onChangeSpacePv: clear the whole space, not
				   just the incoming sub-view's own rect. Every CROSS
				   sub-view currently fits inside that rect, so this changes
				   nothing today - it is here so one that does not cannot
				   leave its spill behind. */
				_DATA->views.spaces_fx[SPACE_CROSS_MAIN]->redrawDirect();
				_STATE->parameters[FFT_SIZE].view->redrawDirect();
				/* the MODULATOR title shares the space, so it has to come
				   back with the selectors */
				_DATA->views.tv_cross_dest->redrawDirect();
				_STATE->parameters[ASCROSS].view->redrawDirect();
				_DATA->views.spaces_fx[SPACE_CROSS_POLAR + old]->delRecursiveDraw();
				_DATA->views.spaces_fx[SPACE_CROSS_POLAR + space]->redrawDirect();
				_DATA->views.spaces_fx[SPACE_CROSS_POLAR + space]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}

	return old;
}

MYFLOAT onChangeSpacePv(tsl::AppState* _appState, Event& e) {
	auto space = (int)e.value;
	auto old = (int)_STATE->params[tindex][ASPV].exchange(space);
	if (old == e.value) return old;
	_STATE->onParamChange(e, tsl::parameters::None);
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, space, old, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASPV].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[SPACE_PV_PH1 + old]->delRecursiveCB();
					_DATA->views.spaces_fx[SPACE_PV_PH1 + space]->addRecursiveCB();
				}
				/* repaint the whole PV space, not just the incoming
				   sub-view: redrawDirect on the sub-view only covers the
				   sub-view's OWN rect, so anything the outgoing algorithm
				   drew outside it stayed on screen. The space is a Layout
				   at prio 20 and the selectors and controls are at 10, so
				   they all come back on top in the same pass. */
				_DATA->views.spaces_fx[SPACE_PV_MAIN]->redrawDirect();
				_STATE->parameters[FFT_SIZE].view->redrawDirect();
				_STATE->parameters[ASPV].view->redrawDirect();
				_DATA->views.spaces_fx[SPACE_PV_PH1 + old]->delRecursiveDraw();
				_DATA->views.spaces_fx[SPACE_PV_PH1 + space]->redrawDirect();
				_DATA->views.spaces_fx[SPACE_PV_PH1 + space]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}

			});
	}

	return old;
}

/* SPECTRAL DELAY algorithm selector: swaps the sub-space view under the
   selector in the second GENCREVERB space. Mirrors onChangeSpacePv, but goes
   through the specdel_subspaces map because RAMP UP/DOWN share one view. */
MYFLOAT onChangeSpecDelMode(tsl::AppState* _appState, Event& e) {
	auto modeSub = [](int m) {
		const int nmodes = (int)std::size(specdel_subspaces);
		return specdel_subspaces[m < 0 || m >= nmodes ? 0 : m];
	};
	auto mode = (int)e.value;
	/* old sessions may carry ids of the removed experimental generators -
	   clamp to RAMP UP so neither the selector nor the sub-space map indexes
	   past the current list */
	if (mode < 0 || mode >= (int)std::size(specdelmodes))
		mode = 0;
	e.value = mode;
	auto old = (int)_STATE->params[tindex][SPECDELMODE].exchange(mode);
	if (old == e.value) return old;
	_STATE->onParamChange(e, tsl::parameters::None);
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, mode, old, e, modeSub]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex &&
				_STATE->parameters[SPECDELMODE].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[modeSub(old)]->delRecursiveCB();
					_DATA->views.spaces_fx[modeSub(mode)]->addRecursiveCB();
				}
				/* repaint the whole alg space, not just the incoming
				   sub-view: the sub-views are sized from their own fixed
				   rows, so an algorithm with fewer sliders covers less
				   ground than the one it replaces and the surplus rows
				   would otherwise stay on screen. A Layout paints its rect
				   with the background at prio 20, ahead of the selector and
				   the sliders at 10, so both are drawn back on top in the
				   same pass. */
				_DATA->views.spaces_fx[SPACE_SPECDELALG]->redrawDirect();
				_STATE->parameters[SPECDELMODE].view->redrawDirect();
				_DATA->views.spaces_fx[modeSub(old)]->delRecursiveDraw();
				_DATA->views.spaces_fx[modeSub(mode)]->redrawDirect();
				_DATA->views.spaces_fx[modeSub(mode)]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}

			});
	}

	return old;
}

MYFLOAT onChangeSpaceGrain(tsl::AppState* _appState, Event& e) {
	auto space = (int)e.value;
	auto spaceold = (int)_STATE->params[tindex][ASGRAN].exchange(space);
	if (spaceold == space)return space;
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, spaceold, space, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASGRAN].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[spaceold]->delRecursiveCB();
					if (spaceold == SPACE_PV_MAIN) {
						_STATE->parameters[FFT_SIZE].view->delRecursiveCB();
					}
					_DATA->views.spaces_fx[space]->addRecursiveCB();
				}

				auto off = static_cast<BypassOffButton*>(_STATE->parameters[OFFGRAIN].view);
				auto bypass = static_cast<BypassOffButton*>(_STATE->parameters[BYPASSGRAINFX].view);

				if (space == SPACE_GRAINSETTINGS || SPACE_GRAINSETTINGS2 == space ||
					SPACE_GRAIN_SEQUENCER == space || SPACE_GRAINSEQUENCER2 == space ||
					SPACE_GRAINGEN == space || SPACE_BPM == space ||
					SPACE_GRAINVCO2 == space
#if defined PLUGIN_MODE || defined STANDALONE_MODE
					|| SPACE_GRAINGAIN == space
#endif
					) {
					off->state = DISABLED;
					bypass->state = DISABLED;
				}
				else if (SPACE_LOOPER == space || SPACE_PITCH == space || SPACE_PV_MAIN == space ||
					SPACE_CROSS_MAIN == space || SPACE_ARP == space) {
					off->state = NORMAL;
					bypass->state = DISABLED;
				}
				else {
					off->state = NORMAL;
					bypass->state = NORMAL;
				}
				_STATE->parameters[ASGRAN].view->addRecursiveDraw();
				_DATA->views.spaces_fx[spaceold]->delRecursiveDraw();
				off->addRecursiveDraw();
				bypass->addRecursiveDraw();
				if (spaceold == SPACE_PV_MAIN) {
					_STATE->parameters[FFT_SIZE].view->delRecursiveDraw();
				}
				_DATA->views.spaces_fx[space]->redrawDirect();
				_DATA->views.spaces_fx[space]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return spaceold;
}

MYFLOAT onChangeSpaceStereoFx(tsl::AppState* _appState, Event& e) {

	auto space = (int)e.value;
	auto old = (int)_STATE->params[tindex][ASSTFX].exchange(
		space);
	if (old == space)return old;
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, old, space, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASSTFX].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[old]->delRecursiveCB();
					_DATA->views.spaces_fx[space]->addRecursiveCB();
				}
				_DATA->views.spaces_fx[old]->delRecursiveDraw();
				_DATA->views.spaces_fx[space]->redrawDirect();
				if (space == SPACE_SPECTRUM) {
					((BypassOffButton*)_STATE->parameters[OFFSTEREOFX].view)->setState(DISABLED, false);

				}
				else {
					((BypassOffButton*)_STATE->parameters[OFFSTEREOFX].view)->setState(NORMAL, false);
				}
				_STATE->parameters[OFFSTEREOFX].view->addRecursiveDraw();
				_STATE->parameters[BYPASSSTEREOFX].view->addRecursiveDraw();
				_DATA->views.spaces_fx[space]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return old;
}

// NOTE: no early-return on old == index — onChangeLFO re-invokes this with the
// current value to re-dispatch the layout for the newly selected LFO.
MYFLOAT onChangeLFOSpace(tsl::AppState* _appState, Event& e) {
	auto index = (int)e.value;
	auto aslfo = e.paramIndex - LFO1SPACE;
	auto old = _STATE->params[tindex][LFO1SPACE + aslfo].exchange(index);
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, index, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[LFO1SPACE].view->visible_) {
				auto* lfo_sync = _DATA->views.lfo_sync_root;
				{
					std::lock_guard lk(_STATE->queue_callback);
					if (index == 0) {
						_DATA->views.lfo_edit_root->delRecursiveCB();
						_DATA->views.lfo_rand_root->delRecursiveCB();
						if (lfo_sync) lfo_sync->delRecursiveCB();
						_DATA->views.space_lfo1->addRecursiveCB();
					}
					else if (index == 1) {
						_DATA->views.space_lfo1->delRecursiveCB();
						_DATA->views.lfo_rand_root->delRecursiveCB();
						if (lfo_sync) lfo_sync->delRecursiveCB();
						_DATA->views.lfo_edit_root->addRecursiveCB();
					}
					else if (index == 2) {
						_DATA->views.space_lfo1->delRecursiveCB();
						_DATA->views.lfo_edit_root->delRecursiveCB();
						if (lfo_sync) lfo_sync->delRecursiveCB();
						_DATA->views.lfo_rand_root->addRecursiveCB();
					}
					else if (lfo_sync) {
						_DATA->views.space_lfo1->delRecursiveCB();
						_DATA->views.lfo_edit_root->delRecursiveCB();
						_DATA->views.lfo_rand_root->delRecursiveCB();
						lfo_sync->addRecursiveCB();
					}
				}

				_STATE->parameters[LFO1SPACE].view->addRecursiveDraw();
				if (index == 0) {
					if (lfo_sync) lfo_sync->delRecursiveDraw();
					_DATA->views.lfo_edit_root->delRecursiveDraw();
					_DATA->views.lfo_rand_root->delRecursiveDraw();
					_STATE->parameters[LFO1POWER].view->addRecursiveDraw();
					_DATA->views.space_lfo1->redrawDirect();
					_DATA->views.space_lfo1->addRecursiveDraw();
				}
				else if (index == 1) {
					if (lfo_sync) lfo_sync->delRecursiveDraw();
					_DATA->views.space_lfo1->delRecursiveDraw();
					_DATA->views.lfo_rand_root->delRecursiveDraw();
					_DATA->views.lfo_edit_root->redrawDirect();
					_DATA->views.lfo_edit_root->addRecursiveDraw();
				}
				else if (index == 2) {
					if (lfo_sync) lfo_sync->delRecursiveDraw();
					_DATA->views.space_lfo1->delRecursiveDraw();
					_DATA->views.lfo_edit_root->delRecursiveDraw();
					_STATE->parameters[LFO1POWER].view->addRecursiveDraw();
					_DATA->views.lfo_rand_root->redrawDirect();
					_DATA->views.lfo_rand_root->addRecursiveDraw();
				}
				else if (lfo_sync) {
					_STATE->parameters[LFO1POWER].view->redrawDirect();
					_DATA->views.space_lfo1->delRecursiveDraw();
					_DATA->views.lfo_edit_root->delRecursiveDraw();
					_DATA->views.lfo_rand_root->delRecursiveDraw();
					lfo_sync->redrawDirect();
					lfo_sync->addRecursiveDraw();
				}
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return old;
}

MYFLOAT onChangeLFO(tsl::AppState* _appState, Event& e) {
	auto index = (int)e.value;
	auto old = _STATE->params[tindex][ASLFO].exchange(index);
	Event ee;
	ee.setup(_STATE, tindex, LFO1SPACE);
	ee.value = ee.getCurrentValue(_STATE);
	onChangeLFOSpace(_STATE, ee);
	return old;
}

MYFLOAT onChangeLFODest(tsl::AppState* _appState, Event& e) {
	auto target = e.value;
	auto old = _STATE->params[tindex][e.paramIndex].exchange(target);
	if (old != target && (e.flags & Event::Redraw || e.flags & Event::Info)) {
		_STATE->toUiThreadQueue.try_push([_STATE, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[LFO1DEST].view->visible_) {
				_STATE->parameters[LFO1DEST].view->addRecursiveDraw();
				_STATE->parameters[LFO1BOUNDA].view->addRecursiveDraw();
				_STATE->parameters[LFO1BOUNDB].view->addRecursiveDraw();
				_STATE->parameters[LFODESTPOWER].view->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return old;
}

MYFLOAT onChangeSpaceFx(tsl::AppState* _appState, Event& e) {
	auto space = (int)e.value;
	auto active = (int)_STATE->params[tindex][ASFX].exchange(space);
	if (active == space)return active;
	if (e.flags & Event::Redraw || e.flags & Event::Info) {
		_STATE->toUiThreadQueue.try_push([_STATE, active, space, e]() mutable {
			if (e.flags & Event::Redraw && _STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASFX].view->visible_) {
				{
					std::lock_guard lk(_STATE->queue_callback);
					_DATA->views.spaces_fx[active]->delRecursiveCB();
					_DATA->views.spaces_fx[space]->addRecursiveCB();
				}
				if (_DATA->callbacks_fx_power[space] == nullptr) {
					((BypassOffButton*)_STATE->parameters[OFFFX].view)->setState(DISABLED, false);
					((BypassOffButton*)_STATE->parameters[BYPASSFX].view)->setState(DISABLED, false);

				}
				else {
					((BypassOffButton*)_STATE->parameters[OFFFX].view)->setState(NORMAL, false);
					((BypassOffButton*)_STATE->parameters[BYPASSFX].view)->setState(NORMAL, false);
				}
				_STATE->parameters[OFFFX].view->addRecursiveDraw();
				_STATE->parameters[BYPASSFX].view->addRecursiveDraw();
				_DATA->views.spaces_fx[active]->delRecursiveDraw();
				_DATA->views.spaces_fx[space]->redrawDirect();
				_DATA->views.spaces_fx[space]->addRecursiveDraw();
			}
			if (e.flags & Event::Info) {
				auto info = (InfoPanel*)_DATA->views.infopanel;
				info->prepareBuffer(e);
			}
			});
	}
	return active;
}

MYFLOAT onChangeNsegs(tsl::AppState* _appState, Event& e) {
	auto nsegs = (int)e.value;
	auto old = _STATE->params[tindex][e.paramIndex].exchange(nsegs);
	if (old == nsegs)return old;
	auto baseId = e.paramIndex - OFFNSEGS;

	float inc = 1.f / (float)nsegs;
	float start = 0;
	const auto joinEnds = _STATE->params[tindex][baseId + OFFJOINENDS].load() == 1.;


	std::vector<tsl::parameters::Event> events{};
	events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, e.paramIndex, nsegs, 0, 0, Event::History | Event::Redraw));
	for (int32_t i = 0; i <= nsegs; i++) {
		if (_STATE->params[tindex][baseId + i].exchange(start) != start) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, baseId + i, start, 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
		}
		start += inc;
	}
	if (joinEnds) {
		auto val = _STATE->params[tindex][baseId + OFFPOINTY + nsegs].load();
		if (_STATE->params[tindex][baseId + OFFPOINTY + nsegs].exchange(val) != val) {
			events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, baseId + OFFPOINTY + nsegs, val, 0, 0, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
		}
	};
	for (int32_t i = 0; i < _STATE->channels; i++)
		events.push_back(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, baseId + OFFRECOMP + i, 1.0));
	events.push_back(tsl::parameters::Event::createRerenderEvent(tindex, baseId, 0));
	std::lock_guard lk(_DATA->snapShot);
	auto groupId = _DATA->snapShot.nextGroupId();
	for (auto& ev : events) {
		ev.groupId = groupId;
		_DATA->snapShot.addEvent(ev);
	}

	for (int32_t i = 0; i < _STATE->channels; i++)
		_STATE->params[tindex][baseId + OFFRECOMP + i].store(1.0);
	_STATE->parameters[baseId].view->redraw();
	return old;
}

MYFLOAT onChangeJoinEnds(tsl::AppState* _appState, Event& e) {
	auto joinEnds = e.value;
	auto old = _STATE->params[tindex][e.paramIndex].exchange(joinEnds);
	if (old == joinEnds)return joinEnds;
	auto baseId = e.paramIndex - OFFJOINENDS;
	if (joinEnds == 1.) {
		auto nsegs = (int)_STATE->params[tindex][baseId + OFFNSEGS].load();

		auto val = _STATE->params[tindex][baseId + OFFPOINTY].load();
		auto yId = baseId + OFFPOINTY + nsegs;

		if (_STATE->params[tindex][yId].exchange(val) == val) {
			_DATA->snapShot.add_task([_STATE, e] {
				_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, e.paramIndex, e.value));

				});
			return old;
		}

		for (int i = 0; i < _STATE->channels; i++)
			_STATE->params[tindex][baseId + OFFRECOMP + i].store(1.0);
		std::lock_guard lk(_DATA->snapShot);

		auto groupId = _DATA->snapShot.nextGroupId();
		_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, e.paramIndex, 1.0, 0, groupId));
		_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, yId, val, 0, groupId, tsl::parameters::Event::History | tsl::parameters::Event::NoInfo));
		for (int i = 0; i < _STATE->channels; i++) {
			_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::SpecialAction, baseId + OFFRECOMP + i, 1.0, 0, groupId));
		}
		_DATA->snapShot.addEvent(tsl::parameters::Event::createRerenderEvent(tindex, baseId, groupId));
		_STATE->toUiThreadQueue.try_push([_STATE, e, baseId] {
			if (tindex == _STATE->active_track.load() && _STATE->parameters[baseId].view->visible_)
				_STATE->parameters[baseId].view->redraw();
			});

	}
	else {
		_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Eventtype::paramUpdate, e.paramIndex, 0.0));

	}
	return old;
}



MYFLOAT Event::onChange(tsl::AppState* _appState) {

	switch (paramIndex) {
	case LFO2NSEGS:
	case LFO3NSEGS:
	case LFO1NSEGS: return onChangeLfoNumSegs(_STATE, *this);
	case LFO2JOIN:
	case LFO3JOIN:
	case LFO1JOIN: return onChangeLfoJoinEnds(_STATE, *this);
	case LFO2SPACE:
	case LFO3SPACE:
	case LFO1SPACE: return onChangeLFOSpace(_STATE, *this);
	case LFO2DEST:
	case LFO3DEST:
	case LFO1DEST: return onChangeLFODest(_STATE, *this);
	case GRAINNSEGS: return onChangeGrainNsegs(_STATE, *this);
	case GRAINJOIN: return onChangeGrainJoin(_STATE, *this);
	case ASGRAINGEN: return onChangeSpaceGrainGen(_STATE, *this);
	case ASPV: return onChangeSpacePv(_STATE, *this);
	case SPECDELMODE: return onChangeSpecDelMode(_STATE, *this);
	case ASCROSS: return onChangeSpaceCross(_STATE, *this);
	case ASSTFX: return onChangeSpaceStereoFx(_STATE, *this);
	case ASLFO: return onChangeLFO(_STATE, *this);
	case ASFX: return onChangeSpaceFx(_STATE, *this);
	case ASGRAN: return onChangeSpaceGrain(_STATE, *this);
	case GRAINFILTERNSEGS:
	case SPECDEL2NSEGS: return onChangeNsegs(_STATE, *this);
	case GRAINFILTERJOINENDS:
	case SPECDEL2JOINENDS: return onChangeJoinEnds(_STATE, *this);
	default: return 0;
	}
}