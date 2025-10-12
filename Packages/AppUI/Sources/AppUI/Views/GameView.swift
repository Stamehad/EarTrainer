import SwiftUI
import Bridge

public struct GameView: View {
    @EnvironmentObject private var viewModel: SessionViewModel
    @State private var selectedChoiceId: String?
    @State private var questionStartedAt = Date()

    public init() {}

    public var body: some View {
        VStack(spacing: 24) {
            if let question = viewModel.currentQuestion {
                Text(question.prompt)
                    .font(.title3.weight(.semibold))
                    .frame(maxWidth: .infinity, alignment: .leading)
                VStack(alignment: .leading, spacing: 12) {
                    ForEach(question.choices) { choice in
                        ChoiceRow(choice: choice, isSelected: selectedChoiceId == choice.id) {
                            selectedChoiceId = choice.id
                        }
                    }
                }

                if let result = viewModel.attemptResult {
                    AttemptResultView(result: result)
                        .transition(.opacity)
                }

                HStack(spacing: 16) {
                    Button("Check") {
                        submitAnswer()
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(selectedChoiceId == nil || viewModel.attemptResult != nil)

                    Button("Next") {
                        viewModel.next()
                    }
                    .buttonStyle(.bordered)
                    .disabled(viewModel.attemptResult == nil)

                    Spacer()

                    Button("Finish") {
                        viewModel.finish()
                    }
                    .buttonStyle(.borderless)
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
        .onChange(of: viewModel.currentQuestion?.id) { _ in
            questionStartedAt = Date()
            selectedChoiceId = nil
        }
    }

    private func submitAnswer() {
        guard let choiceId = selectedChoiceId else { return }
        let latency = Int(Date().timeIntervalSince(questionStartedAt) * 1000)
        viewModel.submit(answer: choiceId, latencyMs: max(latency, 0))
    }
}

private struct ChoiceRow: View {
    let choice: Question.Choice
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action, label: {
            HStack {
                Text(choice.text)
                    .foregroundStyle(.primary)
                Spacer()
                if isSelected {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(.accentColor)
                }
            }
            .padding()
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(isSelected ? Color.accentColor.opacity(0.15) : Color.panelBackground)
            )
        })
        .buttonStyle(.plain)
    }
}

private struct AttemptResultView: View {
    let result: AttemptResult

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label(result.correct ? "Correct" : "Try Again", systemImage: result.correct ? "checkmark.circle" : "xmark.circle")
                .font(.headline)
                .foregroundStyle(result.correct ? Color.green : Color.orange)
            if let message = result.message {
                Text(message)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 14)
                .fill(Color.panelBackground)
        )
    }
}
