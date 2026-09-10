cmake_minimum_required(VERSION 3.15)

project(tslgraphics)

# Add static linking configuration at the top
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # Static linking for Windows

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if (CMAKE_CXX_STANDARD VERSION_LESS 20)
    # If the standard is less than C++20, add a define to use char as char8_t
    message(STATUS "C++ standard is less than C++20. Defining char8_t as char.")
    add_compile_definitions(USE_CHAR_FOR_CHAR8_T)
else ()
    message(STATUS "C++ standard is C++20 or higher. Using native char8_t.")
endif ()

# THE CHECKOUT'S OWN ROOT, derived from this file rather than from anyone's home
# directory. Every path below that points INSIDE CPP-New goes through it, so the
# tree builds wherever it is cloned -- a CI runner, a cloud session, a second
# working copy -- and not only under <home>/programming/CPP-New. HOME_PATH stays
# for the things that live OUTSIDE this repo (the iPlug2 fork, its Skia
# checkout), which really are a property of the machine.
get_filename_component(CPPNEW_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED HOME_PATH)
    if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
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

# THE PREBUILT EXTERNALS -- Skia and friends. They live OUTSIDE any checkout, in
# programming/sources/libraries, so every path to an archive or a Skia header
# goes through this one variable rather than through CPPNEW_ROOT (which is the
# source tree) or through a hand-rolled iPlug2 Dependencies path. Override with
# -DTSL_LIBS_ROOT=... to build against a different set.
if(NOT DEFINED TSL_LIBS_ROOT)
    set(TSL_LIBS_ROOT "${HOME_PATH}/programming/sources/libraries")
