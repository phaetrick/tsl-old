//
// Created by pr on 24.01.19.
//

#include "logger.h"
#include "MidiSaver.h"
#include <app.h>
#include "tools/PlatformPaths.h"
#include <filesystem>
#include <keyboard.h>
#include <sstream>

#ifdef OS_WIN
#define _CRT_NONSTDC_NO_WARNINGS

#include <io.h>
#include <stdio.h>
#include <filesystem>
#endif

struct MidiAssignment {
	uint8_t trackindex{};
	uint16_t miditarget{};
	double min{}, max{};
	uint8_t active_space_lfo{};
	uint8_t active_space_windows{};
	uint8_t active_space_envf{};
	uint8_t active_space_mdelay{};
	uint8_t active_space_granulation{};
	uint8_t active_space_fx{};
	uint8_t active_space_stereo_fx{};
	uint8_t active_space_cross{};
	uint8_t active_space_pv{};
	uint8_t active_space_main{};
	uint8_t dummy2{};
	uint8_t dummy3{};
	uint8_t dummy4{};
	uint8_t dummy5{};
	uint8_t dummy6{};
	uint8_t dummy7{};
	uint8_t dummy8{};
	uint8_t dummy9{};
	uint8_t dummy10{};
};


/*
struct MIDIHEADER {
	char header[3]{};
	int version{};
	char name[101]{};
	long date{};
	long offset{};
};

MidiAssignment midiassignmentscontrol[NUM_MIDICHANNELS][NUM_MIDI_CONTROL]{};
MidiAssignment midiassignmentsnote[NUM_MIDICHANNELS][NUM_MIDI_NOTEON]{};
   if (m.type == ParameterType_double) {
		e.midiState.min = mintoset;
		e.midiState.max = maxtoset;
		_STATE->midicontrolevents[channel][control] = e;
		if (m.paramOffset) {
			auto offset = (int) (_STATE->params[tindex][m.paramOffset].load());
			offset = offset * m.offsetFact;
			id += offset;
		}
		_STATE->midiassignmentscontrol[channel][control].min = mintoset;
		_STATE->midiassignmentscontrol[channel][control].max = maxtoset;
		_STATE->midiassignmentscontrol[channel][control].miditarget = id;
		_STATE->midiassignmentscontrol[channel][control].trackindex = tindex;
#ifdef GRAINSTORM
		_STATE->midiassignmentscontrol[channel][control].active_space_main = GASMAIN;
		_STATE->midiassignmentscontrol[channel][control].active_space_lfo = GASLFO;
		_STATE->midiassignmentscontrol[channel][control].active_space_windows = GASENV;
		_STATE->midiassignmentscontrol[channel][control].active_space_envf = GASFOL;
		_STATE->midiassignmentscontrol[channel][control].active_space_mdelay = GASMDEL;
		_STATE->midiassignmentscontrol[channel][control].active_space_granulation = GASGRAIN;
		_STATE->midiassignmentscontrol[channel][control].active_space_fx = GASFX;
		_STATE->midiassignmentscontrol[channel][control].active_space_stereo_fx = GASSTFX;
		_STATE->midiassignmentscontrol[channel][control].active_space_pv = GASPV;
		_STATE->midiassignmentscontrol[channel][control].active_space_cross = GASCROSS;
#endif
	} else {
		_STATE->midinoteevents[channel][control] = e;

		_STATE->midiassignmentsnote[channel][control].miditarget = id;
		_STATE->midiassignmentsnote[channel][control].trackindex = tindex;
#ifdef GRAINSTORM
		_STATE->midiassignmentscontrol[channel][control].active_space_main = GASMAIN;
		_STATE->midiassignmentsnote[channel][control].active_space_lfo = GASLFO;
		_STATE->midiassignmentsnote[channel][control].active_space_windows = GASENV;
		_STATE->midiassignmentsnote[channel][control].active_space_envf = GASFOL;
		_STATE->midiassignmentsnote[channel][control].active_space_mdelay = GASMDEL;
		_STATE->midiassignmentsnote[channel][control].active_space_granulation = GASGRAIN;
		_STATE->midiassignmentsnote[channel][control].active_space_fx = GASFX;
		_STATE->midiassignmentsnote[channel][control].active_space_stereo_fx = GASSTFX;
		_STATE->midiassignmentsnote[channel][control].active_space_pv = GASPV;
		_STATE->midiassignmentsnote[channel][control].active_space_cross = GASCROSS;
#endif
	}
	return 0;

*/
struct MIDIMAPPING1 {
	MIDIMAPPING1() {
		strncpy(header, "GMM", 3);
		date = static_cast<int64_t>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
		version = 3;
	}
	char header[3]{};
	int version{};
	char name[101]{};
	int64_t date{};  // fixed-width: plain `long` is 4 bytes on Windows (LLP64) vs
	int64_t offset{}; // 8 bytes on macOS/iOS/Android (LP64), which would misalign
	                   // this on-disk format across platforms.
	MidiAssignment midiassignmentscontrol[NUM_MIDICHANNELS][NUM_MIDI_CONTROL]{};
	MidiAssignment midiassignmentsnote[NUM_MIDICHANNELS][NUM_MIDI_NOTEON]{};
};


