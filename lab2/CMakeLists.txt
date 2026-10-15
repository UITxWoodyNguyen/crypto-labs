cmake_minimum_required(VERSION 3.20)
project(lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

message(STATUS "Platform: ${PLATFORM_NAME}, Compiler: ${COMPILER_NAME}")
message(STATUS "Suggested build dir: ${CMAKE_SOURCE_DIR}/build_${PLATFORM_NAME}_${COMPILER_NAME}/")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

# Lab 2: Pure AES implementation (no Crypto++, only OpenSSL for cross-validation / comparison)
# OpenSSL root per platform/compiler. Only applied if the directory exists and the user did not
# pass -DOPENSSL_ROOT_DIR=..., otherwise CMake falls back to the system OpenSSL (e.g. libssl-dev).
if(NOT OPENSSL_ROOT_DIR)
    if(WIN32)
        if(MSVC)
            set(_ossl_dir "D:/UIT/NT219/libs/openssl/msvc")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(_ossl_dir "D:/UIT/NT219/libs/openssl/clang")
        else()
            set(_ossl_dir "D:/UIT/NT219/libs/openssl/gcc")
        endif()
    else()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(_ossl_dir "$ENV{HOME}/Desktop/nt219lib/openssl/clang")
        else()
            set(_ossl_dir "$ENV{HOME}/Desktop/nt219lib/openssl/gcc")
        endif()
    endif()
    if(EXISTS "${_ossl_dir}")
        set(OPENSSL_ROOT_DIR "${_ossl_dir}" CACHE PATH "OpenSSL root dir" FORCE)
    else()
        message(STATUS "OpenSSL dir '${_ossl_dir}' not found -> using system OpenSSL")
    endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Warnings are applied per target (NOT globally) so that -Werror never hits third-party code (Catch2).
option(LAB2_WERROR "Treat warnings as errors (GCC/Clang)" ON)
function(lab2_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(LAB2_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# AES core: from-scratch AES-128 + CTR. NO crypto library is used in these files
# (xval.cpp / OpenSSL is a reference implementation for cross-validation only).
# ---------------------------------------------------------------------------
add_library(aes_pure STATIC
    src/aes128.cpp
    src/ctr.cpp
    src/util.cpp
    src/json.cpp
    src/kat.cpp
    src/stats.cpp
    src/bench.cpp
    src/xval.cpp)
target_include_directories(aes_pure PUBLIC src)
target_link_libraries(aes_pure PUBLIC Threads::Threads)
target_link_libraries(aes_pure PRIVATE OpenSSL::Crypto)
target_compile_definitions(aes_pure PRIVATE AESP_HAVE_OPENSSL=1)
lab2_warnings(aes_pure)
if(WIN32)
    target_link_libraries(aes_pure PUBLIC bcrypt)   # OS CSPRNG (BCryptGenRandom)
endif()

# Build info printed by `aestool bench` (hardware/compiler table in the report)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
set(AESP_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}")
string(STRIP "${AESP_FLAGS}" AESP_FLAGS)
target_compile_definitions(aes_pure PRIVATE
    AESP_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    AESP_BUILD_FLAGS="${AESP_FLAGS}")

add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aes_pure)
lab2_warnings(aestool)
if(WIN32)
    target_link_libraries(aestool PRIVATE shell32)   # CommandLineToArgvW (UTF-8 argv)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 QUIET)           # use an installed Catch2 if present (offline friendly)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.3
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(aes_tests tests/test_aes.cpp)
    target_link_libraries(aes_tests PRIVATE aes_pure Catch2::Catch2WithMain)
    target_compile_definitions(aes_tests PRIVATE
        AESP_KAT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json")
    lab2_warnings(aes_tests)

    add_test(NAME unit_tests COMMAND aes_tests)
    add_test(NAME kat_runner COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
    add_test(NAME cli_end_to_end
        COMMAND ${CMAKE_COMMAND}
                -DTOOL=$<TARGET_FILE:aestool>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/e2e_work
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.cmake)
endif()
