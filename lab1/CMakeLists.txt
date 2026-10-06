# Lab1: AES Tool - sử dụng cấu hình từ root CMakeLists.txt
# Không gọi project() hay cmake_minimum_required() ở đây

# Tìm Crypto++ qua module chung
find_package(CryptoPP REQUIRED)

# Static library
add_library(aescore STATIC src/aescore.cpp)
target_include_directories(aescore PUBLIC src)
target_link_libraries(aescore PUBLIC CryptoPP::CryptoPP)

# Sử dụng cờ compiler chung
include(CompilerWarnings)
target_set_compiler_warnings(aescore)

add_executable(aestool src/main.cpp)
target_link_libraries(aestool PRIVATE aescore)
target_set_compiler_warnings(aestool)

# ---- Tests (Catch2 + KAT) ----
include(CTest)
if(BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.5.2)
  FetchContent_MakeAvailable(Catch2)
  add_executable(test_core tests/test_core.cpp)
  target_link_libraries(test_core PRIVATE aescore Catch2::Catch2WithMain)
  target_set_compiler_warnings(test_core)
  add_test(NAME unit_tests COMMAND test_core)
  add_test(NAME nist_kat COMMAND aestool --kat ${CMAKE_CURRENT_SOURCE_DIR}/kat/vectors.json)
  add_test(NAME cli_negative
           COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:aestool> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_neg
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_negative.cmake)
endif()
