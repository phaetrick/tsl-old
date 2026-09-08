#pragma once
#include <memory>
#include <atomic>
#include <utility>
#include <cstddef>
#include <optional>
#include <semaphore>

namespace tsl {
    template<typename T>
    class SPSCQueue
    {
    public:
        static_assert(std::is_trivially_copyable<T>::value,
            "T must be trivially copyable (POD)");

        explicit SPSCQueue(size_t capacity)
            : m_capacity(nextPow2(capacity)),
            m_mask(m_capacity - 1),
            m_buffer(new T[m_capacity])
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
        }

        ~SPSCQueue()
        {
            delete[] m_buffer;
        }

        // Producer: push item, returns false if queue is full
        bool push(const T& item)
        {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t next = (head + 1) & m_mask;

            if (next == m_tail.load(std::memory_order_acquire))
                return false; // full

            m_buffer[head] = item;
            m_head.store(next, std::memory_order_release);
            return true;
        }

        // Consumer: pop item, returns false if empty
        bool pop(T& out)
        {
            const size_t tail = m_tail.load(std::memory_order_relaxed);

            if (tail == m_head.load(std::memory_order_acquire))
                return false; // empty

            out = m_buffer[tail];
            m_tail.store((tail + 1) & m_mask, std::memory_order_release);
            return true;
        }
        void reset()
        {
            // Safe only when producer & consumer are both paused.
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
        }

    private:
        static size_t nextPow2(size_t v)
        {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFF
            v |= v >> 32;
#endif
            v++;
            return v;
        }

        alignas(64) std::atomic<size_t> m_head;
        alignas(64) std::atomic<size_t> m_tail;

        const size_t m_capacity;
        const size_t m_mask;

