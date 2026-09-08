#include "../public_headers/tools/PlatformPaths.h"
#include "../public_headers/app.h"

#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#endif
#ifdef __ANDROID__
#include <jni.h>
#include <linux/resource.h>
#include <zconf.h>
#include <sys/resource.h>
#endif

std::string tsl::app::getStoragePath(const std::string& uriString) {
#ifdef __ANDROID__
	ATTACH
	if (env && tsl::android::activityclass) {
		jmethodID jfid = env->GetStaticMethodID(tsl::android::activityclass, "getStoragePath",
			"(Ljava/lang/String;)Ljava/lang/String;");
		if (jfid) {
			auto string = (jstring)env->CallStaticObjectMethod(tsl::android::activityclass, jfid,
				uriString.empty() ? nullptr
				: env->NewStringUTF(uriString.c_str()));
			if (string) {
				const char* tmp = env->GetStringUTFChars(string, nullptr);
				std::string ret = tmp;
				env->ReleaseStringUTFChars(string, tmp);
				DETACH
				return ret;
			}
		} else {
			env->ExceptionClear();
		}
	}
	DETACH
	return "";
#elif defined(OS_WIN)
	char path[MAX_PATH];
	// Get the roaming AppData folder (e.g. C:\Users\User\AppData\Roaming)
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
		std::filesystem::path storagePath = path;
		storagePath /= std::filesystem::path(uriString);

		// Create directory if it doesn't exist
		std::error_code ec;
		std::filesystem::create_directories(storagePath, ec); // ignores error

		return storagePath.string();
	}

	// Fallback if SHGetFolderPathA fails
	return "";
#elif defined(__APPLE__)
	std::filesystem::path storagePath = tsl::app::getAppSupportDir();
	storagePath /= std::filesystem::path(uriString);
	std::error_code ec;
	std::filesystem::create_directories(storagePath, ec);
	return storagePath.string();
#else
	return "";
#endif
}

FILE* tsl::app::getFdFromUri(const std::string& uriString, const char* mode) {
#ifdef __ANDROID__
	ATTACH
	if (env && tsl::android::activityclass) {
		jmethodID mid = env->GetStaticMethodID(tsl::android::activityclass, "getFdFromUriString", "(Ljava/lang/String;Ljava/lang/String;)I");
		if (mid) {
			int fd_val = (int)env->CallStaticIntMethod(tsl::android::activityclass, mid,
				env->NewStringUTF(uriString.data()), env->NewStringUTF(mode));
			if (fd_val > 0) {
				FILE* fd = fdopen(fd_val, mode);
				DETACH
				return fd;
			}
		} else {
			env->ExceptionClear();
		}
	}
	DETACH
	return nullptr;
#else
	return fopen(uriString.c_str(), mode);
#endif
}

bool tsl::app::deleteFromUri(const std::string& uriString) {
#ifdef __ANDROID__
	ATTACH
	if (env && tsl::android::activityclass) {
		jmethodID mid = env->GetStaticMethodID(tsl::android::activityclass, "deleteFromUri", "(Ljava/lang/String;)Z");
		if (mid) {
			jboolean result = env->CallStaticBooleanMethod(tsl::android::activityclass, mid, env->NewStringUTF(uriString.c_str()));
			DETACH
			return result;
		} else {
			env->ExceptionClear();
		}
	}
	DETACH
	return false;
#else
	std::error_code ec;
	std::filesystem::remove(uriString, ec);
	return !ec;
#endif
}

std::vector<std::string> tsl::app::getDirContent(const std::string& subDir) {
	std::vector<std::string> strings;
#ifdef __ANDROID__
	ATTACH
	if (env && tsl::android::activityclass) {
		jmethodID mid = env->GetStaticMethodID(tsl::android::activityclass, "getDirContent", "(Ljava/lang/String;)[Ljava/lang/Object;");
		if (mid) {
			auto items = env->CallStaticObjectMethod(tsl::android::activityclass, mid,
				subDir.empty() ? nullptr : env->NewStringUTF(subDir.data()));
			if (items != nullptr) {
				auto size = env->GetArrayLength(static_cast<jobjectArray>(items));
				for (int i = 0; i < size; i++) {
					auto fdss = env->GetObjectArrayElement(static_cast<jobjectArray>(items), i);
					if (fdss != nullptr) {
						auto str = env->GetStringUTFChars((jstring)fdss, nullptr);
						strings.emplace_back(str);
						env->ReleaseStringUTFChars((jstring)fdss, str);
					}
				}
			}
		} else {
			env->ExceptionClear();
		}
	}
	DETACH

#endif
	return strings;
};

