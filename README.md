# Cryptography & Applications — Laboratory Series (Labs 1–6)

A cross-platform suite of modern cryptographic primitives, command-line utilities, test suites, and benchmarking tools built with Modern C++ (C++17), Crypto++, and OpenSSL.

---

## 1. Overview & Project Structure

This repository implements all six laboratory projects specified in the *Cryptography & Applications* laboratory syllabus:

* **Lab 1: Symmetric Encryption with Crypto++** (`aestool`): AES in ECB, CBC, OFB, CFB, CTR, XTS, CCM, and GCM modes; AEAD handling, secure IV generation, and misuse mitigation.
* **Lab 2: Pure C++ AES-128 Implementation** (`aestool_pure`): FIPS-197 compliant manual round transformations and NIST SP 800-38A CTR mode without external cryptographic dependencies.
* **Lab 3: RSA-OAEP & Hybrid Encryption** (`rsatool`): RSA keypair generation (>=3072 bits), OAEP padding (SHA-256), and hybrid envelope encryption combining AES-256-GCM with RSA-OAEP.
* **Lab 4: Hashing, PKI & Vulnerability Demonstrations** (`hashtool`, `certtool`): SHA-2, SHA-3, SHAKE streaming implementations, X.509 certificate parsing/validation, offline MD5 collision demonstration, and length-extension attack PoC.
* **Lab 5: Classical Digital Signatures** (`sigtool`): Deterministic ECDSA (P-256/P-384 per RFC 6979) and probabilistic RSA-PSS (>=3072 bits, SHA-256).
* **Lab 6: Post-Quantum Signatures & Key Encapsulation** (`pqtool`): NIST FIPS 204 (ML-DSA-44/65) signatures, FIPS 203 (ML-KEM-512) encapsulation/decapsulation, and experimental Post-Quantum X.509-like certificates.

### Directory Layout

```text
crypto-labs/
├── CMakeLists.txt                # Root CMake configuration
├── README.md                     # Project documentation & build instructions
├── docs/                         # Technical reports and benchmark plots
│   ├── Cryptography_Labs_Report.pdf
│   └── figures/
├── cmake/                        # Custom CMake modules (FindCryptoPP.cmake, etc.)
├── common/                       # Shared CLI parsing, encoding (Hex/Base64), and benchmark helpers
├── lab1_symmetric_cryptopp/      # Lab 1: aestool
├── lab2_pure_aes/                # Lab 2: aestool_pure (Zero external crypto libs)
├── lab3_rsa_hybrid/              # Lab 3: rsatool
├── lab4_hash_pki/                # Lab 4: hashtool, certtool, attack demonstrations
│   └── attacks/                  # MD5 collision files & length-extension script
├── lab5_digital_signatures/      # Lab 5: sigtool
├── lab6_post_quantum/            # Lab 6: pqtool
├── third_party/                  # Prebuilt libraries / Headers (Crypto++, Catch2, JSON)
└── scripts/                      # Automated build & benchmark runner scripts

```

---

## 2. System Requirements & Dependencies

### Target Compilers & Platforms

* **Windows 10/11**:
* MinGW-w64 (GCC 11.0+ / MSYS2 UCRT64 or MINGW64)
* Visual Studio 2022 (MSVC v143+)


* **Linux**:
* Ubuntu 22.04 / 24.04 LTS (GCC 11+ or Clang 14+)


* **Build System**: CMake 3.20 or newer

### External Dependencies & Libraries

| Dependency | Required Version | Labs Used | Role |
| --- | --- | --- | --- |
| **Crypto++** | >= 8.9.0 | Lab 1, 3, 4, 5 | Symmetric ciphers, RNG, RSA, Hash primitives |
| **OpenSSL** | >= 3.0.0 | Lab 3, 4, 5, 6 | X.509 parsing, PKI validation, PQC / Evp APIs |
| **nlohmann/json** | >= 3.11.0 | All Labs | KAT parsing, sidecar metadata generation |
| **Catch2** | v3.x | All Labs | Unit testing & automated test runners |

---

## 3. Installation & Environment Setup

### A. Windows (via MSYS2 MinGW 64-bit)

