# WORKLOG — Grainstorm PV / cross-synthesis work

Last session: 2026-09-05. No CLAUDE.md exists; this file is the whole handoff.

## GS2 iOS: the AUv3's purchase notice -- a gate dialog like the licence checker's, no feature gate (2026-09-05, in CPP-New)

**Patrick:** "on ios the auv3 should just open an overlay and refer the user
the purchase when it is not purchased. This overlay shall stay and is not
cancelable. See the licensechecker. No need to restrict ui or feature on
auv3." **Where:** CPP-New `6db2857`.

- **The notice** is a dialog of the licence terminal's shape: modal, no
  buttons, `dlg::AuUnlock` (tslui/dialog.h), title "GRAINSTORM AUv3 IS NOT
  UNLOCKED / Buy or restore the AUv3 upgrade in the Grainstorm app's SETUP;
  it unlocks here at once", with the licence spinner's indeterminate bar
  labelled WAITING FOR THE PURCHASE. `dlg::isGate` = the licence five + this
  one; `closeDialog` refuses every user route for a gate (scrim, escape,
  RETURN) exactly as it did for the licence flow.
- **The beat:** `Controller::syncAuGate` (after syncLc in frameTick) asks
  `gs2::auUnlockRequired` once a second: iOS + extension + record unset
  (`tsl::app::isAuPurchased`, the app group's defaults) → open it; record
  set → take it down and stamp `auv3Purchased`. A purchase made in the app
  while the host waits lifts the notice within a second of coming back --
  no restart. Another dialog up (a name being typed) is left alone; the
  gate waits for its next beat.
- **No feature gate:** the shell's commented-out ProcessBlock silence gate is
  gone with a comment saying why; the plugin runs as the app does.
- **Test:** `checkAuGate` through the engine's test hook
  (`forceAuUnlockRequired`): opens on the beat, modal, buttonless, names the
  app; cancel / confirm / scrim tap refused; a press through it does not
  reach the transport; stays up over 40 frames; lifts on the next beat once
  bought; a host that is neither iOS nor an extension never opens it.
- **Tests:** gs2_layout_test 156 FAILs against the 158 baseline with
  `checkAuGate` green. One INTERMITTENT failure seen twice in five full runs
  and never in isolation: "deferred: the preset scan never landed" (the
  IMPORT PRESET scan on UiTasksQueue not taken within ~2.5 s). The path is
  untouched by this change (per-instance queue, correct push/wake protocol
  read through; the guard's decrement runs in the task's destructor after
  the run) -- a race to find, not a regression of the gate. Not chased
  further here.
- **Needs on device:** the AUv3 target must carry the same
  `group.io.github.iplug2` app-group entitlement as the app, or the extension
  never sees the record and the notice never lifts. The CMake/Xcode
  generation's CODE_SIGN_ENTITLEMENTS for BOTH targets is the thing to check.

## iOS: the upgrade view -- Android's InAppPurchases screen in UIKit, the purchase record read at start (2026-09-05, in CPP-New)

**Patrick:** "ios i dont know if wired into settings yet but i need the in app
purchase flow in the same way grainstorm has it. is it possible to open an
iap overflow same as gs android that renders html and has a purchase button
and price?" **Where:** CPP-New `8611d84` (tslgraphics2
PlatformPaths/storekit_ios.mm, public_headers/app.h; grainstorm2
src/host.cpp); here: CMakeLists.txt, resources/upgrade.html,
resources/upgrade_au.html. NOT built: no iOS SDK on this host -- the .mm is
unverified by a compiler; the Linux gs2 targets build.

