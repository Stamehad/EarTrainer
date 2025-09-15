from __future__ import annotations

"""Demo script for analytics module.

Loads Parquet session-degree stats, computes metrics, smooths trends,
and generates basic plots in ./reports.
"""

from pathlib import Path
from analytics.config import AnalyticsConfig
from analytics.prepare import load_and_prepare
from analytics.smoothing import ewma_by_session
from analytics.plots import plot_trend, plot_heatmap, plot_radar_degree, plot_assistance_impact


def _default_parquet() -> Path:
    # Prefer storage/data where the app writes; fall back to data/
    p1 = Path("storage/data/session_degree_stats.parquet")
    if p1.exists():
        return p1
    return Path("data/session_degree_stats.parquet")


def main() -> int:
    cfg = AnalyticsConfig()
    parquet = _default_parquet()
    if not parquet.exists():
        print(f"Parquet file not found: {parquet}")
        return 2

    df = load_and_prepare(parquet, cfg)

    # Build smoothed columns for mark & acc per (scale, category, degree)
    for col in ["mark", "acc"]:
        df = ewma_by_session(df, value_col=col, span=cfg.smoothing_span, group_cols=["scale", "category", "degree"])

    outdir = Path("reports")
    outdir.mkdir(exist_ok=True, parents=True)

    # Trend example
    plot_trend(df, scale="major", category="single_note", degree=7, value_col="mark", save_path=outdir / "trend_major_single_deg7.png")
    # Heatmap example
    plot_heatmap(df, scale="major", value_col="mark", save_path=outdir / "heatmap_major_mark.png")
    # Radar example
    plot_radar_degree(df, scale="major", category="chord", value_col="mark", save_path=outdir / "radar_major_chord_mark.png")
    # Assistance impact
    plot_assistance_impact(df, save_path=outdir / "assistance_vs_mark.png")

    # Snapshot CSV
    snap_cols = [
        "session_id",
        "session_start",
        "scale",
        "category",
        "degree",
        "Q",
        "C",
        "T_ms",
        "acc",
        "rt_mean_ms",
        "rt_factor",
        "assist_factor",
        "mark",
    ]
    df[[c for c in snap_cols if c in df.columns]].to_csv(outdir / "analytics_snapshot.csv", index=False)
    print(f"Reports saved to: {outdir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

