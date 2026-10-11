# Lab 2: Pure AES Implementation

## Objective
Implement AES (Rijndael) from scratch without using Crypto++ or OpenSSL's high-level AES APIs. Use only low-level primitives for learning purposes.

## Topics Covered
- Rijndael key schedule (key expansion)
- SubBytes, ShiftRows, MixColumns, AddRoundKey
- Encryption and decryption rounds
- Block cipher modes implementation (ECB, CBC, CTR)
- Side-channel resistant implementation techniques

## Dependencies
- **OpenSSL** (only for: random bytes, benchmarking comparison, testing)
- No Crypto++

## Source Structure (Planned)
```
lab2_pure_aes/
├── CMakeLists.txt
├── src/
│   ├── aes_pure.hpp       # Pure AES interface
│   ├── aes_pure.cpp       # Implementation
│   ├── aes_tables.hpp     # S-box, inverse S-box, Rcon tables
│   ├── modes.hpp          # Mode implementations
│   └── main.cpp           # CLI tool
├── tests/
│   └── test_aes.cpp       # Unit tests vs NIST vectors
└── kat/
    └── vectors.json       # NIST AES KAT vectors
```

## Building

Same as Lab 1 - each lab builds independently:

```bash
# Windows GCC
mkdir build_windows_gcc && cd build_windows_gcc
cmake .. -G "MinGW Makefiles"
cmake --build .

# Linux GCC
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake ..
cmake --build .
```

## Usage (Planned)

```bash
# Encrypt with pure AES
./aes_pure_tool encrypt --key <hex> --iv <hex> --mode cbc --in file.txt --out file.enc

# Decrypt
./aes_pure_tool decrypt --key <hex> --iv <hex> --mode cbc --in file.enc --out file.dec

# Verify against NIST KAT
./aes_pure_tool --kat kat/vectors.json

# Benchmark vs OpenSSL
./aes_pure_tool bench --compare-openssl
```

## Implementation Requirements

1. **Key Expansion**: Implement Rijndael key schedule for 128/192/256-bit keys
2. **Round Functions**: SubBytes, ShiftRows, MixColumns, AddRoundKey
3. **Modes**: ECB, CBC, CTR (minimum)
4. **Testing**: Pass NIST AES Known Answer Tests (KAT)
5. **Performance**: Within 2x of OpenSSL optimized implementation
6. **Security**: Constant-time S-box access, no data-dependent branches

## Cross-Platform Latency Comparison

| Platform | Compiler | Implementation | Mode | Block Size | Cycles/Byte |
|----------|----------|----------------|------|------------|-------------|
| Windows | MSVC | Pure C++ | CBC | 16KB | TBD |
| Windows | GCC | Pure C++ | CBC | 16KB | TBD |
| Windows | Clang | Pure C++ | CBC | 16KB | TBD |
| Ubuntu | GCC | Pure C++ | CBC | 16KB | TBD |
| Ubuntu | Clang | Pure C++ | CBC | 16KB | TBD |
| All | All | OpenSSL | CBC | 16KB | TBD (baseline) |

## References
- [FIPS 197 - AES Standard](https://csrc.nist.gov/publications/detail/fips/197/final)
- [Rijndael Specification](https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/aes-development/rijndael-ammended.pdf)
- [NIST AES KAT Vectors](https://csrc.nist.gov/Projects/Cryptographic-Algorithm-Validation-Program/Block-Ciphers)
- [Constant-Time AES](https://github.com/veorq/cryptocoding/blob/master/coding-rules.md)