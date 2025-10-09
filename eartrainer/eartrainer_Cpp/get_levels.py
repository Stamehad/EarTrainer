#!/usr/bin/env python3
"""Utility to inspect adaptive drill catalog levels."""

from __future__ import annotations

import argparse
import pathlib
import sys
from typing import Any, Dict, Iterable

try:
    import yaml
except ImportError:  # pragma: no cover
    print("PyYAML is required to run this script.", file=sys.stderr)
    sys.exit(1)


def load_catalog(path: pathlib.Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    if not isinstance(data, dict) or "drills" not in data:
        raise ValueError(f"Catalog {path} does not contain a top-level 'drills' list")
    return data


def format_entry(entry: Dict[str, Any]) -> str:
    drill_id = entry.get("id", "<unknown>")
    level = entry.get("level", "<n/a>")

    defaults = entry.get("defaults", {}) or {}
    tempo = defaults.get("tempo_bpm", "n/a")

    params = entry.get("params") or entry.get("sampler_params") or {}
    allowed = params.get("allowed_degrees")
    if isinstance(allowed, Iterable) and not isinstance(allowed, (str, bytes)):
        allowed_str = ",".join(str(d) for d in allowed)
    else:
        return f"{drill_id} | L={level} | bpm={tempo}"

    return f"{drill_id} | L={level} | bpm={tempo} | allowed_degrees=[{allowed_str}]"


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="List adaptive drill catalog entries")
    parser.add_argument(
        "catalog",
        nargs="?",
        default=pathlib.Path(__file__).with_name("resources") / "adaptive_levels.yml",
        type=pathlib.Path,
        help="Path to adaptive_levels.yml (defaults to resources/adaptive_levels.yml).",
    )
    args = parser.parse_args(list(argv))

    catalog_path = args.catalog
    if not catalog_path.exists():
        print(f"Catalog file not found: {catalog_path}", file=sys.stderr)
        return 1

    data = load_catalog(catalog_path)
    drills = data.get("drills", [])

    for i, entry in enumerate(drills):
        if not isinstance(entry, dict):
            continue
        print(f"{i})", format_entry(entry))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
