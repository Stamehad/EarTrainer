from __future__ import annotations

"""Lightweight MIDI playback helper for notebook experiments."""

import sys
import time
from pathlib import Path
from typing import Iterable, Optional, Sequence

from .models import Note, PromptPlan


class SimpleMidiPlayer:
    """Minimal FluidSynth-backed player for SessionEngine prompts."""

    def __init__(
        self,
        soundfont_path: Optional[str] = None,
        *,
        sample_rate: int = 44100,
        gain: float = 0.5,
        channel: int = 0,
        default_velocity: int = 90,
    ) -> None:
        try:
            import fluidsynth  # type: ignore
        except Exception as exc:  # pragma: no cover - runtime dependency
            msg = (
                "Failed to import pyfluidsynth. "
                "Ensure the Python package AND the native FluidSynth library are installed.\n"
                "- If using conda: conda install -n eartrainer_cpp -c conda-forge fluidsynth pyfluidsynth\n"
                "- If using Homebrew (macOS): brew install fluid-synth\n"
                "Original error: " + repr(exc)
            )
            raise RuntimeError(msg) from exc

        self._fs = fluidsynth.Synth(samplerate=sample_rate, gain=gain)
        self._channel = int(channel)
        self._default_velocity = int(default_velocity)

        driver = "coreaudio" if sys.platform == "darwin" else None
        try:
            if driver:
                self._fs.start(driver=driver)
            else:
                self._fs.start()
        except Exception:
            self._fs.start()

        resolved_sf = self._resolve_soundfont(soundfont_path)
        self._sfid = self._fs.sfload(str(resolved_sf))
        self._fs.program_select(self._channel, self._sfid, 0, 0)

        self._held_notes: set[int] = set()

    def _resolve_soundfont(self, candidate: Optional[str]) -> Path:
        if candidate:
            path = Path(candidate).expanduser().resolve()
            if not path.exists():
                raise FileNotFoundError(f"Soundfont not found: {path}")
            return path

        base = Path(__file__).resolve().parents[4]
        search: Sequence[str] = (
            "soundfonts/GrandPiano.sf2",
            "soundfonts/GM.sf2",
        )
        for rel in search:
            path = base / rel
            if path.exists():
                return path
        raise FileNotFoundError(
            "No default soundfont found. Provide soundfont_path when constructing SimpleMidiPlayer."
        )

    # Public API ----------------------------------------------------------
    def play_prompt(self, prompt: Optional[PromptPlan], *, velocity: Optional[int] = None) -> None:
        """Play the notes contained in a PromptPlan."""
        if prompt is None:
            return
        self.play_notes(prompt.notes, velocity=velocity)

    def play_notes(self, notes: Iterable[Note], *, velocity: Optional[int] = None) -> None:
        """Play a sequence of Note dataclasses."""
        for note in notes:
            self._play_note(note, velocity)
        self.stop_all()

    def play_midi_sequence(
        self,
        sequence: Iterable[tuple[int, int, Optional[int], Optional[bool]]],
    ) -> None:
        """Play raw MIDI triplets (pitch, duration_ms, velocity, tie)."""
        for pitch, dur_ms, vel, tie in sequence:
            midi_note = Note(pitch=pitch, dur_ms=dur_ms, vel=vel, tie=tie)
            self._play_note(midi_note, velocity=None)
        self.stop_all()

    def stop_all(self) -> None:
        """Release any sustained notes to prevent hanging voices."""
        for pitch in list(self._held_notes):
            try:
                self._fs.noteoff(self._channel, pitch)
            except Exception:
                pass
        self._held_notes.clear()
        try:
            self._fs.cc(self._channel, 123, 0)  # All notes off
        except Exception:
            pass

    def close(self) -> None:
        self.stop_all()
        try:
            self._fs.delete()
        except Exception:
            pass

    # Context manager support --------------------------------------------
    def __enter__(self) -> "SimpleMidiPlayer":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    # Internal helpers ---------------------------------------------------
    def _play_note(self, note: Note, override_velocity: Optional[int]) -> None:
        pitch = int(note.pitch)
        dur_ms = max(0, int(note.dur_ms))

        # Treat negative pitches as rests
        if pitch < 0 or dur_ms == 0:
            time.sleep(dur_ms / 1000.0)
            return

        velocity = override_velocity if override_velocity is not None else note.vel
        if velocity is None:
            velocity = self._default_velocity
        velocity = max(0, min(127, int(velocity)))

        is_already_held = pitch in self._held_notes
        if not is_already_held:
            self._fs.noteon(self._channel, pitch, velocity)

        time.sleep(dur_ms / 1000.0)

        tie_forward = bool(note.tie)
        if tie_forward:
            self._held_notes.add(pitch)
        else:
            self._fs.noteoff(self._channel, pitch)
            self._held_notes.discard(pitch)


__all__ = ["SimpleMidiPlayer"]
