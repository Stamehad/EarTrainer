from __future__ import annotations

"""Harmonic interval drill: plays two simultaneous notes (degrees).
User enters two degrees in order: lower then upper."""

import random
from typing import Dict, List, Tuple

from .base_drill import BaseDrill, DrillResult
from ..theory.keys import degree_to_semitone_offset, transpose_key, PITCH_CLASS_NAMES, note_name_to_midi
from ..theory.scales import SCALE_PATTERNS


class HarmonicIntervalDrill(BaseDrill):
    def __init__(self, ctx, synth) -> None:
        super().__init__(ctx, synth)
        self._max_interval_semitones = 12
        self._dur_ms = 1000
        self._sustain = False
        self._last_pair: Tuple[int, int] | None = None

    def configure(self, *, max_interval_semitones: int = 12, dur_ms: int = 1000, sustain: bool = False) -> None:
        self._max_interval_semitones = max(1, int(max_interval_semitones))
        self._dur_ms = max(100, int(dur_ms))
        self._sustain = bool(sustain)

    def _degree_to_candidates(self, degree: int) -> List[int]:
        offset = degree_to_semitone_offset(self.ctx.scale_type, degree)
        if self.ctx.min_midi is not None and self.ctx.max_midi is not None:
            root_pc = PITCH_CLASS_NAMES.index(transpose_key(self.ctx.key_root))
            target_pc = (root_pc + offset) % 12
            return [m for m in range(int(self.ctx.min_midi), int(self.ctx.max_midi) + 1) if (m % 12) == target_pc]
        tonic_midi = note_name_to_midi(transpose_key(self.ctx.key_root), 4)
        return [tonic_midi + offset]

    def _pick_pair(self) -> Tuple[str, int, str, int]:
        """Pick a lower degree d1 (1..7), an interval size k (1..7),
        choose m1 in-range, then set m2 = m1 + diatonic delta.
        Avoid exact repeats when possible."""
        def diatonic_delta(start_deg: int, steps: int) -> int:
            pats = SCALE_PATTERNS.get(self.ctx.scale_type) or SCALE_PATTERNS["major"]
            idx = (start_deg - 1) % 7
            total = 0
            for i in range(int(steps)):
                total += pats[(idx + i) % 7]
            return total

        last_pair = self._last_pair
        for _ in range(16):
            d1 = random.randint(1, 7)
            k = random.randint(1, 7)
            # candidates for d1 within range (or octave 4 default)
            c1 = self._degree_to_candidates(d1)
            m1 = random.choice(c1) if c1 else self._degree_to_candidates(d1)[0]
            delta = diatonic_delta(d1, k)
            m2 = m1 + int(delta)
            # Avoid exact repeat
            pair = (int(m1), int(m2))
            if last_pair is None or (pair != last_pair and pair[::-1] != last_pair):
                d2_wrapped = ((d1 + k - 1) % 7) + 1
                self._last_pair = pair
                self._last_deg_pair = tuple(sorted((d1, d2_wrapped)))
                return str(d1), int(m1), str(d2_wrapped), int(m2)
        # Fallback: return whatever we got last
        d1 = random.randint(1, 7)
        c1 = self._degree_to_candidates(d1)
        m1 = random.choice(c1) if c1 else self._degree_to_candidates(d1)[0]
        d2 = ((d1) % 7) + 1
        m2 = m1 + diatonic_delta(d1, 1)
        self._last_pair = (int(m1), int(m2))
        self._last_deg_pair = tuple(sorted((d1, d2)))
        return str(d1), int(m1), str(d2), int(m2)

    def next_question(self) -> Dict:
        d_low, m_low, d_high, m_high = self._pick_pair()
        if self._sustain:
            try:
                # Sustain pedal on (CC 64)
                self.synth.cc(0, 64, 100)
            except Exception:
                pass
        self.synth.play_chord([m_low, m_high], velocity=95, dur_ms=self._dur_ms)
        if self._sustain:
            try:
                self.synth.cc(0, 64, 0)
            except Exception:
                pass
        return {"truth_degrees": [d_low, d_high], "midi": [m_low, m_high]}

    def play_reference(self) -> None:
        """Override to play uniform-tempo orientation: 1..8 then 5-3-1."""
        root = transpose_key(self.ctx.key_root)
        tonic = note_name_to_midi(root, 4)
        note_dur = 360  # slightly faster
        gap_ms = 60
        seq_up = list(range(1, 8))
        for d in seq_up:
            off = degree_to_semitone_offset(self.ctx.scale_type, d)
            self.synth.note_on(tonic + off, velocity=95, dur_ms=note_dur)
            self.synth.sleep_ms(gap_ms)
        # Top tonic (8)
        self.synth.note_on(tonic + 12, velocity=95, dur_ms=note_dur)
        self.synth.sleep_ms(gap_ms)
        # Descend arpeggio 5-3-1
        for d in (5, 3, 1):
            off = degree_to_semitone_offset(self.ctx.scale_type, d)
            self.synth.note_on(tonic + off, velocity=95, dur_ms=note_dur)
            self.synth.sleep_ms(gap_ms)

    def run(self, num_questions: int, ui_callbacks) -> Dict:
        ask = ui_callbacks["ask"]
        inform = ui_callbacks["inform"]
        activity = ui_callbacks.get("activity", lambda _e=None: None)
        maybe_restart_drone = ui_callbacks.get("maybe_restart_drone", lambda: None)
        duck = ui_callbacks.get("duck_drone", lambda *_a, **_k: None)
        unduck = ui_callbacks.get("unduck_drone", lambda *_a, **_k: None)

        correct_total = 0
        per_degree: Dict[str, Dict[str, int]] = {}
        asked_total = 0
        stopped = False
        for i in range(1, num_questions + 1):
            maybe_restart_drone()
            inform(f"Q{i}/{num_questions}: listen…")
            q = self.next_question()
            truth = [str(x) for x in (q.get("truth_degrees") or [])]
            activity("question_played")
            # Lower then upper
            part_results: List[bool] = []
            answers: List[str] = []
            low_msg_first = None
            low_ans = None
            low_truth = truth[0] if truth else "?"
            for idx, t in enumerate(truth, start=1):
                # Assistance loop for each part; only grade on 1..7
                while True:
                    ans = ask(f"Enter scale degree {idx}/2 (lower then upper), 'c' cadence, 's' scale, 'r' replay, 'p' pathway, 't' tonic, 'q' stop: ")
                    a = ans.strip().lower()
                    if a in {"q", "quit", "stop"}:
                        stopped = True
                        break
                    if a == "r":
                        # Replay the interval
                        midi = q.get("midi") or []
                        if isinstance(midi, list) and len(midi) == 2:
                            self.synth.play_chord([int(midi[0]), int(midi[1])], velocity=95, dur_ms=self._dur_ms)
                        continue
                    if a == "s":
                        duck(); self._play_scale_sequence(); unduck()
                        midi = q.get("midi") or []
                        if isinstance(midi, list) and len(midi) == 2:
                            self.synth.play_chord([int(midi[0]), int(midi[1])], velocity=95, dur_ms=self._dur_ms)
                        continue
                    if a == "c":
                        duck(); self.play_reference(); unduck(); continue
                    if a == "p":
                        duck(); self._play_pathway_sequence(t); unduck(); continue
                    if a == "t":
                        duck();
                        root = transpose_key(self.ctx.key_root)
                        tonic = note_name_to_midi(root, 4)
                        self.synth.note_on(tonic, velocity=95, dur_ms=2000)
                        unduck(); continue
                    # Otherwise grade
                    break
                if stopped:
                    break
                is_correct = (a == t)
                part_results.append(is_correct)
                answers.append(a)
                asked_total += 1
                if is_correct:
                    correct_total += 1
                # Print combined-style line progressively
                if idx == 1:
                    low_msg_first = ("✅ (" + t + ")") if is_correct else f"❌ (ans {a or '?'} → {t})"
                    low_ans = a
                    inform(f"Interval: low {low_msg_first}\n")
                else:
                    up_msg = ("✅ (" + t + ")") if is_correct else f"❌ (ans {a or '?'} → {t})"
                    inform(f"Interval: high {up_msg}\n")
                activity("answer")
                bucket = per_degree.setdefault(t, {"asked": 0, "correct": 0})
                bucket["asked"] += 1
                bucket["correct"] += 1 if is_correct else 0

            # Combined one-line feedback per interval
            if stopped:
                # If low was already printed, don't repeat; if nothing printed (stopped immediately), emit a brief notice
                if not part_results:
                    inform(f"Interval: — (stopped)\n")
                break
            self.synth.sleep_ms(self.ctx.test_note_delay_ms)

        return DrillResult(total=asked_total, correct=correct_total, per_degree=per_degree)
