from __future__ import annotations

"""Harmony helpers for cadence construction using the OO voicing system."""

from typing import List, Optional, Dict

from .scale import Scale
from .chord import Chord
from .voicing_bank import VoicingBank, RegisterPolicy as BankRegisterPolicy
from .voicing_selector import VoicingSelector, DrillPolicy


def _policy_from_dict(voicing_policy: Optional[Dict]) -> BankRegisterPolicy:
    if not voicing_policy:
        return BankRegisterPolicy()
    chord_pc = voicing_policy.get("chord_octave_by_pc") if isinstance(voicing_policy, dict) else None
    bass_oct = voicing_policy.get("bass_octave") if isinstance(voicing_policy, dict) else None
    bass_pc = voicing_policy.get("bass_octave_by_pc") if isinstance(voicing_policy, dict) else None
    return BankRegisterPolicy(
        chord_octave_by_pc=chord_pc,
        bass_octave_by_pc=bass_pc,
        bass_octave=bass_oct,
    )


def build_tonic_chord_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[int]:
    """Build tonic chord (I) using voicing bank and selector."""
    scale = Scale(root_name, scale_type)
    spec = scale.degree_to_chord_spec(1)
    chord = Chord.from_spec(spec, extensions=())
    selector = VoicingSelector(VoicingBank())
    policy = DrillPolicy(register_policy=_policy_from_dict(voicing_policy), allowed_extensions={"triad", "7"})
    voicing = selector.render(chord, policy)
    return voicing.midi_notes


def build_v7_chord_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[int]:
    """Build dominant seventh (V7) using voicing bank and selector."""
    scale = Scale(root_name, scale_type)
    spec = scale.degree_to_chord_spec(5)
    # Force functional dominant quality regardless of minor variant mapping
    chord = Chord(root_name=spec.root_name, quality="dom7", degree=5, extensions=("7",))
    selector = VoicingSelector(VoicingBank())
    policy = DrillPolicy(register_policy=_policy_from_dict(voicing_policy), allowed_extensions={"triad", "7"})
    voicing = selector.render(chord, policy)
    return voicing.midi_notes


def build_cadence_midi(root_name: str, scale_type: str, octave: int = 4, voicing_policy: Optional[Dict] = None) -> List[List[int]]:
    """Return I–V7–I cadence as a list of chord MIDI note lists."""
    i_chord = build_tonic_chord_midi(root_name, scale_type, octave, voicing_policy=voicing_policy)
    v7_chord = build_v7_chord_midi(root_name, scale_type, octave, voicing_policy=voicing_policy)
    return [i_chord, v7_chord, i_chord]
