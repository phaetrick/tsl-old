#include "tools/WaitNotify.h"
#include <iostream>
#include <unordered_map>

using namespace tsl;

int ThreadSignal::acquire_slot() {
    thread_local std::unordered_map<void*, int> instance_to_slot_map;

    auto it = instance_to_slot_map.find(this);
    if (it != instance_to_slot_map.end()) {
        return it->second;
    }

    int s = next_slot_.fetch_add(1, std::memory_order_relaxed);

    if (s >= max_threads_) {
        std::terminate();
    }

    // Ignore all wakeups that happened BEFORE slot acquisition.
    slots_[s].observed_generation =
        slots_[s].generation.load(std::memory_order_acquire);

    instance_to_slot_map[this] = s;
    return s;
}

void ThreadSignal::wake_thread(int slot) noexcept {
    if (slot < 0 || slot >= max_threads_) {
        return;
    }

    auto& s = slots_[slot];

    s.generation.fetch_add(1, std::memory_order_acq_rel);

    // counting_semaphore required here because multiple wakeups
    // may legally accumulate before waiter runs.
    s.sem.release();
}

void ThreadSignal::wake_all() noexcept {
    int active_slots =
        std::min(
            next_slot_.load(std::memory_order_acquire),
            max_threads_);

    for (int i = 0; i < active_slots; ++i) {
        wake_thread(i);
    }
}

void ThreadSignal::wait_for_signal() {
    int slot = acquire_slot();
    auto& s = slots_[slot];

    while (true) {
        if (shutdown_.load(std::memory_order_acquire)) {
            return;
        }

        uint64_t now =
            s.generation.load(std::memory_order_acquire);

        if (now != s.observed_generation) {
            s.observed_generation = now;
            return;
        }

        s.sem.acquire();
    }
}

bool ThreadSignal::wait_for_signal(long millis) {
    int slot = acquire_slot();
    auto& s = slots_[slot];

    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(millis);

    while (true) {
        if (shutdown_.load(std::memory_order_acquire)) {
            return false;
        }

        uint64_t now =
            s.generation.load(std::memory_order_acquire);

        if (now != s.observed_generation) {
            s.observed_generation = now;
            return true;
        }

        auto remain =
            deadline - std::chrono::steady_clock::now();

        if (remain <= std::chrono::nanoseconds(0)) {
            return false;
        }

        if (!s.sem.try_acquire_for(remain)) {
            // Final race check at timeout boundary
            now = s.generation.load(std::memory_order_acquire);

            if (now != s.observed_generation) {
                s.observed_generation = now;
                return true;
            }

            return false;
        }
    }
}

ThreadSignal::WaitToken ThreadSignal::begin_wait() {
    int s = acquire_slot();
    // Tickets are issued only by the slot-owning thread; no atomicity needed.
    return { s, ++slots_[s].next_ticket };
}

void ThreadSignal::complete(WaitToken t) noexcept {
    if (t.slot < 0 || t.slot >= max_threads_ || t.ticket == 0) {
        return;
    }

    auto& s = slots_[t.slot];

    // Monotonic max: a late complete() from an abandoned older ticket
    // must never regress past a newer one.
    uint64_t cur = s.ticket_done.load(std::memory_order_relaxed);
    while (cur < t.ticket &&
           !s.ticket_done.compare_exchange_weak(cur, t.ticket,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }

    s.sem.release();
}

bool ThreadSignal::wait_for_signal(WaitToken t) {
    if (t.slot < 0 || t.slot >= max_threads_) {
        return false;
    }
    auto& s = slots_[t.slot];

    while (true) {
        if (shutdown_.load(std::memory_order_acquire)) {
            return false;
        }

        if (s.ticket_done.load(std::memory_order_acquire) >= t.ticket) {
            return true;
        }

        s.sem.acquire();
    }
}

bool ThreadSignal::wait_for_signal(WaitToken t, long millis) {
    if (t.slot < 0 || t.slot >= max_threads_) {
        return false;
    }
    auto& s = slots_[t.slot];

    auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(millis);

    while (true) {
        if (shutdown_.load(std::memory_order_acquire)) {
            return false;
        }

        if (s.ticket_done.load(std::memory_order_acquire) >= t.ticket) {
            return true;
        }

        auto remain =
            deadline - std::chrono::steady_clock::now();

        if (remain <= std::chrono::nanoseconds(0)) {
            return false;
        }

        if (!s.sem.try_acquire_for(remain)) {
            // Final race check at timeout boundary
            return s.ticket_done.load(std::memory_order_acquire) >= t.ticket;
        }
    }
}

void ThreadSignal::shutdown() noexcept {
    if (shutdown_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    wake_all();
}


ThreadSignal::ThreadSignal(int max_threads)
        : max_threads_(max_threads),
          slots_(new Slot[max_threads]) {}

ThreadSignal::~ThreadSignal() {
    shutdown();
}

