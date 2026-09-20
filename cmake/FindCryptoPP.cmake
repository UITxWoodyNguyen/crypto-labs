# Tìm headers (aes.h)
find_path(CryptoPP_INCLUDE_DIR
    NAMES cryptopp/aes.h aes.h
    PATHS
        "D:/UIT/NT219/libs/cryptopp/include"
        "D:/UIT/NT219/libs/cryptopp"          # Nơi chứa trực tiếp *.h nếu chưa copy vào include/
        "${CMAKE_SOURCE_DIR}/third_party/cryptopp/include"
        "/mingw64/include"
        "/usr/include"
)

# Tìm file thư viện (libcryptopp.a)
find_library(CryptoPP_LIBRARY
    NAMES cryptopp libcryptopp
    PATHS
        "D:/UIT/NT219/libs/cryptopp/lib"
        "D:/UIT/NT219/libs/cryptopp"          # Nơi sinh ra libcryptopp.a sau khi chạy make
        "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
        "/mingw64/lib"
        "/usr/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CryptoPP
    DEFAULT_MSG
    CryptoPP_LIBRARY
    CryptoPP_INCLUDE_DIR
)

if(CryptoPP_FOUND AND NOT TARGET CryptoPP::CryptoPP)
    add_library(CryptoPP::CryptoPP UNKNOWN IMPORTED)
    set_target_properties(CryptoPP::CryptoPP PROPERTIES
        IMPORTED_LOCATION "${CryptoPP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CryptoPP_INCLUDE_DIR}"
    )
endif()