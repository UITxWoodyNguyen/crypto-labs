# Lab 1: Symmetric Encryption with Crypto++

## 1. Overview & Objectives

Implement and evaluate AES symmetric encryption and decryption using the **Crypto++ (8.9+)** library across 8 operating modes (ECB, CBC, CFB, OFB, CTR, XTS, CCM, GCM). The project enforces rigorous cryptographic security hygiene (fail-closed architecture, nonce-reuse prevention, IV length validation, ECB usage restriction), validates mathematically against NIST Known Answer Tests (KAT), and conducts comprehensive cross-platform performance benchmarking between Windows and Linux (Ubuntu).

### Topics Covered
- **AES block cipher** with 128, 192, and 256-bit key sizes (plus 512-bit for XTS dual-key).
- **Block & Stream cipher modes**: ECB, CBC, CFB, OFB, CTR, XTS.
- **Authenticated Encryption with Associated Data (AEAD)**: GCM (Galois/Counter Mode) and CCM (Counter with CBC-MAC).
- **Security Misuse Resistance**: Nonce registry tracking with SHA-256, ECB size thresholds (>16 KiB blocked), and fail-closed error handling.
- **Verification & Testing**: NIST SP 800-38A, SP 800-38C, GCM, and IEEE 1619 KAT vectors, plus negative test matrices.
- **Performance Benchmarking**: Latency, throughput, confidence intervals (CI 95%), and linear cost models for tag overhead.

---

## 2. Supported Modes

| Mode | Type | IV / Nonce Length | Auth Tag Length | Security Status & Use Case |
| :--- | :---: | :---: | :---: | :--- |
| **ECB** | Block | None (0 B) | None | **Insecure** (leaks patterns); blocked for inputs > 16 KiB without `--allow-ecb` |
| **CBC** | Block | 16 bytes | None | Standard legacy block mode; requires PKCS#7 padding |
| **CFB** | Stream | 16 bytes | None | Self-synchronizing stream mode (CFB-128) |
| **OFB** | Stream | 16 bytes | None | Synchronous keystream generation; error non-propagating |
| **CTR** | Stream | 16 bytes | None | Counter mode; highly parallelizable; **catastrophic if nonce reused** |
| **XTS** | Storage | 16 bytes (Tweak) | None | IEEE 1619 disk encryption (requires 256 or 512-bit dual key); no integrity |
| **GCM** | AEAD | 12 bytes (NIST) | 16 bytes | **Recommended**; single-pass AEAD with hardware PCLMULQDQ acceleration |
| **CCM** | AEAD | 7--13 bytes (12 B def) | 4, 8, 16 bytes | Two-pass AEAD (CTR + CBC-MAC); constrained / embedded networking |

---

## 3. Source Structure

```
lab1/
├── CMakeLists.txt              # Cross-platform CMake configuration
├── require.md                  # Lab 1 formal specification and requirements
├── README.md                   # Unified project documentation and run guide
├── kat/
│   └── vectors.json            # 9 NIST / IEEE Known Answer Test vectors
├── lab-guide/
│   ├── windows_guide.md        # Dedicated Windows execution instructions
│   └── ubuntu_guide.md         # Dedicated Ubuntu execution instructions
├── src/
│   ├── cryptopp_all.hpp        # Unified header loading Crypto++ modules
│   ├── aescore.hpp             # Decoupled core encryption interface API
│   ├── aescore.cpp             # AES 8 modes, AEAD, Nonce registry, File/Hex/Base64 I/O
│   └── main.cpp                # CLI driver, KAT runner, Benchmark engine
├── tests/
│   ├── test_core.cpp           # Catch2 unit tests (8 test cases, negative tests)
│   └── cli_negative.cmake      # CMake integration script for CLI negative testing
├── scripts/
│   ├── build_windows.bat       # Automated build script for Windows (MSVC/GCC/Clang)
│   ├── build_linux.sh          # Automated build script for Linux (GCC/Clang)
│   └── plot_bench.py           # Python data processing and chart generation script
├── output/
│   ├── windows/                # Test logs, CSVs, and plots on Windows
│   ├── ubuntu/                 # Test logs, CSVs, and plots on Linux (Ubuntu)
│   └── compare/                # Cross-platform comparison graphs and summary CSVs
│       └── mode_cmp/           # In-depth 4 core comparison plots and datasets
└── docs/
    ├── report_outline.md       # Report structural outline
    ├── report.tex              # Complete academic report in LaTeX
    └── lab1-main-report.md     # Full comprehensive technical report (Markdown)
```

