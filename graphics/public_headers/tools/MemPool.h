#pragma once
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <stdexcept>
#include <mutex>
#include <vector>
#include "defines.h"
#include <platform_config.h>
#include "aligned_memalloc.h"
// -----------------------------------------------------------------------------
// StaticPool — lock-free fixed-block pool, grows by adding arenas on exhaustion
//   - One arena preallocated at construction
//   - Grows under mutex on exhaustion (cold path only)
//   - Normal acquire/release fully lock-free
//   - Arenas never freed — memory grows to peak and stays
//   - 64-byte aligned (AVX-512 / ARM NEON / cache line)
// -----------------------------------------------------------------------------
namespace tsl
{


class StaticPool {
    struct Node { std::atomic<Node*> next; };

    struct Arena {
        uint8_t* mem = nullptr;
        std::atomic<uint8_t*> bump{ nullptr };
        uint8_t* end = nullptr;
        std::atomic<Arena*>   next{ nullptr };

        Arena() = default;
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;

        void init(size_t size) {
            mem = static_cast<uint8_t*>(aligned_malloc(size));
            if (!mem) throw std::bad_alloc();
            bump.store(mem, std::memory_order_relaxed);
            end = mem + size;
        }
        ~Arena() { aligned_free(mem); }
    };

public:

    // blockSize : bytes per block, rounded up to ALIGNMENT
    // arenaSize : bytes per slab — same size used for all grown arenas
    StaticPool(size_t blockSize, size_t arenaSize)
        : blockSize_(roundUp(blockSize))
        , arenaSize_(arenaSize)
    {
        assert(arenaSize_ >= blockSize_);
       // current_.store(newArena(), std::memory_order_release);
    }

    ~StaticPool() {
        Arena* a = current_.load(std::memory_order_relaxed);
        while (a) {
            Arena* prev = a->next.load(std::memory_order_relaxed);
            delete a;
            a = prev;
        }
    }

    StaticPool(const StaticPool&) = delete;
    StaticPool& operator=(const StaticPool&) = delete;

    // Returns ALIGNMENT-aligned pointer to blockSize_ bytes.
    // Grows arena chain if needed. Throws std::bad_alloc only on OS failure.
    void* acquire() {
        // 1. Free list — recycled blocks from any arena
        Node* old = freeHead_.load(std::memory_order_acquire);
        while (old) {
            if (freeHead_.compare_exchange_weak(
                old,
                old->next.load(std::memory_order_relaxed),
                std::memory_order_release,
                std::memory_order_acquire))
                return static_cast<void*>(old);
        }
        while (true) {
            Arena* a = current_.load(std::memory_order_acquire);
            if (!a) {
                grow(nullptr);
                continue;
            }

            uint8_t* expected = a->bump.load(std::memory_order_relaxed);

            if (expected + blockSize_ > a->end) {
                grow(a);
                continue;
            }

            if (a->bump.compare_exchange_weak(
                expected,
                expected + blockSize_,
                std::memory_order_relaxed))
                return static_cast<void*>(expected);
            // CAS lost — another thread bumped, retry
        }
    }

    // Return block to pool. Lock-free.
    void release(void* ptr) {
        if (!ptr) return;
        Node* node = static_cast<Node*>(ptr);
        Node* old = freeHead_.load(std::memory_order_relaxed);
        do {
            node->next.store(old, std::memory_order_relaxed);
        } while (!freeHead_.compare_exchange_weak(
            old, node,
            std::memory_order_release,
            std::memory_order_acquire));
    }

    size_t getBlockSize() const { return blockSize_; }

    int arenaCount() const {
        int n = 0;
        const Arena* a = current_.load(std::memory_order_relaxed);
        while (a) { ++n; a = a->next.load(std::memory_order_relaxed); }
        return n;
    }

private:
    void grow(Arena* exhausted) {
        Arena* fresh = newArena();
        fresh->next.store(exhausted, std::memory_order_release);  // publish next before CAS

        if (!current_.compare_exchange_strong(
            exhausted, fresh,
            std::memory_order_release,  // publish fresh to other threads
            std::memory_order_relaxed)) {
            fresh->next.store(nullptr, std::memory_order_relaxed);
            delete fresh;
        }
    }

    Arena* newArena() {
        Arena* a = new Arena();
        a->init(arenaSize_);
        return a;
    }

    static size_t roundUp(size_t n) {
        return (n + CACHELINE - 1) & ~(CACHELINE - 1);
    }

    const size_t        blockSize_;
    const size_t        arenaSize_;
    alignas(CACHELINE) std::atomic<Arena*> current_{ nullptr };
    alignas(CACHELINE) std::atomic<Node*>  freeHead_{ nullptr };
    alignas(CACHELINE) std::mutex          growMutex_;
};


// -----------------------------------------------------------------------------
// PoolSet — 4 size classes, each backed by a StaticPool
//
//   acquire(bytes) → 64-byte aligned pointer, nullptr if too large
//   release(ptr)   → self-routing via header, no size arg needed
//
//   Block layout (transparent to caller):
//   [size_t payloadClass | pad to CACHELINE | ... usable bytes ...]
//   ^raw                                    ^returned to caller
//
//   A size class is a power of two of PAYLOAD, and the block is that plus one
//   cache line for the header — deliberately NOT a power of two overall.
//
//   Sizing the whole block to a power of two instead is what the obvious version
//   does, and it doubles the memory for every caller whose request is already a
//   power of two: nextPow2(bytes + CACHELINE) sends an exact 8 KB ask into the
//   16 KB class purely to store 8 bytes of tag. That is the common case here, not
//   a corner one — audio buffers and wavetables are power-of-two sized by nature
//   (grainstorm's LFO table is exactly TBLSIZE3 doubles = 8192 bytes, which used
//   to take a 16 KB block). StaticPool is a bump allocator plus a free list and
//   never required a power-of-two block, so carrying the header outside the class
//   costs nothing and halves those allocations.
//
//   Default size classes (payload):
//     Tiny  :   8 KB
//     Small :  64 KB
//     Med   : 512 KB
//     Large :   4 MB
// -----------------------------------------------------------------------------
class PoolSet {
    static constexpr size_t MIN_BLOCK = 8 * 1024;
    static constexpr size_t MAX_POOL = 8 * 1024 * 1024;
    static constexpr size_t BLOCKS_PER_ARENA = 4;
    static constexpr size_t MAX_INDEX = 32;

