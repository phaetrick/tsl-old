#pragma once
#include "RingBufferQueue.h"
#include <atomic>
#include <optional>
#include <cassert>
#include <cstddef>
#include "logger.h"

namespace tsl {
    template<typename T, size_t Capacity, template<typename, size_t> class QueueType>
    class RingPoolBase {
    public:
        RingPoolBase() {
            for (size_t i = 0; i < Capacity; ++i) {
                new (&mem[i * sizeof(T)]) T();
                bool ok = queue.try_push(ptr(i));
                if (!ok) std::terminate();
            }
        }
        ~RingPoolBase() {
            while (auto* p = get()) p->~T();
        }

        void reset() {
            while (queue.try_pop().has_value());
            for (size_t i = 0; i < Capacity; ++i)
                ptr(i)->~T();
            for (size_t i = 0; i < Capacity; ++i) {
                new (&mem[i * sizeof(T)]) T();
                assert(queue.try_push(ptr(i)) == true);
            }
        }

        T* get() { return queue.try_pop().value_or(nullptr); }
        bool release(T* item) { return queue.try_push(item); }
        bool empty() const { return queue.empty(); }
        size_t size()  const { return queue.size(); }

    protected:
        T* ptr(size_t i) {
            return std::launder(reinterpret_cast<T*>(&mem[i * sizeof(T)]));
        }

        QueueType<T*, Capacity> queue;
        alignas(T) std::byte mem[sizeof(T) * Capacity];
    };

    template<typename T, size_t Capacity>
    class RingPoolSPSC : public RingPoolBase<T, Capacity, RingBufferSPSCQueue> {};

    template<typename T, size_t Capacity>
    class RingPoolMPMC : public RingPoolBase<T, Capacity, RingBufferMPMCQueue> {};

    template<typename T, size_t Capacity>
    class RingPoolMPSC : public RingPoolBase<T, Capacity, RingBufferMPSCQueue> {};
}

