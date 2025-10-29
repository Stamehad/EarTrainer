from __future__ import annotations

"""Minimal Tk GUI powered by the C++ SessionEngine."""

import random
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from tkinter import messagebox, ttk
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

try:
    from eartrainer.eartrainer_Cpp.python.eartrainer.models import (
        MidiClip,
        QuestionBundle,
        ResultMetrics,
        ResultReport,
        SessionSpec,
        SessionSummary,
        TypedPayload,
    )
    from eartrainer.eartrainer_Cpp.python.eartrainer.midi import SimpleMidiPlayer
    from eartrainer.eartrainer_Cpp.python.eartrainer.session_engine import SessionEngine
except Exception as exc:  # pragma: no cover - handled at runtime
    SessionEngine = None  # type: ignore
    SimpleMidiPlayer = None  # type: ignore
    _IMPORT_ERROR = exc
else:  # pragma: no cover - used at runtime
    _IMPORT_ERROR = None

from .training_sets import list_sets, get_set

KEYS = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
SCALES = ["major", "natural_minor"]
SUPPORTED_DRILLS = ["note"]


@dataclass
class SessionStep:
    title: str
    spec: SessionSpec


class EarTrainerGUI(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("EarTrainer (C++ SessionEngine)")
        self.geometry("640x520")
        self.resizable(False, False)

        if SessionEngine is None:
            raise RuntimeError(
                f"SessionEngine bindings are unavailable: {_IMPORT_ERROR!r}"
            )

        self.engine = SessionEngine()
        self.player: Optional[SimpleMidiPlayer]
        self.player = self._init_player()

        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self.controller = SessionController(self, self.engine, self.player)

        self.opening_view = OpeningView(self, self.controller)
        self.session_view = SessionView(self, self.controller)

        self.opening_view.pack(fill=tk.BOTH, expand=True)

    # ------------------------------------------------------------------
    def show_opening(self) -> None:
        self.session_view.pack_forget()
        self.opening_view.reset()
        self.opening_view.pack(fill=tk.BOTH, expand=True)

    def show_session(self) -> None:
        self.opening_view.pack_forget()
        self.session_view.reset()
        self.session_view.pack(fill=tk.BOTH, expand=True)

    def _init_player(self) -> Optional[SimpleMidiPlayer]:
        if SimpleMidiPlayer is None:
            messagebox.showwarning(
                "Audio unavailable",
                "pyfluidsynth/FluidSynth not available. Prompts will not play.",
            )
            return None
        try:
            return SimpleMidiPlayer(soundfont_path=str(self._detect_soundfont()))
        except Exception as exc:
            messagebox.showwarning(
                "Audio unavailable",
                f"Unable to initialise FluidSynth player: {exc}.\n"
                "Prompt playback will be disabled.",
            )
            return None

    def _detect_soundfont(self) -> Path:
        repo_root = Path(__file__).resolve().parents[3]
        candidate = repo_root / "soundfonts" / "GrandPiano.sf2"
        if candidate.exists():
            return candidate
        raise FileNotFoundError(
            f"Expected soundfont at {candidate}. Provide GrandPiano.sf2 to enable audio."
        )

    def _on_close(self) -> None:
        self.controller.stop()
        if self.player is not None:
            try:
                self.player.close()
            except Exception:
                pass
        self.destroy()


class OpeningView(ttk.Frame):
    def __init__(self, master: EarTrainerGUI, controller: "SessionController") -> None:
        super().__init__(master)
        self.controller = controller

        self.mode_var = tk.StringVar(value="drill")
        self.drill_var = tk.StringVar(value=SUPPORTED_DRILLS[0])
        self.key_var = tk.StringVar(value=KEYS[0])
        self.scale_var = tk.StringVar(value=SCALES[0])
        self.question_var = tk.StringVar(value="10")
        self.set_selection = tk.StringVar(value="")
        self.rel_down_var = tk.IntVar(value=1)
        self.rel_up_var = tk.IntVar(value=1)

        self._available_sets = self._load_note_sets()
        if self._available_sets:
            self.set_selection.set(self._available_sets[0]["id"])

        self._build()

    def _build(self) -> None:
        pad = {"padx": 10, "pady": 6}

        title = ttk.Label(self, text="EarTrainer Playground", font=("TkDefaultFont", 16, "bold"))
        title.pack(**pad)

        mode_frame = ttk.Frame(self)
        mode_frame.pack(**pad)
        ttk.Label(mode_frame, text="Mode:").grid(row=0, column=0, sticky=tk.W)
        ttk.Radiobutton(mode_frame, text="Drill", variable=self.mode_var, value="drill").grid(row=0, column=1, sticky=tk.W)
        ttk.Radiobutton(mode_frame, text="Training Set", variable=self.mode_var, value="set").grid(row=0, column=2, sticky=tk.W, padx=20)

        form = ttk.Labelframe(self, text="Session Parameters")
        form.pack(fill=tk.X, **pad)

        ttk.Label(form, text="Key").grid(row=0, column=0, sticky=tk.W, padx=6, pady=4)
        ttk.OptionMenu(form, self.key_var, self.key_var.get(), *KEYS).grid(row=0, column=1, sticky=tk.W, pady=4)

        ttk.Label(form, text="Scale").grid(row=0, column=2, sticky=tk.W, padx=6, pady=4)
        ttk.OptionMenu(form, self.scale_var, self.scale_var.get(), *SCALES).grid(row=0, column=3, sticky=tk.W, pady=4)

        # Drill controls
        self.drill_controls = ttk.Frame(form)
        self.drill_controls.grid(row=1, column=0, columnspan=4, sticky=tk.EW, pady=4)
        ttk.Label(self.drill_controls, text="Drill").grid(row=0, column=0, sticky=tk.W, padx=6)
        ttk.OptionMenu(self.drill_controls, self.drill_var, self.drill_var.get(), *SUPPORTED_DRILLS).grid(row=0, column=1, sticky=tk.W)
        ttk.Label(self.drill_controls, text="Questions").grid(row=0, column=2, sticky=tk.W, padx=16)
        ttk.Entry(self.drill_controls, textvariable=self.question_var, width=6).grid(row=0, column=3, sticky=tk.W)
        ttk.Label(self.drill_controls, text="Octaves ↓").grid(row=0, column=4, sticky=tk.W, padx=16)
        ttk.Spinbox(self.drill_controls, from_=0, to=4, textvariable=self.rel_down_var, width=4).grid(row=0, column=5, sticky=tk.W)
        ttk.Label(self.drill_controls, text="Octaves ↑").grid(row=0, column=6, sticky=tk.W, padx=16)
        ttk.Spinbox(self.drill_controls, from_=0, to=4, textvariable=self.rel_up_var, width=4).grid(row=0, column=7, sticky=tk.W)

        # Set controls
        self.set_controls = ttk.Frame(form)
        self.set_controls.grid(row=2, column=0, columnspan=4, sticky=tk.EW, pady=4)
        ttk.Label(self.set_controls, text="Training Set").grid(row=0, column=0, sticky=tk.W, padx=6)
        values = [s["id"] for s in self._available_sets] or ["(none)"]
        self.set_menu = ttk.OptionMenu(self.set_controls, self.set_selection, self.set_selection.get(), *values)
        self.set_menu.grid(row=0, column=1, sticky=tk.W)
        self.set_description = ttk.Label(
            self.set_controls,
            text=self._describe_set(self.set_selection.get()),
            wraplength=520,
            justify=tk.LEFT,
        )
        self.set_description.grid(row=1, column=0, columnspan=4, sticky=tk.W, padx=6, pady=4)

        self.set_selection.trace_add("write", lambda *_: self._update_description())

        start_btn = ttk.Button(self, text="Start", command=self._on_start)
        start_btn.pack(**pad)

    def _update_description(self) -> None:
        self.set_description.config(text=self._describe_set(self.set_selection.get()))

    def _describe_set(self, set_id: str) -> str:
        for entry in self._available_sets:
            if entry["id"] == set_id:
                return entry.get("description", "")
        return ""

    def _load_note_sets(self) -> List[Dict[str, Any]]:
        available = []
        try:
            for set_info in list_sets():
                steps = set_info.get("steps", [])
                note_steps = [s for s in steps if s.get("drill") in SUPPORTED_DRILLS]
                if not note_steps:
                    continue
                entry = dict(set_info)
                entry["steps"] = note_steps
                available.append(entry)
        except Exception:
            pass
        return available

    def _build_steps_from_set(self, set_id: str) -> List[SessionStep]:
        try:
            data = get_set(set_id)
        except KeyError:
            messagebox.showerror("Invalid set", f"Training set '{set_id}' not found")
            return []
        steps = []
        for idx, raw in enumerate(data.get("steps", []), start=1):
            if raw.get("drill") not in SUPPORTED_DRILLS:
                continue
            spec = build_spec(
                drill_kind=raw.get("drill", "note"),
                key=self.key_var.get(),
                scale=self.scale_var.get(),
                questions=int(raw.get("questions", 10)),
                extra_params=_step_params_for_sampler(raw),
                relative_range=(self.rel_down_var.get(), self.rel_up_var.get()),
            )
            steps.append(SessionStep(title=f"{set_id} · Step {idx}", spec=spec))
        if not steps:
            messagebox.showerror("Unsupported set", "Selected set has no compatible note drills.")
        return steps

    def _on_start(self) -> None:
        try:
            questions = max(1, int(self.question_var.get()))
        except ValueError:
            messagebox.showerror("Invalid value", "Questions must be a positive integer")
            return

        key = self.key_var.get()
        scale = self.scale_var.get()

        if self.mode_var.get() == "set":
            set_id = self.set_selection.get()
            if not set_id:
                messagebox.showerror("Select a set", "Choose a training set to continue")
                return
            steps = self._build_steps_from_set(set_id)
        else:
            spec = build_spec(
                drill_kind=self.drill_var.get(),
                key=key,
                scale=scale,
                questions=questions,
                extra_params={},
                relative_range=(self.rel_down_var.get(), self.rel_up_var.get()),
            )
            steps = [SessionStep(title=f"Drill: {spec.drill_kind}", spec=spec)]

        if not steps:
            return
        self.controller.start(steps)

    def reset(self) -> None:
        self.mode_var.set("drill")
        self.drill_var.set(SUPPORTED_DRILLS[0])
        self.question_var.set("10")
        self.rel_down_var.set(1)
        self.rel_up_var.set(1)
        if self._available_sets:
            self.set_selection.set(self._available_sets[0]["id"])
            self._update_description()


class SessionView(ttk.Frame):
    def __init__(self, master: EarTrainerGUI, controller: "SessionController") -> None:
        super().__init__(master)
        self.controller = controller
        self._build()

    def _build(self) -> None:
        pad = {"padx": 10, "pady": 6}

        self.header_var = tk.StringVar(value="")
        self.question_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="")

        ttk.Label(self, textvariable=self.header_var, font=("TkDefaultFont", 14, "bold")).pack(**pad)
        ttk.Label(self, textvariable=self.question_var, font=("TkDefaultFont", 12)).pack(**pad)

        controls = ttk.Frame(self)
        controls.pack(**pad)

        self.answer_entry = ttk.Entry(controls, width=10)
        self.answer_entry.grid(row=0, column=0, padx=4)
        self.answer_entry.bind("<Return>", lambda _e: self.submit_answer())

        ttk.Button(controls, text="Submit", command=self.submit_answer).grid(row=0, column=1, padx=4)
        ttk.Button(controls, text="Play Prompt", command=self.controller.play_prompt).grid(row=0, column=2, padx=12)
        ttk.Button(controls, text="Play Tonic", command=lambda: self.controller.play_reference("Tonic", include_prompt=False)).grid(row=0, column=3, padx=4)
        ttk.Button(controls, text="Stop", command=self._on_stop).grid(row=0, column=4, padx=4)

        ttk.Label(self, textvariable=self.status_var, foreground="#444444", wraplength=600, justify=tk.LEFT).pack(**pad)

        self.log = tk.Text(self, height=16, width=74, state=tk.DISABLED)
        self.log.pack(padx=12, pady=(0, 12))

    def reset(self) -> None:
        self.header_var.set("")
        self.question_var.set("")
        self.status_var.set("")
        self.answer_entry.delete(0, tk.END)
        self.log.configure(state=tk.NORMAL)
        self.log.delete("1.0", tk.END)
        self.log.configure(state=tk.DISABLED)

    def focus_answer(self) -> None:
        self.answer_entry.focus_set()
        self.answer_entry.select_range(0, tk.END)

    def set_header(self, text: str) -> None:
        self.header_var.set(text)

    def show_question(self, bundle: QuestionBundle, index: int, total: int) -> None:
        self.question_var.set(f"Question {index} of {total} — {bundle.question.type}")
        self.status_var.set("Type 1-7 + Enter. Use 'r' to replay the prompt, 't' for tonic assistance.")
        self.answer_entry.delete(0, tk.END)
        self.focus_answer()

    def append_log(self, line: str) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, line + "\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)

    def show_summary(self, summary: SessionSummary, label: str) -> None:
        totals = summary.totals or {}
        corr = totals.get("correct", 0)
        asked = corr + totals.get("incorrect", 0)
        self.append_log(f"[{label}] Summary: {corr}/{asked} correct")

    def submit_answer(self) -> None:
        value = self.answer_entry.get().strip()
        if not value:
            messagebox.showwarning("Answer required", "Type an answer before submitting")
            return
        lowered = value.lower()
        if lowered == "r":
            self.controller.play_prompt()
            self.answer_entry.delete(0, tk.END)
            self.focus_answer()
            return
        if lowered == "t":
            self.controller.play_reference("Tonic", include_prompt=False)
            self.answer_entry.delete(0, tk.END)
            self.focus_answer()
            return
        self.controller.submit_answer(value)

    def _on_stop(self) -> None:
        if messagebox.askyesno("Stop session", "Stop current session and return to the menu?"):
            self.controller.stop()
            self.master.show_opening()


