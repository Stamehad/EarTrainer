from __future__ import annotations

"""Schema constants and Pydantic models for Parquet-backed session stats."""

from datetime import datetime, timezone
from typing import Literal, Optional

import pandas as pd
from pandas.api.types import CategoricalDtype
from pydantic import BaseModel, Field, validator

# --- Constants ---

SCALES = {"major", "natural_minor", "harmonic_minor", "melodic_minor"}
CATEGORIES = {"single_note", "chord", "interval"}
ASSIST_KEYS = ["assist_c", "assist_s", "assist_r", "assist_p", "assist_t"]


def _cat_dtype(categories: set[str]) -> CategoricalDtype:
    return CategoricalDtype(categories=sorted(categories), ordered=False)


DTYPES = {
    "session_id": "string",
    # timezone-aware UTC timestamps
    "session_start": pd.DatetimeTZDtype(tz="UTC"),
    "scale": _cat_dtype(SCALES),
    "category": _cat_dtype(CATEGORIES),
    "degree": "UInt8",
    "Q": "UInt16",
    "C": "UInt16",
    "T_ms": "UInt32",
    "assist_c": "UInt16",
    "assist_s": "UInt16",
    "assist_r": "UInt16",
    "assist_p": "UInt16",
    "assist_t": "UInt16",
}


# --- Pydantic models ---

class SessionDegreeRow(BaseModel):
    session_id: str
    session_start: datetime
    scale: Literal[tuple(SCALES)]  # type: ignore[arg-type]
    category: Literal[tuple(CATEGORIES)]  # type: ignore[arg-type]
    degree: int = Field(ge=1, le=255)
    Q: int = Field(ge=1, le=65535)
    C: int = Field(ge=0, le=65535)
    T_ms: int = Field(ge=0, le=4294967295)
    assist_c: int = Field(default=0, ge=0, le=65535)
    assist_s: int = Field(default=0, ge=0, le=65535)
    assist_r: int = Field(default=0, ge=0, le=65535)
    assist_p: int = Field(default=0, ge=0, le=65535)
    assist_t: int = Field(default=0, ge=0, le=65535)

    @validator("C")
    def _c_le_q(cls, v: int, values):  # type: ignore[override]
        q = int(values.get("Q", 0))
        if v > q:
            raise ValueError("C must be <= Q")
        return v

    @validator("degree")
    def _degree_range(cls, v: int):  # type: ignore[override]
        if not (1 <= int(v) <= 7):
            raise ValueError("degree must be in 1..7 for diatonic stats")
        return v

    @validator("session_start")
    def _ensure_utc(cls, v: datetime):  # type: ignore[override]
        if v.tzinfo is None:
            return v.replace(tzinfo=timezone.utc)
        return v.astimezone(timezone.utc)


class SessionMeta(BaseModel):
    session_id: str
    session_start: datetime
    app_version: Optional[str] = None
    tonic_midi: Optional[int] = Field(default=None, ge=0, le=127)
    notes: Optional[str] = None

    @validator("session_start")
    def _ensure_utc(cls, v: datetime):  # type: ignore[override]
        if v.tzinfo is None:
            return v.replace(tzinfo=timezone.utc)
        return v.astimezone(timezone.utc)

