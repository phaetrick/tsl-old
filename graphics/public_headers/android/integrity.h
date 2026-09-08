//
// Created by pr on 24.03.26.
//
#include "tools/sha256.h"
#include <cstdint>
#include <cstring>
#include <string>
#if defined (__ANDROID__)
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// ── placeholder — PatchHash will overwrite this with the real .text hash ──
// volatile + used attribute prevents the compiler from optimizing it away
// in integrity.cpp — no static, explicitly exported
extern "C" __attribute__((used, visibility("default")))
volatile uint8_t sTextHash[32] = {
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF
};



#include <zlib.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <link.h>
#include <android/log.h>

// Uncomment this for Staging/Debug builds, comment out for final Production
#define INTEGRITY_LOG

__attribute__((visibility("hidden")))
__attribute__((always_inline))
bool verify_integrity(const uint8_t* embedded_hash) {
#ifdef INTEGRITY_LOG
    LOGE("Starting integrity check...");
#endif

    // 1. Find APK path via linker
    struct LibInfo { std::string path; };
    LibInfo lib;
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) {
        auto* out = (LibInfo*)data;
        if (strstr(info->dlpi_name, "libgrainstorm.so") || strstr(info->dlpi_name, "base.apk")) {
            out->path = info->dlpi_name;
            return 1;
        }
        return 0;
    }, &lib);

    if (lib.path.empty()) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Could not find library path via linker");
#endif
        return true;
    }

    std::string apkPath = lib.path;
    auto exclaim = apkPath.find('!');
    if (exclaim != std::string::npos) apkPath = apkPath.substr(0, exclaim);

    bool isApk = (apkPath.size() >= 4 &&
                  apkPath.compare(apkPath.size() - 4, 4, ".apk") == 0);
    if (!isApk) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Path is not an APK (extracted natives?): %s", apkPath.c_str());
#endif
        return true;
    }

#ifdef INTEGRITY_LOG
    LOGE("Using APK path: %s", apkPath.c_str());
#endif

    // Detect ABI from loaded library path
    const char* abiTag = nullptr;
    if      (strstr(lib.path.c_str(), "arm64-v8a"))  abiTag = "arm64-v8a";
    else if (strstr(lib.path.c_str(), "x86_64"))      abiTag = "x86_64";
    else if (strstr(lib.path.c_str(), "armeabi-v7a")) abiTag = "armeabi-v7a";
    else if (strstr(lib.path.c_str(), "x86"))         abiTag = "x86";

    if (!abiTag) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Could not determine ABI from path: %s", lib.path.c_str());
#endif
        return true;
    }

    char targetBuf[64];
    snprintf(targetBuf, sizeof(targetBuf), "lib/%s/libgrainstorm.so", abiTag);
    const char* target = targetBuf;

    int fd = open(apkPath.c_str(), O_RDONLY);
    if (fd < 0) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Open APK failed: %s", strerror(errno));
