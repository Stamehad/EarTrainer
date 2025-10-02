from __future__ import annotations

"""Training set loader for scripted session definitions (YAML)."""

from pathlib import Path
from typing import Any, Dict, List

try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover
    yaml = None  # type: ignore


def _default_sets_path() -> str:
    base = Path(__file__).resolve().parents[2]
    return str(base / "resources" / "training_sets" / "basic.yml")


def load_sets(path: str | None = None) -> Dict[str, Any]:
    if yaml is None:
        return {"version": 1, "sets": {}}
    target = path or _default_sets_path()
    with open(target, "r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle) or {}
    return data


def list_sets(path: str | None = None) -> List[Dict[str, Any]]:
    data = load_sets(path)
    items: List[Dict[str, Any]] = []
    for set_id, definition in (data.get("sets") or {}).items():
        items.append({"id": set_id, **(definition or {})})
    return items


def get_set(set_id: str, path: str | None = None) -> Dict[str, Any]:
    data = load_sets(path)
    definition = (data.get("sets") or {}).get(set_id)
    if not definition:
        raise KeyError(f"Unknown training set: {set_id}")
    return dict(definition)
