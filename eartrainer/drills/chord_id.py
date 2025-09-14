from __future__ import annotations

"""Chord degree identification drill (triads only)."""

import random
from typing import Dict, List, Optional

from .base_drill import BaseDrill
from ..theory.scale import Scale
from ..theory.chord import Chord
from ..theory.voicing_selector import VoicingSelector, RegisterPolicy, DrillPolicy
from ..theory.voicing_bank import VoicingBank, Voicing


class ChordDegreeDrill(BaseDrill):
    def __init__(self, ctx, synth) -> None:
        super().__init__(ctx, synth)
        self._scale = Scale(ctx.key_root, ctx.scale_type)
        self._selector = VoicingSelector(VoicingBank())
        self._bag: List[str] = []
        self._last_degree: Optional[str] = None
        self._last_voicing_label: Optional[str] = None

    def _refill_bag(self) -> None:
        self._bag = list(self.ctx.degrees_in_scope)
        random.shuffle(self._bag)

    def next_question(self) -> Dict:
        if not self._bag:
            self._refill_bag()
        # Avoid immediate repetition unless explicitly allowed
        if not self.ctx.allow_consecutive_degree_repeat:
            if len(self._bag) == 1 and self._bag[0] == self._last_degree:
                self._refill_bag()
            if self._last_degree is not None and len(self._bag) > 1 and self._bag[0] == self._last_degree:
                for i in range(1, len(self._bag)):
                    if self._bag[i] != self._last_degree:
                        self._bag[0], self._bag[i] = self._bag[i], self._bag[0]
                        break

        degree_str = self._bag.pop(0)
        self._last_degree = degree_str
        degree = int(degree_str)

        # Build spec and chord (triad only). If mapping yields 'dom7',
        # treat it as a major triad for this drill.
        spec = self._scale.degree_to_chord_spec(degree)
        quality = "maj" if spec.base_quality == "dom7" else spec.base_quality
        chord = Chord(root_name=spec.root_name, quality=quality, degree=spec.degree, extensions=())

        # Render a voicing and play it. If the degree repeats and repeats are allowed,
        # attempt to select a different inversion/template than last time.
        policy = DrillPolicy(register_policy=RegisterPolicy())
        if self.ctx.allow_consecutive_degree_repeat and degree_str == (self._last_degree or ""):
            voicing = self._render_with_exclusion(chord, policy, avoid_label=self._last_voicing_label)
        else:
            voicing = self._selector.render(chord, policy)
        self.synth.play_chord(voicing.midi_notes, velocity=90, dur_ms=800)
        self._last_voicing_label = voicing.template_label

        return {"truth_degree": degree_str, "midi": voicing.midi_notes}

    def _render_with_exclusion(self, chord: Chord, policy: DrillPolicy, avoid_label: Optional[str]) -> Voicing:
        # Determine allowed extensions per policy (mirror VoicingSelector logic)
        exts = set(chord.extensions) if chord.extensions else set()
        if exts:
            exts = exts.intersection(policy.allowed_extensions)
        else:
            exts = {"triad"} if "triad" in policy.allowed_extensions else set()

        # Collect candidates and exclude the avoided label if possible
        bank = self._selector.bank
        cands = bank.candidates(quality=chord.quality, extensions=exts, bass_policy=policy.bass_policy, instrument="piano")
        if not cands:
            raise ValueError(f"No voicing templates for {chord.quality} with {exts}")
        if avoid_label:
            filtered = [t for t in cands if str(t.get("label", "")) != avoid_label]
            if filtered:
                cands = filtered
        tmpl = policy.rng.choice(cands)
        return bank.render_template(chord.root_name, tmpl, policy=policy.register_policy, clamp_range=policy.register_hint)
