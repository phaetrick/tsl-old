#pragma once
#include <atomic>
#include <optional>
#include "platform_config.h"
#include <memory>
#include <cstddef>
#include <cstdint>
#include <cassert>

namespace tsl {

    template<typename T, size_t Capacity>
    class RingBufferQueueBase {
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2");
        static_assert(Capacity > 1, "Capacity must be greater than 1");

    protected:

#if UINTPTR_MAX == 0xFFFFFFFF
        using sequence_t = uint32_t;
#else
        using sequence_t = uint64_t;
#endif

        struct alignas(CACHELINE) Position {
            std::atomic<sequence_t> cursor{ 0 };
        };

        struct alignas(CACHELINE) Slot {
            std::atomic<sequence_t> sequence{ 0 };
            alignas(T) std::byte data[sizeof(T)];
        };

        static constexpr sequence_t MASK = Capacity - 1;

        std::unique_ptr<Slot[]> slots;
        Position producerPos;

        RingBufferQueueBase() : slots(new Slot[Capacity]) {
            for (size_t i = 0; i < Capacity; ++i)
                slots[i].sequence.store(static_cast<sequence_t>(i), std::memory_order_relaxed);
        }

        // Non-copyable, non-movable
        RingBufferQueueBase(const RingBufferQueueBase&) = delete;
        RingBufferQueueBase& operator=(const RingBufferQueueBase&) = delete;

    public:
        // Multi-producer push � CAS on producerPos
        template<typename U>
        bool try_push(U&& item) {
            sequence_t pos = producerPos.cursor.load(std::memory_order_relaxed);
            while (true) {
                Slot& slot = slots[pos & MASK];
                sequence_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                if (diff == 0) {
                    if (producerPos.cursor.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                        new (&slot.data) T(std::forward<U>(item));
                        slot.sequence.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                }
                else if (diff < 0) {
                    return false; // full
                }
                else {
                    pos = producerPos.cursor.load(std::memory_order_relaxed);
                }
            }
        }

        void reset() {
            producerPos.cursor.store(0, std::memory_order_relaxed);
            for (size_t i = 0; i < Capacity; ++i)
                slots[i].sequence.store(static_cast<sequence_t>(i), std::memory_order_relaxed);
        }
    };

    // -------------------------------------------------------------------------
    // MPSC � single consumer, no CAS on pop
    // -------------------------------------------------------------------------
    template<typename T, size_t Capacity>
    class RingBufferMPSCQueue : public RingBufferQueueBase<T, Capacity> {
        using Base = RingBufferQueueBase<T, Capacity>;
        using typename Base::sequence_t;
        using Base::slots;
        using Base::producerPos;
        using Base::MASK;

        alignas(CACHELINE) typename Base::Position consumerPos;

    public:
        ~RingBufferMPSCQueue() { while (try_pop().has_value()); }

        std::optional<T> try_pop() {
            sequence_t pos = consumerPos.cursor.load(std::memory_order_relaxed);
            typename Base::Slot& slot = slots[pos & MASK];
            sequence_t seq = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                consumerPos.cursor.store(pos + 1, std::memory_order_relaxed);
                T* ptr = reinterpret_cast<T*>(&slot.data);
                std::optional<T> result(std::move(*ptr));
                ptr->~T();
                slot.sequence.store(pos + MASK + 1, std::memory_order_release);
                return result;
            }
            return std::nullopt;
        }

        bool empty() const {
            sequence_t cons = consumerPos.cursor.load(std::memory_order_relaxed);
            const auto& slot = slots[cons & MASK];
            sequence_t seq = slot.sequence.load(std::memory_order_acquire);
            return static_cast<intptr_t>(seq) - static_cast<intptr_t>(cons + 1) != 0;
        }

        size_t size() const {
            sequence_t prod = producerPos.cursor.load(std::memory_order_acquire);
            sequence_t cons = consumerPos.cursor.load(std::memory_order_acquire);
            return static_cast<size_t>(prod - cons);
        }

        // in RingBufferSPSCQueue / RingBufferMPMCQueue / RingBufferMPSCQueue
        void reset() {
            while (try_pop().has_value());
            consumerPos.cursor.store(0, std::memory_order_relaxed);
            Base::reset();
        }
    };

    // -------------------------------------------------------------------------
    // MPMC � multiple consumers, CAS on pop
    // -------------------------------------------------------------------------
    template<typename T, size_t Capacity>
    class RingBufferMPMCQueue : public RingBufferQueueBase<T, Capacity> {
        using Base = RingBufferQueueBase<T, Capacity>;
        using typename Base::sequence_t;
        using Base::slots;
        using Base::producerPos;
        using Base::MASK;

        alignas(CACHELINE) typename Base::Position consumerPos;

    public:
        ~RingBufferMPMCQueue() { while (try_pop().has_value()); }

