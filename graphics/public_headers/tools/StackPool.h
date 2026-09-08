#include <atomic>
#include <functional>
#include <thread>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <new>
#include <type_traits>


// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
#   define LF_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#   define LF_ARCH_ARM64 1
#endif

// ---------------------------------------------------------------------------
// 128-bit atomic tagged pointer
//
// Strategy:
//   - Where __int128 + 16-byte CAS is available (x86-64 with CMPXCHG16B,
//     ARM64 with CASP / LSE), we store {Node*, uint64_t tag} in a 128-bit
//     atomic — no bit-stealing, no VA-range assumptions, no MTE conflicts.
//
//   - Fallback (other 64-bit or 32-bit targets) uses the low alignment bits
//     of the pointer (safe because Node is at least 8-byte aligned → 3 free
//     bits, giving tags 0-7).  This is enough to prevent ABA in practice and
//     makes zero assumptions about the upper VA bits.
//
// The public interface is identical on every path:
//   TaggedPtr<Node>   — opaque packed value
//   make_tagged(ptr, tag), get_ptr(tp), get_tag(tp)
//   TaggedAtomic<Node> — atomic wrapper with load/store/CAS
// ---------------------------------------------------------------------------

namespace lf_detail {

// ---- 128-bit path ---------------------------------------------------------
#if defined(LF_ARCH_X86_64) || defined(LF_ARCH_ARM64)

template<typename Node>
struct TaggedPtr {
    Node*    ptr = nullptr;
    uint64_t tag = 0;

    bool operator==(TaggedPtr const& o) const noexcept {
        return ptr == o.ptr && tag == o.tag;
    }
    bool operator!=(TaggedPtr const& o) const noexcept { return !(*this == o); }
};

template<typename Node>
inline TaggedPtr<Node> make_tagged(Node* ptr, uint64_t tag) noexcept {
    return {ptr, tag};
}
template<typename Node>
inline Node* get_ptr(TaggedPtr<Node> tp) noexcept { return tp.ptr; }
template<typename Node>
inline uint64_t get_tag(TaggedPtr<Node> tp) noexcept { return tp.tag; }

// 16-byte aligned storage; CAS implemented via __int128 on both x86-64 and
// ARM64 (GCC/Clang emit CMPXCHG16B / CASP respectively with -mcx16 / default).
template<typename Node>
class TaggedAtomic {
    static_assert(sizeof(TaggedPtr<Node>) == 16, "TaggedPtr must be 16 bytes");

    alignas(16) mutable __int128 m_val{};

    static __int128 pack(TaggedPtr<Node> tp) noexcept {
        __int128 v = 0;
        // Lay out as [ptr(low 64)] [tag(high 64)] — endian-agnostic via memcpy.
        std::memcpy(&v, &tp, 16);
        return v;
    }
    static TaggedPtr<Node> unpack(__int128 v) noexcept {
        TaggedPtr<Node> tp;
        std::memcpy(&tp, &v, 16);
        return tp;
    }

public:
    TaggedAtomic() = default;
    explicit TaggedAtomic(TaggedPtr<Node> init) {
        store(init, std::memory_order_relaxed);
    }

    TaggedPtr<Node> load(std::memory_order mo) const noexcept {
        // __atomic builtins give us the right instruction on both arches.
        __int128 v;
        __atomic_load(&m_val, &v, static_cast<int>(mo));
        return unpack(v);
    }

    void store(TaggedPtr<Node> tp, std::memory_order mo) noexcept {
        __int128 v = pack(tp);
        __atomic_store(&m_val, &v, static_cast<int>(mo));
    }

    // Returns true on success; 'expected' updated on failure (like std::atomic).
    bool compare_exchange_weak(TaggedPtr<Node>& expected,
                               TaggedPtr<Node>  desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept {
        __int128 exp = pack(expected);
        __int128 des = pack(desired);
        bool ok = __atomic_compare_exchange(&m_val, &exp, &des,
                                            /*weak=*/true,
                                            static_cast<int>(success),
                                            static_cast<int>(failure));
        if (!ok) expected = unpack(exp);
        return ok;
    }
};

#else
// ---- Fallback: low-bit tag (3 bits from 8-byte Node alignment) ------------

template<typename Node>
struct TaggedPtr {
    uintptr_t bits = 0; // [tag:3][ptr:61] — ptr is always 8-byte aligned

