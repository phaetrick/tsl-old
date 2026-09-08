//
// Created by pr on 24.03.26.
//
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#ifdef __ANDROID__
namespace tsl::android{
    std::string getApkPath();
}
#endif

namespace tsl::security{
    inline float getDegradation(const uint8_t* computed, const uint8_t* embedded) {
        uint32_t folded = 0;
        for (int i = 0; i < 32; i++) {
            // sObfKey cancels: (computed^key) ^ (embedded^key) = computed^embedded
            uint8_t diff = computed[i] ^ embedded[i];
            folded ^= ((uint32_t)diff << (i % 24));
        }
        return (folded & 0xFFFFFF) / (float)0xFFFFFF;
    }

    // returns SHA-256 of the signing certificate's raw DER bytes
    // same hash as computed by ApkSigChecker on Android
    // 32 bytes on all platforms

    // compile-time per-index key derivation
    // each byte gets a unique key: base ^ (index * prime) ^ (index >> 2) ^ salt
    static constexpr uint8_t BASE_KEY  = 0x2D;
    static constexpr uint8_t PRIME     = 0x71;
    static constexpr uint8_t SALT      = 0xA3;

    constexpr uint8_t keyFor(size_t i) {
        return static_cast<uint8_t>(
                BASE_KEY ^
                ((i * PRIME) & 0xFF) ^
                ((i >> 2) & 0xFF) ^
                SALT ^
                ((i * i * 0x1F) & 0xFF)  // quadratic term, harder to reverse
        );
    }

    template <size_t N>
    struct SecureHash {
        uint8_t scrambled[N]{};

        template <size_t... I>
        constexpr SecureHash(const uint8_t* data, std::index_sequence<I...>)
                : scrambled{ static_cast<uint8_t>(data[I] ^ keyFor(I))... } {}

        void reveal(uint8_t* out) const {
            for (size_t i = 0; i < N; ++i)
                out[i] = scrambled[i] ^ keyFor(i);
        }
    };

    template <size_t N, size_t... I>
    constexpr auto make_secure_internal(const uint8_t (&raw)[N], std::index_sequence<I...> seq) {
        return SecureHash<N>(raw, seq);
    }

    template <size_t N>
    constexpr auto make_secure(const uint8_t (&raw)[N]) {
        return make_secure_internal(raw, std::make_index_sequence<N>{});
    }

    bool getCertHash(uint8_t out[32]);

    // convenience: compare against SecureHash
    template<size_t N>
    bool verifyCertHash(const SecureHash<N>& expected) {
        static_assert(N == 32, "cert hash must be 32 bytes");
        uint8_t computed[32];
        if (!getCertHash(computed)) return false;
        uint8_t golden[32];
        expected.reveal(golden);
        bool ok = memcmp(computed, golden, 32) == 0;
        memset(computed, 0, 32);
        memset(golden,   0, 32);
        return ok;
    }
#if defined(_WIN32) || defined (__APPLE__)
    bool verifyMySignature2();
#endif
}


