// LicenseChecker.cpp
#include "LicenseChecker.h"
#include "OSCredentialStore.h"

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <functional>
#include <utility>
#include <vector>
#include <algorithm>  // <-- for std::sort
#include <fstream>    // <-- for std::ifstream, std::ofstream
#ifndef _WIN32
#include <netdb.h>
#endif
#include <ctime>      // <-- offline token expiry checks
#include <random>     // <-- for std::random_device, std::mt19937 (if using UUID generation)

#define USE_SSL
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <shlobj.h>   // <-- for SHGetFolderPathA
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
typedef int socklen_t;
using socket_t = SOCKET;
#define close_socket closesocket
#elif __APPLE__
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <arpa/inet.h>
#include <sys/mount.h>     // <-- for statfs
#include <unistd.h>        // <-- for gethostname
using socket_t = int;
#define close_socket close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR    (-1)
#elif __linux__
#include <sys/stat.h>      // <-- for mkdir
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <fstream>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <linux/if_packet.h>  // <-- for AF_PACKET
#include <unistd.h>        // <-- for gethostname
#define closesocket close
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#ifdef USE_SSL
extern "C" {
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
}

class KeyPairGenerator {
public:
	struct KeyPair {
		std::string privateKey;  // PEM format
		std::string publicKey;   // PEM format
		EVP_PKEY* evpKey{};        // For immediate use

		~KeyPair() {
			if (evpKey) EVP_PKEY_free(evpKey);
		}
	};

	// Generate RSA key pair in memory
	static KeyPair generateKeyPair(int keySize = 2048) {
		KeyPair result;
		result.evpKey = nullptr;

		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
		if (!ctx) return result;

		if (EVP_PKEY_keygen_init(ctx) <= 0) {
			EVP_PKEY_CTX_free(ctx);
			return result;
		}

		if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, keySize) <= 0) {
			EVP_PKEY_CTX_free(ctx);
			return result;
		}

		if (EVP_PKEY_keygen(ctx, &result.evpKey) <= 0) {
			EVP_PKEY_CTX_free(ctx);
			return result;
		}

		EVP_PKEY_CTX_free(ctx);

		// Convert to PEM strings
		result.privateKey = keyToPEM(result.evpKey, true);
		result.publicKey = keyToPEM(result.evpKey, false);

		return result;
	}

private:
	static std::string keyToPEM(EVP_PKEY* pkey, bool isPrivate) {
		BIO* bio = BIO_new(BIO_s_mem());
		if (!bio) return "";

		if (isPrivate) {
			PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
		}
		else {
			PEM_write_bio_PUBKEY(bio, pkey);
		}

		BUF_MEM* mem;
		BIO_get_mem_ptr(bio, &mem);
		std::string result(mem->data, mem->length);

		BIO_free(bio);
		return result;
	}
};

class DataSigner {
public:
	// Sign data with EVP_PKEY (from generated key pair)
	static std::string sign(const std::string& data, EVP_PKEY* privateKey) {
		EVP_MD_CTX* ctx = EVP_MD_CTX_new();
		if (!ctx) return "";

		if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, privateKey) <= 0) {
			EVP_MD_CTX_free(ctx);
			return "";
		}

		if (EVP_DigestSignUpdate(ctx, data.c_str(), data.length()) <= 0) {
			EVP_MD_CTX_free(ctx);
			return "";
		}

		size_t sigLen = 0;
		if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) <= 0) {
			EVP_MD_CTX_free(ctx);
			return "";
		}

		std::vector<unsigned char> signature(sigLen);
		if (EVP_DigestSignFinal(ctx, signature.data(), &sigLen) <= 0) {
			EVP_MD_CTX_free(ctx);
			return "";
		}

		EVP_MD_CTX_free(ctx);

		return base64Encode(signature.data(), sigLen);
	}

	// Sign data with PEM string
	static std::string signWithPEM(const std::string& data, const std::string& privateKeyPEM) {
		BIO* bio = BIO_new_mem_buf(privateKeyPEM.c_str(), privateKeyPEM.length());
		if (!bio) return "";

		EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
		BIO_free(bio);

		if (!pkey) return "";

		std::string signature = sign(data, pkey);
		EVP_PKEY_free(pkey);

		return signature;
	}

private:
	static std::string base64Encode(const unsigned char* data, size_t len) {
		BIO* bio = BIO_new(BIO_s_mem());
		BIO* b64 = BIO_new(BIO_f_base64());
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		bio = BIO_push(b64, bio);

		BIO_write(bio, data, len);
		BIO_flush(bio);

		BUF_MEM* bufferPtr;
		BIO_get_mem_ptr(bio, &bufferPtr);

		std::string result(bufferPtr->data, bufferPtr->length);
		BIO_free_all(bio);

		return result;
	}
};

#endif

namespace tsl {
	class DeviceFingerprint {
	public:
		// Constructor that takes CredStore
		DeviceFingerprint(OSCredentialStore* credStore = nullptr)
			: credStore_(credStore) {
		}
		std::string hwFingerprint;
		std::string deviceId;
		static std::string getComputerName() {
#ifdef _WIN32
			char computerName[MAX_COMPUTERNAME_LENGTH + 1];
			DWORD size = sizeof(computerName);
			if (GetComputerNameA(computerName, &size)) {
				return std::string(computerName);
			}
#elif __APPLE__ || __linux__
			char hostname[256];
			if (gethostname(hostname, sizeof(hostname)) == 0) {
				return {hostname};
			}
#endif
			return "UNKNOWN_PC";
		}

		bool generateDeviceInfo() {
			std::vector<std::string> components;
			int validComponents = 0;

			// Try all primary identifiers
			std::string machineGuid = getMachineGuid();
			if (!machineGuid.empty()) {
				components.push_back("MG:" + machineGuid);
				validComponents++;
			}

			std::string mobo = getMotherboardSerial();
			if (!mobo.empty()) {
				components.push_back("MB:" + mobo);
				validComponents++;
			}

			std::string cpu = getCPUId();
			if (!cpu.empty()) {
				components.push_back("CPU:" + cpu);
				validComponents++;
			}

			std::string mac = getPhysicalMacAddress();
			if (!mac.empty()) {
				components.push_back("MAC:" + mac);
				validComponents++;
			}

			std::string osId = getOSProductId();
			if (!osId.empty()) {
				components.push_back("OS:" + osId);
				validComponents++;
			}

			// FALLBACK: If less than 2, add less stable identifiers
			if (validComponents < 2) {
				// Username
				std::string username = getUsername();
				if (!username.empty()) {
					components.push_back("USER:" + username);
					validComponents++;
				}

				// Hostname
				std::string hostname = getComputerName();
				if (!hostname.empty() && hostname != "UNKNOWN_PC") {
					components.push_back("HOST:" + hostname);
					validComponents++;
				}

				// Disk serial (if available)
				std::string diskSerial = getDiskSerial();
				if (!diskSerial.empty()) {
					components.push_back("DISK:" + diskSerial);
					validComponents++;
				}
			}

			// Still nothing? Generate a random persistent ID and store it
			if (validComponents == 0) {
				// Generate and save a UUID to persistent storage
				std::string persistentId = loadOrGeneratePersistentId();
				components.push_back("PERSISTENT:" + persistentId);
				validComponents = 1;
			}

			// Build fingerprint
			std::stringstream ss;
			for (size_t i = 0; i < components.size(); i++) {
				ss << components[i];
				if (i < components.size() - 1) ss << "|";
			}

			hwFingerprint = ss.str();
			deviceId = sha256(hwFingerprint);

			return true; // Always succeed now
		}

private:
	OSCredentialStore* credStore_;

