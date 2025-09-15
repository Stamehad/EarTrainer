from __future__ import annotations

"""Load Parquet stats and compute derived metrics."""

from pathlib import Path
import pandas as pd
from .config import AnalyticsConfig
from .metrics import compute_metrics


def load_and_prepare(parquet_path: Path, cfg: AnalyticsConfig) -> pd.DataFrame:
    """Read stats Parquet and compute metrics with consistent dtypes.

    - Ensures 'scale' and 'category' are categorical.
    - Sorts by (session_start, session_id) if available, else by session_id.
    - Computes metrics and adds a stable session index 'session_idx'.
    """
    df = pd.read_parquet(parquet_path)
    for col in ("scale", "category"):
        if col in df.columns:
            df[col] = df[col].astype("category")
    if "session_start" in df.columns:
        df = df.sort_values(["session_start", "session_id"], kind="stable")
    else:
        df = df.sort_values(["session_id"], kind="stable")

    df = compute_metrics(df, cfg)
    # Stable session order index
    df["session_idx"] = pd.factorize(df["session_id"])[0]
    return df