---

## 4. Dependencies & Prerequisites

### Windows Environment
- **Crypto++**: `D:\UIT\NT219\libs\cryptopp\`
  - Headers: `include/`
  - Compiler libraries: `library/{msvc,gcc,clang}/`
- **OpenSSL**: `D:\UIT\NT219\libs\openssl\`
  - Compiler libraries: `{msvc,gcc,clang}/{include,lib,lib64}/`
- **Tools**:
  - MSYS2 (UCRT64 or MINGW64) with GCC/G++ (`pacman -S --needed base-devel mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake`)
  - Microsoft Visual C++ (MSVC x64) with VS 2022/2026
  - CMake 3.20+
  - Python 3.10+ (with `matplotlib`, `pandas`, `numpy`) for benchmark graph generation

### Linux (Ubuntu / Debian) Environment
- **Crypto++**: `~/Desktop/nt219lib/cryptopp/`
  - Compiler libraries: `{gcc,clang}/include/` and `{gcc,clang}/lib/libcryptopp.a`
- **OpenSSL**: `~/Desktop/nt219lib/openssl/`
  - Per compiler: `{gcc,clang}/`
- **System Packages**:
  ```bash
  sudo apt update
  sudo apt install -y build-essential cmake g++ clang libssl-dev python3 python3-pip
  pip3 install matplotlib pandas numpy
  ```

---

## 5. Building the Project

### Option 1: Using Automated Build Scripts (Recommended)

#### Windows (MSYS2 / CMD)
```bash
# Navigate to lab1/ directory
cd lab1

# MSVC (requires x64 Native Tools Command Prompt for VS)
scripts\build_windows.bat msvc

# GCC (MinGW-w64 / MSYS2)
scripts\build_windows.bat gcc

# Clang (MSYS2)
scripts\build_windows.bat clang

# Clean all build directories
scripts\build_windows.bat clean
```

#### Linux (Ubuntu/Debian)
```bash
# Navigate to lab1/ directory
cd lab1

# Build with GCC
scripts/build_linux.sh gcc

# Build with Clang
scripts/build_linux.sh clang

# Clean build directory
scripts/build_linux.sh gcc --clean
```

---

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

---

### Option 3: Export Shared Library (Optional Extension)

#### Windows
```powershell
cd lab1
cmake -S . -B build_windows_gcc_shared -G "MinGW Makefiles" `
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
      -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows_gcc_shared -j
```

#### Linux
```bash
cd lab1
cmake -S . -B build_ubuntu_gcc_shared -DBUILD_SHARED_LIBS=ON
cmake --build build_ubuntu_gcc_shared
```

---

## 6. Windows Execution & Verification Guide (PowerShell)

All commands should be executed from the `lab1` root directory.

### Step 0: Directory Preparation
```powershell
# Create required output directory structure
mkdir -Force output\windows\key_bin
mkdir -Force output\windows\neg_results
mkdir -Force output\windows\benchmarks
```

### Step 1: Known Answer Tests (NIST KAT)
Execute the official 9 test vectors and save logs:
```powershell
.\build_windows_gcc\aestool.exe --kat kat\vectors.json > output\windows\kat_windows_gcc.txt 2>&1
```

### Step 2: Automated Unit Tests (Catch2 via CTest)
```powershell
cd build_windows_gcc
ctest --output-on-failure > ..\output\windows\test_windows_gcc.txt 2>&1
cd ..
```

