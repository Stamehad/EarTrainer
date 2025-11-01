#if DEBUG
import Foundation
import Bridge

public final class MockBridge: SessionEngine {
    private var storageRoot: URL?
    private var snapshot: ProfileSnapshot?
    private var activeSpec: SessionSpec?
    private var questions: [QuestionBundle] = []
    private var currentIndex: Int = 0
    private var sessionIdentifier: String?
    private var cachedCatalog: [LevelCatalogEntry] = []

    public init() {}

    public func setStorageRoot(_ url: URL) throws {
        storageRoot = url
    }

    public func loadProfile(name: String) throws -> ProfileSnapshot {
        let snapshot = snapshot ?? ProfileSnapshot(name: name)
        self.snapshot = snapshot
        return snapshot
    }

    public func saveProfile(_ snapshot: ProfileSnapshot) throws {
        self.snapshot = snapshot
    }

    public func serializeProfile() throws -> ProfileSnapshot {
        if let snapshot {
            return snapshot
        }
        let generated = ProfileSnapshot(name: "Player")
        self.snapshot = generated
        return generated
    }

    public func startSession(_ spec: SessionSpec) throws {
        activeSpec = spec
        sessionIdentifier = "mock-\(UUID().uuidString)"
        currentIndex = 0
        questions = makeQuestions(from: spec)
    }

    public func nextQuestion() throws -> EngineResponse? {
        guard currentIndex < questions.count else {
            return .summary(makeSummary(total: questions.count, correct: currentIndex))
        }
        let bundle = questions[currentIndex]
        let debugInfo = JSONValue.object([
            "index": JSONValue.int(currentIndex),
            "remaining": JSONValue.int(max(0, questions.count - currentIndex - 1))
        ])
        return .question(QuestionEnvelope(bundle: bundle, debug: debugInfo))
    }

    public func submit(_ report: ResultReport) throws -> EngineResponse? {
        guard currentIndex < questions.count else {
            return .summary(makeSummary(total: questions.count, correct: currentIndex))
        }
        currentIndex += 1
        if currentIndex < questions.count {
            let bundle = questions[currentIndex]
            let debugInfo = JSONValue.object([
                "index": JSONValue.int(currentIndex),
                "submitted": JSONValue.object([
                    "question_id": JSONValue.string(report.questionId),
                    "correct": JSONValue.bool(report.correct)
                ])
            ])
            return .question(QuestionEnvelope(bundle: bundle, debug: debugInfo))
        } else {
            return .summary(makeSummary(total: questions.count, correct: currentIndex))
        }
    }

    public func endSession() throws -> EngineResponse? {
        let total = questions.count
        let correct = total // mock assumes perfect accuracy
        questions = []
        currentIndex = 0
        if var snapshot = snapshot {
            snapshot.totalSessions += 1
            snapshot.updatedAt = Date()
            self.snapshot = snapshot
        }
        return .summary(makeSummary(total: total, correct: correct))
    }

    public var hasActiveSession: Bool {
        sessionIdentifier != nil && currentIndex < questions.count
    }

    public func serializeCheckpoint() throws -> Checkpoint? {
        return nil
    }

    public func restore(checkpoint: Checkpoint) throws {
        activeSpec = checkpoint.spec
        currentIndex = 0
        sessionIdentifier = checkpoint.sessionId
        questions = makeQuestions(from: checkpoint.spec)
    }

    public func levelCatalogEntries(_ spec: SessionSpec) throws -> [LevelCatalogEntry] {
        if cachedCatalog.isEmpty {
            cachedCatalog = [
                LevelCatalogEntry(level: 1, tier: 0, label: "  1-0: MOCK_LEVEL_A"),
                LevelCatalogEntry(level: 1, tier: 1, label: "  1-1: MOCK_LEVEL_B"),
                LevelCatalogEntry(level: 2, tier: 0, label: "  2-0: MOCK_LEVEL_C"),
                LevelCatalogEntry(level: 112, tier: 1, label: "112-1: MOCK_LEVEL_D")
            ]
        }
        return cachedCatalog
    }

    public func orientationPrompt() throws -> MidiClip? {
        nil
    }

    public func assistOptions() throws -> [String] {
        ["Tonic", "ScaleArpeggio"]
    }

    public func assist(kind: String) throws -> AssistBundle? {
        return AssistBundle(questionId: "", kind: kind, promptClip: nil)
    }

    public func drillParamSpec() throws -> JSONValue {
        let intervalFields: [String: JSONValue] = [
            "tempo_bpm": .object(["label": .string("Tempo (BPM)"), "kind": .string("int"), "default": .int(60)]),
            "allowed_degrees": .object(["label": .string("Allowed degrees"), "kind": .string("int_list"), "default": .array([.int(0), .int(2), .int(4)])])
        ]
        return .object([
            "version": .string("v1"),
            "drills": .object([
                "interval": .object([
                    "id": .string("interval_params"),
                    "version": .int(1),
                    "fields": .object(intervalFields)
                ])
            ])
        ])
    }

    private func makeQuestions(from spec: SessionSpec) -> [QuestionBundle] {
        let count = max(1, spec.nQuestions)
        return (0..<count).map { index in
            let questionId = "q\(index + 1)"
            let choicePayload = JSONValue.array([
                JSONValue.object(["id": JSONValue.string("A"), "label": JSONValue.string("Choice A")]),
                JSONValue.object(["id": JSONValue.string("B"), "label": JSONValue.string("Choice B")]),
                JSONValue.object(["id": JSONValue.string("C"), "label": JSONValue.string("Choice C")])
            ])
            let question = JSONValue.object([
                "prompt": JSONValue.string("Question \(index + 1) for \(spec.drillKind.capitalized)"),
                "choices": choicePayload
            ])
            let answer = AnswerPayload.chord(ChordAnswer(rootDegree: 0))
            return QuestionBundle(
                questionId: questionId,
                question: question,
                correctAnswer: answer,
                promptClip: nil,
                uiHints: JSONValue.object([
                    "track_levels": JSONValue.array(spec.trackLevels.map { JSONValue.int($0) })
                ])
            )
        }
    }

    private func makeSummary(total: Int, correct: Int) -> SessionSummary {
        let totals = JSONValue.object([
            "total_questions": JSONValue.int(total),
            "correct": JSONValue.int(correct)
        ])
        return SessionSummary(
            sessionId: sessionIdentifier ?? "mock",
            totals: totals,
            byCategory: JSONValue.array([]),
            results: JSONValue.array([])
        )
    }
}
#endif
