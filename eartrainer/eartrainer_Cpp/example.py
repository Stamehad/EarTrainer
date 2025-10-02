from __future__ import annotations
from dataclasses import dataclass, field
from typing import List, Tuple, Dict, Optional, Iterable
import math
import random

# -----------------------------
# Constants & helpers
# -----------------------------

T_MIN_SEC = 1.0   # hard full-credit boundary
T_MAX_SEC = 9.0   # hard zero-credit boundary

# Normalization helpers (tempo and N are normalized elsewhere if needed)
def clip01(x: float) -> float:
    return max(0.0, min(1.0, x))

def norm_tempo(t: int, t_min: int, t_max: int) -> float:
    return clip01((t - t_min) / (t_max - t_min))

# Convert raw response seconds to normalized speed credit in [0,1]
# 1s -> 1.0 ; 9s -> 0.0 ; linear in between (you can swap to a curve if desired)
def speed_credit_from_seconds(sec: float) -> float:
    if sec <= T_MIN_SEC:
        return 1.0
    if sec >= T_MAX_SEC:
        return 0.0
    # linear map: (1 -> 1) to (9 -> 0)
    return 1.0 - (sec - T_MIN_SEC) / (T_MAX_SEC - T_MIN_SEC)

# Inverse (approx) for convenience if needed later

def seconds_from_speed_credit(credit: float) -> float:
    credit = clip01(credit)
    return T_MIN_SEC + (1.0 - credit) * (T_MAX_SEC - T_MIN_SEC)

# -----------------------------
# Isotonic regression (PAV) — non-increasing fit
# -----------------------------
# We implement a lightweight Pool-Adjacent-Violators algorithm that enforces
# a NON-INCREASING sequence y_hat over x-ordered bins.

@dataclass
class IsoPoint:
    value: float
    weight: float

class IsotonicNonIncreasing:
    """Weighted PAV enforcing non-increasing fitted values.

    Input: list of (value, weight) in order of increasing x (tempo).
    Output: list of fitted values y_hat with y_hat[i] >= y_hat[i+1].
    """
    @staticmethod
    def fit(values: List[float], weights: Optional[List[float]] = None) -> List[float]:
        n = len(values)
        if weights is None:
            weights = [1.0] * n
        # Initialize blocks
        blocks: List[List[IsoPoint]] = [[IsoPoint(values[i], weights[i])] for i in range(n)]

        def block_mean(block: List[IsoPoint]) -> float:
            w = sum(p.weight for p in block)
            if w == 0:
                return 0.0
            return sum(p.value * p.weight for p in block) / w

        means = [block_mean(b) for b in blocks]
        # PAV for non-increasing: enforce means[i] >= means[i+1]
        i = 0
        while i < len(blocks) - 1:
            if means[i] < means[i + 1]:
                # violation -> pool blocks i and i+1
                blocks[i] = blocks[i] + blocks[i + 1]
                del blocks[i + 1]
                means[i] = block_mean(blocks[i])
                # step back if possible to re-check monotonicity
                if i > 0:
                    i -= 1
            else:
                i += 1
        # Expand block means to per-point fitted values
        fitted: List[float] = []
        for b in blocks:
            m = block_mean(b)
            fitted.extend([m] * len(b))
        return fitted

# -----------------------------
# Binned calibrator per N (tempo -> expected response seconds)
# -----------------------------

@dataclass
class BinStat:
    tempo_center: int
    ema_sec: float
    count: int

@dataclass
class TempoCalibrator:
    t_min: int = 60
    t_max: int = 200
    n_bins: int = 10
    ema_lambda: float = 0.1
    # anchor settings to respect boundary conditions
    anchor_fast_sec: float = 0.8  # pseudo-point at fastest tempo (<= 1s)
    anchor_slow_sec: float = 9.5  # pseudo-point at slowest tempo (>= 9s)

    bins: Dict[int, BinStat] = field(default_factory=dict)

    def __post_init__(self):
        if not self.bins:
            edges = [int(round(self.t_min + i * (self.t_max - self.t_min) / (self.n_bins - 1)))
                     for i in range(self.n_bins)]
            for t in edges:
                # initialize with a neutral prior: mid between 1s and 9s (i.e., 5s)
                self.bins[t] = BinStat(tempo_center=t, ema_sec=5.0, count=0)

    def nearest_center(self, tempo: int) -> int:
        return min(self.bins.keys(), key=lambda c: abs(c - tempo))

    def update(self, tempo: int, observed_sec: float):
        c = self.nearest_center(tempo)
        b = self.bins[c]
        lam = self.ema_lambda
        b.ema_sec = (1 - lam) * b.ema_sec + lam * float(observed_sec)
        b.count += 1

    def smoothed_curve(self) -> List[Tuple[int, float]]:
        # Prepare sequences ordered by increasing tempo
        centers = sorted(self.bins.keys())
        values = []
        weights = []
        for i, c in enumerate(centers):
            v = self.bins[c].ema_sec
            w = max(1, self.bins[c].count)
            values.append(v)
            weights.append(w)
        # Add boundary anchors (heavy weights) at ends
        # Fastest (first): expect <= 1s
        values[0] = min(values[0], self.anchor_fast_sec)
        weights[0] = max(weights[0], 50)
        # Slowest (last): expect >= 9s
        values[-1] = max(values[-1], self.anchor_slow_sec)
        weights[-1] = max(weights[-1], 50)
        fitted = IsotonicNonIncreasing.fit(values, weights)
        return list(zip(centers, fitted))

    def predict_seconds(self, tempo: int) -> float:
        curve = self.smoothed_curve()
        centers = [c for c, _ in curve]
        secs = [s for _, s in curve]
        # clamp to ends
        if tempo <= centers[0]:
            return secs[0]
        if tempo >= centers[-1]:
            return secs[-1]
        # linear interp between nearest centers
        for i in range(len(centers) - 1):
            if centers[i] <= tempo <= centers[i + 1]:
                c0, c1 = centers[i], centers[i + 1]
                s0, s1 = secs[i], secs[i + 1]
                a = (tempo - c0) / (c1 - c0)
                return (1 - a) * s0 + a * s1
        return secs[-1]

