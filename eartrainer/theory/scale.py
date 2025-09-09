from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, Set
import random

from .note_utils import NAME_TO_PC, PITCH_CLASS_NAMES_SHARP, note_name_to_midi
from .scales import diatonic_degree_to_pc


MAJOR_DIATONIC_QUALITIES = {1: "maj", 2: "min", 3: "min", 4: "maj", 5: "dom7", 6: "min", 7: "dim"}
NATMIN_DIATONIC_QUALITIES = {1: "min", 2: "dim", 3: "maj", 4: "min", 5: "min", 6: "maj", 7: "maj"}  # placeholder

SCALE_TYPES = {"major", "natural_minor", "harmonic_minor", "melodic_minor"}


@dataclass(frozen=True)
class ChordSpec:
    root_name: str           # absolute note name (e.g., "F#")
    base_quality: str        # "maj"|"min"|"dim"|"aug"|"dom7"|...
    degree: int              # 1..7
    allowed_extensions: Set[str]  # e.g., {"triad","7","9"} (policy may intersect)


class Scale:
    """A concrete key+scale (e.g., Eb major). Responsible for diatonic grammar."""

    def __init__(self, root_name: str, scale_type: str = "major") -> None:
        assert scale_type in SCALE_TYPES, f"Unsupported scale_type: {scale_type}"
        assert root_name in NAME_TO_PC, f"Unknown root: {root_name}"
        self.root_name = root_name
        self.scale_type = scale_type
        self._root_pc = NAME_TO_PC[root_name]

    def random_degree(self, weights: Dict[int, float] | None = None) -> int:
        """Sample a scale degree 1..7. If weights provided, use them."""
        degrees = list(range(1, 8))
        if not weights:
            return random.choice(degrees)
        w = [max(float(weights.get(d, 0.0)), 0.0) for d in degrees]
        total = sum(w)
        if total <= 0:
            return random.choice(degrees)
        r = random.random() * total
        acc = 0.0
        for d, wd in zip(degrees, w):
            acc += wd
            if r <= acc:
                return d
        return degrees[-1]

    def _degree_root_name(self, deg: int) -> str:
        offset = diatonic_degree_to_pc(self.scale_type, deg)
        pc = (self._root_pc + offset) % 12
        return PITCH_CLASS_NAMES_SHARP[pc]

    def degree_to_chord_spec(self, deg: int) -> ChordSpec:
        """Map degree to a functional chord spec in this key."""
        assert 1 <= deg <= 7
        # Determine base quality by scale type (simplified for MVP):
        if self.scale_type == "major":
            q = MAJOR_DIATONIC_QUALITIES[deg]
        else:
            # TODO: expand proper minor-scale diatonic mapping
            q = "min" if deg in (2, 3, 6) else ("maj" if deg in (1, 4) else ("dom7" if deg == 5 else "dim"))
        # Compute absolute root name for this degree
        root_name = self._degree_root_name(deg)
        allowed_ext: Set[str] = {"triad"}
        if q in {"dom7", "maj7", "min7"}:
            allowed_ext |= {"7"}
        return ChordSpec(root_name=root_name, base_quality=q, degree=deg, allowed_extensions=allowed_ext)

    def degree_root_midi(self, degree: int, octave: int) -> int:
        return note_name_to_midi(self._degree_root_name(degree), octave)

    def transpose(self, new_root: str) -> "Scale":
        return Scale(new_root, self.scale_type)
