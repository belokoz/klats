import Foundation
import KlatsCore
import Combine

/// Every user preference, persisted in UserDefaults, observable by the settings window.
@MainActor
final class SettingsStore: ObservableObject {
    static let defaultLayoutHotkey = Hotkey(modifiers: [.option, .command])
    static let defaultCaseHotkey = Hotkey(keyCode: 6, modifiers: [.option, .command])  // 6 is Z

    @Published var layoutHotkey: Hotkey? { didSet { save(layoutHotkey, forKey: Key.layoutHotkey) } }
    @Published var caseHotkey: Hotkey? { didSet { save(caseHotkey, forKey: Key.caseHotkey) } }
    @Published var switchLayoutAfterConversion: Bool { didSet { defaults.set(switchLayoutAfterConversion, forKey: Key.switchLayout) } }
    @Published var caseMode: CaseConverter.Mode { didSet { defaults.set(caseMode.rawValue, forKey: Key.caseMode) } }
    /// Input source ids of the pair to convert between. Empty means «pick automatically».
    @Published var layoutPair: [String] { didSet { defaults.set(layoutPair, forKey: Key.layoutPair) } }
    @Published var onboardingCompleted: Bool { didSet { defaults.set(onboardingCompleted, forKey: Key.onboarding) } }

    /// Runtime only: never remembered across launches.
    @Published var isPaused = false
    /// True while a settings field records a new shortcut, so the global hotkeys stay quiet.
    @Published var isRecordingHotkey = false

    private let defaults: UserDefaults

    private enum Key {
        static let layoutHotkey = "layoutHotkey"
        static let caseHotkey = "caseHotkey"
        static let switchLayout = "switchLayoutAfterConversion"
        static let caseMode = "caseMode"
        static let layoutPair = "layoutPair"
        static let onboarding = "onboardingCompleted"
    }

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        layoutHotkey = Self.load(defaults, Key.layoutHotkey, default: Self.defaultLayoutHotkey)
        caseHotkey = Self.load(defaults, Key.caseHotkey, default: Self.defaultCaseHotkey)
        switchLayoutAfterConversion = defaults.object(forKey: Key.switchLayout) as? Bool ?? true
        caseMode = defaults.string(forKey: Key.caseMode).flatMap(CaseConverter.Mode.init(rawValue:)) ?? .invert
        layoutPair = defaults.stringArray(forKey: Key.layoutPair) ?? []
        onboardingCompleted = defaults.bool(forKey: Key.onboarding)
    }

    // A cleared shortcut is stored as an empty value, so it is not confused with «never set».
    private func save(_ hotkey: Hotkey?, forKey key: String) {
        if let hotkey, let data = try? JSONEncoder().encode(hotkey) {
            defaults.set(data, forKey: key)
        } else {
            defaults.set(Data(), forKey: key)
        }
    }

    private static func load(_ defaults: UserDefaults, _ key: String, default fallback: Hotkey) -> Hotkey? {
        guard let data = defaults.data(forKey: key) else { return fallback }
        if data.isEmpty { return nil }
        return (try? JSONDecoder().decode(Hotkey.self, from: data)) ?? fallback
    }
}
