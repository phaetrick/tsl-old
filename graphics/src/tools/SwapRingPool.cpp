#include "tools/SwapRingPool.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <bit>
#include <cstdint>
#include <algorithm>
#include <limits>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

using namespace tsl;

static std::string parent_directory(const std::string& path)
{
	size_t p = path.find_last_of("/\\\\");
	if (p == std::string::npos)
		return ".";

	return path.substr(0, p);
}

uint64_t SwapPoolBudget::get_physical_ram() {
#if defined(_WIN32)
	MEMORYSTATUSEX st{};
	st.dwLength = sizeof(st);

	if (GlobalMemoryStatusEx(&st))
		return static_cast<uint64_t>(st.ullTotalPhys);

	return 0;

#elif defined(__APPLE__)

	uint64_t mem = 0;
	size_t len = sizeof(mem);

	if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
		return mem;

	return 0;

#elif defined(__ANDROID__) || defined(__linux__)

	long pages = sysconf(_SC_PHYS_PAGES);
	long page_size = sysconf(_SC_PAGE_SIZE);

	if (pages <= 0 || page_size <= 0)
		return 0;

	return static_cast<uint64_t>(pages) *
		static_cast<uint64_t>(page_size);

#else
	return 0;
#endif
}

// UINT64_MAX means "could not measure" - callers must treat that as unknown,
// not as full and not as empty. 0 is a real measurement of a full disk.
uint64_t SwapPoolBudget::get_available_disk(const char* path) {
	if (path == nullptr) {
		return UINT64_MAX;
	}
#if defined(_WIN32)

	ULARGE_INTEGER free_bytes{};

	if (GetDiskFreeSpaceExA(path, &free_bytes, nullptr, nullptr))
		return static_cast<uint64_t>(free_bytes.QuadPart);

	return UINT64_MAX;

#else

	struct statvfs st {};

	if (statvfs(path, &st) != 0) {
		LOGE("SwapPool: statvfs failed for %s", path);
		return UINT64_MAX;
	}

	uint64_t bsize = st.f_frsize;
	if (bsize == 0) bsize = st.f_bsize;

	return static_cast<uint64_t>(st.f_bavail) * bsize;

#endif
}

uint64_t SwapPoolBudget::recommended_pool_size(const char* temp_path) {
	auto parent = parent_directory(temp_path);

	const uint64_t disk = get_available_disk(parent.c_str());
	if (disk == UINT64_MAX) {
		// The measurement FAILED - that is not evidence of a full disk, so it
		// must not turn the pool off. Use the conservative default and let the
		// constructor's real allocation (preallocated, ENOSPC-checked) be the
		// arbiter: a genuinely full disk fails cleanly there.
		LOGE("SwapPool - %s: space query failed, using fallback budget", parent.c_str());
		return fallback_default();
	}
	LOGE("SwapPool - %s:  Space avail: %.3fMB", parent.c_str(), disk / (1024. * 1024.));
	if (disk == 0)
		return 0;

	const uint64_t ram = get_physical_ram();
	LOGE("SwapPool - Physical RAM: %.3fMB", ram/ (1024. * 1024.));

	if (ram == 0)
		return std::min<uint64_t>(
			fallback_default(),
			disk / 4
		);
	// ------------------------------------------------------------
	// RAM-based cap
	// ------------------------------------------------------------
	// We never want history consuming absurd proportions
	// of total memory.
	//
	// Desktop:
	//   allow up to 1/4 RAM
	// Mobile:
	//   more conservative
	// ------------------------------------------------------------

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)

	uint64_t ram_cap = ram / 8;

	// clamp mobile to sane range
	ram_cap = std::clamp<uint64_t>(
		ram_cap,
		256ULL * 1024 * 1024,
		1024ULL * 1024 * 1024
	);

#else

	uint64_t ram_cap = ram / 4;

	// clamp desktop range
	ram_cap = std::clamp<uint64_t>(
		ram_cap,
		256ULL * 1024 * 1024,
		2ULL * 1024 * 1024 * 1024
	);

#endif

	// ------------------------------------------------------------
	// Disk-based cap
	// ------------------------------------------------------------
	// Leave substantial free space.
	// Never consume most of device storage.
	// ------------------------------------------------------------

	uint64_t disk_cap = 0;

	// keep 1 GB free minimum on desktop
	// keep 512 MB free minimum on mobile

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)

	constexpr uint64_t reserve = 512ULL * 1024 * 1024;

#else

	constexpr uint64_t reserve = 2048ULL * 1024 * 1024;

