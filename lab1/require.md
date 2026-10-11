# LAB 1: Mã hóa đối xứng với Crypto++

## ⚠ Ràng buộc
- Bắt buộc dùng thư viện **Crypto++** cho các primitive mật mã.
- **Không** dùng OpenSSL, libsodium hay thư viện mật mã nào khác.
- Được dùng C++ STL.
- Phải biên dịch được trên Windows và Linux.
- Bắt buộc dùng hệ thống build CMake.

## Chuẩn đầu ra
Sau lab này, sinh viên có thể:
1. Dùng Crypto++ an toàn với nhiều mode của block cipher và stream cipher.
2. Xử lý đúng IV/nonce và ngăn chặn việc dùng sai.
3. Thiết kế pipeline file I/O và mã hóa/giải mã dữ liệu (encoding) chắc chắn.
4. Kiểm chứng cài đặt bằng test vector chính thức của NIST.
5. Đo hiệu năng (benchmark) mã hóa đối xứng.
6. Phát hiện và xử lý các lỗi dùng sai phổ biến (ví dụ tái sử dụng nonce).

## 1. Phạm vi thuật toán và cài đặt

### A. Các mode của AES (chọn lúc chạy)
Công cụ phải hỗ trợ: **ECB, CBC, OFB, CFB, CTR, XTS, CCM (AEAD), GCM (AEAD)**.

Chọn mode bằng tham số dòng lệnh:
```
--mode ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm
```

### B. Xử lý AEAD
Với CCM và GCM:
- Cờ `--aead` phải bật chế độ có xác thực.
- Hỗ trợ `--aad FILE` và `--aad-text STRING`.
- Phải kiểm tra authentication tag khi giải mã.
- Nếu tag không hợp lệ → **fail closed** (từ chối, không trả plaintext).

### C. Yêu cầu chống dùng sai (misuse prevention)
Sinh viên phải cài đặt các kiểm tra tường minh:

**Hạn chế với ECB**
- In **WARNING** khi chọn ECB.
- Mặc định chặn file lớn hơn **16 KiB**.
- Chỉ cho phép vượt qua bằng cờ `--allow-ecb`.

**Xử lý IV / Nonce** (với các mode cần IV/nonce)
- Ép đúng độ dài theo từng mode.
- Nếu không cung cấp IV/nonce:
  - Sinh an toàn bằng `AutoSeededRandomPool` của Crypto++.
  - Lưu IV vào header của output hoặc file sidecar JSON.
- Từ chối IV có độ dài không hợp lệ.

**Chống tái sử dụng nonce** (CTR, CCM, GCM)
- Nếu header của output cho thấy cùng key + nonce đã được dùng trước đó → **từ chối thao tác**.
- Phải **mô tả logic phát hiện** trong tài liệu.

## 2. Yêu cầu CLI và I/O

**Mã hóa**
```
aestool encrypt --mode gcm --key key.bin --in msg.txt --out ct.bin
```
**Giải mã**
```
aestool decrypt --mode gcm --key key.bin --in ct.bin --out msg.txt
```

**Xử lý khóa** – hỗ trợ `--key-hex HEXSTRING` và `--key KEYFILE`. File khóa có thể là:
- Nhị phân thô (raw binary), hoặc
- Mã hex kèm header.

**Xử lý IV / Nonce**
```
--iv IVFILE
--nonce NONCEFILE
```
Nếu bỏ qua (trừ ECB/XTS): phải tự sinh an toàn và lưu vào header hoặc file sidecar.

**Đầu vào và encoding**
- Đầu vào từ file hoặc `--text`.
- Đầu ra: ciphertext thô ghi vào file; hiển thị hex hoặc Base64 trên màn hình.
- An toàn với UTF-8.

**Sidecar JSON header (tùy chọn)**
```json
{
 "alg": "AES-256-GCM",
 "iv": "…hex…",
 "aad": "…hex…",
 "tag": "…hex…"
}
```
Header phải chứa: thuật toán, mode, IV/nonce, AAD (nếu có), authentication tag (với mode AEAD).

## 3. Tính đúng đắn và kiểm chứng

### A. Known Answer Tests (KAT)
Kiểm chứng với:
- Vector **NIST SP 800-38A**: CBC, CFB, OFB, CTR.
- Vector GCM của NIST.
- Vector CCM của NIST.

### B. KAT runner
Cài đặt `--kat vectors.json`. Chương trình phải:
- Đọc (parse) file vector JSON.
- Chạy các test case.
- In **PASS/FAIL** cho từng case.
- In bảng tổng kết.

### C. Negative tests (kiểm thử âm)
Sinh viên phải chứng minh:
- Sai key → plaintext sai.
- Sai IV → plaintext sai.
- Sửa ciphertext (không AEAD) → đầu ra bị hỏng.
- Sửa ciphertext (AEAD) → xác thực thất bại.
- Tag không hợp lệ → từ chối giải mã.
- Độ dài IV không hợp lệ → bị từ chối.

Mọi đầu vào sai định dạng đều phải **fail closed**.

## 4. Đánh giá hiệu năng

**Kích thước payload:** 1 KB, 4 KB, 16 KB, 256 KB, 1 MB, 8 MB.

