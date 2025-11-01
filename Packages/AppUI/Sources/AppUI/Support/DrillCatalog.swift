import Foundation
import Bridge

public struct DrillCatalog: Codable {
    public var version: String
    public var drills: [DrillDefinition]

    public var sortedDrills: [DrillDefinition] {
        drills.sorted { $0.displayName.localizedCaseInsensitiveCompare($1.displayName) == .orderedAscending }
    }

    public func definition(for key: String) -> DrillDefinition? {
        drills.first(where: { $0.key == key })
    }

    public init(version: String, drills: [DrillDefinition]) {
        self.version = version
        self.drills = drills
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        version = try container.decodeIfPresent(String.self, forKey: .version) ?? "v1"
        let rawDrills = try container.decodeIfPresent([String: DrillDefinition.Payload].self, forKey: .drills) ?? [:]
        drills = rawDrills.map { key, payload in
            payload.makeDefinition(key: key)
        }
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(version, forKey: .version)
        let payloads = Dictionary(uniqueKeysWithValues: drills.map { ($0.key, $0.makePayload()) })
        try container.encode(payloads, forKey: .drills)
    }

    private enum CodingKeys: String, CodingKey {
        case version
        case drills
    }
}

public struct DrillDefinition: Codable, Identifiable {
    public enum FieldKind: String, Codable {
        case int
        case double
        case bool
        case string
        case intList = "int_list"
        case doubleList = "double_list"
        case stringList = "string_list"
        case option
        case select
        case unknown

        public init(from decoder: Decoder) throws {
            let container = try decoder.singleValueContainer()
            let raw = try container.decode(String.self)
            self = DrillDefinition.FieldKind(rawValue: raw) ?? .unknown
        }
    }

    public struct Field: Codable, Identifiable {
        public var id: String { key }
        public let key: String
        public let label: String
        public let kind: FieldKind
        public let isRequired: Bool
        public let defaultValue: JSONValue?
        public let options: [String]

        fileprivate init(
            key: String,
            label: String?,
            kind: FieldKind,
            required: Bool?,
            defaultValue: JSONValue?,
            options: [String]?
        ) {
            self.key = key
            self.label = label ?? key.replacingOccurrences(of: "_", with: " ").capitalized
            self.kind = kind
            self.isRequired = required ?? false
            self.defaultValue = defaultValue
            self.options = options ?? []
        }
    }

    public var id: String { key }
    public let key: String
    public let specIdentifier: String
    public let displayName: String
    public let details: String?
    public let version: Int?
    public let fields: [Field]

    fileprivate init(
        key: String,
        specIdentifier: String,
        label: String?,
        description: String?,
        version: Int?,
        fields: [Field]
    ) {
        self.key = key
        self.specIdentifier = specIdentifier
        self.displayName = label ?? key.replacingOccurrences(of: "_", with: " ").capitalized
        self.details = description
        self.version = version
        self.fields = fields.sorted { $0.label.localizedCaseInsensitiveCompare($1.label) == .orderedAscending }
    }

    fileprivate func makePayload() -> Payload {
        let fieldMap = Dictionary(uniqueKeysWithValues: fields.map { field in
            (field.key, FieldPayload(
                label: field.label,
                kind: field.kind,
                required: field.isRequired,
                defaultValue: field.defaultValue,
                options: field.options
            ))
        })
        return Payload(id: specIdentifier, label: displayName, description: details, version: version, fields: fieldMap)
    }
}

extension DrillDefinition {
    fileprivate struct Payload: Codable {
        var id: String
        var label: String?
        var description: String?
        var version: Int?
        var fields: [String: FieldPayload]

        func makeDefinition(key: String) -> DrillDefinition {
            let mappedFields = fields.map { fieldKey, payload in
                payload.makeField(key: fieldKey)
            }
            return DrillDefinition(
                key: key,
                specIdentifier: id,
                label: label,
                description: description,
                version: version,
                fields: mappedFields
            )
        }
    }

    fileprivate struct FieldPayload: Codable {
        var label: String?
        var kind: FieldKind
        var required: Bool?
        var defaultValue: JSONValue?
        var options: [String]?

        func makeField(key: String) -> Field {
            Field(key: key, label: label, kind: kind, required: required, defaultValue: defaultValue, options: options)
        }
    }
}
