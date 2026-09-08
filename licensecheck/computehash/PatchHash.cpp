#ifdef ENABLE_PROT_PLUGIN
#include <plugin_magic.h>
#endif
#include <SecurityTools.h>
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <wincrypt.h>
#include <map>
#include <algorithm>
// At the top of your file
#ifdef _WIN32
#include <imagehlp.h>
#pragma comment(lib, "imagehlp.lib")
#pragma comment(lib, "crypt32.lib")
#endif



typedef NTSTATUS(WINAPI* NtQueryInformationProcessPtr)(
	HANDLE ProcessHandle,
	DWORD ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength
	);

#pragma pack(push, 1)
struct SectionHeader {
	BYTE Name[8];
	DWORD VirtualSize;
	DWORD VirtualAddress;
	DWORD SizeOfRawData;
	DWORD PointerToRawData;
	DWORD PointerToRelocations;
	DWORD PointerToLinenumbers;
	WORD NumberOfRelocations;
	WORD NumberOfLinenumbers;
	DWORD Characteristics;
};
#pragma pack(pop)

#ifdef _WIN32
#include <imagehlp.h>
#pragma comment(lib, "imagehlp.lib")
#endif


#ifdef PLUGIN_MODE

#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

#endif

int main(int argc, char* argv[]) {
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: HashPatcher <target.exe|target.dll|target.vst3> [PID]" << std::endl;
		std::cerr << "  If PID is provided, attaches to running process instead of launching" << std::endl;
		return 1;
	}

	const char* targetPath = argv[1];

	
	std::cout << "=== Runtime Memory Hash Patcher ===" << std::endl;
	std::cout << "Target: " << targetPath << std::endl;
	try {
#ifdef PLUGIN_MODE
		// ===== DLL/VST3 MODE =====

// ===== DLL/VST3 MODE =====

		std::string actualPath = targetPath;
		std::string pathStr(targetPath);

		if (pathStr.find(".vst3") != std::string::npos) {
			std::string bundlePath = pathStr;

			std::vector<std::string> possiblePaths = {
				bundlePath + "\\Contents\\x86_64-win\\" +
					bundlePath.substr(bundlePath.find_last_of("\\/") + 1),
				bundlePath + "\\Contents\\x86-win\\" +
					bundlePath.substr(bundlePath.find_last_of("\\/") + 1),
			};

			bool found = false;
			for (const auto& path : possiblePaths) {
				std::ifstream test(path);
				if (test.good()) {
					actualPath = path;
					found = true;
					std::cout << "Found VST3 DLL at: " << actualPath << std::endl;
					break;
				}
			}

			if (!found) {
				std::cout << "Note: VST3 bundle detected but couldn't auto-locate DLL." << std::endl;
				std::cout << "If this fails, please specify the actual .vst3 DLL path inside the bundle." << std::endl;
			}
		}

		// CREATE A TEMPORARY COPY FIRST
		std::string tempCopyPath = actualPath + ".temp_copy.dll";
		std::cout << "Creating temporary copy: " << tempCopyPath << std::endl;

		if (!CopyFileA(actualPath.c_str(), tempCopyPath.c_str(), FALSE)) {
			std::cerr << "Failed to create temporary copy: " << GetLastError() << std::endl;
			return 1;
		}

		std::cout << "Loading temporary copy into current process..." << std::endl;

		// LOAD THE COPY, NOT THE ORIGINAL
		HMODULE hModule = LoadLibraryA(tempCopyPath.c_str());
		if (!hModule) {
			std::cerr << "Failed to load DLL copy: " << GetLastError() << std::endl;
			DeleteFileA(tempCopyPath.c_str());
			return 1;
		}

		std::cout << "DLL loaded at: 0x" << std::hex << (DWORD_PTR)hModule << std::dec << std::endl;

		MODULEINFO modInfo;
		if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
			std::cerr << "Failed to get module information: " << GetLastError() << std::endl;
			DeleteFileA(tempCopyPath.c_str());
			return 1;
		}

		DWORD_PTR imageBase = (DWORD_PTR)modInfo.lpBaseOfDll;
		std::cout << "Image base: 0x" << std::hex << imageBase << std::dec << std::endl;

