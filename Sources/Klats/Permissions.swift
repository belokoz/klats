import AppKit
import ApplicationServices
import Carbon

/// Klats needs exactly one permission, Accessibility: to see the ⌥⌘ chord, to send ⌘C and ⌘V.
@MainActor
enum Permissions {
    static var isTrusted: Bool { AXIsProcessTrusted() }

    /// Shows the standard system prompt that leads to System Settings.
    static func requestWithSystemPrompt() {
        // The literal is the value of kAXTrustedCheckOptionPrompt, which Swift 6 refuses to
        // read directly because the C header declares it as a mutable global.
        let options = ["AXTrustedCheckOptionPrompt": true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(options)
    }

    /// While a password field (or an app that forgot to turn it off) holds secure input,
    /// macOS hides the keyboard from every event tap, so the hotkeys cannot work.
    static var secureInputIsOn: Bool { IsSecureEventInputEnabled() }

    static func openSystemSettings() {
        let address = "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
        if let url = URL(string: address) { NSWorkspace.shared.open(url) }
    }
}
