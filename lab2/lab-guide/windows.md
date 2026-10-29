# Lab 2 — Hướng dẫn build & chạy trên Windows

Thư mục dự án (chứa `CMakeLists.txt`) gọi là `lab2\`. Mọi lệnh chạy từ thư mục này, trừ khi có ghi chú.
Hỗ trợ 3 toolchain: **MSVC**, **MinGW-w64 (GCC)**, **Clang**.

## 1. Cài đặt công cụ

| Công cụ | Ghi chú |
|---|---|
| CMake **>= 3.20** | https://cmake.org/download (tick "Add CMake to PATH") |
| Git | cần để CMake tải Catch2 (v3.5.3) lần đầu; cần internet |
| MSVC | Visual Studio 2022 hoặc *Build Tools* (workload "Desktop development with C++") |
| MinGW-w64 (GCC) | ví dụ MSYS2 UCRT64 / WinLibs; `g++`, `mingw32-make` phải có trong PATH |
| Clang (tuỳ chọn) | LLVM + Ninja (`ninja` trong PATH) |
| Python 3 + matplotlib (tuỳ chọn) | để vẽ biểu đồ benchmark |

### OpenSSL (chỉ để đối chiếu/so sánh, không dùng trong lõi AES)

Cần **một bản OpenSSL build cho đúng compiler** (MSVC / GCC / Clang không dùng lẫn). `CMakeLists.txt` tự tìm theo thứ tự:

1. `-DOPENSSL_ROOT_DIR=...` nếu bạn truyền vào.
2. Thư mục theo compiler (nếu tồn tại):
   - MSVC: `D:/UIT/NT219/libs/openssl/msvc`
   - Clang: `D:/UIT/NT219/libs/openssl/clang`
   - GCC: `D:/UIT/NT219/libs/openssl/gcc`
3. OpenSSL mà CMake tìm thấy trong hệ thống.

Mỗi thư mục phải có dạng `include\openssl\...` và `lib\libcrypto*.lib/.a`.
Khi chạy `aestool.exe`, nếu dùng bản OpenSSL dạng DLL thì phải có `libcrypto-3-x64.dll` (hoặc tương đương) trong PATH hoặc cùng thư mục với file `.exe`.

## 2. Build bằng script `scripts\build.bat`

Mở **cmd** tại `lab2\`:

```bat
scripts\build.bat mingw     :: tạo build_windows_gcc\, build + ctest (MinGW Makefiles)
scripts\build.bat msvc      :: tạo build_windows_msvc\, build Release + ctest
scripts\build.bat           :: giống msvc (mặc định)
```

- Với `msvc`: phải chạy trong **"x64 Native Tools Command Prompt for VS 2022"** (hoặc "Developer Command Prompt") để có compiler trong PATH.
- Với `mingw`: `g++` và `mingw32-make` phải có trong PATH.
- Script chưa hỗ trợ Clang → dùng build thủ công ở mục 3.3.

## 3. Build thủ công bằng cmd

### 3.1. MSVC (Visual Studio)

Mở "x64 Native Tools Command Prompt for VS 2022", rồi:

```bat
mkdir build_windows_msvc
cd build_windows_msvc
cmake ..
cmake --build . --config Release
ctest -C Release --output-on-failure
```

(Muốn chỉ rõ generator: `cmake .. -G "Visual Studio 17 2022" -A x64`.)
File chạy: `build_windows_msvc\Release\aestool.exe`, test: `build_windows_msvc\Release\aes_tests.exe`.

### 3.2. MinGW-w64 (GCC)

```bat
mkdir build_windows_gcc
cd build_windows_gcc
cmake .. -G "MinGW Makefiles"
cmake --build . -j
ctest --output-on-failure
```

File chạy: `build_windows_gcc\aestool.exe`.

### 3.3. Clang (Ninja)

```bat
mkdir build_windows_clang
cd build_windows_clang
cmake .. -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build .
ctest --output-on-failure
```

File chạy: `build_windows_clang\aestool.exe`.

### 3.4. Tuỳ chọn CMake hữu ích

| Tuỳ chọn | Ý nghĩa |
|---|---|
| `-DCMAKE_BUILD_TYPE=Release\|Debug` | cho generator đơn cấu hình (MinGW/Ninja); mặc định `Release` |
| `-DBUILD_TESTS=OFF` | không build unit test |
| `-DLAB2_WERROR=OFF` | tắt `-Werror` (GCC/Clang) |
| `-DOPENSSL_ROOT_DIR=D:/duong/dan/openssl` | chỉ định OpenSSL (dùng dấu `/`) |

Build lại từ đầu: xoá thư mục `build_windows_*` rồi chạy lại.

## 4. Chạy kiểm thử

Ví dụ với MinGW (`cd build_windows_gcc`). Với MSVC thay `aestool.exe` bằng `Release\aestool.exe`, và KAT dùng `..\kat\vectors.json` như nhau.

```bat
ctest --output-on-failure                                :: 3 test: unit_tests, kat_runner, cli_end_to_end
aes_tests.exe                                            :: chi tiết unit test (Catch2)
aestool.exe --kat ..\kat\vectors.json              :: KAT: PASS/FAIL từng case + tổng kết
aestool.exe --kat ..\kat\vectors.json --verbose
aestool.exe xval --ref openssl --cases 50          :: đối chiếu với OpenSSL
```

> Test `cli_end_to_end` dùng `cmake -P` nên chạy được trên cmd, không cần bash.

## 5. Sử dụng CLI

```bat
:: Tạo key 16 byte và IV 16 byte
aestool.exe keygen --out key.bin
aestool.exe ivgen  --out iv.bin

