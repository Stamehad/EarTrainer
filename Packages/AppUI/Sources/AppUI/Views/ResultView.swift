import SwiftUI
import Bridge

public struct ResultView: View {
    @EnvironmentObject private var viewModel: SessionViewModel

    public init() {}

    public var body: some View {
        VStack(spacing: 24) {
            if let summary = viewModel.summary {
                VStack(spacing: 12) {
                    Text("Session Complete")
                        .font(.title.weight(.semibold))
                    Text("You answered \(summary.correctAnswers) of \(summary.totalQuestions) correctly")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                    Divider()
                    StatRow(title: "Accuracy", value: accuracyText(summary))
                    StatRow(title: "Duration", value: durationText(summary))
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding()
                .background(
                    RoundedRectangle(cornerRadius: 16)
                        .fill(Color.panelBackground)
                )
            } else {
                Text("No summary available.")
                    .foregroundStyle(.secondary)
            }

            HStack(spacing: 16) {
                Button("Back to Entrance") {
                    viewModel.resetToEntrance()
                }
                .buttonStyle(.borderedProminent)

                Button("Exit") {
                    viewModel.exitApplication()
                }
                .buttonStyle(.bordered)
            }

            Spacer()
        }
        .padding()
    }

    private func accuracyText(_ summary: SessionSummary) -> String {
        guard summary.totalQuestions > 0 else { return "--" }
        let percent = Double(summary.correctAnswers) / Double(summary.totalQuestions) * 100
        return String(format: "%.0f%%", percent)
    }

    private func durationText(_ summary: SessionSummary) -> String {
        let seconds = Double(summary.durationMs) / 1000.0
        return String(format: "%.1f s", seconds)
    }
}

private struct StatRow: View {
    let title: String
    let value: String

    var body: some View {
        HStack {
            Text(title)
                .foregroundStyle(.secondary)
            Spacer()
            Text(value)
                .font(.headline)
        }
    }
}
