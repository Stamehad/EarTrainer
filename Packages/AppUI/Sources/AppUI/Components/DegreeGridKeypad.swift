import SwiftUI

struct DegreeGridKeypad: View {
    var onDegree: (Degree) -> Void
    var onBackspace: () -> Void
    var onAdvance: () -> Void

    var keyCornerRadius: CGFloat = 12
    var keySpacing: CGFloat = 8

    private struct Key: Identifiable {
        enum Kind {
            case degree(Degree)
            case backspace
            case advance
        }

        let id = UUID()
        let kind: Kind
        let title: String
        let subtitle: String?
    }

    private var rows: [[Key]] {
        [
            [.init(kind: .degree(.d1), title: "1", subtitle: "do"),
             .init(kind: .degree(.d2), title: "2", subtitle: "re"),
             .init(kind: .degree(.d3), title: "3", subtitle: "mi")],

            [.init(kind: .degree(.d4), title: "4", subtitle: "fa"),
             .init(kind: .degree(.d5), title: "5", subtitle: "sol"),
             .init(kind: .degree(.d6), title: "6", subtitle: "la")],

            [.init(kind: .backspace, title: "←", subtitle: nil),
             .init(kind: .degree(.d7), title: "7", subtitle: "ti"),
             .init(kind: .advance, title: "→", subtitle: nil)]
        ]
    }

    var body: some View {
        GeometryReader { geometry in
            let totalWidth = max(geometry.size.width, 1)
            let totalHeight = max(geometry.size.height, 1)
            let columns: CGFloat = 3
            let rowsCount: CGFloat = 3
            let innerWidth = totalWidth - keySpacing * (columns - 1)
            let innerHeight = totalHeight - keySpacing * (rowsCount - 1)
            let keyWidth = max(innerWidth / columns, 44)
            let keyHeight = max(innerHeight / rowsCount, 44)

            VStack(spacing: keySpacing) {
                ForEach(0..<rows.count, id: \.self) { rowIndex in
                    HStack(spacing: keySpacing) {
                        ForEach(rows[rowIndex]) { key in
                            button(for: key)
                                .frame(width: keyWidth, height: keyHeight)
                        }
                    }
                }
            }
            .frame(width: totalWidth, height: totalHeight)
        }
        .accessibilityElement(children: .contain)
    }

    @ViewBuilder
    private func button(for key: Key) -> some View {
        Button {
            triggerHaptics()
            switch key.kind {
            case .degree(let degree):
                onDegree(degree)
            case .backspace:
                onBackspace()
            case .advance:
                onAdvance()
            }
        } label: {
            ZStack {
                RoundedRectangle(cornerRadius: keyCornerRadius)
                    .fill(Color.white)
                    .overlay(
                        RoundedRectangle(cornerRadius: keyCornerRadius)
                            .stroke(Color.black.opacity(0.15), lineWidth: 1)
                    )
                    .shadow(color: .black.opacity(0.06), radius: 3, y: 2)

                VStack(spacing: 4) {
                    Text(key.title)
                        .font(.system(size: 22, weight: .semibold, design: .rounded))
                        .foregroundStyle(.primary)
                    if let subtitle = key.subtitle {
                        Text(subtitle)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .padding(6)
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel(accessibilityLabel(for: key))
    }

    private func accessibilityLabel(for key: Key) -> String {
        switch key.kind {
        case .degree(let degree):
            return "Degree \(degree.label), \(degree.solfege)"
        case .backspace:
            return "Move left"
        case .advance:
            return "Move right"
        }
    }

    private func triggerHaptics() {
#if canImport(UIKit)
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
#endif
    }
}
