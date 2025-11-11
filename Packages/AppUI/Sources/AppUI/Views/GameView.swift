import SwiftUI
import Bridge

private func normalizeDegreeIndex(_ value: Int) -> Int {
    let mod = value % 7
    return mod >= 0 ? mod : mod + 7
}

public struct GameView: View {
    @EnvironmentObject private var viewModel: SessionViewModel
    @Environment(\.verticalSizeClass) private var verticalSizeClass

    @State private var answerContext: AnswerContext?
    @State private var answerSlots: [Degree?] = []
    @State private var answerCursor: Int = 0
    @State private var submissionResult: SubmissionResult?
    @State private var inputLocked = false
    @State private var preparedQuestion: QuestionEnvelope?
    @State private var slotStates: [SlotState] = []

    private let defaultLatencyMs = 3_000

    public init() {}

    public var body: some View {
        VStack(spacing: 16) {
            if let envelope = viewModel.currentQuestion {
                questionContent(for: envelope)
            } else {
                noQuestionState
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .padding()
        .background(Color.appBackground)
        .onAppear {
            syncQuestionStateIfNeeded()
        }
        .onChange(of: viewModel.currentQuestion) { _ in
            syncQuestionStateIfNeeded()
        }
    }

    private func questionContent(for envelope: QuestionEnvelope) -> some View {
        VStack(spacing: 20) {
            header(for: envelope)
            if let context = answerContext {
                VStack(alignment: .leading, spacing: 12) {
                    Text(context.instruction)
                        .font(.headline)
                    AnswerSlotsView(
                        slots: answerSlots,
                        states: slotStates,
                        cursor: answerCursor,
                        result: submissionResult
                    )
                }
                if !answerSlots.isEmpty {
                    HStack {
                        Button("Clear") {
                            clearAnswer()
                        }
                        .disabled(inputLocked || answerSlots.allSatisfy { $0 == nil })
                        Spacer()
                    }
                }
                Spacer(minLength: 12)
                bottomInputSurface
            } else {
                unsupportedQuestionView(for: envelope)
                Spacer()
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
    }

    private func header(for envelope: QuestionEnvelope) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 16) {
                Button {
                    viewModel.playOrientationPrompt()
                } label: {
                    Image(systemName: "tuningfork")
                        .imageScale(.large)
                }
                .buttonStyle(.bordered)
                .accessibilityLabel("Play orientation prompt")

                if envelope.bundle.promptClip != nil {
                    Button {
                        viewModel.replayPromptAudio()
                    } label: {
                        Image(systemName: "music.note")
                            .imageScale(.large)
                    }
                    .buttonStyle(.bordered)
                    .accessibilityLabel("Replay question audio")
                }

                Spacer()

                Button("Finish Session") {
                    viewModel.finish()
                }
                .buttonStyle(.bordered)
            }
        }
    }

    private var bottomInputSurface: some View {
        VStack(spacing: 12) {
            Group {
                switch currentInputMode {
                case .keypad3x3:
                    DegreeGridKeypad(
                        onDegree: handleDegreeSelection,
                        onBackspace: handleBackspace,
                        onAdvance: handleAdvance
                    )
                    .frame(height: 260)
                case .keyboardLinear:
                    DegreeLinearKeyboard(onTap: handleDegreeSelection)
                        .frame(height: 180)
                case .ringPad:
                    DegreeRingPad(onTap: handleDegreeSelection)
                        .frame(height: 300)
                }
            }
            .disabled(inputLocked || answerContext == nil)
        }
    }

    private var noQuestionState: some View {
        VStack(spacing: 16) {
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
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
    }

    private func unsupportedQuestionView(for envelope: QuestionEnvelope) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("This question type isn’t supported yet.")
                .font(.headline)
            Text("Answer type: \(envelope.bundle.correctAnswer.type)")
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var currentInputMode: SessionViewModel.AnswerInputMode {
        if verticalSizeClass == .compact {
            return .keyboardLinear
        }
        return viewModel.answerInputMode
    }

    private func clearAnswer() {
        guard !answerSlots.isEmpty else { return }
        submissionResult = nil
        answerSlots = Array(repeating: nil, count: answerSlots.count)
        answerCursor = 0
        inputLocked = false
        slotStates = Array(repeating: .empty, count: answerSlots.count)
    }

    private func handleDegreeSelection(_ degree: Degree) {
        guard !inputLocked,
              !viewModel.isProcessing,
              let context = answerContext,
              !answerSlots.isEmpty else { return }
        submissionResult = nil

        switch currentInputMode {
        case .keypad3x3:
            setDegree(degree, at: answerCursor, context: context)
            if answerCursor < answerSlots.count - 1 {
                answerCursor += 1
            }
        case .keyboardLinear, .ringPad:
            if let targetIndex = answerSlots.firstIndex(where: { $0 == nil }) {
                setDegree(degree, at: targetIndex, context: context)
                answerCursor = min(targetIndex + 1, answerSlots.count - 1)
            } else if let lastIndex = answerSlots.indices.last {
                setDegree(degree, at: lastIndex, context: context)
                answerCursor = lastIndex
            }
        }
        evaluateIfReady(using: context)
    }

    private func handleBackspace() {
        guard currentInputMode == .keypad3x3,
              !inputLocked,
              !answerSlots.isEmpty else { return }
        submissionResult = nil

        if answerSlots[answerCursor] != nil {
            answerSlots[answerCursor] = nil
            if slotStates.indices.contains(answerCursor) {
                slotStates[answerCursor] = .empty
            }
        } else if answerCursor > 0 {
            answerCursor -= 1
            answerSlots[answerCursor] = nil
            if slotStates.indices.contains(answerCursor) {
                slotStates[answerCursor] = .empty
            }
        }
    }

    private func handleAdvance() {
        guard currentInputMode == .keypad3x3,
              !inputLocked,
              !answerSlots.isEmpty else { return }
        submissionResult = nil
        answerCursor = min(answerCursor + 1, answerSlots.count - 1)
    }

    private func evaluateIfReady(using context: AnswerContext) {
        guard answerSlots.allSatisfy({ $0 != nil }) else { return }
        submitAnswer(for: context)
    }

    private func submitAnswer(for context: AnswerContext) {
        guard !inputLocked else { return }
        let zeroIndexed = answerSlots.compactMap { $0?.zeroIndexedValue }
        guard zeroIndexed.count == context.expectedCount,
              let payload = context.makePayload(from: zeroIndexed) else { return }
        let normalizedInput = zeroIndexed.map(normalizeDegreeIndex)
        let isCorrect = normalizedInput == context.normalizedDegrees
        submissionResult = isCorrect ? .correct : .incorrect
        inputLocked = true
        viewModel.submit(
            answer: payload,
            isCorrect: isCorrect,
            latencyMs: defaultLatencyMs
        )
    }

    private func setDegree(_ degree: Degree, at index: Int, context: AnswerContext) {
        guard answerSlots.indices.contains(index),
              slotStates.indices.contains(index) else { return }
        answerSlots[index] = degree
        let expected = context.normalizedDegrees[index]
        let isCorrect = degree.zeroIndexedValue == expected
        slotStates[index] = isCorrect ? .correct : .incorrect
    }

    private func syncQuestionStateIfNeeded() {
        guard let envelope = viewModel.currentQuestion else {
            preparedQuestion = nil
            answerContext = nil
            answerSlots = []
            answerCursor = 0
            submissionResult = nil
            inputLocked = false
            slotStates = []
            return
        }

        guard preparedQuestion != envelope else { return }
        preparedQuestion = envelope
        let context = buildAnswerContext(from: envelope)
        answerContext = context
        if let context {
            answerSlots = Array(repeating: nil, count: context.expectedCount)
            slotStates = Array(repeating: .empty, count: context.expectedCount)
        } else {
            answerSlots = []
            slotStates = []
        }
        answerCursor = 0
        submissionResult = nil
        inputLocked = false
    }

    private func buildAnswerContext(from envelope: QuestionEnvelope) -> AnswerContext? {
        switch envelope.bundle.correctAnswer {
        case let .chord(answer):
            let roots = answer.rootDegrees.isEmpty ? [answer.rootDegree] : answer.rootDegrees
            return AnswerContext(
                kind: .chord,
                rawDegrees: roots,
                instruction: "Identify the chord’s scale degree."
            )
        case let .melody(answer):
            return AnswerContext(
                kind: .melody,
                rawDegrees: answer.melody,
                instruction: "Enter the degrees for each note in the melody."
            )
        case let .harmony(answer):
            return AnswerContext(
                kind: .harmony,
                rawDegrees: answer.notes,
                instruction: "Identify the harmony."
            )
        }
    }
}

// MARK: - Supporting Types

private struct AnswerSlotsView: View {
    let slots: [Degree?]
    let states: [GameView.SlotState]
    let cursor: Int
    let result: GameView.SubmissionResult?

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 10) {
                ForEach(Array(slots.enumerated()), id: \.offset) { index, slot in
                    slotView(for: slot, index: index)
                }
            }
            .frame(maxWidth: .infinity, alignment: .center)
            if let result {
                ResultBadge(result: result)
                    .transition(.opacity.combined(with: .move(edge: .top)))
                    .frame(maxWidth: .infinity, alignment: .center)
            }
        }
        .animation(.easeInOut(duration: 0.15), value: slots)
        .animation(.easeInOut(duration: 0.15), value: result)
        .frame(maxWidth: .infinity)
    }

    private func slotView(for slot: Degree?, index: Int) -> some View {
        let size: CGFloat = 54
        return RoundedRectangle(cornerRadius: 12)
            .fill(backgroundColor(for: index, slot: slot))
            .frame(width: size, height: size)
            .overlay(
                Text(slot?.label ?? "_")
                    .font(.system(size: 24, weight: .semibold, design: .rounded))
                    .foregroundStyle(.primary)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(borderColor(for: index), lineWidth: 2)
            )
    }

    private func backgroundColor(for index: Int, slot: Degree?) -> Color {
        if let result = result {
            return result.tint.opacity(0.18)
        }
        switch state(for: index) {
        case .correct:
            return Color.green.opacity(0.18)
        case .incorrect:
            return Color.red.opacity(0.18)
        case .empty:
            return slot == nil ? Color.secondary.opacity(0.08) : Color.secondary.opacity(0.18)
        }
    }

    private func borderColor(for index: Int) -> Color {
        if let result = result {
            return result.tint
        }
        switch state(for: index) {
        case .correct:
            return Color.green
        case .incorrect:
            return Color.red
        case .empty:
            return cursor == index ? Color.accentColor : Color.secondary.opacity(0.35)
        }
    }

    private func state(for index: Int) -> GameView.SlotState {
        guard states.indices.contains(index) else { return .empty }
        return states[index]
    }
}

