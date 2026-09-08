---
name: ios-architecture
description: iOS-specific architecture decisions and key code locations
metadata:
  type: project
---

**IAP (StoreKit):** Two products:
- `com.thesecretlaboratory.grainstorm.pro` → sets `dofastrender=true` (unlocks pro grain spaces)
- `com.thesecretlaboratory.grainstorm.au` → sets `auv3Purchased=true` (allows AUv3 audio output)

`checkAndSetupPurchase()` reads the purchase record and releases `readyForUiSetup` semaphore. gs1 calls it from `guiSetup`; gs2 from `createEngine` (host.cpp). BOTH then force `dofastrender=true` (test builds stay open) — arming the gate is deleting that line.

**gs2 upgrade view (tslgraphics2/PlatformPaths/storekit_ios.mm, 2026-09-05):** `purchasePro()`/`purchaseAU()` present a full-screen UIKit screen (Close / title / Restore, WKWebView with the bundle's `upgrade.html` / `upgrade_au.html`, price label + PURCHASE button) — Android's InAppPurchases activity. The payment starts from the button; the SKPaymentQueue observer writes the record and calls `onPurchaseComplete` → `onIAPComplete`. Settings rows: owned → "Upgrade is owned." toast; in the AUv3 → "Please purchase in the Grainstorm app." gs1 (graphics/) keeps the old blind flow.

**Purchase record:** `pro_purchased` / `au_purchased` in NSUserDefaults AND the app group `group.io.github.iplug2` (projects/IPlugEffect-iOS.entitlements) — the AUv3 is another process and only sees the group's. Both read, both written (gs2 only; gs1 still standard defaults only).

**Plugin detection:** `tsl::app::isRunningAsAppExtension()` checks `[NSBundle mainBundle].bundleURL.path.pathExtension == "appex"`. Stored in `_DATA->isRunningAsPlugin`.

**Callbacks only wired when plugin:** `InformHostOfParamChange`, `RequestHistory`, `BeginEndInformHostOfParamChangePrivate` are only set when `isRunningAsPlugin=true`. Null-check `if (_STATE->RequestHistory)` in history.cpp.

**Mic input:** `PLUG_CHANNEL_IO "0-2 1-2 2-2"` on iOS standalone gives `inBuffers[1] = nullptr` for mono mic. After `rsIn.readNextFrame()`, copy `tmp[1] = tmp[0]` when right buffer is null.

**AUv3 purchase gate (gs2, 2026-09-05):** NO silence gate in ProcessBlock (removed; Patrick: "No need to restrict ui or feature on auv3"). Instead `Controller::syncAuGate` shows `dlg::AuUnlock` — a licence-terminal-shaped modal dialog with no buttons that `closeDialog` refuses (`dlg::isGate`) — while `gs2::auUnlockRequired()` is true (iOS + extension + `isAuPurchased()` false), polled once a second; it lifts itself when the app-group record turns true. Test hook `forceAuUnlockRequired(1/0/-1)`. gs1's IPlugEffect.cpp keeps its commented-out gate.

**Param feedback loop fix:** `OnParamChange` returns early if `!isRunningAsPlugin`.
