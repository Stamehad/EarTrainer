from __future__ import annotations

"""Base drill abstractions and context/results models."""

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Protocol

from ..audio.synthesis import Synth
from ..theory.harmony import build_cadence_midi
from ..theory.scales import diatonic_degree_to_pc
from ..theory.keys import transpose_key, note_name_to_midi
from ..theory.pathways import get_pathway
from ..app.explain import trace as xtrace


@dataclass
class DrillContext:
    """Context information for a drill session."""

    key_root: str
    scale_type: str
    reference_mode: str
    degrees_in_scope: List[str]
    include_chromatic: bool = False
    min_midi: int | None = None
    max_midi: int | None = None
    test_note_delay_ms: int = 300
    voicing_bass_octave: int = 2
    voicing_chord_octave_by_pc: list[int] | None = None


@dataclass
class DrillResult:
    """Aggregated result statistics for a drill session."""

    total: int
    correct: int
    per_degree: Dict[str, Dict[str, int]] = field(default_factory=dict)


class BaseDrill:
    """Abstract base for drills."""

    def __init__(self, ctx: DrillContext, synth: Synth) -> None:
        self.ctx = ctx
        self.synth = synth

    def play_reference(self) -> None:
        if self.ctx.reference_mode == "cadence":
            # Pass voicing policy if available
            policy = None
            if self.ctx.voicing_chord_octave_by_pc is not None:
                policy = {
                    "bass_octave": self.ctx.voicing_bass_octave,
                    "chord_octave_by_pc": self.ctx.voicing_chord_octave_by_pc,
                }
            cadence = build_cadence_midi(self.ctx.key_root, self.ctx.scale_type, octave=4, voicing_policy=policy)
            for chord in cadence:
                self.synth.play_chord(chord, velocity=90, dur_ms=800)
                self.synth.sleep_ms(250)
        elif self.ctx.reference_mode == "scale":
            self._play_scale_sequence()
        # Stretch: handle scale/drone in future

    def _play_scale_sequence(self, octave: int = 4) -> None:
        """Play ascending then descending scale (1 octave) for current key/scale.

        Uses the scale pattern from theory.scales and plays notes sequentially.
        """
        root = transpose_key(self.ctx.key_root)
        tonic = note_name_to_midi(root, octave)
        # Build ascending degrees 1..7 then top octave 1
        asc = [tonic + diatonic_degree_to_pc(self.ctx.scale_type, d) for d in range(1, 8)]
        asc.append(tonic + 12)  # top tonic
        # Descend back to tonic (exclude top-most duplicate when reversing)
        desc = list(reversed(asc[:-1]))
        seq = asc + desc
        for m in seq:
            self.synth.note_on(m, velocity=90, dur_ms=300)

    def _play_pathway_sequence(self, degree_str: str, octave: int = 4) -> None:
        """Play pathway (sequence of degrees) for current scale from given degree.

        Supports optional octave offsets per token: e.g., "1@+1" or "1@1" or "5@-1".
        """
        seq_degrees = get_pathway(self.ctx.scale_type, degree_str)
        if not seq_degrees:
            seq_degrees = [degree_str]
        root = transpose_key(self.ctx.key_root)
        tonic = note_name_to_midi(root, octave)
        for token in seq_degrees:
            t = str(token)
            if "@" in t:
                deg_part, off_part = t.split("@", 1)
                off_part = off_part.strip()
                if off_part.startswith("+"):
                    off_part = off_part[1:]
                try:
                    oct_off = int(off_part)
                except Exception:
                    oct_off = 0
            else:
                deg_part = t
                oct_off = 0
            try:
                d = int(deg_part)
            except Exception:
                continue
            base = tonic + 12 * oct_off
            m = base + diatonic_degree_to_pc(self.ctx.scale_type, d)
            self.synth.note_on(m, velocity=95, dur_ms=500)

    def next_question(self) -> Dict:
        raise NotImplementedError

    def grade(self, answer: str, ground_truth: str) -> bool:
        a = answer.strip().lower()
        g = ground_truth.strip().lower()
        return a == g

    def run(self, num_questions: int, ui_callbacks: Dict[str, Callable]) -> DrillResult:
        ask = ui_callbacks["ask"]
        inform = ui_callbacks["inform"]
        confirm_replay_reference = ui_callbacks.get("confirm_replay_reference", lambda: False)
        activity = ui_callbacks.get("activity", lambda _e=None: None)
        maybe_restart_drone = ui_callbacks.get("maybe_restart_drone", lambda: None)
        duck = ui_callbacks.get("duck_drone", lambda *_a, **_k: None)
        unduck = ui_callbacks.get("unduck_drone", lambda *_a, **_k: None)

        correct = 0
        per_degree: Dict[str, Dict[str, int]] = {}

        for i in range(1, num_questions + 1):
            maybe_restart_drone()
            # Announce and play the question audio before prompting
            inform(f"Q{i}/{num_questions}: listen…")
            q = self.next_question()
            xtrace("question_created", {"index": i, "truth": str(q.get("truth_degree"))})
            activity("question_played")
            truth = str(q["truth_degree"])  # "1".."7"

            while True:
                ans = ask("Enter scale degree (1–7), 'c' cadence, 's' scale, 'r' replay note, 'p' pathway, 't' tonic: ")
                if ans.strip().lower() == "r":
                    if "midi" in q:
                        activity("replay_note")
                        midi_obj = q["midi"]
                        if isinstance(midi_obj, list):
                            self.synth.play_chord([int(m) for m in midi_obj], velocity=90, dur_ms=800)
                        else:
                            self.synth.note_on(int(midi_obj), velocity=100, dur_ms=600)
                        xtrace("replay_note", {"index": i})
                    continue
                if ans.strip().lower() == "s":
                    duck()
                    self._play_scale_sequence()
                    unduck()
                    self.synth.sleep_ms(self.ctx.test_note_delay_ms)
                    if "midi" in q:
                        midi_obj = q["midi"]
                        if isinstance(midi_obj, list):
                            self.synth.play_chord([int(m) for m in midi_obj], velocity=90, dur_ms=800)
                        else:
                            self.synth.note_on(int(midi_obj), velocity=100, dur_ms=600)
                    activity("replay_scale")
                    xtrace("replay_scale", {"index": i})
                    continue
                if ans.strip().lower() == "c":
                    duck()
                    self.play_reference()
                    unduck()
                    self.synth.sleep_ms(self.ctx.test_note_delay_ms)
                    if "midi" in q:
                        midi_obj = q["midi"]
                        if isinstance(midi_obj, list):
                            self.synth.play_chord([int(m) for m in midi_obj], velocity=90, dur_ms=800)
                        else:
                            self.synth.note_on(int(midi_obj), velocity=100, dur_ms=600)
                    activity("replay_cadence")
                    xtrace("replay_cadence", {"index": i})
                    continue
                if ans.strip().lower() == "p":
                    duck()
                    self._play_pathway_sequence(truth)
                    unduck()
                    self.synth.sleep_ms(self.ctx.test_note_delay_ms)
                    if "midi" in q:
                        midi_obj = q["midi"]
                        if isinstance(midi_obj, list):
                            self.synth.play_chord([int(m) for m in midi_obj], velocity=90, dur_ms=800)
                        else:
                            self.synth.note_on(int(midi_obj), velocity=100, dur_ms=600)
                    activity("replay_pathway")
                    xtrace("replay_pathway", {"index": i, "degree": truth})
                    continue
                if ans.strip().lower() == "t":
                    # Play tonic (C4 by default transposed to key) for ~2 seconds
                    duck()
                    root = transpose_key(self.ctx.key_root)
                    tonic = note_name_to_midi(root, 4)
                    self.synth.note_on(tonic, velocity=95, dur_ms=2000)
                    unduck()
                    self.synth.sleep_ms(self.ctx.test_note_delay_ms)
                    if "midi" in q:
                        midi_obj = q["midi"]
                        if isinstance(midi_obj, list):
                            self.synth.play_chord([int(m) for m in midi_obj], velocity=90, dur_ms=800)
                        else:
                            self.synth.note_on(int(midi_obj), velocity=100, dur_ms=600)
                    activity("play_tonic")
                    xtrace("play_tonic", {"index": i})
                    continue
                # grade if not replay request
                is_correct = self.grade(ans, truth)
                xtrace("graded", {"index": i, "answer": ans, "truth": truth, "correct": is_correct})
                if is_correct:
                    correct += 1
                    inform("Correct!\n")
                else:
                    inform(f"Incorrect. Answer was {truth}.\n")
                activity("answer")

                # stats update
                bucket = per_degree.setdefault(truth, {"asked": 0, "correct": 0})
                bucket["asked"] += 1
                bucket["correct"] += 1 if is_correct else 0
                break

            # Delay after answer before next question's test note
            self.synth.sleep_ms(self.ctx.test_note_delay_ms)

        return DrillResult(total=num_questions, correct=correct, per_degree=per_degree)
