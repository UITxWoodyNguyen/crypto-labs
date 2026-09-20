cmake_minimum_required(VERSION 3.20)
project(CryptoDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Thêm đường dẫn chứa FindCryptoPP.cmake và CompilerWarnings.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(CryptoPP REQUIRED)
include(CompilerWarnings)

add_executable(demo_app demo.cpp)

target_set_compiler_warnings(demo_app)
target_link_libraries(demo_app PRIVATE CryptoPP::CryptoPP)