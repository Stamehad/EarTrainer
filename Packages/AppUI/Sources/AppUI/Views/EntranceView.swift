import SwiftUI
import Combine
import Bridge

public struct EntranceView: View {
    private enum SessionMode: String, CaseIterable, Identifiable {
        case adaptive
        case manual
        case levelInspector

        var id: String { rawValue }

        var label: String {
            switch self {
            case .adaptive: return "Adaptive"
            case .manual: return "Manual"
            case .levelInspector: return "Level Inspector"
            }
        }

        var description: String {
            switch self {
            case .adaptive:
                return "Recommended progression"
            case .manual:
                return "Build a custom drill"
            case .levelInspector:
                return "Preview any catalog level"
            }
        }
    }

    private struct KeyOption: Identifiable {
        let id: String
        let label: String
        let value: String

        init(id: String, label: String, value: String) {
            self.id = id
            self.label = label
            self.value = value
        }

        init(label: String, value: String) {
            self.init(id: value, label: label, value: value)
        }
    }

    private static let randomKeyValue = "__random__"
    private let adaptiveTrackLevels: [Int] = [1, 111, 220]
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

    private var allKeyOptions: [KeyOption] {
        [KeyOption(id: Self.randomKeyValue, label: "Random", value: Self.randomKeyValue)] + keyOptions
    }

    @EnvironmentObject private var viewModel: SessionViewModel
    @EnvironmentObject private var userStore: UserStore
    @Environment(\.scenePhase) private var scenePhase

    @State private var sessionMode: SessionMode = .adaptive
    @State private var selectedKeyValue: String = Self.randomKeyValue
    @State private var questionCount: Int = 20
    @State private var inspectorSelectionID: String = ""
    @State private var manualSelectionID: String?
    @State private var manualFieldValues: [String: JSONValue] = [:]
    @State private var pendingManualSelection: String?
    @State private var showSettingsSheet = false
    @State private var showCustomSheet = false
    @State private var hasInitialisedState = false

    public init() {}

    public var body: some View {
        NavigationStack {
            Group {
                switch viewModel.route {
                case .entrance:
                    entranceContent
                case .game:
                    GameView()
                case .results:
                    ResultView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .background(Color.appBackground)
            .navigationTitle("Ear Trainer")
            .toolbar {
#if os(iOS)
                ToolbarItem(placement: .navigationBarLeading) {
                    playerMenu
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: { showSettingsSheet = true }) {
                        Label("Session Settings", systemImage: "gearshape")
                    }
                }
#else
                ToolbarItem(placement: .navigation) {
                    playerMenu
                }
                ToolbarItem(placement: .automatic) {
                    Button(action: { showSettingsSheet = true }) {
                        Label("Session Settings", systemImage: "gearshape")
                    }
                }
#endif
            }
        }
        .onAppear {
            viewModel.bootstrap()
            initialiseStateIfNeeded()
        }
        .onChange(of: scenePhase) { newPhase in
            viewModel.handleScenePhase(newPhase)
        }
        .onChange(of: viewModel.inspectorOptions) { _ in
            ensureInspectorSelection()
        }
        .onReceive(viewModel.$drillCatalog.compactMap { $0 }) { catalog in
            applyPendingManualSelectionIfNeeded(using: catalog)
        }
        .onChange(of: sessionMode) { newMode in
            switch newMode {
            case .adaptive:
                break
            case .levelInspector:
                viewModel.loadInspectorOptionsIfNeeded()
                ensureInspectorSelection()
            case .manual:
                viewModel.loadDrillCatalogIfNeeded()
                if let catalog = viewModel.drillCatalog {
                    prepareManualSelection(using: catalog)
                }
            }
        }
        .onChange(of: manualSelectionID) { newValue in
            guard let key = newValue,
                  let definition = viewModel.drillCatalog?.definition(for: key) else { return }
            prepareManualValues(for: definition)
        }
        .sheet(isPresented: $showSettingsSheet) {
            settingsSheet
        }
        .sheet(isPresented: $showCustomSheet) {
            customSheet
        }
    }

    private var entranceContent: some View {
        VStack(alignment: .leading, spacing: 24) {
            VStack(alignment: .leading, spacing: 8) {
                Text("Ready for your next session?")
                    .font(.title2.weight(.semibold))
                Text(sessionMode.description)
                    .font(.callout)
                    .foregroundStyle(.secondary)
                Text(settingsSummary)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }

            Button(action: startSession) {
                HStack {
                    if viewModel.isProcessing {
                        ProgressView()
                            .progressViewStyle(.circular)
                    }
                    Text(startButtonTitle)
                        .font(.headline)
                    Spacer()
                    Image(systemName: "play.fill")
                        .font(.headline)
                }
                .padding()
                .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(viewModel.isProcessing || !isStartReady)

            if sessionMode == .levelInspector {
                inspectorSummaryView
            } else if sessionMode == .manual {
                manualSummaryView
            }

            Spacer(minLength: 12)

            Button(action: { showCustomSheet = true }) {
                Label(customButtonTitle, systemImage: "slider.horizontal.3")
                    .font(.headline)
                    .frame(maxWidth: .infinity)
                    .padding()
            }
            .buttonStyle(.bordered)
        }
        .padding()
    }

    private var inspectorSummaryView: some View {
        Group {
            if let summary = inspectorSelectionSummary {
                infoTile(title: "Level Inspector", detail: summary)
            } else {
                infoTile(title: "Level Inspector", detail: "Choose a level in Custom Drill.")
            }
        }
    }

    private var manualSummaryView: some View {
        Group {
            if let definition = manualDefinition {
                infoTile(title: "Manual Drill", detail: definition.displayName)
            } else {
                infoTile(title: "Manual Drill", detail: "Select a drill in Custom Drill.")
            }
        }
    }

    @ViewBuilder
    private func infoTile(title: String, detail: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(.subheadline.weight(.semibold))
            Text(detail)
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color.panelBackground)
        )
    }

