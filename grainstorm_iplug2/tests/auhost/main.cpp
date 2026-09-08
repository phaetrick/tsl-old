// main.cpp — state/preset verification harness for the Grainstorm AUv2.
//
// The question this answers: after a change arrives from any of the four
// sources, do all three views of the state agree?
//
//   engine  what the DSP is running on          (Event::getCurrentValue)
//   snap    what the snapshot would serialize   (Snapshot::events_)
//   host    what the DAW reads back             (IParam)
//
// The four sources:
//   1. host automation      AudioUnitSetParameter -> OnParamChange(kHost)
//   2. plugin UI            Event::apply(FromUi)        [gsTestApplyFromUi]
//   3. snapshot worker      undo / redo                 [gsTestUndo/Redo]
//   4. host state restore   SetProperty(ClassInfo) -> UnserializeState
//
// A render thread runs throughout, because events flagged ToAudioThread (fx
// power on/off, track enable) only ever apply inside ProcessBlock. The main
// thread pumps the CFRunLoop, because iPlug2's idle timer -- which is what
// drains mParamChangesToHost back to the host -- is a CFRunLoopTimer on the
// main run loop.

#include "AUHost.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// reporting
// ---------------------------------------------------------------------------

struct Report
{
	int                      checks = 0;
	int                      failed = 0;
	std::vector<std::string> failures;
	std::string              section;

	void begin(const char* name)
	{
		section = name;
		printf("\n=== %s ===\n", name);
		fflush(stdout);
	}

	void pass(const char* what)
	{
		checks++;
		printf("  ok    %s\n", what);
		fflush(stdout);
	}

	void fail(const std::string& what)
	{
		checks++;
		failed++;
		failures.push_back(section + ": " + what);
		printf("  FAIL  %s\n", what.c_str());
		fflush(stdout);
	}

	void check(bool cond, const std::string& what)
	{
		if (cond) { checks++; printf("  ok    %s\n", what.c_str()); fflush(stdout); }
		else      fail(what);
	}
};

static Report gReport;

static std::string fmt(const char* f, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, f);
	vsnprintf(buf, sizeof(buf), f, ap);
	va_end(ap);
	return buf;
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

// Runs the main run loop so iPlug2's idle timer fires. Without this,
// mParamChangesToHost never drains and the host sees none of the plugin's own
// changes -- which would look like a plugin bug but is a harness bug.
static void pumpIdle(double seconds)
{
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
}

// Full quiescence: worker queues drained, audio thread round-tripped, restore
// flag clear, and the idle pump run so anything queued for the host lands.
static void settle(AUHost& h, double idleSeconds = 0.25)
{
	int r = h.hooks.sync(3000);
	if (r != 0) {
		gReport.fail(fmt("gsTestSync did not reach quiescence (mask 0x%x: %s%s%s%s)", r,
		                 (r & 1) ? "snapshot-worker " : "",
		                 (r & 2) ? "audio-thread " : "",
		                 (r & 4) ? "snapshot-worker-2 " : "",
		                 (r & 8) ? "isRestoringState-stuck " : ""));
	}
	pumpIdle(idleSeconds);
	h.hooks.sync(3000);
	pumpIdle(0.05);
}

using Rows  = std::vector<GsTestParamRow>;
using State = std::vector<Rows>; // [track][row]

static State readAll(AUHost& h)
{
	State s;
	const int tracks = h.hooks.trackCount();
	const int pc     = h.hooks.paramCount();
	for (int t = 0; t < tracks; t++) {
		Rows rows(pc);
		int n = h.hooks.readParams(t, rows.data(), pc);
		rows.resize(n < 0 ? 0 : n);
		s.push_back(std::move(rows));
	}
	return s;
}

static bool nearly(double a, double b, double tol)
{
	if (std::isnan(a) || std::isnan(b)) return false;
	const double scale = std::max({1.0, std::fabs(a), std::fabs(b)});
	return std::fabs(a - b) <= tol * scale;
}

// One-shot triggers hold no state, and NoValue params carry no value at all --
// neither belongs in a state-coherence comparison.
static bool isStateBearing(const GsTestParamRow& r)
{
	constexpr uint32_t kFlagOneShot = 1u << 0;
	constexpr uint32_t kNoValue     = 1u << 6;
	if (r.dpFlags & kFlagOneShot)    return false;
	if (r.paramFlags & kNoValue)     return false;
	return true;
}

static double expectedEngine(const GsTestParamRow& r)
{
	return r.hasSnap ? r.snap : r.def;
}

// Parameters that are ALREADY incoherent at load, before the harness touches
// anything. They are recorded once during the baseline scenario and then
// reported separately, so a pre-existing defect does not bury every later
// result under the same few hundred lines.
static std::set<std::pair<int, int>> gKnownBadSnap; // (track, pluginIdx)
static std::set<std::pair<int, int>> gKnownBadHost;

// The three-way invariant. maxReport caps the noise when something is broadly
// wrong; the count is always reported in full. Pass record=true exactly once,
// at baseline, to seed the known-bad sets.
static int checkCoherence(AUHost& h, const char* phase, int maxReport = 6, bool record = false)
{
	const State s = readAll(h);

	int snapMismatch = 0, hostMismatch = 0, reported = 0;
	int knownSnap = 0, knownHost = 0;

	for (size_t t = 0; t < s.size(); t++) {
		for (const auto& r : s[t]) {
			if (!isStateBearing(r)) continue;
			const auto key = std::make_pair((int)t, r.pluginIdx);

			// --- snapshot vs engine ---------------------------------------
			bool snapOk;
			if (r.isPower) {
				const int expPow = r.hasSnap ? r.snapPow : r.defPow;
				snapOk = (expPow == r.enginePow);
			} else {
				snapOk = nearly(expectedEngine(r), r.engine, 1e-9);
			}
			if (!snapOk && record) gKnownBadSnap.insert(key);
			// In record mode the baseline still reports them — they are findings.
			if (!snapOk && !record && gKnownBadSnap.count(key)) { knownSnap++; snapOk = true; }

			if (!snapOk) {
				snapMismatch++;
				if (reported < maxReport) {
					reported++;
					std::string m = r.isPower
						? fmt("t%zu p%d(num %d): snapshot says pow=%d, engine has pow=%d (hasSnap=%d)",
						      t, r.pluginIdx, r.num, r.hasSnap ? r.snapPow : r.defPow, r.enginePow, r.hasSnap)
						: fmt("t%zu p%d(num %d): snapshot implies %.17g, engine has %.17g (hasSnap=%d, def=%.17g)",
						      t, r.pluginIdx, r.num, expectedEngine(r), r.engine, r.hasSnap, r.def);
					gReport.fail(std::string(phase) + " snapshot!=engine — " + m);
				}
			}

			// --- host vs engine -------------------------------------------
			double expHost;
			double tol = 1e-6;
			if (r.isPower) {
				expHost = (double)r.enginePow;
			} else if (r.type == 0) {           // ParameterType_bool
				expHost = r.engine > 0.5 ? 1.0 : 0.0;
			} else if (r.type == 2 || r.castInt) { // enum / int: host is in display units
				expHost = r.engineDisplay;
				tol = 1e-5;
			} else {                             // double: host is internal-normalized
				expHost = r.engineNormInt;
				tol = 2e-6;
			}

			bool hostOk = nearly(expHost, r.host, tol);
			if (!hostOk && record) gKnownBadHost.insert(key);
			if (!hostOk && !record && gKnownBadHost.count(key)) { knownHost++; hostOk = true; }

			if (!hostOk) {
				hostMismatch++;
				if (reported < maxReport) {
					reported++;
					gReport.fail(fmt("%s host!=engine — t%zu p%d(num %d, type %d, castInt %d): host has %.17g, engine implies %.17g",
					                 phase, t, r.pluginIdx, r.num, r.type, r.castInt, r.host, expHost));
				}
			}
		}
	}

	const int total = snapMismatch + hostMismatch;
	const std::string known = (knownSnap || knownHost)
		? fmt(" [%d snapshot + %d host pre-existing at load, suppressed]", knownSnap, knownHost)
		: std::string();

	if (record)
		printf("  ....  baseline recorded %zu snapshot and %zu host mismatches as pre-existing; "
		       "later scenarios report only new ones\n", gKnownBadSnap.size(), gKnownBadHost.size());

	if (total == 0)
		gReport.pass(fmt("%s: snapshot == engine == host across all tracks%s", phase, known.c_str()).c_str());
	else
		printf("  ....  %s: %d NEW snapshot mismatches, %d NEW host mismatches (showed %d)%s\n",
		       phase, snapMismatch, hostMismatch, reported, known.c_str());
	return total;
}