#endif
        return true;
    }

    // RAII fd guard
    struct FdGuard {
        int fd;
        explicit FdGuard(int f) : fd(f) {}
        ~FdGuard() { if (fd >= 0) close(fd); }
        FdGuard(const FdGuard&) = delete;
        FdGuard& operator=(const FdGuard&) = delete;
    } fdGuard(fd);

    // 2. ZIP parsing (EOCD & Central Directory)
    struct stat st;
    if (fstat(fd, &st) < 0) return true;
    uint64_t apkSize = (uint64_t)st.st_size;
    if (apkSize < 22) return true;

    constexpr uint32_t MAX_CD_SIZE   = 32u  * 1024 * 1024; // 32MB
    constexpr uint32_t MAX_SO_SIZE   = 128u * 1024 * 1024; // 128MB

    off_t searchStart = (apkSize > 65557) ? (off_t)(apkSize - 65557) : 0;
    size_t eocdBufSize = (size_t)(apkSize - (uint64_t)searchStart);

    std::vector<uint8_t> eocdBuf;
    try { eocdBuf.resize(eocdBufSize); }
    catch (const std::bad_alloc&) { return true; }

    if (pread(fd, eocdBuf.data(), eocdBufSize, searchStart) != (ssize_t)eocdBufSize)
        return true;

    bool     eocdFound  = false;
    uint32_t eocdOffset = 0;
    for (int i = (int)eocdBufSize - 22; i >= 0; i--) {
        if (eocdBuf[i]   == 0x50 && eocdBuf[i+1] == 0x4b &&
            eocdBuf[i+2] == 0x05 && eocdBuf[i+3] == 0x06) {
            eocdOffset = (uint32_t)((uint64_t)searchStart + (uint64_t)i);
            eocdFound  = true;
            break;
        }
    }

    if (!eocdFound) {
#ifdef INTEGRITY_LOG
        LOGE("Error: EOCD not found");
#endif
        return true;
    }

    uint32_t cdSize = 0, cdOffset = 0;
    if (pread(fd, &cdSize,   4, (off_t)eocdOffset + 12) != 4 ||
        pread(fd, &cdOffset, 4, (off_t)eocdOffset + 16) != 4)
        return true;

    if (cdSize == 0 || cdSize > MAX_CD_SIZE) return true;
    if ((uint64_t)cdOffset + cdSize > (uint64_t)eocdOffset) return true;

    std::vector<uint8_t> cd;
    try { cd.resize(cdSize); }
    catch (const std::bad_alloc&) { return true; }

    if (pread(fd, cd.data(), cdSize, (off_t)cdOffset) != (ssize_t)cdSize)
        return true;

    // 3. Locate .so entry in Central Directory
    bool     soFound      = false;
    uint32_t soLocalOffset = 0;
    uint16_t method        = 0;
    uint32_t cSize         = 0;
    uint32_t uSize         = 0;
    size_t   targetLen     = strlen(target);

    uint32_t pos = 0;
    while (pos + 46 <= cdSize) {
        uint16_t nLen, eLen, kLen;
        memcpy(&nLen, &cd[pos+28], 2);
        memcpy(&eLen, &cd[pos+30], 2);
        memcpy(&kLen, &cd[pos+32], 2);

        if ((uint32_t)(pos + 46) + nLen > cdSize) break;

        if (nLen == (uint16_t)targetLen &&
            memcmp(&cd[pos+46], target, targetLen) == 0) {
            memcpy(&method,        &cd[pos+10], 2);
            memcpy(&cSize,         &cd[pos+20], 4);
            memcpy(&uSize,         &cd[pos+24], 4);
            memcpy(&soLocalOffset, &cd[pos+42], 4);
            soFound = true;
            break;
        }
        uint32_t step = 46u + nLen + eLen + kLen;
        if (step == 0) break; // Malformed, avoid infinite loop
        pos += step;
    }

    if (!soFound) {
#ifdef INTEGRITY_LOG
        LOGE("Error: .so not found in ZIP central directory");
#endif
        return true;
    }

    if (uSize == 0 || uSize > MAX_SO_SIZE) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Implausible uSize: %u", uSize);