### Step 3: Prepare Test Data for Negative Testing
Generate keys, IVs, plaintexts, and valid ciphertexts for tamper testing:
```powershell
# Helper function for CSPRNG random generation
function New-RandomFile {
    param([string]$Path, [int]$Bytes)
    $buf = New-Object byte[] $Bytes
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($buf)
    [System.IO.File]::WriteAllBytes($Path, $buf)
}

New-RandomFile "output\windows\key_bin\key.bin"       32
New-RandomFile "output\windows\key_bin\key_wrong.bin" 32
New-RandomFile "output\windows\key_bin\iv.bin"        16
New-RandomFile "output\windows\key_bin\iv_wrong.bin"  16
New-RandomFile "output\windows\key_bin\short_iv.bin"  8

# Create sample plaintext
"Hello World plaintext for testing negative cases" | Out-File -Encoding ascii output\windows\key_bin\plain.txt

# Encrypt to create valid reference ciphertexts (CBC & GCM)
.\build_windows_gcc\aestool.exe encrypt --mode cbc --key output\windows\key_bin\key.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct.bin
.\build_windows_gcc\aestool.exe encrypt --mode gcm --aead --key output\windows\key_bin\key.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct_gcm.bin
```

### Step 4: Negative Testing (Fail-Closed Validations)
Execute negative test cases and record outputs into `output\windows\neg_results\`:
```powershell
# 1. Test wrong key (decryption failure)
.\build_windows_gcc\aestool.exe decrypt --mode cbc --key output\windows\key_bin\key_wrong.bin --in output\windows\key_bin\ct.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_key.txt

# 2. Test wrong IV
.\build_windows_gcc\aestool.exe decrypt --mode cbc --key output\windows\key_bin\key.bin --iv output\windows\key_bin\iv_wrong.bin --in output\windows\key_bin\ct.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_iv.txt

# 3. Test wrong AEAD Tag (GCM authentication failure)
.\build_windows_gcc\aestool.exe decrypt --mode gcm --aead --key output\windows\key_bin\key.bin --in output\windows\key_bin\ct_gcm.bin --out output\windows\key_bin\out.txt 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_tag.txt

# 4. Test invalid IV length (rejection on entry)
.\build_windows_gcc\aestool.exe encrypt --mode cbc --key output\windows\key_bin\key.bin --iv output\windows\key_bin\short_iv.bin --in output\windows\key_bin\plain.txt --out output\windows\key_bin\ct.bin 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
fsutil file createnew output\windows\key_bin\large.bin 32768
.\build_windows_gcc\aestool.exe encrypt --mode ecb --key output\windows\key_bin\key.bin --in output\windows\key_bin\large.bin --out output\windows\key_bin\ct.bin 2>&1 | Tee-Object -FilePath output\windows\neg_results\neg_ecb.txt
```

### Step 5: Performance Benchmarking & Graph Generation
```powershell
# Set Windows power plan to High Performance for stable measurement
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c

# Run benchmark (sweeps all 8 modes, Enc/Dec, and 6 payload sizes)
.\build_windows_gcc\aestool.exe bench --key output\windows\key_bin\key.bin --runs 1000 --out output\windows\benchmarks\bench_windows_gcc.csv

# Generate plots
mkdir -Force output\windows\benchmarks\graph
Get-ChildItem output\windows\benchmarks\*.csv | ForEach-Object {
    python scripts\plot_bench.py $_.FullName --out-dir output\windows\benchmarks\graph --platform windows
}
```

---

## 7. Linux (Ubuntu) Execution & Verification Guide (Bash)

All commands should be executed from the `lab1` root directory.

### Step 0: Directory Preparation
```bash
# Create required output directory structure
mkdir -p output/ubuntu/key_bin
mkdir -p output/ubuntu/neg_results
mkdir -p output/ubuntu/benchmarks
```

### Step 1: Known Answer Tests (NIST KAT)
```bash
# KAT - GCC
./build_ubuntu_gcc/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_gcc.txt 2>&1

# KAT - Clang
./build_ubuntu_clang/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_clang.txt 2>&1
```

### Step 2: Automated Unit Tests (Catch2 via CTest)
```bash
# Unit Test - GCC
cd build_ubuntu_gcc
ctest --output-on-failure > ../output/ubuntu/test_ubuntu_gcc.txt 2>&1
cd ..

# Unit Test - Clang
cd build_ubuntu_clang
ctest --output-on-failure > ../output/ubuntu/test_ubuntu_clang.txt 2>&1
cd ..
```

### Step 3: Prepare Test Data for Negative Testing
```bash
# 1. Generate keys & IVs using CSPRNG (/dev/urandom)
head -c 32 /dev/urandom > output/ubuntu/key_bin/key.bin
head -c 32 /dev/urandom > output/ubuntu/key_bin/key_wrong.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv.bin
head -c 16 /dev/urandom > output/ubuntu/key_bin/iv_wrong.bin
head -c 8  /dev/urandom > output/ubuntu/key_bin/short_iv.bin