    private var startButtonTitle: String {
        switch sessionMode {
        case .adaptive:
            return "Start Adaptive Session"
        case .manual:
            return "Start Manual Session"
        case .levelInspector:
            return "Inspect Level"
        }
    }

    private var customButtonTitle: String {
        switch sessionMode {
        case .adaptive:
            return "Custom Drill Options"
        case .manual:
            return "Edit Manual Drill"
        case .levelInspector:
            return "Choose Level"
        }
    }

    private var settingsSummary: String {
        let key = selectedKeyValue == Self.randomKeyValue ? "Random key" : "Key \(selectedKeyValue)"
        let questions = "\(questionCount) question\(questionCount == 1 ? "" : "s")"
        let input = viewModel.answerInputMode.label
        return [key, questions, input].joined(separator: " • ")
    }

    private var inspectorSelectionSummary: String? {
        guard let entry = viewModel.inspectorOptions.first(where: { $0.id == inspectorSelectionID }) else {
            return nil
        }
        return entry.label
    }

    private var manualDefinition: DrillDefinition? {
        guard let key = manualSelectionID else { return nil }
        return viewModel.drillCatalog?.definition(for: key)
    }

    private var isStartReady: Bool {
        switch sessionMode {
        case .adaptive:
            return true
        case .levelInspector:
            return !inspectorSelectionID.isEmpty || viewModel.inspectorOptions.isEmpty
        case .manual:
            return manualDefinition != nil
        }
    }

    private func startSession() {
        guard !viewModel.isProcessing else { return }
        applySettings()
        switch sessionMode {
        case .adaptive:
            applyAdaptiveConfiguration()
        case .levelInspector:
            applyInspectorConfiguration()
        case .manual:
            guard applyManualConfiguration() else { return }
        }
        viewModel.start()
    }

    private func applySettings() {
        if selectedKeyValue == Self.randomKeyValue {
            viewModel.spec.key = randomKey()
        } else {
            viewModel.spec.key = selectedKeyValue
        }
        viewModel.spec.nQuestions = max(1, questionCount)
    }

    private func applyAdaptiveConfiguration() {
        viewModel.spec.adaptive = true
        viewModel.spec.levelInspect = false
        viewModel.spec.trackLevels = adaptiveTrackLevels
        viewModel.spec.inspectLevel = nil
        viewModel.spec.inspectTier = nil
        viewModel.spec.params = nil
    }

    private func applyInspectorConfiguration() {
        viewModel.spec.adaptive = false
        viewModel.spec.levelInspect = true
        viewModel.spec.trackLevels = []
        applyInspectorSelection(for: inspectorSelectionID)
        viewModel.spec.params = nil
    }

