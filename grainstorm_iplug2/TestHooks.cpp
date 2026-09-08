// TestHooks.cpp — test-only introspection/drive hooks for the AU state harness.
//
// Compiled ONLY when GS_TEST_HOOKS is defined (CMake option -DGS_TEST_HOOKS=ON,
// OFF by default). Nothing here is referenced by a shipping build.
//
// Why this exists: the test host (tests/auhost) loads the .component with
// AudioComponentInstanceNew, so it can drive the two *host-side* change sources
// by itself (automation via AudioUnitSetParameter, state via ClassInfo). The
// other two sources -- the plugin's own UI and the snapshot/undo-redo worker --
// have no host-facing API, and the internal state they mutate is invisible from
// outside. These hooks close that gap:
//
//   gsTestApplyFromUi  makes exactly the call a widget makes
//                      (Event::apply / applyFromExt with SenderFlags::FromUi)
//   gsTestUndo/Redo    makes exactly the call the UNDO/REDO button makes
//   gsTestReadParams   returns, per exposed param, the live engine value, the
//                      value the snapshot carries, and the host-visible IParam
//
// That last one is what makes "is the snapshot the actual state?" checkable.
// No shared library (CPP-New/grainstorm, CPP-New/graphics) is touched.

#ifdef GS_TEST_HOOKS

#include "IPlugEffect.h"

// Must precede TestHooks.h: it selects the exporting form of GS_TEST_EXPORT,
// which the host side must not use.
#define GS_TEST_HOOKS_BUILDING 1
#include "TestHooks.h"

#include <DecoderWindows.h>
#include <IPlugParamDefs.h>
#include <Presets/preset.h>
#include <app.h>
#include <defines.h>
#include <grainstorm.h>
#include <params.h>
#include <player.h>
#include <track.h>
#include <types.h>

// Defined in Presets/save.cpp under the same GS_TEST_HOOKS guard.
bool gsTestSavePresetNamed(tsl::AppState* _appState, const std::string& name, bool isProject);
int32_t filebrowsercallback(tsl::AppState* _appState, const std::string& filename);

#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace tsl::parameters;

