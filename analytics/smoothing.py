from __future__ import annotations

"""Smoothing utilities (EWMA by session)."""

import pandas as pd


def ewma_by_session(
    df: pd.DataFrame,
    value_col: str,
    span: int,
    group_cols: list[str] | None = None,
) -> pd.DataFrame:
    """Apply EWMA smoothing per group over session order without GroupBy.apply on DataFrame.

    This avoids pandas FutureWarning by grouping a Series and aligning the result.
    Returns a copy of df with a new column f"{value_col}_smooth" and rows sorted by session_idx.
    """
    group_cols = group_cols or []
    g = df.sort_values("session_idx").copy()
    # SeriesGroupBy.apply to avoid DataFrameGroupBy.apply warning
    smooth = g.groupby(group_cols, observed=True)[value_col].apply(lambda s: s.ewm(span=span).mean())
    # Drop group index levels to align with g's row index order
    if isinstance(smooth.index, pd.MultiIndex):
        smooth = smooth.reset_index(level=group_cols, drop=True)
    g[f"{value_col}_smooth"] = smooth.astype("float32").values
    return g
