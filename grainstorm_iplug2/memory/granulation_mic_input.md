---
name: granulation-mic-input
description: How mic/external input feeds into granulation ring buffer and grain firing
metadata:
  type: project
---

**Ring buffer fill:** `synth.cpp` fills `_STATE->inputBuf[chan]` via `rsIn.readNextFrame()`. Mono mic: copy `tmp[1] = tmp[0]` AFTER readNextFrame (resampler zeros inactive channels on read).

**Grain firing condition:** `granulate.cpp` line ~490: `if (!envonly && (off != 0 || dawGrainInputGain > 0.0f))` — grains fire when a file is loaded OR GAIN MIC is above -60dB. Before this fix, grains only fired when a file was loaded (`off != 0`).

**Gain conversion:** Both `granulate.cpp` and `granulate_fft.cpp` use `dbToLinear60()` for `GRAININPUT_GAIN_DAW` (GAIN MIC/EXT) and `INPUT_GAIN_SAMPLER`. Maps -60dB→0.0 (silence), 0dB→1.0 (unity). `granulate_fft.cpp` previously used `LOG2NORMALF` (0.001 at -60dB) — fixed to match.

**GAIN MIC param:** min=-60, max=60, initvalue=-60 (silent by default). `SPACE_GRAINGAIN` space contains the knob. Now included in `grainmods1[]` (free tier) on `PLUGIN_MODE || STANDALONE_MODE` targets — previously only in `grainmods2[]` (pro).

**`granulate_fft.cpp`:** No `off != 0` gate on grain firing — grains run unconditionally, per-sample guards (`to >= off || to < 0`) safely fall back to mic-only.
