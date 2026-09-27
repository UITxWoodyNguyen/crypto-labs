# Demo-2: RSA Public-Key Cryptography (Crypto++)

Demonstrates RSA-2048 key generation, OAEP encryption/decryption, PKCS#1 v1.5 digital signatures with SHA-256, and DER key export/import.

## Prerequisites

- **MSYS2 MinGW 64-bit Terminal** (`mingw64.exe`, NOT `ucrt64.exe`)
- Crypto++ library: `pacman -S mingw-w64-x86_64-cryptopp`
- CMake 3.20+, Ninja

## Build & Run Instructions

**Open `mingw64.exe` (MSYS2 MinGW 64-bit) and run:**

```bash
cd /d/UIT/NT219/crypto-labs/demo-2

# Configure
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build -j$(nproc)

# Run
./build/demo2_app.exe
```

> ⚠️ **Important**: Use `mingw64.exe` terminal, NOT `ucrt64.exe` or PowerShell. The Crypto++ package is installed for mingw64 toolchain.

## Expected Output

```
=== CRYPTO++ RSA DEMO (Encryption + Digital Signature) ===

[*] 1. Generating RSA-2048 key pair...
[+] Key pair generated successfully
    Modulus size: 256 bytes (2048 bits)

[*] 2. RSA-OAEP Encryption/Decryption (SHA-256)...
[+] Plaintext: Secret message for RSA encryption!
[+] Ciphertext: 8F3A2B... (256 bytes hex)
[+] Recovered: Secret message for RSA encryption!
[+] Encryption/Decryption: SUCCESS

[*] 3. RSA-PKCS#1 v1.5 Digital Signature (SHA-256)...
[+] Message: Important document to sign
[+] Signature: A1B2C3... (256 bytes hex)
[+] Signature verification: SUCCESS

[*] 4. Testing tampered message detection...
[+] Tampered message correctly rejected: SUCCESS

[*] 5. Key Export/Import (DER format)...
[+] Public Key (DER): 30820122...
[+] Private Key (DER): 308204A0...
[+] Key Export/Import: SUCCESS

=========================================================
>>> ALL TESTS PASSED: RSA Demo completed successfully! <<<
=========================================================
```

## Features Demonstrated

| Feature | Algorithm | Details |
|---------|-----------|---------|
| Key Generation | RSA | 2048-bit |
| Encryption | RSAES-OAEP | SHA-256, MGF1 |
| Signatures | RSASSA-PKCS1v15 | SHA-256 |
| Key Format | DER (ASN.1) | PKCS#8 (private), X.509 (public) |

## Troubleshooting

**Crypto++ not found:**
```bash
pacman -S mingw-w64-x86_64-cryptopp
```

**Wrong compiler (ucrt64) detected:** Ensure you're running from `mingw64.exe` terminal, not PowerShell or `ucrt64.exe`.

**Linker errors:** Ensure Crypto++ is the same MinGW build (MSYS2 package handles this).