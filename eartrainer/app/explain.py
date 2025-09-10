from __future__ import annotations

"""Minimal tracing helpers (Explain Mode).

Enable with CLI flag and emit terse, readable lines at milestones.
"""

import json
from typing import Any, Dict

_ENABLED = False


def enable(flag: bool = True) -> None:
    global _ENABLED
    _ENABLED = bool(flag)


def enabled() -> bool:
    return _ENABLED


def trace(event: str, payload: Dict[str, Any] | None = None) -> None:
    if not _ENABLED:
        return
    try:
        data = (payload or {})
        # keep it short; one line JSON
        print(f"[EXPLAIN] {event} :: {json.dumps(data, separators=(',',':'))}")
    except Exception:
        # best effort
        print(f"[EXPLAIN] {event}")