	static std::string getUsername() {
#ifdef _WIN32
		char username[256];
		DWORD size = sizeof(username);
		if (GetUserNameA(username, &size)) {
			return std::string(username);
		}
#else
		const char* user = getenv("USER");
		if (user) return {user};
#endif
		return "";
	}

	static std::string getDiskSerial() {
#ifdef _WIN32
		DWORD serialNum = 0;
		if (GetVolumeInformationA("C:\\", NULL, 0, &serialNum, NULL, NULL, NULL, 0)) {
			std::stringstream ss;
			ss << std::hex << std::setw(8) << std::setfill('0') << serialNum;
			return ss.str();
		}
#elif __APPLE__
		// macOS: Get boot volume UUID
		struct statfs buf{};
		if (statfs("/", &buf) == 0) {
			return {buf.f_mntfromname};
		}
#elif __linux__
		// Linux: Try to get root filesystem UUID
		std::ifstream file("/proc/mounts");
		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				if (line.find(" / ") != std::string::npos) {
					size_t start = line.find("UUID=");
					if (start != std::string::npos) {
						start += 5;
						size_t end = line.find_first_of(" ,", start);
						return line.substr(start, end - start);
					}
				}
			}
		}
#endif
		return "";
	}

	static std::string generateUUID() {
		std::stringstream ss;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 15);

		const char* hex = "0123456789abcdef";
		for (int i = 0; i < 32; i++) {
			if (i == 8 || i == 12 || i == 16 || i == 20) ss << "-";
			ss << hex[dis(gen)];
		}
		return ss.str();
	}

	// Load or generate persistent ID using CredStore
	[[nodiscard]] std::string loadOrGeneratePersistentId() const {
		if (!credStore_) {
			return ""; // No credential store available
		}

		// Try to load from credential store
		std::string savedId = credStore_->loadCredential("device_persistent_id");
		if (!savedId.empty()) {
			return savedId;
		}

		// Generate new UUID and save to credential store
		std::string newId = generateUUID();
		credStore_->saveCredential("device_persistent_id", newId);

		return newId;
	}


	// ===== MACHINE GUID / UUID =====
	static std::string getMachineGuid() {
#ifdef _WIN32
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			"SOFTWARE\\Microsoft\\Cryptography",
			0, KEY_READ, &hKey) == ERROR_SUCCESS) {

			char value[256];
			DWORD size = sizeof(value);
			if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL,
				(LPBYTE)value, &size) == ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return std::string(value);
			}
			RegCloseKey(hKey);
		}
#elif __APPLE__
		// macOS: Use IOPlatformUUID
		io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
		auto uuidCf = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(ioRegistryRoot,
		                                                                       CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0));
		IOObjectRelease(ioRegistryRoot);

		if (uuidCf) {
			char buffer[256];
			if (CFStringGetCString(uuidCf, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
				CFRelease(uuidCf);
				return {buffer};
			}
			CFRelease(uuidCf);
		}
#elif __linux__
		// Linux: Try /etc/machine-id or /var/lib/dbus/machine-id
		std::ifstream file("/etc/machine-id");
		if (!file.is_open()) {
			file.open("/var/lib/dbus/machine-id");
		}
		if (file.is_open()) {
			std::string id;
			std::getline(file, id);
			file.close();
			if (!id.empty()) {
				return id;
			}
		}
#endif
		return "";
	}

	// ===== MOTHERBOARD / HARDWARE SERIAL =====
	static std::string getMotherboardSerial() {
#ifdef _WIN32
		std::string result;
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			"HARDWARE\\DESCRIPTION\\System\\BIOS",
			0, KEY_READ, &hKey) == ERROR_SUCCESS) {

			char value[256];
			DWORD size = sizeof(value);
			if (RegQueryValueExA(hKey, "BaseBoardProduct", NULL, NULL,
				(LPBYTE)value, &size) == ERROR_SUCCESS) {
				result += std::string(value);
			}

			size = sizeof(value);
			if (RegQueryValueExA(hKey, "BaseBoardSerialNumber", NULL, NULL,
				(LPBYTE)value, &size) == ERROR_SUCCESS) {
				result += std::string(value);
			}

			RegCloseKey(hKey);
		}
		return result;
#elif __APPLE__
		// macOS: Use IOPlatformSerialNumber
		io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
		auto serialCf = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(ioRegistryRoot,
			CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0));
		IOObjectRelease(ioRegistryRoot);

		if (serialCf) {
			char buffer[256];
			if (CFStringGetCString(serialCf, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
				CFRelease(serialCf);
				return {buffer};
			}
			CFRelease(serialCf);
		}
#elif __linux__
		// Linux: Try DMI/SMBIOS data
		std::ifstream file("/sys/class/dmi/id/board_serial");
		if (file.is_open()) {
			std::string serial;
			std::getline(file, serial);
			file.close();
			if (!serial.empty() && serial != "None") {
				return serial;
			}
		}

		// Try product_uuid
		file.open("/sys/class/dmi/id/product_uuid");
		if (file.is_open()) {
			std::string uuid;
			std::getline(file, uuid);
			file.close();
			if (!uuid.empty()) {
				return uuid;
			}
		}
#endif
		return "";
	}

	// ===== CPU ID =====
	static std::string getCPUId() {
#ifdef _WIN32
		int cpuInfo[4] = { 0 };
		__cpuid(cpuInfo, 0);
		char vendor[13] = { 0 };
		*reinterpret_cast<int*>(vendor) = cpuInfo[1];
		*reinterpret_cast<int*>(vendor + 4) = cpuInfo[3];
		*reinterpret_cast<int*>(vendor + 8) = cpuInfo[2];

		__cpuid(cpuInfo, 1);

		std::stringstream ss;
		ss << vendor << "_"
			<< std::hex << std::setw(8) << std::setfill('0') << cpuInfo[3]
			<< std::hex << std::setw(8) << std::setfill('0') << cpuInfo[0];
		return ss.str();
#elif __APPLE__
		char buffer[256];
		size_t size = sizeof(buffer);

		// Get CPU brand string
		if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, NULL, 0) == 0) {
			return {buffer};
		}
