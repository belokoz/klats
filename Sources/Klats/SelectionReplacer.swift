import AppKit
import KlatsCore

/// The whole job of Klats in one place: take the selected text out of the front app, transform
/// it, put it back, and leave the clipboard as it was.
///
/// It goes through the clipboard with synthetic ⌘C and ⌘V because that works in every app where
/// copy and paste work. The Accessibility API can replace text without the clipboard, but only
/// in some apps, so it is left for later as a fast path.
@MainActor
final class SelectionReplacer {
    private enum Timing {
        static let poll: Duration = .milliseconds(10)
        /// How long to wait for the user to let go of the hotkey before sending ⌘C anyway.
        static let modifierRelease: Duration = .milliseconds(600)
        /// How long the front app gets to answer ⌘C. No change means nothing was selected.
        static let copy: Duration = .milliseconds(500)
        /// How long the front app gets to read the clipboard after ⌘V before it is restored.
        static let restore: Duration = .milliseconds(300)
    }

    /// Marks the converted text as not worth keeping, for clipboard managers that honour the
    /// org.nspasteboard convention.
    private static let transientType = NSPasteboard.PasteboardType("org.nspasteboard.TransientType")

    private unowned let settings: SettingsStore
    private var isBusy = false

    init(settings: SettingsStore) {
        self.settings = settings
    }

    func run(_ action: HotkeyTap.ActionID, trigger: String) {
        guard !isBusy else {
            Log.write("\(action.rawValue) via \(trigger): skipped, previous run still in progress")
            return
        }
        isBusy = true
        Task { @MainActor in
            await perform(action, trigger: trigger)
            isBusy = false
        }
    }

    private func perform(_ action: HotkeyTap.ActionID, trigger: String) async {
        let clock = ContinuousClock()
        let started = clock.now
        let frontApp = NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? "unknown"
        var notes: [String] = []
        var outcome = "unknown"
        defer {
            let total = started.duration(to: clock.now)
            let details = notes.isEmpty ? "" : " (" + notes.joined(separator: ", ") + ")"
            Log.write("\(action.rawValue) via \(trigger) in \(frontApp): \(outcome)\(details) [\(total.milliseconds) ms]")
        }

        guard Permissions.isTrusted else {
            outcome = "no accessibility permission"
            Permissions.requestWithSystemPrompt()
            return
        }

        let releaseWait = await wait(upTo: Timing.modifierRelease) { !SyntheticKeys.modifiersAreDown }
        notes.append("modifiers up after \(releaseWait.elapsed.milliseconds) ms\(releaseWait.satisfied ? "" : ", timed out")")

        let pasteboard = NSPasteboard.general
        let snapshot = PasteboardSnapshot(pasteboard)
        let countBeforeCopy = pasteboard.changeCount
        notes.append("clipboard snapshot \(snapshot.byteCount) bytes")

        SyntheticKeys.copy()
        let copyWait = await wait(upTo: Timing.copy) { pasteboard.changeCount != countBeforeCopy }
        guard copyWait.satisfied else {
            outcome = "nothing selected, or the app ignored ⌘C"
            return
        }
        notes.append("copy answered in \(copyWait.elapsed.milliseconds) ms")

        guard let text = pasteboard.string(forType: .string), !text.isEmpty else {
            snapshot.restore(to: pasteboard)
            outcome = "selection is not text"
            return
        }
        notes.append("\(text.count) characters")

        var targetLayout: KeyboardLayout?
        let converted: String
        switch action {
        case .letterCase:
            converted = CaseConverter.convert(text, mode: settings.caseMode)
        case .layout:
            guard let pair = resolveLayoutPair() else {
                snapshot.restore(to: pasteboard)
                outcome = "need two keyboard layouts"
                return
            }
            guard let tableA = InputSources.table(for: pair.0), let tableB = InputSources.table(for: pair.1) else {
                snapshot.restore(to: pasteboard)
                outcome = "could not read a layout"
                return
            }
            let current = InputSources.currentLayout()?.id
            let result = LayoutConverter.convert(
                text, a: tableA, b: tableB, tieBreak: current == tableB.id ? .bToA : .aToB)
            converted = result.text
            targetLayout = result.direction == .aToB ? pair.1 : pair.0
            notes.append("\(result.direction == .aToB ? pair.0.id : pair.1.id) → \(targetLayout?.id ?? "?")")
        }

        guard converted != text else {
            snapshot.restore(to: pasteboard)
            outcome = "nothing to change"
            return
        }

        pasteboard.clearContents()
        pasteboard.setString(converted, forType: .string)
        pasteboard.setData(Data(), forType: Self.transientType)
        let countAfterWrite = pasteboard.changeCount
        SyntheticKeys.paste()

        if let targetLayout, settings.switchLayoutAfterConversion {
            let switched = InputSources.select(targetLayout)
            notes.append(switched ? "input source switched" : "input source switch failed")
        }

        try? await Task.sleep(for: Timing.restore)
        if pasteboard.changeCount == countAfterWrite {
            snapshot.restore(to: pasteboard)
        } else {
            notes.append("clipboard changed meanwhile, not restored")
        }
        outcome = "replaced"
    }

    /// The chosen pair, or the first two enabled layouts when the user left it on «automatic».
    private func resolveLayoutPair() -> (KeyboardLayout, KeyboardLayout)? {
        let layouts = InputSources.enabledLayouts()
        if settings.layoutPair.count == 2 {
            let chosen = settings.layoutPair.compactMap { id in layouts.first { $0.id == id } }
            if chosen.count == 2 { return (chosen[0], chosen[1]) }
        }
        guard layouts.count >= 2 else { return nil }
        return (layouts[0], layouts[1])
    }

    /// Polls `condition` on the main actor, yielding between checks so the event tap and the rest
    /// of the app keep running.
    private func wait(upTo limit: Duration, until condition: () -> Bool) async -> (satisfied: Bool, elapsed: Duration) {
        let clock = ContinuousClock()
        let started = clock.now
        while true {
            if condition() { return (true, started.duration(to: clock.now)) }
            if started.duration(to: clock.now) >= limit { return (false, started.duration(to: clock.now)) }
            try? await Task.sleep(for: Timing.poll)
        }
    }
}

private extension Duration {
    var milliseconds: Int {
        let (seconds, attoseconds) = components
        return Int(seconds) * 1000 + Int(attoseconds / 1_000_000_000_000_000)
    }
}
