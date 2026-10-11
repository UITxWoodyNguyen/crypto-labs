#!/usr/bin/env bash
set -e

modes=("gcm" "cbc" "ctr" "ccm" "xts" "ofb" "cfb" "ecb")

# Tạo thư mục output nếu chưa có
mkdir -p output/windows/benchmarks

# Định nghĩa các mốc bench: "size runs label"
tests=(
    "1024 1000 1KB"
    "4096 1000 4KB"
    "16384 1000 16KB"
    "262144 100 256KB"
    "1048576 50 1MB"
    "8388608 20 8MB"
)

EXE="./build_windows_gcc/aestool.exe"
KEY="output/windows/key_bin/key.bin"

for mode in "${modes[@]}"; do
    extra_flags=()
    if [ "$mode" = "ecb" ]; then
        extra_flags=("--allow-ecb")
    fi

    for t in "${tests[@]}"; do
        read -r size runs label <<< "$t"
        out_csv="output/windows/benchmarks/bench_windows_gcc_${mode}_${label}.csv"
        
        "$EXE" bench --mode "$mode" "${extra_flags[@]}" \
            --key "$KEY" \
            --size "$size" \
            --runs "$runs" \
            --out "$out_csv"
    done
done