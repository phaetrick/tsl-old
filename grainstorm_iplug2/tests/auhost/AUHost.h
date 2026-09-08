// AUHost.h — minimal in-process AUv2 host.
//
// Loads the .component's executable with dlopen and drives it through the
// factory's AudioComponentPlugInInterface, exactly as the AudioUnit component
// manager does: Open -> Lookup(selector) -> call the returned method with the
// interface pointer as self. Everything below the component manager's
// registration layer is the real thing -- the same IPlugAU::AUMethod* entry
// points a DAW reaches.
//
// Going through dlopen rather than AudioComponentInstanceNew is deliberate:
//   * the test build is never installed, so it never shadows or conflicts with
//     the shipping Grainstorm.component (same type/subtype/manufacturer),
//   * the same dlopen handle yields the gsTest* hook symbols,
//   * no dependence on AudioComponentRegistrar cache behaviour.
// What it does NOT cover: the registrar/Info.plist layer. That is not what this
// harness is testing.

#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <string>

#include "../../TestHooks.h"

struct GsTestApi
{
	int32_t (*abiVersion)(void)                                    = nullptr;
	int32_t (*ready)(void)                                         = nullptr;
	int32_t (*paramCount)(void)                                    = nullptr;
	int32_t (*trackCount)(void)                                    = nullptr;
	int32_t (*isRestoringState)(void)                              = nullptr;
	int32_t (*isPlaying)(void)                                     = nullptr;
	int32_t (*applyFromUi)(int32_t, int32_t, double)               = nullptr;
	int32_t (*undo)(int32_t)                                       = nullptr;
	int32_t (*redo)(int32_t)                                       = nullptr;
	int32_t (*hasUndo)(int32_t)                                    = nullptr;
	int32_t (*hasRedo)(int32_t)                                    = nullptr;
	int32_t (*sync)(int32_t)                                       = nullptr;
	int32_t (*readParams)(int32_t, GsTestParamRow*, int32_t)       = nullptr;
	int32_t (*readBlob)(unsigned char*, int32_t)                   = nullptr;
	int32_t (*dumpState)(const char*)                              = nullptr;

	int32_t (*loadAudio)(int32_t, const char*)                     = nullptr;
	int32_t (*audioInfo)(int32_t, GsTestAudioInfo*)                = nullptr;
	int32_t (*setActiveTrack)(int32_t)                             = nullptr;
	int32_t (*presetSave)(const char*, int32_t)                    = nullptr;
	int32_t (*presetCount)(int32_t)                                = nullptr;
	int32_t (*presetLoad)(int32_t, int32_t)                        = nullptr;
	int32_t (*presetName)(int32_t, int32_t, char*, int32_t)        = nullptr;
	int32_t (*midiLearnCC)(int32_t, int32_t, int32_t, int32_t)     = nullptr;
	int32_t (*midiClearLearn)(void)                                = nullptr;
	int32_t (*setSaveAudioWithPreset)(int32_t)                     = nullptr;
	int32_t (*markers)(int32_t*, int32_t*)                         = nullptr;
	int32_t (*readParamByNum)(int32_t, int32_t, GsTestParamRow*)   = nullptr;
	int32_t (*applyByNum)(int32_t, int32_t, double)                = nullptr;

	bool complete() const
	{
		return abiVersion && ready && paramCount && trackCount && isRestoringState &&
		       isPlaying && applyFromUi && undo && redo && hasUndo && hasRedo && sync &&
		       readParams && readBlob && dumpState && loadAudio && audioInfo &&
		       setActiveTrack && presetSave && presetCount && presetLoad && presetName &&
		       midiLearnCC && midiClearLearn && setSaveAudioWithPreset &&
		       markers && readParamByNum && applyByNum;
	}
};

class AUHost
{
public:
	~AUHost();

	// bundlePath is the .component directory. Loads and Opens, but does not
	// Initialize -- so a caller can exercise the pre-initialize window.
	bool open(const std::string& bundlePath, std::string& err);

	// Sets stream formats + max frames, wires a silent input callback, then
	// Initialize. sampleRate/blockSize must be set before this.
	bool initialize(double sampleRate, int blockSize, std::string& err);

	void close();

	// --- AU API ------------------------------------------------------------
	OSStatus getProperty(AudioUnitPropertyID id, AudioUnitScope scope,
	                     AudioUnitElement elem, void* data, UInt32* size) const;
	OSStatus setProperty(AudioUnitPropertyID id, AudioUnitScope scope,
	                     AudioUnitElement elem, const void* data, UInt32 size) const;
	OSStatus getParameter(AudioUnitParameterID id, AudioUnitParameterValue* v) const;
	OSStatus setParameter(AudioUnitParameterID id, AudioUnitParameterValue v) const;
	OSStatus midiEvent(UInt32 status, UInt32 d1, UInt32 d2, UInt32 offset) const;
	OSStatus renderOnce();

	// --- state -------------------------------------------------------------
	// Caller owns the returned plist (CFRelease). Null on failure.
	CFPropertyListRef getClassInfo() const;
	bool              setClassInfo(CFPropertyListRef plist) const;

	// Serializes a plist to bytes so two saves can be compared exactly.
	static bool plistToBytes(CFPropertyListRef plist, std::string& out);

	// Extracts just the plugin's own state chunk from a ClassInfo plist, which
	// is what SerializeState produced. Empty if absent.
	static bool chunkFromClassInfo(CFPropertyListRef plist, std::string& out);

	// --- render thread -----------------------------------------------------
	// A DAW renders continuously; so must we, or every ToAudioThread event
	// (fx power on/off, track enable) is stranded and never applies.
	void startRenderThread();
	void stopRenderThread();
	unsigned long long renderCount() const;

	int paramCount() const { return mParamCount; }
	int blockSize()  const { return mBlockSize; }

	GsTestApi hooks{};

private:
	static OSStatus inputCallback(void* refCon, AudioUnitRenderActionFlags* flags,
	                              const AudioTimeStamp* ts, UInt32 bus,
	                              UInt32 nFrames, AudioBufferList* io);

	bool loadHooks(std::string& err);

	void*                          mDL    = nullptr;
	AudioComponentPlugInInterface* mIface = nullptr;
	bool                           mOpened = false;
	bool                           mInited = false;

	OSStatus (*mInitialize)(void*)                                                                       = nullptr;
	OSStatus (*mUninitialize)(void*)                                                                     = nullptr;
	OSStatus (*mGetProperty)(void*, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, void*, UInt32*)      = nullptr;
	OSStatus (*mSetProperty)(void*, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, const void*, UInt32) = nullptr;
	OSStatus (*mGetParameter)(void*, AudioUnitParameterID, AudioUnitScope, AudioUnitElement, AudioUnitParameterValue*)         = nullptr;
	OSStatus (*mSetParameter)(void*, AudioUnitParameterID, AudioUnitScope, AudioUnitElement, AudioUnitParameterValue, UInt32)  = nullptr;
	OSStatus (*mRender)(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*)           = nullptr;
	OSStatus (*mMIDIEvent)(void*, UInt32, UInt32, UInt32, UInt32)                                        = nullptr;
	OSStatus (*mReset)(void*, AudioUnitScope, AudioUnitElement)                                          = nullptr;

	double mSampleRate = 48000.0;
	int    mBlockSize  = 512;
	int    mParamCount = 0;

	struct RenderState;
	RenderState* mRender_ = nullptr;
};
