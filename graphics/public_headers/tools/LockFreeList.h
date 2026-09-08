#pragma once
#include <atomic>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <vector>

namespace tsl {
    template<typename T>
    class AppendList {
        struct Node {
            T                  data;
            std::atomic<Node*> next{ nullptr };
            template<typename... Args>
            explicit Node(Args&&... args) : data(std::forward<Args>(args)...) {}
        };

    public:
        AppendList() = default;
        ~AppendList() {
            Node* n = head_.load(std::memory_order_relaxed);
            while (n) { Node* x = n->next.load(std::memory_order_relaxed); delete n; n = x; }
        }

        AppendList(const AppendList&) = delete;
        AppendList& operator=(const AppendList&) = delete;

        template<typename... Args>
        uint32_t emplace(Args&&... args) {
            std::lock_guard lock(mutex_);
            Node* n = new Node(std::forward<Args>(args)...);
            if (tail_) tail_->next.store(n, std::memory_order_release);
            else       head_.store(n, std::memory_order_release);
            tail_ = n;
            index_.push_back(&n->data);
            return static_cast<uint32_t>(index_.size() - 1);
        }

        uint32_t push(const T& v) { return emplace(v); }
        uint32_t push(T&& v) { return emplace(std::move(v)); }

        T& operator[](uint32_t idx) const noexcept { return *index_[idx]; }
        uint32_t size() const { std::lock_guard l(mutex_); return static_cast<uint32_t>(index_.size()); }

        // Traversal — lock-free, oldest first
        struct iterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            Node* ptr = nullptr;
            Node* snapshot_tail = nullptr;

            iterator() = default;
            iterator(Node* p, Node* t) : ptr(p), snapshot_tail(t) {}

            T& operator*()  const noexcept { return ptr->data; }
            T* operator->() const noexcept { return &ptr->data; }
            iterator& operator++() noexcept {
                if (ptr == snapshot_tail) { ptr = nullptr; return *this; }
                ptr = ptr->next.load(std::memory_order_acquire);
                return *this;
            }
            iterator  operator++(int) noexcept { auto t = *this; ++*this; return t; }
            bool operator==(const iterator& o) const noexcept { return ptr == o.ptr; }
            bool operator!=(const iterator& o) const noexcept { return ptr != o.ptr; }
        };

        iterator begin() const noexcept {
            Node* t = tail_;
            Node* h = head_.load(std::memory_order_acquire);
            return iterator{ h, t };
        }
        iterator end() const noexcept { return iterator{ nullptr, nullptr }; }

    private:
        std::atomic<Node*>  head_{ nullptr };
        Node* tail_ = nullptr;
        std::vector<T*>     index_;
        mutable std::mutex  mutex_;
    };
 
}
