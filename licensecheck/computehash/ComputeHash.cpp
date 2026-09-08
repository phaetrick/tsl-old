#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

#include <windows.h>
#include <wincrypt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

bool getTextSection(const char* filePath, std::vector<unsigned char>& out)
{
    HANDLE hFile = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFileA failed: " << GetLastError() << "\n";
        return false;
    }

    // Map as an image so the layout matches runtime memory
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    if (!hMapping) {
        std::cerr << "CreateFileMappingA failed: " << GetLastError() << "\n";
        CloseHandle(hFile);
        return false;
    }

    BYTE* base = (BYTE*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "Invalid DOS header\n";
        UnmapViewOfFile(base);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "Invalid NT header\n";
        UnmapViewOfFile(base);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (memcmp(section->Name, ".text", 5) == 0) {
            DWORD rva = section->VirtualAddress;
            DWORD vsize = section->Misc.VirtualSize;
            // safety: ensure vsize > 0
            if (vsize == 0) {
                std::cerr << ".text VirtualSize == 0\n";
                UnmapViewOfFile(base);
                CloseHandle(hMapping);
                CloseHandle(hFile);
                return false;
            }

            out.assign(base + rva, base + rva + vsize);

            UnmapViewOfFile(base);
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return true;
        }
    }

    std::cerr << ".text section not found\n";
    UnmapViewOfFile(base);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return false;
}

bool computeSHA256(const std::vector<unsigned char>& data, unsigned char outHash[32])
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        std::cerr << "CryptAcquireContextW failed: " << GetLastError() << "\n";
        return false;
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        std::cerr << "CryptCreateHash failed: " << GetLastError() << "\n";
        CryptReleaseContext(hProv, 0);
        return false;
    }
    if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0)) {
        std::cerr << "CryptHashData failed: " << GetLastError() << "\n";
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return false;
    }
    DWORD hashLen = 32;
    if (!CryptGetHashParam(hHash, HP_HASHVAL, outHash, &hashLen, 0)) {
        std::cerr << "CryptGetHashParam failed: " << GetLastError() << "\n";
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return false;
    }
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return true;
}

void writeHeader(const char* outPath, const unsigned char hash[32])
{
    std::ofstream out(outPath, std::ios::trunc);
    out << "#pragma once\n\n";
    out << "constexpr unsigned char EXPECTED_HASH[32] = {";
    for (int i = 0; i < 32; ++i) {
        if (i % 8 == 0) out << "\n    ";
        out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        if (i != 31) out << ", ";
    }
    out << "\n};\n\n";
    out << "constexpr bool HASH_VALID = true;\n";
    out.close();
    std::cout << "Wrote header: " << outPath << "\n";
}

#elif __APPLE__
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/loader.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

bool getCodeSection(const char* filePath, std::vector<unsigned char>& codeData) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open file" << std::endl;
        return false;
    }
    
    struct stat st;
    if (fstat(fd, &st) != 0) {
        std::cerr << "Failed to get file size" << std::endl;
        close(fd);
        return false;
    }
    
    void* pBase = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (pBase == MAP_FAILED) {
        std::cerr << "Failed to map file" << std::endl;
        close(fd);
        return false;
    }
    
    const struct mach_header_64* header = (const struct mach_header_64*)pBase;
    const struct load_command* cmd = (const struct load_command*)((char*)header + sizeof(struct mach_header_64));
    
    bool found = false;
    for (uint32_t i = 0; i < header->ncmds; i++) {
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            
            if (strcmp(seg->segname, "__TEXT") == 0) {
                std::cout << "__TEXT segment found, size: " << seg->filesize << " bytes" << std::endl;
                
                const uint8_t* dataStart = (const uint8_t*)pBase + seg->fileoff;
                codeData.assign(dataStart, dataStart + seg->filesize);
                found = true;
                break;
            }
        }
        cmd = (const struct load_command*)((char*)cmd + cmd->cmdsize);
    }
    
    munmap(pBase, st.st_size);
    close(fd);
    
    if (!found) {
        std::cerr << "__TEXT segment not found" << std::endl;
        return false;
    }
    
    return true;
}

bool computeHash(const char* filePath, unsigned char* hash) {
    std::vector<unsigned char> codeData;
    if (!getCodeSection(filePath, codeData)) {
        return false;
    }
    
    std::cout << "Hashing code section (" << codeData.size() << " bytes)..." << std::endl;
    
    CC_SHA256(codeData.data(), (CC_LONG)codeData.size(), hash);
    
    std::cout << "Hash computation successful!" << std::endl;
    return true;
}
#endif

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: GenerateExpectedHash <input.exe> <output_header.h>\n";
        return 1;
    }

    std::vector<unsigned char> code;
    if (!getTextSection(argv[1], code))
        return 1;

    unsigned char hash[32];
    if (!computeSHA256(code, hash))
        return 1;

    std::cout << "SHA-256: ";
    for (int i = 0; i < 32; i++)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    std::cout << "\n";

    writeHeader(argv[2], hash);
    return 0;
}