# 2. Generate sample plaintext
echo "Hello World plaintext for testing negative cases" > output/ubuntu/key_bin/plain.txt

# 3. Encrypt plain text to create valid reference ciphertexts (CBC & GCM)
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin
./build_ubuntu_gcc/aestool encrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct_gcm.bin
```

### Step 4: Negative Testing (Fail-Closed Validations)
```bash
# 1. Test wrong key
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/ubuntu/key_bin/key_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_key.txt

# 2. Test wrong IV
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/iv_wrong.bin --in output/ubuntu/key_bin/ct.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_iv.txt

# 3. Test wrong AEAD Tag (GCM)
./build_ubuntu_gcc/aestool decrypt --mode gcm --aead --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/ct_gcm.bin --out output/ubuntu/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_tag.txt

# 4. Test wrong IV length
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/ubuntu/key_bin/key.bin --iv output/ubuntu/key_bin/short_iv.bin --in output/ubuntu/key_bin/plain.txt --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
dd if=/dev/zero of=output/ubuntu/key_bin/large.bin bs=1K count=32
./build_ubuntu_gcc/aestool encrypt --mode ecb --key output/ubuntu/key_bin/key.bin --in output/ubuntu/key_bin/large.bin --out output/ubuntu/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ecb.txt
```

### Step 5: Performance Benchmarking & Graph Generation
```bash
# Set CPU frequency governor to performance
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Run full benchmark suite (sweeps all modes and payload sizes)
./build_ubuntu_gcc/aestool bench --key output/ubuntu/key_bin/key.bin --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_gcc.csv
./build_ubuntu_clang/aestool bench --key output/ubuntu/key_bin/key.bin --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_clang.csv

