# LAB 2 — Yêu cầu (dịch từ đề gốc) và toàn bộ lệnh cần chạy để lấy đủ điểm

> Dựa trên **đề chính thức trong PDF** (Lab 2 + phần "Common Requirements"). Tên CLI giữ là `aestool`.
> Mục 1–3: yêu cầu. Mục 4: bảng đối chiếu rubric → bằng chứng. Mục 5 (Ubuntu) và Mục 6 (Windows): **mọi lệnh cần chạy**. Mục 7: phần phải tự viết trong báo cáo (không có lệnh nào làm thay được).
> Quy ước: chạy mọi lệnh từ thư mục gốc dự án `lab2/` (nơi có `CMakeLists.txt`). Log/bằng chứng lưu vào `Output/ubuntu/`.

---

## 1. Ràng buộc

- **Không dùng thư viện mật mã ngoài** (OpenSSL, Crypto++, libsodium…) trong phần cài đặt AES/CTR; chỉ được dùng STL.
- Mọi nguyên thủy mật mã **tự cài đặt**. AES-NI **chỉ** được dùng ở phần bonus.
- OpenSSL/Crypto++ chỉ dùng để **đối chiếu kết quả** (cross-validation) và **so sánh hiệu năng**.
- Biên dịch được trên **Ubuntu LTS** và **Windows (MSVC và MinGW64)**; bắt buộc **CMake**, build **out-of-source**.
- Ngôn ngữ C++17 trở lên; input/output UTF-8; **fail closed** với input sai định dạng.

## 2. Yêu cầu kỹ thuật của Lab 2

### 2.1. AES-128 (FIPS-197, bản cập nhật 09/05/2023) — 30 điểm
- Bắt buộc cài: `AddRoundKey`, `SubBytes` (tra S-box), `ShiftRows`, `MixColumns`, `KeyExpansion`.
- Khối 128 bit, khóa 128 bit, 10 vòng. Tuân thủ nghiêm ngặt FIPS-197.

### 2.2. Chế độ CTR (NIST SP 800-38A) — 15 điểm
- IV 128 bit (nonce + counter), bộ đếm tăng đúng; plaintext độ dài tùy ý; xử lý đúng khối cuối thiếu; **không padding**.
- **Phải ghi rõ trong tài liệu:** endianness của counter và hành vi khi counter tràn.
- Quy tắc then chốt: IV phải **duy nhất cho mỗi khóa**; tái sử dụng IV phải được phân tích là **thảm họa**.

### 2.3. CLI và I/O
```
aestool encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
aestool decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin  --out msg.bin
```
- Input/output raw binary; tùy chọn hex để debug; **key đúng 16 byte, IV đúng 16 byte**.
- Phải kiểm tra (fail closed): độ dài key sai, độ dài IV sai, **tràn counter**, lỗi I/O file. Mọi input sai phải dừng an toàn.
- IV thiếu → sinh ngẫu nhiên bằng RNG an toàn của OS và lưu (header/sidecar); không dùng `rand()`/`std::random`.
- Hiển thị trên màn hình mặc định là hex; ghi file mặc định là raw.

### 2.4. Tính đúng đắn (KAT + cross-validation) — 15 điểm
- KAT: vector **FIPS-197** và vector CTR **NIST SP 800-38A**; có runner `--kat vectors.json` in **PASS/FAIL từng case + thống kê tổng**.
- Ciphertext phải **trùng khít** với Crypto++ **hoặc** OpenSSL AES-128-CTR (cùng key và IV).
- Negative tests: sai key → plaintext sai; sai IV → plaintext sai; ciphertext bị sửa → plaintext hỏng; **giải thích vì sao CTR không phát hiện được việc bị sửa**.

