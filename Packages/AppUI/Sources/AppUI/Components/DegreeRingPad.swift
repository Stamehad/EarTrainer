import SwiftUI

struct DegreeRingPad: View {
    var onTap: (Degree) -> Void

    private let order: [Degree] = Degree.allCases

    var body: some View {
        GeometryReader { geometry in
            let size = min(geometry.size.width, geometry.size.height)
            let ringRadius = size * 0.36

            ZStack {
                Circle()
                    .strokeBorder(style: StrokeStyle(lineWidth: 2, dash: [6, 6]))
                    .frame(width: ringRadius * 1.2, height: ringRadius * 1.2)
                    .opacity(0.2)

                ForEach(Array(order.enumerated()), id: \.offset) { index, degree in
                    let angle = Angle(degrees: -90 + (Double(index) * 360 / Double(order.count)))
                    let offsetX = cos(angle.radians) * ringRadius
                    let offsetY = sin(angle.radians) * ringRadius

                    Button {
                        triggerHaptics()
                        onTap(degree)
                    } label: {
                        VStack(spacing: 4) {
                            Text(degree.label)
                                .font(.system(size: 22, weight: .semibold, design: .rounded))
                            Text(degree.solfege)
                                .font(.caption)
                                .opacity(0.7)
                        }
                        .frame(width: size * 0.18, height: size * 0.18)
                        .background(.ultraThinMaterial)
                        .clipShape(Circle())
                        .overlay(
                            Circle()
                                .stroke(.primary.opacity(0.15), lineWidth: 1)
                        )
                        .shadow(radius: 3, y: 2)
                        .contentShape(Circle())
                        .accessibilityLabel("Degree \(degree.label), \(degree.solfege)")
                    }
                    .position(
                        x: geometry.size.width / 2 + offsetX,
                        y: geometry.size.height / 2 + offsetY
                    )
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    private func triggerHaptics() {
#if canImport(UIKit)
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
#endif
    }
}
