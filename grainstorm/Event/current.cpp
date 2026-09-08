#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "track.h"
#include "waveform.h"

using namespace tsl::parameters;



MYFLOAT Event::getCurrentValue(tsl::AppState* _appState) const {

	switch (eventType) {
	case Preset: {
		if (value < 1)return 1;
		auto iv = static_cast<uint32_t>(value);
		return iv % 2 == 0 ? iv - 1 : iv + 1;
	}

	case FxOrder: {
		auto order = fxOrder;
		std::swap(order.newPos, order.oldPos);
		return std::bit_cast<MYFLOAT>(order);
	}
	case Eventtype::SpecialAction:
		return _STATE->params[trackIndex][paramIndex].load();

	case Eventtype::Rerender:
		return 1.;

	case Eventtype::TextEvent:
		return value;

#ifdef GRAINSTORM

	case tsl::parameters::Eventtype::Power: {

		switch (subType) {

		case powerGrainFx:

		case powerFx:

		case powerStereoFx:
			return std::bit_cast<double>(PowerState{ power.pos, static_cast<uint32_t>(_DATA->tracks[trackIndex]->fxpower[paramIndex].load())});

		case powerTrack:
			return _STATE->params[trackIndex][POWERTRACK].load();

		case bypassGrainFx:

		case bypassFx:

		case bypassStereoFx:
			return _DATA->tracks[trackIndex]->bypass[paramIndex].load();

		case followerPower: {

			auto& fol = _STATE->followerMap[trackIndex].at(paramIndex);
			return fol.envpower.load();

		}

		default: return 0.0;

		}

	}

	case lfoDest: {

		auto lfo = _DATA->tracks[trackIndex]->lfos[subType];
		return _DATA->tracks[trackIndex]->lfo[paramIndex].load() == lfo ? 1.0 : 0.0;

	}

	case lfoMin:
		return _STATE->controls[trackIndex][paramIndex].lfo_min.load();

	case lfoMax:
		return _STATE->controls[trackIndex][paramIndex].lfo_max.load();

	case Follower:

		switch (subType) {

		case followerMin:
			return _STATE->followerMap[trackIndex].at(paramIndex).min.load();

		case followerMax:
			return _STATE->followerMap[trackIndex].at(paramIndex).max.load();

		case followerGain:
			return _STATE->followerMap[trackIndex].at(paramIndex).gain.load();

		case followerAtt:
			return _STATE->followerMap[trackIndex].at(paramIndex).att.load();

		case followerDec:
			return _STATE->followerMap[trackIndex].at(paramIndex).rel.load();

		case followerSidechain:
			return _STATE->followerMap[trackIndex].at(paramIndex).source.load();

		default: return 0.0;

		}

		break;
	case Recording:
	{
		auto rec = _DATA->tracks[trackIndex]->filebuffer.load();
		auto state = rec ? rec->state.load() : nullptr;
		if (!(state && rec->poolHandle == poolHandle))return getDefaultValue(_STATE);
		switch (subType) {
		case offStart: return state->off_stop.load();
		case offStop:
			return state->off_start.load();
		case offset: return state->offset.load();
		case playbackDir: return  state->playbackDir.load();
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
		case offStartFromUi:
		case offStopFromUi:
		case offsetFromUi: {
			if (value < 1)return 1;
			auto iv = static_cast<uint32_t>(value);
			return iv % 2 == 0 ? iv - 1 : iv + 1;
		}
		case bounceType: return  state->bounceType.load();
		case waveformView: return  std::bit_cast<double>(state->waveformState.load());
		case recordingChange: break;
		default: return getDefaultValue(_STATE);
		}
	}
	case paramUpdate:
		switch (subType) {
		case EventSubtype::sequencerState: {

			SequencerState s{};
			s.grainscount = (int16_t)_STATE->params[trackIndex][GRAINSCOUNT];
			s.silencecount = (int16_t)_STATE->params[trackIndex][SILENCECOUNT];
			s.arpstep = (int8_t)_STATE->params[trackIndex][ARP_CYCLESTEP];
			s.currentnote = (int8_t)_STATE->params[trackIndex][GRAINSEQACTIVE];
			s.arpcycledir = (int8_t)_STATE->params[trackIndex][ARP_CYCLEDIR];

			return std::bit_cast<double>(s);
		}
		case EventSubtype::grainGenState: {
			GraingenState s{};
			s.count[0] = _STATE->params[trackIndex][GRAINGENCOUNT0];
			s.count[1] = _STATE->channels == 2 ? _STATE->params[trackIndex][GRAINGENCOUNT1].load() : s.count[0];
			return std::bit_cast<double>(s);
		}
		}

#endif
	default:
		return _STATE->params[trackIndex][paramIndex].load();
	}
}