#elif __linux__
		std::ifstream cpuinfo("/proc/cpuinfo");
		if (cpuinfo.is_open()) {
			std::string line;
			while (std::getline(cpuinfo, line)) {
				if (line.find("processor") == 0 || line.find("model name") == 0) {
					size_t pos = line.find(":");
					if (pos != std::string::npos) {
						return line.substr(pos + 2);
					}
				}
			}
			cpuinfo.close();
		}
#endif
		return "";
	}

	// ===== PHYSICAL MAC ADDRESS =====
	static std::string getPhysicalMacAddress() {
#ifdef _WIN32
		IP_ADAPTER_ADDRESSES* addresses = nullptr;
		ULONG bufferSize = 15000;
		DWORD result;

		addresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
		if (!addresses) return "";

		result = GetAdaptersAddresses(AF_UNSPEC,
			GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST,
			NULL, addresses, &bufferSize);

		if (result != NO_ERROR) {
			free(addresses);
			return "";
		}

		std::vector<std::pair<int, std::string>> candidates;

		for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter; adapter = adapter->Next) {
			if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
				adapter->PhysicalAddressLength != 6) {
				continue;
			}

			int priority = (adapter->IfType == IF_TYPE_ETHERNET_CSMACD) ? 100 : 50;

			std::stringstream ss;
			for (UINT i = 0; i < adapter->PhysicalAddressLength; i++) {
				ss << std::hex << std::setw(2) << std::setfill('0')
					<< (int)adapter->PhysicalAddress[i];
			}

			candidates.push_back({ priority, ss.str() });
		}

		free(addresses);

		if (!candidates.empty()) {
			std::sort(candidates.begin(), candidates.end(),
				[](const auto& a, const auto& b) { return a.first > b.first; });
			return candidates[0].second;
		}
#elif __APPLE__ || __linux__
		struct ifaddrs* ifap, * ifaptr;

		if (getifaddrs(&ifap) == 0) {
			std::vector<std::pair<int, std::string>> candidates;

			for (ifaptr = ifap; ifaptr != nullptr; ifaptr = ifaptr->ifa_next) {
				if (ifaptr->ifa_addr == nullptr) continue;

#ifdef __APPLE__
				if (ifaptr->ifa_addr->sa_family == AF_LINK) {
					auto* sdl = reinterpret_cast<struct sockaddr_dl *>(ifaptr->ifa_addr);
					if (sdl->sdl_alen == 6) {
						auto* mac = reinterpret_cast<unsigned char *>(LLADDR(sdl));

						// Skip loopback and zero MACs
						bool isZero = true;
						for (int i = 0; i < 6; i++) {
							if (mac[i] != 0) {
								isZero = false;
								break;
							}
						}
						if (isZero) continue;

						std::stringstream ss;
						for (int i = 0; i < 6; i++) {
							ss << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
						}

						// Prioritize en0 (primary interface)
						int priority = (std::string(ifaptr->ifa_name) == "en0") ? 100 : 50;
						candidates.emplace_back( priority, ss.str() );
					}
				}
#elif __linux__
				if (ifaptr->ifa_addr->sa_family == AF_PACKET) {
					struct ifreq ifr;
					int sock = socket(AF_INET, SOCK_DGRAM, 0);
					if (sock < 0) continue;

					strncpy(ifr.ifr_name, ifaptr->ifa_name, IFNAMSIZ - 1);
					if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
						unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;

						bool isZero = true;
						for (int i = 0; i < 6; i++) {
							if (mac[i] != 0) {
								isZero = false;
								break;
							}
						}

						if (!isZero) {
							std::stringstream ss;
							for (int i = 0; i < 6; i++) {
								ss << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
							}

							// Prioritize eth0/enp interfaces
							std::string name = ifaptr->ifa_name;
							int priority = (name.find("eth") == 0 || name.find("enp") == 0) ? 100 : 50;
							candidates.push_back({ priority, ss.str() });
						}
					}
					close(sock);
				}
#endif
			}

			freeifaddrs(ifap);

			if (!candidates.empty()) {
				std::sort(candidates.begin(), candidates.end(),
					[](const auto& a, const auto& b) { return a.first > b.first; });
				return candidates[0].second;
			}
		}
#endif
		return "";
	}

	// ===== OS-SPECIFIC PRODUCT ID =====
	static std::string getOSProductId() {
#ifdef _WIN32
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
			0, KEY_READ, &hKey) == ERROR_SUCCESS) {

			char value[256];
			DWORD size = sizeof(value);
			if (RegQueryValueExA(hKey, "ProductId", NULL, NULL,
				(LPBYTE)value, &size) == ERROR_SUCCESS) {
				RegCloseKey(hKey);
				return std::string(value);
			}
			RegCloseKey(hKey);
		}
#elif __APPLE__
		// macOS: Use host UUID
		char buffer[256];
		size_t size = sizeof(buffer);
		if (sysctlbyname("kern.uuid", buffer, &size, nullptr, 0) == 0) {
			return {buffer};
		}
#elif __linux__
		// Linux: Use OS ID from /etc/os-release
		std::ifstream file("/etc/os-release");
		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				if (line.find("ID=") == 0) {
					return line.substr(3);
				}
			}
			file.close();
		}
