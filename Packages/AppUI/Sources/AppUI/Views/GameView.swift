import SwiftUI
import Bridge

public struct GameView: View {
    @EnvironmentObject private var viewModel: SessionViewModel
    @State private var questionStartedAt = Date()

    public init() {}

    public var body: some View {
        VStack(spacing: 20) {
            if let envelope = viewModel.currentQuestion {
                VStack(spacing: 16) {
                    HStack {
                        Button("Play Orientation") {
                            viewModel.playOrientationPrompt()
                        }
                        .buttonStyle(.bordered)
                        Spacer()
                    }
                    ScrollView {
                        VStack(alignment: .leading, spacing: 16) {
                            Text("Question ID: \(envelope.bundle.questionId)")
                                .font(.headline)
                            if let questionJSON = viewModel.questionJSON {
                                DebugPanel(title: "Question Highlights", content: questionJSON)
                            }
                            if let debugJSON = viewModel.debugJSON, !debugJSON.isEmpty {
                                DebugPanel(title: "Engine Diagnostic Summary", content: debugJSON)
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    HStack(spacing: 16) {
                        if viewModel.currentQuestion?.bundle.prompt?.midiClip != nil {
                            Button("Replay Audio") {
                                viewModel.replayPromptAudio()
                            }
                            .buttonStyle(.bordered)
                        }

                        Button("Submit Auto Result") {
                            submitAnswer()
                        }
                        .buttonStyle(.borderedProminent)

                        Button("Finish Session") {
                            viewModel.finish()
                        }
                        .buttonStyle(.bordered)

                        Spacer()
                    }
                }
            } else {
                if #available(iOS 17, macOS 14, *) {
                    ContentUnavailableView(label: {
                        Label("No question", systemImage: "questionmark")
                    }, description: {
                        Text("Tap Back to return to the entrance screen.")
                    })
                } else {
                    Text("No question available")
                        .foregroundStyle(.secondary)
                }
                Button("Back to Entrance") {
                    viewModel.resetToEntrance()
                }
            }
        }
        .padding()
        .onAppear {
            questionStartedAt = Date()
        }
        .onChange(of: viewModel.currentQuestion?.bundle.questionId) { _ in
            questionStartedAt = Date()
        }
    }

    private func submitAnswer() {
        let latency = Int(Date().timeIntervalSince(questionStartedAt) * 1000)
        viewModel.submit(latencyMs: max(latency, 0))
    }
}

private struct DebugPanel: View {
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
            .frame(maxHeight: 220)
            .padding(12)
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(Color.panelBackground)
            )
        }
    }
}
