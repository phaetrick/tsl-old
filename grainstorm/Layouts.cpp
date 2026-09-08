#include "Layouts.h"
#include "waveform.h"
#include "track.h"
#include "grainstorm.h"
#include <textview.h>
#include <app.h>

using namespace tsl::graphics;

void SpaceWaveform::addRecursiveDraw() {
	Layout::addRecursiveDraw();
	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	track->waveform->addRecursiveDraw();
};

void SpaceWaveform::addRecursiveCB() {
	Layout::addRecursiveCB();
	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	track->waveform->addRecursiveCB();
};


void SpaceGranulation::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	auto space = static_cast<int>(_STATE->params[tindex][ASGRAN].load());
	auto off = static_cast<BypassOffButton*>(_STATE->parameters[OFFGRAIN].view);
	auto bypass = static_cast<BypassOffButton*>(_STATE->parameters[BYPASSGRAINFX].view);
	off->addRecursiveCB();
	bypass->addRecursiveCB(); 
	_STATE->parameters[ASGRAN].view->addRecursiveCB();
	_DATA->views.spaces_fx[space]->addRecursiveCB();
}

void SpaceGranulation::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	auto space = static_cast<int>(_STATE->params[tindex][ASGRAN].load());
	auto off = static_cast<BypassOffButton*>(_STATE->parameters[OFFGRAIN].view);
	auto bypass = static_cast<BypassOffButton*>(_STATE->parameters[BYPASSGRAINFX].view);


	if (space == SPACE_GRAINSETTINGS || SPACE_GRAINSETTINGS2 == space || SPACE_GRAIN_SEQUENCER == space || SPACE_GRAINSEQUENCER2 == space || SPACE_GRAINGEN == space || SPACE_BPM == space
		|| space == SPACE_GRAINVCO2
#if defined PLUGIN_MODE || defined STANDALONE_MODE
		|| space == SPACE_GRAINGAIN
#endif
		) {
		off->state = DISABLED;
		bypass->state = DISABLED;
	}
	else if (SPACE_LOOPER == space || SPACE_PITCH == space || SPACE_PV_MAIN == space || SPACE_CROSS_MAIN == space || SPACE_ARP == space) {
		off->state = NORMAL;
		bypass->state = DISABLED;
	}
	else {
		off->state = NORMAL;
		bypass->state = NORMAL;
	}

	_STATE->parameters[ASGRAN].view->addRecursiveDraw();
	off->addRecursiveDraw();
	bypass->addRecursiveDraw();
	_DATA->views.spaces_fx[space]->addRecursiveDraw();

};

void SpaceGrainGen::addRecursiveCB() {
	Layout::addRecursiveCB();;
	auto tindex = _STATE->active_track.load();
	auto space = ASGRAINGEN;
	_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)_STATE->params[tindex][ASGRAINGEN].load()]->addRecursiveCB();
}

void SpaceGrainGen::addRecursiveDraw() {
	Layout::addRecursiveDraw();;
	auto tindex = _STATE->active_track.load();
	auto space = ASGRAINGEN;
	_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)_STATE->params[tindex][space].load()]->redrawDirect();
	_DATA->views.spaces_fx[SPACE_GRAINGEN1 + (int)_STATE->params[tindex][space].load()]->addRecursiveDraw();
};

void SpaceGrainEnv::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	_DATA->views.divider_envelopes->redrawDirect();
	_DATA->views.button_analysis->redrawDirect();
	_DATA->views.button_synthesis->redrawDirect();
	_DATA->views.space_grainenv1->redrawDirect();
	if (GASENV == SPACE_GRAINENV1)
		_DATA->views.space_grainenv1->addRecursiveDraw();
	else {
		_DATA->views.space_grainenv2->addRecursiveDraw();
	}
};

void SpaceGrainEnv::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	_DATA->views.button_analysis->addRecursiveCB();
	_DATA->views.button_synthesis->addRecursiveCB();
	if (GASENV == SPACE_GRAINENV1)
		_DATA->views.space_grainenv1->addRecursiveCB();
	else
		_DATA->views.space_grainenv2->addRecursiveCB();;
};

void SpacePv::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[FFT_SIZE].view->addRecursiveCB();
	_STATE->parameters[ASPV].view->addRecursiveCB();
	_DATA->views.spaces_fx[SPACE_PV_PH1 + (int)_STATE->params[tindex][ASPV].load()]->addRecursiveCB();
};

void SpacePv::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[FFT_SIZE].view->addRecursiveDraw();
	_STATE->parameters[ASPV].view->addRecursiveDraw();
	_DATA->views.spaces_fx[SPACE_PV_PH1 + (int)_STATE->params[tindex][ASPV].load()]->addRecursiveDraw();
};

void SpaceCross::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[FFT_SIZE].view->addRecursiveCB();
	_STATE->parameters[ASCROSS].view->addRecursiveCB();

	_DATA->views.spaces_fx[SPACE_CROSS_POLAR + (int)_STATE->params[tindex][ASCROSS].load()]->addRecursiveCB();
};

const char* modnames2[] = { "MODULATOR: TRACK2", "MODULATOR: TRACK3", "MODULATOR: TRACK4",
						  "MODULATOR: TRACK1" };

void SpaceCross::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[ASCROSS].view->addRecursiveDraw();
	_STATE->parameters[FFT_SIZE].view->addRecursiveDraw();
	auto tv = static_cast<TextView*>(_DATA->views.tv_cross_dest);
	tv->text = modnames2[tindex];
	_DATA->views.tv_cross_dest->addRecursiveDraw();

	_DATA->views.spaces_fx[SPACE_CROSS_POLAR + (int)_STATE->params[tindex][ASCROSS].load()]->addRecursiveDraw();
};

