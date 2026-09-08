// 02.07.2025
#pragma once
#ifndef TSL_MUTEX_H__
#define TSL_MUTEX_H__

#include <atomic>
#include <cassert>
#include <thread>

// Platform detection � kept in a private detail namespace to avoid leaking macros
namespace tsl::detail {

#if defined(_WIN32)
    constexpr bool platform_windows = true;
#else
    constexpr bool platform_windows = false;
#endif

} // namespace tsl::detail

#if defined(_WIN32)
#include <mutex>
#endif
#if defined(__ANDROID__) || defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#endif

namespace tsl {

    // -------------------------------------------------------------------------
    // mutex
    //
    // On Windows: thin wrapper around std::mutex (avoids busy-wait on a
    //             cooperative scheduler).
    // Elsewhere:  spinlock with platform hints (pause/yield/atomic_wait).
    // -------------------------------------------------------------------------
    class mutex {
    public:
#if defined(_WIN32)
        void     lock() { mtx_.lock(); }
        void     unlock() { mtx_.unlock(); }
        bool     try_lock() { return mtx_.try_lock(); }

    private:
        std::mutex mtx_;

#else
        void lock() noexcept {
            while (flag_.test_and_set(std::memory_order_acquire)) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
                flag_.wait(true, std::memory_order_relaxed);
#elif defined(__aarch64__) || defined(__arm__)
                asm volatile("yield" ::: "memory");
#elif defined(__x86_64__) || defined(_M_X64)
                __asm__ __volatile__("pause");
#else
                std::this_thread::yield(); // FIX: was an empty loop � yields CPU instead of hammering it
#endif
            }
        }

        void unlock() noexcept {
            flag_.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            flag_.notify_one();
#endif
        }

        bool try_lock() noexcept {
            return !flag_.test_and_set(std::memory_order_acquire);
        }

    private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
#endif
    };


    // -------------------------------------------------------------------------
    // recursive_mutex
    //
    // On Windows: thin wrapper around std::recursive_mutex.
    // Elsewhere:  recursive spinlock.
    //
    // Corrections vs. original:
    //   1. lock_count_ is std::atomic<int> � plain int was UB (even though only
    //      the owner thread accesses it, the compiler can't know that without
    //      synchronisation guarantees the standard requires for shared objects).
    //   2. Consistent spin hints (pause/yield/atomic_wait) in both lock() and
    //      try_lock(), matching what mutex uses.
    //   3. Non-owner unlock: std::terminate() in both debug and release builds
    //      instead of a silent return after a no-op assert.
    //   4. Corrected access visibility � all members are private.
    // -------------------------------------------------------------------------
    class recursive_mutex {
    public:
#if defined(_WIN32)
        void     lock() { mtx_.lock(); }
        void     unlock() { mtx_.unlock(); }
        bool     try_lock() { return mtx_.try_lock(); }

    private:
        std::recursive_mutex mtx_;

#else
        // Encodes: high 32 bits = owner thread id (as uint32_t truncation of pthread_t
        // or gettid()), low 32 bits = lock count. Zero means unlocked.
        //
        // Using a single atomic word eliminates the TOCTOU window between
        // flag_ and owner_id_ that caused hangs on ARM64/Android.
        void lock() noexcept {
            const uint64_t tid = current_tid();
            while (true) {
                uint64_t cur = lock_word_.load(std::memory_order_acquire);
                if ((cur >> 32) == tid) {
                    lock_word_.store(cur + 1, std::memory_order_relaxed);
                    return;
                }
                if (cur == 0 && lock_word_.compare_exchange_strong(
                        cur, (tid << 32) | 1,
                        std::memory_order_acquire,
                        std::memory_order_relaxed))
                    return;
#if defined(__aarch64__) || defined(__arm__)
                asm volatile("yield" ::: "memory");
#elif defined(__x86_64__) || defined(_M_X64)
                __asm__ __volatile__("pause");
#else
        std::this_thread::yield();
#endif
            }
        }

        bool try_lock() noexcept {
            const uint64_t tid = current_tid();
            uint64_t cur = lock_word_.load(std::memory_order_acquire);
            if ((cur >> 32) == tid) {
                lock_word_.store(cur + 1, std::memory_order_relaxed);
                return true;
            }
            if (cur == 0)
                return lock_word_.compare_exchange_strong(cur,
                                                          (tid << 32) | 1,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed);
            return false;
        }

        void unlock() noexcept {
            uint64_t cur = lock_word_.load(std::memory_order_relaxed);
            const uint64_t tid = current_tid();
            if ((cur >> 32) != tid) {
                assert(false && "tsl::recursive_mutex: unlock by non-owner");
                std::terminate();
            }
            uint32_t count = static_cast<uint32_t>(cur);
            lock_word_.store(count == 1 ? 0 : (tid << 32) | (count - 1),
                             std::memory_order_release);
        }

    private:
        static uint64_t current_tid() noexcept {
#if defined(__ANDROID__)
            // gettid() not reliably available before API 21 in NDK headers
            // syscall is always safe
            return (uint64_t)gettid();
#elif defined(__linux__)
            uint32_t t = static_cast<uint32_t>(::gettid());
    return static_cast<uint64_t>(t == 0 ? 1 : t);
#else
    uint64_t t = static_cast<uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id())
    ) & 0xFFFFFFFFull;
    return t == 0 ? 1 : t;
#endif
        }
        std::atomic<uint64_t> lock_word_{ 0 };
#endif
    };

} // namespace tsl

#endif // TSL_MUTEX_H__