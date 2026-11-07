#!/usr/bin/env bash
# Benchmark theo protocol của lab (không cần Jupyter). Kết quả -> Output/ubuntu/{csv,logs}
# Dùng: scripts/bench.sh [build_dir]   (mặc định: build_ubuntu_* đầu tiên)
set -euo pipefail
cd "$(dirname "$0")/.."
DIR="${1:-$(ls -d build_ubuntu_* 2>/dev/null | head -n1)}"
BIN="$DIR/rsatool"
[ -x "$BIN" ] || { echo "Không thấy $BIN - hãy build trước (scripts/build_linux.sh)"; exit 1; }
COMPILER="${DIR##*_}"
mkdir -p Output/ubuntu/csv Output/ubuntu/logs
"$BIN" bench --what all --bits both --runs 30 --keygen-runs 30 --rsa-block 100 --warmup 1.0 \
  --csv "Output/ubuntu/csv/bench_ubuntu_${COMPILER}.csv" | tee Output/ubuntu/logs/bench.log
