from __future__ import annotations

"""Play all voicing templates in succession for C root.

Usage:
  python -m eartrainer.tools.play_voicings --config eartrainer/config/defaults.yml
"""

import argparse

from ..audio.playback import make_synth_from_config
from ..config.config import load_config, validate_config
from ..theory.voicing_bank import VoicingBank


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Play all voicing templates for C root")
    p.add_argument("--config", type=str, default=None, help="Path to YAML config")
    return p.parse_args()


def main() -> None:
    args = _parse_args()
    cfg = validate_config(load_config(args.config))

    synth = make_synth_from_config(cfg)
    synth.program_piano()
    synth.sleep_ms(200)

    bank = VoicingBank()  # defaults to resources/voicings/piano_basic.yml
    templates = bank.templates
    print(f"Found {len(templates)} templates. Playing for root C (C4 RH base)…")

    for t in templates:
        label = t.get("label", "")
        # Clamp right-hand to F3..E5 (53..76)
        v = bank.render_template("C", t, clamp_range=(53, 76))
        print(f"Playing: {label} -> {v.midi_notes}")
        synth.play_chord(v.midi_notes, velocity=90, dur_ms=800)
        synth.sleep_ms(1000)

    synth.close()


if __name__ == "__main__":
    main()
