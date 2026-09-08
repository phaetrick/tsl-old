#include "IPlugEffect.h"
#ifdef OS_IOS
#include <tools/PlatformPaths.h>
#endif
#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include <IPlugParamDefs.h>
#include <defines.h>
#include <iostream>
#include <types.h>
#include <setup.h>


#ifdef ENABLE_INTEGRITY_CHECK
#include <CodeSigning.h>
#include <SecurityTools.h>
#include <magic_hash.h>
#endif
#include <Midi.h>
#include <MidiReceiver.h>
#include <app.h>
#include <grainstorm.h>
#include <infopanel.h>
#include <logger.h>
#include <params.h>
#include <player.h>
#include <preset.h>
#include <setup.h>
#include <vector>
#include <window.h>

IPlugEffect::~IPlugEffect()
{
	TRACE
	cleanUp(_appState);
	_appState = nullptr;
}

IPlugEffect::IPlugEffect(const InstanceInfo& info)
	: Plugin(info, MakeConfig(tsl::iplug::paramCount, 0))
{

	SetChannelConnections(ERoute::kInput, 0, MaxNChannels(ERoute::kInput), true);
	SetChannelConnections(ERoute::kOutput, 0, MaxNChannels(ERoute::kOutput), true);

	// Label your channels
	SetChannelLabel(ERoute::kInput, 0, "Track1 L");
	SetChannelLabel(ERoute::kInput, 1, "Track1 R");
	SetChannelLabel(ERoute::kInput, 2, "Track2 L");
	SetChannelLabel(ERoute::kInput, 3, "Track2 R");
	SetChannelLabel(ERoute::kInput, 4, "Track3 L");
	SetChannelLabel(ERoute::kInput, 5, "Track3 R");
	SetChannelLabel(ERoute::kInput, 6, "Track4 L");
	SetChannelLabel(ERoute::kInput, 7, "Track4 R");
	SetChannelLabel(ERoute::kOutput, 0, "Main L");
	SetChannelLabel(ERoute::kOutput, 1, "Main R");
#if defined OS_IOS
	_appState = tsl::app::setup(tsl::app::isRunningAsAppExtension());
	_DATA->isRunningAsPlugin = tsl::app::isRunningAsAppExtension();
	_appState->onPurchaseComplete = [this](bool success, const std::string& productId) {
		if (!success) return;
		tsl::app::onIAPComplete(_appState, productId);
	};
#else
	_appState = tsl::app::setup(GetAPI() != kAPIAPP);
	_DATA->isRunningAsPlugin = (GetAPI() != kAPIAPP);
#endif
	_appState->openURL = [&](const std::string& url) {
#ifdef _WIN32
		std::string command = "start \"\" \"" + url + "\"";
		system(command.c_str());
#elif defined(OS_IOS)
		tsl::app::openURL(url);
#else
		pid_t pid = fork();
		if (pid == 0)
		{
#ifdef __APPLE__
			execlp("open", "open", url.c_str(), nullptr);
#else
			execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
#endif
			exit(1);
		}
#endif
		};

#ifdef PLUGIN_MODE
	if (_DATA->isRunningAsPlugin) {
	_STATE->InformHostOfParamChange = [this](tsl::parameters::Event& e, tsl::parameters::SenderFlags from) {
		auto idx = e.getPluginIndex(_STATE);
		if (idx >= 0) {
			auto value = e.eventType == tsl::parameters::Eventtype::Power && (e.subType == tsl::parameters::EventSubtype::powerGrainFx || e.subType == tsl::parameters::EventSubtype::powerFx || e.subType == tsl::parameters::EventSubtype::powerStereoFx) ? e.power.pow : e.value;

			int pindex = idx % tsl::iplug::paramCount;
			auto& p = tsl::iplug::dp[pindex];
			auto& par = _STATE->parameters[p.num];
			double normalized = par.toNormalized(value);

			if (from != tsl::parameters::FromUi) {
				_STATE->toUiThreadQueue.try_push([this, idx, value, normalized] {
					paramTimer.reset();
					GetParam(idx)->SetNormalized(normalized);
					InformHostOfParamChange(idx, normalized);
#ifdef VST3_API
					componentHandler->restartComponent(Steinberg::Vst::kParamValuesChanged);
#endif
					});
			}
			else {
				paramTimer.reset();
				GetParam(idx)->SetNormalized(normalized);
				InformHostOfParamChange(idx, normalized);
#ifdef VST3_API
				componentHandler->restartComponent(Steinberg::Vst::kParamValuesChanged);
#endif
			}
		}
		};
	_STATE->InformHostOfParamChangeDirect = [this](tsl::parameters::Event& e) {
		auto idx = e.getPluginIndex(_STATE);
		if (idx >= 0) {
			auto value = e.eventType == tsl::parameters::Eventtype::Power && (e.subType == tsl::parameters::EventSubtype::powerGrainFx || e.subType == tsl::parameters::EventSubtype::powerFx || e.subType == tsl::parameters::EventSubtype::powerStereoFx) ? e.power.pow : e.value;
			int pindex = idx % tsl::iplug::paramCount;
			auto& p = tsl::iplug::dp[pindex];
			auto& par = _STATE->parameters[p.num];
			double normalized = par.toNormalized(value);
			paramTimer.reset();
			GetParam(idx)->SetNormalized(normalized);
			InformHostOfParamChange(idx, normalized);
		}
		};

	_STATE->RequestHistory = [this]() {
#ifdef VST3_API
		componentHandler->restartComponent(Steinberg::Vst::kParamValuesChanged);
#endif
		};

	_STATE->BeginEndInformHostOfParamChangePrivate = [this](tsl::parameters::Event& e, bool end) {
		auto idx = e.getPluginIndex(_STATE);
		if (idx >= 0)

			if (end)
				EndInformHostOfParamChange(idx);
			else
				BeginInformHostOfParamChange(idx);
		};

	_STATE->onParamChange = [this](tsl::parameters::Event& e, tsl::parameters::SenderFlags from) {
		if (_STATE->InformHostOfParamChange && !_DATA->isRestoringState)
			_STATE->InformHostOfParamChange(e, from);
		};
	} // if (isRunningAsPlugin)
#endif

	tsl::app::setupthr(_appState);
	auto sr = _STATE->sr;

	{
		for (int tindex = 0; tindex < 1; tindex++)
		{
			for (int i = 0; i < tsl::iplug::paramCount; i++)
			{
				int index = i + tindex * tsl::iplug::paramCount;
				auto& p = tsl::iplug::dp[i];
				auto& par = _STATE->parameters[p.num];
				par.pluginIndex = i;
				if (p.num == PARAM_NOT_ASSIGNED) {
					GetParam(index)->InitDouble("Reserved", std::numeric_limits<double>::max(), 0.0, std::numeric_limits<double>::max(), 0.01, "", IParam::kFlagCannotAutomate, "Reserved");
					continue;
				}
				char name[100]{};
				int pos = 0;
				tsl::parameters::Event e;
				e.setup(_STATE, tindex, p.num);
				e.toString(_STATE, pos, name, 100, false);


				if (par.type == ParameterType_bool)
				{

					if (p.flags & tsl::iplug::FlagPower) {
						GetParam(index)->InitBool(name, e.getDefaultValue(_STATE) == 1. ? true : false, "", 0, "", "OFF", "ON");
					}
					else if (p.flags & tsl::iplug::FlagBypass) {
						GetParam(index)->InitBool(name, false, "", 0, "", "ON", "OFF");
					}
					else if (p.flags & tsl::iplug::FlagOneShot)
						GetParam(index)->InitBool(name, par.initvalue == 1. ? true : false, "", 0, "", "TRIGGER", "TRIGGER");
					else{
						GetParam(index)->InitBool(name, e.getDefaultValue(_STATE) == 1. ? true : false, "", 0, "", "OFF", "ON");
				}
				}
				else if (par.type == ParameterType_double)
				{
					if (par.flags & Param::CastInt)
					{
						GetParam(index)->InitInt(name, par.toDisplay(sr, par.initvalue), par.toDisplay(sr, par.min), par.toDisplay(sr, par.max), par.valuename ? par.valuename : "", 0);
					}
					else
					{
						GetParam(index)->InitDouble(name,
							(par.initvalue - par.min) / (par.max - par.min),  // normalized init
							0.0, 1.0, 0.001, "", 0);

						GetParam(index)->SetDisplayFunc([this, num = p.num, sr](double norm, WDL_String& str) {
							auto& par = _STATE->parameters[num];

							double internal = par.min + (par.max - par.min) * norm;
							double display = par.toDisplay(sr, internal);
							str.SetFormatted(32, "%.*f %s", par.digits, display, par.valuename ? par.valuename : "");
							});
					}
				}
				else if (par.type == ParameterType_enum)
				{
					// Range must come from whichever list defines the options.
					// Using std::size(par.names) alone underflows to InitInt(...,0,-1)
					// for enums that only set .values, corrupting the value on every
					// state save/restore.
					int count = !par.names.empty() ? (int)par.names.size()
					                               : (int)par.values.size();
					if (count < 1) count = 1;
					GetParam(index)->InitInt(name, par.toDisplay(_STATE->sr, e.getDefaultValue(_STATE)), 0, count - 1, "", 0);
					int i = 0;
					for (auto& sv : par.names)
						GetParam(index)->SetDisplayText(i++, sv.data());

				}
			}
		}

	}

#if IPLUG_EDITOR
	mMakeGraphicsFunc = [&]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT)); };

	mLayoutFunc = [&](IGraphics* pGraphics) { pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false); };
