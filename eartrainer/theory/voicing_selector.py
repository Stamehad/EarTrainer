# eartrainer/theory/voicing_selector.py
from __future__ import annotations
from dataclasses import dataclass, field
from typing import Tuple, Set, Optional
import random
from .chord import Chord
from .voicing_bank import VoicingBank, Voicing, RegisterPolicy

@dataclass
class DrillPolicy:
    allowed_extensions: Set[str] = field(default_factory=lambda: {"triad"})
    bass_policy: str = "root_only"
    register_hint: Tuple[int,int] = (60, 76)  # C4..E5, not used in MVP rendering
    balance_strategy: str = "uniform"
    rng: random.Random = field(default_factory=random.Random)
    register_policy: Optional[RegisterPolicy] = None

class VoicingSelector:
    """Picks a voicing template for a chord (per drill policy) and renders it."""
    def __init__(self, bank: VoicingBank):
        self.bank = bank
        # simple exposure tracker for balancing
        self._exposure = {}

    def render(self, chord: Chord, policy: DrillPolicy) -> Voicing:
        exts = set(chord.extensions) if chord.extensions else set()
        # Intersect with allowed by policy
        if exts:
            exts = exts.intersection(policy.allowed_extensions)
        else:
            # triad if nothing specified
            exts = {"triad"} if "triad" in policy.allowed_extensions else set()

        cands = self.bank.candidates(quality=chord.quality,
                                     extensions=exts,
                                     bass_policy=policy.bass_policy,
                                     instrument="piano")
        if not cands:
            raise ValueError(f"No voicing templates for {chord.quality} with {exts}")

        # Uniform sample (MVP). Later: weight by under-exposure.
        tmpl = policy.rng.choice(cands)
        label = tmpl.get("label","")
        self._exposure[label] = self._exposure.get(label, 0) + 1
        return self.bank.render_template(
            chord.root_name,
            tmpl,
            policy=policy.register_policy,
            clamp_range=policy.register_hint,
        )