        T* const m_buffer;
    };

    


    template<typename T, size_t BlockSize>
    class LockFreePool
    {
    public:
        explicit LockFreePool(size_t totalBlocks)
            : m_totalBlocks(totalBlocks)
        {
            m_storage = new Node[totalBlocks];

            // Initialize free list as a Treiber stack
            for (size_t i = 0; i < totalBlocks - 1; ++i)
                m_storage[i].next.store(&m_storage[i + 1], std::memory_order_relaxed);

            m_storage[totalBlocks - 1].next.store(nullptr, std::memory_order_relaxed);
            m_head.store(&m_storage[0], std::memory_order_release);
        }

        ~LockFreePool()
        {
            delete[] m_storage;
        }

        // Allocate one block (BlockSize*T)
        T* get()
        {
            Node* head = m_head.load(std::memory_order_acquire);
            while (head)
            {
                Node* next = head->next.load(std::memory_order_relaxed);
                if (m_head.compare_exchange_weak(
                    head, next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
                {
                    return head->data;
                }
            }
            return nullptr; // out of blocks
        }

        // Return block
        void release(T* ptr)
        {
            Node* node = reinterpret_cast<Node*>(
                reinterpret_cast<char*>(ptr) - offsetof(Node, data));

            Node* head = m_head.load(std::memory_order_acquire);
            do
            {
                node->next.store(head, std::memory_order_relaxed);
            } while (!m_head.compare_exchange_weak(
                head, node,
                std::memory_order_release,
                std::memory_order_relaxed));
        }
        void reset()
        {
            // Rebuild free-list
            for (size_t i = 0; i < m_totalBlocks - 1; ++i)
                m_storage[i].next.store(&m_storage[i + 1], std::memory_order_relaxed);

            m_storage[m_totalBlocks - 1].next.store(nullptr, std::memory_order_relaxed);

            // Reset stack head
            m_head.store(&m_storage[0], std::memory_order_release);
        }

    private:
        struct Node
        {
            std::atomic<Node*> next;
            T data[BlockSize];  // Fixed-size block
        };

        Node* m_storage;
        const size_t m_totalBlocks;

        std::atomic<Node*> m_head; // Treiber stack
    };

    template<typename T, size_t Capacity>
    class BlockingSPSCQueue {
        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2");
        static_assert(Capacity > 1, "Capacity must be greater than 1");

        // Union-based storage to avoid reinterpret_cast
        union Storage {
            Storage() {}
            ~Storage() {}
            T value;
        };

    public:
        BlockingSPSCQueue() : writeIndex(0), readIndex(0), dataAvailable(0), stopFlag(false) {}

        ~BlockingSPSCQueue() {
            clear();
        }

        bool push(const T& item) {
            if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            size_t w = writeIndex.load(std::memory_order_relaxed);
            size_t r = readIndex.load(std::memory_order_acquire);

            // Use safer capacity calculation to avoid potential overflow issues
            if ((w - r) >= Capacity) [[unlikely]] {
                return false;
            }

            new (&buffer[w & (Capacity - 1)].value) T(item);
            writeIndex.store(w + 1, std::memory_order_release);
            dataAvailable.release();
            return true;
        }

        bool push(T&& item) {
            if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                return false;
            }

            size_t w = writeIndex.load(std::memory_order_relaxed);
            size_t r = readIndex.load(std::memory_order_acquire);

            if ((w - r) >= Capacity) [[unlikely]] {
                return false;
            }

            new (&buffer[w & (Capacity - 1)].value) T(std::move(item));
            writeIndex.store(w + 1, std::memory_order_release);
            dataAvailable.release();
            return true;
        }

        /**
         * Blocking pop.
         * Returns std::nullopt if stopped.
         */
        std::optional<T> pop() {
            // Check stop flag before blocking
            if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                return std::nullopt;
            }

            dataAvailable.acquire();

            // Check again after acquiring semaphore
            if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                return std::nullopt;
            }

            size_t r = readIndex.load(std::memory_order_relaxed);
            T& value_ref = buffer[r & (Capacity - 1)].value;

            // Move construct the item first, then clean up
            T item = std::move(value_ref);
            value_ref.~T();
            readIndex.store(r + 1, std::memory_order_release);

            return item;
        }

        std::optional<T> try_pop() {
            if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                return std::nullopt;
            }

            if (dataAvailable.try_acquire()) {
                // Double-check stop flag after acquiring semaphore
                if (stopFlag.load(std::memory_order_acquire)) [[unlikely]] {
                    return std::nullopt;
                }

                size_t r = readIndex.load(std::memory_order_relaxed);
                T& value_ref = buffer[r & (Capacity - 1)].value;

                // Move construct the item first, then clean up
                T item = std::move(value_ref);
                value_ref.~T();
                readIndex.store(r + 1, std::memory_order_release);

                return item;
            }
            return std::nullopt;
        }

        void stop() {
            stopFlag.store(true, std::memory_order_release);
            // Release multiple times to handle potential race conditions
            // where multiple consumers might be waiting
            for (size_t i = 0; i < Capacity; ++i) {
                dataAvailable.release();
            }
        }

        void reset() {
            clear();
            stopFlag.store(false, std::memory_order_release);
            // Reset semaphore count to 0
            while (dataAvailable.try_acquire()) {}
        }

        bool stopped() const {
            return stopFlag.load(std::memory_order_acquire);
        }

        void clear() {
            while (try_pop_internal()) {}
        }

        size_t size() const {
            size_t w = writeIndex.load(std::memory_order_relaxed);
            size_t r = readIndex.load(std::memory_order_relaxed);
            return (w - r) & (Capacity - 1);
        }

        bool empty() const {
            return size() == 0;
        }

        bool full() const {
            size_t w = writeIndex.load(std::memory_order_relaxed);
            size_t r = readIndex.load(std::memory_order_relaxed);
            return (w - r) >= Capacity;
        }

    private:
        // Internal pop that doesn't check stop flag (used by clear)
        bool try_pop_internal() {
            if (dataAvailable.try_acquire()) {
                size_t r = readIndex.load(std::memory_order_relaxed);
                buffer[r & (Capacity - 1)].value.~T();
                readIndex.store(r + 1, std::memory_order_release);
                return true;
            }
            return false;
        }

        alignas(64) std::atomic<size_t> writeIndex;
        alignas(64) std::atomic<size_t> readIndex;
        alignas(64) std::atomic<bool> stopFlag;

        Storage buffer[Capacity];
        std::counting_semaphore<Capacity> dataAvailable;
    };
    
    template<typename T, uint32_t CAPACITY, typename INDEX_TYPE = uint32_t>
    class LockFreeQueue {
    public:
        /**
         * Implementation details:
         *
         * We have 2 counters: readCounter and writeCounter. Each will increment until it reaches
         * INDEX_TYPE_MAX, then wrap to zero. Unsigned integer overflow is defined behaviour in C++.
         *
         * Each time we need to access our data array we call mask() which gives us the index into the
         * array. This approach avoids having a "dead item" in the buffer to distinguish between full
         * and empty states. It also allows us to have a size() method which is easily calculated.
         *
         * IMPORTANT: This implementation is only thread-safe with a single reader thread and a single
         * writer thread. Have more than one of either will result in Bad Things™.
         */
        static constexpr bool isPowerOfTwo(uint32_t n) { return n > 0 && (n & (n - 1)) == 0; }
        static_assert(isPowerOfTwo(CAPACITY), "Capacity must be a power of 2");
        static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");
        static_assert(CAPACITY <= (std::numeric_limits<INDEX_TYPE>::max() / 2),
            "Capacity too large for index type");

        /**
         * Destructor - properly destroy any remaining elements
         */
        ~LockFreeQueue() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                clear();
            }
        }

        /**
         * Pop a value off the head of the queue
         *
         * @param val - element will be stored in this variable
         * @return true if value was popped successfully, false if the queue is empty
         */
        bool pop(T& val) {
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_relaxed);
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_acquire);

            if (readIndex == writeIndex) {
                return false; // Queue is empty
            }

            val = buffer[mask(readIndex)];
            readCounter.store(readIndex + 1, std::memory_order_release);
            return true;
        }

        /**
         * Pop with move semantics for better performance with expensive-to-copy types
         */
        bool popMove(T& val) {
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_relaxed);
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_acquire);

            if (readIndex == writeIndex) {
                return false; // Queue is empty
            }

            val = std::move(buffer[mask(readIndex)]);
            readCounter.store(readIndex + 1, std::memory_order_release);
            return true;
        }

        /**
         * Add an item to the back of the queue (copy version)
         *
         * @param item - The item to add
         * @return true if item was added, false if the queue was full
         */
        bool push(const T& item) {
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_relaxed);
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_acquire);

            if (writeIndex - readIndex == CAPACITY) {
                return false; // Queue is full
            }

            buffer[mask(writeIndex)] = item;
            writeCounter.store(writeIndex + 1, std::memory_order_release);
            return true;
        }

        /**
         * Add an item to the back of the queue (move version) - fixed parameter type
         */
        bool push(T&& item) {
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_relaxed);
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_acquire);

            if (writeIndex - readIndex == CAPACITY) {
                return false; // Queue is full
            }

            buffer[mask(writeIndex)] = std::move(item);
            writeCounter.store(writeIndex + 1, std::memory_order_release);
            return true;
        }

        /**
         * Emplace construct an item directly in the queue
         */
        template<typename... Args>
        bool emplace(Args&&... args) {
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_relaxed);
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_acquire);

            if (writeIndex - readIndex == CAPACITY) {
                return false; // Queue is full
            }

            new (&buffer[mask(writeIndex)]) T(std::forward<Args>(args)...);
            writeCounter.store(writeIndex + 1, std::memory_order_release);
            return true;
        }

        /**
         * Get the item at the front of the queue but do not remove it
         *
         * @param item - item will be stored in this variable
         * @return true if item was stored, false if the queue was empty
         */
        bool peek(T& item) const {
            INDEX_TYPE readIndex = readCounter.load(std::memory_order_relaxed);
            INDEX_TYPE writeIndex = writeCounter.load(std::memory_order_acquire);

            if (readIndex == writeIndex) {
                return false; // Queue is empty
            }

            item = buffer[mask(readIndex)];
            return true;
        }

        /**
         * Get a const reference to the front item (unsafe - caller must ensure queue is not empty)
         */
        const T& front() const {
            return buffer[mask(readCounter.load(std::memory_order_acquire))];
        }

        /**
         * Get the number of items in the queue
         *
         * @return number of items in the queue
         */
        INDEX_TYPE size() const {
            /**
             * This is worth some explanation:
             *
             * Whilst writeCounter is greater than readCounter the result of (write - read) will always
             * be positive. Simple.
             *
             * But when writeCounter is equal to INDEX_TYPE_MAX (e.g. UINT32_MAX) the next push will
             * wrap it around to zero, the start of the buffer, making writeCounter less than
             * readCounter so the result of (write - read) will be negative.
             *
             * But because we're returning an unsigned type return value will be as follows:
             *
             * returnValue = INDEX_TYPE_MAX - (write - read)
             *
             * e.g. if write is 0, read is 150 and the INDEX_TYPE is uint8_t where the max value is
             * 255 the return value will be (255 - (0 - 150)) = 105.
             */
            return writeCounter.load(std::memory_order_acquire) -
                readCounter.load(std::memory_order_acquire);
        }

        /**
         * Check if the queue is empty
         */
        bool empty() const {
            return readCounter.load(std::memory_order_acquire) ==
                writeCounter.load(std::memory_order_acquire);
        }

        /**
         * Check if the queue is full
         */
        bool full() const {
            return size() == CAPACITY;
        }

        /**
         * Get the maximum capacity of the queue
         */
        constexpr INDEX_TYPE capacity() const {
            return CAPACITY;
        }

        /**
         * Clear all items from the queue (not thread-safe - use only when no other threads are accessing)
         */
        void clear() {
            // Properly destruct remaining elements if needed
            if constexpr (!std::is_trivially_destructible_v<T>) {
                T temp;
                while (pop(temp)) {
                    // Elements are destructed automatically when temp goes out of scope
                }
            }

            readCounter.store(0, std::memory_order_relaxed);
            writeCounter.store(0, std::memory_order_relaxed);
        }

    private:
        constexpr INDEX_TYPE mask(INDEX_TYPE n) const {
            return static_cast<INDEX_TYPE>(n & (CAPACITY - 1));
        }

        // Separate cache lines to avoid false sharing
        alignas(64) std::atomic<INDEX_TYPE> writeCounter{ 0 };
        alignas(64) std::atomic<INDEX_TYPE> readCounter{ 0 };
        alignas(64) T buffer[CAPACITY]; // Align buffer to cache line boundary
    };
}