#endif
}

#if IPLUG_DSP

inline double BeatsPerBar(const ITimeInfo& ti)
{
	const double num = ti.mNumerator > 0 ? (double)ti.mNumerator : 4.0;
	const double den = ti.mDenominator > 0 ? (double)ti.mDenominator : 4.0;
	return num * (4.0 / den);
}

void IPlugEffect::OnParamChange(int paramIdx, EParamSource source, int sampleOffset) {

	if (!_DATA->isRunningAsPlugin) return;
	if (source != EParamSource::kHost || paramTimer.elapsed() < 0.2 || _DATA->isRestoringState || !mFirstSerializeDone)
		return;

	if (GetParam(paramIdx)->Value() == std::numeric_limits<double>::max())return;

	int index = paramIdx % tsl::iplug::paramCount;
	auto tindex = static_cast<int>(floor(paramIdx / tsl::iplug::paramCount));

	auto& p = tsl::iplug::dp[index];
	auto& par = _STATE->parameters[p.num];
	if (par.paramChanging[tindex].load(std::memory_order_acquire) > 0)return;
	tsl::parameters::Event e;
	e.setup(_STATE, tindex, p.num);
	e.flags |= tsl::parameters::Event::FromDaw;
	if (par.type == ParameterType_bool)
	{
		if (p.flags & tsl::iplug::FlagBypass || p.flags & tsl::iplug::FlagPower)
		{

		}
		else
		{
			e.value = GetParam(paramIdx)->Value() > 0.5 ? 1.0 : 0.0;
			e.applyFromExt(_STATE, tsl::parameters::None);
		}
	}
	else if (par.type == ParameterType_double)
	{
		double norm = GetParam(paramIdx)->GetNormalized();
		double internal = par.min + (par.max - par.min) * norm;
		e.value = internal; // already pre-display, no fromDisplay needed
		e.apply(_STATE, tsl::parameters::None);
	}
	else if (par.type == ParameterType_enum)
	{
		e.value = par.fromNormalized(GetParam(paramIdx)->GetNormalized());
		e.apply(_STATE, tsl::parameters::None);
	}
}

