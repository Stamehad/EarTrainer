# eartrainer/drills/chord_id.py
from __future__ import annotations
from dataclasses import dataclass
from typing import Protocol, Dict, Any
from ..theory.scale import Scale
from ..theory.chord import Chord
from ..theory.voicing_bank import Voicing
from ..theory.voicing_selector import VoicingSelector, DrillPolicy

class UICallbacks(Protocol):
    def ask(self, prompt: str) -> str: ...
    def inform(self, msg: str) -> None: ...

@dataclass
class DrillResult:
    total: int
    correct: int

class ChordIDDrill:
    """Chord identification in functional context (tonal key)."""
    def __init__(self, scale: Scale, selector: VoicingSelector, policy: DrillPolicy):
        self.scale = scale
        self.selector = selector
        self.policy = policy

    def play_reference(self) -> None:
        """Play I–V–I cadence using the selector (hook into your Synth externally)."""
        # TODO: produce three Chords (I, V7, I), get Voicings; playback handled by caller
        pass

    def _sample_chord(self) -> Chord:
        """Pick a degree, build spec, sample extensions (MVP: triad only), return Chord."""
        deg = self.scale.random_degree(weights=None)
        spec = self.scale.degree_to_chord_spec(deg)
        # TODO: sample extensions consistent with self.policy (MVP: ())
        return Chord.from_spec(spec, extensions=())

    def run(self, n: int, ui: UICallbacks) -> DrillResult:
        """Orchestrate n questions: render chord → play (handled by caller) → collect answer → grade."""
        correct = 0
        for i in range(n):
            chord = self._sample_chord()
            voicing: Voicing = self.selector.render(chord, self.policy)
            # NOTE: playback occurs outside (synth.play_chord(voicing.midi_notes))
            ui.inform(f"[DEBUG] Voicing: {voicing.template_label}  Notes: {voicing.midi_notes}")
            guess = ui.ask("Enter chord quality (maj/min/dim/dom7): ").strip().lower()
            truth = chord.quality.lower()
            if guess == truth:
                correct += 1
                ui.inform("✓ correct")
            else:
                ui.inform(f"✗ wrong (was {truth})")
        return DrillResult(total=n, correct=correct)