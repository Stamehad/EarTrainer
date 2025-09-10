from __future__ import annotations

"""Result schema dataclasses (scaffold)."""

from dataclasses import dataclass, field
from typing import Any, Dict, Optional
from datetime import datetime


@dataclass
class SessionRecord:
    session_id: str
    started_at: datetime
    user: Optional[str]
    drill_id: str
    key: str
    scale: str
    preset: str
    params: Dict[str, Any] = field(default_factory=dict)


@dataclass
class QuestionRecord:
    q_id: str
    session_id: str
    index: int
    drill_id: str
    payload: Dict[str, Any] = field(default_factory=dict)


@dataclass
class AnswerRecord:
    q_id: str
    answer_text: str
    correct: bool
    attempts: int = 1
    penalty: int = 0
    time_ms: Optional[int] = None
