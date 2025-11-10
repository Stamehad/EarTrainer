from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional, Union


@dataclass
class SessionSpec:
    version: str = "v1"
    drill_kind: str = "note"
    key: str = "C major"
    range: List[int] = field(default_factory=lambda: [48, 72])
    tempo_bpm: Optional[int] = None
    n_questions: int = 10
    generation: str = "adaptive"
    feedback_policy: Dict[str, Any] = field(default_factory=dict)
    assistance_policy: Dict[str, int] = field(default_factory=dict)
    sampler_params: Dict[str, Any] = field(default_factory=dict)
    params: Dict[str, Any] = field(default_factory=dict)
    seed: int = 1
    adaptive: bool = False
    track_levels: List[int] = field(default_factory=list)
    mode: str = "manual"
    level_inspect: bool = False
    inspect_level: Optional[int] = None
    inspect_tier: Optional[int] = None

    def to_json(self) -> Dict[str, Any]:
        data = asdict(self)
        data["range"] = list(self.range)
        data["tempo_bpm"] = self.tempo_bpm
        data["sampler_params"] = dict(self.sampler_params)
        data["params"] = dict(self.params)
        data["track_levels"] = list(self.track_levels)
        return data

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "SessionSpec":
        return cls(
            version=data.get("version", "v1"),
            drill_kind=data.get("drill_kind", "note"),
            key=data.get("key", "C major"),
            range=list(data.get("range", [48, 72])),
            tempo_bpm=data.get("tempo_bpm"),
            n_questions=data.get("n_questions", 10),
            generation=data.get("generation", "adaptive"),
            feedback_policy=dict(data.get("feedback_policy", {})),
            assistance_policy=dict(data.get("assistance_policy", {})),
            sampler_params=dict(data.get("sampler_params", {})),
            params=dict(data.get("params", {})),
            seed=int(data.get("seed", 1)),
            adaptive=bool(data.get("adaptive", False)),
            track_levels=list(data.get("track_levels", [])),
            mode=data.get("mode", "manual"),
            level_inspect=bool(data.get("level_inspect", False)),
            inspect_level=data.get("inspect_level"),
            inspect_tier=data.get("inspect_tier"),
        )


@dataclass
class LevelCatalogEntry:
    level: int
    tier: int
    label: str

    def to_json(self) -> Dict[str, Any]:
        return {"level": self.level, "tier": self.tier, "label": self.label}

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "LevelCatalogEntry":
        return cls(
            level=int(data.get("level", 0)),
            tier=int(data.get("tier", 0)),
            label=str(data.get("label", "")),
        )


@dataclass
class TypedPayload:
    type: str
    payload: Dict[str, Any]

    def to_json(self) -> Dict[str, Any]:
        return {"type": self.type, "payload": self.payload}

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "TypedPayload":
        return cls(type=data["type"], payload=dict(data.get("payload", {})))


@dataclass
class MidiEvent:
    t: int
    type: str
    note: Optional[int] = None
    vel: Optional[int] = None
    control: Optional[int] = None
    value: Optional[int] = None

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "MidiEvent":
        return cls(
            t=int(data["t"]),
            type=str(data["type"]),
            note=data.get("note"),
            vel=data.get("vel"),
            control=data.get("control"),
            value=data.get("value"),
        )

    def to_json(self) -> Dict[str, Any]:
        payload: Dict[str, Any] = {"t": self.t, "type": self.type}
        if self.note is not None:
            payload["note"] = self.note
        if self.vel is not None:
            payload["vel"] = self.vel
        if self.control is not None:
            payload["control"] = self.control
        if self.value is not None:
            payload["value"] = self.value
        return payload


@dataclass
class MidiTrack:
    name: str
    channel: int
    program: int
    events: List[MidiEvent]

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "MidiTrack":
        return cls(
            name=str(data.get("name", "track")),
            channel=int(data.get("channel", 0)),
            program=int(data.get("program", 0)),
            events=[MidiEvent.from_json(ev) for ev in data.get("events", [])],
        )

    def to_json(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "channel": self.channel,
            "program": self.program,
            "events": [event.to_json() for event in self.events],
        }


