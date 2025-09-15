from __future__ import annotations

"""Parquet-backed store for session-degree stats using pandas + pyarrow.

Unit of data: (session × category × degree) summary rows.
"""

from pathlib import Path
from typing import Any

import pandas as pd

try:
    import pyarrow  # noqa: F401
except Exception as _e:  # pragma: no cover
    pyarrow = None  # type: ignore

from .schema import DTYPES, SessionDegreeRow, SessionMeta, SCALES, CATEGORIES, ASSIST_KEYS


DATA_FILE = "session_degree_stats.parquet"
META_FILE = "sessions.parquet"


def _empty_df() -> pd.DataFrame:
    dtypes = DTYPES.copy()
    df = pd.DataFrame({k: pd.Series(dtype=v) for k, v in dtypes.items()})
    return df


def init_store(data_dir: Path) -> None:
    """Ensure data directory and empty Parquet files with correct schema exist."""
    data_dir = Path(data_dir)
    data_dir.mkdir(parents=True, exist_ok=True)
    stats_path = data_dir / DATA_FILE
    meta_path = data_dir / META_FILE
    if not stats_path.exists():
        _empty_df().to_parquet(stats_path, engine="pyarrow", compression="zstd")
    if not meta_path.exists():
        md = pd.DataFrame(
            {
                "session_id": pd.Series(dtype="string"),
                "session_start": pd.Series(dtype=pd.DatetimeTZDtype(tz="UTC")),
                "app_version": pd.Series(dtype="string"),
                "tonic_midi": pd.Series(dtype="UInt8"),
                "notes": pd.Series(dtype="string"),
            }
        )
        md.to_parquet(meta_path, engine="pyarrow", compression="zstd")


def validate_records(records: list[SessionDegreeRow]) -> pd.DataFrame:
    """Validate a list of SessionDegreeRow and return a DataFrame with proper dtypes.

    - Enforces categories/scales, counts, and degree constraints via Pydantic.
    - Returns a pandas DataFrame with categorical and unsigned integer dtypes.
    """
    if not isinstance(records, list):
        raise TypeError("records must be a list[SessionDegreeRow]")
    rows = [SessionDegreeRow.parse_obj(r) if not isinstance(r, SessionDegreeRow) else r for r in records]
    data = [r.dict() for r in rows]
    df = pd.DataFrame(data)
    # Enforce dtypes
    for col, dt in DTYPES.items():
        if col not in df.columns:
            if col in ASSIST_KEYS:
                df[col] = 0
            else:
                df[col] = pd.NA
        df[col] = df[col].astype(dt)
    return df[list(DTYPES.keys())]


def _fix_dtypes(df: pd.DataFrame) -> pd.DataFrame:
    for col, dt in DTYPES.items():
        if col in df.columns:
            df[col] = df[col].astype(dt)
        else:
            # fill missing assistance columns with zeros
            if col in ASSIST_KEYS:
                df[col] = pd.Series(0, index=df.index, dtype=DTYPES[col])
    return df[list(DTYPES.keys())]


def append_session_degree_stats(df_new: pd.DataFrame, data_path: Path) -> None:
    """Append rows to the session_degree_stats table.

    - Reads existing, concatenates, fixes dtypes, removes exact duplicates, and writes back.
    - Uses pyarrow with zstd compression.
    """
    data_path = Path(data_path)
    f = data_path / DATA_FILE
    if f.exists():
        df_old = pd.read_parquet(f, engine="pyarrow")
    else:
        df_old = _empty_df()
    df_new = _fix_dtypes(df_new.copy())
    combined = pd.concat([df_old, df_new], ignore_index=True)
    combined = _fix_dtypes(combined)
    combined = combined.drop_duplicates()  # exact duplicate rows only
    combined.to_parquet(f, engine="pyarrow", compression="zstd", index=False)


