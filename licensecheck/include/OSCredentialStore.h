// OSCredentialStore.h
#pragma once
#include <string>
#include <mutex>

namespace tsl {
    class OSCredentialStore {
    private:
        std::string serviceName;
        std::recursive_mutex mtx; // 🔒 protect all operations

    public:
        explicit OSCredentialStore(const std::string& service);
        // Generic credential methods (for device ID, etc.)
        bool save(const std::string& value);
        std::string load();
        bool clearAll();
        bool has();
        bool saveCredential(const std::string& key, const std::string& value);
        std::string loadCredential(const std::string& key);
        bool deleteCredential(const std::string& key);
        bool hasCredential(const std::string& key);
    private:
#ifdef _WIN32
        bool saveWindowsCredential(const char* fieldNameCStr, const std::string& data);
        std::string loadWindowsCredential(const char* fieldNameCStr);
        bool deleteWindowsCredential(const char* fieldNameCStr);
#elif __APPLE__
        bool saveMacOSKeychain(const char* fieldNameCStr, const std::string& data);
        std::string loadMacOSKeychain(const char* fieldNameCStr);
        bool deleteMacOSKeychain(const char* fieldNameCStr);
#elif __linux__
        bool saveLinuxSecret(const char* fieldNameCStr, const std::string& data);
        std::string loadLinuxSecret(const char* fieldNameCStr);
        bool deleteLinuxSecret(const char* fieldNameCStr);
#endif
    };
}