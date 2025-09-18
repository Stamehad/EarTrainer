from __future__ import annotations

"""Melodic dictation drill: plays a sequence of notes (degrees) and asks
for multiple answers per question (one per note, in order)."""

import random
import time
from contextlib import contextmanager
from typing import Dict, List, Tuple

from .base_drill import BaseDrill, DrillResult
from ..theory.keys import degree_to_semitone_offset, transpose_key, PITCH_CLASS_NAMES, note_name_to_midi
from ..samplers.melody_sampler import MusicalMelodySampler, SamplerConfig


class MelodicDictationDrill(BaseDrill):
    def __init__(self, ctx, synth, sampler=None, use_external_sampler: bool = False) -> None:
        super().__init__(ctx, synth)
        self._seq_len = 3
        self._inter_note_gap_ms = 250
        self._max_interval_semitones = 12
        # Automatic mode: intervals (2 notes) => uniform; melodies (>2) => external sampler
        self._use_external_sampler = (self._seq_len > 2)
        if sampler is not None:
            self._sampler = sampler
        else:
            # Default external sampler (can be disabled via use_external_sampler=False)
            cfg = SamplerConfig(
                seq_len=self._seq_len,
                max_interval_semitones=self._max_interval_semitones,
            )
            self._sampler = MusicalMelodySampler(ctx, cfg)

    def configure(self, *, length: int = 3, inter_note_gap_ms: int = 250, max_interval_semitones: int = 12) -> None:
        self._seq_len = max(2, int(length))
        self._inter_note_gap_ms = max(0, int(inter_note_gap_ms))
        self._max_interval_semitones = max(1, int(max_interval_semitones))
        # Auto-select generator based on length
        self._use_external_sampler = (self._seq_len > 2)
        # Keep external sampler (if used) in sync with new settings
        if hasattr(self, "_sampler") and self._sampler is not None:
            try:
                # Recreate with updated config to avoid coupling to internal fields
                cfg = SamplerConfig(
                    seq_len=self._seq_len,
                    max_interval_semitones=self._max_interval_semitones,
                )
                self._sampler = MusicalMelodySampler(self.ctx, cfg)
            except Exception:
                pass

    def _degree_to_candidates(self, degree: int) -> List[int]:
        """Return MIDI candidates for a degree within ctx min/max; fallback to octave 4."""
        offset = degree_to_semitone_offset(self.ctx.scale_type, degree)
        if self.ctx.min_midi is not None and self.ctx.max_midi is not None:
            root_pc = PITCH_CLASS_NAMES.index(transpose_key(self.ctx.key_root))
            target_pc = (root_pc + offset) % 12
            return [m for m in range(int(self.ctx.min_midi), int(self.ctx.max_midi) + 1) if (m % 12) == target_pc]
        tonic_midi = note_name_to_midi(transpose_key(self.ctx.key_root), 4)
        return [tonic_midi + offset]

    def _pick_uniform_next_degree_midi(self, prev_deg: int | None, prev_midi: int | None) -> Tuple[str, int]:
        """Uniform interval sampler step: pick a directed diatonic step k ∈ [-7,7] uniformly
        (including 0), wrap degrees to 1..7, then choose a MIDI candidate in range while
        respecting max semitone interval. If no candidate fits after retries, fall back to
        the closest-by-semitone candidate for that degree.
        """
        MAX_TRIES = 32
        import math
        rng = random

        # If no previous, choose a starting degree uniformly and sample from all candidates in range.
        if prev_deg is None or prev_midi is None:
            start_deg = int(rng.choice(self.ctx.degrees_in_scope))
            cands = self._degree_to_candidates(start_deg)
            midi = rng.choice(cands) if cands else self._degree_to_candidates(start_deg)[0]
            return str(start_deg), midi

        # Otherwise, sample a uniform diatonic step and realize it.
        last_attempt: tuple[int, List[int]] | None = None
        for _ in range(MAX_TRIES):
            k = rng.randint(-7, 7)  # uniform over [-7,7]
            next_deg = ((int(prev_deg) - 1 + k) % 7) + 1
            cands = self._degree_to_candidates(next_deg)
            if not cands:
                continue

            bounded = [m for m in cands if abs(m - prev_midi) <= self._max_interval_semitones]
            if not bounded:
                last_attempt = (next_deg, cands)
                continue

            direction = 0 if k == 0 else (1 if k > 0 else -1)
            if direction > 0:
                directional = [m for m in bounded if m >= prev_midi]
            elif direction < 0:
                directional = [m for m in bounded if m <= prev_midi]
            else:
                directional = bounded
            candidates = directional or bounded

            last_attempt = (next_deg, candidates)
            return str(next_deg), rng.choice(candidates)

        # Fallback: choose the closest candidate from the best attempt we saw
        if last_attempt:
            next_deg, cands = last_attempt
            closest = min(cands, key=lambda m: abs(m - prev_midi))
            return str(next_deg), closest

        # Last resort (should be unreachable): repeat previous note
        return str(prev_deg), prev_midi

    def _uniform_sequence(self) -> Dict:
        """Build a melodic sequence using a *uniform* diatonic-interval sampler.
        This keeps interval choices uniform (subject to range and max-semitone constraints)
        and is intended for interval/short-sequence drills.
        """
        truth_degrees: List[str] = []
        midis: List[int] = []
        prev_deg: int | None = None
        prev_midi: int | None = None
        for _ in range(self._seq_len):
            d, m = self._pick_uniform_next_degree_midi(prev_deg, prev_midi)
            truth_degrees.append(d)
            midis.append(m)
            prev_deg = int(d)
            prev_midi = m
        return {"truth_degrees": truth_degrees, "midi": midis}

    def next_question(self) -> Dict:
        # Decide how to generate the sequence: external sampler or internal uniform sampler
        if getattr(self, "_use_external_sampler", False) and getattr(self, "_sampler", None) is not None:
            q = self._sampler.sample(seq_len=self._seq_len)
        else:
            q = self._uniform_sequence()

        truth_degrees: List[str] = list(q.get("truth_degrees") or [])
        midis: List[int] = list(q.get("midi") or [])

        # Play the sequence
        for i, m in enumerate(midis):
            self.synth.note_on(int(m), velocity=100, dur_ms=450)
            if i < len(midis) - 1:
                self.synth.sleep_ms(self._inter_note_gap_ms)

        return {"truth_degrees": truth_degrees, "midi": midis}

    def run(self, num_questions: int, ui_callbacks) -> DrillResult:
        """Custom run loop to handle multiple answers per question."""
        ask = ui_callbacks["ask"]
        inform = ui_callbacks["inform"]
        activity = ui_callbacks.get("activity", lambda _e=None: None)
        maybe_restart_drone = ui_callbacks.get("maybe_restart_drone", lambda: None)
        duck = ui_callbacks.get("duck_drone", lambda *_a, **_k: None)
        unduck = ui_callbacks.get("unduck_drone", lambda *_a, **_k: None)

        correct_total = 0
        per_degree: Dict[str, Dict[str, int]] = {}

        for i in range(1, num_questions + 1):
            maybe_restart_drone()
            inform(f"Q{i}/{num_questions}: listen…")
            q = self.next_question()
            truth_list: List[str] = list(q.get("truth_degrees") or [])
            activity("question_played")

            paused_ms = 0.0

            @contextmanager
            def pause_timer():
                nonlocal paused_ms
                _t0 = time.monotonic()
                try:
                    yield
                finally:
                    paused_ms += (time.monotonic() - _t0) * 1000.0

            # Start timing after sequence ends
            t_part = time.monotonic()
            for part_idx, truth in enumerate(truth_list, start=1):
                while True:
                    ans = ask(f"Enter degree {part_idx}/{len(truth_list)} (1–7), or r/s/c/p/t for help: ")
                    a = ans.strip().lower()
                    if a == "r":
                        with pause_timer():
                            midi_obj = q.get("midi")
                            if isinstance(midi_obj, list):
                                for j, m in enumerate(midi_obj):
                                    self.synth.note_on(int(m), velocity=100, dur_ms=450)
                                    if j < len(midi_obj) - 1:
                                        self.synth.sleep_ms(self._inter_note_gap_ms)
                            else:
                                # nothing to replay
                                pass
                        continue
                    if a == "s":
                        with pause_timer():
                            duck(); self._play_scale_sequence(); unduck()
                            self.synth.sleep_ms(self.ctx.test_note_delay_ms)
                            midi_obj = q.get("midi")
                            if isinstance(midi_obj, list):
                                for j, m in enumerate(midi_obj):
                                    self.synth.note_on(int(m), velocity=100, dur_ms=450)
                                    if j < len(midi_obj) - 1:
                                        self.synth.sleep_ms(self._inter_note_gap_ms)
                        continue
                    if a == "c":
                        with pause_timer():
                            duck(); self.play_reference(); unduck()
                        continue
                    if a == "p":
                        with pause_timer():
                            duck(); self._play_pathway_sequence(truth); unduck()
                        continue
                    if a == "t":
                        with pause_timer():
                            duck();
                            root = transpose_key(self.ctx.key_root)
                            tonic = note_name_to_midi(root, 4)
                            self.synth.note_on(tonic, velocity=95, dur_ms=2000)
                            unduck()
                        continue
                    # Grade
                    is_correct = (a == str(truth).strip().lower())
                    if is_correct:
                        correct_total += 1
                        inform("Correct!\n")
                    else:
                        inform(f"Incorrect. Answer was {truth}.\n")
                    activity("answer")

                    # Stats per-degree
                    bucket = per_degree.setdefault(str(truth), {"asked": 0, "correct": 0})
                    bucket["asked"] += 1
                    bucket["correct"] += 1 if is_correct else 0
                    # Record net part time
                    gross_ms = (time.monotonic() - t_part) * 1000.0
                    t_elapsed_ms = max(0, int(round(gross_ms - paused_ms)))
                    meta = bucket.setdefault("meta", {})
                    meta["time_ms_total"] = int(meta.get("time_ms_total", 0)) + t_elapsed_ms
                    # Reset timers for next part
                    paused_ms = 0.0
                    t_part = time.monotonic()
                    break

            # small delay before next question
            self.synth.sleep_ms(self.ctx.test_note_delay_ms)

        return DrillResult(total=num_questions * self._seq_len, correct=correct_total, per_degree=per_degree)
