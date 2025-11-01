import Foundation
import Bridge

public struct UserProfileRecord: Identifiable, Equatable {
    public var id: String { name }
    public let name: String
    public var snapshot: ProfileSnapshot
    public var memory: MemoryPackage?

    public var displayName: String { snapshot.name }
    public var updatedAt: Date { snapshot.updatedAt }
    public var totalSessions: Int { snapshot.totalSessions }

    public init(name: String, snapshot: ProfileSnapshot, memory: MemoryPackage? = nil) {
        self.name = name
        self.snapshot = snapshot
        self.memory = memory
    }
}

public enum UserStoreError: LocalizedError {
    case invalidName
    case duplicateName
    case filesystem(Error)

    public var errorDescription: String? {
        switch self {
        case .invalidName:
            return "Names may include letters, numbers, spaces, hyphen, or underscore."
        case .duplicateName:
            return "That name is already in use."
        case let .filesystem(error):
            return error.localizedDescription
        }
    }
}

@MainActor
public final class UserStore: ObservableObject {
    @Published public private(set) var users: [UserProfileRecord] = []
    @Published public var selectedUser: UserProfileRecord?

    private let decoder: JSONDecoder
    private let encoder: JSONEncoder
    private let fileManager: FileManager

    public init(
        decoder: JSONDecoder? = nil,
        encoder: JSONEncoder? = nil,
        fileManager: FileManager = .default
    ) {
        self.decoder = decoder ?? UserStore.makeDecoder()
        self.encoder = encoder ?? UserStore.makeEncoder()
        self.fileManager = fileManager
        try? reloadUsers()
    }

    public func reloadUsers() throws {
        users = try loadAllRecords()
        if let selected = selectedUser {
            selectedUser = users.first(where: { $0.id == selected.id })
        }
    }

    public func select(_ user: UserProfileRecord) {
        selectedUser = user
    }

    public func selectUser(named name: String) {
        guard let record = users.first(where: { $0.name.caseInsensitiveCompare(name) == .orderedSame }) else {
            return
        }
        selectedUser = record
    }

    public func signOut() {
        selectedUser = nil
    }

    public func createUser(named rawName: String) throws {
        let name = normalized(rawName)
        try validate(name: name)
        if users.contains(where: { $0.name.caseInsensitiveCompare(name) == .orderedSame }) {
            throw UserStoreError.duplicateName
        }
        do {
            let snapshot = ProfileSnapshot(name: name, totalSessions: 0, updatedAt: Date())
            let data = try encoder.encode(snapshot)
            let url = try Paths.profileURL(for: name)
            try data.write(to: url, options: .atomic)
            let record = UserProfileRecord(name: name, snapshot: snapshot, memory: nil)
            users.append(record)
            users.sort(by: Self.sortRecords(lhs:rhs:))
            selectedUser = record
        } catch {
            throw UserStoreError.filesystem(error)
        }
    }

    public func refreshUser(named name: String) throws {
        let record = try loadRecord(named: name)
        if let index = users.firstIndex(where: { $0.id == record.id }) {
            users[index] = record
        } else {
            users.append(record)
        }
        users.sort(by: Self.sortRecords(lhs:rhs:))
        if selectedUser?.id == record.id {
            selectedUser = record
        }
    }

    public func persist(memory: MemoryPackage?, for name: String) throws {
        do {
            if let memory {
                let data = try encoder.encode(memory)
                let url = try Paths.memoryURL(for: name)
                try data.write(to: url, options: .atomic)
            } else {
                let url = try Paths.memoryURL(for: name)
                if fileManager.fileExists(atPath: url.path) {
                    try fileManager.removeItem(at: url)
                }
            }
            try refreshUser(named: name)
        } catch {
            throw UserStoreError.filesystem(error)
        }
    }

    // MARK: - Helpers

    private func loadAllRecords() throws -> [UserProfileRecord] {
        do {
            let root = try Paths.appSupportRoot()
            let contents = try fileManager.contentsOfDirectory(at: root, includingPropertiesForKeys: nil)
            var records: [UserProfileRecord] = []
            for url in contents where url.lastPathComponent.hasSuffix("_profile.json") {
                let data = try Data(contentsOf: url)
                let snapshot = try decoder.decode(ProfileSnapshot.self, from: data)
                let memory = loadMemory(named: snapshot.name)
                records.append(UserProfileRecord(name: snapshot.name, snapshot: snapshot, memory: memory))
            }
            records.sort(by: Self.sortRecords(lhs:rhs:))
            return records
        } catch {
            throw UserStoreError.filesystem(error)
        }
    }

    private func loadRecord(named name: String) throws -> UserProfileRecord {
        do {
            let url = try Paths.profileURL(for: name)
            let data = try Data(contentsOf: url)
            let snapshot = try decoder.decode(ProfileSnapshot.self, from: data)
            let memory = loadMemory(named: snapshot.name)
            return UserProfileRecord(name: snapshot.name, snapshot: snapshot, memory: memory)
        } catch {
            throw UserStoreError.filesystem(error)
        }
    }

    private func loadMemory(named name: String) -> MemoryPackage? {
        guard
            let url = try? Paths.memoryURL(for: name),
            fileManager.fileExists(atPath: url.path),
            let data = try? Data(contentsOf: url),
            let package = try? decoder.decode(MemoryPackage.self, from: data)
        else {
            return nil
        }
        return package
    }

    private func normalized(_ name: String) -> String {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        let components = trimmed.split(whereSeparator: \.isWhitespace)
        return components.joined(separator: " ")
    }

    private func validate(name: String) throws {
        guard !name.isEmpty else {
            throw UserStoreError.invalidName
        }
        let allowed = CharacterSet(charactersIn: "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-")
        if name.unicodeScalars.contains(where: { !allowed.contains($0) }) {
            throw UserStoreError.invalidName
        }
    }

    private static func sortRecords(lhs: UserProfileRecord, rhs: UserProfileRecord) -> Bool {
        if lhs.updatedAt != rhs.updatedAt {
            return lhs.updatedAt > rhs.updatedAt
        }
        return lhs.displayName.localizedCaseInsensitiveCompare(rhs.displayName) == .orderedAscending
    }

    private static func makeDecoder() -> JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }

    private static func makeEncoder() -> JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        return encoder
    }
}

#if DEBUG
extension UserProfileRecord {
    public static func preview(
        name: String,
        sessions: Int,
        updatedAt: Date = Date(),
        memory: MemoryPackage? = nil
    ) -> UserProfileRecord {
        let snapshot = ProfileSnapshot(name: name, totalSessions: sessions, updatedAt: updatedAt)
        return UserProfileRecord(name: name, snapshot: snapshot, memory: memory)
    }
}

extension UserStore {
    public static func preview(records: [UserProfileRecord]) -> UserStore {
        let store = UserStore()
        store.users = records
        store.selectedUser = records.first
        return store
    }
}
#endif