// Compare two full states for equality of the engine column — the check that
// matters after a save/restore round trip.
static int diffEngine(const State& a, const State& b, const char* what, int maxReport = 8)
{
	int diffs = 0, reported = 0;
	for (size_t t = 0; t < a.size() && t < b.size(); t++) {
		for (size_t i = 0; i < a[t].size() && i < b[t].size(); i++) {
			const auto& x = a[t][i];
			const auto& y = b[t][i];
			if (!isStateBearing(x)) continue;

			bool same = x.isPower ? (x.enginePow == y.enginePow)
			                      : nearly(x.engine, y.engine, 1e-9);
			if (!same) {
				diffs++;
				if (reported < maxReport) {
					reported++;
					gReport.fail(x.isPower
						? fmt("%s: t%zu p%d(num %d) pow %d -> %d", what, t, x.pluginIdx, x.num, x.enginePow, y.enginePow)
						: fmt("%s: t%zu p%d(num %d) %.17g -> %.17g", what, t, x.pluginIdx, x.num, x.engine, y.engine));
				}
			}
		}
	}
	if (diffs == 0) gReport.pass(fmt("%s: engine state identical", what).c_str());
	else            printf("  ....  %s: %d differing params (showed %d)\n", what, diffs, reported);
	return diffs;
}

// Pick plugin param slots that are plain, automatable, state-bearing doubles --
// the ones a DAW would actually automate.
static std::vector<int> pickAutomatableDoubles(const Rows& rows, int count, unsigned seed)
{
	std::vector<int> cand;
	for (const auto& r : rows)
		if (isStateBearing(r) && r.type == 1 && !r.castInt && !r.isPower && r.maxVal > r.minVal)
			cand.push_back(r.pluginIdx);

	std::mt19937 rng(seed);
	std::shuffle(cand.begin(), cand.end(), rng);
	if ((int)cand.size() > count) cand.resize(count);
	return cand;
}

// Two state chunks that differ byte-for-byte are not necessarily different
// states: the snapshot format is a name header followed by a flat array of
// 16-byte Event records, and nothing pins their order. Report which it is, so a
// pure reordering is not mistaken for lost state.
static std::string describeChunkDiff(const std::string& a, const std::string& b)
{
	if (a.size() != b.size()) return fmt("sizes differ (%zu vs %zu)", a.size(), b.size());
	if (a.size() < sizeof(int)) return "chunk too small to parse";

	int hdrA = 0, hdrB = 0;
	std::memcpy(&hdrA, a.data(), sizeof(int));
	std::memcpy(&hdrB, b.data(), sizeof(int));
	if (hdrA != hdrB) return fmt("name-header sizes differ (%d vs %d)", hdrA, hdrB);
	if (hdrA < 0 || (size_t)hdrA > a.size()) return "name header out of bounds";

	if (std::memcmp(a.data(), b.data(), hdrA) != 0) return "track names differ";

	constexpr size_t kEventSize = 16; // sizeof(tsl::parameters::Event)
	const size_t evBytes = a.size() - hdrA;
	if (evBytes % kEventSize) return "event section is not a whole number of events";

	std::vector<std::string> ea, eb;
	for (size_t o = hdrA; o + kEventSize <= a.size(); o += kEventSize) {
		ea.emplace_back(a.data() + o, kEventSize);
		eb.emplace_back(b.data() + o, kEventSize);
	}

	size_t inPlace = 0;
	for (size_t i = 0; i < ea.size(); i++) if (ea[i] == eb[i]) inPlace++;

	std::vector<std::string> sa = ea, sb = eb;
	std::sort(sa.begin(), sa.end());
	std::sort(sb.begin(), sb.end());

	if (sa == sb)
		return fmt("same %zu events in a different order (%zu of them in the same slot) — "
		           "state is equivalent, only the serialization order changed",
		           ea.size(), inPlace);

	// Not a permutation. Match records on identity (paramIndex, eventType,
	// subType, trackIndex — bytes 0..4) and report where the rest diverges:
	// a differing flags/groupId byte means bookkeeping, a differing payload
	// means the stored value itself changed across the round trip.
	auto identity = [](const std::string& e) { return e.substr(0, 5); };
	auto payload  = [](const std::string& e) { double d; std::memcpy(&d, e.data() + 8, 8); return d; };

	size_t unmatched = 0, flagsOnly = 0, valueDiff = 0;
	double maxDelta = 0.0;
	std::vector<bool> used(eb.size(), false);

	for (const auto& ra : ea) {
		size_t match = eb.size();
		for (size_t j = 0; j < eb.size(); j++)
			if (!used[j] && identity(eb[j]) == identity(ra)) { match = j; break; }

		if (match == eb.size()) { unmatched++; continue; }
		used[match] = true;

		const double va = payload(ra), vb = payload(eb[match]);
		if (va != vb) {
			valueDiff++;
			const double scale = std::max({1.0, std::fabs(va), std::fabs(vb)});
			maxDelta = std::max(maxDelta, std::fabs(va - vb) / scale);
		} else if (ra != eb[match]) {
			flagsOnly++;
		}
	}

	return fmt("%zu events: %zu with no counterpart, %zu differing only in flags/groupId, "
	           "%zu with a changed value (max relative delta %.3g)",
	           ea.size(), unmatched, flagsOnly, valueDiff, maxDelta);
}

static const GsTestParamRow* findRow(const Rows& rows, int pluginIdx)
{
	for (const auto& r : rows)
		if (r.pluginIdx == pluginIdx) return &r;
	return nullptr;
}

// ---------------------------------------------------------------------------
// scenarios
// ---------------------------------------------------------------------------

