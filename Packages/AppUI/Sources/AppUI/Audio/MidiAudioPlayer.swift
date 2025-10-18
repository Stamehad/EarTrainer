import Foundation
import AVFoundation
import AudioToolbox
import Bridge

enum MidiAudioError: Error {
    case sequenceInit
    case sequenceSerialization
    case soundFontMissing
    case osStatus(OSStatus)
}

final class MidiAudioPlayer {
    static let shared = MidiAudioPlayer()

    private let queue = DispatchQueue(label: "com.eartrainer.midi-player")
    private var soundFontURL: URL?
    private var currentPlayer: AVMIDIPlayer?

    private init() {}

#if os(iOS)
    private func setupAudioSession() {
        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playback, mode: .default, options: [])
            try session.setActive(true, options: [])
            #if DEBUG
            print("[MidiAudioPlayer] AVAudioSession configured: category=playback")
            #endif
        } catch {
            #if DEBUG
            print("[MidiAudioPlayer] Failed to configure AVAudioSession:", error)
            #endif
        }
    }
#endif

    func configureIfAvailable(searchPaths: [URL]) {
        guard soundFontURL == nil else { return }

        var candidates = searchPaths
        let fileManager = FileManager.default
        var inspected = Set<ObjectIdentifier>()
        var discoveredFonts: [URL] = []
        var seenFonts = Set<URL>()

        func appendResources(from bundle: Bundle) {
            let identifier = ObjectIdentifier(bundle)
            guard !inspected.contains(identifier) else { return }
            inspected.insert(identifier)

            if let res = bundle.resourceURL {
                candidates.append(res)
                let core = res.appendingPathComponent("CoreResources", isDirectory: true)
                if fileManager.fileExists(atPath: core.path) {
                    candidates.append(core)
                }
                let soundfonts = core.appendingPathComponent("Soundfonts", isDirectory: true)
                if fileManager.fileExists(atPath: soundfonts.path) {
                    candidates.append(soundfonts)
                }
            }

            if let core = bundle.url(forResource: "CoreResources", withExtension: nil) {
                candidates.append(core)
                let soundfonts = core.appendingPathComponent("Soundfonts", isDirectory: true)
                if fileManager.fileExists(atPath: soundfonts.path) {
                    candidates.append(soundfonts)
                }
            }
            if let coreSoundfonts = bundle.url(forResource: "CoreResources/Soundfonts", withExtension: nil) {
                candidates.append(coreSoundfonts)
            }
        }

        let primaryBundles: [Bundle] = {
            var bundles: [Bundle] = [
                Bundle.main,
                Bundle(for: MidiAudioPlayer.self)
            ]
#if SWIFT_PACKAGE
            bundles.append(Bundle.module)
#endif
            bundles.append(contentsOf: Bundle.allBundles)
            bundles.append(contentsOf: Bundle.allFrameworks)
            if let embedded = Bundle.main.urls(forResourcesWithExtension: "bundle", subdirectory: nil) {
                for url in embedded {
                    if let bundle = Bundle(url: url) {
                        bundles.append(bundle)
                    }
                }
            }
            if let frameworksURL = Bundle.main.privateFrameworksURL,
               let contents = try? fileManager.contentsOfDirectory(at: frameworksURL, includingPropertiesForKeys: nil) {
                for url in contents where url.pathExtension == "bundle" {
                    if let bundle = Bundle(url: url) {
                        bundles.append(bundle)
                    }
                }
            }
            return bundles
        }()

        primaryBundles.forEach { appendResources(from: $0) }

        #if DEBUG
        print("[MidiAudioPlayer] Searching for soundfonts in:")
        candidates.forEach { print("  - \($0.path)") }
        #endif

        for baseURL in candidates {
            let fonts = findSoundFonts(in: baseURL)
            for font in fonts where !seenFonts.contains(font) {
                seenFonts.insert(font)
                discoveredFonts.append(font)
            }
        }
        if soundFontURL == nil {
            soundFontURL = selectPreferredSoundFont(from: discoveredFonts)
        }
        if soundFontURL == nil {
            // Generic: first sf2 at bundle root
            if let bundleURL = Bundle.main.url(forResource: nil, withExtension: "sf2") {
                soundFontURL = bundleURL
            }
        }
        if soundFontURL == nil {
            // Explicit fallbacks by name
            if let gp = Bundle.main.url(forResource: "GrandPiano", withExtension: "sf2") {
                soundFontURL = gp
            } else if let df = Bundle.main.url(forResource: "Default", withExtension: "sf2") {
                soundFontURL = df
            }
        }

        #if DEBUG
        if discoveredFonts.isEmpty {
            print("[MidiAudioPlayer] No custom soundfonts discovered.")
        } else {
            print("[MidiAudioPlayer] Discovered soundfonts:")
            discoveredFonts.forEach { print("  • \($0.lastPathComponent) @ \($0.path)") }
        }
        if let sf = soundFontURL {
            print("[MidiAudioPlayer] Using soundfont:", sf.path)
        } else {
            print("[MidiAudioPlayer] No soundfont found; AVMIDIPlayer may be silent on iOS.")
            if let resPath = Bundle.main.resourcePath {
                let listing = (try? FileManager.default.contentsOfDirectory(atPath: resPath)) ?? []
                print("[MidiAudioPlayer] Bundle resources:", listing)
            }
        }
        #endif

        #if os(iOS)
        setupAudioSession()
        #endif
    }

    func play(clip: MidiClip?) {
        guard let clip = clip, let soundFontURL else {
            return
        }
        queue.async { [weak self] in
            guard let self else { return }
#if DEBUG
            print("[MidiAudioPlayer] Preparing AVMIDIPlayer with soundfont:", soundFontURL.lastPathComponent)
            do {
                let attributes = try FileManager.default.attributesOfItem(atPath: soundFontURL.path)
                if let size = attributes[.size] as? NSNumber {
                    print("[MidiAudioPlayer] Soundfont size: \(size.intValue) bytes")
                }
            } catch {
                print("[MidiAudioPlayer] Unable to read soundfont attributes:", error)
            }
            for track in clip.tracks {
                print("[MidiAudioPlayer] Track '\(track.name)' channel=\(track.channel) program=\(track.program ?? -1) events=\(track.events.count)")
            }
            print("[MidiAudioPlayer] Play request: tempo=\(clip.tempoBpm), ppq=\(clip.ppq), tracks=\(clip.tracks.count)")
#endif
#if os(iOS)
            self.setupAudioSession()
#endif
            do {
                let midiData = try MidiSequenceBuilder.midiData(from: clip)
                let player = try AVMIDIPlayer(data: midiData, soundBankURL: soundFontURL)
                self.currentPlayer?.stop()
                self.currentPlayer = player
                player.prepareToPlay()
                player.play()
            } catch {
                #if DEBUG
                print("[MidiAudioPlayer] Failed to play clip:", error)
                #endif
            }
        }
    }

    private func findSoundFonts(in baseURL: URL) -> [URL] {
        guard let children = try? FileManager.default.contentsOfDirectory(at: baseURL,
                                                                          includingPropertiesForKeys: nil,
                                                                          options: [.skipsHiddenFiles]) else {
            return []
        }
        let soundFonts = children.filter { url in
            let ext = url.pathExtension.lowercased()
            return ext == "sf2" || ext == "dls"
        }
        return soundFonts.sorted { $0.lastPathComponent.localizedCaseInsensitiveCompare($1.lastPathComponent) == .orderedAscending }
    }

    private func selectPreferredSoundFont(from fonts: [URL]) -> URL? {
        guard !fonts.isEmpty else { return nil }
        let preferredNames = [
            "fluidr3_gm",
            "fluidr3",
            "ai_grand_piano",
            "ai-grand-piano",
            "grandpiano",
            "grand_piano",
            "grand-piano",
            "acousticgrand",
            "acousticgrandpiano",
            "default",
            "gm"
        ]
        for name in preferredNames {
            if let match = fonts.first(where: {
                $0.deletingPathExtension().lastPathComponent.lowercased() == name
            }) {
                return match
            }
        }
        return fonts.first
    }
}

