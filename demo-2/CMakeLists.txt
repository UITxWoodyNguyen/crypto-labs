cmake_minimum_required(VERSION 3.20)
project(CryptoDemo2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add path to FindCryptoPP.cmake and CompilerWarnings.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(CryptoPP REQUIRED)
include(CompilerWarnings)

add_executable(demo2_app demo.cpp)

target_set_compiler_warnings(demo2_app)
target_link_libraries(demo2_app PRIVATE CryptoPP::CryptoPP)