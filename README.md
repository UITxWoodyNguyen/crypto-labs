# Crypto Labs - NT219

A collection of cryptography laboratory exercises for NT219 course. Each lab is designed to be built independently with CMake, supporting cross-platform compilation on Windows (MSVC, Clang, GCC) and Linux/Ubuntu (Clang, GCC).

## Repository Structure

```
crypto-labs/
├── cmake/                    # CMake modules (FindCryptoPP.cmake)
├── lab1/                     # Lab 1: Symmetric Encryption with Crypto++
├── lab2_pure_aes/            # Lab 2: Pure AES Implementation
├── lab3_rsa_hybrid/          # Lab 3: RSA Hybrid Encryption
├── lab4_hash_pki/            # Lab 4: Hash Functions & PKI
├── lab5_digital_signatures/  # Lab 5: Digital Signatures
├── lab6_post_quantum/        # Lab 6: Post-Quantum Cryptography
├── demo/                     # Demo projects
├── demo-1/                   # Demo 1
├── demo-2/                   # Demo 2
└── scripts/                  # Build helper scripts
```

## Prerequisites

### Libraries

**Windows** (`D:\UIT\NT219\libs\`):
- Crypto++: `D:\UIT\NT219\libs\cryptopp\`
  - Headers: `include/`
  - Libraries per compiler: `library/{msvc,gcc,clang}/`
- OpenSSL: `D:\UIT\NT219\libs\openssl\`
  - Per compiler: `{msvc,gcc,clang}/{include,lib,lib64}/`

**Linux/Ubuntu** (`~/Desktop/nt219lib/`):
- Crypto++: `~/Desktop/nt219lib/cryptopp/{gcc,clang}/{include,lib}/`
- OpenSSL: `~/Desktop/nt219lib/openssl/{gcc,clang}/{include,lib64}/`

### Compilers

| Platform | Compilers |
|----------|-----------|
| Windows  | MSVC (Visual Studio), Clang (MSYS2), GCC (MSYS2/mingw64) |
| Linux    | GCC, Clang |

**Windows Note**: Use **MSYS2 mingw64** shell (`C:\msys64\mingw64\bin\`), NOT ucrt64.

## Building Each Lab

Each lab has its own `CMakeLists.txt` and builds independently. Build directories are created inside each lab folder with the naming convention:

```
lab<N>/build_<platform>_<compiler>/
```

### Windows (MSYS2 mingw64 shell)

```bash
# Lab 1 with GCC
cd lab1
mkdir build_windows_gcc && cd build_windows_gcc
cmake .. -G "MinGW Makefiles"
cmake --build .

# Lab 1 with Clang
cd lab1
mkdir build_windows_clang && cd build_windows_clang
cmake .. -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=clang++
cmake --build .

# Lab 1 with MSVC (Developer Command Prompt / PowerShell)
cd lab1
mkdir build_windows_msvc && cd build_windows_msvc
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Linux (Ubuntu/Kali)

```bash
# Lab 1 with GCC
cd lab1
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake ..
cmake --build .

# Lab 1 with Clang
cd lab1
mkdir build_ubuntu_clang && cd build_ubuntu_clang
cmake .. -DCMAKE_CXX_COMPILER=clang++
cmake --build .
```

### Build Options

```bash
# Enable tests (default ON)
cmake .. -DBUILD_TESTS=ON

# Disable tests
cmake .. -DBUILD_TESTS=OFF

# Set build type (Release/Debug)
cmake .. -DCMAKE_BUILD_TYPE=Release
```

## Lab Overview

| Lab | Topic | Key Libraries | Status |
|-----|-------|---------------|--------|
| 1 | Symmetric Encryption (AES) | Crypto++, OpenSSL | ✅ Implemented |
| 2 | Pure AES Implementation | OpenSSL | 🚧 Template |
| 3 | RSA Hybrid Encryption | Crypto++, OpenSSL | 🚧 Template |
| 4 | Hash Functions & PKI | Crypto++, OpenSSL | 🚧 Template |
| 5 | Digital Signatures | Crypto++, OpenSSL | 🚧 Template |
| 6 | Post-Quantum Crypto | OpenSSL 3.x+ (OQS) | 🚧 Template |

## Running Tests

```bash
# After building with BUILD_TESTS=ON
cd build_<platform>_<compiler>
ctest --output-on-failure
```

## Benchmarking & Latency Comparison

Each lab executable supports timing measurements. For cross-platform latency comparison:

1. Build the same lab on both Windows and Linux with equivalent compilers
2. Run the executable with benchmark flags (implementation-specific)
3. Record results in a comparison report

Example report structure:
```
| Operation | Windows (MSVC) | Windows (GCC) | Ubuntu (GCC) | Ubuntu (Clang) |
|-----------|----------------|---------------|--------------|----------------|
| AES-256 Encrypt | X ms | Y ms | Z ms | W ms |
| RSA-2048 Sign | ... | ... | ... | ... |
```

## IDE Support

### VS Code / VS Code Insiders
- Install **C/C++ Extension** and **CMake Tools**
- Open folder `crypto-labs/lab1` (or any lab folder)
- CMake Tools will auto-detect `CMakeLists.txt`
- Select kit: GCC (mingw64), Clang, or MSVC

### CLion
- Open `crypto-labs/lab1` as project
- Configure toolchains for each compiler
- Build and run from IDE

## Troubleshooting

### "Crypto++ not found"
- Verify `CRYPTOPP_ROOT_DIR` points to correct installation
- Windows: `D:/UIT/NT219/libs/cryptopp`
- Linux: `~/Desktop/nt219lib/cryptopp`
- Ensure compiler-specific library exists (e.g., `library/gcc/libcryptopp.a`)

### "OpenSSL not found"
- Verify `OPENSSL_ROOT_DIR` for your compiler
- Windows: `D:/UIT/NT219/libs/openssl/{msvc,gcc,clang}`
- Linux: `~/Desktop/nt219lib/openssl/{gcc,clang}`
- OpenSSL must have CMake config files in `lib/cmake/OpenSSL` or `lib64/cmake/OpenSSL`

### MSYS2 mingw64 vs ucrt64
- **Use mingw64**: `C:\msys64\mingw64\bin\gcc.exe`
- **Avoid ucrt64**: `C:\msys64\ucrt64\bin\gcc.exe` (different ABI)

## License

Educational use for NT219 course.