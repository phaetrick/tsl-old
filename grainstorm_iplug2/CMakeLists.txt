cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

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

if(CMAKE_CXX_STANDARD VERSION_LESS 20)
    add_compile_definitions(USE_CHAR_FOR_CHAR8_T)
endif()

set(PROJECT_NAME "Grainstorm")
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)
if(NOT DEFINED HOME_PATH)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(HOME_PATH "C:/Users/phaet")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(HOME_PATH "/Users/patrickropohl")
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
        set(HOME_PATH "/Users/patrickropohl")
    else()
        set(HOME_PATH "/home/pr")
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_definitions(-DOS_WIN)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_definitions(-DOS_MAC)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    add_definitions(-DOS_IOS)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

set(GRAINSTORM_OLD_ROOT "${CMAKE_SOURCE_DIR}/..")

if(NOT TARGET common_interface)
    add_subdirectory(${GRAINSTORM_OLD_ROOT}/common_interface
                     ${CMAKE_BINARY_DIR}/common_interface)
endif()

set(CODE_SIGN_COMPANY_NAME "The Secret Laboratory" CACHE STRING "Company name for code signing")
set(CODE_SIGN_THUMBPRINT "A1B4371191ABD7BBEECD494DA15CD310259406CB" CACHE STRING "Certificate thumbprint")

add_library(common_definitions INTERFACE)
target_compile_definitions(common_definitions INTERFACE
        IGRAPHICS_TSL
        NDEBUG
        IGRAPHICS_NO_SKIA_SKPARAGRAPH
        IGRAPHICS_DISABLE_VSYNC
        GRAINSTORM
        HAS_MIDI
        HAS_GUI
        HAS_AUDIO
        HAS_SINC
        "CODE_SIGN_COMPANY_NAME=\"${CODE_SIGN_COMPANY_NAME}\""
        DOES_INPUT_RESAMPLING
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(common_definitions INTERFACE IGRAPHICS_METAL)
else()
    target_compile_definitions(common_definitions INTERFACE IGRAPHICS_GL3)
endif()

set(IPLUG_BUILD ON)

if(NOT DEFINED BUILD_TARGET)
    set(BUILD_TARGET "app" CACHE STRING "Target to build")
endif()

message(STATUS "Building target: ${BUILD_TARGET}")

if(BUILD_TARGET STREQUAL "app")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_compile_definitions(common_definitions INTERFACE
                STANDALONE_MODE IS_MULTITHREADED)
        set(LICENSE_CHECK_ENABLED OFF)
    else()
        target_compile_definitions(common_definitions INTERFACE
                STANDALONE_MODE LICENSE_CHECK_ENABLED IS_MULTITHREADED)
        set(LICENSE_CHECK_ENABLED ON)
    endif()
    set(STANDALONE_MODE ON)
    set(IPLUG_FORMAT APP)
elseif(BUILD_TARGET STREQUAL "vst3")
    target_compile_definitions(common_definitions INTERFACE
        PLUGIN_MODE LICENSE_CHECK_ENABLED IS_MULTITHREADED)
    set(LICENSE_CHECK_ENABLED ON)
    set(PLUGIN_MODE ON)
    set(IPLUG_FORMAT VST3)
elseif(BUILD_TARGET STREQUAL "auv2")
    target_compile_definitions(common_definitions INTERFACE
        PLUGIN_MODE LICENSE_CHECK_ENABLED IS_MULTITHREADED)
    set(LICENSE_CHECK_ENABLED ON)
    set(PLUGIN_MODE ON)
    set(IPLUG_FORMAT AU)
endif()

option(ENABLE_INTEGRITY_CHECK "Enable runtime integrity checking" OFF)
option(INTEGRITY_CHECK_INSTANT "Fire integrity check after 2s (testing only)" OFF)
if(ENABLE_INTEGRITY_CHECK)
    target_compile_definitions(common_definitions INTERFACE ENABLE_INTEGRITY_CHECK)
endif()
if(INTEGRITY_CHECK_INSTANT)
    target_compile_definitions(common_definitions INTERFACE INTEGRITY_CHECK_INSTANT)
endif()

# ── WHICH LINE THIS BUILD IS ──
#
# grainstorm/ forked into grainstorm2/ and graphics/ into tslgraphics2/ so that
# the new UI could be written without GS1 moving underneath it. This file, and
# the wrapper it names, were the last thing the two lines still shared: until
# now the shell hard-wired the `grainstorm` subdirectory, the `grainstorm` link
# target and IPlugEffect.cpp, and there was no way to build the other line at
# all.
#
# OFF IS GS1, AND OFF IS THE DEFAULT. GS1 works and is shipping; nothing about
# this option may change what it builds. Every line below that the option
# touches keeps its GS1 form verbatim in the else branch -- same subdirectory,
# same binary dir, same link list, same sources -- so with GS2 off the
# configuration is the one that was here before the option existed.
#
# ON is grainstorm2: its own engine copy, its own toolkit copy and
# IPlugEffect2.cpp, which is the wrapper with the master bus in it. One name is
# linked and not four, because grainstorm2/CMakeLists.txt exports a single
# INTERFACE target over gs2_core, gs2_engine, tslgraphics2_core and gs2_gpu
# for exactly this reason -- how that line splits itself up is its business.
# gs_common_interface and tsl_common_interface are GS1 targets, defined by
# grainstorm/CMakeLists.txt, and there is nothing to point them at over here:
# grainstorm2 carries its own include paths and its own definitions through
# gs2_engine_defs.
option(GS2 "Build the GS2 line: grainstorm2 + IPlugEffect2.cpp instead of grainstorm + IPlugEffect.cpp. Not about whether there is a UI -- both lines have one; it selects WHICH engine and wrapper are compiled." OFF)

if(GS2)
    add_subdirectory(${HOME_PATH}/programming/CPP-New/grainstorm2 ${CMAKE_BINARY_DIR}/grainstorm2)
    set(GS_ENGINE_TARGET grainstorm2)
    set(GS_TOOLKIT_TARGET tslgraphics2_core)
    set(GS_WRAPPER IPlugEffect2)
    message(STATUS "GS2 is ON - building the grainstorm2 line (IPlugEffect2.cpp)")
else()
    add_subdirectory(${GRAINSTORM_OLD_ROOT}/grainstorm ${CMAKE_BINARY_DIR}/grainstorm)
    set(GS_ENGINE_TARGET grainstorm)
    set(GS_TOOLKIT_TARGET tslgraphics)
    set(GS_WRAPPER IPlugEffect)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IGRAPHICS_RENDERER "METAL" CACHE STRING "IGraphics renderer" FORCE)