### 2.5. Hiệu năng — 10 điểm
- Đo throughput **mã hóa** và **giải mã** AES-128 với file **1 MiB, 100 MiB, 1 GiB (nếu khả thi)**.
- Phân tích bắt buộc: S-box bảng tra vs S-box tính toán; dung lượng bộ nhớ; hành vi cache; ảnh hưởng dự đoán rẽ nhánh; so sánh với Crypto++.
- Cần: bảng throughput (MB/s) và biểu đồ throughput.
- Phương pháp chung: warm-up 1–2 s; khối ~1.000 phép toán; lặp 30–100 lần (N ≥ 30 lần chạy độc lập); báo cáo **mean, median, độ lệch chuẩn, khoảng tin cậy 95%**, biểu đồ có error bar; bảng phần cứng/OS/compiler/flags; CPU governor ở performance (Linux) / High performance (Windows); PRNG seed cố định chỉ dùng cho dữ liệu giả, **không dùng cho key/IV**.

### 2.6. Thảo luận bảo mật (mục bắt buộc trong báo cáo) — 20 điểm
- **Lõi AES:** rò rỉ timing của S-box dạng bảng; tấn công cache (T-table); tầm quan trọng của constant-time.
- **CTR:** tấn công tái dùng keystream; two-time pad; vì sao tái dùng IV là thảm họa; vì sao CTR chỉ có bảo mật (confidentiality); vì sao cần MAC; vì sao CTR không phải AEAD.
- **Phần cứng:** vì sao AES-NI giảm nhiều rủi ro rò rỉ; đánh đổi hiệu năng ↔ bảo mật.
- Báo cáo phải có: threat model, kịch bản dùng sai, các tấn công liên quan, cấu hình mặc định an toàn, hành vi fail-closed, giới hạn của cài đặt.

### 2.7. Bonus (tối đa +20)
| Bonus | Điểm | Nội dung |
|---|---|---|
| AES-192/256 | +10 | lịch khóa đầy đủ + KAT bắt buộc |
| AES-XTS | +5 | 2 khóa, nhân tweak trong GF(2¹²⁸), giải thích mã hóa ổ đĩa |
| AES-NI hoặc bit-sliced | +5 | phát hiện CPU lúc chạy, so sánh hiệu năng, giải thích constant-time |

> Code hiện tại **chưa** có các bonus này.

## 3. Yêu cầu chung (áp dụng cho Lab 2)

- **Build:** `mkdir build && cd build && cmake .. && cmake --build .`; compile được trên Ubuntu LTS, Windows MSVC và MinGW64.
- **README.md** phải có: danh sách phụ thuộc (kèm phiên bản), hướng dẫn cài (Windows & Linux), lệnh build cả hai HĐH, ví dụ CLI, hạn chế đã biết.
- **Unit test:** Catch2 hoặc GoogleTest; `ctest` phải pass trên cả Windows và Linux.
- **Báo cáo** (một file PDF/DOCX cho cả 6 lab, ~60 trang): mục tiêu, chi tiết cài đặt, kết quả test, phương pháp hiệu năng, bảng/biểu đồ thống kê, phân tích bảo mật, so sánh đa nền tảng, bài học. Theo mẫu: Objective → Design & security → Test methodology → Performance → Discussion → Conclusion → Appendix (ví dụ CLI, build log, độ phủ KAT, tóm tắt unit test).
- **Nộp bài:** mã nguồn, CMake, README, unit test, **binary Windows + Linux**, báo cáo, **bảng tự chấm điểm**, **cam kết liêm chính học thuật** (khai báo việc dùng AI/công cụ nếu có, trích dẫn nguồn). Gói chung: `MSV.All6labs.rar`.
- Phần tấn công/demo chỉ dùng artifact tự tạo, trong môi trường offline; key trong repo chỉ là key test.

---

## 4. Bảng đối chiếu rubric → bằng chứng