#endif

	if (disk > reserve)
		disk_cap = (disk - reserve) / 2;

	LOGE("SwapPool - Disk cap: %.0fMB. RAM cap: %.0fMB", disk_cap / (1024. * 1024.), ram_cap / (1024. * 1024.));

	// final size
	uint64_t result = std::min(ram_cap, disk_cap);

	// absolute safety floor
	constexpr uint64_t MIN_POOL = 256ULL * 1024 * 1024;

	if (result < MIN_POOL)
		return 0;

	return result;
}

PoolBudgetInfo SwapPoolBudget::query(const char* temp_path) {
	PoolBudgetInfo out{};
	auto parent = parent_directory(temp_path);
	LOGE("SwapPool query for %s (parent: %s)", temp_path, parent.c_str());
	out.physical_ram = get_physical_ram();
	out.available_disk = get_available_disk(parent.c_str());
	out.recommended_pool = recommended_pool_size(temp_path);

	return out;
}

uint64_t SwapPoolBudget::fallback_default() {
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
	return 256ULL * 1024 * 1024;
#else
	return 512ULL * 1024 * 1024;
#endif
}

struct PlatformHandles {
#ifdef _WIN32
	HANDLE file = INVALID_HANDLE_VALUE;
	HANDLE map = nullptr;
#else
	int fd = -1;
#endif
};

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

static std::size_t bitmap_bytes(uint64_t page_count) {
	return ((page_count + 63) / 64) * 8; // round up to 8-byte word
}

std::size_t SwapPagePool::data_offset(uint64_t page_count, std::size_t page_size) noexcept {
	std::size_t raw = sizeof(PoolHdr) + ((page_count + 63) / 64) * 8;
	return ((raw + page_size - 1) / page_size) * page_size;
}
// ---------------------------------------------------------------------------

SwapPagePool::SwapPagePool(
	const std::string& filepath,
	std::size_t pool_bytes,
	std::size_t page_size)
	: page_size_(page_size)
	, path_(filepath)
{
	const bool autoSized = (pool_bytes == 0);
	if (autoSized)
		pool_bytes = SwapPoolBudget::recommended_pool_size(filepath.c_str());

	if (pool_bytes == 0) {
		// A MEASURED disk that is too full for the reserve policy - a failed
		// measurement never lands here (recommended_pool_size falls back).
		LOGE("SwapPool: Failed. Recommended pool size was 0.");
		failed_ = true;
		return;
	}

	if (page_size == 0 || (page_size & (page_size - 1)))
		throw std::invalid_argument(
			"SwapPool: page_size must be power of two");

	// The budget can be optimistic - when the space query failed it is a
	// guess, and even a measured value can be stale by map time. If the disk
	// cannot back the budgeted size, an auto-sized pool retries once at the
	// conservative default before giving up.
	uint64_t page_count = 0;
	for (int attempt = 0;; ++attempt) {
		// We don't know page_count yet - iterate to find stable layout.
		// data_offset depends on page_count which depends on data_offset.
		// Solve: estimate conservatively, it converges in 1-2 steps.
		std::size_t hdr_sz = sizeof(PoolHdr);
		uint64_t    est_pages = (pool_bytes - hdr_sz) / page_size;
		std::size_t offset = data_offset(est_pages, page_size);
		page_count = (pool_bytes > offset) ? (pool_bytes - offset) / page_size : 0;

		if (page_count == 0)
			throw std::invalid_argument("SwapPool: pool_bytes too small");

		try {
			map_file(filepath, pool_bytes);
			break;
		}
		catch (...) {
			const auto fb = static_cast<std::size_t>(SwapPoolBudget::fallback_default());
			if (attempt == 0 && autoSized && fb < pool_bytes) {
				LOGE("SwapPool: map_file failed at %.0fMB, retrying at %.0fMB",
					pool_bytes / (1024. * 1024.), fb / (1024. * 1024.));
				pool_bytes = fb;
				continue;
			}
			LOGE("SwapPool: map_file failed.");
			failed_ = true;
			return;
		}
	}

	capacity_bytes_ = pool_bytes;

	PoolHdr* ph = phdr();

	// Always reinitialize allocator state.
	// This pool is treated as temporary scratch storage,
	// not persistent shared memory.

	ph->magic = MAGIC;
	ph->pool_bytes = pool_bytes;
	ph->page_size = page_size;
	ph->page_count = page_count;

	// Mark all pages free (1 = free)
	std::memset(bitmap(), 0xFF, bitmap_bytes(page_count));

	// Clear unused tail bits in last bitmap word
	uint64_t total_bits = ((page_count + 63) / 64) * 64;

	if (total_bits > page_count) {
		uint64_t* bm = bitmap();
		uint64_t words = (page_count + 63) / 64;
		uint64_t used_bits = page_count % 64;

		if (used_bits)
			bm[words - 1] = (1ULL << used_bits) - 1;
	}
	LOGE("SwapPool - Success. %.0fMB allocated.", capacity_bytes_ / (1024. * 1024.));

}

