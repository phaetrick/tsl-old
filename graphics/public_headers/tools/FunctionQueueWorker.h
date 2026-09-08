#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <utility>
#include <type_traits>
#include <thread>

#include "inplace_function.h"
#include "platform_config.h"

namespace tsl {

    template<std::size_t Capacity>
    class MPSCWorker {
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of two");
        static_assert(Capacity > 1, "Capacity must be > 1");

#if INTPTR_MAX == INT64_MAX
        using index_t = std::uint64_t;
#else
        using index_t = std::uint32_t;
#endif

        static constexpr index_t MASK = Capacity - 1;

        using Task = stdext::inplace_function<void(), 64>;

        // ------------------------------------------------------------------
        // Slot — each on its own cache line to prevent false sharing between
        // adjacent slots written by different producers concurrently.
        // ready flag acts as the publication barrier so drain() never touches
        // a half-constructed Task.
        // ------------------------------------------------------------------
        struct alignas(CACHELINE) Slot {
            std::atomic<bool>          ready{ false };
            alignas(Task) unsigned char storage[sizeof(Task)];
        };

        // ------------------------------------------------------------------
        // Hot state — each on its own line (destructive interference)
        // ------------------------------------------------------------------

        // Producer line — written by many producers via fetch_add
        alignas(CACHELINE) std::atomic<index_t> writePos_{ 0 };

        // Consumer line — written only by the single worker thread
        alignas(CACHELINE) std::atomic<index_t> readPos_{ 0 };

        // Wake/stop line — read by both sides but written rarely
        alignas(CACHELINE) std::atomic<index_t> epoch_{ 0 };
        std::atomic<bool>    stop_{ false };  // same line as epoch, read together

        // Buffer — cold relative to the counters
        alignas(CACHELINE) Slot buffer_[Capacity];

        // Declared last — thread starts in constructor body after all
        // members are initialized
        std::thread workerThread_;
        
    public:
        MPSCWorker() {
            workerThread_ = std::thread([this] { run(); });
        }

        ~MPSCWorker() {
            shutdown();
            drain(false); // mop up any tasks pushed in the shutdown race window
        }

        MPSCWorker(const MPSCWorker&) = delete;
        MPSCWorker& operator=(const MPSCWorker&) = delete;

        // ------------------------------------------------------------------
        // add_task — lock-free MPSC push
        // Returns true on success, false if full or stopped.
        //
        // Capacity check is a CAS loop so multiple producers cannot
        // simultaneously overcommit the buffer.
        // ------------------------------------------------------------------
        template<typename F>
        bool add_task(F&& fn) {
            if (stop_.load(std::memory_order_acquire))
                return false;

            index_t pos = writePos_.load(std::memory_order_relaxed);
            while (true) {
                index_t r = readPos_.load(std::memory_order_acquire);
                if (pos - r >= static_cast<index_t>(Capacity))
                    return false; // full

                if (writePos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed))
                    break;
                // pos updated by CAS failure — retry
            }

            Slot& slot = buffer_[pos & MASK];

            // Construct before publishing ready flag
            new (slot.storage) Task(std::forward<F>(fn));
            slot.ready.store(true, std::memory_order_release);

            epoch_.fetch_add(1, std::memory_order_release);
            epoch_.notify_one();
            return true;
        }

        // Convenience overload — binds args into the closure
        template<typename F, typename... Args>
        bool add_task(F&& fn, Args&&... args) {
            return add_task(
                [fn = std::forward<F>(fn),
                ...args = std::forward<Args>(args)]() mutable {
                    fn(args...);
                });
        }

        // ------------------------------------------------------------------
        // add_taskInt — same as add_task but returns how far back in the queue
        // the task landed (0 = it runs next), or -1 if it was NOT queued because
        // the queue was full or the worker is stopped.
        //
        // The return type is signed on purpose. It used to be index_t, which is
        // unsigned, so the index_t(-1) failure value came back as a huge positive
        // number and every `auto res = add_taskInt(...); if (res > 0)` caller read
        // a dropped task as a successful enqueue — reporting "Queued" for work
        // that never ran. Call sites that assign to int32_t were unaffected.
        // ------------------------------------------------------------------
        template<typename F>
        int add_taskInt(F&& fn) {
            if (stop_.load(std::memory_order_acquire))
                return -1;

            index_t pos = writePos_.load(std::memory_order_relaxed);
            while (true) {
                index_t r = readPos_.load(std::memory_order_acquire);
                if (pos - r >= static_cast<index_t>(Capacity))
                    return -1; // full — no spin

                if (writePos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed))
                    break; // claimed our slot

                // CAS failed — another producer beat us, pos updated by CAS.
                // Re-check capacity with the new pos before retrying.
                r = readPos_.load(std::memory_order_acquire);
                if (pos - r >= static_cast<index_t>(Capacity))
                    return -1;
            }