| Hạng mục (điểm) | Bằng chứng cần có | Lệnh/mục |
|---|---|---|
| Lõi AES-128 đúng FIPS-197 (30) | KAT FIPS-197 pass trên cả 3 backend; kiểm tra key schedule App. A.1 | 5.3, 6.3 + `ctest` |
| CTR đúng (15) | SP 800-38A pass; chia chunk bất kỳ; khối cuối lẻ; carry; tràn counter bị chặn; tài liệu endianness/overflow | 5.3, 5.6, README |
| KAT & cross-validation (15) | `--kat` PASS/FAIL + tổng kết; `xval` khớp OpenSSL; so sánh `openssl enc` | 5.3, 5.4, 6.3, 6.4 |
| Chất lượng kỹ thuật & tài liệu (10) | build sạch 2 HĐH, README, ctest pass, fail-closed, binary hai HĐH | 5.1–5.2, 6.1–6.2, 5.9, 6.9 |
| Hiệu năng (10) | CSV + bảng + biểu đồ; header phần cứng; N ≥ 30; CI 95% | 5.7–5.8, 6.7–6.8 |
| Thảo luận bảo mật (20) | phần viết tay (Mục 7) + demo negative test | 5.5, 6.5, Mục 7 |
| Bonus (+20) | chưa làm | — |

---

## 5. UBUNTU — toàn bộ lệnh

### 5.0. Cài đặt (một lần)
```bash
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev pkg-config catch2 clang python3-matplotlib xxd
cmake --version      # phải >= 3.20
mkdir -p Output/ubuntu/kat Output/ubuntu/bench Output/ubuntu/xval Output/ubuntu/bin work
```

### 5.1. Thông tin hệ thống (điền bảng phần cứng trong báo cáo)
```bash
{ echo "== OS"; lsb_release -a 2>/dev/null; uname -a
  echo "== CPU"; lscpu
  echo "== RAM"; free -h
  echo "== Compilers"; g++ --version | head -1; clang++ --version | head -1; cmake --version | head -1; openssl version
} 2>&1 | tee Output/ubuntu/ubuntu_sysinfo.txt
sudo apt install -y linux-tools-common linux-tools-generic   # nếu chưa có cpupower
sudo cpupower frequency-set -g performance                    # cố định governor trước khi đo
cpupower frequency-info -p | tee -a Output/ubuntu/ubuntu_sysinfo.txt
```

### 5.2. Build + ctest (GCC và Clang)
```bash
# Cách A: script
chmod +x scripts/*.sh
scripts/build.sh gcc   2>&1 | tee Output/ubuntu/ubuntu_gcc_build_ctest.txt
scripts/build.sh clang 2>&1 | tee Output/ubuntu/ubuntu_clang_build_ctest.txt

# Cách B: thủ công (out-of-source)
mkdir build_ubuntu_gcc && cd build_ubuntu_gcc
cmake .. && cmake --build . -j"$(nproc)" && ctest --output-on-failure
cd ..
mkdir build_ubuntu_clang && cd build_ubuntu_clang
CC=clang CXX=clang++ cmake .. && cmake --build . -j"$(nproc)" && ctest --output-on-failure
cd ..

# (Tùy chọn, tăng điểm chất lượng) ASan + UBSan
mkdir build_ubuntu_asan && cd build_ubuntu_asan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
cmake --build . -j"$(nproc)" && ctest --output-on-failure 2>&1 | tee ../Output/ubuntu/ubuntu_asan_ctest.txt
cd ..
```

### 5.3. KAT + unit test (đặt biến cho gọn; đổi `build_ubuntu_clang` để chạy lại cho Clang)
```bash
B=build_ubuntu_gcc ; T=$B/aestool
$B/aes_tests 2>&1 | tee Output/ubuntu/ubuntu_gcc_unit_tests.txt
$T --kat kat/vectors.json            | tee Output/ubuntu/ubuntu_gcc_kat.txt
$T --kat kat/vectors.json --verbose  > Output/ubuntu/ubuntu_gcc_kat_verbose.txt
echo "exit=$?"
```

### 5.4. Cross-validation
```bash
$T xval --ref openssl --cases 200 | tee Output/ubuntu/ubuntu_gcc_xval.txt

# Đối chiếu độc lập bằng chính lệnh `openssl enc` (cùng key + IV)
head -c 100000 /dev/urandom > work/msg.bin
K=2b7e151628aed2a6abf7158809cf4f3c ; IV=f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
rm -f work/ct.bin work/ct_ossl.bin
$T encrypt --key-hex $K --iv-hex $IV --in work/msg.bin --out work/ct.bin --quiet
openssl enc -aes-128-ctr -K $K -iv $IV -in work/msg.bin -out work/ct_ossl.bin
cmp work/ct.bin work/ct_ossl.bin && echo "MATCHES OPENSSL" | tee Output/ubuntu/ubuntu_openssl_cli_match.txt
```