namespace {

IPlugEffect*   gPlug     = nullptr;
tsl::AppState* gAppState = nullptr;

constexpr int kTracks = 4;

// Mirrors the ctor in IPlugEffect.cpp: build the Event for a plugin param slot
// exactly as the plugin does, including the paramIndex override the power and
// bypass params need.
bool makeEvent(tsl::AppState* _appState, int tindex, int pluginIdx, Event& e,
               const tsl::iplug::DawParam*& outDp)
{
	if (pluginIdx < 0 || pluginIdx >= tsl::iplug::paramCount) return false;
	const auto& p = tsl::iplug::dp[pluginIdx];
	if (p.num == PARAM_NOT_ASSIGNED) return false;

	e.setup(_appState, tindex, p.num);
	if (p.num == OFFGRAIN || p.num == OFFFX || p.num == OFFSTEREOFX ||
	    p.num == BYPASSFX || p.num == BYPASSGRAINFX || p.num == BYPASSSTEREOFX)
		e.paramIndex = p.offset;

	outDp = &p;
	return true;
}

bool isFxPower(const Event& e)
{
	return e.eventType == Eventtype::Power &&
	       (e.subType == EventSubtype::powerGrainFx ||
	        e.subType == EventSubtype::powerFx ||
	        e.subType == EventSubtype::powerStereoFx);
}

// Fills one row. snapEvents is the track's snapshot event list, already fetched.
bool fillRow(tsl::AppState* _appState, int t, int pluginIdx,
             const std::vector<Event>& snapEvents, GsTestParamRow& row)
{
	Event e;
	const tsl::iplug::DawParam* p = nullptr;
	if (!makeEvent(_appState, t, pluginIdx, e, p)) return false;

	auto& par = _STATE->parameters[p->num];
	const int hostIdx = pluginIdx + t * tsl::iplug::paramCount;

	row = GsTestParamRow{};
	row.pluginIdx = pluginIdx;
	row.num       = p->num;
	row.hostIdx   = hostIdx;
	row.type      = (int32_t)par.type;
	row.eventType = (int32_t)e.eventType;
	row.subType   = (int32_t)e.subType;
	row.castInt    = (par.flags & Param::CastInt) ? 1 : 0;
	row.paramFlags = par.flags;
	row.dpFlags    = (uint32_t)p->flags;
	row.minVal     = par.min;
	row.maxVal     = par.max;

	const double engine = e.getCurrentValue(_appState);
	const double def    = e.getDefaultValue(_appState);

	// Event::operator== matches on eventType/trackIndex/paramIndex/subType --
	// exactly the identity Snapshot::addEvent dedupes on. events_ holds INTERNAL
	// (un-normalized) values; publish_ normalizes only into the blob copy, so
	// these compare directly against getCurrentValue.
	double snapVal = 0.0;
	for (auto& se : snapEvents) {
		if (se == e) { row.hasSnap = 1; snapVal = se.value; break; }
	}

	row.engine = engine;
	row.def    = def;
	row.snap   = snapVal;

	if (isFxPower(e)) {
		row.isPower = 1;
		auto ps = std::bit_cast<PowerState>(engine);
		auto pd = std::bit_cast<PowerState>(def);
		row.enginePow = (int32_t)ps.pow;
		row.enginePos = ps.pos;
		row.defPow    = (int32_t)pd.pow;
		if (row.hasSnap) {
			auto ss = std::bit_cast<PowerState>(snapVal);
			row.snapPow = (int32_t)ss.pow;
			row.snapPos = ss.pos;
		}
	}

	row.engineDisplay = par.toDisplay((int)_STATE->sr, engine);
	row.engineNormInt = (par.max > par.min) ? (engine - par.min) / (par.max - par.min) : 0.0;

	row.host     = gPlug->GetParam(hostIdx)->Value();
	row.hostNorm = gPlug->GetParam(hostIdx)->GetNormalized();
	return true;
}

// Round-trip a marker through the audio thread. A timeout is itself a finding:
// it means ProcessBlock is not running, so every ToAudioThread event is
// stranded -- which is exactly how "power on an effect" silently fails.
bool syncAudioThread(tsl::AppState* _appState, int timeoutMs)
{
	auto flag = std::make_shared<std::atomic<bool>>(false);
	if (!_DATA->toAudioThreadQueue.try_push([flag] { flag->store(true, std::memory_order_release); }))
		return false;

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (!flag->load(std::memory_order_acquire)) {
		if (std::chrono::steady_clock::now() > deadline) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

// Round-trip a marker through the snapshot worker. Deliberately not
// MPSCWorker::post_and_wait: that blocks forever if the worker shuts down
// mid-wait (see the comment on post_and_wait in FunctionQueueWorker.h).
bool syncSnapshotWorker(tsl::AppState* _appState, int timeoutMs)
{
	auto flag = std::make_shared<std::atomic<bool>>(false);
	if (!_DATA->snapShot.add_task([flag] { flag->store(true, std::memory_order_release); }))
		return false;

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (!flag->load(std::memory_order_acquire)) {
		if (std::chrono::steady_clock::now() > deadline) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

// Audio decoding runs on WorkerQueue and only then hands off to the snapshot
// worker, so a load is not finished until this drains too. Its depth is 16;
// a decode of a large file can hold the slot for a long time, hence the
// generous timeouts callers pass for audio.
bool syncWorkerQueue(tsl::AppState* _appState, int timeoutMs)
{
	auto flag = std::make_shared<std::atomic<bool>>(false);
	if (!_STATE->WorkerQueue.add_task([flag] { flag->store(true, std::memory_order_release); }))
		return false;

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (!flag->load(std::memory_order_acquire)) {
		if (std::chrono::steady_clock::now() > deadline) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

void jsonEscape(const char* in, std::string& out)
{
	out.clear();
	if (!in) return;
	for (const char* c = in; *c; ++c) {
		switch (*c) {
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n";  break;
		case '\r': out += "\\r";  break;
		case '\t': out += "\\t";  break;
		default:
			if ((unsigned char)*c < 0x20) { char b[8]; snprintf(b, 8, "\\u%04x", *c); out += b; }
			else out += *c;
		}
	}
}

// %.17g so a value survives the JSON round trip bit-exactly. Non-finite values
// become null rather than invalid JSON -- a null in the report is a finding, not
// a parse error.
void appendNum(std::string& s, double v)
{
	if (std::isnan(v) || std::isinf(v)) { s += "null"; return; }
	char b[40];
	snprintf(b, sizeof(b), "%.17g", v);
	s += b;
}

} // namespace

// A host may hold several instances at once (the project-reload scenario opens
// a second one). The hooks are plain C symbols with no instance argument, so
// they address the most recently constructed LIVE instance; closing it falls
// back to the one before, rather than leaving every hook dead.
namespace {
struct Instance { IPlugEffect* plug; tsl::AppState* state; };
std::vector<Instance> gInstances;

void refreshCurrent()
{
	if (gInstances.empty()) { gPlug = nullptr; gAppState = nullptr; return; }
	gPlug     = gInstances.back().plug;
	gAppState = gInstances.back().state;
}
} // namespace

void gsTestRegisterInstance(IPlugEffect* plug, tsl::AppState* appState)
{
	gInstances.push_back({plug, appState});
	refreshCurrent();
}

void gsTestUnregisterInstance(IPlugEffect* plug)
{
	for (auto it = gInstances.begin(); it != gInstances.end(); ++it) {
		if (it->plug == plug) { gInstances.erase(it); break; }
	}
	refreshCurrent();
}

extern "C" {

int32_t gsTestAbiVersion(void) { return GS_TEST_ABI_VERSION; }

int32_t gsTestReady(void) { return (gPlug && gAppState) ? 1 : 0; }

int32_t gsTestParamCount(void) { return tsl::iplug::paramCount; }

int32_t gsTestTrackCount(void) { return kTracks; }

int32_t gsTestIsRestoringState(void)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	return _DATA->isRestoringState.load() ? 1 : 0;
}

int32_t gsTestIsPlaying(void)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	return _STATE->player.isPlaying() ? 1 : 0;
}

int32_t gsTestApplyFromUi(int32_t tindex, int32_t pluginIdx, double norm)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	if (tindex < 0 || tindex >= kTracks) return -1;

	Event e;
	const tsl::iplug::DawParam* p = nullptr;
	if (!makeEvent(_appState, tindex, pluginIdx, e, p)) return -2;

	auto& par = _STATE->parameters[p->num];

	if (par.type == ParameterType_bool) {
		if ((p->flags & tsl::iplug::FlagBypass) || (p->flags & tsl::iplug::FlagPower)) {
			switch (p->num) {
			case BYPASSGRAINFX:
			case BYPASSSTEREOFX:
			case BYPASSFX:
				e.value = norm > 0.5 ? 1.0 : 0.0;
				e.applyFromExt(_appState, tsl::parameters::FromUi);
				return 0;
			case OFFFX:
			case OFFGRAIN:
			case OFFSTEREOFX:
				// power.pos keeps the INT32_MAX sentinel Event::setup put there.
				e.power.pow = norm > 0.5 ? 1u : 0u;
				e.applyFromExt(_appState, tsl::parameters::FromUi);
				return 0;
			default:
				return -3;
			}
		}
		e.value = norm > 0.5 ? 1.0 : 0.0;
		e.apply(_appState, tsl::parameters::FromUi);
		return 0;
	}

	if (par.type == ParameterType_double) {
		e.value = par.min + (par.max - par.min) * norm;
		e.apply(_appState, tsl::parameters::FromUi);
		return 0;
	}

	if (par.type == ParameterType_enum) {
		e.value = par.fromNormalized(norm);
		e.apply(_appState, tsl::parameters::FromUi);
		return 0;
	}

	return -4;
}

// UNDO/REDO go through applyFromExt, which queues onto the snapshot worker --
// the same path the button takes, so the queueing and ordering are under test.
int32_t gsTestUndo(int32_t tindex)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	if (tindex < 0 || tindex >= kTracks) return -1;
	Event e;
	e.setup(_appState, tindex, UNDOBUTTON);
	e.applyFromExt(_appState, tsl::parameters::FromUi);
	return 0;
}

int32_t gsTestRedo(int32_t tindex)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	if (tindex < 0 || tindex >= kTracks) return -1;
	Event e;
	e.setup(_appState, tindex, REDOBUTTON);
	e.applyFromExt(_appState, tsl::parameters::FromUi);
	return 0;
}

int32_t gsTestHasUndo(int32_t tindex)
{
	if (!gAppState || tindex < 0 || tindex >= kTracks) return -1;
	auto _appState = gAppState;
	return _DATA->snapShot.hasUndos[tindex].load() ? 1 : 0;
}

int32_t gsTestHasRedo(int32_t tindex)
{
	if (!gAppState || tindex < 0 || tindex >= kTracks) return -1;
	auto _appState = gAppState;
	return _DATA->snapShot.hasRedos[tindex].load() ? 1 : 0;
}

int32_t gsTestSync(int32_t timeoutMs)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	int32_t fails = 0;
	// Order follows how work actually flows: decode -> snapshot -> audio thread,
	// then snapshot again because audio-thread work posts back.
	if (!syncWorkerQueue(_appState, timeoutMs))    fails |= 16;
	if (!syncSnapshotWorker(_appState, timeoutMs)) fails |= 1;
	if (!syncAudioThread(_appState, timeoutMs))    fails |= 2;
	if (!syncSnapshotWorker(_appState, timeoutMs)) fails |= 4;

	// isRestoringState gates OnParamChange and freezes SerializeState on the old
	// snapshot; one that never clears is the most damaging failure mode here.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (_DATA->isRestoringState.load()) {
		if (std::chrono::steady_clock::now() > deadline) { fails |= 8; break; }
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return fails;
}

int32_t gsTestReadParams(int32_t t, GsTestParamRow* rows, int32_t maxRows)
{
	if (!gAppState || !gPlug || !rows) return -1;
	if (t < 0 || t >= kTracks) return -1;
	auto _appState = gAppState;

	std::vector<Event> snapEvents;
	_DATA->snapShot.getEvents(snapEvents, t);

	int32_t n = 0;
	for (int i = 0; i < tsl::iplug::paramCount && n < maxRows; i++) {
		if (fillRow(_appState, t, i, snapEvents, rows[n])) n++;
	}
	return n;
}

int32_t gsTestReadBlob(unsigned char* out, int32_t maxBytes)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	auto blob = _DATA->snapShot.get();
	if (!blob || !blob->mem) return -2;
	if (out && maxBytes > 0)
		std::memcpy(out, blob->mem, std::min<int32_t>(maxBytes, blob->size));
	return blob->size;
}