#endif
		return "";
	}

	// ===== SHA256 HASH =====
	static std::string sha256(const std::string& input) {
#ifdef USE_SSL
		unsigned char hash[SHA256_DIGEST_LENGTH];

		EVP_MD_CTX* ctx = EVP_MD_CTX_new();
		EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
		EVP_DigestUpdate(ctx, input.c_str(), input.size());
		unsigned int len = SHA256_DIGEST_LENGTH;
		EVP_DigestFinal_ex(ctx, hash, &len);
		EVP_MD_CTX_free(ctx);

		std::stringstream ss;
		for (unsigned char i : hash) {
			ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
		}
		return ss.str();
#else
		unsigned long hash = 5381;
		for (char c : input) {
			hash = ((hash << 5) + hash) + c;
		}
		std::stringstream ss;
		ss << std::hex << hash;
		return ss.str();
#endif
	}


	};

	namespace {
		std::string urlEncode(const std::string& value) {
			std::ostringstream escaped;
			for (unsigned char c : value) {
				if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
					escaped << c;
				else {
					escaped << "%" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)c;
				}
			}
			return escaped.str();
		}

		std::string extractJsonString(const std::string& json, const std::string& key) {
			std::string searchKey = "\"" + key + "\":\"";
			size_t pos = json.find(searchKey);
			if (pos == std::string::npos) return "";
			size_t start = pos + searchKey.length();
			size_t end = json.find('\"', start);
			if (end == std::string::npos) return "";
			return json.substr(start, end - start);
		}

		bool extractJsonBool(const std::string& json, const std::string& key) {
			std::string searchKey = "\"" + key + "\":";
			size_t pos = json.find(searchKey);
			if (pos == std::string::npos) return false;
			size_t start = pos + searchKey.length();
			while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
			if (start + 4 <= json.length() && json.substr(start, 4) == "true") return true;
			return false;
		}

		int extractJsonInt(const std::string& json, const std::string& key) {
			std::string searchKey = "\"" + key + "\":";
			size_t pos = json.find(searchKey);
			if (pos == std::string::npos) return -1;
			size_t start = pos + searchKey.length();
			while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
			size_t end = start;
			while (end < json.length() && (isdigit((unsigned char)json[end]) || json[end] == '-')) end++;
			if (end > start) {
				try { return std::stoi(json.substr(start, end - start)); }
				catch (...) { return -1; }
			}
			return -1;
		}

		std::string extractJsonObject(const std::string& json, const std::string& key) {
			std::string searchKey = "\"" + key + "\":";
			size_t pos = json.find(searchKey);
			if (pos == std::string::npos) return "";
			size_t start = pos + searchKey.length();
			while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
			if (start >= json.length() || json[start] != '{') return "";
			int braceCount = 0;
			size_t end = start;
			do {
				if (json[end] == '{') braceCount++;
				else if (json[end] == '}') braceCount--;
				end++;
			} while (end < json.length() && braceCount > 0);
			if (braceCount == 0) return json.substr(start, end - start);
			return "";
		}

		std::vector<DeviceInfo> parseDeviceArray(const std::string& json) {
			std::vector<DeviceInfo> devices;

			size_t arrayStart = json.find('[');
			if (arrayStart == std::string::npos) return devices;

			size_t pos = arrayStart + 1;
			while (pos < json.length()) {
				size_t objStart = json.find('{', pos);
				if (objStart == std::string::npos) break;

				int braceCount = 0;
				size_t objEnd = objStart;
				do {
					if (json[objEnd] == '{') braceCount++;
					else if (json[objEnd] == '}') braceCount--;
					objEnd++;
				} while (objEnd < json.length() && braceCount > 0);

				if (braceCount == 0) {
					std::string deviceObj = json.substr(objStart, objEnd - objStart);

					DeviceInfo info;
					info.deviceId = extractJsonString(deviceObj, "deviceId");
					info.deviceName = extractJsonString(deviceObj, "deviceName");
					info.registeredAt = extractJsonString(deviceObj, "registeredAt");
					info.lastSeenAt = extractJsonString(deviceObj, "lastSeenAt");
					info.daysInactive = extractJsonInt(deviceObj, "daysInactive");

					devices.push_back(info);
				}

				pos = objEnd;

				size_t commaPos = json.find(',', pos);
				if (commaPos == std::string::npos || commaPos > json.find(']', pos)) {
					break;
				}
				pos = commaPos + 1;
			}

			return devices;
		}



		bool initWinsock(std::string& lastStatus, std::string& lastError) {
#ifdef _WIN32
			WSADATA wsaData;
			int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (r != 0) {
				lastStatus = "Winsock initialization failed";
				lastError = "WSAStartup error: " + std::to_string(r);
				return false;
			}
#endif
			return true;
		}

		void cleanupWinsock() {
#ifdef _WIN32
			WSACleanup();
#endif
		}

		bool setSockTimeout(socket_t sock, int seconds, std::string& lastStatus) {
#ifdef _WIN32
			DWORD timeout = seconds * 1000;
			if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
				lastStatus = "Failed to set receive timeout";
				return false;
			}
			if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
				lastStatus = "Failed to set send timeout";
				return false;
			}
#else
			struct timeval tv{};
			tv.tv_sec = seconds;
			tv.tv_usec = 0;
			if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
				lastStatus = "Failed to set receive timeout";
				return false;
			}
			if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
				lastStatus = "Failed to set send timeout";
				return false;
			}