### 5.5. Negative tests + hành vi fail-closed
```bash
bash scripts/demo.sh $PWD/$T 2>&1 | tee Output/ubuntu/ubuntu_negative_tests.txt

# Key/IV sai độ dài, mode không hỗ trợ, ghi đè, tràn counter — kiểm tra exit code
{
$T encrypt --key-hex 0011 --text hi;                                           echo "short key      exit=$? (mong đợi 3)"
$T encrypt --key-hex $K --iv-hex 00 --text hi;                                 echo "short IV       exit=$? (mong đợi 3)"
$T encrypt --key-hex $K --iv-hex ffffffffffffffffffffffffffffffff --in work/msg.bin --out work/o.bin --force --quiet
                                                                                echo "counter overflow exit=$? (mong đợi 5)"; ls work/o.bin* 2>&1
$T encrypt --mode gcm --key-hex $K --text hi;                                  echo "bad mode       exit=$? (mong đợi 2)"
$T encrypt --key-hex $K --in work/khong_ton_tai.bin --out work/x.bin;          echo "missing input  exit=$? (mong đợi 4)"
$T decrypt --key-hex $K --iv-hex 00000000000000000000000000000000 --text zz --in-encode hex; echo "bad hex  exit=$? (mong đợi 3)"
} 2>&1 | tee Output/ubuntu/ubuntu_failclosed.txt

# KAT file hỏng / vector sai phải làm runner FAIL
echo '{ not json' > work/bad.json ; $T --kat work/bad.json ; echo "malformed json exit=$? (mong đợi 3)"
sed 's/69c4e0d86a7b0430d8cdb78070b4c55a/00000000000000000000000000000000/' kat/vectors.json > work/bad_kat.json
$T --kat work/bad_kat.json | tail -3 ; echo "wrong vector exit=${PIPESTATUS[0]} (mong đợi 6)"
```

### 5.6. File lớn (streaming, nhiều GiB) + round trip
```bash
rm -f work/big.* work/k.bin
head -c 1073741824 /dev/urandom > work/big.bin             # 1 GiB
$T keygen --out work/k.bin --quiet
{ time $T encrypt --key work/k.bin --in work/big.bin --out work/big.enc --quiet --sbox ttable ; } 2>&1 | tee Output/ubuntu/ubuntu_big_encrypt.txt
{ time $T decrypt --key work/k.bin --in work/big.enc --out work/big.dec --quiet --sbox ttable ; } 2>&1 | tee Output/ubuntu/ubuntu_big_decrypt.txt
sha256sum work/big.bin work/big.dec | tee Output/ubuntu/ubuntu_big_sha256.txt      # 2 hash phải giống nhau
rm -f work/big.*
```

### 5.7. Benchmark (làm với GCC; lặp lại với `B=build_ubuntu_clang` và đổi tên file)
```bash
# Lõi: table + ttable so với OpenSSL, size 1 MiB và 100 MiB, N=30, warm-up 1.5 s (mặc định)
$T bench --sizes 1MiB,100MiB --backends table,ttable --ref openssl --runs 30 \
         --csv Output/ubuntu/bench_ubuntu_gcc.csv | tee Output/ubuntu/bench_ubuntu_gcc.txt

# Backend computed (S-box tính toán, ~1 MB/s): chỉ đo size nhỏ
$T bench --sizes 64KiB,1MiB --backends computed --ref none --runs 30 \
         --csv Output/ubuntu/bench_ubuntu_gcc_computed.csv | tee Output/ubuntu/bench_ubuntu_gcc_computed.txt

# Làm đúng chữ "~1000 phép toán / khối" cho size nhỏ
$T bench --sizes 1MiB --backends ttable --ref openssl --runs 30 --ops 1000 \
         --csv Output/ubuntu/bench_ubuntu_gcc_ops1000.csv | tee Output/ubuntu/bench_ubuntu_gcc_ops1000.txt

# 1 GiB (nếu khả thi; rất lâu; ghi rõ trong báo cáo nếu giảm số lần chạy)
$T bench --sizes 1GiB --backends ttable --ref openssl --runs 30 --dir enc \
         --csv Output/ubuntu/bench_ubuntu_gcc_1gib.csv | tee Output/ubuntu/bench_ubuntu_gcc_1gib.txt
```
Ghi nhận trong báo cáo: header `bench` in ra (CPU, RAM, OS, compiler, flags), chỉ số mean/median/stddev/CI95.

