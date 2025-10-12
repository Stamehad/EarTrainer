import SwiftUI
import Bridge

public struct ResultView: View {
    @EnvironmentObject private var viewModel: SessionViewModel

    public init() {}

    public var body: some View {
        VStack(spacing: 24) {
            if let summary = viewModel.summary {
                SummaryContent(summary: summary)
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
}

private struct SummaryContent: View {
    let summary: SessionSummary

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Session Complete")
                .font(.title.weight(.semibold))
            Text("Session ID: \(summary.sessionId)")
                .font(.footnote)
                .foregroundStyle(.secondary)
            SummaryPanel(title: "Totals", content: summary.totals.prettyPrinted())
            SummaryPanel(title: "By Category", content: summary.byCategory.prettyPrinted())
            SummaryPanel(title: "Results", content: summary.results.prettyPrinted())
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color.panelBackground)
        )
    }
}

private struct SummaryPanel: View {
    let title: String
    let content: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.subheadline.weight(.semibold))
            ScrollView(.vertical, showsIndicators: true) {
                Text(content)
                    .font(.system(.footnote, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
            }
            .frame(maxHeight: 160)
            .padding(10)
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(Color.panelBackground)
            )
        }
    }
}