# -----------------------------
# Drill menu and scoring
# -----------------------------

@dataclass
class DrillCandidate:
    N: int           # number of notes
    tempo: int       # bpm
    pred_sec: float  # predicted response seconds per note
    Fe: float        # expected fitness according to current model
    diff: float      # normalized difficulty [0,1]
    def __str__(self):
        return f"DrillCandidate(N={self.N}, tempo={self.tempo}, pred_sec={self.pred_sec:.2f}, Fe={self.Fe:.3f}, diff={self.diff:.3f})"
    def __repr__(self):
        return self.__str__()

@dataclass
class BoutStats:
    n_items: int = 0
    sum_score: float = 0.0
    @property
    def avg_score(self) -> float:
        return self.sum_score / self.n_items if self.n_items > 0 else 0.0

class DrillHub:
    """Bout-level adaptive melodic hub (single-note, intervals, melodies>=3).

    API:
      dh = DrillHub(F=0.5, p_star=0.85, tempos=[72,84,96], N_set=[1,2,3,4])
      dh.new_bout(F)    # optional: reseed with external fitness
      cand = dh.next()  # -> DrillCandidate
      dh.feedback(correct=True, observed_sec=1.4)
      ... (repeat) ...
      stats = dh.update()  # end-of-bout summary
    """
    def __init__(self,
                 F: float,
                 p_star: float = 0.85,
                 wN: float = 0.6,
                 tempos: Optional[List[int]] = None,
                 N_set: Optional[List[int]] = None,
                 t_min: int = 60,
                 t_max: int = 200,
                 N_max: int = 8):
        self.F = float(F)
        self.p_star = float(p_star)
        self.wN = float(wN)
        self.t_min = int(t_min)
        self.t_max = int(t_max)
        self.N_max = int(N_max)
        self.N_set = N_set or [1, 2, 3, 4]
        # limit active tempos per bout; persistent calibrators handle full range
        self.active_tempos = tempos or [72, 84, 96]
        # per-N calibrators persist on the hub
        self.calibrators: Dict[int, TempoCalibrator] = {
            N: TempoCalibrator(t_min=self.t_min, t_max=self.t_max, n_bins=10)
            for N in range(1, self.N_max + 1)
        }
        self._last_candidate: Optional[DrillCandidate] = None
        self._bout = BoutStats()
        # hysteresis settings
        self.max_bpm_step = 8
        self.allow_N_step = 1
        self._last_N = None
        self._last_tempo = None

    # -----------------
    # Public API
    # -----------------
    def new_bout(self, F: Optional[float] = None, tempos: Optional[List[int]] = None):
        if F is not None:
            self.F = float(F)
        if tempos is not None:
            self.active_tempos = list(tempos)
        self._bout = BoutStats()
        self._last_candidate = None
        self._last_N = None
        self._last_tempo = None

    def next(self) -> DrillCandidate:
        candidates = self._make_menu()
        print(candidates)
        cand = self._sample_from_menu(candidates)
        self._last_candidate = cand
        self._last_N = cand.N
        self._last_tempo = cand.tempo
        return cand

    def feedback(self, correct: bool, observed_sec: float):
        # Update calibrator with raw seconds (clip only for scoring)
        if self._last_candidate is None:
            return
        N = self._last_candidate.N
        self.calibrators[N].update(self._last_candidate.tempo, observed_sec)
        # score this item
        T_clip = speed_credit_from_seconds(observed_sec)
        # expected term uses current model prediction already stored in candidate.Fe
        # For observed score, we mirror fitness formula structure (only the speed term varies here)
        # Use predicted Fe as baseline; recompute the speed term with observed clip to get an observed score proxy
        Fe_pred = self._last_candidate.Fe
        # Break Fe into (omega*N_n*t_n) and (1-omega)*T_clip parts using the same weights
        # For simplicity, reuse Fe_pred magnitude and substitute the speed contribution proportionally
        # A simpler practical approach: treat per-item score = correct * T_clip
        # (You can expand to include the structure term if you prefer.)
        per_item_score = (1.0 if correct else 0.0) * T_clip
        self._bout.n_items += 1
        self._bout.sum_score += per_item_score

    def update(self) -> BoutStats:
        # End-of-bout hook (smoothing happens lazily on predict)
        return self._bout

    # -----------------
    # Internals
    # -----------------
    def _q_difficulty(self, N: int, tempo: int) -> float:
        # normalized N and tempo
        n = (N - 1) / (self.N_max - 1) if self.N_max > 1 else 0.0
        t = norm_tempo(tempo, self.t_min, self.t_max)
        return clip01(self.wN * n + (1.0 - self.wN) * t)

    def _expected_fitness(self, N: int, tempo: int) -> Tuple[float, float]:
        # Predict expected response seconds via calibrator, then clip to speed credit
        pred_sec = self.calibrators[N].predict_seconds(tempo)
        T_clip = speed_credit_from_seconds(pred_sec)
        q = self._q_difficulty(N, tempo)
        # Your simple Fe structure: p* * (omega*N_n*t_n + (1-omega)*T_clip)
        Fe = self.p_star * (self.wN * q + (1.0 - self.wN) * T_clip)  # reuse q for structure term compactly
        return max(0.0, min(1.0, Fe)), q 

    def _make_menu(self) -> List[DrillCandidate]:
        # Build candidates from active tempos and allowed N values
        menu: List[DrillCandidate] = []
        target = self.F
        for N in self.N_set:
            for tempo in self._tempos_with_rate_limits():
                pred_sec = self.calibrators[N].predict_seconds(tempo)
                # respect hard boundaries for feasibility
                if pred_sec < T_MIN_SEC or pred_sec > T_MAX_SEC:
                    continue
                Fe, diff = self._expected_fitness(N, tempo)
                menu.append(DrillCandidate(N=N, tempo=tempo, pred_sec=pred_sec, Fe=Fe, diff=diff))
        # Keep the 6 closest to target fitness
        menu.sort(key=lambda c: abs(c.Fe - target))
        return menu[:6]

    def _tempos_with_rate_limits(self) -> Iterable[int]:
        if self._last_tempo is None:
            return list(self.active_tempos)
        # allow only tempos within +/- max_bpm_step of last tempo plus the current
        allowed = []
        for t in self.active_tempos:
            if abs(t - self._last_tempo) <= self.max_bpm_step or t == self._last_tempo:
                allowed.append(t)
        if not allowed:
            allowed = [self._last_tempo]
        return allowed

    def _sample_from_menu(self, menu: List[DrillCandidate]) -> DrillCandidate:
        if not menu:
            # fallback: make a conservative default
            N = self._last_N if self._last_N is not None else min(self.N_set)
            tempo = self._last_tempo if self._last_tempo is not None else min(self.active_tempos)
            pred = self.calibrators[N].predict_seconds(tempo)
            Fe, diff = self._expected_fitness(N, tempo)
            return DrillCandidate(N, tempo, pred, Fe, diff)
        # 70/20/10 matched/easier/harder around the closest
        menu_sorted = sorted(menu, key=lambda c: c.Fe)
        target = self.F
        # find insertion index
        idx = 0
        while idx < len(menu_sorted) and menu_sorted[idx].Fe < target:
            idx += 1
        center = min(idx, len(menu_sorted) - 1)
        choices: List[DrillCandidate] = [menu_sorted[center]]
        easier = menu_sorted[max(0, center - 1)]
        harder = menu_sorted[min(len(menu_sorted) - 1, center + 1)]
        r = random.random()
        if r < 0.7:
            pick = choices[0]
        elif r < 0.9:
            pick = easier
        else:
            pick = harder
        # N step limiter
        if self._last_N is not None and abs(pick.N - self._last_N) > self.allow_N_step:
            pick = DrillCandidate(self._last_N, pick.tempo, self.calibrators[self._last_N].predict_seconds(pick.tempo), self._expected_fitness(self._last_N, pick.tempo))
        return pick

# -----------------------------
# Minimal demo stub (no I/O) — optional for quick sanity checks
# -----------------------------
if __name__ == "__main__":
    dh = DrillHub(F=0.2, tempos=[80, 88, 96], N_set=[1,2,3,4], t_min=60, t_max=200)
    dh.new_bout(F=0.2)
    for _ in range(5):
        cand = dh.next()
        # fake feedback: observed seconds rises slightly with tempo and N
        noise = random.uniform(-0.15, 0.15)
        base = 1.2 + 0.1 * (cand.N - 1) + 0.005 * (cand.tempo - 80)
        observed = max(0.7, base + noise)
        dh.feedback(correct=True, observed_sec=observed)
        print(f"Q: N={cand.N}, tempo={cand.tempo}, pred_sec={cand.pred_sec:.2f}, Fe={cand.Fe:.3f} | observed_sec={observed:.2f}")
    stats = dh.update()
    print({"items": stats.n_items, "avg_score": round(stats.avg_score,3)})