private enum MidiSequenceBuilder {
    static func midiData(from clip: MidiClip) throws -> Data {
        var sequence: MusicSequence?
        try checkStatus(NewMusicSequence(&sequence))
        guard let sequence else {
            throw MidiAudioError.sequenceInit
        }
        defer { DisposeMusicSequence(sequence) }

        try buildSequence(sequence, from: clip)

        var data: Unmanaged<CFData>?
        try checkStatus(MusicSequenceFileCreateData(sequence, .midiType, .eraseFile, Int16(clip.ppq), &data))
        guard let cfData = data?.takeRetainedValue() else {
            throw MidiAudioError.sequenceSerialization
        }
        return cfData as Data
    }

    private static func buildSequence(_ sequence: MusicSequence, from clip: MidiClip) throws {
        var tempoTrack: MusicTrack?
        try checkStatus(MusicSequenceGetTempoTrack(sequence, &tempoTrack))
        if let tempoTrack {
            try checkStatus(MusicTrackNewExtendedTempoEvent(tempoTrack, 0, Double(max(clip.tempoBpm, 1))))
        }

        for track in clip.tracks {
            var musicTrack: MusicTrack?
            try checkStatus(MusicSequenceNewTrack(sequence, &musicTrack))
            guard let musicTrack else { continue }

            if let program = track.program {
                var message = MIDIChannelMessage(status: 0xC0 | UInt8(track.channel & 0x0F),
                                                 data1: UInt8(program & 0x7F),
                                                 data2: 0,
                                                 reserved: 0)
                try checkStatus(MusicTrackNewMIDIChannelEvent(musicTrack, 0, &message))
            }

            var active = [NoteKey: NoteState]()
            for event in track.events.sorted(by: { $0.t < $1.t }) {
                let timestamp = MusicTimeStamp(Double(event.t) / Double(max(1, clip.ppq)))
                let channel = UInt8(track.channel & 0x0F)
                switch event.type.lowercased() {
                case "note_on":
                    guard let noteValue = event.note else { continue }
                    let velocity = UInt8(event.vel ?? 64)
                    let key = NoteKey(note: noteValue, channel: channel)
                    if velocity == 0 {
                        if let state = active.removeValue(forKey: key) {
                            try addNoteEvent(track: musicTrack,
                                             start: state.start,
                                             end: timestamp,
                                             note: UInt8(noteValue & 0x7F),
                                             velocity: state.velocity,
                                             channel: channel)
                        }
                    } else {
                        active[key] = NoteState(start: timestamp, velocity: velocity)
                    }
                case "note_off":
                    guard let noteValue = event.note else { continue }
                    let key = NoteKey(note: noteValue, channel: channel)
                    if let state = active.removeValue(forKey: key) {
                        try addNoteEvent(track: musicTrack,
                                         start: state.start,
                                         end: timestamp,
                                         note: UInt8(noteValue & 0x7F),
                                         velocity: state.velocity,
                                         channel: channel)
                    }
                case "control_change":
                    guard let control = event.control, let value = event.value else { continue }
                    var message = MIDIChannelMessage(status: 0xB0 | channel,
                                                     data1: UInt8(control & 0x7F),
                                                     data2: UInt8(value & 0x7F),
                                                     reserved: 0)
                    try checkStatus(MusicTrackNewMIDIChannelEvent(musicTrack, timestamp, &message))
                default:
                    continue
                }
            }

            let fallbackDuration = 1.0 / Double(max(1, clip.ppq))
            for (key, state) in active {
                try addNoteEvent(track: musicTrack,
                                 start: state.start,
                                 end: state.start + fallbackDuration,
                                 note: UInt8(key.note & 0x7F),
                                 velocity: state.velocity,
                                 channel: key.channel)
            }
        }
    }

    private static func addNoteEvent(track: MusicTrack,
                                     start: MusicTimeStamp,
                                     end: MusicTimeStamp,
                                     note: UInt8,
                                     velocity: UInt8,
                                     channel: UInt8) throws {
        let duration = max(Float32(end - start), 1.0 / 480.0)
        var message = MIDINoteMessage(channel: channel,
                                      note: note,
                                      velocity: velocity,
                                      releaseVelocity: 0,
                                      duration: duration)
        try checkStatus(MusicTrackNewMIDINoteEvent(track, start, &message))
    }

    private static func checkStatus(_ status: OSStatus) throws {
        guard status == noErr else {
            throw MidiAudioError.osStatus(status)
        }
    }

    private struct NoteKey: Hashable {
        let note: Int
        let channel: UInt8
    }

    private struct NoteState {
        let start: MusicTimeStamp
        let velocity: UInt8
    }
}
