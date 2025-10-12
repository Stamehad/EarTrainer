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
    @Published public private(set) var currentQuestion: Question?
    @Published public private(set) var attemptResult: AttemptResult?
    @Published public private(set) var summary: SessionSummary?
    @Published public var errorBanner: String?
    @Published public private(set) var isProcessing: Bool = false

    private let engine: SessionEngine
    private let profileName: String
    private var profile: ProfileSnapshot?
    private var hasBootstrapped = false

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
            attemptResult = nil
            summary = nil
            try engine.startSession(spec)
            currentQuestion = try engine.nextQuestion()
            route = .game
            try clearCheckpointOnDisk()
        }
    }

    public func submit(answer choiceId: String, latencyMs: Int) {
        guard !isProcessing, let question = currentQuestion else { return }
        perform {
            let answer = Answer(questionId: question.id, choiceId: choiceId, latencyMs: latencyMs)
            attemptResult = try engine.submit(answer)
        }
    }

    public func next() {
        guard !isProcessing else { return }
        perform {
            attemptResult = nil
            if let question = try engine.nextQuestion() {
                currentQuestion = question
            } else {
                try finishSession()
            }
        }
    }

    public func finish() {
        guard !isProcessing else { return }
        perform {
            try finishSession()
        }
    }

    public func resetToEntrance() {
        currentQuestion = nil
        attemptResult = nil
        summary = nil
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
        try engine.setStorageRoot(root)
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
        let data = try Data(contentsOf: url)
        let checkpoint = try decoder.decode(Checkpoint.self, from: data)
        if let savedSpec = checkpoint.spec {
            spec = savedSpec
        }
        try engine.restore(checkpoint: checkpoint)
        currentQuestion = try engine.nextQuestion()
        attemptResult = nil
        summary = nil
        route = currentQuestion == nil ? .entrance : .game
    }

    private func finishSession() throws {
        summary = try engine.endSession()
        currentQuestion = nil
        attemptResult = nil
        route = .results
        try clearCheckpointOnDisk()
        try saveProfileToDisk()
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
                    guard var snapshot = try self.engine.serializeCheckpoint() else {
                        return nil
                    }
                    snapshot.spec = self.spec
                    return snapshot
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
