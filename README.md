# EarTrainer (MVP CLI)

Functional ear training MVP with a simple CLI that plays a cadence and runs a single-note degree identification drill.

## Install

```
python -m venv .venv && source .venv/bin/activate
pip install -e .
```

Place a SoundFont at `./soundfonts/GrandPiano.sf2` (or update path in `eartrainer/config/defaults.yml`).

## Run

```
eartrainer --config eartrainer/config/defaults.yml
```

What works in MVP:

- Cadence playback (I–V7–I)
- Single note degree drill (50 Q)
- Stats JSON + printed summary

See `eartrainer/resources/README.md` for SoundFont details.

