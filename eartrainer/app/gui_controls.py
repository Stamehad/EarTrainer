from __future__ import annotations

"""GUI control builders for Drill and Set modes.

Provides small, focused builders to keep gui.py simpler.
"""

import tkinter as tk
from tkinter import ttk
from typing import List, Dict, Any


KEYS: List[str] = [
    "Random", "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B",
]
SCALES = ["major", "natural_minor"]
DRILLS = ["note", "chord", "chord_relative"]


def build_drill_controls(app) -> ttk.Frame:
    frm = ttk.Frame(app)

    ttk.Label(frm, text="Key:").grid(row=0, column=0, sticky=tk.W, padx=4)
    ttk.OptionMenu(frm, app.key_var, app.key_var.get(), *KEYS).grid(row=0, column=1, sticky=tk.W)

    ttk.Label(frm, text="Scale:").grid(row=0, column=2, sticky=tk.W, padx=12)
    ttk.OptionMenu(frm, app.scale_var, app.scale_var.get(), *SCALES).grid(row=0, column=3, sticky=tk.W)

    ttk.Label(frm, text="Drill:").grid(row=0, column=4, sticky=tk.W, padx=12)
    ttk.OptionMenu(frm, app.drill_var, app.drill_var.get(), *DRILLS).grid(row=0, column=5, sticky=tk.W)

    ttk.Label(frm, text="# Questions:").grid(row=0, column=6, sticky=tk.W, padx=12)
    ttk.Entry(frm, textvariable=app.questions_var, width=6).grid(row=0, column=7, sticky=tk.W)

    ttk.Label(frm, text="Degrees:").grid(row=0, column=8, sticky=tk.W, padx=12)
    ttk.Entry(frm, textvariable=app.degrees_var, width=18).grid(row=0, column=9, sticky=tk.W)

    ttk.Button(frm, text="Start", command=app.start_session).grid(row=0, column=10, padx=12)

    return frm


def build_set_controls(app, available_sets: List[Dict[str, Any]]) -> ttk.Frame:
    frm = ttk.Frame(app)
    ttk.Label(frm, text="Key:").grid(row=0, column=0, sticky=tk.W, padx=4)
    ttk.OptionMenu(frm, app.key_var, app.key_var.get(), *KEYS).grid(row=0, column=1, sticky=tk.W)

    ttk.Label(frm, text="Set:").grid(row=0, column=2, sticky=tk.W, padx=12)
    set_ids = [s.get("id", "") for s in available_sets]
    ttk.OptionMenu(frm, app.set_var, app.set_var.get(), *(set_ids if set_ids else [""])).grid(row=0, column=3, sticky=tk.W)

    ttk.Button(frm, text="Start", command=app.start_session).grid(row=0, column=4, padx=12)
    return frm

