import SwiftUI
import AppUI
import Bridge

@main
struct EarTrainerApp: App {
    @StateObject private var viewModel: SessionViewModel

    init() {
#if DEBUG
        let useMock = ProcessInfo.processInfo.environment["USE_MOCK_BRIDGE"] == "1"
        let engine: SessionEngine = useMock ? MockBridge() : Bridge()
#else
        let engine: SessionEngine = Bridge()
#endif
        _viewModel = StateObject(wrappedValue: SessionViewModel(engine: engine))
    }

    var body: some Scene {
        WindowGroup {
            EntranceView()
                .environmentObject(viewModel)
        }
    }
}