1. Open **MSYS2 MinGW 64-bit terminal** (`mingw64.exe`).
2. Install base development tools and OpenSSL:
```bash
pacman -Syu
pacman -S --needed base-devel mingw-w64-x86_64-toolchain \
                   mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-ninja \
                   mingw-w64-x86_64-openssl

```


3. *(Optional)* Build and install Crypto++ into MinGW64 if not using prebuilts:
```bash
git clone [https://github.com/weidai11/cryptopp.git](https://github.com/weidai11/cryptopp.git) /tmp/cryptopp
cd /tmp/cryptopp
make -j$(nproc) CXX="/mingw64/bin/g++"
mkdir -p /mingw64/include/cryptopp
cp *.h /mingw64/include/cryptopp/
cp libcryptopp.a /mingw64/lib/

```



### B. Linux (Ubuntu 22.04/24.04 LTS)

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
                    libcrypto++-dev libcrypto++-doc libcrypto++-utils \
                    libssl-dev nlohmann-json3-dev

```

---

## 4. Build Instructions

Out-of-source builds are strictly enforced.

### Building on Windows (MSYS2 MinGW)

```bash
# 1. Clone repository
git clone <repo-url> crypto-labs
cd crypto-labs

# 2. Configure project
cmake -B build -G "Ninja" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=ON \
      -DBUILD_BENCHMARKS=ON

# 3. Compile all targets
cmake --build build -j$(nproc)

```

### Building on Windows (MSVC)

```cmd
cd crypto-labs
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

```

### Building on Linux (Ubuntu)

```bash
cd crypto-labs
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

```

### Artifacts & Executables

Compiled binaries will be generated in `build/bin/`:

* `aestool` (Lab 1)
* `aestool_pure` (Lab 2)
* `rsatool` (Lab 3)
* `hashtool`, `certtool` (Lab 4)
* `sigtool` (Lab 5)
* `pqtool` (Lab 6)

---

## 5. Unified Command-Line Interface (CLI) Usage

All binaries follow standard Unix-style conventions:

* Input: `--in <file>` (binary-safe) or `--text "<string>"` (UTF-8)
* Output: `--out <file>` (binary) or terminal standard output (Hex / Base64)
* Encodings: `--encode hex|base64|raw` (Default: `hex` for screen, `raw` for file)
* Fail-Closed Policy: Malformed arguments, authentication tag mismatches, or file corruptions cause immediate termination with non-zero exit codes.

### Lab 1: `aestool` (Crypto++)

```bash
# Encryption using AES-256-GCM (auto-generates IV, outputs sidecar metadata)
./aestool encrypt --mode gcm --key-hex 000102...1f --in plaintext.txt --out ct.bin

# Decryption with sidecar verification (fails closed if tampered)
./aestool decrypt --mode gcm --key-hex 000102...1f --in ct.bin --out recovered.txt

# Run Known Answer Tests (NIST SP 800-38A / SP 800-38D)
./aestool --kat ../lab1_symmetric_cryptopp/tests/vectors/nist_aes_kat.json

```

### Lab 2: `aestool_pure` (Pure C++ AES-128-CTR)

```bash
# Encrypt arbitrary file with manual CTR mode
./aestool_pure encrypt --mode ctr --key key16.bin --iv iv16.bin --in data.bin --out data.enc

# Decrypt and verify identical round-trip
./aestool_pure decrypt --mode ctr --key key16.bin --iv iv16.bin --in data.enc --out data.dec

```

### Lab 3: `rsatool` (RSA-OAEP & Hybrid Mode)

```bash
# Generate 3072-bit RSA Keypair
./rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem

# Encrypt (Auto-switches to AES-GCM envelope if file > RSA limit)
./rsatool encrypt --in document.pdf --pub pub.pem --out envelope.bin

# Decrypt envelope
./rsatool decrypt --in envelope.bin --priv priv.pem --out decrypted.pdf

```

### Lab 4: `hashtool` & `certtool`

```bash
# Streamed hashing for multi-GB files (SHA-256 / SHA3-512)
./hashtool --algo sha256 --in large_disk.iso --stream

