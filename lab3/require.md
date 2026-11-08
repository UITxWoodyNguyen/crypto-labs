# Hướng Dẫn Thực Hiện Lab 3: RSA-OAEP & Hybrid Encryption (Crypto++ / OpenSSL)

Tài liệu này tổng hợp chi tiết yêu cầu kỹ thuật, kiến trúc hệ thống, hướng dẫn triển khai bằng C++ (hỗ trợ cả Crypto++ và OpenSSL), kịch bản kiểm thử, phương pháp đo kiểm hiệu năng và cấu trúc báo cáo cho **Lab 3**.

---

## 1. Mục Tiêu Thực Hành (Learning Outcomes)

Sau khi hoàn thành Lab 3, sinh viên phải nắm vững:
1. **Quản lý cặp khóa RSA**: Sinh khóa ngẫu nhiên an toàn, xuất/nhập khóa theo các định dạng chuẩn công nghiệp (PEM, DER) kèm siêu dữ liệu (metadata JSON).
2. **Mã hóa phi đối xứng RSA-OAEP**: Cấu hình chuẩn xác tham số đệm OAEP kết hợp SHA-256 và hàm tạo mặt nạ MGF1(SHA-256), hỗ trợ nhãn đệm (`label`).
3. **Giới hạn kích thước và Mã hóa Lai (Hybrid/Envelope Encryption)**:
   - Hiểu rõ chặn trên kích thước bản rõ của RSA: $mLen \le k - 2hLen - 2$.
   - Tự động chuyển đổi sang cơ chế phong bì số (Envelope Encryption) kết hợp **AES-256-GCM** khi dữ liệu vượt ngưỡng.
4. **Xử lý lỗi an toàn (Fail-Closed & Constant-Time)**: Nhận biết, cô lập và báo lỗi khi bản mã, nhãn hoặc phong bì bị giả mạo mà không để lộ kênh phụ (timing side-channel / oracle).
5. **Đo kiểm hiệu năng chuẩn thống kê**: Đo đạc độ trễ (latency), thông lượng (throughput) với khoảng tin cậy 95% ($95\%\text{ CI}$) giữa các kích cỡ khóa (RSA-3072 vs RSA-4096) và cơ chế lai trên các kích cỡ tệp khác nhau ($1\text{ KiB}, 1\text{ MiB}, 100\text{ MiB}$).

---

## 2. Tiêu Chuẩn Thuật Toán & Tham Số Bắt Buộc

| Thành phần | Tham số bắt buộc | Ghi chú kỹ thuật |
| :--- | :--- | :--- |
| **Thuật toán phi đối xứng** | RSA | Khóa $\ge 3072\text{ bits}$ (Bắt buộc so sánh RSA-3072 và RSA-4096) |
| **Sơ đồ đệm RSA** | OAEP | Cung cấp tính an toàn ngữ nghĩa chống tấn công bản mã chọn lọc chủ động (IND-CCA2) |
| **Hàm băm & MGF** | SHA-256, MGF1(SHA-256) | $hLen = 32\text{ bytes}$ |
| **Mã hóa đối xứng (Lai)** | AES-256-GCM | Key: 256 bits, IV/Nonce: 96 bits (12 bytes), Tag: 128 bits (16 bytes) |
| **Định dạng khóa** | PEM, DER | Khóa công khai (`SubjectPublicKeyInfo`), Khóa bí mật (`PKCS#8`) |
| **Định dạng phong bì** | JSON Envelope | Gói gọn ciphertext, wrapped key, IV, tag, metadata |

### Công thức tính giới hạn bản rõ RSA-OAEP
Với kích thước khóa Modulus $n$ có độ dài $k$ bytes ($k = 384\text{ bytes}$ đối với RSA-3072) và hàm băm SHA-256 ($hLen = 32\text{ bytes}$):
$$mLen_{\max} = k - 2 \cdot hLen - 2 = 384 - 2(32) - 2 = 318\text{ bytes}$$
* Đối với RSA-4096: $k = 512\text{ bytes} \implies mLen_{\max} = 512 - 64 - 2 = 446\text{ bytes}$.
* **Quy tắc bắt buộc:** Nếu kích thước bản rõ $mLen > mLen_{\max}$, chương trình **phải tự động chuyển sang chế độ Hybrid Encryption**.

---

## 3. Thiết Kế Giao Diện Dòng Lệnh (CLI Interface)

