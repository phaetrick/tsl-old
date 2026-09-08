# Simplified CMakeLists.txt for Visual Studio with presets - STATIC LINKING
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # Changed from "MultiThreadedDLL"

# ============================================================================
# CRITICAL: CLEAR CMAKE DEFAULT FLAGS BEFORE project()
# ============================================================================
# This MUST be done before project() to prevent CMake from setting defaults
set(CMAKE_CXX_FLAGS "" CACHE STRING "C++ compiler flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "C++ Debug flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "C++ Release flags" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "" CACHE STRING "C++ RelWithDebInfo flags" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "" CACHE STRING "C++ MinSizeRel flags" FORCE)

set(CMAKE_C_FLAGS "" CACHE STRING "C compiler flags" FORCE)
set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "C Debug flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "C Release flags" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "" CACHE STRING "C RelWithDebInfo flags" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "" CACHE STRING "C MinSizeRel flags" FORCE)

message(STATUS "Cleared all CMake default compiler flags - will use interface library only")
set(PROJECT_NAME "Common Interfaces")


# ============================================================================
# COMPILER DETECTION - Detect real Clang vs Clang-CL vs MSVC vs GCC
# ============================================================================
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")    # Check if using Clang-CL (MSVC compatibility mode) or real Clang
    if (CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(USING_CLANG_CL TRUE)
        message(STATUS "Compiler: Clang-CL (MSVC compatibility mode)")
    else()
        set(USING_REAL_CLANG TRUE)
        message(STATUS "Compiler: Real Clang (native GCC-style flags)")
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(USING_MSVC TRUE)
    message(STATUS "Compiler: Pure MSVC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(USING_GCC TRUE)
    message(STATUS "Compiler: GCC")
endif()

# ============================================================================
# SHARED INTERFACE LIBRARY FOR COMMON COMPILE OPTIONS AND DEFINITIONS
# ============================================================================
# This interface can be linked by both the main target and protplugin
add_library(common_interface INTERFACE)

add_library(no_exceptions_interface INTERFACE)

# Add compiler-specific flags to the interface with proper generator expressions
# Since we cleared CMAKE_CXX_FLAGS, these are the ONLY flags that will be used

if (USING_REAL_CLANG OR USING_GCC)
    # ========================================================================
    # REAL CLANG or GCC - Use native GCC-style flags
    # ========================================================================
    message(STATUS "Using native Clang/GCC flags (GCC-style)")

    target_compile_options(common_interface INTERFACE
            -w

            $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Debug>:-O0>

            $<$<CONFIG:Release>:-Ofast>
            $<$<CONFIG:Release>:-flto>
            $<$<CONFIG:Release>:-fvisibility=hidden>

            $<$<CONFIG:RelWithDebInfo>:-Ofast>
            $<$<CONFIG:RelWithDebInfo>:-g>
            $<$<CONFIG:RelWithDebInfo>:-flto>
            $<$<CONFIG:RelWithDebInfo>:-fvisibility=hidden>
        )

        # ARM architecture flags
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm$")
            target_compile_options(common_interface INTERFACE
                -mthumb
                -march=armv7-a
                -mfloat-abi=softfp
                -mfpu=neon        # baseline — NOT neon globally (see FLAC note)
            )
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            target_compile_options(common_interface INTERFACE
                -march=armv8-a
            )
        endif()

    target_link_options(common_interface INTERFACE
            $<$<CONFIG:Release>:-flto>
            $<$<AND:$<CONFIG:Release>,$<NOT:$<PLATFORM_ID:Darwin>>>:-fuse-ld=lld>
            $<$<CONFIG:RelWithDebInfo>:-flto>
            $<$<AND:$<CONFIG:RelWithDebInfo>,$<NOT:$<PLATFORM_ID:Darwin>>>:-fuse-ld=lld>
    )
    target_compile_options(no_exceptions_interface INTERFACE
        -fno-exceptions             # Disable C++ exceptions
        -fno-rtti                   # Disable RTTI for no_exceptions_interface only
    )

elseif (USING_CLANG_CL OR USING_MSVC)
    # ========================================================================
    # CLANG-CL or MSVC - Use MSVC-style flags
    # ========================================================================
    message(STATUS "Using MSVC-style flags (Clang-CL or MSVC)")

    target_compile_options(common_interface INTERFACE
        # Base flags for all configs - these replace CMake defaults completely
        /nologo         # Suppress startup banner
        /W0             # Disable all warnings
        /GR             # ENABLE RTTI (changed from /GR-)
        /EHsc           # ENABLE exceptions (added)
        /Zc:__cplusplus # Report correct __cplusplus value

        # Debug configuration - replaces /MDd /Zi /Ob0 /Od /RTC1
        $<$<CONFIG:Debug>:/MTd>          # Static runtime debug (instead of /MDd)
        $<$<CONFIG:Debug>:/Zi>           # Debug info
        $<$<CONFIG:Debug>:/Od>           # No optimization

        # Release configuration - replaces /MD /O2 /Ob2 /DNDEBUG
        $<$<CONFIG:Release>:/MT>         # Static runtime release (instead of /MD)
        $<$<CONFIG:Release>:/O2>         # Optimize for speed
        $<$<CONFIG:Release>:/Zi>         # Debug info in release too (for PDB)
        $<$<CONFIG:Release>:/GL>         # Whole program optimization

        # RelWithDebInfo - replaces /MD /O2 /Ob1 /Zi /DNDEBUG
        $<$<CONFIG:RelWithDebInfo>:/MT>  # Static runtime
        $<$<CONFIG:RelWithDebInfo>:/O2>  # Optimize
        $<$<CONFIG:RelWithDebInfo>:/Zi>  # Debug info
        $<$<CONFIG:RelWithDebInfo>:/GL>  # Whole program optimization
    )

    # Set linker flags for Release builds
    target_link_options(common_interface INTERFACE
        $<$<CONFIG:Release>:/DEBUG>      # Generate PDB even in release
        $<$<CONFIG:Release>:/LTCG>       # Link-time code generation
        $<$<CONFIG:Release>:/OPT:REF>    # Remove unreferenced functions
        $<$<CONFIG:Release>:/OPT:ICF>    # Identical COMDAT folding

        $<$<CONFIG:RelWithDebInfo>:/DEBUG>
        $<$<CONFIG:RelWithDebInfo>:/LTCG>
        $<$<CONFIG:RelWithDebInfo>:/OPT:REF>
        $<$<CONFIG:RelWithDebInfo>:/OPT:ICF>
    )
    
    target_compile_options(no_exceptions_interface INTERFACE
        /EHs-c-         # Disable C++ exceptions (replaces CMake's /EHsc)
        /GR-            # Disable RTTI for no_exceptions_interface only
    )
else()
    message(WARNING "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

    # Add RELEASEBUILD definition for Release configurations
    target_compile_definitions(common_interface INTERFACE
        $<$<CONFIG:Release>:RELEASEBUILD>
        $<$<CONFIG:RelWithDebInfo>:STAGINGBUILD>
    )

target_compile_features(common_interface INTERFACE cxx_std_20 c_std_11)