else()
    set(IGRAPHICS_RENDERER "GL3" CACHE STRING "IGraphics renderer")
endif()
set(IGRAPHICS_BACKEND "TSL" CACHE STRING "IGraphics backend")
set(IPLUG2_DIR ${GRAINSTORM_OLD_ROOT}/iPlug2)
include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(PROJECT_DIR "${CMAKE_SOURCE_DIR}")

set(SRC_FILES
    "${PROJECT_DIR}/config.h"
    "${PROJECT_DIR}/${GS_WRAPPER}.h"
    "${PROJECT_DIR}/${GS_WRAPPER}.cpp"
)

# Test-only introspection hooks used by tests/auhost. OFF in every shipping
# build; when ON, the bundle exports a handful of gsTest* symbols and nothing
# else changes.
option(GS_TEST_HOOKS "Build test-only state introspection hooks (tests/auhost)" OFF)
if(GS_TEST_HOOKS)
    list(APPEND SRC_FILES "${PROJECT_DIR}/TestHooks.cpp")
    target_compile_definitions(common_definitions INTERFACE GS_TEST_HOOKS)
    message(STATUS "GS_TEST_HOOKS enabled - DO NOT SHIP THIS BUILD")
endif()
source_group(TREE ${PROJECT_DIR} FILES ${SRC_FILES})

set(RESOURCES
    "${PROJECT_DIR}/resources/fonts/Roboto-Regular.ttf"
)
# The UPGRADE VIEW's pages (iOS, GS2 line): what the Android app renders in
# its InAppPurchases activity, rendered here by tslgraphics2's storekit_ios.mm
# in a WKWebView. GS1's bundle stays as it was.
if(GS2 AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND RESOURCES
        "${PROJECT_DIR}/resources/upgrade.html"
        "${PROJECT_DIR}/resources/upgrade_au.html"
    )
endif()

add_library(_base INTERFACE)
target_include_directories(_base INTERFACE ${PROJECT_DIR} ${PROJECT_DIR}/resources)
if(GS2)
    target_link_libraries(_base INTERFACE ${GS_ENGINE_TARGET} common_definitions common_interface)
else()
    target_link_libraries(_base INTERFACE ${GS_ENGINE_TARGET} common_definitions common_interface gs_common_interface tsl_common_interface)