#endif
			return true;
		}
	}

	class HttpClient {
	public:
		HttpClient(std::string  hostname, int serverPort, bool https, std::string& lastStatusMessage, std::string& lastErrorDetails, int& lastHttpStatusCode, int timeout = 30)
			: host(std::move(hostname)), port(serverPort), useHttps(https), lastStatusMessage(lastStatusMessage), lastErrorDetails(lastErrorDetails), lastHttpStatusCode(lastHttpStatusCode), timeoutSeconds(timeout) {
#ifdef USE_SSL
			ssl_ctx = nullptr;
			if (useHttps && !initSSL()) {
				std::cerr << "SSL initialization failed" << std::endl;
				useHttps = false;
			}
#else
			if (https) {
				std::cerr << "HTTPS requested but SSL support not compiled in. Using HTTP." << std::endl;
				useHttps = false;
			}
#endif
		}

		~HttpClient() {
#ifdef USE_SSL
			cleanupSSL();
#endif
		}
		std::string sendHttpRequest(const std::string& method, const std::string& path,
			const std::string& data = "", const std::string& contentType = "") {

			lastStatusMessage.clear();
			lastErrorDetails.clear();
			lastHttpStatusCode = -1;

			if (!initWinsock(lastStatusMessage, lastErrorDetails)) {
				lastHttpStatusCode = 0;
				return "";
			}

			socket_t sock = createSocketInternal(lastStatusMessage);
			if (sock == INVALID_SOCKET) {
				lastHttpStatusCode = 0;
				cleanupWinsock();
				return "";
			}

			if (!connectToServerInternal(sock, host, port)) {
				lastHttpStatusCode = 0;
				close_socket(sock);
				cleanupWinsock();
				return "";
			}

			std::string response;

#ifdef USE_SSL
			SSL* sslHandle = nullptr;
			if (useHttps) {
				sslHandle = SSL_new(ssl_ctx);
				if (!sslHandle) {
					lastStatusMessage = "SSL initialization failed";
					lastHttpStatusCode = 0;
					close_socket(sock);
					cleanupWinsock();
					return "";
				}
				SSL_set_fd(sslHandle, sock);
				SSL_set_tlsext_host_name(sslHandle, host.c_str());

				if (SSL_connect(sslHandle) <= 0) {
					lastStatusMessage = "SSL initialization failed";
					lastHttpStatusCode = 0;
					ERR_print_errors_fp(stderr);
					SSL_free(sslHandle);
					close_socket(sock);
					cleanupWinsock();
					return "";
				}
			}
#endif

			std::stringstream req;
			req << method << " " << path << " HTTP/1.1\r\n";
			req << "Host: " << host;
			if ((useHttps && port != 443) || (!useHttps && port != 80)) req << ":" << port;
			req << "\r\n";
			req << "Connection: close\r\n";
			req << "User-Agent: LicenseChecker/1.0\r\n";

			if (!contentType.empty()) req << "Content-Type: " << contentType << "\r\n";
			if (!cookies.empty()) req << "Cookie: " << cookies << "\r\n";
			if (!data.empty()) req << "Content-Length: " << data.length() << "\r\n";

			req << "\r\n";
			if (!data.empty()) req << data;

			std::string reqStr = req.str();

#ifdef USE_SSL
			if (useHttps && sslHandle) {
				if (SSL_write(sslHandle, reqStr.c_str(), (int)reqStr.length()) <= 0) {
					lastStatusMessage = "Failed to send HTTP request";
					lastHttpStatusCode = 0;
					SSL_free(sslHandle);
					close_socket(sock);
					cleanupWinsock();
					return "";
				}
			}
			else
#endif
			{
				if (send(sock, reqStr.c_str(), (int)reqStr.length(), 0) <= 0) {
					lastStatusMessage = "Failed to send HTTP request";
					lastHttpStatusCode = 0;
#ifdef _WIN32
					lastErrorDetails = "Send error: " + std::to_string(WSAGetLastError());
#endif
					close_socket(sock);
					cleanupWinsock();
					return "";
				}
			}

			char buffer[4096];
			int bytes = 0;
			while (true) {
#ifdef USE_SSL
				if (useHttps && sslHandle) bytes = SSL_read(sslHandle, buffer, (int)sizeof(buffer) - 1);
				else
#endif
					bytes = recv(sock, buffer, (int)sizeof(buffer) - 1, 0);

				if (bytes > 0) {
					buffer[bytes] = '\0';
					response += buffer;

					if (response.find("\r\n\r\n") != std::string::npos) {
						size_t clPos = response.find("Content-Length: ");
						if (clPos != std::string::npos) {
							size_t clEnd = response.find("\r\n", clPos);
							if (clEnd != std::string::npos) {
								size_t valStart = clPos + 16; // strlen("Content-Length: ")
								int contentLength = std::stoi(response.substr(valStart, clEnd - valStart));
								size_t bodyStart = response.find("\r\n\r\n") + 4;
								if (response.length() - bodyStart >= (size_t)contentLength) break;
							}
						}
					}
				}
				else if (bytes == 0) {
					break;
				}
				else {
					lastStatusMessage = "Error receiving response";
					lastHttpStatusCode = 0;
#ifdef _WIN32
					int wsaErr = WSAGetLastError();
					if (wsaErr == WSAETIMEDOUT) {
						lastStatusMessage = "Response timeout after " + std::to_string(timeoutSeconds) + " seconds";
					}
					lastErrorDetails = "Receive error: " + std::to_string(wsaErr);
#endif
					break;
				}
			}

#ifdef USE_SSL
			if (useHttps && sslHandle) {
				SSL_shutdown(sslHandle);
				SSL_free(sslHandle);
			}
#endif

			close_socket(sock);
			cleanupWinsock();

			if (response.empty()) {
				if (lastStatusMessage.empty()) lastStatusMessage = "Empty response received";
				if (lastHttpStatusCode == -1) lastHttpStatusCode = 0;
				return "";
			}

			lastHttpStatusCode = parseHttpStatusCode(response);

			if (lastHttpStatusCode == -1) {
				lastStatusMessage = "Malformed HTTP response - no valid status code found";
				return "";
			}

			if (lastHttpStatusCode >= 400) {
				std::stringstream em;
				em << "HTTP " << lastHttpStatusCode << ": ";
				switch (lastHttpStatusCode) {
				case 400: em << " (Bad Request)"; break;
				case 401: em << " (Unauthorized)"; break;
				case 403: em << " (Forbidden)"; break;
				case 404: em << " (Not Found)"; break;
				case 500: em << " (Internal Server Error)"; break;
				case 502: em << " (Bad Gateway)"; break;
				case 503: em << " (Service Unavailable)"; break;
				default: break;
				}
				lastStatusMessage = em.str();
				extractAndMergeCookies(response);

				// CHANGE: Return the body instead of empty string
				size_t bodyPos = response.find("\r\n\r\n");
				if (bodyPos != std::string::npos) {
					return response.substr(bodyPos + 4);  // Return JSON body
				}
				return "";  // Only return empty if no body found
			}

			if (lastHttpStatusCode >= 300 && lastHttpStatusCode < 400) {
				lastStatusMessage = "Redirect received but not handled";
				extractAndMergeCookies(response);
				return "";
			}

			extractAndMergeCookies(response);

			size_t bodyPos = response.find("\r\n\r\n");
			if (bodyPos != std::string::npos) {
				lastStatusMessage = "Request completed successfully";
				return response.substr(bodyPos + 4);
			}

			lastStatusMessage = "Malformed HTTP response - no valid status code found";
			return response;
		}

		void setTimeout(int s) { timeoutSeconds = s; }
		[[nodiscard]] int getTimeout() const { return timeoutSeconds; }

		void addCookie(const std::string& c) {
			if (!cookies.empty() && !c.empty()) cookies += "; ";
			cookies += c;
		}


		bool testInternetConnection() {
			lastStatusMessage.clear();
			lastErrorDetails.clear();
#ifdef _WIN32
			if (!initWinsock(lastStatusMessage, lastErrorDetails)) return false;
#endif

			const socket_t sock = createSocketInternal(lastStatusMessage);
			if (sock == INVALID_SOCKET) {
				cleanupWinsock();
				return false;
			}
			struct sockaddr_in serv {};
			serv.sin_family = AF_INET;
			serv.sin_port = htons(443);
			inet_pton(AF_INET, "1.1.1.1", &serv.sin_addr);

			const bool connected = (connect(sock, reinterpret_cast<struct sockaddr *>(&serv), sizeof(serv)) != SOCKET_ERROR);
			close_socket(sock);
			cleanupWinsock();

			lastStatusMessage = connected ? "Internet connection available" : "No internet connection available";
			return connected;
		}

		bool testServerReachability() {
			const std::string resp = sendHttpRequest("GET", "/api/ping");
			if (resp.empty()) {
				if (lastHttpStatusCode >= 400) lastStatusMessage = "HTTP " + std::to_string(lastHttpStatusCode);
				else if (lastStatusMessage.empty()) lastStatusMessage = "Server unreachable";
				return false;
			}
			bool ok = resp.find("\"pong\":") != std::string::npos;
			lastStatusMessage = ok ? "Server is reachable" : "Server responded but ping failed";
			return ok;
		}
	private:
		std::string hwFingerprint;
		std::string cookies;
		std::string host;
		bool useHttps;
		int port;
		int timeoutSeconds;
		std::string& lastStatusMessage;
		std::string& lastErrorDetails;
		int& lastHttpStatusCode;

#ifdef USE_SSL
		SSL_CTX* ssl_ctx;

		bool initSSL() {
			const SSL_METHOD* method = TLS_client_method();
			ssl_ctx = SSL_CTX_new(method);
			if (!ssl_ctx) {
				lastStatusMessage = "SSL context creation failed";
				lastErrorDetails = "SSL_CTX_new() returned null";
				ERR_print_errors_fp(stderr);
				return false;
			}
			SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
			SSL_CTX_set_cipher_list(ssl_ctx, "DEFAULT:!aNULL:!eNULL:!MD5:!3DES:!DES:!RC4:!IDEA:!SEED:!aDSS:!SRP:!PSK");
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
			SSL_CTX_set_ciphersuites(ssl_ctx, "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
#endif
			SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
			return true;
		}

		void cleanupSSL() {
			if (ssl_ctx) {
				SSL_CTX_free(ssl_ctx);
				ssl_ctx = nullptr;
			}
		}
#endif

		socket_t createSocketInternal(std::string& lastStatus) {
			socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sock == INVALID_SOCKET) {
				lastStatus = "Socket creation failed";
#ifdef _WIN32
				lastErrorDetails = "WSA error: " + std::to_string(WSAGetLastError());
#else
				lastErrorDetails = "errno: " + std::to_string(errno);
#endif
				return INVALID_SOCKET;
			}
			if (!setSockTimeout(sock, timeoutSeconds, lastStatus)) {
				close_socket(sock);
				return INVALID_SOCKET;
			}
			return sock;
		}

		bool connectToServerInternal(socket_t sock, const std::string& hostname, int serverPort) {
			struct sockaddr_in serverAddr {};
			serverAddr.sin_family = AF_INET;
			serverAddr.sin_port = htons(serverPort);

			if (hostname == "localhost" || hostname == "127.0.0.1") {
				if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) != 1) {
					lastStatusMessage = "Connection failed to " + hostname + ":" + std::to_string(serverPort);
					return false;
				}
			}
			else {
				auto he = gethostbyname(hostname.c_str());
				if (!he) {
					lastStatusMessage = "DNS resolution failed for " + hostname;
#ifdef _WIN32
					lastErrorDetails = "WSA error: " + std::to_string(WSAGetLastError());
#else
					lastErrorDetails = "h_errno: " + std::to_string(h_errno);
#endif
					return false;
				}
				memcpy(&serverAddr.sin_addr, he->h_addr_list[0], he->h_length);
			}

			if (connect(sock, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
				lastStatusMessage = "Connection failed to " + hostname + ":" + std::to_string(serverPort);
#ifdef _WIN32
				lastErrorDetails = "WSA error: " + std::to_string(WSAGetLastError());
#else
				lastErrorDetails = "errno: " + std::to_string(errno);
#endif
				return false;
			}
			return true;
		}

		static int parseHttpStatusCode(const std::string& response) {
			size_t httpPos = response.find("HTTP/1.1");
			if (httpPos == std::string::npos) return -1;
			size_t spacePos = response.find(' ', httpPos);
			if (spacePos == std::string::npos) return -1;
			size_t codeStart = spacePos + 1;
			size_t codeEnd = response.find(' ', codeStart);
			if (codeEnd == std::string::npos) {
				codeEnd = response.find('\r', codeStart);
				if (codeEnd == std::string::npos) codeEnd = response.find('\n', codeStart);
			}
			if (codeEnd == std::string::npos || (codeEnd - codeStart) != 3) return -1;
			try { return std::stoi(response.substr(codeStart, 3)); }
			catch (...) { return -1; }
		}

		void extractAndMergeCookies(const std::string& response) {
			std::string newCookies;
			size_t pos = 0;
			const std::string setCookie = "Set-Cookie: ";
			const std::string crlf = "\r\n";

			while ((pos = response.find(setCookie, pos)) != std::string::npos) {
				size_t start = pos + setCookie.length();
				size_t end = response.find(crlf, start);
				if (end != std::string::npos) {
					std::string full = response.substr(start, end - start);
					size_t semi = full.find(';');
					std::string cookie = (semi != std::string::npos) ? full.substr(0, semi) : full;

					if (cookie.find("next-auth") != std::string::npos ||
						cookie.find("csrf") != std::string::npos ||
						cookie.find("session") != std::string::npos) {

						if (!newCookies.empty()) newCookies += "; ";
						newCookies += cookie;
					}
				}
				pos = (pos == std::string::npos) ? std::string::npos : pos + 1;
			}

			if (!newCookies.empty()) {
				if (!cookies.empty()) cookies += "; ";
				cookies += newCookies;
			}
		}
	};


	// ======================= Offline licence token =========================
	//
	// Wire format, deliberately NOT a JWT:
	//
	//     base64url(payload JSON) "." base64url(ed25519 signature)
	//
	// The algorithm is fixed here rather than read from the token, so the whole
	// JWT algorithm-confusion family (`alg: none`, HMAC/RSA substitution) simply
	// does not apply. It is also far less parsing code.
	//
	// The embedded key is the PUBLIC half. Signing happens only on the server.
	namespace {

		const char* kLicensePublicKeyPEM =
			"-----BEGIN PUBLIC KEY-----\n"
			"MCowBQYDK2VwAyEATmy5sN7Z1aq9gqucm1msnOFNyTKgPA3NPvp96r2rEss=\n"
			"-----END PUBLIC KEY-----\n";

		// Credential-store keys.
		const char* kTokenCredKey = "lic_token";
		// Highest wall-clock time ever observed. Without this, "set the system
		// clock back a year" would trivially defeat expiry.
		const char* kClockCredKey = "lic_seen";

		// Come back to the server once the token is inside this many days of
		// expiry. Well before it lapses, so a normally-online machine refreshes
		// silently and never sees a warning.
		constexpr long kRefreshWithinDays = 7;
		// Tolerance for timezone/DST/NTP jitter before calling it a rollback.
		constexpr long kClockSlackSeconds = 24 * 60 * 60;

		std::string base64UrlDecode(const std::string& in) {
			static const std::string tbl =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string s;
			s.reserve(in.size() + 4);
			for (char c : in) {
				if (c == '-') s += '+';
				else if (c == '_') s += '/';
				else if (c == '\n' || c == '\r' || c == '=') continue;
				else s += c;
			}
			std::string out;
			int val = 0, bits = -8;
			for (unsigned char c : s) {
				size_t idx = tbl.find(static_cast<char>(c));
				if (idx == std::string::npos) return "";   // reject junk outright
				val = (val << 6) + static_cast<int>(idx);
				bits += 6;
				if (bits >= 0) {
					out += static_cast<char>((val >> bits) & 0xFF);
					bits -= 8;
				}
			}
			return out;
		}

		// Ed25519 signature check. Ed25519 has no separate digest step, hence the
		// null EVP_MD and the one-shot EVP_DigestVerify.
		bool ed25519Verify(const std::string& message, const std::string& signature) {
#ifdef USE_SSL
			if (signature.size() != 64) return false;      // Ed25519 sigs are fixed size

			BIO* bio = BIO_new_mem_buf(kLicensePublicKeyPEM, -1);
			if (!bio) return false;
			EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
			BIO_free(bio);
			if (!pkey) return false;

			EVP_MD_CTX* ctx = EVP_MD_CTX_new();
			if (!ctx) { EVP_PKEY_free(pkey); return false; }

			bool ok = false;
			if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
				ok = EVP_DigestVerify(
					ctx,
					reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
					reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
			}
			EVP_MD_CTX_free(ctx);
			EVP_PKEY_free(pkey);
			return ok;
#else
			(void)message; (void)signature;
			return false;   // without SSL we cannot verify, so we must not trust
#endif
		}

		long nowUnix() {
			return static_cast<long>(std::time(nullptr));
		}
	} // namespace

	void LicenseChecker::clearLicenseToken() const {
		credStore->deleteCredential(kTokenCredKey);
	}

	TokenStatus LicenseChecker::checkStoredToken() const {
		return checkStoredToken(productNumber);
	}

	TokenStatus LicenseChecker::checkStoredToken(int productNumber_) const {
		TokenStatus st;

		const std::string token = credStore->loadCredential(kTokenCredKey);
		if (token.empty()) {
			st.reason = "No stored licence token";
			return st;
		}
		st.present = true;

		const size_t dot = token.find('.');
		if (dot == std::string::npos || dot == 0 || dot + 1 >= token.size()) {
			st.tampered = true;
			st.reason = "Malformed licence token";
			return st;
		}

		const std::string payloadB64 = token.substr(0, dot);
		const std::string sig = base64UrlDecode(token.substr(dot + 1));

		// Signature is computed over the base64url payload text exactly as sent,
		// so we must verify the ENCODED form and not a re-serialised copy.
		if (sig.empty() || !ed25519Verify(payloadB64, sig)) {
			st.tampered = true;
			st.reason = "Licence token signature is not valid";
			return st;
		}

		const std::string payload = base64UrlDecode(payloadB64);
		if (payload.empty()) {
			st.tampered = true;
			st.reason = "Unreadable licence token payload";
			return st;
		}

		const std::string did = extractJsonString(payload, "did");
		const int pn = extractJsonInt(payload, "pn");
		const long iat = static_cast<long>(extractJsonInt(payload, "iat"));
		const long exp = static_cast<long>(extractJsonInt(payload, "exp"));
		const int graceDays = extractJsonInt(payload, "grace");
		st.purchaseType = extractJsonInt(payload, "typ");

		// Bind to this machine. Copying the credential store to another computer
		// yields a token whose device id no longer matches its fingerprint.
		if (did.empty() || did != deviceFingerPrint->deviceId) {
			st.tampered = true;
			st.reason = "Licence token was issued for a different device";
			return st;
		}
		if (pn != productNumber_) {
			st.tampered = true;
			st.reason = "Licence token was issued for a different product";
			return st;
		}

		// Clock-rollback guard. Track the highest time we have ever seen; if the
		// clock is now meaningfully behind that, someone is winding it back to
		// keep an expired token alive.
		long seen = 0;
		const std::string seenStr = credStore->loadCredential(kClockCredKey);
		if (!seenStr.empty()) {
			try { seen = std::stol(seenStr); } catch (...) { seen = 0; }
		}
		const long now = nowUnix();

		if (seen > 0 && now < seen - kClockSlackSeconds) {
			st.tampered = true;
			st.reason = "System clock has been set backwards";
			return st;
		}
		// A token issued in the future is equally suspect.
		if (iat > now + kClockSlackSeconds) {
			st.tampered = true;
			st.reason = "Licence token is dated in the future";
			return st;
		}
		if (now > seen) {
			credStore->saveCredential(kClockCredKey, std::to_string(now));
		}

		const long secsLeft = exp - now;
		st.daysRemaining = static_cast<int>(secsLeft / 86400);

		if (secsLeft > 0) {
			st.valid = true;
			st.refreshDue = secsLeft < kRefreshWithinDays * 86400;
			st.reason = st.refreshDue ? "Licence token valid, refresh due"
			                          : "Licence token valid";
			return st;
		}

		st.expired = true;
		st.refreshDue = true;
		const long graceSecs = static_cast<long>(graceDays > 0 ? graceDays : 0) * 86400;
		const long graceLeft = graceSecs + secsLeft;   // secsLeft is negative here
		if (graceLeft > 0) {
			st.inGrace = true;
			st.graceDaysRemaining = static_cast<int>(graceLeft / 86400);
			st.reason = "Licence token expired, running on offline grace period";
		} else {
			st.reason = "Licence token expired and the grace period has ended";
		}
		return st;
	}

	LicenseChecker::LicenseChecker(const std::string& baseUrl, int port, bool useHttps, int productNumber,
		const std::string& appName, std::function<void(const std::string&)>logger)
		: baseUrl(baseUrl), useHttps(useHttps), port(port), productNumber(productNumber), logger(std::move(logger)) {
		credStore = std::make_unique<OSCredentialStore>(appName);
		deviceFingerPrint = std::make_unique<DeviceFingerprint>(credStore.get());
		deviceFingerPrint->generateDeviceInfo();
		parseBaseUrl(baseUrl);
		httpClient = std::make_unique<HttpClient>(host, port, useHttps, lastStatusMessage, lastErrorDetails, lastHttpStatusCode);
	}

	LicenseChecker::~LicenseChecker() = default;

	void LicenseChecker::parseBaseUrl(const std::string& url) {
		size_t protocolEnd = url.find("://");
		if (protocolEnd != std::string::npos) {
			std::string protocol = url.substr(0, protocolEnd);
			useHttps = (protocol == "https");

			size_t hostStart = protocolEnd + 3;
			size_t portStart = url.find(':', hostStart);
			size_t pathStart = url.find('/', hostStart);

			if (portStart != std::string::npos && (pathStart == std::string::npos || portStart < pathStart)) {
				host = url.substr(hostStart, portStart - hostStart);
				size_t portEnd = (pathStart != std::string::npos) ? pathStart : url.length();
				port = std::stoi(url.substr(portStart + 1, portEnd - portStart - 1));
			}
			else {
				size_t hostEnd = (pathStart != std::string::npos) ? pathStart : url.length();
				host = url.substr(hostStart, hostEnd - hostStart);
				port = useHttps ? 443 : 80;
			}
		}
		else {
			host = url;
			if (port == 0) port = useHttps ? 443 : 3000;
		}
	}


	int LicenseChecker::login(const std::string& email, const std::string& password) {
		KeyPairGenerator keygen;
		auto kp = KeyPairGenerator::generateKeyPair();
		if (kp.privateKey.empty() || kp.publicKey.empty()) {
			lastStatusMessage = "Failed to generate key pair";
			return false;
		}
		std::string post =
			"&email=" + urlEncode(email) +
			"&password=" + urlEncode(password) +
			"&productNumber=" + std::to_string(productNumber) +
			"&deviceId=" + urlEncode(deviceFingerPrint->deviceId) +
			"&deviceName=" + urlEncode(tsl::DeviceFingerprint::getComputerName()) +
			"&publicKey=" +urlEncode(kp.publicKey);

		std::string loginResp = httpClient->sendHttpRequest("POST", "/api/auth/login", post, "application/x-www-form-urlencoded");
		int returnCode = 1;
		std::string message;
		if (!loginResp.empty()) {
			if (loginResp.find("\"error\"") != std::string::npos) {
				// Extract the error message from JSON
				message = extractJsonString(loginResp, "error");
			}
			else if (loginResp.find("\"message\"") != std::string::npos) {
				// Extract the error message from JSON
				message = extractJsonString(loginResp, "message");
			}
			if (loginResp.find("\"action\"") != std::string::npos) {
				// Extract the error message from JSON
				auto action = extractJsonString(loginResp, "action");
				if (!action.empty()) {
					returnCode = action == "stop" ? 0 : action == "tryagain" ? 1 : 2;
				}
			}
			if (loginResp.find("\"waitTimeMs\"") != std::string::npos) {
				waitTimeMs = extractJsonInt(loginResp, "waitTimeMs");
				returnCode = 3;				
			}
		}
		if (lastHttpStatusCode != 200) {
			if (message.empty()) {
				if (lastStatusMessage.empty()) {
					if (lastHttpStatusCode >= 400) lastStatusMessage = "HTTP " + std::to_string(lastHttpStatusCode); else lastStatusMessage = "Empty login response";
				}
			}
			else lastStatusMessage = message;	
		}
		else {
			if (message.empty()) lastStatusMessage = "No response from server";
			else lastStatusMessage = message;

		}
		if(returnCode == 2)
		if (!credStore->save(kp.privateKey)) { lastStatusMessage = "Login successful but failed to save credentials."; return 1; };
        return returnCode;
	}

	bool LicenseChecker::hasPrivateKey() {
		return credStore->has();
	}

	bool LicenseChecker::validateProduct(const int productNumber_) {
		ValidationResult r = validateProductDetailed(productNumber_);
		return r.success;
	}


	ValidationResult LicenseChecker::validateProductDetailed(const int productNumber_, const std::function<void(const std::string& result)>& logger_) {
		auto privateKey = credStore->load();
		if (privateKey.empty()) {
		}
		DataSigner signer;
		std::string signature = DataSigner::signWithPEM(deviceFingerPrint->deviceId, privateKey);

		ValidationResult result;
		std::stringstream jsonData;
		jsonData << "{"
			<< "\"productNumber\":" << productNumber_;

		if (!deviceFingerPrint->deviceId.empty()) {
			jsonData << R"(,"deviceId":")" << deviceFingerPrint->deviceId << "\"";
		}

		if (!signature.empty()) {
			jsonData << R"(,"deviceIdSigned":")" << signature << "\"";
		}



		jsonData << "}";

		std::string response = httpClient->sendHttpRequest("POST",
			"/api/license/validateProduct", jsonData.str(), "application/json");
		result.statusCode = lastHttpStatusCode;

		if (response.empty()) {
			if (lastHttpStatusCode >= 400) result.status = "HTTP " + std::to_string(lastHttpStatusCode);
			else result.status = lastStatusMessage.empty() ? "No response from license validation server" : lastStatusMessage;
			return result;
		}

		if (logger_) logger_(response);

		result.success = extractJsonBool(response, "success");
		result.status = extractJsonString(response, "status");
		result.action = extractJsonString(response, "action");
		if (result.action.empty()) result.action = "stop";
		std::string trialObj = extractJsonObject(response, "trial");
		if (!trialObj.empty()) {
			result.hasTrial = true;
			result.daysRemaining = extractJsonInt(trialObj, "daysRemaining");
			result.totalTrialDays = extractJsonInt(trialObj, "totalTrialDays");
			result.expiredDaysAgo = extractJsonInt(trialObj, "expiredDaysAgo");
			result.expiresAt = extractJsonString(trialObj, "expiresAt");
		}

		
		if (result.status.empty()) result.status = result.success ? "Product validation successful" : "Product validation failed";

		// Persist the offline token so this machine can validate itself with no
		// network until the token nears expiry.
		if (result.success) {
			const std::string token = extractJsonString(response, "token");
			if (!token.empty()) {
				credStore->saveCredential(kTokenCredKey, token);
				// Anchor the rollback guard at issue time, so a machine whose
				// clock is already set far in the past cannot bank extra life.
				const long now = nowUnix();
				const std::string seenStr = credStore->loadCredential(kClockCredKey);
				long seen = 0;
				if (!seenStr.empty()) { try { seen = std::stol(seenStr); } catch (...) { seen = 0; } }
				if (now > seen) credStore->saveCredential(kClockCredKey, std::to_string(now));
			}
		}
		else if (result.action != "tryagain") {
			// The server refused this licence for a non-transient reason: revoked,
			// refunded, trial over ("stop"), or this device is no longer registered
			// ("signin"). Drop the offline token rather than let a stale one keep
			// the product alive for the remainder of its 30 days.
			//
			// Phrased as "anything except tryagain" on purpose. Listing the bad
			// actions would mean any action added later silently defaults to
			// KEEPING the token, which is the fail-open direction; this way a new
			// action defaults to dropping it, and only the explicitly transient
			// case preserves it.
			credStore->deleteCredential(kTokenCredKey);
		}

		lastStatusMessage = result.status;
		return result;

	}

	bool LicenseChecker::clearAll() const {
		return credStore->clearAll();
	}

	bool LicenseChecker::isInternetConnected() const {
		return httpClient->testInternetConnection();
	}

	bool LicenseChecker::isServerReachable() const {
		return httpClient->testServerReachability();
	}

	LicenseChecker::ConnectivityStatus LicenseChecker::checkConnectivity() const {
		ConnectivityStatus status;
		status.internetAvailable = isInternetConnected();
		if (!status.internetAvailable) {
			status.serverReachable = false;
			status.statusMessage = "No internet connection available";
			return status;
		}
		status.serverReachable = isServerReachable();
		status.statusMessage = status.serverReachable ? "All connections OK" : "Internet available but license server unreachable";
		return status;
	}

	bool LicenseChecker::canConnectToLicenseServer() const {
		return isInternetConnected() && isServerReachable();
	}


} // namespace tsl