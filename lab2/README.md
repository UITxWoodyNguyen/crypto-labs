# Lab 2 — AES-128 in pure C++ (CTR mode, FIPS-197)

`aestool`: an AES-128 block cipher written from scratch (FIPS-197) with CTR mode (NIST SP 800-38A).
**No external crypto library is used in the core.** OpenSSL is linked *only* for cross-validation and
benchmark comparison (`xval`, `bench`).

## Features
- AES-128: `KeyExpansion`, `SubBytes`, `ShiftRows`, `MixColumns`, `AddRoundKey` + inverse cipher (10 rounds).
- Three selectable backends (`--sbox`) for the table-vs-computed analysis:
  | Backend | Memory | Notes |
  |---|---|---|
  | `table` (default) | 256 B S-box (+256 B inverse) | byte-oriented, FIPS-197 style, secret-indexed lookups |
  | `ttable` | 4 x 1 KiB T-tables | fastest, classic cache-timing target |
  | `computed` | no tables | S(x)=affine(x^254), branch-free GF(2^8); ~1 MB/s, no secret-dependent index/branch |
- CTR: arbitrary length, no padding, streaming (multi-GiB), chunk-independent.
- KAT runner (`--kat`), cross-validation (`xval`), benchmark (`bench`) with mean/median/stddev/95% CI.
- Fail-closed input validation with distinct exit codes.

## CTR counter specification
- IV = 16-byte **initial counter block** (nonce ‖ counter).
- Counter = the whole block as a **128-bit big-endian integer** (byte 15 least significant), +1 per block (SP 800-38A B.1, m=128).
- **Overflow:** the counter never wraps. After block `FF..FF` is used, requesting more keystream throws
  `CounterOverflow` (exit 5) instead of repeating keystream.
- Auto-generated IV = 96-bit random nonce ‖ 32-bit zero counter (OS CSPRNG), saved to `<out>.iv` (raw 16 bytes).
- **The (key, IV) pair must never repeat.** CTR provides confidentiality only — no integrity.

## Requirements
CMake >= 3.20, C++17 compiler (GCC/Clang/MSVC/MinGW-w64), OpenSSL >= 1.1 (reference only),
Catch2 v3 (installed copy is used if found, otherwise v3.5.3 is fetched via FetchContent).
OpenSSL is looked up in `D:/UIT/NT219/libs/openssl/{msvc,clang,gcc}` (Windows) or
`~/Desktop/nt219lib/openssl/{clang,gcc}` (Linux) if those exist; otherwise the system OpenSSL is used.
Override with `-DOPENSSL_ROOT_DIR=...`.

```bash
# Ubuntu LTS
sudo apt install build-essential cmake libssl-dev catch2
# Windows: install CMake + MSVC (VS Build Tools) or MinGW-w64 + an OpenSSL build for that compiler
```

## Build

### Prebuilt binaries

Prebuilt CLI binaries are stored under `release/<OS_name>/`, where
`<OS_name>` is `ubuntu` or `windows`. Building from source is optional when
using one of these binaries:

| OS | Example binary |
| :--- | :--- |
| Ubuntu | `release/ubuntu/aestool_gcc` |
| Windows (MinGW/GCC) | `release/windows/aestool_gcc.exe` |
| Windows (MSVC) | `release/windows/aestool_msvc.exe` |

Only binaries available for the current platform are present. Check the
release directory before running:

```bash
# Ubuntu
cd lab2
ls -l release/ubuntu
./release/ubuntu/aestool_gcc --help
./release/ubuntu/aestool_gcc --kat kat/vectors.json
```

```powershell
# Windows PowerShell
cd lab2
Get-ChildItem release\windows
.\release\windows\aestool_gcc.exe --help
```

Run commands from the `lab2` root so relative paths resolve correctly. The
build commands below are still used when rebuilding from source or running
the test suite.

```bash
# Linux
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc && cmake .. && cmake --build . -j && ctest --output-on-failure
# Windows MinGW64
mkdir build_windows_gcc && cd build_windows_gcc && cmake .. -G "MinGW Makefiles" && cmake --build . && ctest
# Windows MSVC
mkdir build_windows_msvc && cd build_windows_msvc && cmake .. && cmake --build . --config Release && ctest -C Release
```
Options: `-DBUILD_TESTS=OFF`, `-DLAB2_WERROR=OFF` (warnings are errors on GCC/Clang by default).
Helper scripts: `scripts/build.sh`, `scripts/build.bat`.

## Quick start

For a prebuilt binary, set `AESTOOL` to the binary for the current OS. The
examples below then work without copying the executable into the repository
root:

```bash
cd lab2
AESTOOL=./release/ubuntu/aestool_gcc
$AESTOOL keygen --out key.bin
$AESTOOL encrypt --mode ctr --key key.bin --in msg.bin --out ct.bin      # also writes ct.bin.iv
$AESTOOL decrypt --mode ctr --key key.bin --in ct.bin  --out msg2.bin    # reads ct.bin.iv
# explicit IV (as in the assignment)
$AESTOOL ivgen --out iv.bin
$AESTOOL encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
# screen output (hex), UTF-8 text
$AESTOOL encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --text "xin chào"
$AESTOOL --kat kat/vectors.json             # PASS/FAIL per case + summary
$AESTOOL xval --ref all                     # compare with OpenSSL
```

On Windows PowerShell, use for example:

```powershell
$AESTOOL = ".\release\windows\aestool_gcc.exe"
& $AESTOOL --help
```

## Command reference
| Option | Meaning |
|---|---|
| `encrypt` / `decrypt` | CTR transform (same operation) |
| `--key FILE` / `--key-hex HEX` | exactly 16 bytes (raw file / 32 hex chars) |
| `--iv FILE` / `--iv-hex HEX` | exactly 16 bytes; omitted on encrypt ⇒ random + sidecar; omitted on decrypt ⇒ `<in>.iv` |
| `--in FILE` / `--text STR` | input (UTF-8 text); `--in-encode hex\|base64\|raw` decodes it |
| `--out FILE` | output file; default raw binary; `--encode hex\|base64` writes text |
| `--encode` | screen default hex (encrypt) / raw (decrypt) |
| `--sbox table\|computed\|ttable` | backend |
| `--force` | overwrite existing outputs (otherwise refused) |
| `--kat FILE` | run Known Answer Tests |
| `bench` | `--sizes 1MiB,100MiB,1GiB --runs 30 --backends table,ttable,computed --ref all --dir enc\|dec\|both --ops N --warmup S --target-sec S --csv FILE` |

Exit codes: 0 ok · 1 internal · 2 usage · 3 invalid input · 4 I/O · 5 counter overflow · 6 KAT/xval failure.

## Testing
`ctest` runs: Catch2 unit tests (FIPS-197 key schedule, S-box, all backends, SP 800-38A CTR, chunking, carry/overflow,
wrong key/IV, tamper malleability, two-time pad, codecs, JSON, file streaming, cross-validation), the KAT runner, and a CLI
end-to-end test. `scripts/demo.sh <tool>` prints the negative-test demonstrations.
KAT sources: FIPS-197 App. B/C.1, NIST AESAVS ECB sets, SP 800-38A F.5.1; extra carry vectors are cross-generated
with OpenSSL (marked in `kat/vectors.json`).

## Benchmarking
Protocol: warm-up (default 1.5 s) → N=30 runs/case → mean, median, stddev, 95% CI (Student t). Ops per run are
auto-sized so a run lasts ≈0.25 s (cap 1000); use `--ops 1000` to follow the "1000 operations" wording literally
(slow for large sizes). Run `bench --sizes 1MiB,100MiB` (add `1GiB` if memory allows) on each OS, then
`python3 scripts/plot_bench.py bench.csv` for plots. Set the CPU governor to *performance* / Windows *High performance*,
and fill the hardware table from the header the tool prints. `computed` is ~100x slower; restrict it to small sizes.
For the full lab protocol, run `scripts/bench.sh` on Ubuntu or `scripts\bench.bat` on Windows;
the scripts write separate 1MiB/100MiB/1GiB CSV files under `Output/<OS>/bench` using
500/100/30 runs respectively. Multiple CSV files can be plotted together:
`python3 scripts/plot_bench.py first.csv second.csv --out-prefix Output/combined`.
`scripts/compare.py` keeps the combined plots and also writes one throughput/latency plot
per payload size to `Output/compare`.

## Known limitations
- Only AES-128 + CTR (AES-192/256, XTS, AES-NI are optional bonuses, not implemented).
- `table`/`ttable` backends use secret-dependent memory indices → **vulnerable to cache-timing attacks** by design (analysis topic);
  `computed` avoids that but is not audited and the surrounding code (I/O, XOR) is not hardened.
- `--key-hex` is visible in process listings; prefer `--key FILE`. Key buffers are wiped best-effort.
- No nonce database: reuse across separate runs with `--iv` cannot be detected (a warning is printed).
- Windows console output of non-ASCII text depends on the console code page (arguments are read as UTF-16 → UTF-8).