struct MIDIMAPPING2 {
	MIDIMAPPING2() {
		strncpy(header, "GMM", 3);
		date = static_cast<int64_t>(tsl::time::nanosecondsSinceEpoch() / tsl::time::nanosPerSecond);
		version = 4;
	}
	char header[3]{};
	int version{};
	char name[101]{};
	int64_t date{};
	int64_t numEvents{};
	};


#ifdef __ANDROID__

int read_midiheader(JNIEnv* env, jclass thiz, jstring _uri, jobject obj) {
	jboolean isCopy;
	const char* uri = env->GetStringUTFChars(_uri, &isCopy);
	std::string n = uri;
	env->ReleaseStringUTFChars(_uri, uri);
	FILE* fd = tsl::app::getFdFromUri(n);
	if (!fd) {
		LOGE("READ HEADER: Could not open input file.");
		return 0;
	}
	MIDIHEADER pr;
	size_t bytes = 3 + sizeof(int);
	if (fread(&pr, 1, bytes, fd) != bytes) {
		LOGE("Error reading header");
		fclose(fd);
		return 0;
	}
	if (strcmp(pr.header, "GMM") != 0) {
		LOGE("No preset file");
		fclose(fd);
		return 0;
	}
	rewind(fd);
	if (fread(&pr, 1, sizeof(MIDIHEADER), fd) != sizeof(MIDIHEADER)) {
		LOGE("Error reading preset");
		fclose(fd);
		return 0;
	}
	fclose(fd);
	jclass clazz = env->GetObjectClass(obj);
	if (0 == clazz) {
		LOGE("GetObjectClass returned 0.");
		return 0;
	}
	//jfieldID fid = (*env)->GetFieldID(env, clazz, "version", "I");
	//(*env)->SetIntField(env, obj, fid, pr->version);
	jfieldID fid = env->GetFieldID(clazz, "time", "J");
	env->SetLongField(obj, fid, pr.version >= 2 ? pr.date : 0);
	fid = env->GetFieldID(clazz, "version", "I");
	env->SetIntField(obj, fid, pr.version);
	fid = env->GetFieldID(clazz, "name", "Ljava/lang/String;");
	auto estr = (jstring)env->NewStringUTF(pr.name);
	env->SetObjectField(obj, fid, estr);
	return 1;
}



jint save_midimapping_callback(JNIEnv* env, jclass thiz, jstring _name) {
	if (_name == nullptr) {
		return 0;
	}
	auto _appState = __STATE;
	jboolean isCopy;
	const char* nname = env->GetStringUTFChars(_name, &isCopy);
	std::string name = nname;
	env->ReleaseStringUTFChars(_name, nname);
	return saveMappingGotFileName(_STATE, name);
}
#endif

