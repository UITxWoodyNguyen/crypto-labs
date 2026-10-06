# Tìm headers (aes.h) - headers ở trực tiếp trong include/ (flat structure)
find_path(CryptoPP_INCLUDE_DIR
    NAMES aes.h
    PATHS
        "D:/UIT/NT219/libs/cryptopp/include"
        "${CMAKE_SOURCE_DIR}/third_party/cryptopp/include"
        "/mingw64/include"
        "/usr/include"
)

# Tìm file thư viện - ưu tiên MSVC static library
if(MSVC)
    # Ưu tiên x64/Output/Release - static library đầy đủ symbols
    find_library(CryptoPP_LIBRARY
        NAMES cryptlib cryptopp-static cryptopp
        PATHS
            "D:/UIT/NT219/libs/cryptopp/x64/Output/Release"
            "D:/UIT/NT219/libs/cryptopp/x64/Output/Debug"
            "D:/UIT/NT219/libs/cryptopp/x64/cryptlib/Release"
            "D:/UIT/NT219/libs/cryptopp/x64/cryptlib/Debug"
            "D:/UIT/NT219/libs/cryptopp/Win32/cryptlib/Release"
            "D:/UIT/NT219/libs/cryptopp/Win32/cryptlib/Debug"
            "D:/UIT/NT219/libs/cryptopp/library/msvc"
            "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
    )
else()
    find_library(CryptoPP_LIBRARY
        NAMES cryptopp libcryptopp
        PATHS
            "D:/UIT/NT219/libs/cryptopp/library/gcc"
            "D:/UIT/NT219/libs/cryptopp/library/clang"
            "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
            "/mingw64/lib"
            "/usr/lib"
    )
endif()

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