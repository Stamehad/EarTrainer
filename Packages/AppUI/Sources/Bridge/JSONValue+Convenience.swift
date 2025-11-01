import Foundation

extension JSONValue {
    public var intValue: Int? {
        switch self {
        case let .int(value):
            return value
        case let .double(value):
            return Int(value)
        case let .string(text):
            return Int(text)
        default:
            return nil
        }
    }

    public var doubleValue: Double? {
        switch self {
        case let .double(value):
            return value
        case let .int(value):
            return Double(value)
        case let .string(text):
            return Double(text)
        default:
            return nil
        }
    }

    public var boolValue: Bool? {
        switch self {
        case let .bool(value):
            return value
        case let .int(value):
            return value != 0
        case let .string(text):
            switch text.lowercased() {
            case "true", "yes", "1":
                return true
            case "false", "no", "0":
                return false
            default:
                return nil
            }
        default:
            return nil
        }
    }

    public var stringValue: String? {
        switch self {
        case let .string(value):
            return value
        case let .int(value):
            return String(value)
        case let .double(value):
            return String(value)
        case let .bool(value):
            return value ? "true" : "false"
        default:
            return nil
        }
    }

    public var arrayValue: [JSONValue]? {
        if case let .array(values) = self {
            return values
        }
        return nil
    }
}
