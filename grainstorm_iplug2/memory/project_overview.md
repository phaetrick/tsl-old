---
name: project-overview
description: Grainstorm project structure, repos, and key architecture
metadata:
  type: project
---

Grainstorm is a granular synthesizer built on iPlug2, with iOS, Android, desktop standalone, and AUv3 plugin targets.

Three repos involved:
- `/Users/patrickropohl/programming/grainstorm_iplug2` — iPlug2 wrapper (IPlugEffect.cpp, config.h)
- `/Users/patrickropohl/programming/CPP-New/grainstorm` — core app logic (synth.cpp, granulate.cpp, granulate_fft.cpp, gui.cpp, etc.)
- `/Users/patrickropohl/programming/sources/iPlug2` — iPlug2 framework (modified IPlugAUPlayer.mm for iOS mic permission)

**Why:** iOS uses the same binary for standalone container AND AUv3 extension — `GetAPI()` always returns `kAPIAUv3`, so runtime detection uses `isRunningAsAppExtension()` (bundle path extension check).

**How to apply:** Any plugin-vs-standalone branching on iOS must use `isRunningAsPlugin` (set from `isRunningAsAppExtension()`), not `GetAPI()`.