int saveMappingGotFileName(tsl::AppState* _appState, const std::string& name) {
	if (name.empty())return 0;
	std::string preset_dir;
	FILE* fd{};
#ifdef __ANDROID__
	preset_dir = tsl::app::getStoragePath("midimappings");
#else
	preset_dir = tsl::app::getStoragePath("Grainstorm/Midimappings") + "/";
#endif
	std::stringstream ss;
	ss << preset_dir;
	if (preset_dir.empty()) {
		showToast(_STATE, "Save MIDImapping: Could not open output file.");
		return -1;
	}
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
		std::string trimmed = tsl::trimToValidFilename(name); // trim whitespace if needed
		if (!trimmed.empty())
			ss << "-" << trimmed;
		fd = fopen(ss.str().c_str(), "wb");

	}
	if (!fd) {
		showToast(_STATE, "Save MIDImapping: Could not open output file.");
		return -1;
	}
	auto midimapping = std::make_unique<MIDIMAPPING2>();
	snprintf(midimapping->name, 100, "%s", name.c_str());

	// collect assigned events
	std::vector<tsl::parameters::Event> savedEvents;
	for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
		for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
			const auto& e = _STATE->midicontrolevents[ch][i];
			if (e.eventType != tsl::parameters::Eventtype::NoParam) {
				savedEvents.push_back(e);
			}
		}
		for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
			const auto& e = _STATE->midinoteevents[ch][i];
			if (e.eventType != tsl::parameters::Eventtype::NoParam)
				savedEvents.push_back(e);
		}
	}

	midimapping->numEvents = savedEvents.size();  // count of saved events

	// ... open fd ...

	if (fwrite(midimapping.get(), sizeof(MIDIMAPPING2), 1, fd) != 1) {
		LOGE("Error writing midimapping");
		showToast(_STATE, "Save MIDImapping: Write error.");
		fclose(fd);
		return -1;
	}
	// write events
	for (const auto& e : savedEvents) {
		if (fwrite(&e, sizeof(tsl::parameters::Event), 1, fd) != 1) {
			LOGE("Error writing midimapping");
			showToast(_STATE, "Save MIDImapping: Write error.");
			fclose(fd);
			return -1;
		}
	}
	fclose(fd);
	{
		auto h = std::make_shared<MIDIHEADER2>();
		memcpy(&h->h, midimapping.get(), sizeof(MIDIHEADER));
		h->name = midimapping->name;
		h->path = preset_uri.empty() ? ss.str() : preset_uri;
		_STATE->mappings.insertFirst(h);
	}
	showToast(_STATE, "MIDImapping saved.");
	return 0;
}


void saveMidimapping(tsl::AppState* _appState) {
	tsl::graphics::AlphaPopUp kbd(_STATE);
	kbd.setTitle("MIDImapping Name:");
	kbd.setToken(_STATE->waitNotify.begin_wait());

	tsl::graphics::AlphaPopUp::InputResult result{};
	kbd.onCompleteCallback = [&result](const tsl::graphics::AlphaPopUp::InputResult& r) {
		if (r.confirmed) {
			result = r;
			return 1;
		}
		else return 0;
		};
	kbd.setText("My MIDImapping");
	kbd.init();
	kbd.addDraw();
	kbd.addCB();
	_STATE->waitNotify.wait_for_signal(kbd.token());
	kbd.deldraw();
	kbd.delCB();

	if (result.confirmed) {
		saveMappingGotFileName(_appState, result.text);
	}
	else showToast(_STATE, "Save cancelled.");
}


