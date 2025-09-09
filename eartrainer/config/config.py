from __future__ import annotations

"""Configuration loading and validation for EarTrainer.

This module loads YAML configuration, applies defaults, and validates
that enumerations and paths are sane for the MVP CLI.
"""

from pathlib import Path
from typing import Any, Dict, Optional

import sys

try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover - dependency issues handled at runtime
    yaml = None  # type: ignore


ALLOWED_BACKENDS = {"fluidsynth"}
ALLOWED_SCALE_TYPES = {"major", "natural_minor", "harmonic_minor", "melodic_minor"}
ALLOWED_REFERENCE_MODES = {"cadence", "scale", "drone"}
ALLOWED_DRILL_TYPES = {"note"}  # future: interval, chord
ALLOWED_INSTRUMENTS = {"piano"}


def _load_yaml(path: Path) -> Dict[str, Any]:
    if yaml is None:
        print("ERROR: pyyaml is not installed. Please install dependencies.", file=sys.stderr)
        sys.exit(1)
    try:
        with path.open("r", encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except FileNotFoundError:
        print(f"ERROR: Config file not found: {path}", file=sys.stderr)
        sys.exit(1)


def load_config(path: Optional[str] = None) -> Dict[str, Any]:
    """Load configuration from YAML or defaults.

    Args:
        path: Optional path to a YAML config. If None, use package defaults.

    Returns:
        A dictionary with configuration values.
    """
    if path:
        cfg = _load_yaml(Path(path))
    else:
        default_path = Path(__file__).with_name("defaults.yml")
        cfg = _load_yaml(default_path)
    return cfg


def validate_config(cfg: Dict[str, Any]) -> Dict[str, Any]:
    """Apply defaults and validate configuration values.

    Performs sanity checks on enums and ensures the soundfont path exists
    when using the FluidSynth backend.

    Args:
        cfg: The raw configuration dictionary.

    Returns:
        The validated and merged configuration dictionary.
    """
    # Shallow defaults for missing sections
    cfg.setdefault("audio", {})
    cfg.setdefault("context", {})
    cfg.setdefault("drill", {})
    cfg.setdefault("stats", {})
    cfg.setdefault("transposition", {})
    cfg.setdefault("ui", {})
    cfg.setdefault("drone", {})

    audio = cfg["audio"]
    context = cfg["context"]
    drill = cfg["drill"]
    stats = cfg["stats"]
    transp = cfg["transposition"]

    # Apply section defaults
    audio.setdefault("backend", "fluidsynth")
    audio.setdefault("soundfont_path", "./soundfonts/GrandPiano.sf2")
    audio.setdefault("sample_rate", 44100)
    audio.setdefault("gain", 0.5)
    audio.setdefault("instrument", "piano")

    context.setdefault("key", "C")
    context.setdefault("scale_type", "major")
    context.setdefault("reference_mode", "cadence")
    context.setdefault("drone", {"tones": ["I", "V"], "sustain": False})
    # Voicing policy
    context.setdefault(
        "voicing",
        {
            "bass_octave": 2,
            "chord_octave_by_pc": [4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3],
            "selection_strategy": "weighted_random",
        },
    )

    drill.setdefault("type", "note")
    drill.setdefault("questions", 50)
    drill.setdefault("allow_replay_reference", True)
    drill.setdefault("degrees_in_scope", ["1", "2", "3", "4", "5", "6", "7"])
    drill.setdefault("include_chromatic", False)
    drill.setdefault("test_note_delay_ms", 300)

    stats.setdefault("output_path", "./session_stats.json")
    stats.setdefault("show_per_question_feedback", True)
    stats.setdefault("show_summary", True)

    transp.setdefault("randomize_key_each_session", True)

    # Drone defaults
    drone = cfg["drone"]
    drone.setdefault("enabled", False)
    drone.setdefault("template", "root5")
    drone.setdefault("instrument", "strings")
    drone.setdefault("volume", 0.35)
    drone.setdefault("fade_in_ms", 200)
    drone.setdefault("fade_out_ms", 250)
    drone.setdefault("inactivity_timeout_s", 10)
    drone.setdefault("restart_on_key_change", True)
    drone.setdefault("auto_pause_during_question", False)
    drone.setdefault("channel", 1)
    drone.setdefault("templates_path", "eartrainer/resources/drones/strings_basic.yml")

    # Enum validations
    backend = audio.get("backend")
    if backend not in ALLOWED_BACKENDS:
        print(f"WARNING: Unsupported audio backend '{backend}', falling back to 'fluidsynth'.")
        audio["backend"] = "fluidsynth"

    instrument = audio.get("instrument")
    if instrument not in ALLOWED_INSTRUMENTS:
        print(f"WARNING: Unsupported instrument '{instrument}', using 'piano'.")
        audio["instrument"] = "piano"

    scale_type = context.get("scale_type")
    if scale_type not in ALLOWED_SCALE_TYPES:
        print(f"WARNING: Unsupported scale_type '{scale_type}', using 'major'.")
        context["scale_type"] = "major"

    reference_mode = context.get("reference_mode")
    if reference_mode not in ALLOWED_REFERENCE_MODES:
        print(f"WARNING: Unsupported reference_mode '{reference_mode}', using 'cadence'.")
        context["reference_mode"] = "cadence"

    drill_type = drill.get("type")
    if drill_type not in ALLOWED_DRILL_TYPES:
        print(f"WARNING: Unsupported drill type '{drill_type}', using 'note'.")
        drill["type"] = "note"

    # Ensure soundfont exists for FluidSynth
    if audio["backend"] == "fluidsynth":
        sf_path = Path(audio.get("soundfont_path", ""))
        if not sf_path.exists():
            print(
                f"ERROR: SoundFont not found at '{sf_path}'. Place a .sf2 in ./soundfonts and update the path.",
                file=sys.stderr,
            )
            sys.exit(1)

    # Normalize degrees to strings "1".."7"
    degrees = drill.get("degrees_in_scope", [])
    drill["degrees_in_scope"] = [str(d) for d in degrees]

    return cfg
