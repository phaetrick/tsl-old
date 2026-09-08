//StringObfuscator.h
#pragma once

#include <string>
#include <cstddef>
#include <cstring>


namespace tsl {
    namespace obf {
        // secure memset helper that resists compiler optimization
      // Portable secure memset helper (no Windows.h dependency)
        inline void secureMemset(void* ptr, int value, size_t len) {
            if (!ptr || len == 0) return;

#if defined(__STDC_LIB_EXT1__)
            // Use C11's safe memset if available
            memset_s(ptr, len, value, len);
#else
            // Volatile pointer trick to avoid optimization
            volatile unsigned char* p = reinterpret_cast<volatile unsigned char*>(ptr);
            while (len--) {
                *p++ = static_cast<unsigned char>(value);
            }
#endif
        }



        constexpr unsigned char getKey(std::size_t index) {
            return static_cast<unsigned char>((index * 131 + OBFUSCATION_SEED) % 256);
        }

        constexpr char encryptChar(char c, std::size_t index) {
            return static_cast<char>(c ^ getKey(index));
        }

        inline char decryptChar(char c, std::size_t index) {
            return static_cast<char>(c ^ getKey(index));
        }

        template <std::size_t N>
        class ObfuscatedString {
        private:
            char data[N];               // encrypted bytes
            mutable char decrypted[N];  // decrypted buffer for c_str()

        public:
            constexpr ObfuscatedString(const char(&str)[N]) : data{}, decrypted{} {
                static_assert(N > 0, "String must not be empty");
                for (std::size_t i = 0; i < N; ++i) {
                    data[i] = encryptChar(str[i], i);
                    decrypted[i] = '\0';
                }
            }

            // returns a fresh std::string with plaintext
            std::string str() const {
                std::string result;
                if (N > 0) result.reserve(N - 1);
                for (std::size_t i = 0; i < N - 1; ++i) {
                    result.push_back(decryptChar(data[i], i));
                }
                return result;
            }

            // returns const char* valid while this object remains alive; fills mutable buffer
            const char* c_str() const {
                for (std::size_t i = 0; i < N; ++i) {
                    decrypted[i] = decryptChar(data[i], i);
                }
                decrypted[N - 1] = '\0';
                return decrypted;
            }

            // securely wipe cached decrypted buffer
            void wipe_decrypted() const {
                secureMemset((void*)decrypted, 0, sizeof(decrypted));
            }

            operator std::string() const { return str(); }
        };

        template <std::size_t N>
        constexpr ObfuscatedString<N> makeObfuscated(const char(&str)[N]) {
            return ObfuscatedString<N>(str);
        }

        // RAII for decrypt + auto wipe when leaving scope
        template <std::size_t N>
        class ScopedDecrypted {
            const ObfuscatedString<N>& ob;
            const char* ptr;
        public:
            explicit ScopedDecrypted(const ObfuscatedString<N>& s) : ob(s), ptr(ob.c_str()) {}
            ~ScopedDecrypted() { ob.wipe_decrypted(); }
            operator const char* () const { return ptr; }
            const char* get() const { return ptr; }
        };

    } // namespace obf
} // namespace tsl

#define OBF_OBJ(s) (tsl::obf::makeObfuscated(s))
#define OBF_STR(s) (tsl::obf::makeObfuscated(s).str())
#define OBF_USE(str) tsl::obf::ScopedDecrypted<sizeof(str)>(OBF_OBJ(str)).get()

