from __future__ import annotations

"""Analytics configuration (hyperparameters) using Pydantic."""

from typing import Dict
from pydantic import BaseModel, Field


class AnalyticsConfig(BaseModel):
    """Hyperparameters for analytics computations and smoothing.

    - alpha: RT penalty scale (>0)
    - T_ref_ms: reference reaction time in ms (>0)
    - assist_weights: weights for assistance types (c,s,r,p,t)
    - smoothing_span: EWMA span in sessions (>1)
    """

    alpha: float = Field(0.9, gt=0)
    T_ref_ms: int = Field(2000, gt=0)
    assist_weights: Dict[str, float] = Field(
        default_factory=lambda: {"c": 0.20, "s": 0.15, "r": 0.10, "p": 0.15, "t": 0.30}
    )
    smoothing_span: int = Field(10, gt=1)