void getMappings(std::vector<std::shared_ptr<MIDIHEADER2>>& presets) {
#ifdef __ANDROID__
	std::vector<std::string> strings = tsl::app::getDirContent("midimappings");

	for (const auto& f : strings) {
		{
			FILE* fd = tsl::app::getFdFromUri(f);

#else
	std::string path = tsl::app::getStoragePath("Grainstorm/Midimappings");

	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (is_regular_file(entry)) {
			FILE* fd = fopen(entry.path().string().c_str(), "rb");
#endif
			if (fd == nullptr) {
				LOGE("Error reading mappings.");
				continue;
			}
			MIDIHEADER h;
			if (fread(&h, sizeof(MIDIHEADER), 1, fd) != 1) {
				LOGE("Error reading mapping.");
				fclose(fd);
				continue;
			}
			if (!strncmp(h.header, "GMM", 3)) {

				auto  presetHeader2 = std::make_shared<MIDIHEADER2>();
				presetHeader2->h = h;
#ifdef __ANDROID__
				presetHeader2->path = f;
#else
				presetHeader2->path = entry.path().string();
#endif
				presetHeader2->name = h.name;
				presets.emplace_back(presetHeader2);
			}
			else {
#ifdef __ANDROID__
				LOGE("%s Not a mapping file. ", f.c_str());
#else
				LOGE("%s Not a mapping file. ", entry.path().string().c_str());
#endif
			}
			fclose(fd);
		}
	}
	std::sort(presets.begin(), presets.end(),
		[](std::shared_ptr<MIDIHEADER2>& a, std::shared_ptr<MIDIHEADER2>& b) { return a->h.date > b->h.date; });
}

void loadMidiMapping(tsl::AppState * _appState,
	const std::shared_ptr<MIDIHEADER2>& h)
{
#ifdef __ANDROID__
	FILE* fd = tsl::app::getFdFromUri(h->path, "rb");
#else
	FILE* fd = fopen(h->path.c_str(), "rb");
#endif

	if (!fd) {
		showToast(_STATE, "Could not open mapping.");
		return;
	}

	if (h->h.version < 4)
	{
		auto pr = std::make_unique<MIDIMAPPING1>();

		if (fread(pr.get(), sizeof(MIDIMAPPING1), 1, fd) != 1) {
			showToast(_STATE, "Error reading midimapping");
			fclose(fd);
			return;
		}

		{
			std::lock_guard lk(_STATE->mutex_midi);
			for (int chan = 0; chan < NUM_MIDICHANNELS; chan++) {
				for (int cn = 0; cn < NUM_MIDI_CONTROL; cn++) {
					auto& c = pr->midiassignmentscontrol[chan][cn];
					tsl::parameters::Event e{};
					switch (c.miditarget) {
#ifdef GRAINSTORM
					case LFOCPS:
					case LFO1CPS:
 {
						e.setup(_STATE, c.trackindex, LFO1CPS);
						e.paramIndex = LFO1CPS + c.active_space_lfo * LFONUMPARAMS;
						break;
					}

					case MDELAY1GAIN:
					case MDELAY1HP:
					case MDELAY1LP:
					case MDELAY1DEL:
					case MDELAY1FB: {
						e.setup(_STATE, c.trackindex, c.miditarget);
						e.paramIndex = c.miditarget + c.active_space_mdelay * (MDELAY2FB - MDELAY1FB);
						break;
					}
#else
#endif
					default:
						e.setup(_STATE, c.trackindex, c.miditarget);
						break;
					}
					e.midiState.channel = chan;
					e.midiState.num = cn;
					e.midiState.type = ParameterType_double;
					auto& m = _STATE->parameters[e.getDisplayParam()];
					e.midiState.min = std::clamp(m.toNormalized(m.fromDisplay(_STATE->sr, c.min)), 0., 1.) * UINT16_MAX;
					e.midiState.max = std::clamp(m.toNormalized(m.fromDisplay(_STATE->sr, c.max)), 0., 1.) * UINT16_MAX;
					_STATE->midicontrolevents[chan][cn] = std::move(e);
				}
				for (int cn = 0; cn < NUM_MIDI_NOTEON; cn++) {
					auto& c = pr->midiassignmentsnote[chan][cn];
					tsl::parameters::Event e{};
					switch (c.miditarget) {
#ifdef GRAINSTORM
					case OFFGRAIN: {
						e.setup(_STATE, c.trackindex, OFFGRAIN);
						e.paramIndex = c.active_space_granulation;
						break;
					}
					case OFFFX: {
						e.setup(_STATE, c.trackindex, OFFFX);
						e.paramIndex = c.active_space_fx;
						break;
					}
					case OFFSTEREOFX: {
						e.setup(_STATE, c.trackindex, OFFSTEREOFX);
						e.paramIndex = c.active_space_stereo_fx;
						break;
					}
					case BYPASSGRAINFX: {
						e.setup(_STATE, c.trackindex, BYPASSGRAINFX);
						e.paramIndex = c.active_space_granulation;
						break;
					}
					case BYPASSFX: {
						e.setup(_STATE, c.trackindex, BYPASSFX);
						e.paramIndex = c.active_space_fx;
						break;
					}
					case BYPASSSTEREOFX: {
						e.setup(_STATE, c.trackindex, BYPASSSTEREOFX);
						e.paramIndex = c.active_space_stereo_fx;
						break;
					}
					case POWERTRACK:
						e.setup(_STATE, c.trackindex, POWERTRACK);
						break;
					case LFOPOW:
					case LFO1POWER:
					 {
						e.setup(_STATE, c.trackindex, LFO1POWER);
						e.paramIndex = LFO1POWER + c.active_space_lfo * LFONUMPARAMS;
						break;
					}
					case LFOSYNC:
					case LFO1SYNC:
					 {
						e.setup(_STATE, c.trackindex, LFO1SYNC);
						e.paramIndex = LFO1SYNC + c.active_space_lfo;
						break;
					}
					case LFOBACKW:
					case LFO1BACKW:
					 {
						e.setup(_STATE, c.trackindex, LFO1BACKW);
						e.paramIndex = LFO1BACKW + c.active_space_lfo;
						break;
					}
					case LFOSTOP:
					case LFO1STOP:
					 {
						e.setup(_STATE, c.trackindex, LFO1STOP);
						e.paramIndex = LFO1STOP + c.active_space_lfo;
						break;
					}
					case LFOPLAY:
					case LFO1PLAY:
					 {
						e.setup(_STATE, c.trackindex, LFO1PLAY);
						e.paramIndex = LFO1PLAY + c.active_space_lfo;
						break;
					}

					case LFOFORW:
					case LFO1FORW:
					 {
						e.setup(_STATE, c.trackindex, LFO1FORW);
						e.paramIndex = LFO1FORW + c.active_space_lfo;
						break;
					}
					case LFODIR:
					case LFO1DIR:
					 {
						e.setup(_STATE, c.trackindex, LFO1DIR);
						e.paramIndex = LFO1DIR + c.active_space_lfo * LFONUMPARAMS;
						break;
					}
					case LFOSLOW:
					case LFO1SLOW:
					 {
						e.setup(_STATE, c.trackindex, LFO1SLOW);
						e.paramIndex = LFO1SLOW + c.active_space_lfo;
						break;
					}
					case LFOFAST:
					case LFO1FAST:
					 {
						e.setup(_STATE, c.trackindex, LFO1FAST);
						e.paramIndex = LFO1FAST + c.active_space_lfo;
						break;
					}
					case MDELAY1_SLOW:
					case MDELAY_SLOW: {
						e.setup(_STATE, c.trackindex, MDELAY1_SLOW);
						e.paramIndex = MDELAY1_SLOW + c.active_space_mdelay;
						break;
					}
					case MDELAY1_FAST:
					case MDELAY_FAST: {
						e.setup(_STATE, c.trackindex, MDELAY1_FAST);
						e.paramIndex = MDELAY1_FAST + c.active_space_mdelay;
						break;
					}
					case MDELAY1_SYNC:
					case MDELAY_SYNC: {
						e.setup(_STATE, c.trackindex, MDELAY1_SYNC);
						e.paramIndex = MDELAY1_SYNC + c.active_space_mdelay;
						break;
					}
					case MDELAY1HOLD:
					case MDELAY8HOLD: {
						e.setup(_STATE, c.trackindex, c.miditarget);
						e.paramIndex = c.miditarget + c.active_space_mdelay * (MDELAY2HOLD - MDELAY1HOLD);
						break;
					}
					case MDELAYPOW:
					case MDELAY1_POW:
					 {
						e.setup(_STATE, c.trackindex, MDELAY1_POW);
						e.paramIndex = MDELAY1_POW + c.active_space_mdelay;
						break;
					}
#else
#endif
                    default:
						e.setup(_STATE, c.trackindex, c.miditarget);
						break;
					}
					e.midiState.channel = chan;
					e.midiState.num = cn;
					e.midiState.type = ParameterType_bool;
					e.midiState.min = 0;
					e.midiState.max = 0;
					e.midiState.inc = 1;
					_STATE->midinoteevents[chan][cn] = std::move(e);
				}
			}
		}
	}
	else
	{
		auto pr = std::make_unique<MIDIMAPPING2>();

		if (fread(pr.get(), sizeof(MIDIMAPPING2), 1, fd) != 1) {
			showToast(_STATE, "Error reading midimapping");
			fclose(fd);
			return;
		}

		{
			std::lock_guard lk(_STATE->mutex_midi);

			std::vector<tsl::parameters::Event> loadedEvents;
			loadedEvents.reserve(pr->numEvents);

			for (int i = 0; i < pr->numEvents; i++) {
				tsl::parameters::Event e{};
				if (fread(&e, sizeof(tsl::parameters::Event), 1, fd) != 1) {
					showToast(_STATE, "Error reading MIDImapping events");
					fclose(fd);
					return;
				}
				loadedEvents.push_back(e);
			}
			if (loadedEvents.empty()) {
				showToast(_STATE, "No assignment found MIDImapping.");
				fclose(fd);
				return;

			}
			// all succeeded — clear existing
			for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
				for (int i = 0; i < NUM_MIDI_CONTROL; i++)
					_STATE->midicontrolevents[ch][i] = tsl::parameters::Event{};
				for (int i = 0; i < NUM_MIDI_NOTEON; i++)
					_STATE->midinoteevents[ch][i] = tsl::parameters::Event{};
			}

			// assign loaded events
			for (const auto& e : loadedEvents) {
				if (e.eventType == tsl::parameters::Eventtype::NoParam)
					continue;
				const int ch = e.midiState.channel;
				const int num = e.midiState.num;
				if (e.midiState.type == ParameterType_bool ||
					e.midiState.type == ParameterType_enum) {
					if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_NOTEON)
						_STATE->midinoteevents[ch][num] = e;
				}
				else {
					if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_CONTROL)
						_STATE->midicontrolevents[ch][num] = e;
				}
			}
		}
	}

	fclose(fd);

	showToast(_STATE, "MIDImapping imported.");
}