SwapPagePool::~SwapPagePool() {
	if (failed_) return;

	unmap();
#ifdef _WIN32
	DeleteFileA(path_.c_str());
#else
	unlink(path_.c_str());
#endif
}
// ---------------------------------------------------------------------------
// acquire / release
// ---------------------------------------------------------------------------

SwapPagePool::Allocation SwapPagePool::acquire(std::size_t size) {
	if (failed_) return { nullptr, null_token() };

	if (size == 0) return { nullptr, null_token() };

	const uint32_t need = static_cast<uint32_t>((size + page_size_ - 1) / page_size_);

	std::lock_guard lock(mutex_);
	const uint32_t first = find_free_run(need);
	if (first == UINT32_MAX) return { nullptr, null_token() };

	mark(first, need, false);
	return { page_ptr(first), Token{first, need} };
}

void SwapPagePool::release(Token token) noexcept {
	if (failed_ || !token.valid()) return;
	std::lock_guard lock(mutex_);
	mark(token.page, token.count, true);
}

// ---------------------------------------------------------------------------
// Bitmap ops
// ---------------------------------------------------------------------------

// Find first run of `count` consecutive 1-bits. Returns UINT32_MAX if none.
uint32_t SwapPagePool::find_free_run(uint32_t count) const noexcept {
	const uint64_t page_count = phdr()->page_count;
	uint64_t* bm = bitmap();
	const uint64_t words = (page_count + 63) / 64;

	uint32_t run_start = 0, run_len = 0;

	for (uint64_t w = 0; w < words; ++w) {
		uint64_t word = bm[w];
		for (int b = 0; b < 64; ++b) {
			const uint32_t page = static_cast<uint32_t>(w * 64 + b);
			if (page >= page_count) goto done;
			if (word & (1ULL << b)) {
				if (run_len == 0) run_start = page;
				if (++run_len == count) return run_start;
			}
			else {
				run_len = 0;
			}
		}
	}
done:
	return UINT32_MAX;
}

void SwapPagePool::mark(uint32_t first, uint32_t count, bool free) noexcept {
	uint64_t* bm = bitmap();
	for (uint32_t i = first; i < first + count; ++i) {
		uint64_t& word = bm[i / 64];
		const uint64_t bit = 1ULL << (i % 64);
		if (free) word |= bit;
		else      word &= ~bit;
	}
}

// ---------------------------------------------------------------------------
// Layout accessors
// ---------------------------------------------------------------------------

SwapPagePool::PoolHdr* SwapPagePool::phdr() const noexcept {
	return reinterpret_cast<PoolHdr*>(base_);
}

uint64_t* SwapPagePool::bitmap() const noexcept {
	return reinterpret_cast<uint64_t*>(
		static_cast<char*>(base_) + sizeof(PoolHdr));
}

void* SwapPagePool::page_data() const noexcept {
	const PoolHdr* ph = phdr();
	std::size_t offset = data_offset(ph->page_count, static_cast<std::size_t>(ph->page_size));
	return static_cast<char*>(base_) + offset;
}

void* SwapPagePool::page_ptr(uint32_t idx) const noexcept {
	return static_cast<char*>(page_data()) + static_cast<std::size_t>(idx) * page_size_;
}

std::size_t SwapPagePool::used() const {
	std::lock_guard l(mutex_);
	const PoolHdr* ph = phdr();
	uint64_t* bm = bitmap();
	const uint64_t words = (ph->page_count + 63) / 64;
	uint64_t free_pages = 0;
	for (uint64_t w = 0; w < words; ++w)
		free_pages += std::popcount(bm[w]);
	return static_cast<std::size_t>(ph->page_count - free_pages) * page_size_;
}