#else
		// ===== EXE MODE =====

		const char* exePath = targetPath;
		PROCESS_INFORMATION pi = { 0 };

		// ===== LAUNCH NEW PROCESS =====
		STARTUPINFOA si = { sizeof(si) };

		if (!CreateProcessA(exePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			std::cerr << "Failed to create process: " << GetLastError() << std::endl;
			return 1;
		}

		std::cout << "Waiting for application window..." << std::endl;

		HWND appWindow = NULL;
		for (int attempt = 0; attempt < 100; attempt++) {
			EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
				DWORD pid;
				GetWindowThreadProcessId(hwnd, &pid);
				PROCESS_INFORMATION* pi = (PROCESS_INFORMATION*)lParam;

				if (pid == pi->dwProcessId && IsWindowVisible(hwnd)) {
					char title[256];
					GetWindowTextA(hwnd, title, sizeof(title));
					if (strlen(title) > 0) {
						pi->hThread = (HANDLE)hwnd;
						return FALSE;
					}
				}
				return TRUE;
				}, (LPARAM)&pi);

			if (pi.hThread != NULL && (HWND)pi.hThread != 0) {
				appWindow = (HWND)pi.hThread;
				break;
			}

			Sleep(100);
		}

		if (appWindow) {
			std::cout << "Window detected! Waiting for full initialization..." << std::endl;
			Sleep(1000);
		}
		else {
			std::cout << "Warning: No window found, proceeding anyway..." << std::endl;
		}

		std::cout << "Process created (PID: " << pi.dwProcessId << ")" << std::endl;

		HMODULE ntdll = GetModuleHandleA("ntdll.dll");
		auto NtQueryInformationProcess = (NtQueryInformationProcessPtr)GetProcAddress(ntdll, "NtQueryInformationProcess");

		if (!NtQueryInformationProcess) {
			std::cerr << "Failed to get NtQueryInformationProcess" << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}

		PROCESS_BASIC_INFORMATION pbi = { 0 };
		ULONG returnLength = 0;
		NTSTATUS status = NtQueryInformationProcess(pi.hProcess, 0, &pbi, sizeof(pbi), &returnLength);

		if (status != 0) {
			std::cerr << "NtQueryInformationProcess failed: 0x" << std::hex << status << std::dec << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}

#ifdef _WIN64
		const SIZE_T pebImageBaseOffset = 0x10;
#else
		const SIZE_T pebImageBaseOffset = 0x8;
#endif

		DWORD_PTR imageBase = 0;
		SIZE_T bytesRead = 0;

		if (!ReadProcessMemory(pi.hProcess, (BYTE*)pbi.PebBaseAddress + pebImageBaseOffset,
			&imageBase, sizeof(imageBase), &bytesRead)) {
			std::cerr << "Failed to read image base from PEB: " << GetLastError() << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}

		std::cout << "Image base: 0x" << std::hex << imageBase << std::dec << std::endl;
#endif

		// ===== COMMON CODE FOR BOTH MODES =====

		IMAGE_DOS_HEADER dosHeader = { 0 };

#ifdef PLUGIN_MODE
		memcpy(&dosHeader, (void*)imageBase, sizeof(dosHeader));
#else
		if (!ReadProcessMemory(pi.hProcess, (LPCVOID)imageBase, &dosHeader, sizeof(dosHeader), &bytesRead)) {
			std::cerr << "Failed to read DOS header: " << GetLastError() << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}
#endif

		if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
			std::cerr << "Invalid DOS signature" << std::endl;
#ifdef PLUGIN_MODE
			FreeLibrary(hModule);
#else
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
#endif
			return 1;
		}

		IMAGE_NT_HEADERS ntHeaders = { 0 };

#ifdef PLUGIN_MODE
		memcpy(&ntHeaders, (void*)(imageBase + dosHeader.e_lfanew), sizeof(ntHeaders));
#else
		if (!ReadProcessMemory(pi.hProcess, (LPCVOID)(imageBase + dosHeader.e_lfanew),
			&ntHeaders, sizeof(ntHeaders), &bytesRead)) {
			std::cerr << "Failed to read NT headers: " << GetLastError() << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}
#endif

		if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
			std::cerr << "Invalid NT signature" << std::endl;
#ifdef PLUGIN_MODE
			FreeLibrary(hModule);
