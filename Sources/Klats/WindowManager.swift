import AppKit
import SwiftUI

/// Owns the settings and first-run windows so SwiftUI content is not deallocated while shown.
/// Klats is a menu bar app, so it only shows a Dock icon while one of its windows is open, and
/// slips back into the background when the last one closes.
@MainActor
final class WindowManager: NSObject, NSWindowDelegate {
    private let store: SettingsStore
    private let appState: AppState
    private var settingsWindow: NSWindow?
    private var onboardingWindow: NSWindow?
    private var aboutWindow: NSWindow?

    init(store: SettingsStore, appState: AppState) {
        self.store = store
        self.appState = appState
        super.init()
    }

    func showSettings() {
        if settingsWindow == nil {
            let view = SettingsView(store: store, appState: appState) { [weak self] in self?.grantPermission() }
            settingsWindow = makeWindow(title: L("Настройки Клац"), content: view)
        }
        present(settingsWindow)
    }

    func showOnboarding() {
        if onboardingWindow == nil {
            let view = OnboardingView(store: store, appState: appState,
                                      onGrant: { [weak self] in self?.grantPermission() },
                                      onFinish: { [weak self] in self?.onboardingWindow?.close() })
            onboardingWindow = makeWindow(title: L("Добро пожаловать в Клац"), content: view)
        }
        present(onboardingWindow)
    }

    func showOnboardingIfNeeded() {
        if !store.onboardingCompleted { showOnboarding() }
    }

    func showAbout() {
        if aboutWindow == nil {
            aboutWindow = makeWindow(title: L("О программе Клац"), content: AboutView(), hideTitle: true)
        }
        present(aboutWindow)
    }

    private func grantPermission() {
        Permissions.requestWithSystemPrompt()
        Permissions.openSystemSettings()
    }

    private func makeWindow(title: String, content: some View, hideTitle: Bool = false) -> NSWindow {
        let window = NSWindow(contentViewController: NSHostingController(rootView: content))
        window.styleMask = [.titled, .closable]
        window.title = title
        if hideTitle {
            window.titleVisibility = .hidden
            window.titlebarAppearsTransparent = true
        }
        window.isReleasedWhenClosed = false
        window.delegate = self
        window.center()
        return window
    }

    private func present(_ window: NSWindow?) {
        guard let window else { return }
        if window.contentView?.fittingSize.height ?? 0 < 100 {
            window.setContentSize(NSSize(width: 480, height: 600))
            window.center()
        }
        activate()
        window.makeKeyAndOrderFront(nil)
        Log.write("window «\(window.title)» shown at \(NSStringFromRect(window.frame)), app active: \(NSApp.isActive)")
    }

    private func activate() {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func windowWillClose(_ notification: Notification) {
        // When the onboarding window closes for the first time, remember it is done.
        if (notification.object as? NSWindow) == onboardingWindow, !store.onboardingCompleted {
            store.onboardingCompleted = true
        }
        // Once every Klats window is gone, drop the Dock icon and go back to the menu bar.
        DispatchQueue.main.async { [weak self] in
            let anyVisible = [self?.settingsWindow, self?.onboardingWindow, self?.aboutWindow].contains { $0?.isVisible == true }
            if !anyVisible { NSApp.setActivationPolicy(.accessory) }
        }
    }
}