### 5.8. Biểu đồ
```bash
python3 scripts/plot_bench.py Output/ubuntu/bench_ubuntu_gcc.csv Output/ubuntu/plot_ubuntu_gcc
# -> Output/ubuntu/plot_ubuntu_gcc_throughput.png, Output/ubuntu/plot_ubuntu_gcc_latency.png
python3 scripts/plot_bench.py Output/ubuntu/bench_ubuntu_gcc_computed.csv Output/ubuntu/plot_ubuntu_gcc_computed
```

### 5.9. Đóng gói binary + nộp
```bash
cp build_ubuntu_gcc/aestool   release/ubuntu/aestool_gcc
cp build_ubuntu_clang/aestool release/ubuntu/aestool_clang
ldd release/ubuntu/aestool_gcc | tee Output/ubuntu/ubuntu_ldd.txt      # ghi nhận phụ thuộc (libcrypto)
sudo cpupower frequency-set -g powersave   # (tuỳ chọn) trả governor về như cũ
# Nén (loại thư mục build và file tạm)
cd .. && zip -r lab2_submit.zip lab2 -x "lab2/build_*/*" "lab2/work/*"
```

---

## 6. WINDOWS — toàn bộ lệnh (cmd)

> MSVC: mở **"x64 Native Tools Command Prompt for VS 2022"**. MinGW: `g++` và `mingw32-make` trong PATH. Clang: LLVM + Ninja trong PATH.
> Trong cmd, `> file 2>&1` ghi log nhưng không hiện trên màn hình; muốn vừa xem vừa ghi, dùng PowerShell `... | Tee-Object file`.

### 6.0. Cài đặt (một lần)
CMake ≥ 3.20, Git, Visual Studio Build Tools (C++), MinGW-w64, (tuỳ chọn) LLVM + Ninja, Python + matplotlib (`pip install matplotlib`), và các bản OpenSSL riêng cho từng compiler tại
`D:\UIT\NT219\libs\openssl\{msvc,gcc,clang}` (hoặc truyền `-DOPENSSL_ROOT_DIR=...`).
```bat
mkdir Output\windows\kat Output\windows\bench Output\windows\xval Output\windows\bin work
cmake --version
```

### 6.1. Thông tin hệ thống
```bat
(systeminfo | findstr /B /C:"OS Name" /C:"OS Version" /C:"Total Physical Memory" ^
 & powershell -NoProfile -Command "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | Format-List" ^
 & cl 2^>^&1 ^| findstr /C:"Version" ^
 & g++ --version ^& clang++ --version ^& cmake --version) > Output\windows\windows_sysinfo.txt 2>&1
powercfg /setactive SCHEME_MIN
powercfg /getactivescheme >> Output\windows\windows_sysinfo.txt
```
(Nếu lệnh gộp ở trên phức tạp, chạy riêng từng lệnh và ghi tay vào bảng phần cứng của báo cáo.)

