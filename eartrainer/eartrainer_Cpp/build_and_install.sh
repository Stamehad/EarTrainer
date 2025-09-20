#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$SCRIPT_DIR/build"
CPP_DIR="$SCRIPT_DIR/cpp"
PY_DIR="$SCRIPT_DIR/python"

cmake -S "$CPP_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -- -j
pip install -e "$PY_DIR"
