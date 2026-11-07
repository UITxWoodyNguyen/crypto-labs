#!/usr/bin/env bash
# ==============================================================================
# scripts/demo.sh - Chạy Bước 4 (Kiểm tra thủ công rsatool)
# Sinh toàn bộ kết quả vào: lab3/Output/windows/demo
#
# Cách dùng:
#     ./scripts/demo.sh [thư_mục_build]
# Ví dụ:
#     ./scripts/demo.sh
#     ./scripts/demo.sh build_windows_msvc
#     ./scripts/demo.sh build_ubuntu_gcc
# ==============================================================================

set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 1. Tìm binary rsatool
BUILD_ARG="${1:-}"
BIN=""

if [[ -n "$BUILD_ARG" ]]; then
    if [[ -x "$ROOT_DIR/$BUILD_ARG/Release/rsatool.exe" ]]; then
        BIN="$ROOT_DIR/$BUILD_ARG/Release/rsatool.exe"
    elif [[ -x "$ROOT_DIR/$BUILD_ARG/rsatool.exe" ]]; then
        BIN="$ROOT_DIR/$BUILD_ARG/rsatool.exe"
    elif [[ -x "$ROOT_DIR/$BUILD_ARG/rsatool" ]]; then
        BIN="$ROOT_DIR/$BUILD_ARG/rsatool"
    fi
fi

if [[ -z "$BIN" ]]; then
    for cand in \
        "$ROOT_DIR/build_windows_msvc/Release/rsatool.exe" \
        "$ROOT_DIR/build_windows_msvc/rsatool.exe" \
        "$ROOT_DIR/build_windows_gcc/rsatool.exe" \
        "$ROOT_DIR/build_windows_clang/rsatool.exe" \
        "$ROOT_DIR/build_ubuntu_gcc/rsatool" \
        "$ROOT_DIR/build_ubuntu_clang/rsatool" \
        "$ROOT_DIR/build_linux/rsatool" \
        "$ROOT_DIR/build/Release/rsatool.exe" \
        "$ROOT_DIR/build/rsatool"; do
        if [[ -x "$cand" || -f "$cand" ]]; then
            BIN="$cand"
            break
        fi
    done
fi

if [[ -z "$BIN" ]]; then
    echo "[ERROR] Không tìm thấy rsatool!"
    echo "Vui lòng build dự án trước hoặc truyền đúng thư mục build: ./scripts/demo.sh <build_dir>"
    exit 1
fi

echo "=============================================================================="
echo "CHẠY BƯỚC 4: DEMO THỦ CÔNG RSATOOL"
echo "Binary: $BIN"
echo "=============================================================================="

# 2. Tạo thư mục làm việc tại Output/windows/demo (hoặc tuỳ biến qua DEMO_OUTPUT)
DEMO_DIR="${DEMO_OUTPUT:-$ROOT_DIR/Output/windows/demo}"
mkdir -p "$DEMO_DIR"
cd "$DEMO_DIR"
echo "Thư mục làm việc: $(pwd)"
echo

# ------------------------------------------------------------------------------
# 4.1 Sinh khoá
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.1] Sinh khoá RSA-3072: pub.pem, priv.pem, pub.der, priv.der, keys_metadata.json"
echo "=============================================================================="
"$BIN" keygen --bits 3072 --pub pub.pem --priv priv.pem
echo
echo "Nội dung keys_metadata.json:"
cat keys_metadata.json
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
"$BIN" decrypt --in ct.bin --priv priv.pem --label ctx
echo

# ------------------------------------------------------------------------------
# 4.3 Mã hoá tệp lớn (tự chuyển sang hybrid) rồi giải mã và so sánh
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.3] Mã hoá tệp lớn 1 MiB (tự chuyển sang hybrid) và giải mã kiểm tra hash"
echo "=============================================================================="
echo "Đang tạo tệp ngẫu nhiên 1 MiB (big.bin)..."
if command -v openssl >/dev/null 2>&1; then
    openssl rand -out big.bin 1048576 2>/dev/null || head -c 1048576 /dev/urandom > big.bin
else
    head -c 1048576 /dev/urandom > big.bin
fi

echo "Mã hoá big.bin -> big.env.json:"
"$BIN" encrypt --in big.bin --pub pub.pem --out big.env.json --verbose
echo

echo "Giải mã big.env.json -> big.dec:"
"$BIN" decrypt --in big.env.json --priv priv.pem --out big.dec
echo

echo "So sánh dữ liệu giữa big.bin và big.dec:"
if cmp -s big.bin big.dec; then
    echo "[PASS] Hash/dữ liệu trùng khớp (True)"
