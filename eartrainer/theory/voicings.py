from __future__ import annotations

"""Voicing engine: data-driven chord voicings with register policy."""

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import random

from .keys import (
    PITCH_CLASS_NAMES,
    transpose_key,
    note_name_to_midi,
)
from .scales import diatonic_degree_to_pc

try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover
    yaml = None  # type: ignore


_VOICING_CACHE: Optional[Dict] = None


def _voicings_path() -> Path:
    # eartrainer/theory/voicings.py -> resources/voicings.yml
    return Path(__file__).resolve().parents[1] / "resources" / "voicings.yml"


def load_voicing_bank() -> Dict:
    global _VOICING_CACHE
    if _VOICING_CACHE is not None:
        return _VOICING_CACHE
    path = _voicings_path()
    if yaml is None:
        raise RuntimeError("pyyaml is required to load voicings.yml")
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    _VOICING_CACHE = data
    return data


def choose_voicing(chord_type: str, allowed_ids: Optional[List[str]] = None, strategy: str = "weighted_random") -> Dict:
    bank = load_voicing_bank()
    variants: List[Dict] = bank.get(chord_type, [])
    if allowed_ids:
        variants = [v for v in variants if v.get("id") in allowed_ids]
    if not variants:
        raise ValueError(f"No voicings available for type '{chord_type}'")
    if strategy == "weighted_random":
        weights = [float(v.get("weight", 1.0)) for v in variants]
        total = sum(weights)
        if total <= 0:
            return random.choice(variants)
        r = random.random() * total
        acc = 0.0
        for v, w in zip(variants, weights):
            acc += w
            if r <= acc:
                return v
        return variants[-1]
    # default: uniform
    return random.choice(variants)


@dataclass
class RegisterPolicy:
    bass_octave: int = 2
    chord_octave_by_pc: List[int] = None  # type: ignore

    def __post_init__(self) -> None:
        if self.chord_octave_by_pc is None:
            self.chord_octave_by_pc = [4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3]
        if len(self.chord_octave_by_pc) != 12:
            raise ValueError("chord_octave_by_pc must have 12 entries")


def _degree_root_name(key_root: str, scale_type: str, degree: int) -> str:
    root_pc = PITCH_CLASS_NAMES.index(transpose_key(key_root))
    offset = diatonic_degree_to_pc(scale_type, degree)
    pc = (root_pc + offset) % 12
    return PITCH_CLASS_NAMES[pc]


def build_voiced_chord(
    key_root: str,
    scale_type: str,
    degree: int,
    chord_type: str,
    policy: Optional[RegisterPolicy] = None,
    allowed_voicing_ids: Optional[List[str]] = None,
    selection_strategy: str = "weighted_random",
) -> List[int]:
    """Build a voiced chord (with independent bass) as MIDI notes.

    - Chooses a voicing pattern for the chord type.
    - Places the chord stack at a register chosen by policy.
    - Adds a bass on the root at policy.bass_octave.
    """
    if policy is None:
        policy = RegisterPolicy()
    root_name = _degree_root_name(key_root, scale_type, degree)
    root_pc = PITCH_CLASS_NAMES.index(root_name)
    chord_oct = int(policy.chord_octave_by_pc[root_pc])
    root_midi_stack = note_name_to_midi(root_name, chord_oct)
    bass_midi = note_name_to_midi(root_name, int(policy.bass_octave))

    variant = choose_voicing(chord_type, allowed_ids=allowed_voicing_ids, strategy=selection_strategy)
    intervals: List[int] = [int(x) for x in variant.get("intervals", [])]
    chord_midis = [root_midi_stack + iv for iv in intervals]

    return [bass_midi] + chord_midis

