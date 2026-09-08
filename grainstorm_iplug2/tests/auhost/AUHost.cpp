#include "AUHost.h"

#include <dlfcn.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace {

// The bundle's executable, e.g. Foo.component/Contents/MacOS/Foo.
std::string executablePath(const std::string& bundlePath)
{
	std::string base = bundlePath;
	while (!base.empty() && base.back() == '/') base.pop_back();
	auto slash = base.find_last_of('/');
	std::string leaf = slash == std::string::npos ? base : base.substr(slash + 1);
	auto dot = leaf.find_last_of('.');
	if (dot != std::string::npos) leaf = leaf.substr(0, dot);
	return base + "/Contents/MacOS/" + leaf;
}

} // namespace

struct AUHost::RenderState
{
	std::thread            thread;
	std::atomic<bool>      run{false};
	std::atomic<unsigned long long> count{0};
};

AUHost::~AUHost() { close(); }

bool AUHost::open(const std::string& bundlePath, std::string& err)
{
	const std::string exe = executablePath(bundlePath);
	mDL = dlopen(exe.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!mDL) { err = std::string("dlopen failed: ") + dlerror(); return false; }

	using FactoryFn = AudioComponentPlugInInterface* (*)(const AudioComponentDescription*);
	auto factory = (FactoryFn)dlsym(mDL, "IPlugEffect_Factory");
	if (!factory) { err = "IPlugEffect_Factory not exported by the bundle"; return false; }

	AudioComponentDescription desc{};
	desc.componentType         = 'aumf';
	desc.componentSubType      = 'Ipef';
	desc.componentManufacturer = 'TSLa';

	mIface = factory(&desc);
	if (!mIface) { err = "factory returned null"; return false; }

	// Open's second argument becomes IPlugAU::mCI, which is what the plugin puts
	// in the AudioUnitEvent it sends on InformHostOfParamChange. Passing the
	// interface pointer gives us a stable handle a listener can match on.
	if (mIface->Open(mIface, (AudioUnit)mIface) != noErr) { err = "Open failed"; return false; }
	mOpened = true;

	auto look = [&](SInt16 sel) { return mIface->Lookup(sel); };
	mInitialize   = (decltype(mInitialize))   look(kAudioUnitInitializeSelect);
	mUninitialize = (decltype(mUninitialize)) look(kAudioUnitUninitializeSelect);
	mGetProperty  = (decltype(mGetProperty))  look(kAudioUnitGetPropertySelect);
	mSetProperty  = (decltype(mSetProperty))  look(kAudioUnitSetPropertySelect);
	mGetParameter = (decltype(mGetParameter)) look(kAudioUnitGetParameterSelect);
	mSetParameter = (decltype(mSetParameter)) look(kAudioUnitSetParameterSelect);
	mRender       = (decltype(mRender))       look(kAudioUnitRenderSelect);
	mMIDIEvent    = (decltype(mMIDIEvent))    look(kMusicDeviceMIDIEventSelect);
	mReset        = (decltype(mReset))        look(kAudioUnitResetSelect);

	if (!mInitialize || !mGetProperty || !mSetProperty || !mGetParameter ||
	    !mSetParameter || !mRender) {
		err = "plugin did not vend the required AU selectors";
		return false;
	}

	// Global param count, so scenarios can address every track's block.
	AudioUnitParameterID ids[8192];
	UInt32 sz = sizeof(ids);
	if (mGetProperty(mIface, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids, &sz) == noErr)
		mParamCount = (int)(sz / sizeof(AudioUnitParameterID));

	return loadHooks(err);
}

