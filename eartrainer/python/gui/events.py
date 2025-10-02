from __future__ import annotations

"""Tiny pub/sub event bus usable by the new GUI."""

from typing import Any, Callable, Dict, List


class EventBus:
    def __init__(self) -> None:
        self._subs: Dict[str, List[Callable[[Any], None]]] = {}

    def subscribe(self, event: str, handler: Callable[[Any], None]) -> None:
        self._subs.setdefault(event, []).append(handler)

    def emit(self, event: str, payload: Any) -> None:
        for handler in self._subs.get(event, []):
            try:
                handler(payload)
            except Exception:
                # Best effort; keep the bus operational even if a handler fails.
                pass
