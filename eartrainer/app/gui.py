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


KEYS = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
SCALES = ["major", "natural_minor"]
DRILLS = ["note", "chord"]


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

        self._build_controls()
        self._build_console()

        self._input_event = threading.Event()
        self._input_value = ""
        self._running_thread: threading.Thread | None = None

    def _build_controls(self) -> None:
        frm = ttk.Frame(self)
        frm.pack(side=tk.TOP, fill=tk.X, padx=10, pady=10)

        ttk.Label(frm, text="Key:").grid(row=0, column=0, sticky=tk.W, padx=4)
        ttk.OptionMenu(frm, self.key_var, self.key_var.get(), *KEYS).grid(row=0, column=1, sticky=tk.W)

        ttk.Label(frm, text="Scale:").grid(row=0, column=2, sticky=tk.W, padx=12)
        ttk.OptionMenu(frm, self.scale_var, self.scale_var.get(), *SCALES).grid(row=0, column=3, sticky=tk.W)

        ttk.Label(frm, text="Drill:").grid(row=0, column=4, sticky=tk.W, padx=12)
        ttk.OptionMenu(frm, self.drill_var, self.drill_var.get(), *DRILLS).grid(row=0, column=5, sticky=tk.W)

        ttk.Checkbutton(frm, text="Drone", variable=self.drone_var).grid(row=0, column=6, sticky=tk.W, padx=12)

        ttk.Label(frm, text="# Questions:").grid(row=0, column=7, sticky=tk.W, padx=12)
        ttk.Entry(frm, textvariable=self.questions_var, width=6).grid(row=0, column=8, sticky=tk.W)

        ttk.Button(frm, text="Start", command=self.start_session).grid(row=0, column=9, padx=12)

    def _build_console(self) -> None:
        console_frame = ttk.Frame(self)
        console_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=6)

        self.text = tk.Text(console_frame, height=20, wrap=tk.WORD)
        self.text.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        entry_frame = ttk.Frame(console_frame)
        entry_frame.pack(side=tk.BOTTOM, fill=tk.X)
        ttk.Label(entry_frame, text="Answer:").pack(side=tk.LEFT)
        self.entry = ttk.Entry(entry_frame)
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=6)
        ttk.Button(entry_frame, text="Submit", command=self._on_submit).pack(side=tk.LEFT)
        # Allow pressing Enter to submit
        self.entry.bind("<Return>", lambda _e: self._on_submit())

    def log(self, msg: str) -> None:
        self.text.insert(tk.END, msg + "\n")
        self.text.see(tk.END)

    def _on_submit(self) -> None:
        self._input_value = self.entry.get()
        self.entry.delete(0, tk.END)
        self._input_event.set()

    def _ui_callbacks(self, drone_mgr: DroneManager | None, session_key: str, scale_type: str) -> Dict[str, Any]:
        def ask(prompt: str) -> str:
            self.log(prompt)
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
            self.log(msg.rstrip())

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
        questions = max(1, int(self.questions_var.get() or 10))

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

            sm = SessionManager(cfg, synth, drone=drone_mgr)
            overrides = {"questions": questions, "key": key, "scale_type": scale_type}
            sm.start_session(drill, "default", overrides)
            ui = self._ui_callbacks(drone_mgr, key, scale_type)
            try:
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
        self.log(f"Starting: drill={drill}, key={key}, scale={scale_type}, questions={questions}, drone={self.drone_var.get()}")
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


def main() -> int:
    app = App()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
