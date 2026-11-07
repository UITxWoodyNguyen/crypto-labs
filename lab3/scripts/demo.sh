#!/usr/bin/env bash
# ==============================================================================
# scripts/demo.sh - Chạy Bước 4 (Kiểm tra thủ công rsatool)
# Sinh toàn bộ kết quả vào: lab3/Output/ubuntu/demo
#
# Cách dùng:
#     ./scripts/demo.sh [thư_mục_build]
# Ví dụ:
#     ./scripts/demo.sh
#     ./scripts/demo.sh build_ubuntu_gcc
#     ./scripts/demo.sh build_ubuntu_clang
#
# Biến môi trường tuỳ chọn:
#     DEMO_OUTPUT=/duong/dan/khac   # đổi thư mục xuất kết quả
# ==============================================================================

set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OS_NAME="ubuntu"

# 1. Tìm binary rsatool
BUILD_ARG="${1:-}"
BIN=""

if [[ -n "$BUILD_ARG" ]]; then
    for cand in \
        "$ROOT_DIR/$BUILD_ARG/rsatool" \
        "$ROOT_DIR/$BUILD_ARG/Release/rsatool"; do
        if [[ -f "$cand" && -x "$cand" ]]; then BIN="$cand"; break; fi
    done
fi

if [[ -z "$BIN" ]]; then
    for cand in \
        "$ROOT_DIR/build_ubuntu_gcc/rsatool" \
        "$ROOT_DIR/build_ubuntu_clang/rsatool" \
        "$ROOT_DIR/build_linux/rsatool" \
        "$ROOT_DIR/build/rsatool"; do
        if [[ -f "$cand" && -x "$cand" ]]; then BIN="$cand"; break; fi
    done
fi

if [[ -z "$BIN" ]]; then
    echo "[ERROR] Không tìm thấy rsatool!"
    echo "Hãy build trước (scripts/build_linux.sh) hoặc truyền đúng thư mục build: ./scripts/demo.sh <build_dir>"
    exit 1
fi

echo "=============================================================================="
echo "CHẠY BƯỚC 4: DEMO THỦ CÔNG RSATOOL"
echo "Binary: $BIN"
echo "=============================================================================="

# 2. Thư mục làm việc: Output/ubuntu/demo (hoặc DEMO_OUTPUT)
DEMO_DIR="${DEMO_OUTPUT:-$ROOT_DIR/Output/$OS_NAME/demo}"
mkdir -p "$DEMO_DIR"
cd "$DEMO_DIR" || exit 1
echo "Thư mục làm việc: $(pwd)"
echo

PASS_COUNT=0
FAIL_COUNT=0
pass() { echo "[PASS] $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo "[FAIL] $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

# ------------------------------------------------------------------------------
# 4.1 Sinh khoá
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.1] Sinh khoá RSA-3072: pub.pem, priv.pem, pub.der, priv.der, keys_metadata.json"
echo "=============================================================================="
"$BIN" keygen --bits 3072 --pub pub.pem --priv priv.pem
rc=$?
if [[ $rc -eq 0 && -f pub.pem && -f priv.pem && -f pub.der && -f priv.der && -f keys_metadata.json ]]; then
    pass "keygen tạo đủ 5 file"
else
    fail "keygen (rc=$rc) thiếu file"
fi
echo
echo "Nội dung keys_metadata.json:"
cat keys_metadata.json
echo
ls -l priv.pem
echo

# ------------------------------------------------------------------------------
# 4.2 Mã hoá ngắn (RSA-OAEP trực tiếp) và giải mã
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.2] Mã hoá ngắn (RSA-OAEP trực tiếp) và giải mã"
echo "=============================================================================="
echo "1. In bản mã hex trực tiếp ra màn hình:"
"$BIN" encrypt --text "hello" --pub pub.pem --label ctx
echo

echo "2. Mã hoá ra tệp ct.bin:"
"$BIN" encrypt --text "hello" --pub pub.pem --label ctx --out ct.bin
echo

echo "3. Giải mã từ ct.bin:"
out="$("$BIN" decrypt --in ct.bin --priv priv.pem --label ctx)"
echo "$out"
if [[ "$out" == "hello" && "$(stat -c %s ct.bin)" -eq 384 ]]; then
    pass "round trip RSA-OAEP trực tiếp (ciphertext 384 byte)"
else
    fail "round trip RSA-OAEP trực tiếp"
fi
echo

# ------------------------------------------------------------------------------
# 4.3 Mã hoá tệp lớn (tự chuyển sang hybrid) rồi giải mã và so sánh
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.3] Mã hoá tệp lớn 1 MiB (tự chuyển sang hybrid) và giải mã kiểm tra dữ liệu"
echo "=============================================================================="
echo "Đang tạo tệp ngẫu nhiên 1 MiB (big.bin)..."
head -c 1048576 /dev/urandom > big.bin

echo "Mã hoá big.bin -> big.env.json:"
"$BIN" encrypt --in big.bin --pub pub.pem --out big.env.json --verbose
echo

