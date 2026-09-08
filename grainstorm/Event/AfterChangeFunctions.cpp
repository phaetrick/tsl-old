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


void recomputeGrainEnv1(tsl::AppState* _appState, Event& e) {
	auto track = _DATA->tracks[tindex];
	_STATE->params[track->index][RECOMPUTEGRAINENV1] = 1.0;
	_STATE->params[track->index][RERENDERGRAINENV1] = 1.0;

	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		auto v = _STATE->parameters[GRAINENVELOPEVIEW].view;
		if (_STATE->active_track.load() == tindex && v->visible_) {
			v->redraw();
		}
		});
}

void recomputeGrainEnv2(tsl::AppState* _appState, Event& e) {
	_STATE->params[tindex][RECOMPUTEGRAINENV2] = 1.0;
	_STATE->params[tindex][RERENDERGRAINENV2] = 1.0;

	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		auto v = _STATE->parameters[GRAINENVELOPEVIEW].view;
		if (_STATE->active_track.load() == tindex && v->visible_) {
			v->redraw();
		}
		});
}


void grainCurveUpdated(tsl::AppState* _appState, Event& e) {
	auto track = _DATA->tracks[tindex];
	_STATE->params[track->index][RECOMPUTEGRAINENV3] = 1.0;
	_STATE->params[track->index][RERENDERGRAINENV3] = 1.0;
}


void
redrawComp(tsl::AppState* _appState, Event& e) {


	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		auto active = _STATE->active_track.load();
		if (active == tindex && _DATA->views.IRMonoComp->visible_) {
			_DATA->views.IRMonoComp->redraw();
		}
		else if (active == tindex && _DATA->views.IRSTComp->visible_) {
			_DATA->views.IRSTComp->redraw();
		}
		});
}

void rendergrainenv1(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		if (_STATE->active_track.load() == tindex && _STATE->parameters[GRAINENVELOPEVIEW].view->visible_)
			_STATE->parameters[GRAINENVELOPEVIEW].view->redraw();
		});

}


void
renderlfoenv(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		auto active = _STATE->active_track.load();
		if (active == tindex && GASMAIN == SPACE_LFOS && GASLFO == e.subType && _DATA->views.env_win_lfo->visible_)
			_DATA->views.env_win_lfo->redraw();
		});
}

void adjustTrackTime(tsl::AppState* _appState, Event& e) {
	auto track = _DATA->tracks[tindex];
	track->computeLoopTime();
}


void afterChangeLFOCurve(tsl::AppState* _appState, Event& e) {
	auto track = _DATA->tracks[tindex];
	LFO* lfo = track->lfos[(e.paramIndex - LFO1CURVE) / LFONUMPARAMS];
	lfo->store(LFORECOMPUTE, 1.0);
	lfo->store(LFOREDRAW, 1.0);
}

void afterChangeSpaceGrainEnv(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {

		if (_STATE->active_track.load() == tindex && _STATE->parameters[GRAINENVSPACE].view->visible_) {
			_STATE->parameters[AOUTERCYCLES].view->addRecursiveDraw();
			_STATE->parameters[AOUTERDEPTH].view->addRecursiveDraw();
			_STATE->parameters[ENVINNER].view->addRecursiveDraw();
			_STATE->parameters[ENVOUTER].view->addRecursiveDraw();
		}
		});
}

void  cbFolest(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		if (_STATE->active_track.load() == e.trackIndex && _STATE->parameters[FOLLOWERDEST].view->visible_) {
			_STATE->parameters[FOLLOWERGAIN].view->addRecursiveDraw();
			_STATE->parameters[FOLLOWERATT].view->addRecursiveDraw();
			_STATE->parameters[FOLLOWERREL].view->addRecursiveDraw();
			_STATE->parameters[FOLLOWERBOUNDA].view->addRecursiveDraw();
			_STATE->parameters[FOLLOWERBOUNDB].view->addRecursiveDraw();

			_STATE->parameters[FOLLOWERDESTPOWER].view->addRecursiveDraw();
			_STATE->parameters[FOLLOWERSRC].view->addRecursiveDraw();

		}
		});
}

