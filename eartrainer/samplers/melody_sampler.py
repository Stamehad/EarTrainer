from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Protocol, Sequence, Tuple
import random
from collections import deque

# Reuse your existing theory helpers
from ..theory.keys import (
    degree_to_semitone_offset,
    transpose_key,
    PITCH_CLASS_NAMES,
    note_name_to_midi,
)


# ---- Context adapter so we don't couple to the drill/synth ----
class ContextAdapter(Protocol):
    @property
    def scale_type(self) -> str: ...
    @property
    def key_root(self) -> str: ...
    @property
    def degrees_in_scope(self) -> Sequence[int]: ...
    @property
    def min_midi(self) -> Optional[int]: ...
    @property
    def max_midi(self) -> Optional[int]: ...


# ---- Config for the sampler ----
@dataclass
class SamplerConfig:
    seq_len: int = 3
    max_interval_semitones: int = 12

    # “musicality” base weights for diatonic steps k ∈ {-7..7}
    # Larger values = more likely (we’ll normalize).
    base_interval_weights: Dict[int, float] = field(
        default_factory=lambda: {
            -7: 0.05, -6: 0.06, -5: 0.12, -4: 0.18, -3: 0.22, -2: 0.45, -1: 1.00,
             0: 0.10,  # allow unisons but keep rarer
             1: 1.00,  2: 0.45,  3: 0.22,  4: 0.18,  5: 0.12,  6: 0.06,  7: 0.05
        }
    )

    # Modifiers (see methods below)
    leap_recovery_boost: float = 1.6           # encourage step in opposite direction after a leap
    same_dir_after_big_leap_penalty: float = 0.35
    same_dir_run_cap_after: int = 4            # after N same-direction moves, penalize continuing
    same_dir_run_penalty: float = 0.3
    unison_run_decay: float = 0.35             # repeated unisons decay by ρ^r
    boundary_reflect_boost: float = 2.0        # boost inward motion near range edge

    # Tessitura / “center of mass” octave; None = infer from ctx on the fly
    center_octave: Optional[int] = None

    # Recent sequence memory (avoid immediate repetition)
    # If >0, we remember last `recent_memory_size` sequences and resample if identical.
    recent_memory_size: int = 16
    max_resample_tries: int = 20               # how many times to try avoiding a repeat

    # RNG
    rng_seed: Optional[int] = None


def _normalize(weights: Dict[int, float]) -> Dict[int, float]:
    s = sum(max(0.0, w) for w in weights.values())
    if s <= 0:
        # fallback: uniform
        n = len(weights)
        return {k: 1.0 / n for k in weights}
    return {k: max(0.0, w) / s for k, w in weights.items()}


def _choose_weighted(rng: random.Random, weights: Dict[int, float]) -> int:
    # weights are normalized
    r = rng.random()
    acc = 0.0
    for k, p in weights.items():
        acc += p
        if r <= acc:
            return k
    # floating-point tail
    return next(reversed(weights))  # last key


def _degree_to_candidates_in_range(ctx: ContextAdapter, degree: int) -> List[int]:
    """All MIDI candidates for a degree within ctx min/max; fallback to octave 4."""
    offset = degree_to_semitone_offset(ctx.scale_type, degree)
    if ctx.min_midi is not None and ctx.max_midi is not None:
        root_pc = PITCH_CLASS_NAMES.index(transpose_key(ctx.key_root))
        target_pc = (root_pc + offset) % 12
        return [m for m in range(int(ctx.min_midi), int(ctx.max_midi) + 1) if (m % 12) == target_pc]
    tonic_midi = note_name_to_midi(transpose_key(ctx.key_root), 4)
    return [tonic_midi + offset]


def _apply_max_interval(prev_midi: Optional[int], cands: List[int], max_semi: int) -> List[int]:
    if prev_midi is None:
        return cands
    lim = int(max_semi)
    return [m for m in cands if abs(m - prev_midi) <= lim] or cands


