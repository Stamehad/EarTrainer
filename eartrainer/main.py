from __future__ import annotations

"""CLI entry point for EarTrainer MVP."""

import argparse
import sys
from typing import Dict

from . import __version__
from .audio.playback import make_synth_from_config
from .audio.drone import DroneManager
from .config.config import load_config, validate_config
from .drills.base_drill import DrillContext
from .drills.note_drill import NoteDegreeDrill
from .stats.stats import format_summary, new_session_stats, update_stats, write_stats
from .util.randomness import choose_random_key, seed_if_needed
from .theory.keys import note_str_to_midi


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="EarTrainer CLI")
    p.add_argument("--config", type=str, default=None, help="Path to YAML config")
    p.add_argument("--version", action="store_true", help="Print version and exit")
    return p.parse_args()


def cli() -> None:
    args = _parse_args()
    if args.version:
        print(f"eartrainer {__version__}")
        sys.exit(0)

    seed_if_needed()

    cfg = validate_config(load_config(args.config))

    # Determine session key
    context = cfg["context"]
    if cfg.get("transposition", {}).get("randomize_key_each_session", True):
        session_key = choose_random_key()
    else:
        session_key = context.get("key", "C")
    scale_type = context.get("scale_type", "major")
    reference_mode = context.get("reference_mode", "cadence")

    print(f"Starting EarTrainer session in key {session_key} ({scale_type}).")

    # Synth
    synth = make_synth_from_config(cfg)
    synth.program_piano()
    # Small warmup to ensure audio backend is ready
    synth.sleep_ms(150)

    # Drill context
    drill_cfg = cfg["drill"]
    # Optional note range for drill
    min_midi = None
    max_midi = None
    nr = drill_cfg.get("note_range")
    if isinstance(nr, dict) and "min_note" in nr and "max_note" in nr:
        try:
            min_midi = int(note_str_to_midi(str(nr["min_note"])) )
            max_midi = int(note_str_to_midi(str(nr["max_note"])) )
            if min_midi > max_midi:
                min_midi, max_midi = max_midi, min_midi
        except Exception:
            min_midi = None
            max_midi = None

    ctx = DrillContext(
        key_root=session_key,
        scale_type=scale_type,
        reference_mode=reference_mode,
        degrees_in_scope=list(drill_cfg.get("degrees_in_scope", ["1", "2", "3", "4", "5", "6", "7"])),
        include_chromatic=bool(drill_cfg.get("include_chromatic", False)),
        min_midi=min_midi,
        max_midi=max_midi,
        test_note_delay_ms=int(drill_cfg.get("test_note_delay_ms", 300)),
        voicing_bass_octave=int(context.get("voicing", {}).get("bass_octave", 2)),
        voicing_chord_octave_by_pc=list(
            context.get("voicing", {}).get(
                "chord_octave_by_pc", [4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3]
            )
        ),
    )

    # For MVP always note drill
    drill = NoteDegreeDrill(ctx, synth)

    # Optional drone setup
    drone_mgr = None
    drone_cfg = cfg.get("drone", {})
    if bool(drone_cfg.get("enabled", False)):
        drone_mgr = DroneManager(synth, drone_cfg)
        try:
            drone_mgr.configure_channel()
            # Start drone at session key
            from .theory.scale import Scale
            scale = Scale(session_key, scale_type)
            drone_mgr.start(scale, template_id=str(drone_cfg.get("template", "root5")))
        except Exception as e:
            print(f"[WARN] Drone initialization failed: {e}")

    # Play reference once
    drill.play_reference()
    synth.sleep_ms(ctx.test_note_delay_ms)

    # UI callbacks
    last_input = {"value": None}

    def ask(prompt: str) -> str:
        val = input(prompt)
        last_input["value"] = val
        if drone_mgr:
            drone_mgr.ping_activity("input")
        return val

    def inform(msg: str) -> None:
        print(msg)

    def confirm_replay_reference() -> bool:
        return (last_input.get("value", "").strip().lower() == "r")

    def activity(event: str = "") -> None:
        if drone_mgr:
            drone_mgr.ping_activity(event)

    def maybe_restart_drone() -> None:
        if drone_mgr:
            from .theory.scale import Scale
            scale = Scale(session_key, scale_type)
            drone_mgr.restart_if_key_changed(scale)

    num_q = int(drill_cfg.get("questions", 50))
    result = drill.run(num_q, {"ask": ask, "inform": inform, "confirm_replay_reference": confirm_replay_reference, "activity": activity, "maybe_restart_drone": maybe_restart_drone})

    # Aggregate to session stats
    stats = new_session_stats()
    # update per asked degree
    for d, data in result.per_degree.items():
        asked = int(data.get("asked", 0))
        corr = int(data.get("correct", 0))
        for _ in range(asked):
            update_stats(stats, d, correct=(corr > 0))
            corr -= 1

    # Ensure total/correct reflect result
    stats["total"] = result.total
    stats["correct"] = result.correct
    stats["per_degree"] = result.per_degree

    # Write stats
    out_path = cfg.get("stats", {}).get("output_path", "./session_stats.json")
    write_stats(stats, out_path)

    # Summary
    print("\nSession Summary:")
    print(format_summary(stats))

    # Cleanup
    if drone_mgr:
        try:
            drone_mgr.stop("drill_end")
        except Exception:
            pass
    synth.close()


if __name__ == "__main__":
    cli()
