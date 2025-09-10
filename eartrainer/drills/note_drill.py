from __future__ import annotations

"""Single note identification by scale degree drill."""

import random
from typing import Dict, List, Optional

from .base_drill import BaseDrill
from ..theory.keys import degree_to_semitone_offset, note_name_to_midi, transpose_key, PITCH_CLASS_NAMES


class NoteDegreeDrill(BaseDrill):
    """Drill that plays a single note for a random diatonic degree."""

    def __init__(self, ctx, synth):
        super().__init__(ctx, synth)
        self._bag: List[str] = []
        self._last_degree: Optional[str] = None

    def _refill_bag(self) -> None:
        self._bag = list(self.ctx.degrees_in_scope)
        random.shuffle(self._bag)

    def next_question(self) -> Dict:
        if not self._bag:
            self._refill_bag()
        # Pop ensuring no immediate repeat if alternatives exist
        if len(self._bag) == 1 and self._bag[0] == self._last_degree:
            # Force refill to avoid repetition
            self._refill_bag()
        # Select next degree avoiding immediate repeat when possible
        if self._last_degree is not None and len(self._bag) > 1 and self._bag[0] == self._last_degree:
            # Swap with a different element
            for i in range(1, len(self._bag)):
                if self._bag[i] != self._last_degree:
                    self._bag[0], self._bag[i] = self._bag[i], self._bag[0]
                    break
        degree_str = self._bag.pop(0)
        self._last_degree = degree_str
        degree = int(degree_str)

        offset = degree_to_semitone_offset(self.ctx.scale_type, degree)

        # If a note range is provided (min_midi/max_midi), pick a candidate within that range
        midi: int
        if self.ctx.min_midi is not None and self.ctx.max_midi is not None:
            root_pc = PITCH_CLASS_NAMES.index(transpose_key(self.ctx.key_root))
            target_pc = (root_pc + offset) % 12
            candidates: List[int] = [m for m in range(self.ctx.min_midi, self.ctx.max_midi + 1) if (m % 12) == target_pc]
            if candidates:
                midi = random.choice(candidates)
            else:
                # Fallback if misconfigured range
                tonic_midi = note_name_to_midi(transpose_key(self.ctx.key_root), 4)
                midi = tonic_midi + offset
        else:
            # compute MIDI around octave 4
            tonic_midi = note_name_to_midi(transpose_key(self.ctx.key_root), 4)
            midi = tonic_midi + offset

        # Play the note
        self.synth.note_on(midi, velocity=100, dur_ms=600)

        return {"truth_degree": degree_str, "midi": midi}
