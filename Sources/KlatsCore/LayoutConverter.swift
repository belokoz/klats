/// Re-types text as if the same keys had been pressed in the other layout.
public enum LayoutConverter {
    public enum Direction: Sendable, Equatable {
        case aToB
        case bToA
    }

    public struct Result: Sendable, Equatable {
        public let text: String
        public let direction: Direction
    }

    /// The source layout is the one that owns more of the text's characters. Characters present
    /// in both layouts (digits, most punctuation) do not vote. A tie falls back to `tieBreak`,
    /// which the app derives from the currently active layout.
    public static func detectDirection(
        _ text: String, a: LayoutTable, b: LayoutTable, tieBreak: Direction = .aToB
    ) -> Direction {
        var onlyA = 0
        var onlyB = 0
        for character in text {
            let inA = a.contains(character)
            let inB = b.contains(character)
            if inA && !inB { onlyA += 1 }
            if inB && !inA { onlyB += 1 }
        }
        if onlyA == onlyB { return tieBreak }
        return onlyA > onlyB ? .aToB : .bToA
    }

    /// One direction per call: every character found in the source layout is replaced by the
    /// character on the same key in the target layout. Everything else passes through untouched.
    public static func convert(
        _ text: String, a: LayoutTable, b: LayoutTable, tieBreak: Direction = .aToB
    ) -> Result {
        let direction = detectDirection(text, a: a, b: b, tieBreak: tieBreak)
        let (source, target) = direction == .aToB ? (a, b) : (b, a)
        let converted = text.map { character -> Character in
            guard let stroke = source.reverse[character], let mapped = target.forward[stroke] else {
                return character
            }
            return mapped
        }
        return Result(text: String(converted), direction: direction)
    }
}
