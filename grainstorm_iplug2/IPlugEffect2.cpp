// ── THE V2 WRAPPER ──
//
// IPlugEffect.cpp forked. grainstorm/ was copied to grainstorm2/ and graphics/
// to tslgraphics2/ so that the new UI could be written without GS1 moving
// underneath it; this file is the last link in that chain, and until now the
// only one the two lines still shared. GS1 works and is shipping. Nothing here
// may reach back into it.
//
// What is different from IPlugEffect.cpp is the master bus, and only that. The
// bus is a FIFTH PARAMETER ROW, not a fifth set of parameter ids -- see the
// engine's types.h -- so on this side it is a FIFTH BLOCK at the end of the
// host's flat parameter list: masterParamBase = paramCount * NUM_TRACKS, and
// "everything that is bigger than the last param from track4 is a master
// effect param". Six places had to learn that: the config's parameter count,
// the ctor's registration pass, OnIdle, SyncHostParamsFromEngine and
// OnParamChange, plus the paramChanging counter those last two read.
//
// Everything else -- the state route in particular -- is 1:1 with GS1 and is
// meant to stay that way. Every read and write of a parameter goes through
// Event::setup / apply / applyFromExt / getCurrentValue, on the master row
// exactly as on a track row; params[] is never touched directly from here.

#include "IPlugEffect2.h"
#ifdef OS_IOS
#include <tools/PlatformPaths.h>
#endif
#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include <IPlugParamDefs.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <defines.h>
#include <gs_common.h>
#include <iostream>
#include <types.h>


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
#include <setup.h>
#include <vector>
#include <window.h>
constexpr auto minSnapSize = next_pow_2(512 * sizeof(tsl::parameters::Event) + 4 * 128);

constexpr int  tsl::parameters::supposeSnapSize(int bufsize) {
	return bufsize > minSnapSize ? next_pow_2(bufsize) : minSnapSize;
};

#ifdef GS_TEST_HOOKS
// Defined in TestHooks.cpp; compiled only with -DGS_TEST_HOOKS=ON.
void gsTestRegisterInstance(IPlugEffect*, tsl::AppState*);
void gsTestUnregisterInstance(IPlugEffect*);
#endif

IPlugEffect::~IPlugEffect()
{
	TRACE
#ifdef GS_TEST_HOOKS
		gsTestUnregisterInstance(this);
#endif
		cleanUp(_appState);

}

IPlugEffect::IPlugEffect(const InstanceInfo& info)
	// totalParamCount, not paramCount * 4: the host's list is the four track
	// blocks AND the master block that follows them. It is paramCount *
	// NUM_TRACKS + masterParamCount, which is where the 3196 this used to
	// declare went -- 3500 now.
	: Plugin(info, MakeConfig(tsl::iplug::totalParamCount, 0))
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
	// NOTE: _STATE->onParamChange is wired by the Snapshot constructor (History/snapshot.cpp).
	// It is the single hub for all four change sources (UI, undo/redo, MIDI, preset load):
	// it records events into the snapshot/undo system and calls the hooks below unless the
	// event carries the FromDaw flag. Do NOT reassign it here — that severs history and
	// DAW state serialization.

	// All callers are serialized onto the snapshot worker thread before reaching these
	// hooks, so pushing into the SPSC queue is safe. Delivery to the host happens on the
	// main thread in OnIdle(), which also runs while the editor is closed.
	auto pushParamToHost = [this](tsl::parameters::Event& e) {
		auto idx = e.getPluginIndex(_STATE);
		if (idx >= 0) {
			auto value = e.eventType == tsl::parameters::Eventtype::Power && (e.subType == tsl::parameters::EventSubtype::powerGrainFx || e.subType == tsl::parameters::EventSubtype::powerFx || e.subType == tsl::parameters::EventSubtype::powerStereoFx) ? e.power.pow : e.value;
			mParamChangesToHost.Push(ParamTuple{ idx, value });
		}
		};
	_STATE->InformHostOfParamChange = [pushParamToHost](tsl::parameters::Event& e, tsl::parameters::SenderFlags) {
		pushParamToHost(e);
		};
	_STATE->InformHostOfParamChangeDirect = pushParamToHost;

	_STATE->RequestHistory = [this]() {
		mParamSyncRequested.store(true, std::memory_order_release);
		};

	_STATE->BeginEndInformHostOfParamChangePrivate = [this](tsl::parameters::Event& e, bool end) {
		auto idx = e.getPluginIndex(_STATE);
		if (idx >= 0)

			if (end)
				EndInformHostOfParamChange(idx);
			else
				BeginInformHostOfParamChange(idx);
		};
	} // if (isRunningAsPlugin)