def upsert_session_meta(meta: SessionMeta, data_path: Path) -> None:
    """Insert or update a single session metadata row keyed by session_id."""
    data_path = Path(data_path)
    f = data_path / META_FILE
    row = SessionMeta.parse_obj(meta).dict()
    df_new = pd.DataFrame([row])
    # cast
    df_new = df_new.astype(
        {
            "session_id": "string",
            "session_start": pd.DatetimeTZDtype(tz="UTC"),
            "app_version": "string",
            "tonic_midi": "UInt8",
            "notes": "string",
        }
    )
    if f.exists():
        df = pd.read_parquet(f, engine="pyarrow")
        # drop any existing with same session_id
        if "session_id" in df.columns and not df.empty:
            df = df[df["session_id"].astype("string") != row["session_id"]]
        df = pd.concat([df, df_new], ignore_index=True)
    else:
        df = df_new
    df.to_parquet(f, engine="pyarrow", compression="zstd", index=False)


def load_all(data_path: Path) -> pd.DataFrame:
    """Load the full session_degree_stats, ensuring dtypes, and compute convenience columns.

    Adds:
    - acc: float32 = C / Q
    - rt_mean_ms: float32 = T_ms / Q
    """
    f = Path(data_path) / DATA_FILE
    if not f.exists():
        return _empty_df().assign(acc=pd.Series(dtype="float32"), rt_mean_ms=pd.Series(dtype="float32"))
    df = pd.read_parquet(f, engine="pyarrow")
    df = _fix_dtypes(df)
    # Avoid division warnings; compute as float32
    q = df["Q"].astype("float32").where(df["Q"] > 0, other=1.0)
    df["acc"] = (df["C"].astype("float32") / q).astype("float32")
    df["rt_mean_ms"] = (df["T_ms"].astype("float32") / q).astype("float32")
    return df


def query_trend(df: pd.DataFrame, *, scale: str, category: str, degree: int) -> pd.DataFrame:
    """Filter rows for a given (scale, category, degree) and sort by session_start."""
    if scale not in SCALES:
        raise ValueError(f"Unknown scale: {scale}")
    if category not in CATEGORIES:
        raise ValueError(f"Unknown category: {category}")
    dff = df[(df["scale"].astype("string") == scale) & (df["category"].astype("string") == category) & (df["degree"].astype(int) == int(degree))]
    return dff.sort_values("session_start").reset_index(drop=True)


def export_ndjson(df: pd.DataFrame, out_path: Path) -> None:
    """Export a DataFrame to line-delimited JSON (NDJSON) for quick inspection."""
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    df.to_json(out_path, orient="records", lines=True, date_format="iso")


if __name__ == "__main__":
    # Minimal usage demo
    from datetime import datetime, timezone
    from .schema import SessionDegreeRow

    data_dir = Path("storage/data")
    init_store(data_dir)

    # Build two example sessions (degree rows)
    sid1 = "sess-001"
    sid2 = "sess-002"
    now = datetime.now(timezone.utc)
    recs = [
        SessionDegreeRow(session_id=sid1, session_start=now, scale="major", category="single_note", degree=d, Q=2, C=1, T_ms=1500)
        for d in range(1, 8)
    ]
    recs += [
        SessionDegreeRow(session_id=sid1, session_start=now, scale="major", category="chord", degree=d, Q=2, C=2, T_ms=3000)
        for d in range(1, 8)
    ]
    recs2 = [
        SessionDegreeRow(session_id=sid2, session_start=now, scale="major", category="single_note", degree=7, Q=4, C=3, T_ms=3200)
    ]

    df_new = validate_records(recs)
    append_session_degree_stats(df_new, data_dir)
    df_new2 = validate_records(recs2)
    append_session_degree_stats(df_new2, data_dir)

    df_all = load_all(data_dir)
    trend = query_trend(df_all, scale="major", category="single_note", degree=7)
    print(trend[["session_start", "session_id", "Q", "C", "acc", "rt_mean_ms"]].tail(5))

    export_ndjson(trend, data_dir / "trend_major_single_note_7.ndjson")

