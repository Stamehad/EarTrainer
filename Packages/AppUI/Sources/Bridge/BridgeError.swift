import Foundation

public enum BridgeError: LocalizedError {
    case engineError(String)
    case encodingFailed(Error)
    case decodingFailed(Error)
    case missingData(String)

    public var errorDescription: String? {
        switch self {
        case let .engineError(message):
            return message
        case let .encodingFailed(error),
             let .decodingFailed(error):
            return error.localizedDescription
        case let .missingData(context):
            return "Missing data: \(context)"
        }
    }
}
