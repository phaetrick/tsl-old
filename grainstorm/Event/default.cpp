#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "track.h"
#include "waveform.h"
#include <cmath>

using namespace tsl::parameters;
#ifdef PLUGIN_MODE

#include <IPlugParamDefs.h>

int Event::getPluginIndex(tsl::AppState* _appState)const {
	// A base index of -1 (not automatable) must stay -1: adding trackIndex *
	// paramCount to it would alias a valid index on another track's last param.
	int base = -1;
	if (eventType == Eventtype::paramUpdate) base = _STATE->parameters[paramIndex].pluginIndex;
	else if (eventType == Eventtype::Power) {
		switch (subType) {

		case EventSubtype::bypassGrainFx:
		case EventSubtype::bypassFx:
		case EventSubtype::bypassStereoFx:
			base = tsl::iplug::BypassParams[paramIndex];
			break;
		case EventSubtype::powerGrainFx:
		case EventSubtype::powerFx:
		case EventSubtype::powerStereoFx:
			base = tsl::iplug::PowerParams[paramIndex];
			break;
		case EventSubtype::powerTrack:
			base = _STATE->parameters[POWERTRACK].pluginIndex;
			break;
		default: break;
		}

	}
	return base < 0 ? -1 : base + trackIndex * tsl::iplug::paramCount;
}
#endif

MYFLOAT Event::getDefaultValue(tsl::AppState* _appState) const {

	switch (eventType) {
	case Preset: return 1;
	case Eventtype::FxOrder: return std::bit_cast<double>(FxOrderState{ -100, -100 });
	case Eventtype::SpecialAction:
		return 1.;
	case Eventtype::Rerender:
		return 1.;
	case Eventtype::TextEvent:
		return 0;
#ifdef GRAINSTORM
	case tsl::parameters::Eventtype::Power: {
		switch (subType) {
		case tsl::parameters::EventSubtype::powerTrack:
			return trackIndex == 0 ? 1.0 : 0.0;
		case EventSubtype::powerFx:
		case EventSubtype::powerStereoFx:
		case EventSubtype::powerGrainFx:
			return std::bit_cast<double>(PowerState{ power.pos, 0 });
		default:
			return 0.0;
		}
	}
	case lfoDest:
		return (subType == 0 && paramIndex == GRAINSIZE) ||
			(subType == 1 && paramIndex == SPEED) ||
			(subType == 2 && paramIndex == PREGAIN) ? 1.0 : 0.0;
	case lfoMin:
		return _STATE->parameters[paramIndex].initvalue;
	case lfoMax:
		return _STATE->parameters[paramIndex].initvalue;
	case Follower:
		switch (subType) {
		case followerMin:
		case followerMax:
			return _STATE->parameters[paramIndex].initvalue;
		case followerGain:
			return _STATE->parameters[FOLLOWERGAIN].initvalue;
		case followerAtt:
			return _STATE->parameters[FOLLOWERATT].initvalue;
		case followerDec:
			return _STATE->parameters[FOLLOWERREL].initvalue;
		case followerSidechain:
			return trackIndex;
		default:
			return 0.0;
		}
	case Recording:
		switch (subType) {
		case offset:
		case offStart:
		case offStop:
		case offsetFromUi:
		case offStartFromUi:
		case offStopFromUi:
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
		case LoopSave8:
			return 0;
		case playDirFromUi:
		case playbackDir: return 1.;
		case bounceType: return NO_BOUNCE;
		case waveformView: return std::bit_cast<double>(WaveformState{ 0., 1. });

		default: return std::bit_cast<double>(_STATE);

		}
	case paramUpdate:
		// Every param TRACK::setDefaults() seeds with the track index must report
		// that same index as its default. Claiming 0 here has two consequences:
		// the host's IParam is initialised to the wrong value on tracks 1..3, and
		// Snapshot::addEvent erases any event whose value equals the default --
		// so selecting track 1 as the source on track 2 recorded nothing and
		// setDefaults() put it back to 2 on the next load. Keep this list in sync
		// with TRACK::setDefaults().
		if (paramIndex == DISTRSOURCE || paramIndex == MONOCOMPSRC ||
			paramIndex == PITCHDETECTFXTRACK || paramIndex == FOLLOWER1SRC ||
			paramIndex == FOLLOWER2SRC || paramIndex == FOLLOWER3SRC)
			return trackIndex;
		switch (subType) {
		case EventSubtype::sequencerState: {
			return std::bit_cast<double>(SequencerState{});
		}
		case EventSubtype::grainGenState: {
			return std::bit_cast<double>(GraingenState{});
		}
		}
#endif
	default:
		return _STATE->parameters[paramIndex].initvalue;
	}
}


