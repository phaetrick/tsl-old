// ── THE V2 WRAPPER'S HEADER ──
//
// IPlugEffect.h forked, the way grainstorm/ forked into grainstorm2/ and
// graphics/ into tslgraphics2/. It is byte-identical to IPlugEffect.h as of the
// fork and is expected to stay close to it: everything the master bus needed
// went into the .cpp, and the class this declares has neither a new member nor
// a new override because of it. It exists so that the two lines can move apart
// without GS1's header being the place they meet.
//
// The CLASS NAME is still IPlugEffect. config.h's PLUG_CLASS_NAME is what
// iPlug2 builds its factory symbol, its AU view controller and its bundle
// entry points out of, and renaming the class would rename all of them.
// Which of the two files is compiled is CMake's GS2 option, not a name.

#pragma once
#include <chrono>
#include <atomic>

namespace tools {
    class AtomicTimer {
    public:
        double elapsedReplace() {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> res = end - beg.load();
            beg.store(end);
            return res.count();
        }

        double elapsed() {
            std::chrono::duration<double> res = std::chrono::system_clock::now() - beg.load();
            return res.count();
        }

        void reset() { beg.store(std::chrono::system_clock::now()); }
        void set0() { beg.store(std::chrono::time_point<std::chrono::system_clock>{}); };

    private:
        std::atomic<std::chrono::time_point<std::chrono::system_clock>> beg{
                std::chrono::system_clock::now() };
    };
}


#include "IPlug_include_in_plug_hdr.h"

using namespace iplug;
using namespace igraphics;

class IPlugEffect final : public Plugin
{
public:
  IPlugEffect(const InstanceInfo& info);
  ~IPlugEffect();

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  //void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnParamChange(int paramIdx, EParamSource source, int sampleOffset) override;

  //void OnParamChange(int paramIdx)override;
  /** Override this method in your plug-in class to do something prior to playback etc. (e.g.clear buffers, update internal DSP with the latest sample rate) */
  void OnReset() override;

  /** Override OnActivate() which should be called by the API class when a plug-in is "switched on" by the host on a track when the channel count is known.
   * This may not work reliably because different hosts have different interpretations of "activate".
   * Unlike OnReset() which called when the transport is reset or the sample rate changes OnActivate() is a good place to handle change of I/O connections.
   * @param active \c true if the host has activated the plug-in */
  void OnActivate(bool active) override;
  // SAVE PRESET/STATE: Only get the size and pointer from your StreamWrapper
  bool SerializeState(IByteChunk& chunk) const override;
  // LOAD PRESET/STATE: Read the entire binary data back into your StreamWrapper
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

  std::vector<sample> buf;
#endif

  // Runs on the main thread via iPlug2's idle timer, independent of the editor
  // being open. Drains param changes queued from the snapshot worker thread and
  // delivers them to the host.
  void OnIdle() override;

	// Pushes every host-visible parameter whose IParam disagrees with the engine
	// back to the host. Main thread only. See the call site in OnIdle().
	void SyncHostParamsFromEngine();

private:
	bool mIsInitialized = false;
	// "The host has established this instance's state." Set by SerializeState,
	// by UnserializeState, and as a backstop on the first ProcessBlock -- AU
	// ordering is instantiate -> SetProperty(ClassInfo) -> Initialize -> render,
	// so by the first render the host has had its chance to send state.
	// OnParamChange ignores host writes until then; when only SerializeState set
	// it, a project load (which never serializes) left the gate shut for the
	// whole session and every automation write was silently dropped.
	// Written from the audio thread, read from the main thread. mutable because
	// SerializeState is const.
	mutable std::atomic<bool> mHostStateEstablished{ false };
	tools::AtomicTimer paramTimer{};
	// Producer: snapshot worker thread only. Consumer: main thread (OnIdle).
	IPlugQueue<ParamTuple> mParamChangesToHost{ 4096 };
	std::atomic<bool> mParamSyncRequested{ false };
	// Raised by UnserializeState. A restore applies its events with the FromDaw
	// flag, which suppresses the push back to the host, and track->reset() /
	// setDefaults() rewrite params with no event at all -- so without this sweep
	// nothing ever updates the IParams and the host reads pre-restore values for
	// the rest of the session.
	std::atomic<bool> mHostResyncPending{ false };
};
