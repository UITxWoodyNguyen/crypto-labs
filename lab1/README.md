# Lab 1: Symmetric Encryption with Crypto++

## Overview
Command-line tool `aestool` implementing AES symmetric encryption with multiple modes using Crypto++ library.

## Supported Modes
| Mode | Type | Key Size | IV/Nonce Size | Notes |
|------|------|----------|---------------|-------|
| ECB | Block | 128/192/256 | N/A | ⚠ Warning, limited to 16KB without `--allow-ecb` |
| CBC | Block | 128/192/256 | 16 bytes | PKCS#7 padding |
| CFB | Stream | 128/192/256 | 16 bytes | No padding |
| OFB | Stream | 128/192/256 | 16 bytes | No padding |
| CTR | Stream | 128/192/256 | 16 bytes | No padding |
| XTS | Block | 256/512 | 16 bytes (tweak) | No padding, ≥16 bytes input |
| CCM | AEAD | 128/192/256 | 7-13 bytes | Tag: 4/8/16 bytes |
| GCM | AEAD | 128/192/256 | 12 bytes | Tag: 16 bytes |

## Dependencies
- **Crypto++** 8.7+ (built with `/MT` on Windows)
- **OpenSSL** 3.0+ (for KAT vectors only)
- **CMake** 3.20+
- **C++17** compatible compiler (MSVC, GCC, Clang)

### Windows
- Visual Studio 2022+ (MSVC)
- Or MinGW-w64 (GCC 11+)

### Linux
- Ubuntu 20.04/22.04/24.04 LTS
- `build-essential cmake git libcrypto++-dev`

---

## Building

### Windows (MSVC)
```cmd
REM Open "x64 Native Tools Command Prompt for VS 2022"
cd crypto-labs\lab1\scripts
build_windows.bat
```

Optional: clean rebuild
```cmd
build_windows.bat clean
```

Output: `build\lab1\Release\aestool.exe`

### Linux
```bash
cd crypto-labs
./lab1/scripts/build_linux.sh
```

Optional flags:
```bash
./lab1/scripts/build_linux.sh --clean       # clean rebuild
./lab1/scripts/build_linux.sh --install-deps # install deps via apt (sudo)
```

Custom Crypto++ path:
```bash
CRYPTOPP_ROOT=/opt/cryptopp ./lab1/scripts/build_linux.sh
# or
CRYPTOPP_INCLUDE_DIR=/opt/cryptopp/include CRYPTOPP_LIBRARY=/opt/cryptopp/lib/libcryptopp.a \
  ./lab1/scripts/build_linux.sh
```

Output: `build/lab1/aestool`

### Manual CMake (both platforms)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Windows: add `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`

---

## CLI Usage

### Key Generation
```bash
# 256-bit key, raw binary
aestool keygen --bits 256 --out key.bin

# 256-bit key, hex format with header
aestool keygen --bits 256 --hex --out key.hex
```

### Encryption
```bash
# Basic AES-256-GCM
aestool encrypt --mode gcm --aead --key key.bin --in msg.txt --out ct.bin

# With auto-generated nonce, show hex output
aestool encrypt --mode ctr --key key.bin --text "Hello World"

# With explicit nonce file
aestool encrypt --mode gcm --aead --key key.bin --nonce nonce.bin --in msg.txt --out ct.bin

# CCM with custom tag length
aestool encrypt --mode ccm --aead --key key.bin --tag-len 8 --in msg.txt --out ct.bin

# ECB (requires --allow-ecb for >16KB)
aestool encrypt --mode ecb --allow-ecb --key key.bin --in large.bin --out ct.bin
```

### Decryption
```bash
# With sidecar JSON (auto-loads IV/tag/AAD)
aestool decrypt --mode gcm --aead --key key.bin --in ct.bin --out msg.txt

# Explicit tag and IV
aestool decrypt --mode gcm --aead --key key.bin --iv nonce.bin --tag-hex <tag> --in ct.bin --out msg.txt

# Output to stdout
aestool decrypt --mode ctr --key key.bin --iv nonce.bin --text <ciphertext_hex> --encode hex
```

### Known Answer Tests (KAT)
```bash
aestool --kat lab1/kat/vectors.json
# or
aestool kat --kat lab1/kat/vectors.json
```

### Benchmark
```bash
# Full benchmark (30 runs, 6 sizes, 8 modes)
aestool bench --out bench.csv

# Quick benchmark
aestool bench --quick --out bench.csv

# Plot results (requires Python: pandas, matplotlib)
python3 lab1/scripts/plot_bench.py bench.csv
```

---

## Security Features

### Misuse Prevention
- **ECB**: Warning + blocked for files >16KB (override with `--allow-ecb`)
- **IV/Nonce**: Validated per mode; auto-generated via `AutoSeededRandomPool` if omitted
- **Nonce Reuse**: Registry `.aestool_nonces` prevents key+nonce reuse in CTR/CCM/GCM
- **AEAD**: Tag verification mandatory; fail-closed on auth failure
- **Fail Closed**: All errors abort without writing plaintext

### Key/IV Handling
- Keys: raw binary or hex (with `AESKEY-HEX\n` header)
- IV/Nonce: file (raw or hex) or direct hex string
- AAD: file (`--aad`) or string (`--aad-text`)

### Sidecar JSON Header
Generated on encrypt, auto-loaded on decrypt:
```json
{
  "alg": "AES-256-GCM",
  "mode": "gcm",
  "iv": "hex...",
  "aad": "hex...",
  "tag": "hex...",
  "tag_len": "16"
}
```

---

## Testing

```bash
# Unit tests (Catch2)
ctest -C Release -R unit_tests

# KAT vectors (NIST SP 800-38A, GCM, CCM, XTS)
ctest -C Release -R nist_kat
# or
./build/lab1/Release/aestool.exe --kat lab1/kat/vectors.json

# Negative CLI tests
ctest -C Release -R cli_negative
```

Expected: 9/9 KAT pass, all unit tests pass, all negative tests pass.

---

## Benchmarking

Measures throughput (MB/s) and latency (µs/op) across:
- Sizes: 1KB, 4KB, 16KB, 256KB, 1MB, 8MB
- Modes: all 8 modes (encrypt/decrypt)
- Runs: 30 (default), statistical analysis (mean/median/sd/CI95)

Output CSV format:
```csv
mode,dir,size_bytes,ops_per_block,runs,thr_mean_MBps,thr_median_MBps,thr_sd,thr_ci95,lat_mean_us,lat_median_us,lat_sd,lat_ci95
```

---

## Project Structure
```
lab1/
├── CMakeLists.txt          # Build config
├── build.md                # Build guide
├── README.md               # This file
├── kat/
│   └── vectors.json        # NIST KAT vectors
├── scripts/
│   ├── build_windows.bat   # Windows build script
│   ├── build_linux.sh      # Linux build script
│   └── plot_bench.py       # Plotting script
├── src/
│   ├── main.cpp            # CLI entry point
│   ├── aescore.hpp         # Core API
│   ├── aescore.cpp         # Crypto++ implementations
│   └── cryptopp_all.hpp    # Crypto++ headers
└── tests/
    ├── test_core.cpp       # Unit tests (Catch2)
    └── cli_negative.cmake  # Negative CLI tests
```

---

## Known Limitations
- XTS: no authentication (integrity not protected)
- Nonce registry: local file only (not distributed)
- Benchmark: single-threaded, user-space timing
- No hardware AES-NI detection (Crypto++ handles automatically)

---

## License
Academic use only. Part of NT219 Cryptography Labs.