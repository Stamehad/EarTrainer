from __future__ import annotations

"""Entry point for the upcoming EarTrainer GUI.

This is a minimal stub so packaging and entry-points resolve while the new
interface is under development. It performs a lightweight connectivity check
against the C++ SessionEngine bindings and then informs the user that the GUI
is not implemented yet.
"""

from typing import Sequence

try:
    from eartrainer.eartrainer_Cpp.python.eartrainer import SessionEngine
except ImportError:  # pragma: no cover - until bindings ship everywhere
    SessionEngine = None  # type: ignore


def main(argv: Sequence[str] | None = None) -> int:
    if SessionEngine is None:
        print(
            "EarTrainer GUI scaffolding is in place, but the C++ SessionEngine "
            "bindings are not importable yet."
        )
        return 1

    # Simple smoke check so downstream tooling knows the bindings are wired up.
    try:
        engine = SessionEngine()
        caps = engine.capabilities()
    except Exception as exc:  # pragma: no cover - interactive feedback only
        print("Failed to communicate with SessionEngine bindings:", exc)
        return 1

    print("EarTrainer GUI TODO")
    print("SessionEngine capabilities detected:")
    for key, value in caps.items():
        print(f"  - {key}: {value}")
    print("Implement the new GUI in eartrainer/python/gui as needed.")
    return 0


if __name__ == "__main__":  # pragma: no cover - manual execution helper
    raise SystemExit(main())
