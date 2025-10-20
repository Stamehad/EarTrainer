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

    @EnvironmentObject private var viewModel: SessionViewModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var seedText: String = ""
    @State private var tempoText: String = ""
    @State private var initialFitnessText: String = ""
    @State private var selectedMode: ModeSelection = .adaptive
    @State private var inspectorLevelText: String = ""
    @State private var inspectorTierText: String = ""

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
            seedText = String(viewModel.spec.seed)
            tempoText = viewModel.spec.tempoBpm.map(String.init) ?? ""
            if case let .double(value) = viewModel.spec.samplerParams["initial_fitness"] {
                initialFitnessText = String(value)
            } else if case let .int(value) = viewModel.spec.samplerParams["initial_fitness"] {
                initialFitnessText = String(value)
            } else {
                initialFitnessText = ""
            }
            selectedMode = viewModel.spec.levelInspect ? .levelInspector : .adaptive
            inspectorLevelText = viewModel.spec.inspectLevel.map(String.init) ?? ""
            inspectorTierText = viewModel.spec.inspectTier.map(String.init) ?? ""
            if selectedMode == .adaptive && viewModel.spec.trackLevels != adaptiveTrackLevels {
                viewModel.spec.trackLevels = adaptiveTrackLevels
            }
            syncSpecWithSelectedMode(preserveInspectorValues: true)
        }
        .onChange(of: scenePhase) { newPhase in
            viewModel.handleScenePhase(newPhase)
        }
        .onChange(of: selectedMode) { _ in
            syncSpecWithSelectedMode(preserveInspectorValues: false)
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
                TextField("Drill Kind", text: Binding(
                    get: { viewModel.spec.drillKind },
                    set: { viewModel.spec.drillKind = $0 }
                ))
                TextField("Key", text: Binding(
                    get: { viewModel.spec.key },
                    set: { viewModel.spec.key = $0 }
                ))
                Stepper(value: Binding(
                    get: { rangeValue(index: 0) },
                    set: { setRangeValue($0, index: 0) }
                ), in: 0...120) {
                    HStack {
                        Text("Range Min (MIDI)")
                        Spacer()
                        Text("\(rangeValue(index: 0))")
                            .foregroundStyle(.secondary)
                    }
                }
                Stepper(value: Binding(
                    get: { rangeValue(index: 1) },
                    set: { setRangeValue($0, index: 1) }
                ), in: 0...127) {
                    HStack {
                        Text("Range Max (MIDI)")
                        Spacer()
                        Text("\(rangeValue(index: 1))")
                            .foregroundStyle(.secondary)
                    }
                }
                Stepper(value: Binding(
                    get: { viewModel.spec.nQuestions },
                    set: { viewModel.spec.nQuestions = max(1, $0) }
                ), in: 1...50) {
                    HStack {
                        Text("Questions")
                        Spacer()
                        Text("\(viewModel.spec.nQuestions)")
                            .foregroundStyle(.secondary)
                    }
                }
                #if os(iOS)
                TextField("Tempo BPM (optional)", text: $tempoText)
                    .keyboardType(.numberPad)
                #else
                TextField("Tempo BPM (optional)", text: $tempoText)
                #endif
                #if os(iOS)
                TextField("Initial Fitness (optional)", text: $initialFitnessText)
                    .keyboardType(.decimalPad)
                #else
                TextField("Initial Fitness (optional)", text: $initialFitnessText)
                #endif
                #if os(iOS)
                TextField("Seed", text: $seedText)
                    .keyboardType(.numberPad)
                #else
                TextField("Seed", text: $seedText)
                #endif
            }

            if selectedMode == .levelInspector {
                Section("Level Inspector") {
                    #if os(iOS)
                    TextField("Level", text: $inspectorLevelText)
                        .keyboardType(.numberPad)
                    #else
                    TextField("Level", text: $inspectorLevelText)
                    #endif
                    #if os(iOS)
                    TextField("Tier", text: $inspectorTierText)
                        .keyboardType(.numberPad)
                    #else
                    TextField("Tier", text: $inspectorTierText)
                    #endif
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
        if let value = Int(seedText) {
            viewModel.spec.seed = value
        }
        if let tempo = Int(tempoText) {
            viewModel.spec.tempoBpm = tempo
        } else {
            viewModel.spec.tempoBpm = nil
        }
        if let fitness = Double(initialFitnessText) {
            viewModel.spec.samplerParams["initial_fitness"] = .double(fitness)
        } else {
            viewModel.spec.samplerParams.removeValue(forKey: "initial_fitness")
        }
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
            viewModel.spec.inspectLevel = parseInteger(inspectorLevelText)
            viewModel.spec.inspectTier = parseInteger(inspectorTierText)
        }
        viewModel.start()
    }

    private func parseInteger(_ text: String) -> Int? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        return Int(trimmed)
    }

    private func syncSpecWithSelectedMode(preserveInspectorValues: Bool) {
        switch selectedMode {
        case .adaptive:
            viewModel.spec.adaptive = true
            viewModel.spec.levelInspect = false
            viewModel.spec.trackLevels = adaptiveTrackLevels
            viewModel.spec.inspectLevel = nil
            viewModel.spec.inspectTier = nil
            if !preserveInspectorValues {
                inspectorLevelText = ""
                inspectorTierText = ""
            }
        case .levelInspector:
            viewModel.spec.adaptive = false
            viewModel.spec.levelInspect = true
            viewModel.spec.trackLevels = []
            if !preserveInspectorValues {
                viewModel.spec.inspectLevel = nil
                viewModel.spec.inspectTier = nil
                inspectorLevelText = ""
                inspectorTierText = ""
            }
        }
    }

    private func rangeValue(index: Int) -> Int {
        guard viewModel.spec.range.indices.contains(index) else { return 0 }
        return viewModel.spec.range[index]
    }

    private func setRangeValue(_ value: Int, index: Int) {
        if viewModel.spec.range.count < 2 {
            viewModel.spec.range = [48, 72]
        }
        viewModel.spec.range[index] = value
        if index == 0 {
            viewModel.spec.range[0] = min(value, viewModel.spec.range[1])
        } else {
            viewModel.spec.range[1] = max(value, viewModel.spec.range[0])
        }
    }
}