    static size_t nextPow2(size_t n) {
        if (n <= MIN_BLOCK) return MIN_BLOCK;
        n--;
        n |= n >> 1; n |= n >> 2; n |= n >> 4;
        n |= n >> 8; n |= n >> 16; n |= n >> 32;
        return std::max(++n, MIN_BLOCK);
    }

    static size_t sizeIndex(size_t blockSize) {
        size_t idx = 0, s = MIN_BLOCK;
        while (s < blockSize) { s <<= 1; ++idx; }
        return idx;
    }

    // `payload` is the power-of-two size class; the pool's blocks carry one extra
    // cache line for the header, so the usable bytes are the full class.
    StaticPool* getOrCreate(size_t payload) {
        size_t idx = sizeIndex(payload);
        assert(idx < MAX_INDEX);

        StaticPool* p = slots_[idx].load(std::memory_order_acquire);
        if (p) return p;

        const size_t blockSize = payload + CACHELINE;
        StaticPool* fresh = new StaticPool(blockSize, blockSize * BLOCKS_PER_ARENA);
        StaticPool* expected = nullptr;

        if (!slots_[idx].compare_exchange_strong(
            expected, fresh,
            std::memory_order_release,
            std::memory_order_acquire)) {
            delete fresh;
            return expected;
        }
        return fresh;
    }

    std::atomic<StaticPool*> slots_[MAX_INDEX]{};

public:
    PoolSet() = default;

    ~PoolSet() {
        for (auto& slot : slots_)
            delete slot.load(std::memory_order_relaxed);
    }

    PoolSet(const PoolSet&) = delete;
    PoolSet& operator=(const PoolSet&) = delete;

    void* acquire(size_t bytes) {
        // Round the PAYLOAD to a power of two; the header rides outside the class.
        const size_t payload = nextPow2(bytes);
        void* raw;
        size_t tag;

        if (payload > MAX_POOL) {
            raw = aligned_malloc(bytes + CACHELINE);
            if (!raw) return nullptr;
            tag = 0;
        }
        else {
            StaticPool* pool = getOrCreate(payload);
            raw = pool->acquire();
            if (!raw) return nullptr;
            // Tag with the size CLASS, not the block size: release() routes with
            // sizeIndex(), which only inverts cleanly for a power of two.
            tag = payload;
        }

        static_cast<size_t*>(raw)[0] = tag;
        return static_cast<uint8_t*>(raw) + CACHELINE;
    }

    void release(void* ptr) {
        if (!ptr) return;
        void* raw = static_cast<uint8_t*>(ptr) - CACHELINE;
        size_t tag = *static_cast<size_t*>(raw);

        if (tag == 0) {
            aligned_free(raw);
            return;
        }
        // tag is the payload size CLASS (a power of two), which is what sizeIndex
        // inverts; the block it came from is one cache line larger than this.
        size_t idx = sizeIndex(tag);
        if (idx < MAX_INDEX) {
            StaticPool* p = slots_[idx].load(std::memory_order_acquire);
            if (p) p->release(raw);
        }
    }

    template<typename T>
    T* acquire(size_t count) {
        return static_cast<T*>(acquire(count * sizeof(T)));
    }

    void printStats() const {
        for (size_t i = 0; i < MAX_INDEX; ++i) {
            StaticPool* p = slots_[i].load(std::memory_order_relaxed);
            if (p) printf("  [%2zu] blockSize=%8zu  arenas=%d\n",
                i, p->getBlockSize(), p->arenaCount());
        }
    }
};


// -----------------------------------------------------------------------------
// PoolAllocator — STL allocator backed by PoolSet
// Use with std::vector, std::deque etc.
// Always reserve() upfront — vector reallocs burn pool blocks.
// -----------------------------------------------------------------------------
template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind { using other = PoolAllocator<U>; };

    explicit PoolAllocator(PoolSet* p) noexcept : pool_(p) {}

    template<typename U>
    PoolAllocator(const PoolAllocator<U>& o) noexcept : pool_(o.pool_) {}

    T* allocate(size_type n) {
        if (n == 0) return nullptr;
        void* p = pool_->acquire(n * sizeof(T));
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }

    void deallocate(T* p, size_type) noexcept { pool_->release(p); }

    bool operator==(const PoolAllocator& o) const noexcept { return pool_ == o.pool_; }
    bool operator!=(const PoolAllocator& o) const noexcept { return pool_ != o.pool_; }

    PoolSet* pool_;
};
template<typename T>
using PoolVector = std::vector<T, PoolAllocator<T>>;
}