Công cụ dòng lệnh phải có tên tệp nhị phân là `rsatool` tuân thủ chuẩn:

### 3.1. Sinh khóa (`keygen`)
```bash
# Tạo cặp khóa RSA 3072-bit lưu ra PEM và DER cùng metadata
rsatool keygen --bits 3072 --pub pub.pem --priv priv.pem
```
* **Đầu ra bổ sung:** Tạo tệp DER song song (ví dụ: `pub.der`, `priv.der`) và tệp `keys_metadata.json`:
  ```json
  {
    "creation_time": "2026-11-03T06:51:00Z",
    "modulus_bits": 3072,
    "hash": "SHA-256",
    "mgf": "MGF1-SHA256"
  }
  ```

### 3.2. Mã hóa (`encrypt`)
```bash
# Mã hóa dữ liệu trực tiếp hoặc tự động chuyển sang Hybrid
rsatool encrypt --in msg.bin --pub pub.pem --out ct.bin [--label "optional-label"] [--encode hex|base64|raw]
```
* **Hành vi xử lý:**
  - Nếu kích thước `msg.bin` $\le mLen_{\max}$: Mã hóa RSA-OAEP trực tiếp. Lưu ciphertext nhị phân (hoặc hex/base64 nếu có cờ `--encode`).
  - Nếu kích thước `msg.bin` $> mLen_{\max}$: Tạo khóa phiên AES-256 ngẫu nhiên, sinh IV ngẫu nhiên 96-bit, mã hóa dữ liệu bằng AES-GCM, bọc khóa AES bằng RSA-OAEP, và xuất ra tệp phong bì lai (Hybrid Envelope).

### 3.3. Giải mã (`decrypt`)
```bash
# Giải mã bản mã hoặc phong bì số
rsatool decrypt --in ct.bin --priv priv.pem --out msg.bin [--label "optional-label"]
```
* **Hành vi xử lý:** Tự nhận diện tệp đầu vào là bản mã thô RSA-OAEP hay cấu trúc phong bì JSON để tiến hành giải mã khóa đối xứng và dữ liệu tương ứng.

---

## 4. Đặc Tả Định Dạng Phong Bì Lai (Hybrid Envelope Format)

Khi kích thước bản rõ vượt quá giới hạn RSA-OAEP, cấu trúc tệp đầu ra `--out` được lưu trữ dưới định dạng JSON có cấu trúc như sau:

```json
{
  "mode": "RSA-OAEP-AES-GCM",
  "rsa_modulus": 3072,
  "hash": "SHA-256",
  "mgf": "MGF1-SHA256",
  "label": "",
  "wrapped_key": "<Base64 encoded RSA-encrypted AES-256 key>",
  "iv": "<Base64 encoded 96-bit IV/Nonce>",
  "tag": "<Base64 encoded 128-bit AES-GCM authentication tag>",
  "ciphertext": "<Base64 encoded AES-GCM ciphertext payload>"
}
```
*(Ghi chú: Nếu payload nhị phân rất lớn, sinh viên có thể ghi header JSON riêng hoặc sidecar file, tuy nhiên mô hình nhúng payload Base64 hoặc nhị phân đính kèm sau JSON header cần được mô tả rõ trong báo cáo).*

---

## 5. Kiến Trúc Mã Nguồn Đề Xuất (C++17 & CMake)

### 5.1. Cấu trúc thư mục
```text
lab3-rsa-hybrid/
├── CMakeLists.txt
├── include/
│   ├── rsa_oaep.hpp
│   ├── hybrid_engine.hpp
│   ├── key_manager.hpp
│   ├── utils.hpp
│   └── json.hpp (hoặc nlohmann/json)
├── src/
│   ├── main.cpp
│   ├── rsa_oaep.cpp
│   ├── hybrid_engine.cpp
│   ├── key_manager.cpp
│   └── utils.cpp
├── tests/
│   ├── test_rsa.cpp
│   ├── test_negative.cpp
│   └── vectors/
│       └── rsa_oaep_vectors.json
└── README.md
```

### 5.2. File `CMakeLists.txt` mẫu

