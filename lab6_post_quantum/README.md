# Lab 6: Post-Quantum Cryptography

## Objective
Explore and benchmark post-quantum cryptographic algorithms (KEM and signatures) using OpenSSL 3.x with OQS provider or liboqs.

## Topics Covered
- **KEM (Key Encapsulation Mechanisms)**:
  - ML-KEM (FIPS 203) - Kyber: ML-KEM-512, ML-KEM-768, ML-KEM-1024
  - Classic McEliece
  - HQC
- **Digital Signatures**:
  - ML-DSA (FIPS 204) - Dilithium: ML-DSA-44, ML-DSA-65, ML-DSA-87
  - SLH-DSA (FIPS 205) - SPHINCS+: SLH-DSA-SHA2-128s/f, SLH-DSA-SHA2-192s/f, SLH-DSA-SHA2-256s/f
  - FALCON (FN-DSA): FALCON-512, FALCON-1024
- Hybrid schemes (Classical + PQC)
- Migration strategies
- Performance benchmarking

## Dependencies
- **OpenSSL 3.x+** with **OQS Provider** (or **liboqs** directly)
- **Optional**: Crypto++ for classical crypto comparison

### Installing OQS Provider

**Windows (MSYS2)**:
```bash
pacman -S mingw-w64-x86_64-openssl mingw-w64-x86_64-liboqs mingw-w64-x86_64-oqs-provider
```

**Ubuntu**:
```bash
# Build from source or use PPA
sudo apt install libssl-dev liboqs-dev oqs-provider
```

## Source Structure (Planned)
```
lab6_post_quantum/
├── CMakeLists.txt
├── src/
│   ├── pqc_kem.hpp        # KEM interface
│   ├── pqc_kem.cpp        # ML-KEM, McEliece, HQC
│   ├── pqc_sig.hpp        # Signature interface
│   ├── pqc_sig.cpp        # ML-DSA, SLH-DSA, FALCON
│   ├── hybrid.hpp         # Hybrid classical+PQC
│   ├── hybrid.cpp         # X25519+ML-KEM, etc.
│   └── main.cpp           # CLI tool
├── tests/
│   ├── test_kem.cpp       # KEM tests
│   └── test_sig.cpp       # Signature tests
└── kat/
    ├── kem_vectors.json   # NIST PQC KAT
    └── sig_vectors.json
```

## Building

```bash
# Windows GCC (with OQS provider in path)
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
# List available PQC algorithms
./pqc_tool list

# KEM Operations
./pqc_tool kem keygen --algo ml-kem-768 --out-pub pub.pem --out-priv priv.pem
./pqc_tool kem encapsulate --algo ml-kem-768 --pub-key pub.pem --out-ct ct.bin --out-ss ss.bin
./pqc_tool kem decapsulate --algo ml-kem-768 --priv-key priv.pem --ct ct.bin --out-ss ss.bin

# Signature Operations
./pqc_tool sig keygen --algo ml-dsa-65 --out-pub pub.pem --out-priv priv.pem
./pqc_tool sig sign --algo ml-dsa-65 --priv-key priv.pem --in doc.pdf --out sig.bin
./pqc_tool sig verify --algo ml-dsa-65 --pub-key pub.pem --in doc.pdf --sig sig.bin

# Hybrid KEM (Classical + PQC)
./pqc_tool hybrid kem keygen --classical x25519 --pqc ml-kem-768 --out-pub pub.pem --out-priv priv.pem
./pqc_tool hybrid kem encapsulate --pub-key pub.pem --out-ct ct.bin --out-ss ss.bin

# Benchmark
./pqc_tool bench --algo ml-kem-768 --iterations 100
./pqc_tool bench --algo ml-dsa-65 --iterations 100
./pqc_tool bench --all --iterations 50
```

## Algorithm Parameters (NIST PQC Standards)

### ML-KEM (Kyber) - FIPS 203

| Parameter Set | Public Key | Ciphertext | Shared Secret | Security | NIST Level |
|---------------|------------|------------|---------------|----------|------------|
| ML-KEM-512 | 800 B | 768 B | 32 B | 128-bit | 1 |
| ML-KEM-768 | 1184 B | 1088 B | 32 B | 192-bit | 3 |
| ML-KEM-1024 | 1568 B | 1568 B | 32 B | 256-bit | 5 |

### ML-DSA (Dilithium) - FIPS 204

| Parameter Set | Public Key | Signature | Security | NIST Level |
|---------------|------------|-----------|----------|------------|
| ML-DSA-44 | 1312 B | 2420 B | 128-bit | 2 |
| ML-DSA-65 | 1952 B | 3309 B | 192-bit | 3 |
| ML-DSA-87 | 2592 B | 4627 B | 256-bit | 5 |

### SLH-DSA (SPHINCS+) - FIPS 205

| Parameter Set | Public Key | Signature | Security | Type |
|---------------|------------|-----------|----------|------|
| SLH-DSA-SHA2-128s | 32 B | 7856 B | 128-bit | Small |
| SLH-DSA-SHA2-128f | 32 B | 17088 B | 128-bit | Fast |
| SLH-DSA-SHA2-192s | 48 B | 16224 B | 192-bit | Small |
| SLH-DSA-SHA2-192f | 48 B | 35664 B | 192-bit | Fast |
| SLH-DSA-SHA2-256s | 64 B | 29792 B | 256-bit | Small |
| SLH-DSA-SHA2-256f | 64 B | 49856 B | 256-bit | Fast |

## Cross-Platform Latency Comparison

| Platform | Compiler | Algorithm | KeyGen (ms) | Encaps/Sign (ms) | Decaps/Verify (ms) |
|----------|----------|-----------|-------------|------------------|-------------------|
| Windows | MSVC | ML-KEM-768 | TBD | TBD | TBD |
| Windows | GCC | ML-KEM-768 | TBD | TBD | TBD |
| Ubuntu | GCC | ML-KEM-768 | TBD | TBD | TBD |
| Windows | MSVC | ML-DSA-65 | TBD | TBD | TBD |
| Windows | GCC | ML-DSA-65 | TBD | TBD | TBD |
| Ubuntu | Clang | ML-DSA-65 | TBD | TBD | TBD |
| Windows | MSVC | SLH-DSA-128f | TBD | TBD | TBD |
| Windows | MSVC | X25519+ML-KEM-768 | TBD | TBD | TBD |

## Migration Considerations
1. **Hybrid Approach**: Combine classical (X25519, ECDSA) + PQC during transition
2. **Key Sizes**: PQC keys/signatures are significantly larger - plan for MTU, storage
3. **Performance**: PQC is generally slower than classical - benchmark your use case
4. **Standards**: Follow NIST FIPS 203/204/205 and IETF hybrid drafts
5. **Testing**: Use NIST ACVP for validation

## References
- [NIST PQC Standardization](https://csrc.nist.gov/Projects/post-quantum-cryptography)
- [FIPS 203 - ML-KEM](https://csrc.nist.gov/pubs/fips/203/final)
- [FIPS 204 - ML-DSA](https://csrc.nist.gov/pubs/fips/204/final)
- [FIPS 205 - SLH-DSA](https://csrc.nist.gov/pubs/fips/205/final)
- [OQS Provider](https://github.com/open-quantum-safe/oqs-provider)
- [liboqs](https://github.com/open-quantum-safe/liboqs)
- [IETF Hybrid KEM Draft](https://datatracker.ietf.org/doc/draft-ietf-tls-hybrid-design/)
- [OpenSSL 3 Providers](https://www.openssl.org/docs/man3.0/man7/OSSL_PROVIDER.html)