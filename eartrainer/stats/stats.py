from __future__ import annotations

"""Basic session stats: JSON-based aggregation and formatting."""

import json
from pathlib import Path
from typing import Dict


def new_session_stats() -> Dict:
    """Create a new, empty stats structure."""
    return {"total": 0, "correct": 0, "per_degree": {}}


def update_stats(stats: Dict, degree: str, correct: bool) -> None:
    """Update stats for a single question outcome."""
    stats["total"] = int(stats.get("total", 0)) + 1
    if correct:
        stats["correct"] = int(stats.get("correct", 0)) + 1
    per = stats.setdefault("per_degree", {})
    bucket = per.setdefault(degree, {"asked": 0, "correct": 0})
    bucket["asked"] += 1
    bucket["correct"] += 1 if correct else 0


def write_stats(stats: Dict, path: str) -> None:
    """Write stats as JSON to path."""
    p = Path(path)
    with p.open("w", encoding="utf-8") as f:
        json.dump(stats, f, indent=2)


def format_summary(stats: Dict) -> str:
    """Return a human-readable summary of stats."""
    total = int(stats.get("total", 0))
    correct = int(stats.get("correct", 0))
    lines = [f"Total: {correct}/{total} correct"]
    per = stats.get("per_degree", {})
    for d in sorted(per.keys(), key=lambda x: int(x) if x.isdigit() else 99):
        asked = per[d].get("asked", 0)
        corr = per[d].get("correct", 0)
        lines.append(f"Degree {d}: {corr}/{asked}")
    return "\n".join(lines)

