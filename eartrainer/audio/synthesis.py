from __future__ import annotations

"""Abstract-ish audio synthesis interface.

Concrete implementations should provide basic note and chord playback
methods needed by the MVP drills.
"""

import time
from typing import List


class Synth:
    """Abstract-like synth interface for playback engines."""

    def __init__(self, sample_rate: int, gain: float) -> None:
        self.sample_rate = sample_rate
        self.gain = gain

    def program_piano(self) -> None:
        """Program a basic piano sound (General MIDI) if applicable."""
        raise NotImplementedError

    def note_on(self, midi: int, velocity: int = 100, dur_ms: int = 500) -> None:
        """Play a single note for a duration in milliseconds."""
        raise NotImplementedError

    def play_chord(self, midis: List[int], velocity: int = 90, dur_ms: int = 800) -> None:
        """Play a chord (simultaneous notes)."""
        raise NotImplementedError

    def sleep_ms(self, ms: int) -> None:
        time.sleep(ms / 1000.0)

    def close(self) -> None:
        """Release resources."""
        pass

    # Optional advanced channel controls (for drones)
    def select_program(self, channel: int, bank: int, preset: int) -> None:
        raise NotImplementedError

    def cc(self, channel: int, control: int, value: int) -> None:
        raise NotImplementedError

    def note_on_raw(self, channel: int, midi: int, velocity: int) -> None:
        raise NotImplementedError

    def note_off_raw(self, channel: int, midi: int) -> None:
        raise NotImplementedError

    def all_notes_off(self, channel: int) -> None:
        raise NotImplementedError