void IPlugEffect::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{

#if defined OS_IOS
	// TODO: re-enable IAP gate before release
	// if (_DATA->isRunningAsPlugin && !_appState->auv3Purchased) {
	// 	for (int ch = 0; ch < MaxNChannels(ERoute::kOutput); ++ch)
	// 		memset(outputs[ch], 0, nFrames * sizeof(sample));
	// 	return;
	// }
#endif
	int channelMask = 0;
	if (IsChannelConnected(ERoute::kOutput, 0))
		channelMask |= (1 << OUTPUT_ACTIVE_BIT_L);
	if (IsChannelConnected(ERoute::kOutput, 1))
		channelMask |= (1 << OUTPUT_ACTIVE_BIT_R);

	for (int i = 0; i < MAX_CHANNELS; ++i)
		if (IsChannelConnected(ERoute::kInput, i))
			channelMask |= (1 << i);

	if (_STATE->player._isplaying.load())
	{
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		if (_DATA->isRunningAsPlugin) {
		const ITimeInfo& ti = mTimeInfo;
		_DATA->hostTimeSnapshot.beatsPerBar = BeatsPerBar(ti);
		_DATA->hostTimeSnapshot.bpm = ti.mTempo;
		_DATA->hostTimeSnapshot.loopEnabled = ti.mTransportLoopEnabled;
		_DATA->hostTimeSnapshot.loopEndPPQ = ti.mCycleEnd;
		_DATA->hostTimeSnapshot.loopStartPPQ = ti.mCycleStart;
		_DATA->hostTimeSnapshot.ppqNow = ti.mPPQPos;
		_DATA->hostTimeSnapshot.running = ti.mTransportIsRunning;
		bool discontinuity = false;

		if (ti.mTransportIsRunning && !_DATA->hostTimeSnapshot.wasRunning)
			discontinuity = true;

		if (ti.mTransportLoopEnabled &&
			_DATA->hostTimeSnapshot.lastPpqPos >= ti.mCycleEnd &&
			ti.mPPQPos <= ti.mCycleStart + 1e-6)
			discontinuity = true;

		if (ti.mTransportIsRunning)
		{
			double expectedAdvance = (double)nFrames * (ti.mTempo / 60.0) / GetSampleRate();
			double actualAdvance = ti.mPPQPos - _DATA->hostTimeSnapshot.lastPpqPos;
			if (fabs(actualAdvance - expectedAdvance) > 0.01)
				discontinuity = true;
		}

		if (!ti.mTransportIsRunning && _DATA->hostTimeSnapshot.wasRunning)
			discontinuity = true;

		if (discontinuity)
			_DATA->hostTimeSnapshot.triggerPPQ = ti.mPPQPos;
		_DATA->hostTimeSnapshot.wasRunning = ti.mTransportIsRunning;
		_DATA->hostTimeSnapshot.lastPpqPos = _DATA->hostTimeSnapshot.ppqNow;
		}
#endif
		_STATE->player.onAudioReady(inputs, outputs, nFrames, channelMask);

		if (_STATE->player.isrecording.load(std::memory_order_acquire)) {
			int totalSamples = nFrames * MAX_CHANNELS;
			int srcFrame = 0;
			while (totalSamples > 0) {
				auto recItem = static_cast<tsl::AudioBuffer<sampleTSL>*>(
					_STATE->pool.acquire(sizeof(tsl::AudioBuffer<sampleTSL>)));
				if (!recItem) break;
				int chunkSamples = std::min(totalSamples, tsl::audioBufferDefaultSize);
				recItem->frames = chunkSamples / MAX_CHANNELS;
				recItem->channels = MAX_CHANNELS;
				int idx = 0;
				for (int f = 0; f < recItem->frames; f++, srcFrame++) {
					recItem->data[idx++] = (sampleTSL)outputs[0][srcFrame];
					recItem->data[idx++] = (sampleTSL)outputs[1][srcFrame];
				}
				if (!_STATE->player.recQueue.try_push(recItem)) {
					_STATE->pool.release(recItem);
				} else {
					_STATE->waitNotify.wake_thread(_STATE->player.slot);
				}
				totalSamples -= chunkSamples;
			}
		}
	}
	else
	{
		bool in0 = IsChannelConnected(ERoute::kInput, 0);
		bool in1 = IsChannelConnected(ERoute::kInput, 1);
		bool out0 = IsChannelConnected(ERoute::kOutput, 0);
		bool out1 = IsChannelConnected(ERoute::kOutput, 1);

		for (int i = 0; i < nFrames; ++i)
		{
			outputs[0][i] = (out0 && in0) ? inputs[0][i] : 0.0;
			outputs[1][i] = (out1 && in1) ? inputs[1][i] : 0.0;
		}
	}

}