endif()
if(WIN32)
    target_link_libraries(_base INTERFACE Shlwapi Comctl32 Wininet)
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    target_link_libraries(_base INTERFACE "-framework StoreKit")
    if(GS2)
        # The upgrade view's WKWebView (tslgraphics2/PlatformPaths/storekit_ios.mm).
        target_link_libraries(_base INTERFACE "-framework WebKit")
    endif()
endif()
# Diagnostics only. Guarded because get_target_property is a hard error on a
# target that does not exist, and which toolkit target exists depends on the
# option above.
if(TARGET ${GS_TOOLKIT_TARGET})
    get_target_property(GS_INCLUDES ${GS_TOOLKIT_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "${GS_TOOLKIT_TARGET} includes: ${GS_INCLUDES}")
endif()

if(TARGET ${GS_ENGINE_TARGET})
    get_target_property(GS_INCLUDES2 ${GS_ENGINE_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "${GS_ENGINE_TARGET} includes: ${GS_INCLUDES2}")
endif()
iplug_add_plugin(${PROJECT_NAME}
    SOURCES ${SRC_FILES}
    RESOURCES ${RESOURCES}
    FORMATS ${IPLUG_FORMAT}
    LINK _base
)
if(APPLE AND BUILD_TARGET STREQUAL "auv2")
    if(TARGET ${ACTUAL_TARGET})
        message(STATUS "Forcing ${ACTUAL_TARGET} to compile with Mach-O Type: MH_DYLIB")
        set_target_properties(${ACTUAL_TARGET} PROPERTIES
                MACH_O_TYPE MH_DYLIB
        )
    endif()
endif()
# Determine actual CMake target name created by iplug_add_plugin
# Typical pattern is ${PROJECT_NAME}-${FORMAT_LOWER} — verify with your iPlug2 version
string(TOLOWER "${IPLUG_FORMAT}" FORMAT_LOWER)
set(ACTUAL_TARGET "${PROJECT_NAME}-${FORMAT_LOWER}")

# Fallback: if above doesn't exist, try just PROJECT_NAME
if(NOT TARGET ${ACTUAL_TARGET})
    set(ACTUAL_TARGET "${PROJECT_NAME}")
endif()

message(STATUS "Actual CMake target: ${ACTUAL_TARGET}")
if(APPLE AND BUILD_TARGET STREQUAL "auv2")
    if(TARGET ${ACTUAL_TARGET})
        message(STATUS "Overriding bundle type with shared dynamic library flags")
        target_link_options(${ACTUAL_TARGET} PRIVATE
                "-Xlinker" "-dylib"
        )
    endif()
endif()

if(WIN32)
    if(ENABLE_INTEGRITY_CHECK)
        add_subdirectory(${HOME_PATH}/programming/CPP-New/security ${CMAKE_BINARY_DIR}/security)
        target_link_libraries(${ACTUAL_TARGET} PRIVATE tslsecurity magic_hash)
    endif()
    target_link_libraries(${ACTUAL_TARGET} PRIVATE Shlwapi Comctl32 Wininet)
    message(STATUS "Linked Shlwapi/Comctl32/Wininet to ${ACTUAL_TARGET}")
    set(CMAKE_RC_FLAGS "/I${PROJECT_DIR}/resources /I${PROJECT_DIR}/resources/fonts /I${PROJECT_DIR}/resources/img ${CMAKE_RC_FLAGS}")
endif()

# ============================================================================
# Code signing — define sign_target unconditionally, no-op by default
# ============================================================================
function(sign_target TARGET_NAME)
endfunction()

if(WIN32)
    file(GLOB _signtool_paths "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    find_program(SIGNTOOL_EXECUTABLE
        NAMES signtool.exe
        PATHS ${_signtool_paths}
        NO_DEFAULT_PATH
    )
    if(SIGNTOOL_EXECUTABLE)
        message(STATUS "Found signtool: ${SIGNTOOL_EXECUTABLE}")
        # Redefine with actual implementation
        function(sign_target TARGET_NAME)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Signing ${TARGET_NAME}..."
                COMMAND "${SIGNTOOL_EXECUTABLE}" sign
                    /sha1 ${CODE_SIGN_THUMBPRINT}
                    /fd SHA256 /v
                    "$<TARGET_FILE:${TARGET_NAME}>"
                COMMENT "Code signing ${TARGET_NAME}"
                VERBATIM
            )
        endfunction()
        message(STATUS "Windows code signing enabled: ${CODE_SIGN_COMPANY_NAME}")
    else()
        message(WARNING "signtool.exe not found - signing disabled")
    endif()

elseif(APPLE)
    find_program(CODESIGN_EXECUTABLE codesign)
    if(CODESIGN_EXECUTABLE)
        set(SIGNING_IDENTITY "Apple Development: phaetrick@gmail.com (GGA39N86YW)" CACHE STRING "Code signing identity")
        set(BUNDLE_ID "com.thesecretlaboratory" CACHE STRING "Bundle identifier" FORCE)
        function(sign_target TARGET_NAME)
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                # Debug: ad-hoc sign with get-task-allow so LLDB can attach
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                        COMMAND ${CODESIGN_EXECUTABLE}
                        --force --deep
                        --sign "-"
                        --entitlements "${CMAKE_SOURCE_DIR}/debug.entitlements"
                        --identifier "${BUNDLE_ID}"
                        "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
                        COMMENT "Code signing ${TARGET_NAME} (debug)"
                        VERBATIM
                )
            else()
                # Release: hardened runtime as before
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                        COMMAND ${CODESIGN_EXECUTABLE}
                        --force --deep
                        --sign "${SIGNING_IDENTITY}"
                        --options runtime --timestamp
                        --identifier "${BUNDLE_ID}"
                        "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
                        COMMAND ${CODESIGN_EXECUTABLE} --verify --deep --strict --verbose=2
                        "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
                        COMMENT "Code signing ${TARGET_NAME} (release)"
                        VERBATIM
                )
            endif()
        endfunction()
        message(STATUS "macOS code signing enabled")
    endif()
