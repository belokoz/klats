import CoreGraphics

/// The key presses Klats sends on the user's behalf: ⌘C and ⌘V, nothing else.
@MainActor
enum SyntheticKeys {
    /// Stamped on every event Klats posts, so its own event tap lets them through untouched.
    static let marker: Int64 = 0x4B4C_4154  // "KLAT"

    private static let modifierMask: CGEventFlags = [.maskShift, .maskControl, .maskAlternate, .maskCommand]

    static func copy() { postCommand(InputSources.keyCode(typing: "c") ?? 8) }
    static func paste() { postCommand(InputSources.keyCode(typing: "v") ?? 9) }

    /// True while the user still holds any modifier. Sending ⌘C while ⌥ is physically down can
    /// reach the app as ⌥⌘C, so the pipeline waits for the keys to come up first.
    static var modifiersAreDown: Bool {
        !CGEventSource.flagsState(.hidSystemState).intersection(modifierMask).isEmpty
    }

    private static func postCommand(_ keyCode: UInt16) {
        let source = CGEventSource(stateID: .hidSystemState)
        for keyDown in [true, false] {
            guard let event = CGEvent(keyboardEventSource: source, virtualKey: keyCode, keyDown: keyDown) else { continue }
            event.flags = .maskCommand
            event.setIntegerValueField(.eventSourceUserData, value: marker)
            event.post(tap: .cghidEventTap)
        }
    }
}
