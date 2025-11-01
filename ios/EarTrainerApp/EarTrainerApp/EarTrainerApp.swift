import SwiftUI
import AppUI
import Bridge

@main
struct EarTrainerApp: App {
    private let engine: SessionEngine
    @StateObject private var userStore: UserStore

    init() {
#if DEBUG
        let useMock = ProcessInfo.processInfo.environment["USE_MOCK_BRIDGE"] == "1"
        self.engine = useMock ? MockBridge() : Bridge()
#else
        self.engine = Bridge()
#endif
        _userStore = StateObject(wrappedValue: UserStore())
    }

    var body: some Scene {
        WindowGroup {
            RootCoordinatorView(engine: engine)
                .environmentObject(userStore)
        }
    }
    
//    var body: some Scene {
//        WindowGroup {
//            DegreeKeyboardLinear_Harness()
//        }
//    }
}

#Preview("App") {
    let records = [
        UserProfileRecord.preview(name: "Alice", sessions: 4),
        UserProfileRecord.preview(name: "Bob", sessions: 1)
    ]
    return RootCoordinatorView(engine: MockBridge())
        .environmentObject(UserStore.preview(records: records))
}
