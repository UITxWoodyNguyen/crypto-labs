# Lab 2 — Hướng dẫn build & chạy trên Ubuntu

Thư mục dự án (chứa `CMakeLists.txt`) gọi là `lab2/`. Mọi lệnh bên dưới chạy từ thư mục này, trừ khi có ghi chú khác.

## 1. Cài đặt công cụ

```bash
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev pkg-config
# (tuỳ chọn) Catch2 cài sẵn để build offline; nếu không có, CMake tự tải v3.5.3 (cần internet + git)
sudo apt install -y catch2
# (tuỳ chọn) Clang
sudo apt install -y clang
# (tuỳ chọn) vẽ biểu đồ benchmark
sudo apt install -y python3-matplotlib
```

Kiểm tra phiên bản — CMake phải **>= 3.20**:

```bash
cmake --version
g++ --version        # cần hỗ trợ C++17 (GCC >= 9)
```

> Ubuntu 20.04 chỉ có CMake 3.16 → cài bản mới: `pip install --user cmake` hoặc dùng apt repo của Kitware.

### OpenSSL (chỉ để đối chiếu/so sánh, không dùng trong lõi AES)

`CMakeLists.txt` tự tìm theo thứ tự:

1. Tham số `-DOPENSSL_ROOT_DIR=...` nếu bạn truyền vào.
2. `~/Desktop/nt219lib/openssl/gcc` (GCC) hoặc `~/Desktop/nt219lib/openssl/clang` (Clang) — nếu thư mục tồn tại.
3. OpenSSL của hệ thống (`libssl-dev`).

Chỉ cần `libssl-dev` là đủ để build.

## 2. Build bằng script `scripts/build.sh`

```bash
chmod +x scripts/*.sh
scripts/build.sh gcc       # tạo build_ubuntu_gcc/, build, rồi chạy ctest
scripts/build.sh clang     # tạo build_ubuntu_clang/
```

Script thực hiện: `mkdir build_ubuntu_<cc>` → `cmake ..` → `cmake --build . -j` → `ctest --output-on-failure`.

## 3. Build thủ công bằng lệnh

### GCC

```bash
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake ..
cmake --build . -j"$(nproc)"
ctest --output-on-failure
```

### Clang

```bash
mkdir build_ubuntu_clang && cd build_ubuntu_clang
CC=clang CXX=clang++ cmake ..
cmake --build . -j"$(nproc)"
ctest --output-on-failure
```

### Các tuỳ chọn CMake hữu ích

| Tuỳ chọn | Ý nghĩa |
|---|---|
| `-DCMAKE_BUILD_TYPE=Release\|Debug` | mặc định `Release` (dùng Release khi đo hiệu năng) |
| `-DBUILD_TESTS=OFF` | không build unit test |
| `-DLAB2_WERROR=OFF` | tắt `-Werror` |
| `-DOPENSSL_ROOT_DIR=/duong/dan` | chỉ định OpenSSL |

Build lại từ đầu: `rm -rf build_ubuntu_gcc` rồi chạy lại các lệnh trên.

Sau khi build xong, các file nằm trong thư mục build:
`aestool` (CLI), `aes_tests` (unit test), `libaes_pure.a`.

## 4. Chạy kiểm thử

```bash
cd build_ubuntu_gcc
ctest --output-on-failure          # 3 test: unit_tests, kat_runner, cli_end_to_end
./aes_tests                        # chi tiết unit test (Catch2)
./aestool --kat ../kat/vectors.json            # KAT: PASS/FAIL từng case + tổng kết
./aestool --kat ../kat/vectors.json --verbose
./aestool xval --ref openssl --cases 50        # đối chiếu với OpenSSL
```

## 5. Sử dụng CLI

```bash
cd build_ubuntu_gcc

# Tạo key 16 byte và IV 16 byte
./aestool keygen --out key.bin
./aestool ivgen  --out iv.bin

# Mã hoá / giải mã với IV chỉ định (đúng như đề)
./aestool encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
./aestool decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin  --out msg2.bin

# Không truyền --iv: tự sinh IV ngẫu nhiên và lưu vào ct.bin.iv; decrypt tự đọc lại
./aestool encrypt --key key.bin --in msg.bin --out ct.bin
./aestool decrypt --key key.bin --in ct.bin  --out msg2.bin
cmp msg.bin msg2.bin && echo "OK"

# Xuất ra màn hình (hex), văn bản UTF-8
./aestool encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
                        --iv-hex  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff --text "xin chào"

# Chọn backend S-box: table (mặc định) | ttable | computed
./aestool encrypt --key key.bin --in msg.bin --out ct.bin --sbox ttable --force
```

- `--force`: cho phép ghi đè file output (mặc định bị từ chối).
- Exit code: `0` ok · `2` sai cú pháp · `3` input không hợp lệ · `4` lỗi I/O · `5` tràn bộ đếm CTR · `6` KAT/xval fail.

## 6. Demo negative test

```bash
scripts/demo.sh $PWD/build_ubuntu_gcc/aestool
```

In ra: sai key → plaintext sai, sai IV → plaintext sai, ciphertext bị sửa → **không bị phát hiện** (CTR không có xác thực), IV sai độ dài → bị từ chối, tràn counter → bị từ chối.

## 7. Benchmark

Chuẩn bị (để kết quả công bằng): cố định CPU governor ở `performance`.

```bash
sudo apt install -y linux-tools-common cpufrequtils      # nếu chưa có
sudo cpupower frequency-set -g performance               # hoặc: sudo cpufreq-set -g performance
```

Chạy (từ thư mục build):

```bash
./aestool bench --sizes 1MiB,100MiB --runs 30 --csv bench_ubuntu_gcc.csv
# thêm 1GiB nếu đủ RAM:  --sizes 1MiB,100MiB,1GiB
# backend computed rất chậm (~1 MB/s), chỉ nên đo size nhỏ:
./aestool bench --sizes 64KiB,1MiB --backends computed --runs 30
```

Vẽ biểu đồ:

```bash
python3 ../scripts/plot_bench.py bench_ubuntu_gcc.csv plot_ubuntu_gcc
# sinh ra plot_ubuntu_gcc_throughput.png và plot_ubuntu_gcc_latency.png
```

Phần header mà `bench` in ra (CPU, RAM, OS, compiler, flags) dùng để điền bảng phần cứng trong báo cáo.

## 8. Xử lý lỗi thường gặp

| Triệu chứng | Cách xử lý |
|---|---|
| `CMake 3.20 or higher is required` | Cài CMake mới hơn (xem mục 1) |
| `Could NOT find OpenSSL` | `sudo apt install libssl-dev` hoặc `-DOPENSSL_ROOT_DIR=...` |
| Lỗi khi tải Catch2 | Cài `sudo apt install catch2` hoặc kiểm tra internet/git |
| `output file ... already exists (use --force)` | Thêm `--force` hoặc xoá file cũ |
| Muốn build sạch | Xoá thư mục `build_ubuntu_*` rồi chạy lại |