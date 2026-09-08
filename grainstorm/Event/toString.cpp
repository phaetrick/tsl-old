#include "app.h"
#include "params.h"
#include "grainstorm.h"

using namespace tsl::parameters;

const char* getFxName(uint8_t subType, uint16_t paramIndex) {
	const auto& values = subType == tsl::parameters::EventSubtype::powerGrainFx || subType == tsl::parameters::EventSubtype::bypassGrainFx ? grainmod2values : subType == tsl::parameters::EventSubtype::powerFx || subType == tsl::parameters::EventSubtype::bypassFx ? fxtypes2values : reverbtypevalues;
	const auto& names = subType == tsl::parameters::EventSubtype::powerGrainFx || subType == tsl::parameters::EventSubtype::bypassGrainFx ? grainmods2 : subType == tsl::parameters::EventSubtype::powerFx || subType == tsl::parameters::EventSubtype::bypassFx ? fxtypes2 : reverbtypes;
	return names[findIndexFloat(values, paramIndex)].data();
}

static void offsetToString(int& pos, char buffer[], size_t bufferSize, size_t offset, double sr) {
	double secs = offset / sr;
	int min = (int)(secs / 60);
	int s = (int)(secs) % 60;
	int ms = (int)(secs * 1000) % 1000;
	pos += snprintf(buffer + pos, bufferSize - pos, "%02d:%02d:%03d", min, s, ms);
	pos = std::min(pos, (int)bufferSize - 1);
}

static bool checkFileValid(tsl::AppState* _appState, int& pos, char buffer[], size_t bufferSize, Event& e) {
	if (e.poolHandle == tsl::INVALID_POOL_HANDLE) {
		pos += snprintf(buffer + pos, bufferSize - pos, ": Invalid PoolHandle");
		pos = std::min(pos, (int)bufferSize - 1);
		return false;
	}

	else if (e.poolHandle == tsl::NO_SOUND_PRESENT) {
		pos += snprintf(buffer + pos, bufferSize - pos, ": CLEAR");
		pos = std::min(pos, (int)bufferSize - 1);
		return false;
	}
	else {
		auto rec = _DATA->tracks[e.trackIndex]->filebuffer.load();
		if (!rec) {
			pos += snprintf(buffer + pos, bufferSize - pos, ": No sound present");
			pos = std::min(pos, (int)bufferSize - 1);
			return false;
		}
		if (rec->poolHandle != e.poolHandle) {
			pos += snprintf(buffer + pos, bufferSize - pos, ": Target Mismatch");
			pos = std::min(pos, (int)bufferSize - 1);
			return false;
		};

		auto& pd = _STATE->recordings[e.poolHandle];
		std::lock_guard lk(pd.mutex);
		if (!pd.isValid()) {
			pos += snprintf(buffer + pos, bufferSize - pos, ": File removed from swap", pd.numEdits, _STATE->parameters[pd.lastEdit].name);
			pos = std::min(pos, (int)bufferSize - 1);
			return false;
		}


	}
	return true;
}

