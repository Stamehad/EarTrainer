# Ear Trainer Core (C++/Python)

This workspace contains a standalone C++ SessionEngine implementation plus a
pybind11-powered Python façade. The core generates ear-training questions,
tracks session progress, and returns assistance/scoring metadata so a UI layer
can focus on interaction and playback.

## Layout

```
cpp/                # CMake project for the C++ static library + tests
python/             # Python package (eartrainer) and CLI demo
```

Key components:

- `cpp/include/ear/` – public headers for the SessionEngine API.
- `cpp/src/` – engine implementation, JSON bridge helpers, pybind bindings.
- `cpp/drills/` – samplers + drill modules for note and chord drills.
- `cpp/tests/` – acceptance-style regression tests (no external framework).
- `python/eartrainer/` – dataclasses, wrapper, and `ui_demo.py` CLI.
- `resources/voicings/` – reference voicing data used by the C++ chord drill.

## Building the C++ core

```bash
mkdir -p build && cd build
cmake ../cpp
cmake --build .
ctest --output-on-failure
```

The build produces the static library `libear_core.a` and the `ear_core_tests`
executable. All tests must pass before integrating further changes.

### Python extension `_earcore`

If `pybind11` is available in your environment, the CMake configuration auto-
compiles the `_earcore` extension module. When `pybind11` is missing the build
continues without the extension – install `pybind11` (e.g. `pip install
pybind11`) to enable it.

## Python package & demo

After building the extension, install the Python package in editable mode from
`eartrainer/eartrainer_Cpp/python`:

```bash
cd eartrainer/eartrainer_Cpp/python
pip install -e .
python -m pytest
```

Run the simple CLI demo:

```bash
python -m eartrainer.ui_demo
```

The demo surfaces each generated question, lets you pull assists, and submits
results back into the C++ core.

## Testing overview

The acceptance tests in `cpp/tests/test_session_engine.cpp` cover:

1. Deterministic question sequencing for note drills with a fixed seed.
2. Assistance purity (no RNG advancement).
3. Idempotent result submission.
4. RNG advance discipline (only via `next_question`).
5. JSON round-trip fidelity for the bridge helpers.

A lightweight Python test (`python/tests/test_bridge.py`) exercises the end-to-
end binding.

## Next steps

- Extend samplers/drills to cover additional musical material or user-tunable
  difficulty knobs.
- Flesh out `StorageAdapter` and scoring analytics once persistence targets are
  defined.
- Integrate with the production UI by consuming `QuestionBundle` prompt plans
  and returning `ResultReport`s from real user interactions.
