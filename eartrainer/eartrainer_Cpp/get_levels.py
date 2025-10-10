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
    tier = entry.get("tier", "<n/a>")

    defaults = entry.get("defaults", {}) or {}
    tempo = defaults.get("tempo_bpm", "n/a")

    params = entry.get("params") or entry.get("sampler_params") or {}
    allowed = params.get("allowed_degrees")
    s = f"{drill_id} | L={level}, T={tier} | bpm={tempo}"
    if isinstance(allowed, Iterable) and not isinstance(allowed, (str, bytes)):
        allowed_str = ",".join(str(d) for d in allowed)
    else:
        return s

    return s+ f" | allowed_degrees=[{allowed_str}]"


def iter_catalog_files(path: pathlib.Path) -> Iterable[pathlib.Path]:
    if path.is_file():
        yield path
        return

    if not path.is_dir():
        raise FileNotFoundError(f"Catalog path is neither file nor directory: {path}")

    for candidate in sorted(path.glob("*_levels.yml")):
        if candidate.is_file():
            yield candidate


def print_catalog(title: str, path: pathlib.Path) -> None:
    data = load_catalog(path)
    drills = data.get("drills", [])

    print(f"# Track: {title}") # ({path})")
    for i, entry in enumerate(drills):
        if not isinstance(entry, dict):
            continue
        print(f"  {i})", format_entry(entry))
    print()


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="List adaptive drill catalog entries")
    parser.add_argument(
        "catalog",
        nargs="?",
        default=pathlib.Path(__file__).with_name("resources"),
        type=pathlib.Path,
        help=(
            "Path to a catalog file or directory containing *_levels.yml files "
            "(defaults to resources/)."
        ),
    )
    args = parser.parse_args(list(argv))

    catalog_path = args.catalog.resolve()
    if not catalog_path.exists():
        print(f"Catalog path not found: {catalog_path}", file=sys.stderr)
        return 1

    try:
        files = list(iter_catalog_files(catalog_path))
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if not files:
        print(f"No *_levels.yml files found in {catalog_path}", file=sys.stderr)
        return 1

    for catalog_file in files:
        track_name = catalog_file.stem.replace("_levels", "")
        try:
            print_catalog(track_name, catalog_file)
        except Exception as exc:
            print(f"Failed to load {catalog_file}: {exc}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
