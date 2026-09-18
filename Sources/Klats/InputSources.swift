import Carbon
import KlatsCore

struct KeyboardLayout {
    let source: TISInputSource
    let id: String
    let name: String
}

/// Thin wrapper over Text Input Sources: which layouts are on, which is active, what each key
/// types in each of them. Tables are rebuilt on every use: it takes a fraction of a millisecond,
/// so there is no cache to invalidate when the user changes layouts.
@MainActor
enum InputSources {
    /// Printable keys of the main block. 36 is Return, 48 Tab, 49 Space; the keypad is left out
    /// so that «1» always means the key in the number row.
    private static let keyCodes: [UInt16] = (0...50).filter { ![36, 48, 49].contains($0) }

    static func enabledLayouts() -> [KeyboardLayout] {
        let filter: [CFString: Any] = [
            kTISPropertyInputSourceCategory: kTISCategoryKeyboardInputSource as Any,
            kTISPropertyInputSourceType: kTISTypeKeyboardLayout as Any,
            kTISPropertyInputSourceIsEnabled: true,
            kTISPropertyInputSourceIsSelectCapable: true,
        ]
        guard let list = TISCreateInputSourceList(filter as CFDictionary, false)?.takeRetainedValue() as? [TISInputSource] else {
            return []
        }
        return list.compactMap(layout(from:))
    }

    static func currentLayout() -> KeyboardLayout? {
        guard let source = TISCopyCurrentKeyboardLayoutInputSource()?.takeRetainedValue() else { return nil }
        return layout(from: source)
    }

    /// The Latin layout macOS pairs with the current one: ABC while Russian is active, Dvorak for
    /// Dvorak users. This is the layout that names keys in shortcuts.
    static func currentLatinLayout() -> KeyboardLayout? {
        guard let source = TISCopyCurrentASCIICapableKeyboardLayoutInputSource()?.takeRetainedValue() else { return nil }
        return layout(from: source)
    }

    @discardableResult
    static func select(_ layout: KeyboardLayout) -> Bool {
        TISSelectInputSource(layout.source) == noErr
    }

    static func table(for layout: KeyboardLayout) -> LayoutTable? {
        guard let raw = TISGetInputSourceProperty(layout.source, kTISPropertyUnicodeKeyLayoutData) else { return nil }
        let data = Unmanaged<CFData>.fromOpaque(raw).takeUnretainedValue() as Data
        let keyboardType = UInt32(LMGetKbdType())
        var forward: [KeyStroke: Character] = [:]

        data.withUnsafeBytes { buffer in
            guard let keyLayout = buffer.bindMemory(to: UCKeyboardLayout.self).baseAddress else { return }
            for keyCode in keyCodes {
                for shift in [false, true] {
                    var deadKeyState: UInt32 = 0
                    var length = 0
                    var characters = [UniChar](repeating: 0, count: 4)
                    let modifiers: UInt32 = shift ? UInt32((shiftKey >> 8) & 0xFF) : 0
                    let status = UCKeyTranslate(
                        keyLayout, keyCode, UInt16(kUCKeyActionDown), modifiers, keyboardType,
                        OptionBits(kUCKeyTranslateNoDeadKeysMask), &deadKeyState,
                        characters.count, &length, &characters)
                    guard status == noErr, length > 0 else { continue }
                    let string = String(utf16CodeUnits: characters, count: length)
                    guard string.count == 1, let character = string.first, !character.isWhitespace else { continue }
                    forward[KeyStroke(keyCode: keyCode, shift: shift)] = character
                }
            }
        }
        return forward.isEmpty ? nil : LayoutTable(id: layout.id, forward: forward)
    }

    /// The key that types `character` without Shift in the active layout, if it has one.
    /// ⌘C must go to the key that means «c» for the user: in Dvorak that is not key 8.
    static func keyCode(typing character: Character) -> UInt16? {
        guard let layout = currentLatinLayout(), let stroke = table(for: layout)?.reverse[character], !stroke.shift else {
            return nil
        }
        return stroke.keyCode
    }

    private static func layout(from source: TISInputSource) -> KeyboardLayout? {
        guard TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) != nil,
              let id = string(source, kTISPropertyInputSourceID) else { return nil }
        return KeyboardLayout(source: source, id: id, name: string(source, kTISPropertyLocalizedName) ?? id)
    }

    private static func string(_ source: TISInputSource, _ key: CFString) -> String? {
        guard let raw = TISGetInputSourceProperty(source, key) else { return nil }
        return Unmanaged<CFString>.fromOpaque(raw).takeUnretainedValue() as String
    }
}
