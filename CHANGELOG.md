# Changelog

All notable changes to this project will be documented in this file.

## v0.3.0 – New CLI + SessionManager; drone auto-restart; improved randomness
- Switch console entrypoint to `eartrainer.app.cli:main`.
- Add SessionManager and DrillRegistry; presets for the note drill.
- Integrate DroneManager with UI activity: auto-restart after inactivity timeout; optional ducking during cadence.
- Improve note drill randomness with a shuffle-bag and no immediate repeats.
- Keep existing behavior (cadence + single-note degree drill) with new orchestration.

## v0.2.0 – OO voicing cadence + drone feature
- Cadence uses OO voicing path (Scale/Chord + VoicingBank + VoicingSelector).
- Add voicing templates for piano; right-hand range clamping.
- Introduce DroneManager (strings) with templates, fade in/out, inactivity timeout.
- FluidSynth wrapper gains per-channel program/CC and raw note on/off.

## v0.1.0 – Initial MVP
- CLI with I–V7–I cadence and single-note degree drill (50 questions default).
- FluidSynth playback with SoundFont; stats JSON output and printed summary.