static void scenarioBaseline(AUHost& h)
{
	gReport.begin("1. Load / baseline coherence");

	gReport.check(h.hooks.ready() == 1, "test hooks bound to a live plugin instance");
	gReport.check(h.hooks.isPlaying() == 1,
	              "player reports playing (PLUGIN_MODE default) — audio-thread events can apply");

	const int pc     = h.hooks.paramCount();
	const int tracks = h.hooks.trackCount();
	gReport.check(h.paramCount() == pc * tracks,
	              fmt("host sees %d params = %d per track x %d tracks", h.paramCount(), pc, tracks));

	const auto before = h.renderCount();
	pumpIdle(0.3);
	gReport.check(h.renderCount() > before,
	              fmt("render thread is running (%llu blocks)", h.renderCount()));

	settle(h);
	checkCoherence(h, "baseline", 6, /*record=*/true);

	extern std::string gDumpDir;
	if (!gDumpDir.empty()) {
		const std::string p = gDumpDir + "/baseline-state.json";
		h.hooks.dumpState(p.c_str());
		printf("  ....  baseline dump: %s\n", p.c_str());
	}
}
std::string gDumpDir;

static void scenarioHostAutomationBeforeFirstSave(AUHost& h)
{
	gReport.begin("2. Host automation before the first state save");

	// OnParamChange returns early while !mFirstSerializeDone. A DAW that pushes
	// automation right after instantiation -- before ever asking for state --
	// therefore has those writes dropped by the engine while the host-side
	// IParam does change. That is a real divergence, so it gets its own check.
	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[0], 3, 11);
	if (picks.empty()) { gReport.fail("no automatable double params found"); return; }

	std::vector<double> engineBefore;
	for (int idx : picks) engineBefore.push_back(findRow(st[0], idx)->engine);

	for (int idx : picks) h.setParameter(idx, 0.77f);
	settle(h);

	st = readAll(h);
	int applied = 0;
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* r = findRow(st[0], picks[i]);
		if (!nearly(r->engine, engineBefore[i], 1e-9)) applied++;
	}

	if (applied == (int)picks.size())
		gReport.pass("automation before the first save reached the engine");
	else
		gReport.fail(fmt("automation before the first save reached the engine for only %d of %zu params "
		                 "— OnParamChange is gated on mFirstSerializeDone, so pre-save host writes are dropped "
		                 "while the host-side IParam still moves",
		                 applied, picks.size()));

	// Whatever the outcome, the three views must still agree with each other.
	checkCoherence(h, "after pre-save automation");

	// Arm mFirstSerializeDone for every scenario that follows, so they test the
	// steady state a DAW is normally in rather than re-testing this gate.
	if (CFPropertyListRef p = h.getClassInfo()) CFRelease(p);
	settle(h);
	printf("  ....  ClassInfo read once — mFirstSerializeDone is now set for the remaining scenarios\n");

	// The host-side IParams that the dropped writes moved are now out of step
	// with the engine. Put them back so later scenarios start from a coherent
	// state rather than inheriting this scenario's damage.
	st = readAll(h);
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* r = findRow(st[0], picks[i]);
		h.setParameter(r->hostIdx, (float)r->engineNormInt);
	}
	settle(h, 0.4);
}

static void scenarioHostAutomation(AUHost& h)
{
	gReport.begin("3. Host automation (DAW writes a parameter)");

	// OnParamChange also drops anything arriving within 200 ms of the plugin's
	// own last push to the host (paramTimer). Give it room so this scenario
	// tests automation, not the guard.
	pumpIdle(0.4);

	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[1], 6, 23);
	if (picks.empty()) { gReport.fail("no automatable double params found"); return; }

	const int base = 1 * h.hooks.paramCount(); // track 1's block
	std::mt19937 rng(4242);
	std::uniform_real_distribution<double> dist(0.15, 0.85);

	std::vector<double> targets;
	for (int idx : picks) {
		const double v = dist(rng);
		targets.push_back(v);
		h.setParameter(base + idx, (float)v);
	}
	settle(h);

	st = readAll(h);
	int ok = 0;
	std::string firstBad;
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* r = findRow(st[1], picks[i]);
		const double expected = r->minVal + (r->maxVal - r->minVal) * targets[i];
		// float32 at the AU boundary, so compare with float precision.
		if (nearly(r->engine, expected, 1e-6)) ok++;
		else if (firstBad.empty())
			firstBad = fmt("p%d(num %d): wrote %.6f -> expected engine %.17g, got %.17g",
			               r->pluginIdx, r->num, targets[i], expected, r->engine);
	}
	gReport.check(ok == (int)picks.size(),
	              fmt("all %zu automated params reached the engine%s", picks.size(),
	                  firstBad.empty() ? "" : (" — " + firstBad).c_str()));

	int inSnap = 0;
	for (int idx : picks) if (findRow(st[1], idx)->hasSnap) inSnap++;
	gReport.check(inSnap == (int)picks.size(),
	              fmt("all %zu automated params were recorded in the snapshot (%d)", picks.size(), inSnap));

	checkCoherence(h, "after host automation");
}

static void scenarioUiChange(AUHost& h)
{
	gReport.begin("4. UI-originated change (plugin's own widgets)");

	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[2], 5, 77);
	if (picks.empty()) { gReport.fail("no automatable double params found"); return; }

	const int base = 2 * h.hooks.paramCount();

	std::vector<float> hostBefore;
	for (int idx : picks) {
		AudioUnitParameterValue v = 0;
		h.getParameter(base + idx, &v);
		hostBefore.push_back(v);
	}

	for (int idx : picks) h.hooks.applyFromUi(2, idx, 0.31);
	settle(h, 0.4);

	st = readAll(h);

	int engineOk = 0, hostOk = 0, snapOk = 0;
	std::string firstBad;
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* r = findRow(st[2], picks[i]);
		const double expected = r->minVal + (r->maxVal - r->minVal) * 0.31;
		if (nearly(r->engine, expected, 1e-9)) engineOk++;
		if (r->hasSnap) snapOk++;

		// The host must learn about it: OnIdle drains mParamChangesToHost and
		// calls SetParameterValue, so AudioUnitGetParameter should now agree.
		AudioUnitParameterValue v = 0;
		h.getParameter(base + picks[i], &v);
		if (nearly(v, r->engineNormInt, 1e-5)) hostOk++;
		else if (firstBad.empty())
			firstBad = fmt("p%d(num %d): host reads %.9g, engine implies %.9g (was %.9g)",
			               r->pluginIdx, r->num, (double)v, r->engineNormInt, (double)hostBefore[i]);
	}

	gReport.check(engineOk == (int)picks.size(),
	              fmt("all %zu UI changes reached the engine (%d)", picks.size(), engineOk));
	gReport.check(snapOk == (int)picks.size(),
	              fmt("all %zu UI changes were recorded in the snapshot (%d)", picks.size(), snapOk));
	gReport.check(hostOk == (int)picks.size(),
	              fmt("all %zu UI changes were pushed to the host (%d)%s", picks.size(), hostOk,
	                  firstBad.empty() ? "" : (" — " + firstBad).c_str()));

	checkCoherence(h, "after UI change");
}

