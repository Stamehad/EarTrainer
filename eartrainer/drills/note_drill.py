from __future__ import annotations

"""Single note identification by scale degree drill."""

import random
from typing import Dict, List

from .base_drill import BaseDrill
from ..theory.keys import degree_to_semitone_offset, note_name_to_midi, transpose_key, PITCH_CLASS_NAMES


class NoteDegreeDrill(BaseDrill):
    """Drill that plays a single note for a random diatonic degree."""

    def next_question(self) -> Dict:
        degrees = self.ctx.degrees_in_scope
        degree_str = random.choice(degrees)
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
