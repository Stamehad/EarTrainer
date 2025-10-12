import Foundation
import Bridge

public enum Paths {
    private static let bundleIdentifier = "EarTrainer"
    private static var overrideRoot: URL?

    public static func setOverrideRoot(_ url: URL?) {
        overrideRoot = url
    }

    public static func appSupportRoot() throws -> URL {
        if let overrideRoot {
            try FileManager.default.createDirectory(at: overrideRoot, withIntermediateDirectories: true)
            return overrideRoot
        }
        let fileManager = FileManager.default
        guard let baseURL = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            throw BridgeError.missingData("Application Support directory")
        }
        let root = baseURL.appendingPathComponent(bundleIdentifier, isDirectory: true)
        if !fileManager.fileExists(atPath: root.path) {
            try fileManager.createDirectory(at: root, withIntermediateDirectories: true)
        }
        return root
    }

    public static func profileURL(for profileName: String) throws -> URL {
        try appSupportRoot().appendingPathComponent("\(profileName)_profile.json", isDirectory: false)
    }

    public static func checkpointURL() throws -> URL {
        try appSupportRoot().appendingPathComponent("session_checkpoint.json", isDirectory: false)
    }
}
