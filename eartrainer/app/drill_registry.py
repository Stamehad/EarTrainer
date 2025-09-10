from __future__ import annotations

"""Drill registry and metadata.

Discover drills, expose metadata, validate parameters, and construct
drill instances via a simple factory.
"""

from dataclasses import dataclass
from typing import Any, Dict, List

from ..drills.note_drill import NoteDegreeDrill
from ..drills.base_drill import DrillContext
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
    return [_note_drill_meta()]


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
    if drill_id != "note":
        raise KeyError(f"Unsupported drill for factory: {drill_id}")

    ctx = DrillContext(
        key_root=scale["key"],
        scale_type=scale["scale_type"],
        reference_mode="cadence",
        degrees_in_scope=list(params.get("degrees_in_scope", ["1","2","3","4","5","6","7"])),
        include_chromatic=bool(params.get("include_chromatic", False)),
        min_midi=params.get("min_midi"),
        max_midi=params.get("max_midi"),
        test_note_delay_ms=int(params.get("test_note_delay_ms", 300)),
    )
    return NoteDegreeDrill(ctx, synth)