void SpaceLFO::addRecursiveCB() {
	Layout::addRecursiveCB();
	auto tindex = _STATE->active_track.load();
	auto val = (int)_STATE->params[tindex][LFO1SPACE + GASLFO].load();
	if (val == 0) {
		_DATA->views.space_lfo1->addRecursiveCB();
	}
	else if (val == 1) {
		_DATA->views.lfo_edit_root->addRecursiveCB();
	}
	else if (val == 2){
		_DATA->views.lfo_rand_root->addRecursiveCB();
	}
	else if (_DATA->views.lfo_sync_root) {
		_DATA->views.lfo_sync_root->addRecursiveCB();
	}
};

void SpaceLFO::addRecursiveDraw() {
	Layout::addRecursiveDraw();
	auto tindex = _STATE->active_track.load();
	auto activeLFO = GASLFO;
	auto val = (int)_STATE->params[tindex][LFO1SPACE + activeLFO].load();
	if (val == 0) {
		_DATA->views.space_lfo1->redrawDirect();
		_DATA->views.space_lfo1->addRecursiveDraw();
	}
	else if (val == 1) {
		View::addRecursiveDraw();
		_DATA->views.lfo_edit_root->redrawDirect();
		_DATA->views.lfo_edit_root->addRecursiveDraw();
	}
	else if (val == 2) {
		_DATA->views.lfo_rand_root->redrawDirect();
		_DATA->views.lfo_rand_root->addRecursiveDraw();
	}
	else if (_DATA->views.lfo_sync_root) {
		_DATA->views.lfo_sync_root->redrawDirect();
		_DATA->views.lfo_sync_root->addRecursiveDraw();
	}

};



void SpaceFX::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	int32_t active = GASFX;
	_STATE->parameters[OFFFX].view->addRecursiveCB();
	_STATE->parameters[BYPASSFX].view->addRecursiveCB();
	_STATE->parameters[ASFX].view->addRecursiveCB();
	_DATA->views.spaces_fx[active]->addRecursiveCB();
};

void SpaceFX::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	auto space = GASFX;
	if (_DATA->callbacks_fx_power[space] == nullptr) {
		((BypassOffButton*)_STATE->parameters[OFFFX].view)->setState(DISABLED, false);
		((BypassOffButton*)_STATE->parameters[BYPASSFX].view)->setState(DISABLED, false);

	}
	else {
		((BypassOffButton*)_STATE->parameters[OFFFX].view)->setState(NORMAL, false);
		((BypassOffButton*)_STATE->parameters[BYPASSFX].view)->setState(NORMAL, false);
	};
	_STATE->parameters[OFFFX].view->addRecursiveDraw();
	_STATE->parameters[BYPASSFX].view->addRecursiveDraw();
	_STATE->parameters[ASFX].view->addRecursiveDraw();
	_DATA->views.spaces_fx[space]->redrawDirect();
	_DATA->views.spaces_fx[space]->addRecursiveDraw();
};

void SpaceFOL::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[FFT_SIZE].view->addRecursiveCB();
	_STATE->parameters[ASPV].view->addRecursiveCB();
	_DATA->views.spaces_fx[SPACE_PV_PH1 + (int)_STATE->params[tindex][ASPV].load()]->addRecursiveCB();
};

void SpaceFOL::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[FFT_SIZE].view->addRecursiveDraw();
	_STATE->parameters[ASPV].view->addRecursiveDraw();
	_DATA->views.spaces_fx[SPACE_PV_PH1 + (int)_STATE->params[tindex][ASPV].load()]->addRecursiveDraw();
};

/* mode -> sub-space view, clamped so an out-of-range saved value can't walk
   off the map */
static int specdelSub(int mode) {
	const int nmodes = (int)std::size(specdel_subspaces);
	if (mode < 0 || mode >= nmodes)
		mode = 0;
	return specdel_subspaces[mode];
}

void SpaceSpecDelAlg::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[SPECDELMODE].view->addRecursiveCB();
	_DATA->views.spaces_fx[specdelSub(
		(int)_STATE->params[tindex][SPECDELMODE].load())]->addRecursiveCB();
};

void SpaceSpecDelAlg::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	_STATE->parameters[SPECDELMODE].view->addRecursiveDraw();
	auto sub = _DATA->views.spaces_fx[specdelSub(
		(int)_STATE->params[tindex][SPECDELMODE].load())];
	sub->redrawDirect();
	sub->addRecursiveDraw();
};

void SpaceStereo::addRecursiveCB() {
	auto tindex = _STATE->active_track.load();
	auto space = GASSTFX;
	_STATE->parameters[ASSTFX].view->addRecursiveCB();
	_STATE->parameters[OFFSTEREOFX].view->addRecursiveCB();
	_STATE->parameters[BYPASSSTEREOFX].view->addRecursiveCB();
	_DATA->views.spaces_fx[space]->addRecursiveCB();
};

void SpaceStereo::addRecursiveDraw() {
	auto tindex = _STATE->active_track.load();
	auto space = GASSTFX;
	_STATE->parameters[ASSTFX].view->addRecursiveDraw();
	if(space == SPACE_SPECTRUM)
		((BypassOffButton*)_STATE->parameters[OFFSTEREOFX].view)->setState(DISABLED, false);
	else
		((BypassOffButton*)_STATE->parameters[OFFSTEREOFX].view)->setState(NORMAL, false);
	_STATE->parameters[OFFSTEREOFX].view->addRecursiveDraw();
	_STATE->parameters[BYPASSSTEREOFX].view->addRecursiveDraw();
	_DATA->views.spaces_fx[space]->redrawDirect();
	_DATA->views.spaces_fx[space]->addRecursiveDraw();
};
