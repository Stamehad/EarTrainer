import unittest

from copy import deepcopy

from eartrainer.models import ResultMetrics, ResultReport, SessionSpec
from eartrainer.session_engine import SessionEngine


class SessionEngineBridgeTests(unittest.TestCase):
    def test_basic_cycle(self) -> None:
        engine = SessionEngine()
        spec = SessionSpec(
            drill_kind="note",
            n_questions=1,
            generation="eager",
            assistance_policy={"GuideTone": 1},
            seed=321,
        )
        session_id = engine.create_session(spec)

        next_payload = engine.next_question(session_id)
        self.assertEqual(next_payload.question_id, "q-001")

        tonic = engine.session_assist(session_id, "Tonic")
        self.assertEqual(tonic.kind, "Tonic")
        self.assertIsNotNone(tonic.prompt_clip)

        assist_bundle = engine.assist(session_id, next_payload.question_id, "GuideTone")
        self.assertEqual(assist_bundle.question_id, next_payload.question_id)

        report = ResultReport(
            question_id=next_payload.question_id,
            final_answer=deepcopy(next_payload.correct_answer),
            correct=True,
            metrics=ResultMetrics(rt_ms=800, attempts=1, assists_used={}),
            client_info={"platform": "python-test"},
        )

        submission = engine.submit_result(session_id, report)
        if hasattr(submission, "results"):
            self.assertIsNotNone(submission.results)
        else:
            # When the engine returns the question bundle again, force completion and fetch summary
            engine.submit_result(session_id, report)
            summary = engine.next_question(session_id)
            self.assertTrue(hasattr(summary, "results"))


if __name__ == "__main__":
    unittest.main()
