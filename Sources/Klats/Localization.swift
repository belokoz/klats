import Foundation

/// Russian is the source language: the keys are the Russian strings themselves, and other
/// languages live in `<lang>.lproj/Localizable.strings` next to the app.
func L(_ russian: String) -> String {
    NSLocalizedString(russian, comment: "")
}
