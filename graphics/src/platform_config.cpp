//
// Created by pr on 30.03.26.
//
#include "platform_config.h"

// --- OS-Specific Headers (Hidden in the .cpp) ---
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <vector>
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
#include <unistd.h>
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fstream>
#endif

namespace tsl {

    MemoryInfo get_memory_stats() {
        MemoryInfo info = {0, 0, 0};

        // 1. Get Process RSS (Actual Physical RAM used by THIS app)
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        info.process_rss_bytes = (uint64_t)pmc.WorkingSetSize;
    }
#elif defined(__APPLE__)
        struct mach_task_basic_info m_info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&m_info, &count) == KERN_SUCCESS) {
        info.process_rss_bytes = (uint64_t)m_info.resident_size;
    }
#elif defined(__linux__) || defined(__ANDROID__)
        auto f = fopen("/proc/self/statm", "r");
        if (f) {
            uint64_t size = 0, resident = 0;
            if (fscanf(f, "%llu %llu", &size, &resident) == 2) {
                long page_size = sysconf(_SC_PAGESIZE);
                info.process_rss_bytes = resident * page_size;
            }
            fclose(f);
        }

#endif

        // 2. Get Global System Memory (Total and Available)
#if defined(_WIN32)
        MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        info.total_phys_bytes = status.ullTotalPhys;
        info.available_phys_bytes = status.ullAvailPhys;
    }
#elif defined(__APPLE__)
        int64_t memsize = 0;
    size_t sz = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &sz, NULL, 0) == 0) {
        info.total_phys_bytes = (uint64_t)memsize;
    }
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t host_count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &host_count) == KERN_SUCCESS) {
        info.available_phys_bytes = (uint64_t)vm_stats.free_count * sysconf(_SC_PAGESIZE);
    }
#elif defined(__linux__) || defined(__ANDROID__)
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            info.total_phys_bytes = (uint64_t)si.totalram * si.mem_unit;
        }
        f = fopen("/proc/meminfo", "r");
        if (f) {
            char line[128];

            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "MemAvailable:", 13) == 0) {
                    char* p = line + 13;
                    while (*p < '0' || *p > '9') ++p;
                    info.available_phys_bytes = strtoull(p, nullptr, 10) * 1024ULL;
                    break;
                }
            }
            fclose(f);
        }

#endif

        return info;
    }

    int get_physical_core_count() {
#if defined(_WIN32)
        DWORD length = 0;
        GetLogicalProcessorInformation(nullptr, &length);
        // ERROR_INSUFFICIENT_BUFFER is the expected failure mode; any other error → fallback
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0)
            return -1;

        const size_t count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(count);
        if (!GetLogicalProcessorInformation(buffer.data(), &length))
            return -1;

        int phys_cores = 0;
        for (const auto& info : buffer)
            if (info.Relationship == RelationProcessorCore) ++phys_cores;
        return phys_cores;

#elif defined(__APPLE__)
        int phys_cores = 0;
        size_t size = sizeof(phys_cores);
        // hw.physicalcpu reflects the online physical cores (respects power management)
        // hw.physicalcpu_max is the silicon maximum regardless of online state
        if (sysctlbyname("hw.physicalcpu", &phys_cores, &size, nullptr, 0) == 0)
            return phys_cores;
        return -1;
#elif defined(__linux__) || defined(__ANDROID__)
        // Simple way for Linux: parse /proc/cpuinfo or use sysconf
        // Physical cores is tricky on Linux without parsing text,
        // but sysconf(_SC_NPROCESSORS_ONLN) is the standard "available" count.
        return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
        return 1; // Fallback
    }

} // namespace tsl