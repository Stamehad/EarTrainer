# EarTrainer — Overview

Short, high‑level map of the current system.

```mermaid
flowchart TD
  CLI[CLI] --> SM[SessionManager]
  SM --> DR[DrillRegistry]
  SM --> S[Synth]
  SM --> DM[DroneManager]
  SM --> D(Drill)
  D -->|music logic| Theory[Scale/Chord/Voicing]
  SM --> RM[ResultManager]
```

- CLI: entrypoint `eartrainer` (run/list/show-params)
- SessionManager: orchestrates session, builds services, runs drill
- DrillRegistry: discovers drills and presets
- Drill: current NoteDegreeDrill (single‑note degree ID)
- Theory: Scale, Chord, VoicingBank/Selector for cadence
- Audio: FluidSynth Synth + DroneManager
- Results: scaffolded manager (in‑memory for now)
