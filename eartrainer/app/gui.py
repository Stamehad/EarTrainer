from __future__ import annotations

"""Very simple Tkinter GUI to launch a drill.

Lets users pick key, scale (major/natural_minor), drill (note/chord),
enable/disable drone, and number of questions, then runs the session.
"""

import threading
import tkinter as tk
from tkinter import ttk, messagebox
from typing import Any, Dict

from ..config.config import load_config, validate_config
from ..audio.playback import make_synth_from_config
from ..audio.drone import DroneManager
from ..theory.scale import Scale
from ..util.randomness import seed_if_needed
from .session_manager import SessionManager
from .training_sets import list_sets as ts_list_sets, get_set as ts_get_set


KEYS = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
SCALES = ["major", "natural_minor"]
DRILLS = ["note", "chord", "chord_relative"]


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("EarTrainer (Minimal GUI)")
        self.geometry("760x520")

        self.key_var = tk.StringVar(value="C")
        self.scale_var = tk.StringVar(value="major")
        self.drill_var = tk.StringVar(value="note")
        self.drone_var = tk.BooleanVar(value=False)
        self.questions_var = tk.IntVar(value=10)
        self.flicker_var = tk.BooleanVar(value=True)
        # Degrees selection and chord-repeat option
        self.degrees_var = tk.StringVar(value="1,2,3,4,5,6,7")
        self.allow_repeat_var = tk.BooleanVar(value=False)
        # Training sets (YAML-backed)
        self.set_var = tk.StringVar(value="")
        try:
            self._available_sets = ts_list_sets()
        except Exception:
            self._available_sets = []

        self._build_controls()
        self._build_console()

        self._input_event = threading.Event()
        self._input_value = ""
        self._running_thread: threading.Thread | None = None
        self._asked = 0
        self._correct = 0
        self._last_answer = ""
        self._seq_mode = False
        self._seq_total = 0
        self._seq_current = 0
        self._current_q_label = ""

    def _build_controls(self) -> None:
        frm = ttk.Frame(self)
        frm.pack(side=tk.TOP, fill=tk.X, padx=10, pady=10)

        ttk.Label(frm, text="Key:").grid(row=0, column=0, sticky=tk.W, padx=4)
        ttk.OptionMenu(frm, self.key_var, self.key_var.get(), *KEYS).grid(row=0, column=1, sticky=tk.W)

        ttk.Label(frm, text="Scale:").grid(row=0, column=2, sticky=tk.W, padx=12)
        ttk.OptionMenu(frm, self.scale_var, self.scale_var.get(), *SCALES).grid(row=0, column=3, sticky=tk.W)

        ttk.Label(frm, text="Drill:").grid(row=0, column=4, sticky=tk.W, padx=12)
        ttk.OptionMenu(frm, self.drill_var, self.drill_var.get(), *DRILLS).grid(row=0, column=5, sticky=tk.W)

        # Optional training set selector if any sets are available
        col = 6
        if self._available_sets:
            ttk.Label(frm, text="Set:").grid(row=0, column=6, sticky=tk.W, padx=12)
            set_ids = [s.get("id", "") for s in self._available_sets]
            ttk.OptionMenu(frm, self.set_var, self.set_var.get(), *( [""] + set_ids )).grid(row=0, column=7, sticky=tk.W)
            col = 8

        ttk.Checkbutton(frm, text="Drone", variable=self.drone_var).grid(row=0, column=col, sticky=tk.W, padx=12)

        ttk.Label(frm, text="# Questions:").grid(row=0, column=col+1, sticky=tk.W, padx=12)
        ttk.Entry(frm, textvariable=self.questions_var, width=6).grid(row=0, column=col+2, sticky=tk.W)

        ttk.Label(frm, text="Degrees:").grid(row=0, column=col+3, sticky=tk.W, padx=12)
        ttk.Entry(frm, textvariable=self.degrees_var, width=16).grid(row=0, column=col+4, sticky=tk.W)

        ttk.Checkbutton(frm, text="Allow repeat", variable=self.allow_repeat_var).grid(row=0, column=col+5, sticky=tk.W, padx=12)

        ttk.Button(frm, text="Start", command=self.start_session).grid(row=0, column=col+6, padx=12)
        # Move gear to status bar (bottom) near score, right-justified. See _build_console.

    def _build_console(self) -> None:
        console_frame = ttk.Frame(self)
        console_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=6)

        # Instructions shown once per session
        self.instructions = ttk.Label(console_frame, text="", anchor=tk.W, justify=tk.LEFT)
        self.instructions.pack(side=tk.TOP, fill=tk.X, pady=(0, 6))

        self.text = tk.Text(console_frame, height=20, wrap=tk.WORD)
        self.text.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        entry_frame = ttk.Frame(console_frame)
        entry_frame.pack(side=tk.BOTTOM, fill=tk.X)
        ttk.Label(entry_frame, text="Answer:").pack(side=tk.LEFT)
        self.entry = tk.Entry(entry_frame)
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=6)
        ttk.Button(entry_frame, text="Submit", command=self._on_submit).pack(side=tk.LEFT)
        # Allow pressing Enter to submit
        self.entry.bind("<Return>", lambda _e: self._on_submit())

        # Status bar with running score
        status = ttk.Frame(self)
        status.pack(side=tk.BOTTOM, fill=tk.X, padx=10, pady=(0, 8))
        self.score_var = tk.StringVar(value="Score: 0/0")
        ttk.Label(status, textvariable=self.score_var, anchor=tk.W).pack(side=tk.LEFT)
        ttk.Button(status, text="⚙️", width=2, command=self.open_options).pack(side=tk.RIGHT)

    def log(self, msg: str) -> None:
        self.text.insert(tk.END, msg + "\n")
        self.text.see(tk.END)

    def _flicker_widget(self, widget: tk.Widget, color: str = "#ff0000", times: int = 1, interval_ms: int = 120) -> None:
        """Briefly flicker a widget background color to draw attention (UI thread safe)."""
        try:
            original = widget.cget("background")
        except Exception:
            return

        def step(i: int) -> None:
            try:
                widget.configure(background=(color if i % 2 == 0 else original))
            except Exception:
                return
            if i < times * 2:
                self.after(interval_ms, step, i + 1)
            else:
                try:
                    widget.configure(background=original)
                except Exception:
                    pass

        # Schedule on UI loop
        self.after(0, step, 0)

    def _flash_incorrect(self) -> None:
        # Flicker both the console and the answer entry for feedback
        if self.flicker_var.get():
            self._flicker_widget(self.text)
            self._flicker_widget(self.entry)

    def _on_submit(self) -> None:
        self._input_value = self.entry.get()
        self._last_answer = self._input_value
        self.entry.delete(0, tk.END)
        self._input_event.set()

    def _ui_callbacks(self, drone_mgr: DroneManager | None, session_key: str, scale_type: str) -> Dict[str, Any]:
        def ask(prompt: str) -> str:
            # Show instructions once at top; avoid repeating in console
            if "Enter scale degree" in prompt and not self.instructions.cget("text"):
                self.instructions.configure(text=prompt)
            self._input_event.clear()
            self.entry.focus_set()
            self.wait_variable(tk.BooleanVar(value=False)) if False else None  # no-op; keeps signature
            # Poll for event to avoid freezing the UI
            while not self._input_event.wait(timeout=0.05):
                self.update()
                if drone_mgr:
                    drone_mgr.ping_activity("poll")
            return self._input_value

        def inform(msg: str) -> None:
            m = msg.rstrip()
            # Track current question label (but do not log yet)
            if m.startswith("Q") and ":" in m:
                # Rewrite Qx/N numbering in sequence mode, store label only
                if self._seq_mode:
                    self._seq_current += 1
                    self._current_q_label = f"Q{self._seq_current}/{self._seq_total}"
                else:
                    # Extract the prefix before ':'
                    try:
                        self._current_q_label = m.split(":", 1)[0]
                    except Exception:
                        self._current_q_label = "Q?"
                return

            # One-line per-question output
            if m.startswith("Incorrect. Answer was "):
                try:
                    truth = m.split("Incorrect. Answer was ", 1)[1].strip(".")
                except Exception:
                    truth = "?"
                heard = (self._last_answer or "?").strip()
                pretty = f"{self._current_q_label or 'Q?'}: ❌ Answer was {truth}. You heard {heard}."
                self._asked += 1
                self._flash_incorrect()
                self.score_var.set(f"Score: {self._correct}/{self._asked}")
                self.log(pretty)
                return
            if m.startswith("Correct!"):
                self._asked += 1
                self._correct += 1
                pretty = f"{self._current_q_label or 'Q?'}: ✅"
                self.score_var.set(f"Score: {self._correct}/{self._asked}")
                self.log(pretty)
                return

            # Fallback: log any other messages unchanged
            self.log(m)

        def confirm_replay_reference() -> bool:
            # Not used by current prompt mapping but kept for compatibility
            return False

        def activity(event: str = "") -> None:
            if drone_mgr:
                drone_mgr.ping_activity(event)

        def maybe_restart_drone() -> None:
            if drone_mgr:
                scale = Scale(session_key, scale_type)
                drone_mgr.restart_if_key_changed(scale)
                drone_mgr.ensure_running(scale)

        def duck_drone(level: float = 0.15) -> None:
            if drone_mgr:
                drone_mgr.set_mix(level)

        def unduck_drone() -> None:
            if drone_mgr:
                # Base mix comes from config; DroneManager already keeps volume
                pass

        return {
            "ask": ask,
            "inform": inform,
            "confirm_replay_reference": confirm_replay_reference,
            "activity": activity,
            "maybe_restart_drone": maybe_restart_drone,
            "duck_drone": duck_drone,
            "unduck_drone": unduck_drone,
        }

    def start_session(self) -> None:
        if self._running_thread and self._running_thread.is_alive():
            messagebox.showwarning("Running", "A session is already running.")
            return
        seed_if_needed()
        key = self.key_var.get()
        scale_type = self.scale_var.get()
        drill = self.drill_var.get()
        set_id = self.set_var.get().strip() if hasattr(self, "set_var") else ""
        questions = max(1, int(self.questions_var.get() or 10))

        # Parse degrees list (accept comma-separated, allow spaces)
        def _parse_degrees(s: str) -> list[str]:
            raw = [t.strip() for t in (s or "").split(",")]
            out: list[str] = []
            for t in raw:
                if not t:
                    continue
                # Simple range support like 1-5
                if "-" in t:
                    a, b = t.split("-", 1)
                    try:
                        start = int(a)
                        end = int(b)
                        if 1 <= start <= 7 and 1 <= end <= 7:
                            if start <= end:
                                out.extend([str(x) for x in range(start, end + 1)])
                            else:
                                out.extend([str(x) for x in range(end, start + 1)])
                        continue
                    except Exception:
                        pass
                # Single token 1..7
                try:
                    v = int(t)
                    if 1 <= v <= 7:
                        out.append(str(v))
                except Exception:
                    continue
            # dedupe preserving order
            seen = set()
            deduped = []
            for d in out:
                if d not in seen:
                    seen.add(d)
                    deduped.append(d)
            return deduped or ["1","2","3","4","5","6","7"]

        cfg = validate_config(load_config(None))
        cfg_drone = cfg.get("drone", {})
        cfg_drone["enabled"] = bool(self.drone_var.get())

        def runner() -> None:
            synth = make_synth_from_config(cfg)
            synth.program_piano()
            synth.sleep_ms(150)
            drone_mgr = None
            if cfg_drone.get("enabled", False):
                drone_mgr = DroneManager(synth, cfg_drone)
                try:
                    drone_mgr.configure_channel()
                    drone_mgr.start(Scale(key, scale_type), template_id=str(cfg_drone.get("template", "root5")))
                except Exception as e:
                    self.log(f"[WARN] Drone init failed: {e}")

            # Reset UI counters
            self._asked = 0
            self._correct = 0
            self.score_var.set("Score: 0/0")
            self.instructions.configure(text="")

            sm = SessionManager(cfg, synth, drone=drone_mgr)
            ui = self._ui_callbacks(drone_mgr, key, scale_type)
            try:
                if set_id:
                    # Run selected YAML training set
                    try:
                        sdef = ts_get_set(set_id)
                        steps_def = sdef.get("steps", []) or []
                        # Keep full step dicts so we can pass per-step params
                        steps: list[dict] = [dict(st) for st in steps_def if st.get("drill") and st.get("questions")]
                    except Exception as e:
                        self.log(f"[ERROR] Failed to load set '{set_id}': {e}")
                        return
                    self._seq_mode = True
                    self._seq_total = sum(int(st.get("questions", 0)) for st in steps)
                    self._seq_current = 0
                    summaries = []
                    for idx, step in enumerate(steps):
                        d_id = str(step.get("drill"))
                        n_q = int(step.get("questions", 0))
                        overrides = {"questions": n_q, "key": key, "scale_type": scale_type}
                        # Merge explicit per-step params if provided
                        step_params = step.get("params", {}) or {}
                        if isinstance(step_params, dict):
                            overrides.update(step_params)
                        # degrees_in_scope: prefer step override, else UI
                        if "degrees_in_scope" in step:
                            try:
                                degs = [str(x) for x in (step.get("degrees_in_scope") or [])]
                            except Exception:
                                degs = []
                            overrides["degrees_in_scope"] = degs if degs else _parse_degrees(self.degrees_var.get())
                        else:
                            overrides["degrees_in_scope"] = _parse_degrees(self.degrees_var.get())
                        # chord-only flags
                        if d_id == "chord":
                            if "allow_consecutive_repeat" in step:
                                overrides["allow_consecutive_repeat"] = bool(step.get("allow_consecutive_repeat"))
                            else:
                                overrides["allow_consecutive_repeat"] = bool(self.allow_repeat_var.get())
                        # Announce sub-drill start with degree subset if not all
                        if d_id == "note":
                            label = "Starting drill: scale degrees"
                        elif d_id == "chord_relative":
                            label = "Starting drill: chords relative"
                        else:
                            label = "Starting drill: chords"
                        degs_for_label = list(overrides.get("degrees_in_scope") or [])
                        all_degrees = ["1","2","3","4","5","6","7"]
                        if degs_for_label and degs_for_label != all_degrees:
                            label += " " + "-".join(degs_for_label)
                        self.log(label)
                        sm.start_session(d_id, "default", overrides)
                        summary = sm.run(ui, skip_reference=(idx > 0))
                        # Build per-step label for summary (include chord subset when not all)
                        step_label = (
                            "Notes" if d_id == "note" else (
                                "Chords relative" if d_id == "chord_relative" else (
                                    "Chords" if d_id == "chord" else d_id.capitalize()
                                )
                            )
                        )
                        if d_id in ("chord", "chord_relative") and degs_for_label and degs_for_label != all_degrees:
                            step_label += " " + "-".join(degs_for_label)
                        summaries.append((step_label, summary))
                    total = sum(s[1].get("total", 0) for s in summaries)
                    correct = sum(s[1].get("correct", 0) for s in summaries)
                    self.log("")
                    self.log("Session Summary:")
                    if summaries:
                        width = max(len("Score"), max(len(lbl) for (lbl, _s) in summaries))
                        self.log(f"{'Score'.ljust(width)} : {correct}/{total}")
                        for lbl, s in summaries:
                            self.log(f"{lbl.ljust(width)} : {s.get('correct',0)}/{s.get('total',0)}")
                    else:
                        self.log(f"Score: {correct}/{total}")
                    self._seq_mode = False
                else:
                    overrides = {"questions": questions, "key": key, "scale_type": scale_type}
                    overrides["degrees_in_scope"] = _parse_degrees(self.degrees_var.get())
                    if drill in ("chord", "chord_relative"):
                        overrides["allow_consecutive_repeat"] = bool(self.allow_repeat_var.get())
                    sm.start_session(drill, "default", overrides)
                    summary = sm.run(ui)
                    self.log("")
                    self.log("Session Summary:")
                    self.log(self._format_summary(summary))
            finally:
                if drone_mgr:
                    try:
                        drone_mgr.stop("session_end")
                    except Exception:
                        pass
                synth.close()

        self.text.delete(1.0, tk.END)
        if set_id:
            try:
                sdef = ts_get_set(set_id)
                steps = sdef.get("steps", []) or []
                total_q = sum(int(step.get("questions", 0)) for step in steps)
                name = sdef.get("name", set_id)
                self.log(f"Starting set: {name}, key={key}, scale={scale_type}, total_questions={total_q}, drone={self.drone_var.get()}")
            except Exception as e:
                self.log(f"[ERROR] Failed to load set '{set_id}': {e}")
                return
        else:
            # Friendly single-drill start message
            if drill == "note":
                label = "Starting drill: scale degrees"
            elif drill == "chord_relative":
                label = "Starting drill: chords relative"
            else:
                label = "Starting drill: chords"
            degs_for_label = _parse_degrees(self.degrees_var.get())
            if degs_for_label and degs_for_label != ["1","2","3","4","5","6","7"] and drill in ("chord", "chord_relative"):
                label += " " + "-".join(degs_for_label)
            self.log(label)
        self._running_thread = threading.Thread(target=runner, daemon=True)
        self._running_thread.start()

    @staticmethod
    def _format_summary(summary: Dict[str, Any]) -> str:
        total = int(summary.get("total", 0))
        correct = int(summary.get("correct", 0))
        parts = [f"Score: {correct}/{total}"]
        per = summary.get("per_degree", {}) or {}
        # Inline per-degree results, ordered numerically when possible
        items = []
        for k in sorted(per.keys(), key=lambda x: int(x) if str(x).isdigit() else 999):
            asked = int(per[k].get("asked", 0))
            corr = int(per[k].get("correct", 0))
            if asked > 0:
                items.append(f"{k}: {corr}/{asked}")
        if items:
            parts.append(", ".join(items))
        return "\n".join(parts)

    def open_options(self) -> None:
        win = tk.Toplevel(self)
        win.title("Options")
        win.transient(self)
        win.resizable(False, False)
        pad = {"padx": 10, "pady": 8}
        ttk.Checkbutton(win, text="Flicker on wrong", variable=self.flicker_var).grid(row=0, column=0, sticky=tk.W, **pad)
        ttk.Button(win, text="Close", command=win.destroy).grid(row=1, column=0, sticky=tk.E, **pad)


def main() -> int:
    app = App()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
