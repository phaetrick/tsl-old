// TestHooks.h — ABI shared between the test-only hooks compiled into the plugin
// (TestHooks.cpp, gated on GS_TEST_HOOKS) and the external test host
// (tests/auhost), which dlsym's these symbols out of the loaded bundle.
//
// Both sides are built from this repo with the same toolchain, so a plain struct
// is a safe wire format. Bump GS_TEST_ABI_VERSION on any layout change; the host
// checks it before reading anything.

#pragma once

#include <stdint.h>

#define GS_TEST_ABI_VERSION 4

// The plugin target builds with -fvisibility=hidden and -flto, so without an
// explicit attribute these are compiled but never land in the export table and
// dlsym cannot find them. `used` additionally keeps LTO from dropping a
// function nothing inside the bundle calls.
#ifdef GS_TEST_HOOKS_BUILDING
	#define GS_TEST_EXPORT __attribute__((visibility("default"), used))
#else
	#define GS_TEST_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// One row per host-exposed parameter, per track. The three value columns are the
// whole point of the harness:
//   engine — Event::getCurrentValue(): what the DSP is actually running on
//   snap   — the value carried by the snapshot (hasSnap==0 means "no event",
//            which the format defines as "at default", so def applies)
//   host   — the IParam value the DAW reads back
// For fx power params (power==1) the packed PowerState double is meaningless to
// compare directly, so pow/pos are split out instead.
typedef struct GsTestParamRow
{
	int32_t pluginIdx;    // index within one track's param block
	int32_t num;          // internal param id (types_grainstorm.h enum)
	int32_t hostIdx;      // pluginIdx + track * paramCount
	int32_t type;         // ParameterType_bool / _double / _enum
	int32_t eventType;    // tsl::parameters::Eventtype
	int32_t subType;      // tsl::parameters::EventSubtype
	int32_t isPower;      // 1 = fx power event, use pow/pos fields
	int32_t hasSnap;      // 1 = the snapshot carries an event for this param
	int32_t castInt;      // Param::CastInt — host param is in display units
	uint32_t paramFlags;  // Param::ParamFlags (NoValue, NoAssignment, ...)
	uint32_t dpFlags;     // tsl::iplug::ParamFlags (FlagPower/FlagBypass/FlagOneShot)

	double engine;        // live engine value (internal units)
	double def;           // default value (internal units)
	double snap;          // snapshot value (internal units), valid if hasSnap
	double host;          // IParam::Value()
	double hostNorm;      // IParam::GetNormalized()
	double minVal;
	double maxVal;

	// Derived from engine via the Param library conversions the ctor declared,
	// so the host can predict what IParam::Value() should be without knowing the
	// per-type mapping. Deliberately computed from Param::toDisplay (library),
	// not from the plugin glue under test.
	double engineDisplay;   // par.toDisplay(sr, engine) — for CastInt and enum params
	double engineNormInt;   // (engine - min) / (max - min) — for plain double params

	int32_t enginePow, enginePos;   // isPower only
	int32_t snapPow,   snapPos;     // isPower only, valid if hasSnap
	int32_t defPow,    pad2;
} GsTestParamRow;

// --- lifecycle -------------------------------------------------------------
GS_TEST_EXPORT int32_t gsTestAbiVersion(void);
GS_TEST_EXPORT int32_t gsTestReady(void);
GS_TEST_EXPORT int32_t gsTestParamCount(void);
GS_TEST_EXPORT int32_t gsTestTrackCount(void);
GS_TEST_EXPORT int32_t gsTestIsRestoringState(void);
GS_TEST_EXPORT int32_t gsTestIsPlaying(void);

// --- drive the two non-host change sources ---------------------------------
// norm is 0..1 over the parameter's internal [min,max] — the same convention the
// plugin's InitDouble uses, so it matches what a host would write.
GS_TEST_EXPORT int32_t gsTestApplyFromUi(int32_t track, int32_t pluginIdx, double norm);
GS_TEST_EXPORT int32_t gsTestUndo(int32_t track);
GS_TEST_EXPORT int32_t gsTestRedo(int32_t track);
GS_TEST_EXPORT int32_t gsTestHasUndo(int32_t track);
GS_TEST_EXPORT int32_t gsTestHasRedo(int32_t track);