#endif

	tsl::app::setupthr(_appState);
	auto sr = _STATE->sr;

	{
		for (int tindex = 0; tindex < 4; tindex++)
		{
			for (int i = 0; i < tsl::iplug::paramCount; i++)
			{
				int index = i + tindex * tsl::iplug::paramCount;
				auto& p = tsl::iplug::dp[i];
				if (p.num == PARAM_NOT_ASSIGNED) {
					// A finite 0..1 range: the old DBL_MAX sentinel became inf/NaN in the
					// host's float32 param path and broke auval's state save/restore check.
					GetParam(index)->InitDouble("Reserved", 0.0, 0.0, 1.0, 0.01, "", IParam::kFlagCannotAutomate, "Reserved");
					continue;
				}
				auto& par = _STATE->parameters[p.num];
				par.pluginIndex = i;
				char name[100]{};
				int pos = 0;
				pos = snprintf(name, 100, "%s ", tracknames[tindex].data());
				tsl::parameters::Event e;
				e.setup(_STATE, tindex, p.num);
				if (p.num == OFFGRAIN || p.num == OFFFX || p.num == OFFSTEREOFX || p.num == BYPASSFX || p.num == BYPASSGRAINFX || p.num == BYPASSSTEREOFX)
					e.paramIndex = p.offset;
				e.toString(_STATE, pos, name, 100, false);


				if (par.type == ParameterType_bool)
				{

					if (p.flags & tsl::iplug::FlagPower) {
						GetParam(index)->InitBool(name, e.getDefaultValue(_STATE) == 1. ? true : false, "", 0, tracknames[tindex].data(), "OFF", "ON");
					}
					else if (p.flags & tsl::iplug::FlagBypass) {
						GetParam(index)->InitBool(name, false, "", 0, tracknames[tindex].data(), "ON", "OFF");
					}
					else if (p.flags & tsl::iplug::FlagOneShot)
						GetParam(index)->InitBool(name, par.initvalue == 1. ? true : false, "", 0, tracknames[tindex].data(), "TRIGGER", "TRIGGER");
					else{
						GetParam(index)->InitBool(name, e.getDefaultValue(_STATE) == 1. ? true : false, "", 0, tracknames[tindex].data(), "OFF", "ON");
				}
				}
				else if (par.type == ParameterType_double)
				{
					if (par.flags & Param::CastInt)
					{
						GetParam(index)->InitInt(name, par.toDisplay(sr, par.initvalue), par.toDisplay(sr, par.min), par.toDisplay(sr, par.max), par.valuename, 0, tracknames[tindex].data());
					}
					else
					{
						// Value = internal-normalized 0..1 so host sliders sit at the same
						// position as the app UI in EVERY host/format (AU hosts scale
						// linearly by value; VST3 normalized == value). Natural units are
						// provided as strings via the display func, which AU/AUv3/VST3
						// wrappers surface through their value->string mechanisms.
						GetParam(index)->InitDouble(name,
							(par.initvalue - par.min) / (par.max - par.min),  // normalized init
							0.0, 1.0, 0.001, "", 0, tracknames[tindex].data());

						GetParam(index)->SetDisplayFunc([this, num = p.num, sr](double norm, WDL_String& str) {
							auto& par = _STATE->parameters[num];

							double internal = par.min + (par.max - par.min) * norm;
							double display = par.toDisplay(sr, internal);
							str.SetFormatted(32, "%.*f %s", par.digits, display, par.valuename);
							});

						// Inverse of the display func, so values typed into a host's
						// param field ("2.5 x") map back to the normalized 0..1 value.
						GetParam(index)->SetStringToValueFunc([this, num = p.num, sr](const char* s, double& v) {
							auto& par = _STATE->parameters[num];

							double internal = par.fromDisplay(sr, atof(s));
							v = (internal - par.min) / (par.max - par.min);
							return true;
							});
					}
				}
				else if (par.type == ParameterType_enum)
				{
					GetParam(index)->InitInt(name, par.toDisplay(_STATE->sr, e.getDefaultValue(_STATE)), 0, std::size(par.names) - 1, "", 0, tracknames[tindex].data());
					int i = 0;
					for (auto& sv : par.names)
						GetParam(index)->SetDisplayText(i++, sv.data());

				}
			}
		}

		// ── THE MASTER BUS, AS A FIFTH BLOCK ──
		//
		// A fifth PASS, not a fifth track. The four passes above walk dp[] once
		// per track and place their parameters at i + tindex * paramCount; this
		// one walks masterDp[] once and places its parameters at
		// masterParamBase + i, after everything the four tracks claimed. That
		// ordering is the whole contract with the host -- an id past the last
		// track parameter is a master parameter -- and it is what lets the
		// master block be appended to later without moving a single index a
		// session might already have automated.
		//
		// The row is addressed by handing MASTER_ROW where a track index goes.
		// Event::setup answers it before its switch and the event comes back
		// carrying Eventtype::MasterParam, which is what every later read of
		// this parameter follows back to params[MASTER_ROW]. There is no second
		// entry point and no master-only parameter id.
		//
		// THREE BRANCHES THE TRACK PASSES HAVE AND THIS ONE DOES NOT, all for
		// the same reason -- masterDp[] does not contain the entries that need
		// them, and it is not dp[] and does not inherit its rules:
		//
		//   - No PARAM_NOT_ASSIGNED branch. dp[] carries reserved slots because
		//     inserting into it would move three tracks' worth of indices;
		//     masterDp[] is the last block, so it is appended to instead and
		//     holds no reserved slots at all.
		//   - No FlagPower / FlagBypass branch, and no paramIndex override for
		//     the OFF*/BYPASS* ids. On a track those resolve through the
		//     track's own two effect queues; the master chain is a third queue
		//     and has no host switches yet. When it gets them, they arrive as
		//     new entries appended here and BOTH branches come back with them.
		//   - No FlagOneShot branch, for the same reason: no entry has it.
		for (int i = 0; i < tsl::iplug::masterParamCount; i++)
		{
			const int index = tsl::iplug::masterParamBase + i;
			auto& p = tsl::iplug::masterDp[i];
			auto& par = _STATE->parameters[p.num];

			// DELIBERATELY NOT par.pluginIndex = i.
			//
			// pluginIndex is the parameter's index in dp[] -- the TRACK block --
			// and the four passes above have already written it correctly for
			// every id in here, because masterDp[] is dp[]'s own FX entries read
			// back out and holds nothing dp[] does not. Writing i over it would
			// break the track answer to buy nothing: a master event resolves its
			// host index through tsl::iplug::MasterParams instead, which is the
			// id -> masterDp position map, and Event::getPluginIndex takes that
			// branch on the event TYPE before it ever reads pluginIndex.
			char name[100]{};
			int pos = 0;
			// "MASTER " where a track pass writes "TRACK1 " -- the bus is a peer
			// of the four tracks in the host's list, not a fifth one of them.
			pos = snprintf(name, 100, "MASTER ");
			tsl::parameters::Event e;
			e.setup(_STATE, MASTER_ROW, p.num);
			e.toString(_STATE, pos, name, 100, false);

			if (par.type == ParameterType_bool)
			{
				GetParam(index)->InitBool(name, e.getDefaultValue(_STATE) == 1. ? true : false, "", 0, "MASTER", "OFF", "ON");
			}
			else if (par.type == ParameterType_double)
			{
				if (par.flags & Param::CastInt)
				{
					GetParam(index)->InitInt(name, par.toDisplay(sr, par.initvalue), par.toDisplay(sr, par.min), par.toDisplay(sr, par.max), par.valuename, 0, "MASTER");
				}
				else
				{
					// Same mapping as a track's: internal-normalized 0..1 as the
					// value, natural units as the display string. The range
					// lives in Param, which is per id and not per row, so the
					// master copy of a parameter reads and writes exactly like
					// the track copy.
					GetParam(index)->InitDouble(name,
						(par.initvalue - par.min) / (par.max - par.min),  // normalized init
						0.0, 1.0, 0.001, "", 0, "MASTER");

					GetParam(index)->SetDisplayFunc([this, num = p.num, sr](double norm, WDL_String& str) {
						auto& par = _STATE->parameters[num];

						double internal = par.min + (par.max - par.min) * norm;
						double display = par.toDisplay(sr, internal);
						str.SetFormatted(32, "%.*f %s", par.digits, display, par.valuename);
						});

					GetParam(index)->SetStringToValueFunc([this, num = p.num, sr](const char* s, double& v) {
						auto& par = _STATE->parameters[num];

						double internal = par.fromDisplay(sr, atof(s));
						v = (internal - par.min) / (par.max - par.min);
						return true;
						});
				}
			}
			else if (par.type == ParameterType_enum)
			{
				GetParam(index)->InitInt(name, par.toDisplay(_STATE->sr, e.getDefaultValue(_STATE)), 0, std::size(par.names) - 1, "", 0, "MASTER");
				int n = 0;
				for (auto& sv : par.names)
					GetParam(index)->SetDisplayText(n++, sv.data());
			}
		}

	}