```cmake
cmake_minimum_required(VERSION 3.16)
project(rsatool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tùy chọn thư viện: OpenSSL hoặc Crypto++
find_package(OpenSSL REQUIRED)
# Hoặc nếu dùng Crypto++:
# find_package(Cryptopp REQUIRED)

add_library(rsa_hybrid_core
    src/rsa_oaep.cpp
    src/hybrid_engine.cpp
    src/key_manager.cpp
    src/utils.cpp
)
target_include_directories(rsa_hybrid_core PUBLIC include ${OPENSSL_INCLUDE_DIR})
target_link_libraries(rsa_hybrid_core PUBLIC OpenSSL::Crypto)

add_executable(rsatool src/main.cpp)
target_link_libraries(rsatool PRIVATE rsa_hybrid_core)

# Cấu hình Unit Tests
enable_testing()
add_subdirectory(tests)
```

---

## 6. Đoạn Mã Triển Khai Tham Khảo (Sử dụng OpenSSL 3.x EVP API)

### 6.1. Khởi tạo Khóa & RSA-OAEP Encrypt với EVP_PKEY

```cpp
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <vector>
#include <stdexcept>
#include <string>

std::vector<uint8_t> rsa_oaep_encrypt(EVP_PKEY* pub_key, 
                                      const std::vector<uint8_t>& plaintext,
                                      const std::string& label = "") {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pub_key, nullptr);
    if (!ctx) throw std::runtime_error("Loi khoi tao EVP_PKEY_CTX");

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Loi encrypt_init");
    }

    // Cấu hình OAEP đệm SHA-256 + MGF1(SHA-256)
    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
    EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());

    if (!label.empty()) {
        // Cấp phát bản sao label vì OpenSSL sẽ chiếm dụng bộ nhớ này khi set
        unsigned char* l_copy = (unsigned char*)OPENSSL_memdup(label.data(), label.size());
        EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, l_copy, label.size());
    }

    size_t outlen = 0;
    // Lấy kích thước buffer đầu ra
    if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, plaintext.data(), plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Kich thuoc du lieu vuot qua gioi han RSA-OAEP");
    }

    std::vector<uint8_t> ciphertext(outlen);
    if (EVP_PKEY_encrypt(ctx, ciphertext.data(), &outlen, plaintext.data(), plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Mã hóa RSA-OAEP that bai");
    }

    ciphertext.resize(outlen);
    EVP_PKEY_CTX_free(ctx);
    return ciphertext;
}
```

### 6.2. Giải mã RSA-OAEP với EVP_PKEY

```cpp
std::vector<uint8_t> rsa_oaep_decrypt(EVP_PKEY* priv_key,
                                      const std::vector<uint8_t>& ciphertext,
                                      const std::string& label = "") {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(priv_key, nullptr);
    if (!ctx) throw std::runtime_error("Loi khoi tao EVP_PKEY_CTX");

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Loi decrypt_init");
    }

    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
    EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());

    if (!label.empty()) {
        unsigned char* l_copy = (unsigned char*)OPENSSL_memdup(label.data(), label.size());
        EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, l_copy, label.size());
    }

    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, ciphertext.data(), ciphertext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Giai ma that bai: Ban ma khong hop le hoac sai nhan/khoa");
    }

    std::vector<uint8_t> plaintext(outlen);
    if (EVP_PKEY_decrypt(ctx, plaintext.data(), &outlen, ciphertext.data(), ciphertext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Giai ma that bai (fail closed)");
    }

    plaintext.resize(outlen);
    EVP_PKEY_CTX_free(ctx);
    return plaintext;
}
```

---

## 7. Kịch Bản Kiểm Thử Bắt Buộc (Negative Testing & Validation)

Hệ thống phải có bộ kiểm thử tự động (sử dụng Catch2 hoặc GTest) bao gồm tối thiểu 5 trường hợp kiểm thử tiêu cực sau:

1. **Altered RSA Ciphertext Test**: Thay đổi ngẫu nhiên 1 bit trong bản mã RSA $\to$ Quá trình giải mã phải thất bại, ném ngoại lệ an toàn, không rò rỉ nguyên nhân chi tiết qua thông báo lỗi.
2. **Altered AES-GCM Ciphertext Test**: Thay đổi 1 bit trong phần payload của phong bì lai $\to$ Quá trình xác thực thẻ (Tag verification) thất bại ngay lập tức, từ chối trả về bất kỳ phần dữ liệu đã giải mã nào.
3. **Wrong Private Key Test**: Sinh cặp khóa RSA thứ hai, dùng khóa bí mật này để giải mã ciphertext tạo bởi khóa thứ nhất $\to$ Thất bại hoàn toàn.
4. **Wrong OAEP Label Test**: Mã hóa với `--label "SECURE_CONTEXT"`, nhưng giải mã với `--label "WRONG_CONTEXT"` hoặc bỏ trống $\to$ Thất bại.
5. **Tampered Envelope Header Test**: Thay đổi trường `iv`, `tag` hoặc sửa đổi `wrapped_key` trong JSON envelope $\to$ Quá trình phân tích cú pháp hoặc giải mã phải ngắt lập tức (`fail-closed`).