**What was there.** The settings rows (Unlock Pro / Restore Purchase / Unlock
AUv3) were already wired in gs2 to the same StoreKit calls as gs1 -- but
those were a BLIND flow: `purchasePro()` ran an SKProductsRequest and added
the payment straight away, no page, no price. And gs2's createEngine never
called `checkAndSetupPurchase()`, so a purchase made on an earlier run was
never read back (the flag is forced open on iOS anyway, as gs1's is).

**The upgrade view** (storekit_ios.mm): `purchasePro()` / `purchaseAU()`
now PRESENT a full-screen `TSLUpgradeViewController` over the app's key
window (foreground UIWindowScene -> root -> presented chain): a title bar
(Close / title / Restore), a WKWebView showing the bundle's `upgrade.html`
or `upgrade_au.html` (loadFileURL with read access to the resources
folder), and a bottom bar with the App Store price ("Price: 9,99 €" via
NSNumberFormatter on the product's priceLocale) and a PURCHASE button that
appears once the product arrived -- the Android screen's shape and its
words ("Retrieving price...", "Failed to retrieve price.", "Billing not
initialized.", "Upgrade is owned."). PURCHASE adds the payment; the same
SKPaymentQueue observer finishes it, writes the record, calls
`onPurchaseComplete` (the shell's lambda -> `onIAPComplete`) and tells the
view: "Thank you very much." -> dismiss; a cancel is silent; a failure says
so. Restore inside the view reports "Nothing to restore." when it finds
nothing. A bundle without the html gets a built-in page with the same words.
Product requests carry a completion block per request (NSMapTable), so a
price fetch and a purchase no longer share one delegate path.

**Settings rows** (host.cpp): open the view; an owned upgrade toasts
"Upgrade is owned." (MainSettingFragment's behaviour); in the AUv3 all three
toast "Please purchase in the Grainstorm app." -- the extension has no
window and no store. createEngine on iOS now runs gs1's two lines in gs1's
order: `checkAndSetupPurchase()` then the forced-open store; arming the
gate is deleting the second line (same for gs1).

**The record** is NSUserDefaults AND the app group's
(`group.io.github.iplug2`, the one the iOS entitlements carry): the AUv3 is
another process, and `au_purchased` written to standardUserDefaults by the
app was invisible there -- `auv3Purchased` could never be true in the
extension, which is one reason the ProcessBlock gate is still commented
out. Both stores are read, both written; without the entitlement it is the
app's own defaults as before.

**Bundle:** `upgrade.html` / `upgrade_au.html` (Android's upgrade.html text,
self-contained CSS, "handled by Apple", no screenshots -- Android's are of
the old UI) go into the iOS bundle under `GS2`; `-framework WebKit` linked
under `GS2` on iOS. GS1's bundle and link line are untouched.

**Left as found:** dofastrender forced open on iOS (test builds stay
open); the AUv3 silence gate commented out; `Grainstorm.storekit` (the
Xcode StoreKit configuration for sandbox testing) not touched.

## GS2 revision: panels built off the UI thread, the name-dialog worker parked, the settle asserted, the bus in the snapshot (2026-09-05, in CPP-New)

**Patrick:** "full revision of gs2. Gs1 defers heavy ui construction, for
example track settings to a worker, gs2 doesnt and some buttons for example
show tracksetting take a bit before button goes back from pressed state (UI
Lag). askforfilename the worker thread has to wait for the filename and must
not continue before it is processed waiting on state waitnotify. also check
logoswirler, it shall settle as original icon and not slightly rotated.
Recheck au and vst3 host ui midi snapshot (undo redo)". No agents.
**Where:** CPP-New `173add4`. Linux clang build only; not run on a
device or in a host here.

**UI lag — what ran on the press, and which of gs1's queues it moved to.**
gs2's view tree is a pure function of state and costs nothing; the lag was
work the press did BEFORE it returned:
- Track settings: `aiAvailable` (JNI on Android, a stat per model folder on
  desktop) decided the FROM AI row on the press. Now `aiBuiltIn` (the
  compiled-in model count) decides it; the models' state is fetched later.
- ACTIVE EFFECTS (FX ORDER): three `trackFxChain` rendezvous with the audio
  thread (500 ms timeout each) on the press, and again twice a second while
  the panel stood. Now `requestFxChains` on `_DATA->snapShot` (the thread gs1
  reads the chains on); `Controller::pollPanels` takes the result on a later
  frame and opens the panel (`openEffectOrderPanelWith`) or refreshes the open
  one — never mid-drag. gs1's toast when the worker has a backlog: "ACTIVE
  EFFECTS Queued. Pos N."
- SAVE/IMPORT PRESET, SAVE/LOAD MAPPING: the directory scan and every
  header's decode on the press. Now `requestPresetList` /
  `requestMidiMappingList` on `UiTasksQueue`; the panel opens when the list
  lands (`open…PanelWith`), the header cache installed on the taking thread.
- AI MODELS: a JNI round trip per model on the press and on the beat. Now
  `requestAiModelRows` on `UiTasksQueue`; `refreshAiModelRows` on the panel.
- Mechanism (host.cpp): one latch per kind under `gPanelMutex` (result +
  ready flag), an in-flight count held by an `InFlight` RAII guard the task
  captures as a shared_ptr — so a task destroyed unrun at teardown still
  counts down (hand-counted, it left "pending" set for the life of the
  process). `PendingPanel` on the controller remembers which panel to open
  and where. `fxChainsPending()` etc. tell the beat not to stack requests.

**askForFilename:** gs2 latched the question (`requestLoopName`) and the
worker ran on; the rename happened on the UI thread and the queue behind it
moved before the file had its name. Now `askLoopName` (gs2_loop_record.h,
host.cpp): `waitNotify.begin_wait()` → latch → `wait_for_signal(token)`; the
dialog's SAVE/DELETE goes through `answerLoopName`, which `complete(token)`s,
and the WORKER finishes the file (`finishLoopName`: trimToValidFilename
rename / remove / the toasts, gs1's). `destroyRequested` +
`waitNotify.shutdown()` wakes it → false, the file stays as written. Same
three sites as gs1: RECLOOPButton and RECORDButton (Event/ApplyFromUi.cpp,
snapshot worker / RecordingQueue) and save_loop (gs2_save_loop.cpp,
WorkerQueue). DEFERRED.md's AlphaPopUp row: ANSWERED. Test:
`checkLoopNameParksWorker` (the worker has not returned while the dialog is
up; SAVE/DELETE/EMPTY outcomes done by the worker).

**LogoSwirler / storm settle:** gs1's GL swirler is analytically the icon at
progress 0. gs2's StormSplash measured in the probe against the drawn icon
at the end of the blend: centre ±0.3 px, principal axis 0.00°, coverage
0.998 — it stands as the icon. The splash probe now ASSERTS it (centre
≤ 1.5 px, axis ≤ 0.25°, coverage ≥ 0.85; exit 2), so a regression is a red
probe and not a phone video.

**AU/VST3 host UI ↔ MIDI ↔ snapshot ↔ undo/redo:** IPlugEffect2.cpp's route
is gs1's method for method; the master block is the one intended
difference. Engine faults found on the way and fixed:
- PLUGIN build `Snapshot::clear(int)` erased the BUS's events on RESET TRACK
  1 (a master event carries trackIndex 0; the type carries the row) and told
  the host every master parameter was at default → `!isMaster()` guard, as
  the app build already had.
- `Snapshot::apply` (host restore / project load naming a sound) called
  `track->waveform->setup` through the new UI's null → guarded.
- The bus's power events now restore like a track's three: held back and
  replayed in descending position order (snapshot apply, Presets/load.cpp
  v22, history's isPowerOff); a learned MIDI note's Power switch covers
  `powerMasterFx` (it wrote a plain 1.0 over a PowerState).
- tslgraphics2 MidiSaver: `directory_iterator` with an error_code — the
  throwing overload made "no mappings folder yet" a std::terminate the first
  time the list was asked for.

**Tests:** gs2_layout_test 156 FAILs against the 158 baseline — the two
stale strip FAILs (ACTIVE EFFECTS / INPUT ROUTING "nothing acts on it") are
gone, nothing new. New: `checkDeferredPanels` (press → the settings panel
stays front and unpressed, the request is pending, the FX ORDER panel lands
with the effect just switched on; IMPORT PRESET the same), the parked-worker
test above, the splash settle. The cells POWER check had a scheduling race
(the flip is two thread hops from the press and four blocks were not always
enough — measured ~1 ms late when late) → a bounded settle. gs2_splash_probe:
"settle: OK". gs2_looptodisk_probe does not link here (pre-existing:
decodeWithMediaFoundation).

## Storm splash: the rotation found on the phone's own frames; tiles of the icon (2026-09-05)

**Patrick:** "still rotated" (and, rightly, "u take me for dumb? of course
its the new build"); "grains shall settle into real icon".
**Where:** CPP-New `9140c2a`.

**Method that worked:** `adb shell screenrecord --time-limit 40` (passive,
no input) while Patrick relaunched; `ffprobe … signalstats YAVG` to find the
launch by brightness; ffmpeg crops of the frames; principal-axis angle +
tallest-bar lean measured in python. Settled grains vs drawn icon: same
centre ±2 px, lean 0.1°/0.0° → NOT rotated at rest. The frame before
landing: the cloud skewed on a diagonal = a coherent swirl.
**Cause:** the per-grain LCG (`rng*1664525+1013904223`) — consecutive
outputs correlate, and each grain drew angle then spin SIGN in sequence →
grains from one side all spun the same way. **Fix:** `std::mt19937`.
Verified in the probe at 720×1600 @ 2 px/dp (the phone's surface): the
arriving cloud is a symmetric blur.

**Grains = icon tiles:** icon rasterized at its standing size (box =
0.72·min(w,h)), cut into cell_ px squares (2 px on the phone, grown until
≤ 20000 grains), each in its pixel's colour, drawn with kSquare_Cap;
nothing moves at rest (no jitter/shimmer/breath); shimmer only in flight.
The crossfade to the drawn icon stays (edges only). resize() re-cuts from
the same seed.

Android .so + mac build; not re-verified on device after the fix.

## Storm splash: grains crossfade into the icon itself for the hold (2026-09-05)

**Patrick:** "it is slightly rotated. and i thought u would display it
ungranulated". **Where:** CPP-New `761c6bf`.
- Rotation: already gone in `e297a61` (the ±0.03 rad hold rocking); a build
  from before it still rocks.
- Ungranulated: HOLD now = the icon drawn (drawImageRect into the grains'
  box, breathing with them). Blends: gather→icon 0.35 s (`kBlend`), icon→
  grains 0.35 s at release, then the scatter. `grainMix`/`iconMix` in
  draw(); kHoldMin 0.9 so both blends fit. Probe hold frame = the crisp icon.

## Storm splash: the endless loop, the "tiled" logo, the tilt (2026-09-05)

**Patrick:** "now in an endless logo loop and still doesnt settle to show
logo orignal (logo settles rotated and tiled)". **Where:** CPP-New `e297a61`.

- **Endless loop:** after `finished()` the draw thread `splash.reset()`
  and the next frame's `if (!splash) create` started a new storm.
  `stormDone` (draw-thread local) → plays once per run.
- **"Tiled":** the icon (grainstorm2/icon.cpp, 512², 7.6 KB) is the mark on
  an OPAQUE black rounded square → alpha-only sampling made ~90 % of grains
  black (invisible) and thinned the white ones onto a 6-px lattice. Now a
  grain = alpha > 96 AND luminance > 40 (`isInk`): 5120 grains, all on the
  bars/brackets; the hold frame is the icon, solid and grainy.
- **"Rotated":** the hold's ±0.03 rad rocking is gone (breath 1 % only).
- Probe writes `splash-0-icon.png` (the decoded icon) next to the frames.
- Builds both trees; NOT device-run.

## Storm splash: frame-driven clock, grains in from past the edges (2026-09-05)

**Patrick:** "storm logo sometimes stop midstorm doesnt settle completely";
"storm appears mid screen, tiles should move in offscreen".
**Where:** CPP-New `cd129f9` (splash.h/.cpp, main_android.cpp, splash_probe).

- **Clock = frames drawn** (`clock_ += min(dt, 1/60)`-style: a late frame
  counts as one frame). A stall (pause/permission prompt/OEM splash
  hand-off — the vsync chain stops kicking while paused) no longer hangs
  the gather or jumps it; it resumes where it was. The shell logs
  `storm: N ms without a frame (paused=, engineUp=)` for any gap > 250 ms
  — READ LOGCAT for this line to learn what stalled it on device.
- **Paths**: each grain starts at `edgeDist(target, a0) × (1..1.6)` past the
  window rect (+12 % margin) along its own angle, ease-out in with a curve
  (`spin·(1-q)²`); exit mirrored with ease-in. Old: circle of 1.2–2.1
  half-diagonals round the centre → sideways grains of a tall phone were
  off-screen most of the gather.
- Probe feeds 60 fps frames up to each shot and asserts a 1 s stall
  advances one frame. Builds both trees; NOT device-run.

## Android black screens on relaunch + the storm that only sometimes played (2026-09-05)

**Patrick:** "several times black screen nothing rendering, logo not shown
only sometimes". **Where:** CPP-New `9f31fcb`, android `1b54cc0`.

1. **`Shared::quit` never cleared** (pre-existing): DESTROY sets it; a
   relaunch in the same process re-runs android_main with the old `g` →
   the new draw thread's `while (!quit)` exits at once → black, nothing
   rendered. android_main now resets quit/paused/window/sizeChanged/
   vsyncChainActive/readySignalled/touch ring at its start.
2. **Dead engine adopted on relaunch** (mine, yesterday): destroyEngine
   does NOT null the handle; `g.engine`/`engineReady` survived the draw
   thread's exit → the next draw thread adopted a destroyed engine while
   the new setup thread raced to replace it. Both cleared right after
   destroyEngine.
3. **Storm skipped when the engine was fast** (mine): the storm ran only
   while `!engineReady` → an engine ready before the first frame = no
   logo. It always plays; readiness only releases the hold. Phases now
   1.6 / ≥0.7 / 1.0 s.
4. Java: `nativeEngineReady` (static) cleared per onCreate, so a
   relaunched activity keeps its androidx splash until the storm's first
   frame (native signals onNativeReady again per run).

**Android relaunch = same process, android_main re-entered with the
process-wide `Shared g` intact.** Anything per-run in `g` must be reset
there. Android .so + mac test bed build; NOT device-run.

## Android: setup thread, the storm splash, and Java's fast-render answer waited for (2026-09-05)

**Patrick:** engine creation on the render thread "might cause ANRs, in gs1
we had a setup thread ... and showed the animation while being setup, see
window.cpp"; and "a storm of grains forming the logo then disappearing
again as a storm". **Where:** CPP-New `master` `de7bde4`.

**Setup thread** (app/main_android.cpp `Shared::setupThread`): started in
android_main before any window (GS1's `__STATE->setup_thread`), runs
createEngine, publishes `g.engine` + `engineReady`. Draw thread adopts it
(renderer->setEngine) and builds the Controller engine-first AFTER the
storm; joins the setup thread before destroyEngine.

**Fast-render latch** (`grainstorm2/gs2_fastrender.h`, impl in setup.cpp):
guiSetup → `latchFastRender(isOpen)` (works before the state exists) +
store into AppState when present. createEngine (Android) waits ≤10 s
(GS1's try_acquire_for) on the setup thread for the latch, THEN stamps the
enum tables (`initEnumConversionTables` splits ASGRAN/ASFX/ASSTFX on it).
Previously createEngine forced TRUE on every platform (comment: "when gs2
gains an Android shell that is where this stops being unconditional") and
my first guiSetup dropped an answer that arrived before the state → a
non-purchaser would have run FULL with full tables. Mid-run purchase: UI
follows next frame (syncPage compares ScreenRef::fast), tables stay —
GS1 parity.

**Storm splash** (`include/gs2/splash.h`, `src/splash.cpp` in gs2_gpu):
`StormSplash(logo, w, h, pxPerDp, seed)`; grains = ~9000 samples of the app
icon's opaque pixels (2–4 per pixel, jittered in-cell, per-pixel colour in
≤32 buckets); GATHER 1.9 s (tightening spiral, smoothstep onto target),
HOLD ≥0.9 s (breathing 1.5 %, tilt ±0.03 rad, granular shimmer) until
`ready`, SCATTER 1.1 s (opening spiral, fade). One drawPoints per bucket.
Java's androidx splash hands over to the storm's first frame
(signalReady once, `readySignalled`); touches during the storm are dropped
(ring drained). `gs2_splash_probe [w h]` → 7 PNGs in build/ (viewed: the
mark reads as the grainy waveform icon with its corner brackets).

**Verified:** Android .so links (Release .cxx tree); mac test bed compiles
gs2_gpu with the splash; layout FAIL set unchanged (160). NOT device-run:
thread hand-off, ANR behaviour, the storm on a real GPU, the 10 s wait.

## Android: every Java→native is registered — grainstorm's whole table, guiSetup defined (2026-09-05)

**Patrick:** "make sure java into native methods are present as in gs1, got
another crash when disabling mic-pause playback in android settings".
**Where:** CPP-New `master` `433fdeb` (grainstorm2/setup.cpp,
app/main_android.cpp).

**Cause:** gs2's Android JNI_OnLoad hand-picked FOUR of GS1's nineteen
natives; the Java side is GS1's own (MainActivity, MainSettingFragment,
MidiEngine, PresetActivity, BillingManager), so every other call was an
UnsatisfiedLinkError — REC (fixed earlier), LOOPTODISK (yesterday), now
"Pause Playback" → java_set_pauseplayback; still waiting: output/mic
format, save-audio, MIDI port (java_receive_midievent), preset activity
(read_header/read_midiheader/save_*_callback/java_load_preset),
java_control, java_ofl (billing), guiSetup.

**Fix (as in GS1):** GS1's `tsl::android::methodTable` in setup.cpp was
compiled out under GS2_NEW_UI because guiSetup (gui/gui_setup.cpp) did not
link. `guiSetup` is now defined in setup.cpp — GS1's body minus the
readyForUiSetup release (nobody waits; a binary semaphore released 3× is
UB) — and **it is the only place the Android app's fast-render/purchase
state reaches the engine** (MainActivity calls guiSetup(isOpen(), w, h)
after billing; the gs2 shell never set dofastrender before). The table is
unguarded; main_android.cpp's JNI_OnLoad registers it WHOLE on
MyApplication (as tslgraphics2/src/android-main.cpp does for GS1); the
hand-picked natives[] is gone.

**Verified:** libgrainstorm.so links in the app's configured Release
tree (`ninja -C app/.cxx/Release/4g3v6a3t/arm64-v8a grainstorm`, the NDK
cmake's ninja) — a usable way to compile gs2 for Android WITHOUT gradle.
Not device-run.

**RULE going forward:** never hand-pick natives; Java's `native static`
declarations in MyApplication.java == GS1's table == what must link.

## LOOPTODISK crash: the case sat in an Android-only sub-block; java_save_loop was never defined (2026-09-05)

**Patrick:** "Looptodisk crash". **Where:** CPP-New `master` `767da69`.

**Cause 1 (both platforms):** in grainstorm2/Event/ApplyFromUi.cpp the
standalone block now OPENS with an `#ifdef __ANDROID__` sub-block holding
SETTINGSBUTTON/EJECTBUTTON (another session's 8c6a974 this morning). My
EDITORSAVE case was inserted "before `case SETTINGSBUTTON:`" → inside that
sub-block → compiled OUT on desktop (row did nothing) and IN on Android.
**Cause 2 (Android, the crash):** its Android branch is GS1's
recFile(SAVE_LOOP); Java then calls native `java_save_loop(fd, track)` —
never defined in the gs2 tree and deliberately NOT registered in
app/main_android.cpp's natives table ("that is Editor.cpp's") →
UnsatisfiedLinkError on the first press.

**Fix:** case moved directly under `#if defined STANDALONE_MODE ||
PLUGIN_MODE` with its own inner Android branch (RECLOOPButton's shape);
gs2_save_loop.cpp now has BOTH entries over one body (desktop
`save_loop`, Android `java_save_loop` taking Java's fd like
synth.cpp's java_record_loop); main_android.cpp registers java_save_loop
(local prototype, not Editor.h — that drags old-toolkit view headers in).

**Verified:** new host tool `gs2_looptodisk_probe` (test/looptodisk_probe.cpp):
engine + 1 s tone + the real press → WorkerQueue → the WAV appears in
~/Library/Application Support/com.thesecretlaboratory.grainstorm/Grainstorm/
Recordings (192 044 B = 1 s stereo 16-bit). Before the move the press
wrote nothing while a direct save_loop call did. Layout test FAIL set
unchanged (160). Mac app tree builds. Android NOT built here.

**TRAPS for anyone editing ApplyFromUi.cpp in the gs2 tree:** (1) print
the `#if` map (`grep -n '^#if\|^#else\|^#endif'`) before inserting a case
— the standalone block's first ~70 lines are Android-only; (2) any GS1
`recFile(...)` round trip needs its native return leg DEFINED in the gs2
tree AND listed in main_android.cpp's `natives[]` — the gs1 methodTable
in setup.cpp is compiled out under GS2_NEW_UI.

## RECORD: the track-settings sub-list, the track-menu row, the live popup; Android models refresh (2026-09-05)

**Patrick:** settings menu for all but Android with GS1's entries; track
settings RECORD (RECTO/LOOPTO/LOOPTODISK) through ApplyFromUi; GS1's
RECORDButton (whole output) as a RECORD entry in the track/master/settings
popup with a floating "Recording" popup (time, MB, cancel, waveform like
FROM MIC), same popup for RECTO1..4; Android models view not refreshing
while open. **"For all see gs1. must do the same stuff, use same queues,
mechanisms, important."**
**Where:** CPP-New `master` `54844bb`, android `f77dd79`.

**Settings (non-Android):** already GS1's list, gating and mechanisms —
settings.cpp carries GS1's XML verbatim with the same #ifs (iOS-only
Audio/Microphone/Pro/AUv3, GS_AIGEN AI Models, non-mobile credentials,
per-platform privacy URL), host.cpp wireSettings applies the same keys the
same way (buffer_size on the snapShot worker with the toast, credentials
via OSCredentialStore, purchases via AppState). Nothing to add; verified
by reading, not changed.

**RECORD panel** (panelId::RecordView, menuGo::RecordRow): rows are
EDITORLOOP1..EDITORSAVE (fast) / ..EDITORREC4 (free) by param name —
LOOPTO1..4, RECTO1..4, LOOPTODISK — exactly ListView::show's ranges; a
press = setEngineActiveTrack + pressTrackParam → applyFromExt(FromUi) →
GS1's cases (snapShot / RecordingQueue / WorkerQueue). MIDI-learn
intercept + learn accent as the editor panel. `recordAction*` seams in
host.h.

**LOOPTODISK had NO engine half in gs2:** the desktop EDITORSAVE case was
dropped from grainstorm2/Event/ApplyFromUi.cpp and `save_loop` declared
but never defined. Restored verbatim (with GS1's Android recFile(SAVE_LOOP)
branch inside — this tree's Android engine compiles the standalone block)
+ NEW `grainstorm2/gs2_save_loop.cpp` = GS1 Editor.cpp save_loop with the
loop-render overlay (loopRecordTrack/Fraction, abort_loop_record) instead
of the InfoPanel and requestLoopName instead of the AlphaPopUp.

**RECORD in the track menu** (menuGo::LiveRecord): only under fast render
(GS1 draws RecordButton only then); label STOP RECORDING while running;
press = pressTrackParam(RECORDButton) → GS1's RecordingQueue task around
Player::record_live_thread / recStop. GS1's Android branch
(recFile(RECORD_LIVE)) restored inside the case; askName → requestLoopName.

**Live popup** dlg::LiveRecording (=23, tslui/dialog.h): driven by
`player.isrecording` (pollLiveRecording, the mic popup's pattern) so
RECORD and RECTOn share it; title RECORDING / REC TO TRACK n
(liveRecTarget_ set by the pressed row); status "mm : ss : ms  x.x MB"
from player.recoff (bytes by format; 16-bit frames for a track); STOP =
recStop; waveform = last 3 s from a lock-free ring `renderEngine` fills
on the audio thread (host.cpp LiveTap, sized in createEngine). Android:
readout, no picture (Oboe callback never passes renderEngine).

**Android models refresh:** "none" state restores the XML explainer
(delete while open); every row re-polls while resumed (1.5 s active / 3 s
idle), onResume/onPause. Not device-run.

**Verified:** gs2_layout_test +24 checks (checkRecordPanelRows drives the
real pointer path; proven to fail on the old disabled row), FAIL set
unchanged (160 pre-existing); grainstorm2_iplug2 app tree builds. NOT run
by hand: the live recording, RECTO, LOOPTODISK, the popup's picture. The
Android engine/APK not built here (SAVE_LOOP/RECORD_LIVE are defines.h).

**Parallel-session note:** grainstorm2/Event/ApplyFromUi.cpp was touched
today by another session (8c6a974/cfb8a98/736358d: desktop EJECTBUTTON via
an `AppState::openFilePicker` hook nothing sets); my edits sit beside it.

## Eject on macOS did nothing — FROM FILE now asks the shell (2026-09-05, in CPP-New)

**Patrick:** "Eject on macos doesnt work also check on ios."
**Where:** CPP-New `master` `ca5f3e8` (controller.cpp menu::LoadFile,
controller.h comment, test/main.cpp checkEjectAsksTheShell).

**Cause:** with an engine attached, FROM FILE pressed EJECTBUTTON into the
engine; only the engine-less test bed set `loadRequested_`. GS1's
Event/ApplyFromUi has EJECTBUTTON cases for ANDROID (JNI filebrowser) and
OS_IOS (UiTasksQueue → AppState::openFileBrowser → fileBrowserCallback →
filebrowsercallback) and NONE for desktops — the press died there while
both mac shells (app/main_mac.mm NSOpenPanel, grainstorm2_iplug2's
PromptForFile in ServiceLoadRequest, polled from OnFrame) waited on
takeLoadRequest().

**Fix:** everywhere but `__ANDROID__`, set the load request (shell opens
the picker, answers via Controller::loadFile = gs2's loadIntoTrackAsync
with the progress display, same as drag & drop). Android keeps the engine
→ Java route. Drag & drop on mac was never affected.

**iOS:** by inspection only, NOT built or run — no gs2 iOS tree exists.
With the fix, iOS uses the iPlug2 shell's async PromptForFile
(IGraphicsIOS.mm has it) → LoadFile, the same path as mac. The old engine
route would have needed `filebrowsercallback` for Apple, which gs2 only
defines in DecoderLinux/Windows/Android — so it could not have linked.

**Test:** checkEjectAsksTheShell drives the real pointer path with an
engine (LOAD tile → FROM FILE row via selectorLayout) and expects
takeLoadRequest() for track 1; proven to FAIL on the old code. +6 checks,
FAIL set unchanged (160 pre-existing). The app tree
(`grainstorm2_iplug2/cmake-build-release`, Ninja Release) rebuilt clean:
controller.cpp recompiled, out/Grainstorm2.app relinked, deployed and
signed. NOT run by hand — Patrick's eyes.

## Master made to reflect the whole tree (2026-09-05)

**Patrick:** "make master reflect all changes up to now". Everything of
mine was already on master; what remained were stale uncommitted snapshots
from earlier sessions, all last edited Aug 26–31 (nothing live):
- CPP-New `cce05e5` generative engine work + tools/ssh-sharim note, and
  `a64b458` the `gen2/` sketch (sources only; the three built render binaries
  in gen2/tools stay untracked — cce05e5's message claims gen2, a relative
  glob missed it). Syntax-only compile clean; NOT built or listened to.
- CPP-New `97846da` va INTEGRITY_TEST_FAST hook (off unless passed to CMake).
- android `dd3105d` gradle native path grainstorm/ → grainstorm2/ (the tree the
  device APKs really come from) + preferences_main.xml cleanup.
- Left untracked on purpose: `grainstorm_iplug2/.new` — an Aug 20 HTML
  "Grainstorm 2 architecture decisions" draft under a hidden name; every
  session since has left it. Say the word and it goes in under a real name.

## Android FROM AI "no model" while Settings says installed — R8 keep rules (2026-09-04, android repo)

**Patrick:** "Android path load from ai, Downloaded file, shows downloaded,
correct in Settings. Still Eject-From Ai Dialog shows No Model found."
**Where:** `~/AndroidStudioProjects/grainstorm` `master` `99426da`
(app/proguard-rules.pro only).

**Cause:** gs2's `grainstorm2/src/aigen.cpp` asks Java BY NAME
(`aiStatic` → GetStaticMethodID) for `aiModelPresent`, `aiModelOpen`,
`getAiLinkDir` — the fd/MediaStore model API added 2026-09-03/04 when
models moved to Downloads/Grainstorm. None was in the MainActivity keep
block (only `getAiModelsDir`/`startAiModelDownload`/`queryAiModelDownload`
from 08-17). Minified builds (release; staging via initWith) RENAME
aiModelPresent (Java calls it → survives shrinking, not obfuscation) and
REMOVE the other two. The lookup returns null, the exception is cleared
silently, and jniModelPresent falls back to `getAiModelsDir()/<dir>/<file>`
— the old app-storage path the new flow never writes → "NO MODEL INSTALLED
- SEE SETTINGS". The Java settings page calls aiModelState directly, so it
says Installed. Debug builds never show it (not minified). Same class of
bug as the 08-17 getAiModelsDir keep rule; memory has it.

**Fixed:** the three methods added to the keep block. Needs a rebuilt
staging/release APK to verify; NOT device-verified.

**Second thing to know:** the settings row reads "Downloaded - verifying..."
(state `success`) between the transfer ending and the sha256 pass over
1.7 GB finishing (aiVerifyPending, background thread); in THAT window the
dialog is right to say no model. "Installed (1.7 GB)" is the installed
state.

**Parallel session note:** the android repo had UNCOMMITTED changes when I
came (build.gradle switching the native path grainstorm/ → grainstorm2/,
preferences_main.xml cleanup) — not mine, left untouched. The committed
gradle still builds the GS1 tree, whose aigen.cpp only knows
getAiModelsDir() — a GS1-tree APK cannot see Downloads-based models at all.

## GS2 UI: glyph-less triggers are bare cells — SEQUENCER1 LOAD/SAVE (2026-09-04, in CPP-New)

**Patrick:** "sequencer1 load save buttons dont respect standard padding".
**Where:** CPP-New `master` `d96d279` (params.h, pages.cpp, screens.cpp,
test/main.cpp).

drawTriggerCell paints a trigger without a glyph as a TEXT button (name on
the face, grainstorm's TextButton2), but the layout sized it as a titled
cell — an empty name band over every LOAD/SAVE box, 39dp box to box down
the page against 16 everywhere else. `cellDrawsBare(const PageControl&)`
= the id list OR (Kind::Button with iconForParam == None); the standing
body asks by control at all five bare decisions. LOAD/SAVE rows are one
button tall now, 17px box to box measured, level with the tiles above.
gs2_layout_test had NO bare-cell notion (every non-transport flat cell =
switch height); it now accepts tile height, or switch height beside a
titled neighbour. FAIL set byte-identical (160, pre-existing).

## GS2 UI: no rules in the banks either — VCO2 / FM2 blocks flush (2026-09-04, in CPP-New)

**Patrick:** "there are still separator lines drawn for example VCO2".
**Where:** CPP-New `master` `e69e613` (screens.cpp makeOscBank / makeFmBank /
the uncalled makeUnitBank; layout-sheet.svg regenerated).

The three bank builders drew a 1px hairline between units in both shapes.
Gone. Blocks carry the bar's padding themselves, so with no rule and no gap
they sit the bars' distance apart (16dp ink to ink) across and down, first
ink 8dp in — the same place as every other page after the origin fix. The
standing strips used to pad kBarPad and gap kBarGap on top of the blocks'
own padding (VCO2 standing: 33dp between oscillators with the rule in the
middle); they pad 0 / gap 0 now and the fit arithmetic (per, perCol,
wantW) states the flush layout. `Paint::Rule` has no user left in
screens.cpp; the only hairline separator still painted is the settings
dialog's category heading (drawSettingRow), which is not a page.
`gs2_layout_test` FAIL set byte-identical (160, all pre-existing). Render
68-page-pv-appwindow (which actually shows VCO2 lying) checked by eye.

## GS2 UI: the standing body's origin — same widget, same place on every page (2026-09-04, in CPP-New)

**Patrick:** "why do the same view types jump. for example grain fx BPM and
SEQUENCER1, buttons of bpm correctly aligned with general track buttons to
the left. Sequencer1 also has buttons in first row but those are drawn
offset to the top" — then "happens everywhere RESON and RINGMOD for
example. Same types, knobs, but positions differs".

**Where:** CPP-New `master` `e47c8fb` (grainstorm2/src/screens.cpp,
layout-sheet.svg regenerated).

**Why:** two page engines with two origins. Bars, the transport bank
(`makeBank`) and PageFlow all put a cell 4dp inside their edge and its ink
4dp inside the cell — ink at (8, 8) from the content edge, 16dp ink-to-ink
between neighbours in both directions. `makeStandingBody` (every
pageStacksAlways / pageStandsFree / row-stacked page: SEQUENCER1, RESON,
GRANULATION, GRAINGEN, the whole fx and stereo sections…) opened each row
with a kBarPad spacer that the row's own kBarGap then FOLLOWED, so the first
cell sat 12dp in (ink 16), and put nothing above the first row (ink 4), with
rows flush (ink 8 apart). Measured on the desktop renders: SEQUENCER1's tiles
at x=139 y=127 against BPM's and the section column's x=131 y=131. RINGMOD
is a PageFlow page, RESON a standing one — same knobs, 8dp right, 4dp up.

**Fix** (`screens.cpp`, makeStandingBody): the body is a bar's inside —
`pad(kBarPad)` all round and `gap(kBarGap)` between rows; the lead spacers
are gone; totalPx counts the gaps and bodyH adds the pads in pixels.
Wrap arithmetic (`per`, `perT`) untouched, so no page re-wraps. Every host
container pads 0, so nothing doubles. This retires the 2026-09-01 "row gap
0 → ink 8, the bars' 8" reading: the bars' buttons are 16dp ink-to-ink, and
so is everything else in the app now.

**Verified:** SEQUENCER1 tiles, BPM tiles and the section column's buttons
all at x 131–178 / y 131–178 in the 110dpi app-window renders; GRANULATION
slabs at (131,131), rows 16px apart; LPC VOCODER (fx, standing) at x=131.
`gs2_layout_test` FAIL set byte-identical to before (160, all pre-existing).

## GS2 UI: sep() is a grouping sentinel only — no rule, no air (2026-09-04, in CPP-New)

**Patrick's word:** "use sep() only to define groups and to decide which
controls should be displayed together (this is already correctly handled).
Do not make padding decisions based on sep(). Also drop drawing of the
separator line entirely for sep()." A page with breaks must space exactly
like a page without.

**Where:** CPP-New `master` `36d70c9` (grainstorm2/src/screens.cpp,
pages.cpp comment, test/render_main.cpp, layout-sheet.svg regenerated).

**Done, both engines in `grainstorm2/src/screens.cpp`:**
- `makeStandingBody`: no `page.standing.sep*` hairline + 2×kBarPad spacers
  before a group-opening row, no 1px standing mini-rule between cells where
  a group starts mid-row (that one also cost a kBarGap each side — the
  visible sep/no-sep difference). `GridCell` lost `boundaryBefore`;
  `anyEmitted` is gone. `emitOwnLine` likewise draws nothing before a line.
- `PageFlow::finish`: no `.sep` between stacked groups, no `.vsep` between
  side-by-side sets, and the +1px they were charged in level-2 packing,
  `pageWDp` and `pageHDp` is gone with them.
- Grouping decisions UNCHANGED: own-line groups (seph/tiles/stackAll), "a
  group that cannot finish the row starts a fresh one", takeMixTail's stop
  at a break. Rules between UNITS (osc bank, fm bank, makeUnitBank) remain —
  they are not sep()-driven.

**Verified:** `gs2_layout_test` 195 → 160 FAILs, none new; the 35 that went
were cells a pixel short ("400px tall where a knob is 402") — the rule's
pixel had been shaved off them by Stack's overflow shrink. All 160 left are
pre-existing from other sessions' in-flight work (power/bypass buttons on
ARP/RESON/…, RESON cell count). Renders of GRAINFILTER and the stacked
phone page show no hairlines and one gap between and within groups.

**Side fix:** `test/render_main.cpp` did not compile — `fast` was inserted
into `ScreenRef` before `masterPage`, so the positional initializer landed
`MasterPage::Effects` on a bool; added `false,`. gs2_render's page ids have
also drifted ("68-page-pv" renders VCO2) — left alone.

**Not rebuilt:** this repo's app tree (`cmake-build-release`). Pure removal
in shared source; the test-bed tree compiled it clean. Build both before a
device run.


## GS2 UI: subspace layout sweep, honest gutters, fling fix (2026-08-31/09-01, in CPP-New)

**Where:** CPP-New `master` `c19ade1` (grainstorm2/ + tslgraphics2/ui),
pa-grainstorm2 `master` `74c3c0d` (event-time stamps in
Grainstorm2Plugin.cpp). Patrick live-tests on device + Mac app; the workflow
is HIS eyes only — edit, compile `make gs2_core gs2_gpu` in
`CPP-New/grainstorm2/build`, he rebuilds. No exploratory renders unless
debugging blind (scratchpad probe recipe: a copy of test/render_probe.cpp +
the aigen-rig link line; render_main.cpp's stub drawIcon paints BOXES, not
glyphs — don't mistake that for the app).

**The standing layout laws (all in pages.cpp predicates + screens.cpp
makeStandingBody):** groups are always horizontal; grid-flow packs groups
into shared rows and wraps (`pageStandsFree` — the fx/stereo catch-all);
`pageStacksAlways` pages give every group its own line, vertical scroll both
shapes; per-page V orders via `standingPageFor`/`pageStandsAsColumn`. ONE
ink size per window (metrics.h): buttonSideDp 54, knob dial shortest/6 clamp
88..130, colWidth = dial+52, 4dp cell insets → 8dp rhythm. NO compressed
pages except on Patrick's word. DRY before WET everywhere. emitOwnLine sizes
each wrapped row at ITS OWN cells' height (a toggle row is button-tall even
inside a knob group's line).

**The gutter rule (this session's core fix, Patrick's wording):** the scroll
bar's strip (6 ink + 24 lane + 8 glass edge on mobile) is SUBTRACTED from
the parent layout's width/height, and ONLY where a bar will actually exist.
Every packer measures at full extent first and re-packs against the gutter
only on overflow: makeStandingBody two-passes (`buildBody(0)` then
`buildBody(kScrollGutterDp)` if `fixedHDp() > heightDp`), EQ10/banks/
sequencer/vwrap solve it analytically in a `for (g : {0, gutter})` loop,
editor+display pages build the body FIRST and derive `scrolls` from it.
PageFlow keeps the thin seam lane (flow pages rarely scroll). A page that
fits fills the whole width minus its own padding.

**Bare cells:** `cellDrawsBare` = SPACEDYNEQ, ASMDEL, MDELAY1_POW — a
sub-switcher's VALUE is its label; a bare row is one button tall (that was
MDELAY's phantom "top padding": the switcher's empty name band). Bare Power
draws the standard slab+ring, square at buttonSide; bare ENUMS keep colW.

**Pages restructured to the PV/CROSS/GRAINGEN shape:** MDELAY (MODE /
MIX+GAIN / bare ASMDEL+power / transport tiles / FREEZE+BACKW / line knobs;
in `pageMixStaysInGroup` so takeMixTail can't steal MDELAY1GAIN), MULTICOMP,
DELAY (transport / HOLD+BACKW / knobs / DRY+WET LAST), PPDELAY (same).
Transport clusters are their own groups → 54dp tile rows, sorted by
transportRank; sync lit/grey runs through transportSwitchParam/
transportSyncingOf inside drawTriggerCell/drawEnumCell (the generic act::
block never runs for param tiles).

**The fling-reversal fix (device-confirmed "that worked"):** the tracker and
scroller were CORRECT; the hosts lied. Real event times on pointerMove AND
pointerUp on all three shells; no addMovement at release; Android
ACTION_POINTER_UP of the active finger ENDS the gesture at its own coords
(the synthetic-move teleport to another finger was THE bug). Hardened:
Scroller clamps `passed >= 0`, Android drains the touch ring before reading
the frame clock.

**State / next:** everything above compiles clean in both trees; MDELAY row
geometry probe-verified, the rest of this session device-verified by Patrick
EXCEPT the newest DELAY/PPDELAY tables and the app-wide conditional gutters
on less-visited pages — sweep continues page by page as he reports. Two
build trees, build BOTH (this repo's shell is hand-typed, diverges
silently). Append params before NUM_PARAMS only; v22 presets store raw ids.

## GS2 UI: track menu wired, MIDI mapping + learn (2026-08-25, in CPP-New)

**The work lives in CPP-New, branch `claude/gs2-ui-midi-mapping-s4z65a`** —
eight commits over `grainstorm2/` + `tslgraphics2/` (plus one in
`pa-grainstorm2`, same branch); this repo carries only this note. The track settings rows that were stubbed (`enabled=false`) all run
now, each one gs1's workflow behind a gs2 panel:

- **LOOP POS** — 16-slot panel per track; SAVE inline, LOAD keeps
  ApplyFromUi's fade-deps → apply-on-audio-thread → fade-up sequence, queued
  on the worker (the fade sleeps ~70ms; gs1 ate that on its UI thread).
- **SYNCING** — 4×16 switches, gs1's direct `params[track][id]` write (no
  Event/undo — routing switches). Row state is a new `MenuItem::accent`
  (green=on, refreshed per frame against the engine, behind-panel included).
- **EDITOR** — COPY..MAXIMIZE off the parameter table; acts on the track ON
  SCREEN at press time (gs1's active_track), so COPY/switch/PASTE works.
  Engine half is fadeinout copied minus InfoPanel + Waveform::setup strands.
  RECORD row stays deferred.
- **IMPORT/SAVE MIDI MAPPING** — the preset panels against the mapping store
  (MidiSaver joins the core build; `saveMidimapping`'s AlphaPopUp compiled
  out under GS2_NEW_UI). Overwrite keeps collect-write-remove order.
- **MIDI LEARN** — mode = `_STATE->midilearning`, toggled from a settings
  row; learnable cells/panel rows/waveform handles turn skcol::midilearning
  (layer keys mix the mode in); tap opens the learn dialog (MIN/MAX prefilled
  display units for CC, ++/-- checkbox for enum); the RECEIVER's learning
  gate is bridged via `gs2_midi_learn.h` under GS2_NEW_UI (the gs1 branch
  read the old MidiLearning view); save = MidiLearning::save under mutex_midi.

**Settings (same branch, `c011d58`)**: SETUP — the destination after MASTER —
is now grainstorm's Settings2 list. `gs2/settings.h` keeps its XML vocabulary,
its parse rules and its SettingsManager store
(`getStoragePath("Grainstorm")/settings.conf`), so a preference set in either
app is set in both; only the old-toolkit View is replaced. The writes keep
gs1's forms (bool 1/0, dropdown as its OPTION VALUE, slider as PERCENT) and
fire the int-change callback that makes a setting take effect;
`gs2::wireSettings` (called from the shell, pa-grainstorm2 same branch) is the
engine half and applies the stored values once at startup. The model needs
neither engine nor Skia — `test/settings_probe.sh` RUNS on a build host: 212
checks over the parser, the store round-trip, the callback forms and the row
geometry at seven viewports. gs1's "AI Sound Generation" category is left out
(its AiModelsView never came across).

**AI models (`77dca66`)**: the "AI Sound Generation" settings row and the panel
behind it — gs1's AiModelsView + modelRowAction. One row per model, name over a
live state line on a half-second beat (gs1's exact wording, orange while busy),
tap = download / resume / cancel / delete with gs1's four confirms as gs2
dialogs. Two toolkit additions it needed: `MenuItem::sub` (a row that is a name
AND a state is two facts; one 23-char label loses half) and
`DialogState::extraLabel` + `hit::Extra` (a partial download has three answers:
resume, delete, leave). Category is behind gs1's own `GS_AIGEN && !__ANDROID__`,
so a build without the rig has no row and the panel says NOT IN THIS BUILD.
**Not exercised against a real download** — no generator on this host.

**Master split (`b3e92b8`)**: the master section was a MIXER and an EFFECT
CHAIN on one page — the four tracks' POSTGAIN and the bus's hung off the bottom
of whichever effect was showing, as the page flow's last group. They are two
pages now, GAIN and EFFECTS, with two chips in the bus's own strip (where a
track's strip keeps its section buttons). Which of the two is a new
`ScreenRef::masterPage`, not a parameter — nothing in the engine has an opinion
about it — kept across a navigation the way `section` is, so leaving the bus and
coming back returns to the page you left. `Navigator::tap` now builds every
destination through one `dest()` helper (page/sub cleared, everything else
kept), and the switcher menu goes through the same shape. The gain page is a
real `PageFlow` via a new `PageFlow::addCell` (a `PageControl` cannot say which
parameter ROW it edits, and these are one parameter on five rows), so the five
faders WRAP on a phone — 2x3 at 1080x2340 — instead of scrolling sideways with
two off the edge. `masterContentDp` takes the ref: only the effects page is
charged for the page selector. **The flow's `tail` is gone** — `finish(tail)`,
the threading through `makePage`/`makePageBody`, the `under` wrapper and
`seamName` existed for that one caller. New `test/master_probe.sh`: 701 checks
at twelve viewports, RUNS on a build host (no Skia, no engine, no DSP — it
defines the six symbols it would otherwise reach, each with the reason it is
safe). Both regressions were seeded and caught; the twelfth viewport (420x600)
was swept for, since 1252 of 9240 window shapes tell the two `masterContentDp`
arithmetics apart and none of the ordinary ones does.

**Waveform readout (`e44654b`)**: gs1's InfoPanel had no counterpart in gs2 —
`POS: mm : ss : mmm  / mm : ss : mmm  LOOP: mm : ss : mmm     LOAD: NN%`, or
POWER OFF. It is now a strip above the waveform (gs1's own reading order; gs1
puts it across the whole window only because one bar serves four tracks). Every
number is the engine's: `file`/`loop` are TRACK::time_file and
TRACK::time_play_dur, which `computeLoopTime()` already maintains at all eight
call sites and which carry the plugin's LOOPSYNCDAWTRANSPORT branch —
`gs2::waveInfo` reads them, it does not derive them. Three deliberate
divergences: it reads the track's OWN sound (gs1 hops through DISTRSOURCE
because its panel is global, but this one sits over a picture); no empty prompt
(gs2's waveform already says "drop a sound here"); and it gives way on a window
where paying for it would leave less waveform than one flat control is tall
(320×480 is the whole of that case). Everything else is gs1's: the three LOOP
forms (m:s:ms → hours → years past 8760 of them), 80/100% colour thresholds,
POWER OFF/POW OFF, fixed columns measured from the widest each field can print
(POS counts ms, so a column placed after its neighbour would jitter every
frame), and the fit-the-template font sizing from `InfoPanel::init`.
`Paint::WaveInfo` is a perm view in the root pass beside the playhead, for the
playhead's reason. New `test/waveinfo_probe.sh`: 126 checks, no Skia and no
engine, both regressions seeded and caught.

**Undo says what it undid (`9d79a21`)** — gs1's InfoPanel has a second job:
it is swapped out for a one-second message whenever an event carrying
`Event::Info` is applied, which is every event an undo or redo replays
(`History::add` stamps it on what it records). gs2 had nothing. Now
`redrawEvent`'s Info branch, under `GS2_NEW_UI`, latches the event's own
`toString()` and `Controller::frameTick` raises it as a 1s toast (gs1's
duration; `showToast3`, not `showToast`, whose 44-char RT cap would cut the
value off). **A latch, not a toast per event**: one undo press replays a whole
groupId, so a message each would answer one press with five — gs1's single
overwritten buffer showed only the last too. Two slots + a counter because the
writer can be the audio thread. The row is always named (gs2 never maintains
`active_track` — only an old-toolkit button wrote it — and a toast lands over
any screen); row 4 says MASTER.

**This also closed a live crash.** DEFERRED carried `redrawEvent`'s null
InfoPanel as a known risk, safe only because nothing drains `toUiThreadQueue`.
But `apply.cpp`'s `recordingChange` case called `prepareBuffer` **directly**,
not through that queue — so **undoing a sound load dereferenced null**. Fixed,
along with `TextEvent` (whose whole purpose is the message) being posted into
the undrained queue. The nine copies in `OnChangeFunctions.cpp` stay dropped;
each is a param change `Event::apply` already described, so that is a duplicate.

Verified: all libs + gpu-side sources compile clean (clang, Linux host —
needed `pmmintrin.h` in tools.h and an inplace_function capture boxed in
queueSavePresetNamed: libstdc++ string is 32B vs libc++ 24B).
`checkTrackSettingsActions` (new, in test/main.cpp) drives everything through
real panel geometry: loop slots round-trip through the engine, syncing flips
TRACK_CONTROLS_ACTIVE, REVERSE lands a reversed loop, a mapping saves/lists/
deletes, and learn goes end-to-end with real bytes through onMidiMsg — CC
2/15 learned on a knob, CC 127 drives it to display max. **Run the suite on
the Mac (gs2_layout_test needs Skia/gpu targets); not run on this host.**

## GRAINGEN: density integrated, not sampled (2026-08-18, `4611129`)

User: "the next request to get the wanted dens is triggered by the previous …
the samplerate of the dens lookups isn't the real one." Exactly right — the
period was computed only at the previous fire, so at low densities a DENSITY
envelope (LFO / follower / wind / knob) was inaudible until the stale period
expired. Now a k-rate (64-sample) pass rescales the pending countdown by how
the algorithm's current period basis moved — the remaining FRACTION of the
interval is preserved, which is the discrete integral of the rate, i.e. the
generator now behaves like an oscillator under FM.

- Per-fire draws (jitter, swing, figure accel) survive as constant factors.
- Constant density is bit-identical (ratio 1 → no-op); rounding not
  truncation, or a smooth LFO would bias every period a fraction short.
- FIGURE/GLISS rests are absolute seconds — excluded via `_rescalable`.
- BOUNCE rescales against `(2h/vel + 0.02)` with the LIVE SPEED knob, so
  SPEED works mid-flight; FOLLOW answers the follower at k-rate now.
- Replaces the CHIMES-only "shorten on gust" hack (now two-sided and general).

Measured (`dens_rescale_test.cpp`, scratchpad): 1→100 Hz step mid-wait
answers in 5 ms (was 400 ms); a 1→100 Hz linear sweep lands 51 fires where
the rate integral says 50.5 — the OLD behavior produced exactly 1 fire in
that second; 120 s sine-LFO run shows no rounding drift (121 fires ≈ 120
expected); constant-density fire times byte-identical with the path on/off.

## GRAINGEN figuration algorithms: CHIMES / FIGURE / GLISS (2026-08-18, uncommitted)

Three new GRAINGEN algorithms (algs 5–7, panels SPACE_GRAINGEN6–8 — those enum
slots already existed as reserves, nothing shifted). Unlike the rejected
CLOUD/EUCLID/CHAOS round, these write **correlated sequences** into the
per-grain pitch/gain/size buffers — figures with memory, not per-grain dice
(which LFO-RND and RANDOM PITCH already cover). All three compose with the
16-step sequencer (multipliers on top of its per-step values) and follow
DENSITY, including its LFO.

- **CHIMES** — wind-chime model. One wind field (the SPLINE class reused, 0..1,
  cps around GUST) drives strike rate (`dens * wind^3`), strike energy, tube
  choice (walk between neighbours in a lull, jump anywhere in a gust) and grain
  size (0.6–1.0, soft strikes ring shorter) together. WIND knob bends the
  weather distribution via a power curve. K-rate branch also SHORTENS the
  pending count when a gust arrives so lulls aren't sticky.
- **FIGURE** — whole pentatonic gestures at DENSITY note rate: RUN / ARP /
  ZIGZAG (a +a/−b lace) / CASCADE / ANY, folded into RANGE by reflection, accel
  straddling 1, run/arp/cascade fade out (1 − 0.72u²), zigzag arches, REST
  between figures. NOTES/RANGE/REST knobs.
- **GLISS** — phase-continuous sweeps across grains: WANDER / UP / DOWN / ARCH
  / FALL / RISSET UP / RISSET DN. Jump modes get raised-cos edge fades.
  **Risset reworked (`90ff828`) after the user heard "a repeating rise, not
  endless"**: a single bell-weighted voice can never make the illusion — it IS
  fade-in/rise/fade-out on repeat. Now successive grains go round-robin to
  **3 octave-spaced components** a third of the journey apart in a fixed
  3-octave window; sin² bells sum to a constant 3/2 (no loudness pumping) and
  every wrap sits at zero gain. For Risset, RANGE = rise speed (semis per
  TIME), window fixed. RANGE/TIME/REST.

Files: `graingenerator.{h,cpp}` (per-channel state structs; the neutral
`sizeBuf/gainBuf/pitchBuf = 1` writes are now hoisted BEFORE the switch so
algorithms can overwrite them), `types_grainstorm.h` (11 params before
NUM_PARAMETERS), `gs_common.h` (3 mode strings + 2 enum-name arrays),
`ParameterInit.cpp`, `gui/space_graingen.cpp`.

Panel layout, second pass (user: SHAPE selector was invisible): each panel is
a COLUMN (HorizontalLayout — the swapped-name trap in the layout memory), so
the selector is now its own `6.5, PERCENTAGE_FROM_MAIN_WINDOW` row — the same
recipe as the ALGORITHM selector's 2026-08-18 fix — with all three knobs in
ONE knob_height_total row below it (was: two knob rows + a WRAP selector row
that over-subscribed the column and never got space). Third pass: 1.25%
separator View between selector and knob row.

Live-testing fixes, same day:
- **Algorithm switch left a channel silent** (user hit GLISS → FOLLOW): the
  new algorithms park `_count` for up to 60 s (rest/lull); the switch branch
  now zeroes `_count` on every algorithm change so the new one fires at once.
- **BOUNCE HOLD precedence flipped — HOLD now wins over LOOP.** Sim
  (`bounce_hold_sim.cpp`, scratchpad) showed the 08-15 "LOOP wins" choice made
  HOLD bit-identical to LOOP alone (96 vs 98 fires/5 min), i.e. inaudible from
  any looping patch — the likely "hold not working" report. HOLD+LOOP-off was
  already correct (~20 Hz roll after settling) but note the defaults take
  30–60 s to reach the floor (BOUNDA 10 m at SPEED 1 m/s — physics, not a
  bug). Old presets with BOTH boxes checked now hold instead of looping.

Verified by a standalone kernel property harness (scratchpad
`graingen_figuration_test.cpp`, all corners of every param): bounds, liveness,
continuity. It caught one real bug before first run: **RISSET phase advanced
more than 1 per grain when TIME < one grain period** (T 0.1 s at DENSITY 1 Hz)
— a single `phase -= 1` never caught up and pitch pinned at the ±3-oct clamp;
fixed by stripping the whole integer part. Grain-size multipliers stay ≤ 1.0
by design — there is NO upper clamp after the seqsize multiply in granulate,
>1 would read past the grain buffer (sequencer SIZE steps stop at 1 for the
same reason).

macOS app builds green (cmake-build-debug, Grainstorm-app). **NOT ear-verified,
not committed** (CPP-New also carries older unrelated uncommitted work).

## RECORD AI->TRACK (2026-08-17, CPP-New `ce6455a`)

Track settings → RECORD AI->TRACK: prompt popup → Stable Audio 3 Small runs
locally (audio.cpp/ggml/Metal) on the RecordingQueue → result resampled
44.1→48k → pushed to the active track via the snapShot worker (mic-stop
sequence). `sfx:` prompt prefix switches to the SFX model. Model stays
resident (~2 GB) between prompts; ~1.5–4 s per 10 s on the M4.

- Feature code: `CPP-New/grainstorm/aigen.{h,cpp}`; menu in `TrackSettings.cpp`.
- GS_AIGEN auto-enables in `grainstorm/CMakeLists.txt` (macOS only) when
  `~/programming/grain-prompt-test` has built libs; models live in that rig's
  `models/` dir (`GS_AIGEN_MODELS_ROOT` / `GS_AIGEN_SECONDS` env overrides).
- ⚠ SIGABRT-on-quit trap, fixed and reproduced both ways in
  `grain-prompt-test/exit_test.cpp`: ggml's Metal device static is created
  LAZILY (during session/first run) and shares one LIFO with atexit — the
  hook that frees the resident session must be registered AFTER the first
  `run()`, else ggml tears down first and `ggml_metal_rsets_free` asserts.
- UI flow verified on device (menu → "piano" → waveform looping on track 1).
  AU targets pick GS_AIGEN up on their own rebuild — auval/soak not yet run
  with it; iOS unaffected (stub).
- **Android port (CPP-New `fc04a7e`, android repo `9471b65`)**: CPU backend
  + mem_saver, models dir via JNI `MainActivity.getAiModelsDir()`
  → `Android/data/me.rocks.grainstorm/files/aigen-models/` (adb-pushable).
  Engine cross-compiled at `grain-prompt-test/build-android/` (NDK 28,
  android-24, c++_shared, dotprod+fp16).

### Android: verified on device, tuned, progress + cancel (CPP-New `401a51a`, android `381060d`, rig `60f1a9b`)

Runs on a Redmi 12 (Helio G88, 2× A75 + 6× A55, Mali-G52 MC2, 3.7 GB RAM).
**10 s clip: 92.9 s → 49.9 s (1.86×)**, no quality change, via two settings:

- `threads` now follows `hardware_concurrency`; it keeps scaling to all eight
  cores despite six being little (2/4/6/8 thr = 116 / 92.9 / 80.4 / 69.4 s).
- `duration_padding_seconds` 6 → 1. The engine default makes a 10 s request
  generate 16 s and truncate, and on CPU **cost is linear in generated length**
  (30 s = 228.7 s), unlike Metal where it looked flat. It buys nothing: the
  model is conditioned on `request.durations_seconds` (conditioner.cpp:36), so
  it composes its ending at the *requested* duration and the clip fades at 10 s
  regardless — fixed-seed A/B, −11.6 dB at padding 1 vs −11.7 dB at 6. To
  actually avoid the fade, request longer and truncate yourself.
- `mem_saver` must stay ON (off = 134.7 s and 344k major faults).
- Steps 8→4 is a further 1.70× but is a quality decision, not a free win.

**⚠ Two release-only bugs, both silent, both fixed:**

1. **R8 stripped `MainActivity.getAiModelsDir()`** — called only via
   `GetStaticMethodID`, so it needs its own line in the MainActivity keep block.
   Every minified build reported "AI models not found" with the engine fully
   linked and working. This was the real cause of the feature looking dead on
   device; debug builds hide it. Same class of bug as the audioBufIndex note
   already in `proguard-rules.pro`.
2. **Progress/cancel hook must not be an `inline thread_local` in a header.**
   The engine archive and the app are built by different CMake projects and
   ended up with separate instances in release builds only — progress froze at
   "READING PROMPT" and **cancel did nothing** while generation ran on. Define
   it in exactly one TU inside the engine.

**Progress dialog**: a `FloatingView` subclass, so its corner close *is* the
cancel (hooks `delRecursive*`, no tslgraphics edit). Shows phase + step k/8 +
elapsed. Cancel is real, not deferred: `ggml_backend_cpu_set_abort_callback`
cuts mid-step and CPU drops 466% → 62% within 3 s (measured 160 ms to unwind in
the harness). A second RECORD AI->TRACK while busy is refused with a toast.
Needs `grain-prompt-test/grainstorm-run-control.patch` on the audio.cpp clone.

**GPU: measured, not worth it here.** ggml-vulkan *does* run on the Mali-G52
(test-backend-ops passes), but q8_0 matmul is 4.33 GFLOPS vs the CPU's 8.75 —
CPU wins on every type. audio.cpp's own graph also aborts at
ggml-vulkan.cpp:6805 (descriptor-set accounting). Keep the recipe for phones
with real GPUs, and note ggml `abort()`s on failure, so "try Vulkan, fall back"
cannot be done in-process — it needs a crash-marker file like browser GPU
blacklists.

**`assembleStaging` works** (release-optimized + localTest key → installs over
an existing install with no data loss). The old "~/patchhash missing" note is
stale: that gradle block is commented out. Engine archives are prebuilt
**Release** regardless of gradle variant, so debug-vs-release barely moves
generation speed — it only changes the app around it.

### SFX default, m: prefix, model downloads (2026-08-17 evening)

- **"Prompts ignored" on staging was NOT a regression** — 8-seed sweep showed
  identical distributions across the padding/thread changes, and three audits
  cleared the prompt path. It's model variance with terse prompts (CFG off).
  Only descriptive prompts pass Patrick's ear test. The progress dialog now
  shows the prompt verbatim as the engine receives it.
- **SFX is the default model** (better grain fodder; the music model's riffs
  ear-tested poorly). **`m:` prefix** switches to music; `m;`/`sfx;`/`sfx:`
  silently accepted for hardware-keyboard typos. Prompt title says so.
- **The on-screen keyboards type `:` on every layout and have NO `;` at all**
  (user call: nothing needs semicolons, the AI prefixes need colons).
  Implemented as sentinel vkey 0x100 (0xFE is VKEY_OEM_CLEAR) translated
  directly, so hardware ';' keys are untouched. Compact numbers page also got
  honest shift labels + a caps key (tslgraphics edits, user-approved).
- **Model downloads shipped (Android)**: Settings → AI Sound Generation, one
  entry per model with live state. Java owns the whole flow (MainActivity
  `aiModelClick` + DownloadManager + sha256 verify + rename;
  MainSettingFragment wires taps/summaries) because Android settings is the
  Java preferences screen — NOT the in-app Skia dialog (first attempt landed
  there; it never shows on Android). Files come from HF commit-pinned URLs
  (free CDN, resumable; sha256 matches x-linked-etag AND local computation).
  Model table lives in BOTH MainActivity.AI_MODELS and aigen.cpp kModels —
  keep in sync. R8 keeps added for start/query methods.
- ⚠ **DownloadManager preallocates the destination at full size the moment
  the transfer starts** — a size-based "is it done" check is fooled instantly,
  and (on MIUI at least) writes are segmented, so a tail-bytes check lies too.
  Only DM's own STATUS_SUCCESSFUL (or a full sha256) means complete. The
  app-side flow does this right (verify only fires on status success); my
  first two shell watchers did not.
- **Download E2E PASSED on device**: enqueue → notification → segmented
  download → DM success → in-app sha256 verify → atomic move into the model
  dir; the installed file hashed byte-identical to the pinned upstream. Tap on
  an installed model offers DELETE (confirm dialog, frees 1.7 GB, staged file
  cleared too); summaries show download size up front; the category leads with
  the runs-locally/private/unlimited explainer (no model named in-app — the
  license-required "Powered by Stability AI" credit goes on the website OSS
  page, still TODO).

- **Length slider (2026-08-18, CPP-New `b7f95f0`, android `3dab2c1`)**: log
  1–60 s slider (default 10, remembered across dialogs) in the AI prompt
  popup. Composed as a `DurationSlider` childView_ on AlphaPopUp exactly the
  SavePresetView way — subclass routes pointer events to the child FIRST
  (its box lies outside the popup rect, where the base class reads a tap as
  dismiss), pointer latched on DOWN so drags survive leaving the box.
  tslgraphics untouched. 40 ms raised-cosine declick on the clip end fixes
  the abrupt cap heard at 10 s. Long probes (Metal, fixed seed): music holds
  45 s at level; SFX holds 60 s for continuous textures (factory drone
  −18.9 dBFS flat) but narrative prompts compose a scene ending early (rain
  → −53 dB by 30 s) — model behaviour, not a limit. CPU cost stays linear
  (~5 s per generated second on the Redmi), settings copy says so now.
- **Manifest query carries `os=android`** (android `f331381`, forum
  `3463caa`, deployed): future endpoint can gate by version AND platform;
  a request without `os` is an older Android build.

### Settings2 — gs-style settings dialog (2026-08-18)

- **`tsl::graphics::Settings2`** (`graphics/public_headers/settings2.h` +
  `graphics/src/settings2.cpp`, added to graphics CMakeLists) replaces the
  Material-styled `Settings` for grainstorm: same XML vocabulary
  (bool/dropdown/slider/action/url), same `SettingsManager` persistence +
  int-change callback, same public API (`loadFromXml`, `getByKey→setAction`,
  `show/hide/showBlocking`), same full-screen letterbox-covering own window.
  Old class left untouched — **PocketAnalog still instantiates `Settings`**
  (va/callbacks_loop_controls.cpp); switch it the same way once the gs look
  is approved.
- **gs design language**: FloatingView-style bold title bar + round-cap X;
  checkbox = stroked square + X mark (colours/caps synced with grainstorm
  `CheckBoxView::render`); dropdowns show the current value + a
  DropDownView-style ▾ triangle, right-aligned — tap opens a real
  `RecyclerView` option popup (active option in `skcol::active`);
  `ScrollViewBase` scroll with fling/overscroll glow/fading 2·lw scrollbar;
  ESC closes (view takes `hasFocus`). Text: items `textsize2*.9`, section
  titles `*.8` bold (deliberately a bit smaller, per request), descriptions
  `*.7` `lighter_grey`.
- **First design review (2026-08-18):** overall look approved. Two
  corrections, both applied: **no `‹ ›` cycle arrows** on settings choices —
  "it's settings, NOT something you switch through like an effect param";
  and the **press highlight must hug the actionable thing** (checkbox box /
  value+▾ tag / action name), never the whole row incl. description. A
  temporary "Scroll Test TEMP" category (12+ filler prefs, loudly commented)
  is inserted in ApplyFromUi's XML so overflow scrolling and the popup's own
  scrolling can be exercised — **REMOVE after sign-off**. Raw-string trap
  hit there: `(temp)"` inside `R"(...)"` ends the literal — no `)"` inside
  plain raw strings.
- **Popup wiring quirk**: `RecyclerView`'s row views are constructed
  internally, so the option rows reach their `Settings2` through
  `OptionRow::owner` (inline static atomic) — fine because there is one
  settings view per app; a `parent` pointer would have hijacked
  RecyclerView's parent-relative popup placement.
- gs switched in `grainstorm/Event/ApplyFromUi.cpp` (SETTINGSBUTTON case:
  include + two type names; Android path untouched — Java settings).
  `settingsView` slot is `AtomicSharedPtr<void>`, so no type change there.
- **Built clean**: `cmake-build-release` → Grainstorm-app, `settings2.cpp.o`
  in `libtslgraphics.a`, 129 `Settings2` symbols in the shipped binary.
  **Runtime/visual check still pending** — Patrick declined screen-driving,
  so the look has not been eyeballed; open Settings (gear) in the app from
  `~/Applications/Grainstorm.app`. Uncommitted in CPP-New.

### Spectral Filter — PHASE-0 clicking found and fixed (2026-08-18, CPP-New `04c96f8`)

- **Root cause**: MAG LP decouples level from content, and the FTRACK
  confidence gate only truly holds against *digital* silence —
  `w = magIn/(yn·0.02)` is full tracking at −34 dB input-to-output, half at
  −40, 5% at −60. Real between-hit residue (hats, tails, floor) lives exactly
  there, so it steered still-loud smeared bins in raw 93.75 Hz frequency steps
  of up to ±46.9 Hz at PHASE 0. That stepping was the clicking; the gated-tone
  harness never saw it because its gaps are digitally silent.
- **Fix (`SPECFILT_FCOUPLE 4.0`)**: frequency-path cutoff capped at 4× the
  magnitude path's — the user's manual PHASE-up remedy applied automatically,
  scaling with MAG; bit-identical at MAG 0. **Ear-verified: "clicking is
  gone."** Consequence: PHASE knob is a no-op below the floor while MAG is up
  (MAG 0.6 → floor ≈ effective PHASE 0.38). `SPECFILT_FCOUPLE` is the single
  dial — raise toward 6–8 for more PHASE range at the cost of click margin.
- **Tried and REJECTED by ear**: absolute frequency hold below −46 dB (dead
  zone in the gate ramp). Verdict: "wobbling sound when both mag and phase are
  high, also reduces their overall effect" — hard-frozen bins sit pinned a few
  Hz apart and beat *periodically*, and the junk-walk's slow diffusion is
  audibly part of the high-MAG/PHASE sound. The walk is load-bearing; don't
  freeze it. Reverted same day; comment in `pv.cpp` records it.

## Layout & build

- Build project (cwd): `/Users/patrickropohl/programming/grainstorm_iplug2`
- **Engine source lives elsewhere**: `/Users/patrickropohl/programming/CPP-New/grainstorm`
  (shared graphics/framework lib: `/Users/patrickropohl/programming/CPP-New/graphics`).
  `CPP-New` is its own git repo — that is where all edits below live.
- Build (ninja is not on PATH):
  ```bash
  cd /Users/patrickropohl/programming/grainstorm_iplug2/cmake-build-debug && /Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja Grainstorm-app
  ```
  Single object: `… ninja grainstorm/CMakeFiles/grainstorm.dir/pv.cpp.o`.
  `IS_MULTITHREADED` is defined in this build. AU/VST are separate targets and
  need their own rebuild to pick up engine changes.

## State

**Committed** (`aa301d7`, in CPP-New): the PV + cross-synthesis overhaul —
bug fixes across `pv.cpp/pv.h`, `cross_lpc` rewritten to a continuous
reflection-coefficient lattice, source-RMS grain normalizer, vocoder per-band
followers, dead code removed. **Ear-verified good by the user.**

**Committed** (`0eb4ac7`, in CPP-New): a mixed commit — the user's own PV work
(`pv.cpp` +783, `ParameterInit.cpp`, `gui.cpp`, `track.h`, …) **plus** the
cross-sync fixes **plus** the thread-priority work. The priority work was then
found to deadlock and is reverted in the working tree (below).

### ⚠️ CrossBarrier deadlock (2026-07-25) — cause and fix

The `CrossBarrier` version in `0eb4ac7` **deadlocks with cross-synthesis on**.
It parked the consumer on a separate `std::atomic_flag` that
`loop_continue`/`loop_exit` set as an *edge*. Edges can be erased:

1. Carrier ends buffer N: `loop_exit()` sets `_count = 1000000`, sets the flag.
2. The modulator worker has not consumed that edge yet.
3. Audio thread starts buffer N+1 → `setupTracks` → `cross_barrier[i].reset()`
   (`synth.cpp:370`) → **`_flag.clear()` erases the edge**, `_count` back to 0.
4. Modulator calls `_flag.wait(false)` → parks forever.
5. It never reaches `workerLatch.done()`, so `WorkerLatch::wait()` — an
   unbounded spin **on the audio thread** — never returns. Whole app frozen.

**Rule: never park on an edge when the state you care about is a value.**
Fixed by waiting on `_count` itself (`_count.wait(c)` on the exact value just
loaded). No lost-wakeup window exists: an update that lands before the park
simply makes the value differ, and `wait` returns at once. `_flag` is gone.
Every writer notifies — **including `reset()`**, which otherwise leaves a
consumer parked on the previous buffer's `1000000`.

Note this also explains why the failure is a hard freeze rather than a glitch:
the audio thread's latch spin has no timeout, so one parked modulator hangs
everything.

Cross-sync fixes (in `0eb4ac7`; the glitch fix held, but see the deadlock above):

| File | Change |
|---|---|
| `grainstorm/granulate_fft.cpp` | non-MT modulator read-position fix (see below) |
| `grainstorm/synth.cpp` | FFT plan pre-warm moved to its own pass, now covers modulators too |
| `graphics/public_headers/tools/AudioSemaphore.h` | `CrossBarrier` consumer blocks instead of spinning |

Not attributed between the two candidate causes (blocking `CrossBarrier` vs.
modulator FFT pre-warm) — both changed together, neither reverted to bisect.

**Uncommitted**: the revert of the priority work — deletes
`graphics/src/tools/threadtsl.cpp`, restores `threadtsl.h` and
`graphics/CMakeLists.txt`, restores the `WorkerSemaphore` body. Builds clean.

### Thread-priority work — reinstated after the spin fix (2026-07-25)

`setPriority` had its whole body inside `#ifdef __ANDROID__`, so on macOS/Windows
worker threads had **never** been boosted, while the audio thread is RT-promoted
by the host and waits on them. Now implemented for Darwin + Windows; Android
carried over byte-identical (its real boost is the Java
`Process.setThreadPriority` path in `app.cpp`, not this function).

It was reverted once mid-session on a **wrong diagnosis** — the freeze was the
`CrossBarrier` edge bug above, not the promotion. Reinstated, but only *after*
the prerequisite below, because the hazard was real even if it was not the cause.

**Prerequisite, done first: `AudioSemaphore::wait()` is now spin-then-block**
(2048-iteration spin, then a value-based `count_.wait(c)`; `signal()` notifies).
It was an unbounded pure spin, i.e. a core burned for as long as the producer
takes, unpreemptible once the thread is boosted. That is why promotion must
never come first. `WorkerLatch::wait()` is **still** an unbounded spin, but it
runs on the audio thread, which the host already promotes — the priority change
does not touch it.

Order for any future work of this kind: **bound the spins, verify, then promote.**

Verified linked (it was previously a header no-op that compiled to nothing):
`nm` shows `T tsl::Thread::setPriority(int)` and an undefined
`_pthread_setschedparam` in the built binary.

Findings worth keeping from the attempt:
- `startThread(-19)` passes an **Android nice value**, not the `Thread::` enum —
  read as an enum it means "below normal", the opposite of intent. Any mapping
  needs `<= -16`→Realtime, `<0`→High, `0..3`→enum, positive→Low.
- **macOS reports `SCHED_OTHER` as 15..47, default 31** — *not* `0..0` like
  Linux. The Android branch's literal `sched_priority = 0` is accepted without
  error and pins the thread *below everything*. Darwin values must be
  range-relative. (`SCHED_RR` 39 was verified to set and read back, rc=0, no
  privileges — the mechanism works; the consequences are the problem.)
- **`WorkerSemaphore` is dead code** — declared in `AudioSemaphore.h`, never
  instantiated. Workers use `tsl::BinarySemaphore` (`ChannelThread::sem`,
  `tools.h`), which blocks properly, so idle workers were never the issue. Its
  100 ms spin deadline is harmless today, but it *would* spin ~continuously if
  used: `count_` settles at 0 (not −1) after the first buffer, so its "cold"
  precheck stops skipping the spin.

## Cross-synthesis model (needed to read the above)

Ring: `destinationz` = next track, `lockerz` = previous. For cross owner A the
modulator is `A.destinationz`. A track runs `compute_fft` (`granulate_fft.cpp` —
**modulator analysis only**, fills `fft_out`, produces no audio) iff
`lockerz->crossPowerPre`; the carrier runs `granulate` and its cross function
(in `pv.cpp`) reads `destinationz->fft_out`.
The `crossfunc` assigned at `granulate_fft.cpp:48` is **dead** — the real call is
in `granulate.cpp` (~line 832).

- **MT**: carrier feeds each grain via `CrossBarrier::loop_continue`, ends the
  buffer with `loop_exit`; modulator does `wait_rt → process → sem_cross.signal`
  per grain; carrier consumes with `sem_cross.wait()` once per grain (silent
  grains in `granulate.cpp`, non-silent inside the cross function). Balanced —
  all 7 cross fns pair signal/wait.
- **non-MT**: no barrier; carrier calls `compute_fft(destinationz, ch, false)`
  inline per grain. Main loop skips modulators (`synth_func_tmp != compute_fft`).

## The two open bugs and what was done

1. **non-MT modulator read position wrong (~2×/stalling).** Cause: the
   `state->offset` write was gated behind the `source->updated_this_cycle` CAS,
   which fires only on the *first* grain — but non-MT runs `compute_fft` once per
   grain, so only grain 1's offset persisted and `setupTracks` reloaded that each
   buffer. Fix: in the `!isMultiThreaded` branch, store the offset every grain
   (last wins), independent of the CAS. MT was already correct (block runs once
   after the grain loop).
2. **MT glitches / "sync broken".** `CrossBarrier::wait_rt` was a pure
   `cpu_relax()` spin (the pre-refactor version blocked on a futex), and
   `AudioSemaphore::wait()` is *also* a pure spin (`signal()` only does
   `fetch_add`; the OS handle is used only by the unused `*Slow` variants). Both
   sides spinning ⇒ the consumer starves the thread it waits on; worse, the first
   active track's ch0 runs on the **audio thread** and can be a modulator, so it
   spins to the buffer deadline. Fix: bounded spin (512) then block on an inline
   `atomic_flag`; restored `loop_exit`'s `_count.store(1000000)` (releases the
   consumer regardless of count drift). Also fixed: modulator FFT plans were
   never pre-warmed (pre-warm keyed off `cross_power_tmp`, false on a modulator),
   so it hit `make_unique<FFT>` on a worker thread.

**Both resolved (2026-07-24), user-confirmed by ear.** MT glitch-free, non-MT
modulator read position correct, and no regression after the thread-priority
change went in. Caveats to carry forward:

- The MT fix was **never bisected** — blocking `CrossBarrier` and the modulator
  FFT pre-warm were changed together, so which one actually fixed it is unknown.
  Relevant if MT ever misbehaves again.
- `AudioSemaphore::wait()` (the `sem_cross` side) is still a pure spin,
  deliberately left alone — it is used elsewhere, so the blast radius is wider.
  It is the first candidate if MT glitches return.

**Next step: none queued.** The 6 files are verified and ready to commit
(not yet committed as of this writing).

## Rules / do-not-repeat (each of these cost a wrong turn)

- **`fileoffset += playbackspeed * playbackspeed_dir * step_length` in
  `granulate_fft.cpp` is CORRECT.** The modulator advances one hop per grain;
  that is why `step_length` is passed through the barrier. I removed it once —
  the modulator crawled. Do not touch.
- **Do NOT smooth the cross grain normalizer across grains.** `normalize_grain`
  (`pv.cpp`) must stay exact per-grain: the gain spikes on a near-silent grain,
  and smoothing carries that spike onto the next full-level grain → clicks.
- **Normalizer matches RMS, not peak**, against `track->srcRmsTmp[ch]` (measured
  in `granulate.cpp` just before `crossfunc`). Peak-matching left algorithms tens
  of dB apart (crest factor varies hugely); an earlier `maxTmp_src*maxTmp_mod`
  target evaluated to 0 and muted LPC entirely. Trim knobs:
  `CROSS_LEVEL_TRIM`, `CROSS_CONV_TRIM`, `CROSS_CREST_MAX`.
- **`freqwarp` keeps native phase** (`outbuf[i].i = inbuf[i].i`) by design —
  carrying phase across a bin shift would need PV phase propagation. Not a bug.
- **PV/cross use a fixed Hann window on purpose** — the user does *not* want the
  user-selectable grain envelope wired into the spectral path. Leave the
  `!cross_power && !pv_power` gate in `granulate.cpp` alone.
- **`CPP-New/grainstorm` and `CPP-New/graphics` are shared libraries** — do not
  edit without asking. `AudioSemaphore.h` and `threadtsl.*` were edited only on
  explicit request. Note `WorkerLatch` is also used by `ModalReverb.cpp`
  (`reset(4)` at two call sites), so its blast radius is wider than the synth.
- **`WorkerLatch::wait()` is still an unbounded spin, deliberately.** Unlike the
  cross barrier it sits in no ping-pong cycle — the audio thread runs its own
  slice first, then waits — so it does not starve what it waits on. If it ever
  is changed, do **not** copy `CrossBarrier`'s `kSpin = 512`: the latch waits out
  a real slice of DSP, so a short spin would hit the futex every buffer and add
  wake latency where there is currently none. It needs a *much* longer spin.
- **`WorkerSemaphore::signal()` does `store(1)`, not `fetch_add`** — two signals
  before a wait collapse into one. Latent: a worker that ever misses a buffer
  loses a wake and `WorkerLatch` then never reaches 0 (audio thread hangs).
  Untouched so far; the "at most one pending slice" semantics look deliberate.
- **`CROSS_LPC` genuinely needs the 2N transform**, same as `CROSS_CONV`:
  `LPC2::computeReflection` (`vocoder.cpp`) gets the autocorrelation via a
  zero-padded 2N FFT, because a length-N transform yields the *circular*
  autocorrelation. Carrier side only reaches it when `CROSSLPCW` (whitening) is
  on; the modulator side always does. `needsDouble` in `synth.cpp` is correct.
- My first-principles reasoning about levels and rates was wrong repeatedly here.
  Prefer `git log`/`git show <commit>^:path` against the last-known-good version
  and the user's empirical reports over re-deriving the model.