### 6.2. Build + ctest (MSVC, MinGW, Clang)
```bat
:: Cách A: script
scripts\build.bat msvc  > Output\windows\windows_msvc_build_ctest.txt 2>&1
scripts\build.bat mingw > Output\windows\windows_mingw_build_ctest.txt 2>&1

:: Cách B: thủ công — MSVC
mkdir build_windows_msvc & cd build_windows_msvc
cmake ..
cmake --build . --config Release
ctest -C Release --output-on-failure
cd ..

:: MinGW-w64 (GCC)
mkdir build_windows_gcc & cd build_windows_gcc
cmake .. -G "MinGW Makefiles"
cmake --build . -j
ctest --output-on-failure
cd ..

:: Clang (tuỳ chọn)
mkdir build_windows_clang & cd build_windows_clang
cmake .. -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build .
ctest --output-on-failure
cd ..
```

### 6.3. KAT + unit test (đặt biến theo toolchain đang kiểm thử)
```bat
:: MSVC:
set T=build_windows_msvc\Release\aestool.exe
set U=build_windows_msvc\Release\aes_tests.exe
:: MinGW:  set T=build_windows_gcc\aestool.exe   & set U=build_windows_gcc\aes_tests.exe
:: Clang:  set T=build_windows_clang\aestool.exe & set U=build_windows_clang\aes_tests.exe

%U% > Output\windows\windows_msvc_unit_tests.txt 2>&1
%T% --kat kat\vectors.json > Output\windows\windows_msvc_kat.txt
%T% --kat kat\vectors.json --verbose > Output\windows\windows_msvc_kat_verbose.txt
echo exit=%ERRORLEVEL%
```
Đổi tên file `windows_msvc_*` thành `windows_mingw_*` / `windows_clang_*` khi lặp lại.

### 6.4. Cross-validation
```bat
%T% xval --ref openssl --cases 200 > Output\windows\windows_msvc_xval.txt

:: Đối chiếu bằng openssl.exe (nếu có trong PATH hoặc dùng đường dẫn đầy đủ tới bin\openssl.exe)
powershell -NoProfile -Command "$b=New-Object byte[] 100000; (New-Object Random).NextBytes($b); [IO.File]::WriteAllBytes('work\msg.bin',$b)"
set K=2b7e151628aed2a6abf7158809cf4f3c
set IV=f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
del work\ct.bin work\ct_ossl.bin 2>nul
%T% encrypt --key-hex %K% --iv-hex %IV% --in work\msg.bin --out work\ct.bin --quiet
openssl enc -aes-128-ctr -K %K% -iv %IV% -in work\msg.bin -out work\ct_ossl.bin
fc /b work\ct.bin work\ct_ossl.bin > Output\windows\windows_openssl_cli_match.txt
type Output\windows\windows_openssl_cli_match.txt
```
(`fc` in "FC: no differences encountered" là đạt.)

### 6.5. Negative tests + fail-closed
```bat
(
echo hello CTR> work\msg.txt
%T% keygen --out work\key.bin --quiet
%T% encrypt --key work\key.bin --in work\msg.txt --out work\ct2.bin --force

echo == giai ma dung
%T% decrypt --key work\key.bin --in work\ct2.bin --out work\dec.txt --force --quiet
fc /b work\msg.txt work\dec.txt

echo == sai key
%T% keygen --out work\key2.bin --force --quiet
%T% decrypt --key work\key2.bin --in work\ct2.bin --out work\w1.txt --force --quiet
fc /b work\msg.txt work\w1.txt

echo == sai IV
%T% decrypt --key work\key.bin --iv-hex 00000000000000000000000000000001 --in work\ct2.bin --out work\w2.txt --force --quiet
fc /b work\msg.txt work\w2.txt

echo == IV sai do dai (mong doi exit 3)
%T% decrypt --key work\key.bin --iv-hex 00 --in work\ct2.bin --out work\x.txt --quiet
echo exit=%ERRORLEVEL%

echo == key sai do dai (mong doi exit 3)
%T% encrypt --key-hex 0011 --text hi
echo exit=%ERRORLEVEL%

echo == tran counter (mong doi exit 5, khong de lai file)
%T% encrypt --key work\key.bin --iv-hex ffffffffffffffffffffffffffffffff --in work\msg.bin --out work\o.bin --force --quiet
echo exit=%ERRORLEVEL%
dir work\o.bin* 2>&1

echo == mode khong ho tro (mong doi exit 2)
%T% encrypt --mode gcm --key work\key.bin --text hi
echo exit=%ERRORLEVEL%

echo == file input khong ton tai (mong doi exit 4)
%T% encrypt --key work\key.bin --in work\khong_co.bin --out work\x.bin
echo exit=%ERRORLEVEL%

echo == KAT file hong (mong doi exit 3)
echo { not json> work\bad.json
%T% --kat work\bad.json
echo exit=%ERRORLEVEL%
) > Output\windows\windows_msvc_negative_tests.txt 2>&1
type Output\windows\windows_msvc_negative_tests.txt

:: Vector KAT sai phải làm runner FAIL (exit 6)
powershell -NoProfile -Command "(Get-Content kat\vectors.json -Raw) -replace '69c4e0d86a7b0430d8cdb78070b4c55a','00000000000000000000000000000000' | Set-Content work\bad_kat.json"
%T% --kat work\bad_kat.json
echo wrong-vector exit=%ERRORLEVEL%
```
Ciphertext bị sửa **không bị phát hiện**: sửa 1 byte của `work\ct2.bin` bằng hex editor, giải mã vẫn exit 0, chỉ đúng byte đó đổi (đây chính là điểm cần giải thích trong báo cáo).

