#include "app.h"
#include "params.h"
#include "grainstorm.h"
using namespace tsl::parameters;

void Event::setup(tsl::AppState* _appState, int tindex, uint16_t id, uint16_t* displayParam, std::atomic<MYFLOAT>** ref) {
	if (id == PARAM_NOT_ASSIGNED) {
		paramIndex = id;
		eventType = tsl::parameters::Eventtype::NoParam;
		return;
	}
	
	flags = Redraw;

	if (!(_STATE->parameters[id].flags & Param::NoAssignment) && !(_STATE->parameters[id].flags & Param::NoValue))flags |= History;
	
	groupId = 0;
	trackIndex = tindex;

	switch (id) {

#ifdef GRAINSTORM
	case LOOPSTARTFROMEXT:
	case LOOPSTOPFROMEXT:
	case LOOPPOSFROMEXT:
		eventType = LoopFromExt;
		paramIndex = id;
		break;
	case LOOPPOS0:
	case LOOPPOS1:
	case LOOPPOS2:
	case LOOPPOS3:
	case LOOPPOS4:
	case LOOPPOS5:
	case LOOPPOS6:
	case LOOPPOS7:
		paramIndex = id;
		eventType = paramUpdate;
		flags |= ToWorkerThread;
		break;
	case LFO1BOUNDA:
	case LFO1BOUNDB: {
		auto aslfo = GASLFO;
		subType = aslfo;
		paramIndex = _STATE->params[tindex][LFO1DEST + aslfo * LFONUMPARAMS].load();
		if (ref) {
			*ref = id == LFO1BOUNDA ? &_STATE->controls[tindex][paramIndex].lfo_min
				: &_STATE->controls[tindex][paramIndex].lfo_max;
		}
		eventType = id == LFO1BOUNDA ? Eventtype::lfoMin : Eventtype::lfoMax;
		if (displayParam)
			*displayParam = paramIndex;
		break;
	}
	case LFODESTPOWER: {
		eventType = Eventtype::lfoDest;
		subType = GASLFO;
		auto lfo = _DATA->tracks[trackIndex]->lfos[subType];
		paramIndex = lfo->dest();
		if (ref) *ref = nullptr;
		if (displayParam) *displayParam = LFODESTPOWER;
		break;
	}

	case FOLLOWERBOUNDA:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerMin;
		if (displayParam) *displayParam = paramIndex;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).min;
		break;

	case FOLLOWERBOUNDB:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerMax;
		if (displayParam) *displayParam = paramIndex;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).max;
		break;

	case FOLLOWERGAIN:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerGain;
		if (displayParam) *displayParam = FOLLOWERGAIN;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).gain;
		break;

	case FOLLOWERATT:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerAtt;
		if (displayParam) *displayParam = FOLLOWERATT;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).att;
		break;

	case FOLLOWERREL:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerDec;
		if (displayParam) *displayParam = FOLLOWERREL;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).rel;
		break;
	case FOLLOWERSRC:
		eventType = tsl::parameters::Eventtype::Follower;
		paramIndex = _STATE->params[tindex][FOLLOWERDEST].load();
		subType = tsl::parameters::EventSubtype::followerSidechain;
		if (displayParam) *displayParam = FOLLOWERSRC;
		if (ref) *ref = &_STATE->followerMap[tindex].at(paramIndex).source;
		break;
	
	case OFFGRAIN:
	case OFFFX:
	case OFFSTEREOFX:
	case BYPASSGRAINFX:
	case BYPASSFX:
	case BYPASSSTEREOFX:
	case POWERTRACK:
	case FOLLOWERDESTPOWER: {
		auto track = _DATA->tracks[tindex];
		eventType = Power;
		switch (id) {
		case OFFGRAIN: {
			subType = EventSubtype::powerGrainFx;
			paramIndex = GASGRAIN;
			flags |= ToWorkerThread;
			power.pos = INT32_MAX;
			if (displayParam) *displayParam = OFFGRAIN;
			if (ref) *ref = &track->fxpower[paramIndex];
			break;
		}
		case OFFFX: {
			subType = tsl::parameters::EventSubtype::powerFx;
			paramIndex = GASFX;
			flags |= ToWorkerThread;
			power.pos = INT32_MAX;
			if (displayParam) *displayParam = OFFFX;
			if (ref) *ref = &track->fxpower[paramIndex];
			break;
		}
		case OFFSTEREOFX: {
			subType = tsl::parameters::EventSubtype::powerStereoFx;
			paramIndex = GASSTFX;
			flags |= ToWorkerThread;
			power.pos = INT32_MAX;
			if (displayParam) *displayParam = OFFSTEREOFX;
			if (ref) *ref = &track->fxpower[paramIndex];
			break;
		}
		case BYPASSGRAINFX: {
			subType = tsl::parameters::EventSubtype::bypassGrainFx;
			paramIndex = GASGRAIN;
			if (displayParam) *displayParam = BYPASSGRAINFX;
			if (ref) *ref = &track->bypass[paramIndex];
			break;
		}
		case BYPASSFX: {
			subType = tsl::parameters::EventSubtype::bypassFx;
			paramIndex = GASFX;
			if (displayParam) *displayParam = BYPASSFX;
			if (ref) *ref = &track->bypass[paramIndex];
			break;
		}
		case BYPASSSTEREOFX: {
			subType = tsl::parameters::EventSubtype::bypassStereoFx;
			paramIndex = GASSTFX;
			if (displayParam) *displayParam = BYPASSSTEREOFX;
			if (ref) *ref = &track->bypass[paramIndex];
			break;
		}
		case POWERTRACK: {
			subType = tsl::parameters::EventSubtype::powerTrack;
			flags |= ToWorkerThread;
			if (displayParam) *displayParam = POWERTRACK;
			if (ref) *ref = &_STATE->params[tindex][POWERTRACK];
			break;
		}
		case FOLLOWERDESTPOWER: {
			subType = tsl::parameters::EventSubtype::followerPower;
			auto folindex = (int)_STATE->params[tindex][FOLLOWERDEST].load();
			paramIndex = folindex;
			if (ref) *ref = &_STATE->followerMap[tindex].at(folindex).envpower;
			if (displayParam) *displayParam = FOLLOWERDESTPOWER;
			break;
		}
		default:
			break;
		} // closes inner switch
		break;
	} // closes outer OFFGRAIN/OFFFX/... case block

	
#endif
	default: {
		eventType = tsl::parameters::Eventtype::paramUpdate;
		if (_STATE->parameters[id].paramOffset) {
			int offset = _STATE->params[tindex][_STATE->parameters[id].paramOffset].load();
			offset *= _STATE->parameters[id].offsetFact;
			paramIndex = id + offset;
		}
		else
			paramIndex = id;
		if (_STATE->parameters[id].flags & Param::HasOnChange) flags |= DoOnChange;
		if (displayParam) *displayParam = id;
		if (ref) *ref = &_STATE->params[tindex][paramIndex];
		break;
	}
	}
}