#else
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
#endif
			return 1;
		}

		std::cout << "Found " << ntHeaders.FileHeader.NumberOfSections << " sections" << std::endl;

		std::vector<IMAGE_SECTION_HEADER> sections(ntHeaders.FileHeader.NumberOfSections);
		DWORD_PTR sectionHeaderAddr = imageBase + dosHeader.e_lfanew + sizeof(DWORD) +
			sizeof(IMAGE_FILE_HEADER) + ntHeaders.FileHeader.SizeOfOptionalHeader;

#ifdef PLUGIN_MODE
		memcpy(sections.data(), (void*)sectionHeaderAddr, sections.size() * sizeof(IMAGE_SECTION_HEADER));
#else
		if (!ReadProcessMemory(pi.hProcess, (LPCVOID)sectionHeaderAddr, sections.data(),
			sections.size() * sizeof(IMAGE_SECTION_HEADER), &bytesRead)) {
			std::cerr << "Failed to read section headers: " << GetLastError() << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}
#endif

		DWORD_PTR textAddress = 0;
		DWORD textSize = 0;

		for (const auto& section : sections) {
			char name[9] = { 0 };
			memcpy(name, section.Name, 8);
			std::cout << "Section: " << name << " at RVA 0x" << std::hex << section.VirtualAddress
				<< ", size: 0x" << section.Misc.VirtualSize << std::dec << std::endl;

			if (memcmp(section.Name, ".text", 5) == 0) {
				textAddress = imageBase + section.VirtualAddress;
				textSize = section.Misc.VirtualSize;
				std::cout << ".text section found:" << std::endl;
				std::cout << "  Address: 0x" << std::hex << textAddress << std::dec << std::endl;
				std::cout << "  Size: " << textSize << " bytes (" << (textSize / 1024) << " KB)" << std::endl;
				break;
			}
		}

		if (!textAddress) {
			std::cerr << "Could not find .text section" << std::endl;
#ifdef PLUGIN_MODE
			FreeLibrary(hModule);
#else
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
#endif
			return 1;
		}

		std::vector<unsigned char> textData(textSize);
		std::cout << "Reading " << textSize << " bytes from memory..." << std::endl;

#ifdef PLUGIN_MODE
		memcpy(textData.data(), (void*)textAddress, textSize);
		std::cout << "Successfully copied " << textSize << " bytes" << std::endl;
#else
		if (!ReadProcessMemory(pi.hProcess, (LPCVOID)textAddress, textData.data(), textSize, &bytesRead)) {
			std::cerr << "Failed to read .text section: " << GetLastError() << std::endl;
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
			return 1;
		}
		std::cout << "Successfully read " << bytesRead << " bytes" << std::endl;
#endif

		std::cout << "First 64 bytes: ";
		for (int i = 0; i < 64 && i < textSize; i++) {
			printf("%02x ", textData[i]);
			if ((i + 1) % 16 == 0) std::cout << std::endl << "                ";
		}
		std::cout << std::endl;



		// Compute SHA256 hash
		unsigned char hash[32];
		if (!tsl::security::computeSHA256FromMemory(textData.data(), textData.size(), hash)) {
			std::cerr << "Failed to compute hash" << std::endl;
#ifdef PLUGIN_MODE
			FreeLibrary(hModule);
#else
			TerminateProcess(pi.hProcess, 0);
			CloseHandle(pi.hProcess);
			if (pi.hThread) CloseHandle(pi.hThread);
#endif
			return 1;
		}

		std::cout << "Runtime hash: ";
		for (int i = 0; i < 32; i++) {
			printf("%02x", hash[i]);
		}
		std::cout << std::endl;

		// Clean up
#ifdef PLUGIN_MODE
		// DON'T call FreeLibrary - just exit, the copy will be deleted
		std::cout << "Skipping FreeLibrary (temp copy will be deleted)" << std::endl;

		// The temp copy is still loaded, but we'll patch the ORIGINAL file
		const char* patchPath = actualPath.c_str(); // ORIGINAL, not the copy

#else
		std::cout << "Terminating process before patching file..." << std::endl;
		TerminateProcess(pi.hProcess, 0);
		CloseHandle(pi.hProcess);
		if (pi.hThread) CloseHandle(pi.hThread);
		Sleep(1000);
		const char* patchPath = targetPath;
