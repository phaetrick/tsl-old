#include "settings.h"     // Include the necessary header for free (on Windows)

#include "logger.h"

using namespace tsl::settings;
fs::path GetSettingsPath(const std::string& appName)
{
#ifdef _WIN32
	char* appData = nullptr;
	size_t len = 0;
	_dupenv_s(&appData, &len, "APPDATA");
	fs::path base = appData ? fs::path(appData) : fs::temp_directory_path();
	free(appData);
#else
	LOGE("1");
	const char* home = getenv("HOME");
	fs::path base;
	if (home && home[0] != '\0') {
		base = fs::path(home);
	} else {
		// Fallback for sandboxed apps
		base = fs::temp_directory_path();
	}
#endif
	fs::path result = base / appName;
	// Create directory if it doesn't exist
	std::error_code ec;
	LOGE("2");
	fs::create_directories(result, ec);
	return result / "settings.json";
	LOGE("3");
}