endif()

# ============================================================================
# Post-build: integrity hash → sign → copy
# ============================================================================
if(ENABLE_INTEGRITY_CHECK)
    add_executable(PatchHash)
    target_sources(PatchHash PRIVATE
        "${HOME_PATH}/programming/CPP-New/licensecheck/computehash/PatchHash.cpp")
    if(WIN32)
        target_link_libraries(PatchHash PRIVATE crypt32 imagehlp tslsecurity common_definitions common_interface)
    else()
        target_link_libraries(PatchHash PRIVATE tslsecurity common_definitions common_interface)
    endif()

    add_custom_command(TARGET ${ACTUAL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Patching integrity hash..."
        COMMAND ${CMAKE_COMMAND} -E sleep 1
        COMMAND $<TARGET_FILE:PatchHash> "$<TARGET_FILE:${ACTUAL_TARGET}>"
        COMMAND ${CMAKE_COMMAND} -E echo "Integrity hash patched."
        COMMENT "Integrity hashing"
        VERBATIM
    )
    message(STATUS "Build order: Compile → Link → Patch Hash → Sign → Copy")
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    sign_target(${ACTUAL_TARGET})
endif()

if(GS_TEST_HOOKS)
    # A hooks build must never reach ${HOME_PATH}/out — that is where the
    # release artifacts are picked up from. It stays in the build tree, and
    # tests/auhost loads it from there by path.
    message(STATUS "GS_TEST_HOOKS: skipping the copy to ${HOME_PATH}/out")
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_custom_command(TARGET ${ACTUAL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying bundle to output directory..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${HOME_PATH}/out"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${HOME_PATH}/out/$<TARGET_BUNDLE_CONTENT_DIR:${ACTUAL_TARGET}>/.."
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:${ACTUAL_TARGET}>"
            "${HOME_PATH}/out/$<TARGET_BUNDLE_DIR_NAME:${ACTUAL_TARGET}>"
            COMMENT "Final copy to ${HOME_PATH}/out"
            VERBATIM
    )
    if(BUILD_TARGET STREQUAL "app")
        add_custom_command(TARGET ${ACTUAL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Creating DMG..."
            COMMAND hdiutil create
                -volname "${PROJECT_NAME}"
                -srcfolder "${HOME_PATH}/out/$<TARGET_BUNDLE_DIR_NAME:${ACTUAL_TARGET}>"
                -ov -format UDZO
                "${HOME_PATH}/out/${PROJECT_NAME}.dmg"
            COMMENT "Creating DMG at ${HOME_PATH}/out/${PROJECT_NAME}.dmg"
            VERBATIM
        )
    endif()
elseif(NOT APPLE)
    add_custom_command(TARGET ${ACTUAL_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying to output directory..."
            COMMAND ${CMAKE_COMMAND} -E make_directory "${HOME_PATH}/out"
            COMMAND ${CMAKE_COMMAND} -E remove -f "${HOME_PATH}/out/$<TARGET_FILE_NAME:${ACTUAL_TARGET}>"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${ACTUAL_TARGET}>" "${HOME_PATH}/out/"
            COMMENT "Final copy to ${HOME_PATH}/out"
            VERBATIM
    )
endif()