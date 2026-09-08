#pragma once
//
// Created by pr on 28.12.20.
//

#ifndef GRAINSTORM_LOGGER_H
#define GRAINSTORM_LOGGER_H

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

struct LogEntry {
    std::string message;
    std::string timestamp;

    LogEntry() = default;
    LogEntry(const std::string& msg, const std::string& ts)
        : message(msg), timestamp(ts) {
    }
};

class LogRingBuffer {
public:
    LogRingBuffer(size_t capacity)
        : capacity(capacity), buffer(capacity), head(0), size(0) {
    }

    void log(const std::string& msg) {
        std::lock_guard lock(writeMutex);

        // Generate timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        buffer[head] = LogEntry(msg, oss.str());
        head = (head + 1) % capacity;
        size = std::min(size + 1, capacity);
        hasChanged = true; // Mark that the buffer has changed
    }

    bool snapshot(std::vector<LogEntry>& result, bool& firstRun) {
        std::lock_guard lock(writeMutex);
        if (size == 0 || (!firstRun && !hasChanged)) return false;

        firstRun = hasChanged = false;
        result.clear();
        result.reserve(size);

        size_t start = (head + capacity - size) % capacity;
        for (size_t i = 0; i < size; ++i) {
            result.push_back(buffer[(start + i) % capacity]);
        }
        return true;
    }

    // Convenience method to get formatted strings with timestamps
    bool snapshotFormatted(std::vector<std::string>& result, bool& firstRun) {
        std::vector<LogEntry> entries;
        if (!snapshot(entries, firstRun)) return false;

        result.clear();
        result.reserve(entries.size());
        for (const auto& entry : entries) {
            result.push_back("[" + entry.timestamp + "] " + entry.message);
        }
        return true;
    }

private:
    const size_t capacity;
    std::vector<LogEntry> buffer;
    size_t head;
    size_t size;
    std::mutex writeMutex;
    bool hasChanged = false; // Flag to indicate if the buffer has changed since the last snapshot
};
namespace tsl {
	extern LogRingBuffer logRingBuffer; // Adjust capacity as needed
}
#include <string>
#include<iostream>

template< typename... argv >
std::string stringf( const char* format, argv... args ) {
    const size_t SIZE = std::snprintf( NULL, 0, format, args... );

    std::string output;
    output.resize(SIZE+1);
    std::snprintf( &(output[0]), SIZE+1, format, args... );
    tsl::logRingBuffer.log(output); // Log the message to the ring buffer
    return output;
}


#ifdef __ANDROID__
#include <android/log.h>

#define LOG_TAG "Grainstorm"

#define LOGI(fmt, ...) do { \
    std::string msg = stringf(fmt, ##__VA_ARGS__); \
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, msg.c_str()); \
} while (0)

#define LOGD(fmt, ...) do { \
    std::string msg = stringf(fmt, ##__VA_ARGS__); \
    __android_log_write(ANDROID_LOG_DEBUG, LOG_TAG, msg.c_str()); \
} while (0)

#define LOGW(fmt, ...) do { \
    std::string msg = stringf(fmt, ##__VA_ARGS__); \
    __android_log_write(ANDROID_LOG_WARN, LOG_TAG, msg.c_str()); \
} while (0)

#define LOGE(fmt, ...) do { \
    std::string msg = stringf(fmt, ##__VA_ARGS__); \
    __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, msg.c_str()); \
} while (0)

#elif defined __APPLE__
extern "C" void tsl_nslog(const char* msg);
#define LOGE(fmt, ...) do { \
std::string msg = stringf(fmt, ##__VA_ARGS__); \
    tsl_nslog(msg.c_str()); \
    } while (0)

#define LOGD(fmt, ...) do { \
std::string msg = stringf(fmt, ##__VA_ARGS__); \
    tsl_nslog(msg.c_str()); \
    } while (0)

#define LOGI(fmt, ...) do { \
std::string msg = stringf(fmt, ##__VA_ARGS__); \
    tsl_nslog(msg.c_str()); \
    } while (0)


#define LOGW(fmt, ...) do { \
std::string msg = stringf(fmt, ##__VA_ARGS__); \
    tsl_nslog(msg.c_str()); \
    } while (0)

#else
#define LOGD(...)  std::cout << stringf(__VA_ARGS__)  << std::endl;
#define LOGE(...)  std::cout << stringf(__VA_ARGS__) << std::endl;
#define LOGI(...)  std::cout << stringf(__VA_ARGS__) << std::endl;
#define LOGW(...)  std::cout << stringf(__VA_ARGS__) << std::endl;
#endif




#endif //GRAINSTORM_LOGGER_H