**Chỉ số:**
- Thông lượng (MB/s).
- Độ trễ trên mỗi thao tác (latency per operation).

**Nghiên cứu so sánh bắt buộc:**
- Windows so với Linux.
- Stream mode so với block mode.
- AEAD so với non-AEAD.
- Overhead của tag (GCM/CCM).

**Phải có:** bảng, đồ thị, và phân tích về lập lịch của hệ điều hành (OS scheduling) và overhead của system call.

## 5. Thảo luận về kỹ thuật an toàn (mục bắt buộc trong báo cáo)

**Mức mode**
- Vì sao ECB không an toàn.
- Rủi ro padding oracle của CBC.
- Thảm họa khi tái sử dụng nonce trong CTR.
- Đảm bảo của AEAD so với mode không AEAD.
- Vì sao GCM đòi hỏi nonce duy nhất.
- Hạn chế của XTS (không có tính toàn vẹn).

**Mức cài đặt**
- Sinh số ngẫu nhiên đúng cách.
- Lưu IV an toàn.
- Xác minh authentication tag.
- Xử lý lỗi (fail closed).
- Rủi ro khi lưu trữ khóa.

**Cân nhắc đa nền tảng**
- Khác biệt về RNG giữa các hệ điều hành.
- Hành vi của hệ thống file.
- Nguyên nhân gây chênh lệch hiệu năng.

## 6. Mở rộng tùy chọn (+5 điểm thưởng)
- Xuất lõi mã hóa thành: `.dll` (Windows), `.so` (Linux), `.a` / `.lib`.
- Xây GUI tối giản bằng Python hoặc C#.
- GUI phải gọi thư viện đã biên dịch (không viết lại logic mật mã).

## Thang điểm (100 điểm)

| Thành phần | Điểm |
|---|---|
| Tính đúng đắn & KAT | 25 |
| Vệ sinh bảo mật (security hygiene) | 15 |
| UX & thiết kế I/O | 10 |
| Phương pháp đo hiệu năng | 20 |
| Build đa nền tảng | 10 |
| Chất lượng báo cáo | 20 |
| Thưởng: xuất thư viện + GUI | +5 |

## Tác động học tập kỳ vọng
Sau lab này, sinh viên hiểu:
- Cách dùng thực tế thư viện mã hóa đối xứng.
- Khác biệt giữa block mode, stream mode và AEAD.
- Vòng đời đúng của IV/nonce.
- Các rủi ro dùng sai mật mã trong thực tế.
- Đánh đổi hiệu năng giữa các nền tảng.
- Tầm quan trọng của xác thực trong mã hóa hiện đại.

---

## Yêu cầu chung áp dụng cho mọi lab (tóm tắt, vì Lab 1 cũng phải tuân thủ)

- **Build:** bắt buộc CMake, hỗ trợ out-of-source (`mkdir build && cd build && cmake .. && cmake --build .`); phải biên dịch được trên Ubuntu LTS, Windows (MSVC và MinGW64).
- **README.md** phải có: danh sách dependency (kèm phiên bản), hướng dẫn cài đặt (Windows & Linux), lệnh build cho cả hai OS, ví dụ dùng CLI, các hạn chế đã biết.
- **CLI thống nhất:** `mytool <command> [--in FILE | --text "…"] [--out FILE] [--key FILE | --key-hex HEX] [--iv HEX] [--nonce HEX] [--mode MODE] [--aead] [--encode hex|base64|raw] [--threads N] [--kat vectors.json] [--verbose]`; nhận và xuất UTF-8; fail closed khi đầu vào sai định dạng.
- **Ngẫu nhiên:** chỉ dùng `AutoSeededRandomPool` hoặc `RAND_bytes`; **cấm** `rand()` và `std::random` cho mục đích mật mã.
- **Kích thước khóa tối thiểu:** AES 128/192/256 bit (nên dùng 256 trong báo cáo).
- **Unit test:** dùng Catch2 hoặc GoogleTest; `ctest` phải pass trên cả Windows và Linux.
- **Phương pháp đo hiệu năng:** khởi động (warm-up) 1–2 giây; chạy khối ~1.000 thao tác; lặp 30–100 lần; báo cáo trung bình, trung vị, độ lệch chuẩn, khoảng tin cậy 95%; ghi rõ bảng phần cứng/OS/compiler; đặt CPU governor ở chế độ hiệu năng cao.
- **Báo cáo:** một file PDF/DOCX chung cho cả 6 lab (thường khoảng 60 trang), gồm mục tiêu, chi tiết cài đặt, kết quả test, phương pháp đo, bảng và đồ thị thống kê, phân tích bảo mật, so sánh đa nền tảng, bài học rút ra.
- **Nộp bài:** một file nén duy nhất (`MSV.All6labs.rar`) gồm mã nguồn, CMake, README, unit test, binary Windows và Linux, báo cáo, bảng tự chấm điểm và cam kết liêm chính học thuật (kèm khai báo việc dùng AI/công cụ hỗ trợ nếu có).

Nếu bạn muốn, mình có thể xuất bản dịch này thành file Word hoặc PDF.