import SwiftUI

public struct EntranceView: View {
    @EnvironmentObject private var viewModel: SessionViewModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var seedText: String = ""
    @State private var tempoText: String = ""
    @State private var trackLevelsText: String = ""
    @State private var initialFitnessText: String = ""

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
            trackLevelsText = viewModel.spec.trackLevels.map(String.init).joined(separator: ", ")
            if case let .double(value) = viewModel.spec.samplerParams["initial_fitness"] {
                initialFitnessText = String(value)
            } else if case let .int(value) = viewModel.spec.samplerParams["initial_fitness"] {
                initialFitnessText = String(value)
            } else {
                initialFitnessText = ""
            }
        }
        .onChange(of: scenePhase) { newPhase in
            viewModel.handleScenePhase(newPhase)
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
                Toggle("Adaptive Bout", isOn: Binding(
                    get: { viewModel.spec.adaptive },
                    set: { viewModel.spec.adaptive = $0 }
                ))
                #if os(iOS)
                TextField("Tempo BPM (optional)", text: $tempoText)
                    .keyboardType(.numberPad)
                #else
                TextField("Tempo BPM (optional)", text: $tempoText)
                #endif
                #if os(iOS)
                TextField("Track Levels (comma-separated)", text: $trackLevelsText)
                    .textInputAutocapitalization(.never)
                #else
                TextField("Track Levels (comma-separated)", text: $trackLevelsText)
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
        let levels = trackLevelsText
            .split(separator: ",")
            .compactMap { Int($0.trimmingCharacters(in: .whitespacesAndNewlines)) }
        viewModel.spec.trackLevels = levels
        if let fitness = Double(initialFitnessText) {
            viewModel.spec.samplerParams["initial_fitness"] = .double(fitness)
        } else {
            viewModel.spec.samplerParams.removeValue(forKey: "initial_fitness")
        }
        viewModel.start()
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
