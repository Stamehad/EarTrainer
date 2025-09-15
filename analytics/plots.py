from __future__ import annotations

"""Matplotlib plots for trends, heatmaps, radar, and assistance impact."""

from typing import Optional
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def plot_trend(
    df: pd.DataFrame,
    *,
    scale: Optional[str] = None,
    category: Optional[str] = None,
    degree: Optional[int] = None,
    value_col: str = "mark",
    save_path: Optional[str | bytes | "os.PathLike[str]"] = None,
) -> None:
    g = df.copy()
    if scale is not None:
        g = g[g["scale"].astype("string") == scale]
    if category is not None:
        g = g[g["category"].astype("string") == category]
    if degree is not None:
        g = g[g["degree"].astype(int) == int(degree)]
    if g.empty:
        return
    g = g.sort_values("session_idx")
    plt.figure()
    plt.plot(g["session_idx"], g[value_col], marker="o", linestyle="", label=value_col)
    smooth_col = f"{value_col}_smooth"
    if smooth_col in g.columns:
        plt.plot(g["session_idx"], g[smooth_col], linewidth=2, label=f"{value_col} (EWMA)")
    plt.xlabel("Session")
    plt.ylabel(value_col)
    bits = [b for b in [scale, category, (f"deg={degree}" if degree is not None else None)] if b]
    plt.title("Trend — " + ", ".join(bits) if bits else "Trend")
    plt.legend()
    if save_path:
        plt.savefig(save_path, bbox_inches="tight", dpi=150)
    plt.close()


def plot_heatmap(
    df: pd.DataFrame,
    *,
    scale: Optional[str] = None,
    value_col: str = "mark",
    save_path: Optional[str | bytes | "os.PathLike[str]"] = None,
) -> None:
    g = df.copy()
    if scale is not None:
        g = g[g["scale"].astype("string") == scale]
    if g.empty:
        return
    pivot = (
        g.groupby(["category", "degree"], observed=True)[value_col].mean().unstack("degree").sort_index()
    )
    if pivot.empty:
        return
    M = pivot.to_numpy()
    plt.figure()
    im = plt.imshow(M, aspect="auto", origin="lower")
    plt.colorbar(im, label=value_col)
    plt.xticks(ticks=np.arange(pivot.shape[1]), labels=pivot.columns.astype(str))
    plt.yticks(ticks=np.arange(pivot.shape[0]), labels=pivot.index.astype(str))
    title = f"Heatmap ({value_col})" + (f" — {scale}" if scale else "")
    plt.title(title)
    plt.xlabel("Degree")
    plt.ylabel("Category")
    if save_path:
        plt.savefig(save_path, bbox_inches="tight", dpi=150)
    plt.close()


def plot_radar_degree(
    df: pd.DataFrame,
    *,
    scale: Optional[str] = None,
    category: Optional[str] = None,
    value_col: str = "mark",
    save_path: Optional[str | bytes | "os.PathLike[str]"] = None,
) -> None:
    g = df.copy()
    if scale is not None:
        g = g[g["scale"].astype("string") == scale]
    if category is not None:
        g = g[g["category"].astype("string") == category]
    if g.empty:
        return
    prof = g.groupby("degree", observed=True)[value_col].mean().sort_index()
    if prof.empty:
        return
    vals = prof.values
    labels = prof.index.astype(str).tolist()
    vals = np.concatenate([vals, vals[:1]])
    angles = np.linspace(0, 2 * np.pi, len(labels), endpoint=False)
    angles = np.concatenate([angles, angles[:1]])

    fig = plt.figure()
    ax = fig.add_subplot(111, polar=True)
    ax.plot(angles, vals)
    ax.fill(angles, vals, alpha=0.1)
    ax.set_xticks(angles[:-1])
    ax.set_xticklabels(labels)
    title = "Degree Profile — " + value_col
    if scale:
        title += f" — {scale}"
    if category:
        title += f" — {category}"
    ax.set_title(title)
    if save_path:
        plt.savefig(save_path, bbox_inches="tight", dpi=150)
    plt.close()


def plot_assistance_impact(
    df: pd.DataFrame,
    *,
    save_path: Optional[str | bytes | "os.PathLike[str]"] = None,
) -> None:
    assists = df[["assist_c", "assist_s", "assist_r", "assist_p", "assist_t"]].sum(axis=1)
    if assists.empty:
        return
    plt.figure()
    plt.scatter(assists, df.get("mark", assists * 0), alpha=0.4)
    plt.xlabel("Total assistance (counts per row)")
    plt.ylabel("Mark")
    plt.title("Assistance vs Mark")
    if save_path:
        plt.savefig(save_path, bbox_inches="tight", dpi=150)
    plt.close()