static void scenarioUndoRedo(AUHost& h)
{
	gReport.begin("5. Undo / redo from the snapshot worker");

	const int track = 3;
	const int base  = track * h.hooks.paramCount();

	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[track], 1, 99);
	if (picks.empty()) { gReport.fail("no automatable double params found"); return; }
	const int idx = picks[0];

	const double engineBefore = findRow(st[track], idx)->engine;

	h.hooks.applyFromUi(track, idx, 0.62);
	settle(h, 0.4);

	auto afterChange = readAll(h);
	const double engineAfter = findRow(afterChange[track], idx)->engine;
	gReport.check(!nearly(engineBefore, engineAfter, 1e-9),
	              fmt("UI change moved the value (%.9g -> %.9g)", engineBefore, engineAfter));
	gReport.check(h.hooks.hasUndo(track) == 1, "undo is available after the change");

	h.hooks.undo(track);
	settle(h, 0.4);

	auto afterUndo = readAll(h);
	const auto* ru = findRow(afterUndo[track], idx);
	gReport.check(nearly(ru->engine, engineBefore, 1e-9),
	              fmt("undo restored the engine value (%.9g, expected %.9g)", ru->engine, engineBefore));

	AudioUnitParameterValue hv = 0;
	h.getParameter(base + idx, &hv);
	gReport.check(nearly((double)hv, ru->engineNormInt, 1e-5),
	              fmt("undo was pushed to the host (host %.9g, engine implies %.9g)", (double)hv, ru->engineNormInt));
	checkCoherence(h, "after undo");

	gReport.check(h.hooks.hasRedo(track) == 1, "redo is available after the undo");
	h.hooks.redo(track);
	settle(h, 0.4);

	auto afterRedo = readAll(h);
	const auto* rr = findRow(afterRedo[track], idx);
	gReport.check(nearly(rr->engine, engineAfter, 1e-9),
	              fmt("redo re-applied the engine value (%.9g, expected %.9g)", rr->engine, engineAfter));

	h.getParameter(base + idx, &hv);
	gReport.check(nearly((double)hv, rr->engineNormInt, 1e-5),
	              fmt("redo was pushed to the host (host %.9g, engine implies %.9g)", (double)hv, rr->engineNormInt));
	checkCoherence(h, "after redo");
}

static void scenarioAudioThreadPower(AUHost& h)
{
	gReport.begin("6. Effect power (audio-thread-only work)");

	// Power events carry ToWorkerThread and hand off to the audio thread; they
	// are the case that cannot complete unless ProcessBlock is running.
	auto st = readAll(h);
	std::vector<int> powers;
	for (const auto& r : st[0])
		if (r.isPower && isStateBearing(r)) powers.push_back(r.pluginIdx);

	if (powers.empty()) { gReport.fail("no fx power params exposed"); return; }
	printf("  ....  %zu fx power params exposed per track\n", powers.size());

	const int idx  = powers[0];
	const int base = 0;

	const auto* r0 = findRow(st[0], idx);
	const int   startPow = r0->enginePow;
	const int   target   = startPow ? 0 : 1;

	// Drive it from the UI first.
	h.hooks.applyFromUi(0, idx, target ? 1.0 : 0.0);
	settle(h, 0.5);

	st = readAll(h);
	const auto* r1 = findRow(st[0], idx);
	gReport.check(r1->enginePow == target,
	              fmt("UI power toggle applied on the audio thread (pow %d -> %d, wanted %d)",
	                  startPow, r1->enginePow, target));

	AudioUnitParameterValue hv = 0;
	h.getParameter(base + idx, &hv);
	gReport.check(nearly((double)hv, (double)r1->enginePow, 1e-5),
	              fmt("power state reached the host (host %.3g, engine pow %d)", (double)hv, r1->enginePow));

	gReport.check(r1->hasSnap ? (r1->snapPow == r1->enginePow) : (r1->defPow == r1->enginePow),
	              fmt("snapshot agrees with the engine power (hasSnap=%d snapPow=%d defPow=%d enginePow=%d)",
	                  r1->hasSnap, r1->snapPow, r1->defPow, r1->enginePow));

	// Now the same toggle from the host side.
	pumpIdle(0.4);
	const int target2 = target ? 0 : 1;
	h.setParameter(base + idx, (float)target2);
	settle(h, 0.5);

	st = readAll(h);
	const auto* r2 = findRow(st[0], idx);
	gReport.check(r2->enginePow == target2,
	              fmt("host power toggle applied on the audio thread (pow %d -> %d, wanted %d)",
	                  r1->enginePow, r2->enginePow, target2));

	checkCoherence(h, "after power toggles");
}

static void scenarioSaveRestoreSameInstance(AUHost& h)
{
	gReport.begin("7. Save / restore on the same instance");

	CFPropertyListRef saved = h.getClassInfo();
	gReport.check(saved != nullptr, "host got ClassInfo (SerializeState)");
	if (!saved) return;

	std::string savedChunk;
	gReport.check(AUHost::chunkFromClassInfo(saved, savedChunk) && !savedChunk.empty(),
	              fmt("ClassInfo carries a state chunk (%zu bytes)", savedChunk.size()));

	// The chunk the host received must be exactly the snapshot the plugin
	// published; SerializeState is a straight PutBytes of it.
	{
		int blobSize = h.hooks.readBlob(nullptr, 0);
		std::vector<unsigned char> blob(blobSize > 0 ? blobSize : 0);
		if (blobSize > 0) h.hooks.readBlob(blob.data(), blobSize);
		gReport.check(blobSize > 0 && savedChunk.size() == (size_t)blobSize &&
		                  std::memcmp(savedChunk.data(), blob.data(), blobSize) == 0,
		              fmt("saved chunk is byte-identical to the published snapshot (%zu vs %d bytes)",
		                  savedChunk.size(), blobSize));
	}

	const State stateAtSave = readAll(h);

	// Perturb every track through a different source, so the restore has real
	// work to undo.
	pumpIdle(0.4);
	auto now = readAll(h);
	for (int t = 0; t < h.hooks.trackCount(); t++) {
		auto picks = pickAutomatableDoubles(now[t], 4, 500u + t);
		for (size_t i = 0; i < picks.size(); i++) {
			if (i % 2 == 0) h.hooks.applyFromUi(t, picks[i], 0.11 + 0.07 * i);
			else            h.setParameter(t * h.hooks.paramCount() + picks[i], (float)(0.9 - 0.05 * i));
		}
	}
	settle(h, 0.5);

	const State perturbed = readAll(h);
	{
		int moved = 0;
		for (size_t t = 0; t < perturbed.size(); t++)
			for (size_t i = 0; i < perturbed[t].size(); i++)
				if (isStateBearing(perturbed[t][i]) &&
				    !nearly(perturbed[t][i].engine, stateAtSave[t][i].engine, 1e-9)) moved++;
		gReport.check(moved > 0, fmt("perturbation actually changed %d params", moved));
	}

	gReport.check(h.setClassInfo(saved), "host set ClassInfo (UnserializeState)");
	settle(h, 0.8);

	gReport.check(h.hooks.isRestoringState() == 0,
	              "isRestoringState cleared after the restore (it gates automation and freezes saves)");

	const State restored = readAll(h);
	diffEngine(stateAtSave, restored, "restore vs state at save");
	checkCoherence(h, "after restore");

	// Saving again must produce the same bytes, or the state is not a fixpoint.
	CFPropertyListRef resaved = h.getClassInfo();
	std::string resavedChunk;
	if (resaved && AUHost::chunkFromClassInfo(resaved, resavedChunk)) {
		gReport.check(resavedChunk.size() == savedChunk.size(),
		              fmt("re-saved chunk has the same size (%zu vs %zu)", resavedChunk.size(), savedChunk.size()));
		if (resavedChunk.size() == savedChunk.size())
			gReport.check(std::memcmp(resavedChunk.data(), savedChunk.data(), savedChunk.size()) == 0,
			              "save -> restore -> save is byte-identical" +
			                  (resavedChunk == savedChunk
			                       ? std::string()
			                       : " — " + describeChunkDiff(savedChunk, resavedChunk)));
	} else {
		gReport.fail("could not re-read ClassInfo after the restore");
	}
	if (resaved) CFRelease(resaved);
	CFRelease(saved);
}

