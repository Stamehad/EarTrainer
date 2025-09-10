# Theory: Scale, Chord, Voicing

```mermaid
flowchart LR
  Scale -->|degree_to_pc| PC[Pitch Class]
  Scale -->|degree_to_chord_spec| Spec[ChordSpec]
  Spec --> Chord
  Chord -->|quality+ext| VS[VoicingSelector]
  VS --> VB[VoicingBank]
  VS -->|render| MIDI
```

- `Scale`: degree mapping and `ChordSpec` creation.
- `Chord`: concrete chord (root, quality, extensions).
- `VoicingBank`: template catalog (right hand, bass; tags).
- `VoicingSelector`: chooses a template and renders to MIDI with register policy and clamping.