            Slot& slot = buffer_[pos & MASK];
            new (slot.storage) Task(std::forward<F>(fn));
            slot.ready.store(true, std::memory_order_release);

            epoch_.fetch_add(1, std::memory_order_release);
            epoch_.notify_one();

            // Difference of two monotonic counters, always < Capacity.
            return static_cast<int>(pos - readPos_.load(std::memory_order_acquire));
        }

        template<typename F, typename... Args>
        int add_taskInt(F&& fn, Args&&... args) {
            return add_taskInt(
                [fn = std::forward<F>(fn),
                ...args = std::forward<Args>(args)]() mutable {
                    fn(args...);
                });
        }

        // DO NOT USE — unused, and unsafe in this app's shutdown model. Kept only
        // until it is decided whether to delete it outright.
        //
        // It parks on a private condition_variable that AppState's teardown
        // cannot reach. Teardown is `destroyRequested = true; waitNotify.shutdown()`
        // (wakes every parked waiter) and only THEN joins the workers -- so any
        // wait that is not a waitNotify wait is invisible to it. Worse, shutdown()
        // runs drain(execute=false), which destroys queued tasks WITHOUT invoking
        // them, so `done` is never set even on a clean exit and the waiter parks
        // forever; UiTasksQueue.shutdown() then joins that thread and ~AppState
        // never returns. On desktop that is a process that will not close.
        //
        // To wait on a worker, use the waitNotify token idiom (begin_wait /
        // complete / wait_for_signal + a destroyRequested check), or better, chain
        // a continuation onto the next worker instead of blocking at all -- see
        // tsl::preset::save_preset.
        template<typename F>
        bool post_and_wait(F&& fn) {
            std::mutex mtx;
            std::condition_variable cv;
            bool done = false;

            bool queued = add_task([fn = std::forward<F>(fn), &mtx, &cv, &done]() mutable {
                fn();
                std::lock_guard<std::mutex> lk(mtx);
                done = true;
                cv.notify_one();
                });

            if (!queued) return false;

            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&done] { return done; });
            return true;
        }

        void shutdown() {
            bool expected = false;
            if (!stop_.compare_exchange_strong(expected, true,
                std::memory_order_release,
                std::memory_order_relaxed))
                return; // already shut down

            epoch_.fetch_add(1, std::memory_order_release);
            epoch_.notify_all();

            if (workerThread_.joinable())
                workerThread_.join();
        }
    private:
        // ------------------------------------------------------------------
        // Worker loop
        // ------------------------------------------------------------------
        void run() {
            
            for (;;) {
                index_t e = epoch_.load(std::memory_order_acquire);

                if (stop_.load(std::memory_order_acquire)) {
                    drain(/*execute=*/false);
                    break;
                }

                drain(/*execute=*/true);

                // Sleep only if nothing new arrived while we were draining
                epoch_.wait(e, std::memory_order_relaxed);
            }

        }

        // ------------------------------------------------------------------
        // drain — consume all currently visible slots.
        // execute=true  → invoke then destroy each Task  (normal operation)
        // execute=false → destroy only, no invocation    (shutdown path)
        // ------------------------------------------------------------------
        void drain(bool execute) {
            index_t r = readPos_.load(std::memory_order_relaxed);

            for (;;) {
                index_t w = writePos_.load(std::memory_order_acquire);
                if (r == w) break;

                while (r != w) {
                    Slot& slot = buffer_[r & MASK];

                    // Spin until the producer has finished constructing the Task.
                    // Window is narrow (preemption between fetch_add and construction)
                    // but must be handled to avoid invoking a half-built object.
                    while (!slot.ready.load(std::memory_order_acquire))
                        std::this_thread::yield();

                    Task* task = reinterpret_cast<Task*>(slot.storage);
                    if (execute) (*task)();
                    task->~Task();
                    slot.ready.store(false, std::memory_order_release);
                    ++r;

                    // Publish per task, not once at the end of the pass. The slot
                    // is fully destroyed and marked not-ready above, so it is safe
                    // for a producer to claim now. Publishing only at the end meant
                    // a single long task (Save Project parks this worker for the
                    // whole modal dialog) held readPos_ at where the pass started,
                    // so add_task saw the queue as full and silently dropped every
                    // event pushed meanwhile — undo history included.
                    readPos_.store(r, std::memory_order_release);
                }
            }

            readPos_.store(r, std::memory_order_release);
        }

        // ------------------------------------------------------------------
        // shutdown — idempotent, safe to call from destructor
        // ------------------------------------------------------------------
        
    };

} // namespace tsl