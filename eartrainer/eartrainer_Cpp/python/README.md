# Eartrainer Python Package

This package provides a thin Python wrapper around the C++ SessionEngine.

Usage:
- Build the C++ core and `_earcore` pybind11 module with CMake under
  `eartrainer/eartrainer_Cpp/build`.
- Install this package in editable mode:
  `pip install -e .`
- Ensure Python can find the compiled `_earcore` module (e.g., add the CMake
  build directory to `PYTHONPATH`).

The high-level API is exposed via `eartrainer.session_engine.SessionEngine` and
its dataclasses in `eartrainer.models`.
