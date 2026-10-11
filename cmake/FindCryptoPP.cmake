# FindCryptoPP.cmake - Cross-platform Crypto++ finder
# Supports: Windows (MSVC, GCC/mingw64, Clang) and Linux (GCC, Clang)

# --- Configuration hints ---
# Windows: set CRYPTOPP_ROOT_DIR to D:/UIT/NT219/libs/cryptopp (or similar)
# Linux:   set CRYPTOPP_ROOT_DIR to ~/Desktop/nt219lib/cryptopp (or similar)

if(NOT CRYPTOPP_ROOT_DIR)
    if(WIN32)
        set(CRYPTOPP_ROOT_DIR "D:/UIT/NT219/libs/cryptopp" CACHE PATH "Root directory of Crypto++ installation")
    else()
        set(CRYPTOPP_ROOT_DIR "$ENV{HOME}/Desktop/nt219lib/cryptopp" CACHE PATH "Root directory of Crypto++ installation")
    endif()
endif()

# Determine compiler-specific subdirectory
if(MSVC)
    set(CRYPTOPP_COMPILER_SUBDIR "library/msvc")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(WIN32)
        set(CRYPTOPP_COMPILER_SUBDIR "library/clang")
    else()
        set(CRYPTOPP_COMPILER_SUBDIR "clang")
    endif()
else()
    # Default to GCC
    if(WIN32)
        set(CRYPTOPP_COMPILER_SUBDIR "library/gcc")
    else()
        set(CRYPTOPP_COMPILER_SUBDIR "gcc")
    endif()
endif()

# --- Find headers ---
# Headers are at: ${CRYPTOPP_ROOT_DIR}/include/ (flat structure: aes.h directly in include/)
find_path(CryptoPP_INCLUDE_DIR
    NAMES aes.h
    PATHS
        "${CRYPTOPP_ROOT_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party/cryptopp/include"
        "/mingw64/include"
        "/usr/include"
    NO_DEFAULT_PATH
)

# --- Find library ---
# Library location varies by platform and compiler:
# Windows MSVC:   ${CRYPTOPP_ROOT_DIR}/library/msvc/cryptlib.lib (or x64/Output/Release/cryptlib.lib)
# Windows GCC:    ${CRYPTOPP_ROOT_DIR}/library/gcc/libcryptopp.a
# Windows Clang:  ${CRYPTOPP_ROOT_DIR}/library/clang/libcryptopp.a
# Linux GCC:      ${CRYPTOPP_ROOT_DIR}/gcc/lib/libcryptopp.a
# Linux Clang:    ${CRYPTOPP_ROOT_DIR}/clang/lib/libcryptopp.a

if(MSVC)
    find_library(CryptoPP_LIBRARY
        NAMES cryptlib cryptopp-static cryptopp
        PATHS
            "${CRYPTOPP_ROOT_DIR}/x64/Output/Release"
            "${CRYPTOPP_ROOT_DIR}/x64/Output/Debug"
            "${CRYPTOPP_ROOT_DIR}/x64/cryptlib/Release"
            "${CRYPTOPP_ROOT_DIR}/x64/cryptlib/Debug"
            "${CRYPTOPP_ROOT_DIR}/Win32/cryptlib/Release"
            "${CRYPTOPP_ROOT_DIR}/Win32/cryptlib/Debug"
            "${CRYPTOPP_ROOT_DIR}/${CRYPTOPP_COMPILER_SUBDIR}"
            "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
        NO_DEFAULT_PATH
    )
elseif(WIN32)
    # MinGW GCC or Clang on Windows
    find_library(CryptoPP_LIBRARY
        NAMES cryptopp libcryptopp
        PATHS
            "${CRYPTOPP_ROOT_DIR}/${CRYPTOPP_COMPILER_SUBDIR}"
            "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
            "/mingw64/lib"
        NO_DEFAULT_PATH
    )
else()
    # Linux (GCC or Clang)
    find_library(CryptoPP_LIBRARY
        NAMES cryptopp libcryptopp
        PATHS
            "${CRYPTOPP_ROOT_DIR}/${CRYPTOPP_COMPILER_SUBDIR}/lib"
            "${CMAKE_SOURCE_DIR}/third_party/cryptopp/lib"
            "/usr/lib"
            "/usr/local/lib"
        NO_DEFAULT_PATH
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

# Mark as advanced
mark_as_advanced(CryptoPP_INCLUDE_DIR CryptoPP_LIBRARY CRYPTOPP_ROOT_DIR CRYPTOPP_COMPILER_SUBDIR)