void Event::toString(tsl::AppState* _appState, int& pos, char buffer[], size_t bufferSize, bool renderValue) {

	switch (eventType)
	{
	case LoopFromExt: {
		pos += snprintf(buffer + pos, bufferSize - pos, "%s ", _STATE->parameters[paramIndex].category);
		pos = std::min(pos, (int)bufferSize - 1);
		pos += snprintf(buffer + pos, bufferSize - pos, "%s", _STATE->parameters[paramIndex].name);
		pos = std::min(pos, (int)bufferSize - 1);
		if (renderValue) {
			pos += snprintf(buffer + pos, bufferSize - pos, ": ");
			pos = std::min(pos, (int)bufferSize - 1);

			auto rec = _DATA->tracks[trackIndex]->filebuffer.load();
			auto state = rec ? rec->state.load() : nullptr;
			if (!state) {
				pos += snprintf(buffer + pos, bufferSize - pos, "EMPTRY TRACK");
				pos = std::min(pos, (int)bufferSize - 1);
				return;
			}
			else {
				auto ns = *state;
				size_t samplePos = 0;
				switch (paramIndex) {
				case LOOPSTARTFROMEXT: {
					samplePos = ns.off_stop.load() * value;
					break;
				}

				case LOOPSTOPFROMEXT: {
					samplePos = ns.off_start.load() + (rec->off - ns.off_start.load()) * value;
					break;
				}

				case LOOPPOSFROMEXT: {
					samplePos = ns.off_start.load() + (ns.off_stop.load() - ns.off_start.load()) * value;
					break;
				}
				default:break;
				}
				offsetToString(pos, buffer, bufferSize, samplePos, _STATE->sr);
				pos = std::min(pos, (int)bufferSize - 1);
			}
		}
	}
					break;
	case Preset: {
		pos += snprintf(buffer + pos, bufferSize - pos, "STATE LOAD %s", ((uint32_t)value) & 1 ? "REDO" : "UNDO");
		pos = std::min(pos, (int)bufferSize - 1);
	}
			   break;

	case FxOrder: {
		pos += snprintf(buffer + pos, bufferSize - pos, "POSITION %s", getFxName(subType, paramIndex));
		pos = std::min(pos, (int)bufferSize - 1);
		if (renderValue) {
			pos += snprintf(buffer + pos, bufferSize - pos, ": %d", fxOrder.newPos + 1);
			pos = std::min(pos, (int)bufferSize - 1);
		}
	}
				break;

	case Recording: {
		switch (subType) {
		case LoopLoad1:
		case LoopLoad2:
		case LoopLoad3:
		case LoopLoad4:
		case LoopLoad5:
		case LoopLoad6:
		case LoopLoad7:
		case LoopLoad8: {
			pos += snprintf(buffer + pos, bufferSize - pos, "%s", _STATE->parameters[LOOPLOAD0 + subType].name);
			pos = std::min(pos, (int)bufferSize - 1);
			if (!checkFileValid(_STATE, pos, buffer, bufferSize, *this))
				return;
			else {
				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				if (pd.loadUndoRedos[trackIndex].size() < (int)value) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": Data not avail at %d / %d", (int)value, pd.loadUndoRedos[trackIndex].size());
					pos = std::min(pos, (int)bufferSize - 1);
				}
			}
			return;
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
			pos += snprintf(buffer + pos, bufferSize - pos, "%s", _STATE->parameters[LOOPSAVE0 + subType - LoopSave1].name);
			pos = std::min(pos, (int)bufferSize - 1);
			if (!checkFileValid(_STATE, pos, buffer, bufferSize, *this))
				return;
			else {
				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				if (pd.loadUndoRedos[trackIndex].size() < (int)value) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": Data not avail at %d / %d", (int)value, pd.loadUndoRedos[trackIndex].size());
					pos = std::min(pos, (int)bufferSize - 1);
				}
			}
			return;
		}
					  break;
		case recordingChange:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOAD");
			break;
		case offStart:
		case offStartFromUi:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOOP START");
			break;
		case offStop:
		case offStopFromUi:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOOP STOP");
			break;
		case playbackDir:
		case playDirFromUi:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOOP DIR");
			break;
		case bounceType:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOOP MODE");
			break;
		case offset:
		case offsetFromUi:
			pos += snprintf(buffer + pos, bufferSize - pos, "LOOP READPOS");
			break;
		case waveformView:
			pos += snprintf(buffer + pos, bufferSize - pos, "WAVEFORM VIEW");
			break;
		default: break;
		}
		pos = std::min(pos, (int)bufferSize - 1);

		if (renderValue) {

			if (subType == recordingChange) {
				if (poolHandle == tsl::INVALID_POOL_HANDLE) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": Invalid PoolHandle");
					pos = std::min(pos, (int)bufferSize - 1);
					break;
				}

				else if (poolHandle == tsl::NO_SOUND_PRESENT) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": CLEAR");
					pos = std::min(pos, (int)bufferSize - 1);
					break;
				}


				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				const std::string& path = pd.fileName;
				std::string name;
				if (path.empty() || path == noSoundLoaded)
					name = "FROM SCRATCH";
				else if (path == presetHasAudio)
					name = "FROM PRESET";
				else {
					name = path;
					auto slash = path.find_last_of("/\\");
					if (slash != std::string::npos) name = path.substr(slash + 1);
					auto dot = name.find_last_of('.');
					if (dot != std::string::npos) name = name.substr(0, dot);
					if (name.size() > 20) { name = name.substr(0, 20); name += "..."; }
				}
				pos += snprintf(buffer + pos, bufferSize - pos, ": %s", name.empty() ? "EMPTY" : name.c_str());
				pos = std::min(pos, (int)bufferSize - 1);
				if (pd.numEdits > 0) {
					pos += snprintf(buffer + pos, bufferSize - pos, " EDIT #%d: %s", pd.numEdits, _STATE->parameters[pd.lastEdit].name);
					pos = std::min(pos, (int)bufferSize - 1);
				}
				if (!pd.isValid()) {
					pos += snprintf(buffer + pos, bufferSize - pos, " - Removed from swap", pd.numEdits, _STATE->parameters[pd.lastEdit].name);
					pos = std::min(pos, (int)bufferSize - 1);
				}
				return;
			}

			if (!checkFileValid(_STATE, pos, buffer, bufferSize, *this)) break; // break out of Recording case


			switch (subType) {
			case offsetFromUi:
			case offStopFromUi:
			case offStartFromUi: {
				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				if (pd.waveformUndoRedos[trackIndex].size() < (int)value) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": Data not avail at %d / %d", (int)value, pd.waveformUndoRedos[trackIndex].size());
					pos = std::min(pos, (int)bufferSize - 1);
					break;
				}
				auto& ur = pd.waveformUndoRedos[trackIndex][(int)value - 1];
				pos += snprintf(buffer + pos, bufferSize - pos, ": ");
				pos = std::min(pos, (int)bufferSize - 1);
				size_t samplePos = subType == offsetFromUi ? ur.offset : ur.loopBound;
				offsetToString(pos, buffer, bufferSize, samplePos, _STATE->sr);
				pos = std::min(pos, (int)bufferSize - 1);

				break;
			}
			case offset:
			case offStop:
			case offStart:
				pos += snprintf(buffer + pos, bufferSize - pos, ": ");
				pos = std::min(pos, (int)bufferSize - 1);
				offsetToString(pos, buffer, bufferSize, value, _STATE->sr);
				break;
			case playbackDir:
			case playDirFromUi:
				pos += snprintf(buffer + pos, bufferSize - pos, value == 1.0 ? ": FORW" : ": BACKW");
				break;
			case bounceType: {
				const char* bt[] = { "NORMAL", "PING PONG", "SPECIAL" };
				pos += snprintf(buffer + pos, bufferSize - pos, ": %s", bt[(int)value]);
				break;
			}
			case waveformView: {
				auto& pd = _STATE->recordings[poolHandle];
				std::lock_guard lk(pd.mutex);
				pos += snprintf(buffer + pos, bufferSize - pos, ": ");
				pos = std::min(pos, (int)bufferSize - 1);
				offsetToString(pos, buffer, bufferSize, waveformState.startPos * pd.off, _STATE->sr);
				pos += snprintf(buffer + pos, bufferSize - pos, " / %.1fx", waveformState.zoom);
				break;
			}
			default: break;
			}
			pos = std::min(pos, (int)bufferSize - 1);
		}
		break;
	}

	case TextEvent: {
		switch (subType) {
		case EventSubtype::syncEvent:
			pos += snprintf(buffer + pos, bufferSize - pos, "%s %s", text, _STATE->parameters[paramIndex].name);
			pos = std::min(pos, (int)bufferSize - 1);
			break;
		default: // EventSubtype::normalTextEvent:
			pos += snprintf(buffer + pos, bufferSize - pos, "%s", text);
			pos = std::min(pos, (int)bufferSize - 1);
			break;
		}
		break;
	}

	case Follower: {
		pos += snprintf(buffer + pos, bufferSize - pos, "FOLLOWER ");
		pos = std::min(pos, (int)bufferSize - 1);

		auto& target = _STATE->parameters[paramIndex];
		int id{};
		if (target.category) {
			pos += snprintf(buffer + pos, bufferSize - pos, "%s ", target.category);
			pos = std::min(pos, (int)bufferSize - 1);
		}
		pos += snprintf(buffer + pos, bufferSize - pos, "%s ", target.name);
		pos = std::min(pos, (int)bufferSize - 1);
		switch (subType) {
		case tsl::parameters::EventSubtype::followerPower:
			pos += snprintf(buffer + pos, bufferSize - pos, "POWER");
			pos = std::min(pos, (int)bufferSize - 1);
			if (renderValue) {
				pos += snprintf(buffer + pos, bufferSize - pos, value == 1.0 ? " ON" : " OFF");
				pos = std::min(pos, (int)bufferSize - 1);
			}
			break;
		case tsl::parameters::EventSubtype::followerSidechain:
			pos += snprintf(buffer + pos, bufferSize - pos, "INPUT");
			if (renderValue) {
				pos += snprintf(buffer + pos, bufferSize - pos, ": %s", tracknames[(int)value].data());
				pos = std::min(pos, (int)bufferSize - 1);
			}
			break;
		case tsl::parameters::EventSubtype::followerMin:
			pos += snprintf(buffer + pos, bufferSize - pos, "BOUNDA");
			pos = std::min(pos, (int)bufferSize - 1);
			id = paramIndex;
			break;
		case tsl::parameters::EventSubtype::followerMax:
			pos += snprintf(buffer + pos, bufferSize - pos, "BOUNDB");
			pos = std::min(pos, (int)bufferSize - 1);
			id = paramIndex;
			break;

		case tsl::parameters::EventSubtype::followerGain:
			pos += snprintf(buffer + pos, bufferSize - pos, "GAIN");
			pos = std::min(pos, (int)bufferSize - 1);
			id = FOLLOWERGAIN;
			break;
		case tsl::parameters::EventSubtype::followerAtt:
			pos += snprintf(buffer + pos, bufferSize - pos, "ATTACK");
			pos = std::min(pos, (int)bufferSize - 1);
			id = FOLLOWERATT;
			break;
		case tsl::parameters::EventSubtype::followerDec:
			pos += snprintf(buffer + pos, bufferSize - pos, "RELEASE");
			pos = std::min(pos, (int)bufferSize - 1);
			id = FOLLOWERREL;
			break;
		default:
			pos += snprintf(buffer + pos, bufferSize - pos, "BUG: %d %g", subType, value);
			pos = std::min(pos, (int)bufferSize - 1);

			break;

		}

		if (renderValue && id != 0 && !(_STATE->parameters[id].flags & Param::NoValue)) {
			auto& miditarget = _STATE->parameters[id];
			auto display = miditarget.toDisplay(_appState->sr, value);
			pos += snprintf(buffer + pos, bufferSize - pos, ": %.*f", miditarget.digits, display);
			pos = std::min(pos, (int)bufferSize - 1);
			if (miditarget.valuename) {
				pos += snprintf(buffer + pos, bufferSize - pos, " %s", miditarget.valuename);
				pos = std::min(pos, (int)bufferSize - 1);
			}
		}

		break;
	}

	case tsl::parameters::Eventtype::paramUpdate:
		switch (subType) {
		case EventSubtype::grainGenState: {
			pos += snprintf(buffer + pos, bufferSize - pos, "GRAINGENSTATE");
			pos = std::min(pos, (int)bufferSize - 1);
			if (renderValue) {
				pos += snprintf(buffer + pos, bufferSize - pos, ": CountL: %d CountR: %d", graingenState.count[0], graingenState.count[1]);
				pos = std::min(pos, (int)bufferSize - 1);
			}
		}
										return;

		case EventSubtype::sequencerState: {
			pos += snprintf(buffer + pos, bufferSize - pos, "SEQSTATE");
			pos = std::min(pos, (int)bufferSize - 1);
			if (renderValue) {
				pos += snprintf(buffer + pos, bufferSize - pos, ": Note: %d Grains: %d Silence: %d Arpstep: %d Arpdir %d", sequencerState.currentnote, sequencerState.grainscount, sequencerState.silencecount, sequencerState.arpstep, sequencerState.arpcycledir);
				pos = std::min(pos, (int)bufferSize - 1);
			}
		}
										 return;
		default: break;
		}

	case tsl::parameters::Eventtype::lfoMin:
	case tsl::parameters::Eventtype::lfoMax:
	{
		auto& miditarget = _STATE->parameters[paramIndex];
		switch (eventType) {
		case tsl::parameters::Eventtype::paramUpdate:
			if (miditarget.category) {
				pos += snprintf(buffer + pos, bufferSize - pos, "%s ", miditarget.category);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			if (miditarget.subcategory) {
				pos += snprintf(buffer + pos, bufferSize - pos, "%s ", miditarget.subcategory);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			if (miditarget.name) {
				pos += snprintf(buffer + pos, bufferSize - pos, "%s", miditarget.name);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			break;
		case tsl::parameters::Eventtype::lfoMin:
		case tsl::parameters::Eventtype::lfoMax:
			pos += snprintf(buffer + pos, bufferSize - pos, "LFO TARGET ");
			pos = std::min(pos, (int)bufferSize - 1);

			{
				const auto& values = lfoValues;
				const auto& names = lfoItems;
				const char* name = names[findIndexFloat(values, paramIndex)].data();
				pos += snprintf(buffer + pos, bufferSize - pos, "%s", name);
				pos = std::min(pos, (int)bufferSize - 1);
			}

			pos += snprintf(buffer + pos, bufferSize - pos, eventType == lfoMin ? " BOUNDA" : " BOUNDB");
			pos = std::min(pos, (int)bufferSize - 1);
			break;
		default:
			break;
		}

		if (renderValue && !(miditarget.flags & Param::NoValue)) {
			if (miditarget.name && !strcmp(miditarget.name, "RATIO")) {
				if (value == 0.)
					pos += snprintf(buffer + pos, bufferSize - pos, ": BYPASS");
				else if (value == 1.0)
					pos += snprintf(buffer + pos, bufferSize - pos, ": INF : 1");
				else
					pos += snprintf(buffer + pos, bufferSize - pos, ": %.1f : 1", 1. / (1. - value));
				pos = std::min(pos, (int)bufferSize - 1);
			}
			else {
				auto display = miditarget.toDisplay(_appState->sr, value);
				if (miditarget.type == ParameterType_enum && (int)display < miditarget.names.size()) {
					pos += snprintf(buffer + pos, bufferSize - pos, ": %s", miditarget.names[(int)display].data());
				}
				else if (miditarget.type == ParameterType_bool) {
					if (!miditarget.names.empty()) {
						if ((int)display < miditarget.names.size())
							pos += snprintf(buffer + pos, bufferSize - pos, ": %s", miditarget.names[(int)display].data());
					}
					else
						pos += snprintf(buffer + pos, bufferSize - pos, ": %s", display == 0. ? "OFF" : "ON");
				}
				else {
					pos += snprintf(buffer + pos, bufferSize - pos, ": %.*f", miditarget.digits, display);
					pos = std::min(pos, (int)bufferSize - 1);
					if (miditarget.valuename) {
						pos += snprintf(buffer + pos, bufferSize - pos, " %s", miditarget.valuename);
						pos = std::min(pos, (int)bufferSize - 1);
					}
				}
			}
		}
		break;
	}

	case tsl::parameters::Eventtype::Power: {
		switch (subType) {
		case tsl::parameters::EventSubtype::followerPower: {
			pos += snprintf(buffer + pos, bufferSize - pos, "FOLLOWER ");
			pos = std::min(pos, (int)bufferSize - 1);

			auto& target = _STATE->parameters[paramIndex];
			int id{};
			if (target.category) {
				pos += snprintf(buffer + pos, bufferSize - pos, "%s ", target.category);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			pos += snprintf(buffer + pos, bufferSize - pos, "%s ", target.name);
			pos = std::min(pos, (int)bufferSize - 1);
			pos += snprintf(buffer + pos, bufferSize - pos, "POWER");
			pos = std::min(pos, (int)bufferSize - 1);
			if (renderValue) {
				auto pow = value == 1.0 ? "ON" : "OFF";
				pos += snprintf(buffer + pos, bufferSize - pos, " %s", pow);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			break;

		}
		default: {
			const char* type2{};
			auto type = subType <= tsl::parameters::EventSubtype::powerGrainFx || subType == tsl::parameters::EventSubtype::powerTrack ? "POWER" : "BYPASS";
			if (subType == tsl::parameters::EventSubtype::powerTrack) type2 = "TRACK";
			else {
				type2 = getFxName(subType, paramIndex);
			}
			pos += snprintf(buffer + pos, bufferSize - pos, "%s %s", type2, type);
			pos = std::min(pos, (int)bufferSize - 1);
			if (renderValue) {
				double renderValue = 0.;
				switch (subType) {
				case powerFx:
				case powerGrainFx:
				case powerStereoFx:
					renderValue = power.pow;
					break;
				default: renderValue = value;
					break;
				}
				auto pow = renderValue == 1.0 ? "ON" : "OFF";
				pos += snprintf(buffer + pos, bufferSize - pos, " %s", pow);
				pos = std::min(pos, (int)bufferSize - 1);
			}
			break;
		}

		}

		break;
	}
	case tsl::parameters::Eventtype::lfoDest: {
		const auto& values = lfoValues;
		const auto& names = lfoItems;
		const char* type2 = names[findIndexFloat(values, paramIndex)].data();
		auto lfoname = _DATA->track1.lfos[subType]->name;
		pos += snprintf(buffer + pos, bufferSize - pos, "%s TARGET %s", lfoname, type2);
		pos = std::min(pos, (int)bufferSize - 1);
		if (renderValue) {
			auto pow = value == 1.0 ? "ON" : "OFF";
			pos += snprintf(buffer + pos, bufferSize - pos, " %s", pow);
			pos = std::min(pos, (int)bufferSize - 1);
		}
		break;
	}
	case tsl::parameters::Eventtype::Rerender: {
		pos += snprintf(buffer + pos, bufferSize - pos, "Rerender Track: %d %s", trackIndex, _STATE->parameters[paramIndex].name != nullptr ? _STATE->parameters[paramIndex].name : "Null");
		pos = std::min(pos, (int)bufferSize - 1);
		break;
	}
	case tsl::parameters::Eventtype::SpecialAction: {
		pos += snprintf(buffer + pos, bufferSize - pos, "SpecialAction Track: %d Par: %d  %g", trackIndex, paramIndex, value);
		pos = std::min(pos, (int)bufferSize - 1);
		break;
	}
	default:
		break;
	}
}