private struct ResultBadge: View {
    let result: GameView.SubmissionResult

    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: result.systemImage)
                .foregroundStyle(result.tint)
            Text(result.message)
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(result.tint)
        }
        .padding(.vertical, 4)
    }
}

extension GameView {
    fileprivate enum SubmissionResult: Equatable {
        case correct
        case incorrect

        var isCorrect: Bool {
            switch self {
            case .correct:
                return true
            case .incorrect:
                return false
            }
        }

        var tint: Color {
            isCorrect ? .green : .red
        }

        var message: String {
            isCorrect ? "Correct" : "Incorrect"
        }

        var systemImage: String {
            isCorrect ? "checkmark.circle.fill" : "xmark.circle.fill"
        }
    }

    fileprivate enum SlotState: Equatable {
        case empty
        case correct
        case incorrect
    }

    fileprivate struct AnswerContext: Equatable {
        enum Kind: Equatable {
            case chord
            case melody
            case harmony
        }

        let kind: Kind
        let rawDegrees: [Int]
        let normalizedDegrees: [Int]
        let instruction: String

        var expectedCount: Int {
            max(rawDegrees.count, 1)
        }

        func makePayload(from zeroIndexedDegrees: [Int]) -> AnswerPayload? {
            let canonical = canonicalDegrees(from: zeroIndexedDegrees)
            switch kind {
            case .chord:
                guard let value = canonical.first else { return nil }
                return .chord(ChordAnswer(rootDegree: value))
            case .melody:
                return .melody(MelodyAnswer(melody: canonical))
            case .harmony:
                return .harmony(HarmonyAnswer(notes: canonical))
            }
        }

        init(kind: Kind, rawDegrees: [Int], instruction: String) {
            let sanitized = rawDegrees.isEmpty ? [0] : rawDegrees
            self.kind = kind
            self.rawDegrees = sanitized
            self.normalizedDegrees = sanitized.map(normalizeDegreeIndex)
            self.instruction = instruction
        }

        private func canonicalDegrees(from userInput: [Int]) -> [Int] {
            guard userInput.count == rawDegrees.count else { return userInput }
            return zip(userInput, rawDegrees).map { value, target in
                normalizeDegreeIndex(value) == normalizeDegreeIndex(target) ? target : value
            }
        }
    }
}
