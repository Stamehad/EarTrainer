from __future__ import annotations

"""Session Manager: orchestrates drills, policies, and persistence.

This is an initial scaffold with a minimal run loop compatible with the
existing NoteDegreeDrill. It is CLI-agnostic and front-end ready.
"""

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, Optional
from datetime import datetime

from ..audio.synthesis import Synth
from ..audio.drone import DroneManager
from ..theory.scale import Scale
from .drill_registry import list_drills, get_drill, make_drill


@dataclass(frozen=True)
class SessionContext:
    user_id: Optional[str]
    started_at: datetime
    key: str
    scale_type: str
    drill_id: str
    preset: str
    params: Dict[str, Any]


@dataclass
class RuntimeState:
    index: int = 0
    ended_at: Optional[datetime] = None


class SessionManager:
    def __init__(self, cfg: Dict[str, Any], synth: Synth, drone: Optional[DroneManager] = None) -> None:
        self.cfg = cfg
        self.synth = synth
        self.drone = drone
        self.ctx: Optional[SessionContext] = None
        self.state = RuntimeState()
        self._drill = None
        self._scale: Optional[Scale] = None

    def start_session(self, drill_id: str, preset: str, overrides: Dict[str, Any]) -> None:
        # Resolve params: defaults.yml (drill) → preset → CLI overrides
        dm = get_drill(drill_id)
        preset_params = dict(dm.presets.get(preset, {}))
        params = {**preset_params, **(overrides or {})}

        # Key/scale from overrides or config
        key = overrides.get("key") or self.cfg.get("context", {}).get("key", "C")
        scale_type = overrides.get("scale_type") or self.cfg.get("context", {}).get("scale_type", "major")

        self._scale = Scale(key, scale_type)
        self.ctx = SessionContext(
            user_id=None,
            started_at=datetime.utcnow(),
            key=key,
            scale_type=scale_type,
            drill_id=drill_id,
            preset=preset,
            params=params,
        )

        # Construct drill
        self._drill = make_drill(
            drill_id,
            scale={"key": key, "scale_type": scale_type},
            synth=self.synth,
            drone=self.drone,
            params=params,
            mistake_manager=None,
            results_sink=None,
        )

    def preview_params(self) -> Dict[str, Any]:
        assert self.ctx is not None
        return dict(self.ctx.params)

    def run(self, ui: Dict[str, Callable[[str], Any]]) -> Dict[str, Any]:
        assert self.ctx is not None and self._drill is not None
        # Minimal run loop: play reference and delegate question loop to existing drill
        self._drill.play_reference()
        num_q = int(self.ctx.params.get("questions", 10))
        result = self._drill.run(num_q, ui)
        self.state.ended_at = datetime.utcnow()
        return {
            "total": result.total,
            "correct": result.correct,
            "per_degree": result.per_degree,
            "started_at": self.ctx.started_at.isoformat(),
            "ended_at": self.state.ended_at.isoformat() if self.state.ended_at else None,
        }

    def stop(self) -> None:
        self.state.ended_at = datetime.utcnow()

    def change_key(self, new_key: str) -> None:
        assert self.ctx is not None
        self.ctx = SessionContext(
            user_id=self.ctx.user_id,
            started_at=self.ctx.started_at,
            key=new_key,
            scale_type=self.ctx.scale_type,
            drill_id=self.ctx.drill_id,
            preset=self.ctx.preset,
            params=self.ctx.params,
        )
        self._scale = Scale(new_key, self.ctx.scale_type)
