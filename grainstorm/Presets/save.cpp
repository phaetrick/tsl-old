#include <app.h>
#include "grainstorm.h"
#include "flac.h"
#include "preset.h"
#include "presetdef.h"

#include <filesystem>

#include "tools/PlatformPaths.h"
using namespace tsl::preset;



struct SaveDummy2 {
	explicit SaveDummy2(const bool isProject, TRACK* t) : header(isProject),
		_isProject(isProject) {
		load(t);
	};

	void storePresetName(std::string& presetname) {
		name = presetname;
	}

	void load(TRACK* t) {
		_appState = t->_appState;
		auto rec = t->filebuffer.load();
		audioPath =
			_DATA->saveAudioWithPreset && rec && rec->off > 0 ? presetHasAudio : rec && rec->off > 0 ? rec->fileName : noSoundLoaded;
		trackindex = t->index;
		header.off = rec ? rec->off : 0;
		_DATA->snapShot.getEvents(events, trackindex);
		preset.numEvents = events.size();
		;
	}

#ifdef __ANDROID__
	int32_t write(tsl::FileWrapper& wrapper, std::string& message) {

		header.namelen = name.size();
		header.audioPathLen = audioPath.size();
		if (wrapper.write(&header, sizeof(PresetHeader2023), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (wrapper.write(name.data(), name.size(), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (wrapper.write(audioPath.data(), audioPath.size(), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		long cur = wrapper.tell();
		if (wrapper.write(&preset, sizeof(Preset22), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (events.size() > 0) {
			for (auto& e : events)e.toNormalized(_STATE);
			if (wrapper.write(events.data(), sizeof(tsl::parameters::Event), events.size()) !=
				events.size()) {
				message.append(_isProject ? " Save Project: Write error."
					: " Save Preset: Write error.");
				return 1;
			}
		}
		preset.numEvents = events.size();
		if (audioPath == presetHasAudio) {
			FlacEncoder encoder;
			encoder.init(&wrapper, _DATA->tracks[trackindex]);
			wrapper.seek(0L, SEEK_END);
		}
		preset.end = wrapper.tell();

		if (wrapper.seek(cur, SEEK_SET) == -1 || wrapper.write(&preset, sizeof(Preset22), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		wrapper.seek(0L, SEEK_END);
		return 0;
	};

#else
	int32_t write(tsl::FileWrapper& wrapper, std::string& message) {
		if (audioPath == presetHasAudio && !presetPath.empty()) {
			std::stringstream ss;
			ss << presetPath << "-audio-" << trackindex + 1 << ".flac";
			FILE* fd = fopen(ss.str().c_str(), "wb");

			if (!fd) {
				message.append(
					_isProject ? "Save Project: Could not open output file."
							   : " Save Preset: Could not open output file.");
				audioPath = noSoundLoaded;
			} else {
				tsl::FileWrapper audioWrapper;
				audioWrapper.init(fd, true);

				FlacEncoder encoder;
				encoder.init(&audioWrapper, _DATA->tracks[trackindex]);
				audioPath = ss.str();
			}
		}

		header.namelen = name.size();
		header.audioPathLen = audioPath.size();

		if (wrapper.write(&header, sizeof(PresetHeader2023), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (wrapper.write(name.data(), name.size(), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (wrapper.write(audioPath.data(), audioPath.size(), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		long cur = wrapper.tell();
		if (wrapper.write(&preset, sizeof(Preset22), 1) != 1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		if (!events.empty()) {
			for (auto& e : events)e.toNormalized(_STATE);
			if (wrapper.write(events.data(), sizeof(tsl::parameters::Event), events.size()) !=
				events.size()) {
				message.append(_isProject ? " Save Project: Write error."
					: " Save Preset: Write error.");
				return 1;
			}
		}

		preset.numEvents = events.size();
		preset.end = wrapper.tell();

		if (wrapper.seek(cur, SEEK_SET) == -1 || wrapper.write(&preset, sizeof(Preset22), 1) != 1 || wrapper.seek(0L, SEEK_END) == -1) {
			message.append(
				_isProject ? " Save Project: Write error." : " Save Preset: Write error.");
			return 1;
		}
		return 0;
	};

#endif

	tsl::AppState* _appState{};
	bool saveAudio{};
	bool _isProject;
	int32_t trackindex{};
	std::string audioPath, name;
	PresetHeader2023 header;
	Preset22 preset{};
	std::vector<tsl::parameters::Event> events;
	std::string presetPath;
};






// Returns true only if the file was fully written. The caller relies on this to
// decide whether the item being replaced may be removed from storage.
static bool savePresetGotFileName(tsl::AppState* _appState, std::string presetname, const std::shared_ptr<std::vector<SaveDummy2>>& presets) {
	tsl::FileWrapper wrapper;

	if (!presetname.empty() && presets != nullptr && !presets->empty()) {
		std::string message = presets->at(0)._isProject ? "" : _DATA->tracks[presets->at(
			0).trackindex]->name;

		for (auto& dummy : *presets)
			dummy.storePresetName(presetname);
		std::string preset_dir;
#ifdef __ANDROID__
		preset_dir = tsl::app::getStoragePath(
			presets->at(0)._isProject ? "projects" : "presets");
#else
		preset_dir = tsl::app::getStoragePath(
			presets->at(0)._isProject ? "Grainstorm/Projects/" : "Grainstorm/Presets/");
#endif
		if (preset_dir.empty()) {
			message.append(presets->at(0)._isProject ? "Save Project: Write Error."
				: " Save Preset: Write Error");
			showToast(_STATE, message.c_str());
			return false;
		}
		FILE* fd = nullptr;
		std::stringstream ss;
		ss << preset_dir;

		std::string preset_uri;
		if (preset_dir.starts_with("pipe:/")) {
			auto sep = preset_dir.find('|');
			if (sep != std::string::npos) {
				preset_uri = preset_dir.substr(sep + 1);
				preset_dir = preset_dir.substr(0, sep);
			}
			preset_dir.erase(preset_dir.begin(), preset_dir.begin() + 6);
			fd = fdopen(std::strtol(preset_dir.c_str(), nullptr, 10), "wb");
		}
		else {
			ss << tsl::time::millisecondsSinceEpoch();
			std::string trimmed = tsl::trimToValidFilename(presetname); // trim whitespace if needed
			if (!trimmed.empty())
				ss << "-" << trimmed;
			fd = fopen(ss.str().c_str(), "wb");

		}
		bool failed = false;

		if (!fd) {
			message.append(
				presets->at(0)._isProject ? "Save Project: Could not open output file."
										  : " Save Preset: Could not open output file.");
			failed = true;
		}

		if (!failed) {
			wrapper.init(fd, true);
			for (auto& dummy : *presets) {
				dummy.presetPath = ss.str();
				if (dummy.write(wrapper, message)) {
					failed = true;
					break;
				}
			}
		}

		if (!failed) {
			auto pw = std::make_shared<PresetWrapper>();
			pw->version = 22;
			pw->isProject = presets->at(0)._isProject;
			pw->date = presets->at(0).header.date;
			pw->name = presetname;
			pw->audioFilePath = presets->at(0).audioPath;
			pw->path = preset_uri.empty() ? ss.str() : preset_uri;
			auto& q = presets->at(0)._isProject ? _DATA->projects : _DATA->presets;
			{
				std::lock_guard lock(q);
				q.insertFirst(pw);
			}
			message.append(presets->at(0)._isProject ? "Project Saved." : " Preset Saved.");
		}

		showToast(_STATE, message.data());
		presets->clear();
		return !failed;
	}
	return false;
}


using OT = std::shared_ptr<tsl::preset::PresetWrapper>;


// Runs on the Snapshot worker (queued from the TRACK SETTINGS item callback).
//
// The two halves belong on different threads:
//
//   capture  -- must stay HERE. SaveDummy2 builds the preset out of the track's
//               history via snapShot.getEvents(), and control changes reach
//               events_ through snapShot.add_task(), so FIFO on this one worker
//               is what guarantees a knob moved just before Save is in the file.
//   dialog   -- must NOT stay here. deleteOverwriteFunc parks its thread for the
//               whole modal, i.e. on human latency. This worker is the undo
//               recorder: while it is parked, nothing drains it, and every
//               control change queued meanwhile is dropped once it fills.
//
// So capture, then hand the dialog to UiTasksQueue -- the thread that already
// parks on modals (ImportPreset::importPreset, saveMapping, ListView::show).
// A chain rather than a rendezvous on purpose: no blocking primitive is added,
// so nothing here can miss AppState's shutdown path (destroyRequested +
// waitNotify.shutdown() before the workers are joined).
void tsl::preset::save_preset(TRACK* t, const bool isProject) {
	auto _appState = t->_appState;


	auto presets = std::make_shared<std::vector<SaveDummy2>>();
	if (isProject) {
		for (auto track : _DATA->tracks) {
			presets->emplace_back(true, track);
		}
	}
	else {
		presets->emplace_back(false, t);
	}

	const bool queued = _STATE->UiTasksQueue.add_task([_appState, presets, isProject] {
		tsl::app::deleteOverwriteFunc<OT>(
			_STATE,
			isProject ? _DATA->projects : _DATA->presets,
			isProject ? "Save Project" : "Save Preset",
			std::function<std::vector<OT>()>(
				[isProject] {
					std::vector<OT> presets;
					getPresets(presets, isProject);
					return presets;
				}),
			[presets](tsl::AppState* _appState, std::string& name) {
				return savePresetGotFileName(_appState, name, presets);
			});
		});

	if (!queued)
		showToast(_STATE, isProject ? "Save Project: UI busy, try again."
								    : "Save Preset: UI busy, try again.");
}



#ifdef GS_TEST_HOOKS
// Test-only entry point for tests/auhost (GS_TEST_HOOKS is off in every shipping
// build). save_preset() routes through UiTasksQueue and a name-entry dialog,
// neither of which runs headless, so a harness cannot reach the serializer
// through it. This is exactly the step after the user has typed a name --
// savePresetGotFileName is static, so the wrapper has to live here.
bool gsTestSavePresetNamed(tsl::AppState* _appState, const std::string& name, bool isProject) {
	auto presets = std::make_shared<std::vector<SaveDummy2>>();
	if (isProject) {
		for (auto track : _DATA->tracks)
			presets->emplace_back(true, track);
	}
	else {
		presets->emplace_back(false, _DATA->tracks[_STATE->active_track.load()]);
	}
	return savePresetGotFileName(_appState, name, presets);
}
#endif
