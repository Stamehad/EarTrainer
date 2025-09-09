from __future__ import annotations

"""Harmony helpers for simple cadence and chord voicings.

Provides I–V7–I cadence construction and simple closed-position chords.
"""

from typing import List, Optional, Dict

from .keys import note_name_to_midi, transpose_key, PITCH_CLASS_NAMES
from .scales import SCALE_PATTERNS
from .voicings import RegisterPolicy, build_voiced_chord


def _pc_of(name: str) -> int:
    return PITCH_CLASS_NAMES.index(transpose_key(name))


def _triad_intervals(scale_type: str) -> List[int]:
    # Major triad for major; minor triad for all minor variants
    if scale_type == "major":
        return [0, 4, 7]
    return [0, 3, 7]


def build_tonic_chord_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[int]:
    """Build tonic chord with explicit voicing and bass.

    Voicing (example for C major):
      - Bass: C2
      - Chord: C4–E4–G4–C5 (root doubled on top)

    For minor keys, the third is minor (e.g., A–C–E).
    """
    # Use voicing engine: triad_major/minor, prefer root on top pattern
    chord_type = "triad_major" if scale_type == "major" else "triad_minor"
    policy = RegisterPolicy(**voicing_policy) if voicing_policy else RegisterPolicy()
    return build_voiced_chord(
        key_root=root_name,
        scale_type=scale_type,
        degree=1,
        chord_type=chord_type,
        policy=policy,
        allowed_voicing_ids=["maj_root_top", "min_root_top"],
    )


def build_v7_chord_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[int]:
    """Build dominant seventh chord (V7) with explicit voicing and bass.

    Voicing (example for key of C):
      - Bass: G2
      - Chord: D4–F4–G4–B4

    Implementation details:
      - V root is a perfect fifth above tonic.
      - Chord tones are placed into octave 4 with nearest-octave wrapping
        to match the target voicing pattern.
    """
    # Use voicing engine: dominant seventh; prefer D-F-G-B style where applicable
    policy = RegisterPolicy(**voicing_policy) if voicing_policy else RegisterPolicy()
    return build_voiced_chord(
        key_root=root_name,
        scale_type=scale_type,
        degree=5,
        chord_type="seventh_dominant",
        policy=policy,
        allowed_voicing_ids=["dom7_df_gb", "dom7_root_top"],
    )


def build_cadence_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[List[int]]:
    """Return I–V7–I cadence as a list of chord MIDI note lists."""
    i_chord = build_tonic_chord_midi(root_name, scale_type, octave, voicing_policy=voicing_policy)
    v7_chord = build_v7_chord_midi(root_name, scale_type, octave, voicing_policy=voicing_policy)
    return [i_chord, v7_chord, i_chord]
