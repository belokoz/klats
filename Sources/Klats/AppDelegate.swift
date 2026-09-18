import AppKit
import Combine
import KlatsCore

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private let settings = SettingsStore()
    private let appState = AppState()
    private lazy var replacer = SelectionReplacer(settings: settings)
    private lazy var statusItem = StatusItemController(store: settings, appState: appState)
    private lazy var windows = WindowManager(store: settings, appState: appState)
    private let hotkeys = HotkeyTap()
    private var permissionTimer: Timer?
    private var reportedTapFailure = false
    private var cancellables: Set<AnyCancellable> = []

    @objc func openSettings(_ sender: Any?) { windows.showSettings() }
    @objc func showAbout(_ sender: Any?) { windows.showAbout() }

    func applicationDidFinishLaunching(_ notification: Notification) {
        MainMenu.install()
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "dev"
        let layouts = InputSources.enabledLayouts().map(\.id).joined(separator: ", ")
        Log.write("---- Klats \(version) started from \(Bundle.main.bundlePath) on macOS \(ProcessInfo.processInfo.operatingSystemVersionString); layouts: \(layouts); trusted: \(Permissions.isTrusted); secure input: \(Permissions.secureInputIsOn); login item: \(LoginItem.isEnabled)")
        LoginItem.refreshLocation()

        statusItem.onConvertLayout = { [weak self] in self?.replacer.run(.layout, trigger: "menu") }
        statusItem.onChangeCase = { [weak self] in self?.replacer.run(.letterCase, trigger: "menu") }
        statusItem.onTogglePause = { [weak self] in self?.settings.isPaused.toggle() }
        statusItem.onOpenSettings = { [weak self] in self?.windows.showSettings() }
        statusItem.onShowAbout = { [weak self] in self?.windows.showAbout() }
        statusItem.onOpenLog = { NSWorkspace.shared.open(Log.fileURL) }

        hotkeys.onAction = { [weak self] action, trigger in self?.replacer.run(action, trigger: trigger) }

        // Re-apply the hotkeys whenever the user edits them.
        settings.$layoutHotkey.combineLatest(settings.$caseHotkey)
            .sink { [weak self] _, _ in self?.reconfigureHotkeys() }
            .store(in: &cancellables)
        settings.$isPaused
            .sink { [weak self] paused in self?.hotkeys.isPaused = paused; self?.statusItem.refresh() }
            .store(in: &cancellables)
        settings.$isRecordingHotkey
            .sink { [weak self] recording in self?.hotkeys.isSuspended = recording }
            .store(in: &cancellables)

        reconfigureHotkeys()
        // Screenshot runs (--debug-show) must not stack system prompts on the screen.
        if !Permissions.isTrusted, !CommandLine.arguments.contains("--debug-show") { Permissions.requestWithSystemPrompt() }
        startHotkeysWhenAllowed()
        windows.showOnboardingIfNeeded()

        // `open Клац.app --args --debug-show settings,about` opens windows straight away, for screenshots.
        let arguments = CommandLine.arguments
        if let index = arguments.firstIndex(of: "--debug-show"), index + 1 < arguments.count {
            for screen in arguments[index + 1].split(separator: ",") {
                switch screen {
                case "settings": windows.showSettings()
                case "onboarding": windows.showOnboarding()
                case "menu": statusItem.popUpMenu()
                case "about": windows.showAbout()
                default: break
                }
            }
        }
    }

    private func reconfigureHotkeys() {
        var bindings: [HotkeyTap.Binding] = []
        if let hotkey = settings.layoutHotkey { bindings.append(.init(action: .layout, hotkey: hotkey)) }
        if let hotkey = settings.caseHotkey { bindings.append(.init(action: .letterCase, hotkey: hotkey)) }
        hotkeys.configure(bindings)
    }

    /// The permission can arrive at any moment while the app runs, so keep checking until the
    /// event tap is up. One cheap call a second, and the timer stops for good once it succeeds.
    private func startHotkeysWhenAllowed() {
        if tryStartHotkeys() { return }
        permissionTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in
                guard let self, self.tryStartHotkeys() else { return }
                self.permissionTimer?.invalidate()
                self.permissionTimer = nil
            }
        }
    }

    private func tryStartHotkeys() -> Bool {
        appState.refresh()
        statusItem.refresh()
        guard Permissions.isTrusted else { return false }
        if hotkeys.start() {
            Log.write("event tap is up, hotkeys are live")
            return true
        }
        if !reportedTapFailure {
            reportedTapFailure = true
            Log.write("permission looks granted but the event tap could not be created; toggling the permission off and on usually fixes it")
        }
        return false
    }
}
