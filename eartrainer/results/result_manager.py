from __future__ import annotations

"""Results Manager (scaffold).

This initial version provides an in-memory/JSON-like interface. Swap
to SQLite later without changing call sites.
"""

from typing import Any, Dict, List, Optional
from uuid import uuid4
from datetime import datetime

from .schema import SessionRecord, QuestionRecord, AnswerRecord


class ResultManager:
    def __init__(self) -> None:
        self._sessions: Dict[str, SessionRecord] = {}
        self._questions: Dict[str, QuestionRecord] = {}
        self._answers: Dict[str, AnswerRecord] = {}

    def start_session(self, ctx: Dict[str, Any]) -> str:
        session_id = str(uuid4())
        rec = SessionRecord(
            session_id=session_id,
            started_at=datetime.utcnow(),
            user=ctx.get("user"),
            drill_id=ctx["drill_id"],
            key=ctx["key"],
            scale=ctx["scale_type"],
            preset=ctx.get("preset", "default"),
            params=ctx.get("params", {}),
        )
        self._sessions[session_id] = rec
        return session_id

    def record(self, session_id: str, index: int, drill_id: str, question: Dict[str, Any], answer: Dict[str, Any]) -> None:
        q_id = question.get("id") or f"{session_id}:{index}"
        self._questions[q_id] = QuestionRecord(q_id=q_id, session_id=session_id, index=index, drill_id=drill_id, payload=question)
        self._answers[q_id] = AnswerRecord(
            q_id=q_id,
            answer_text=str(answer.get("answer")),
            correct=bool(answer.get("correct", False)),
            attempts=int(answer.get("attempts", 1)),
            penalty=int(answer.get("penalty", 0)),
            time_ms=answer.get("time_ms"),
        )

    def end_session(self, session_id: str) -> None:
        # Placeholder for persistence/flush
        return

    def summarize(self, session_id: str) -> Dict[str, Any]:
        # Minimal aggregate: total/correct
        total = 0
        correct = 0
        for qid, ans in self._answers.items():
            if qid.startswith(session_id + ":"):
                total += 1
                if ans.correct:
                    correct += 1
        return {"session_id": session_id, "total": total, "correct": correct}
