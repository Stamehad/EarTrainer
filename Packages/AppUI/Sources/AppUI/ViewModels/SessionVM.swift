import Foundation
import SwiftUI
import Bridge
#if canImport(UIKit)
import UIKit
#endif

@MainActor
public final class SessionViewModel: ObservableObject {
    public enum Route {
        case entrance
        case game
        case results
    }

    @Published public private(set) var route: Route = .entrance
    @Published public var spec: SessionSpec
    @Published public private(set) var currentQuestion: QuestionEnvelope?
    @Published public private(set) var summary: SessionSummary?
    @Published public private(set) var questionJSON: String?
    @Published public private(set) var debugJSON: String?
    @Published public var errorBanner: String?
    @Published public private(set) var isProcessing: Bool = false

    private let engine: SessionEngine
    private let profileName: String
    private var profile: ProfileSnapshot?
    private var hasBootstrapped = false
    private var questionsAnswered = 0

    private let encoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }()

    private let decoder: JSONDecoder = {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }()

    private let displayEncoder: JSONEncoder = {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }()

    public init(engine: SessionEngine, profileName: String = "default") {
        self.engine = engine
        self.profileName = profileName
        self.spec = SessionSpec()
    }

    // MARK: - Lifecycle

    public func bootstrap() {
        guard !hasBootstrapped else { return }
        hasBootstrapped = true
        do {
            try prepareEngine()
            try loadProfileSnapshot()
            try restoreCheckpointIfAvailable()
        } catch {
            presentError(error)
        }
    }

    public func handleScenePhase(_ phase: ScenePhase) {
        switch phase {
        case .active:
            do {
                try restoreCheckpointIfAvailable()
            } catch {
                presentError(error)
            }
        case .inactive, .background:
            persistStateInBackground()
        default:
            break
        }
    }

    // MARK: - Session Control

    public func start() {
        guard !isProcessing else { return }
        perform {
            summary = nil
            questionJSON = nil
            debugJSON = nil
            questionsAnswered = 0
            try engine.startSession(spec)
            route = .game
            try clearCheckpointOnDisk()
            try fetchNextQuestion()
        }
    }

    public func submit(latencyMs: Int = 1_000) {
        guard !isProcessing, let question = currentQuestion else { return }
        perform {
            let report = buildReport(for: question.bundle, latencyMs: latencyMs)
            let response = try engine.submit(report)
            questionsAnswered += 1
            if let response {
                handle(response: response)
            } else {
                currentQuestion = nil
            }
        }
    }

    public func next() {
        guard !isProcessing else { return }
        perform { try fetchNextQuestion() }
    }

    public func finish() {
        guard !isProcessing else { return }
        perform {
            if let response = try engine.endSession() {
                handle(response: response)
            } else {
                summary = nil
                route = .entrance
            }
        }
    }

    public func resetToEntrance() {
        currentQuestion = nil
        summary = nil
        questionJSON = nil
        debugJSON = nil
        route = .entrance
    }

    public func exitApplication() {
#if canImport(UIKit)
        UIApplication.shared.perform(#selector(NSXPCConnection.suspend))
#endif
    }

    // MARK: - Private helpers

    private func prepareEngine() throws {
        let root = try Paths.appSupportRoot()
        try ensureResourcesAvailable(at: root)
        try engine.setStorageRoot(root)
    }

    private func ensureResourcesAvailable(at root: URL) throws {
        let fileManager = FileManager.default
        let destination = root.appendingPathComponent("resources", isDirectory: true)
        let bundle = ResourceBundle.bundle
        guard let source = bundle.url(forResource: "CoreResources", withExtension: nil) else {
            throw BridgeError.missingData("CoreResources bundle")
        }
        if !fileManager.fileExists(atPath: destination.path) {
            try fileManager.createDirectory(at: destination, withIntermediateDirectories: true)
        }
        if let enumerator = fileManager.enumerator(at: source, includingPropertiesForKeys: [.isDirectoryKey]) {
            for case let fileURL as URL in enumerator {
                let resourceValues = try fileURL.resourceValues(forKeys: [.isDirectoryKey])
                let relativePath = fileURL.path.replacingOccurrences(of: source.path + "/", with: "")
                let targetURL = destination.appendingPathComponent(relativePath)
                if resourceValues.isDirectory == true {
                    try fileManager.createDirectory(at: targetURL, withIntermediateDirectories: true)
                } else {
                    if fileManager.fileExists(atPath: targetURL.path) {
                        try fileManager.removeItem(at: targetURL)
                    }
                    try fileManager.copyItem(at: fileURL, to: targetURL)
                }
            }
        }
    }

    private enum ResourceBundle {
        static var bundle: Bundle = {
            #if SWIFT_PACKAGE
            return Bundle.module
            #else
            return Bundle(for: BundleMarker.self)
            #endif
        }()

        private final class BundleMarker {}
    }

    private func loadProfileSnapshot() throws {
        let url = try Paths.profileURL(for: profileName)
        let manager = FileManager.default
        if manager.fileExists(atPath: url.path) {
            let data = try Data(contentsOf: url)
            let snapshot = try decoder.decode(ProfileSnapshot.self, from: data)
            try engine.saveProfile(snapshot)
            profile = snapshot
        } else {
            var snapshot = try engine.loadProfile(name: profileName)
            snapshot.updatedAt = Date()
            profile = snapshot
            try writeProfile(snapshot)
        }
    }

    private func restoreCheckpointIfAvailable() throws {
        let url = try Paths.checkpointURL()
        let manager = FileManager.default
        guard manager.fileExists(atPath: url.path) else {
            return
        }
        do {
            let data = try Data(contentsOf: url)
            let checkpoint = try decoder.decode(Checkpoint.self, from: data)
            spec = checkpoint.spec
            try engine.restore(checkpoint: checkpoint)
            try fetchNextQuestion()
        } catch {
            try? clearCheckpointOnDisk()
            throw error
        }
    }

    private func saveProfileToDisk() throws {
        var snapshot = try engine.serializeProfile()
        snapshot.updatedAt = Date()
        try writeProfile(snapshot)
    }

    private func writeProfile(_ snapshot: ProfileSnapshot) throws {
        let url = try Paths.profileURL(for: profileName)
        let data = try encoder.encode(snapshot)
        try data.write(to: url, options: .atomic)
        profile = snapshot
    }

    private func clearCheckpointOnDisk() throws {
        let url = try Paths.checkpointURL()
        let manager = FileManager.default
        if manager.fileExists(atPath: url.path) {
            try manager.removeItem(at: url)
        }
    }

    private func fetchNextQuestion() throws {
        if let response = try engine.nextQuestion() {
            handle(response: response)
        } else {
            currentQuestion = nil
            questionJSON = nil
            debugJSON = nil
        }
    }

    private func handle(response: EngineResponse) {
        switch response {
        case let .question(envelope):
            currentQuestion = envelope
            summary = nil
            questionJSON = prettyJSONString(envelope.bundle)
            debugJSON = envelope.debug?.prettyPrinted()
            route = .game
        case let .summary(report):
            currentQuestion = nil
            questionJSON = nil
            debugJSON = nil
            summary = report
            route = .results
            questionsAnswered = 0
            do {
                try clearCheckpointOnDisk()
                try saveProfileToDisk()
            } catch {
                presentError(error)
            }
        }
    }

    private func buildReport(for bundle: QuestionBundle, latencyMs: Int) -> ResultReport {
        let metrics = ResultMetrics(
            rtMs: latencyMs,
            attempts: 1,
            questionCount: questionsAnswered + 1,
            assistsUsed: [:],
            firstInputRtMs: latencyMs
        )
        let clientInfo: [String: JSONValue] = [
            "auto_submit": .bool(true),
            "latency_ms": .int(latencyMs)
        ]
        return ResultReport(
            questionId: bundle.questionId,
            finalAnswer: bundle.correctAnswer,
            correct: true,
            metrics: metrics,
            attempts: [],
            clientInfo: clientInfo
        )
    }

    private func prettyJSONString<T: Encodable>(_ value: T) -> String? {
        guard let data = try? displayEncoder.encode(value) else { return nil }
        return String(data: data, encoding: .utf8)
    }

    private func persistStateInBackground() {
        Task(priority: .background) { [profileName] in
            do {
                let snapshot: ProfileSnapshot = try await MainActor.run {
                    var profile = try self.engine.serializeProfile()
                    profile.updatedAt = Date()
                    self.profile = profile
                    return profile
                }

                let checkpoint: Checkpoint? = try await MainActor.run {
                    try self.engine.serializeCheckpoint()
                }

                let profileEncoder = JSONEncoder()
                profileEncoder.dateEncodingStrategy = .iso8601
                let profileData = try profileEncoder.encode(snapshot)
                let profileURL = try Paths.profileURL(for: profileName)
                try profileData.write(to: profileURL, options: .atomic)

                let checkpointURL = try Paths.checkpointURL()
                if let checkpoint {
                    let checkpointEncoder = JSONEncoder()
                    checkpointEncoder.dateEncodingStrategy = .iso8601
                    let checkpointData = try checkpointEncoder.encode(checkpoint)
                    try checkpointData.write(to: checkpointURL, options: .atomic)
                } else {
                    let manager = FileManager.default
                    if manager.fileExists(atPath: checkpointURL.path) {
                        try manager.removeItem(at: checkpointURL)
                    }
                }
            } catch {
                await MainActor.run {
                    self.presentError(error)
                }
            }
        }
    }

    private func perform(_ action: () throws -> Void) {
        isProcessing = true
        defer { isProcessing = false }
        do {
            try action()
        } catch {
            presentError(error)
        }
    }

    private func presentError(_ error: Error) {
        errorBanner = (error as? LocalizedError)?.errorDescription ?? error.localizedDescription
    }
}