### 6.6. File lớn (streaming) + round trip
```bat
del work\big.* work\k.bin 2>nul
powershell -NoProfile -Command "$b=New-Object byte[] 1GB; (New-Object Random).NextBytes($b); [IO.File]::WriteAllBytes('work\big.bin',$b)"
%T% keygen --out work\k.bin --quiet
powershell -NoProfile -Command "Measure-Command { & '%T%' encrypt --key work\k.bin --in work\big.bin --out work\big.enc --quiet --sbox ttable } | Select-Object TotalSeconds" > Output\windows\windows_big_encrypt.txt
powershell -NoProfile -Command "Measure-Command { & '%T%' decrypt --key work\k.bin --in work\big.enc --out work\big.dec --quiet --sbox ttable } | Select-Object TotalSeconds" > Output\windows\windows_big_decrypt.txt
(certutil -hashfile work\big.bin SHA256 & certutil -hashfile work\big.dec SHA256) > Output\windows\windows_big_sha256.txt
type Output\windows\windows_big_sha256.txt
del work\big.*
```
(Hai giá trị SHA-256 phải giống nhau.)

### 6.7. Benchmark (lặp lại cho từng toolchain, đổi tên file)
```bat
%T% bench --sizes 1MiB,100MiB --backends table,ttable --ref openssl --runs 30 --csv Output\windows\bench_windows_msvc.csv > Output\windows\bench_windows_msvc.txt
%T% bench --sizes 64KiB,1MiB --backends computed --ref none --runs 30 --csv Output\windows\bench_windows_msvc_computed.csv > Output\windows\bench_windows_msvc_computed.txt
%T% bench --sizes 1MiB --backends ttable --ref openssl --runs 30 --ops 1000 --csv Output\windows\bench_windows_msvc_ops1000.csv > Output\windows\bench_windows_msvc_ops1000.txt
:: 1 GiB (nếu khả thi; rất lâu)
%T% bench --sizes 1GiB --backends ttable --ref openssl --runs 30 --dir enc --csv Output\windows\bench_windows_msvc_1gib.csv > Output\windows\bench_windows_msvc_1gib.txt
```
Với MinGW/Clang: đặt lại `T` rồi chạy lại các lệnh trên với tên `bench_windows_mingw*` / `bench_windows_clang*`. RAM không tự lấy được trên Windows → điền tay từ `systeminfo`.

### 6.8. Biểu đồ
```bat
python scripts\plot_bench.py Output\windows\bench_windows_msvc.csv Output\windows\plot_windows_msvc
python scripts\plot_bench.py Output\windows\bench_windows_msvc_computed.csv Output\windows\plot_windows_msvc_computed
```