class SessionController:
    def __init__(
        self,
        app: EarTrainerGUI,
        engine: SessionEngine,
        player: Optional[SimpleMidiPlayer],
    ) -> None:
        self.app = app
        self.engine = engine
        self.player = player
        self.steps: List[SessionStep] = []
        self.current_step_index = -1
        self.session_id: Optional[str] = None
        self.current_bundle: Optional[QuestionBundle] = None
        self.question_started_at: float = time.time()
        self.audio_thread: Optional[threading.Thread] = None
        self.current_total = 0
        self.reference_pending = False
        self.session_assists: Dict[str, MidiClip] = {}

    # Session lifecycle -------------------------------------------------
    def start(self, steps: List[SessionStep]) -> None:
        self.steps = steps
        self.current_step_index = -1
        self.app.show_session()
        self.app.session_view.set_header("Starting session…")
        self.reference_pending = False
        self._advance_step()

    def stop(self) -> None:
        self.steps = []
        self.current_step_index = -1
        self.session_id = None
        self.current_bundle = None
        self.current_total = 0
        self.reference_pending = False
        self.session_assists.clear()

    def _advance_step(self) -> None:
        self.current_step_index += 1
        if self.current_step_index >= len(self.steps):
            messagebox.showinfo("Session complete", "All steps finished. Returning to menu.")
            self.app.show_opening()
            return
        step = self.steps[self.current_step_index]
        try:
            self.session_id = self.engine.create_session(step.spec)
        except Exception as exc:
            messagebox.showerror("Session error", f"Failed to create session: {exc}")
            self.app.show_opening()
            return
        self._load_session_assists()
        self.current_total = int(step.spec.n_questions)
        self.app.session_view.set_header(step.title)
        self.app.session_view.append_log(f"--- {step.title} ---")
        self.reference_pending = True
        self._next_question()

    def _next_question(self) -> None:
        if not self.session_id:
            return
        try:
            payload = self.engine.next_question(self.session_id)
        except Exception as exc:
            messagebox.showerror("Session error", f"Failed to fetch next question: {exc}")
            self.app.show_opening()
            return
        self._handle_engine_payload(payload)

    # Audio -------------------------------------------------------------
    def play_prompt(self) -> None:
        if self.player is None or self.current_bundle is None:
            return
        prompt_clip = self.current_bundle.prompt_clip

        def worker() -> None:
            try:
                self.player.play_prompt(prompt_clip)
            except Exception:
                pass

        self._start_audio_thread(worker)

    def play_reference(self, kind: str = "ScaleArpeggio", *, include_prompt: bool = False) -> None:
        if self.player is None:
            if include_prompt:
                self.play_prompt()
            return

        clip = self._get_session_assist_clip(kind)
        if clip is None:
            if include_prompt:
                self.play_prompt()
            return

        def worker() -> None:
            try:
                self.player.play_prompt(clip)
                if include_prompt and self.current_bundle is not None:
                    self.player.play_prompt(self.current_bundle.prompt_clip)
            except Exception:
                pass

        self._start_audio_thread(worker)

    # Answer submission -------------------------------------------------
    def submit_answer(self, answer: str) -> None:
        if self.session_id is None or self.current_bundle is None:
            return
        bundle = self.current_bundle
        expected_payload = bundle.correct_answer.payload or {}
        if expected_payload:
            answer_key = next(iter(expected_payload.keys()))
            expected_value = expected_payload[answer_key]
        else:
            answer_key = "value"
            expected_value = ""

        if answer not in {"1", "2", "3", "4", "5", "6", "7"}:
            messagebox.showwarning("Invalid answer", "Enter a scale degree between 1 and 7.")
            return

        display_answer = answer.strip()
        coerced_answer = _coerce_answer(answer, expected_value)

        is_degree_answer = (
            answer_key in {"degree", "scale_degree"}
            and isinstance(expected_value, int)
            and 0 <= expected_value <= 6
        )

        if is_degree_answer and isinstance(coerced_answer, int):
            coerced_answer = max(coerced_answer - 1, 0)

        is_correct = str(coerced_answer).strip().lower() == str(expected_value).strip().lower()
        elapsed_ms = int((time.time() - self.question_started_at) * 1000)

        report = ResultReport(
            question_id=bundle.question_id,
            final_answer=TypedPayload(
                type=bundle.correct_answer.type,
                payload={answer_key: coerced_answer},
            ),
            correct=is_correct,
            metrics=ResultMetrics(
                rt_ms=max(elapsed_ms, 1),
                attempts=1,
                assists_used={},
                first_input_rt_ms=max(elapsed_ms, 1),
            ),
            client_info={"source": "gui"},
        )

        expected_display_value = (
            expected_value + 1 if is_degree_answer else expected_value
        )
        if isinstance(expected_display_value, (int, float, str)):
            expected_display = f"'{expected_display_value}'"
        else:
            expected_display = str(expected_display_value)
        if is_correct:
            log_line = f"{bundle.question_id}: answered '{display_answer}' ✅"
        else:
            log_line = f"{bundle.question_id}: answered '{display_answer}' ❌ ({expected_display})"
        self.app.session_view.append_log(log_line)

        try:
            next_payload = self.engine.submit_result(self.session_id, report)
        except Exception as exc:
            messagebox.showerror("Submission error", f"Failed to submit answer: {exc}")
            self.app.show_opening()
            return

        if not is_correct:
            corrected = ResultReport(
                question_id=bundle.question_id,
                final_answer=TypedPayload(
                    type=bundle.correct_answer.type,
                    payload=dict(expected_payload),
                ),
                correct=True,
                metrics=ResultMetrics(
                    rt_ms=max(elapsed_ms, 1),
                    attempts=1,
                    assists_used={},
                    first_input_rt_ms=max(elapsed_ms, 1),
                ),
                client_info={"source": "gui"},
            )
            try:
                next_payload = self.engine.submit_result(self.session_id, corrected)
            except Exception as exc:
                messagebox.showerror("Submission error", f"Failed to advance after reveal: {exc}")
                self.app.show_opening()
                return

        self._advance_from_payload(bundle, next_payload)

    def _advance_from_payload(
        self,
        previous: QuestionBundle,
        payload: QuestionBundle | SessionSummary,
    ) -> None:
        resolved = payload
        if (
            isinstance(resolved, QuestionBundle)
            and resolved.question_id == previous.question_id
        ):
            if not self.session_id:
                messagebox.showerror("Session error", "Session has ended unexpectedly.")
                self.app.show_opening()
                return
            try:
                resolved = self.engine.next_question(self.session_id)
            except Exception as exc:
                messagebox.showerror("Session error", f"Failed to advance to the next question: {exc}")
                self.app.show_opening()
                return
        self._handle_engine_payload(resolved)

    def _handle_engine_payload(self, payload: QuestionBundle | SessionSummary) -> None:
        if isinstance(payload, SessionSummary):
            step = self.steps[self.current_step_index]
            self.app.session_view.show_summary(payload, step.title)
            self._advance_step()
            return

        self.current_bundle = payload
        try:
            q_index = int(payload.question_id.split("-")[-1])
        except ValueError:
            q_index = 1
        self.question_started_at = time.time()
        self.app.session_view.show_question(payload, q_index, self.current_total)
        if self.reference_pending:
            self.reference_pending = False
            self.play_reference("ScaleArpeggio", include_prompt=True)
        else:
            self.play_prompt()

    def _start_audio_thread(self, fn: Callable[[], None]) -> None:
        if self.player is None:
            return
        if self.audio_thread and self.audio_thread.is_alive():
            return
        self.audio_thread = threading.Thread(target=fn, daemon=True)
        self.audio_thread.start()

    def _load_session_assists(self) -> None:
        self.session_assists.clear()
        if self.session_id is None:
            return
        try:
            options = self.engine.assist_options(self.session_id)
        except Exception:
            return
        for label in options:
            try:
                bundle = self.engine.assist(self.session_id, label)
            except Exception:
                continue
            if bundle.prompt_clip is not None:
                self.session_assists[label.lower()] = bundle.prompt_clip

    def _get_session_assist_clip(self, kind: str) -> Optional[MidiClip]:
        return self.session_assists.get(kind.lower())