int32_t gsTestDumpState(const char* path)
{
	if (!gAppState || !gPlug || !path) return -1;
	auto _appState = gAppState;

	std::string out;
	out.reserve(1 << 20);
	std::string esc;

	out += "{\n";
	out += "  \"paramCount\": ";           appendNum(out, tsl::iplug::paramCount);
	out += ",\n  \"sr\": ";                appendNum(out, _STATE->sr);
	out += ",\n  \"isRestoringState\": ";  out += _DATA->isRestoringState.load() ? "true" : "false";
	out += ",\n  \"isPlaying\": ";         out += _STATE->player.isPlaying() ? "true" : "false";

	// The published blob: byte-identical to what SerializeState hands the host,
	// so the host can diff it against the ClassInfo payload.
	auto blob = _DATA->snapShot.get();
	out += ",\n  \"blobSize\": ";  appendNum(out, blob ? blob->size : -1);
	out += ",\n  \"blob\": \"";
	if (blob && blob->mem) {
		static const char* hex = "0123456789abcdef";
		for (int i = 0; i < blob->size; i++) {
			out += hex[(blob->mem[i] >> 4) & 0xf];
			out += hex[blob->mem[i] & 0xf];
		}
	}
	out += "\"";

	out += ",\n  \"tracks\": [\n";
	for (int t = 0; t < kTracks; t++) {
		std::vector<Event> snapEvents;
		_DATA->snapShot.getEvents(snapEvents, t);

		out += "    {\n      \"track\": ";  appendNum(out, t);
		out += ",\n      \"hasUndo\": ";    out += _DATA->snapShot.hasUndos[t].load() ? "true" : "false";
		out += ",\n      \"hasRedo\": ";    out += _DATA->snapShot.hasRedos[t].load() ? "true" : "false";
		out += ",\n      \"snapEventCount\": "; appendNum(out, (double)snapEvents.size());

		out += ",\n      \"params\": [\n";
		bool first = true;
		for (int i = 0; i < tsl::iplug::paramCount; i++) {
			GsTestParamRow row;
			if (!fillRow(_appState, t, i, snapEvents, row)) continue;

			if (!first) out += ",\n";
			first = false;

			jsonEscape(_STATE->parameters[row.num].name, esc);
			out += "        {\"i\": ";   appendNum(out, row.pluginIdx);
			out += ", \"num\": ";        appendNum(out, row.num);
			out += ", \"hostIdx\": ";    appendNum(out, row.hostIdx);
			out += ", \"name\": \"" + esc + "\"";
			out += ", \"type\": ";       appendNum(out, row.type);
			out += ", \"eventType\": ";  appendNum(out, row.eventType);
			out += ", \"subType\": ";    appendNum(out, row.subType);
			out += ", \"castInt\": ";    out += row.castInt ? "true" : "false";
			out += ", \"min\": ";        appendNum(out, row.minVal);
			out += ", \"max\": ";        appendNum(out, row.maxVal);

			if (row.isPower) {
				out += ", \"power\": true";
				out += ", \"enginePow\": "; appendNum(out, row.enginePow);
				out += ", \"enginePos\": "; appendNum(out, row.enginePos);
				out += ", \"defPow\": ";    appendNum(out, row.defPow);
				if (row.hasSnap) {
					out += ", \"snapPow\": "; appendNum(out, row.snapPow);
					out += ", \"snapPos\": "; appendNum(out, row.snapPos);
				} else {
					out += ", \"snapPow\": null, \"snapPos\": null";
				}
			} else {
				out += ", \"power\": false";
				out += ", \"engine\": "; appendNum(out, row.engine);
				out += ", \"def\": ";    appendNum(out, row.def);
				out += ", \"snap\": ";   if (row.hasSnap) appendNum(out, row.snap); else out += "null";
			}

			out += ", \"host\": ";     appendNum(out, row.host);
			out += ", \"hostNorm\": "; appendNum(out, row.hostNorm);
			out += "}";
		}
		out += "\n      ],\n";

		// Non-parameter events (Recording, FxOrder, Preset, ...) carry state the
		// param table cannot express; dump them raw so nothing is invisible.
		out += "      \"events\": [\n";
		for (size_t k = 0; k < snapEvents.size(); k++) {
			auto& se = snapEvents[k];
			if (k) out += ",\n";
			out += "        {\"eventType\": "; appendNum(out, (double)se.eventType);
			out += ", \"subType\": ";          appendNum(out, (double)se.subType);
			out += ", \"paramIndex\": ";       appendNum(out, (double)se.paramIndex);
			out += ", \"flags\": ";            appendNum(out, (double)se.flags);
			out += ", \"value\": ";            appendNum(out, se.value);
			out += "}";
		}
		out += "\n      ]\n    }";
		if (t < kTracks - 1) out += ",";
		out += "\n";
	}
	out += "  ]\n}\n";

	FILE* f = fopen(path, "wb");
	if (!f) return -2;
	const size_t written = fwrite(out.data(), 1, out.size(), f);
	fclose(f);
	return written == out.size() ? 0 : -3;
}