endif()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    message("TSL Windows ${CMAKE_BUILD_TYPE} Build")
    add_definitions(-DFLAC__NO_DLL)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    add_definitions(-DOS_IOS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    add_definitions(-DOS_ANDROID)
endif()

# NEW - USE THIS:
if(NOT TARGET common_interface)
    add_subdirectory(${CPPNEW_ROOT}/common_interface
                     ${CMAKE_BINARY_DIR}/common_interface)
endif()

set(TSL_INTERFACE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/common_interface)
set (TSL_INTERFACE_HEADERS
    ${TSL_INTERFACE_DIR}/include/types_grainstorm.h
    ${TSL_INTERFACE_DIR}/include/types.h
    ${TSL_INTERFACE_DIR}/include/defines.h
    ${TSL_INTERFACE_DIR}/include/IPlugParamDefs.h
    ${TSL_INTERFACE_DIR}/include/InputEvent.h
    ${TSL_INTERFACE_DIR}/include/params.h
    ${TSL_INTERFACE_DIR}/include/CommonConstants.h
)
add_library(tsl_common_interface ${TSL_INTERFACE_HEADERS})
target_link_libraries(tsl_common_interface common_definitions common_interface)
target_sources(tsl_common_interface  PRIVATE ${TSL_INTERFACE_DIR}/src/params.cpp ${TSL_INTERFACE_DIR}/src/CommonConstants.cpp)
target_include_directories(tsl_common_interface PUBLIC ${TSL_INTERFACE_DIR}/include)

set(ASM_PATH ${CMAKE_CURRENT_SOURCE_DIR}/asm)
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(FONTS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/fonts)

set(PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/public_headers)

set(PUBLIC_INCLUDE_DIRS ${PUBLIC_INCLUDES})


set(SOURCE_FILES

        ${SRC_PATH}/player.cpp
        ${SRC_PATH}/app.cpp
        ${SRC_PATH}/platform_config.cpp
        ${SRC_PATH}/PocketFFT.cpp
        ${SRC_PATH}/pffft.c
        ${SRC_PATH}/RealFft.cpp
        ${SRC_PATH}/tools/aligned_memalloc.cpp
        ${SRC_PATH}/tools.cpp
        ${SRC_PATH}/tools/WaitNotify.cpp
        ${SRC_PATH}/tools/AudioSemaphore.cpp
        ${SRC_PATH}/tools/threadtsl.cpp
        ${SRC_PATH}/tools/SwapRingPool.cpp
        ${SRC_PATH}/security/signature.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/PlatformPaths.cpp
)
set_source_files_properties(
        ${SRC_PATH}/tools.cpp
        PROPERTIES COMPILE_FLAGS "${CXX20_FLAG}"
)
set(HEADER_FILES
        ${PUBLIC_INCLUDES}/tools/LockFreeList.h
        ${PUBLIC_INCLUDES}/tools/SwapRingPool.h
        ${PUBLIC_INCLUDES}/security/signature.h
        ${PUBLIC_INCLUDES}/platform_config.h
        ${PUBLIC_INCLUDES}/app.h
        ${PUBLIC_INCLUDES}/tools/CircularBuffer.h
        ${PUBLIC_INCLUDES}/PocketFFT.h
        ${PUBLIC_INCLUDES}/pffft.h
        ${PUBLIC_INCLUDES}/RealFft.h
        ${PUBLIC_INCLUDES}/tools/queuetsl.h
        ${PUBLIC_INCLUDES}/tools/threadtsl.h
        ${PUBLIC_INCLUDES}/tools.h
        ${PUBLIC_INCLUDES}/logger.h
        # ${PUBLIC_INCLUDES}/tools/mutextsl.h
        ${PUBLIC_INCLUDES}/tools/AtomicSharedPtr.h
        ${PUBLIC_INCLUDES}/tools/WaitNotify.h
        ${PUBLIC_INCLUDES}/tools/LockFreeQueue.h
        ${PUBLIC_INCLUDES}/tools/RingPool.h
        ${PUBLIC_INCLUDES}/tools/RingBufferQueue.h
        ${PUBLIC_INCLUDES}/tools/LockFreeQueue.h
        ${PUBLIC_INCLUDES}/tools/inplace_function.h
        ${PUBLIC_INCLUDES}/tools/FunctionQueueWorker.h
        ${PUBLIC_INCLUDES}/tools/MemPool.h
        ${PUBLIC_INCLUDES}/tools/aligned_memalloc.h
        ${PUBLIC_INCLUDES}/tools/StackVector.h
        ${PUBLIC_INCLUDES}/tools/AudioSemaphore.h
        ${PUBLIC_INCLUDES}/tools/sha256.h
        ${PUBLIC_INCLUDES}/tools/PlatformPaths.h
)

if (IPLUG_BUILD)
    set(SOURCE_FILES ${SOURCE_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/common_interface/src/IPlugParamDefs.cpp
    )
endif ()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    set(SOURCE_FILES ${SOURCE_FILES}
            "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/applelog.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/PlatformPaths.mm"
    )
    if (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
        set(SOURCE_FILES ${SOURCE_FILES}
                "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/storekit_ios.mm"
                "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/ios_filepicker.mm"
                "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/IOSUtils.mm"
        )
    endif ()
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(SOURCE_FILES ${SOURCE_FILES}
            "${CMAKE_CURRENT_SOURCE_DIR}/PlatformPaths/PlatformPaths_win.cpp")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
endif()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        message("TSL AMD64")
        set(SOURCE_FILES ${SOURCE_FILES}
                ${SRC_PATH}/ComplexMultiplyAMD64.cpp
        )
        add_definitions(-DAMD64)
    endif ()
endif ()


set(CUSTOM_LIBS tsl_common_interface)

if (HAS_MIDI)
    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/MidiSaver.cpp
            ${SRC_PATH}/MidiLearning.cpp
            ${SRC_PATH}/Midi.cpp
    )
    set(HEADER_FILES ${HEADER_FILES}
            ${PUBLIC_INCLUDES}/MidiSaver.h
            ${PUBLIC_INCLUDES}/MidiLearning.h
            ${PUBLIC_INCLUDES}/Midi.h
    )
endif ()

if (IS_GRAINSTORM)
    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/Follower.cpp
            ${SRC_PATH}/settings.cpp
    )
    set(HEADER_FILES ${HEADER_FILES}
            ${PUBLIC_INCLUDES}/Follower.h
            ${PUBLIC_INCLUDES}/settings.h
    )
endif ()

if (LICENSE_CHECK_ENABLED)
    add_subdirectory(${CPPNEW_ROOT}/licensecheck LCHECK)
    set(CUSTOM_LIBS ${CUSTOM_LIBS} licensecheck)
    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/lc.cpp
    )
    set(HEADER_FILES ${HEADER_FILES}
            ${PUBLIC_INCLUDES}/lc.h)

