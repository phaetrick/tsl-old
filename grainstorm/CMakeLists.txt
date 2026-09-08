cmake_minimum_required(VERSION 3.15)
message("Hello Grainstorm ${CMAKE_HOST_SYSTEM_NAME}  ${CMAKE_BUILD_TYPE}")

project(Grainstorm)
set(CMAKE_VERBOSE_MAKEFILE on)

# Add static linking configuration at the top
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # Static linking for Windows

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
    add_definitions(-DOS_WIN)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    add_definitions(-DOS_MAC)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    add_definitions(-DOS_IOS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    add_definitions(-DOS_ANDROID)
endif()


# NEW - USE THIS:
if(NOT TARGET common_interface)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common_interface
                     ${CMAKE_BINARY_DIR}/common_interface)
endif()



set(HAS_MIDI ON CACHE BOOL "" FORCE)
set(HAS_GUI ON CACHE BOOL "" FORCE)
set(HAS_AUDIO ON CACHE BOOL "" FORCE)
set(IS_GRAINSTORM ON CACHE BOOL "" FORCE)
set(FILE_SUPPORT ON CACHE BOOL "" FORCE)

add_definitions(-DNDEBUG  -DGRAINSTORM  -DHAS_MIDI -DHAS_GUI -DHAS_AUDIO -DHAS_SINC -DDOES_SOUNDEDITING)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build type: Debug")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)


if(CMAKE_CXX_STANDARD VERSION_LESS 20)
    # If the standard is less than C++20, add a define to use char as char8_t
    message(STATUS "C++ standard is less than C++20. Defining char8_t as char.")
    add_compile_definitions(USE_CHAR_FOR_CHAR8_T)
