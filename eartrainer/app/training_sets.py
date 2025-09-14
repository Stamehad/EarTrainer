from __future__ import annotations

"""Training set loader (YAML).

Loads named sets of drill steps from a YAML resource: versioned, human-friendly.
"""

from pathlib import Path
from typing import Any, Dict, List

try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover
    yaml = None  # type: ignore


def _default_sets_path() -> str:
    return str(Path(__file__).resolve().parents[1] / "resources" / "training_sets" / "basic.yml")


def load_sets(path: str | None = None) -> Dict[str, Any]:
    if yaml is None:
        return {"version": 1, "sets": {}}
    p = path or _default_sets_path()
    with open(p, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    return data


def list_sets(path: str | None = None) -> List[Dict[str, Any]]:
    data = load_sets(path)
    items = []
    for sid, sdef in (data.get("sets") or {}).items():
        items.append({"id": sid, **(sdef or {})})
    return items


def get_set(set_id: str, path: str | None = None) -> Dict[str, Any]:
    data = load_sets(path)
    s = (data.get("sets") or {}).get(set_id)
    if not s:
        raise KeyError(f"Unknown training set: {set_id}")
    return dict(s)

