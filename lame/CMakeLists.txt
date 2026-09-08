# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED HOME_PATH)
    if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
        set(HOME_PATH "C:/Users/phaet")
    elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
        set(HOME_PATH "/Users/patrickropohl")
    else ()
        set(HOME_PATH "/home/pr")
    endif ()
endif()
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    message("TSL Windows ${CMAKE_BUILD_TYPE} Build")
endif()

# NEW - USE THIS:
if(NOT TARGET common_interface)
    add_subdirectory(${HOME_PATH}/programming/CPP-New/common_interface
                     ${CMAKE_BINARY_DIR}/common_interface)
endif()

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             mp3lame

             # Sets the library as a shared library.
	     STATIC

             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             libmp3lame/lame.c
             libmp3lame/util.c
             libmp3lame/psymodel.c
             libmp3lame/fft.c
             libmp3lame/tables.c
             libmp3lame/encoder.c
             libmp3lame/newmdct.c
             libmp3lame/quantize_pvt.c
             libmp3lame/reservoir.c
             libmp3lame/bitstream.c
             libmp3lame/VbrTag.c
             libmp3lame/version.c
             libmp3lame/takehiro.c
             libmp3lame/quantize.c
             libmp3lame/vbrquantize.c
             libmp3lame/gain_analysis.c
             libmp3lame/id3tag.c
             libmp3lame/set_get.c
             libmp3lame/presets.c )

include_directories(include)

add_definitions(-DHAVE_CONFIG_H -DSTDC_HEADERS -DHAVE_MEMCPY -DHAVE_STRCHR)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

if(ANDROID)
find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       mp3lame
                       common_interface
                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )

endif()