static void scenarioRestoreIntoFreshInstance(AUHost& h, const std::string& bundlePath)
{
	gReport.begin("8. Restore into a freshly loaded instance (project reload)");

	CFPropertyListRef saved = h.getClassInfo();
	if (!saved) { gReport.fail("could not read ClassInfo from the source instance"); return; }
	const State sourceState = readAll(h);

	AUHost fresh;
	std::string err;
	if (!fresh.open(bundlePath, err)) { gReport.fail("second instance: " + err); CFRelease(saved); return; }
	if (!fresh.initialize(48000.0, 512, err)) { gReport.fail("second instance: " + err); CFRelease(saved); return; }
	fresh.startRenderThread();
	pumpIdle(0.3);
	settle(fresh);

	// A DAW restoring a project sets state on an instance that has never been
	// asked to save, so mFirstSerializeDone is false here. That must not matter
	// for UnserializeState.
	gReport.check(fresh.setClassInfo(saved), "fresh instance accepted ClassInfo");
	settle(fresh, 1.0);

	gReport.check(fresh.hooks.isRestoringState() == 0, "fresh instance cleared isRestoringState");

	const State freshState = readAll(fresh);
	diffEngine(sourceState, freshState, "fresh instance vs source instance");
	checkCoherence(fresh, "fresh instance after restore");

	// And the fresh instance must be able to save the same thing back.
	CFPropertyListRef freshSaved = fresh.getClassInfo();
	std::string a, b;
	if (freshSaved && AUHost::chunkFromClassInfo(saved, a) && AUHost::chunkFromClassInfo(freshSaved, b)) {
		gReport.check(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0,
		              fmt("fresh instance re-saves identical bytes (%zu vs %zu)", a.size(), b.size()));
	} else {
		gReport.fail("could not compare saved chunks between instances");
	}

	if (freshSaved) CFRelease(freshSaved);
	CFRelease(saved);

	fresh.stopRenderThread();
	fresh.close();
}

static void scenarioMidi(AUHost& h)
{
	gReport.begin("9. MIDI while rendering");

	const auto before = readAll(h);

	// Notes and a CC sweep, delivered the way a host delivers them.
	for (int i = 0; i < 16; i++) {
		h.midiEvent(0x90, 48 + (i % 12), 100, 0);
		pumpIdle(0.02);
		h.midiEvent(0x80, 48 + (i % 12), 0, 0);
		h.midiEvent(0xB0, 7, (UInt32)(i * 8), 0);
	}
	settle(h, 0.4);

	gReport.check(h.hooks.ready() == 1, "plugin survived the MIDI burst");
	checkCoherence(h, "after MIDI");

	// Unmapped CCs must not silently rewrite state.
	const auto after = readAll(h);
	int changed = 0;
	for (size_t t = 0; t < before.size(); t++)
		for (size_t i = 0; i < before[t].size(); i++)
			if (isStateBearing(before[t][i]) && !nearly(before[t][i].engine, after[t][i].engine, 1e-9))
				changed++;
	printf("  ....  %d params changed as a result of MIDI (0 expected with no MIDI mapping)\n", changed);
}

// Audio decoding runs on WorkerQueue and a large file holds that slot for a
// while, so audio work gets a longer leash than parameter work.
static void settleAudio(AUHost& h, double idleSeconds = 1.0)
{
	int r = h.hooks.sync(30000);
	if (r != 0)
		gReport.fail(fmt("gsTestSync did not reach quiescence after audio work (mask 0x%x)", r));
	pumpIdle(idleSeconds);
	h.hooks.sync(30000);
	pumpIdle(0.1);
}

static std::vector<std::string> gAudioFiles;

static void scenarioLoadAudio(AUHost& h)
{
	gReport.begin("11. Load audio files into tracks");

	if (gAudioFiles.empty()) {
		gReport.fail("no audio files supplied (--audio-dir); audio coverage skipped");
		return;
	}

	int loaded = 0;
	for (size_t i = 0; i < gAudioFiles.size() && i < 4; i++) {
		const int t = (int)i;
		const auto& f = gAudioFiles[i];

		const int rc = h.hooks.loadAudio(t, f.c_str());
		settleAudio(h, 1.5);

		GsTestAudioInfo info{};
		const int got = h.hooks.audioInfo(t, &info);

		if (got == 1 && info.off > 0) {
			loaded++;
			gReport.pass(fmt("track %d loaded '%s' (%.0f frames, %.1f s)",
			                 t, info.fileName, info.off, info.off / 48000.0).c_str());
		} else {
			gReport.fail(fmt("track %d failed to load '%s' (loadAudio rc=%d, hasAudio=%d)",
			                 t, f.c_str(), rc, got));
		}
	}

	gReport.check(loaded > 0, fmt("%d track(s) hold audio", loaded));
	checkCoherence(h, "after loading audio");
}

// The Recording a track holds is referenced from the snapshot by file name and
// length, so a state round trip has to bring the same audio back.
static void scenarioAudioSurvivesStateRoundTrip(AUHost& h)
{
	gReport.begin("12. Audio survives a host state round trip");

	std::vector<GsTestAudioInfo> before(4);
	int withAudio = 0;
	for (int t = 0; t < 4; t++)
		if (h.hooks.audioInfo(t, &before[t]) == 1) withAudio++;

	if (!withAudio) { gReport.fail("no track holds audio; nothing to round trip"); return; }

	CFPropertyListRef saved = h.getClassInfo();
	if (!saved) { gReport.fail("could not read ClassInfo"); return; }

	// Perturb, then restore, and check the audio came back with it.
	for (int t = 0; t < 4; t++) {
		auto st = readAll(h);
		auto picks = pickAutomatableDoubles(st[t], 3, 900u + t);
		for (size_t i = 0; i < picks.size(); i++) h.hooks.applyFromUi(t, picks[i], 0.23 + 0.1 * i);
	}
	settleAudio(h, 0.6);

	gReport.check(h.setClassInfo(saved), "host restored the state");
	settleAudio(h, 2.0);

	int same = 0;
	for (int t = 0; t < 4; t++) {
		GsTestAudioInfo now{};
		h.hooks.audioInfo(t, &now);
		if (before[t].hasAudio != now.hasAudio) {
			gReport.fail(fmt("track %d: hasAudio %d -> %d across the restore",
			                 t, before[t].hasAudio, now.hasAudio));
			continue;
		}
		if (!before[t].hasAudio) { same++; continue; }

		if (std::string(before[t].fileName) != now.fileName)
			gReport.fail(fmt("track %d: file '%s' -> '%s'", t, before[t].fileName, now.fileName));
		else if (!nearly(before[t].off, now.off, 1e-9))
			gReport.fail(fmt("track %d: length %.0f -> %.0f frames", t, before[t].off, now.off));
		// offset is deliberately not compared: it is the live read position, and
		// a powered track advances it every block, so it is a moving target
		// rather than a value a restore can be held to. offStart/offStop are
		// static loop bounds and must survive exactly.
		else if (!nearly(before[t].offStart, now.offStart, 1e-6) ||
		         !nearly(before[t].offStop, now.offStop, 1e-6))
			gReport.fail(fmt("track %d: loop bounds moved (start %.1f->%.1f stop %.1f->%.1f)",
			                 t, before[t].offStart, now.offStart, before[t].offStop, now.offStop));
		else same++;
	}
	gReport.check(same == 4, fmt("all 4 tracks kept their audio across the restore (%d)", same));
	checkCoherence(h, "after audio state round trip");

	CFRelease(saved);
}