        std::optional<T> try_pop() {
            sequence_t pos = consumerPos.cursor.load(std::memory_order_relaxed);
            while (true) {
                typename Base::Slot& slot = slots[pos & MASK];
                sequence_t seq = slot.sequence.load(std::memory_order_acquire);
                intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                if (diff == 0) {
                    if (consumerPos.cursor.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                        T* ptr = reinterpret_cast<T*>(&slot.data);
                        std::optional<T> result(std::move(*ptr));
                        ptr->~T();
                        slot.sequence.store(pos + MASK + 1, std::memory_order_release);
                        return result;
                    }
                }
                else if (diff < 0) {
                    return std::nullopt; // empty
                }
                else {
                    pos = consumerPos.cursor.load(std::memory_order_relaxed);
                }
            }
        }

        bool empty() const {
            sequence_t cons = consumerPos.cursor.load(std::memory_order_relaxed);
            const auto& slot = slots[cons & MASK];
            sequence_t seq = slot.sequence.load(std::memory_order_acquire);
            return static_cast<intptr_t>(seq) - static_cast<intptr_t>(cons + 1) != 0;
        }

        size_t size() const {
            sequence_t prod = producerPos.cursor.load(std::memory_order_acquire);
            sequence_t cons = consumerPos.cursor.load(std::memory_order_acquire);
            return static_cast<size_t>(prod - cons);
        }

        // in RingBufferSPSCQueue / RingBufferMPMCQueue / RingBufferMPSCQueue
        void reset() {
            while (try_pop().has_value());
            consumerPos.cursor.store(0, std::memory_order_relaxed);
            Base::reset();
        }
    };

    // -------------------------------------------------------------------------
// SPSC — single producer, single consumer, no CAS anywhere
// -------------------------------------------------------------------------
    template<typename T, size_t Capacity>
    class RingBufferSPSCQueue {
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2");

        
#if UINTPTR_MAX == 0xFFFFFFFF
        using sequence_t = uint32_t;
#else
        using sequence_t = uint64_t;
#endif
        static_assert(Capacity <= std::numeric_limits<sequence_t>::max() / 2,
            "Capacity too large for sequence_t — risk of wraparound ambiguity");

        static constexpr sequence_t MASK = Capacity - 1;


        struct alignas(CACHELINE) Slot {
            alignas(T) std::byte data[sizeof(T)];
        };

        // Producer cache line — only written by producer thread
        alignas(CACHELINE) sequence_t _prod_head { 0 };      // producer's own index, plain
        sequence_t _cons_cache{ 0 };                         // producer's cached view of consumer

        // Consumer cache line — only written by consumer thread  
        alignas(CACHELINE) sequence_t _cons_head { 0 };      // consumer's own index, plain
        sequence_t _prod_cache{ 0 };                         // consumer's cached view of producer

        // Shared — read by both, written by each respective side
        // These are the "published" positions the other side reads
        alignas(CACHELINE) std::atomic<sequence_t> _prod_pub{ 0 };
        alignas(CACHELINE) std::atomic<sequence_t> _cons_pub{ 0 };

        alignas(CACHELINE) std::unique_ptr<Slot[]> _slots{ new Slot[Capacity] };

    public:
        template<typename U>
        bool try_push(U&& item) {
            sequence_t pos = _prod_head;
            sequence_t cons = _cons_cache;

            // Queue full by cached view — re-check the real consumer position
            if (pos - cons >= Capacity) {
                cons = _cons_cache = _cons_pub.load(std::memory_order_acquire);
                if (pos - cons >= Capacity)
                    return false;
            }

            new (&_slots[pos & MASK].data) T(std::forward<U>(item));

            // Publish to consumer — release so the constructed object is visible
            _prod_pub.store(++_prod_head, std::memory_order_release);
            return true;
        }

        std::optional<T> try_pop() {
            sequence_t pos = _cons_head;
            sequence_t prod = _prod_cache;

            // Queue empty by cached view — re-check the real producer position
            if (pos == prod) {
                prod = _prod_cache = _prod_pub.load(std::memory_order_acquire);
                if (pos == prod)
                    return std::nullopt;
            }

            T* ptr = reinterpret_cast<T*>(&_slots[pos & MASK].data);
            std::optional<T> result(std::move(*ptr));
            ptr->~T();

            // Publish to producer — release so the destructed slot is visible
            _cons_pub.store(++_cons_head, std::memory_order_release);
            return result;
        }

        bool empty() const {
            return _cons_pub.load(std::memory_order_acquire) ==
                _prod_pub.load(std::memory_order_acquire);
        }

        size_t size() const {
            return static_cast<size_t>(
                _prod_pub.load(std::memory_order_acquire) -
                _cons_pub.load(std::memory_order_acquire));
        }

        void reset() {
            while (try_pop().has_value());
            _prod_head = _cons_head = 0;
            _prod_cache = _cons_cache = 0;
            _prod_pub.store(0, std::memory_order_relaxed);
            _cons_pub.store(0, std::memory_order_relaxed);
        }

        ~RingBufferSPSCQueue() {
            while (try_pop().has_value());
        }
    };

} // namespace tsl