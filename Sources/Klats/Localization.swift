import Foundation

/// Russian is the source language: the keys are the Russian strings themselves, and other
/// languages live in `<lang>.lproj/Localizable.strings` next to the app.
func L(_ russian: String) -> String {
    let text = NSLocalizedString(russian, comment: "")
    // macOS 12 still calls it System Preferences. Russian uses «Системные настройки» for both.
    return Compat.isMacOS12 ? text.replacingOccurrences(of: "System Settings", with: "System Preferences") : text
}
