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
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

  std::vector<sample> buf;
#endif
 
private:
	bool mIsInitialized = false;
	bool mFirstSerializeDone = false;
	tools::AtomicTimer paramTimer{};
};