else()
    message(STATUS "C++ standard is C++20 or higher. Using native char8_t.")
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    add_definitions(-DIS_MULTITHREADED -DHAS_FILE -DHAS_MICREC -DIS_SINGLETON -DIGRAPHICS_GL3)

    message(${ANDROID_ABI})
    set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
    set(can_use_assembler TRUE)
    enable_language(ASM)

    add_definitions(-DANDROID -DOC_ARM_ASM)

    if ("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
    set(ASM_SRCS ModalReverb-armv7.s)
    elseif ("${ANDROID_ABI}" STREQUAL "arm64-v8a")
        set(ASM_SRCS ModalReverb-arm64.S)
        set_source_files_properties(ModalReverb-arm64.S PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
    endif ()

    set(OS_SPECIFIC_HEADER_FILES
            DecoderAndroid.h
    )
    set(OS_SPECIFIC_SOURCE_FILES
            DecoderAndroid.cpp
    )

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

    message("LINUX")
    add_definitions(-DLINUX -DJUCE_USE_SSE_INTRINSICS)
    set(OS_SPECIFIC_SOURCE_FILES
            dummyfunctions.cpp
    )
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"  OR ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    enable_language(OBJCXX ASM)

    set(ASM_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/ModalReverb-arm64.S)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/ModalReverb-arm64.S
            PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")




    set(OS_SPECIFIC_SOURCE_FILES
            DecoderApple.mm
            DecoderWindows.cpp

    )
    set(OS_SPECIFIC_HEADER_FILES
            DecoderWindows.h
            WindowsMediaDecoder.h
    )

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

    set(FFMPEG_DIR ${HOME_PATH}/programming/sources/libraries/ffmpeg/linux-x86_64)
    set(OS_SPECIFIC_SOURCE_FILES
            DecoderWindows.cpp
           WindowsMediaDecoder.cpp
    )
    set(OS_SPECIFIC_HEADER_FILES
            DecoderWindows.h
            WindowsMediaDecoder.h
    )
    set_source_files_properties(
        WindowsMediaDecoder.cpp
        PROPERTIES COMPILE_FLAGS "${CXX17_FLAG}"
    )
endif ()

set(GFX_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../graphics)
add_subdirectory(${GFX_LOCATION} GRAPHICS)

set(GS_INTERFACE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/common_interface)
set (GS_INTERFACE_HEADERS 
    ${GS_INTERFACE_DIR}/include/gs_common.h
)
add_library(gs_common_interface ${GS_INTERFACE_HEADERS})
target_link_libraries(gs_common_interface tsl_common_interface common_definitions)
target_sources(gs_common_interface  PRIVATE ${GS_INTERFACE_DIR}/src/gs_common.cpp)
target_include_directories(gs_common_interface PUBLIC ${GS_INTERFACE_DIR}/include)



# The GUI. Was one gui.cpp; now one translation unit per space, all sharing
# gui/gui_internal.h (private to the folder). gui/gui.h stays the public header
# - the rest of the project includes it as "gui/gui.h".
set(GUI_SOURCE_FILES
        gui/gui_setup.cpp
        gui/track_gui.cpp
        gui/space_env_follower.cpp
        gui/space_fx.cpp
        gui/space_fx_stereo.cpp
        gui/space_fx_grain.cpp
        gui/space_pv.cpp
        gui/space_cross.cpp
        gui/space_graingen.cpp
        gui/space_arp.cpp
        gui/space_windows.cpp
        gui/space_lfo.cpp
        gui/space_waveform.cpp
)

set(GUI_HEADER_FILES
        gui/gui.h
        gui/gui_internal.h
)

set(SOURCE_FILES
        ${GUI_SOURCE_FILES}
        aigen.cpp
        Presets/load.cpp
        Presets/save.cpp 
        Presets/flac.cpp
        Presets/presetdef.cpp
        Presets/common.cpp
        Presets/DeleteOverwriteList.cpp
        TrackSettings/fxOrder.cpp
        Effects/gaintask.cpp
        History/history.cpp 
        History/snapshot.cpp
        Event/ApplyFromUi.cpp
        Event/setup.cpp
        Event/toString.cpp
        Event/EventHandler.cpp
        Event/OnChangeFunctions.cpp
        Event/AfterChangeFunctions.cpp
        Event/apply.cpp
        Event/current.cpp
        Event/default.cpp
        Recording/PositionsHandler.cpp
        Recorder.cpp
        Editor.cpp
        SyncTargets.cpp
        Vox.cpp
        Vox2.cpp
        GrainWaveset.cpp
        GrainDrive.cpp
        GrainDisperse.cpp
        GrainVowel.cpp
        GrainPluck.cpp
        resonbank.cpp
        graingenerator.cpp
        filt.cpp
        SpectrumAnalyzer.cpp
        Butterworth.cpp
        TrackSettings.cpp
        DelayBase.cpp
        ModalReverb.cpp
        distortion.cpp
        dplimit1.cc
        moogladder.cpp
        sequencer.cpp
        Reverb.cpp
        Shimmer.cpp
        reverbprogenitor.cpp
        Modal.cpp
        correlation.cpp
        chorus.cpp
        Allpass.cpp
        PitchShifter.cpp
        pitchdetect.cpp
        pitchmap.cpp
        pitchmap2.cpp
        base.cpp
        envwin2.cpp
        pv.cpp
        fm.cpp
        physmod.cpp
        lfo.cpp
        envelope_window.cpp
        checkbox.cpp
        ControlItem.cpp
        random.cpp
        vco.cpp
        lpc.cpp
        icon.cpp
        MidiReceiver.cpp
        callbacks_fxpower.cpp        
        Convolver.cpp
        vocoder.cpp
        Biquad.cpp
        saturator.cpp
        oscil.cpp
        slider.cpp
        eq.cpp
        DecoderView.cpp
        delay.cpp
        selector.cpp
        grainstorm.cpp
        phaser.cpp
        filter.cpp
        plusminuscontrol.cpp
        compressor.cpp
        infopanel.cpp
        waveform.cpp
        synth.cpp
        setup.cpp
        meter.cpp
        button.cpp
        track.cpp
        ffttools.cpp
        frequencyresponce.cpp
        Noise.cpp
        Layouts.cpp
        SpectrumAnalyzerView.cpp
        granulate.cpp
        granulate_loop.cpp
        granulate_fft.cpp
        degradation.cpp
        ParameterInit.cpp
)

set(HEADER_FILES
        ${GUI_HEADER_FILES}
        aigen.h
        Presets/flac.h
        Presets/preset.h
        Presets/presetdef.h
        TrackSettings/fxOrder.h
        Effects/gaintask.h
        Recorder.h
        SpectrumAnalyzerView.h
        DynEqView.h
        DecoderView.h
        DecData.h
        Editor.h
        SyncTargets.h
        resonbank.h
        graingenerator.h
        
        filt.h
        SpectrumAnalyzer.h
        Butterworth.h
        TrackSettings.h
        DelayBase.h
        ModalReverb.h
        distortion.h
        dplimit1.cc
        moogladder.h
        
        grainqueue.h
        sequencer.h
        Reverb.h
        Shimmer.h
        reverbprogenitor.h
        Modal.h
        correlation.h
        chorus.h
        Allpass.h
        PitchShifter.h
        pitchdetect.h
        base.h
        envwin2.h
        pv.h
        fm.h
        physmod.h
        lfo.h
        envelope_window.h
        
        checkbox.h
        ControlItem.h
        random.h
        vco.h
        lpc.h
        MidiReceiver.h
        callbacks_fxpower.h
        
        Convolver.h
        vocoder.h
        Biquad.h
        saturator.h
        oscil.h
        slider.h
        eq.h
        delay.h
        selector.h
        grainstorm.h
        phaser.h
        filter.h
        plusminuscontrol.h
        compressor.h
        infopanel.h
        waveform.h
        synth.h
        setup.h
        meter.h
        button.h
        track.h
        ffttools.h
        buttonview.h
        frequencyresponce.h
        Noise.h
        Layouts.h
        granulate.h
        granulate_loop.h
        granulate_fft.h
        degradation.h
        History.h
        ParameterInit.h
)
    # AVX2: enable AVX2 ONLY for this file
if (USING_CLANG_CL)
    set_source_files_properties(ModalReverb.cpp PROPERTIES
        COMPILE_FLAGS "/arch:AVX2"
    )
elseif (USING_REAL_CLANG OR USING_GCC)
    set_source_files_properties(ModalReverb.cpp PROPERTIES
        COMPILE_FLAGS "-mavx2 -mfma"
    )
endif()

if(ENABLE_PROT_PLUGIN)
add_library(gs_api_export INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/api_export/include/gs_api_export.h)
target_include_directories(gs_api_export INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/api_export/include)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    add_library(common_definitions INTERFACE)

    add_definitions(-DHAS_FILE -DHAS_MICREC)
    set(FFMPEG_DIR ${HOME_PATH}/programming/sources/libraries/ffmpeg/${ANDROID_ABI})
    add_library(avformat SHARED IMPORTED)
    set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavformat.so)
    add_library(avutil SHARED IMPORTED)
    set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavutil.so)
    add_library(avcodec SHARED IMPORTED)
    set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavcodec.so)
    add_library(swresample SHARED IMPORTED)
    set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libswresample.so)

    add_library(grainstorm SHARED ${SOURCE_FILES} ${ASM_SRCS} ${OS_SPECIFIC_SOURCE_FILES} ${HEADER_FILES} ${OS_SPECIFIC_HEADER_FILES})
    target_link_libraries(grainstorm PUBLIC tslgraphics gs_common_interface tsl_common_interface PRIVATE common_interface PRIVATE avutil PRIVATE avcodec PRIVATE avformat PRIVATE swresample)

    # RECORD AI->TRACK: audio.cpp engine cross-compiled by the grain-prompt-test
    # rig (cmake -B build-android, NDK toolchain, CPU backend). Absent silently
    # when the rig isn't built on this machine. Models are pushed to the app's
    # external files dir (MainActivity.getAiModelsDir).
    set(GS_AIGEN_ANDROID_ROOT ${HOME_PATH}/programming/grain-prompt-test)
    if (EXISTS ${GS_AIGEN_ANDROID_ROOT}/build-android/libengine_runtime.a)
        message(STATUS "GS_AIGEN enabled (android): ${GS_AIGEN_ANDROID_ROOT}")
        target_compile_definitions(grainstorm PRIVATE GS_AIGEN)
        target_include_directories(grainstorm PRIVATE
                ${GS_AIGEN_ANDROID_ROOT}/audio.cpp/include
                ${GS_AIGEN_ANDROID_ROOT}/audio.cpp/external/ggml/include
        )
        target_link_libraries(grainstorm PRIVATE
                ${GS_AIGEN_ANDROID_ROOT}/build-android/libengine_runtime.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/ggml/src/libggml.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/ggml/src/libggml-cpu.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/ggml/src/libggml-base.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/external/sentencepiece/src/libsentencepiece.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/libcjson_vendor.a
                ${GS_AIGEN_ANDROID_ROOT}/build-android/libyaml_vendor.a
        )
    endif()

    include_directories(
            ${FFMPEG_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
    set(FFMPEG_DIR ${HOME_PATH}/programming/sources/libraries/ffmpeg/linux-x86_64)
    add_library(avformat SHARED IMPORTED)
    set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavformat.so)
    add_library(avutil SHARED IMPORTED)
    set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavutil.so)
    add_library(avcodec SHARED IMPORTED)
    set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libavcodec.so)
    add_library(swresample SHARED IMPORTED)
    set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${FFMPEG_DIR}/lib/libswresample.so)

    add_library(grainstorm SHARED ${SOURCE_FILES} ${ASM_SRCS} ${OS_SPECIFIC_SOURCE_FILES})
    target_link_libraries(grainstorm PUBLIC gs_common_interface tslgraphics tsl_common_interface PRIVATE avutil avcodec avformat swresample)
    include_directories(
            ${FFMPEG_DIR}/include
            ${GFX_LOCATION}
            ${CMAKE_CURRENT_SOURCE_DIR}
            /home/pr/programming/tsl/font)
    target_include_directories(${GRAINSTORM_TARGET_NAME} INTERFACE
            ${FFMPEG_DIR}/include
            ${GFX_LOCATION}
            ${CMAKE_CURRENT_SOURCE_DIR}
            /home/pr/programming/tsl/font)

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        add_library(grainstorm ${SOURCE_FILES} ${ASM_SRCS} ${OS_SPECIFIC_SOURCE_FILES} ${HEADER_FILES} ${OS_SPECIFIC_HEADER_FILES})
        target_link_libraries(grainstorm PUBLIC   common_definitions common_interface tsl_common_interface gs_common_interface tslgraphics mfplat mfreadwrite mf mfuuid)
        target_include_directories(grainstorm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_features(grainstorm PUBLIC cxx_std_20)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" OR ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    if(${CMAKE_SYSTEM_NAME} STREQUAL "iOS" OR IOS OR CMAKE_OSX_SYSROOT MATCHES "iphone")
        message(STATUS "Building for iOS")
        set(PLATFORM_IOS TRUE)
    else()
        message(STATUS "Building for macOS")
        set(PLATFORM_MACOS TRUE)
    endif()
    
    add_library(grainstorm ${SOURCE_FILES} ${ASM_SRCS} ${OS_SPECIFIC_SOURCE_FILES} ${HEADER_FILES} ${OS_SPECIFIC_HEADER_FILES})
    
    target_link_libraries(grainstorm 
        PUBLIC tslgraphics common_definitions common_interface
        PUBLIC tsl_common_interface gs_common_interface
    )
    
    target_include_directories(grainstorm PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # macOS/iOS specific settings
    set_target_properties(grainstorm PROPERTIES
        MACOSX_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
        FRAMEWORK FALSE  # Set to TRUE if you want a .framework instead of .dylib
    )
    
    # Link required Apple frameworks
    target_link_libraries(grainstorm PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )

    # RECORD AI->TRACK: text-to-sample via the audio.cpp rig (macOS dev spike).
    # Absent silently when the rig isn't built on this machine.
    if (PLATFORM_MACOS)
        set(GS_AIGEN_ROOT "$ENV{HOME}/programming/grain-prompt-test" CACHE PATH
            "audio.cpp text-to-sample test rig")
        if (EXISTS ${GS_AIGEN_ROOT}/build/audio.cpp/libengine_runtime.a)
            message(STATUS "GS_AIGEN enabled: ${GS_AIGEN_ROOT}")
            target_compile_definitions(grainstorm PRIVATE GS_AIGEN)
            target_include_directories(grainstorm PRIVATE
                ${GS_AIGEN_ROOT}/audio.cpp/include
                ${GS_AIGEN_ROOT}/audio.cpp/external/ggml/include
            )
            target_link_libraries(grainstorm PUBLIC
                ${GS_AIGEN_ROOT}/build/audio.cpp/libengine_runtime.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/ggml/src/libggml.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/ggml/src/libggml-cpu.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/ggml/src/ggml-blas/libggml-blas.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/ggml/src/ggml-metal/libggml-metal.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/ggml/src/libggml-base.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/external/sentencepiece/src/libsentencepiece.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/libcjson_vendor.a
                ${GS_AIGEN_ROOT}/build/audio.cpp/libyaml_vendor.a
                "-framework Metal"
                "-framework MetalKit"
                "-framework Foundation"
                "-framework Accelerate"
            )
        endif()
    endif()

endif ()

source_group("Source Files" FILES  ${SOURCE_FILES} ${ASM_SRCS} ${OS_SPECIFIC_SOURCE_FILES})
source_group("Header Files" FILES  ${HEADER_FILES} ${OS_SPECIFIC_HEADER_FILES})
source_group("GUI" FILES ${GUI_SOURCE_FILES} ${GUI_HEADER_FILES})
get_target_property(INCLUDES tslgraphics INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Grainstorm include dirs: ${INCLUDES}")
message("Grainstorm C++ compiler flags: ${CMAKE_CXX_FLAGS}")
get_target_property(MY_APP_DEFINITIONS grainstorm COMPILE_DEFINITIONS)
get_directory_property(compile_defs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMPILE_DEFINITIONS)
message("Compile definitions: ${compile_defs}")
