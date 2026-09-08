#pragma once
//
// Android performance hints (ADPF) for audio worker threads.
//
// The problem this solves is that a worker which sleeps as soon as its block is
// done looks idle to the CPU governor, so its core is downclocked and the thread
// is free to migrate to a little core — and the next block then starts slow on a
// cold CPU, missing a deadline it would otherwise have made comfortably.
//
// The old fix is oboe::StabilizedCallback, which grainstorm carries as
// generateLoad: spin out the rest of the block with cpu_relax so the core never
// looks idle. It works, and there is no version of it that does not heat, because
// keeping the core busy IS the mechanism. Measured on a Redmi 23100RN82L: 11 s of
// CPU per 12 s of wall clock, roughly 90% of a core, for as long as audio plays.
//
// This asks instead. The thread declares the deadline it is working to and
// reports how long each block actually took; the power manager boosts and places
// it accordingly. Same job, ~5% of a core measured on the same device.
//
// USE: one session per worker thread, opened from inside that thread (the
// session is bound to a tid, which only exists once the thread runs). Report
// every block — a session with no reports gives the governor nothing to act on
// and does nothing at all. Keep the spin as the fallback for when active() is
// false, which is any device below API 33 and any device that does not implement
// the feature.
//
// Oboe 1.10's AudioStream::setPerformanceHintEnabled covers Oboe's own callback
// thread through the same mechanism, and tsl::Player::BuildStream enables it. It
// does not reach app worker threads, which is why this exists separately.

// Android only, and deliberately not abstracted over other platforms. Every
// caller sits inside an #ifdef __ANDROID__ already — the spin this replaces is
// Android-only too — so a no-op stub for other platforms would be dead code that
// merely invites someone to call it on desktop and expect something to happen.
#ifdef __ANDROID__

#include <cstdint>
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace tsl {

class AdpfHint {
public:
    AdpfHint() = default;
    AdpfHint(const AdpfHint&) = delete;
    AdpfHint& operator=(const AdpfHint&) = delete;
    ~AdpfHint() { stop(); }

    /** Open a session for the calling thread. targetNanos is the deadline it is
     *  working to — one block of audio, not a fraction of one.
     *
     *  Call from inside the worker; the tid is taken here. Returns false when
     *  the platform does not provide the API, which is the caller's cue to fall
     *  back to spinning. */
    bool startForCurrentThread(int64_t targetNanos) {
        if (mSession) return true;
        if (targetNanos <= 0) return false;

        // dlsym rather than a link-time reference: these arrived in API 33 and
        // these apps ship a much lower minSdk, so a direct reference would fail
        // to load on older devices rather than degrade.
        void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (!lib) return false;

        auto getManager = (void* (*)())dlsym(lib, "APerformanceHint_getManager");
        mCreate = (void* (*)(void*, const int32_t*, size_t, int64_t))dlsym(
            lib, "APerformanceHint_createSession");
        mReport = (int (*)(void*, int64_t))dlsym(lib, "APerformanceHint_reportActualWorkDuration");
        mUpdate = (int (*)(void*, int64_t))dlsym(lib, "APerformanceHint_updateTargetWorkDuration");
        mClose = (void (*)(void*))dlsym(lib, "APerformanceHint_closeSession");
        if (!getManager || !mCreate || !mReport || !mUpdate || !mClose) return false;

        void* manager = getManager();
        if (!manager) return false;

        const int32_t tids[1] = {int32_t(syscall(SYS_gettid))};
        mSession = mCreate(manager, tids, 1, targetNanos);
        mTargetNanos = targetNanos;
        return mSession != nullptr;
    }

    bool active() const { return mSession != nullptr; }

    /** Whether this device implements ADPF at all.
     *
     *  A different question from active(): that one asks whether THIS object
     *  holds a session, this one asks whether anyone could open one. The caller
     *  that needs it is a thread covered by somebody else's session rather than
     *  its own -- Oboe's audio callback, which player.cpp hands to
     *  setPerformanceHintEnabled(true). That call is a documented no-op where
     *  the platform lacks the feature, so "is the platform there" is exactly
     *  the test for whether that thread really got the treatment and may skip
     *  its fallback spin. Guessing instead would strip the spin from the
     *  pre-API-33 devices that are the only reason it exists.
     *
     *  Resolved once and cached: the answer cannot change while the process
     *  lives, and dlopen/dlsym are too expensive to repeat per audio block.
     *  Thread-safe by C++11 static initialisation. The handle is deliberately
     *  never dlclosed, matching startForCurrentThread -- libandroid.so is
     *  already loaded into every Android process.
     *
     *  Checks the same five symbols and the same manager startForCurrentThread
     *  needs, so a true here means a session would genuinely open, not merely
     *  that the library exists. */
    static bool platformHasAdpf() {
        static const bool available = [] {
            void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
            if (!lib) return false;

            auto getManager = (void* (*)())dlsym(lib, "APerformanceHint_getManager");
            if (!getManager
                || !dlsym(lib, "APerformanceHint_createSession")
                || !dlsym(lib, "APerformanceHint_reportActualWorkDuration")
                || !dlsym(lib, "APerformanceHint_updateTargetWorkDuration")
                || !dlsym(lib, "APerformanceHint_closeSession"))
                return false;

            return getManager() != nullptr;
        }();
        return available;
    }

    /** Cheap and idempotent; safe to call every block. */
    void setTarget(int64_t targetNanos) {
        if (mSession && targetNanos > 0 && targetNanos != mTargetNanos) {
            mUpdate(mSession, targetNanos);
            mTargetNanos = targetNanos;
        }
    }

    /** Report how long the block actually took. This is the half that matters:
     *  the governor adjusts by comparing it against the target. */
    void report(int64_t actualNanos) {
        if (mSession && actualNanos > 0) mReport(mSession, actualNanos);
    }

    void stop() {
        if (mSession) {
            mClose(mSession);
            mSession = nullptr;
        }
    }

private:
    void* mSession{};
    int64_t mTargetNanos{};
    void* (*mCreate)(void*, const int32_t*, size_t, int64_t){};
    int (*mReport)(void*, int64_t){};
    int (*mUpdate)(void*, int64_t){};
    void (*mClose)(void*){};
};

}  // namespace tsl

#endif  // __ANDROID__