// --- audio -----------------------------------------------------------------

int32_t gsTestSetActiveTrack(int32_t track)
{
	if (!gAppState || track < 0 || track >= kTracks) return -1;
	auto _appState = gAppState;
	_STATE->active_track.store(track);
	return 0;
}

int32_t gsTestLoadAudio(int32_t track, const char* path)
{
	if (!gAppState || !path || track < 0 || track >= kTracks) return -1;
	auto _appState = gAppState;

	// filebrowsercallback loads into the active track, exactly as the file
	// browser does, so select it first rather than reaching past the real path.
	_STATE->active_track.store(track);
	return filebrowsercallback(_appState, std::string(path));
}

int32_t gsTestAudioInfo(int32_t track, GsTestAudioInfo* out)
{
	if (!gAppState || !out || track < 0 || track >= kTracks) return -1;
	auto _appState = gAppState;

	*out = GsTestAudioInfo{};
	auto rec = _DATA->tracks[track]->filebuffer.load();
	if (!rec) return 0;

	out->hasAudio   = 1;
	out->poolHandle = (int32_t)rec->poolHandle;
	out->numEdits   = (int32_t)rec->numEdits;
	out->off        = (double)rec->off;
	std::snprintf(out->fileName, sizeof(out->fileName), "%s", rec->fileName.c_str());

	if (auto st = rec->state.load()) {
		out->offStart = st->off_start.load();
		out->offStop  = st->off_stop.load();
		out->offset   = st->offset.load();
	}
	return 1;
}

