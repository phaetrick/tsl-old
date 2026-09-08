cmake_minimum_required(VERSION 3.15)
project(LicenseChecker LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate a random obfuscation seed each build
string(TIMESTAMP NOW "%s")  # seconds since epoch
math(EXPR RAND_SEED "${NOW} % 1000000000")  # keep it within int range
# Pass it to compiler
add_compile_definitions(OBFUSCATION_SEED=${RAND_SEED})
message(STATUS "Using obfuscation seed: ${RAND_SEED}")

# --- OBFUSCATION OPTIONS ---
option(ENABLE_OBFUSCATION "Enable LLVM-based code obfuscation" OFF)
set(OBFUSCATOR_TOOL "C:/Users/phaet/programming/sources/obf/obfuscator-tool.exe" CACHE FILEPATH "Path to obfuscator tool")
set(OBFUSCATION_FLAGS "-strings -bcf -sub -skip-verify" CACHE STRING "Obfuscator flags")
set(CLANG_EXECUTABLE "C:/Program Files/LLVM/bin/clang++.exe" CACHE FILEPATH "Path to clang++ for IR generation")

# --- Paths relative to this CMakeLists.txt ---
set(PUBLIC_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LicenseChecker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OsCredentialStore.cpp
)
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LicenseChecker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/StringObfuscation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/OsCredentialStore.h
)

if(WIN32)
    if(NOT DEFINED OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "$ENV{HOME}/programming/sources/libraries/openssl/win")
    endif()
    set(OPENSSL_MSVC_STATIC_RT TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # No OpenSSL needed - license check disabled, App Store handles purchasing
elseif(APPLE)
    if(NOT DEFINED OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "$ENV{HOME}/programming/sources/libraries/openssl/mac")
    endif()
endif()
set(OPENSSL_USE_STATIC_LIBS TRUE)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_package(OpenSSL REQUIRED)
endif()

if(ENABLE_OBFUSCATION)
    message(STATUS "=== OBFUSCATION ENABLED ===")
    message(STATUS "Obfuscator: ${OBFUSCATOR_TOOL}")
    message(STATUS "Clang++: ${CLANG_EXECUTABLE}")
    message(STATUS "Flags: ${OBFUSCATION_FLAGS}")
    
    # Verify tools exist
    if(NOT EXISTS ${OBFUSCATOR_TOOL})
        message(FATAL_ERROR "Obfuscator tool not found at: ${OBFUSCATOR_TOOL}")
    endif()
    
    if(NOT EXISTS ${CLANG_EXECUTABLE})
        message(FATAL_ERROR "clang++ not found at: ${CLANG_EXECUTABLE}\nPlease set CLANG_EXECUTABLE to correct path")
    endif()
    
    set(OBFUSCATED_OBJECTS)
    
    foreach(SOURCE_FILE ${SOURCE_FILES})
        get_filename_component(FILE_NAME ${SOURCE_FILE} NAME_WE)
        get_filename_component(FILE_DIR ${SOURCE_FILE} DIRECTORY)
        
        set(IR_FILE "${CMAKE_BINARY_DIR}/obf_ir/${FILE_NAME}.ll")
        set(OBF_IR_FILE "${CMAKE_BINARY_DIR}/obf_ir/${FILE_NAME}_obf.ll")
        set(OBF_OBJ_FILE "${CMAKE_BINARY_DIR}/obf_obj/${FILE_NAME}_obf${CMAKE_CXX_OUTPUT_EXTENSION}")
        
        # Create directories
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/obf_ir")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/obf_obj")
        
        # Step 1: Compile to LLVM IR
        add_custom_command(
            OUTPUT ${IR_FILE}
            COMMAND ${CLANG_EXECUTABLE}
                -S -emit-llvm
                -std=c++17
                -O2
                -fexceptions
                ${SOURCE_FILE}
                -o ${IR_FILE}
                -I${PUBLIC_INCLUDE_DIRS}
                -I${OPENSSL_INCLUDE_DIR}
                -DOBFUSCATION_SEED=${RAND_SEED}
                -DNDEBUG
                -DOS_WIN
                -D_WIN32_WINNT=0x0A00
                -DLICENSE_CHECK_ENABLED
                -DRELEASEBUILD
            DEPENDS ${SOURCE_FILE} ${HEADER_FILES}
            COMMENT "Generating IR for ${FILE_NAME}.cpp"
            VERBATIM
        )
        
        # Step 2: Obfuscate IR
        add_custom_command(
            OUTPUT ${OBF_IR_FILE}
            COMMAND ${OBFUSCATOR_TOOL}
                -v
                -strings
                ${IR_FILE}
                -o ${OBF_IR_FILE}
            DEPENDS ${IR_FILE} ${OBFUSCATOR_TOOL}
            COMMENT "Obfuscating ${FILE_NAME}.cpp"
            VERBATIM
        )
        
        # Step 3: Compile obfuscated IR to object
        add_custom_command(
            OUTPUT ${OBF_OBJ_FILE}
            COMMAND ${CLANG_EXECUTABLE}
                -c ${OBF_IR_FILE}
                -o ${OBF_OBJ_FILE}
                -fexceptions
            DEPENDS ${OBF_IR_FILE}
            COMMENT "Compiling obfuscated ${FILE_NAME}.cpp"
            VERBATIM
        )
        
        list(APPEND OBFUSCATED_OBJECTS ${OBF_OBJ_FILE})
    endforeach()
    
    # Create library from obfuscated objects ONLY
    add_library(licensecheck STATIC ${HEADER_FILES})
    
    # Add obfuscated objects as sources
    target_sources(licensecheck PRIVATE ${OBFUSCATED_OBJECTS})
    
    set_source_files_properties(${OBFUSCATED_OBJECTS} PROPERTIES 
        EXTERNAL_OBJECT true
        GENERATED true
    )
    
    # DO NOT add original SOURCE_FILES here!
    
    # Explicitly set language since we're using object files
    set_target_properties(licensecheck PROPERTIES 
        LINKER_LANGUAGE CXX
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
    )
    
else()
    message(STATUS "Obfuscation disabled - building normally")
    add_library(licensecheck ${SOURCE_FILES} ${HEADER_FILES})
    
    set_target_properties(licensecheck PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
    )
endif()

target_include_directories(licensecheck
    PUBLIC ${PUBLIC_INCLUDE_DIRS}
    PRIVATE ${OPENSSL_INCLUDE_DIR}
)

# Clear problematic flags and set clean ones
if(ENABLE_OBFUSCATION)
    # For obfuscated build, we already compiled - just set interface flags
    set_target_properties(licensecheck PROPERTIES
        LINKER_LANGUAGE CXX
    )
else()
    # Override parent flags for normal build
    if (MSVC)
        target_compile_options(licensecheck PRIVATE 
            /EHsc
            $<$<CONFIG:Release>:/O2>
        )
    else()
        target_compile_options(licensecheck PRIVATE 
            -O2
            -fexceptions
        )
    endif()
endif()

target_link_libraries(licensecheck PRIVATE OpenSSL::SSL OpenSSL::Crypto common_definitions)
if(WIN32)
    target_link_libraries(licensecheck PRIVATE ws2_32 crypt32)
elseif(APPLE)
    target_link_libraries(licensecheck PRIVATE
            "-framework IOKit"
            "-framework Security"
    )
endif()
# Print summary
if(ENABLE_OBFUSCATION)
    message(STATUS "=== Build Configuration ===")
    message(STATUS "Obfuscated objects will be generated at: ${CMAKE_BINARY_DIR}/obf_obj/")
    message(STATUS "Use 'strings <library>' to verify string encryption")
endif()
