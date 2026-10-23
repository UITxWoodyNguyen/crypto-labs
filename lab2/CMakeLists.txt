cmake_minimum_required(VERSION 3.20)
project(aes_lab2 LANGUAGES CXX)

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

# --- Crypto++ (optional reference implementation for cross-validation) ---
find_package(CryptoPP QUIET)

# --- OpenSSL (reference implementation for cross-validation and benchmarking) ---
# OpenSSL provides CMake config files in lib/cmake/OpenSSL or lib64/cmake/OpenSSL
# Hint paths for different platforms/compilers (same as Lab 1)
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

# Threads (needed for OpenSSL / benchmark)
find_package(Threads REQUIRED)

# =============================================================================
# Compiler Warning Flags
# =============================================================================
option(LAB2_WERROR "Treat warnings as errors (GCC/Clang)" ON)
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LAB2_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

# =============================================================================
# Targets
# =============================================================================

# --- Static library: aes_pure (Pure AES-128 core + CTR mode) ---
add_library(aes_pure STATIC
    src/aes128.cpp
    src/ctr.cpp
    src/util.cpp
    src/json.cpp
    src/kat.cpp
    src/stats.cpp
    src/bench.cpp
    src/xval.cpp
)
target_include_directories(aes_pure PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(aes_pure PUBLIC
    Threads::Threads
)
target_link_libraries(aes_pure PRIVATE
    OpenSSL::Crypto
    OpenSSL::SSL
)
target_compile_definitions(aes_pure PRIVATE AESP_HAVE_OPENSSL=1)

if(CryptoPP_FOUND)
    target_link_libraries(aes_pure PRIVATE CryptoPP::CryptoPP)
    target_compile_definitions(aes_pure PRIVATE AESP_HAVE_CRYPTOPP=1)
endif()

if(WIN32)
    target_link_libraries(aes_pure PUBLIC bcrypt)   # OS CSPRNG (BCryptGenRandom)
endif()

# Build info printed by `aestool bench` (hardware/compiler table in the report)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
set(AESP_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}")
string(STRIP "${AESP_FLAGS}" AESP_FLAGS)
target_compile_definitions(aes_pure PRIVATE
    AESP_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    AESP_BUILD_FLAGS="${AESP_FLAGS}"
)

# --- Executable: aestool ---
add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aes_pure)
if(WIN32)
    target_link_libraries(aestool PRIVATE shell32)   # CommandLineToArgvW (UTF-8 argv)
endif()

# =============================================================================
# Tests
# =============================================================================
option(BUILD_TESTS "Build unit tests, KAT runner, and CLI tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Fetch Catch2 for unit tests (prefer installed v3 if available)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.3
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    # On Windows Clang, skip unit tests due to ABI incompatibility with GCC-built libs
    set(SKIP_TESTS_FOR_CLANG_WINDOWS FALSE)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(SKIP_TESTS_FOR_CLANG_WINDOWS TRUE)
        message(WARNING "Windows Clang: Skipping unit tests due to ABI incompatibility with GCC-built Crypto++/OpenSSL")
    endif()

    # Unit tests (Catch2)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_aes.cpp" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_executable(aes_tests tests/test_aes.cpp)
        target_link_libraries(aes_tests PRIVATE aes_pure Catch2::Catch2WithMain)
        target_compile_definitions(aes_tests PRIVATE
            AESP_KAT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json"
        )
        # Clang on Windows: __COUNTER__ is a Microsoft extension, disable C2y warning for tests
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(aes_tests PRIVATE -Wno-c2y-extensions)
        endif()
        add_test(NAME unit_tests COMMAND aes_tests)
    endif()

    # KAT runner
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_test(NAME kat_runner COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
    endif()

    # CLI end-to-end regression tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake" AND NOT SKIP_TESTS_FOR_CLANG_WINDOWS)
        add_test(NAME cli_end_to_end
            COMMAND ${CMAKE_COMMAND}
                    -DTOOL=$<TARGET_FILE:aestool>
                    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e_work
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake
        )
    endif()
endif()

# =============================================================================
# Installation (Optional)
# =============================================================================
install(TARGETS aes_pure aestool
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.hpp")
