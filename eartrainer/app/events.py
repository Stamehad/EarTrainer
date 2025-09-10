from __future__ import annotations

"""Tiny pub/sub event bus (scaffold)."""

from typing import Callable, Dict, List, Any


class EventBus:
    def __init__(self) -> None:
        self._subs: Dict[str, List[Callable[[Any], None]]] = {}

    def subscribe(self, event: str, handler: Callable[[Any], None]) -> None:
        self._subs.setdefault(event, []).append(handler)

    def emit(self, event: str, payload: Any) -> None:
        for h in self._subs.get(event, []):
            try:
                h(payload)
            except Exception:
                # Best effort; keep going
                pass
