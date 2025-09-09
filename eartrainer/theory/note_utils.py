# eartrainer/theory/note_utils.py
from __future__ import annotations
from typing import Dict

PITCH_CLASS_NAMES_SHARP = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
NAME_TO_PC: Dict[str, int] = {
    "C":0,"B#":0, "C#":1,"Db":1, "D":2,"D#":3,"Eb":3, "E":4,"Fb":4,
    "F":5,"E#":5, "F#":6,"Gb":6, "G":7,"G#":8,"Ab":8, "A":9,"A#":10,"Bb":10, "B":11,"Cb":11
}

DEGREE_TO_SEMITONE = {
    "1": 0, "b2":1, "2":2, "#2":3, "b3":3, "3":4, "4":5, "#4":6, "b5":6, "5":7,
    "#5":8, "b6":8, "6":9, "bb7":9, "b7":10, "7":11
}

def note_name_to_midi(name: str, octave: int) -> int:
    """Middle C (C4) -> 60."""
    pc = NAME_TO_PC[name]
    return 12 * (octave + 1) + pc  # C4=60

def add_semitones(midi: int, semitones: int) -> int:
    return midi + semitones