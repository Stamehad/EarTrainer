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
    var hasActiveSession: Bool { false }
}

extension SessionViewModel {
    /// The safest default: does not auto-play audio, does not touch disk.
    @MainActor
    public static func preview() -> SessionViewModel {
        let vm = SessionViewModel(engine: NoopSessionEngine(), profileName: "preview")
        // Seed any UI state you want to see in Canvas:
        vm.resetToEntrance()        // .entrance route
        // You can also set vm.route = .game or prefill vm.summary for results view previews.
        return vm
    }

    /// Example variants you can use to preview specific screens:
    @MainActor
    public static func previewGame() -> SessionViewModel {
        let vm = SessionViewModel(engine: NoopSessionEngine(), profileName: "preview")
        vm.spec = SessionSpec()     // customize if your GameView reads spec
        vm.resetToEntrance()
        vm.setPreviewRoute(.game)
        return vm
    }
}
#endif
