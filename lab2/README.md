# Lab 2 — Tự cài đặt AES-128 và CTR bằng C++17

`aestool` là chương trình dòng lệnh cài đặt **AES-128 từ đầu bằng C++** theo
FIPS-197 và chế độ **CTR** theo NIST SP 800-38A. Phần lõi AES/CTR không dùng
Crypto++, libsodium hay OpenSSL để mã hóa; OpenSSL chỉ được dùng làm
implementation tham chiếu cho cross-validation và benchmark.

> **Cảnh báo bảo mật:** CTR chỉ cung cấp tính bí mật, không cung cấp tính toàn
> vẹn/xác thực. Không dùng Lab 2 như một giao thức bảo mật hoàn chỉnh trong
> production. Không bao giờ tái sử dụng cặp `(key, IV)`.

## Mục lục

1. [Mục tiêu và phạm vi](#1-mục-tiêu-và-phạm-vi)
2. [Tính năng](#2-tính-năng)
3. [Cấu trúc dự án](#3-cấu-trúc-dự-án)
4. [Yêu cầu môi trường](#4-yêu-cầu-môi-trường)
5. [Biên dịch](#5-biên-dịch)
6. [Sử dụng nhanh](#6-sử-dụng-nhanh)
7. [Tham chiếu CLI](#7-tham-chiếu-cli)
8. [Đặc tả AES-CTR](#8-đặc-tả-aes-ctr)
9. [Kiểm thử và kiểm chứng](#9-kiểm-thử-và-kiểm-chứng)
10. [Benchmark và báo cáo](#10-benchmark-và-báo-cáo)
11. [Phân tích bảo mật](#11-phân-tích-bảo-mật)
12. [Giới hạn và lỗi thường gặp](#12-giới-hạn-và-lỗi-thường-gặp)
13. [Checklist hoàn thành/nộp bài](#13-checklist-hoàn-thànhnộp-bài)

---

## 1. Mục tiêu và phạm vi

Lab này hiện thực các yêu cầu chính của Lab 2:

- AES-128, khóa 128 bit, block 128 bit, 10 vòng theo FIPS-197.
- Các phép biến đổi `AddRoundKey`, `SubBytes`, `ShiftRows`, `MixColumns`,
  `KeyExpansion` và inverse cipher.
- CTR cho dữ liệu có độ dài tùy ý, không padding, hỗ trợ streaming.
- Ba backend S-box để phân tích đánh đổi hiệu năng/bộ nhớ/cache:
  `table`, `ttable`, `computed`.
- KAT từ FIPS-197 và NIST SP 800-38A, kiểm chứng với OpenSSL, unit test và
  kiểm thử CLI end-to-end.
- Đo throughput, latency và thống kê mean/median/stddev/95% CI.
- Kiểm tra input lỗi theo nguyên tắc **fail closed**.

AES-192/256, XTS và AES-NI/bit-sliced là các hạng mục bonus; bản hiện tại
chưa triển khai các bonus này.

## 2. Tính năng

### 2.1. AES và backend S-box

| Backend | Bảng tra | Đặc điểm |
|---|---:|---|
| `table` (mặc định) | S-box 256 B và inverse S-box 256 B | Byte-oriented, dễ đối chiếu FIPS-197; có truy cập bộ nhớ phụ thuộc dữ liệu |
| `ttable` | 4 bảng, mỗi bảng 1 KiB | Nhanh hơn trong một số máy; là mục tiêu kinh điển của cache-timing attack |
| `computed` | Không lưu S-box | Tính `S(x) = affine(x^254)` trong GF(2^8), giảm secret-indexed lookup nhưng chậm hơn đáng kể |

### 2.2. CTR

- IV là 16 byte, xem như `nonce || counter`.
- Counter là **toàn bộ block 128 bit theo big-endian**; byte thứ 15 là byte
  ít quan trọng nhất và được tăng trước.
- Không padding; block cuối được cắt theo đúng số byte plaintext.
- IV tự sinh dùng CSPRNG của hệ điều hành: 12 byte nonce ngẫu nhiên và 4 byte
  counter ban đầu bằng zero.
- Khi bỏ `--iv` lúc encrypt, IV được lưu ở `<output>.iv`; khi decrypt, IV
  được đọc từ `<input>.iv`.
- Counter không được wrap. Nếu cần tạo block sau `FF FF ... FF`, chương trình
  dừng với exit code `5` và không tạo output không hợp lệ.

### 2.3. I/O và an toàn lỗi

- Key AES-128 bắt buộc đúng 16 byte; IV bắt buộc đúng 16 byte.
- File mặc định là raw binary; output trên màn hình mặc định là hex khi
  encrypt.
- Có hỗ trợ hex/base64 cho input và output.
- Không ghi đè file nếu chưa có `--force`.
- Key/IV sai độ dài, file không tồn tại, mode không hỗ trợ, JSON KAT hỏng và
  counter overflow đều bị từ chối, không tiếp tục với giá trị mặc định.
- Windows nhận đối số UTF-16 rồi chuyển sang UTF-8; dữ liệu `--text` có thể
  chứa Unicode.

## 3. Cấu trúc dự án

```text
lab2/
├── CMakeLists.txt              # Cấu hình CMake, OpenSSL và Catch2
├── README.md                   # Tài liệu này
├── src/
│   ├── aes128.hpp/.cpp         # AES-128, key schedule, các backend S-box
│   ├── aes_tables.hpp          # S-box và inverse S-box
│   ├── ctr.hpp/.cpp            # CTR memory API và streaming file API
│   ├── errors.hpp              # Error và exit code
│   ├── json.hpp/.cpp           # Parser JSON tối thiểu cho KAT
│   ├── kat.hpp/.cpp            # KAT runner
│   ├── xval.hpp/.cpp           # Cross-validation với OpenSSL
│   ├── bench.hpp/.cpp          # Benchmark encrypt/decrypt
│   ├── stats.hpp/.cpp          # Mean, median, stddev, confidence interval
│   ├── util.hpp/.cpp           # File, encoding, CSPRNG và tiện ích
│   └── main.cpp                # CLI aestool
├── tests/
│   ├── test_aes.cpp            # Catch2 unit tests
│   └── cli_e2e.cmake           # CLI integration test
├── kat/
│   └── vectors.json            # FIPS-197, AESAVS và SP 800-38A vectors
├── scripts/
│   ├── build_linux.sh          # Build/test/KAT trên Linux
│   ├── build_windows.bat       # Build/test/KAT trên Windows
│   ├── build.sh                # Wrapper build Linux
│   ├── build.bat               # Wrapper build Windows
│   ├── bench.sh/.bat           # Benchmark theo protocol của lab
│   ├── demo.sh                 # Demo round-trip và negative tests
│   ├── plot_bench.py           # Vẽ đồ thị từ CSV
│   └── compare.py              # Tổng hợp benchmark nhiều CSV
├── gui/
│   └── aestool_gui.py          # GUI tùy chọn
├── release/
│   ├── ubuntu/                 # Binary Linux có sẵn (nếu được build)
│   └── windows/                # Binary Windows có sẵn (nếu được build)
└── Output/                     # Log, CSV và kết quả theo từng hệ điều hành
```

Tất cả lệnh bên dưới được chạy từ thư mục gốc `lab2/`, tức thư mục chứa
`CMakeLists.txt`.

## 4. Yêu cầu môi trường

### Bắt buộc

- CMake **>= 3.20**.
- Compiler hỗ trợ C++17: GCC, Clang, MSVC hoặc MinGW-w64.
- OpenSSL **>= 1.1** để cross-validation/benchmark.
- Catch2 v3 để chạy unit test. CMake ưu tiên bản đã cài; nếu không có,
  `FetchContent` sẽ tải Catch2 v3.5.3 trong lúc configure.
- Python 3 và `matplotlib` nếu cần vẽ biểu đồ.

### OpenSSL

CMake tự ưu tiên các thư mục theo compiler:

| Nền tảng | MSVC | GCC/MinGW | Clang |
|---|---|---|---|
| Windows | `D:/UIT/NT219/libs/openssl/msvc` | `D:/UIT/NT219/libs/openssl/gcc` | `D:/UIT/NT219/libs/openssl/clang` |
| Linux | `$HOME/Desktop/nt219lib/openssl/gcc` | `$HOME/Desktop/nt219lib/openssl/gcc` | `$HOME/Desktop/nt219lib/openssl/clang` |

Có thể ghi đè bằng `-DOPENSSL_ROOT_DIR=...`. Nếu dùng OpenSSL hệ thống,
đảm bảo `openssl`, header và thư viện tương ứng nằm trong PATH/include/library
để `find_package(OpenSSL REQUIRED)` tìm được.

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev pkg-config catch2
# Chỉ cần nếu muốn vẽ biểu đồ:
sudo apt install -y python3 python3-pip
python3 -m pip install --user matplotlib
```

### Windows

Cài CMake, Git và một trong các toolchain sau:

- MSVC: Visual Studio Build Tools, workload **Desktop development with C++**,
  sau đó mở **x64 Native Tools Command Prompt for VS 2022**.
- MinGW-w64: dùng MSYS2 MinGW64/UCRT64, bảo đảm `g++.exe` và
  `mingw32-make.exe` có trong PATH.
- Clang: LLVM/Clang và generator phù hợp.

## 5. Biên dịch

### 5.1. Linux — script khuyến nghị

```bash
cd lab2
scripts/build_linux.sh gcc
scripts/build_linux.sh clang
```

Script thực hiện configure, build, `ctest` và KAT. Xóa build hiện tại trước
khi build lại:

```bash
scripts/build_linux.sh gcc --clean
```

### 5.2. Linux — CMake thủ công

```bash
cd lab2
cmake -S . -B build_ubuntu_gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build_ubuntu_gcc -j"$(nproc)"
ctest --test-dir build_ubuntu_gcc --output-on-failure
```

Với Clang:

```bash
cmake -S . -B build_ubuntu_clang \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build_ubuntu_clang -j"$(nproc)"
ctest --test-dir build_ubuntu_clang --output-on-failure
```

Tùy chọn hữu ích:

```bash
cmake -S . -B build_ubuntu_gcc \
  -DBUILD_TESTS=OFF \
  -DLAB2_WERROR=OFF
```

`LAB2_WERROR` mặc định bật trên GCC/Clang; chỉ tắt khi cần chẩn đoán warning
do toolchain.

### 5.3. Windows — script khuyến nghị

Chạy từ **Command Prompt**, không phải PowerShell:

```bat
cd /d D:\UIT\NT219\crypto-labs\lab2
scripts\build_windows.bat msvc
scripts\build_windows.bat gcc
scripts\build_windows.bat clang
```

Script tự chọn thư mục `build_windows_msvc`, `build_windows_gcc` hoặc
`build_windows_clang`, chạy test và KAT. Xóa build:

```bat
scripts\build_windows.bat msvc clean
```

### 5.4. Windows — CMake thủ công

MSVC (chạy trong x64 Native Tools Command Prompt):

```bat
cd /d D:\UIT\NT219\crypto-labs\lab2
cmake -S . -B build_windows_msvc -DCMAKE_BUILD_TYPE=Release ^
  -DOPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/msvc
cmake --build build_windows_msvc --config Release
ctest --test-dir build_windows_msvc -C Release --output-on-failure
```

MinGW-w64:

```bat
cmake -S . -B build_windows_gcc -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DOPENSSL_ROOT_DIR=D:/UIT/NT219/libs/openssl/gcc
cmake --build build_windows_gcc
ctest --test-dir build_windows_gcc --output-on-failure
```

Binary thường nằm ở:

```text
MSVC:  build_windows_msvc\Release\aestool.exe
MinGW: build_windows_gcc\aestool.exe
Clang: build_windows_clang\aestool.exe
```

### 5.5. Binary dựng sẵn

Nếu có binary trong `release/`, không cần build lại để thử CLI:

```bash
cd lab2
./release/ubuntu/aestool_gcc --help
./release/ubuntu/aestool_gcc --kat kat/vectors.json
```

```powershell
cd D:\UIT\NT219\crypto-labs\lab2
Get-ChildItem release\windows
& .\release\windows\aestool_gcc.exe --help
```

## 6. Sử dụng nhanh

### 6.1. Tạo key, encrypt và decrypt bằng IV sidecar

```bash
cd lab2
AESTOOL=./build_ubuntu_gcc/aestool
$AESTOOL keygen --out work-key.bin
$AESTOOL encrypt --mode ctr --key work-key.bin \
  --in message.bin --out ciphertext.bin
# Sinh ciphertext.bin.iv; decrypt tự đọc sidecar này
$AESTOOL decrypt --mode ctr --key work-key.bin \
  --in ciphertext.bin --out recovered.bin
cmp message.bin recovered.bin
```

PowerShell:

```powershell
$AESTOOL = ".\build_windows_msvc\Release\aestool.exe"
& $AESTOOL keygen --out work\key.bin
& $AESTOOL encrypt --key work\key.bin --in work\message.bin --out work\ct.bin
& $AESTOOL decrypt --key work\key.bin --in work\ct.bin --out work\recovered.bin
fc /b work\message.bin work\recovered.bin
```

Tạo IV riêng và truyền rõ ràng:

```bash
$AESTOOL ivgen --out iv.bin
$AESTOOL encrypt --key work-key.bin --iv iv.bin \
  --in message.bin --out ciphertext.bin
$AESTOOL decrypt --key work-key.bin --iv iv.bin \
  --in ciphertext.bin --out recovered.bin
```

Khi truyền IV thủ công, chương trình cảnh báo vì người dùng phải tự bảo đảm
không tái sử dụng IV với cùng key.

### 6.2. Dữ liệu text và encoding

```bash
# Key hex phải có đúng 32 ký tự hex = 16 byte
$AESTOOL encrypt \
  --key-hex 2b7e151628aed2a6abf7158809cf4f3c \
  --text "xin chào"

# Đọc plaintext hex và ghi ciphertext base64
$AESTOOL encrypt --key work-key.bin \
  --in message.hex --in-encode hex \
  --out ciphertext.b64 --encode base64
```

Không dùng `--key-hex` cho secret production vì key có thể xuất hiện trong
process listing hoặc shell history; ưu tiên `--key FILE`.

## 7. Tham chiếu CLI

Xem toàn bộ usage bằng:

```bash
aestool --help
```

### Lệnh

| Lệnh | Mục đích |
|---|---|
| `encrypt` / `decrypt` | CTR transform; encrypt và decrypt dùng cùng phép XOR keystream |
| `keygen` | Sinh key AES-128 16 byte bằng OS CSPRNG |
| `ivgen` | Sinh IV 16 byte: 12 byte nonce + 4 byte counter zero |
| `--kat FILE` | Chạy Known Answer Tests, in PASS/FAIL từng case và tổng kết |
| `xval` | So sánh kết quả với OpenSSL |
| `bench` | Benchmark backend và reference implementation |

### Tùy chọn chính

| Tùy chọn | Quy tắc |
|---|---|
| `--mode ctr` | Chỉ `ctr` được hỗ trợ |
| `--key FILE` / `--key-hex HEX` | Chọn đúng một; key đúng 16 byte/32 hex |
| `--iv FILE` / `--iv-hex HEX` | IV đúng 16 byte/32 hex |
| `--in FILE` / `--text STR` | Chọn đúng một input |
| `--in-encode raw\|hex\|base64` | Encoding của input |
| `--out FILE` | File output; mặc định raw binary |
| `--encode raw\|hex\|base64` | Encoding output |
| `--sbox table\|ttable\|computed` | Chọn backend AES |
| `--force` | Cho phép ghi đè output |
| `--quiet` / `--verbose` | Giảm/tăng thông tin chẩn đoán |

### Exit code

| Mã | Ý nghĩa |
|---:|---|
| `0` | Thành công |
| `1` | Lỗi nội bộ |
| `2` | Sai cú pháp/usage |
| `3` | Input không hợp lệ |
| `4` | Lỗi I/O hoặc output đã tồn tại |
| `5` | Counter overflow |
| `6` | KAT hoặc cross-validation thất bại |

### KAT và cross-validation

```bash
$AESTOOL --kat kat/vectors.json
$AESTOOL --kat kat/vectors.json --verbose
$AESTOOL xval --ref openssl --cases 200
$AESTOOL xval --ref all --cases 200
```

`--kat` trả exit code `6` nếu có vector fail. `xval` dùng các input ngẫu
nhiên độc lập để so sánh AES/CTR thuần với reference.

## 8. Đặc tả AES-CTR

Với key `K`, IV `C_0` và plaintext block `P_i`:

```text
S_i = AES_K(C_i)
C_i = C_0 + i  (modulo 2^128, big-endian)
Ciphertext_i = P_i XOR S_i
```

Trong implementation này phép cộng modulo không được phép wrap khi yêu cầu
block mới vượt quá `FF...FF`; thay vào đó ném `CounterOverflow`. Đây là
chủ ý để tránh tái sử dụng keystream do wrap-around.

CTR không cần padding. Với plaintext 23 byte, chỉ 23 byte đầu của keystream
block cuối được XOR. Encrypt và decrypt là cùng một phép toán:

```text
P_i = Ciphertext_i XOR S_i
```

### IV và two-time pad

Nếu cùng `(key, IV)` được dùng cho hai plaintext:

```text
C1 XOR C2 = (P1 XOR S) XOR (P2 XOR S) = P1 XOR P2
```

Keystream bị lộ theo cách này có thể giúp khôi phục thông tin của cả hai
message. CTR cũng không phát hiện ciphertext bị sửa: sửa một bit ciphertext
sẽ sửa đúng bit tương ứng trong plaintext và chương trình vẫn có thể exit `0`.
Ứng dụng thực tế phải dùng AEAD (ví dụ AES-GCM) hoặc CTR kết hợp MAC với
quy trình xác thực trước khi sử dụng plaintext.

## 9. Kiểm thử và kiểm chứng

### 9.1. `ctest`

```bash
ctest --test-dir build_ubuntu_gcc --output-on-failure
```

Các nhóm test gồm:

- AES-128 key schedule và vector FIPS-197.
- S-box/inverse S-box và cả ba backend.
- CTR vector SP 800-38A, block cuối không đủ, chunking và carry.
- Counter overflow, key/IV sai, file lỗi và overwrite protection.
- Wrong key/IV, ciphertext tamper và hiện tượng CTR malleability.
- Hex/base64, JSON KAT, streaming file và CLI end-to-end.

### 9.2. Negative tests tối thiểu

```bash
# Sai key và sai IV phải cho plaintext sai
$AESTOOL keygen --out work/key-wrong.bin
$AESTOOL decrypt --key work/key-wrong.bin \
  --in ciphertext.bin --out wrong-key.bin --force

$AESTOOL decrypt --key work-key.bin \
  --iv-hex 00000000000000000000000000000001 \
  --in ciphertext.bin --out wrong-iv.bin --force

# Key/IV sai độ dài: kỳ vọng exit 3
$AESTOOL encrypt --key-hex 0011 --text hi
$AESTOOL decrypt --key work-key.bin --iv-hex 00 \
  --in ciphertext.bin --out invalid.bin

# IV toàn FF và input cần block kế tiếp: kỳ vọng exit 5
$AESTOOL encrypt --key work-key.bin \
  --iv-hex ffffffffffffffffffffffffffffffff \
  --text "counter overflow"
```

Không xem “decrypt sai key không báo lỗi” là bug của CTR: vì CTR không có
authentication, mọi 16-byte key đều là một keystream hợp lệ.

### 9.3. Log kết quả

Tạo thư mục log theo hệ điều hành trước khi chạy:

```bash
mkdir -p Output/ubuntu/kat Output/ubuntu/xval Output/ubuntu/bench
```

Trên Windows:

```bat
mkdir Output\windows\kat Output\windows\xval Output\windows\bench
```

Các script build tự lưu KAT vào `Output/<OS>/kat/`. Có thể chuyển hướng
stdout/stderr để lưu thêm log build, ctest và negative tests.

## 10. Benchmark và báo cáo

### 10.1. Chạy benchmark

Benchmark mặc định đo encrypt/decrypt, warm-up, nhiều lần chạy và ghi CSV:

```bash
$AESTOOL bench --sizes 1MiB,100MiB \
  --backends table,ttable --ref openssl \
  --runs 30 --csv Output/ubuntu/bench/bench_gcc.csv
```

Đo backend `computed` riêng vì chậm:

```bash
$AESTOOL bench --sizes 64KiB,1MiB \
  --backends computed --ref none --runs 30 \
  --csv Output/ubuntu/bench/bench_computed.csv
```

Theo protocol đầy đủ của lab:

```bash
scripts/bench.sh
```

Windows:

```bat
scripts\bench.bat
```

Các script tạo benchmark cho 1 MiB, 100 MiB và 1 GiB (30 lần cho 1 GiB;
1 GiB có thể bỏ qua nếu không đủ RAM/thời gian).

### 10.2. Thống kê và đồ thị

Tool ghi mean, median, standard deviation và 95% confidence interval. Tùy
chọn quan trọng:

| Tùy chọn | Ý nghĩa |
|---|---|
| `--sizes` | Ví dụ `1MiB,100MiB,1GiB` |
| `--runs` | Số lần chạy độc lập, nên `>= 30` |
| `--backends` | `table,ttable,computed` |
| `--ref` | `all`, `openssl` hoặc `none` |
| `--dir` | `enc`, `dec` hoặc `both` |
| `--ops` | Số operation mỗi run; dùng `1000` khi cần bám literal đề |
| `--warmup` | Thời gian warm-up |
| `--target-sec` | Thời lượng mục tiêu mỗi run |
| `--csv` | Đường dẫn CSV |

Vẽ đồ thị:

```bash
python3 scripts/plot_bench.py Output/ubuntu/bench/bench_gcc.csv
python3 scripts/compare.py Output/ubuntu/bench/bench_gcc.csv \
  Output/windows/bench/bench_windows_1MiB.csv
```

Trong báo cáo cần ghi:

- CPU, số core/thread, RAM, OS, compiler, CMake, OpenSSL và compile flags.
- Governor Linux ở `performance`; Windows power plan ở `High performance`.
- Số run, warm-up, payload size, backend, hướng encrypt/decrypt và reference.
- Bảng throughput MB/s, latency và error bar/95% CI.
- Phân tích `table`/`ttable`/`computed`: bộ nhớ, cache, branch prediction,
  secret-dependent index và đánh đổi hiệu năng–rò rỉ thông tin.
- So sánh implementation thuần với OpenSSL; không dùng PRNG seeded cho
  key/IV. Seed cố định chỉ được dùng để tạo dữ liệu benchmark giả.

## 11. Phân tích bảo mật

README/report phải nêu rõ các điểm sau:

1. **Table S-box và T-table:** lookup theo byte bí mật có thể làm lộ pattern
   cache; đối thủ đo timing/cache có thể suy luận key.
2. **Computed S-box:** loại bỏ bảng lookup bí mật trong lõi S-box nhưng không
   tự động chứng minh toàn bộ chương trình constant-time; I/O và XOR vẫn cần
   được đánh giá.
3. **CTR không xác thực:** ciphertext tamper không bị phát hiện; cần MAC hoặc
   AEAD, ưu tiên dùng AES-GCM trong ứng dụng mới.
4. **Nonce/IV uniqueness:** random IV không thay thế cho nonce registry; các
   process độc lập vẫn có thể tái sử dụng IV thủ công.
5. **AES-NI:** implementation phần cứng thường nhanh hơn và giảm rủi ro
   lookup-table timing, nhưng phải kiểm tra CPU/runtime và đo riêng.
6. **Fail closed:** input sai phải dừng trước khi tạo output; counter overflow
   không được wrap; output tồn tại không bị ghi đè nếu thiếu `--force`.
7. **Threat model:** phân biệt đối thủ đọc file, sửa ciphertext, đo cache/timing
   và người dùng vô tình reuse IV; nêu rõ giới hạn của demo offline này.

## 12. Giới hạn và lỗi thường gặp

- Chỉ hỗ trợ AES-128 và CTR; không phải AES-192/256, XTS hay AEAD.
- `table` và `ttable` có secret-dependent memory access theo chủ ý để phục vụ
  phân tích lab; không coi đây là implementation chống side-channel.
- Không có nonce database giữa các lần chạy; chương trình chỉ cảnh báo khi
  IV được truyền thủ công.
- `--key-hex` có thể lộ secret trong process listing/shell history.
- Nếu decrypt không truyền `--iv`, file input phải có sidecar `<input>.iv`.
- `--out` đã tồn tại cần `--force`; với encrypt tự sinh IV, sidecar cũng được
  kiểm tra để tránh trạng thái ciphertext/IV không nhất quán.
- Build Windows Clang có thể bỏ qua unit test do ABI không tương thích với
  thư viện OpenSSL/GCC dựng sẵn; đọc warning của CMake và ghi rõ trong report.
- `Catch2` không tìm thấy có thể khiến configure tải dependency qua mạng; dùng
  bản Catch2 v3 cài sẵn nếu môi trường offline.

## 13. Checklist hoàn thành/nộp bài

### Code và build

- [ ] Không dùng thư viện crypto ngoài trong AES/CTR core.
- [ ] Build out-of-source bằng GCC/Clang trên Ubuntu.
- [ ] Build bằng MSVC và/hoặc MinGW-w64 trên Windows.
- [ ] `ctest --output-on-failure` pass; log được lưu.
- [ ] Có binary Linux và Windows theo yêu cầu nộp bài.

### Đúng đắn và an toàn

- [ ] FIPS-197 AES KAT pass trên backend cần kiểm tra.
- [ ] SP 800-38A CTR KAT pass.
- [ ] `xval` khớp OpenSSL.
- [ ] Test sai key, sai IV, tamper, input sai độ dài và counter overflow.
- [ ] README/report ghi rõ big-endian counter, overflow và sidecar IV.

### Hiệu năng

- [ ] Có CSV cho encrypt/decrypt ở 1 MiB, 100 MiB và 1 GiB nếu khả thi.
- [ ] Có ít nhất 30 run độc lập, warm-up và 95% CI.
- [ ] Có bảng hardware/OS/compiler/flags và biểu đồ error bar.
- [ ] So sánh `table`, `ttable`, `computed` và OpenSSL.

### Báo cáo và hồ sơ nộp

- [ ] Báo cáo theo mạch: Objective → Design & Security → Test Methodology →
  Performance → Discussion → Conclusion → Appendix.
- [ ] Có build log, test log, KAT summary, benchmark CSV/plot và negative-test
  evidence.
- [ ] Có bảng tự chấm điểm theo rubric.
- [ ] Có cam kết liêm chính học thuật và khai báo việc dùng AI/công cụ nếu có.
- [ ] Đóng gói source, CMake, README, test, binary và report theo yêu cầu môn
  học; không đưa secret thật vào repository.

