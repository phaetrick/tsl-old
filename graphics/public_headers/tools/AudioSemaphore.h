// AudioSemaphore.h
#pragma once
#include "defines.h"
#include "platform_config.h"
#include <atomic>
#include <cstdint>
#include <chrono>

#ifndef cpu_relax
#if defined(_MSC_VER) || defined(__clang_cl__)
#include <immintrin.h>
inline void cpu_relax() { _mm_pause(); }
#elif defined(__x86_64__) || defined(__i386__)
inline void cpu_relax() { asm volatile("rep; nop" ::: "memory"); }
#elif defined(__aarch64__) || defined(__arm__)
inline void cpu_relax() { asm volatile("yield" ::: "memory"); }
#else
inline void cpu_relax() { asm volatile("" ::: "memory"); }
#endif
#endif

namespace tsl {
   
    class AudioSemaphore {
    public:
        AudioSemaphore();
        ~AudioSemaphore();
        AudioSemaphore(const AudioSemaphore&) = delete;
        AudioSemaphore& operator=(const AudioSemaphore&) = delete;

        void signal();
        void signalSlow();
        void wait();
        void waitSlow();

    private:
        alignas(CACHELINE) std::atomic<int32_t> count_{ 0 };
        void* sem_ = nullptr; // opaque OS handle
    };

    class WorkerLatch {
    public:
        // Call once per buffer, before launching workers.
        //
        // Notify for the same reason CrossBarrier::reset does: wait() parks on
        // the value it last observed, so a waiter left over from a previous
        // batch -- SynthThread::stop and record_loop both wait on this latch
        // from off the audio thread -- must be given a chance to re-read. It
        // costs nothing when nobody is parked.
        void reset(int workerCount) {
            count_.store(workerCount, std::memory_order_release);
            count_.notify_all();
        }

        // Called by each worker when done with their slice.
        //
        // Notify only on the 0 transition, not on every decrement. notify_all()
        // wakes everyone parked on this atomic whatever value they parked on,
        // so the single wake at 0 releases the waiter regardless of where it
        // went to sleep -- and the intermediate decrements, which would each
        // wake it only to send it straight back, cost no syscall at all. This
        // relies on exactly N done() calls following reset(N), which is already
        // the invariant the old spin depended on to terminate.
        void done() {
            if (count_.fetch_sub(1, std::memory_order_release) == 1)
                count_.notify_all();
        }

        // Waits until all workers finish. spinNanos is how long to stay on the
        // CPU before giving up and parking; pass a fraction of the block period.
        //
        // The spin budget is a TIME, not an iteration count, and it has to be
        // generous. An earlier version used a fixed 2048 cpu_relax iterations --
        // two to four microseconds on ARM -- which sounds reasonable until you
        // notice when this is called: the caller runs its own slice FIRST and
        // only then waits, so what remains is the skew between it and the
        // slowest worker, which is tens to hundreds of microseconds. A 2048-spin
        // never once caught that, so every single block paid a full futex sleep
        // and wake and none ever got the fast path. Worst of both.
        //
        // Parking mid-block is also the exact disease tools/AdpfHint.h exists to
        // treat: a thread that sleeps looks idle to the governor, gets
        // downclocked and migrated to a little core, and comes back slow. Doing
        // that to the dispatcher once per block cost ~90% reported LOAD on
        // device. So the spin must be long enough that the normal case never
        // reaches the park at all.
        //
        // The park still matters -- it is what stops a descheduled worker
        // turning this into an unbounded burn on a thread at audio priority,
        // which is what it replaced. It is the pathological path, not the
        // common one.
        //
        // Clock is read once per kSpinChunk iterations: often enough to honour
        // the budget, rarely enough that the read is not itself the cost.
        //
        // Park on the exact value just loaded, never on a literal 0: a done()
        // landing between the load and the park makes the value differ, and
        // wait() returns at once instead of sleeping on a wake it missed.
        void wait(int64_t spinNanos = kDefaultSpinNanos) {
            int c = count_.load(std::memory_order_acquire);
            if (c <= 0) return;

            // steady_clock, not tools.h's system_clock helper: this is a duration
            // budget, and a wall clock that NTP can step backwards under a
            // spinning audio thread is the one way to turn it into a hang. It
            // also keeps this header free of tools.h.
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::nanoseconds(spinNanos);
            for (;;) {
                for (int i = 0; i < kSpinChunk; ++i) {
                    if ((c = count_.load(std::memory_order_acquire)) <= 0)
                        return;
                    cpu_relax();
                }
                if (std::chrono::steady_clock::now() >= deadline)
                    break;
            }

            while ((c = count_.load(std::memory_order_acquire)) > 0)
                count_.wait(c, std::memory_order_acquire);
        }

    private:
        // Iterations between clock reads.
        static constexpr int kSpinChunk = 64;

