cmake_minimum_required(VERSION 3.20)
project(Demo1Project LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# 1. Tìm Crypto++
find_package(CryptoPP REQUIRED)

# 2. Tìm OpenSSL
# Chỉ ép đường dẫn cứng trên Windows nếu có
if(WIN32)
    set(OPENSSL_ROOT_DIR "D:/UIT/NT219/libs/openssl361" CACHE PATH "Path to OpenSSL installation")
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

find_package(OpenSSL REQUIRED)

include(CompilerWarnings OPTIONAL)

add_executable(demo1_app demo.cpp)

if(COMMAND target_set_compiler_warnings)
    target_set_compiler_warnings(demo1_app)
endif()

target_include_directories(demo1_app PRIVATE 
    ${OPENSSL_INCLUDE_DIR}
)

# Link thư viện (các lib socket ws2_32/crypt32 chỉ cần trên Windows)
target_link_libraries(demo1_app PRIVATE 
    CryptoPP::CryptoPP
    OpenSSL::Crypto
    OpenSSL::SSL
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
)