# SHAKE256 XOF with custom output length
./hashtool --algo shake256 --outlen 64 --in input.bin

# Parse X.509 Certificate and verify signature chain
./certtool parse --in certificate.pem --verify --ca ca_root.pem

```

### Lab 5: `sigtool` (ECDSA & RSA-PSS)

```bash
# Generate ECDSA P-256 keypair
./sigtool keygen --algo ecdsa-p256 --pub ec_pub.pem --priv ec_priv.pem

# Sign binary file (Deterministic RFC 6979)
./sigtool sign --algo ecdsa-p256 --in document.docx --priv ec_priv.pem --out doc.sig

# Verify detached signature
./sigtool verify --algo ecdsa-p256 --in document.docx --sig doc.sig --pub ec_pub.pem

```

### Lab 6: `pqtool` (ML-DSA & ML-KEM)

```bash
# Keygen for Post-Quantum Signature (ML-DSA-44)
./pqtool keygen --algo mldsa-44 --pub pq_pub.pem --priv pq_priv.pem

# Key Encapsulation (ML-KEM-512)
./pqtool encaps --algo mlkem-512 --pub kem_pub.pem --ct kem.ct --ss shared.bin

# Key Decapsulation
./pqtool decaps --algo mlkem-512 --priv kem_priv.pem --ct kem.ct --ss recovered_ss.bin

```

---

## 6. Testing & Validation

All test targets are integrated with CMake's test runner `ctest`.

```bash
cd build
# Run all automated tests with verbose output
ctest --output-on-failure -V

```

### Test Coverage Highlights

* **Known Answer Tests (KATs)**: NIST test vectors parsed from JSON format for AES, SHA, RSA-OAEP, and PQC primitives.
* **Negative Security Tests**:
* Wrong key / corrupted IV rejection.
* Ciphertext tampering / Bit-flip AEAD tag verification failure.
* Nonce reuse detection in CTR, GCM, and CCM modes.
* Counter overflow handling.



---

## 7. Performance & Benchmarking Methodology

Benchmarks execute across payload sizes (1 KiB, 4 KiB, 16 KiB, 256 KiB, 1 MiB, 8 MiB, 100 MiB) following strict statistical evaluation:

* **Warm-up**: 1–2 seconds cache stabilization.
* **Execution**: $N \ge 30$ independent runs per block.
* **Collected Metrics**: Mean, Median, Standard Deviation, and 95% Confidence Intervals ($CI = \bar{x} \pm 1.96 \cdot \frac{s}{\sqrt{N}}$).

To run the automated benchmark runner:

```bash
python3 scripts/run_benchmarks.py --build-dir ./build --output-dir ./docs/figures

```

---

## 8. Security Hygiene & Design Decisions

1. **Cryptographically Secure Pseudorandomness**: All IVs, salts, and ephemeral keys are generated via Crypto++ `AutoSeededRandomPool` or OpenSSL `RAND_bytes`. Use of `rand()` or `std::default_random_engine` is strictly prohibited.
2. **ECB Protection**: `aestool` prints security warnings when ECB is specified and enforces a 16 KiB file limit unless explicitly bypassed with `--allow-ecb`.
3. **Fail-Closed Execution**: Any MAC mismatch, signature verification failure, or decoding discrepancy immediately truncates intermediate buffers and returns an error state.
4. **Sidecar Headers**: Nonces and authentication tags are decoupled using structured JSON headers, preserving raw binary file integrity.

---

## 9. Known Limitations

* **Lab 2 AES-NI**: Pure C++ AES implementation defaults to software S-Box; AES-NI hardware acceleration requires explicit compiler flag `-maes`.
* **Cross-OS PQC**: ML-DSA and ML-KEM implementations rely on OpenSSL 3.2+ PQC backends; systems running OpenSSL 3.0 LTS use fallback reference C implementations in `third_party/`.

---

## 10. Academic Integrity & Ethics Statement

* This laboratory series was developed solely for educational and defensive cybersecurity analysis.
* All attack demonstrations (e.g., MD5 collision generation and length-extension manipulations) were conducted in an isolated, offline sandbox environment using synthetic test files.
* No testing was performed against third-party production infrastructure.
