#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace tsl {

	struct PoolBudgetInfo {
		uint64_t physical_ram = 0;
		uint64_t available_disk = 0;
		uint64_t recommended_pool = 0;
	};

	class SwapPoolBudget {
	public:

		static uint64_t get_physical_ram();

		static uint64_t get_available_disk(const char* path);

		static uint64_t recommended_pool_size(const char* temp_path);

		static PoolBudgetInfo query(const char* temp_path);

		// Conservative budget used when the disk cannot be measured, and as
		// the retry size when the measured budget cannot actually be backed.
		static uint64_t fallback_default();
	};

}

namespace tsl {
	class SwapPagePool {
	public:
		struct Token {
			uint32_t page;  // first page index
			uint32_t count; // number of pages
			bool valid() const noexcept { return count > 0; }
		};
		static constexpr Token null_token() noexcept { return { 0, 0 }; }

		SwapPagePool(const std::string& filepath, std::size_t pool_bytes = 0,
			std::size_t page_size = 64 * 1024);
		~SwapPagePool();

		SwapPagePool(const SwapPagePool&) = delete;
		SwapPagePool& operator=(const SwapPagePool&) = delete;

		// Returns pointer + token, or {nullptr, null_token()} if no contiguous
		// run of pages is available. Call release() on any token to make room.
		struct Allocation { void* ptr; Token token; };
		Allocation acquire(std::size_t size);

		// Release any previously acquired allocation. O(1).
		void release(Token token) noexcept;

		std::size_t used()      const;
		std::size_t capacity()  const;
		std::size_t page_size() const noexcept { return page_size_; }
		std::size_t capacity_bytes_{};

		static std::string  default_path(const std::string& filename = "grainstorm_history.swap");
		static std::size_t  recommended_size();

		static void         set_android_cache_dir(const std::string& path);
	private:
		struct PoolHdr {
			uint64_t magic;
			uint64_t pool_bytes;
			uint64_t page_size;
			uint64_t page_count;
			// followed by page_count bits of free bitmap (1=free), padded to 8 bytes
		};
		static constexpr uint64_t MAGIC = 0x504147504F4F4CLLU; // "PAGPOOL"
		static std::size_t data_offset(uint64_t page_count, std::size_t page_size) noexcept;

		uint64_t* bitmap()    const noexcept;
		void* page_data() const noexcept;
		void* page_ptr(uint32_t idx) const noexcept;

		// Returns first page of a free run of `count` pages, or UINT32_MAX.
		uint32_t find_free_run(uint32_t count) const noexcept;
		void     mark(uint32_t first, uint32_t count, bool free) noexcept;

		void map_file(const std::string& path, std::size_t size);
		void unmap() noexcept;

		PoolHdr* phdr() const noexcept;

		void* base_ = nullptr;
		void* handles_ = nullptr;
		std::size_t map_size_ = 0;
		std::size_t page_size_ = 0;
		mutable std::mutex mutex_;
		std::string path_{};
		bool failed_{};
	};
}