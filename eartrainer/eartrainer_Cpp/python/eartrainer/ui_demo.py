from __future__ import annotations

import sys
from .models import (
    AssistBundle,
    ChordAnswer,
    HarmonyAnswer,
    MelodyAnswer,
    QuestionBundle,
    ResultMetrics,
    ResultReport,
    SessionSpec,
    SessionSummary,
)
from .session_engine import SessionEngine


def _print_prompt(bundle: QuestionBundle) -> None:
    clip = bundle.prompt_clip
    if clip is None:
        return
    track_names = ", ".join(track.name for track in clip.tracks)
    print(f"  Prompt clip: tempo={clip.tempo_bpm} bpm tracks=[{track_names}]")


def main() -> None:
    engine = SessionEngine()
    spec = SessionSpec(
        drill_kind="note",
        n_questions=3,
        generation="adaptive",
        assistance_policy={"GuideTone": 1, "Replay": 3},
        seed=123,
    )
    session_id = engine.create_session(spec)
    print("Starting ear training demo. Type 'assist KIND' for help or 'quit' to exit.\n")

    while True:
        next_payload = engine.next_question(session_id)
        if isinstance(next_payload, SessionSummary):
            summary = next_payload
            print("Session complete!")
            correct = summary.totals.get("correct", 0)
            incorrect = summary.totals.get("incorrect", 0)
            print(f"Correct: {correct} / {correct + incorrect}")
            print(f"Average RT(ms): {summary.totals.get('avg_rt_ms')}")
            break

        bundle = next_payload
        print(f"Question {bundle.question_id}")
        _print_prompt(bundle)

        while True:
            user = input("Your answer> ").strip()
            if user.lower() == "quit":
                print("Exiting session.")
                sys.exit(0)
            if user.startswith("assist "):
                _, kind = user.split(maxsplit=1)
                assist_payload = engine.assist(session_id, kind)
                if isinstance(assist_payload, AssistBundle):
                    print(f"Assist[{kind}] available (clip={'yes' if assist_payload.prompt_clip else 'no'})")
                continue
            break

        expected = bundle.correct_answer
        correct = False
        final_answer = expected
        try:
            if isinstance(expected, ChordAnswer):
                expected_value = str(expected.root_degree)
                correct = user.strip() == expected_value
                final_answer = ChordAnswer.single(root_degree=int(user))
            elif isinstance(expected, MelodyAnswer):
                expected_value = " ".join(str(v) for v in expected.melody)
                entered = [int(part) for part in user.split() if part.strip()]
                correct = entered == expected.melody
                final_answer = MelodyAnswer(melody=entered)
            elif isinstance(expected, HarmonyAnswer):
                expected_value = " ".join(str(v) for v in expected.notes)
                entered = [int(part) for part in user.split() if part.strip()]
                correct = entered == expected.notes
                final_answer = HarmonyAnswer(notes=entered)
        except ValueError:
            correct = False
            final_answer = expected

        metrics = ResultMetrics(rt_ms=1200, attempts=1, assists_used={}, first_input_rt_ms=900)
        report = ResultReport(
            question_id=bundle.question_id,
            final_answer=final_answer,
            correct=correct,
            metrics=metrics,
            client_info={"source": "ui_demo"},
        )
        engine.submit_result(session_id, report)

    print("\nThanks for trying the demo!")


if __name__ == "__main__":  # pragma: no cover - manual run
    main()
