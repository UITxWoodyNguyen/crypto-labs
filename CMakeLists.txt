cmake_minimum_required(VERSION 3.20)
project(CryptographyLabs VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands cho clangd / VS Code Intellisense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tùy chọn build
option(BUILD_TESTS "Build all unit tests and KAT runners" ON)
option(BUILD_BENCHMARKS "Build benchmark suites" ON)

# Đưa thư mục cmake module vào path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Tìm các thư viện cần thiết
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Include modules
add_subdirectory(third_party)
add_subdirectory(common)

# Từng Lab độc lập
add_subdirectory(lab1_symmetric_cryptopp)
add_subdirectory(lab2_pure_aes)
add_subdirectory(lab3_rsa_hybrid)
add_subdirectory(lab4_hash_pki)
add_subdirectory(lab5_digital_signatures)
add_subdirectory(lab6_post_quantum)

if(BUILD_TESTS)
    enable_testing()
endif()