    @discardableResult
    private func applyManualConfiguration() -> Bool {
        guard let manualDefinition else { return false }
        viewModel.spec.adaptive = false
        viewModel.spec.levelInspect = false
        viewModel.spec.trackLevels = []
        viewModel.spec.inspectLevel = nil
        viewModel.spec.inspectTier = nil
        viewModel.spec.drillKind = manualDefinition.key

        let filtered = manualDefinition.fields.reduce(into: [String: JSONValue]()) { result, field in
            if let value = manualFieldValues[field.key] ?? field.defaultValue {
                result[field.key] = normalize(value, for: field)
            }
        }
        viewModel.spec.params = filtered.isEmpty ? nil : filtered
        return true
    }

    private func normalize(_ value: JSONValue, for field: DrillDefinition.Field) -> JSONValue {
        switch field.kind {
        case .int:
            if let number = value.intValue {
                return .int(number)
            }
        case .double:
            if let number = value.doubleValue {
                return .double(number)
            }
        case .bool:
            if let flag = value.boolValue {
                return .bool(flag)
            }
        case .string:
            if let text = value.stringValue {
                return .string(text)
            }
        case .option, .select:
            if let text = value.stringValue {
                return .string(text)
            }
        case .intList:
            if let array = value.arrayValue {
                let sanitized = array.compactMap { element -> JSONValue? in
                    guard let number = element.intValue else { return nil }
                    return .int(number)
                }
                return .array(sanitized)
            }
        case .doubleList:
            if let array = value.arrayValue {
                let sanitized = array.compactMap { element -> JSONValue? in
                    guard let number = element.doubleValue else { return nil }
                    return .double(number)
                }
                return .array(sanitized)
            }
        case .stringList:
            if let array = value.arrayValue {
                let sanitized = array.compactMap { element -> JSONValue? in
                    guard let text = element.stringValue else { return nil }
                    return .string(text)
                }
                return .array(sanitized)
            }
        case .unknown:
            break
        }
        return value
    }

    private func initialiseStateIfNeeded() {
        guard !hasInitialisedState else { return }
        hasInitialisedState = true

        questionCount = max(1, viewModel.spec.nQuestions)
        if questionCount == 5 {
            questionCount = 20
        }
        viewModel.spec.nQuestions = questionCount

        if viewModel.spec.levelInspect {
            sessionMode = .levelInspector
            inspectorSelectionID = inspectorIdentifier(level: viewModel.spec.inspectLevel, tier: viewModel.spec.inspectTier)
            viewModel.loadInspectorOptionsIfNeeded()
        } else if viewModel.spec.adaptive {
            sessionMode = .adaptive
        } else {
            sessionMode = .manual
            pendingManualSelection = viewModel.spec.drillKind
            if let params = viewModel.spec.params {
                manualFieldValues = params
            }
            viewModel.loadDrillCatalogIfNeeded()
        }

        selectedKeyValue = Self.randomKeyValue
    }

    private func ensureInspectorSelection() {
        guard sessionMode == .levelInspector else { return }
        if viewModel.inspectorOptions.isEmpty {
            inspectorSelectionID = ""
            return
        }
        if viewModel.inspectorOptions.contains(where: { $0.id == inspectorSelectionID }) {
            return
        }
        if let first = viewModel.inspectorOptions.first {
            inspectorSelectionID = first.id
        }
    }

    private func prepareManualSelection(using catalog: DrillCatalog) {
        if let pending = pendingManualSelection {
            manualSelectionID = pending
            pendingManualSelection = nil
            if let definition = catalog.definition(for: pending) {
                prepareManualValues(for: definition)
            }
            return
        }
        if manualSelectionID == nil,
           let first = catalog.sortedDrills.first {
            manualSelectionID = first.key
            prepareManualValues(for: first)
        }
    }

    private func applyPendingManualSelectionIfNeeded(using catalog: DrillCatalog) {
        guard sessionMode == .manual else { return }
        prepareManualSelection(using: catalog)
    }

    private func prepareManualValues(for definition: DrillDefinition) {
        var updated = manualFieldValues
        let validKeys = Set(definition.fields.map { $0.key })
        for field in definition.fields {
            if updated[field.key] == nil {
                if let defaultValue = field.defaultValue {
                    updated[field.key] = defaultValue
                }
            }
        }
        updated = updated.filter { validKeys.contains($0.key) }
        manualFieldValues = updated
    }