#endif

		// Patch the ORIGINAL file on disk
		std::cout << "\nPatching original file: " << patchPath << std::endl;

		std::ifstream inFile(patchPath, std::ios::binary);
		if (!inFile) {
			std::cerr << "Failed to open original file for reading" << std::endl;
#ifdef PLUGIN_MODE
			DeleteFileA(tempCopyPath.c_str());
#endif
			return 1;
		}

		inFile.seekg(0, std::ios::end);
		size_t fileSize = inFile.tellg();
		inFile.seekg(0);
		std::vector<unsigned char> fileData(fileSize);
		inFile.read((char*)fileData.data(), fileSize);
		inFile.close();


		unsigned char magicMarker[32] = {
			'T','S','L','_','I','N','T','E','G','R','I','T','Y','_','H','A',
			'S','H','_','P','L','A','C','E','H','O','L','D','E','R','!','!'
		};

		int patchCount = 0;
		for (size_t i = 0; i < fileSize - 32; i++) {
			if (memcmp(&fileData[i], magicMarker, 32) == 0) {
				memcpy(&fileData[i], hash, 32);
				patchCount++;
				std::cout << "Patched at file offset: 0x" << std::hex << i << std::dec << std::endl;
			}
		}

		if (patchCount == 0) {
			std::cerr << "ERROR: Magic marker not found in file!" << std::endl;
			return 1;
		}

#ifdef ENABLE_PROT_PLUGIN

		if (true) {
			std::cout << "=== Dll Mode ===" << std::endl;
			std::cout << "Will try to encrypt dll." << std::endl;

			// Scan for DEADBEEF pattern
			size_t magic_start = std::string::npos;
			size_t magic_stop = std::string::npos;

			for (size_t i = 0; i <= fileData.size() - 8; ++i) {
				if (std::memcmp(&fileData[i], tsl::security::PLUGIN_MAGIC_START, 8) == 0) {
					magic_start = i;
					break;
				}
			}

			for (size_t i = 0; i <= fileData.size() - 8; ++i) {
				if (std::memcmp(&fileData[i], tsl::security::PLUGIN_MAGIC_END, 8) == 0) {
					magic_stop = i;
					break;
				}
			}

			if (magic_start != std::string::npos && magic_stop != std::string::npos) {
				std::cout << "Found " << tsl::security::PLUGIN_MAGIC_START << " at offset : 0x" << std::hex << magic_start << std::dec << std::endl;
				std::cout << "Found " << tsl::security::PLUGIN_MAGIC_END << " at offset : 0x" << std::hex << magic_stop << std::dec << std::endl;

				std::cout << "Expected size to patch: " << tsl::security::plugin_compressed_size << " bytes. Patching: " << (magic_stop - (magic_start + tsl::security::PLUGIN_MAGIC_START_SIZE)) << " bytes." << std::endl;

				for (size_t j = magic_start + tsl::security::PLUGIN_MAGIC_START_SIZE, h = 0; j < magic_stop; j++, h++) {
					fileData[j] ^= hash[h % 32];
				}

				std::cout << "Successfully patched " << tsl::security::plugin_compressed_size << " bytes at offset 0x"
					<< std::hex << magic_start << std::dec << std::endl;
			}
			else {
				std::cout << "Could not find DEADBEEF pattern in binary!" << std::endl;
				return 1;
			}



		}

#endif

		std::ofstream outFile(patchPath, std::ios::binary | std::ios::trunc);
		if (!outFile) {
			std::cerr << "Failed to open original file for writing" << std::endl;
#ifdef PLUGIN_MODE
			DeleteFileA(tempCopyPath.c_str());
#endif
			return 1;
		}

		outFile.write((char*)fileData.data(), fileSize);
		outFile.close();

#ifdef PLUGIN_MODE
		// Clean up the temporary copy
		std::cout << "Deleting temporary copy..." << std::endl;
		// The copy is still loaded in memory, but we can still try to delete it
		// Windows will mark it for deletion and remove it when process exits
		if (!DeleteFileA(tempCopyPath.c_str())) {
			std::cout << "Note: Temp file will be deleted on exit" << std::endl;
		}
#endif

		std::cout << "\n=== Success! ===" << std::endl;
		std::cout << "Patched " << patchCount << " location(s) in the file." << std::endl;
		std::cout << "Your executable is now ready with integrity checking." << std::endl;

	}
	catch (const std::exception& e) {
		std::cerr << "EXCEPTION: " << e.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cerr << "UNKNOWN EXCEPTION occurred" << std::endl;
		return 1;
	}
#ifdef PLUGIN_MODE
	_exit(0); // Quick exit without calling FreeLibrary
#endif

	return 0;
}