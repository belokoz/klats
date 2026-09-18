/// The four modifier keys Klats cares about. Caps Lock, Fn and the numeric-pad flag are ignored.
public struct Modifiers: OptionSet, Hashable, Sendable {
    public let rawValue: UInt8
    public init(rawValue: UInt8) { self.rawValue = rawValue }

    public static let shift = Modifiers(rawValue: 1 << 0)
    public static let control = Modifiers(rawValue: 1 << 1)
    public static let option = Modifiers(rawValue: 1 << 2)
    public static let command = Modifiers(rawValue: 1 << 3)
}

/// Detects a hotkey made of modifier keys only, such as ⌥⌘.
///
/// The system cannot register such a hotkey, so Klats watches modifier state itself. The chord
/// fires on release, and only if nothing else happened while it was held. That keeps ⌥⌘Esc,
/// ⌥⌘D, ⌥⌘-drag and every other real shortcut working exactly as before.
public struct ChordDetector: Sendable {
    public let chord: Modifiers
    /// Holding the chord longer than this means the user was doing something else.
    public let maxHold: Double

    private enum State: Sendable {
        case idle
        case armed(since: Double)
        /// Something disqualified this press. Wait until every modifier is released.
        case blocked
    }

    private var state: State = .idle
    private var last: Modifiers = []

    public init(chord: Modifiers, maxHold: Double = 1.0) {
        self.chord = chord
        self.maxHold = maxHold
    }

    /// Feed every modifier change. Returns true when the chord fires.
    public mutating func flagsChanged(_ now: Modifiers, at time: Double) -> Bool {
        defer { last = now }
        switch state {
        case .idle:
            if now == chord && last.isStrictSubset(of: chord) {
                state = .armed(since: time)
            } else if !now.isSubset(of: chord) {
                state = .blocked
            }
            return false

        case .armed(let since):
            if now == chord { return false }
            if now.isStrictSubset(of: chord) {
                // Released. Require a full release before arming again, so letting go of the
                // second key never fires a second time.
                state = now.isEmpty ? .idle : .blocked
                return time - since <= maxHold
            }
            state = .blocked
            return false

        case .blocked:
            if now.isEmpty { state = .idle }
            return false
        }
    }

    /// Feed every key press, mouse press and scroll. Any of them while a modifier is down means
    /// the user is in the middle of a real shortcut, so this press can no longer be the chord.
    public mutating func otherInput() {
        if !last.isEmpty { state = .blocked }
    }
}