:: Mã hoá / giải mã với IV chỉ định (đúng như đề)
aestool.exe encrypt --mode ctr --key key.bin --iv iv.bin --in msg.bin --out ct.bin
aestool.exe decrypt --mode ctr --key key.bin --iv iv.bin --in ct.bin  --out msg2.bin

:: Không truyền --iv: tự sinh IV ngẫu nhiên, lưu vào ct.bin.iv; decrypt tự đọc lại
aestool.exe encrypt --key key.bin --in msg.bin --out ct.bin
aestool.exe decrypt --key key.bin --in ct.bin  --out msg2.bin
fc /b msg.bin msg2.bin

:: Xuất ra màn hình (hex), văn bản UTF-8
aestool.exe encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff --text "xin chào"

:: Chọn backend S-box: table (mặc định) | ttable | computed
aestool.exe encrypt --key key.bin --in msg.bin --out ct.bin --sbox ttable --force
```

- `--force`: cho phép ghi đè file output (mặc định bị từ chối).
- Exit code: `0` ok · `2` sai cú pháp · `3` input không hợp lệ · `4` lỗi I/O · `5` tràn bộ đếm CTR · `6` KAT/xval fail. Xem bằng `echo %ERRORLEVEL%`.
- Với `--text` có tiếng Việt: tham số dòng lệnh được đọc ở dạng UTF-16 rồi đổi sang UTF-8 nên chính xác; nhưng **hiển thị** trên cmd có thể lỗi font — chạy `chcp 65001` trước.

## 6. Demo negative test (cmd)

`scripts\demo.sh` là bash (dùng được trong Git Bash). Với cmd, chạy thủ công:

```bat
echo hello CTR> msg.txt
aestool.exe keygen --out key.bin --quiet
aestool.exe encrypt --key key.bin --in msg.txt --out ct.bin

:: 1) Giải mã đúng
aestool.exe decrypt --key key.bin --in ct.bin --out dec.txt --quiet
fc /b msg.txt dec.txt

:: 2) Sai key -> plaintext sai
aestool.exe keygen --out key2.bin --quiet
aestool.exe decrypt --key key2.bin --in ct.bin --out w1.txt --quiet
fc /b msg.txt w1.txt

:: 3) Sai IV -> plaintext sai
aestool.exe decrypt --key key.bin --iv-hex 00000000000000000000000000000001 --in ct.bin --out w2.txt --quiet
fc /b msg.txt w2.txt

:: 4) IV sai độ dài -> bị từ chối (exit 3)
aestool.exe decrypt --key key.bin --iv-hex 00 --in ct.bin --out x.txt --quiet
echo exit=%ERRORLEVEL%

:: 5) Tràn counter -> bị từ chối (exit 5)
aestool.exe encrypt --key key.bin --iv-hex ffffffffffffffffffffffffffffffff --in msg.txt --out o.bin --quiet
echo exit=%ERRORLEVEL%
```

Ciphertext bị sửa thì **không bị phát hiện** (CTR không có xác thực): sửa 1 byte của `ct.bin` bằng hex editor, giải mã vẫn thành công (exit 0) và chỉ đúng byte đó bị đổi.

## 7. Benchmark

Chuẩn bị để kết quả công bằng:
- Control Panel → Power Options → **High performance** (hoặc Ultimate Performance), cắm sạc, đóng các ứng dụng nặng.
- Luôn dùng build **Release**.

```bat
aestool.exe bench --sizes 1MiB,100MiB --runs 30 --csv bench_windows_gcc.csv
:: thêm 1GiB nếu đủ RAM:   --sizes 1MiB,100MiB,1GiB
:: backend computed rất chậm (~1 MB/s), chỉ đo size nhỏ:
aestool.exe bench --sizes 64KiB,1MiB --backends computed --runs 30
```

Vẽ biểu đồ (từ thư mục build):

```bat
python ..\scripts\plot_bench.py bench_windows_gcc.csv plot_windows_gcc
```

Phần header `bench` in ra (CPU, RAM, OS, compiler, flags) dùng để điền bảng phần cứng trong báo cáo. Trên Windows, CPU lấy từ biến môi trường; RAM hiển thị "unknown" nên bạn điền tay (Task Manager → Performance).
Để so sánh chéo MSVC / GCC / Clang, đặt tên CSV khác nhau (`bench_windows_msvc.csv`, `bench_windows_clang.csv`).

## 8. Xử lý lỗi thường gặp

| Triệu chứng | Cách xử lý |
|---|---|
| `CMake 3.20 or higher is required` | Cài CMake mới hơn |
| `Could NOT find OpenSSL` | Đặt OpenSSL đúng compiler vào thư mục ở mục 1, hoặc `-DOPENSSL_ROOT_DIR=...`; xoá thư mục build rồi cấu hình lại (cache cũ có thể giữ đường dẫn sai) |
| Chạy `.exe` báo thiếu `libcrypto-3-x64.dll` | Copy DLL vào cạnh file `.exe` hoặc thêm thư mục `bin` của OpenSSL vào PATH |
| `cmake` không tìm thấy compiler (MSVC) | Mở "x64 Native Tools Command Prompt", không dùng cmd thường |
| `CMAKE_MAKE_PROGRAM is not set` (MinGW) | Thêm `mingw32-make` vào PATH, hoặc dùng `-G Ninja` |
| Lỗi khi tải Catch2 | Kiểm tra internet và Git trong PATH |
| `output file ... already exists (use --force)` | Thêm `--force` hoặc xoá file cũ |
| Trộn OpenSSL của compiler này với compiler khác gây lỗi link | Mỗi compiler dùng bản OpenSSL riêng và thư mục build riêng |