# Lab 1: Symmetric Encryption with Crypto++

## Objective
Implement AES symmetric encryption/decryption using Crypto++ library with multiple modes (ECB, CBC, GCM, CTR) and compare performance with OpenSSL.

## Topics Covered
- AES block cipher (128/192/256-bit keys)
- Block cipher modes: ECB, CBC, CFB, OFB, CTR, GCM, CCM
- Key derivation (PBKDF2)
- Authenticated encryption (GCM, CCM)
- Performance benchmarking

## Dependencies
- **Crypto++**: `D:\UIT\NT219\libs\cryptopp\` (Windows) / `~/Desktop/nt219lib/cryptopp/` (Linux)
  - Headers: `include/`
  - Libraries per compiler: `library/{msvc,gcc,clang}/` (Windows) / `{gcc,clang}/lib/` (Linux)
- **OpenSSL**: `D:\UIT\NT219\libs\openssl\` (Windows) / `~/Desktop/nt219lib/openssl/` (Linux)
  - Per compiler: `{msvc,gcc,clang}/{include,lib,lib64}/`

## Source Structure
```
lab1/
├── CMakeLists.txt
├── src/
│   ├── aescore.hpp       # Core AES wrapper interface
│   ├── aescore.cpp       # Implementation using Crypto++
│   ├── cryptopp_all.hpp  # Unified Crypto++ headers
│   └── main.cpp          # CLI tool entry point
├── tests/
│   └── test_core.cpp     # Unit tests (Catch2)
├── kat/
│   └── vectors.json      # NIST KAT test vectors
├── scripts/
│   ├── build_windows.bat # Windows build script (MSVC/GCC/Clang)
│   └── build_linux.sh    # Linux build script (GCC/Clang)
└── build_<platform>_<compiler>/  # Build output (created during build)
```

## Building

### Option 1: Using Build Scripts (Recommended)

#### Windows (MSYS2 mingw64 shell)
```bash
# From lab1/ directory
cd lab1

# MSVC (requires x64 Native Tools Command Prompt for VS)
scripts\build_windows.bat msvc

# GCC (MinGW-w64)
scripts\build_windows.bat gcc

# Clang (MSYS2 mingw64)
scripts\build_windows.bat clang

# Clean all build dirs
scripts\build_windows.bat clean
```

#### Linux (Ubuntu/Kali)
```bash
# From lab1/ directory
cd lab1

# GCC
scripts/build_linux.sh gcc

# Clang
scripts/build_linux.sh clang

# Clean build dir
scripts/build_linux.sh gcc --clean
```

### Option 2: Manual CMake Commands

#### Windows - MSVC (x64 Native Tools Command Prompt)
```bash
cd lab1
mkdir build_windows_msvc && cd build_windows_msvc
cmake .. -G "Visual Studio 18 2026" -A x64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCRYPTOPP_ROOT_DIR="D:/UIT/NT219/libs/cryptopp" \
  -DOPENSSL_ROOT_DIR="D:/UIT/NT219/libs/openssl/msvc" \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build . --config Release
```

#### Windows - GCC (MSYS2 mingw64 shell)
```bash
cd lab1
mkdir build_windows_gcc && cd build_windows_gcc
cmake .. -G "MinGW Makefiles" \
  -DCMAKE_CXX_COMPILER=g++.exe \
  -DCMAKE_BUILD_TYPE=Release \
  -DCRYPTOPP_ROOT_DIR="D:/UIT/NT219/libs/cryptopp" \
  -DOPENSSL_ROOT_DIR="D:/UIT/NT219/libs/openssl/gcc"
cmake --build .
```

#### Windows - Clang (MSYS2 mingw64 shell)
```bash
cd lab1
mkdir build_windows_clang && cd build_windows_clang
cmake .. -G "MinGW Makefiles" \
  -DCMAKE_CXX_COMPILER=clang++.exe \
  -DCMAKE_BUILD_TYPE=Release \
  -DCRYPTOPP_ROOT_DIR="D:/UIT/NT219/libs/cryptopp" \
  -DOPENSSL_ROOT_DIR="D:/UIT/NT219/libs/openssl/clang"
