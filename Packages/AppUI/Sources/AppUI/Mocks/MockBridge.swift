#if DEBUG
import Foundation
import Bridge

public final class MockBridge: SessionEngine {
    private var storageRoot: URL?
    private var snapshot: ProfileSnapshot?
    private var activeSpec: SessionSpec?
    private var questions: [Question] = []
    private var currentIndex: Int = 0

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
        currentIndex = 0
        questions = makeQuestions(from: spec)
    }

    public func nextQuestion() throws -> Question? {
        guard currentIndex < questions.count else {
            return nil
        }
        return questions[currentIndex]
    }

    public func submit(_ answer: Answer) throws -> AttemptResult {
        guard currentIndex < questions.count else {
            throw BridgeError.engineError("No question available")
        }
        let question = questions[currentIndex]
        let correctId = question.choices.first?.id ?? "A"
        let isCorrect = answer.choiceId == correctId
        currentIndex += 1
        return AttemptResult(
            questionId: question.id,
            correct: isCorrect,
            correctAnswerId: correctId,
            scoreDelta: isCorrect ? 1 : 0,
            message: isCorrect ? "Nice!" : "Keep practicing"
        )
    }

    public func endSession() throws -> SessionSummary {
        let total = questions.count
        let correct = total // mock assumes perfect accuracy
        questions = []
        currentIndex = 0
        if var snapshot = snapshot {
            snapshot.totalSessions += 1
            snapshot.updatedAt = Date()
            self.snapshot = snapshot
        }
        return SessionSummary(totalQuestions: total, correctAnswers: correct, durationMs: 90_000)
    }

    public var hasActiveSession: Bool {
        currentIndex < questions.count
    }

    public func serializeCheckpoint() throws -> Checkpoint? {
        guard hasActiveSession else { return nil }
        return Checkpoint(currentQuestion: currentIndex, totalQuestions: questions.count, spec: activeSpec)
    }

    public func restore(checkpoint: Checkpoint) throws {
        activeSpec = checkpoint.spec ?? activeSpec
        if let spec = activeSpec {
            questions = makeQuestions(from: spec)
        }
        currentIndex = min(checkpoint.currentQuestion, questions.count)
    }

    private func makeQuestions(from spec: SessionSpec) -> [Question] {
        let basePrompt = spec.drill.capitalized
        return (0..<max(1, spec.count)).map { index in
            let id = "q\(index + 1)"
            let choiceA = Question.Choice(id: "A", text: "Option A")
            let choiceB = Question.Choice(id: "B", text: "Option B")
            let choiceC = Question.Choice(id: "C", text: "Option C")
            return Question(
                id: id,
                prompt: "\(basePrompt) question \(index + 1)",
                choices: [choiceA, choiceB, choiceC],
                metadata: ["set": spec.setId]
            )
        }
    }
}
#endif