endif ()


if (HAS_GUI)
    message("Has GUI")
    set(PUBLIC_INCLUDE_DIRS ${PUBLIC_INCLUDE_DIRS} ${FONTS_PATH})
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        add_library(skia STATIC IMPORTED)
        add_library(freetype STATIC IMPORTED)
        set(SKIA_PATH ${TSL_LIBS_ROOT}/skia/android)
        set_target_properties(freetype PROPERTIES IMPORTED_LOCATION ${SKIA_PATH}/out/${ANDROID_ABI}/libfreetype2.a)
        set_target_properties(skia PROPERTIES IMPORTED_LOCATION ${SKIA_PATH}/out/${ANDROID_ABI}/libskia.a)
        set(CUSTOM_LIBS ${CUSTOM_LIBS} GLESv3 EGL skia)
        set(SKIA_INCLUDES_PATH ${SKIA_PATH}/includes)
        set(SOURCE_FILES ${SRC_PATH}/window_anim.cpp ${SOURCE_FILES})
        set(HEADER_FILES ${PUBLIC_INCLUDES}/window_anim.h ${HEADER_FILES})
        set(SKIA_INCLUDES
                ${SKIA_INCLUDES_PATH}
                ${SKIA_INCLUDES_PATH}/include/core
                ${SKIA_INCLUDES_PATH}/include/gpu
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh/gl
                ${SKIA_INCLUDES_PATH}/include/android
                ${SKIA_INCLUDES_PATH}/include/config
                ${SKIA_INCLUDES_PATH}/include/gpu/gl
                ${SKIA_INCLUDES_PATH}/src/gpu/gl
                ${SKIA_INCLUDES_PATH}/include/utils
                ${SKIA_INCLUDES_PATH}/include/effects
        )
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        file(GLOB SOURCE_FILES ${SOURCE_FILES} glad2.c)
        # THE IMPORTED TARGET HAS TO BE DECLARED HERE. Every other branch of this
        # block calls add_library(skia STATIC IMPORTED) itself; this one only
        # set the property, so configuring for Linux failed outright with "Can
        # not find target to add properties to: skia" -- the branch had never
        # been run since the Android branch above stopped declaring it for
        # everyone.
        add_library(skia STATIC IMPORTED)
        # sources/libraries is this machine's own Skia, built by iPlug2's
        # Dependencies/IGraphics/build-skia-linux.sh from the same feature set as
        # the libraries Windows and macOS ship, and installed here by
        # install-skia-linux.sh. See sources/libraries/README.md.
        set(SKIA_PATH ${TSL_LIBS_ROOT}/skia/linux-x64)
        # Pulled at configure when missing -- the same end-to-end recipe a hand
        # build runs (libraries/get-skia-linux.sh). First time is a real Skia
        # build, ~15 min; after that this never fires.
        if (NOT EXISTS "${SKIA_PATH}/lib/libskia.a")
            message(STATUS "tslgraphics: no Linux Skia at ${SKIA_PATH} -- pulling "
                           "(first time this BUILDS Skia, ~15 min)")
            execute_process(COMMAND "${TSL_LIBS_ROOT}/get-skia-linux.sh" x64
                            RESULT_VARIABLE _tslg_skia_pull)
            if (NOT _tslg_skia_pull EQUAL 0 OR NOT EXISTS "${SKIA_PATH}/lib/libskia.a")
                message(FATAL_ERROR "tslgraphics: Skia pull failed -- see ${TSL_LIBS_ROOT}/README.md")
            endif ()
        endif ()
        set_target_properties(skia PROPERTIES IMPORTED_LOCATION ${SKIA_PATH}/lib/libskia.a)
        # fontconfig, because that is how Skia answers "give me a face" on this
        # platform and it is the SYSTEM's -- everything else the archive needs
        # (freetype, libpng, expat) is folded into libskia.a by is_official_build.
        set(CUSTOM_LIBS ${CUSTOM_LIBS} skia GL fontconfig)
        # The headers that MATCH that archive: a header from one Skia tree and an
        # archive from another link with a signature nobody wrote. In this layout
        # every non-Android platform shares the one header tree beside them.
        set(SKIA_INCLUDES_PATH ${TSL_LIBS_ROOT}/skia/headers)
        set(SKIA_INCLUDES
                ${SKIA_INCLUDES_PATH}
                ${SKIA_INCLUDES_PATH}/include/core
                ${SKIA_INCLUDES_PATH}/include/gpu
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh/gl
                ${SKIA_INCLUDES_PATH}/include/config
                ${SKIA_INCLUDES_PATH}/include/gpu/gl
                ${SKIA_INCLUDES_PATH}/src/gpu/gl
                ${SKIA_INCLUDES_PATH}/include/utils
                ${SKIA_INCLUDES_PATH}/include/effects)
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(GLAD_INCLUDES_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${TSL_LIBS_ROOT}/glad_GL3/include)
        set(PUBLIC_INCLUDE_DIRS ${PUBLIC_INCLUDE_DIRS} ${GLAD_INCLUDES_PATH})
        add_library(skia STATIC IMPORTED)

        if (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(SKIA_INCLUDES_PATH ${TSL_LIBS_ROOT}/skia/headers)
            set_target_properties(skia PROPERTIES IMPORTED_LOCATION
                ${TSL_LIBS_ROOT}/skia/win-arm64/lib/skia.lib)
            set(CUSTOM_LIBS ${CUSTOM_LIBS} OpenGL32 skia)
        else()
            set(SKIA_INCLUDES_PATH ${TSL_LIBS_ROOT}/skia/headers)
            set_target_properties(skia PROPERTIES IMPORTED_LOCATION
                ${TSL_LIBS_ROOT}/skia/win-x64/lib/skia.lib)
            add_library(libpng STATIC IMPORTED)
            set_target_properties(libpng PROPERTIES IMPORTED_LOCATION
                ${TSL_LIBS_ROOT}/skia/win-x64/lib/libpng.lib)
            set(CUSTOM_LIBS ${CUSTOM_LIBS} OpenGL32 skia libpng)
        endif()

        set(SKIA_INCLUDES
                ${SKIA_INCLUDES_PATH}
                ${SKIA_INCLUDES_PATH}/include/core
                ${SKIA_INCLUDES_PATH}/include/gpu
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh/gl
         #       ${SKIA_INCLUDES_PATH}/include/android
                ${SKIA_INCLUDES_PATH}/include/config
                ${SKIA_INCLUDES_PATH}/include/gpu/gl
                ${SKIA_INCLUDES_PATH}/src/gpu/gl
                ${SKIA_INCLUDES_PATH}/include/utils
                ${SKIA_INCLUDES_PATH}/include/effects)
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")




        set(SKIA_INCLUDES_PATH ${TSL_LIBS_ROOT}/skia/headers)
        add_library(skia STATIC IMPORTED)
        set_target_properties(skia PROPERTIES IMPORTED_LOCATION
                ${TSL_LIBS_ROOT}/skia/mac/lib/libskia.a)
        add_library(freetype STATIC IMPORTED)
        set_target_properties(freetype PROPERTIES IMPORTED_LOCATION
                ${TSL_LIBS_ROOT}/skia/mac/lib/libfreetype.a)
        set(CUSTOM_LIBS ${CUSTOM_LIBS} skia freetype)
        set(SKIA_INCLUDES
                ${TSL_LIBS_ROOT}/skia/mac/include/freetype2
                ${SKIA_INCLUDES_PATH}
                ${SKIA_INCLUDES_PATH}/include/core
                ${SKIA_INCLUDES_PATH}/include/gpu
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh
                ${SKIA_INCLUDES_PATH}/include/gpu/ganesh/gl
                ${SKIA_INCLUDES_PATH}/include/config
                ${SKIA_INCLUDES_PATH}/include/gpu/gl
                ${SKIA_INCLUDES_PATH}/src/gpu/gl
                ${SKIA_INCLUDES_PATH}/include/utils
                ${SKIA_INCLUDES_PATH}/include/effects)
        if(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
            set(SOURCE_FILES ${SOURCE_FILES}
                    ${SRC_PATH}/skia.mm)
        endif()
    endif ()

    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/FloatingView.cpp
            ${SRC_PATH}/views.cpp
            ${SRC_PATH}/envelope.cpp
            ${SRC_PATH}/window.cpp
            ${SRC_PATH}/scrollview.cpp
            ${SRC_PATH}/PlusMinusControlBase.cpp
            ${SRC_PATH}/Toast.cpp
            ${SRC_PATH}/Input.cpp
            ${SRC_PATH}/keyboard.cpp
            ${SRC_PATH}/skia.cpp
            ${SRC_PATH}/view.cpp
            ${SRC_PATH}/EnterValue.cpp
            ${SRC_PATH}/textview.cpp
            ${SRC_PATH}/ButtonBase.cpp
            ${SRC_PATH}/knob.cpp
            ${SRC_PATH}/LogView.cpp
            ${SRC_PATH}/keydefines.cpp
            ${SRC_PATH}/SettingsView.cpp
            ${SRC_PATH}/settings2.cpp
            ${SRC_PATH}/DynamicDialog.cpp
            ${SRC_PATH}/AnimatedSpinner.cpp
            ${SRC_PATH}/BackView.cpp
            ${SRC_PATH}/RecyclerView.cpp

    )

    set(HEADER_FILES ${HEADER_FILES}
            ${PUBLIC_INCLUDES}/FloatingScrollView.h

            ${PUBLIC_INCLUDES}/FloatingView.h
            ${PUBLIC_INCLUDES}/views.h
            ${PUBLIC_INCLUDES}/envelope.h
            ${PUBLIC_INCLUDES}/window.h
            ${PUBLIC_INCLUDES}/scrollview.h
            ${PUBLIC_INCLUDES}/PlusMinusControlBase.h
            ${PUBLIC_INCLUDES}/RecyclerView.h
            ${PUBLIC_INCLUDES}/Toast.h
            ${PUBLIC_INCLUDES}/Input.h
            ${PUBLIC_INCLUDES}/VelocityTracker.h
            ${PUBLIC_INCLUDES}/keyboard.h
            ${PUBLIC_INCLUDES}/keydefines.h
            ${PUBLIC_INCLUDES}/skia.h
            ${PUBLIC_INCLUDES}/colours.h
            ${PUBLIC_INCLUDES}/view.h
            ${PUBLIC_INCLUDES}/EnterValue.h
            ${PUBLIC_INCLUDES}/textview.h
            ${PUBLIC_INCLUDES}/ButtonBase.h
            ${PUBLIC_INCLUDES}/knob.h

            ${PUBLIC_INCLUDES}/LogView.h
            ${PUBLIC_INCLUDES}/keydefines.h
            ${PUBLIC_INCLUDES}/SettingsView.h
            ${PUBLIC_INCLUDES}/settings2.h
            ${PUBLIC_INCLUDES}/DynamicDialog.h
            ${PUBLIC_INCLUDES}/AnimatedSpinner.h
            ${PUBLIC_INCLUDES}/BackView.h
    )
    if (NEW_UI)
        set(HEADER_FILES ${HEADER_FILES}

                ${PUBLIC_INCLUDES}/ui2/view2.h
                ${PUBLIC_INCLUDES}/ui2/ButtonBase2.h
                ${PUBLIC_INCLUDES}/ui2/style.h
                ${PUBLIC_INCLUDES}/ui2/knob2.h
                ${PUBLIC_INCLUDES}/ui2/GLFullScreenAnimation.h
                ${PUBLIC_INCLUDES}/ui2/RecyclerView2.h
                ${PUBLIC_INCLUDES}/ui2/RecycleList.h
        )

        set(SOURCE_FILES ${SOURCE_FILES}

                ${SRC_PATH}/ui2/view2.cpp
                ${SRC_PATH}/ui2/Buttonbase2.cpp
                ${SRC_PATH}/ui2/knob2.cpp
                ${SRC_PATH}/ui2/style.cpp
                ${SRC_PATH}/ui2/GLFullScreenAnimation.cpp
        )
    endif ()

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        set(SOURCE_FILES ${SOURCE_FILES}
                ${SRC_PATH}/InsetsHandler.cpp
        )
        set(HEADER_FILES ${HEADER_FILES}
                ${PUBLIC_INCLUDES}/InsetsHandler.h
        )
    endif ()

    list(APPEND PUBLIC_INCLUDE_DIRS ${SKIA_INCLUDES})

    if (USE_IMGUI)
        set(IMGUI_LOCATION ${CPPNEW_ROOT}/imgui)

        set(SOURCE_FILES ${SOURCE_FILES}
                ${SRC_PATH}/imguitsl.cpp
                ${IMGUI_LOCATION}/imgui.cpp
                ${IMGUI_LOCATION}/imgui_draw.cpp
                ${IMGUI_LOCATION}/imgui_tables.cpp
                ${IMGUI_LOCATION}/imgui_widgets.cpp
        )
        set(HEADER_FILES
                ${HEADER_FILES}
                ${PUBLIC_INCLUDES}/imguitsl.h
                ${IMGUI_LOCATION}/imgui.h
        )
        list(APPEND PUBLIC_INCLUDE_DIRS
                ${IMGUI_LOCATION}
        )
    endif ()

    set(CUSTOM_LIBS ${CUSTOM_LIBS})
endif ()

if (HAS_AUDIO)
    set(SOURCE_FILES ${SOURCE_FILES} ${SRC_PATH}/player.cpp ${SRC_PATH}/audio/Recording.cpp ${SRC_PATH}/audio/PoolData.cpp)
    set(HEADER_FILES ${HEADER_FILES} ${PUBLIC_INCLUDES}/player.h ${PUBLIC_INCLUDES}/resample.h ${PUBLIC_INCLUDES}/audio/Recording.h ${PUBLIC_INCLUDES}/audio/ZdfFilters.h ${PUBLIC_INCLUDES}/audio/Oversampler.h)

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        # Find the Oboe package
        find_package(oboe REQUIRED CONFIG)
        set(CUSTOM_LIBS ${CUSTOM_LIBS} oboe::oboe)
    endif ()
    # 1. Include the zstd subdirectory 
#    add_subdirectory(${CPPNEW_ROOT}/zstd ZSTD)
#    set(CUSTOM_LIBS ${CUSTOM_LIBS} zstd_static)
#    target_include_directories(zstd_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

if (FILE_SUPPORT)
    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/FileWrapper.cpp)
    set(HEADER_FILES ${HEADER_FILES}
            ${PUBLIC_INCLUDES}/FileWrapper.h)

    set(FLAC_PATH ${CPPNEW_ROOT}/flac)
