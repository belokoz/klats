/// A user-chosen shortcut. Without a key code it is a chord of modifiers only, such as ⌥⌘.
public struct Hotkey: Codable, Hashable, Sendable {
    public var keyCode: UInt16?
    public var modifiers: Modifiers

    public init(keyCode: UInt16? = nil, modifiers: Modifiers) {
        self.keyCode = keyCode
        self.modifiers = modifiers
    }

    public var isChord: Bool { keyCode == nil }
}

extension Modifiers: Codable {
    public init(from decoder: Decoder) throws {
        self.init(rawValue: try decoder.singleValueContainer().decode(UInt8.self))
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }
}
