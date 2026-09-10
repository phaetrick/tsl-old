cmake_minimum_required(VERSION 3.15)

# THE PREBUILT EXTERNALS -- Skia and friends. They live OUTSIDE any checkout, in
# programming/sources/libraries, so every path to an archive or a Skia header
# goes through this one variable instead of through the source tree or a
# hand-rolled iPlug2 Dependencies path. Set here, above the platform split,
# because this file is the top-level list file for the Android build: one
# -DTSL_LIBS_ROOT=... reaches graphics too, which keeps the same default for
# standalone configures. The HOST decides, not the target -- an Android build
# reads these archives off the machine doing the building.
if(NOT DEFINED TSL_LIBS_ROOT)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(TSL_LIBS_ROOT "C:/Users/phaet/programming/sources/libraries")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(TSL_LIBS_ROOT "/Users/patrickropohl/programming/sources/libraries")
    else()
        set(TSL_LIBS_ROOT "/home/pr/programming/sources/libraries")
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")

message(${ANDROID_ABI})
set(can_use_assembler TRUE)
enable_language(ASM)
add_definitions(-DPOCKETANALOG -DANDROID -DOS_ANDROID -DOC_ARM_ASM -DAUDIO_NO_THREADS -DNDEBUG -DHAS_SINC -DIS_SINGLETON -DIGRAPHICS_GL3)

if(RELEASEBUILD)
    add_definitions(-DRELEASEBUILD)
endif()

option(FILE_SUPPORT "File Supprt" ON)
option(HAS_GUI "Has GUI" ON)
option(HAS_MIDI "Has MIDI" ON)
option(HAS_AUDIO "Has Audio" ON)

add_definitions(-DHAS_MIDI -DHAS_GUI -DHAS_FILE -DHAS_AUDIO)


function(set_spec_compileflags files)
    set(FLAGS "-Ofast")
    foreach (file ${${files}})
        set_source_files_properties(
                ${file} PROPERTIES COMPILE_FLAGS ${FLAGS}
        )
    endforeach (file)
endfunction()


if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    message("debug mode")
endif (CMAKE_BUILD_TYPE EQUAL "DEBUG")

if ("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=neon")
endif ()
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -std=c11 -Ofast -fno-strict-aliasing -Wno-format -Wno-writable-strings -Wno-comment -Wno-switch-bool -Wno-unused-variable -Wno-unused-function -Wno-missing-declarations")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -std=c++20 -Ofast -ffast-math -Wno-format -Wno-writable-strings -Wno-comment -Wno-switch-bool -Wno-unused-variable -Wno-unused-function -Wno-missing-declarations -fomit-frame-pointer -ftree-vectorize")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_DEBUG} -fvisibility=hidden -DRELEASEBUILD") #-fvisibility=hidden"
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -fvisibility=hidden -DRELEASEBUILD")

add_library(common_definitions INTERFACE)

set(GFX_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../graphics)
add_subdirectory(${GFX_LOCATION} GRAPHICS)

file(GLOB SOURCE_FILES
        sequencer.cpp
        dplimit1.cpp
        #iconrender.cpp
        pianoview.cpp
        moogladder.cpp
        logger.cpp
        #   window.cpp
        reverbprogenitor.cpp
        Allpass.cpp
        callbacks_loop_controls.cpp
        checkbox.cpp
        random.cpp
        vco.cpp
        icon_voltaic.cpp
        Queue.cpp
        MidiReceiver.cpp
        #  android-main.cpp
        slider.cpp
        types.cpp
        selector.cpp
        grainstorm.cpp
        # plusminuscontrol.cpp
        gui.cpp
        modrange.cpp
        infopanel.cpp
        synth.cpp
        setup.cpp
        meter.cpp
        #       file.cpp
        button.cpp
        ffttools.cpp
        buttonview.cpp
        preset.cpp
        presetbrowser.cpp
        presetwindow.cpp
        EventHandler.cpp)

add_library(nativelib SHARED ${SOURCE_FILES})
target_link_libraries(nativelib tslgraphics)

set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

include_directories(
        ${GFX_LOCATION}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../grainstorm
        ${ANDROID_NDK}/sources/android/cpufeatures
        ${ANDROID_NDK}/sources/android/native_app_glue
)
target_include_directories(nativelib INTERFACE
        ${GFX_LOCATION}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

else() # Desktop (macOS, Windows, iOS)

message("Hello PocketAnalog (va) ${CMAKE_SYSTEM_NAME} ${CMAKE_BUILD_TYPE}")
add_definitions(-DAUDIO_NO_THREADS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# THE CHECKOUT'S OWN ROOT, derived from this file rather than from anyone's home
# directory. Every path below that points INSIDE CPP-New goes through it, so the
# tree builds wherever it is cloned -- a CI runner, a cloud session, a second
# working copy -- and not only under <home>/programming/CPP-New. HOME_PATH stays
# for the things that live OUTSIDE this repo (the iPlug2 fork, its Skia
# checkout), which really are a property of the machine.
get_filename_component(CPPNEW_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

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


set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/Allpass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MidiReceiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/button.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/buttonview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/callbacks_loop_controls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/checkbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dplimit1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ffttools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grainstorm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/icon_voltaic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infopanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/meter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modrange.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moogladder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pianoview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/preset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presetbrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/random.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reverbprogenitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/slider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/setup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EventHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presetwindow.cpp
${CMAKE_CURRENT_SOURCE_DIR}/synth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vco.cpp
)

add_library(va STATIC ${SOURCE_FILES})

target_include_directories(va PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CPPNEW_ROOT}/graphics
)

target_link_libraries(va
    PUBLIC tslgraphics common_definitions common_interface tsl_common_interface
)

target_compile_features(va PUBLIC cxx_std_20)

endif()
