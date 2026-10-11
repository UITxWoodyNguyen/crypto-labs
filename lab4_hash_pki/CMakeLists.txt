cmake_minimum_required(VERSION 3.20)
project(lab4_hash_pki LANGUAGES CXX)

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

# Lab 4: Hash & PKI (needs both Crypto++ and OpenSSL)
find_package(CryptoPP REQUIRED)

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
find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Targets (to be implemented)
# add_library(hash_pki STATIC src/hash_pki.cpp)
# target_link_libraries(hash_pki PUBLIC CryptoPP::CryptoPP OpenSSL::Crypto OpenSSL::SSL Threads::Threads)
# add_executable(hash_pki_tool src/main.cpp)
# target_link_libraries(hash_pki_tool PRIVATE hash_pki)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
endif()