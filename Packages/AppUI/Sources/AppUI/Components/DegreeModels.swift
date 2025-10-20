import Foundation

enum Degree: Int, CaseIterable, Identifiable {
    case d1 = 1, d2, d3, d4, d5, d6, d7

    var id: Int { rawValue }

    var label: String {
        String(rawValue)
    }

    var solfege: String {
        switch self {
        case .d1: return "do"
        case .d2: return "re"
        case .d3: return "mi"
        case .d4: return "fa"
        case .d5: return "sol"
        case .d6: return "la"
        case .d7: return "ti"
        }
    }

    var zeroIndexedValue: Int {
        rawValue - 1
    }

    static func fromZeroIndexed(_ value: Int) -> Degree? {
        Degree(rawValue: value + 1)
    }
}