void afterChangeMDelay(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		if (_STATE->active_track.load() == e.trackIndex && _STATE->parameters[ASMDEL].view->visible_) {
			_STATE->parameters[MDELAY1_POW].view->addRecursiveDraw();
			_STATE->parameters[MDELAY1_CONTROLS_ACTIVE].view->addRecursiveDraw();
			_STATE->parameters[MDELAY1DUMMY1].view->addRecursiveDraw();
			_DATA->views.mdelay_controlpanel2->addRecursiveDraw();
		}
		});
	
}

void afterChangeMComp(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		if (_STATE->active_track.load() == e.trackIndex && _STATE->parameters[SPACEMULTICOMP].view->visible_) {
			_DATA->views.spaces_fx[SPACE_MULTICOMP]->redrawDirect();
			_DATA->views.spaces_fx[SPACE_MULTICOMP]->addRecursiveDraw();
		}
		});
	
	
}

void afterChangeDynEq(tsl::AppState* _appState, Event& e) {
	_STATE->toUiThreadQueue.try_push([_STATE, e] {
		if (_STATE->active_track.load() == e.trackIndex && _STATE->parameters[SPACEDYNEQ].view->visible_) {
			_DATA->views.spaces_fx[SPACE_DYNEQ5]->redrawDirect();
			_DATA->views.spaces_fx[SPACE_DYNEQ5]->addRecursiveDraw();
		}
		});
	
}

void afterChangeCurve(tsl::AppState* _appState, Event& e) {
	auto baseId = e.paramIndex - OFFCURVE;
	for (int32_t i = 0; i < _STATE->channels; i++)
		_STATE->params[tindex][baseId + OFFRECOMP + i].store(1.0);
	_STATE->toUiThreadQueue.try_push([_STATE, e, baseId] {
		auto v = _STATE->parameters[baseId].view;
		if (_STATE->active_track.load() == tindex && v->visible_) {
			v->redraw();
		}
		});
}


void Event::afterChange(tsl::AppState* _appState) {

	switch (paramIndex) {
	case SPEED: adjustTrackTime(_STATE, *this); return;
	case GRAINENVSPACE:  afterChangeSpaceGrainEnv(_STATE, *this); return;
	case AOUTERCYCLES:
	case AOUTERDEPTH:
	case ENVOUTER:
	case ENVINNER: recomputeGrainEnv1(_STATE, *this); return;
	case AOUTERCYCLES2:
	case AOUTERDEPTH2:
	case ENVOUTER2:
	case ENVINNER2: recomputeGrainEnv2(_STATE, *this); return;
	case GRAINENVINTERPOL: rendergrainenv1(_STATE, *this); return;
	case GRAINCURVE: grainCurveUpdated(_STATE, *this); return;
	case LFO2EDITFUNC:
	case LFO3EDITFUNC:
	case LFO1EDITFUNC: afterChangeLFOCurve(_STATE, *this); return;
	case COMPTHR:
	case COMPKNEE:
	case COMPRATIO:
	case STCOMPTHR:
	case STCOMPKNEE:
	case STCOMPRATIO: redrawComp(_STATE, *this); return;
	case FOLLOWERDEST: cbFolest(_STATE, *this); return;
	case ASMDEL: afterChangeMDelay(_STATE, *this); return;
	case SPACEMULTICOMP: afterChangeMComp(_STATE, *this); return;
	case SPACEDYNEQ: afterChangeDynEq(_STATE, *this); return;
	case GRAINFILTERCURVE:
	case SPECDEL2CURVE: afterChangeCurve(_STATE, *this); return;
	case LFO1STOPPED:
	case LFO2STOPPED:
	case LFO3STOPPED:
		if(_STATE->active_track.load() == trackIndex)_STATE->toUiThreadQueue.try_push([_STATE] {
			if (_STATE->parameters[LFO1PLAY].view->visible_)
				_STATE->parameters[LFO1PLAY].view->redraw();
			});
		return;
	case TRACKSTOPPED:
		if (_STATE->active_track.load() == trackIndex)_STATE->toUiThreadQueue.try_push([_STATE] {
			if (_STATE->parameters[PLAYButton].view->visible_)
				_STATE->parameters[PLAYButton].view->redraw();
			});
		return;
	default: ;
	}
}