from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional


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
    seed: int = 1

    def to_json(self) -> Dict[str, Any]:
        data = asdict(self)
        data["range"] = list(self.range)
        data["tempo_bpm"] = self.tempo_bpm
        data["sampler_params"] = dict(self.sampler_params)
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
            seed=int(data.get("seed", 1)),
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


@dataclass
class Prompt:
    modality: str
    midi_clip: Optional[MidiClip] = None

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "Prompt":
        modality = str(data.get("modality", ""))
        clip = None
        if modality == "midi-clip":
            clip = MidiClip.from_json(data.get("midi_clip", {}))
        return cls(modality=modality, midi_clip=clip)


@dataclass
class QuestionBundle:
    question_id: str
    question: TypedPayload
    correct_answer: TypedPayload
    prompt: Optional[Prompt]
    ui_hints: Dict[str, Any]

    def to_json(self) -> Dict[str, Any]:
        return {
            "question_id": self.question_id,
            "question": self.question.to_json(),
            "correct_answer": self.correct_answer.to_json(),
            "prompt": self.prompt.to_json() if self.prompt else None,
            "ui_hints": dict(self.ui_hints),
        }

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "QuestionBundle":
        prompt_data = data.get("prompt")
        prompt = Prompt.from_json(prompt_data) if isinstance(prompt_data, dict) else None
        return cls(
            question_id=data["question_id"],
            question=TypedPayload.from_json(data["question"]),
            correct_answer=TypedPayload.from_json(data["correct_answer"]),
            prompt=prompt,
            ui_hints=dict(data.get("ui_hints", {})),
        )


@dataclass
class AssistBundle:
    question_id: str
    kind: str
    prompt: Optional[Prompt]
    ui_delta: Dict[str, Any]

    @classmethod
    def from_json(cls, data: Dict[str, Any]) -> "AssistBundle":
        prompt = data.get("prompt")
        return cls(
            question_id=data["question_id"],
            kind=data["kind"],
            prompt=Prompt.from_json(prompt) if isinstance(prompt, dict) else None,
            ui_delta=dict(data.get("ui_delta", {})),
        )


@dataclass
class ResultMetrics:
    rt_ms: int
    attempts: int
    assists_used: Dict[str, int] = field(default_factory=dict)
    first_input_rt_ms: Optional[int] = None

    def to_json(self) -> Dict[str, Any]:
        return {
            "rt_ms": self.rt_ms,
            "attempts": self.attempts,
            "assists_used": dict(self.assists_used),
            "first_input_rt_ms": self.first_input_rt_ms,
        }


@dataclass
class ResultReport:
    question_id: str
    final_answer: TypedPayload
    correct: bool
    metrics: ResultMetrics
    client_info: Dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> Dict[str, Any]:
        data = {
            "question_id": self.question_id,
            "final_answer": self.final_answer.to_json(),
            "correct": self.correct,
            "metrics": self.metrics.to_json(),
            "client_info": dict(self.client_info),
        }
        return data


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
