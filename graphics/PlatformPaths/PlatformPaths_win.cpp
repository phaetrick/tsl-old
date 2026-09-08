#include "../public_headers/tools/PlatformPaths.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

std::string getAppSupportDir() {
    char path[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    std::filesystem::path dir = std::filesystem::path(path)
        / "The Secret Laboratory" / "Grainstorm";
    std::filesystem::create_directories(dir);
    return dir.string();
}