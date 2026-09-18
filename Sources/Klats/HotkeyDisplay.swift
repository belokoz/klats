import AppKit
import KlatsCore

extension Modifiers {
    /// Modifier glyphs in the order macOS uses: ⌃ ⌥ ⇧ ⌘.
    var symbols: [String] {
        var result: [String] = []
        if contains(.control) { result.append("⌃") }
        if contains(.option) { result.append("⌥") }
        if contains(.shift) { result.append("⇧") }
        if contains(.command) { result.append("⌘") }
        return result
    }

    init(_ flags: NSEvent.ModifierFlags) {
        var modifiers: Modifiers = []
        if flags.contains(.shift) { modifiers.insert(.shift) }
        if flags.contains(.control) { modifiers.insert(.control) }
        if flags.contains(.option) { modifiers.insert(.option) }
        if flags.contains(.command) { modifiers.insert(.command) }
        self = modifiers
    }
}

@MainActor
extension Hotkey {
    /// One label per key, for keycaps: ["⌥", "⌘", "Z"].
    var keycaps: [String] {
        var caps = modifiers.symbols
        if let keyCode { caps.append(KeyNames.name(for: keyCode)) }
        return caps
    }

    /// One string, for menus and messages: "⌥⌘Z".
    var displayString: String { keycaps.joined() }
}

@MainActor
enum KeyNames {
    private static let special: [UInt16: String] = [
        36: "↩", 48: "⇥", 49: "␣", 51: "⌫", 53: "⎋", 117: "⌦",
        115: "↖", 119: "↘", 116: "⇞", 121: "⇟", 123: "←", 124: "→", 125: "↓", 126: "↑",
        122: "F1", 120: "F2", 99: "F3", 118: "F4", 96: "F5", 97: "F6", 98: "F7", 100: "F8",
        101: "F9", 109: "F10", 103: "F11", 111: "F12", 105: "F13", 107: "F14", 113: "F15",
        65: "⌨.", 67: "⌨*", 69: "⌨+", 71: "⌨⌧", 75: "⌨/", 76: "⌤", 78: "⌨-", 81: "⌨=",
        82: "⌨0", 83: "⌨1", 84: "⌨2", 85: "⌨3", 86: "⌨4", 87: "⌨5", 88: "⌨6", 89: "⌨7", 91: "⌨8", 92: "⌨9",
    ]

    /// The label printed on the key: taken from the active Latin layout, so Dvorak users see
    /// their own letters. Falls back to a raw key number for anything unknown.
    static func name(for keyCode: UInt16) -> String {
        if let name = special[keyCode] { return name }
        if let layout = InputSources.currentLatinLayout(), let table = InputSources.table(for: layout),
           let character = table.forward[KeyStroke(keyCode: keyCode, shift: false)] {
            return String(character).uppercased()
        }
        return "#\(keyCode)"
    }
}