        // Used when the caller does not pass a budget (ModalReverb). 1 ms is
        // well past any plausible worker skew while still bounding the burn to a
        // fraction of a block, which is the whole point of not spinning forever.
        static constexpr int64_t kDefaultSpinNanos = 1000000;

        alignas(CACHELINE) std::atomic<int> count_{ 0 };
    };

    

    // Hand-off from the cross-synthesis carrier (producer, one call per grain)
    // to its modulator thread (consumer, runs compute_fft).
    //
    // The consumer must BLOCK, not spin. It waits on the carrier, and the
    // carrier in turn waits on the modulator's sem_cross before it produces the
    // next grain, so the two ping-pong. A spinning consumer never yields its
    // core, which starves the very thread it is waiting for; worse, the first
    // active track's channel 0 runs on the *audio* thread, and that can be a
    // modulator, so a spin there burns the whole buffer deadline and glitches.
    // A short spin first keeps the fast path cheap when the carrier is already
    // running, then we fall back to a real blocking wait.
    //
    // The wait is on _count ITSELF -- never on a separate flag. An earlier
    // version parked on an atomic_flag that loop_continue/loop_exit set as an
    // edge, and that deadlocked: reset() runs on the audio thread once per
    // buffer (setupTracks) and cleared the flag, erasing an edge published by
    // loop_exit that the modulator had not consumed yet. The modulator then
    // parked forever, never reached workerLatch.done(), and WorkerLatch::wait()
    // -- an unbounded spin on the audio thread at the time -- hung the whole
    // app. (That wait parks now, so the same deadlock would present as silence
    // rather than a pegged core; it is still a deadlock.)
    // Value-based waiting has no such window: we park on the exact value just
    // observed, so any subsequent change returns immediately, and an edge that
    // arrives before we park simply makes the value differ. Every writer must
    // still notify, reset() included.
    class CrossBarrier {
    public:
        CrossBarrier() { reset(); }

        void reset() {
            _exitloop = false;
            _silence = false;
            _step_point = _step_length = 0;
            _delta = 0;
            // Release + notify: a consumer parked on the previous buffer's value
            // (loop_exit's 1000000) must be woken, or it never observes the
            // reset and stays parked.
            _count.store(0, std::memory_order_release);
            _count.notify_all();
        }

        template<typename T1, typename T2>
        void wait_rt(T1& step_point, T2& step_length, bool& exitloop, bool& silence) {
            int c = _count.load(std::memory_order_acquire);
            if (c <= _delta) {
                for (int i = 0; i < kSpin &&
                     (c = _count.load(std::memory_order_acquire)) <= _delta; ++i)
                    cpu_relax();
                while ((c = _count.load(std::memory_order_acquire)) <= _delta)
                    _count.wait(c, std::memory_order_acquire);
            }
            ++_delta;
            step_point = _step_point;
            step_length = _step_length;
            exitloop = _exitloop;
            silence = _silence;
        }

        template<typename T1, typename T2>
        void loop_continue(T1 step_point, T2 step_length, bool silence) {
            _step_point = step_point;
            _step_length = step_length;
            _silence = silence;
            _count.fetch_add(1, std::memory_order_release);
            _count.notify_all();
        }

        void loop_exit() {
            _exitloop = true;
            // Store rather than increment: this releases the consumer whatever
            // the count reached, so a buffer can never end with it still parked.
            _count.store(1000000, std::memory_order_release);
            _count.notify_all();
        }

    private:
        static constexpr int kSpin = 512;

        alignas(CACHELINE) std::atomic<int> _count{};
        int     _delta{};
        bool    _silence{};
        bool    _exitloop{};
        MYFLOAT _step_point{};
        MYFLOAT _step_length{};
    };


    // NOTE: currently unused -- nothing in the codebase instantiates this. The
    // audio workers wait on tsl::BinarySemaphore (ChannelThread::sem) instead.
    class WorkerSemaphore {
    public:
        void signal() {
            count_.store(1, std::memory_order_release);
            count_.notify_one();
        }

        void wait() {
            // precheck: only spin if semaphore is warm (count >= 0 means active)
            if (count_.load(std::memory_order_acquire) >= 0) {
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                while (std::chrono::steady_clock::now() < deadline) {
                    if (count_.load(std::memory_order_acquire) > 0) goto acquired;
					for (int i = 0; i < 100; i++) cpu_relax();
                }
            }
            // cold start or spin expired � OS sleep
            while (true) {
                int v = count_.load(std::memory_order_acquire);
                if (v > 0) goto acquired;
                count_.wait(v, std::memory_order_acquire);
            }
        acquired:
            count_.fetch_sub(1, std::memory_order_acquire);
        }

    private:
        alignas(CACHELINE) std::atomic<int> count_{ -1 }; // cold by default
    };


} // namespace tsl