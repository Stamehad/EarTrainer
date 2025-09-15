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
from .explain import trace as xtrace
from pathlib import Path
from uuid import uuid4
from storage.schema import SessionDegreeRow
from storage.store import init_store as storage_init_store, validate_records as storage_validate_records, append_session_degree_stats as storage_append


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
        xtrace("session_started", {"drill": drill_id, "preset": preset, "key": key, "scale": scale_type})

    def preview_params(self) -> Dict[str, Any]:
        assert self.ctx is not None
        return dict(self.ctx.params)

    def run(self, ui: Dict[str, Callable[[str], Any]], *, skip_reference: bool = False) -> Dict[str, Any]:
        assert self.ctx is not None and self._drill is not None
        # Minimal run loop: duck drone if configured, play reference and delegate loop
        duck = ui.get("duck_drone", lambda *_a, **_k: None)
        unduck = ui.get("unduck_drone", lambda *_a, **_k: None)
        if not skip_reference:
            duck()
            self._drill.play_reference()
            unduck()
        # Small delay between cadence and first question
        try:
            delay_ms = int(self.ctx.params.get("test_note_delay_ms", 300))
        except Exception:
            delay_ms = 300
        self.synth.sleep_ms(delay_ms)
        num_q = int(self.ctx.params.get("questions", 10))
        result = self._drill.run(num_q, ui)
        self.state.ended_at = datetime.utcnow()
        summary = {
            "total": result.total,
            "correct": result.correct,
            "per_degree": result.per_degree,
            "started_at": self.ctx.started_at.isoformat(),
            "ended_at": self.state.ended_at.isoformat() if self.state.ended_at else None,
        }
        xtrace("session_ended", summary)
        # Persist results to JSON if configured
        try:
            # Write Parquet session-degree rows for this drill unless disabled (set mode aggregates separately)
            if not bool(self.ctx.params.get("stats_disable", False)):
                # Map drill id to category label
                drill = self.ctx.drill_id
                category = "single_note" if drill == "note" else ("chord" if drill in ("chord", "chord_relative") else drill)
                sid = str(uuid4())
                rows: list[SessionDegreeRow] = []
                per = summary.get("per_degree", {}) or {}
                for d_k, d_st in per.items():
                    asked = int(d_st.get("asked", 0))
                    if asked <= 0:
                        continue
                    corr = int(d_st.get("correct", 0))
                    meta = d_st.get("meta", {}) or {}
                    ac = meta.get("assist_counts", {}) or {}
                    tms = int(meta.get("time_ms_total", 0) or 0)
                    rows.append(
                        SessionDegreeRow(
                            session_id=sid,
                            session_start=self.ctx.started_at,
                            scale=self.ctx.scale_type,
                            category=category,
                            degree=int(d_k) if str(d_k).isdigit() else 0,
                            Q=asked,
                            C=corr,
                            T_ms=tms,
                            assist_c=int(ac.get("c", 0)),
                            assist_s=int(ac.get("s", 0)),
                            assist_r=int(ac.get("r", 0)),
                            assist_p=int(ac.get("p", 0)),
                            assist_t=int(ac.get("t", 0)),
                        )
                    )
                if rows:
                    storage_init_store(Path("storage/data"))
                    df_rows = storage_validate_records(rows)
                    storage_append(df_rows, Path("storage/data"))
        except Exception:
            pass
        return summary

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
