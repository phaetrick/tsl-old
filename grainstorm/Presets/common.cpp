#include "preset.h"
#include "presetdef.h"
#include <logger.h>
#include <app.h>

#include <filesystem>

#include "tools/PlatformPaths.h"

#ifdef __aarch64__
constexpr int minVersion = 17;
#else
constexpr int minVersion = 20;
#endif
using namespace tsl::preset;

bool PresetWrapper::initFromFile(const std::string& _path) {
#ifdef __ANDROID__
	FILE* fd = tsl::app::getFdFromUri(_path);
#else
	FILE* fd = fopen(_path.c_str(), "rb");
#endif
	if (fd == nullptr) {
		return false;
	}
	tsl::FileWrapper wrapper;
	if (wrapper.init(fd, true) != 0)return false;
	path = _path;
	return init(wrapper);
};


bool PresetWrapper::initFromMem(unsigned char* data, int size) {
	if (data == nullptr || size == 0)return false;
	presetMemory.resize(size);
	std::memcpy(presetMemory.data(), data, presetMemory.size());
	tsl::FileWrapper wrapper;
	if (wrapper.init(data, size) != 0)return false;
	return init(wrapper);
};

struct PreHeader {
	char header[3];
	int32_t version;
};

bool PresetWrapper::init(tsl::FileWrapper& wrapper) {

	PreHeader preHeader{};
	if (wrapper.read(&preHeader, sizeof(PreHeader), 1) != 1) {
		LOGE("Error reading header");
		return false;
	}
	if ((strncmp(preHeader.header, "GSP", 3) != 0 && strncmp(preHeader.header, "GPR", 3) != 0) ||
		preHeader.version < minVersion) {
		LOGE("Incorrect version or header");
		return false;
	}
	wrapper.rewind();
	if (preHeader.version < 20) {
		PRESET_HEADER h;
		if (wrapper.read(&h, sizeof(PRESET_HEADER), 1) != 1) {
			return false;
		}
		version = h.version;
		isProject = !strncmp(h.header, "GPR", 3);
		date = h.date;
		name = h.name;
		name += " (!)";
		audioFilePath = h.audio_file_path;

	}
	else {
		PresetHeader2023 pr;
		if (wrapper.read(&pr, sizeof(PresetHeader2023), 1) != 1 || pr.namelen == 0) {
			LOGE(errorReadingPreset);
			return false;
		}
		std::string tmp(pr.namelen, '\0');
		if (wrapper.read(tmp.data(), pr.namelen, 1) != 1) {
			LOGE("Error reading preset name.");
			return false;
		}
		version = pr.version;
		isProject = !strncmp(pr.header, "GPR", 3);
		date = pr.date;
		name = tmp;
		if (pr.audioPathLen) {
			tmp.resize(pr.audioPathLen, '\0');
			if (wrapper.read(tmp.data(), pr.audioPathLen, 1) != 1) {
				LOGE("Error reading filepath name.");
				return false;
			}
			audioFilePath = tmp;
		}
	}
	return true;
}

void tsl::preset::getPresets(std::vector<std::shared_ptr<PresetWrapper>>& presets, const bool isProject) {
#ifdef __ANDROID__
	std::vector<std::string> strings = tsl::app::getDirContent(
		isProject ? "projects" : "presets");

	for (const auto& f : strings) {
		auto presetToLoad = std::make_shared<PresetWrapper>();
		if (!presetToLoad->initFromFile(f) || presetToLoad->isProject != isProject) {
			//showToast(_STATE, isProject ? "Error reading projects." : "Error reading presets.");
			continue;
		}
#else
	std::string path = tsl::app::getStoragePath(isProject ? "Grainstorm/Projects" : "Grainstorm/Presets");

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (!is_regular_file(entry))continue;
		auto& f = entry.path();
		auto presetToLoad = std::make_shared<PresetWrapper>();
		if (!presetToLoad->initFromFile(f.string()) || presetToLoad->isProject != isProject) {
			//showToast(_STATE, isProject ? "Error reading projects." : "Error reading presets.");
			continue;
		}
#endif

		presets.push_back(std::move(presetToLoad));
	}
	std::sort(presets.begin(), presets.end(),
		[](std::shared_ptr<PresetWrapper>& a, std::shared_ptr <PresetWrapper>& b) { return a->date > b->date; });
	}