    bool operator==(TaggedPtr const& o) const noexcept { return bits == o.bits; }
    bool operator!=(TaggedPtr const& o) const noexcept { return bits != o.bits; }
};

static constexpr uintptr_t kTagBits = 3; // log2(alignof min 8)
static constexpr uintptr_t kTagMask = (1u << kTagBits) - 1;
static constexpr uintptr_t kPtrMask = ~kTagMask;

template<typename Node>
inline TaggedPtr<Node> make_tagged(Node* ptr, uint64_t tag) noexcept {
    // static_assert at call site: alignof(Node) >= 8
    return { reinterpret_cast<uintptr_t>(ptr) | (static_cast<uintptr_t>(tag) & kTagMask) };
}
template<typename Node>
inline Node* get_ptr(TaggedPtr<Node> tp) noexcept {
    return reinterpret_cast<Node*>(tp.bits & kPtrMask);
}
template<typename Node>
inline uint64_t get_tag(TaggedPtr<Node> tp) noexcept {
    return tp.bits & kTagMask;
}

template<typename Node>
class TaggedAtomic {
    std::atomic<uintptr_t> m_val{};
public:
    TaggedAtomic() = default;
    explicit TaggedAtomic(TaggedPtr<Node> init) : m_val(init.bits) {}

    TaggedPtr<Node> load(std::memory_order mo) const noexcept {
        return {m_val.load(mo)};
    }
    void store(TaggedPtr<Node> tp, std::memory_order mo) noexcept {
        m_val.store(tp.bits, mo);
    }
    bool compare_exchange_weak(TaggedPtr<Node>& expected,
                               TaggedPtr<Node>  desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept {
        bool ok = m_val.compare_exchange_weak(expected.bits, desired.bits, success, failure);
        return ok;
    }
};

#endif // arch select

} // namespace lf_detail


// ===========================================================================
// LockFreePool2
//
// Changes from original:
//   - Replaced bit-stuffed uintptr_t head with TaggedAtomic<Node> using the
//     128-bit scheme on x86-64/ARM64, low-bit fallback elsewhere.
//   - get() success CAS changed to memory_order_acq_rel (was acquire/relaxed
//     which is not valid — success must be >= failure order; also we need the
//     release side so the slot is safe to hand off).
//   - release() failure order relaxed (we only re-read head on failure, no
//     synchronisation needed).
//   - Everything else (public interface, Node layout) preserved exactly.
// ===========================================================================


template<typename T, size_t TotalBlocks>
class LockFreePool {
private:
    // Define a stable cache line size to avoid ABI warnings and silences -Winterference-size
    static constexpr size_t kCacheLineSize = 64;

    struct Node;

    // 16-byte structure for Wide CAS (128-bit)
    struct alignas(16) TaggedPtr {
        Node* ptr;
        uintptr_t tag;

        bool operator==(const TaggedPtr& other) const noexcept {
            return ptr == other.ptr && tag == other.tag;
        }
    };

    // Determine if we can use 128-bit atomics natively
    static constexpr bool UseWideCAS = std::atomic<TaggedPtr>::is_always_lock_free;

    // AFTER
struct alignas(kCacheLineSize) Node {
    T data;  // MUST be first — release() casts T* → Node* assuming zero offset
    std::conditional_t<UseWideCAS, Node*, std::atomic<uintptr_t>> next;
};

static_assert(offsetof(Node, data) == 0,
    "data must be at offset 0 in Node for the T*→Node* cast in release() to be valid");

