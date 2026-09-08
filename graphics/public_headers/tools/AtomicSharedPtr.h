#pragma once
#include <memory>
#include <atomic>

namespace tsl {

    template <typename T>
    class AtomicSharedPtr {
    public:

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L

        AtomicSharedPtr() = default;
        explicit AtomicSharedPtr(std::shared_ptr<T> p) : ptr(std::move(p)) {}

        void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_seq_cst) {
            ptr.store(std::move(p), order);
        }

        std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const {
            return ptr.load(order);
        }

        AtomicSharedPtr& operator=(std::shared_ptr<T> p) {
            store(std::move(p));
            return *this;
        }

        bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
            std::memory_order order = std::memory_order_seq_cst) {
            return ptr.compare_exchange_strong(expected, std::move(desired), order);
        }

    private:
        std::atomic<std::shared_ptr<T>> ptr;

#else

        AtomicSharedPtr() = default;
        explicit AtomicSharedPtr(std::shared_ptr<T> p) { std::atomic_store_explicit(&ptr, std::move(p), std::memory_order_relaxed); }

        void store(const std::shared_ptr<T>& p, std::memory_order order = std::memory_order_seq_cst) {
            std::atomic_store_explicit(&ptr, p, order);
        }

        std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const {
            return std::atomic_load_explicit(&ptr, order);
        }

        AtomicSharedPtr& operator=(const std::shared_ptr<T>& p) {
            store(p);
            return *this;
        }

        bool compare_exchange_strong(std::shared_ptr<T>& expected, const std::shared_ptr<T>& desired,
            std::memory_order order = std::memory_order_seq_cst) {
            return std::atomic_compare_exchange_strong_explicit(&ptr, &expected, desired, order, order);
        }

    private:
        mutable std::shared_ptr<T> ptr;

#endif

        // Non-copyable, non-movable (matches std::atomic semantics)
        AtomicSharedPtr(const AtomicSharedPtr&) = delete;
        AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;
        AtomicSharedPtr(AtomicSharedPtr&&) = delete;
        AtomicSharedPtr& operator=(AtomicSharedPtr&&) = delete;
    };

} // namespace tsl
