#if DEBUG
import Foundation
import SwiftUI
import Bridge

/// Minimal no-op engine for previews only.
/// Implement only what Preview uses; everything else can fatalError.
final class NoopSessionEngine: SessionEngine {
    func setStorageRoot(_ url: URL) throws {}
    func startSession(_ spec: SessionSpec) throws {}
    func nextQuestion() throws -> EngineResponse? { nil }
    func submit(_ report: ResultReport) throws -> EngineResponse? { nil }
    func endSession() throws -> EngineResponse? { nil }
    func restore(checkpoint: Checkpoint) throws {}
    func serializeCheckpoint() throws -> Checkpoint? { nil }
    func saveProfile(_ snapshot: ProfileSnapshot) throws {}
    func loadProfile(name: String) throws -> ProfileSnapshot {
        ProfileSnapshot(name: name, updatedAt: Date())
    }
    func serializeProfile() throws -> ProfileSnapshot {
        ProfileSnapshot(name: "preview", updatedAt: Date())
    }
    func levelCatalogEntries(_ spec: SessionSpec) throws -> [LevelCatalogEntry] {
        [
            LevelCatalogEntry(level: 1, tier: 0, label: "1-0: PREVIEW_NOTE"),
            LevelCatalogEntry(level: 1, tier: 1, label: "1-1: PREVIEW_CHORD"),
            LevelCatalogEntry(level: 2, tier: 0, label: "2-0: PREVIEW_MELODY")
        ]
    }
    func orientationPrompt() throws -> MidiClip? {
        nil
    }
    func assistOptions() throws -> [String] { [] }
    func assist(kind: String) throws -> AssistBundle? { nil }
    var hasActiveSession: Bool { false }
}

extension SessionViewModel {
    @MainActor
    private static func makePreviewViewModel(startingRoute: Route? = nil) -> SessionViewModel {
        let useNoop = ProcessInfo.processInfo.environment["PREVIEW_USE_NOOP_ENGINE"] == "1"
        let engine: SessionEngine = useNoop ? NoopSessionEngine() : Bridge()
        let vm = SessionViewModel(engine: engine, profileName: "preview")
        vm.resetToEntrance()
        vm.loadInspectorOptionsIfNeeded()
        if let route = startingRoute {
            vm.setPreviewRoute(route)
        }
        return vm
    }

    /// Default preview used by EntranceView.
    @MainActor
    public static func preview() -> SessionViewModel {
        makePreviewViewModel(startingRoute: nil)
    }

    /// Example variant you can use to preview specific screens.
    @MainActor
    public static func previewGame() -> SessionViewModel {
        makePreviewViewModel(startingRoute: .game)
    }
}
#endif