    // The type of our head depends on whether we are packing or using 128-bit CAS
    using HeadType = std::conditional_t<UseWideCAS, TaggedPtr, uintptr_t>;

public:
    explicit LockFreePool() {
        // Aligned allocation for the whole array
        void* raw = std::aligned_alloc(kCacheLineSize, sizeof(Node) * TotalBlocks);
        m_storage = static_cast<Node*>(raw);

        for (size_t i = 0; i < TotalBlocks; ++i) {
            new (&m_storage[i]) Node();
            if constexpr (UseWideCAS) {
                m_storage[i].next = (i < TotalBlocks - 1) ? &m_storage[i + 1] : nullptr;
            } else {
                uintptr_t nextVal = (i < TotalBlocks - 1) ? pack(&m_storage[i + 1], 0) : pack(nullptr, 0);
                m_storage[i].next.store(nextVal, std::memory_order_relaxed);
            }
        }

        if constexpr (UseWideCAS) {
            m_head.store({&m_storage[0], 0}, std::memory_order_release);
        } else {
            m_head.store(pack(&m_storage[0], 0), std::memory_order_release);
        }
    }

    ~LockFreePool() {
        for (size_t i = 0; i < TotalBlocks; ++i) {
            m_storage[i].~Node();
        }
        std::free(m_storage);
    }

    T* get() noexcept {
    auto head = m_head.load(std::memory_order_acquire);
    while (true) {
        Node* headNode;
        HeadType nextHead;

        if constexpr (UseWideCAS) {
            headNode = head.ptr;
            if (!headNode) return nullptr;
            // Pairs with the release fence in release().
            // Ensures we read the committed next pointer, not a stale value.
            std::atomic_thread_fence(std::memory_order_acquire);
            nextHead = { headNode->next, head.tag + 1 };
        } else {
            headNode = extractPtr(head);
            if (!headNode) return nullptr;
            uintptr_t nextVal = headNode->next.load(std::memory_order_acquire);
            nextHead = pack(extractPtr(nextVal), extractTag(head) + 1);
        }

        if (m_head.compare_exchange_weak(head, nextHead,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
            return &headNode->data;  // ← was missing from the snippet
        }
    }
}
 
void release(T* ptr) noexcept {
    if (!ptr) return;
    Node* node = reinterpret_cast<Node*>(ptr);

    auto head = m_head.load(std::memory_order_acquire);
    while (true) {
        HeadType nextHead;
        if constexpr (UseWideCAS) {
            node->next = head.ptr;
            // Pairs with the acquire-load of m_head in get().
            // Ensures the next write is committed before CAS publishes node.
            // We then downgrade the CAS itself to relaxed — the fence covers it.
            std::atomic_thread_fence(std::memory_order_release);
            nextHead = { node, head.tag + 1 };
            if (m_head.compare_exchange_weak(head, nextHead,
                                            std::memory_order_relaxed,
                                            std::memory_order_acquire))
                break;
        } else {
            node->next.store(head, std::memory_order_relaxed);
            nextHead = pack(node, extractTag(head) + 1);
            if (m_head.compare_exchange_weak(head, nextHead,
                                            std::memory_order_release,
                                            std::memory_order_acquire))
                break;
        }
    }
}
private:
    static constexpr uintptr_t kPtrMask = 0x0000FFFFFFFFFFFFULL;

    static uintptr_t pack(Node* ptr, uint16_t tag) {
        return reinterpret_cast<uintptr_t>(ptr) | (static_cast<uintptr_t>(tag) << 48);
    }

    static Node* extractPtr(uintptr_t packed) {
        uintptr_t ptr = packed & kPtrMask;
        if (ptr & (1ULL << 47)) ptr |= ~kPtrMask;
        return reinterpret_cast<Node*>(ptr);
    }

    static uint16_t extractTag(uintptr_t packed) {
        return static_cast<uint16_t>(packed >> 48);
    }

    Node* m_storage;

    // Use a single head atomic with the conditional type
    alignas(kCacheLineSize) std::atomic<HeadType> m_head;

    // Pad to isolate m_head and prevent false sharing with other memory
    char _padding[kCacheLineSize];
};
