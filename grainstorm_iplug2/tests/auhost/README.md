# Grainstorm AU state harness

An in-process AUv2 host that drives the plugin from all four state-change
sources and checks, after every step, that the three views of the state agree:

| view     | where it comes from            | read via                    |
|----------|--------------------------------|-----------------------------|
| `engine` | what the DSP is running on     | `Event::getCurrentValue()`  |
| `snap`   | what a save would serialize    | `Snapshot::events_`         |
| `host`   | what the DAW reads back        | `IParam` / `AudioUnitGetParameter` |

The four sources:

1. **host automation** — `AudioUnitSetParameter` → `OnParamChange(kHost)`
2. **plugin UI** — `Event::apply(FromUi)` / `applyFromExt(FromUi)`, the exact
   calls `slider.cpp`, `checkbox.cpp` and `button.cpp` make
3. **snapshot worker** — undo / redo, queued the way the UNDO/REDO buttons queue
4. **host state restore** — `SetProperty(ClassInfo)` → `UnserializeState`

A render thread runs throughout, because events flagged `ToAudioThread` (fx
power on/off, track enable) only apply inside `ProcessBlock`. The main thread
pumps the CFRunLoop, because iPlug2's idle timer — which drains
`mParamChangesToHost` back to the host — is a `CFRunLoopTimer` on the *main* run
loop. Without either, results are meaningless.

## Building and running

The plugin must be built with the test hooks, into its own tree so it can never
be mistaken for a shippable artifact:

```bash
cmake -S . -B cmake-build-autest -G Ninja -DCMAKE_MAKE_PROGRAM=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja -DBUILD_TARGET=auv2 -DCMAKE_BUILD_TYPE=Release -DGS_TEST_HOOKS=ON -DIPLUG_DEPLOY_PLUGINS=OFF
```

```bash
cmake --build cmake-build-autest --target Grainstorm-au
```

```bash
tests/auhost/build.sh
```

```bash
./tests/auhost/auhost --bundle cmake-build-autest/out/Grainstorm.component --soak 12
```

Options: `--bundle <path.component>`, `--dump-dir <dir>` (writes
`baseline-state.json` and `final-state.json`), `--soak <n>` (0 disables).
Exit status is non-zero if any check failed.

`-DGS_TEST_HOOKS=ON` also suppresses the post-build copy to `~/out`, and
`-DIPLUG_DEPLOY_PLUGINS=OFF` stops iPlug2 from installing the bundle over the
real `Grainstorm.component` in `~/Library/Audio/Plug-Ins/Components`. Keep both.

## How the plugin is loaded

`dlopen` on the bundle executable, then the factory's
`AudioComponentPlugInInterface`: `Open` → `Lookup(selector)` → call the returned
method. That is what the AudioUnit component manager does, so every
`IPlugAU::AUMethod*` entry point a DAW reaches is exercised. It avoids
registering a second component with the same type/subtype/manufacturer as the
shipping build, and the same `dlopen` handle yields the `gsTest*` hooks.

What it does **not** cover: the AudioComponentRegistrar / Info.plist layer, and
AUv3 (out-of-process). The core state machinery is shared with AUv3, the wrapper
is not.

## The hooks

`TestHooks.cpp` in the repo root, compiled only under `-DGS_TEST_HOOKS=ON`, with
the ABI in `TestHooks.h`. No shared library (`CPP-New/grainstorm`,
`CPP-New/graphics`) is touched. They must be marked
`__attribute__((visibility("default"), used))` — the plugin target builds with
`-fvisibility=hidden -flto`, so without it they compile but never reach the
export table.

Known limitation: the hooks are plain C symbols with no instance argument, so
they address the **most recently constructed live instance**. Scenario 8 opens a
second instance and relies on this; if you add scenarios that interleave two
live instances, add an explicit selector first.

## Reading the output

The baseline scenario records every parameter that is *already* incoherent at
load and suppresses it afterwards, so later scenarios report only new
divergence. Lines read:

```
after undo: snapshot == engine == host across all tracks [31 snapshot + 31 host pre-existing at load, suppressed]
```

The soak re-baselines at its own start for the same reason — otherwise it just
re-reports whatever scenario 7 left behind, once per iteration.

A `gsTestSync did not reach quiescence` failure is worth reading closely: mask
bit 2 means the audio thread never ran (so nothing `ToAudioThread` could apply)
and bit 8 means `isRestoringState` stayed true, which gates `OnParamChange` and
freezes `SerializeState` on the old snapshot.

## Coverage

| source | scenario | driven by |
|---|---|---|
| host automation | 2, 3 | `AudioUnitSetParameter` |
| plugin UI | 4 | `Event::apply(FromUi)` |
| undo / redo | 5 | snapshot worker |
| audio-thread-only work | 6 | fx power, both sides |
| host state save/restore | 7, 8, 12 | `ClassInfo` |
| MIDI, unmapped | 9 | `MusicDeviceMIDIEvent` |
| audio file load | 11, 12 | `filebrowsercallback` |
| in-app presets, same track | 13 | `savePresetGotFileName` / `loadPresetThreadFunc` |
| in-app presets, **cross track** | 15 | save on track 2, load onto track 0 |
| MIDI-learned CC | 14 | `midicontrolevents` + real CC |
| all of the above, mixed | 10 | randomized soak |

Audio needs `--audio <file>` (repeatable); without it scenarios 11–12 report a
skip and the soak omits its audio and preset steps. WAV, MP3 and FLAC all decode.

Not covered: `FxOrder` events, AUv3 (the core is shared, the wrapper is not), and
the AudioComponentRegistrar layer.

## Status

All 93 checks pass. Any failure is a regression.

## Fixed: the MDELAY1DEL snapshot loss

Was: a host `ClassInfo` save/restore dropped the snapshot event for
`MDELAY1DEL` while leaving the engine and the host's IParam correct, so the
*next* save wrote the tap out as its 1000 ms default. Silent data loss.

Root cause, two lines in `ParameterInit.cpp`:

```c
_STATE->parameters[SPACE_GRAINENV1].flags |= Param::NoAssignment;
_STATE->parameters[SPACE_GRAINENV2].flags |= Param::NoAssignment;
```

`SPACE_GRAINENV1/2` are `_space_windows` members with values 0 and 1, not
`ParameterNum`. `parameters[]` is indexed by `ParameterNum`, so those lines
stamped `NoAssignment` onto `parameters[0]` (`PARAM_NOT_ASSIGNED`) and
`parameters[1]` (`MDELAY1DEL`). Grain-env spaces are windows, not parameters —
the real parameter is `GRAINENVSPACE`, flagged correctly elsewhere.

The chain from there: `TRACK::reset()` deliberately skips `NoAssignment` params,
so a restore left the tap holding its old value. The snapshot event then applied
a value the engine already had; `Event::apply` only calls `onParamChange` when
the value actually changes, so `Snapshot::addEvent` never ran and the event
vanished from `events_`.

The design is sound and worth stating, because it is what made the bug findable:
the snapshot stores only non-default values, since a restore resets every track
to init and then applies events. That contract holds precisely as long as reset
really does return every param to its init value. `NoAssignment` params are
exempt from reset by intent, so a stray `NoAssignment` flag silently exempts a
parameter from being persisted at all.

How it was found: soak `action 77` is the `getClassInfo`/`setClassInfo` branch;
`--soak 16` was clean and `--soak 17` broken, and the two `final-state.json`
dumps differed in exactly one field (`snap` 36.928 -> `null`, engine and host
unchanged). Temporary logging in `addEvent`/`publish_`/`deserialize`/`getEvents`
showed the event present in the chunk and never re-added, then a bisect of
`initParams` with flag probes located the stray write.

If a parameter ever goes missing from saved state again, check its flags for a
stray `NoAssignment` first — and check any `parameters[...]` index that is
spelled with a name from a non-`ParameterNum` enum.
