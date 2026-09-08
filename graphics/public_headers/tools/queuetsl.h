#pragma once
#include "defines.h"
#include "platform_config.h"
#include "aligned_memalloc.h"
#include "tools/threadtsl.h"
#include <mutex>
#include <functional>
#include <vector>
#include <mutex>
#include <deque>
#include <array>      // For std::array
#include <utility>    // For std::move
#include <memory>

namespace tsl {
#include <cstdint>
#include <cstddef>
#include <new>
#include <memory>
#include <vector>
    template<typename T>
    struct FastQueue {

        // Pad each node to a full cache line to avoid false sharing during traversal
        // and ensure pointer chasing doesn't straddle lines unnecessarily.
        struct alignas(CACHELINE) QueueNode {
            T          data{};
            QueueNode* next{ nullptr };
            QueueNode* prev{ nullptr };
            // Implicit padding to CACHELINE inserted by alignas
        };

        static_assert(alignof(QueueNode) == CACHELINE, "Node must be cacheline-aligned");

        // Allocate a raw aligned buffer for 'capacity' nodes.
        // Using aligned_alloc so the first node is also cacheline-aligned.
        explicit FastQueue(std::size_t capacity = 250)
            : _capacity(capacity)
            , _buf(static_cast<QueueNode*>(
                ::operator new(capacity * sizeof(QueueNode), std::align_val_t{ CACHELINE })))
        {
            // Placement-construct all nodes, then seed the free list
            for (std::size_t i = 0; i < capacity; ++i)
                new (_buf + i) QueueNode{};

            // Build a singly-linked free stack via _nodes (no heap alloc per node)
            _nodes.reserve(capacity);
            for (std::size_t i = 0; i < capacity; ++i)
                _nodes.push_back(_buf + i);
        }

        ~FastQueue() {
            for (std::size_t i = 0; i < _capacity; ++i)
                (_buf + i)->~QueueNode();
            ::operator delete(_buf, std::align_val_t{ CACHELINE });
        }

        // Non-copyable; could add move if needed
        FastQueue(const FastQueue&) = delete;
        FastQueue& operator=(const FastQueue&) = delete;

        // -----------------------------------------------------------------------
        // Core interface
        // -----------------------------------------------------------------------

        // Returns pointer to new element's storage, or nullptr if pool exhausted.
        [[nodiscard]] T* push() {
            QueueNode* node = _alloc();
            if (!node) return nullptr;

            if (!_first) {
                _first = _last = node;
            }
            else {
                _last->next = node;
                node->prev = _last;
                _last = node;
            }
            return &node->data;
        }

        void pop_front() {
            if (!_first) return;
            QueueNode* old = _first;
            if (_first->next) {
                _first = _first->next;
                _first->prev = nullptr;
            }
            else {
                _first = _last = nullptr;
            }
            _free(old);
        }

        // Delete an arbitrary node by pointer; returns the node that followed it
        // (nullptr if it was the tail). Safe to use while iterating.
        QueueNode* del(QueueNode* node) {
            QueueNode* after = node->next;
            _unlink(node);
            _free(node);
            return after;
        }

        void flush() {
            _first = _last = nullptr;
            _nodes.clear();
            for (std::size_t i = 0; i < _capacity; ++i)
                _nodes.push_back(_buf + i);
        }

        // -----------------------------------------------------------------------
        // Accessors
        // -----------------------------------------------------------------------

        [[nodiscard]] QueueNode* front() const noexcept { return _first; }
        [[nodiscard]] QueueNode* back()  const noexcept { return _last; }

        [[nodiscard]] std::size_t size()     const noexcept { return _capacity - _nodes.size(); }
        [[nodiscard]] std::size_t capacity() const noexcept { return _capacity; }
        [[nodiscard]] bool        empty()    const noexcept { return _first == nullptr; }
        [[nodiscard]] bool        full()     const noexcept { return _nodes.empty(); }

        // O(n) positional access — kept for compatibility but prefer iteration
        T* at(std::size_t pos) noexcept {
            QueueNode* n = _first;
            for (std::size_t i = 0; n; ++i, n = n->next)
                if (i == pos) return &n->data;
            return nullptr;
        }

        alignas(CACHELINE) QueueNode* _first { nullptr };
    private:
        // -----------------------------------------------------------------------
        // Internal helpers
        // -----------------------------------------------------------------------