// The app's own preset system, which is a different path from the host's
// ClassInfo: it goes through Snapshot::apply(fromDaw=false), which also builds
// a preset undo/redo entry.
static void scenarioAppPresets(AUHost& h)
{
	gReport.begin("13. In-app preset save / load");

	const int before = h.hooks.presetCount(0);
	gReport.check(before >= 0, fmt("enumerated existing presets (%d)", before));

	// Embed the audio in the preset, the way Android does by default. That takes
	// the other branch of the writer: FLAC data is appended after the event
	// block and the header is then rewritten by seeking backwards.
	h.hooks.setSaveAudioWithPreset(1);
	h.hooks.setActiveTrack(0);
	settle(h, 0.3);

	// Make track 0 distinctive so a reload is detectable: plain params AND a few
	// effect power states, because "the preset loads but the track stays at init"
	// shows up in the power states as much as in the values.
	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[0], 5, 1234);
	if (picks.empty()) { gReport.fail("no automatable params to mark the preset with"); return; }
	for (size_t i = 0; i < picks.size(); i++) h.hooks.applyFromUi(0, picks[i], 0.42 + 0.05 * i);

	std::vector<int> powerPicks;
	for (const auto& r : st[0])
		if (r.isPower && isStateBearing(r) && powerPicks.size() < 3) powerPicks.push_back(r.pluginIdx);
	for (int idx : powerPicks) h.hooks.applyFromUi(0, idx, 1.0); // switch the effect on
	settle(h, 0.6);

	auto marked = readAll(h);
	std::vector<double> markedVals;
	for (int idx : picks) markedVals.push_back(findRow(marked[0], idx)->engine);

	const std::string name = "auhost-test-" + std::to_string((long)time(nullptr));
	gReport.check(h.hooks.presetSave(name.c_str(), 0) == 0, "saved a preset through the app's serializer");
	settleAudio(h, 1.0);

	const int after = h.hooks.presetCount(0);
	gReport.check(after == before + 1, fmt("preset count %d -> %d", before, after));

	// Move the params away, then load the preset back.
	for (size_t i = 0; i < picks.size(); i++) h.hooks.applyFromUi(0, picks[i], 0.88 - 0.04 * i);
	settle(h, 0.5);

	int index = -1;
	for (int i = 0; i < after; i++) {
		char buf[256]{};
		if (h.hooks.presetName(i, 0, buf, sizeof(buf)) == 0 && name == buf) { index = i; break; }
	}
	gReport.check(index >= 0, fmt("found the saved preset by name ('%s')", name.c_str()));
	if (index < 0) return;

	gReport.check(h.hooks.presetLoad(index, 0) == 0, "loaded the preset back");
	settleAudio(h, 1.5);

	auto restored = readAll(h);
	int ok = 0;
	std::string firstBad;
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* r = findRow(restored[0], picks[i]);
		if (nearly(r->engine, markedVals[i], 1e-6)) ok++;
		else if (firstBad.empty())
			firstBad = fmt("p%d(num %d): saved %.17g, came back %.17g", r->pluginIdx, r->num, markedVals[i], r->engine);
	}
	gReport.check(ok == (int)picks.size(),
	              fmt("preset restored all %zu marked params (%d)%s", picks.size(), ok,
	                  firstBad.empty() ? "" : (" — " + firstBad).c_str()));

	// The symptom to catch is "the preset loads but the track is still at init",
	// which a handful of marked params can easily miss. Compare the whole track.
	int diff = 0, stillDefault = 0, powerDiff = 0, reported = 0;
	for (size_t i = 0; i < marked[0].size() && i < restored[0].size(); i++) {
		const auto& m = marked[0][i];
		const auto& r = restored[0][i];
		if (!isStateBearing(m)) continue;

		if (m.isPower) {
			if (m.enginePow != r.enginePow) {
				powerDiff++;
				if (reported < 5) {
					reported++;
					gReport.fail(fmt("preset lost effect power: p%d(num %d) saved pow=%d, came back pow=%d",
					                 m.pluginIdx, m.num, m.enginePow, r.enginePow));
				}
			}
			continue;
		}
		if (!nearly(m.engine, r.engine, 1e-6)) {
			diff++;
			// Landing exactly on the default is the signature of "nothing applied".
			if (nearly(r.engine, r.def, 1e-9)) stillDefault++;
			if (reported < 5) {
				reported++;
				gReport.fail(fmt("preset lost param p%d(num %d): saved %.17g, came back %.17g (default %.17g)",
				                 m.pluginIdx, m.num, m.engine, r.engine, r.def));
			}
		}
	}
	gReport.check(diff == 0 && powerDiff == 0,
	              fmt("whole track matches the saved preset (%d params differ, %d of them sitting at their default; %d power states differ)",
	                  diff, stillDefault, powerDiff));

	// A preset load is a user action, so unlike a host restore it must be undoable.
	gReport.check(h.hooks.hasUndo(0) == 1, "preset load left an undo entry");

	checkCoherence(h, "after in-app preset load");
}

