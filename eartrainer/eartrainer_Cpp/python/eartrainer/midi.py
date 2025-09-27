from __future__ import annotations

"""Lightweight MIDI playback helper for notebook experiments."""

import sys
import time
from pathlib import Path
from typing import Iterable, Optional, Sequence, Any

from .models import Prompt, MidiClip, MidiTrack, MidiEvent


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
    def play_prompt(self, prompt: Optional[Prompt], *, velocity: Optional[int] = None) -> None:
        """Play the content of a Prompt. Supports modality 'midi-clip'."""
        if prompt is None:
            return
        if prompt.modality == "midi-clip" and prompt.midi_clip is not None:
            self.play_midi_clip(prompt.midi_clip)
            return
        # Fallback: ignore unknown modalities silently

    # Back-compat helper to render simple sequential notes if ever needed.
    # def play_notes(self, notes: Iterable[Note], *, velocity: Optional[int] = None) -> None:
    #     for note in notes:
    #         self._play_note(note, velocity)
    #     self.stop_all()

    # Semantic helpers ---------------------------------------------------
    def play_chord_midis(self, midis: Iterable[int], *, velocity: int = 90, dur_ms: int = 900) -> None:
        """Play a chord (simultaneous notes) given raw MIDI pitches."""
        pitches = [int(m) for m in midis]
        vel = max(0, min(127, int(velocity)))
        for p in pitches:
            if p >= 0:
                self._fs.noteon(self._channel, p, vel)
        time.sleep(max(0, dur_ms) / 1000.0)
        for p in pitches:
            if p >= 0:
                self._fs.noteoff(self._channel, p)

    def play_interval(self, bottom_midi: int, top_midi: int, *, melodic: bool = True,
                      descending: Optional[bool] = None, step_ms: int = 600, gap_ms: int = 0,
                      velocity: int = 90) -> None:
        """Play an interval from payload fields.

        - If melodic=True: play in the direction indicated by 'descending' or inferred from pitches.
        - If melodic=False: play harmonic (both notes together).
        """
        b = int(bottom_midi)
        t = int(top_midi)
        if not melodic:
            self.play_chord_midis([b, t], velocity=velocity, dur_ms=step_ms)
            return

        if descending is None:
            descending = t < b
        order = (t, b) if descending else (b, t)
        for i, p in enumerate(order):
            self._fs.noteon(self._channel, p, max(0, min(127, int(velocity))))
            time.sleep(max(0, step_ms) / 1000.0)
            self._fs.noteoff(self._channel, p)
            if i == 0 and gap_ms > 0:
                time.sleep(gap_ms / 1000.0)

    def play_bundle(self, bundle: Any, *, strategy: str = "prompt") -> None:
        """Play a QuestionBundle/AssistBundle using a strategy.

        - strategy="prompt": play exactly the PromptPlan notes.
        - strategy="semantic": derive playback from question payload (e.g., chord voicing, interval direction).
        """
        if strategy == "prompt" or not hasattr(bundle, "question"):
            self.play_prompt(getattr(bundle, "prompt", None))
            return

        q = bundle.question
        payload = getattr(q, "payload", {}) or {}
        qtype = getattr(q, "type", "")
        if qtype == "chord" and "voicing_midi" in payload:
            self.play_chord_midis(payload.get("voicing_midi", []))
            return
        if qtype == "interval" and "bottom_midi" in payload and "top_midi" in payload:
            # Heuristic: assume melodic interval unless indicated otherwise; infer direction from pitch ordering
            self.play_interval(payload["bottom_midi"], payload["top_midi"], melodic=True)
            return
        # Fallback
        self.play_prompt(getattr(bundle, "prompt", None))

    def play_midi_sequence(
        self,
        sequence: Iterable[tuple[int, int, Optional[int], Optional[bool]]],
    ) -> None:
        """Play raw MIDI triplets (pitch, duration_ms, velocity, tie)."""
        for pitch, dur_ms, vel, tie in sequence:
            midi_note = Note(pitch=pitch, dur_ms=dur_ms, vel=vel, tie=tie)
            self._play_note(midi_note, velocity=None)
        self.stop_all()

    def play_midi_clip(self, clip: MidiClip) -> None:
        """Play a multi-track MIDI clip described in engine JSON.

        - Honors per-track channel and program
        - Schedules note_on/note_off events by clip timing (ticks)
        """
        # Program setup per track
        for tr in clip.tracks:
            try:
                self._fs.program_select(int(tr.channel), self._sfid, 0, int(tr.program))
            except Exception:
                pass

        # Flatten and schedule events across tracks
        events: list[tuple[int, str, int, Optional[int], int]] = []
        for tr in clip.tracks:
            for ev in tr.events:
                if ev.type not in ("note_on", "note_off"):
                    continue
                note = -1 if ev.note is None else int(ev.note)
                vel = ev.vel
                events.append((int(ev.t), ev.type, note, vel, int(tr.channel)))
        events.sort(key=lambda x: x[0])

        # Convert ticks to seconds
        s_per_tick = 60.0 / (float(clip.tempo_bpm) * float(clip.ppq))
        now_ticks = 0
        for t, etype, note, vel, ch in events:
            if t > now_ticks:
                time.sleep((t - now_ticks) * s_per_tick)
                now_ticks = t
            if note < 0:
                continue
            if etype == "note_on":
                v = int(vel if vel is not None else self._default_velocity)
                v = max(0, min(127, v))
                self._fs.noteon(ch, note, v)
            elif etype == "note_off":
                self._fs.noteoff(ch, note)
        # Ensure any remaining sounding notes are silenced
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
    # def _play_note(self, note: Note, override_velocity: Optional[int]) -> None:
    #     ... legacy path removed ...


__all__ = ["SimpleMidiPlayer"]
