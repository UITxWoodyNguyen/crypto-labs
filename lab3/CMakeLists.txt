cmake_minimum_required(VERSION 3.20)
project(lab3_rsa_hybrid LANGUAGES CXX)

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
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPILER_NAME "clang")
else()
    set(COMPILER_NAME "gcc")
endif()

message(STATUS "Platform: ${PLATFORM_NAME}, Compiler: ${COMPILER_NAME}")
message(STATUS "Suggested build dir: ${CMAKE_SOURCE_DIR}/build_${PLATFORM_NAME}_${COMPILER_NAME}/")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")

# Lab 3: RSA Hybrid Encryption (needs both Crypto++ and OpenSSL)
# Linux package-manager installations commonly use /usr/include/crypto++
# and architecture-specific library directories. Seed the shared finder with
# those paths without changing the finder used by the other labs.
if(NOT WIN32)
    find_path(CryptoPP_INCLUDE_DIR
        NAMES aes.h
        HINTS
            "$ENV{HOME}/Desktop/nt219lib/cryptopp/include"
            /usr/include/crypto++
            /usr/local/include/crypto++
    )
    find_library(CryptoPP_LIBRARY
        NAMES cryptopp
        HINTS
            "$ENV{HOME}/Desktop/nt219lib/cryptopp/gcc/lib"
            /usr/lib
            /usr/lib64
            /usr/local/lib
    )
endif()
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

# Cảnh báo chỉ áp dụng cho target của lab (không áp cho Catch2 tải về).
# Tắt -Werror: cmake .. -DLAB3_WERROR=OFF
option(LAB3_WERROR "Treat warnings as errors (GCC/Clang)" ON)
function(lab3_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(LAB3_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---- Targets ----
add_library(rsa_hybrid_core STATIC
    src/utils.cpp
    src/rsa_oaep.cpp
    src/key_manager.cpp
    src/hybrid_engine.cpp
    src/kat_runner.cpp
    src/bench.cpp
)
target_include_directories(rsa_hybrid_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Crypto++ chưa được dùng trong code hiện tại (mã hoá dùng OpenSSL EVP) nhưng vẫn link theo template môn học.
target_link_libraries(rsa_hybrid_core PUBLIC
    CryptoPP::CryptoPP OpenSSL::Crypto OpenSSL::SSL Threads::Threads)
if(WIN32)
    target_link_libraries(rsa_hybrid_core PUBLIC ws2_32 crypt32)  # cần khi OpenSSL link tĩnh
endif()
lab3_set_warnings(rsa_hybrid_core)

add_executable(rsatool src/main.cpp)
target_link_libraries(rsatool PRIVATE rsa_hybrid_core)
lab3_set_warnings(rsatool)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.3
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    add_subdirectory(tests)
endif()
