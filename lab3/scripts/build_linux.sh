#!/usr/bin/env bash
# Build + test trên Ubuntu LTS (GCC mặc định; đặt CXX=clang++ để dùng Clang).
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build_ubuntu_gcc}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
ctest --output-on-failure