#endif
        return true;
    }
    if (cSize == 0 || cSize > MAX_SO_SIZE) {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Implausible cSize: %u", cSize);
#endif
        return true;
    }

    // 4. Parse local file header to find data offset
    if ((uint64_t)soLocalOffset + 30 > apkSize) return true;

    uint16_t lN = 0, lE = 0;
    if (pread(fd, &lN, 2, (off_t)soLocalOffset + 26) != 2 ||
        pread(fd, &lE, 2, (off_t)soLocalOffset + 28) != 2)
        return true;

    uint64_t dataOff = (uint64_t)soLocalOffset + 30 + lN + lE;
    if (dataOff + (uint64_t)(method == 0 ? uSize : cSize) > apkSize) return true;

    // 5. Extract ELF into buffer
    std::vector<uint8_t> elfBuffer;
    try { elfBuffer.resize(uSize); }
    catch (const std::bad_alloc&) { return true; }

    if (method == 0) { // STORED
#ifdef INTEGRITY_LOG
        LOGE("Library is uncompressed, reading directly...");
#endif
        if (pread(fd, elfBuffer.data(), uSize, (off_t)dataOff) != (ssize_t)uSize)
            return true;
    }
    else if (method == 8) { // DEFLATED
#ifdef INTEGRITY_LOG
        LOGE("Library is compressed, decompressing...");
#endif
        std::vector<uint8_t> cData;
        try { cData.resize(cSize); }
        catch (const std::bad_alloc&) { return true; }

        if (pread(fd, cData.data(), cSize, (off_t)dataOff) != (ssize_t)cSize)
            return true;

        z_stream strm   = {};
        strm.next_in    = cData.data();
        strm.avail_in   = cSize;
        strm.next_out   = elfBuffer.data();
        strm.avail_out  = uSize;

        if (inflateInit2(&strm, -15) != Z_OK) return true;
        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        if (ret != Z_STREAM_END) {
#ifdef INTEGRITY_LOG
            LOGE("Decompression failed: %d", ret);
#endif
            return true;
        }
    }
    else {
#ifdef INTEGRITY_LOG
        LOGE("Fail-Soft: Unknown compression method %u", (unsigned)method);
#endif
        return true;
    }

    // fd no longer needed; FdGuard closes it on scope exit

    // 6. ELF validation
    if (uSize < sizeof(Elf64_Ehdr)) return true;

    uint8_t*    elfBase = elfBuffer.data();
    Elf64_Ehdr* ehdr    = (Elf64_Ehdr*)elfBase;

    if (memcmp(ehdr->e_ident, "\x7f" "ELF", 4) != 0) {
#ifdef INTEGRITY_LOG
        LOGE("Not a valid ELF header");
#endif
        return true;
    }

    // 7. Try section headers first (.text section), fall back to program headers
    uint8_t* hashPtr  = nullptr;
    uint32_t hashSize = 0;

    bool shdrsValid =
            ehdr->e_shnum       >  0                                   &&
            ehdr->e_shstrndx    <  ehdr->e_shnum                       &&
            ehdr->e_shoff       >  0                                   &&
            (uint64_t)ehdr->e_shoff + (uint64_t)ehdr->e_shnum * sizeof(Elf64_Shdr) <= uSize;

    if (shdrsValid) {
#ifdef INTEGRITY_LOG
        LOGE("Using section headers to locate .text");
#endif
        Elf64_Shdr* shdrs = (Elf64_Shdr*)(elfBase + ehdr->e_shoff);

        Elf64_Shdr& strShdr = shdrs[ehdr->e_shstrndx];
        bool strTabValid =
                strShdr.sh_offset > 0                                  &&
                strShdr.sh_size   > 0                                  &&
                strShdr.sh_offset + strShdr.sh_size <= uSize;

        if (strTabValid) {
            const char* strTab     = (const char*)(elfBase + strShdr.sh_offset);
            size_t      strTabSize = strShdr.sh_size;

            for (int i = 0; i < ehdr->e_shnum; i++) {
                uint32_t nameOff = shdrs[i].sh_name;
                if (nameOff >= strTabSize) continue;

                // Ensure name is null-terminated within bounds
                size_t remaining = strTabSize - nameOff;
                if (strnlen(strTab + nameOff, remaining) >= remaining) continue;

                if (strcmp(strTab + nameOff, ".text") != 0) continue;

                uint64_t secEnd = shdrs[i].sh_offset + shdrs[i].sh_size;
                if (shdrs[i].sh_size == 0 || secEnd > uSize) continue;

                hashPtr  = elfBase + shdrs[i].sh_offset;
                hashSize = (uint32_t)shdrs[i].sh_size;
                break;
            }
        }
    }
    uint8_t computed[32];

    if (!hashPtr) {
#ifdef INTEGRITY_LOG
        LOGE("Section headers unavailable or .text not found, falling back to PT_LOAD|PF_X");
#endif
        if (ehdr->e_phnum == 0 || ehdr->e_phoff == 0) {
#ifdef INTEGRITY_LOG
            LOGE("Fail-Soft: No program headers");
#endif
            return true;
        }

        uint64_t phEnd = (uint64_t)ehdr->e_phoff +
                         (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr);
        if (phEnd > uSize) return true;

        Elf64_Phdr* phdrs  = (Elf64_Phdr*)(elfBase + ehdr->e_phoff);
        Elf64_Phdr* execSeg = nullptr;

        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type != PT_LOAD)  continue;
            if (!(phdrs[i].p_flags & PF_X))  continue;
            if (phdrs[i].p_filesz == 0)      continue;
            if (execSeg != nullptr) {
#ifdef INTEGRITY_LOG
                LOGE("Fail-Soft: Multiple executable PT_LOAD segments");
#endif
                return true;
            }
            execSeg = &phdrs[i];
        }

        if (!execSeg) {
#ifdef INTEGRITY_LOG
            LOGE("Fail-Soft: No executable PT_LOAD segment found");
#endif
            return true;
        }

        uint64_t segEnd = execSeg->p_offset + execSeg->p_filesz;
        if (segEnd > uSize) return true;

        tsl::sha256::sha256(elfBase + execSeg->p_offset,
                            (size_t)execSeg->p_filesz, computed);
#ifdef INTEGRITY_LOG
        LOGE("SHA256 source: PT_LOAD|PF_X");
#endif
    }
    else {
        tsl::sha256::sha256(hashPtr, hashSize, computed);
#ifdef INTEGRITY_LOG
        LOGE("SHA256 source: .text section");
#endif
    }

#ifdef INTEGRITY_LOG
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + (i*2), "%02x", computed[i]);
    LOGE("Computed SHA256: %s", hex);
#endif

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= computed[i] ^ embedded_hash[i];
    return diff == 0;
}
#endif