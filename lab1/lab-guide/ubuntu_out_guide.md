# Lab 1: Comprehensive Ubuntu Execution Guide (`lab_ubuntu_runguide.md`)

> **Note:** All commands are tailored specifically for the environment on Ubuntu. Test data is **shared** between OSes via `output/key_bin`; OS-specific outputs go to `output/ubuntu`.

---

## 0. Initial Setup & Directory Preparation

Run all commands from the repository root: `/home/dell/home-crypto/crypto-labs/lab1`

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Create all required output directories
mkdir -p output/key_bin
mkdir -p output/ubuntu/neg_results
mkdir -p output/ubuntu/benchmarks
```

---

## 1. Install Dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ clang libssl-dev
```

---

## 2. Build Project (GCC & Clang)

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Build with GCC
scripts/build_linux.sh gcc

# Build with Clang
scripts/build_linux.sh clang
```

**Expected Build Output Executables:**
- `build_ubuntu_gcc/aestool`
- `build_ubuntu_clang/aestool`

---

## 3. Known Answer Tests (KAT)

Execute KAT vectors and store outputs in `output/ubuntu/`:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# KAT - GCC
./build_ubuntu_gcc/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_gcc.txt 2>&1

# KAT - Clang
./build_ubuntu_clang/aestool --kat kat/vectors.json > output/ubuntu/kat_ubuntu_clang.txt 2>&1
```

---

## 4. Unit Tests (Catch2 via CTest)

```bash
# Unit Test - GCC
cd /home/dell/home-crypto/crypto-labs/lab1/build_ubuntu_gcc
ctest --output-on-failure > /home/dell/home-crypto/crypto-labs/lab1/output/ubuntu/test_ubuntu_gcc.txt 2>&1

# Unit Test - Clang
cd /home/dell/home-crypto/crypto-labs/lab1/build_ubuntu_clang
ctest --output-on-failure > /home/dell/home-crypto/crypto-labs/lab1/output/ubuntu/test_ubuntu_clang.txt 2>&1
```

---

## 5. Prepare Test Data for Negative Testing (Shared between OSes)

Generate keys, IVs, plaintexts, and ciphertexts in `output/key_bin/`. **Only generate once** — the same files are reused by the Windows guide.

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# 1. Generate keys & IVs
head -c 32 /dev/urandom > output/key_bin/key.bin
head -c 32 /dev/urandom > output/key_bin/key_wrong.bin
head -c 16 /dev/urandom > output/key_bin/iv.bin
head -c 16 /dev/urandom > output/key_bin/iv_wrong.bin
head -c 8  /dev/urandom > output/key_bin/short_iv.bin

# 2. Generate sample plaintext
echo "Hello World plaintext for testing negative cases" > output/key_bin/plain.txt

# 3. Encrypt plain text to create valid ciphertext (CBC & GCM)
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/key_bin/key.bin --in output/key_bin/plain.txt --out output/key_bin/ct.bin
./build_ubuntu_gcc/aestool encrypt --mode gcm --aead --key output/key_bin/key.bin --in output/key_bin/plain.txt --out output/key_bin/ct_gcm.bin

# 4. Large payload for ECB policy test
dd if=/dev/zero of=output/key_bin/large.bin bs=1K count=32
```

---

## 6. Negative Testing (Fail-Closed Validations)

Run negative tests and capture outputs into `output/ubuntu/neg_results/`:

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# 1. Test wrong key
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/key_bin/key_wrong.bin --in output/key_bin/ct.bin --out output/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_key.txt

# 2. Test wrong IV
./build_ubuntu_gcc/aestool decrypt --mode cbc --key output/key_bin/key.bin --iv output/key_bin/iv_wrong.bin --in output/key_bin/ct.bin --out output/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_iv.txt

# 3. Test wrong AEAD Tag (GCM)
./build_ubuntu_gcc/aestool decrypt --mode gcm --aead --key output/key_bin/key.bin --in output/key_bin/ct_gcm.bin --out output/key_bin/out.txt 2>&1 | tee output/ubuntu/neg_results/neg_tag.txt

# 4. Test wrong IV length
./build_ubuntu_gcc/aestool encrypt --mode cbc --key output/key_bin/key.bin --iv output/key_bin/short_iv.bin --in output/key_bin/plain.txt --out output/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ivlen.txt

# 5. Test ECB policy restriction on large payload without --allow-ecb
./build_ubuntu_gcc/aestool encrypt --mode ecb --key output/key_bin/key.bin --in output/key_bin/large.bin --out output/key_bin/ct.bin 2>&1 | tee output/ubuntu/neg_results/neg_ecb.txt
```

---

## 7. Performance Benchmarks

Set CPU frequency governor to `performance` and benchmark modes:

> **Note:** The `bench` subcommand in `aestool` is pre-configured to sweep **all modes** (encrypt + decrypt) across **6 block sizes** in a single run. Only `--runs` and `--out` are effective flags — `--mode` and `--size` are ignored.

```bash
cd /home/dell/home-crypto/crypto-labs/lab1

# Set governor to performance mode for consistent measurement
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Run Benchmarks (GCC) — covers all modes & all 6 block sizes
./build_ubuntu_gcc/aestool bench --key output/key_bin/key.bin --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_gcc.csv

# Run Benchmarks (Clang) — covers all modes & all 6 block sizes
./build_ubuntu_clang/aestool bench --key output/key_bin/key.bin --runs 1000 --out output/ubuntu/benchmarks/bench_ubuntu_clang.csv
```

`plot_bench.py` run:

```bash
cd ~/crypto-labs/lab1

mkdir -p output/ubuntu/benchmarks/graph

# Vòng lặp chạy script cho từng file CSV và đẩy kết quả ra thư mục graph
for csv_file in output/ubuntu/benchmarks/*.csv; do
    python3 scripts/plot_bench.py "$csv_file" --out-dir output/ubuntu/benchmarks/graph --platform linux
done
```

---

## 8. Export Shared Library (Optional)

```bash
cd /home/dell/home-crypto/crypto-labs/lab1
cmake -S . -B build_ubuntu_gcc_shared -DBUILD_SHARED_LIBS=ON
cmake --build build_ubuntu_gcc_shared
```