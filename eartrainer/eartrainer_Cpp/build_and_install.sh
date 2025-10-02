#!/usr/bin/env bash
set -euo pipefail

#
# Robust builder for the C++ core and Python bridge.
#
# Features:
# - Always activates the dedicated conda env (eartrainer_cpp)
# - Uses the Python from that environment for both CMake and pip
# - Passes explicit Python executable to CMake to ensure correct binding
#

echo "[eartrainer] Starting C++ build + Python install"

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$SCRIPT_DIR/build"
CPP_DIR="$SCRIPT_DIR/cpp"
PY_DIR="$SCRIPT_DIR/python"

# 1) Activate the dedicated conda env for the C++ core (default eartrainer_cpp)
TARGET_ENV="${ET_CONDA_ENV:-eartrainer_cpp}"
if command -v conda >/dev/null 2>&1; then
  # shellcheck disable=SC1091
  eval "$(conda shell.bash hook)" || true
  if [[ "${CONDA_DEFAULT_ENV:-}" != "$TARGET_ENV" ]]; then
    echo "[eartrainer] Activating conda env: $TARGET_ENV"
    if ! conda activate "$TARGET_ENV"; then
      echo "[eartrainer] Warning: failed to activate $TARGET_ENV; continuing with current shell"
    fi
  else
    echo "[eartrainer] Using current conda env: $CONDA_DEFAULT_ENV"
  fi
else
  echo "[eartrainer] Warning: conda not found on PATH; assuming environment already prepared"
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
if "$PYTHON_EXE" -m pip install --no-build-isolation -e "$PY_DIR"; then
  echo "[eartrainer] Editable install succeeded"
else
  echo "[eartrainer] Warning: pip install failed (continuing because local sources are available)"
fi

# Ensure the freshly built _earcore module is accessible to Python.
EXT_MODULE=$(find "$BUILD_DIR" -maxdepth 1 -name '_earcore*.so' -print -quit)
if [[ -n "$EXT_MODULE" ]]; then
  TARGET_DIR="$PY_DIR/eartrainer"
  mkdir -p "$TARGET_DIR"
  cp "$EXT_MODULE" "$TARGET_DIR/"
  echo "[eartrainer] Copied $(basename "$EXT_MODULE") into $TARGET_DIR"
  cp "$EXT_MODULE" "$SCRIPT_DIR/$(basename "$EXT_MODULE")"
  echo "[eartrainer] Copied $(basename "$EXT_MODULE") into $SCRIPT_DIR"
else
  echo "[eartrainer] Warning: compiled _earcore module not found in $BUILD_DIR"
fi

echo "[eartrainer] Done."