# Generate plots
mkdir -p output/ubuntu/benchmarks/graph
for csv_file in output/ubuntu/benchmarks/*.csv; do
    python3 scripts/plot_bench.py "$csv_file" --out-dir output/ubuntu/benchmarks/graph --platform linux
done
```

---

## 8. CLI Command Specification & Examples

## 8.1 PyQt6 GUI

Lab 1 có GUI PyQt6 để gọi `aestool` mà không cần nhập thủ công các tham số
command-line. GUI hoạt động trong thư mục `lab1`, tự tìm executable ở các
thư mục build chuẩn (`build_ubuntu_gcc`, `build_ubuntu_clang`,
`build_windows_gcc`, `build_windows_clang` và `build_windows_msvc/Release`).

Cài PyQt6 nếu môi trường chưa có:

```bash
python3 -m pip install PyQt6
```

Khởi động GUI:

```bash
cd lab1
python3 gui/aestool_gui.py
```

GUI hỗ trợ:

- Encrypt/decrypt với ECB, CBC, CFB, OFB, CTR, XTS, CCM và GCM.
- Chọn key/IV từ file hoặc nhập key/IV dạng hex.
- AAD, authentication tag, metadata JSON, nonce database và các cờ bảo vệ
  như `--aead`, `--allow-ecb`, `--verbose`.
- Sinh key 256-bit, chạy KAT và benchmark nhanh.

GUI chỉ chạy lệnh sau khi nhấn nút tương ứng. Benchmark được ghi vào thư mục
do người dùng chọn; nên dùng `output/ubuntu/benchmarks/` hoặc
`output/windows/benchmarks/` khi chạy đúng nền tảng.

### Help & Usage
```bash
./aestool --help
```

### Key Generation
```bash
# Generate 256-bit raw binary key
./aestool keygen --bits 256 --out key.bin

# Generate 256-bit key in Hex format with AESKEY-HEX header
./aestool keygen --bits 256 --hex --out key.hex
```

### Encryption
```bash
# CBC encryption with key file and automatic IV generation (stored in document.enc.json)
./aestool encrypt --mode cbc --key key.bin --in document.pdf --out document.enc

# GCM authenticated encryption with Associated Data (AAD)
./aestool encrypt --mode gcm --aead --key key.bin --aad-text "Header-Metadata" \
                  --in document.pdf --out document.enc

# Text encryption outputting Base64 to terminal
./aestool encrypt --mode ctr --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
                  --text "Secret message" --encode base64
```

### Decryption
```bash
# GCM decryption (automatically reads IV, Tag, and AAD from document.enc.json)
./aestool decrypt --mode gcm --aead --key key.bin --in document.enc --out document_dec.pdf

# CBC decryption specifying explicit IV
./aestool decrypt --mode cbc --key key.bin --iv-hex 000102030405060708090a0b0c0d0e0f \
                  --in document.enc --out document_dec.pdf
```

### Known Answer Tests Runner
```bash
./aestool --kat kat/vectors.json
```

### Performance Benchmark Runner
```bash
./aestool bench --runs 1000 --out bench_results.csv
```

### Standard Exit Codes
- `0 (EXIT_OK)`: Operation succeeded.
- `1 (EXIT_USAGE)`: Command-line error, bad arguments, invalid format, or security constraint violation (ECB blocked, nonce reuse detected).
- `2 (EXIT_AUTH)`: Decryption or authentication verification failed (Integrity Tag mismatch, bad ciphertext).
- `3 (EXIT_KAT)`: One or more KAT vector checks failed.

---

## 9. Verified Build Matrix & Test Results

| Platform | Compiler | Build Config | Build Status | CTest Status | KAT Verification |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Windows 10** | MSVC 19.38 (VS 2022/2026) | Release x64 | Pass | 3/3 Passed (0.31s) | 9/9 Passed (100%) |
| **Windows 10** | MinGW-w64 GCC 13.1.0 | Release x64 | Pass | 3/3 Passed (0.28s) | 9/9 Passed (100%) |
| **Windows 10** | Clang 16.0 / 22.1 (MSYS2) | Release x64 | Pass | 3/3 Passed | 9/9 Passed (100%) |
| **Ubuntu 22.04 LTS** | GCC 11.4.0 | Release x86_64 | Pass | 3/3 Passed (0.07s) | 9/9 Passed (100%) |
| **Ubuntu 22.04 LTS** | Clang 14.0.0 | Release x86_64 | Pass | 3/3 Passed (0.08s) | 9/9 Passed (100%) |

---

## 10. Security & Implementation Notes

1. **Crypto++ Exclusivity:** All symmetric primitives, cipher modes, CSPRNGs (`AutoSeededRandomPool`), and hash functions (`SHA256`) strictly use Crypto++ 8.9+.
2. **Fail-Closed Design:** In AEAD decryption (GCM/CCM) or when authentication fails, no plaintext output file is written to disk, preventing plaintext leakage. Error messages remain neutral to thwart padding/error oracle attacks.
3. **Nonce-Reuse Prevention:** CTR, CCM, and GCM record hashes of used tuples $\text{SHA-256}(Key \parallel Mode \parallel IV)$ in `.aestool_nonces`. Attempting to encrypt with an existing pair immediately aborts.
4. **ECB Mitigation:** Input sizes $> 16\text{ KiB}$ are refused by default unless explicitly overridden with `--allow-ecb`.
5. **Constant-Time Verification:** AEAD tags are checked using constant-time comparison inside Crypto++'s `AuthenticatedDecryptionFilter` to mitigate timing side channels.
6. **Binary Stream Safety:** All file operations open streams with `std::ios::binary` to ensure multi-byte UTF-8 character strings and raw binary blocks remain untruncated across Windows and Linux.

---

## 11. References

- **NIST FIPS 197**: [Advanced Encryption Standard (AES)](https://csrc.nist.gov/publications/detail/fips/197/final)
- **NIST SP 800-38A**: [Recommendation for Block Cipher Modes of Operation](https://csrc.nist.gov/publications/detail/sp/800-38a/final)
- **NIST SP 800-38C**: [Recommendation for Block Cipher Modes of Operation: The CCM Mode](https://csrc.nist.gov/publications/detail/sp/800-38c/final)
- **NIST SP 800-38D**: [Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- **IEEE Std 1619-2007**: [Cryptographic Protection of Data on Block-Oriented Storage Devices (XTS-AES)](https://standards.ieee.org/)
- **RFC 5116**: [An Interface and Algorithms for Authenticated Encryption](https://tools.ietf.org/html/rfc5116)
- **Crypto++ Library Documentation**: [Crypto++ AES & Wiki](https://www.cryptopp.com/wiki/AES)