# Lab 4: Hash Functions & PKI

## Objective
Implement and analyze cryptographic hash functions, build a simple PKI with certificate generation, signing, and verification.

## Topics Covered
- Hash functions: SHA-2 (224/256/384/512), SHA-3 (224/256/384/512), BLAKE2
- HMAC construction
- Key derivation functions (PBKDF2, HKDF, Argon2)
- X.509 certificate structure
- Certificate signing requests (CSR)
- Certificate Authority (CA) operations
- Certificate chain validation
- OCSP/CRL basics

## Dependencies
- **Crypto++** (Hashes, HMAC, PBKDF2, HKDF, X.509)
- **OpenSSL** (X.509, EVP_MD, PKI utilities for comparison)

## Source Structure (Planned)
```
lab4_hash_pki/
├── CMakeLists.txt
├── src/
│   ├── hash_demo.hpp      # Hash function demonstrations
│   ├── hash_demo.cpp      # SHA-2, SHA-3, BLAKE2, HMAC
│   ├── kdf_demo.hpp       # KDF demonstrations
│   ├── kdf_demo.cpp       # PBKDF2, HKDF, Argon2
│   ├── pki.hpp            # PKI operations
│   ├── pki.cpp            # Cert gen, sign, verify
│   └── main.cpp           # CLI tool
├── tests/
│   ├── test_hash.cpp      # Hash/KDF tests
│   └── test_pki.cpp       # PKI tests
└── kat/
    ├── hash_vectors.json  # NIST hash KAT
    └── pki_vectors.json   # PKI test vectors
```

## Building

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
# Hash demonstrations
./hash_pki_tool hash --algo sha256 --in file.txt
./hash_pki_tool hmac --algo sha256 --key <hex> --in file.txt

# KDF demonstrations
./hash_pki_tool kdf --algo pbkdf2 --password "secret" --salt <hex> --iter 100000 --len 32
./hash_pki_tool kdf --algo hkdf --ikm <hex> --salt <hex> --info "context" --len 32

# PKI operations
# Generate CA
./hash_pki_tool pki ca-gen --out-ca ca.pem --out-key ca_key.pem --days 3650

# Generate CSR
./hash_pki_tool pki csr-gen --key key.pem --out csr.pem --cn "example.com"

# Sign certificate (CA)
./hash_pki_tool pki sign --ca ca.pem --ca-key ca_key.pem --csr csr.pem --out cert.pem --days 365

# Verify certificate chain
./hash_pki_tool pki verify --ca ca.pem --cert cert.pem

# Full chain: CA -> Intermediate -> Leaf
./hash_pki_tool pki chain-gen --out-dir ./pki_output
```

## Hash Functions Comparison

| Algorithm | Output Size | Speed (cycles/byte) | Security Level |
|-----------|-------------|---------------------|----------------|
| SHA-224 | 224 bits | ~8 | 112 bits |
| SHA-256 | 256 bits | ~8 | 128 bits |
| SHA-384 | 384 bits | ~12 | 192 bits |
| SHA-512 | 512 bits | ~12 | 256 bits |
| SHA3-224 | 224 bits | ~15 | 112 bits |
| SHA3-256 | 256 bits | ~15 | 128 bits |
| SHA3-384 | 384 bits | ~20 | 192 bits |
| SHA3-512 | 512 bits | ~20 | 256 bits |
| BLAKE2s | 256 bits | ~5 | 128 bits |
| BLAKE2b | 512 bits | ~3 | 256 bits |

## Cross-Platform Latency Comparison

| Platform | Compiler | Algorithm | Data Size | Time (μs) |
|----------|----------|-----------|-----------|-----------|
| Windows | MSVC | SHA-256 | 1 MB | TBD |
| Windows | GCC | SHA-256 | 1 MB | TBD |
| Ubuntu | GCC | SHA-256 | 1 MB | TBD |
| Ubuntu | Clang | SHA-256 | 1 MB | TBD |
| Windows | MSVC | SHA3-256 | 1 MB | TBD |
| Windows | MSVC | BLAKE2b | 1 MB | TBD |
| Windows | MSVC | PBKDF2 (100k) | 32 bytes | TBD |
| Windows | MSVC | Argon2id | 32 bytes | TBD |

## References
- [FIPS 180-4 - SHA](https://csrc.nist.gov/publications/detail/fips/180/4/final)
- [FIPS 202 - SHA-3](https://csrc.nist.gov/publications/detail/fips/202/final)
- [RFC 7693 - BLAKE2](https://tools.ietf.org/html/rfc7693)
- [RFC 8018 - PBKDF2](https://tools.ietf.org/html/rfc8018)
- [RFC 5869 - HKDF](https://tools.ietf.org/html/rfc5869)
- [RFC 9106 - Argon2](https://tools.ietf.org/html/rfc9106)
- [RFC 5280 - X.509 PKI](https://tools.ietf.org/html/rfc5280)
- [Crypto++ X.509](https://www.cryptopp.com/wiki/X.509)