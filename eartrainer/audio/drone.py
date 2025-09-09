from __future__ import annotations

"""DroneManager: sustained string drone on a dedicated FluidSynth channel."""

import threading
import time
from typing import Callable, Dict, List, Optional, Tuple

from ..audio.synthesis import Synth
from ..theory.scale import Scale
from ..theory.scales import diatonic_degree_to_pc
from ..theory.keys import note_name_to_midi, PITCH_CLASS_NAMES


def _load_yaml(path: str) -> Dict:
    import yaml  # type: ignore
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


class DroneManager:
    def __init__(self, synth: Synth, cfg: Dict, templates_loader: Optional[Callable[[str], Dict]] = None) -> None:
        self.synth = synth
        self.cfg = cfg
        self.channel: int = int(cfg.get("channel", 1))
        self.volume: float = float(cfg.get("volume", 0.35))
        self.fade_in_ms: int = int(cfg.get("fade_in_ms", 200))
        self.fade_out_ms: int = int(cfg.get("fade_out_ms", 250))
        self.timeout_s: int = int(cfg.get("inactivity_timeout_s", 10))
        self.template_id: str = str(cfg.get("template", "root5"))
        self.templates_path: str = str(cfg.get("templates_path", "eartrainer/resources/drones/strings_basic.yml"))
        self.templates_loader = templates_loader or _load_yaml

        self._templates = self.templates_loader(self.templates_path)
        self._defaults = self._templates.get("defaults", {})
        self._inst_programs = (self._defaults.get("instrument_programs") or {})
        self._active_notes: List[int] = []
        self._timer: Optional[threading.Timer] = None
        self._lock = threading.Lock()
        self._running = False
        self._last_scale_key = None  # (root_name, scale_type)

    # Channel and program configuration
    def configure_channel(self) -> None:
        program_name = str(self.cfg.get("instrument", "strings"))
        preset = int(self._inst_programs.get(program_name, 48))
        # GM bank 0, preset as defined
        self.synth.select_program(self.channel, 0, preset)
        # Set base volume via CC7
        vol = max(0, min(127, int(self.volume * 127)))
        self.synth.cc(self.channel, 7, vol)
        # Expression CC11 full initially (fade will adjust as needed)
        self.synth.cc(self.channel, 11, 0)

    def _degree_to_pc(self, scale: Scale, degree: int) -> int:
        return (scale._root_pc + diatonic_degree_to_pc(scale.scale_type, degree)) % 12

    def _render_template(self, scale: Scale, template_id: str) -> List[int]:
        # version/notes parsing
        tmpl = None
        for t in self._templates.get("templates", []) or []:
            if t.get("id") == template_id:
                tmpl = t
                break
        if tmpl is None:
            raise ValueError(f"Drone template not found: {template_id}")
        notes_tokens = list(tmpl.get("notes") or [])
        lh_octave = int(self._defaults.get("lh_octave", 2))
        mid_octave = int(self._defaults.get("mid_octave", 3))

        midis: List[int] = []
        for i, tok in enumerate(notes_tokens):
            # token like "1@+0" or "5@+1"
            if "@+" in tok:
                deg_str, offs = tok.split("@+")
                oct_off = int(offs)
            elif "@-" in tok:
                deg_str, offs = tok.split("@-")
                oct_off = -int(offs)
            else:
                deg_str = tok
                oct_off = 0
            degree = int(deg_str)
            base_oct = lh_octave if i == 0 else mid_octave
            target_oct = base_oct + oct_off
            # compute tonic MIDI at target octave then add degree offset
            root_name = PITCH_CLASS_NAMES[scale._root_pc]
            tonic_midi = note_name_to_midi(root_name, target_oct)
            pc_offset = diatonic_degree_to_pc(scale.scale_type, degree)
            midis.append(tonic_midi + pc_offset)
        return midis

    def _fade(self, start: int, end: int, ms: int) -> None:
        steps = max(1, ms // 30)
        for i in range(steps + 1):
            val = int(start + (end - start) * (i / steps))
            self.synth.cc(self.channel, 11, val)
            time.sleep(ms / 1000.0 / steps)

    def _arm_timer(self) -> None:
        if self.timeout_s <= 0:
            return
        if self._timer:
            self._timer.cancel()
        self._timer = threading.Timer(self.timeout_s, self._on_timeout)
        self._timer.daemon = True
        self._timer.start()

    def _on_timeout(self) -> None:
        with self._lock:
            if not self._running:
                return
            # Fade out and stop
            try:
                self._fade(127, 0, self.fade_out_ms)
            except Exception:
                pass
            finally:
                self._stop_notes()
                self._running = False

    def _stop_notes(self) -> None:
        # Safety: All Notes Off
        try:
            for n in self._active_notes:
                self.synth.note_off_raw(self.channel, n)
        finally:
            self.synth.all_notes_off(self.channel)
            self._active_notes.clear()

    def start(self, scale: Scale, template_id: Optional[str] = None) -> None:
        with self._lock:
            if template_id is None:
                template_id = self.template_id
            notes = self._render_template(scale, template_id)
            self._last_scale_key = (scale.root_name, scale.scale_type)
            # Start at expression 0, then fade in to 127 (scaled by CC7 volume)
            self.synth.cc(self.channel, 11, 0)
            for n in notes:
                self.synth.note_on_raw(self.channel, n, 90)
            self._active_notes = notes
            self._running = True
            self._arm_timer()
        # Fade outside lock
        try:
            self._fade(0, 127, self.fade_in_ms)
        except Exception:
            pass

    def stop(self, reason: str = "") -> None:
        with self._lock:
            if not self._running:
                return
            try:
                self._fade(127, 0, self.fade_out_ms)
            except Exception:
                pass
            finally:
                self._stop_notes()
                self._running = False
                if self._timer:
                    self._timer.cancel()
                    self._timer = None

    def ping_activity(self, event: str = "") -> None:
        # Reset inactivity timer
        with self._lock:
            if self._running:
                self._arm_timer()

    def restart_if_key_changed(self, scale: Scale) -> None:
        with self._lock:
            if not bool(self.cfg.get("restart_on_key_change", True)):
                return
            key = (scale.root_name, scale.scale_type)
            if key != self._last_scale_key and self._running:
                # stop current and start new
                try:
                    self._fade(127, 0, self.fade_out_ms)
                except Exception:
                    pass
                finally:
                    self._stop_notes()
                    self._running = False
                # Start new key
                self._last_scale_key = key
                notes = self._render_template(scale, self.template_id)
                self.synth.cc(self.channel, 11, 0)
                for n in notes:
                    self.synth.note_on_raw(self.channel, n, 90)
                self._active_notes = notes
                self._running = True
                self._arm_timer()

    def set_mix(self, volume: float) -> None:
        self.volume = float(volume)
        vol = max(0, min(127, int(self.volume * 127)))
        self.synth.cc(self.channel, 7, vol)

    def is_running(self) -> bool:
        return self._running

