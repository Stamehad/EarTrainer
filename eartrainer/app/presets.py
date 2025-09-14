from __future__ import annotations

"""Curated human-friendly parameter presets per drill.

Presets help users select sensible defaults quickly without many flags.
"""

NOTE_PRESETS = {
    "beginner": {
        "questions": 10,
        "degrees_in_scope": ["1", "2", "3", "4", "5"],
        "include_chromatic": False,
        "note_range": {"min_note": "C3", "max_note": "C5"},
        "test_note_delay_ms": 300,
    },
    "default": {
        "questions": 20,
        "degrees_in_scope": ["1", "2", "3", "4", "5", "6", "7"],
        "include_chromatic": False,
        "note_range": {"min_note": "C2", "max_note": "C5"},
        "test_note_delay_ms": 250,
    },
    "advanced": {
        "questions": 40,
        "degrees_in_scope": ["1", "2", "3", "4", "5", "6", "7"],
        "include_chromatic": True,
        "note_range": {"min_note": "C2", "max_note": "C6"},
        "test_note_delay_ms": 200,
    },
}

CHORD_PRESETS = {
    "beginner": {
        "questions": 10,
        "degrees_in_scope": ["1", "2", "3", "4", "5"],
        "allow_consecutive_repeat": False,
        "test_note_delay_ms": 300,
    },
    "default": {
        "questions": 20,
        "degrees_in_scope": ["1", "2", "3", "4", "5", "6", "7"],
        "allow_consecutive_repeat": False,
        "test_note_delay_ms": 250,
    },
    "advanced": {
        "questions": 30,
        "degrees_in_scope": ["1", "2", "3", "4", "5", "6", "7"],
        "allow_consecutive_repeat": True,
        "test_note_delay_ms": 200,
    },
}

CHORD_RELATIVE_PRESETS = {
    "beginner": {
        "questions": 10,
        "degrees_in_scope": ["2", "3", "4", "5"],
        "allow_consecutive_repeat": False,
        "orientation": "both",
        "weight_from_root": 1.0,
        "weight_to_root": 1.0,
        "inter_chord_gap_ms": 300,
    },
    "default": {
        "questions": 20,
        "degrees_in_scope": ["2", "3", "4", "5", "6", "7"],
        "allow_consecutive_repeat": False,
        "orientation": "both",
        "weight_from_root": 1.0,
        "weight_to_root": 1.0,
        "inter_chord_gap_ms": 250,
    },
    "advanced": {
        "questions": 30,
        "degrees_in_scope": ["2", "3", "4", "5", "6", "7"],
        "allow_consecutive_repeat": True,
        "orientation": "both",
        "weight_from_root": 1.0,
        "weight_to_root": 1.0,
        "inter_chord_gap_ms": 200,
    },
}
