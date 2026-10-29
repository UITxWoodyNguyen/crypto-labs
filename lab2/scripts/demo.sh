#!/usr/bin/env bash
# demo.sh - Demonstrate negative tests & failure modes for Lab 2 (aestool)
# Usage: ./scripts/demo.sh <path-to-aestool>

set -u

TOOL="${1:-./aestool}"
if [[ ! -x "$TOOL" && ! -f "$TOOL" ]]; then
    echo "[ERROR] Tool not found or not executable: $TOOL"
    echo "Usage: $0 <path-to-aestool>"
    exit 1
fi

TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t 'aestool_demo')"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "================================================================================"
echo "DEMONSTRATION: Negative Tests & Fail-Closed Behavior (Lab 2 - aestool)"
echo "Tool: $TOOL"
echo "================================================================================"
echo

run_demo() {
    local desc="$1"
    local expect_rc="$2"
    shift 2
    echo "--- Case: $desc ---"
    echo "Command: $TOOL $*"
    set +e
    "$TOOL" "$@" 2>&1
    local rc=$?
    set -e
    echo "Exit code: $rc (expected: $expect_rc)"
    if [[ $rc -eq $expect_rc ]]; then
        echo "Result: PASS (failed as expected)"
    else
        echo "Result: UNEXPECTED EXIT CODE"
    fi
    echo
}

# 1. Invalid key length (must be 16 bytes = 32 hex chars)
run_demo "Invalid key length (short key)" 3 encrypt --key-hex 00112233 --text "test"

# 2. Invalid IV length (must be 16 bytes = 32 hex chars)
run_demo "Invalid IV length (short IV)" 3 encrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex 00 --text "test"

# 3. Malformed hex characters in input
run_demo "Malformed hex in input" 3 decrypt --key-hex 2b7e151628aed2a6abf7158809cf4f3c --iv-hex 00000000000000000000000000000000 --text "zz" --in-encode hex

# 4. Refuse to overwrite existing output file without --force
echo "dummy" > "$TMP_DIR/out.bin"
"$TOOL" keygen --out "$TMP_DIR/key.bin" --quiet
run_demo "Refuse overwrite without --force" 4 encrypt --key "$TMP_DIR/key.bin" --text "test" --out "$TMP_DIR/out.bin"

# 5. Missing input file
run_demo "Missing input file" 4 encrypt --key "$TMP_DIR/key.bin" --in "$TMP_DIR/nonexistent.bin" --out "$TMP_DIR/x.bin"

# 6. Counter overflow rejection (fail-closed, never reuse keystream)
run_demo "Counter overflow (max IV exhausted)" 5 encrypt --key "$TMP_DIR/key.bin" --text "two blocks of data here !!!!!!!!!" --iv-hex ffffffffffffffffffffffffffffffff --force --out "$TMP_DIR/ovf.bin"

# 7. Unsupported cipher mode (CTR only)
run_demo "Unsupported mode (gcm)" 2 encrypt --mode gcm --key "$TMP_DIR/key.bin" --text "test"

echo "================================================================================"
echo "Demonstration complete!"
echo "================================================================================"

