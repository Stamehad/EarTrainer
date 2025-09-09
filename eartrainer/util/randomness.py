from __future__ import annotations

"""Randomness helpers for key selection and seeding."""

import os
import random
from typing import List

try:
    import numpy as _np  # type: ignore
except Exception:  # pragma: no cover - optional
    _np = None  # type: ignore


_KEYS: List[str] = [
    "C",
    "Db",
    "D",
    "Eb",
    "E",
    "F",
    "Gb",
    "G",
    "Ab",
    "A",
    "Bb",
    "B",
]


def seed_if_needed() -> None:
    """Seed RNGs if SEED env var is set."""
    seed = os.environ.get("SEED")
    if seed is not None:
        try:
            s = int(seed)
        except ValueError:
            return
        random.seed(s)
        if _np is not None:
            try:
                _np.random.seed(s)
            except Exception:
                pass


def choose_random_key() -> str:
    """Choose a random key from 12 keys including flats."""
    return random.choice(_KEYS)

