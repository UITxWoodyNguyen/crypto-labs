# Header và outline báo cáo Lab 1

Báo cáo nộp là **một file chung cho cả 6 lab**, nên mình viết Lab 1 thành một chương, kèm phần đầu (header) dùng chung cho cả báo cáo. Chương này nên chiếm khoảng 9 đến 11 trang trong tổng ~60 trang.

## A. Header (trang bìa)

```
TRƯỜNG ĐẠI HỌC CÔNG NGHỆ THÔNG TIN, ĐHQG-HCM            [Khoa / Bộ môn]

BÁO CÁO THỰC HÀNH
NT219: CRYPTOGRAPHY & APPLICATIONS
LABORATORY SERIES (LABS 1–6)

Lab 1: Symmetric Encryption with Crypto++
(AES: ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM)

Sinh viên thực hiện:  [Họ và tên]        MSSV: [MSSV]
Lớp: [Mã lớp]                           Giảng viên / TA: [Tên]
Nhóm (nếu có): [Danh sách thành viên]
Ngày nộp: [dd/mm/yyyy]
Mã nguồn / archive: MSV.All6labs.rar
```

Sau trang bìa là các trang đầu báo cáo:
1. Mục lục (bao gồm danh mục hình và danh mục bảng).
2. **Cam kết liêm chính học thuật** (Appendix A của đề: tự làm, trích dẫn thư viện và nguồn KAT, demo tấn công chỉ trên dữ liệu tự tạo, key trong repo chỉ là key thử).
3. **Khai báo hỗ trợ AI/công cụ** (đề yêu cầu nếu có dùng).
4. **Bảng tự chấm điểm** (Appendix B): đặt ở đầu hoặc cuối đều được, nhưng phải có.

## B. Outline chương Lab 1

### 1. Mục tiêu và chuẩn đầu ra (≈ 0,5 trang)
- 1.1 Mục tiêu của Lab 1 (6 chuẩn đầu ra trong đề).
- 1.2 Ràng buộc: chỉ dùng Crypto++, không OpenSSL, CMake, build trên Windows và Linux.
- 1.3 Phạm vi: 8 mode AES, AEAD, chống dùng sai, KAT, benchmark.

### 2. Thiết kế và cài đặt (≈ 2 trang)
- 2.1 Kiến trúc tổng thể (đưa **Hình 1** sơ đồ kiến trúc: CLI, lõi `aescore`, Crypto++, file lưu trữ).
- 2.2 Cấu trúc project và hệ thống build (CMake, out-of-source, MSVC, MinGW64, Ubuntu).
- 2.3 Giao diện dòng lệnh (bảng tham số `encrypt`, `decrypt`, `keygen`, `--kat`, `bench`; mã thoát 0/1/2/3).
- 2.4 Định dạng dữ liệu:
  - ciphertext thô, sidecar JSON (`alg, mode, iv, aad, tag`), file key (thô hoặc `AESKEY-HEX`);
  - quy ước độ dài key và IV theo mode (bảng).
- 2.5 Xử lý khóa, IV và nonce (sinh bằng `AutoSeededRandomPool`, lưu vào sidecar).
- 2.6 Xử lý AEAD (GCM, CCM, tách tag, AAD, tag 4/8/16 của CCM).

### 3. Thiết kế bảo mật và chống dùng sai (≈ 1,5 trang)
- 3.1 Mô hình đe dọa (threat model) và các kịch bản dùng sai.
- 3.2 Hạn chế với ECB: cảnh báo, chặn trên 16 KiB, `--allow-ecb`.
- 3.3 Kiểm tra IV/nonce: độ dài đúng theo mode, từ chối IV sai.
- 3.4 **Chống tái sử dụng nonce** (mô tả logic phát hiện theo yêu cầu đề): `SHA-256(key | mode | iv)`, file `.aestool_nonces`, các giới hạn.
- 3.5 Fail closed: không ghi file khi xác thực lỗi, thông báo lỗi không lộ nguyên nhân, phân loại mã thoát.

### 4. Phương pháp và kết quả kiểm thử (≈ 1,5 trang)
- 4.1 Known Answer Tests (**Bảng 1**: 9 vector, nguồn trích dẫn: NIST SP 800-38A cho ECB/CBC/CFB/OFB/CTR, vector GCM, NIST SP 800-38C cho CCM, IEEE 1619 cho XTS; kết quả PASS/FAIL).
- 4.2 KAT runner `--kat` (định dạng JSON, tổng kết).
- 4.3 Kiểm thử âm (**Bảng 2**: sai key, sai IV, sửa ciphertext non-AEAD, sửa ciphertext AEAD, tag sai, IV sai độ dài, hex/base64/JSON hỏng; kết quả mong đợi và thực tế, kèm mã thoát).
- 4.4 Unit test và `ctest` (3 nhóm: `unit_tests`, `nist_kat`, `cli_negative`); kết quả trên Windows và Linux.
- 4.5 Ảnh chụp màn hình minh chứng (cảnh báo ECB, nonce reuse bị từ chối, tag sai).

