cmake_minimum_required(VERSION 3.16)
project(flac_minimal C CXX)

if(NOT DEFINED HOME_PATH)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(HOME_PATH "C:/Users/phaet")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(HOME_PATH "/Users/patrickropohl")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
        set(HOME_PATH "/Users/patrickropohl")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
            set(HOME_PATH "/Users/patrickropohl")
        else()
            set(HOME_PATH "/home/pr")
        endif()
    else()
        set(HOME_PATH "/home/pr")
    endif()
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    message("TSL Windows ${CMAKE_BUILD_TYPE} Build")
endif()

# NEW - USE THIS:
if(NOT TARGET common_interface)
    add_subdirectory(${HOME_PATH}/programming/CPP-New/common_interface
                     ${CMAKE_BINARY_DIR}/common_interface)
endif()

# ── Options ───────────────────────────────────────────────────────────────────
set(FLAC_SOURCE_DIR "${HOME_PATH}/programming/CPP-New/flac" CACHE PATH "Path to FLAC source root")
message("HOME_PATH = ${HOME_PATH}")
message("FLAC_SOURCE_DIR = ${FLAC_SOURCE_DIR}")
if(NOT FLAC_SOURCE_DIR OR NOT EXISTS "${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder.c")
    message(FATAL_ERROR "Set FLAC_SOURCE_DIR to the FLAC source root")
endif()

# ── Source list ───────────────────────────────────────────────────────────────
# Only the C library — no libFLAC++ wrappers, no flac/metaflac tools,
# no Ogg, no extra codec support.
set(FLAC_SOURCES
    # Core codec
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitmath.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitreader.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitwriter.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/cpu.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/crc.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed_intrin_sse2.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed_intrin_ssse3.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed_intrin_avx2.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed_intrin_sse42.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/float.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/format.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_sse2.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_sse41.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_avx2.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_fma.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_neon.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/md5.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/memory.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/metadata_iterators.c   # FLAC__metadata_object_*
    ${FLAC_SOURCE_DIR}/src/libFLAC/metadata_object.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_decoder.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder_framing.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/window.c
)
if(WIN32)
    list(APPEND FLAC_SOURCES
        ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder_intrin_sse2.c
        ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder_intrin_ssse3.c
        ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder_intrin_avx2.c
        ${FLAC_SOURCE_DIR}/src/libFLAC/windows_unicode_filenames.c
	${FLAC_SOURCE_DIR}/src/share/win_utf8_io/win_utf8_io.c
    )
endif()
# Drop SIMD sources that don't exist on the target (NDK/ARM64 has neon, not avx)
foreach(src IN LISTS FLAC_SOURCES)
    if(EXISTS "${src}")
        list(APPEND FLAC_SOURCES_FILTERED "${src}")
    endif()
endforeach()

# ── Target ────────────────────────────────────────────────────────────────────
add_library(FLAC STATIC ${FLAC_SOURCES_FILTERED})
target_link_libraries(FLAC PRIVATE common_interface)
if(APPLE)
    target_compile_definitions(FLAC PRIVATE HAVE_FSEEKO)
endif()
target_include_directories(FLAC PUBLIC
    ${FLAC_SOURCE_DIR}/include          # public: FLAC/stream_encoder.h etc.
    ${FLAC_SOURCE_DIR}/src/libFLAC/include  # private internals (private.h etc.)
)

target_compile_definitions(FLAC PRIVATE
    HAVE_CONFIG_H        # we're not running autoconf
    FLAC__NO_DLL             # static build, suppress __declspec(dllimport)
    FLAC__HAS_OGG=0          # no Ogg support needed

    # Tell libFLAC which SIMD paths are available.
    # Override these per-platform below.
    #FLAC__HAS_X86INTRIN=0
    #FLAC__HAS_NEONINTRIN=0
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    # AArch64 — NEON always present
    target_compile_definitions(FLAC PRIVATE
        FLAC__HAS_NEONINTRIN=1
        FLAC__HAS_X86INTRIN=0
    )
    target_compile_options(FLAC PRIVATE -march=armv8-a)

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
    # ARMv7 — NEON optional, scope it to SIMD files only
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
        target_compile_definitions(flac PRIVATE FLAC__HAS_NEONINTRIN=1 FLAC__HAS_X86INTRIN=0)
        target_compile_options(flac PRIVATE
            -march=armv7-a
            -mfloat-abi=softfp
            -mfpu=vfpv3          # baseline for all FLAC TUs
        )
        if(EXISTS "${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_neon.c")
            set_source_files_properties(
                "${FLAC_SOURCE_DIR}/src/libFLAC/lpc_intrin_neon.c"
                PROPERTIES COMPILE_OPTIONS "-march=armv7-a;-mfloat-abi=softfp;-mfpu=neon"
            )
        endif()

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(FLAC PRIVATE
        FLAC__HAS_X86INTRIN=1
        FLAC__HAS_NEONINTRIN=0
    )
    target_compile_options(FLAC PRIVATE -msse2)
    foreach(simd_flags
        "fixed_intrin_sse2.c;-msse2"
        "fixed_intrin_ssse3.c;-mssse3"
        "fixed_intrin_sse42.c;-msse4.2"
        "fixed_intrin_avx2.c;-mavx2"
        "lpc_intrin_sse2.c;-msse2"
        "lpc_intrin_sse41.c;-msse4.1"
        "lpc_intrin_avx2.c;-mavx2"
        "lpc_intrin_fma.c;-mavx2;-mfma"
    )
        list(GET simd_flags 0 filename)
        list(SUBLIST simd_flags 1 -1 flags)
        set(full_path "${FLAC_SOURCE_DIR}/src/libFLAC/${filename}")
        if(EXISTS "${full_path}")
            set_source_files_properties("${full_path}"
                PROPERTIES COMPILE_OPTIONS "${flags}")
        endif()
    endforeach()

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86$")
    # 32-bit x86 — skip SIMD
    target_compile_definitions(FLAC PRIVATE
        FLAC__HAS_X86INTRIN=0
        FLAC__HAS_NEONINTRIN=0
    )
endif()

# ── C standard ────────────────────────────────────────────────────────────────
target_compile_features(FLAC PRIVATE c_std_99)

# ── Suppress noisy warnings from third-party source ──────────────────────────
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(FLAC PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-implicit-fallthrough
    )
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h" "
/* stub config.h for libFLAC — generated by CMake */
#pragma once

#define FLAC__HAS_OGG 0
#define FLAC__NO_DLL 1

#define VERSION \"1.4.3\"
#define PACKAGE_VERSION \"1.4.3\"

/* Prevent lpc.c from defining its own lround() which conflicts with
   the NDK sysroot declaration in math.h */
#define HAVE_LROUND 1

/* Pull in PRIu64 and friends */
#define HAVE_INTTYPES_H 1
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
")

target_include_directories(FLAC PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

