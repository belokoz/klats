import AppKit
import KlatsCore

/// Once per launch Klats asks GitHub for the number of the latest release and, when it is newer,
/// offers to download it. That request is the only one the app makes: no text, no settings, no
/// identifiers go with it. The download itself happens in the browser, and the user installs the
/// new version the same way as the first one. Switched off in the settings.
@MainActor
enum UpdateChecker {
    // belokoz/klats by its numeric id: it follows renames and transfers, and a stranger who later
    // registers the name cannot answer here.
    private static let latestRelease = URL(string: "https://api.github.com/repositories/1375942669/releases/latest")!

    static func checkAtLaunch(settings: SettingsStore) {
        let arguments = CommandLine.arguments
        // `--debug-pretend-version 0.0.1` checks as if that old version were running, to see the offer.
        let pretended = arguments.firstIndex(of: "--debug-pretend-version").flatMap { $0 + 1 < arguments.count ? arguments[$0 + 1] : nil }
        if pretended == nil {
            // The first run already has the latest version, and screenshot runs must not ask GitHub.
            guard settings.checkForUpdates, settings.onboardingCompleted, !arguments.contains("--debug-show") else { return }
        }
        guard let current = AppVersion(pretended ?? Links.version) else { return }
        Task { await check(current: current) }
    }

    private static func check(current: AppVersion) async {
        let configuration = URLSessionConfiguration.ephemeral
        // Klats often starts at login, before the network is up: wait for it instead of failing.
        configuration.waitsForConnectivity = true
        configuration.timeoutIntervalForResource = 120
        configuration.httpAdditionalHeaders = ["Accept": "application/vnd.github+json", "User-Agent": "Klats/\(Links.version)"]
        let session = URLSession(configuration: configuration)
        defer { session.finishTasksAndInvalidate() }

        do {
            let (data, response) = try await session.data(from: latestRelease)
            let status = (response as? HTTPURLResponse)?.statusCode ?? 0
            guard status == 200, let release = LatestRelease(githubJSON: data) else {
                Log.write("update check: no usable answer (HTTP \(status))")
                return
            }
            guard release.version > current else {
                Log.write("update check: \(current) is up to date (latest \(release.version))")
                return
            }
            Log.write("update check: \(release.version) is out, offering it to \(current)")
            await waitForQuietKeyboard()
            // runModal inside this Task would hold up every other MainActor task, the hotkey
            // pipeline included, until the alert closes. Run it from the run loop instead.
            RunLoop.main.perform(inModes: [.default]) {
                MainActor.assumeIsolated { offer(release, current: current) }
            }
        } catch {
            Log.write("update check failed: \(error.localizedDescription)")
        }
    }

    /// The offer must not land in the middle of typing, where the next Return would press «Скачать»
    /// and Klats's own ⌘V could go to the alert. Wait until the keyboard has been still for 3 seconds.
    private static func waitForQuietKeyboard() async {
        func idle(_ type: CGEventType) -> Double {
            CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: type)
        }
        while min(idle(.keyDown), idle(.flagsChanged)) < 3 {
            await Task.sleep(milliseconds: 1000)
        }
    }

    private static func offer(_ release: LatestRelease, current: AppVersion) {
        let alert = NSAlert()
        alert.icon = AppIcon.image(size: 64)
        alert.messageText = String(format: L("Вышел Клац %@"), release.version.description)
        alert.informativeText = String(format: L("У вас версия %@. Скачайте новую, завершите Клац в его меню, откройте скачанный образ и перетащите Клац в «Программы» с заменой. При первом запуске macOS снова попросит подтвердить, как при установке. Настройки и Универсальный доступ сохранятся."), current.description)
        alert.addButton(withTitle: L("Скачать"))
        alert.addButton(withTitle: L("Не сейчас"))
        // AppKit gives Esc only to a button titled Cancel; here Esc means «Не сейчас».
        alert.buttons[1].keyEquivalent = "\u{1b}"
        NSApp.activate(ignoringOtherApps: true)
        if alert.runModal() == .alertFirstButtonReturn {
            Log.write("update offer: download opened")
            NSWorkspace.shared.open(release.downloadURL)
        } else {
            Log.write("update offer: postponed")
        }
    }
}
