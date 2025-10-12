import SwiftUI

public struct EntranceView: View {
    @EnvironmentObject private var viewModel: SessionViewModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var seedText: String = ""

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
            seedText = viewModel.spec.seed.map(String.init) ?? ""
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
                TextField("Drill", text: Binding(
                    get: { viewModel.spec.drill },
                    set: { viewModel.spec.drill = $0 }
                ))
                TextField("Set", text: Binding(
                    get: { viewModel.spec.setId },
                    set: { viewModel.spec.setId = $0 }
                ))
                Stepper(value: Binding(
                    get: { viewModel.spec.count },
                    set: { viewModel.spec.count = max(1, $0) }
                ), in: 1...50) {
                    HStack {
                        Text("Questions")
                        Spacer()
                        Text("\(viewModel.spec.count)")
                            .foregroundStyle(.secondary)
                    }
                }
#if os(iOS)
                TextField("Seed (optional)", text: $seedText)
                    .keyboardType(.numberPad)
#else
                TextField("Seed (optional)", text: $seedText)
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
        if seedText.isEmpty {
            viewModel.spec.seed = nil
        } else if let value = Int(seedText) {
            viewModel.spec.seed = value
        }
        viewModel.start()
    }
}