// The comment at NUM_PARAMETERS in types_grainstorm.h says ids past the marker
// are never saved. The v22 format serializes Snapshot::events_, and addEvent
// takes any paramUpdate whose value differs from its default -- no id bound in
// the writer, the loader, or the DAW blob. This scenario settles it
// empirically: mark initialized plain-double knobs past the marker, round-trip
// a preset, and report whether they come back.
static void scenarioAfterMarkerPersistence(AUHost& h)
{
	gReport.begin("15. Ids past NUM_PARAMETERS in a preset round trip");

	int32_t numParameters = 0, numParams = 0;
	if (h.hooks.markers(&numParameters, &numParams) != 0 || numParams <= numParameters) {
		gReport.fail("gsTestMarkers");
		return;
	}

	h.hooks.setActiveTrack(0);
	settle(h, 0.3);

	// The control: one ordinary before-marker knob driven through the same
	// by-num path, so a failure of the mechanism itself cannot masquerade as
	// "past the marker is not saved". Its (eventType, subType) is also the
	// template the candidates must match -- Event::setup maps some ids onto
	// special event types whose apply/getCurrentValue semantics are not a
	// plain params[] write, and those would muddy the experiment.
	GsTestParamRow control{};
	int controlNum = -1;
	constexpr uint32_t kNoValue = 1u << 6;
	for (int num = numParameters - 1; num > 0; num--) {
		GsTestParamRow r{};
		if (h.hooks.readParamByNum(0, num, &r) != 0) continue;
		if (r.type != 1 || r.isPower || !(r.maxVal > r.minVal)) continue;
		if (r.paramFlags & kNoValue) continue;
		control = r; controlNum = num; break;
	}
	gReport.check(controlNum >= 0, fmt("control knob below the marker (id %d)", controlNum));
	if (controlNum < 0) return;

	std::vector<int> nums{ controlNum };
	std::vector<GsTestParamRow> rows{ control };
	for (int num = numParameters + 1; num < numParams && nums.size() < 7; num++) {
		GsTestParamRow r{};
		if (h.hooks.readParamByNum(0, num, &r) != 0) continue;
		if (r.type != 1 || r.isPower || !(r.maxVal > r.minVal)) continue;
		if (r.paramFlags & kNoValue) continue;
		if (r.eventType != control.eventType || r.subType != control.subType) continue;
		nums.push_back(num); rows.push_back(r);
	}
	gReport.check(nums.size() > 1, fmt("found %zu plain knobs past the marker", nums.size() - 1));
	if (nums.size() <= 1) return;

	auto mark = [&](size_t i) {
		const auto& r = rows[i];
		double v = r.minVal + (0.31 + 0.06 * (double)i) * (r.maxVal - r.minVal);
		if (nearly(v, r.def, 1e-9)) v = r.minVal + 0.57 * (r.maxVal - r.minVal);
		return v;
	};
	for (size_t i = 0; i < nums.size(); i++) h.hooks.applyByNum(0, (int32_t)nums[i], mark(i));
	settle(h, 0.6);

	std::vector<double> markedVals;
	for (size_t i = 0; i < nums.size(); i++) {
		GsTestParamRow r{};
		h.hooks.readParamByNum(0, nums[i], &r);
		markedVals.push_back(r.engine);
	}

	const std::string name = "auhost-marker-" + std::to_string((long)time(nullptr));
	gReport.check(h.hooks.presetSave(name.c_str(), 0) == 0, "saved a preset");
	settleAudio(h, 1.0);

	// Move everything away so a restore is detectable, avoiding both the mark
	// and the default.
	for (size_t i = 0; i < nums.size(); i++) {
		const auto& r = rows[i];
		double away = r.minVal + 0.9 * (r.maxVal - r.minVal);
		if (nearly(away, markedVals[i], 1e-9) || nearly(away, r.def, 1e-9))
			away = r.minVal + 0.13 * (r.maxVal - r.minVal);
		h.hooks.applyByNum(0, (int32_t)nums[i], away);
	}
	settle(h, 0.5);

	int index = -1;
	const int count = h.hooks.presetCount(0);
	for (int i = 0; i < count; i++) {
		char buf[256]{};
		if (h.hooks.presetName(i, 0, buf, sizeof(buf)) == 0 && name == buf) { index = i; break; }
	}
	gReport.check(index >= 0, "found the saved preset by name");
	if (index < 0) return;
	gReport.check(h.hooks.presetLoad(index, 0) == 0, "loaded it back");
	settleAudio(h, 1.5);

	int restored = 0, atDefault = 0;
	std::string firstLoss;
	for (size_t i = 0; i < nums.size(); i++) {
		GsTestParamRow r{};
		h.hooks.readParamByNum(0, nums[i], &r);
		const bool ok = nearly(r.engine, markedVals[i], 1e-6);
		if (i == 0) {
			gReport.check(ok, fmt("control id %d restored through the by-num path", nums[i]));
			continue;
		}
		if (ok) restored++;
		else {
			if (nearly(r.engine, r.def, 1e-9)) atDefault++;
			if (firstLoss.empty())
				firstLoss = fmt(" — first loss: id %d saved %.17g came back %.17g (default %.17g)",
				                nums[i], markedVals[i], r.engine, r.def);
		}
	}
	const int candidates = (int)nums.size() - 1;
	gReport.check(restored == candidates,
	              fmt("ids past NUM_PARAMETERS persisted through the round trip (%d/%d restored, %d on their default)%s",
	                  restored, candidates, atDefault, firstLoss.c_str()));
}

// Saving on one track and loading onto another is the ordinary case: the preset
// list does not care which track a preset came from. Scenario 13 saves and loads
// on the same track, which hides whether the events are retargeted.
static void scenarioPresetCrossTrack(AUHost& h)
{
	gReport.begin("15. Preset saved on one track, loaded onto another");

	const int src = 2, dst = 0;

	h.hooks.setSaveAudioWithPreset(0);
	h.hooks.setActiveTrack(src);
	settle(h, 0.3);

	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[src], 5, 31337);
	if (picks.empty()) { gReport.fail("no automatable params available"); return; }

	for (size_t i = 0; i < picks.size(); i++) h.hooks.applyFromUi(src, picks[i], 0.37 + 0.06 * i);
	settle(h, 0.5);

	auto marked = readAll(h);
	std::vector<double> srcVals;
	for (int idx : picks) srcVals.push_back(findRow(marked[src], idx)->engine);

	const std::string name = "auhost-cross-" + std::to_string((long)time(nullptr));
	gReport.check(h.hooks.presetSave(name.c_str(), 0) == 0,
	              fmt("saved a preset from track %d", src));
	settleAudio(h, 1.0);

	// Now load it onto a different track.
	h.hooks.setActiveTrack(dst);
	settle(h, 0.3);

	const auto beforeLoad = readAll(h);
	std::vector<double> dstBefore, srcBefore;
	for (int idx : picks) {
		dstBefore.push_back(findRow(beforeLoad[dst], idx)->engine);
		srcBefore.push_back(findRow(beforeLoad[src], idx)->engine);
	}

	int index = -1;
	const int n = h.hooks.presetCount(0);
	for (int i = 0; i < n; i++) {
		char buf[256]{};
		if (h.hooks.presetName(i, 0, buf, sizeof(buf)) == 0 && name == buf) { index = i; break; }
	}
	if (index < 0) { gReport.fail("could not find the preset just saved"); return; }

	gReport.check(h.hooks.presetLoad(index, 0) == 0, fmt("loaded that preset onto track %d", dst));
	settleAudio(h, 1.5);

	auto after = readAll(h);

	int appliedToDst = 0, leakedToSrc = 0;
	std::string firstBad;
	for (size_t i = 0; i < picks.size(); i++) {
		const auto* d = findRow(after[dst], picks[i]);
		const auto* s = findRow(after[src], picks[i]);

		if (nearly(d->engine, srcVals[i], 1e-6)) appliedToDst++;
		else if (firstBad.empty())
			firstBad = fmt("p%d(num %d): track %d should now hold %.9g, holds %.9g (was %.9g)",
			               d->pluginIdx, d->num, dst, srcVals[i], d->engine, dstBefore[i]);

		// The source track must be left alone by a load aimed at another track.
		if (!nearly(s->engine, srcBefore[i], 1e-9)) leakedToSrc++;
	}

	gReport.check(appliedToDst == (int)picks.size(),
	              fmt("preset values landed on the destination track (%d of %zu)%s",
	                  appliedToDst, picks.size(), firstBad.empty() ? "" : (" — " + firstBad).c_str()));
	gReport.check(leakedToSrc == 0,
	              fmt("the load did not write back into the source track (%d params changed there)", leakedToSrc));

	checkCoherence(h, "after cross-track preset load");
}