uint16_t Event::getDisplayParam() const {

	switch (eventType) {
	case tsl::parameters::Eventtype::Power: {
		switch (subType) {
		case tsl::parameters::EventSubtype::powerTrack:
			return POWERTRACK;
		case EventSubtype::powerFx: return OFFFX;
		case EventSubtype::powerStereoFx: return OFFSTEREOFX;
		case EventSubtype::powerGrainFx: return OFFGRAIN;
		case EventSubtype::followerPower: return FOLLOWERDESTPOWER;
		default: break;
		}
	}
										  break;
	case lfoDest:
		return LFODESTPOWER;
	case lfoMin:
		return paramIndex;
	case lfoMax:
		return paramIndex;
	case Follower:
		switch (subType) {
		case followerMin:
		case followerMax:
			return paramIndex;
		case followerGain:
			return FOLLOWERGAIN;
		case followerAtt:
			return FOLLOWERATT;
		case followerDec:
			return FOLLOWERREL;
		case followerSidechain:
			return FOLLOWERSRC;
		default:
			break;
		}
		break;

	case Preset:
	case FxOrder: break;

	case paramUpdate:
		switch (subType) {
		case EventSubtype::sequencerState:
		case EventSubtype::grainGenState: return 0;
		default: break;
		}
		return paramIndex;
	case LoopFromExt:return paramIndex;
	default: break;
	}

	return 0;
}

void Event::toNormalized(tsl::AppState *_appState)  {
	switch (eventType) {
	case Recording:
	case lfoDest:
	case Preset:
	case FxOrder:
	case LoopFromExt:return;
	case tsl::parameters::Eventtype::Power: {
		switch (subType) {
		case EventSubtype::followerPower:
		case tsl::parameters::EventSubtype::powerTrack:
			return ;
		case EventSubtype::powerFx:
		case EventSubtype::powerStereoFx:
		case EventSubtype::powerGrainFx:
			//value = PowerState::normalize(power);
			return;
		default: break;
		}
	}
	break;
	case paramUpdate:
		switch (subType) {
		case EventSubtype::sequencerState:
		case EventSubtype::grainGenState: return;
		default: break;
		}
	case Follower:
	case lfoMin:
	case lfoMax: {
		auto& m = _STATE->parameters[getDisplayParam()];
		value = m.toNormalized(value);
	}
	break;
	default: break;
	}
}
void Event::fromNormalized(tsl::AppState* _appState) {

	switch (eventType) {
	case Recording:
	case lfoDest:
	case Preset:
	case FxOrder:
	case LoopFromExt:return;
	case tsl::parameters::Eventtype::Power: {
		switch (subType) {
		case EventSubtype::followerPower:
		case tsl::parameters::EventSubtype::powerTrack:
			return;
		case EventSubtype::powerFx:
		case EventSubtype::powerStereoFx:
		case EventSubtype::powerGrainFx:
			//power = PowerState::denormalize(value);
			return;
		default: break;
		}
	}
										  break;
	case paramUpdate:
		switch (subType) {
		case EventSubtype::sequencerState:
		case EventSubtype::grainGenState: return;
		default: break;
		}
	case Follower:
	case lfoMin:
	case lfoMax: {
		auto& m = _STATE->parameters[getDisplayParam()];
		value = m.fromNormalized(value);
		// Presets written before per-point min/max were initialized contain inf/NaN
		if (!std::isfinite(value)) value = m.initvalue;
	}
    break;
	default: break;
	}
}

