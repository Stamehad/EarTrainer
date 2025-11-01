import SwiftUI
import Bridge

public struct RootCoordinatorView: View {
    @EnvironmentObject private var userStore: UserStore
    private let engine: SessionEngine

    public init(engine: SessionEngine) {
        self.engine = engine
    }

    public var body: some View {
        Group {
            if let user = userStore.selectedUser {
                SessionHostView(engine: engine, user: user)
                    .id(user.id)
            } else {
                UserSelectionView()
            }
        }
        .animation(.easeInOut(duration: 0.2), value: userStore.selectedUser?.id)
    }
}

private struct SessionHostView: View {
    @EnvironmentObject private var store: UserStore
    private let engine: SessionEngine
    private let user: UserProfileRecord
    @StateObject private var viewModel: SessionViewModel

    init(engine: SessionEngine, user: UserProfileRecord) {
        self.engine = engine
        self.user = user
        _viewModel = StateObject(wrappedValue: SessionViewModel(engine: engine, profileName: user.name))
    }

    var body: some View {
        EntranceView()
            .environmentObject(viewModel)
            .background(Color.appBackground)
            .onAppear {
                refreshUserRecord()
            }
            .onChange(of: viewModel.summary) { _ in
                refreshUserRecord()
            }
            .onChange(of: viewModel.lastMemory) { _ in
                refreshUserRecord()
            }
    }

    private func refreshUserRecord() {
        do {
            try store.refreshUser(named: user.name)
        } catch {
#if DEBUG
            print("[SessionHost] Failed to refresh user \(user.name): \(error)")
#endif
        }
    }
}

#if DEBUG
#Preview("Coordinator - Selection") {
    RootCoordinatorView(engine: MockBridge())
        .environmentObject(UserStore.preview(records: [
            UserProfileRecord.preview(name: "Alice", sessions: 2),
            UserProfileRecord.preview(name: "Bob", sessions: 0)
        ]))
}

#Preview("Coordinator - Session") {
    let store = UserStore.preview(records: [
        UserProfileRecord.preview(name: "Alice", sessions: 2)
    ])
    store.selectUser(named: "Alice")
    return RootCoordinatorView(engine: MockBridge())
        .environmentObject(store)
}
#endif
