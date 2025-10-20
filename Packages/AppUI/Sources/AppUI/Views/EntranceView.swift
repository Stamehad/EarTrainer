import SwiftUI

public struct EntranceView: View {
    private enum ModeSelection: String, CaseIterable, Identifiable {
        case adaptive
        case levelInspector = "level_inspector"

        var id: String { rawValue }

        var label: String {
            switch self {
            case .adaptive:
                return "Adaptive"
            case .levelInspector:
                return "Level Inspector"
            }
        }
    }

    private let adaptiveTrackLevels: [Int] = [1, 111, 220]
    private struct KeyOption: Identifiable {
        let id: String
        let label: String
        let value: String

        init(label: String, value: String) {
            self.id = value
            self.label = label
            self.value = value
        }

        init(id: String, label: String, value: String) {
            self.id = id
            self.label = label
            self.value = value
        }
    }

    private let keyOptions: [KeyOption] = [
        KeyOption(label: "C", value: "C"),
        KeyOption(id: "C#", label: "C#/Db", value: "C#"),
        KeyOption(label: "D", value: "D"),
        KeyOption(id: "Eb", label: "D#/Eb", value: "Eb"),
        KeyOption(label: "E", value: "E"),
        KeyOption(label: "F", value: "F"),
        KeyOption(id: "F#", label: "F#/Gb", value: "F#"),
        KeyOption(label: "G", value: "G"),
        KeyOption(id: "Ab", label: "G#/Ab", value: "Ab"),
        KeyOption(label: "A", value: "A"),
        KeyOption(id: "Bb", label: "A#/Bb", value: "Bb"),
        KeyOption(label: "B", value: "B")
    ]

    private let keyCanonicalMap: [String: String] = [
        "C": "C",
        "C#": "C#",
        "Db": "C#",
        "D": "D",
        "D#": "Eb",
        "Eb": "Eb",
        "E": "E",
        "F": "F",
        "F#": "F#",
        "Gb": "F#",
        "G": "G",
        "G#": "Ab",
        "Ab": "Ab",
        "A": "A",
        "A#": "Bb",
        "Bb": "Bb",
        "B": "B"
    ]

    @EnvironmentObject private var viewModel: SessionViewModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var selectedMode: ModeSelection = .adaptive
    @State private var selectedKeyValue: String = "C"
    @State private var inspectorSelectionID: String = ""

    public init() {}

