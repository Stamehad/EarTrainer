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

public enum JSONValue: Codable, Equatable {
    case string(String)
    case int(Int)
    case double(Double)
    case bool(Bool)
    case array([JSONValue])
    case object([String: JSONValue])
    case null

    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if container.decodeNil() {
            self = .null
        } else if let value = try? container.decode(Bool.self) {
            self = .bool(value)
        } else if let value = try? container.decode(Int.self) {
            self = .int(value)
        } else if let value = try? container.decode(Double.self) {
            self = .double(value)
        } else if let value = try? container.decode(String.self) {
            self = .string(value)
        } else if let value = try? container.decode([JSONValue].self) {
            self = .array(value)
        } else if let value = try? container.decode([String: JSONValue].self) {
            self = .object(value)
        } else {
            throw DecodingError.dataCorruptedError(in: container, debugDescription: "Unsupported JSON value")
        }
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case let .string(value):
            try container.encode(value)
        case let .int(value):
            try container.encode(value)
        case let .double(value):
            try container.encode(value)
        case let .bool(value):
            try container.encode(value)
        case let .array(values):
            try container.encode(values)
        case let .object(values):
            try container.encode(values)
        case .null:
            try container.encodeNil()
        }
    }

    public var isNull: Bool {
        if case .null = self { return true }
        return false
    }

    public func prettyPrinted() -> String {
        let anyValue = toFoundation()
        guard JSONSerialization.isValidJSONObject(anyValue) else {
            return descriptionFallback()
        }
        guard let data = try? JSONSerialization.data(withJSONObject: anyValue, options: [.prettyPrinted, .sortedKeys]),
              let string = String(data: data, encoding: .utf8) else {
            return descriptionFallback()
        }
        return string
    }

    private func descriptionFallback() -> String {
        switch self {
        case let .string(value):
            return value
        case let .int(value):
            return String(value)
        case let .double(value):
            return String(value)
        case let .bool(value):
            return String(value)
        case let .array(values):
            return "[\(values.map { $0.descriptionFallback() }.joined(separator: ", "))]"
        case let .object(values):
            let content = values
                .sorted { $0.key < $1.key }
                .map { "\($0.key): \($0.value.descriptionFallback())" }
                .joined(separator: ", ")
            return "{\(content)}"
        case .null:
            return "null"
        }
    }

    private func toFoundation() -> Any {
        switch self {
        case let .string(value):
            return value
        case let .int(value):
            return value
        case let .double(value):
            return value
        case let .bool(value):
            return value
        case let .array(values):
            return values.map { $0.toFoundation() }
        case let .object(values):
            return values.mapValues { $0.toFoundation() }
        case .null:
            return NSNull()
        }
    }
}

public struct SessionSpec: Codable, Equatable {
    public var version: String
    public var drillKind: String
    public var key: String
    public var range: [Int]
    public var tempoBpm: Int?
    public var nQuestions: Int
    public var generation: String
    public var assistancePolicy: [String: Int]
    public var samplerParams: [String: JSONValue]
    public var params: [String: JSONValue]?
    public var seed: Int
    public var adaptive: Bool
    public var trackLevels: [Int]
    public var levelInspect: Bool
    public var inspectLevel: Int?
    public var inspectTier: Int?

