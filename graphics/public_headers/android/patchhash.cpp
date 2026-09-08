//
// Created by pr on 24.03.26.
//
// patchhash.cpp — run as a post-build tool
#include <cstdio>
#include <cstring>
#include <vector>
#include <elf.h>
#include <tools/sha256.h> // your standalone sha256

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
    if (memcmp(ehdr->e_ident, "\x7fELF", 4)) { fprintf(stderr, "Not ELF\n"); return 1; }

    auto* shdr = (Elf64_Shdr*)(data.data() + ehdr->e_shoff);
    const char* shstrtab = (const char*)(data.data() + shdr[ehdr->e_shstrndx].sh_offset);

    // find .text
    size_t textOff = 0, textSize = 0;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (strcmp(shstrtab + shdr[i].sh_name, ".text") == 0) {
            textOff  = shdr[i].sh_offset;
            textSize = shdr[i].sh_size;
            break;
        }
    }
    if (!textSize) { fprintf(stderr, ".text not found\n"); return 1; }

    // hash .text
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
