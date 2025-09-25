# EarTrainer (Python CLI + Minimal GUI)

EarTrainer is a functional ear‑training toolkit for drilling scale degrees, harmonic intervals, melodic lines, and chords — all in a clear tonal context.

- Chords are voiced realistically: each harmony is served from a small, curated set of three distinct right‑hand voicings (different top notes) plus a bass, to increase variety and reduce overfitting.
- Melodies are sampled with musical bias — small steps favored, consecutive big leaps discouraged — to sound more "line‑like" while still exploring the key space.

Planned features
- Chord progressions: sampled and from a library of common progressions.
- Inversions and extensions: add 1st/2nd inversions, 7ths and beyond.
- Melodic lines with harmony: sampled and song‑inspired accompaniment.
- Non‑diatonic awareness: identify important borrowed/altered chords.
- Chord‑relative pitch ID: identify notes relative to the current chord.
- Natural modulations: key changes that evolve within a session.

This README documents the Python version on the `main` branch. A C++ core is under active development on a separate local branch and not pushed yet (see note at the end).

## Install

Using Conda (recommended):

```
conda create -n eartrainer_py python=3.11 -y
conda activate eartrainer_py
pip install -U pip
pip install -e .
```

Or with venv:

```
python -m venv .venv
source .venv/bin/activate
pip install -U pip
pip install -e .
```

Provide a SoundFont (FluidSynth): see `eartrainer/resources/README.md` for options (GrandPiano.sf2 or GM.sf2) and set `audio.soundfont_path` in `eartrainer/config/defaults.yml` if needed.

## Run (CLI)

List available drills and presets:

```
eartrainer list-drills
eartrainer show-params --drill note --preset default
```

Run a single drill:

```
eartrainer run --config eartrainer/config/defaults.yml \
  --drill note --preset default --questions 40 --key C --scale major
```

Run a sequence (multiple drills back‑to‑back):

```
eartrainer run-sequence --config eartrainer/config/defaults.yml \
  --steps note:40,chord:40 --key Random --scale major
```

Assistance and context (CLI)
- Reference playback (cadence) before the drill.
- Optional drone (sustained I/V) with auto‑duck during questions.
- Replay reference shortcut (press `r` then Enter when prompted if enabled).

## Run (GUI)

```
eartrainer-gui
```

The GUI lets you pick Key, Scale, Drill, question count, and toggle drone. It also supports running predefined “Sets” (see below) from YAML.

## Drills vs. Sets

- A “Drill” is a single training mode (e.g., note, chord, chord_relative, melodic, harmonic_interval). Drills expose presets and parameters via `list-drills` and `show-params`.
- A “Set” is a scripted session composed of multiple steps (drills + counts) defined in YAML under `eartrainer/resources/training_sets/` (e.g., `basic.yml`). The GUI can run these sets directly.

## SoundFonts

See `eartrainer/resources/README.md` for details. In short:
- Place a Grand Piano at `soundfonts/GrandPiano.sf2` or a GM set at `soundfonts/GM.sf2`.
- Update `audio.soundfont_path` in `eartrainer/config/defaults.yml` if you use different names/locations.

## Configuration

- Default config: `eartrainer/config/defaults.yml`
- Training sets: `eartrainer/resources/training_sets/`
- Drone presets (GM): `eartrainer/resources/drones/`

Common tweaks: drill length, allowed degrees, key/scale, drone template/volume, output paths.

## Development

- Dependencies: `pip install -r requirements.txt` (or rely on `pyproject.toml` when installing the package).
- Entry points: `eartrainer` (CLI), `eartrainer-gui` (GUI).

## About the C++ Core (WIP)

A cross‑platform C++ SessionEngine (with Python bindings) is being developed on a separate local branch to support future integrations (e.g., iOS/Android). It is not part of `main` yet and does not affect the Python CLI/GUI.
