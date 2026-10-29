#!/usr/bin/env bash
# scripts/build.sh - Shortcut to scripts/build_linux.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build_linux.sh" "$@"

