from __future__ import annotations

"""Chord relative drill: play two chords where one is the tonic (1) and the other
is a target degree. User identifies the non-root degree.

Designed to extend later to longer chains (3–4 chords) by generalizing the
stimulus builder and grading to handle multi-part answers.
"""

import random
from typing import Dict, List, Optional, Tuple

from .base_drill import BaseDrill
from ..theory.scale import Scale
from ..theory.chord import Chord
from ..theory.voicing_selector import VoicingSelector, RegisterPolicy, DrillPolicy
from ..theory.voicing_bank import VoicingBank, Voicing


class TwoChordRelativeDrill(BaseDrill):
    def __init__(self, ctx, synth) -> None:
        super().__init__(ctx, synth)
        self._scale = Scale(ctx.key_root, ctx.scale_type)
        self._selector = VoicingSelector(VoicingBank())
        self._bag: List[str] = []
        self._last_target_degree: Optional[str] = None
        self._last_target_voicing_label: Optional[str] = None
        # Parameters
        self._orientation = "both"  # from_root | to_root | both
        self._weight_from = 1.0
        self._weight_to = 1.0
        self._gap_ms = 250
        self._repeat_each = 1  # times to play each chord in the pair

    # Hooks to set optional parameters via context.params in factory
    def configure(self, *, orientation: str = "both", weight_from_root: float = 1.0, weight_to_root: float = 1.0, inter_chord_gap_ms: int = 250, repeat_each: int = 1) -> None:
        t = (orientation or "both").strip().lower()
        if t not in ("from_root", "to_root", "both"):
            t = "both"
        self._orientation = t
        try:
            self._weight_from = float(weight_from_root)
        except Exception:
            self._weight_from = 1.0
        try:
            self._weight_to = float(weight_to_root)
        except Exception:
            self._weight_to = 1.0
        try:
            self._gap_ms = int(inter_chord_gap_ms)
        except Exception:
            self._gap_ms = 250
        try:
            self._repeat_each = max(1, int(repeat_each))
        except Exception:
            self._repeat_each = 1

    def _refill_bag(self) -> None:
        # Exclude 1 from target set
        candidates = [d for d in self.ctx.degrees_in_scope if str(d) != "1"]
        if not candidates:
            candidates = ["2", "3", "4", "5", "6", "7"]
        self._bag = list(candidates)
        random.shuffle(self._bag)

    def _choose_orientation(self) -> str:
        if self._orientation in ("from_root", "to_root"):
            return self._orientation
        # weighted random between from_root and to_root
        total = max(0.0, self._weight_from) + max(0.0, self._weight_to)
        if total <= 0:
            return random.choice(["from_root", "to_root"])  # fallback
        r = random.random() * total
        return "from_root" if r < max(0.0, self._weight_from) else "to_root"

    def _render_with_exclusion(self, chord: Chord, policy: DrillPolicy, avoid_label: Optional[str]) -> Voicing:
        exts = set(chord.extensions) if chord.extensions else set()
        if exts:
            exts = exts.intersection(policy.allowed_extensions)
        else:
            exts = {"triad"} if "triad" in policy.allowed_extensions else set()
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

    def next_question(self) -> Dict:
        if not self._bag:
            self._refill_bag()
        # Avoid immediate repeat of target degree unless allowed
        if not self.ctx.allow_consecutive_degree_repeat:
            if len(self._bag) == 1 and self._bag[0] == self._last_target_degree:
                self._refill_bag()
            if self._last_target_degree is not None and len(self._bag) > 1 and self._bag[0] == self._last_target_degree:
                for i in range(1, len(self._bag)):
                    if self._bag[i] != self._last_target_degree:
                        self._bag[0], self._bag[i] = self._bag[i], self._bag[0]
                        break

        target_str = self._bag.pop(0)
        self._last_target_degree = target_str
        target = int(target_str)
        orient = self._choose_orientation()

        # Build triad specs; coerce dom7 to maj for this drill
        root_spec = self._scale.degree_to_chord_spec(1)
        root_quality = "maj" if root_spec.base_quality == "dom7" else root_spec.base_quality
        target_spec = self._scale.degree_to_chord_spec(target)
        target_quality = "maj" if target_spec.base_quality == "dom7" else target_spec.base_quality
        root_chord = Chord(root_name=root_spec.root_name, quality=root_quality, degree=root_spec.degree, extensions=())
        target_chord = Chord(root_name=target_spec.root_name, quality=target_quality, degree=target_spec.degree, extensions=())

        policy = DrillPolicy(register_policy=RegisterPolicy())
        root_voicing = self._selector.render(root_chord, policy)
        if target_str == (self._last_target_degree or ""):
            target_voicing = self._render_with_exclusion(target_chord, policy, avoid_label=self._last_target_voicing_label)
        else:
            target_voicing = self._selector.render(target_chord, policy)

        seq: List[Tuple[str, Voicing]]
        if orient == "from_root":
            seq = [("root", root_voicing), ("target", target_voicing)]
        else:
            seq = [("target", target_voicing), ("root", root_voicing)]

        # Build an onset list with equal spacing between all onsets
        onset_voicings: List[Voicing] = []
        if orient == "from_root":
            onset_voicings.extend([root_voicing] * self._repeat_each)
            onset_voicings.extend([target_voicing] * self._repeat_each)
        else:
            onset_voicings.extend([target_voicing] * self._repeat_each)
            onset_voicings.extend([root_voicing] * self._repeat_each)

        for i, v in enumerate(onset_voicings):
            self.synth.play_chord(v.midi_notes, velocity=90, dur_ms=800)
            if i < len(onset_voicings) - 1:
                self.synth.sleep_ms(self._gap_ms)

        # Prepare replay function
        def _replay() -> None:
            for i, v in enumerate(onset_voicings):
                self.synth.play_chord(v.midi_notes, velocity=90, dur_ms=800)
                if i < len(onset_voicings) - 1:
                    self.synth.sleep_ms(self._gap_ms)

        self._last_target_voicing_label = target_voicing.template_label

        return {"truth_degree": target_str, "replay": _replay}