// --- the app's own preset system -------------------------------------------

int32_t gsTestPresetSave(const char* name, int32_t isProject)
{
	if (!gAppState || !name) return -1;
	auto _appState = gAppState;
	return gsTestSavePresetNamed(_appState, std::string(name), isProject != 0) ? 0 : -2;
}

int32_t gsTestPresetCount(int32_t isProject)
{
	if (!gAppState) return -1;
	std::vector<std::shared_ptr<tsl::preset::PresetWrapper>> presets;
	tsl::preset::getPresets(presets, isProject != 0);
	return (int32_t)presets.size();
}

int32_t gsTestPresetName(int32_t index, int32_t isProject, char* out, int32_t maxLen)
{
	if (!gAppState || !out || maxLen <= 0) return -1;
	std::vector<std::shared_ptr<tsl::preset::PresetWrapper>> presets;
	tsl::preset::getPresets(presets, isProject != 0);
	if (index < 0 || index >= (int32_t)presets.size()) return -2;
	std::snprintf(out, maxLen, "%s", presets[index]->name.c_str());
	return 0;
}

int32_t gsTestPresetLoad(int32_t index, int32_t isProject)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;

	std::vector<std::shared_ptr<tsl::preset::PresetWrapper>> presets;
	tsl::preset::getPresets(presets, isProject != 0);
	if (index < 0 || index >= (int32_t)presets.size()) return -2;

	// The same call the preset list makes once a row is picked. Runs the whole
	// in-app load path -- which, unlike the host's ClassInfo restore, goes
	// through Snapshot::apply(fromDaw=false) and so also builds a preset
	// undo/redo entry.
	auto t = _DATA->tracks[_STATE->active_track.load()];
	return tsl::preset::loadPresetThreadFunc(t, presets[index]) ? 0 : -3;
}