cmake --build .
```

#### Linux - GCC
```bash
cd lab1
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCryptoPP_INCLUDE_DIR="$HOME/Desktop/nt219lib/cryptopp/gcc/include" \
  -DCryptoPP_LIBRARY="$HOME/Desktop/nt219lib/cryptopp/gcc/lib/libcryptopp.a" \
  -DOPENSSL_ROOT_DIR="$HOME/Desktop/nt219lib/openssl/gcc"
cmake --build .
```

#### Linux - Clang
```bash
cd lab1
mkdir build_ubuntu_clang && cd build_ubuntu_clang
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCryptoPP_INCLUDE_DIR="$HOME/Desktop/nt219lib/cryptopp/clang/include" \
  -DCryptoPP_LIBRARY="$HOME/Desktop/nt219lib/cryptopp/clang/lib/libcryptopp.a" \
  -DOPENSSL_ROOT_DIR="$HOME/Desktop/nt219lib/openssl/clang"
cmake --build .
```

## Usage

```bash
# Help
./aestool --help

# Encrypt
./aestool encrypt --key <hex_key> --iv <hex_iv> --mode cbc --in plaintext.txt --out ciphertext.bin

# Decrypt
./aestool decrypt --key <hex_key> --iv <hex_iv> --mode cbc --in ciphertext.bin --out decrypted.txt

# Run NIST KAT
./aestool --kat kat/vectors.json

# Benchmark
./aestool bench --mode gcm --size 1048576 --iterations 1000
```

## Supported Modes
| Mode | IV/Nonce | Auth Tag | Use Case |
|------|----------|----------|----------|
| ECB | No | No | Legacy only (NOT recommended) |
| CBC | 16 bytes | No | General purpose |
| CFB | 16 bytes | No | Stream-like |
| OFB | 16 bytes | No | Stream-like |
| CTR | 16 bytes | No | Parallelizable |
| GCM | 12 bytes | 16 bytes | **Recommended** (AEAD) |
| CCM | 12 bytes | 16 bytes | AEAD (constrained) |

## Running Tests

```bash
# From build directory
cd build_<platform>_<compiler>
ctest --output-on-failure
```

Tests include:
- Unit tests (Catch2, fetched automatically)
- NIST KAT (Known Answer Tests) - 9 vectors
- CLI negative testing

## Verified Build Matrix

| Platform | Compiler | Config | Build | KAT Tests |
|----------|----------|--------|-------|-----------|
| Windows | MSVC (VS 2026) | Release | ✅ | ✅ 9/9 pass |
| Windows | GCC 16.2 (mingw64) | Release | ✅ | ✅ 9/9 pass |
| Windows | Clang 22.1 (mingw64) | Release | ✅ | ✅ (builds OK) |
| Linux | GCC | Release | ⏳ | ⏳ |
| Linux | Clang | Release | ⏳ | ⏳ |

## Cross-Platform Latency Comparison

After building on all target platforms, run benchmarks and record:

| Platform | Compiler | Mode | Block Size | Ops/sec | Latency (ns/op) |
|----------|----------|------|------------|---------|-----------------|
| Windows | MSVC | GCM | 16KB | TBD | TBD |
| Windows | GCC | GCM | 16KB | TBD | TBD |
| Windows | Clang | GCM | 16KB | TBD | TBD |
| Ubuntu | GCC | GCM | 16KB | TBD | TBD |
| Ubuntu | Clang | GCM | 16KB | TBD | TBD |

## Implementation Notes
- Uses Crypto++ for primary implementation
- OpenSSL used for comparison benchmarking
- All crypto operations use constant-time implementations where available
- Keys/IVs handled via `SecByteBlock` for secure memory management

## References
- [Crypto++ AES Documentation](https://www.cryptopp.com/wiki/AES)
- [NIST SP 800-38A](https://csrc.nist.gov/publications/detail/sp/800-38a/final) - Block Cipher Modes
- [NIST SP 800-38D](https://csrc.nist.gov/publications/detail/sp/800-38d/final) - GCM
- [RFC 5116](https://tools.ietf.org/html/rfc5116) - AEAD