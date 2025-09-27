#!/usr/bin/env bash
set -euo pipefail

#
# Robust builder for the C++ core and Python bridge.
#
# Features:
# - Optional conda activation via ET_CONDA_ENV or CONDA_DEFAULT_ENV
# - Uses the Python from the active environment for both CMake and pip
# - Passes explicit Python executable to CMake to ensure correct binding
#

echo "[eartrainer] Starting C++ build + Python install"

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$SCRIPT_DIR/build"
CPP_DIR="$SCRIPT_DIR/cpp"
PY_DIR="$SCRIPT_DIR/python"

# 1) Try to activate conda env if requested
if command -v conda >/dev/null 2>&1; then
  # shellcheck disable=SC1091
  eval "$(conda shell.bash hook)" || true
  if [[ -n "${ET_CONDA_ENV:-}" ]]; then
    echo "[eartrainer] Activating conda env: $ET_CONDA_ENV"
    conda activate "$ET_CONDA_ENV" || echo "[eartrainer] Warning: failed to activate $ET_CONDA_ENV"
  elif [[ -n "${CONDA_DEFAULT_ENV:-}" ]]; then
    echo "[eartrainer] Using current conda env: $CONDA_DEFAULT_ENV"
  fi
else
  echo "[eartrainer] conda not found on PATH; proceeding without explicit activation"
fi

# 2) Check for cmake
if ! command -v cmake >/dev/null 2>&1; then
  echo "[eartrainer] Error: cmake not found on PATH. Install cmake or activate the correct environment."
  exit 127
fi

PYTHON_EXE=$(python -c 'import sys; print(sys.executable)')
echo "[eartrainer] Using Python: $PYTHON_EXE"

mkdir -p "$BUILD_DIR"
echo "[eartrainer] Configuring CMake project ..."
cmake -S "$CPP_DIR" -B "$BUILD_DIR" \
  -DPython_EXECUTABLE="$PYTHON_EXE" \
  -DPYBIND11_FINDPYTHON=ON

echo "[eartrainer] Building ..."
cmake --build "$BUILD_DIR" -- -j

echo "[eartrainer] Installing Python bridge (editable) ..."
"$PYTHON_EXE" -m pip install -e "$PY_DIR"

echo "[eartrainer] Done."
