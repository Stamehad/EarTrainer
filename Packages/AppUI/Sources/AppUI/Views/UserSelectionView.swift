import SwiftUI
import Bridge

public struct UserSelectionView: View {
    @EnvironmentObject private var store: UserStore
    @State private var newName: String = ""
    @State private var errorMessage: String?
    @FocusState private var nameFieldFocused: Bool

    private let relativeFormatter: RelativeDateTimeFormatter = {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .full
        return formatter
    }()

    public init() {}

    public var body: some View {
        NavigationStack {
            VStack(spacing: 24) {
                existingUsersSection
                Divider()
                newUserSection
                Spacer()
            }
            .frame(maxWidth: 520)
            .padding()
            .navigationTitle("Choose a Player")
        }
        .task {
            try? store.reloadUsers()
        }
    }

    private var existingUsersSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Existing Players")
                .font(.headline)
            if store.users.isEmpty {
                Text("No players yet. Create one below to get started.")
                    .foregroundStyle(.secondary)
            } else {
                ForEach(store.users, id: \.id) { user in
                    userRow(for: user)
                }
            }
        }
    }

    private var newUserSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Add New Player")
                .font(.headline)
            TextField("Player name", text: $newName)
#if os(iOS)
                .textInputAutocapitalization(.words)
                .disableAutocorrection(true)
#endif
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(
                    RoundedRectangle(cornerRadius: 12)
                        .strokeBorder(Color.secondary.opacity(0.3))
                )
                .focused($nameFieldFocused)
                .onSubmit(createUser)
            if let errorMessage {
                Text(errorMessage)
                    .font(.footnote)
                    .foregroundStyle(.red)
            }
            Button("Create Player", action: createUser)
                .buttonStyle(.borderedProminent)
                .disabled(!isCreateEnabled)
        }
    }

    private var isCreateEnabled: Bool {
        !newName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    private func createUser() {
        let trimmed = newName.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        do {
            try store.createUser(named: trimmed)
            errorMessage = nil
            newName = ""
            nameFieldFocused = false
        } catch {
            errorMessage = (error as? LocalizedError)?.errorDescription ?? error.localizedDescription
        }
    }

    private func userRow(for user: UserProfileRecord) -> some View {
        Button {
            store.select(user)
        } label: {
            HStack(alignment: .center, spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(user.displayName)
                        .font(.headline)
                    if let detail = detailText(for: user) {
                        Text(detail)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                }
                Spacer()
                if store.selectedUser?.id == user.id {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(Color.accentColor)
                } else {
                    Image(systemName: "chevron.right")
                        .foregroundStyle(Color.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.vertical, 10)
            .padding(.horizontal, 12)
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(Color.panelBackground)
            )
        }
        .buttonStyle(.plain)
    }

    private func detailText(for user: UserProfileRecord) -> String? {
        var parts: [String] = []
        if user.totalSessions > 0 {
            let sessionLabel = user.totalSessions == 1 ? "session" : "sessions"
            parts.append("\(user.totalSessions) \(sessionLabel)")
        } else {
            parts.append("No sessions yet")
        }
        let relative = relativeFormatter.localizedString(for: user.updatedAt, relativeTo: Date())
        parts.append("updated \(relative)")
        if let levelSummary = adaptiveLevelSummary(for: user.memory) {
            parts.append(levelSummary)
        }
        return parts.joined(separator: " • ")
    }

    private func adaptiveLevelSummary(for memory: MemoryPackage?) -> String? {
        guard
            let adaptive = memory?.adaptive,
            case let .object(payload) = adaptive,
            let levelValue = payload["level"],
            case let .object(level) = levelValue,
            let current = decodeInt(level["current_level"])
        else {
            return nil
        }
        var parts: [String] = ["Level \(current)"]
        if let suggested = decodeInt(level["suggested_level"]), suggested != current {
            parts.append("→ \(suggested)")
        }
        if let track = decodeString(level["track_name"]), !track.isEmpty {
            parts.append(track)
        }
        return parts.joined(separator: " ")
    }

    private func decodeInt(_ value: JSONValue?) -> Int? {
        guard let value else { return nil }
        switch value {
        case let .int(number):
            return number
        case let .double(number):
            return Int(number)
        case let .string(text):
            return Int(text)
        default:
            return nil
        }
    }

    private func decodeString(_ value: JSONValue?) -> String? {
        guard let value else { return nil }
        switch value {
        case let .string(text):
            return text
        case let .int(number):
            return String(number)
        default:
            return nil
        }
    }
}

#if DEBUG
#Preview("User Selection") {
    let records = [
        UserProfileRecord.preview(name: "Alice", sessions: 3, updatedAt: Date().addingTimeInterval(-7200)),
        UserProfileRecord.preview(name: "Bob", sessions: 0, updatedAt: Date().addingTimeInterval(-86400 * 3))
    ]
    return UserSelectionView()
        .environmentObject(UserStore.preview(records: records))
}
#endif
