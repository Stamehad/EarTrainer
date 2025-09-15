from __future__ import annotations

from pathlib import Path
from typing import Any

import pandas as pd

try:
    import pyarrow  # noqa: F401
except Exception:  # pragma: no cover
    pyarrow = None  # type: ignore

from .schema import DTYPES, SessionDegreeRow, SessionMeta, SCALES, CATEGORIES, ASSIST_KEYS


DATA_FILE = "session_degree_stats.parquet"
META_FILE = "sessions.parquet"


def _empty_df() -> pd.DataFrame:
    dtypes = DTYPES.copy()
    df = pd.DataFrame({k: pd.Series(dtype=v) for k, v in dtypes.items()})
    return df


def init_store(data_dir: Path) -> None:
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
    if not isinstance(records, list):
        raise TypeError("records must be a list[SessionDegreeRow]")
    rows = [SessionDegreeRow.parse_obj(r) if not isinstance(r, SessionDegreeRow) else r for r in records]
    data = [r.dict() for r in rows]
    df = pd.DataFrame(data)
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
            if col in ASSIST_KEYS:
                df[col] = pd.Series(0, index=df.index, dtype=DTYPES[col])
    return df[list(DTYPES.keys())]


def append_session_degree_stats(df_new: pd.DataFrame, data_path: Path) -> None:
    data_path = Path(data_path)
    f = data_path / DATA_FILE
    if f.exists():
        df_old = pd.read_parquet(f, engine="pyarrow")
    else:
        df_old = _empty_df()
    df_new = _fix_dtypes(df_new.copy())
    combined = pd.concat([df_old, df_new], ignore_index=True)
    combined = _fix_dtypes(combined)
    combined = combined.drop_duplicates()
    combined.to_parquet(f, engine="pyarrow", compression="zstd", index=False)


def upsert_session_meta(meta: SessionMeta, data_path: Path) -> None:
    data_path = Path(data_path)
    f = data_path / META_FILE
    row = SessionMeta.parse_obj(meta).dict()
    df_new = pd.DataFrame([row])
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
        if "session_id" in df.columns and not df.empty:
            df = df[df["session_id"].astype("string") != row["session_id"]]
        df = pd.concat([df, df_new], ignore_index=True)
    else:
        df = df_new
    df.to_parquet(f, engine="pyarrow", compression="zstd", index=False)


def load_all(data_path: Path) -> pd.DataFrame:
    f = Path(data_path) / DATA_FILE
    if not f.exists():
        return _empty_df().assign(acc=pd.Series(dtype="float32"), rt_mean_ms=pd.Series(dtype="float32"))
    df = pd.read_parquet(f, engine="pyarrow")
    df = _fix_dtypes(df)
    q = df["Q"].astype("float32").where(df["Q"] > 0, other=1.0)
    df["acc"] = (df["C"].astype("float32") / q).astype("float32")
    df["rt_mean_ms"] = (df["T_ms"].astype("float32") / q).astype("float32")
    return df


def query_trend(df: pd.DataFrame, *, scale: str, category: str, degree: int) -> pd.DataFrame:
    if scale not in SCALES:
        raise ValueError(f"Unknown scale: {scale}")
    if category not in CATEGORIES:
        raise ValueError(f"Unknown category: {category}")
    dff = df[(df["scale"].astype("string") == scale) & (df["category"].astype("string") == category) & (df["degree"].astype(int) == int(degree))]
    return dff.sort_values("session_start").reset_index(drop=True)


def export_ndjson(df: pd.DataFrame, out_path: Path) -> None:
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    df.to_json(out_path, orient="records", lines=True, date_format="iso")

