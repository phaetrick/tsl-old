# One XRUN on every stream start

Fixed and measured on device, 2026-08-13. Kept because the *cause* is a
property of how Android starts a stream, and anything that changes the
coordinator hand-off can bring it back.

## Symptom

Dropped audio blocks at every stream start — one per POWER off→on cycle, shown
as the info panel's `XRUN #n` (`render_xrun`, `infopanel.cpp`), which appears
for a second and reverts. Reproduced 10 launches out of 10 on a Redmi
23100RN82L, Android 15, OpenSLES, 48 kHz, 1920-frame callback (40 ms),
8192-frame device buffer.

It was actually **two** dropped blocks, not one: blocks 1 and 2. Only the panel
is transient, so a fast pair of drops reads as a single flash.

**Android + multithreaded only.** `_DATA->open` is set solely under
`if (isMultiThreaded)`, so the drop path in `PlayerBase::synthFunc` is
unreachable single-threaded.

## Cause

**At stream start the device does not ask for blocks at the block rate.** It
asks for several back-to-back to fill the buffer it is about to start playing
out of. Measured callback arrival times, three consecutive launches:

```
cb 0 at 0.00 ms      cb 0 at 0.00 ms      cb 0 at 0.00 ms
cb 1 at 6.23 ms      cb 1 at 5.05 ms      cb 1 at 6.46 ms
cb 2 at 7.23 ms      cb 2 at 6.06 ms      cb 2 at 7.51 ms
cb 3 at 36.24 ms     cb 3 at 35.83 ms     cb 3 at 35.58 ms
cb 4 at 71.98 ms     cb 4 at 71.65 ms     cb 4 at 71.46 ms
```

Three blocks demanded inside 7 ms, then the ~40 ms cadence. The pipeline
produces one block per callback and dispatches the next pass at the end of the
callback, so the pass dispatched by callback 0 is still running when 1 and 2
arrive: both find `open` set, both drop.

## What this was NOT

**Not a slow cold first pass.** That was the standing hypothesis and it is
wrong. Instrumented, the first pass costs, from the dispatch that starts it:

```
wake 0.04-0.25 ms | ADPF session + LOGD 4.0-15.7 ms | run 3.7-10.2 ms
```

7.8 ms to 23 ms in total, against a 40 ms block. Runs that finished the pass in
7.8 ms still dropped, because the gap they had to fit into was 1 ms, not 40.
Speeding the pass up cannot fix this and is not what fixed it.

The one-time costs in that figure are real, though, and remain available if the
margin ever needs widening: `SynthThread::run` and `ChannelThread::run` both
open their ADPF session (and log it) on the first *dispatched block* rather than
at thread start, which puts 4–16 ms of binder call inside the first block's
budget. `hint.setTarget` already runs per block, so moving the session creation
above the loop would cost nothing but a briefly-wrong initial target.

Also previously ruled out, and still worth not re-chasing: a stale `open` flag
surviving the previous stop (two writers only; `SynthThread::run` always
completes `synthFuncIntern` once it acquires), and the stale `currentBufSize`
(a genuine defect, fixed by publishing `getFramesPerDataCallback()` at stream
open in `player.cpp`, but not this bug).

## Fix

`PlayerBase::synthFunc`, `synth.cpp`. For the first `kPrimingBlocks` (8) blocks
of a stream, a callback that finds `open` set **waits** for the coordinator, up
to one block period, instead of dropping the pass. `DATA::startBlocks` counts
the window and `onPlayerStop` re-arms it.

The wait is free in that window by construction: the device is only asking
back-to-back because it is filling a buffer it has not started playing from
yet, so a block that lands late lands into a buffer with room for it. It sleeps
in 200 µs slices rather than spinning — the audio callback is the highest
priority thread in the process, and spinning takes a core from precisely the
pass it is waiting for.

**Deliberately not the steady-state behaviour.** Once the cadence is real the
buffer is drained as fast as it is filled, waiting only moves the hole to the
next block, and dropping the pass whole and concealing is the right answer.

A pre-roll — dispatching the coordinator at `stream->start()` — was the fix
proposed before the callback times were known. It does not work: it fills the
queue for callback 0 but the 1 ms gap between callbacks 1 and 2 is still
unservable. It would also have needed a hook in `graphics/src/player.cpp`; the
fix as it stands is entirely inside `grainstorm/`.

Callback 0 still emits one silent 40 ms block, because nothing has been
produced when it arrives. That is inherent to a one-block-ahead pipeline, is not
counted as a drop, and is inaudible at power-on. A pre-roll is the only thing
that would remove it.

## Verified

10 launches and 6 POWER off→on cycles, zero drops, against 10 of 10 dropping
before the change. The drop counter was read from an instrumented staging build
that recorded into preallocated atomics and dumped them from a detached thread
— never `LOGx` from the audio thread or `SynthThread`, since `stringf`
allocates and writes to a socket, which is the same order of cost as the
deadline being measured.

## Build and measure

```bash
cd ~/AndroidStudioProjects/grainstorm && ./gradlew installStaging
```

`staging` is `initWith release` plus local signing — a Release **native**
config. Do not measure on `debug`: it passes `-DCMAKE_BUILD_TYPE=Debug` and the
`.so` is 96 MB against 36 MB, which distorts every timing.

Keep the screen awake (`adb shell svc power stayon true`) — a launch onto a
locked screen never starts audio and silently produces no measurement at all.
Check `SmartPower` / `PowerKeeper.Audio` in logcat too: MIUI backgrounds and
throttles the app, and samples taken then are meaningless.

## Still open from the same session

**Non-MT LOAD on Android was never verified.** Every audio fix landed
2026-08-13 was confirmed on the MT path only, because `drops` increments solely
under `isMultiThreaded`. The user's report of 90% LOAD with MT off has never
been reproduced or explained.

## Related

`LOAD` is wall-clock (`diff` over audio time), so blocking and preemption count
as load while consuming no CPU. If LOAD and per-thread CPU disagree, that is the
signature of a park or a stall rather than DSP cost — see the notes in
`tools/AudioSemaphore.h` and `tools/AdpfHint.h`.
