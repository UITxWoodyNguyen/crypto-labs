#!/usr/bin/env bash
set -euo pipefail

LAB_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_DIR"
OUT_DIR="$LAB_DIR/Output/ubuntu/bench"
mkdir -p "$OUT_DIR"

if [[ -n "${AESTOOL:-}" ]]; then
    TOOL="$AESTOOL"
else
    TOOL="$LAB_DIR/build_ubuntu_gcc/aestool"
fi
if [[ ! -x "$TOOL" ]]; then
    echo "[ERROR] aestool executable not found: $TOOL" >&2
    echo "        Set AESTOOL or build with scripts/build_linux.sh first." >&2
    exit 1
fi

run_bench() {
    local size="$1"
    local runs="$2"
    local output="$OUT_DIR/bench_ubuntu_${size}.csv"
    echo "[INFO] ${size}: ${runs} runs -> ${output}"
    "$TOOL" bench --sizes "$size" --runs "$runs" --csv "$output"
}

run_bench 1MiB 500
run_bench 100MiB 100
run_bench 1GiB 30
