cmake_minimum_required(VERSION 3.20)
project(aes_lab1 LANGUAGES CXX)

# =============================================================================
# Project Configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile commands for clangd/VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Platform & Compiler Detection
# =============================================================================
if(WIN32)
    set(PLATFORM_NAME "windows")
else()
    set(PLATFORM_NAME "ubuntu")
endif()

if(MSVC)
    set(COMPILER_NAME "msvc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_NAME "clang")
else()
    set(COMPILER_NAME "gcc")
endif()

# Build directory naming hint (for user reference)
message(STATUS "Platform: ${PLATFORM_NAME}, Compiler: ${COMPILER_NAME}")
message(STATUS "Suggested build dir: ${CMAKE_SOURCE_DIR}/build_${PLATFORM_NAME}_${COMPILER_NAME}/")

# =============================================================================
# Module Path (for FindCryptoPP.cmake)
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

# =============================================================================
# Find Dependencies
# =============================================================================

# --- Crypto++ ---
find_package(CryptoPP REQUIRED)

# --- OpenSSL ---
# OpenSSL provides CMake config files in lib/cmake/OpenSSL or lib64/cmake/OpenSSL
# Hint paths for different platforms/compilers
if(WIN32)
    if(MSVC)
        set(OPENSSL_ROOT_DIR "D:/UIT/NT219/libs/openssl/msvc" CACHE PATH "OpenSSL root dir" FORCE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(OPENSSL_ROOT_DIR "D:/UIT/NT219/libs/openssl/clang" CACHE PATH "OpenSSL root dir" FORCE)
    else()
        set(OPENSSL_ROOT_DIR "D:/UIT/NT219/libs/openssl/gcc" CACHE PATH "OpenSSL root dir" FORCE)
    endif()
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(OPENSSL_ROOT_DIR "$ENV{HOME}/Desktop/nt219lib/openssl/clang" CACHE PATH "OpenSSL root dir" FORCE)
    else()
        set(OPENSSL_ROOT_DIR "$ENV{HOME}/Desktop/nt219lib/openssl/gcc" CACHE PATH "OpenSSL root dir" FORCE)
    endif()
endif()

find_package(OpenSSL REQUIRED)

# Threads (needed for OpenSSL)
find_package(Threads REQUIRED)

# =============================================================================
# Compiler Warning Flags
# =============================================================================
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# =============================================================================
# Targets
# =============================================================================

# --- Static library: aescore ---
add_library(aescore STATIC src/aescore.cpp)
target_include_directories(aescore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(aescore PUBLIC
    CryptoPP::CryptoPP
    OpenSSL::Crypto
    OpenSSL::SSL
    Threads::Threads
)

# --- Executable: aestool ---
add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aescore)

# =============================================================================
# Tests (Optional)
# =============================================================================
option(BUILD_TESTS "Build unit tests and KAT runners" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Fetch Catch2 for unit tests
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_core.cpp")
        add_executable(test_core tests/test_core.cpp)
        target_link_libraries(test_core PRIVATE aescore Catch2::Catch2WithMain)
        # Clang on Windows: __COUNTER__ is a Microsoft extension, disable C2y warning for tests
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(test_core PRIVATE -Wno-c2y-extensions)
        endif()
        add_test(NAME unit_tests COMMAND test_core)
    endif()
    
    # KAT test (if vectors exist)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")
        add_test(NAME nist_kat COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
    endif()
    
    # CLI negative tests (if script exists)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_negative.cmake")
        add_test(NAME cli_negative
            COMMAND ${CMAKE_COMMAND}
                -DTOOL=$<TARGET_FILE:aestool>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_neg
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_negative.cmake
        )
    endif()
endif()

# =============================================================================
# Installation (Optional)
# =============================================================================
install(TARGETS aescore aestool
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")