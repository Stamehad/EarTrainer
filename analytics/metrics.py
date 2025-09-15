from __future__ import annotations

"""Metric computations for per-row analytics."""

import numpy as np
import pandas as pd
from .config import AnalyticsConfig

ASSIST_KEYS = ["assist_c", "assist_s", "assist_r", "assist_p", "assist_t"]
ASSIST_SHORT = ["c", "s", "r", "p", "t"]


def compute_metrics(df: pd.DataFrame, cfg: AnalyticsConfig) -> pd.DataFrame:
    """Compute accuracy, mean RT, factors, and composite mark.

    Returns a copy with added columns:
    - acc, rt_mean_ms, rt_factor, assist_factor, mark
    """
    out = df.copy()
    # Avoid divide by zero; Q should be >=1 by construction
    q = out["Q"].astype("float32").where(out["Q"] > 0, other=1.0)
    out["acc"] = (out["C"].astype("float32") / q).astype("float32")
    out["rt_mean_ms"] = (out["T_ms"].astype("float32") / q).astype("float32")

    # Reaction time factor: exp(-alpha * rt_mean/T_ref)
    out["rt_factor"] = np.exp(-float(cfg.alpha) * (out["rt_mean_ms"] / float(cfg.T_ref_ms))).astype("float32")

    # Assistance factor: 1/(1+ sum(w_i * count_i))
    weights = np.array([cfg.assist_weights.get(k, 0.0) for k in ASSIST_SHORT], dtype="float32")
    assists = out[ASSIST_KEYS].astype("float32").to_numpy(copy=False)
    pen = (assists * weights).sum(axis=1, dtype="float32")
    out["assist_factor"] = (1.0 / (1.0 + pen)).astype("float32")

    # Composite mark
    out["mark"] = (out["acc"] * out["rt_factor"] * out["assist_factor"]).clip(0, 1).astype("float32")
    return out