// --- quiescence ------------------------------------------------------------
// Drains snapshot worker -> audio thread -> snapshot worker, then waits for
// isRestoringState to clear. Returns 0 when fully quiescent, else a bitmask:
// 1 = snapshot worker, 2 = audio thread, 4 = snapshot worker (2nd),
// 8 = isRestoringState stuck true.
GS_TEST_EXPORT int32_t gsTestSync(int32_t timeoutMs);

// --- read state ------------------------------------------------------------
// Fills up to maxRows rows for one track; returns the number written, or <0.
GS_TEST_EXPORT int32_t gsTestReadParams(int32_t track, GsTestParamRow* rows, int32_t maxRows);

// Copies the currently published snapshot blob (byte-identical to what
// SerializeState hands the host). Returns the blob size, or <0. Pass out=NULL to
// query the size.
GS_TEST_EXPORT int32_t gsTestReadBlob(unsigned char* out, int32_t maxBytes);

// Human-readable JSON dump of everything above, for post-mortem inspection.
GS_TEST_EXPORT int32_t gsTestDumpState(const char* path);

// --- audio -----------------------------------------------------------------
// What a track currently holds. A Recording is referenced from the snapshot by
// file name + length, so these are the fields a state round trip must preserve.
typedef struct GsTestAudioInfo
{
	int32_t hasAudio;
	int32_t poolHandle;
	int32_t numEdits;
	int32_t pad;
	double  off;        // length in frames
	double  offStart;
	double  offStop;
	double  offset;
	char    fileName[512];
} GsTestAudioInfo;

// Drives filebrowsercallback — the exact entry point the file browser uses.
// Asynchronous (WorkerQueue -> snapshot worker); follow with gsTestSync.
GS_TEST_EXPORT int32_t gsTestLoadAudio(int32_t track, const char* path);
GS_TEST_EXPORT int32_t gsTestAudioInfo(int32_t track, GsTestAudioInfo* out);
GS_TEST_EXPORT int32_t gsTestSetActiveTrack(int32_t track);

// --- the app's own preset system (NOT the host's ClassInfo) ----------------
// Saves via savePresetGotFileName, i.e. everything after the name dialog.
GS_TEST_EXPORT int32_t gsTestPresetSave(const char* name, int32_t isProject);
// Number of presets/projects currently on disk.
GS_TEST_EXPORT int32_t gsTestPresetCount(int32_t isProject);
// Loads by index into the current active track (presets) or all tracks
// (projects), through loadPresetThreadFunc — the same call the preset list makes.
GS_TEST_EXPORT int32_t gsTestPresetLoad(int32_t index, int32_t isProject);
GS_TEST_EXPORT int32_t gsTestPresetName(int32_t index, int32_t isProject, char* out, int32_t maxLen);

// --- MIDI learn ------------------------------------------------------------
// Creates the mapping MidiLearning would create, so a subsequent real CC from
// the host exercises the midicontrolevents path rather than the unmapped
// fallback. Range is the parameter's full min..max.
GS_TEST_EXPORT int32_t gsTestMidiLearnCC(int32_t track, int32_t pluginIdx, int32_t channel, int32_t cc);
GS_TEST_EXPORT int32_t gsTestMidiClearLearn(void);

// Embeds the track audio in the preset (FLAC) instead of storing a file path.
// Desktop defaults this off and Android on, so the two platforms take different
// branches of the preset writer -- the embedded branch appends FLAC data after
// the events and then seeks back to rewrite the header.
GS_TEST_EXPORT int32_t gsTestSetSaveAudioWithPreset(int32_t on);

// --- params addressed by internal enum id ----------------------------------
// Params outside dp[] have no pluginIdx, so the hooks above cannot reach them.
// These address the whole id space, to test what actually persists across the
// NUM_PARAMETERS marker.
GS_TEST_EXPORT int32_t gsTestMarkers(int32_t* numParameters, int32_t* numParams);
// Fills the library-side fields of one row (pluginIdx/hostIdx are -1 and the
// host columns stay 0 -- a param outside dp[] has no host view).
GS_TEST_EXPORT int32_t gsTestReadParamByNum(int32_t track, int32_t num, GsTestParamRow* row);
// Applies VALUE (internal units) the way a widget does: Event::setup + apply
// with FromUi. Follow with gsTestSync.
GS_TEST_EXPORT int32_t gsTestApplyByNum(int32_t track, int32_t num, double value);

#ifdef __cplusplus
}
#endif
