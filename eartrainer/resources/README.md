SoundFonts
==========

The Python CLI uses FluidSynth and loads a SoundFont (`.sf2`) from the path configured under `audio.soundfont_path`.
There are two common setups you can use:

1) Grand Piano SoundFont (single‑instrument)
- Place a piano SoundFont at `./soundfonts/GrandPiano.sf2` (project root).
- Ensure `audio.soundfont_path` points to that file. The default in code falls back to `./soundfonts/GrandPiano.sf2` if the YAML value is missing.
- The synth selects General MIDI bank 0, preset 0 (Acoustic Grand) on channel 0.

2) General MIDI (GM) SoundFont
- Place a GM SoundFont at `./soundfonts/GM.sf2` and set `audio.soundfont_path: ./soundfonts/GM.sf2` in `eartrainer/config/defaults.yml`.
- This provides a full GM preset map (e.g., Acoustic Grand on program 0, Strings on program 48, etc.).
- The CLI programs channel 0 to Acoustic Grand automatically; the drone feature (if used) can select strings via the GM preset map defined in `eartrainer/resources/drones/strings_basic.yml`.

Important
- The application does not require any specific file name, but the defaults assume `./soundfonts/GrandPiano.sf2` or `./soundfonts/GM.sf2` exist. If you use different filenames or locations, update `audio.soundfont_path` accordingly.
- If the configured file does not exist, the app exits with an error. Check `eartrainer/config/config.py` for details.

Where to configure
- Default YAML: `eartrainer/config/defaults.yml` (edit `audio.soundfont_path`).
- Runtime check and fallback defaults: `eartrainer/config/config.py`.

Licensing
- Ensure you have the rights to use and distribute any SoundFont files you include. Many SoundFonts are licensed and not redistributable.
