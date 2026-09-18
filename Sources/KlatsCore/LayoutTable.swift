/// A keyboard layout as two lookup tables: key stroke → character and character → key stroke.
/// The app builds these from the real system layouts; tests build them from fixtures.
public struct LayoutTable: Sendable {
    public let id: String
    public let forward: [KeyStroke: Character]
    public let reverse: [Character: KeyStroke]

    public init(id: String, forward: [KeyStroke: Character]) {
        self.id = id
        self.forward = forward

        // A character can sit on several keys. Prefer the unshifted stroke, then the lowest
        // key code, so the reverse mapping is deterministic.
        let ordered = forward.sorted { lhs, rhs in
            if lhs.key.shift != rhs.key.shift { return !lhs.key.shift }
            return lhs.key.keyCode < rhs.key.keyCode
        }
        var reverse: [Character: KeyStroke] = [:]
        for (stroke, character) in ordered where reverse[character] == nil {
            reverse[character] = stroke
        }
        self.reverse = reverse
    }

    public func contains(_ character: Character) -> Bool {
        reverse[character] != nil
    }
}
