from __future__ import annotations

"""FluidSynth-based audio playback implementation."""

from typing import Dict, List
import sys

from .synthesis import Synth


class FluidSynthSynth(Synth):
    """Concrete Synth using pyfluidsynth."""

    def __init__(self, soundfont_path: str, sample_rate: int = 44100, gain: float = 0.5) -> None:
        super().__init__(sample_rate=sample_rate, gain=gain)
        try:
            import fluidsynth  # type: ignore
        except Exception as e:  # pragma: no cover - runtime dependency
            raise RuntimeError("pyfluidsynth is not installed") from e

        self._fs = fluidsynth.Synth(samplerate=sample_rate, gain=gain)
        # Start audio driver; prefer CoreAudio on macOS to avoid SDL warnings
        driver = None
        if sys.platform == "darwin":
            driver = "coreaudio"
        try:
            if driver:
                self._fs.start(driver=driver)
            else:
                self._fs.start()
        except Exception:
            # Fallback to default driver if preferred one fails
            self._fs.start()
        sfid = self._fs.sfload(soundfont_path)
        self._sfid = sfid
        self._fs.program_select(0, sfid, 0, 0)  # channel 0 = Acoustic Grand

    def program_piano(self) -> None:
        # Already set in __init__ for GM acoustic grand
        pass

    def note_on(self, midi: int, velocity: int = 100, dur_ms: int = 500) -> None:
        self._fs.noteon(0, midi, velocity)
        self.sleep_ms(dur_ms)
        self._fs.noteoff(0, midi)

    def play_chord(self, midis: List[int], velocity: int = 90, dur_ms: int = 800) -> None:
        for m in midis:
            self._fs.noteon(0, m, velocity)
        self.sleep_ms(dur_ms)
        for m in midis:
            self._fs.noteoff(0, m)

    def close(self) -> None:
        try:
            self._fs.delete()
        except Exception:
            pass

    # Advanced channel controls
    def select_program(self, channel: int, bank: int, preset: int) -> None:
        self._fs.program_select(int(channel), self._sfid, int(bank), int(preset))

    def cc(self, channel: int, control: int, value: int) -> None:
        self._fs.cc(int(channel), int(control), max(0, min(127, int(value))))

    def note_on_raw(self, channel: int, midi: int, velocity: int) -> None:
        self._fs.noteon(int(channel), int(midi), max(0, min(127, int(velocity))))

    def note_off_raw(self, channel: int, midi: int) -> None:
        self._fs.noteoff(int(channel), int(midi))

    def all_notes_off(self, channel: int) -> None:
        # CC#123 = All Notes Off
        self._fs.cc(int(channel), 123, 0)


def make_synth_from_config(cfg: Dict) -> Synth:
    """Factory for Synth from config dict."""
    audio = cfg.get("audio", {})
    backend = audio.get("backend", "fluidsynth")
    if backend == "fluidsynth":
        return FluidSynthSynth(
            soundfont_path=audio.get("soundfont_path"),
            sample_rate=int(audio.get("sample_rate", 44100)),
            gain=float(audio.get("gain", 0.5)),
        )
    raise ValueError(f"Unsupported backend: {backend}")
