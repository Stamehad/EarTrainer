from __future__ import annotations

from typing import Union

from . import models

import sys
import importlib

_core = sys.modules.get("eartrainer.eartrainer_Cpp._earcore")
if _core is None:  # pragma: no cover - resolution differs between editable/wheel installs
    try:
        _core = importlib.import_module("eartrainer.eartrainer_Cpp._earcore")
    except ModuleNotFoundError:
        try:
            from .. import _earcore as _core  # type: ignore
        except ImportError:
            from . import _earcore as _core  # type: ignore


Next = Union[models.QuestionBundle, models.SessionSummary]


class SessionEngine:
    """Pythonic façade over the C++ SessionEngine exposed via pybind11."""

    def __init__(self) -> None:
        self._engine = _core.SessionEngine()

    def create_session(self, spec: models.SessionSpec) -> str:
        return self._engine.create_session(spec.to_json())

    def next_question(self, session_id: str) -> Next:
        payload = self._engine.next_question(session_id)
        if "question" in payload:
            return models.QuestionBundle.from_json(payload)
        return models.SessionSummary.from_json(payload)

    def assist(self, session_id: str, question_id: str, kind: str) -> models.AssistBundle:
        payload = self._engine.assist(session_id, question_id, kind)
        return models.AssistBundle.from_json(payload)

    def session_assist(self, session_id: str, kind: str) -> models.AssistBundle:
        payload = self._engine.session_assist(session_id, kind)
        return models.AssistBundle.from_json(payload)

    def submit_result(self, session_id: str, report: models.ResultReport) -> Next:
        payload = self._engine.submit_result(session_id, report.to_json())
        if "question" in payload:
            return models.QuestionBundle.from_json(payload)
        return models.SessionSummary.from_json(payload)

    def session_key(self, session_id: str) -> str:
        return str(self._engine.session_key(session_id))

    def orientation_prompt(self, session_id: str) -> models.Prompt:
        payload = self._engine.orientation_prompt(session_id)
        return models.Prompt.from_json(payload)

    def end_session(self, session_id: str) -> models.MemoryPackage:
        payload = self._engine.end_session(session_id)
        return models.MemoryPackage.from_json(payload)

    def capabilities(self) -> dict:
        return dict(self._engine.capabilities())

    def debug_state(self, session_id: str) -> dict:
        return dict(self._engine.debug_state(session_id))

    def adaptive_diagnostics(self, session_id: str) -> dict:
        return dict(self._engine.adaptive_diagnostics(session_id))


__all__ = ["SessionEngine", "Next"]
