/// One physical key press that produces a character: a virtual key code plus the Shift state.
public struct KeyStroke: Hashable, Sendable {
    public let keyCode: UInt16
    public let shift: Bool

    public init(keyCode: UInt16, shift: Bool) {
        self.keyCode = keyCode
        self.shift = shift
    }
}
