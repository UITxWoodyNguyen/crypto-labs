# Add Catch2
include(Catch)

add_executable(rsa_tests test_rsa.cpp test_negative.cpp)
target_link_libraries(rsa_tests PRIVATE rsa_hybrid_core Catch2::Catch2WithMain)
lab3_set_warnings(rsa_tests)
catch_discover_tests(rsa_tests)

add_test(NAME kat_runner COMMAND rsatool --kat ${CMAKE_CURRENT_SOURCE_DIR}/vectors/rsa_oaep_vectors.json)