    public var body: some View {
        NavigationStack {
            Group {
                switch viewModel.route {
                case .entrance:
                    entranceForm
                case .game:
                    GameView()
                case .results:
                    ResultView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .background(Color.appBackground)
            .navigationTitle("Ear Trainer")
        }
        .onAppear {
            viewModel.bootstrap()
            let canonicalKey = canonicalKeyValue(viewModel.spec.key)
            selectedKeyValue = canonicalKey
            viewModel.spec.key = canonicalKey
            selectedMode = viewModel.spec.levelInspect ? .levelInspector : .adaptive
            if selectedMode == .adaptive && viewModel.spec.trackLevels != adaptiveTrackLevels {
                viewModel.spec.trackLevels = adaptiveTrackLevels
            }
            syncSpecWithSelectedMode(preserveInspectorValues: true)
            if selectedMode == .levelInspector {
                viewModel.loadInspectorOptionsIfNeeded()
                updateInspectorSelectionFromSpec()
            }
        }
        .onChange(of: scenePhase) { newPhase in
            viewModel.handleScenePhase(newPhase)
        }
        .onChange(of: selectedMode) { newMode in
            syncSpecWithSelectedMode(preserveInspectorValues: false)
            if newMode == .levelInspector {
                viewModel.loadInspectorOptionsIfNeeded()
                updateInspectorSelectionFromSpec()
            }
        }
        .onChange(of: selectedKeyValue) { newValue in
            viewModel.spec.key = newValue
        }
        .onChange(of: inspectorSelectionID) { newValue in
            applyInspectorSelection(for: newValue)
        }
        .onReceive(viewModel.$inspectorOptions) { _ in
            if selectedMode == .levelInspector {
                updateInspectorSelectionFromSpec()
            }
        }
        .overlay(alignment: .top) {
            if let message = viewModel.errorBanner {
                ErrorBannerView(message: message) {
                    viewModel.errorBanner = nil
                }
            }
        }
    }

    private var entranceForm: some View {
        Form {
            Section("Mode") {
                Picker("Mode", selection: $selectedMode) {
                    ForEach(ModeSelection.allCases) { mode in
                        Text(mode.label).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
            }

            Section("Session") {
                VStack(alignment: .leading, spacing: 12) {
                    HStack {
                        Text("Key")
                        Spacer()
                        Picker(selection: Binding(
                            get: { selectedKeyValue },
                            set: { newValue in selectedKeyValue = newValue }
                        ), content: {
                            ForEach(keyOptions) { option in
                                Text(option.label).tag(option.value)
                            }
                        }, label: {
                            EmptyView()
                        })
                        #if os(iOS)
                        .pickerStyle(.menu)
                        //.pickerStyle(.wheel)
                        #else
                        .pickerStyle(.menu)
                        #endif
                        .labelsHidden()
                        .frame(maxWidth: 140)
                    }
                    HStack {
                        Text("Questions")
                        Spacer()
                        Text("\(viewModel.spec.nQuestions)")
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                        Stepper(value: Binding(
                            get: { viewModel.spec.nQuestions },
                            set: { viewModel.spec.nQuestions = max(1, $0) }
                        ), in: 1...50) {
                            EmptyView()
                        }
                        .frame(width: 94)
                        .labelsHidden()
                    }
                }
            }

            if selectedMode == .levelInspector {
                Section("Level Inspector") {
                    if viewModel.inspectorOptions.isEmpty {
                        ProgressView("Loading levels…")
                            .frame(maxWidth: .infinity, alignment: .center)
                            .task {
                                viewModel.loadInspectorOptionsIfNeeded()
                                updateInspectorSelectionFromSpec()
                            }
                    } else {
                        Picker("Level & Tier", selection: Binding(
                            get: { inspectorSelectionID },
                            set: { newValue in inspectorSelectionID = newValue }
                        )) {
                            ForEach(viewModel.inspectorOptions) { option in
                                Text(option.label).tag(option.id)
                            }
                        }
                        #if os(iOS)
                        //.pickerStyle(.menu)
                        .pickerStyle(.wheel)
                        #else
                        .pickerStyle(.menu)
                        #endif
                    }
                }
            }

            Section {
                Button(action: startSession) {
                    HStack {
                        if viewModel.isProcessing {
                            ProgressView()
                                .progressViewStyle(.circular)
                        }
                        Text("Start Session")
                            .frame(maxWidth: .infinity)
                    }
                }
                .disabled(viewModel.isProcessing)
            }
        }
        .scrollContentBackground(.hidden)
    }

    private func startSession() {
        viewModel.spec.key = selectedKeyValue
        switch selectedMode {
        case .adaptive:
            viewModel.spec.adaptive = true
            viewModel.spec.levelInspect = false
            viewModel.spec.trackLevels = adaptiveTrackLevels
            viewModel.spec.inspectLevel = nil
            viewModel.spec.inspectTier = nil
        case .levelInspector:
            viewModel.spec.adaptive = false
            viewModel.spec.levelInspect = true
            viewModel.spec.trackLevels = []
            applyInspectorSelection(for: inspectorSelectionID)
        }
        viewModel.start()
    }

    private func syncSpecWithSelectedMode(preserveInspectorValues: Bool) {
        switch selectedMode {
        case .adaptive:
            viewModel.spec.adaptive = true
            viewModel.spec.levelInspect = false
            viewModel.spec.trackLevels = adaptiveTrackLevels
            viewModel.spec.inspectLevel = nil
            viewModel.spec.inspectTier = nil
            inspectorSelectionID = ""
        case .levelInspector:
            viewModel.spec.adaptive = false
            viewModel.spec.levelInspect = true
            viewModel.spec.trackLevels = []
            if !preserveInspectorValues {
                viewModel.spec.inspectLevel = nil
                viewModel.spec.inspectTier = nil
                inspectorSelectionID = ""
            } else if let level = viewModel.spec.inspectLevel, let tier = viewModel.spec.inspectTier {
                inspectorSelectionID = inspectorIdentifier(level: level, tier: tier)
            }
        }
    }

    private func canonicalKeyValue(_ key: String) -> String {
        let trimmed = key.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return "C" }
        let components = trimmed.split(separator: " ")
        var tonic = components.first.map(String.init) ?? "C"
        tonic = tonic.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !tonic.isEmpty else { return "C" }
        let firstChar = tonic.removeFirst()
        var normalized = String(firstChar).uppercased()
        if let modifier = tonic.first, modifier == "#" || modifier == "b" || modifier == "B" {
            normalized.append(modifier == "B" ? "b" : String(modifier))
        }
        if let canonical = keyCanonicalMap[normalized] {
            return canonical
        }
        let head = String(normalized.prefix(1))
        return keyCanonicalMap[head] ?? "C"
    }

    private func inspectorIdentifier(level: Int, tier: Int) -> String {
        "\(level)-\(tier)"
    }

    private func applyInspectorSelection(for id: String) {
        guard !id.isEmpty,
              let entry = viewModel.inspectorOptions.first(where: { $0.id == id }) else {
            viewModel.spec.inspectLevel = nil
            viewModel.spec.inspectTier = nil
            return
        }
        viewModel.spec.inspectLevel = entry.level
        viewModel.spec.inspectTier = entry.tier
    }

    private func updateInspectorSelectionFromSpec() {
        if let level = viewModel.spec.inspectLevel,
           let tier = viewModel.spec.inspectTier {
            let id = inspectorIdentifier(level: level, tier: tier)
            if viewModel.inspectorOptions.contains(where: { $0.id == id }) {
                inspectorSelectionID = id
                return
            }
        }
        if let first = viewModel.inspectorOptions.first {
            inspectorSelectionID = first.id
            applyInspectorSelection(for: first.id)
        } else {
            inspectorSelectionID = ""
        }
    }

}
