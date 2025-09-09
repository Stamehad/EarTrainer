from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple, Dict, Any
from .scale import ChordSpec


@dataclass(frozen=True)
class Chord:
    """A concrete chord instance for one playback/question."""

    root_name: str
    quality: str                 # "maj" | "min" | "dim" | "aug" | "dom7" | "maj7" | ...
    degree: int                  # functional degree 1..7
    extensions: Tuple[str, ...]  # empty -> triad

    def to_symbol(self) -> str:
        """Return a compact symbol for grading/logs (e.g., 'ii', 'V7')."""
        # TODO: encode case & suffix based on quality/extensions
        return f"deg{self.degree}:{self.quality}{''.join(self.extensions)}"

    def as_voicing_request(self) -> Dict[str, Any]:
        """Minimal info the voicing selector needs."""
        return {
            "root_name": self.root_name,
            "quality": self.quality,
            "extensions": set(self.extensions),
        }

    @staticmethod
    def from_spec(spec: ChordSpec, extensions: Tuple[str, ...] = ()) -> "Chord":
        """Build a concrete Chord by adding sampled extensions to a ChordSpec."""
        return Chord(
            root_name=spec.root_name,
            quality=spec.base_quality,
            degree=spec.degree,
            extensions=extensions,
        )
