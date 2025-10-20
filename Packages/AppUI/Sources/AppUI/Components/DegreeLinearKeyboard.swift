import SwiftUI

struct DegreeLinearKeyboard: View {
    var onTap: (Degree) -> Void
    var showsLabels: Bool = true
    var keyCornerRadius: CGFloat = 12

    private let keys: [Degree] = Degree.allCases

    var body: some View {
        GeometryReader { geometry in
            let keyWidth = geometry.size.width / CGFloat(keys.count)
            let keyHeight = geometry.size.height

            ZStack(alignment: .topLeading) {
                ForEach(Array(keys.enumerated()), id: \.offset) { index, degree in
                    Button {
                        triggerHaptics()
                        onTap(degree)
                    } label: {
                        ZStack {
                            RoundedRectangle(cornerRadius: keyCornerRadius)
                                .fill(Color.white)
                                .overlay(
                                    RoundedRectangle(cornerRadius: keyCornerRadius)
                                        .stroke(Color.black.opacity(0.15), lineWidth: 1)
                                )
                                .shadow(color: .black.opacity(0.06), radius: 3, y: 2)

                            if showsLabels {
                                VStack(spacing: 4) {
                                    Text(degree.label)
                                        .font(.title3.weight(.semibold))
                                    Text(degree.solfege)
                                        .font(.caption)
                                        .opacity(0.7)
                                }
                                .foregroundStyle(.primary)
                            }
                        }
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Degree \(degree.label), \(degree.solfege)")
                    .frame(width: keyWidth - 6, height: keyHeight - 6)
                    .position(
                        x: (CGFloat(index) + 0.5) * keyWidth,
                        y: keyHeight / 2
                    )
                }
            }
            .background(Color(white: 0.95))
        }
    }

    private func triggerHaptics() {
#if canImport(UIKit)
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
#endif
    }
}