std::size_t SwapPagePool::capacity() const {
	return static_cast<std::size_t>(phdr()->page_count) * page_size_;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static std::string g_android_cache_dir = "";

void SwapPagePool::set_android_cache_dir(const std::string& path) {
	g_android_cache_dir = path;
}

std::string SwapPagePool::default_path(const std::string& filename) {
#if defined(_WIN32)
	char buf[MAX_PATH];
	DWORD len = GetTempPathA(MAX_PATH, buf);
	if (len == 0) throw std::runtime_error("SwapPagePool: GetTempPathA failed");
	return std::string(buf, len) + filename;
#elif defined(__ANDROID__)
	if (!g_android_cache_dir.empty()) {
		return g_android_cache_dir + "/" + filename;
	}
	const char* tmp = getenv("TMPDIR");
	if (!tmp) tmp = "/data/local/tmp";
	return std::string(tmp) + "/" + filename;
#else
	const char* tmp = getenv("TMPDIR");
	if (!tmp) tmp = "/tmp";
	return std::string(tmp) + "/" + filename;
#endif
}

std::size_t SwapPagePool::recommended_size() {
#if defined(__APPLE__) && TARGET_OS_IPHONE
	return 256ULL * 1024 * 1024;
#elif defined(__ANDROID__)
	return 512ULL * 1024 * 1024;
#else
	return 2ULL * 1024 * 1024 * 1024;
#endif
}

// ---------------------------------------------------------------------------
// Platform mapping (identical to before)
// ---------------------------------------------------------------------------

#ifdef _WIN32

void SwapPagePool::map_file(const std::string& path, std::size_t size) {
	auto* h = new PlatformHandles{};
	h->file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
		nullptr, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
	if (h->file == INVALID_HANDLE_VALUE) { delete h; throw std::runtime_error("SwapPagePool: CreateFile failed"); }
	LARGE_INTEGER li{}; li.QuadPart = static_cast<LONGLONG>(size);
	h->map = CreateFileMappingA(h->file, nullptr, PAGE_READWRITE, li.HighPart, li.LowPart, nullptr);
	if (!h->map) { CloseHandle(h->file); delete h; throw std::runtime_error("SwapPagePool: CreateFileMapping failed"); }
	base_ = MapViewOfFile(h->map, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (!base_) { CloseHandle(h->map); CloseHandle(h->file); delete h; throw std::runtime_error("SwapPagePool: MapViewOfFile failed"); }
	handles_ = h; map_size_ = size;
}

void SwapPagePool::unmap() noexcept {
	auto* h = static_cast<PlatformHandles*>(handles_);
	if (!h) return;
	if (base_) { FlushViewOfFile(base_, 0); UnmapViewOfFile(base_); base_ = nullptr; }
	if (h->map) CloseHandle(h->map);
	if (h->file != INVALID_HANDLE_VALUE) CloseHandle(h->file);
	delete h; handles_ = nullptr;
}

#else

void SwapPagePool::map_file(const std::string& path, std::size_t size) {
	auto* h = new PlatformHandles{};
	h->fd = open(path.c_str(), O_RDWR | O_CREAT, 0600);
	if (h->fd < 0) { delete h; throw std::runtime_error("SwapPagePool: open failed"); }
	struct stat st {};
	fstat(h->fd, &st);
	if (static_cast<std::size_t>(st.st_size) < size) {
		// Reserve the blocks FOR REAL before mapping. ftruncate alone makes a
		// SPARSE file: it succeeds on a full disk and the failure surfaces
		// later as SIGBUS when an unbacked page is first touched mid-audio.
		// Preallocation moves that failure here, where the constructor can
		// retry smaller or disable the pool cleanly. Only ENOSPC is fatal -
		// a filesystem without preallocation support keeps the old sparse
		// behaviour rather than losing the pool. (Windows needs none of this:
		// NTFS allocates on extension, so CreateFileMapping fails cleanly.)
#if defined(__APPLE__)
		fstore_t store{};
		store.fst_flags = F_ALLOCATEALL;
		store.fst_posmode = F_PEOFPOSMODE;
		store.fst_offset = 0;
		store.fst_length = static_cast<off_t>(size) - st.st_size;
		if (fcntl(h->fd, F_PREALLOCATE, &store) == -1 && errno == ENOSPC) {
			close(h->fd); delete h;
			throw std::runtime_error("SwapPagePool: not enough disk space");
		}
#elif defined(__ANDROID__) || defined(__linux__)
		const int perr = posix_fallocate(h->fd, 0, static_cast<off_t>(size));
		if (perr == ENOSPC) {
			close(h->fd); delete h;
			throw std::runtime_error("SwapPagePool: not enough disk space");
		}
#endif
		if (ftruncate(h->fd, static_cast<off_t>(size)) < 0) {
			close(h->fd); delete h; throw std::runtime_error("SwapPagePool: ftruncate failed");
		}
	}
	base_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, h->fd, 0);
	if (base_ == MAP_FAILED) { close(h->fd); delete h; throw std::runtime_error("SwapPagePool: mmap failed"); }
	madvise(base_, size, MADV_RANDOM);
	handles_ = h; map_size_ = size;
}

void SwapPagePool::unmap() noexcept {
	auto* h = static_cast<PlatformHandles*>(handles_);
	if (!h) return;
	if (base_ != MAP_FAILED) { msync(base_, map_size_, MS_ASYNC); munmap(base_, map_size_); base_ = MAP_FAILED; }
	if (h->fd >= 0) close(h->fd);
	delete h; handles_ = nullptr;
}

#endif