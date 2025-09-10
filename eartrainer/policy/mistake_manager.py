from __future__ import annotations

"""Mistake Manager interfaces and simple strategies (scaffold)."""

from dataclasses import dataclass
from typing import Literal, Protocol, Any


@dataclass(frozen=True)
class Decision:
    action: Literal["next", "retry", "replay", "reveal"]
    penalty: int
    feedback: str = ""


class MistakeManager(Protocol):
    def decide(self, question: Any, user_answer: str, is_correct: bool) -> Decision: ...


class ImmediateReveal:
    """Correct → next; wrong → reveal, mark wrong, next."""

    def decide(self, question: Any, user_answer: str, is_correct: bool) -> Decision:
        if is_correct:
            return Decision(action="next", penalty=0, feedback="")
        return Decision(action="reveal", penalty=1, feedback="")
