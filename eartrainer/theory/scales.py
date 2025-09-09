from __future__ import annotations

"""Scale patterns and helpers for 12-TET.

Provides step patterns and utilities to map diatonic degrees to
pitch-class offsets.
"""

from typing import List


SCALE_PATTERNS = {
    "major": [2, 2, 1, 2, 2, 2, 1],
    "natural_minor": [2, 1, 2, 2, 1, 2, 2],
    "harmonic_minor": [2, 1, 2, 2, 1, 3, 1],
    "melodic_minor": [2, 1, 2, 2, 2, 2, 1],
}


def diatonic_degree_to_pc(scale_type: str, degree: int) -> int:
    """Return semitone offset (pitch class) from tonic for a diatonic degree.

    Args:
        scale_type: One of SCALE_PATTERNS keys.
        degree: 1-based diatonic degree (1..7).

    Returns:
        Semitone offset from tonic (0..11).
    """
    if degree < 1 or degree > 7:
        raise ValueError("degree must be 1..7")
    steps = SCALE_PATTERNS.get(scale_type)
    if steps is None:
        raise ValueError(f"Unsupported scale_type: {scale_type}")
    if degree == 1:
        return 0
    # Sum steps up to degree-1
    return sum(steps[: degree - 1]) % 12


def build_scale_pcs(scale_type: str) -> List[int]:
    """Build pitch-class offsets for 7 diatonic degrees.

    Args:
        scale_type: One of SCALE_PATTERNS keys.

    Returns:
        List of 7 integers representing semitone offsets from tonic.
    """
    return [diatonic_degree_to_pc(scale_type, d) for d in range(1, 8)]