@dataclass
class MidiClip:
    ppq: int
    tempo_bpm: int
    length_ticks: int
    tracks: List[MidiTrack]
    format: str = "midi-clip/v1"

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "MidiClip":
        return cls(
            ppq=int(data.get("ppq", 480)),
            tempo_bpm=int(data.get("tempo_bpm", 90)),
            length_ticks=int(data.get("length_ticks", 0)),
            tracks=[MidiTrack.from_json(t) for t in data.get("tracks", [])],
            format=str(data.get("format", "midi-clip/v1")),
        )

    def to_json(self) -> Dict[str, Any]:
        return {
            "format": self.format,
            "ppq": self.ppq,
            "tempo_bpm": self.tempo_bpm,
            "length_ticks": self.length_ticks,
            "tracks": [track.to_json() for track in self.tracks],
        }


@dataclass
class ChordAnswer:
    root_degrees: List[int]
    bass_deg: List[Optional[int]]
    top_deg: List[Optional[int]]
    expect_root: List[bool]
    expect_bass: List[bool]
    expect_top: List[bool]

    @property
    def root_degree(self) -> int:
        return self.root_degrees[0] if self.root_degrees else 0

    def to_json(self) -> Dict[str, Any]:
        def opt_array(values: List[Optional[int]]) -> List[Optional[int]]:
            return [v if v is not None else None for v in values]

        return {
            "type": "chord",
            "root_degrees": list(self.root_degrees),
            "bass_deg": opt_array(self.bass_deg),
            "top_deg": opt_array(self.top_deg),
            "expect_root": list(self.expect_root),
            "expect_bass": list(self.expect_bass),
            "expect_top": list(self.expect_top),
            "root_degree": self.root_degree,
        }

    @classmethod
    def single(cls, root_degree: int, bass_deg: Optional[int] = None, top_deg: Optional[int] = None) -> "ChordAnswer":
        return cls(
            root_degrees=[root_degree],
            bass_deg=[bass_deg],
            top_deg=[top_deg],
            expect_root=[True],
            expect_bass=[bass_deg is not None],
            expect_top=[top_deg is not None],
        )

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "ChordAnswer":
        def parse_int_list(key: str) -> List[int]:
            values = data.get(key)
            if isinstance(values, list):
                return [int(v) for v in values]
            if key == "root_degrees" and "root_degree" in data:
                return [int(data.get("root_degree", 0))]
            return []

        def parse_optional_list(key: str) -> List[Optional[int]]:
            values = data.get(key)
            if isinstance(values, list):
                return [int(v) if v is not None else None for v in values]
            if key in {"bass_deg", "top_deg"} and key in data:
                return [data.get(key)]
            return []

        def parse_bool_list(key: str, default: bool) -> List[bool]:
            values = data.get(key)
            if isinstance(values, list):
                return [bool(v) for v in values]
            return []

        root_degrees = parse_int_list("root_degrees")
        if not root_degrees:
            root_degrees = [int(data.get("root_degree", 0))]
        count = len(root_degrees)

        def pad_list(values: List[Any], pad_value: Any) -> List[Any]:
            if len(values) < count:
                values = list(values) + [pad_value] * (count - len(values))
            return values

        bass = pad_list(parse_optional_list("bass_deg"), None)
        top = pad_list(parse_optional_list("top_deg"), None)
        expect_root = pad_list(parse_bool_list("expect_root", True) or [True], True)
        expect_bass = pad_list(parse_bool_list("expect_bass", False) or [False], False)
        expect_top = pad_list(parse_bool_list("expect_top", True) or [True], True)

        return cls(
            root_degrees=root_degrees,
            bass_deg=bass,
            top_deg=top,
            expect_root=expect_root,
            expect_bass=expect_bass,
            expect_top=expect_top,
        )


@dataclass
class MelodyAnswer:
    melody: List[int]

    def to_json(self) -> Dict[str, Any]:
        return {"type": "melody", "melody": list(self.melody)}

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "MelodyAnswer":
        return cls(melody=[int(v) for v in data.get("melody", [])])


