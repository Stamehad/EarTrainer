from __future__ import annotations

"""CLI for EarTrainer using SessionManager and DrillRegistry."""

import argparse
from typing import Any

from ..config.config import load_config, validate_config
from ..audio.playback import make_synth_from_config
from ..audio.drone import DroneManager
from ..theory.scale import Scale
from ..util.randomness import choose_random_key, seed_if_needed

from .drill_registry import list_drills, get_drill
from .session_manager import SessionManager


def _build_ui(drone_mgr: DroneManager | None, session_key: str, scale_type: str, *, auto_duck: bool, base_volume: float) -> dict[str, Any]:
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
            # Ensure drone restarts promptly after a timeout when user resumes
            from ..theory.scale import Scale as _Scale
            drone_mgr.ensure_running(_Scale(session_key, scale_type))

    def maybe_restart_drone() -> None:
        if drone_mgr:
            scale = Scale(session_key, scale_type)
            drone_mgr.restart_if_key_changed(scale)
            drone_mgr.ensure_running(scale)

    def duck_drone(level: float = 0.15) -> None:
        if drone_mgr and auto_duck:
            drone_mgr.set_mix(level)

    def unduck_drone() -> None:
        if drone_mgr and auto_duck:
            drone_mgr.set_mix(base_volume)

    return {
        "ask": ask,
        "inform": inform,
        "confirm_replay_reference": confirm_replay_reference,
        "activity": activity,
        "maybe_restart_drone": maybe_restart_drone,
        "duck_drone": duck_drone,
        "unduck_drone": unduck_drone,
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="eartrainer")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("list-drills")

    sp = sub.add_parser("show-params")
    sp.add_argument("--drill", required=True)
    sp.add_argument("--preset", default="default")

    rp = sub.add_parser("run")
    rp.add_argument("--config", default=None)
    rp.add_argument("--drill", default="note")
    rp.add_argument("--preset", default="default")
    rp.add_argument("--key", default=None)
    rp.add_argument("--scale", default=None)
    rp.add_argument("--questions", type=int, default=None)

    args = p.parse_args(argv)

    if args.cmd == "list-drills":
        for m in list_drills():
            print(f"{m.id}: {m.name} - {m.description} | presets: {', '.join(m.presets.keys())}")
        return 0

    if args.cmd == "show-params":
        m = get_drill(args.drill)
        print(f"Drill {m.id}: {m.name}")
        print("Presets:")
        for name, params in m.presets.items():
            print(f"  - {name}: {params}")
        return 0

    if args.cmd == "run":
        seed_if_needed()
        cfg = validate_config(load_config(args.config))

        # Determine key/scale
        ctx = cfg.get("context", {})
        transp = cfg.get("transposition", {})
        key = args.key or ctx.get("key")
        scale_type = args.scale or ctx.get("scale_type", "major")
        if not args.key and transp.get("randomize_key_each_session", True):
            key = choose_random_key()

        # Build synth and optional drone
        synth = make_synth_from_config(cfg)
        synth.program_piano()
        synth.sleep_ms(150)

        drone_mgr = None
        drone_cfg = cfg.get("drone", {})
        if bool(drone_cfg.get("enabled", False)):
            drone_mgr = DroneManager(synth, drone_cfg)
            try:
                drone_mgr.configure_channel()
                scale = Scale(key, scale_type)
                drone_mgr.start(scale, template_id=str(drone_cfg.get("template", "root5")))
            except Exception as e:
                print(f"[WARN] Drone init failed: {e}")

        # Session manager
        sm = SessionManager(cfg, synth, drone=drone_mgr)
        overrides: dict[str, Any] = {
            "key": key,
            "scale_type": scale_type,
        }
        if args.questions is not None:
            overrides["questions"] = args.questions
        sm.start_session(args.drill, args.preset, overrides)

        # UI callbacks
        auto_duck = bool(drone_cfg.get("auto_pause_during_question", False))
        base_vol = float(drone_cfg.get("volume", 0.35))
        ui = _build_ui(drone_mgr, key, scale_type, auto_duck=auto_duck, base_volume=base_vol)

        # Run
        summary = sm.run(ui)
        print("\nSession Summary:")
        print(summary)

        # Cleanup
        if drone_mgr:
            try:
                drone_mgr.stop("session_end")
            except Exception:
                pass
        synth.close()
        return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
