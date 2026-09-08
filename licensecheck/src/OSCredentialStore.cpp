// OSCredentialStore.cpp
#include "OSCredentialStore.h"
#include "StringObfuscator.h"

#include <iostream>
#include <mutex>
#include <cstring>

#ifdef _WIN32
#ifndef CRED_PERSIST_CURRENT_USER
#define CRED_PERSIST_CURRENT_USER 2
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#elif __APPLE__
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#elif __linux__
#include <libsecret/secret.h>
#endif

namespace tsl {

    // ---------------- Constructor ----------------
    OSCredentialStore::OSCredentialStore(const std::string& service)
        : serviceName(service) {
    }
    bool OSCredentialStore::save(const std::string& value) { return saveCredential("key", value); }
    std::string OSCredentialStore::load() { return loadCredential("key"); }
    bool OSCredentialStore::clearAll() { return deleteCredential("key"); };
    bool OSCredentialStore::has() { return hasCredential("key"); }
    
    

    // ===============================================
    // Platform-specific implementations
    // ===============================================

#ifdef _WIN32

    bool OSCredentialStore::saveWindowsCredential(const char* fieldNameCStr, const std::string& data) {
        // build targetName from runtime serviceName + "_" + fieldNameCStr
        std::string targetName = serviceName;
        targetName.push_back('_');
        targetName.append(fieldNameCStr);
        // Convert targetName to wide string for Cred APIs
        std::wstring wTargetName(targetName.begin(), targetName.end());

        // Store UTF-8 as binary in CredentialBlob
        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(wTargetName.c_str());
        cred.UserName = nullptr;
        cred.CredentialBlobSize = static_cast<DWORD>(data.size());
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(data.data()));
        cred.Persist = CRED_PERSIST_CURRENT_USER;

        if (!CredWriteW(&cred, 0)) {
            DWORD err = GetLastError();
            std::cerr << "CredWriteW failed (target=" << targetName << "): " << err << std::endl;
            return false;
        }
        return true;
    }

    std::string OSCredentialStore::loadWindowsCredential(const char* fieldNameCStr) {
        std::string targetName = serviceName;
        targetName.push_back('_');
        targetName.append(fieldNameCStr);
        std::wstring wTargetName(targetName.begin(), targetName.end());
        PCREDENTIALW pcred = nullptr;

        if (CredReadW(wTargetName.c_str(), CRED_TYPE_GENERIC, 0, &pcred)) {
            std::string data(reinterpret_cast<char*>(pcred->CredentialBlob),
                pcred->CredentialBlobSize);
            CredFree(pcred);
            return data;
        }
        else {
            DWORD err = GetLastError();
            if (err != ERROR_NOT_FOUND) {
                std::cerr << "CredReadW failed (target=" << targetName << "): " << err << std::endl;
            }
        }
        return {};
    }

    bool OSCredentialStore::deleteWindowsCredential(const char* fieldNameCStr) {
        std::string targetName = serviceName;
        targetName.push_back('_');
        targetName.append(fieldNameCStr);
        std::wstring wTargetName(targetName.begin(), targetName.end());

        if (!CredDeleteW(wTargetName.c_str(), CRED_TYPE_GENERIC, 0)) {
            DWORD err = GetLastError();
            if (err != ERROR_NOT_FOUND) {
                std::cerr << "CredDeleteW failed (target=" << targetName << "): " << err << std::endl;
                return false;
            }
            return false;
        }
        return true;
    }

#endif // _WIN32