@dataclass
class HarmonyAnswer:
    notes: List[int]

    def to_json(self) -> Dict[str, Any]:
        return {"type": "harmony", "notes": list(self.notes)}

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "HarmonyAnswer":
        return cls(notes=[int(v) for v in data.get("notes", [])])


AnswerPayload = Union[ChordAnswer, MelodyAnswer, HarmonyAnswer]


def answer_from_json(data: Dict[str, Any]) -> AnswerPayload:
    answer_type = data.get("type")
    if answer_type == "chord":
        return ChordAnswer.from_json(data)
    if answer_type == "melody":
        return MelodyAnswer.from_json(data)
    if answer_type == "harmony":
        return HarmonyAnswer.from_json(data)
    raise ValueError(f"Unsupported answer payload type: {answer_type}")


def answer_to_json(answer: AnswerPayload) -> Dict[str, Any]:
    return answer.to_json()


@dataclass
class QuestionBundle:
    question_id: str
    question: Dict[str, Any]
    correct_answer: AnswerPayload
    prompt_clip: Optional[MidiClip]
    ui_hints: Dict[str, Any]

    def to_json(self) -> Dict[str, Any]:
        return {
            "question_id": self.question_id,
            "question": dict(self.question),
            "correct_answer": answer_to_json(self.correct_answer),
            "prompt_clip": self.prompt_clip.to_json() if self.prompt_clip else None,
            "ui_hints": dict(self.ui_hints),
        }

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "QuestionBundle":
        prompt_clip = data.get("prompt_clip")
        raw_hints = data.get("ui_hints")
        hints = dict(raw_hints) if isinstance(raw_hints, dict) else {}
        return cls(
            question_id=data["question_id"],
            question=dict(data.get("question", {})) if isinstance(data.get("question"), dict) else {},
            correct_answer=answer_from_json(dict(data.get("correct_answer", {}))),
            prompt_clip=MidiClip.from_json(prompt_clip) if isinstance(prompt_clip, dict) else None,
            ui_hints=hints,
        )


@dataclass
class AssistBundle:
    question_id: str
    kind: str
    prompt_clip: Optional[MidiClip]

    def to_json(self) -> Dict[str, Any]:
        return {
            "question_id": self.question_id,
            "kind": self.kind,
            "prompt_clip": self.prompt_clip.to_json() if self.prompt_clip else None,
        }

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "AssistBundle":
        clip = data.get("prompt_clip") if isinstance(data, dict) else None
        return cls(
            question_id=data["question_id"],
            kind=data["kind"],
            prompt_clip=MidiClip.from_json(clip) if isinstance(clip, dict) else None,
        )


@dataclass
class ResultMetrics:
    rt_ms: int
    attempts: int
    question_count: int = 1
    assists_used: Dict[str, int] = field(default_factory=dict)
    first_input_rt_ms: Optional[int] = None

    def to_json(self) -> Dict[str, Any]:
        return {
            "rt_ms": self.rt_ms,
            "attempts": self.attempts,
            "question_count": self.question_count,
            "assists_used": dict(self.assists_used),
            "first_input_rt_ms": self.first_input_rt_ms,
        }


@dataclass
class ResultAttempt:
    label: str
    correct: bool
    attempts: int = 0
    answer_fragment: Optional[TypedPayload] = None
    expected_fragment: Optional[TypedPayload] = None

    def to_json(self) -> Dict[str, Any]:
        data: Dict[str, Any] = {
            "label": self.label,
            "correct": self.correct,
            "attempts": self.attempts,
        }
        data["answer_fragment"] = (
            self.answer_fragment.to_json() if self.answer_fragment else None
        )
        data["expected_fragment"] = (
            self.expected_fragment.to_json() if self.expected_fragment else None
        )
        return data

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "ResultAttempt":
        answer = data.get("answer_fragment")
        expected = data.get("expected_fragment")
        return cls(
            label=str(data.get("label", "")),
            correct=bool(data.get("correct", False)),
            attempts=int(data.get("attempts", 0)),
            answer_fragment=TypedPayload.from_json(answer) if isinstance(answer, dict) else None,
            expected_fragment=TypedPayload.from_json(expected) if isinstance(expected, dict) else None,
        )


