import SwiftUI
import Bridge
import Foundation

public struct ResultView: View {
    @EnvironmentObject private var viewModel: SessionViewModel

    public init() {}

    public var body: some View {
        VStack(spacing: 24) {
            if let summary = viewModel.summary {
                SummaryContent(summary: summary)
                if let adaptive = viewModel.lastMemory?.adaptiveSummaryModel {
                    AdaptiveSummaryView(summary: adaptive)
                }
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
            SummaryPanel(title: "Totals", content: summary.totals.readableDescription())
            SummaryPanel(title: "By Category", content: summary.byCategory.readableDescription())
            SummaryPanel(title: "Results", content: summary.results.readableDescription())
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
        let displayText = content.isEmpty ? "No data available." : content
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.subheadline.weight(.semibold))
            ScrollView(.vertical, showsIndicators: true) {
                Text(displayText)
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

private struct AdaptiveSummaryView: View {
    let summary: AdaptiveSummaryModel

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Adaptive Outcome")
                .font(.title3.weight(.semibold))

            HStack {
                Label(summary.levelUp ? "Promotion" : "Hold", systemImage: summary.levelUp ? "arrow.up.circle.fill" : "pause.circle")
                    .font(.headline)
                    .foregroundStyle(summary.levelUp ? .green : .secondary)
                Spacer()
                Text("Bout: \(formatScore(summary.boutAverage)) / \(formatScore(summary.graduateThreshold))")
                    .font(.subheadline.monospaced())
            }

            if let level = summary.level {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Track: \(level.trackName.isEmpty ? "Track \(level.trackIndex + 1)" : level.trackName)")
                        .font(.subheadline.weight(.semibold))
                    Text("Current lesson: \(level.currentLevel)")
                    if let suggested = level.suggestedLevel {
                        Text("Suggested lesson: \(suggested)")
                            .foregroundStyle(.primary)
                    } else {
                        Text("No change suggested")
                            .foregroundStyle(.secondary)
                    }
                }
                .font(.footnote)
            }

            if !summary.drills.isEmpty {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Drill Scores")
                        .font(.subheadline.weight(.semibold))
                    ForEach(summary.drills) { drill in
                        HStack {
                            Text(drill.id)
                            Spacer()
                            Text(drill.family.capitalized)
                                .foregroundStyle(.secondary)
                            Text(formatScore(drill.score))
                                .font(.system(.footnote, design: .monospaced))
                        }
                        .font(.footnote)
                    }
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 16)
                .fill(Color.panelBackground)
        )
    }

    private func formatScore(_ value: Double?) -> String {
        guard let value else { return "–" }
        return String(format: "%.2f", value)
    }
}

private struct AdaptiveSummaryModel {
    struct Drill: Identifiable {
        let id: String
        let family: String
        let score: Double?
    }

    struct LevelDecision {
        let trackIndex: Int
        let trackName: String
        let currentLevel: Int
        let suggestedLevel: Int?
    }

    let hasScore: Bool
    let boutAverage: Double?
    let graduateThreshold: Double?
    let levelUp: Bool
    let drills: [Drill]
    let level: LevelDecision?

    init?(json value: JSONValue) {
        guard case let .object(dict) = value else { return nil }
        hasScore = dict["has_score"]?.boolValue ?? false
        boutAverage = dict["bout_average"]?.doubleValue
        graduateThreshold = dict["graduate_threshold"]?.doubleValue
        levelUp = dict["level_up"]?.boolValue ?? false

        if let drillValue = dict["drills"], case let .object(entries) = drillValue {
            var parsed: [Drill] = []
            for (key, payload) in entries {
                guard case let .object(info) = payload else { continue }
                let family = info["family"]?.stringValue ?? ""
                let score = info["ema_score"]?.doubleValue
                parsed.append(Drill(id: key, family: family, score: score))
            }
            drills = parsed.sorted { $0.id < $1.id }
        } else {
            drills = []
        }

        if let levelValue = dict["level"], case let .object(levelDict) = levelValue {
            level = LevelDecision(
                trackIndex: levelDict["track_index"]?.intValue ?? -1,
                trackName: levelDict["track_name"]?.stringValue ?? "",
                currentLevel: levelDict["current_level"]?.intValue ?? 0,
                suggestedLevel: levelDict["suggested_level"]?.intValue
            )
        } else {
            level = nil
        }
    }
}

private extension MemoryPackage {
    var adaptiveSummaryModel: AdaptiveSummaryModel? {
        guard let adaptive else { return nil }
        return AdaptiveSummaryModel(json: adaptive)
    }
}