// --- MIDI learn -------------------------------------------------------------

int32_t gsTestMidiLearnCC(int32_t track, int32_t pluginIdx, int32_t channel, int32_t cc)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	if (track < 0 || track >= kTracks) return -1;
	if (channel < 0 || channel >= NUM_MIDICHANNELS) return -1;
	if (cc < 0 || cc >= NUM_MIDI_CONTROL) return -1;

	Event e;
	const tsl::iplug::DawParam* p = nullptr;
	if (!makeEvent(_appState, track, pluginIdx, e, p)) return -2;

	auto& m = _STATE->parameters[e.getDisplayParam()];
	// midicontrolevents is the continuous-controller table; MidiLearning routes
	// bool/enum params to midinoteevents instead.
	if (m.type != ParameterType_double) return -3;

	// Mirrors the assignment in MidiLearning.cpp, mapped over the full range.
	e.midiState.type    = m.type;
	e.midiState.channel = (uint8_t)channel;
	e.midiState.num     = (uint8_t)cc;
	e.midiState.min     = (uint16_t)(m.toNormalized(m.min) * UINT16_MAX);
	e.midiState.max     = (uint16_t)(m.toNormalized(m.max) * UINT16_MAX);

	_STATE->midicontrolevents[channel][cc] = e;
	return 0;
}