def _diatonic_interval_steps(curr_midi: int, next_midi: int, ctx: ContextAdapter) -> int:
    """Approximate diatonic step count from pitch-class movement and scale. We’ll do a simple PC delta heuristic here."""
    # This is a minimal placeholder that works “musically enough” for weighting.
    # If you have a robust diatonic distance util elsewhere, swap it in.
    root_pc = PITCH_CLASS_NAMES.index(transpose_key(ctx.key_root))
    curr_pc = (curr_midi % 12 - root_pc) % 12
    next_pc = (next_midi % 12 - root_pc) % 12
    # map scale pcs to degrees in [0..6], then subtract (signed)
    # A simple lookup table per scale_type would be most accurate.
    # For now we approximate by nearest-degree mapping:
    scale_pcs_major = [0, 2, 4, 5, 7, 9, 11]
    pcs = scale_pcs_major if ctx.scale_type.lower().startswith("maj") else scale_pcs_major  # TODO: minor variants
    def nearest_degree(pc: int) -> int:
        # argmin over abs distance in semitones
        d, idx = min((abs(pc - s), i) for i, s in enumerate(pcs))
        return idx
    return nearest_degree(next_pc) - nearest_degree(curr_pc)


class MusicalMelodySampler:
    """
    A reusable, dependency-light melody generator.
    Produces dict: {'truth_degrees': [str,...], 'midi': [int,...]}.

    - Pure (no synth/UI)
    - Deterministic given rng_seed
    - Optional small memory to avoid repeating the exact same sequence
    """

    def __init__(self, ctx: ContextAdapter, config: SamplerConfig = SamplerConfig()):
        self.ctx = ctx
        self.cfg = config
        self.rng = random.Random(config.rng_seed)
        self._recent: deque[Tuple[str, ...]] = deque(maxlen=max(0, config.recent_memory_size))

    # -------- public API --------
    def sample(self, *, seq_len: Optional[int] = None) -> Dict[str, List[int] | List[str]]:
        L = int(seq_len or self.cfg.seq_len)

        # Try a few times to avoid repeating a recent identical melody (degrees only).
        for _ in range(max(1, self.cfg.max_resample_tries)):
            degs, midis = self._sample_once(L)
            sig = tuple(degs)
            if self.cfg.recent_memory_size <= 0 or sig not in self._recent:
                if self.cfg.recent_memory_size > 0:
                    self._recent.append(sig)
                return {"truth_degrees": degs, "midi": midis}

        # If we couldn’t avoid repetition, just return the last attempt.
        return {"truth_degrees": degs, "midi": midis}

    # -------- core logic --------
    def _sample_once(self, L: int) -> Tuple[List[str], List[int]]:
        truth_degrees: List[str] = []
        midis: List[int] = []

        # Pick a starting degree (uniform over ctx.degrees_in_scope for now;
        # you can swap in a tonic/dominant bias later if desired).
        start_deg = int(self.rng.choice(self.ctx.degrees_in_scope))
        start_midi = self._choose_register_for_degree(start_deg, prev_midi=None)
        truth_degrees.append(str(start_deg))
        midis.append(start_midi)

        # Track simple context state
        same_dir_run = 0
        prev_interval_steps: Optional[int] = None
        unison_run = 0

        for _ in range(1, L):
            # Build weights over diatonic step deltas k ∈ {-7..7}
            base = dict(self.cfg.base_interval_weights)  # shallow copy
            base = self._apply_context_modifiers(
                base=base,
                prev_midi=midis[-1],
                prev_interval_steps=prev_interval_steps,
                same_dir_run=same_dir_run,
                unison_run=unison_run,
            )
            probs = _normalize(base)

            # Draw an interval in DIATONIC STEPS; then realize it as a degree and MIDI in range
            k = _choose_weighted(self.rng, probs)

            # Choose the next degree relative to the last *degree* (wrap 1..7).
            last_deg = int(truth_degrees[-1])
            next_deg = ((last_deg - 1 + k) % 7) + 1

            # Choose a MIDI candidate for that degree in range while respecting max interval
            direction = 0 if k == 0 else (1 if k > 0 else -1)
            next_midi = self._choose_register_for_degree(next_deg, prev_midi=midis[-1], direction=direction)

            # Update context
            truth_degrees.append(str(next_deg))
            midis.append(next_midi)

            # Use the sampled diatonic step k to drive state (don’t infer from MIDI PCs)
            if k == 0:
                unison_run += 1
            else:
                unison_run = 0

            if prev_interval_steps is not None and (k * prev_interval_steps) > 0:
                same_dir_run += 1
            else:
                same_dir_run = 1  # reset (counts current move)

            prev_interval_steps = k

        return truth_degrees, midis

    # -------- helpers --------
    def _choose_register_for_degree(
        self,
        degree: int,
        prev_midi: Optional[int],
        *,
        direction: Optional[int] = None,
    ) -> int:
        cands = _degree_to_candidates_in_range(self.ctx, degree)
        cands = _apply_max_interval(prev_midi, cands, self.cfg.max_interval_semitones)

        # If no candidates (shouldn’t happen), fall back to first available mapping
        if not cands:
            fallback = _degree_to_candidates_in_range(self.ctx, degree)
            return fallback[0] if fallback else note_name_to_midi(transpose_key(self.ctx.key_root), 4)

        if prev_midi is not None:
            choices = list(cands)
            if direction is not None:
                if direction > 0:
                    filtered = [m for m in choices if m >= prev_midi]
                elif direction < 0:
                    filtered = [m for m in choices if m <= prev_midi]
                else:
                    filtered = choices
                if filtered:
                    choices = filtered

            if self.ctx.min_midi is not None and self.ctx.max_midi is not None:
                near_top = prev_midi >= (self.ctx.max_midi - 2)
                near_bot = prev_midi <= (self.ctx.min_midi + 2)
                if (near_top and direction is not None and direction > 0) or (near_bot and direction is not None and direction < 0):
                    choices = list(cands)  # override direction to move inward
                if near_top or near_bot:
                    target = (self.ctx.max_midi - 4) if near_top else (self.ctx.min_midi + 4)
                    return min(choices, key=lambda m: abs(m - target))

            if self.cfg.center_octave is not None:
                center_midi = note_name_to_midi(transpose_key(self.ctx.key_root), self.cfg.center_octave)
                alpha, beta = 0.7, 0.3  # favor continuity
                def score(m: int) -> float:
                    return alpha * abs(m - prev_midi) + beta * abs(m - center_midi)
                return min(choices, key=score)
            else:
                return min(choices, key=lambda m: abs(m - prev_midi))

        # No previous note: optionally steer toward center octave; otherwise sample uniformly
        if self.cfg.center_octave is not None:
            center_midi = note_name_to_midi(transpose_key(self.ctx.key_root), self.cfg.center_octave)
            weights = [1.0 / (1.0 + abs(m - center_midi)) for m in cands]
            s = sum(weights) or 1.0
            t = self.rng.random() * s
            acc = 0.0
            for m, w in zip(cands, weights):
                acc += w
                if t <= acc:
                    return m
            return cands[-1]
        else:
            return self.rng.choice(cands)

    def _apply_context_modifiers(
        self,
        *,
        base: Dict[int, float],
        prev_midi: Optional[int],
        prev_interval_steps: Optional[int],
        same_dir_run: int,
        unison_run: int,
    ) -> Dict[int, float]:
        # After a big leap (|k|>=3), penalize continuing a big leap in same direction;
        # and boost recovery by step in opposite direction.
        if prev_interval_steps is not None:
            if abs(prev_interval_steps) >= 3:
                same_dir = 1 if prev_interval_steps > 0 else -1
                for k in list(base.keys()):
                    if k * same_dir > 0 and abs(k) >= 3:
                        base[k] *= self.cfg.same_dir_after_big_leap_penalty
                # recovery by step
                rec_k = -1 if prev_interval_steps > 0 else 1
                if rec_k in base:
                    base[rec_k] *= self.cfg.leap_recovery_boost

            # Soft cap long runs in one direction
            if same_dir_run >= self.cfg.same_dir_run_cap_after:
                same_dir = 1 if prev_interval_steps > 0 else -1
                for k in list(base.keys()):
                    if k * same_dir > 0:
                        base[k] *= self.cfg.same_dir_run_penalty

        # Unison run decay (allow unisons but reduce repeated ones)
        if 0 in base and unison_run > 0:
            base[0] *= (self.cfg.unison_run_decay ** unison_run)

        # Additional strong clamp after 2+ consecutive unisons
        if 0 in base and unison_run >= 2:
            base[0] *= 0.15

        # Range reflection near hard bounds (if we have prev_midi and bounds)
        if prev_midi is not None and (self.ctx.min_midi is not None and self.ctx.max_midi is not None):
            top_threshold = int(self.ctx.max_midi) - 2
            bot_threshold = int(self.ctx.min_midi) + 2
            if prev_midi >= top_threshold:
                # discourage going further up; boost downward motion
                for k in list(base.keys()):
                    if k < 0:
                        base[k] *= self.cfg.boundary_reflect_boost
                    elif k > 0:
                        base[k] *= 0.0
            elif prev_midi <= bot_threshold:
                for k in list(base.keys()):
                    if k > 0:
                        base[k] *= self.cfg.boundary_reflect_boost
                    elif k < 0:
                        base[k] *= 0.0

        return base