#ifdef __APPLE__

    static CFDictionaryRef makeQuery(const std::string& service, const std::string& account) {
        CFStringRef svc = CFStringCreateWithBytes(nullptr, (const UInt8*)service.data(), service.size(), kCFStringEncodingUTF8, false);
        CFStringRef acc = CFStringCreateWithBytes(nullptr, (const UInt8*)account.data(), account.size(), kCFStringEncodingUTF8, false);
        CFMutableDictionaryRef q = CFDictionaryCreateMutable(nullptr, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(q, kSecAttrService, svc);
        CFDictionarySetValue(q, kSecAttrAccount, acc);
        CFRelease(svc);
        CFRelease(acc);
        return q;
    }

    bool OSCredentialStore::saveMacOSKeychain(const char* fieldNameCStr, const std::string& data) {
        std::string account = serviceName + "_" + fieldNameCStr;

        CFDictionaryRef query = makeQuery(serviceName, account);
        // Delete existing item silently before adding
        SecItemDelete(query);
        CFRelease(query);

        CFDataRef value = CFDataCreate(nullptr, (const UInt8*)data.data(), (CFIndex)data.size());
        CFMutableDictionaryRef item = CFDictionaryCreateMutable(nullptr, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFStringRef svc = CFStringCreateWithBytes(nullptr, (const UInt8*)serviceName.data(), serviceName.size(), kCFStringEncodingUTF8, false);
        CFStringRef acc = CFStringCreateWithBytes(nullptr, (const UInt8*)account.data(), account.size(), kCFStringEncodingUTF8, false);
        CFDictionarySetValue(item, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(item, kSecAttrService, svc);
        CFDictionarySetValue(item, kSecAttrAccount, acc);
        CFDictionarySetValue(item, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock);
        CFDictionarySetValue(item, kSecValueData, value);
        CFRelease(svc);
        CFRelease(acc);
        CFRelease(value);

        OSStatus status = SecItemAdd(item, nullptr);
        CFRelease(item);
        return status == errSecSuccess;
    }

    std::string OSCredentialStore::loadMacOSKeychain(const char* fieldNameCStr) {
        std::string account = serviceName + "_" + fieldNameCStr;

        CFDictionaryRef baseQuery = makeQuery(serviceName, account);
        CFMutableDictionaryRef query = CFDictionaryCreateMutableCopy(nullptr, 0, baseQuery);
        CFRelease(baseQuery);
        CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

        CFDataRef result = nullptr;
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&result);
        CFRelease(query);

        if (status == errSecSuccess && result) {
            std::string data((const char*)CFDataGetBytePtr(result), CFDataGetLength(result));
            CFRelease(result);
            return data;
        }
        return {};
    }

    bool OSCredentialStore::deleteMacOSKeychain(const char* fieldNameCStr) {
        std::string account = serviceName + "_" + fieldNameCStr;
        CFDictionaryRef query = makeQuery(serviceName, account);
        OSStatus status = SecItemDelete(query);
        CFRelease(query);
        return status == errSecSuccess || status == errSecItemNotFound;
    }

#endif // __APPLE__

#ifdef __linux__

    static const SecretSchema OSCredentialSchema = {
        "com.example.OSCredentialStore",
        SECRET_SCHEMA_NONE,
        {
            { "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "field",   SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr,   SECRET_SCHEMA_ATTRIBUTE_STRING }
        }
    };

    bool OSCredentialStore::saveLinuxSecret(const char* fieldNameCStr, const std::string& data) {
        std::string label = serviceName;
        label.push_back('_');
        label.append(fieldNameCStr);

        GError* error = nullptr;

        gboolean result = secret_password_store_sync(
            &OSCredentialSchema,
            SECRET_COLLECTION_DEFAULT,
            label.c_str(),
            data.c_str(),
            nullptr,
            &error,
            "service", serviceName.c_str(),
            "field", fieldNameCStr,
            nullptr
        );

        if (error) {
            std::cerr << "secret_password_store_sync error (label=" << label << "): " << error->message << std::endl;
            g_error_free(error);
            return false;
        }

        return result;
    }

    std::string OSCredentialStore::loadLinuxSecret(const char* fieldNameCStr) {
        GError* error = nullptr;

        gchar* password = secret_password_lookup_sync(
            &OSCredentialSchema,
            nullptr,
            &error,
            "service", serviceName.c_str(),
            "field", fieldNameCStr,
            nullptr
        );

        if (error) {
            std::cerr << "secret_password_lookup_sync error (service=" << serviceName << ", field=" << fieldNameCStr << "): " << error->message << std::endl;
            g_error_free(error);
            return {};
        }

        if (password) {
            std::string result(password);
            secret_password_free(password);
            return result;
        }
        return {};
    }

    bool OSCredentialStore::deleteLinuxSecret(const char* fieldNameCStr) {
        GError* error = nullptr;

        gboolean result = secret_password_clear_sync(
            &OSCredentialSchema,
            nullptr,
            &error,
            "service", serviceName.c_str(),
            "field", fieldNameCStr,
            nullptr
        );

        if (error) {
            std::cerr << "secret_password_clear_sync error (service=" << serviceName << ", field=" << fieldNameCStr << "): " << error->message << std::endl;
            g_error_free(error);
            return false;
        }

        return result;
    }

#endif // __linux__
    bool OSCredentialStore::saveCredential(const std::string& key, const std::string& value) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
#ifdef _WIN32
        return saveWindowsCredential(key.c_str(), value);
#elif __APPLE__
        return saveMacOSKeychain(key.c_str(), value);
#elif __linux__
        return saveLinuxSecret(key.c_str(), value);
#else
        return false;
#endif
    }

    std::string OSCredentialStore::loadCredential(const std::string& key) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
#ifdef _WIN32
        return loadWindowsCredential(key.c_str());
#elif __APPLE__
        return loadMacOSKeychain(key.c_str());
#elif __linux__
        return loadLinuxSecret(key.c_str());
#else
        return {};
#endif
    }

    bool OSCredentialStore::deleteCredential(const std::string& key) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
#ifdef _WIN32
        return deleteWindowsCredential(key.c_str());
#elif __APPLE__
        return deleteMacOSKeychain(key.c_str());
#elif __linux__
        return deleteLinuxSecret(key.c_str());
#else
        return false;
#endif
    }

    bool OSCredentialStore::hasCredential(const std::string& key) {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        return !loadCredential(key).empty();
    }
} // namespace tsl
