from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Any, Set, Tuple, Optional
import yaml
from .note_utils import NAME_TO_PC, DEGREE_TO_SEMITONE, note_name_to_midi


@dataclass(frozen=True)
class Voicing:
    midi_notes: List[int]          # sorted low→high
    template_label: str
    meta: Dict[str, Any]


@dataclass
class RegisterPolicy:
    """Per-pitch-class register policy for bass and chord.

    - chord_octave_by_pc: octave indices for C..B (len=12)
    - bass_octave_by_pc: octave indices for C..B (len=12); if None, use bass_octave
    - bass_octave: single octave applied if bass_octave_by_pc is not provided
    """

    chord_octave_by_pc: Optional[List[int]] = None
    bass_octave_by_pc: Optional[List[int]] = None
    bass_octave: Optional[int] = None

    def chord_oct_for_pc(self, pc: int, fallback: int) -> int:
        if self.chord_octave_by_pc and len(self.chord_octave_by_pc) == 12:
            return int(self.chord_octave_by_pc[pc])
        return int(fallback)

    def bass_oct_for_pc(self, pc: int, fallback: int) -> int:
        if self.bass_octave_by_pc and len(self.bass_octave_by_pc) == 12:
            return int(self.bass_octave_by_pc[pc])
        if self.bass_octave is not None:
            return int(self.bass_octave)
        return int(fallback)


class VoicingBank:
    """Loads small, curated voicing templates from YAML and offers filtered candidates."""

    def __init__(self, path: Optional[str] = None):
        if path is None:
            path = str(Path(__file__).resolve().parents[1] / "resources" / "voicings" / "piano_basic.yml")
        with open(path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        self.defaults: Dict[str, Any] = data.get("defaults", {})
        self.templates: List[Dict[str, Any]] = list(data.get("templates") or [])

    def default_octaves(self) -> Tuple[int, int]:
        rh = int(self.defaults.get("rh_base_octave", 4))
        lh = int(self.defaults.get("lh_bass_octave", 2))
        return rh, lh

    def candidates(
        self,
        quality: str,
        extensions: Set[str],
        bass_policy: str = "root_only",
        instrument: str = "piano",
    ) -> List[Dict[str, Any]]:
        """Return template dicts matching tags (quality, extensions⊆, bass policy, instrument)."""
        cands: List[Dict[str, Any]] = []
        for t in self.templates:
            tags = t.get("tags", {})
            if tags.get("quality") != quality:
                continue
            if tags.get("instrument") != instrument:
                continue
            if tags.get("bass") != bass_policy:
                continue
            tmpl_exts = set(tags.get("extensions") or [])
            if not extensions:
                if "triad" in tmpl_exts:
                    cands.append(t)
            else:
                if extensions.issubset(tmpl_exts):
                    cands.append(t)
        return cands

    # --- Rendering utility (called by selector) ---
    def render_template(
        self,
        root_name: str,
        tmpl: Dict[str, Any],
        policy: Optional[RegisterPolicy] = None,
        clamp_range: Optional[Tuple[int, int]] = None,
    ) -> Voicing:
        """Convert a single template + root into concrete MIDI notes.

        The base octaves are chosen from the policy per pitch-class. If no policy
        is provided, fall back to defaults in the YAML.
        """
        pc = NAME_TO_PC[root_name]
        rh_fallback, lh_fallback = self.default_octaves()
        if policy is None:
            rh_base = rh_fallback
            lh_base = lh_fallback
        else:
            rh_base = policy.chord_oct_for_pc(pc, rh_fallback)
            lh_base = policy.bass_oct_for_pc(pc, lh_fallback)

        # Bass
        bass_degrees = tmpl.get("bass", ["1@-1"])  # default keeps compatibility if no policy given
        bass_notes: List[int] = []
        for deg in bass_degrees:
            degree, oct_shift = _parse_degree(deg)
            root_midi = note_name_to_midi(root_name, lh_base)
            bass_notes.append(root_midi + DEGREE_TO_SEMITONE[degree] + 12 * oct_shift)

        # Right hand
        rh_notes: List[int] = []
        for deg in tmpl.get("right_hand", []):
            degree, oct_shift = _parse_degree(deg)
            root_midi = note_name_to_midi(root_name, rh_base)
            rh_notes.append(root_midi + DEGREE_TO_SEMITONE[degree] + 12 * oct_shift)

        # Optional clamp of right hand into a target MIDI range
        if clamp_range is not None and rh_notes:
            low, high = int(clamp_range[0]), int(clamp_range[1])
            # Shift entire RH block by octaves until it fits in [low, high]
            while max(rh_notes) > high:
                rh_notes = [n - 12 for n in rh_notes]
            while min(rh_notes) < low:
                rh_notes = [n + 12 for n in rh_notes]

        notes = sorted(bass_notes + rh_notes)
        return Voicing(
            midi_notes=notes,
            template_label=str(tmpl.get("label", "")),
            meta={"rh_base_octave": rh_base, "lh_bass_octave": lh_base},
        )


def _parse_degree(token: str) -> Tuple[str, int]:
    """Parse tokens like 'b3', '5', '1@+1' -> (degree, octave_shift)."""
    if "@+" in token:
        d, shift = token.split("@+")
        return d, int(shift)
    if "@-" in token:
        d, shift = token.split("@-")
        return d, -int(shift)
    return token, 0
