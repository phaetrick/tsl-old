//
// Created by pr on 24.03.26.
//

// patchhash.cpp — run as a post-build tool
#include <cstdio>
#include <cstring>
#include <vector>
#include <elf.h>

namespace tsl::sha256 {
// Public domain SHA256 - based on LibTomCrypt / Brad Conte's implementation
    struct SHA256State {
        uint32_t h[8];
        uint64_t len;
        uint8_t buf[64];
        uint32_t bufLen;
    };

    static constexpr uint32_t kK[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
            0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc,
            0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351,
            0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585,
            0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
            0xc67178f2
    };

#define ROTR(x, n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x, y, z)  (((x)&(y))^(~(x)&(z)))
#define MAJ(x, y, z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define EP1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define SIG0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define SIG1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

    static void sha256_transform(SHA256State &s, const uint8_t *data) {
        uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t) data[i * 4] << 24) | ((uint32_t) data[i * 4 + 1] << 16) |
                   ((uint32_t) data[i * 4 + 2] << 8) | (uint32_t) data[i * 4 + 3];
        for (int i = 16; i < 64; i++)
            w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
        a = s.h[0];
        b = s.h[1];
        c = s.h[2];
        d = s.h[3];
        e = s.h[4];
        f = s.h[5];
        g = s.h[6];
        h = s.h[7];
        for (int i = 0; i < 64; i++) {
            t1 = h + EP1(e) + CH(e, f, g) + kK[i] + w[i];
            t2 = EP0(a) + MAJ(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        s.h[0] += a;
        s.h[1] += b;
        s.h[2] += c;
        s.h[3] += d;
        s.h[4] += e;
        s.h[5] += f;
        s.h[6] += g;
        s.h[7] += h;
    }

    static void sha256_init(SHA256State &s) {
        s.h[0] = 0x6a09e667;
        s.h[1] = 0xbb67ae85;
        s.h[2] = 0x3c6ef372;
        s.h[3] = 0xa54ff53a;
        s.h[4] = 0x510e527f;
        s.h[5] = 0x9b05688c;
        s.h[6] = 0x1f83d9ab;
        s.h[7] = 0x5be0cd19;
        s.len = 0;
        s.bufLen = 0;
    }

    static void sha256_update(SHA256State &s, const uint8_t *data, size_t len) {
        while (len--) {
            s.buf[s.bufLen++] = *data++;
            s.len += 8;
            if (s.bufLen == 64) {
                sha256_transform(s, s.buf);
                s.bufLen = 0;
            }
        }
    }

    static void sha256_final(SHA256State &s, uint8_t out[32]) {
        uint32_t i = s.bufLen;
        s.buf[i++] = 0x80;
        if (i > 56) {
            while (i < 64) s.buf[i++] = 0;
            sha256_transform(s, s.buf);
            i = 0;
        }
        while (i < 56) s.buf[i++] = 0;
        for (int j = 7; j >= 0; j--) { s.buf[i++] = (s.len >> (j * 8)) & 0xff; }
        sha256_transform(s, s.buf);
        for (i = 0; i < 4; i++)
            for (int j = 0; j < 8; j++)
                out[j * 4 + i] = (s.h[j] >> ((3 - i) * 8)) & 0xff;
    }

// ── drop-in replacement for the sha256() call in ApkSigChecker ───────────
    static void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
        SHA256State s;
        sha256_init(s);
        sha256_update(s, data, len);
        sha256_final(s, out);
    }
}
// placeholder embedded in the binary — must be unique, recognizable
static constexpr uint8_t kPlaceholder[32] = {
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF,0xDE,0xAD,0xBE,0xEF
};

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "Usage: patchhash <libgrainstorm.so>\n"); return 1; }

    // read entire .so
    FILE* f = fopen(argv[1], "r+b");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);

    // parse ELF64
    auto* ehdr = (Elf64_Ehdr*)data.data();
    static constexpr uint8_t kElfMagic[4] = { 0x7f, 'E', 'L', 'F' };
    if (memcmp(ehdr->e_ident, kElfMagic, 4)) { fprintf(stderr, "Not ELF\n"); return 1; }
    auto* shdr = (Elf64_Shdr*)(data.data() + ehdr->e_shoff);
    const char* shstrtab = (const char*)(data.data() + shdr[ehdr->e_shstrndx].sh_offset);

    // find the Executable Segment instead of .text section
    size_t textOff = 0, textSize = 0;
    auto* phdr = (Elf64_Phdr*)(data.data() + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        // PT_LOAD segment with Execute (PF_X) flag
        if (phdr[i].p_type == PT_LOAD && (phdr[i].p_flags & PF_X)) {
            textOff  = phdr[i].p_offset;
            textSize = phdr[i].p_filesz;
            break;
        }
    }

    if (!textSize) { fprintf(stderr, "Executable segment not found\n"); return 1; }
// zero placeholder region before hashing if it falls inside exec segment
    for (size_t i = textOff; i <= textOff + textSize - 32; i++) {
        if (memcmp(data.data() + i, kPlaceholder, 32) == 0) {
            memset(data.data() + i, 0, 32);
            fprintf(stderr, "Zeroed placeholder at 0x%zx before hashing\n", i);
            break;
        }
    }
// Hash this segment
    uint8_t hash[32];
    tsl::sha256::sha256(data.data() + textOff, textSize, hash);

    printf("SHA256 of .text: ");
    for (int i = 0; i < 32; i++) printf("%02x", hash[i]);
    printf("\n");

    // find placeholder in .rodata and patch it
    bool found = false;
    for (size_t i = 0; i <= size - 32; i++) {
        if (memcmp(data.data() + i, kPlaceholder, 32) == 0) {
            memcpy(data.data() + i, hash, 32);
            found = true;
            printf("Patched at offset 0x%zx\n", i);
            break;
        }
    }
    if (!found) { fprintf(stderr, "Placeholder not found\n"); return 1; }

    // write back
    rewind(f);
    fwrite(data.data(), 1, size, f);
    fclose(f);
    return 0;
}
