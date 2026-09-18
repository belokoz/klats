import CoreGraphics
import KlatsCore

/// One interceptor for every hotkey.
///
/// A chord such as ⌥⌘ is made of modifiers only, which the system cannot register as a hotkey,
/// so Klats has to watch modifier state itself. Registering ⌥⌘Z through the old Carbon API next
/// to it would not work: the system swallows a Carbon hotkey before anyone else sees the key,
/// the chord detector would never learn that Z was pressed, and letting go of ⌥⌘ after ⌥⌘Z
/// would fire the layout conversion too. With a single event tap every key arrives in order.
@MainActor
final class HotkeyTap {
    enum ActionID: String {
        case layout
        case letterCase = "case"
    }

    struct Binding {
        let action: ActionID
        let hotkey: Hotkey
    }

    var onAction: ((ActionID, _ trigger: String) -> Void)?

    /// Paused by the user: everything passes through, nothing fires.
    var isPaused = false
    /// A settings field is recording a shortcut: same as paused, but temporary.
    var isSuspended = false

    private var combos: [Binding] = []
    private var chords: [(action: ActionID, hotkey: Hotkey, detector: ChordDetector)] = []
    private var tap: CFMachPort?
    private var runLoopSource: CFRunLoopSource?
    private var swallowedKeys: Set<UInt16> = []

    var isRunning: Bool { tap != nil }

    func configure(_ bindings: [Binding]) {
        combos = bindings.filter { !$0.hotkey.isChord }
        chords = bindings.filter(\.hotkey.isChord).map { ($0.action, $0.hotkey, ChordDetector(chord: $0.hotkey.modifiers)) }
    }

    /// Fails without the Accessibility permission.
    func start() -> Bool {
        guard tap == nil else { return true }
        let types: [CGEventType] = [
            .keyDown, .keyUp, .flagsChanged, .scrollWheel,
            .leftMouseDown, .rightMouseDown, .otherMouseDown, .leftMouseUp, .rightMouseUp, .otherMouseUp,
        ]
        let mask = types.reduce(CGEventMask(0)) { $0 | (CGEventMask(1) << CGEventMask($1.rawValue)) }
        guard let tap = CGEvent.tapCreate(
            tap: .cgSessionEventTap, place: .headInsertEventTap, options: .defaultTap,
            eventsOfInterest: mask, callback: hotkeyTapCallback,
            userInfo: Unmanaged.passUnretained(self).toOpaque())
        else { return false }

        let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0)
        CFRunLoopAddSource(CFRunLoopGetMain(), source, .commonModes)
        CGEvent.tapEnable(tap: tap, enable: true)
        self.tap = tap
        runLoopSource = source
        return true
    }

    private var mayFire: Bool { !isPaused && !isSuspended }

    private static var mouseButtonIsDown: Bool {
        [CGMouseButton.left, .right, .center].contains { CGEventSource.buttonState(.hidSystemState, button: $0) }
    }

    /// Returns true when the event must not reach the front app.
    fileprivate func handle(_ type: CGEventType, _ event: CGEvent) -> Bool {
        if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
            // The system switches a tap off if it ever answers too slowly. Switch it back on.
            if let tap { CGEvent.tapEnable(tap: tap, enable: true) }
            Log.write("event tap was disabled by the system, re-enabled")
            return false
        }
        if event.getIntegerValueField(.eventSourceUserData) == SyntheticKeys.marker {
            return false
        }

        switch type {
        case .flagsChanged:
            let modifiers = Modifiers(event.flags)
            let time = Double(event.timestamp) / 1_000_000_000
            for index in chords.indices where chords[index].detector.flagsChanged(modifiers, at: time) {
                guard mayFire else { continue }
                // ⌥⌘ pressed in the middle of a drag (Finder makes an alias that way) is not ours.
                if Self.mouseButtonIsDown {
                    Log.write("\(chords[index].hotkey.displayString) ignored: a mouse button is down")
                } else {
                    onAction?(chords[index].action, chords[index].hotkey.displayString)
                }
            }
            return false

        case .keyDown:
            for index in chords.indices { chords[index].detector.otherInput() }
            let keyCode = UInt16(event.getIntegerValueField(.keyboardEventKeycode))
            let modifiers = Modifiers(event.flags)
            guard mayFire, let binding = combos.first(where: { $0.hotkey.keyCode == keyCode && $0.hotkey.modifiers == modifiers }) else {
                return false
            }
            swallowedKeys.insert(keyCode)
            if event.getIntegerValueField(.keyboardEventAutorepeat) == 0 {
                onAction?(binding.action, binding.hotkey.displayString)
            }
            return true

        case .keyUp:
            let keyCode = UInt16(event.getIntegerValueField(.keyboardEventKeycode))
            return swallowedKeys.remove(keyCode) != nil

        default:
            for index in chords.indices { chords[index].detector.otherInput() }
            return false
        }
    }
}

private func hotkeyTapCallback(
    proxy: CGEventTapProxy, type: CGEventType, event: CGEvent, userInfo: UnsafeMutableRawPointer?
) -> Unmanaged<CGEvent>? {
    guard let userInfo else { return Unmanaged.passUnretained(event) }
    // The tap's run loop source lives on the main run loop, so this always runs on the main
    // thread. The compiler cannot know that about a C callback, hence the explicit promise.
    nonisolated(unsafe) let context = userInfo
    nonisolated(unsafe) let received = event
    let swallow = MainActor.assumeIsolated {
        Unmanaged<HotkeyTap>.fromOpaque(context).takeUnretainedValue().handle(type, received)
    }
    return swallow ? nil : Unmanaged.passUnretained(event)
}

extension Modifiers {
    init(_ flags: CGEventFlags) {
        var modifiers: Modifiers = []
        if flags.contains(.maskShift) { modifiers.insert(.shift) }
        if flags.contains(.maskControl) { modifiers.insert(.control) }
        if flags.contains(.maskAlternate) { modifiers.insert(.option) }
        if flags.contains(.maskCommand) { modifiers.insert(.command) }
        self = modifiers
    }
}