### 5. Đánh giá hiệu năng (≈ 2,5 trang)
- 5.1 Bảng phần cứng và môi trường (CPU, nhân/luồng, RAM, ổ đĩa, OS, compiler và cờ biên dịch, power plan / CPU governor).
- 5.2 Giao thức đo: warm-up, kích thước payload (1 KB, 4 KB, 16 KB, 256 KB, 1 MB, 8 MB), khối thao tác, số lần chạy, key và IV ngẫu nhiên, phạm vi đo (gồm key setup và cấp phát bộ nhớ).
- 5.3 Phương pháp thống kê: mean, median, độ lệch chuẩn, khoảng tin cậy 95% (ghi rõ xấp xỉ giá trị t).
- 5.4 Kết quả (**Bảng 3**: throughput và latency; **Hình 2 đến 3**: đồ thị có error bar).
- 5.5 Các so sánh bắt buộc:
  - Windows so với Linux (**Hình 4**: lưới so sánh, tỷ lệ so với baseline, bảng tổng kết có cột ý nghĩa thống kê);
  - stream mode so với block mode;
  - AEAD so với non-AEAD;
  - overhead của tag (GCM, CCM).
- 5.6 Diễn giải: lập lịch OS, overhead system call, cache, ảnh hưởng tăng tốc phần cứng (AES-NI), nguyên nhân chênh lệch giữa các nền tảng (không chỉ liệt kê số).

### 6. Phân tích bảo mật (≈ 2 trang), mục bắt buộc theo đề
- 6.1 Mức mode:
  - ECB không an toàn (có thể dẫn chứng bằng ví dụ pattern);
  - rủi ro padding oracle của CBC;
  - thảm họa khi dùng lại nonce trong CTR (keystream reuse);
  - đảm bảo của AEAD so với non-AEAD;
  - vì sao GCM cần nonce duy nhất;
  - XTS không có tính toàn vẹn.
- 6.2 Mức cài đặt: sinh ngẫu nhiên đúng cách, lưu IV an toàn, xác minh tag, xử lý lỗi fail closed, rủi ro lưu key (key trong `std::string` chưa xóa an toàn khỏi bộ nhớ).
- 6.3 Cân nhắc đa nền tảng:
  - nguồn RNG (Linux `getrandom`/`/dev/urandom`, Windows CNG `BCryptGenRandom`);
  - hành vi file system (ghi file, kết thúc dòng, đường dẫn);
  - nguyên nhân khác biệt hiệu năng.
- 6.4 Hạn chế của cài đặt (đọc toàn bộ file vào RAM, nonce registry chỉ bắt trùng IV chính xác, JSON parser tối giản...).

### 7. Thảo luận (≈ 0,5 trang)
- 7.1 Lỗi phát hiện và đã sửa trong quá trình làm.
- 7.2 So sánh giữa các nền tảng và compiler (MSVC, MinGW64, GCC trên Ubuntu).
- 7.3 Đánh đổi bảo mật và hiệu năng.

### 8. Kết luận và hướng phát triển (≈ 0,5 trang)
- Các điểm chính rút ra, bài học kinh nghiệm.
- Hướng mở rộng: stream I/O cho file lớn, xóa key an toàn, thư viện `.dll`/`.so` kèm GUI (bonus +5).

### Phụ lục của Lab 1
- P1. Ví dụ dùng CLI (lệnh và kết quả).
- P2. Log build (MSVC, MinGW64, Ubuntu) và kết quả `ctest`.
- P3. Độ phủ KAT và tóm tắt unit test.
- P4. Mẫu file CSV benchmark và sidecar JSON.
- P5. Tham khảo: NIST SP 800-38A, SP 800-38C, GCM spec (McGrew và Viega), IEEE 1619, tài liệu Crypto++, FIPS-197.

## C. Ánh xạ outline với thang điểm Lab 1

| Thang điểm (đề) | Phần báo cáo chứng minh |
|---|---|
| Correctness và KAT (25) | Mục 4.1, 4.2 |
| Security hygiene (15) | Mục 3, 6 |
| UX và thiết kế I/O (10) | Mục 2.3, 2.4 |
| Phương pháp đo hiệu năng (20) | Mục 5 |
| Build đa nền tảng (10) | Mục 2.2, phụ lục P2 |
| Chất lượng báo cáo (20) | Toàn bộ chương, cấu trúc và diễn giải |

## D. Lưu ý khi viết

- Số liệu benchmark và đồ thị so sánh phải lấy từ **CSV thật** chạy trên máy của bạn (đồ thị mình thử trước đó chỉ dùng dữ liệu giả lập). Ghi rõ cấu hình máy và compiler cho từng bản đo.
- Mục diễn giải (5.6, 6) là nơi lấy điểm: đề nhấn mạnh "interpretation, not just raw numbers" và "understanding beyond implementation".
- Dùng mẫu báo cáo của đề (Objective, Design & security, Test methodology, Performance, Discussion, Conclusion, Appendix) làm khung; outline trên đã bám theo thứ tự đó.

Nếu bạn muốn, mình có thể xuất outline này thành file Word kèm mục lục tự động, hoặc viết nháp một mục cụ thể (ví dụ mục 3.4 về chống tái sử dụng nonce hoặc mục 6.1).