else
    echo "[FAIL] Dữ liệu không khớp!"
    exit 1
fi
echo

# ------------------------------------------------------------------------------
# 4.4 Thử các trường hợp phải thất bại (fail-closed)
# ------------------------------------------------------------------------------
echo "=============================================================================="
echo "[4.4] Thử các trường hợp phải thất bại (fail-closed)"
# Mỗi lệnh phải báo lỗi, $? != 0 và KHÔNG tạo file đầu ra
echo "=============================================================================="

echo "--- 1. Sai label (yêu cầu exit code 3, o1.bin KHÔNG được tạo) ---"
"$BIN" encrypt --text "secret" --pub pub.pem --label SECURE_CONTEXT --out c1.bin >/dev/null
rm -f o1.bin
set +e
"$BIN" decrypt --in c1.bin --priv priv.pem --label WRONG_CONTEXT --out o1.bin
rc=$?
set -e
exists="False"
[[ -f o1.bin ]] && exists="True"
echo "Exit code: $rc (mong đợi: 3) | o1.bin tồn tại: $exists"
if [[ $rc -ne 0 && "$exists" == "False" ]]; then
    echo "[PASS] Fail-closed đúng yêu cầu."
else
    echo "[FAIL] Kiểm tra thất bại!"
fi
echo

echo "--- 2. Sai khoá private (yêu cầu exit code != 0, o2.bin KHÔNG được tạo) ---"
mkdir -p other
"$BIN" keygen --bits 3072 --pub other/pub.pem --priv other/priv.pem >/dev/null
rm -f o2.bin
set +e
"$BIN" decrypt --in big.env.json --priv other/priv.pem --out o2.bin
rc=$?
set -e
exists="False"
[[ -f o2.bin ]] && exists="True"
echo "Exit code: $rc | o2.bin tồn tại: $exists"
if [[ $rc -ne 0 && "$exists" == "False" ]]; then
    echo "[PASS] Fail-closed đúng yêu cầu."
else
    echo "[FAIL] Kiểm tra thất bại!"
fi
echo

echo "--- 3. Sửa 1 byte của bản mã RSA (yêu cầu exit code != 0, o3.bin KHÔNG được tạo) ---"
cp ct.bin ct_bad.bin
if command -v python3 >/dev/null 2>&1; then
    python3 -c "b = bytearray(open('ct_bad.bin','rb').read()); b[10] ^= 1; open('ct_bad.bin','wb').write(b)"
elif command -v perl >/dev/null 2>&1; then
    perl -e 'open(my $f, "+<", "ct_bad.bin"); binmode $f; seek($f, 10, 0); read($f, my $b, 1); seek($f, 10, 0); print $f chr(ord($b) ^ 1);'
elif command -v powershell >/dev/null 2>&1; then
    powershell -NoProfile -Command "$ct = [IO.File]::ReadAllBytes('ct.bin'); $ct[10] = $ct[10] -bxor 1; [IO.File]::WriteAllBytes('ct_bad.bin', $ct)"
fi
rm -f o3.bin
set +e
"$BIN" decrypt --in ct_bad.bin --priv priv.pem --label ctx --out o3.bin
rc=$?
set -e
exists="False"
[[ -f o3.bin ]] && exists="True"
echo "Exit code: $rc | o3.bin tồn tại: $exists"
if [[ $rc -ne 0 && "$exists" == "False" ]]; then
    echo "[PASS] Fail-closed đúng yêu cầu."
else
    echo "[FAIL] Kiểm tra thất bại!"
fi
echo

echo "--- 4. Khoá 2048 bit bị từ chối (yêu cầu exit code 2, p2048.pem KHÔNG được tạo) ---"
rm -f p2048.pem s2048.pem
set +e
"$BIN" keygen --bits 2048 --pub p2048.pem --priv s2048.pem
rc=$?
set -e
exists="False"
[[ -f p2048.pem ]] && exists="True"
echo "Exit code: $rc (mong đợi: 2) | p2048.pem tồn tại: $exists"
if [[ $rc -eq 2 && "$exists" == "False" ]]; then
    echo "[PASS] Từ chối khoá 2048 bit đúng yêu cầu."
else
    echo "[FAIL] Kiểm tra thất bại!"
fi
echo

cd "$ROOT_DIR"
echo "=============================================================================="
echo "HOÀN THÀNH BƯỚC 4 THÀNH CÔNG!"
echo "Toàn bộ artifacts đã được lưu tại: $DEMO_DIR"
echo "=============================================================================="
