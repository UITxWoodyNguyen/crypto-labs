# Lab 3: RSA Hybrid Encryption

## Objective
Implement hybrid encryption combining RSA (asymmetric) with AES (symmetric) for efficient encryption of large messages.

## Topics Covered
- RSA key generation (2048, 3072, 4096-bit)
- RSA encryption/decryption (OAEP padding)
- Hybrid encryption: RSA-KEM + AES-GCM
- Key encapsulation mechanisms (KEM)
- Digital envelope / hybrid encryption pattern

## Dependencies
- **Crypto++** (RSA, AES-GCM, OAEP)
- **OpenSSL** (RSA, EVP APIs for comparison)

## Source Structure (Planned)
```
lab3_rsa_hybrid/
├── CMakeLists.txt
├── src/
│   ├── rsa_hybrid.hpp     # Hybrid encryption interface
│   ├── rsa_hybrid.cpp     # Implementation
│   ├── rsa_keys.hpp       # Key generation/serialization
│   ├── kem.hpp            # Key encapsulation
│   └── main.cpp           # CLI tool
├── tests/
│   └── test_rsa.cpp       # Unit tests
└── kat/
    └── vectors.json       # Test vectors
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
# Generate RSA key pair
./rsa_hybrid_tool keygen --bits 3072 --out-private priv.pem --out-public pub.pem

# Encrypt file (hybrid)
./rsa_hybrid_tool encrypt --pub-key pub.pem --in large_file.dat --out encrypted.bin

# Decrypt file
./rsa_hybrid_tool decrypt --priv-key priv.pem --in encrypted.bin --out decrypted.dat

# Benchmark
./rsa_hybrid_tool bench --bits 2048 --data-size 1048576
```

## Hybrid Encryption Design

```
Sender                                    Receiver
-----                                     --------
Generate random AES-256 key (K)          
Encrypt data with AES-GCM(K)     ──────▶  Decrypt with AES-GCM(K)
Encrypt K with RSA-OAEP(pub)     ──────▶  Decrypt K with RSA-OAEP(priv)
Send: (ciphertext, encrypted_K, iv, tag)
```

## Security Considerations
- Use RSA-OAEP with SHA-256 (not PKCS#1 v1.5)
- AES-GCM for authenticated encryption
- Unique IV per encryption (96-bit for GCM)
- Secure random for key generation
- Constant-time RSA operations where possible

## Cross-Platform Latency Comparison

| Platform | Compiler | RSA Bits | Operation | Time (ms) |
|----------|----------|----------|-----------|-----------|
| Windows | MSVC | 2048 | KeyGen | TBD |
| Windows | MSVC | 2048 | Encapsulate | TBD |
| Windows | MSVC | 2048 | Decapsulate | TBD |
| Windows | GCC | 2048 | KeyGen | TBD |
| Ubuntu | GCC | 2048 | KeyGen | TBD |
| Ubuntu | Clang | 2048 | KeyGen | TBD |

## References
- [RFC 8017 - PKCS#1 v2.2](https://tools.ietf.org/html/rfc8017) (RSAES-OAEP)
- [NIST SP 800-56B](https://csrc.nist.gov/publications/detail/sp/800-56b/rev-2/final) - RSA KEM
- [Crypto++ RSA](https://www.cryptopp.com/wiki/RSA)
- [OpenSSL EVP RSA](https://www.openssl.org/docs/man3.0/man7/EVP_PKEY-RSA.html)