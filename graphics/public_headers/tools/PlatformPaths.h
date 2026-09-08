#pragma once
#include <string>
#include <vector>

// Returns writable app data dir, creates it if needed.
// macOS: ~/Library/Application Support/com.thesecretlaboratory.grainstorm
// Windows: %APPDATA%\The Secret Laboratory\Grainstorm

namespace tsl::app{
    std::vector<std::string> getDirContent(const std::string& subDir);

    FILE* getFdFromUri(const std::string& uriString, const char* mode = "rb");

    bool deleteFromUri(const std::string& uriString);

    std::string getStoragePath(const std::string& subDir);

    std::string getAppSupportDir();

#ifdef OS_IOS
    void openURL(const std::string& url);
#endif
}
