cmake_minimum_required(VERSION 3.20)
project(Demo1Project LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# 1. Tìm thư viện Crypto++
find_package(CryptoPP REQUIRED)

# 2. Tìm thư viện OpenSSL (Ưu tiên đường dẫn đã cài đặt ở --prefix)
set(OPENSSL_ROOT_DIR "D:/UIT/NT219/libs/openssl361" CACHE PATH "Path to OpenSSL installation")
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

include(CompilerWarnings)

add_executable(demo1_app demo.cpp)

target_set_compiler_warnings(demo1_app)

target_include_directories(demo1_app PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(demo1_app PRIVATE 
    CryptoPP::CryptoPP
    OpenSSL::Crypto
    OpenSSL::SSL
    ws2_32
    crypt32
)