# Lab 5: Digital Signatures

## Objective
Implement and analyze digital signature algorithms: RSA-PSS, ECDSA, EdDSA. Compare performance and security properties.

## Topics Covered
- RSA-PSS (Probabilistic Signature Scheme)
- ECDSA (Elliptic Curve DSA) - P-256, P-384, P-521
- EdDSA (Edwards-curve DSA) - Ed25519, Ed448
- Signature generation and verification
- Key generation and serialization
- Batch verification
- Signature formats (ASN.1/DER, raw)

## Dependencies
- **Crypto++** (RSA-PSS, ECDSA, EdDSA)
- **OpenSSL** (EVP_PKEY API for all algorithms)

## Source Structure (Planned)
```
lab5_digital_signatures/
├── CMakeLists.txt
├── src/
│   ├── signatures.hpp     # Unified signature interface
│   ├── rsa_pss.cpp        # RSA-PSS implementation
│   ├── ecdsa.cpp          # ECDSA implementation
│   ├── eddsa.cpp          # EdDSA implementation
│   ├── key_management.hpp # Key gen, serialization
│   └── main.cpp           # CLI tool
├── tests/
│   ├── test_rsa_pss.cpp   # RSA-PSS tests
│   ├── test_ecdsa.cpp     # ECDSA tests
│   └── test_eddsa.cpp     # EdDSA tests
└── kat/
    ├── rsa_pss_vectors.json
    ├── ecdsa_vectors.json
    └── eddsa_vectors.json
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
# Generate keys
./dsig_tool keygen --algo rsa-pss --bits 3072 --out-priv priv.pem --out-pub pub.pem
./dsig_tool keygen --algo ecdsa --curve p256 --out-priv priv.pem --out-pub pub.pem
./dsig_tool keygen --algo eddsa --curve ed25519 --out-priv priv.pem --out-pub pub.pem

# Sign
./dsig_tool sign --algo rsa-pss --priv-key priv.pem --in document.pdf --out sig.bin
./dsig_tool sign --algo ecdsa --priv-key priv.pem --in document.pdf --out sig.bin
./dsig_tool sign --algo eddsa --priv-key priv.pem --in document.pdf --out sig.bin

# Verify
./dsig_tool verify --algo rsa-pss --pub-key pub.pem --in document.pdf --sig sig.bin
./dsig_tool verify --algo ecdsa --pub-key pub.pem --in document.pdf --sig sig.bin
./dsig_tool verify --algo eddsa --pub-key pub.pem --in document.pdf --sig sig.bin

# Batch verify (EdDSA)
./dsig_tool batch-verify --algo eddsa --pub-key pub.pem --inputs files.txt --sigs sigs/

# Benchmark
./dsig_tool bench --algo rsa-pss --bits 3072 --iterations 1000
./dsig_tool bench --algo ecdsa --curve p256 --iterations 1000
./dsig_tool bench --algo eddsa --curve ed25519 --iterations 1000
```

## Algorithm Comparison

| Algorithm | Key Size | Sig Size | Security | Speed (sign) | Speed (verify) | Notes |
|-----------|----------|----------|----------|--------------|----------------|-------|
| RSA-PSS | 3072 bits | 384 bytes | 128-bit | Slow | Fast | Widely supported |
| RSA-PSS | 4096 bits | 512 bytes | 150-bit | Slower | Fast | Higher security |
| ECDSA P-256 | 256 bits | 64 bytes | 128-bit | Fast | Medium | Requires good RNG |
| ECDSA P-384 | 384 bits | 96 bytes | 192-bit | Medium | Medium | NIST Suite B |
| ECDSA P-521 | 521 bits | 132 bytes | 256-bit | Slow | Slow | |
| Ed25519 | 256 bits | 64 bytes | 128-bit | Very Fast | Very Fast | Deterministic, no RNG needed |
| Ed448 | 448 bits | 114 bytes | 224-bit | Fast | Fast | Higher security |

## Security Notes
- **ECDSA**: Requires cryptographically secure random nonce (k) for each signature. Reuse of k leaks private key.
- **EdDSA**: Deterministic - derives nonce from private key + message. No RNG needed for signing.
- **RSA-PSS**: Probabilistic - uses salt for randomized signatures. More secure than PKCS#1 v1.5.

## Cross-Platform Latency Comparison

| Platform | Compiler | Algorithm | Sign (μs) | Verify (μs) | KeyGen (ms) |
|----------|----------|-----------|-----------|-------------|-------------|
| Windows | MSVC | RSA-PSS-3072 | TBD | TBD | TBD |
| Windows | GCC | RSA-PSS-3072 | TBD | TBD | TBD |
| Ubuntu | GCC | RSA-PSS-3072 | TBD | TBD | TBD |
| Windows | MSVC | ECDSA-P256 | TBD | TBD | TBD |
| Windows | MSVC | Ed25519 | TBD | TBD | TBD |
| Ubuntu | Clang | Ed25519 | TBD | TBD | TBD |

## References
- [RFC 8017 - RSA-PSS](https://tools.ietf.org/html/rfc8017)
- [FIPS 186-5 - DSS](https://csrc.nist.gov/publications/detail/fips/186/5/final) (ECDSA)
- [RFC 8032 - EdDSA](https://tools.ietf.org/html/rfc8032)
- [Crypto++ Digital Signatures](https://www.cryptopp.com/wiki/Digital_Signatures)
- [OpenSSL EVP Sign/Verify](https://www.openssl.org/docs/man3.0/man3/EVP_DigestSignInit.html)