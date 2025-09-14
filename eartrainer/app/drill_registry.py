from __future__ import annotations

"""Drill registry and metadata.

Discover drills, expose metadata, validate parameters, and construct
drill instances via a simple factory.
"""

from dataclasses import dataclass
from typing import Any, Dict, List

from ..drills.note_drill import NoteDegreeDrill
from ..drills.chord_id import ChordDegreeDrill
from ..drills.base_drill import DrillContext
from ..drills.chord_relative import TwoChordRelativeDrill
from ..audio.synthesis import Synth


@dataclass(frozen=True)
class DrillMeta:
    id: str
    name: str
    description: str
    parameters_schema: Dict[str, Any]
    presets: Dict[str, Dict[str, Any]]


def _note_drill_meta() -> DrillMeta:
    from .presets import NOTE_PRESETS

    return DrillMeta(
        id="note",
        name="Single Note Degree",
        description="Identify diatonic scale degrees (1–7) by ear.",
        parameters_schema={
            "type": "object",
            "properties": {
                "questions": {"type": "integer", "minimum": 1, "default": 10},
                "degrees_in_scope": {"type": "array", "items": {"type": "string"}},
                "include_chromatic": {"type": "boolean", "default": False},
                "note_range": {
                    "type": "object",
                    "properties": {
                        "min_note": {"type": "string"},
                        "max_note": {"type": "string"},
                    },
                },
            },
            "required": ["questions"],
        },
        presets=NOTE_PRESETS,
    )


def list_drills() -> List[DrillMeta]:
    return [_note_drill_meta(), _chord_drill_meta(), _chord_relative_meta()]


def get_drill(drill_id: str) -> DrillMeta:
    for m in list_drills():
        if m.id == drill_id:
            return m
    raise KeyError(f"Unknown drill id: {drill_id}")


def make_drill(
    drill_id: str,
    *,
    scale: Dict[str, str],
    synth: Synth,
    drone: Any | None,
    params: Dict[str, Any],
    mistake_manager: Any | None,
    results_sink: Any | None,
):
    """Factory that builds the concrete drill with validated params.

    For now supports only the existing NoteDegreeDrill.
    """
    ctx = DrillContext(
        key_root=scale["key"],
        scale_type=scale["scale_type"],
        reference_mode="cadence",
        degrees_in_scope=list(params.get("degrees_in_scope", ["1","2","3","4","5","6","7"])),
        include_chromatic=bool(params.get("include_chromatic", False)),
        min_midi=params.get("min_midi"),
        max_midi=params.get("max_midi"),
        test_note_delay_ms=int(params.get("test_note_delay_ms", 300)),
        allow_consecutive_degree_repeat=bool(params.get("allow_consecutive_repeat", False)),
    )

    if drill_id == "note":
        return NoteDegreeDrill(ctx, synth)
    if drill_id == "chord":
        return ChordDegreeDrill(ctx, synth)
    if drill_id == "chord_relative":
        dr = TwoChordRelativeDrill(ctx, synth)
        dr.configure(
            orientation=str(params.get("orientation", "both")),
            weight_from_root=float(params.get("weight_from_root", 1.0)),
            weight_to_root=float(params.get("weight_to_root", 1.0)),
            inter_chord_gap_ms=int(params.get("inter_chord_gap_ms", 250)),
        )
        return dr
    raise KeyError(f"Unsupported drill for factory: {drill_id}")

def _chord_drill_meta() -> DrillMeta:
    from .presets import CHORD_PRESETS
    return DrillMeta(
        id="chord",
        name="Chord Degree (triads)",
        description="Identify diatonic triads by degree (1–7).",
        parameters_schema={
            "type": "object",
            "properties": {
                "questions": {"type": "integer", "minimum": 1, "default": 10},
                "degrees_in_scope": {"type": "array", "items": {"type": "string"}},
                "allow_consecutive_repeat": {"type": "boolean", "default": False},
            },
            "required": ["questions"],
        },
        presets=CHORD_PRESETS,
    )


def _chord_relative_meta() -> DrillMeta:
    from .presets import CHORD_RELATIVE_PRESETS
    return DrillMeta(
        id="chord_relative",
        name="Chord Relative (1↔︎X)",
        description="Hear two chords where one is I and the other is a diatonic triad; identify the non-root degree.",
        parameters_schema={
            "type": "object",
            "properties": {
                "questions": {"type": "integer", "minimum": 1, "default": 10},
                "degrees_in_scope": {"type": "array", "items": {"type": "string"}},
                "allow_consecutive_repeat": {"type": "boolean", "default": False},
                "orientation": {"type": "string", "enum": ["from_root", "to_root", "both"], "default": "both"},
                "weight_from_root": {"type": "number", "default": 1.0},
                "weight_to_root": {"type": "number", "default": 1.0},
                "inter_chord_gap_ms": {"type": "integer", "default": 250},
            },
            "required": ["questions"],
        },
        presets=CHORD_RELATIVE_PRESETS,
    )