int32_t gsTestSetSaveAudioWithPreset(int32_t on)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	_DATA->saveAudioWithPreset = on != 0;
	return 0;
}

int32_t gsTestMidiClearLearn(void)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
		for (int n = 0; n < NUM_MIDI_CONTROL; n++)
			_STATE->midicontrolevents[ch][n].eventType = Eventtype::NoParam;
		for (int n = 0; n < NUM_MIDI_NOTEON; n++)
			_STATE->midinoteevents[ch][n].eventType = Eventtype::NoParam;
	}
	return 0;
}

int32_t gsTestMarkers(int32_t* numParameters, int32_t* numParams)
{
	if (!numParameters || !numParams) return -1;
	*numParameters = (int32_t)NUM_PARAMETERS;
	*numParams     = (int32_t)NUM_PARAMS;
	return 0;
}

int32_t gsTestReadParamByNum(int32_t track, int32_t num, GsTestParamRow* out)
{
	if (!gAppState || !out) return -1;
	auto _appState = gAppState;
	if (track < 0 || track >= kTracks) return -1;
	if (num <= 0 || num >= NUM_PARAMS) return -2;

	auto& par = _STATE->parameters[num];
	Event e;
	e.setup(_appState, track, num);

	*out = GsTestParamRow{};
	out->pluginIdx  = -1;
	out->hostIdx    = -1;
	out->num        = num;
	out->type       = (int32_t)par.type;
	out->eventType  = (int32_t)e.eventType;
	out->subType    = (int32_t)e.subType;
	out->isPower    = isFxPower(e) ? 1 : 0;
	out->castInt    = (par.flags & Param::CastInt) ? 1 : 0;
	out->paramFlags = par.flags;
	out->minVal     = par.min;
	out->maxVal     = par.max;
	out->engine     = e.getCurrentValue(_appState);
	out->def        = e.getDefaultValue(_appState);
	return 0;
}

int32_t gsTestApplyByNum(int32_t track, int32_t num, double value)
{
	if (!gAppState) return -1;
	auto _appState = gAppState;
	if (track < 0 || track >= kTracks) return -1;
	if (num <= 0 || num >= NUM_PARAMS) return -2;

	Event e;
	e.setup(_appState, track, num);
	e.value = value;
	e.apply(_appState, tsl::parameters::FromUi);
	return 0;
}

} // extern "C"

#endif // GS_TEST_HOOKS
