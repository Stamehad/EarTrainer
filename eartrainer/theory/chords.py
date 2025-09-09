from __future__ import annotations

"""Chord helpers: resolve chord types and roots by degree."""

from typing import Literal

from .scales import diatonic_degree_to_pc
from .keys import PITCH_CLASS_NAMES, transpose_key, note_name_to_midi


ChordType = Literal["triad_major", "triad_minor", "triad_dim", "seventh_dominant"]


def chord_type_for_degree(scale_type: str, degree: int) -> ChordType:
    """Return a basic triad chord type for a degree (major scale mapping)."""
    if scale_type == "major":
        mapping = {
            1: "triad_major",
            2: "triad_minor",
            3: "triad_minor",
            4: "triad_major",
            5: "triad_major",
            6: "triad_minor",
            7: "triad_dim",
        }
        return mapping.get(degree, "triad_major")  # type: ignore
    # Fallback: assume major-like for MVP
    return chord_type_for_degree("major", degree)


def chord_root_name(key_root: str, scale_type: str, degree: int) -> str:
    root_pc = PITCH_CLASS_NAMES.index(transpose_key(key_root))
    offset = diatonic_degree_to_pc(scale_type, degree)
    pc = (root_pc + offset) % 12
    return PITCH_CLASS_NAMES[pc]


def chord_root_midi(key_root: str, scale_type: str, degree: int, octave: int) -> int:
    name = chord_root_name(key_root, scale_type, degree)
    return note_name_to_midi(name, octave)