---

## 8. Phương Pháp Đo Kiểm Hiệu Năng (Benchmarking Protocol)

### 8.1. Thiết lập thông số đo
* **Warm-up**: Chạy làm nóng cache từ 1 đến 2 giây trước khi ghi nhận dữ liệu.
* **Số lượt lặp ($N$)**: Tối thiểu $N \ge 30$ lượt độc lập cho mỗi trường hợp để đảm bảo độ tin cậy thống kê.
* **Thu thập dữ liệu**:
  - Trung bình (Mean)
  - Trung vị (Median)
  - Độ lệch chuẩn (Standard Deviation)
  - Khoảng tin cậy 95% ($95\%\text{ CI}$)

### 8.2. Ma trận đo kiểm cần thực hiện

| Đối tượng | Kích cỡ / Tham số | Chỉ số cần đo |
| :--- | :--- | :--- |
| **RSA Pure** | RSA-3072 vs RSA-4096 | Thời gian sinh khóa (ms), Thời gian mã hóa (ms/op), Thời gian giải mã (ms/op) |
| **Hybrid Mode** | Tệp $1\text{ KiB}, 1\text{ MiB}, 100\text{ MiB}$ | Thông lượng mã hóa/giải mã ($\text{MB/s}$), Thời gian trễ toàn phần |
| **So sánh hệ thống** | Windows vs Linux | So sánh chênh lệch giữa hai OS trên cùng một cấu hình phần cứng |

---

## 9. Hướng Dẫn Nội Dung Báo Cáo Kỹ Thuật (Report Checklist)

Trong báo cáo tổng hợp, phần Lab 3 cần làm rõ các câu hỏi lý thuyết và kỹ thuật chuyên sâu sau:

### Phân tích an toàn tầng thuật toán:
* **Tại sao OAEP đạt chuẩn an toàn IND-CCA2?** Cơ chế 2 mặt nạ ngẫu nhiên ($r$) ngăn chặn việc biến đổi bản mã (malleability) như thế nào?
* **Tại sao PKCS#1 v1.5 bị cấm trong mã hóa hiện đại?** Trình bày cơ chế tấn công Bleichenbacher Million-Message Attack dựa trên padding oracle.
* **Tại sao RSA không thể và không nên dùng để mã hóa tệp lớn trực tiếp?** Giới hạn toán học của Modulus và chi phí tính toán luỹ thừa modulo so với mã hóa khối đối xứng.

### Phân tích hệ thống lai & Hướng mở rộng:
* **So sánh Envelope Encryption với Pure RSA:** Về mặt tốc độ xử lý và tính toàn vẹn (Authenticated Encryption).
* **Vấn đề Forward Secrecy (Bảo mật chuyển tiếp):** Tại sao RSA Key Exchange trong TLS 1.2 không đảm bảo Forward Secrecy, và tại sao TLS 1.3 loại bỏ hoàn toàn RSA Key Exchange để chuyển sang (EC)DHE?
* **Liên hệ Lab 6 (Post-Quantum):** Phân tích sự chuyển dịch từ RSA sang cơ chế KEM (Key Encapsulation Mechanism) như **ML-KEM (Kyber)**.

---

## 10. Điểm Thưởng Nâng Cao (Bonus Tasks - Tối đa +15 Điểm)

1. **Thực hiện giải thuật OAEP thủ công (+5 điểm)**:
   - Tự hiện thực hàm `MGF1`, hàm tạo mặt nạ XOR và quá trình kiểm tra padding.
   - Thảo luận chi tiết cách thức mã hóa/giải mã để tránh lỗ hổng thời gian (Constant-time decoding).
2. **Nghiên cứu chuyên sâu bảo mật Lai (+10 điểm)**:
   - So sánh cơ chế **AES-GCM** với mô hình **AES-CTR + HMAC-SHA256** (Encrypt-then-MAC).
   - Tiến hành thực nghiệm đo độ biến thiên thời gian (timing variance) khi thẻ xác thực bị sai lệch nhằm chứng minh việc chống rò rỉ kênh phụ.