import Foundation

public struct Settings: Codable, Equatable {
    public var showLatency: Bool
    public var enableAssistance: Bool
    public var useCBOR: Bool

    public init(showLatency: Bool = false, enableAssistance: Bool = false, useCBOR: Bool = false) {
        self.showLatency = showLatency
        self.enableAssistance = enableAssistance
        self.useCBOR = useCBOR
    }
}

public struct SessionSpec: Codable, Equatable {
    public var drill: String
    public var setId: String
    public var count: Int
    public var seed: Int?

    public init(drill: String = "intervals", setId: String = "default", count: Int = 2, seed: Int? = nil) {
        self.drill = drill
        self.setId = setId
        self.count = count
        self.seed = seed
    }
}

public struct Question: Codable, Equatable, Identifiable {
    public struct Choice: Codable, Equatable, Identifiable {
        public var id: String
        public var text: String
        public init(id: String, text: String) {
            self.id = id
            self.text = text
        }
    }

    public var id: String
    public var prompt: String
    public var choices: [Choice]
    public var metadata: [String: String]?

    public init(id: String, prompt: String, choices: [Choice], metadata: [String: String]? = nil) {
        self.id = id
        self.prompt = prompt
        self.choices = choices
        self.metadata = metadata
    }
}

public struct Answer: Codable, Equatable {
    public var questionId: String
    public var choiceId: String
    public var latencyMs: Int
    public var metadata: [String: String]?

    public init(questionId: String, choiceId: String, latencyMs: Int, metadata: [String: String]? = nil) {
        self.questionId = questionId
        self.choiceId = choiceId
        self.latencyMs = latencyMs
        self.metadata = metadata
    }
}

public struct AttemptResult: Codable, Equatable {
    public var questionId: String
    public var correct: Bool
    public var correctAnswerId: String?
    public var scoreDelta: Int
    public var message: String?
    public var metadata: [String: String]?

    public init(
        questionId: String,
        correct: Bool,
        correctAnswerId: String? = nil,
        scoreDelta: Int = 0,
        message: String? = nil,
        metadata: [String: String]? = nil
    ) {
        self.questionId = questionId
        self.correct = correct
        self.correctAnswerId = correctAnswerId
        self.scoreDelta = scoreDelta
        self.message = message
        self.metadata = metadata
    }
}

public struct SessionSummary: Codable, Equatable {
    public var totalQuestions: Int
    public var correctAnswers: Int
    public var durationMs: Int
    public var metadata: [String: String]?

    public init(
        totalQuestions: Int,
        correctAnswers: Int,
        durationMs: Int,
        metadata: [String: String]? = nil
    ) {
        self.totalQuestions = totalQuestions
        self.correctAnswers = correctAnswers
        self.durationMs = durationMs
        self.metadata = metadata
    }
}

public struct ProfileSnapshot: Codable, Equatable {
    public var name: String
    public var totalSessions: Int
    public var settings: Settings
    public var updatedAt: Date

    public init(
        name: String,
        totalSessions: Int = 0,
        settings: Settings = Settings(),
        updatedAt: Date = Date()
    ) {
        self.name = name
        self.totalSessions = totalSessions
        self.settings = settings
        self.updatedAt = updatedAt
    }
}

public struct Checkpoint: Codable, Equatable {
    public var currentQuestion: Int
    public var totalQuestions: Int
    public var spec: SessionSpec?

    public init(currentQuestion: Int, totalQuestions: Int, spec: SessionSpec? = nil) {
        self.currentQuestion = currentQuestion
        self.totalQuestions = totalQuestions
        self.spec = spec
    }
}