#if IPLUG_EDITOR
	mMakeGraphicsFunc = [&]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT)); };

	mLayoutFunc = [&](IGraphics* pGraphics) { pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false); };
#endif

#ifdef GS_TEST_HOOKS
	gsTestRegisterInstance(this, _appState);
#endif
}

void IPlugEffect::OnIdle()
{
	ParamTuple p;
	while (mParamChangesToHost.Pop(p)) {
		paramTimer.reset();
		double norm = p.value;
		// WHICH BLOCK THE INDEX CAME OUT OF. `p.idx % paramCount` is a track
		// answer and only a track answer: a master index is >= masterParamBase,
		// and the modulo would hand back an arbitrary dp[] entry -- a different
		// parameter, with a different type and a different range, so the
		// re-normalisation below would be done with the wrong mapping and the
		// host would be told a value that means nothing.
		auto& dpe = p.idx >= tsl::iplug::masterParamBase
			? tsl::iplug::masterDp[p.idx - tsl::iplug::masterParamBase]
			: tsl::iplug::dp[p.idx % tsl::iplug::paramCount];
		auto& par = _STATE->parameters[dpe.num];
		if (par.type == ParameterType_double && (par.flags & Param::CastInt)) {
			// Int params have a display-unit range; queued value is normalized in
			// internal space, so re-normalize through the display mapping.
			double internal = par.min + (par.max - par.min) * p.value;
			norm = GetParam(p.idx)->ToNormalized(par.toDisplay(_STATE->sr, internal));
		}
		// Updates the IParam (so hosts reading values via the controller stay in
		// sync) and calls InformHostOfParamChange. The resulting OnParamChange
		// fires with kUI, which our OnParamChange ignores.
		SetParameterValue(p.idx, norm);
	}

	if (mParamSyncRequested.exchange(false, std::memory_order_acq_rel)) {
#ifdef VST3_API
		componentHandler->restartComponent(Steinberg::Vst::kParamValuesChanged);
#endif
	}

	// Deliberately after the queue drain: the restore itself queues nothing (its
	// events carry FromDaw), so there is nothing to interleave with, and waiting
	// for isRestoringState to clear means the engine has settled.
	if (mHostResyncPending.load(std::memory_order_acquire) && !_DATA->isRestoringState.load()) {
		mHostResyncPending.store(false, std::memory_order_release);
		SyncHostParamsFromEngine();
	}
}

