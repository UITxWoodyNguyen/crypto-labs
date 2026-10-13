# Lab 1: Comprehensive Windows Execution Guide (`lab_windows_runguide.md`)

> **Note:** All commands are tailored specifically for the environment on Windows (run in **Windows PowerShell**). Test data is **shared** between OSes via `output/key_bin` — if you already ran the Ubuntu guide, **skip Section 5** and reuse the existing files. OS-specific outputs go to `output/windows`.

---

## 0. Initial Setup & Directory Preparation

Run all commands from the repository root: `D:\UIT\NT219\crypto-labs\lab1`

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# Create all required output directories
mkdir output\key_bin -Force
mkdir output\windows\neg_results -Force
mkdir output\windows\benchmarks -Force
```

---

## 1. Install Dependencies

Install the required tools manually:

- **MSYS2** (provides GCC, G++, Make, CMake) — https://www.msys2.org/
- **CMake** — https://cmake.org/download/
- **Git for Windows** (optional) — https://git-scm.com/download/win
- **vcpkg** (recommended for OpenSSL) — https://github.com/microsoft/vcpkg

```powershell
# After installing MSYS2, open "MSYS2 UCRT64" shell and install toolchain
pacman -Syu
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-openssl

# Add MSYS2 UCRT64 bin to PATH (adjust if MSYS2 is installed elsewhere)
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path

# Verify
gcc --version
g++ --version
```

> The build script `scripts\build_windows_gcc.bat` is expected to invoke GCC/G++ from MSYS2 UCRT64.

---

## 2. Build Project (GCC)

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# Build with GCC
scripts\build_windows_gcc.bat gcc
```

**Expected Build Output Executables:**
- `build_windows_gcc\aestool.exe`

> If your project does not ship `build_windows_gcc.bat`, replace the above with explicit CMake commands:
> ```powershell
> cmake -S . -B build_windows_gcc -G "MinGW Makefiles" `
>       -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
>       -DCMAKE_BUILD_TYPE=Release
> cmake --build build_windows_gcc -j
> ```

---

## 3. Known Answer Tests (KAT)

Execute KAT vectors and store outputs in `output\windows\`:

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# KAT - GCC
.\build_windows_gcc\aestool.exe --kat kat\vectors.json > output\windows\kat_windows_gcc.txt 2>&1
```

---

## 4. Unit Tests (Catch2 via CTest)

```powershell
# Unit Test - GCC
cd D:\UIT\NT219\crypto-labs\lab1\build_windows_gcc
ctest --output-on-failure > D:\UIT\NT219\crypto-labs\lab1\output\windows\test_windows_gcc.txt 2>&1
```

---

## 5. Prepare Test Data for Negative Testing (Shared between OSes)

> **Skip this section if `output\key_bin\` already contains the files** (e.g. you already ran the Ubuntu guide). The same files are reused across OSes.

Generate keys, IVs, plaintexts, and ciphertexts in `output\key_bin\`:

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# 1. Generate keys & IVs (PowerShell equivalent of /dev/urandom)
function New-RandomFile {
    param([string]$Path, [int]$Bytes)
    $buf = New-Object byte[] $Bytes
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($buf)
    [System.IO.File]::WriteAllBytes($Path, $buf)
}

New-RandomFile "output\key_bin\key.bin"       32
New-RandomFile "output\key_bin\key_wrong.bin" 32
New-RandomFile "output\key_bin\iv.bin"        16
New-RandomFile "output\key_bin\iv_wrong.bin"  16
New-RandomFile "output\key_bin\short_iv.bin"  8

# 2. Generate sample plaintext
"Hello World plaintext for testing negative cases" | Out-File -Encoding ascii output\key_bin\plain.txt

# 3. Encrypt plain text to create valid ciphertext (CBC & GCM)
.\build_windows_gcc\aestool.exe encrypt --mode cbc --key output\key_bin\key.bin --in output\key_bin\plain.txt --out output\key_bin\ct.bin
.\build_windows_gcc\aestool.exe encrypt --mode gcm --aead --key output\key_bin\key.bin --in output\key_bin\plain.txt --out output\key_bin\ct_gcm.bin

# 4. Large payload for ECB policy test
fsutil file createnew output\key_bin\large.bin 32768
```

---

## 6. Negative Testing (Fail-Closed Validations)

Run negative tests and capture outputs into `output\windows\neg_results\`:

> **Important:** Wrap each command in `cmd /c "... 2>&1"` so stderr is merged by `cmd` **before** PowerShell sees it. Otherwise PowerShell interprets stderr as `NativeCommandError` and pollutes the log file with its own error record.

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# 1. Test wrong key
cmd /c ".\build_windows_gcc\aestool.exe decrypt --mode cbc --key output\key_bin\key_wrong.bin --in output\key_bin\ct.bin --out output\key_bin\out.txt 2>&1" | Tee-Object -FilePath output\windows\neg_results\neg_key.txt

# 2. Test wrong IV
cmd /c ".\build_windows_gcc\aestool.exe decrypt --mode cbc --key output\key_bin\key.bin --iv output\key_bin\iv_wrong.bin --in output\key_bin\ct.bin --out output\key_bin\out.txt 2>&1" | Tee-Object -FilePath output\windows\neg_results\neg_iv.txt

# 3. Test wrong AEAD Tag (GCM)
cmd /c ".\build_windows_gcc\aestool.exe decrypt --mode gcm --aead --key output\key_bin\key.bin --in output\key_bin\ct_gcm.bin --out output\key_bin\out.txt 2>&1" | Tee-Object -FilePath output\windows\neg_results\neg_tag.txt

# 4. Test wrong IV length
cmd /c ".\build_windows_gcc\aestool.exe encrypt --mode cbc --key output\key_bin\key.bin --iv output\key_bin\short_iv.bin --in output\key_bin\plain.txt --out output\key_bin\ct.bin 2>&1" | Tee-Object -FilePath output\windows\neg_results\neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
cmd /c ".\build_windows_gcc\aestool.exe encrypt --mode ecb --key output\key_bin\key.bin --in output\key_bin\large.bin --out output\key_bin\ct.bin 2>&1" | Tee-Object -FilePath output\windows\neg_results\neg_ecb.txt
```

---

## 7. Performance Benchmarks

Set the Windows power plan to **High Performance** and benchmark modes:

> **Note:** The `bench` subcommand in `aestool` is pre-configured to sweep **all modes** (encrypt + decrypt) across **6 block sizes** in a single run. Only `--runs` and `--out` are effective flags — `--mode` and `--size` are ignored.

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

# Set power plan to High Performance for consistent measurement
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c   # High Performance GUID

# Run Benchmarks (GCC) — covers all modes & all 6 block sizes
.\build_windows_gcc\aestool.exe bench --key output\key_bin\key.bin --runs 1000 --out output\windows\benchmarks\bench_windows_gcc.csv
```

`plot_bench.py` run:

```powershell
cd D:\UIT\NT219\crypto-labs\lab1

mkdir output\windows\benchmarks\graph -Force

# Loop through each CSV and write results to the graph directory
Get-ChildItem output\windows\benchmarks\*.csv | ForEach-Object {
    python scripts\plot_bench.py $_.FullName --out-dir output\windows\benchmarks\graph --platform windows
}
```

---

## 8. Export Shared Library (Optional)

```powershell
cd D:\UIT\NT219\crypto-labs\lab1
cmake -S . -B build_windows_gcc_shared -G "MinGW Makefiles" `
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
      -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows_gcc_shared -j
```