# Helper functions ------------------------------------------------------

def build_spec(
    *,
    drill_kind: str,
    key: str,
    scale: str,
    questions: int,
    extra_params: Dict[str, Any],
    relative_range: Tuple[int, int] | None,
) -> SessionSpec:
    key_phrase = f"{key} {scale.replace('_', ' ')}"
    sampler_params = {k: v for k, v in (extra_params or {}).items() if k not in {"drill", "questions"}}
    if drill_kind == "note" and relative_range is not None:
        down, up = relative_range
        sampler_params.setdefault("relative_octaves_down", max(0, int(down)))
        sampler_params.setdefault("relative_octaves_up", max(0, int(up)))
    seed = random.randint(1, 2**31 - 1)
    return SessionSpec(
        drill_kind=drill_kind,
        key=key_phrase,
        n_questions=max(1, int(questions)),
        assistance_policy={"Replay": 2},
        sampler_params=sampler_params,
        generation="adaptive",
        seed=seed,
    )


def _step_params_for_sampler(raw: Dict[str, Any]) -> Dict[str, Any]:
    j_params = dict(raw)
    j_params.pop("drill", None)
    j_params.pop("questions", None)
    j_params.pop("preset", None)
    return j_params


def _coerce_answer(answer: str, expected: Any) -> Any:
    if expected is None:
        return answer
    if isinstance(expected, bool):
        lowered = answer.strip().lower()
        if lowered in {"true", "1", "y", "yes"}:
            return True
        if lowered in {"false", "0", "n", "no"}:
            return False
        return bool(answer)
    if isinstance(expected, int):
        try:
            return int(answer)
        except ValueError:
            return answer
    if isinstance(expected, float):
        try:
            return float(answer)
        except ValueError:
            return answer
    if isinstance(expected, list):
        if not answer:
            return []
        parts = [p.strip() for p in answer.replace(",", " ").split() if p.strip()]
        return [_coerce_answer(p, expected[0] if expected else p) for p in parts]
    if isinstance(expected, dict):
        return {k: _coerce_answer(answer, v) for k, v in expected.items()}
    return str(answer)


# Entry point -----------------------------------------------------------

def main(argv: Sequence[str] | None = None) -> int:
    if SessionEngine is None:
        print(
            "EarTrainer GUI requires the C++ SessionEngine bindings."
        )
        if _IMPORT_ERROR:
            print("Import error:", _IMPORT_ERROR)
        return 1

    app = EarTrainerGUI()
    try:
        app.mainloop()
    except KeyboardInterrupt:  # pragma: no cover - manual quit
        pass
    return 0


if __name__ == "__main__":  # pragma: no cover - manual execution
    raise SystemExit(main())
