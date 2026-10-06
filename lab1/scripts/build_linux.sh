#!/usr/bin/env bash
# build_linux.sh - Build + test Lab 1 (aestool) tren Linux (Ubuntu LTS).
#
# Cach dung:   bash scripts/build_linux.sh [--clean] [--install-deps]
#   --clean         xoa thu muc build truoc khi cau hinh lai
#   --install-deps  tu cai build-essential, cmake, git, libcrypto++-dev bang apt (can sudo)
#
# Mac dinh dung Crypto++ cua he thong (libcrypto++-dev). Neu Crypto++ nam o noi khac:
#   CRYPTOPP_ROOT=/duong/dan/prefix bash scripts/build_linux.sh
#        (prefix phai co include/cryptopp/aes.h va lib/libcryptopp.a hoac .so)
#   hoac:
#   CRYPTOPP_INCLUDE_DIR=/thu/muc/cha/cua/cryptopp CRYPTOPP_LIBRARY=/duong/dan/libcryptopp.a \
#       bash scripts/build_linux.sh
#   hoac dat bien moi truong OPENSSL_ROOT_DIR neu OpenSSL khong o vi tri mac dinh.
set -euo pipefail
cd "$(dirname "$0")/../.."

CLEAN=0; INSTALL=0
for a in "$@"; do
  case "$a" in
    --clean) CLEAN=1 ;;
    --install-deps) INSTALL=1 ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "Tham so khong hop le: $a"; exit 1 ;;
  esac
done

have_cryptopp() {
  [[ -n "${CRYPTOPP_INCLUDE_DIR:-}" && -n "${CRYPTOPP_LIBRARY:-}" ]] && return 0
  [[ -n "${CRYPTOPP_ROOT:-}" ]] && return 0
  [[ -f /usr/include/crypto++/aes.h || -f /usr/include/cryptopp/aes.h || -f /usr/local/include/cryptopp/aes.h ]]
}

if [[ $INSTALL -eq 1 ]]; then
  sudo apt update
  sudo apt install -y build-essential cmake git libcrypto++-dev libssl-dev
fi

for tool in g++ cmake git; do
  command -v "$tool" >/dev/null || { echo "[LOI] Thieu '$tool'. Chay lai voi --install-deps"; exit 1; }
done
have_cryptopp || { echo "[LOI] Khong thay Crypto++. Chay lai voi --install-deps hoac dat CRYPTOPP_ROOT"; exit 1; }

[[ $CLEAN -eq 1 ]] && rm -rf build

ARGS=(-DCMAKE_BUILD_TYPE=Release)
if [[ -n "${CRYPTOPP_INCLUDE_DIR:-}" && -n "${CRYPTOPP_LIBRARY:-}" ]]; then
  ARGS+=(-DCRYPTOPP_INCLUDE_DIR="$CRYPTOPP_INCLUDE_DIR" -DCRYPTOPP_LIBRARY="$CRYPTOPP_LIBRARY")
elif [[ -n "${CRYPTOPP_ROOT:-}" ]]; then
  ARGS+=(-DCMAKE_PREFIX_PATH="$CRYPTOPP_ROOT")
fi
command -v ninja >/dev/null && ARGS+=(-G Ninja)

# OpenSSL path (optional)
if [[ -n "${OPENSSL_ROOT_DIR:-}" ]]; then
  ARGS+=(-DOpenSSL_ROOT_DIR="$OPENSSL_ROOT_DIR")
fi

echo "== 1/4 Cau hinh =="
cmake -S . -B build "${ARGS[@]}"
echo "== 2/4 Build =="
cmake --build build -j"$(nproc)"
echo "== 3/4 ctest =="
(cd build && ctest --output-on-failure)
echo "== 4/4 KAT =="
./build/lab1/aestool --kat lab1/kat/vectors.json

echo
echo "Xong. Binary: $(pwd)/build/lab1/aestool"