    public init(
        version: String = "v1",
        drillKind: String = "interval",
        key: String = "C major",
        range: [Int] = [48, 72],
        tempoBpm: Int? = nil,
        nQuestions: Int = 5,
        generation: String = "adaptive",
        assistancePolicy: [String: Int] = [:],
        samplerParams: [String: JSONValue] = [:],
        params: [String: JSONValue]? = nil,
        seed: Int = 1,
        adaptive: Bool = true,
        trackLevels: [Int] = [1],
        levelInspect: Bool = false,
        inspectLevel: Int? = nil,
        inspectTier: Int? = nil
    ) {
        self.version = version
        self.drillKind = drillKind
        self.key = key
        self.range = range
        self.tempoBpm = tempoBpm
        self.nQuestions = nQuestions
        self.generation = generation
        self.assistancePolicy = assistancePolicy
        self.samplerParams = samplerParams
        self.params = params
        self.seed = seed
        self.adaptive = adaptive
        self.trackLevels = trackLevels
        self.levelInspect = levelInspect
        self.inspectLevel = inspectLevel
        self.inspectTier = inspectTier
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        version = try container.decodeIfPresent(String.self, forKey: .version) ?? "v1"
        drillKind = try container.decodeIfPresent(String.self, forKey: .drillKind) ?? "interval"
        key = try container.decodeIfPresent(String.self, forKey: .key) ?? "C major"
        range = try container.decodeIfPresent([Int].self, forKey: .range) ?? [48, 72]
        tempoBpm = try container.decodeIfPresent(Int.self, forKey: .tempoBpm)
        nQuestions = try container.decodeIfPresent(Int.self, forKey: .nQuestions) ?? 5
        generation = try container.decodeIfPresent(String.self, forKey: .generation) ?? "adaptive"
        assistancePolicy = try container.decodeIfPresent([String: Int].self, forKey: .assistancePolicy) ?? [:]
        samplerParams = try container.decodeIfPresent([String: JSONValue].self, forKey: .samplerParams) ?? [:]
        params = try container.decodeIfPresent([String: JSONValue].self, forKey: .params)
        seed = try container.decodeIfPresent(Int.self, forKey: .seed) ?? 1
        adaptive = try container.decodeIfPresent(Bool.self, forKey: .adaptive) ?? false
        trackLevels = try container.decodeIfPresent([Int].self, forKey: .trackLevels) ?? [1]
        levelInspect = try container.decodeIfPresent(Bool.self, forKey: .levelInspect) ?? false
        inspectLevel = try container.decodeIfPresent(Int.self, forKey: .inspectLevel)
        inspectTier = try container.decodeIfPresent(Int.self, forKey: .inspectTier)
    }

    private enum CodingKeys: String, CodingKey {
        case version
        case drillKind = "drill_kind"
        case key
        case range
        case tempoBpm = "tempo_bpm"
        case nQuestions = "n_questions"
        case generation
        case assistancePolicy = "assistance_policy"
        case samplerParams = "sampler_params"
        case params
        case seed
        case adaptive
        case trackLevels = "track_levels"
        case levelInspect = "level_inspect"
        case inspectLevel = "inspect_level"
        case inspectTier = "inspect_tier"
    }
}

public struct LevelCatalogEntry: Codable, Equatable, Identifiable {
    public var level: Int
    public var tier: Int
    public var label: String

    public init(level: Int, tier: Int, label: String) {
        self.level = level
        self.tier = tier
        self.label = label
    }

    public var id: String {
        "\(level)-\(tier)"
    }
}

public struct TypedPayload: Codable, Equatable {
    public var type: String
    public var payload: JSONValue

    public init(type: String, payload: JSONValue) {
        self.type = type
        self.payload = payload
    }
}

public struct MidiEvent: Codable, Equatable {
    public var t: Int
    public var type: String
    public var note: Int?
    public var vel: Int?
    public var control: Int?
    public var value: Int?
}

public struct MidiTrack: Codable, Equatable {
    public var name: String
    public var channel: Int
    public var program: Int?
    public var events: [MidiEvent]
}

public struct MidiClip: Codable, Equatable {
    public var format: String
    public var ppq: Int
    public var tempoBpm: Int
    public var lengthTicks: Int
    public var tracks: [MidiTrack]

    private enum CodingKeys: String, CodingKey {
        case format
        case ppq
        case tempoBpm = "tempo_bpm"
        case lengthTicks = "length_ticks"
        case tracks
    }
}

public struct ChordAnswer: Equatable {
    public var rootDegree: Int
    public var bassDeg: Int?
    public var topDeg: Int?

