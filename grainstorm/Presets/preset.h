#pragma once
#include <cstring>
#include <utility>
#include <audio/Recording.h>
#include <FileWrapper.h>
#include <Follower.h>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

struct TRACK;

namespace tsl {
	struct AppState;
}

namespace tsl::preset {
	struct PresetParam {
		uint8_t type;
		uint16_t id;
		double val;
	};


	struct PresetWrapper {
		bool initFromFile(const std::string& path);
		bool initFromMem(unsigned char* data, int size);
		bool init(FileWrapper&);
		template<typename T>
		void resize() {
			_memory.resize(sizeof(T));
			preset = _memory.data();
		}

		template<typename T>
		T* get() {
			return static_cast<T*>(preset);
		}

		void* preset{};
		std::vector<char> _memory;
		std::vector<unsigned char> presetMemory;
		int32_t version{};
		double sampleRate{};
		TRACK* track;
		long date;
		long off;
		bool importAudio{}, isProject{};
		std::string path, name, audioFilePath;
		std::vector<PresetParam> params;
		std::vector<uint16_t> bypass, fxpower, q_pos, lfo1, lfo2, lfo3;
		std::vector<FollowerParams> followerParams;
		std::shared_ptr<Recording> rec{};
		std::vector<tsl::parameters::Event> events;
	};

	void loadPreset(PresetWrapper&);

	void save_preset(TRACK*, const bool isProject = false);

	bool save_preset(tsl::AppState* _appState, std::vector<unsigned char>& destinationBuf, std::string name, const bool isProject = true);

	void unBlockSavePresetThread();

	void getPresets(std::vector<std::shared_ptr<PresetWrapper>>& presets, const bool isProject);

	bool loadPresetThreadFunc(TRACK* t, std::shared_ptr <PresetWrapper> header);

	bool load_preset(tsl::AppState*, unsigned char* data, int size, bool async = false);
}
#ifdef __ANDROID__

#include <jni.h>

jint save_preset_callback(JNIEnv* env, jclass thiz, jstring preset_name);

int32_t read_header(JNIEnv* env, jclass thiz, jstring uri, jobject obj, jboolean isProect);
void java_load_preset(JNIEnv* env, jclass thiz, jstring _name);
#endif