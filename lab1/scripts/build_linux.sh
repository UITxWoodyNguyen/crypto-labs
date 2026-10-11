#!/usr/bin/env bash
# build_linux.sh - Build + test Lab 1 (aestool) on Linux (Ubuntu/Kali).
#
# Usage (from lab1 folder):
#     scripts/build_linux.sh [compiler] [--clean] [--install-deps]
#
# Compilers:
#     gcc    - GCC (default)
#     clang  - Clang
#
# Examples:
#     scripts/build_linux.sh gcc
#     scripts/build_linux.sh clang
#     scripts/build_linux.sh gcc --clean
#     scripts/build_linux.sh clang --install-deps
#
# Env vars (optional):
#     CRYPTOPP_ROOT     Crypto++ root (default: ~/Desktop/nt219lib/cryptopp)
#     OPENSSL_ROOT_DIR  OpenSSL root per compiler (auto-detected if not set)

set -euo pipefail

# Resolve lab1 directory (where this script's parent's parent is)
LAB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_DIR"

COMPILER="gcc"
CLEAN=0
INSTALL=0

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        gcc|clang) COMPILER="$arg" ;;
        --clean) CLEAN=1 ;;
        --install-deps) INSTALL=1 ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *) echo "[ERROR] Unknown argument: $arg"; exit 1 ;;
    esac
done

# Install dependencies if requested
if [[ $INSTALL -eq 1 ]]; then
    echo "[INFO] Installing dependencies..."
    sudo apt update
    sudo apt install -y build-essential cmake git libssl-dev
    # Note: libcrypto++-dev from apt may be older; prefer custom build in ~/Desktop/nt219lib
fi

# Check required tools
for tool in cmake; do
    command -v "$tool" >/dev/null || { echo "[ERROR] Missing '$tool'. Run with --install-deps"; exit 1; }
done

# Compiler-specific setup
case "$COMPILER" in
    gcc)
        BUILD_DIR="build_ubuntu_gcc"
        CMAKE_CXX_COMPILER=""
        CRYPTOPP_COMPILER_SUBDIR="gcc"
        OPENSSL_COMPILER_SUBDIR="gcc"
        ;;
    clang)
        BUILD_DIR="build_ubuntu_clang"
        CMAKE_CXX_COMPILER="-DCMAKE_CXX_COMPILER=clang++"
        CRYPTOPP_COMPILER_SUBDIR="clang"
        OPENSSL_COMPILER_SUBDIR="clang"
        command -v clang++ >/dev/null || { echo "[ERROR] clang++ not found"; exit 1; }
        ;;
    *)
        echo "[ERROR] Invalid compiler: $COMPILER (use gcc or clang)"
        exit 1
        ;;
esac

# Default library roots
CRYPTOPP_ROOT="${CRYPTOPP_ROOT:-$HOME/Desktop/nt219lib/cryptopp}"
OPENSSL_ROOT="${OPENSSL_ROOT_DIR:-$HOME/Desktop/nt219lib/openssl/$OPENSSL_COMPILER_SUBDIR}"

# Verify Crypto++ exists
if [[ ! -d "$CRYPTOPP_ROOT/$CRYPTOPP_COMPILER_SUBDIR" ]]; then
    echo "[ERROR] Crypto++ not found at $CRYPTOPP_ROOT/$CRYPTOPP_COMPILER_SUBDIR"
    echo "        Expected structure: $CRYPTOPP_ROOT/{gcc,clang}/{include,lib}/"
    exit 1
fi

# Verify OpenSSL exists
if [[ ! -d "$OPENSSL_ROOT" ]]; then
    echo "[ERROR] OpenSSL not found at $OPENSSL_ROOT"
    echo "        Expected: $OPENSSL_ROOT/{include,lib64}/ with CMake config in lib64/cmake/OpenSSL"
    exit 1
fi

# Clean if requested
[[ $CLEAN -eq 1 ]] && rm -rf "$BUILD_DIR"

echo
echo "================================================================================"
echo "Building Lab 1 - AES Tool"
echo "Compiler: $COMPILER"
echo "Build dir: $BUILD_DIR"
echo "Crypto++ root: $CRYPTOPP_ROOT"
echo "OpenSSL root: $OPENSSL_ROOT"
echo "================================================================================"
echo

# Configure
echo "== 1/4 Configure =="
cmake -S . -B "$BUILD_DIR" \
    ${CMAKE_CXX_COMPILER} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCRYPTOPP_ROOT_DIR="$CRYPTOPP_ROOT" \
    -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT" \
    -GNinja
if [[ $? -ne 0 ]]; then
    echo "[ERROR] Configure failed."
    exit 1
fi

# Build
echo "== 2/4 Build =="
cmake --build "$BUILD_DIR" -j"$(nproc)"
if [[ $? -ne 0 ]]; then
    echo "[ERROR] Build failed."
    exit 1
fi

# Test
echo "== 3/4 Test (ctest) =="
(cd "$BUILD_DIR" && ctest --output-on-failure)
if [[ $? -ne 0 ]]; then
    echo "[ERROR] Tests failed."
    exit 1
fi

# KAT
echo "== 4/4 KAT (Known Answer Tests) =="
EXE="$BUILD_DIR/aestool"
if [[ ! -f "$EXE" ]]; then
    echo "[ERROR] Executable not found: $EXE"
    exit 1
fi
"$EXE" --kat kat/vectors.json
if [[ $? -ne 0 ]]; then
    echo "[ERROR] KAT failed."
    exit 1
fi

echo
echo "================================================================================"
echo "SUCCESS! Binary: $LAB_DIR/$EXE"
echo "================================================================================"