    public init(rootDegree: Int, bassDeg: Int? = nil, topDeg: Int? = nil) {
        self.rootDegree = rootDegree
        self.bassDeg = bassDeg
        self.topDeg = topDeg
    }
}

public struct MelodyAnswer: Equatable {
    public var melody: [Int]

    public init(melody: [Int]) {
        self.melody = melody
    }
}

public struct HarmonyAnswer: Equatable {
    public var notes: [Int]

    public init(notes: [Int]) {
        self.notes = notes
    }
}

public enum AnswerPayload: Equatable {
    case chord(ChordAnswer)
    case melody(MelodyAnswer)
    case harmony(HarmonyAnswer)
}

extension AnswerPayload: Codable {
    private enum CodingKeys: String, CodingKey {
        case type
        case rootDegree = "root_degree"
        case bassDeg = "bass_deg"
        case topDeg = "top_deg"
        case melody
        case notes
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let type = try container.decode(String.self, forKey: .type)
        switch type {
        case "chord":
            let answer = ChordAnswer(
                rootDegree: try container.decode(Int.self, forKey: .rootDegree),
                bassDeg: try container.decodeIfPresent(Int.self, forKey: .bassDeg),
                topDeg: try container.decodeIfPresent(Int.self, forKey: .topDeg)
            )
            self = .chord(answer)
        case "melody":
            let answer = MelodyAnswer(melody: try container.decode([Int].self, forKey: .melody))
            self = .melody(answer)
        case "harmony":
            let answer = HarmonyAnswer(notes: try container.decode([Int].self, forKey: .notes))
            self = .harmony(answer)
        default:
            throw DecodingError.dataCorruptedError(forKey: .type, in: container, debugDescription: "Unknown answer payload type \(type)")
        }
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        switch self {
        case let .chord(answer):
            try container.encode("chord", forKey: .type)
            try container.encode(answer.rootDegree, forKey: .rootDegree)
            try container.encodeIfPresent(answer.bassDeg, forKey: .bassDeg)
            try container.encodeIfPresent(answer.topDeg, forKey: .topDeg)
        case let .melody(answer):
            try container.encode("melody", forKey: .type)
            try container.encode(answer.melody, forKey: .melody)
        case let .harmony(answer):
            try container.encode("harmony", forKey: .type)
            try container.encode(answer.notes, forKey: .notes)
        }
    }
}

public extension AnswerPayload {
    var type: String {
        switch self {
        case .chord:
            return "chord"
        case .melody:
            return "melody"
        case .harmony:
            return "harmony"
        }
    }

    var jsonValue: JSONValue {
        switch self {
        case let .chord(answer):
            var object: [String: JSONValue] = [
                "type": .string("chord"),
                "root_degree": .int(answer.rootDegree)
            ]
            object["bass_deg"] = answer.bassDeg.map(JSONValue.int) ?? .null
            object["top_deg"] = answer.topDeg.map(JSONValue.int) ?? .null
            return .object(object)
        case let .melody(answer):
            return .object([
                "type": .string("melody"),
                "melody": .array(answer.melody.map(JSONValue.int))
            ])
        case let .harmony(answer):
            return .object([
                "type": .string("harmony"),
                "notes": .array(answer.notes.map(JSONValue.int))
            ])
        }
    }
}

public struct QuestionBundle: Codable, Equatable {
    public var questionId: String
    public var question: JSONValue?
    public var correctAnswer: AnswerPayload
    public var promptClip: MidiClip?
    public var uiHints: JSONValue

    public init(
        questionId: String,
        question: JSONValue? = nil,
        correctAnswer: AnswerPayload,
        promptClip: MidiClip? = nil,
        uiHints: JSONValue = .null
    ) {
        self.questionId = questionId
        self.question = question
        self.correctAnswer = correctAnswer
        self.promptClip = promptClip
        self.uiHints = uiHints
    }

    private enum CodingKeys: String, CodingKey {
        case questionId = "question_id"
        case question
        case correctAnswer = "correct_answer"
        case promptClip = "prompt_clip"
        case uiHints = "ui_hints"
    }
}