set(LAME_PATH ${CPPNEW_ROOT}/lame)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

add_subdirectory(${CPPNEW_ROOT}/flac ${CMAKE_BINARY_DIR}/flac)


    add_library(mp3lame STATIC IMPORTED)
    set_target_properties(mp3lame PROPERTIES
        IMPORTED_LOCATION "${LAME_PATH}/build-windows/mp3lame.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LAME_PATH}/include"
    )

else()
    add_subdirectory(${LAME_PATH} LAME)
    list(APPEND PUBLIC_INCLUDE_DIRS
        ${LAME_PATH}/include
    )
    add_subdirectory(${CPPNEW_ROOT}/flac-mini FLAC)
    list(APPEND PUBLIC_INCLUDE_DIRS
        ${FLAC_PATH}/include
    )
endif()


set(CUSTOM_LIBS ${CUSTOM_LIBS} FLAC mp3lame)

endif ()


if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(CMAKE_CXX_STANDARD 20)
    set(can_use_assembler TRUE)
    enable_language(ASM)
    add_definitions(-DANDROID -DOC_ARM_ASM)

    set(SOURCE_FILES ${SOURCE_FILES}
            ${SRC_PATH}/android-main.cpp
    )

    list(APPEND PUBLIC_INCLUDE_DIRS
            ${ANDROID_NDK}/sources/android/cpufeatures
            ${ANDROID_NDK}/sources/android/native_app_glue
    )

    if ("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
        set(ASM_SRCS ${ASM_PATH}/complexMultiply-armv7.s)
    elseif ("${ANDROID_ABI}" STREQUAL "arm64-v8a")
        set(ASM_SRCS ${ASM_PATH}/resample-arm64.S ${ASM_PATH}/complexMultiply-arm64.S)
    else ()
        set(SOURCE_FILES ${SOURCE_FILES}
                ${SRC_PATH}/ComplexMultiply.cpp)
    endif ()

    add_library(app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    add_library(cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
    find_library(log-lib log)
    find_library(z-lib z)
    set(CUSTOM_LIBS ${CUSTOM_LIBS} app-glue cpufeatures log m android z)
    message("tslgraphics C++ compiler flags: ${CMAKE_CXX_FLAGS}")

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(CUSTOM_LIBS ${CUSTOM_LIBS} common_interface)
    set_source_files_properties(
            ${SRC_PATH}/app.cpp
            PROPERTIES COMPILE_FLAGS "${CXX20_FLAG}"
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
enable_language(ASM)
set(ASM_SRCS
${ASM_PATH}/resample-arm64.S
${ASM_PATH}/complexMultiply-arm64.S
)
set_source_files_properties(
${ASM_PATH}/resample-arm64.S
${ASM_PATH}/complexMultiply-arm64.S
PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp -march=armv8-a+simd")

endif ()


message(STATUS "Includes: ${PUBLIC_INCLUDE_DIRS}")

set(SOURCE_FILES ${SOURCE_FILES} ${ASM_SRCS})

add_library(tslgraphics ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(tslgraphics PUBLIC ${PUBLIC_INCLUDE_DIRS})
target_link_libraries(tslgraphics PUBLIC ${CUSTOM_LIBS} common_definitions)
if(APPLE AND NOT IOS)
    target_compile_definitions(tslgraphics PRIVATE OS_MAC)
endif()

if(APPLE)
    # RealFft dispatches large transforms to vDSP; Accelerate ships with the SDK.
    target_link_libraries(tslgraphics PUBLIC "-framework Accelerate")
endif()

target_compile_options(tslgraphics PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
)

add_library(tslgraphics_headers INTERFACE)
target_include_directories(tslgraphics_headers INTERFACE ${PUBLIC_INCLUDES})
target_compile_features(tslgraphics_headers INTERFACE cxx_std_20)

message("---------------------------------------------------------     ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
