// AudioSemaphore.cpp
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

#include "tools/AudioSemaphore.h"


namespace tsl {

    AudioSemaphore::AudioSemaphore() {
#ifdef _WIN32
        sem_ = CreateSemaphoreA(nullptr, 0, LONG_MAX, nullptr);
#elif defined(__APPLE__)
        sem_ = dispatch_semaphore_create(0);
#else
        sem_ = new sem_t();
        sem_init(static_cast<sem_t*>(sem_), 0, 0);
#endif
    }

    AudioSemaphore::~AudioSemaphore() {
#ifdef _WIN32
        CloseHandle(sem_);
#elif defined(__APPLE__)
        dispatch_release(static_cast<dispatch_semaphore_t>(sem_));
#else
        sem_destroy(static_cast<sem_t*>(sem_));
        delete static_cast<sem_t*>(sem_);
#endif
    }

namespace {
    // Bounded spin before wait() parks. The wait here is real work -- the
    // carrier waiting on the modulator's per-grain FFT -- so too short a spin
    // costs a futex round trip every grain, and too long re-creates the burn we
    // are removing. This is the knob to turn if CPU or latency looks wrong.
    constexpr int kSpin = 2048;
}

    void AudioSemaphore::signal() {
        count_.fetch_add(1, std::memory_order_release);
        // Must notify: wait() now parks instead of spinning, so an unnotified
        // increment would leave the consumer asleep forever. Cheap when nobody
        // is parked -- libc++ checks its waiter table before syscalling, and the
        // bounded spin above means the fast path rarely parks at all.
        count_.notify_all();
    }



    void AudioSemaphore::signalSlow() {
        int32_t prev = count_.fetch_add(1, std::memory_order_release);
        if (prev < 0) {
#ifdef _WIN32
            ReleaseSemaphore(sem_, 1, nullptr);
#elif defined(__APPLE__)
            dispatch_semaphore_signal(static_cast<dispatch_semaphore_t>(sem_));
#else
            sem_post(static_cast<sem_t*>(sem_));
#endif
        }
    }

    // Realtime-safe: audio thread is the waiter.
    //
    // Spin-then-block, not a pure spin. This used to spin unboundedly on
    // count_ >= 0, which burns a core for as long as the producer takes and can
    // never be preempted once the waiting thread is priority-boosted -- the
    // reason worker threads could not safely be promoted before.
    //
    // The block waits on count_ itself, on the exact value just loaded, so an
    // increment landing before we park simply makes the value differ and wait()
    // returns at once. No separate flag, hence no erasable wake edge.
    //
    // NOTE (pre-existing, unchanged): the count_ >= 0 predicate is only correct
    // for a single waiter. With two waiters both decrement, and one signal
    // leaves count_ still negative, so neither proceeds. Both users (sem_cross,
    // stereosem) are strictly 1:1.
    void AudioSemaphore::wait() {
        int32_t prev = count_.fetch_sub(1, std::memory_order_acquire);
        if (prev > 0)
            return;                       // token was already available

        for (int i = 0; i < kSpin; ++i) {
            if (count_.load(std::memory_order_acquire) >= 0)
                return;
            cpu_relax();
        }

        int32_t c;
        while ((c = count_.load(std::memory_order_acquire)) < 0)
            count_.wait(c, std::memory_order_acquire);
    }

    void AudioSemaphore::waitSlow() {
        int32_t prev = count_.fetch_sub(1, std::memory_order_acquire);
        if (prev <= 0) {
#ifdef _WIN32
            WaitForSingleObject(sem_, INFINITE);
#elif defined(__APPLE__)
            dispatch_semaphore_wait(static_cast<dispatch_semaphore_t>(sem_), DISPATCH_TIME_FOREVER);
#else
            sem_wait(static_cast<sem_t*>(sem_));
#endif
        }
    }

} // namespace tsl