public struct AssistBundle: Codable, Equatable {
    public var questionId: String
    public var kind: String
    public var promptClip: MidiClip?

    public init(questionId: String, kind: String, promptClip: MidiClip? = nil) {
        self.questionId = questionId
        self.kind = kind
        self.promptClip = promptClip
    }

    private enum CodingKeys: String, CodingKey {
        case questionId = "question_id"
        case kind
        case promptClip = "prompt_clip"
    }
}

public struct ResultMetrics: Codable, Equatable {
    public var rtMs: Int
    public var attempts: Int
    public var questionCount: Int
    public var assistsUsed: [String: Int]
    public var firstInputRtMs: Int?

    public init(
        rtMs: Int,
        attempts: Int,
        questionCount: Int = 1,
        assistsUsed: [String: Int] = [:],
        firstInputRtMs: Int? = nil
    ) {
        self.rtMs = rtMs
        self.attempts = attempts
        self.questionCount = questionCount
        self.assistsUsed = assistsUsed
        self.firstInputRtMs = firstInputRtMs
    }

    private enum CodingKeys: String, CodingKey {
        case rtMs = "rt_ms"
        case attempts
        case questionCount = "question_count"
        case assistsUsed = "assists_used"
        case firstInputRtMs = "first_input_rt_ms"
    }
}

public struct ResultAttempt: Codable, Equatable {
    public var label: String
    public var correct: Bool
    public var attempts: Int
    public var answerFragment: TypedPayload?
    public var expectedFragment: TypedPayload?

    private enum CodingKeys: String, CodingKey {
        case label
        case correct
        case attempts
        case answerFragment = "answer_fragment"
        case expectedFragment = "expected_fragment"
    }
}

public struct ResultReport: Codable, Equatable {
    public var questionId: String
    public var finalAnswer: AnswerPayload
    public var correct: Bool
    public var metrics: ResultMetrics
    public var attempts: [ResultAttempt]
    public var clientInfo: [String: JSONValue]

    public init(
        questionId: String,
        finalAnswer: AnswerPayload,
        correct: Bool,
        metrics: ResultMetrics,
        attempts: [ResultAttempt] = [],
        clientInfo: [String: JSONValue] = [:]
    ) {
        self.questionId = questionId
        self.finalAnswer = finalAnswer
        self.correct = correct
        self.metrics = metrics
        self.attempts = attempts
        self.clientInfo = clientInfo
    }

    private enum CodingKeys: String, CodingKey {
        case questionId = "question_id"
        case finalAnswer = "final_answer"
        case correct
        case metrics
        case attempts
        case clientInfo = "client_info"
    }
}

public struct SessionSummary: Codable, Equatable {
    public var sessionId: String
    public var totals: JSONValue
    public var byCategory: JSONValue
    public var results: JSONValue

    public init(sessionId: String, totals: JSONValue, byCategory: JSONValue, results: JSONValue) {
        self.sessionId = sessionId
        self.totals = totals
        self.byCategory = byCategory
        self.results = results
    }

    private enum CodingKeys: String, CodingKey {
        case sessionId = "session_id"
        case totals
        case byCategory = "by_category"
        case results
    }
}

public struct MemoryPackage: Codable, Equatable {
    public var summary: SessionSummary
    public var adaptive: JSONValue?
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
    public var sessionId: String
    public var spec: SessionSpec

    public init(sessionId: String, spec: SessionSpec) {
        self.sessionId = sessionId
        self.spec = spec
    }
}

public struct QuestionEnvelope: Equatable {
    public var bundle: QuestionBundle
    public var debug: JSONValue?

    public init(bundle: QuestionBundle, debug: JSONValue? = nil) {
        self.bundle = bundle
        self.debug = debug
    }
}

public enum EngineResponse: Equatable {
    case question(QuestionEnvelope)
    case summary(SessionSummary)
}
