# Demo-1: Multi-Library Verification (Crypto++ & OpenSSL)

Independent verification setup to test C++17 compilation, CMake multi-target linking, and runtime compatibility between Crypto++ and OpenSSL.

## Prerequisite Checks
1. Crypto++ headers reside in `<CRYPTOPP_ROOT>/include/cryptopp/` or `<CRYPTOPP_ROOT>/cryptopp/`.
2. OpenSSL static libraries and headers exist under `D:/UIT/NT219/libs/openssl361/`.

## Build & Run Instructions

Inside **MSYS2 MinGW 64-bit Terminal** (`mingw64.exe`):

```bash
cd /d/UIT/NT219/crypto-labs/demo-1

# Configure
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build -j$(nproc)

# Run
./build/demo1_app.exe