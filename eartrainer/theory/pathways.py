from __future__ import annotations

"""Pathway definitions loader for scale-degree practice.

Loads YAML mappings of scale_type -> degree -> sequence of degrees (as strings).
"""

from functools import lru_cache
from pathlib import Path
from typing import Dict, List

try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover
    yaml = None  # type: ignore


@lru_cache(maxsize=1)
def load_pathways(path: str | None = None) -> Dict[str, Dict[str, List[str]]]:
    if yaml is None:  # pragma: no cover
        return {}
    if path is None:
        path = str(Path(__file__).resolve().parents[1] / "resources" / "pathways" / "basic.yml")
    with open(path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    # drop version key if present
    data.pop("version", None)
    return data  # type: ignore[return-value]


def get_pathway(scale_type: str, degree: str, path: str | None = None) -> List[str]:
    data = load_pathways(path)
    table = data.get(scale_type, {})
    seq = table.get(str(degree), [])
    return list(seq)