bool AUHost::loadHooks(std::string& err)
{
	auto sym = [&](const char* n) { return dlsym(mDL, n); };
	hooks.abiVersion       = (decltype(hooks.abiVersion))       sym("gsTestAbiVersion");
	hooks.ready            = (decltype(hooks.ready))            sym("gsTestReady");
	hooks.paramCount       = (decltype(hooks.paramCount))       sym("gsTestParamCount");
	hooks.trackCount       = (decltype(hooks.trackCount))       sym("gsTestTrackCount");
	hooks.isRestoringState = (decltype(hooks.isRestoringState)) sym("gsTestIsRestoringState");
	hooks.isPlaying        = (decltype(hooks.isPlaying))        sym("gsTestIsPlaying");
	hooks.applyFromUi      = (decltype(hooks.applyFromUi))      sym("gsTestApplyFromUi");
	hooks.undo             = (decltype(hooks.undo))             sym("gsTestUndo");
	hooks.redo             = (decltype(hooks.redo))             sym("gsTestRedo");
	hooks.hasUndo          = (decltype(hooks.hasUndo))          sym("gsTestHasUndo");
	hooks.hasRedo          = (decltype(hooks.hasRedo))          sym("gsTestHasRedo");
	hooks.sync             = (decltype(hooks.sync))             sym("gsTestSync");
	hooks.readParams       = (decltype(hooks.readParams))       sym("gsTestReadParams");
	hooks.readBlob         = (decltype(hooks.readBlob))         sym("gsTestReadBlob");
	hooks.dumpState        = (decltype(hooks.dumpState))        sym("gsTestDumpState");
	hooks.loadAudio        = (decltype(hooks.loadAudio))        sym("gsTestLoadAudio");
	hooks.audioInfo        = (decltype(hooks.audioInfo))        sym("gsTestAudioInfo");
	hooks.setActiveTrack   = (decltype(hooks.setActiveTrack))   sym("gsTestSetActiveTrack");
	hooks.presetSave       = (decltype(hooks.presetSave))       sym("gsTestPresetSave");
	hooks.presetCount      = (decltype(hooks.presetCount))      sym("gsTestPresetCount");
	hooks.presetLoad       = (decltype(hooks.presetLoad))       sym("gsTestPresetLoad");
	hooks.presetName       = (decltype(hooks.presetName))       sym("gsTestPresetName");
	hooks.midiLearnCC      = (decltype(hooks.midiLearnCC))      sym("gsTestMidiLearnCC");
	hooks.midiClearLearn   = (decltype(hooks.midiClearLearn))   sym("gsTestMidiClearLearn");
	hooks.setSaveAudioWithPreset = (decltype(hooks.setSaveAudioWithPreset)) sym("gsTestSetSaveAudioWithPreset");
	hooks.markers          = (decltype(hooks.markers))          sym("gsTestMarkers");
	hooks.readParamByNum   = (decltype(hooks.readParamByNum))   sym("gsTestReadParamByNum");
	hooks.applyByNum       = (decltype(hooks.applyByNum))       sym("gsTestApplyByNum");

	if (!hooks.complete()) {
		err = "gsTest* hooks missing — build the plugin with -DGS_TEST_HOOKS=ON";
		return false;
	}
	if (hooks.abiVersion() != GS_TEST_ABI_VERSION) {
		err = "GsTestParamRow ABI mismatch between plugin and host";
		return false;
	}
	return true;
}

OSStatus AUHost::inputCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*,
                               UInt32, UInt32 nFrames, AudioBufferList* io)
{
	// Silence. The harness verifies state, not audio content; a non-silent
	// source would only make the render thread's cost less predictable.
	if (io)
		for (UInt32 i = 0; i < io->mNumberBuffers; i++)
			if (io->mBuffers[i].mData)
				std::memset(io->mBuffers[i].mData, 0, io->mBuffers[i].mDataByteSize);
	return noErr;
}

bool AUHost::initialize(double sampleRate, int blockSize, std::string& err)
{
	mSampleRate = sampleRate;
	mBlockSize  = blockSize;

	UInt32 maxFrames = (UInt32)blockSize;
	if (mSetProperty(mIface, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
	                 0, &maxFrames, sizeof(maxFrames)) != noErr) {
		err = "could not set MaximumFramesPerSlice";
		return false;
	}

	AudioStreamBasicDescription asbd{};
	asbd.mSampleRate       = sampleRate;
	asbd.mFormatID         = kAudioFormatLinearPCM;
	asbd.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
	asbd.mBytesPerPacket   = 4;
	asbd.mFramesPerPacket  = 1;
	asbd.mBytesPerFrame    = 4;
	asbd.mChannelsPerFrame = 2;
	asbd.mBitsPerChannel   = 32;

	if (mSetProperty(mIface, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof(asbd)) != noErr) {
		err = "could not set input stream format";
		return false;
	}
	if (mSetProperty(mIface, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof(asbd)) != noErr) {
		err = "could not set output stream format";
		return false;
	}

	AURenderCallbackStruct cb{};
	cb.inputProc       = inputCallback;
	cb.inputProcRefCon = this;
	mSetProperty(mIface, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));

	if (mInitialize(mIface) != noErr) { err = "AudioUnitInitialize failed"; return false; }
	mInited = true;
	return true;
}

void AUHost::close()
{
	stopRenderThread();
	if (mInited && mUninitialize) { mUninitialize(mIface); mInited = false; }
	if (mOpened && mIface)        { mIface->Close(mIface); mOpened = false; mIface = nullptr; }
	// The bundle is deliberately left loaded: a second AUHost in the same
	// process reuses it, and dlclose on a plugin with live worker threads is a
	// good way to crash on exit for reasons that have nothing to do with the
	// behaviour under test.
	mDL = nullptr;
}

OSStatus AUHost::getProperty(AudioUnitPropertyID id, AudioUnitScope scope,
                             AudioUnitElement elem, void* data, UInt32* size) const
{
	return mGetProperty(mIface, id, scope, elem, data, size);
}

OSStatus AUHost::setProperty(AudioUnitPropertyID id, AudioUnitScope scope,
                             AudioUnitElement elem, const void* data, UInt32 size) const
{
	return mSetProperty(mIface, id, scope, elem, data, size);
}

OSStatus AUHost::getParameter(AudioUnitParameterID id, AudioUnitParameterValue* v) const
{
	return mGetParameter(mIface, id, kAudioUnitScope_Global, 0, v);
}

