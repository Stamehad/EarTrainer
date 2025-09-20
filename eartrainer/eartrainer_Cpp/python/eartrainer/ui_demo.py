from __future__ import annotations

import sys
from .models import (
    AssistBundle,
    QuestionBundle,
    ResultMetrics,
    ResultReport,
    SessionSpec,
    SessionSummary,
    TypedPayload,
)
from .session_engine import SessionEngine


def _print_prompt(bundle: QuestionBundle) -> None:
    prompt = bundle.prompt
    if prompt is None:
        return
    note_desc = ", ".join(str(note.pitch) for note in prompt.notes)
    print(f"  Prompt modality: {prompt.modality} notes=[{note_desc}]")


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
        print(f"Question {bundle.question_id} ({bundle.question.type})")
        _print_prompt(bundle)

        while True:
            user = input("Your answer> ").strip()
            if user.lower() == "quit":
                print("Exiting session.")
                sys.exit(0)
            if user.startswith("assist "):
                _, kind = user.split(maxsplit=1)
                assist_payload = engine.assist(session_id, bundle.question_id, kind)
                if isinstance(assist_payload, AssistBundle):
                    print(f"Assist[{kind}]: {assist_payload.ui_delta.get('message')}")
                continue
            break

        expected_payload = bundle.correct_answer.payload
        if isinstance(expected_payload, dict) and expected_payload:
            answer_key = next(iter(expected_payload.keys()))
            expected_value = expected_payload[answer_key]
        else:
            answer_key = "value"
            expected_value = expected_payload
        correct = str(user).strip().lower() == str(expected_value).strip().lower()

        metrics = ResultMetrics(rt_ms=1200, attempts=1, assists_used={}, first_input_rt_ms=900)
        report = ResultReport(
            question_id=bundle.question_id,
            final_answer=TypedPayload(type=bundle.correct_answer.type, payload={answer_key: user}),
            correct=correct,
            metrics=metrics,
            client_info={"source": "ui_demo"},
        )
        engine.submit_result(session_id, report)

    print("\nThanks for trying the demo!")


if __name__ == "__main__":  # pragma: no cover - manual run
    main()