        QueueNode* _alloc() {
            if (_nodes.empty()) return nullptr;
            QueueNode* n = _nodes.back();
            _nodes.pop_back();
            n->next = n->prev = nullptr;
            n->data = T{};
            return n;
        }

        void _free(QueueNode* n) {
            _nodes.push_back(n);
        }

        void _unlink(QueueNode* node) noexcept {
            if (node == _first) {
                _first = node->next;
                if (_first) _first->prev = nullptr;
                else        _last = nullptr;
            }
            else if (node == _last) {
                _last = node->prev;
                _last->next = nullptr;
            }
            else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
            }
        }

        // -----------------------------------------------------------------------
        // Data — hot fields first, grouped to avoid cross-cacheline loads
        // -----------------------------------------------------------------------

        QueueNode* _last{ nullptr };

        alignas(CACHELINE) QueueNode* _buf { nullptr };
        std::size_t           _capacity{ 0 };
        std::vector<QueueNode*> _nodes;   // free stack
    };


    template<typename T>
    struct NodeQ {
        T         data{};
        int       prio{ 0 };
        NodeQ<T>* next{ nullptr };
        NodeQ<T>* prev{ nullptr };

        void clear(const T& nil_val = T{}) {
            data = nil_val;
            next = nullptr;
            prev = nullptr;
            prio = 0;
        }

        bool operator==(const T& other) const { return data == other; }

        NodeQ& operator=(const NodeQ<T>& o) {
            data = o.data;
            prio = o.prio;
            next = nullptr;
            prev = nullptr;
            return *this;
        }
    };


    template<typename T, std::size_t CAPACITY = 128>
    class QueueUnsafe : public std::recursive_mutex {
        static_assert(CAPACITY > 0, "CAPACITY must be > 0");
        static_assert(CAPACITY <= 65535, "CAPACITY too large for uint16_t indexing");

    public:
        // -----------------------------------------------------------------------
        // Hot fields — head/tail on their own cache line
        // -----------------------------------------------------------------------
        alignas(CACHELINE) NodeQ<T>* _first{ nullptr };
        NodeQ<T>* _last{ nullptr };

        // -----------------------------------------------------------------------
        // Construction / destruction
        // -----------------------------------------------------------------------
        QueueUnsafe() { init_pool(); }
        QueueUnsafe(int /*size*/) = delete;

        QueueUnsafe(const QueueUnsafe& o) { init_pool(); *this = o; }

        ~QueueUnsafe() {
            for (auto& n : _avail) n.clear();
        }

        // -----------------------------------------------------------------------
        // Iterator
        // -----------------------------------------------------------------------
        class iterator {
            friend class QueueUnsafe<T, CAPACITY>;
            NodeQ<T>* _node;
        public:
            explicit iterator(NodeQ<T>* node) : _node(node) {}

            iterator& operator++() { _node = _node->next; return *this; }
            iterator  operator++(int) { iterator tmp = *this; ++(*this); return tmp; }

            bool operator==(iterator o) const { return _node == o._node; }
            bool operator!=(iterator o) const { return !(*this == o); }

            T& operator*() { return  _node->data; }
            T* operator->() { return &_node->data; }

            using difference_type = std::ptrdiff_t;
            using value_type = T;
            using pointer = T*;
            using reference = T&;
            using iterator_category = std::forward_iterator_tag;
        };

        iterator begin() { return iterator(_first); }
        iterator end() { return iterator(nullptr); }

        iterator erase(iterator pos) {
            if (!pos._node) return end();
            NodeQ<T>* next = pos._node->next;
            del(pos._node);
            return iterator(next);
        }

        // -----------------------------------------------------------------------
        // Operators / named access
        // -----------------------------------------------------------------------
        QueueUnsafe& operator=(const QueueUnsafe& o) {
            if (this == &o) return *this;
            reset();
            for (auto* n = o._first; n && !isFull(); n = n->next) {
                NodeQ<T>* node = newNode();
                node->data = n->data;
                node->prio = n->prio;
                if (!_first) {
                    _first = _last = node;
                }
                else {
                    _last->next = node;
                    node->prev = _last;
                    _last = node;
                }
            }
            return *this;
        }

        T& operator[](int pos) { return get(pos); }
        T& at(int pos) { return get(pos); }

        // -----------------------------------------------------------------------
        // Mutators
        // -----------------------------------------------------------------------
        void reset() {
            _first = _last = nullptr;
            init_pool();
        }

        // Priority-sorted insert with duplicate detection. Returns 1 on success, 0 on
        // duplicate or full.
        int add(T task, int prio = 0) {
            if (isFull()) return 0;

            // Duplicate scan + find insertion point in one pass
            NodeQ<T>* prev = nullptr;
            NodeQ<T>* cur = _first;
            while (cur && cur->prio >= prio) {
                if (cur->data == task) return 0;   // duplicate
                prev = cur;
                cur = cur->next;
            }
            // Also scan remaining lower-priority nodes for duplicates
            for (auto* n = cur; n; n = n->next)
                if (n->data == task) return 0;

            NodeQ<T>* node = newNode();
            node->data = std::move(task);
            node->prio = prio;
            insertNode(node, prev, cur);
            return 1;
        }

        // Plain tail-push, no priority, no duplicate check.
        // Returns nullptr on full, pointer to stored data otherwise.
        T* push(T data) {
            if (isFull()) return nullptr;
            NodeQ<T>* node = newNode();
            node->data = std::move(data);
            if (!_first) {
                _first = _last = node;
            }
            else {
                _last->next = node;
                node->prev = _last;
                _last = node;
            }
            return &node->data;
        }

        // Plain tail-push, no priority, no duplicate check.
        // Returns nullptr on full, pointer to stored data otherwise.
        T* insertFirst(T data) {
            if (isFull()) return nullptr;
            NodeQ<T>* node = newNode();
            node->data = std::move(data);
            if (!_first) {
                _first = _last = node;
            }
            else {
                _first->prev = node;
                node->next = _first;
                _first = node;
            }
            return &node->data;
        }

        void del(NodeQ<T>* node) {
            if (!node) return;
            removeFromList(node);
            returnNode(node);
        }

        // Returns 1 if found and deleted, 0 otherwise.
        int del(T data) {
            for (auto* n = _first; n; n = n->next) {
                if (n->data == data) { del(n); return 1; }
            }
            return 0;
        }

        // -----------------------------------------------------------------------
        // Queries
        // -----------------------------------------------------------------------
        [[nodiscard]] int  size()   const { return static_cast<int>(CAPACITY - _free_count); }
        [[nodiscard]] bool isFull() const { return _free_count == 0; }
        [[nodiscard]] bool empty()  const { return _first == nullptr; }

        // Returns nullptr if not found.
        NodeQ<T>* get(const T& data) {
            for (auto* n = _first; n; n = n->next)
                if (n->data == data) return n;
            return nullptr;
        }

        // Returns -1 if not found.
        int pos(const T& data) const {
            int i = 0;
            for (auto* n = _first; n; n = n->next, ++i)
                if (n->data == data) return i;
            return -1;
        }

    protected:
        // -----------------------------------------------------------------------
        // Pool storage — each node cacheline-aligned, contiguous array
        // -----------------------------------------------------------------------
        alignas(CACHELINE) std::array<NodeQ<T>, CAPACITY> _avail;

        // Free-list stack. uint16_t saves space; CAPACITY <= 65535 enforced above.
        alignas(CACHELINE) std::array<uint16_t, CAPACITY> _free_indices;
        uint16_t _free_count{ 0 };

        // -----------------------------------------------------------------------
        // Pool helpers
        // -----------------------------------------------------------------------
        void init_pool() {
            _free_count = static_cast<uint16_t>(CAPACITY);
            for (uint16_t i = 0; i < CAPACITY; ++i) {
                _avail[i].clear();
                _free_indices[i] = i;
            }
            _first = _last = nullptr;
        }

        NodeQ<T>* newNode() {
            if (_free_count == 0) return nullptr;
            uint16_t  idx = _free_indices[--_free_count];
            NodeQ<T>* n = &_avail[idx];
            n->prio = 0;
            n->data = T{};
            n->next = n->prev = nullptr;
            return n;
        }

        void returnNode(NodeQ<T>* node) {
            node->clear();
            _free_indices[_free_count++] = static_cast<uint16_t>(node - _avail.data());
        }

        void insertNode(NodeQ<T>* node, NodeQ<T>* prev, NodeQ<T>* next) {
            node->prev = prev;
            node->next = next;
            if (prev)  prev->next = node; else _first = node;
            if (next)  next->prev = node; else _last = node;
        }

        void removeFromList(NodeQ<T>* node) {
            if (node->prev) node->prev->next = node->next; else _first = node->next;
            if (node->next) node->next->prev = node->prev; else _last = node->prev;
        }

        // Bidirectional index access — searches from nearest end. Returns sentinel
        // ref on out-of-bounds (consistent with original behavior, caller beware).
        T& get(int index) {
            static T sentinel{};
            int sz = size();
            if (index < 0 || index >= sz) return sentinel;

            if (index <= sz / 2) {
                int i = 0;
                for (auto* n = _first; n; n = n->next, ++i)
                    if (i == index) return n->data;
            }
            else {
                int i = sz - 1;
                for (auto* n = _last; n; n = n->prev, --i)
                    if (i == index) return n->data;
            }
            return sentinel; // unreachable, but silences warnings
        }
    };

    class CQueueOneTask {
    public:
        CQueueOneTask() {
            thread = std::thread(&CQueueOneTask::run, this);
        }

        ~CQueueOneTask() {
            shouldExit.store(true);
            add_task([]() {});
            if (thread.joinable())thread.join();
        }

        template<class F, class... Args>
        bool add_task(F f, Args &&... args) {
            if (!lock.test_and_set(std::memory_order_release)) {
                func = std::bind(f, std::forward<Args>(args)...);
                lock.notify_one();
                return true;
            }
            return false;
        }

        bool add_task(std::function<void()> &f) {
            if (!lock.test_and_set(std::memory_order_release)) {
                func = f;
                lock.notify_one();
                return true;
            }
            return false;
        }

    private:
        std::thread thread;

        void run() {
            do {
                if (shouldExit)break;
                lock.wait(false, std::memory_order_acquire);
                func();
                lock.clear();
            } while (true);
        }

        std::atomic_flag lock{};

        std::atomic<bool> shouldExit{};
        std::function<void()> func;
    };


    template<uint32_t CAPACITY = 128, typename INDEX_TYPE = uint32_t>
    class CQueue {
        static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be a power of two");

    public:
        CQueue() {
            workerThread = std::thread(&CQueue::run, this);
        }

        ~CQueue() {
            exit();
        }

        template<class F, class... Args>
        bool add_task(F &&f, Args &&... args) {
            lock_queue();
            if (isFull()) {
                unlock_queue();
                return false;
            }

            tasks[mask(writePos)] = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            ++writePos;

            unlock_queue();

            hasWork.store(true, std::memory_order_release);
            hasWork.notify_one();
            return true;
        }

        template<class F, class... Args>
        int add_taskInt(F &&f, Args &&... args) {
            lock_queue();
            if (isFull()) {
                unlock_queue();
                return 0;
            }
            auto s = size();
            tasks[mask(writePos)] = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            ++writePos;

            unlock_queue();

            hasWork.store(true, std::memory_order_release);
            hasWork.notify_one();
            return s;
        }

        void exit() {
            // signal exit

            exitFlag.store(true, std::memory_order_release);
            clear(); // clear tasks to avoid dangling references
            hasWork.store(true, std::memory_order_release);
            hasWork.notify_one();

            if (workerThread.joinable())
                workerThread.join();
        }

        void clear() {
            lock_queue();
            readPos = writePos = 0;
            for (auto &task: tasks) {
                task = {};
            }
            unlock_queue();
        }

    private:
        std::function<void()> tasks[CAPACITY]{};
        std::atomic_flag lock = ATOMIC_FLAG_INIT;
        std::atomic<bool> hasWork{false};
        std::atomic<bool> exitFlag{false};
        std::thread workerThread;

        INDEX_TYPE readPos = 0;
        INDEX_TYPE writePos = 0;

        INDEX_TYPE mask(INDEX_TYPE n) const {
            return static_cast<INDEX_TYPE>(n & (CAPACITY - 1));
        }

        INDEX_TYPE size() const {
            return writePos - readPos;
        }

        bool isEmpty() const {
            return readPos == writePos;
        }

        bool isFull() const {
            return size() == CAPACITY;
        }

        void lock_queue() {
            while (lock.test_and_set(std::memory_order_acquire));
        }

        void unlock_queue() {
            lock.clear(std::memory_order_release);
        }

        void run() {
            while (true) {
                // Wait until there’s work or exit signal
                hasWork.wait(false, std::memory_order_relaxed);


                while (true) {
                    lock_queue();

                    if (isEmpty()) {
                        unlock_queue();
                        break;
                    }

                    auto &slot = tasks[mask(readPos)];
                    auto task = std::move(slot);
                    slot = {};
                    ++readPos;

                    unlock_queue();

                    task(); // Run the task
                }

                hasWork.store(false, std::memory_order_release);
                if (exitFlag.load(std::memory_order_acquire))
                    break;

            }
        }
    };
};