void IPlugEffect::OnReset()
{

	sampleRateFromApp(_appState, GetSampleRate());
	mIsInitialized = true;

}

void IPlugEffect::OnActivate(bool active) {
}




bool IPlugEffect::SerializeState(IByteChunk& chunk) const
{
	const_cast<IPlugEffect*>(this)->mFirstSerializeDone = true;
	return SerializeParams(chunk);
}

int IPlugEffect::UnserializeState(const IByteChunk& chunk, int startPos)
{
	int pos = UnserializeParams(chunk, startPos);

	_DATA->isRestoringState = true;
	for (int paramIdx = 0; paramIdx < tsl::iplug::paramCount; paramIdx++) {
		if (GetParam(paramIdx)->Value() == std::numeric_limits<double>::max()) continue;
		auto& p = tsl::iplug::dp[paramIdx];
		auto& par = _STATE->parameters[p.num];
		tsl::parameters::Event e;
		e.setup(_STATE, 0, p.num);
		e.flags |= tsl::parameters::Event::FromDaw;
		if (par.type == ParameterType_bool) {
			if (!(p.flags & tsl::iplug::FlagBypass) && !(p.flags & tsl::iplug::FlagPower)) {
				e.value = GetParam(paramIdx)->Value() > 0.5 ? 1.0 : 0.0;
				e.applyFromExt(_STATE, tsl::parameters::None);
			}
		} else if (par.type == ParameterType_double) {
			double norm = GetParam(paramIdx)->GetNormalized();
			e.value = par.min + (par.max - par.min) * norm;
			e.apply(_STATE, tsl::parameters::None);
		} else if (par.type == ParameterType_enum) {
			e.value = par.fromNormalized(GetParam(paramIdx)->GetNormalized());
			e.apply(_STATE, tsl::parameters::None);
		}
	}
	_DATA->isRestoringState = false;
	return pos;
}

constexpr uint8_t STATUS_REALTIME_EVENT = 0x01;
constexpr uint8_t STATUS_TIMING_CLOCK = 0xF8;

void IPlugEffect::ProcessMidiMsg(const IMidiMsg& msg)
{
	const uint8_t status = static_cast<uint8_t>(msg.mStatus);
	const uint8_t type = status & 0xF0;
switch (type)
	{
	case 0x90: // Note On
	{
		// many controllers send NoteOn velocity 0 as NoteOff
		if (msg.mData2 == 0)
		{
			onMidiMsg(_STATE,
				0x80,
				msg.mData1,
				0);
		}
		else
		{
			onMidiMsg(_STATE,
				status,
				msg.mData1,
				msg.mData2);
		}

		break;
	}

	case 0x80: // Note Off
	case 0xA0: // Poly Aftertouch
	case 0xB0: // Control Change
	case 0xD0: // Channel Aftertouch
	{
		onMidiMsg(_STATE,
			status,
			msg.mData1,
			msg.mData2);

		break;
	}

	default:
		break;
	}
}

#endif