@dataclass
class ResultReport:
    question_id: str
    final_answer: AnswerPayload
    correct: bool
    metrics: ResultMetrics
    attempts: List[ResultAttempt] = field(default_factory=list)
    client_info: Dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> Dict[str, Any]:
        data = {
            "question_id": self.question_id,
            "final_answer": answer_to_json(self.final_answer),
            "correct": self.correct,
            "metrics": self.metrics.to_json(),
            "client_info": dict(self.client_info),
            "attempts": [attempt.to_json() for attempt in self.attempts],
        }
        return data

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "ResultReport":
        metrics = ResultMetrics(
            rt_ms=int(data.get("metrics", {}).get("rt_ms", 0)),
            attempts=int(data.get("metrics", {}).get("attempts", 0)),
            question_count=int(data.get("metrics", {}).get("question_count", 1)),
            assists_used=dict(data.get("metrics", {}).get("assists_used", {})),
            first_input_rt_ms=data.get("metrics", {}).get("first_input_rt_ms"),
        )
        attempts_payload = data.get("attempts", [])
        attempts = [
            ResultAttempt.from_json(entry) for entry in attempts_payload
            if isinstance(entry, dict)
        ]
        return cls(
            question_id=data["question_id"],
            final_answer=answer_from_json(dict(data.get("final_answer", {}))),
            correct=bool(data.get("correct", False)),
            metrics=metrics,
            attempts=attempts,
            client_info=dict(data.get("client_info", {})),
        )


@dataclass
class SessionSummary:
    session_id: str
    totals: Dict[str, Any]
    by_category: List[Dict[str, Any]]
    results: List[Dict[str, Any]]

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "SessionSummary":
        return cls(
            session_id=data["session_id"],
            totals=dict(data.get("totals", {})),
            by_category=list(data.get("by_category", [])),
            results=list(data.get("results", [])),
        )


@dataclass
class AdaptiveDrillMemory:
    family: str
    ema_score: Optional[float]

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "AdaptiveDrillMemory":
        return cls(
            family=str(data.get("family", "")),
            ema_score=data.get("ema_score"),
        )


@dataclass
class AdaptiveLevelProposal:
    track_index: int
    track_name: str
    current_level: int
    suggested_level: Optional[int]

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "AdaptiveLevelProposal":
        return cls(
            track_index=int(data.get("track_index", -1)),
            track_name=str(data.get("track_name", "")),
            current_level=int(data.get("current_level", 0)),
            suggested_level=data.get("suggested_level"),
        )


@dataclass
class AdaptiveMemory:
    has_score: bool
    bout_average: float
    graduate_threshold: float
    level_up: bool
    drills: Dict[str, AdaptiveDrillMemory] = field(default_factory=dict)
    level: Optional[AdaptiveLevelProposal] = None

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "AdaptiveMemory":
        drills_data = data.get("drills", {})
        drills = {
            key: AdaptiveDrillMemory.from_json(value if isinstance(value, dict) else {})
            for key, value in drills_data.items()
        }
        level_data = data.get("level")
        level = None
        if isinstance(level_data, dict):
            level = AdaptiveLevelProposal.from_json(level_data)
        return cls(
            has_score=bool(data.get("has_score", False)),
            bout_average=float(data.get("bout_average", 0.0)),
            graduate_threshold=float(data.get("graduate_threshold", 0.0)),
            level_up=bool(data.get("level_up", False)),
            drills=drills,
            level=level,
        )


@dataclass
class MemoryPackage:
    summary: SessionSummary
    adaptive: Optional[AdaptiveMemory] = None

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "MemoryPackage":
        summary = SessionSummary.from_json(data.get("summary", {}))
        adaptive_data = data.get("adaptive")
        adaptive = AdaptiveMemory.from_json(adaptive_data) if isinstance(adaptive_data, dict) else None
        return cls(summary=summary, adaptive=adaptive)