    private func inspectorIdentifier(level: Int?, tier: Int?) -> String {
        guard let level, let tier else { return "" }
        return "\(level)-\(tier)"
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

    private func randomKey() -> String {
        keyOptions.randomElement()?.value ?? "C"
    }

    private var settingsSheet: some View {
        NavigationStack {
            Form {
                Section("Key") {
                    Picker("Key", selection: $selectedKeyValue) {
                        ForEach(allKeyOptions) { option in
                            Text(option.label).tag(option.id)
                        }
                    }
                }

                Section("Questions") {
                    Stepper(value: $questionCount, in: 1...200) {
                        Text("\(questionCount) question\(questionCount == 1 ? "" : "s")")
                    }
                }

                Section("Input Method") {
                    Picker("Answer Input", selection: $viewModel.answerInputMode) {
                        ForEach(SessionViewModel.AnswerInputMode.allCases) { mode in
                            Text(mode.label).tag(mode)
                        }
                    }
                    .pickerStyle(.menu)
                }
            }
            .navigationTitle("Session Settings")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { showSettingsSheet = false }
                }
            }
        }
        .presentationDetents([.medium, .large])
    }

    private var customSheet: some View {
        NavigationStack {
            Form {
                Section("Mode") {
                    Picker("Mode", selection: $sessionMode) {
                        ForEach(SessionMode.allCases) { mode in
                            Text(mode.label).tag(mode)
                        }
                    }
                    .pickerStyle(.segmented)
                }

                switch sessionMode {
                case .adaptive:
                    Section(footer: Text("Adaptive mode adapts to your progress and is recommended for daily practice.")) {
                        EmptyView()
                    }
                case .levelInspector:
                    inspectorConfigurationSection
                case .manual:
                    manualConfigurationSection
                }
            }
            .navigationTitle("Custom Drill")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { showCustomSheet = false }
                }
            }
            .task {
                switch sessionMode {
                case .adaptive:
                    break
                case .levelInspector:
                    viewModel.loadInspectorOptionsIfNeeded()
                case .manual:
                    viewModel.loadDrillCatalogIfNeeded()
                }
            }
        }
        .presentationDetents([.large])
    }

    @ViewBuilder
    private var inspectorConfigurationSection: some View {
        Section("Level & Tier") {
            if viewModel.inspectorOptions.isEmpty {
                ProgressView("Loading levels…")
            } else {
                Picker("Level", selection: $inspectorSelectionID) {
                    ForEach(viewModel.inspectorOptions) { option in
                        Text(option.label).tag(option.id)
                    }
                }
#if os(iOS)
                .pickerStyle(.wheel)
#else
                .pickerStyle(.menu)
#endif
            }
        }
    }

    @ViewBuilder
    private var manualConfigurationSection: some View {
        Section("Drill") {
            if let catalog = viewModel.drillCatalog {
                Picker("Drill Kind", selection: manualSelectionBinding) {
                    ForEach(catalog.sortedDrills) { drill in
                        Text(drill.displayName).tag(drill.key)
                    }
                }
                if let definition = manualDefinition {
                    if let details = definition.details, !details.isEmpty {
                        Text(details)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                }
            } else {
                ProgressView("Loading drills…")
            }
        }

        if let definition = manualDefinition, viewModel.drillCatalog != nil {
            Section("Parameters") {
                ForEach(definition.fields) { field in
                    manualFieldRow(for: field)
                }
            }
        }
    }

    private var manualSelectionBinding: Binding<String> {
        Binding(
            get: {
                manualSelectionID ?? viewModel.drillCatalog?.sortedDrills.first?.key ?? ""
            },
            set: { newValue in
                manualSelectionID = newValue
            }
        )
    }

    @ViewBuilder
    private func manualFieldRow(for field: DrillDefinition.Field) -> some View {
        switch field.kind {
        case .bool:
            Toggle(field.label, isOn: boolBinding(for: field))
        case .int:
#if os(iOS)
            TextField(field.label, text: intStringBinding(for: field))
                .keyboardType(.numbersAndPunctuation)
#else
            TextField(field.label, text: intStringBinding(for: field))
#endif
        case .double:
#if os(iOS)
            TextField(field.label, text: doubleStringBinding(for: field))
                .keyboardType(.numbersAndPunctuation)
#else
            TextField(field.label, text: doubleStringBinding(for: field))
#endif
        case .string:
            TextField(field.label, text: stringBinding(for: field))
        case .intList:
            textFieldWithoutAutocapitalization(label: field.label, binding: intListBinding(for: field))
        case .doubleList:
            textFieldWithoutAutocapitalization(label: field.label, binding: doubleListBinding(for: field))
        case .stringList:
            TextField(field.label, text: stringListBinding(for: field))
        case .option, .select:
            if field.options.isEmpty {
                unsupportedField(field)
            } else {
                Picker(field.label, selection: stringBinding(for: field)) {
                    ForEach(field.options, id: \.self) { option in
                        Text(option).tag(option)
                    }
                }
                .pickerStyle(.menu)
            }
        case .unknown:
            unsupportedField(field)
        }
    }

    @ViewBuilder
    private func textFieldWithoutAutocapitalization(label: String, binding: Binding<String>) -> some View {
#if os(iOS)
        TextField(label, text: binding)
            .textInputAutocapitalization(.never)
#else
        TextField(label, text: binding)
#endif
    }

    private func unsupportedField(_ field: DrillDefinition.Field) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(field.label)
                .font(.subheadline)
            Text("Unsupported field type")
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
    }

    private func boolBinding(for field: DrillDefinition.Field) -> Binding<Bool> {
        Binding(
            get: {
                manualFieldValues[field.key]?.boolValue ?? field.defaultValue?.boolValue ?? false
            },
            set: { newValue in
                manualFieldValues[field.key] = .bool(newValue)
            }
        )
    }

    private func stringBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                manualFieldValues[field.key]?.stringValue ?? field.defaultValue?.stringValue ?? ""
            },
            set: { newValue in
                if newValue.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                } else {
                    manualFieldValues[field.key] = .string(newValue)
                }
            }
        )
    }

    private func intStringBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                if let value = manualFieldValues[field.key]?.intValue {
                    return String(value)
                }
                if let value = field.defaultValue?.intValue {
                    return String(value)
                }
                return ""
            },
            set: { newValue in
                if let value = Int(newValue) {
                    manualFieldValues[field.key] = .int(value)
                } else if newValue.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                }
            }
        )
    }

    private func doubleStringBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                if let value = manualFieldValues[field.key]?.doubleValue {
                    return NumberFormatter.decimalFormatter.string(from: NSNumber(value: value)) ?? ""
                }
                if let value = field.defaultValue?.doubleValue {
                    return NumberFormatter.decimalFormatter.string(from: NSNumber(value: value)) ?? ""
                }
                return ""
            },
            set: { newValue in
                if let value = Double(newValue) {
                    manualFieldValues[field.key] = .double(value)
                } else if newValue.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                }
            }
        )
    }

    private func intListBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                let values = manualFieldValues[field.key]?.arrayValue ?? field.defaultValue?.arrayValue ?? []
                let ints = values.compactMap { $0.intValue }
                return ints.map(String.init).joined(separator: ", ")
            },
            set: { newValue in
                let tokens = newValue.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
                let ints = tokens.compactMap { Int($0) }
                if ints.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                } else {
                    manualFieldValues[field.key] = .array(ints.map(JSONValue.int))
                }
            }
        )
    }

    private func doubleListBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                let values = manualFieldValues[field.key]?.arrayValue ?? field.defaultValue?.arrayValue ?? []
                let doubles = values.compactMap { $0.doubleValue }
                return doubles.map { NumberFormatter.decimalFormatter.string(from: NSNumber(value: $0)) ?? String($0) }.joined(separator: ", ")
            },
            set: { newValue in
                let tokens = newValue.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
                let doubles = tokens.compactMap { Double($0) }
                if doubles.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                } else {
                    manualFieldValues[field.key] = .array(doubles.map(JSONValue.double))
                }
            }
        )
    }

    private func stringListBinding(for field: DrillDefinition.Field) -> Binding<String> {
        Binding(
            get: {
                let values = manualFieldValues[field.key]?.arrayValue ?? field.defaultValue?.arrayValue ?? []
                let strings = values.compactMap { $0.stringValue }
                return strings.joined(separator: ", ")
            },
            set: { newValue in
                let tokens = newValue.split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces) }
                if tokens.isEmpty {
                    manualFieldValues.removeValue(forKey: field.key)
                } else {
                    manualFieldValues[field.key] = .array(tokens.map(JSONValue.string))
                }
            }
        )
    }

    private var playerMenu: some View {
        Menu {
            Button("Switch Player") {
                viewModel.resetToEntrance()
                userStore.signOut()
            }
        } label: {
            Label(userStore.selectedUser?.displayName ?? "Player", systemImage: "person.crop.circle")
        }
    }
}

private extension NumberFormatter {
    static let decimalFormatter: NumberFormatter = {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        formatter.minimumFractionDigits = 0
        formatter.maximumFractionDigits = 3
        return formatter
    }()
}
