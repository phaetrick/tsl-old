#pragma once

#include <atomic>
#include <memory>
#include <semaphore>
#include <chrono>
#include "platform_config.h"

namespace tsl {
	class ThreadSignal {
	private:
		struct alignas(tsl::CACHELINE) Slot {
			std::atomic<uint64_t> generation{ 0 };

			// Last generation observed by this slot owner.
			// Only accessed by the owning thread.
			uint64_t observed_generation{ 0 };

			// Token API: highest completed ticket (written by wakers via complete()).
			std::atomic<uint64_t> ticket_done{ 0 };
			// Last ticket issued. Only accessed by the owning thread.
			uint64_t next_ticket{ 0 };

			std::counting_semaphore<std::numeric_limits<int>::max()> sem{ 0 };
		};

	public:
		// Token-based rendezvous: begin_wait() before publishing the work, pass the
		// token to the completing side, which calls complete(token). wait_for_signal(token)
		// returns only when THAT ticket completed — stale wakes from older operations
		// on the same thread can never satisfy it. One outstanding token per thread.
		struct WaitToken {
			int slot{ -1 };
			uint64_t ticket{ 0 };
		};

		explicit ThreadSignal(int max_threads = 16);

		~ThreadSignal();

		ThreadSignal(const ThreadSignal&) = delete;
		ThreadSignal& operator=(const ThreadSignal&) = delete;

		int acquire_slot();

		void wake_thread(int slot) noexcept;
		void wake_all() noexcept;

		void wait_for_signal();
		bool wait_for_signal(long millis);

		WaitToken begin_wait();
		void complete(WaitToken t) noexcept;
		bool wait_for_signal(WaitToken t);
		bool wait_for_signal(WaitToken t, long millis);

		// Timed sleep immune to stray wakes (only shutdown ends it early).
		// Replaces the acquire_slot()+wait_for_signal(ms) sleep idiom, which
		// consumed rendezvous wakes belonging to other operations.
		void sleep_for(long millis) { wait_for_signal(begin_wait(), millis); }

		void shutdown() noexcept;

	private:
		int                     max_threads_;
		std::unique_ptr<Slot[]> slots_;

		std::atomic<int>  next_slot_{ 0 };
		std::atomic<bool> shutdown_{ false };
	};
}