echo "Giải mã big.env.json -> big.dec:"
"$BIN" decrypt --in big.env.json --priv priv.pem --out big.dec
echo

echo "So sánh dữ liệu giữa big.bin và big.dec:"
if cmp -s big.bin big.dec; then
    pass "hybrid 1 MiB: dữ liệu trùng khớp"
else
    fail "hybrid 1 MiB: dữ liệu không khớp"
fi
echo

echo "Ranh giới 318 / 319 byte:"
head -c 318 /dev/urandom > m318.bin
head -c 319 /dev/urandom > m319.bin
"$BIN" encrypt --in m318.bin --pub pub.pem --out c318.bin
"$BIN" encrypt --in m319.bin --pub pub.pem --out c319.bin
if [[ "$(stat -c %s c318.bin)" -eq 384 ]]; then pass "318 B -> RSA-OAEP trực tiếp (384 B)"; else fail "318 B"; fi
if [[ "$(head -c 1 c319.bin)" == "{" ]]; then pass "319 B -> hybrid (JSON envelope)"; else fail "319 B"; fi
echo

# ------------------------------------------------------------------------------
# 4.4 Thử các trường hợp phải thất bại (fail-closed)
# Mỗi lệnh phải báo lỗi, exit code != 0 và KHÔNG tạo file đầu ra
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.4] Thử các trường hợp phải thất bại (fail-closed)"
echo "=============================================================================="

echo "--- 1. Sai label (mong đợi exit code 3, o1.bin KHÔNG được tạo) ---"
"$BIN" encrypt --text "secret" --pub pub.pem --label SECURE_CONTEXT --out c1.bin >/dev/null
rm -f o1.bin
"$BIN" decrypt --in c1.bin --priv priv.pem --label WRONG_CONTEXT --out o1.bin
rc=$?
exists="False"; [[ -f o1.bin ]] && exists="True"
echo "Exit code: $rc (mong đợi: 3) | o1.bin tồn tại: $exists"
if [[ $rc -eq 3 && "$exists" == "False" ]]; then pass "sai label -> fail-closed"; else fail "sai label"; fi
echo

echo "--- 2. Sai khoá private (mong đợi exit code != 0, o2.bin KHÔNG được tạo) ---"
mkdir -p other
"$BIN" keygen --bits 3072 --pub other/pub.pem --priv other/priv.pem >/dev/null
rm -f o2.bin
"$BIN" decrypt --in big.env.json --priv other/priv.pem --out o2.bin
rc=$?
exists="False"; [[ -f o2.bin ]] && exists="True"
echo "Exit code: $rc | o2.bin tồn tại: $exists"
if [[ $rc -ne 0 && "$exists" == "False" ]]; then pass "sai khoá private -> fail-closed"; else fail "sai khoá private"; fi
echo

echo "--- 3. Sửa 1 byte của bản mã RSA (mong đợi exit code != 0, o3.bin KHÔNG được tạo) ---"
cp ct.bin ct_bad.bin
if command -v python3 >/dev/null 2>&1; then
    python3 -c "b = bytearray(open('ct_bad.bin','rb').read()); b[10] ^= 1; open('ct_bad.bin','wb').write(b)"
else
    # dự phòng không cần python: ghi đè byte thứ 11 bằng giá trị khác
    orig=$(dd if=ct_bad.bin bs=1 skip=10 count=1 2>/dev/null | od -An -tu1 | tr -d ' ')
    new=$(( (orig ^ 1) & 255 ))
    printf "$(printf '\\%03o' "$new")" | dd of=ct_bad.bin bs=1 seek=10 count=1 conv=notrunc 2>/dev/null
fi
rm -f o3.bin
"$BIN" decrypt --in ct_bad.bin --priv priv.pem --label ctx --out o3.bin
rc=$?
exists="False"; [[ -f o3.bin ]] && exists="True"
echo "Exit code: $rc | o3.bin tồn tại: $exists"
if [[ $rc -ne 0 && "$exists" == "False" ]]; then pass "bản mã RSA bị sửa -> fail-closed"; else fail "bản mã RSA bị sửa"; fi
echo

echo "--- 4. Khoá 2048 bit bị từ chối (mong đợi exit code 2, p2048.pem KHÔNG được tạo) ---"
rm -f p2048.pem s2048.pem
"$BIN" keygen --bits 2048 --pub p2048.pem --priv s2048.pem
rc=$?
exists="False"; [[ -f p2048.pem ]] && exists="True"
echo "Exit code: $rc (mong đợi: 2) | p2048.pem tồn tại: $exists"
if [[ $rc -eq 2 && "$exists" == "False" ]]; then pass "từ chối khoá 2048 bit"; else fail "khoá 2048 bit"; fi
echo

cd "$ROOT_DIR" || exit 1
echo "=============================================================================="
echo "KẾT QUẢ BƯỚC 4: $PASS_COUNT PASS, $FAIL_COUNT FAIL"
echo "Toàn bộ artifacts đã được lưu tại: $DEMO_DIR"
echo "=============================================================================="
[[ $FAIL_COUNT -eq 0 ]]