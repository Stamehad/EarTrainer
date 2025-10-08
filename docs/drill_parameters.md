# Drill Parameters Reference

This document lists the current, supported parameters for each drill family in the C++ core, how they’re interpreted, and example YAML snippets. Parameters are supplied in a `DrillSpec` under `params` (legacy name: `sampler_params`). Common “engine defaults” (tempo, key, range, assists) live under `defaults`.

Notes:
- Degrees are zero-based: `0..6` map to I..VII.
- Time knobs use beats; effective milliseconds are computed using tempo.
- The engine converts a drill’s `PromptPlan` to a platform‑agnostic `midi-clip` JSON at the API boundary.

## Common Defaults (all families)
- `defaults.key: string` — musical key for tonic/scale inference (e.g., `"C"`, `"Eb"`).
- `defaults.tempo_bpm: int` — tempo for prompts (and pathways if not overridden).
- `defaults.range_min: int`, `defaults.range_max: int` — absolute MIDI limits used by realization helpers (melody/chord); may be ignored by some note candidates (see per-family docs).
- `defaults.assistance_policy: { string:int }` — budget of assists exposed to the UI.

## Note Drill (family: `note`)
Lead note generation with optional “pathway” continuation.

Parameters (`params`):
- `allowed_degrees: [int]` — restrict lead degrees (0..6). If omitted, any degree is allowed.
- `avoid_repeat: bool` — try to avoid repeating the last degree/MIDI when possible (default: true).
- `range_below_semitones: int` — allowed semitone span below the central tonic (defaults to 12 if unspecified).
- `range_above_semitones: int` — allowed semitone span above the central tonic (defaults to 12 if unspecified).
- `note_range_semitones: int` — legacy symmetric window (if present, it populates both fields above).
- `use_pathway: bool` — if true, append a pathway after the lead note (see pathways.cpp bank).
- `pathway_repeat_lead: bool` — if true, re-emit the lead degree as the first pathway step.
- `pathway_step_beats: float` — duration in beats for each pathway step.
- `pathway_rest_beats: float` — a single rest inserted between the lead and the first pathway step (0 = no rest).
- `pathway_tempo_bpm: int` — tempo override for the pathway portion; falls back to `defaults.tempo_bpm`.
- `note_step_beats: float` — optional lead-note duration in beats (falls back to an internal default).
- `note_tempo_bpm: int` — optional tempo override for the lead; falls back to `defaults.tempo_bpm`.

Prompt shape:
- Lead: one note (dur from `note_step_beats` at `note_tempo_bpm` or `defaults.tempo_bpm`).
- Optional rest: `pathway_rest_beats` at pathway tempo.
- Pathway: sequence of notes, each `pathway_step_beats` at `pathway_tempo_bpm` (or defaults tempo).

Example (pathway I–IV):
```yaml
- id: PW_1_4_60
  family: note
  level: 1
  defaults:
    key: C
    tempo_bpm: 60
    range_min: 60
    range_max: 72
    assistance_policy: { Replay: 2, TempoDown: 1 }
  params:
    allowed_degrees: [0, 1, 2, 3]
    use_pathway: true
    pathway_repeat_lead: false
    pathway_step_beats: 1.0
    pathway_rest_beats: 0.5
```

## Interval Drill (family: `interval`)
Two-note intervals with ascending/descending prompt.

Parameters (`params`):
- `interval_allowed_bottom_degrees: [int]` — preferred filter for bottom degrees.
- `interval_allowed_degrees: [int]` or `allowed_degrees: [int]` — fallbacks if the above not provided.
- `interval_allowed_sizes: [int]` — interval sizes (1..7) to choose from.
- `interval_range_semitones: int` — candidate window for bottom note around central tonic.
- `interval_avoid_repeat: bool` — avoid repeated bottom degree/MIDI (falls back to `avoid_repeat`).
- `avoid_repeat: bool` — fallback for repeat avoidance.

Prompt shape:
- Two notes, ordered ascending or descending per sampled orientation.

Example:
```yaml
- id: INT_STEPWISE_72
  family: interval
  level: 2
  defaults: { key: C, tempo_bpm: 72, range_min: 55, range_max: 79, assistance_policy: { Replay: 2, GuideTone: 1 } }
  params:
    interval_allowed_sizes: [1,2,3,4]
    interval_range_semitones: 24
```

## Melody Drill (family: `melody`)
Weighted diatonic step model with musical modifiers and recent‑sequence suppression.

Parameters (`params`):
- `melody_length: int` — number of steps (default: 3; minimum: 1).

Prompt shape:
- Sequence of notes, `count_in: true`; per-note durations are fixed internally today.

Example:
```yaml
- id: MELODY_SHORT_60
  family: melody
  level: 1
  defaults: { key: C, tempo_bpm: 60, range_min: 60, range_max: 72, assistance_policy: { Replay: 2, TempoDown: 1 } }
  params: { melody_length: 2 }
```

## Chord Drill (family: `chord`)
Triad/voicing prompt (block chord) with simple degree/quality selection.

Parameters (`params`):
- `chord_allowed_degrees: [int]` — restrict root degrees (fallback: `allowed_degrees`).
- `chord_avoid_repeat: bool` — avoid repeated degree/voicing (fallback: `avoid_repeat`).
- `avoid_repeat: bool` — fallback for repeat avoidance.
- `add_seventh: bool` — optional flag in payload (generator currently triad‑based).
- `chord_range_semitones: int` — influences register mapping when realizing degrees to MIDI (via helpers).

Prompt shape:
- Block chord (‘midi_block’): simultaneous note_on with shared duration; bass + right‑hand voicing realized near targets.

Example:
```yaml
- id: CHORD_TRIADS_72
  family: chord
  level: 3
  defaults: { key: C, tempo_bpm: 72, range_min: 48, range_max: 84, assistance_policy: { Replay: 2, GuideTone: 2 } }
  params: { chord_range_semitones: 24 }
```

## Behavioral Notes
- Central tonic / register: helper functions compute `central_tonic_midi(key)` and then map degrees via `degree_to_offset`. Register constraints are enforced by per-family logic:
  - NoteDrill uses `range_below_semitones`/`range_above_semitones` (or the legacy `note_range_semitones`, which sets both) for candidate selection; results are also clamped to `defaults.range_min/max`.
  - Interval/Chord/Melody rely on helper realizations and may additionally clamp to `defaults.range_min/max`.
- Pathway logic: pathway content comes from the pathways bank (see `cpp/drills/pathways.cpp`), inferred from the `key`/scale. The `allowed_degrees` filter only applies to the lead degree for NoteDrill.
- Legacy naming: older YAML used `sampler_params`; the core now merges `drill_params` and `sampler_params` into `params` inside `DrillSpec`. Both are accepted for backward compatibility.

## Output Summary
- Each drill emits a `DrillOutput`:
  - `question`: TypedPayload (e.g., `"note"`, `"interval"`, `"melody"`, `"chord"`)
  - `correct_answer`: TypedPayload
  - `prompt`: PromptPlan (modality `"midi"` or `"midi_block"`); converted to `"midi-clip"` JSON by the bridge
  - `ui_hints`: includes `answer_kind`, `allowed_assists`, `assist_budget`
