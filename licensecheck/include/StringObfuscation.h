#pragma once
#include <string>
#include <array>
#include <cstddef>

namespace tsl {
    namespace obf {

        // Compile-time random seed based on __TIME__
        constexpr unsigned int seed() {
            return (__TIME__[7] - '0') * 1u +
                (__TIME__[6] - '0') * 10u +
                (__TIME__[4] - '0') * 60u +
                (__TIME__[3] - '0') * 600u +
                (__TIME__[1] - '0') * 3600u +
                (__TIME__[0] - '0') * 36000u;
        }

        // Linear congruential generator for compile-time random numbers
        constexpr unsigned int lcg(unsigned int x) {
            return (x * 48271u) % 2147483647u;
        }

        // Generate compile-time random key with unique value per index
        template<size_t N>
        struct KeyGenerator {
            static constexpr unsigned char value = static_cast<unsigned char>(lcg(seed() + N) % 256);
        };

        // Helper to decrypt a single character at runtime using the compile-time key
        template<size_t Index>
        constexpr char decryptChar(char c) {
            return static_cast<char>(c ^ KeyGenerator<Index>::value);
        }

        // Recursive template to decrypt string at runtime
        template<size_t Index, size_t N>
        struct StringDecryptor {
            static void decrypt(const char* encrypted, char* output) {
                output[Index] = decryptChar<Index>(encrypted[Index]);
                StringDecryptor<Index + 1, N>::decrypt(encrypted, output);
            }
        };

        // Specialization to end recursion
        template<size_t N>
        struct StringDecryptor<N, N> {
            static void decrypt(const char*, char*) {}
        };

        // XOR encryption at compile time
        template<size_t... Index>
        constexpr auto xorString(const char* str, std::index_sequence<Index...>) {
            return std::array<char, sizeof...(Index)>{{
                    static_cast<char>(str[Index] ^ KeyGenerator<Index>::value)...
                }};
        }

        // String obfuscation wrapper
        template<size_t N>
        class ObfuscatedString {
        private:
            std::array<char, N> data;

        public:
            constexpr ObfuscatedString(const char(&str)[N])
                : data(xorString(str, std::make_index_sequence<N>{})) {
            }

            std::string decrypt() const {
                char buffer[N];
                StringDecryptor<0, N - 1>::decrypt(data.data(), buffer);
                buffer[N - 1] = '\0';
                return std::string(buffer);
            }

            operator std::string() const {
                return decrypt();
            }
        };

        // Helper macro for easy string obfuscation
#define OBF_STR(str) (::tsl::obf::ObfuscatedString<sizeof(str)>(str).decrypt())

// Stack-based string that clears itself on destruction (for sensitive data)
        class SecureString {
        private:
            char* buffer;
            size_t length;

            void clear() {
                if (buffer) {
                    // Overwrite with zeros multiple times
                    for (int pass = 0; pass < 3; ++pass) {
                        for (size_t i = 0; i < length; ++i) {
                            buffer[i] = 0;
                        }
                    }
                    delete[] buffer;
                    buffer = nullptr;
                }
            }

        public:
            SecureString(const std::string& str) {
                length = str.length();
                buffer = new char[length + 1];
                memcpy(buffer, str.c_str(), length);
                buffer[length] = '\0';
            }

            ~SecureString() {
                clear();
            }

            // Non-copyable
            SecureString(const SecureString&) = delete;
            SecureString& operator=(const SecureString&) = delete;

            const char* c_str() const { return buffer; }
            std::string str() const { return std::string(buffer, length); }
            operator std::string() const { return str(); }
        };

    } // namespace obf
} // namespace tsl