#include "defines.h"
#include "tools/PlatformPaths.h"

#ifdef __ANDROID__
#include "DecoderAndroid.h"
#else
#include "settings.h"
#include "DecoderWindows.h"
#endif

#include "preset.h"
#include "track.h"
#include "waveform.h"
#include "grainstorm.h"
#include "button.h"
#include "convolver.h"


#include <memory>
#include <filesystem>
#include <fstream>

#ifndef off_t
#define off_t long
#endif
#include "Effects/gaintask.h"

#include "flac.h"
#include "presetdef.h"

bool sortbysec(const std::pair<int, int>& a,
	const std::pair<int, int>& b) {
	return (a.second < b.second);
}

struct NewHeader {
	char header[3]{};
	int32_t version{};
	char name[100]{};
	bool import_audio{};
	long date{};
	uint8_t num_track{};
	char audio_file_path[4096]{};
	int32_t numparameters{};
	long off{};
};

bool
tsl::preset::loadPresetThreadFunc(TRACK* track, std::shared_ptr<PresetWrapper> h) {
	auto _appState = track->_appState;

	tsl::FileWrapper wrapper;
	if (h->presetMemory.empty()) {
#ifdef __ANDROID__
		ATTACH
			FILE* fd = tsl::app::getFdFromUri(h->path, "rb");
		DETACH
#else
		FILE* fd = fopen(h->path.data(), "rb");
#endif
		if (!fd) {
			showToast(_STATE, "Load Preset: Could not open preset file.");
			return false;
		}

		if (wrapper.init(fd, true) != 0) {
			showToast(_STATE, "Load Preset: Wrapper init failed.");
			return false;
		}
	}
	else {
		if (wrapper.init(const_cast<unsigned char*>(h->presetMemory.data()), (int)h->presetMemory.size())
			!= 0) {
			showToast(_STATE, "Load Preset: Wrapper init failed.");
			return false;
		}
	}

	std::vector<PresetWrapper> presets;
	std::vector<TRACK*> tracks;

	if (h->isProject) {
		for (auto tt : _DATA->tracks)
			tracks.push_back(tt);
	}
	else {
		tracks.push_back(track);
	}

	for (auto t : tracks) {
		PresetWrapper ptl{};

		ptl.track = t;
		ptl.version = h->version;
		ptl.isProject = h->isProject;
		ptl.off = _STATE->sr * 420;

		if (ptl.version < 20) {
			PRESET_HEADER header;
			long offset{};
			if (offset = wrapper.tell() < 0 ||
				wrapper.read(&header, sizeof(PRESET_HEADER), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			ptl.version = header.version;
			ptl.audioFilePath = header.audio_file_path;

			if (wrapper.seek(offset, SEEK_SET)) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

		}
		else {
			PresetHeader2023 header;
			if (wrapper.read(&header, sizeof(PresetHeader2023), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (header.audioPathLen > 0) {
				wrapper.seek(header.namelen, SEEK_CUR);
				std::string tmp(header.audioPathLen, '\0');
				if (wrapper.read(tmp.data(), header.audioPathLen, 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				ptl.audioFilePath = tmp;
				ptl.off = header.off;
			}
		}

		std::string name;
		if (!ptl.audioFilePath.empty()) {
#ifdef __ANDROID__
			ATTACH
				auto str = (jstring)env->CallStaticObjectMethod(tsl::android::activityclass,
					env->GetStaticMethodID(
						tsl::android::activityclass,
						"getFilePath",
						"(Ljava/lang/String;Z)Ljava/lang/String;"),
					env->NewStringUTF(
						ptl.audioFilePath.c_str()),
					ptl.isProject);
			if (str != nullptr) {
				const char* jcVal = env->GetStringUTFChars(str, JNI_FALSE);
				name.append(jcVal);
				env->ReleaseStringUTFChars(str, jcVal);
			}
			DETACH
#else
			if (ptl.audioFilePath != noSoundLoaded)
				name = ptl.audioFilePath;
#endif
		}

		if (!name.empty()) {
			if (ptl.version >= 17 && ptl.version < 20) {
				NewHeader newHeader;
				long offset{};
				if (offset = wrapper.tell() < 0 ||
					wrapper.read(&newHeader, 1, sizeof(NewHeader) != sizeof(NewHeader))) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				if (wrapper.seek(offset, SEEK_SET)) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				ptl.off = newHeader.off;
			}

			if (name != presetHasAudio) {
				if (ptl.off > 0 && ptl.off <= _STATE->sr * 420) {
					ptl.rec = dec(t, name, ptl.off);
				}
				else {
					ptl.rec = dec(t, name, _STATE->sr * 420);
				}
				if (!ptl.rec) {
					showToast(_STATE, "Error loading audio");
				}
				else if (ptl.rec->off == 0 && !ptl.rec->statusMessage.empty()) {
					showToast(_STATE, ptl.rec->statusMessage.c_str());
				}
			}
			ptl.importAudio = true;
		}
		if (17 == ptl.version) {
			ptl.resize<Preset17>();
			auto pr = ptl.get<Preset17>();
			if (wrapper.read(pr, 1, sizeof(Preset17)) != sizeof(Preset17)) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			pr->import_a = ptl.importAudio;
		}
		else if (18 == ptl.version) {
			ptl.resize<Preset18>();
			auto pr = ptl.get<Preset18>();
			if (wrapper.read(pr, sizeof(Preset18), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			pr->import_a = ptl.importAudio;
			ptl.bypass.resize(pr->numfx);
			ptl.fxpower.resize(pr->numfx);
			ptl.q_pos.resize(pr->numfx);

			std::vector<float> tmp;
			tmp.resize(pr->numparameters);
			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ NormalParam, static_cast<uint16_t>(i), tmp[i] });

			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ LFOmin, static_cast<uint16_t>(i), tmp[i] });

			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ LFOmax, static_cast<uint16_t>(i), tmp[i] });

			for (auto& val : ptl.bypass) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.fxpower) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.q_pos) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
		}
		else if (19 == ptl.version) {
			ptl.resize<Preset19>();
			auto pr = ptl.get<Preset19>();
			if (wrapper.read(pr, sizeof(Preset19), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			ptl.bypass.resize(pr->numfx);
			ptl.fxpower.resize(pr->numfx);
			ptl.q_pos.resize(pr->numfx);
			ptl.lfo1.resize(pr->lfo1targets);
			ptl.lfo2.resize(pr->lfo2targets);
			ptl.lfo3.resize(pr->lfo3targets);

			std::vector<float> tmp;
			tmp.resize(pr->numparameters);
			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ NormalParam, static_cast<uint16_t>(i), tmp[i] });

			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ LFOmin, static_cast<uint16_t>(i), tmp[i] });

			if (wrapper.read(tmp.data(), sizeof(float), pr->numparameters) != pr->numparameters) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			for (int32_t i = 0; i < tmp.size(); i++)
				ptl.params.push_back({ LFOmax, static_cast<uint16_t>(i), tmp[i] });

			for (auto& val : ptl.bypass) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.fxpower) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.q_pos) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.lfo1) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.lfo2) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
			for (auto& val : ptl.lfo3) {
				float temp;
				if (wrapper.read(&temp, sizeof(temp), 1) != 1) {
					showToast(_STATE, errorReadingPreset);
					return false;
				}
				else {
					val = temp;
				}
			}
		}
		else if (20 == ptl.version) {
			ptl.resize<Preset20>();
			auto pr = ptl.get<Preset20>();

			if (wrapper.read(pr, sizeof(Preset20), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			ptl.params.resize(pr->numparams);
			ptl.bypass.resize(pr->numbypass);
			ptl.fxpower.resize(pr->numfx);
			ptl.lfo1.resize(pr->numlfo1targets);
			ptl.lfo2.resize(pr->numlfo2targets);
			ptl.lfo3.resize(pr->numlfo3targets);

			if (wrapper.read(ptl.params.data(), sizeof(PresetParam), pr->numparams) !=
				pr->numparams) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			if (wrapper.read(ptl.bypass.data(), sizeof(uint16_t), ptl.bypass.size()) !=
				ptl.bypass.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.fxpower.data(), sizeof(uint16_t), ptl.fxpower.size()) !=
				ptl.fxpower.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			if (wrapper.read(ptl.lfo1.data(), sizeof(uint16_t), ptl.lfo1.size()) !=
				ptl.lfo1.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.lfo2.data(), sizeof(uint16_t), ptl.lfo2.size()) !=
				ptl.lfo2.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.lfo3.data(), sizeof(uint16_t), ptl.lfo3.size()) !=
				ptl.lfo3.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}


			if (name == presetHasAudio) {
				FlacDecoder decoder;
				if ((ptl.rec = decoder.init(&wrapper, t, pr->end)) == nullptr) {
					std::string msg = t->name;
					msg.append(": Error decoding audio included in preset.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
				}
				else if (ptl.rec->off == 0) {
					std::string msg = t->name;
					msg.append(": No samples decoded.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
					ptl.rec = nullptr;
				}
				else {
					ptl.importAudio = true;
				}
			}


			if (wrapper.seek(pr->end, SEEK_SET) < 0) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
		}
		else if (21 == ptl.version) {
			ptl.resize<Preset21>();
			auto pr = ptl.get<Preset21>();

			if (wrapper.read(pr, sizeof(Preset21), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			ptl.params.resize(pr->numparams);
			ptl.bypass.resize(pr->numbypass);
			ptl.fxpower.resize(pr->numfx);
			ptl.lfo1.resize(pr->numlfo1targets);
			ptl.lfo2.resize(pr->numlfo2targets);
			ptl.lfo3.resize(pr->numlfo3targets);
			ptl.followerParams.resize(pr->numfollowers);

			if (wrapper.read(ptl.params.data(), sizeof(PresetParam), pr->numparams) !=
				pr->numparams) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			if (wrapper.read(ptl.bypass.data(), sizeof(uint16_t), ptl.bypass.size()) !=
				ptl.bypass.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.fxpower.data(), sizeof(uint16_t), ptl.fxpower.size()) !=
				ptl.fxpower.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

			if (wrapper.read(ptl.lfo1.data(), sizeof(uint16_t), ptl.lfo1.size()) !=
				ptl.lfo1.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.lfo2.data(), sizeof(uint16_t), ptl.lfo2.size()) !=
				ptl.lfo2.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.lfo3.data(), sizeof(uint16_t), ptl.lfo3.size()) !=
				ptl.lfo3.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (wrapper.read(ptl.followerParams.data(), sizeof(FollowerParams),
				ptl.followerParams.size()) !=
				ptl.followerParams.size()) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (name == presetHasAudio) {
				FlacDecoder decoder;
				if ((ptl.rec = decoder.init(&wrapper, t, pr->end)) == nullptr) {
					std::string msg = t->name;
					msg.append(": Error decoding audio included in preset.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
				}
				else if (ptl.rec->off == 0) {
					std::string msg = t->name;
					msg.append(": No samples decoded.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
					ptl.rec = nullptr;
				}
				else {
					ptl.importAudio = true;
				}
			}

			if (wrapper.seek(pr->end, SEEK_SET) < 0) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}

		}
		else if (22 == ptl.version) {
			ptl.resize<Preset22>();
			auto pr = ptl.get<Preset22>();

			if (wrapper.read(pr, sizeof(Preset22), 1) != 1) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
			if (pr->numEvents > 0) {
				ptl.events.resize(pr->numEvents);
				if (wrapper.read(ptl.events.data(), sizeof(tsl::parameters::Event), pr->numEvents) !=
					pr->numEvents) {
					showToast(_STATE, errorReadingPreset);
					return false;
				};
			}
			if (name == presetHasAudio) {
				FlacDecoder decoder;
				if ((ptl.rec = decoder.init(&wrapper, t, pr->end)) == nullptr) {
					std::string msg = t->name;
					msg.append(": Error decoding audio included in preset.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
				}
				else if (ptl.rec->off == 0) {
					std::string msg = t->name;
					msg.append(": No samples decoded.");
					showToast(_STATE, msg.c_str());
					ptl.importAudio = false;
					ptl.rec = nullptr;
				}
				else {
					ptl.importAudio = true;
				}
			}
			if (wrapper.seek(pr->end, SEEK_SET) < 0) {
				showToast(_STATE, errorReadingPreset);
				return false;
			}
		}

		presets.push_back(std::move(ptl));
	}

	_DATA->snapShot.add_taskInt([_STATE, presets = std::move(presets)] mutable {
		std::vector<TRACK*> gainTracks{};
		auto isPlaying = _STATE->player.isPlaying();
		if (isPlaying) {
			for (auto& pr : presets) {
				if (_STATE->params[pr.track->index][POWERTRACK] == 1.0) {
					gainTracks.push_back(pr.track);
				}
			}
			if (gainTracks.size() > 0) {
				for (auto t : gainTracks) {
					t->gainTask.setTarget(0, -120);
				}
				_STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
				auto token = _STATE->waitNotify.begin_wait();
				_DATA->toAudioThreadQueue.try_push([_STATE, gainTracks, token] mutable {
					for (auto t : gainTracks) {
						t->disabled = true;
					}
					_STATE->waitNotify.complete(token);
					});
				_STATE->waitNotify.wait_for_signal(token, tsl::gainTaskFadeMs);

			}
		}

		while (auto e = _DATA->snapShot.queue.try_pop()) {
			e->flags &= ~tsl::parameters::Event::History;
			_DATA->snapShot.addEvent(std::move(*e));
		}

		auto oldSnap = _DATA->snapShot.get();

		std::vector<tsl::parameters::Event> fileEvents;
		for (auto& pr : presets) {
			fileEvents.push_back(std::move(pr.track->currentAudioToEvent()));
			_DATA->snapShot.clear(pr.track->index);
			tsl::preset::loadPreset(pr);
			fileEvents.push_back(std::move(pr.track->currentAudioToEvent()));
		}

		auto newSnap = _DATA->snapShot.get();

		_DATA->snapShot.OnPresetLoaded(oldSnap, newSnap, fileEvents);

		if (isPlaying && gainTracks.size() > 0)
			_DATA->toAudioThreadQueue.try_push([_STATE, gainTracks] {
			for (auto t : gainTracks) {
				t->disabled = false;
				t->gainTask.setTarget(-120, 0);
			}
				});
		});
	return true;
}


void setLoopPositions(tsl::preset::PresetWrapper& ptl, std::shared_ptr<tsl::Recording>& rec) {
	auto state = rec ? rec->state.load() : nullptr;
	if (ptl.importAudio && state != nullptr) {
		auto _appState = ptl.track->_appState;
		auto tindex = ptl.track->index;
		for (int i = 0; i < 8; i++) {
			rec->positions[i][tsl::LoopStart] = _STATE->params[tindex][LOOPSTART0 + i * 5].load();
			rec->positions[i][tsl::LoopStop] = _STATE->params[tindex][LOOPSTOP0 + i * 5].load();
			rec->positions[i][tsl::ReadPos] = _STATE->params[tindex][LOOPPOS0 + i * 5].load();
			rec->positions[i][tsl::BounceType] = _STATE->params[tindex][LOOPTYPE0 + i * 5].load();
			rec->positions[i][tsl::PlaybackDirection] = _STATE->params[tindex][LOOPDIR0 + i * 5].load();
			rec->positions[i][tsl::WaveformStartPosition] = _STATE->params[tindex][WAVEFORMPOS01 + i * 2].load();
			rec->positions[i][tsl::WaveformZoom] = _STATE->params[tindex][WAVEFORMZOOM01 + i * 2].load();
		}
		double oldLength = ptl.off;
		double newLength = rec->off;
		if (oldLength != newLength) {
			rec->off = oldLength; //adjust assumes off isnt updated yet
			rec->adjustPositions(oldLength, newLength - oldLength);
			rec->off = newLength;
		}

	}
}

template<typename T> void setLoopPositions(tsl::preset::PresetWrapper& ptl, std::shared_ptr<tsl::Recording>& rec) {
	auto state = rec ? rec->state.load() : nullptr;
	if (ptl.importAudio && state != nullptr) {
		auto pr = ptl.get<T>();
		state->off_start = pr->off_start;
		state->off_stop = pr->off_stop;
		state->offset = pr->offset;
		state->waveformState = { pr->waveformpos, pr->waveformzoom };
		state->bounceType = pr->bounce_type;
		state->playbackDir = pr->playbackspeed_dir;
		setLoopPositions(ptl, rec);
	}
}



// Attack/release params that were linear milliseconds up to preset version 21 and
// are Param::ParamCurve::Log10 (stored as 20*log10(ms)) from 22 on. Version 22 itself
// was never shipped, so everything below it needs converting on load.
static constexpr uint16_t kMsToLog10Params[] = {
	VOCATT, VOCREL,
	COMPATT, COMPREL,
	STCOMPATT, STCOMPDEC,
	MULTICOMPATTACK1, MULTICOMPATTACK2, MULTICOMPATTACK3,
	MULTICOMPRELEASE1, MULTICOMPRELEASE2, MULTICOMPRELEASE3,
	DYNEQ5ATT0, DYNEQ5ATT1, DYNEQ5ATT2, DYNEQ5ATT3, DYNEQ5ATT4,
	DYNEQ5REL0, DYNEQ5REL1, DYNEQ5REL2, DYNEQ5REL3, DYNEQ5REL4,
};

static void migrateMsParamsToLog10(tsl::AppState* _appState, tsl::preset::PresetWrapper& ptl) {
	for (auto id : kMsToLog10Params) {
		if (id >= ptl.params.size())
			continue;
		auto& val = ptl.params[id].val;
		// Old files can hold 0 (several of these had min = 0); log10(0) is -inf.
		val = LOG10D20F(std::max(val, 0.01));
		const auto& p = _STATE->parameters[id];
		CLAMP(val, p.min, p.max); // CLAMP assigns to its first argument
	}
}

// DISTDRIVE was 0..60 dB up to preset version 21 and is a plain 0..1 knob from
// 22 on, remapped in DISTORT::compute onto the shaper gain
// k = kDistDriveMin * (kDistDriveMax / kDistDriveMin)^(knob^kDistDriveCurve).
// The old value reached the shaper as k = 6 * 10^(dB/20), so inverting that
// curve gives the knob position that sounds the same. Anything over ~29.5 dB
// lands past the top of the new range and clamps to 1 - k beyond kDistDriveMax
// is an ideal square wave either way, and above 40 dB the old code was
// NaN-punching samples, so those settings never had a sound to preserve.
static double distDriveDbToKnob(double db) {
	const double k = 6. * pow(10., db * .05);
	double u = log(k / kDistDriveMin) / log(kDistDriveMax / kDistDriveMin);
	// Clamped before the root, not after: pow() of a negative base with a
	// fractional exponent is NaN.
	CLAMP(u, 0., 1.); // CLAMP assigns to its first argument
	return pow(u, 1. / kDistDriveCurve);
}

static void migrateDistDrive(tsl::preset::PresetWrapper& ptl) {
	if (17 == ptl.version) {
		// 17 keeps its values in the struct rather than in ptl.params.
		auto pr = ptl.get<Preset17>();
		if (pr && DISTDRIVE < pr->numparameters && DISTDRIVE < ARRAY_LEN(pr->fxvalues)) {
			pr->fxvalues[DISTDRIVE] = (float)distDriveDbToKnob(pr->fxvalues[DISTDRIVE]);
			pr->lfomin[DISTDRIVE] = (float)distDriveDbToKnob(pr->lfomin[DISTDRIVE]);
			pr->lfomax[DISTDRIVE] = (float)distDriveDbToKnob(pr->lfomax[DISTDRIVE]);
		}
		return;
	}
	// 18..21 route everything through ptl.params. Matched on id rather than
	// indexed by it: 20/21 read the records back in whatever order they were
	// written, and the LFO range for DRIVE is stored in the same domain, so all
	// three entry types have to convert.
	for (auto& p : ptl.params)
		if (p.id == DISTDRIVE)
			p.val = distDriveDbToKnob(p.val);
}

void
tsl::preset::loadPreset(PresetWrapper& ptl) {
	TRACK* t = ptl.track;
	auto _appState = t->_appState;

	int32_t version = ptl.version;
	auto& rec = ptl.rec;
	if (version < 22) {
		migrateMsParamsToLog10(_appState, ptl);
		migrateDistDrive(ptl);
	}
#if defined __ANDROID__
	if (ptl.isProject)
		t->setToInitState();
	else {
		t->reset();
		t->setDefaults();
	}
#else
	t->setToInitState();
#endif


	if (17 == version) {
		auto pr = ptl.get<Preset17>();
		load_preset17(t, pr);
		setLoopPositions<Preset17>(ptl, rec);
	}
	else if (18 == version) {
		load_preset18(t, ptl);
		setLoopPositions<Preset18>(ptl, rec);
	}
	else if (19 == version) {
		load_preset19(t, ptl);
		setLoopPositions<Preset19>(ptl, rec);
	}
	else if (20 == version) {
		load_preset20(t, ptl);
		setLoopPositions<Preset20>(ptl, rec);
	}
	else if (21 == version) {
		load_preset21(t, ptl);
		setLoopPositions<Preset21>(ptl, rec);
	}
	else if (22 == version) {
		auto pr = ptl.get<Preset22>();

		std::string name;
		std::vector<tsl::parameters::Event> powerEvents;
		auto isPower = [](tsl::parameters::Event& ev) {
			return ev.eventType == tsl::parameters::Power && (ev.subType == tsl::parameters::powerGrainFx || ev.subType == tsl::parameters::powerFx || ev.subType == tsl::parameters::powerStereoFx);
			};


       // int i = 0;

		for (auto& e : ptl.events) {
/*
			char buf[100]{};
			int pos = 0;
			pos = snprintf(buf + pos, 100 - pos, "%d: ", i++);
			e.toString(_STATE, pos, buf, 100, true);
			LOGE("%s", buf);
*/
			// Events carry the track they were SAVED from, and Event::apply writes
			// to params[e.trackIndex][...]. Without this, a preset saved on track 3
			// and loaded onto track 0 applied every value back to track 3 and left
			// track 0 at its defaults -- while the audio still arrived, because
			// that path uses the destination track directly. That combination is
			// what "the sound loads but nothing else does" looks like.
			// For a project this is a no-op: sections are read in track order, so
			// ptl.track->index already equals the stored index.
			e.trackIndex = (uint8_t)ptl.track->index;

			if(!ptl.isProject){
				if (e.eventType == tsl::parameters::paramUpdate && e.paramIndex == DISTRSOURCE) {
					continue;
				}
				else if (e.eventType == tsl::parameters::Power && e.subType == tsl::parameters::powerTrack) {
					continue;
				}
			}
			if (rec && e.eventType == tsl::parameters::Recording) {
				auto s = rec->state.load();
				
				switch (e.subType) {
				case tsl::parameters::EventSubtype::offset:
					s->offset = e.value;
					break;
				case tsl::parameters::EventSubtype::offStart:
					s->off_start = e.value;
					break;
				case tsl::parameters::EventSubtype::offStop:
					s->off_stop = e.value;
					break;
				case tsl::parameters::EventSubtype::waveformView:
					s->waveformState = e.waveformState;
					break;
				case tsl::parameters::EventSubtype::bounceType:
					s->bounceType = e.value;
					break;
				case tsl::parameters::EventSubtype::playbackDir:
					s->playbackDir = e.value;
					break;
				default:
					break;
				}
				continue;
			}
			e.setFlag(tsl::parameters::Event::ToWorkerThread, false);
			e.setFlag(tsl::parameters::Event::ToAudioThread, false);
			e.setFlag(tsl::parameters::Event::Info, false);
			e.setFlag(tsl::parameters::Event::History, false);
			e.setFlag(tsl::parameters::Event::Redraw, false);
			
			if (isPower(e)) {
				powerEvents.push_back(e);
			}			
			else{
				auto ne = e;
				ne.fromNormalized(_STATE);

				ne.apply(_STATE, tsl::parameters::FromHistory);
			}
		}
		std::sort(powerEvents.begin(), powerEvents.begin() + powerEvents.size(), [](tsl::parameters::Event& a, tsl::parameters::Event& b) {return a.power.pos > b.power.pos; });

		for (auto& e : powerEvents)e.apply(_STATE, tsl::parameters::FromHistory);
		setLoopPositions(ptl, rec);

		// Event replay restores param values only — the recompute SpecialActions
		// the editors emit are kept out of events_ (history-only), so trigger the
		// same recompute ritual the legacy loaders run.
		_STATE->params[t->index][RECOMPUTEGRAINENV1] = 1.0;
		_STATE->params[t->index][RECOMPUTEGRAINENV2] = 1.0;
		_STATE->params[t->index][RECOMPUTEGRAINENV3] = 1.0;

		t->lfo1.store(LFORECOMPUTE, 1.0);
		t->lfo1.store(LFOREDRAW, 1.0);
		t->lfo2.store(LFORECOMPUTE, 1.0);
		t->lfo2.store(LFOREDRAW, 1.0);
		t->lfo3.store(LFORECOMPUTE, 1.0);
		t->lfo3.store(LFOREDRAW, 1.0);

		for (int32_t i = 0; i < _STATE->channels; i++) {
			_STATE->params[t->index][SPECDEL2X0 + OFFRECOMP + i].store(1.0);
			_STATE->params[t->index][GRAINFILTERENVX0 + OFFRECOMP + i].store(1.0);
			_STATE->params[t->index][SPECFILTX0 + OFFRECOMP + i].store(1.0);
		}
	}


	if (_STATE->params[t->index][VOCBW] > 1.0)
		_STATE->params[t->index][VOCBW].store(1.0);

	if (_STATE->params[t->index][BRBW].load() > 1.0)
		_STATE->params[t->index][BRBW].store(1.0);

	if (_STATE->params[t->index][SPECDELDEL].load() > SPECDELMAXDELMS)
		_STATE->params[t->index][SPECDELDEL].store(SPECDELMAXDELMS);

	_STATE->params[t->index][GRAINVCODETLR].store(
		std::abs(_STATE->params[t->index][GRAINVCODETLR].load()));
	if (ptl.importAudio && rec) {
		t->loadAudio(rec);
	}
	_STATE->WorkerQueue.add_task([_STATE, isProject = ptl.isProject, t, rec = std::move(ptl.rec)] mutable {

		if (rec && rec->off > 0) {
			t->waveform->setup(rec);
		}
		if (_STATE->active_track.load() == t->index) {
			tsl::graphics::TrackButton::func(_appState, t->index);
		}
		if (!isProject) {
			char text[100];
			snprintf(text, 100, "%s Preset Imported.", t->name);
			showToast(_STATE, text);

		}
		else if (t == _DATA->tracks[3]) {
			showToast(_STATE, "Project imported.");
		}
		});
}

struct PreHeader {
	char header[3];
	int32_t version;
};


bool tsl::preset::load_preset(tsl::AppState* _appState, unsigned char* data, int size, bool async) {
	PresetWrapper ptl;
	if (!ptl.initFromMem(data, size)) return false;
	auto ptl_ptr = std::make_shared<PresetWrapper>(std::move(ptl));

	if (async) {
		return _STATE->WorkerQueue.add_task([_appState, ptl_ptr]() mutable {
			while (!_STATE->initdone.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			loadPresetThreadFunc(_DATA->tracks[_STATE->active_track.load()], ptl_ptr);
			});
	}
	else {
		while (!_STATE->initdone.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		return loadPresetThreadFunc(_DATA->tracks[_STATE->active_track.load()], ptl_ptr);
	}
}



#ifdef __ANDROID__

int32_t read_header(JNIEnv* env, jclass thiz, jstring _uri, jobject obj, jboolean isProject) {
	jboolean isCopy;
	const char* uri = env->GetStringUTFChars(_uri, &isCopy);
	std::string n = uri;
	env->ReleaseStringUTFChars(_uri, uri);
	tsl::preset::PresetWrapper pr;
	if (!pr.initFromFile(n) || pr.isProject != isProject)return 0;
	jclass clazz = env->GetObjectClass(obj);
	if (nullptr == clazz) {
		LOGE("GetObjectClass returned 0.");
		return 0;
	}

	jfieldID fid = env->GetFieldID(clazz, "time", "J");
	env->SetLongField(obj, fid, pr.version >= 16 ? pr.date : 0);
	fid = env->GetFieldID(clazz, "version", "I");
	env->SetIntField(obj, fid, pr.version);
	fid = env->GetFieldID(clazz, "name", "Ljava/lang/String;");
	if (pr.version >= 17) {
		auto estr = (jstring)env->NewStringUTF(
			pr.name.c_str());
		env->SetObjectField(obj, fid, estr);
	}
	else {
		std::string str33("(Outdated, sorry) ");
		str33.append(pr.name);
		auto estr = (jstring)env->NewStringUTF(
			str33.data());
		env->SetObjectField(obj, fid, estr);
	}
	fid = env->GetFieldID(clazz, "filepath", "Ljava/lang/String;");
	auto estr2 = (jstring)env->NewStringUTF(pr.audioFilePath.c_str());
	env->SetObjectField(obj, fid, estr2);
	return 1;
}

jint save_preset_callback(JNIEnv* env, jclass thiz, jstring _name) {
	auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return -1;

	if (_name != nullptr) {
		jboolean isCopy;
		const char* name = env->GetStringUTFChars(_name, &isCopy);
		std::string n = name;
		env->ReleaseStringUTFChars(_name, name);
		//   savePresetGotFileName(n);
		//_DATA->snapShot.add_task(savePresetGotFileName, _STATE, n);
	}
	return 0;
}

void java_load_preset(JNIEnv* env, jclass thiz, jstring _name) {
	auto _appState = __STATE;
	if (_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return;

	if (_name != nullptr) {
		jboolean isCopy;
		const char* name = env->GetStringUTFChars(_name, &isCopy);
		std::string n = name;
		env->ReleaseStringUTFChars(_name, name);

		_DATA->snapShot.add_task([_appState, n]() {
			while (!_STATE->initdone.load())usleep(10 * 1000);
			auto presetToLoad = std::make_shared<tsl::preset::PresetWrapper>();
			if (!presetToLoad->initFromFile(n)) {
				showToast(_STATE, errorReadingPreset);
				return;
			}
			loadPresetThreadFunc(_DATA->tracks[_STATE->active_track.load()], presetToLoad);
			});

	}
}


#endif

