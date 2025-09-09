from __future__ import annotations

"""Key and pitch utilities for mapping to MIDI.

Includes pitch-class names, enharmonic handling, and diatonic degree
to semitone offset mapping via scales.
"""

from typing import Dict, Tuple

from .scales import diatonic_degree_to_pc


PITCH_CLASS_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

_ENHARMONIC: Dict[str, str] = {
    "Db": "C#",
    "Eb": "D#",
    "Gb": "F#",
    "Ab": "G#",
    "Bb": "A#",
    # common edge enharmonics (not used as keys here but supported)
    "B#": "C",
    "E#": "F",
    "Cb": "B",
    "Fb": "E",
}


def _normalize_name(name: str) -> str:
    return _ENHARMONIC.get(name, name)


def note_name_to_midi(name: str, octave: int) -> int:
    """Convert note name and octave to MIDI number.

    Uses C4 = 60 (MIDI middle C). Supports sharps and flats.

    Args:
        name: Pitch name like "C", "C#", "Db", etc.
        octave: Integer octave number (e.g., 4 => 60 for C).

    Returns:
        MIDI note number (0..127).
    """
    norm = _normalize_name(name)
    if norm not in PITCH_CLASS_NAMES:
        raise ValueError(f"Unsupported note name: {name}")
    pc = PITCH_CLASS_NAMES.index(norm)
    midi = (octave + 1) * 12 + pc  # C4 -> 60
    if midi < 0 or midi > 127:
        raise ValueError("MIDI out of range")
    return midi


def degree_to_semitone_offset(scale_type: str, degree: int) -> int:
    """Return semitone offset from tonic for a diatonic degree (1..7)."""
    return diatonic_degree_to_pc(scale_type, degree)


def transpose_key(root: str) -> str:
    """Validate and normalize a key root name.

    Args:
        root: Root name potentially with flats.

    Returns:
        Normalized sharp-based name.
    """
    norm = _normalize_name(root)
    if norm not in PITCH_CLASS_NAMES:
        raise ValueError(f"Invalid key root: {root}")
    return norm


def note_str_to_midi(note: str) -> int:
    """Parse a note string like 'C4', 'Db3', 'G#5' into a MIDI number."""
    if not note or len(note) < 2:
        raise ValueError(f"Invalid note string: {note}")
    name = note[0].upper()
    idx = 1
    if idx < len(note) and note[idx] in ("#", "b"):
        name += note[idx]
        idx += 1
    try:
        octave = int(note[idx:])
    except ValueError as e:
        raise ValueError(f"Invalid octave in note string: {note}") from e
    return note_name_to_midi(name, octave)