OSStatus AUHost::setParameter(AudioUnitParameterID id, AudioUnitParameterValue v) const
{
	return mSetParameter(mIface, id, kAudioUnitScope_Global, 0, v, 0);
}

OSStatus AUHost::midiEvent(UInt32 status, UInt32 d1, UInt32 d2, UInt32 offset) const
{
	if (!mMIDIEvent) return kAudioUnitErr_InvalidProperty;
	return mMIDIEvent(mIface, status, d1, d2, offset);
}

OSStatus AUHost::renderOnce()
{
	const int ch = 2;
	std::vector<float> buf(ch * mBlockSize, 0.f);

	unsigned char alBytes[sizeof(AudioBufferList) + sizeof(AudioBuffer)]{};
	auto* abl = reinterpret_cast<AudioBufferList*>(alBytes);
	abl->mNumberBuffers = ch;
	for (int c = 0; c < ch; c++) {
		abl->mBuffers[c].mNumberChannels = 1;
		abl->mBuffers[c].mDataByteSize   = (UInt32)(mBlockSize * sizeof(float));
		abl->mBuffers[c].mData           = buf.data() + c * mBlockSize;
	}

	AudioTimeStamp ts{};
	ts.mFlags       = kAudioTimeStampSampleTimeValid;
	ts.mSampleTime  = (Float64)(mRender_ ? mRender_->count.load() * mBlockSize : 0);

	AudioUnitRenderActionFlags flags = 0;
	return mRender(mIface, &flags, &ts, 0, (UInt32)mBlockSize, abl);
}

void AUHost::startRenderThread()
{
	if (mRender_) return;
	mRender_ = new RenderState();
	mRender_->run.store(true);

	mRender_->thread = std::thread([this] {
		const int    ch      = 2;
		const double period  = mBlockSize / mSampleRate;
		std::vector<float> buf((size_t)ch * mBlockSize, 0.f);

		unsigned char alBytes[sizeof(AudioBufferList) + sizeof(AudioBuffer)]{};
		auto* abl = reinterpret_cast<AudioBufferList*>(alBytes);
		abl->mNumberBuffers = ch;
		for (int c = 0; c < ch; c++) {
			abl->mBuffers[c].mNumberChannels = 1;
			abl->mBuffers[c].mDataByteSize   = (UInt32)(mBlockSize * sizeof(float));
			abl->mBuffers[c].mData           = buf.data() + (size_t)c * mBlockSize;
		}

		auto next = std::chrono::steady_clock::now();
		while (mRender_->run.load(std::memory_order_acquire)) {
			AudioTimeStamp ts{};
			ts.mFlags      = kAudioTimeStampSampleTimeValid;
			ts.mSampleTime = (Float64)(mRender_->count.load() * mBlockSize);

			AudioUnitRenderActionFlags flags = 0;
			mRender(mIface, &flags, &ts, 0, (UInt32)mBlockSize, abl);
			mRender_->count.fetch_add(1, std::memory_order_release);

			// Wall-clock paced, like a real device callback. Running flat out
			// would starve the worker threads this harness is trying to observe.
			next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
			            std::chrono::duration<double>(period));
			std::this_thread::sleep_until(next);
		}
	});
}

void AUHost::stopRenderThread()
{
	if (!mRender_) return;
	mRender_->run.store(false, std::memory_order_release);
	if (mRender_->thread.joinable()) mRender_->thread.join();
	delete mRender_;
	mRender_ = nullptr;
}

unsigned long long AUHost::renderCount() const
{
	return mRender_ ? mRender_->count.load(std::memory_order_acquire) : 0;
}

CFPropertyListRef AUHost::getClassInfo() const
{
	CFPropertyListRef plist = nullptr;
	UInt32 sz = sizeof(plist);
	if (mGetProperty(mIface, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &plist, &sz) != noErr)
		return nullptr;
	return plist;
}

bool AUHost::setClassInfo(CFPropertyListRef plist) const
{
	if (!plist) return false;
	return mSetProperty(mIface, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global,
	                    0, &plist, sizeof(plist)) == noErr;
}

bool AUHost::plistToBytes(CFPropertyListRef plist, std::string& out)
{
	out.clear();
	if (!plist) return false;
	CFErrorRef cferr = nullptr;
	CFDataRef data = CFPropertyListCreateData(kCFAllocatorDefault, plist,
	                                          kCFPropertyListBinaryFormat_v1_0, 0, &cferr);
	if (!data) { if (cferr) CFRelease(cferr); return false; }
	out.assign((const char*)CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
	CFRelease(data);
	return true;
}

bool AUHost::chunkFromClassInfo(CFPropertyListRef plist, std::string& out)
{
	out.clear();
	if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) return false;
	auto dict = (CFDictionaryRef)plist;

	// iPlug2 stores the SerializeState chunk under kAUPresetDataKey ("data").
	CFStringRef key = CFSTR("data");
	const void* val = CFDictionaryGetValue(dict, key);
	if (!val || CFGetTypeID(val) != CFDataGetTypeID()) return false;

	auto data = (CFDataRef)val;
	out.assign((const char*)CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
	return true;
}