// Walks every host-visible parameter and pushes the ones whose IParam no longer
// matches the engine. Only the differing ones: mParamChangesToHost holds 4096
// and there are totalParamCount params, so a blanket push would sit well past
// the limit for no benefit.
//
// This deliberately does NOT touch paramTimer. That timer exists to ignore host
// echoes of our own pushes for 200 ms, but a project load is followed almost
// immediately by automation playback -- re-arming it here would drop exactly the
// writes the host makes first.
void IPlugEffect::SyncHostParamsFromEngine()
{
	if (!_DATA->isRunningAsPlugin) return;

	const auto sr = _STATE->sr;

	for (int tindex = 0; tindex < 4; tindex++) {
		for (int i = 0; i < tsl::iplug::paramCount; i++) {
			auto& p = tsl::iplug::dp[i];
			if (p.num == PARAM_NOT_ASSIGNED) continue;

			const int index = i + tindex * tsl::iplug::paramCount;
			auto& par = _STATE->parameters[p.num];

			// Same Event construction as the ctor, including the paramIndex
			// override the power/bypass params need.
			tsl::parameters::Event e;
			e.setup(_STATE, tindex, p.num);
			if (p.num == OFFGRAIN || p.num == OFFFX || p.num == OFFSTEREOFX ||
				p.num == BYPASSFX || p.num == BYPASSGRAINFX || p.num == BYPASSSTEREOFX)
				e.paramIndex = p.offset;

			const double current = e.getCurrentValue(_STATE);

			// Convert to the normalized value SetParameterValue expects, using
			// the mapping each param was declared with in the ctor.
			double norm;
			if (e.eventType == tsl::parameters::Eventtype::Power &&
				(e.subType == tsl::parameters::EventSubtype::powerGrainFx ||
				 e.subType == tsl::parameters::EventSubtype::powerFx ||
				 e.subType == tsl::parameters::EventSubtype::powerStereoFx)) {
				// Packed PowerState; only the on/off bit is exposed to the host.
				norm = std::bit_cast<tsl::parameters::PowerState>(current).pow ? 1.0 : 0.0;
			}
			else if (par.type == ParameterType_bool) {
				norm = current > 0.5 ? 1.0 : 0.0;
			}
			else if (par.type == ParameterType_enum || (par.flags & Param::CastInt)) {
				// Declared with InitInt over display units.
				norm = GetParam(index)->ToNormalized(par.toDisplay(sr, current));
			}
			else if (par.type == ParameterType_double) {
				// Declared with InitDouble over internal-normalized 0..1.
				norm = (par.max > par.min) ? (current - par.min) / (par.max - par.min) : 0.0;
			}
			else continue;

			if (std::isnan(norm)) continue;
			norm = std::clamp(norm, 0.0, 1.0);

			// float32 is what the host round-trips through, so anything closer
			// than that is not a real disagreement.
			if (std::fabs(GetParam(index)->GetNormalized() - norm) < 1e-6) continue;

			// Updates the IParam and informs the host. The resulting
			// OnParamChange arrives with kUI, which OnParamChange ignores.
			SetParameterValue(index, norm);
		}
	}

	// ── AND THE MASTER BLOCK ──
	//
	// The same sweep over the fifth block. It is a separate pass rather than a
	// fifth turn of the loop above for the same reasons the ctor's is: the
	// index is masterParamBase + i and not i + tindex * paramCount, the table
	// is masterDp[] and not dp[], and the block has neither reserved slots nor
	// power pairs, so the two branches that exist for those do not belong here.
	//
	// A restore reaches the master row exactly as it reaches a track row -- the
	// events it applies carry FromDaw and are suppressed on the way out, and
	// setDefaults() rewrites params with no event at all -- so without this the
	// host would read pre-restore values for every master parameter for the
	// rest of the session.
	for (int i = 0; i < tsl::iplug::masterParamCount; i++) {
		auto& p = tsl::iplug::masterDp[i];

		const int index = tsl::iplug::masterParamBase + i;
		auto& par = _STATE->parameters[p.num];

		// MASTER_ROW where the pass above hands tindex. No paramIndex override:
		// no power or bypass entry is in this block.
		tsl::parameters::Event e;
		e.setup(_STATE, MASTER_ROW, p.num);

		const double current = e.getCurrentValue(_STATE);

		// The same three mappings the master pass in the ctor declared these
		// with. No packed-PowerState case: that is what a power pair needs, and
		// this block has none.
		double norm;
		if (par.type == ParameterType_bool) {
			norm = current > 0.5 ? 1.0 : 0.0;
		}
		else if (par.type == ParameterType_enum || (par.flags & Param::CastInt)) {
			norm = GetParam(index)->ToNormalized(par.toDisplay(sr, current));
		}
		else if (par.type == ParameterType_double) {
			norm = (par.max > par.min) ? (current - par.min) / (par.max - par.min) : 0.0;
		}
		else continue;

		if (std::isnan(norm)) continue;
		norm = std::clamp(norm, 0.0, 1.0);

		if (std::fabs(GetParam(index)->GetNormalized() - norm) < 1e-6) continue;

		SetParameterValue(index, norm);
	}
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
	if (source != EParamSource::kHost || paramTimer.elapsed() < 0.2 || _DATA->isRestoringState.load() ||
		!mHostStateEstablished.load(std::memory_order_acquire))
		return;

	// ── WHICH ROW THE HOST IS WRITING TO ──
	//
	// The master test comes FIRST, before the modulo, because the modulo has no
	// answer for a master index. Run on one, `paramIdx % paramCount` picks an
	// arbitrary dp[] entry -- a real parameter of some track, with the wrong id,
	// the wrong type and the wrong range -- and the write would land on it.
	// (The division would give 4, which is MASTER_ROW and would look right; it
	// is right by coincidence of the block being one paramCount long, and it is
	// not what decides anything here.)
	//
	// Everything downstream is then row-agnostic: Event::setup answers
	// MASTER_ROW before its switch, and Param holds the range per id and not
	// per row, so the bool / double / enum arms below are the ones a track uses
	// with nothing added.
	const tsl::iplug::DawParam* dpe;
	int tindex;
	if (paramIdx >= tsl::iplug::masterParamBase) {
		dpe = &tsl::iplug::masterDp[paramIdx - tsl::iplug::masterParamBase];
		tindex = MASTER_ROW;
	}
	else {
		dpe = &tsl::iplug::dp[paramIdx % tsl::iplug::paramCount];
		tindex = static_cast<int>(floor(paramIdx / tsl::iplug::paramCount));
	}

	auto& p = *dpe;
	if (p.num == PARAM_NOT_ASSIGNED) return;
	auto& par = _STATE->parameters[p.num];
	tsl::parameters::Event e;
	e.setup(_STATE, tindex, p.num);
	// paramRow(), and the event has to exist before the counter can be read.
	// paramChanging is "an edit on this row is in flight" and is what drops the
	// host's echo of a gesture the UI is still holding; it is one counter per
	// PARAM ROW, so the master bus has its own. Reading it at e.trackIndex
	// instead would read 0 for a master event -- master events carry trackIndex
	// 0 so that the preset format does not change -- and a master drag would
	// silently swallow the host's writes to track 1's same parameter.
	if (par.paramChanging[e.paramRow()].load(std::memory_order_acquire) > 0)return;
	e.flags |= tsl::parameters::Event::FromDaw;
	if (par.type == ParameterType_bool)
	{
		if (p.flags & tsl::iplug::FlagBypass || p.flags & tsl::iplug::FlagPower)
		{
			switch (p.num)
			{


			case BYPASSGRAINFX:
			case BYPASSSTEREOFX:
			case BYPASSFX: {
				e.paramIndex = p.offset;
				e.value = GetParam(paramIdx)->Value() > 0.5 ? 1.0 : 0.0;
				e.applyFromExt(_STATE, tsl::parameters::None);
			}
						 break;


			case OFFFX:
			case OFFGRAIN:
			case OFFSTEREOFX: {
				e.paramIndex = p.offset;
				e.power.pow = GetParam(paramIdx)->Value() > 0.5 ? 1.0 : 0.0;
				e.applyFromExt(_STATE, tsl::parameters::None);
			}
							break;
			}
		}
		else
		{
			e.value = GetParam(paramIdx)->Value() > 0.5 ? 1.0 : 0.0;
			e.applyFromExt(_STATE, tsl::parameters::None);
		}
	}
	else if (par.type == ParameterType_double)
	{
		if (par.flags & Param::CastInt)
			// Int params carry the display value; map back to internal.
			e.value = par.fromDisplay(_STATE->sr, GetParam(paramIdx)->Value());
		else
			// Value is internal-normalized 0..1.
			e.value = par.min + (par.max - par.min) * GetParam(paramIdx)->Value();
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

	// NO SILENCE GATE for an unpurchased AUv3 on iOS, by decision: the plugin
	// shows the purchase notice instead (gs2 Controller::syncAuGate, a dialog
	// that refuses every close and lifts itself once the upgrade is bought in
	// the app) and otherwise runs exactly as the app does -- Patrick: "No need
	// to restrict ui or feature on auv3". The purchase record it reads is the
	// app group's defaults (tslgraphics2/PlatformPaths/storekit_ios.mm).
	int channelMask = 0;
	if (IsChannelConnected(ERoute::kOutput, 0))
		channelMask |= (1 << OUTPUT_ACTIVE_BIT_L);
	if (IsChannelConnected(ERoute::kOutput, 1))
		channelMask |= (1 << OUTPUT_ACTIVE_BIT_R);

	for (int i = 0; i < MAX_CHANNELS; ++i)
		if (IsChannelConnected(ERoute::kInput, i))
			channelMask |= (1 << i);

	// Must be constructed BEFORE the _isplaying read below, never inside the
	// branch: record_loop clears _isplaying and then waits for this count to hit
	// zero, so a pass that has already passed the check but not yet been counted
	// would be invisible to it and would run alongside record_loop, writing
	// currentBufSize and ringPos out from under it. See DATA::AudioPassGuard in
	// grainstorm.h for the handshake. Purely a bookkeeping guard -- it changes
	// no audio behaviour and neither branch below is touched.
	DATA::AudioPassGuard pass(_DATA);

	// By the first render the host has had its chance to send state (AU order is
	// instantiate -> SetProperty(ClassInfo) -> Initialize -> render), so from
	// here on host automation is genuine and must not be ignored.
	if (!mHostStateEstablished.load(std::memory_order_relaxed))
		mHostStateEstablished.store(true, std::memory_order_release);

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
	mHostStateEstablished.store(true, std::memory_order_release);
	auto current = _DATA->snapShot.get();
	if (!current || !chunk.PutBytes(current->mem, current->size))
		return false;
	return true;
}
int IPlugEffect::UnserializeState(const IByteChunk& chunk, int startPos)
{
	//    return UnserializeParams(chunk, startPos);
	int pos = startPos;
	if (pos < 0)
		return -1;

	auto loadedPresetDataSize = chunk.Size() - pos;

	auto bytes = _STATE->pool.acquire<unsigned char>(tsl::parameters::supposeSnapSize(loadedPresetDataSize));
	if (bytes == nullptr)return -1;
	pos = chunk.GetBytes(bytes, loadedPresetDataSize, pos);
	if (pos < 0) {
		_STATE->pool.release(bytes);
		return -1;
	}

	auto ret = _DATA->snapShot.load(bytes, loadedPresetDataSize, true);
	if (!ret)
		return -1;

	// A project load never calls SerializeState, so this is the point at which
	// the host has established our state.
	mHostStateEstablished.store(true, std::memory_order_release);
	// snapShot.load() is asynchronous -- it queues onto the snapshot worker. The
	// sweep has to wait for that to finish, so OnIdle picks it up once
	// isRestoringState clears.
	mHostResyncPending.store(true, std::memory_order_release);

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
	case 0xB0: // Control Change
	{
		onMidiMsg(_STATE,
			status,
			msg.mData1,
			msg.mData2);

		break;
	}

	case 0xA0: // Poly Aftertouch
	{
		//_DATA->preQueue.push({0, (MYFLOAT)msg.mData2, 0xA0, 0, msg.mData1});
		break;
	}

	case 0xD0: // Channel Pressure
	{
		//_DATA->preQueue.push({0, (MYFLOAT)msg.mData1, 0xD0, 0, 0});
		break;
	}

	default:
		break;
	}
}

#endif