// threadtsl.cpp
//
// Thread::setPriority used to have its whole body inside `#ifdef __ANDROID__`,
// so on macOS and Windows it compiled to nothing and every worker thread ran at
// default priority. That matters for the audio worker pool: the audio thread is
// promoted to real-time by the host and then waits on those workers
// (WorkerLatch), so leaving them at default priority is a standing inversion.
//
// The implementation lives here rather than in the header because Windows needs
// <windows.h>, which this project never pulls into a public header.
//
// The Android branch is carried over verbatim and is deliberately unchanged:
// there, the real boost comes from tprio() -> Java Process.setThreadPriority
// (app.cpp), not from this function.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

#include "tools/threadtsl.h"

namespace tsl {

#if !defined(__ANDROID__)
namespace {

    // Callers pass one of two conventions, so normalise before mapping:
    //
    //   * the Thread:: enum, 0..3 (Low, Normal, High, Realtime)
    //   * an Android nice value, where *more negative is higher* -- namely the
    //     startThread(-19) used for the channel and synth threads, which is
    //     Android's THREAD_PRIORITY_URGENT_AUDIO. Read as an enum, -19 would
    //     fall through to "below normal", i.e. the exact opposite of intent.
    //
    // The two overlap on 0..3, where the enum meaning wins because that is what
    // the existing Android branch already assumes.
    int normalise(const int prior) {
        if (prior <= -16) return Thread::RealtimePriority;   // urgent audio
        if (prior < 0)    return Thread::HighPriority;
        if (prior <= Thread::RealtimePriority) return prior; // enum, as written
        return Thread::LowPriority;                          // positive nice
    }

} // namespace
#endif

void Thread::setPriority(const int prior) {
#if defined(__ANDROID__)
    // Unchanged from the original header implementation.
    auto policy = (prior <= 0) ? SCHED_OTHER : SCHED_RR;
    auto minp = sched_get_priority_min(policy);
    auto maxp = sched_get_priority_max(policy);

    struct sched_param param;

    switch (prior) {
        case LowPriority:
        case NormalPriority:    param.sched_priority = 0; break;
        case HighPriority:      param.sched_priority = minp + (maxp - minp) / 4; break;
        case RealtimePriority:
        default:                param.sched_priority = minp + (3 * (maxp - minp) / 4); break;
    }

    pthread_setschedparam(pthread_self(), policy, &param);

#elif defined(_WIN32)
    int win = THREAD_PRIORITY_NORMAL;
    switch (normalise(prior)) {
        case LowPriority:      win = THREAD_PRIORITY_BELOW_NORMAL;  break;
        case NormalPriority:   win = THREAD_PRIORITY_NORMAL;        break;
        case HighPriority:     win = THREAD_PRIORITY_HIGHEST;       break;
        case RealtimePriority: win = THREAD_PRIORITY_TIME_CRITICAL; break;
        default: break;
    }
    SetThreadPriority(GetCurrentThread(), win);
    // A production audio-worker boost on Windows wants MMCSS
    // (AvSetMmThreadCharacteristics "Pro Audio"), which also has to be reverted
    // on thread exit. THREAD_PRIORITY_TIME_CRITICAL is the direct analogue of
    // what the enum promises and adds no link dependency.

#elif defined(__APPLE__)
    // SCHED_RR is honoured on macOS and needs no privileges (verified: setting
    // it from a plain std::thread returns 0 and reads back). The host's own
    // audio thread runs under the time-constraint policy, which outranks
    // SCHED_RR, so a worker boosted here still cannot preempt it.
    //
    // Priorities must be range-relative, NOT the literal 0 the Android branch
    // uses. On macOS both policies report 15..47 and the default is 31, so a
    // bare 0 is accepted and silently drops the thread below everything --
    // demoting the threads this function exists to protect.
    const int level = normalise(prior);
    const auto policy = (level >= HighPriority) ? SCHED_RR : SCHED_OTHER;
    const auto minp = sched_get_priority_min(policy);
    const auto maxp = sched_get_priority_max(policy);
    const auto span = maxp - minp;

    struct sched_param param {};

    switch (level) {
        case LowPriority:       param.sched_priority = minp + span / 4; break;
        case NormalPriority:    param.sched_priority = minp + span / 2; break;   // the default
        case HighPriority:      param.sched_priority = minp + span / 2; break;   // RR already outranks OTHER
        case RealtimePriority:
        default:                param.sched_priority = minp + (3 * span / 4); break;
    }

    pthread_setschedparam(pthread_self(), policy, &param);

#else
    (void) prior;   // other platforms unchanged: still a no-op
#endif
}

} // namespace tsl
