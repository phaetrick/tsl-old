#pragma once
#include <cstddef>

#if defined(__cpp_lib_hardware_interference_size)
#  include <new>
namespace tsl {
inline constexpr std::size_t CACHELINE = std::hardware_destructive_interference_size;
inline constexpr std::size_t CACHELINE_SHARED = std::hardware_constructive_interference_size;
}
#else
namespace tsl {
inline constexpr std::size_t CACHELINE = 64;
inline constexpr std::size_t CACHELINE_SHARED = 64;
}
#endif

#include <cstdint>

namespace tsl {

    struct MemoryInfo {
        uint64_t total_phys_bytes;
        uint64_t available_phys_bytes;
        uint64_t process_rss_bytes; // Current app's actual RAM usage
    };

// Returns all relevant memory stats in one fast call
    MemoryInfo get_memory_stats();

// Returns number of physical cores (not just logical threads)
// Useful for your DynamicThreadManager to avoid over-subscription
    int get_physical_core_count();

} // namespace platform

