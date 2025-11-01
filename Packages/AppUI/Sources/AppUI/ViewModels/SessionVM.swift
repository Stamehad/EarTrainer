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

    public enum AnswerInputMode: String, CaseIterable, Identifiable {
        case keypad3x3
        case keyboardLinear
        case ringPad

        public var id: String { rawValue }

        var label: String {
            switch self {
            case .keypad3x3:
                return "Keypad 3×3"
            case .keyboardLinear:
                return "Linear Keyboard"
            case .ringPad:
                return "Ring Pad"
            }
        }
    }

    @Published public private(set) var route: Route = .entrance
    @Published public var spec: SessionSpec
    @Published public private(set) var currentQuestion: QuestionEnvelope?
    @Published public private(set) var summary: SessionSummary?
    @Published public private(set) var lastMemory: MemoryPackage?
    @Published public private(set) var questionJSON: String?
    @Published public private(set) var debugJSON: String?
    @Published public var errorBanner: String?
    @Published public private(set) var isProcessing: Bool = false
    @Published public private(set) var inspectorOptions: [LevelCatalogEntry] = []
    @Published public private(set) var drillCatalog: DrillCatalog?
    @Published public var answerInputMode: AnswerInputMode = .keypad3x3

    private let engine: SessionEngine
    private let profileName: String
    private var profile: ProfileSnapshot?
    private var hasBootstrapped = false
    private var hasCapturedMemory = false
    private var questionsAnswered = 0
    private let audioPlayer = MidiAudioPlayer.shared
    private var suppressNextPromptAutoPlay = false
    private var needsDeferredPromptPlayback = false
    private var deferredPromptPlaybackDelay: TimeInterval = 0

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
            if let stored = try loadStoredMemory() {
                lastMemory = stored
            }
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
            suppressNextPromptAutoPlay = false
            needsDeferredPromptPlayback = false
            deferredPromptPlaybackDelay = 0
            hasCapturedMemory = false
            try engine.startSession(spec)
            playOrientationPrompt(autoTriggered: true)
            route = .game
            try clearCheckpointOnDisk()
            try fetchNextQuestion()
        }
    }

    public func submit(answer: AnswerPayload, isCorrect: Bool, latencyMs: Int = 3_000) {
        guard !isProcessing, let question = currentQuestion else { return }
        perform {
            let report = buildReport(
                for: question.bundle,
                answer: answer,
                isCorrect: isCorrect,
                latencyMs: latencyMs,
                autoSubmit: false
            )
            let response = try engine.submit(report)
            questionsAnswered += 1
            if let response {
                handle(response: response, delay: 0.5)
            } else {
                currentQuestion = nil
            }
        }
    }

    public func submit(latencyMs: Int = 1_000) {
        guard !isProcessing, let question = currentQuestion else { return }
        perform {
            let report = buildReport(
                for: question.bundle,
                answer: question.bundle.correctAnswer,
                isCorrect: true,
                latencyMs: latencyMs,
                autoSubmit: true
            )
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
        suppressNextPromptAutoPlay = false
        needsDeferredPromptPlayback = false
        deferredPromptPlaybackDelay = 0
        hasCapturedMemory = false
    }

    // MARK: - Level Inspector

    public func loadInspectorOptionsIfNeeded() {
        guard inspectorOptions.isEmpty else { return }
        do {
            let entries = try engine.levelCatalogEntries(spec)
            inspectorOptions = entries
        } catch {
            presentError(error)
        }
    }

    public func loadDrillCatalogIfNeeded() {
        guard drillCatalog == nil else { return }
        do {
            let payload = try engine.drillParamSpec()
            let data = try displayEncoder.encode(payload)
            let catalog = try decoder.decode(DrillCatalog.self, from: data)
            drillCatalog = catalog
        } catch {
            presentError(error)
        }
    }

    public func exitApplication() {
#if canImport(UIKit)
        UIApplication.shared.perform(#selector(NSXPCConnection.suspend))
#endif
    }

    public func replayPromptAudio() {
        guard let clip = currentQuestion?.bundle.promptClip else { return }
        audioPlayer.play(clip: clip)
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
        if !fileManager.fileExists(atPath: destination.path) {
            try fileManager.createDirectory(at: destination, withIntermediateDirectories: true)
        }
        var searchPaths: [URL] = [destination]

        if let source = locateCoreResourcesFolder() {
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
        } else {
#if DEBUG
            print("[SessionVM] CoreResources not bundled; relying on built-in defaults and top-level resources.")
#endif
        }
        if let mainResources = Bundle.main.resourceURL {
            searchPaths.append(mainResources)
        }
        audioPlayer.configureIfAvailable(searchPaths: searchPaths)
    }

    private func locateCoreResourcesFolder() -> URL? {
        let fileManager = FileManager.default
        var inspected = Set<ObjectIdentifier>()
        var inspectedBundles: [Bundle] = []

        func bundlesToInspect() -> [Bundle] {
            var bundles: [Bundle] = [
                Bundle.main,
                Bundle(for: SessionViewModel.self)
            ]
#if SWIFT_PACKAGE
            bundles.append(Bundle.module)
#endif
            bundles.append(contentsOf: Bundle.allBundles)
            bundles.append(contentsOf: Bundle.allFrameworks)
            if let embedded = Bundle.main.urls(forResourcesWithExtension: "bundle", subdirectory: nil) {
                for url in embedded {
                    if let bundle = Bundle(url: url) {
                        bundles.append(bundle)
                    }
                }
            }
            return bundles
        }

        for bundle in bundlesToInspect() {
            let identifier = ObjectIdentifier(bundle)
            guard !inspected.contains(identifier) else { continue }
            inspected.insert(identifier)
            inspectedBundles.append(bundle)

            if let url = bundle.url(forResource: "CoreResources", withExtension: nil),
               fileManager.fileExists(atPath: url.path) {
                return url
            }
            if let resourceURL = bundle.resourceURL {
                let candidate = resourceURL.appendingPathComponent("CoreResources", isDirectory: true)
                if fileManager.fileExists(atPath: candidate.path) {
                    return candidate
                }
            }
        }

        if let frameworksURL = Bundle.main.privateFrameworksURL {
            if let contents = try? fileManager.contentsOfDirectory(at: frameworksURL, includingPropertiesForKeys: nil) {
                for url in contents where url.pathExtension == "bundle" {
                    if let bundle = Bundle(url: url) {
                        let identifier = ObjectIdentifier(bundle)
                        if !inspected.contains(identifier) {
                            inspected.insert(identifier)
                            inspectedBundles.append(bundle)
                        }
                        if let resourceURL = bundle.resourceURL {
                            let candidate = resourceURL.appendingPathComponent("CoreResources", isDirectory: true)
                            if fileManager.fileExists(atPath: candidate.path) {
                                return candidate
                            }
                        }
                        if let location = bundle.url(forResource: "CoreResources", withExtension: nil),
                           fileManager.fileExists(atPath: location.path) {
                            return location
                        }
                    }
                }
            }
        }

        return nil
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

    private func loadStoredMemory() throws -> MemoryPackage? {
        let url = try Paths.memoryURL(for: profileName)
        let manager = FileManager.default
        guard manager.fileExists(atPath: url.path) else {
            return nil
        }
        let data = try Data(contentsOf: url)
        return try decoder.decode(MemoryPackage.self, from: data)
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

    private func handleMemoryPersistence(with memory: MemoryPackage?) {
        guard !hasCapturedMemory else { return }
        if let memory {
            persist(memory: memory)
            return
        }
        do {
            if let response = try engine.endSession(),
               case let .summary(_, fetchedMemory) = response,
               let fetchedMemory {
                persist(memory: fetchedMemory)
            }
        } catch {
            presentError(error)
        }
    }

    private func persist(memory: MemoryPackage) {
        lastMemory = memory
        hasCapturedMemory = true
        do {
            try saveMemoryToDisk(memory)
        } catch {
            presentError(error)
        }
    }

    private func saveMemoryToDisk(_ memory: MemoryPackage) throws {
        let url = try Paths.memoryURL(for: profileName)
        let data = try encoder.encode(memory)
        try data.write(to: url, options: .atomic)
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

    private func handle(response: EngineResponse, delay: TimeInterval = 0) {
        guard delay <= 0 else {
            Task { [weak self] in
                let nanoseconds = UInt64(max(delay, 0) * 1_000_000_000)
                try await Task.sleep(nanoseconds: nanoseconds)
                await MainActor.run {
                    self?.apply(response: response)
                }
            }
            return
        }
        apply(response: response)
    }

    @MainActor
    private func apply(response: EngineResponse) {
        switch response {
        case let .question(envelope):
            currentQuestion = envelope
            summary = nil
            questionJSON = summariseQuestionBundle(envelope.bundle) ?? prettyJSONString(envelope.bundle)
            debugJSON = summariseDebugInfo(envelope.debug) ?? envelope.debug?.prettyPrinted()
            route = .game
            if !suppressNextPromptAutoPlay,
               let clip = envelope.bundle.promptClip {
                audioPlayer.play(clip: clip)
            }
            suppressNextPromptAutoPlay = false
            if needsDeferredPromptPlayback {
                let delay = deferredPromptPlaybackDelay
                needsDeferredPromptPlayback = false
                deferredPromptPlaybackDelay = 0
                if delay > 0 {
                    DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
                        self?.replayPromptAudio()
                    }
                } else {
                    replayPromptAudio()
                }
            }
        case let .summary(report, memory):
            currentQuestion = nil
            questionJSON = nil
            debugJSON = nil
            summary = report
            handleMemoryPersistence(with: memory)
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

    private func summariseQuestionBundle(_ bundle: QuestionBundle) -> String? {
        var summary: [String: JSONValue] = [:]
        summary["correct_answer"] = bundle.correctAnswer.jsonValue
        if let question = bundle.question,
           case let .object(payload) = question,
           let degrees = payload["degrees"] {
            summary["question_degrees"] = degrees
        }
        return prettyJSONString(summary)
    }

    private func summariseDebugInfo(_ debug: JSONValue?) -> String? {
        guard let debug else { return nil }
        guard case let .object(items) = debug else { return debug.prettyPrinted() }
        let keys = [
            "mode",
            "session_id",
            "total_questions",
            "level_inspector_level",
            "level_inspector_tier",
            "drill_kind"
        ]
        var summary: [String: JSONValue] = [:]
        for key in keys {
            if let value = items[key] {
                summary[key] = value
            }
        }
        return summary.isEmpty ? nil : prettyJSONString(summary)
    }

    private func clipDuration(_ clip: MidiClip) -> TimeInterval {
        guard clip.tempoBpm > 0, clip.ppq > 0 else { return 0 }
        let beats = Double(clip.lengthTicks) / Double(clip.ppq)
        let secondsPerBeat = 60.0 / Double(clip.tempoBpm)
        return beats * secondsPerBeat
    }

    public func playOrientationPrompt(autoTriggered: Bool = false) {
        do {
            guard let clip = try engine.orientationPrompt() else {
                if autoTriggered {
                    suppressNextPromptAutoPlay = false
                    needsDeferredPromptPlayback = false
                    deferredPromptPlaybackDelay = 0
                }
                return
            }
            if autoTriggered {
                suppressNextPromptAutoPlay = true
                needsDeferredPromptPlayback = true
                deferredPromptPlaybackDelay = max(clipDuration(clip) + 0.1, 0)
            } else {
                needsDeferredPromptPlayback = false
                deferredPromptPlaybackDelay = 0
            }
            audioPlayer.play(clip: clip)
        } catch {
            presentError(error)
        }
    }

    private func buildReport(
        for bundle: QuestionBundle,
        answer: AnswerPayload,
        isCorrect: Bool,
        latencyMs: Int,
        autoSubmit: Bool
    ) -> ResultReport {
        let metrics = ResultMetrics(
            rtMs: latencyMs,
            attempts: 1,
            questionCount: questionsAnswered + 1,
            assistsUsed: [:],
            firstInputRtMs: latencyMs
        )
        var clientInfo: [String: JSONValue] = [
            "auto_submit": .bool(autoSubmit),
            "latency_ms": .int(latencyMs),
            "is_correct": .bool(isCorrect)
        ]
        if !autoSubmit {
            clientInfo["input_mode"] = .string(answerInputMode.rawValue)
        }
        return ResultReport(
            questionId: bundle.questionId,
            finalAnswer: answer,
            correct: isCorrect,
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

#if DEBUG
extension SessionViewModel {
    @MainActor
    func setPreviewRoute(_ route: Route) {
        self.route = route
    }
}
#endif
