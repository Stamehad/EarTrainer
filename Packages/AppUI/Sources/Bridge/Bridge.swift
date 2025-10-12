import Foundation
import CEarTrainerBridge

public protocol SessionEngine {
    func setStorageRoot(_ url: URL) throws
    func loadProfile(name: String) throws -> ProfileSnapshot
    func saveProfile(_ snapshot: ProfileSnapshot) throws
    func serializeProfile() throws -> ProfileSnapshot
    func startSession(_ spec: SessionSpec) throws
    func nextQuestion() throws -> Question?
    func submit(_ answer: Answer) throws -> AttemptResult
    func endSession() throws -> SessionSummary
    var hasActiveSession: Bool { get }
    func serializeCheckpoint() throws -> Checkpoint?
    func restore(checkpoint: Checkpoint) throws
}

public final class Bridge: SessionEngine {
    private let encoder: JSONEncoder
    private let decoder: JSONDecoder

    public convenience init() {
        self.init(encoder: Bridge.makeEncoder(), decoder: Bridge.makeDecoder())
    }

    public init(encoder: JSONEncoder, decoder: JSONDecoder) {
        self.encoder = encoder
        self.decoder = decoder
    }

    // MARK: - SessionEngine

    public func setStorageRoot(_ url: URL) throws {
        try url.path.withCString { pointer in
            try callStatus { set_storage_root(pointer) }
        }
    }

    public func loadProfile(name: String) throws -> ProfileSnapshot {
        let result = try name.withCString { pointer in
            try callString { load_profile(pointer) }
        }
        guard let result else {
            throw BridgeError.missingData("Profile")
        }
        return try decode(ProfileSnapshot.self, from: result)
    }

    public func saveProfile(_ snapshot: ProfileSnapshot) throws {
        let payload = try encode(snapshot)
        try payload.withCString { pointer in
            try callStatus { deserialize_profile(pointer) }
        }
    }

    public func serializeProfile() throws -> ProfileSnapshot {
        guard let result = try callString({ serialize_profile() }) else {
            throw BridgeError.missingData("Profile serialization")
        }
        return try decode(ProfileSnapshot.self, from: result)
    }

    public func startSession(_ spec: SessionSpec) throws {
        let payload = try encode(spec)
        try payload.withCString { pointer in
            try callStatus { start_session(pointer) }
        }
    }

    public func nextQuestion() throws -> Question? {
        guard let payload = try callString({ next_question() }) else {
            return nil
        }
        return try decode(Question.self, from: payload)
    }

    public func submit(_ answer: Answer) throws -> AttemptResult {
        let payload = try encode(answer)
        let response = try payload.withCString { pointer in
            try callString { feedback(pointer) }
        }
        guard let response else {
            throw BridgeError.missingData("AttemptResult")
        }
        return try decode(AttemptResult.self, from: response)
    }

    public func endSession() throws -> SessionSummary {
        guard let payload = try callString({ end_session() }) else {
            throw BridgeError.missingData("SessionSummary")
        }
        return try decode(SessionSummary.self, from: payload)
    }

    public var hasActiveSession: Bool {
        has_active_session() != 0
    }

    public func serializeCheckpoint() throws -> Checkpoint? {
        guard let payload = try callString({ serialize_checkpoint() }) else {
            return nil
        }
        return try decode(Checkpoint.self, from: payload)
    }

    public func restore(checkpoint: Checkpoint) throws {
        let payload = try encode(checkpoint)
        try payload.withCString { pointer in
            try callStatus { deserialize_checkpoint(pointer) }
        }
    }

    // MARK: - Persistence helpers

    public func loadCheckpointIfAny(from url: URL) throws -> Checkpoint? {
        guard FileManager.default.fileExists(atPath: url.path) else {
            return nil
        }
        let data = try Data(contentsOf: url)
        let checkpoint = try decoder.decode(Checkpoint.self, from: data)
        try restore(checkpoint: checkpoint)
        return checkpoint
    }

    public func saveCheckpoint(_ checkpoint: Checkpoint, to url: URL) throws {
        let data = try encoder.encode(checkpoint)
        try data.write(to: url, options: .atomic)
    }

    public func clearCheckpoint(at url: URL) throws {
        if FileManager.default.fileExists(atPath: url.path) {
            try FileManager.default.removeItem(at: url)
        }
    }

    // MARK: - Private helpers

    private func callStatus(_ body: () -> UnsafeMutablePointer<CChar>?) throws {
        if let pointer = body() {
            defer { free_string(pointer) }
            let message = String(cString: pointer)
            throw BridgeError.engineError(message)
        }
    }

    private func callString(_ body: () -> UnsafeMutablePointer<CChar>?) throws -> String? {
        guard let pointer = body() else {
            return nil
        }
        defer { free_string(pointer) }
        return String(cString: pointer)
    }

    private func encode<T: Encodable>(_ value: T) throws -> String {
        do {
            let data = try encoder.encode(value)
            guard let string = String(data: data, encoding: .utf8) else {
                throw BridgeError.missingData("UTF8 conversion")
            }
            return string
        } catch let error as BridgeError {
            throw error
        } catch {
            throw BridgeError.encodingFailed(error)
        }
    }

    private func decode<T: Decodable>(_ type: T.Type, from string: String) throws -> T {
        guard let data = string.data(using: .utf8) else {
            throw BridgeError.missingData("UTF8 decoding")
        }
        do {
            return try decoder.decode(type, from: data)
        } catch let error as BridgeError {
            throw error
        } catch {
            throw BridgeError.decodingFailed(error)
        }
    }

    private static func makeEncoder() -> JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }

    private static func makeDecoder() -> JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }
}