static void scenarioMidiLearn(AUHost& h)
{
	gReport.begin("14. MIDI-learned CC drives a parameter");

	const int track = 1, channel = 0, cc = 21;
	auto st = readAll(h);
	auto picks = pickAutomatableDoubles(st[track], 1, 555);
	if (picks.empty()) { gReport.fail("no automatable double param to learn"); return; }
	const int idx = picks[0];

	gReport.check(h.hooks.midiLearnCC(track, idx, channel, cc) == 0,
	              fmt("learned CC %d ch %d -> track %d param %d", cc, channel, track, idx));

	const double engineBefore = findRow(st[track], idx)->engine;

	// A real CC from the host, through ProcessMidiMsg on the audio thread.
	h.midiEvent(0xB0 | channel, cc, 100, 0);
	settle(h, 0.5);

	st = readAll(h);
	const auto* r = findRow(st[track], idx);
	gReport.check(!nearly(r->engine, engineBefore, 1e-9),
	              fmt("the mapped CC moved the engine value (%.6g -> %.6g)", engineBefore, r->engine));
	gReport.check(r->hasSnap == 1, "the mapped CC change was recorded in the snapshot");

	AudioUnitParameterValue hv = 0;
	h.getParameter(track * h.hooks.paramCount() + idx, &hv);
	gReport.check(nearly((double)hv, r->engineNormInt, 1e-5),
	              fmt("the mapped CC change reached the host (host %.6g, engine implies %.6g)",
	                  (double)hv, r->engineNormInt));

	checkCoherence(h, "after MIDI-learned CC");

	h.hooks.midiClearLearn();
	settle(h, 0.2);
}

extern std::string gDumpDir;
static void scenarioSoak(AUHost& h, int iterations, unsigned seed)
{
	gReport.begin("10. Randomized soak (mixed sources, invariant after every step)");

	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> pick(0, 99);
	std::uniform_real_distribution<double> val(0.05, 0.95);

	const int tracks = h.hooks.trackCount();
	int violations = 0;

	// Earlier scenarios leave real divergence behind (the restore does not
	// resync the host's IParams). Re-baseline here so the soak reports only what
	// the soak itself introduces, instead of re-reporting scenario 7 ten times.
	gKnownBadSnap.clear();
	gKnownBadHost.clear();
	checkCoherence(h, "soak start", 0, /*record=*/true);

	for (int it = 0; it < iterations; it++) {
		const int t = rng() % tracks;
		auto st = readAll(h);
		auto cands = pickAutomatableDoubles(st[t], 3, (unsigned)rng());

		const int action = pick(rng);
		bool didAudio = false;

		if (action < 28 && !cands.empty()) {
			pumpIdle(0.25); // clear the 200 ms guard before host-side writes
			for (int idx : cands) h.setParameter(t * h.hooks.paramCount() + idx, (float)val(rng));
		} else if (action < 56 && !cands.empty()) {
			for (int idx : cands) h.hooks.applyFromUi(t, idx, val(rng));
		} else if (action < 66) {
			h.hooks.undo(t);
		} else if (action < 74) {
			h.hooks.redo(t);
		} else if (action < 82) {
			CFPropertyListRef p = h.getClassInfo();
			if (p) { h.setClassInfo(p); CFRelease(p); }
		} else if (action < 88) {
			// MIDI, both mapped and unmapped, while everything else is in flight.
			for (int n = 0; n < 6; n++) {
				h.midiEvent(0x90, 40 + (int)(rng() % 30), 90, 0);
				h.midiEvent(0x80, 40 + (int)(rng() % 30), 0, 0);
				h.midiEvent(0xB0, 21, (UInt32)(rng() % 128), 0);
			}
		} else if (action < 94 && !gAudioFiles.empty()) {
			// Reloading audio under load is the heaviest thing a user does: it
			// runs the decoder, archives whatever the track held, and rewrites
			// the snapshot's Recording events.
			h.hooks.loadAudio(t, gAudioFiles[rng() % gAudioFiles.size()].c_str());
			didAudio = true;
		} else {
			const int n = h.hooks.presetCount(0);
			if (n > 0) {
				h.hooks.setActiveTrack(t);
				h.hooks.presetLoad((int)(rng() % n), 0);
				didAudio = true; // preset loads can pull audio in
			}
		}

		if (didAudio) settleAudio(h, 1.0);
		else          settle(h, 0.3);
		const int bad = checkCoherence(h, fmt("soak %d/%d (action %d, track %d)", it + 1, iterations, action, t).c_str(), 3);

		// Capture the transition into the first failure: the previous step's
		// state was written on the way past, so "before" and "after" can be
		// diffed to see exactly which action broke the invariant and what the
		// rest of the state (selectors, events) looked like on each side.
		if (bad && violations == 0 && !gDumpDir.empty()) {
			h.hooks.dumpState(fmt("%s/soak-after-break.json", gDumpDir.c_str()).c_str());
			printf("  ....  first break at step %d (action %d): dumped before/after to %s\n",
			       it + 1, action, gDumpDir.c_str());
		}
		if (!bad && !gDumpDir.empty())
			h.hooks.dumpState(fmt("%s/soak-before-break.json", gDumpDir.c_str()).c_str());

		if (bad) violations++;
	}

	gReport.check(violations == 0, fmt("invariant held through all %d soak steps (%d steps with violations)",
	                                   iterations, violations));
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
	std::string bundle = "cmake-build-autest/out/Grainstorm.component";
	std::string dumpDir;
	int soakIters = 12;

	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--bundle" && i + 1 < argc) bundle = argv[++i];
		else if (a == "--dump-dir" && i + 1 < argc) { dumpDir = argv[++i]; extern std::string gDumpDir; gDumpDir = dumpDir; }
		else if (a == "--soak" && i + 1 < argc) soakIters = atoi(argv[++i]);
		else if (a == "--audio" && i + 1 < argc) gAudioFiles.push_back(argv[++i]);
		else if (a == "--help") {
			printf("usage: auhost [--bundle <path.component>] [--dump-dir <dir>] [--soak <n>]\n"
			       "              [--audio <file.wav>]...   (repeatable; enables audio + soak audio steps)\n");
			return 0;
		}
	}

	printf("Grainstorm AU state harness\n");
	printf("bundle: %s\n", bundle.c_str());

	AUHost h;
	std::string err;
	if (!h.open(bundle, err))                     { printf("\nFATAL: %s\n", err.c_str()); return 2; }
	if (!h.initialize(48000.0, 512, err))         { printf("\nFATAL: %s\n", err.c_str()); return 2; }

	h.startRenderThread();
	pumpIdle(0.5);

	scenarioBaseline(h);
	scenarioHostAutomationBeforeFirstSave(h);
	scenarioHostAutomation(h);
	scenarioUiChange(h);
	scenarioUndoRedo(h);
	scenarioAudioThreadPower(h);
	scenarioSaveRestoreSameInstance(h);
	scenarioRestoreIntoFreshInstance(h, bundle);
	scenarioMidi(h);
	scenarioLoadAudio(h);
	scenarioAudioSurvivesStateRoundTrip(h);
	scenarioAppPresets(h);
	scenarioAfterMarkerPersistence(h);
	scenarioPresetCrossTrack(h);
	scenarioMidiLearn(h);
	if (soakIters > 0) scenarioSoak(h, soakIters, 20260812u);

	if (!dumpDir.empty()) {
		const std::string p = dumpDir + "/final-state.json";
		h.hooks.dumpState(p.c_str());
		printf("\nstate dump: %s\n", p.c_str());
	}

	h.stopRenderThread();
	h.close();

	printf("\n=======================================\n");
	printf("checks: %d   failed: %d\n", gReport.checks, gReport.failed);
	if (gReport.failed) {
		printf("\nfailures:\n");
		for (const auto& f : gReport.failures) printf("  - %s\n", f.c_str());
	}
	printf("=======================================\n");
	return gReport.failed ? 1 : 0;
}