### 6.9. Đóng gói binary + nộp
```bat
copy build_windows_msvc\Release\aestool.exe release\windows\aestool_msvc.exe
copy build_windows_gcc\aestool.exe          release\windows\aestool_mingw.exe
:: Nếu exe liên kết động với OpenSSL, copy kèm libcrypto-*.dll vào release\windows\
```
Nén thư mục `lab2` (loại `build_*` và `work`) cùng các thư mục `release`, `evidence`; gộp vào `MSV.All6labs.rar` cùng 5 lab còn lại.

---

## 7. Phần phải tự viết trong báo cáo (không có lệnh nào làm thay)

**Cross-OS / so sánh (dựa trên CSV ở 5.7 và 6.7):** Windows vs Linux, GCC vs Clang vs MSVC, `table` vs `ttable` vs `computed`, pure vs OpenSSL; kèm bảng + biểu đồ + diễn giải (không chỉ ghi số).

**Phân tích hiệu năng bắt buộc:** S-box bảng tra vs tính toán; dung lượng bộ nhớ (256 B / 4 KiB T-table / 0 B); hành vi cache; ảnh hưởng dự đoán rẽ nhánh; so sánh với thư viện tham chiếu (OpenSSL dùng AES-NI nên nhanh hơn nhiều bậc).

**Thảo luận bảo mật:** các mục ở 2.6, cộng threat model, kịch bản dùng sai, secure defaults (IV tự sinh, từ chối ghi đè, tràn counter bị chặn, cảnh báo khi tự đặt IV), fail-closed, giới hạn cài đặt (không constant-time với `table`/`ttable`, khóa `--key-hex` lộ qua process list, không có nhận diện tái dùng IV giữa các lần chạy).

**Tài liệu hóa CTR:** counter là số 128-bit big-endian trên toàn khối IV; không quay vòng, vượt `FF..FF` thì báo lỗi (exit 5); IV tự sinh = 96-bit nonce ngẫu nhiên ‖ 32-bit counter bằng 0, lưu ở `<out>.iv`.

**Phụ lục:** ví dụ CLI, log build, độ phủ KAT (nguồn: FIPS-197, AESAVS, SP 800-38A; vector carry do OpenSSL sinh), tóm tắt unit test.

**Bảng tự chấm (Lab 2) — mẫu:**

| Hạng mục | Điểm tối đa | Tự chấm |
|---|---|---|
| Lõi AES-128 đúng FIPS-197 | 30 | |
| CTR đúng | 15 | |
| KAT & cross-validation | 15 | |
| Chất lượng kỹ thuật & tài liệu | 10 | |
| Hiệu năng | 10 | |
| Thảo luận bảo mật | 20 | |
| Bonus (nếu có) | +20 | |

**Cam kết liêm chính (mẫu, dán vào báo cáo):** bài làm là của tôi; đã trích dẫn thư viện, nguồn KAT và tài liệu tham khảo; mọi demo tấn công chỉ thực hiện trên artifact tự tạo trong môi trường offline; key trong repo chỉ là key test; **công cụ AI đã được sử dụng để hỗ trợ viết mã/tài liệu — khai báo cụ thể phạm vi** (đề yêu cầu khai báo).

---

## 8. Lưu ý trung thực

- Đề yêu cầu so sánh hiệu năng với **Crypto++**; bản CMake hiện tại (theo mẫu của bạn) chỉ dùng **OpenSSL**. Đề cho phép cross-validate bằng Crypto++ **hoặc** OpenSSL, nhưng nếu giảng viên đòi so sánh hiệu năng đúng với Crypto++ thì cần bật lại (code đã hỗ trợ `AESP_HAVE_CRYPTOPP`, chỉ cần thêm lại vào CMake).
- Chưa làm bonus (AES-192/256, XTS, AES-NI/bit-sliced).
- Chưa có báo cáo, binary Windows và log chạy thật trên máy của bạn; các lệnh ở Mục 5–6 là để tạo ra chúng. Mình đã chạy thử Mục 5 (rút gọn) trên Ubuntu + GCC; các lệnh Windows chưa được chạy thử.
- `computed` rất chậm (~1 MB/s): không đưa size lớn vào.