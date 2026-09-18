import SwiftUI
import KlatsCore

/// A field that records a shortcut, including one made of modifiers only, such as ⌥⌘.
/// Click to record, press the combination, Esc cancels, ⌫ clears.
struct HotkeyRecorder: View {
    @Binding var hotkey: Hotkey?
    /// What the × button restores.
    let defaultValue: Hotkey
    @ObservedObject var store: SettingsStore
    /// Returns an error message for a rejected combination, or nil when it is fine.
    var validate: (Hotkey) -> String?
    var onError: (String?) -> Void = { _ in }

    @State private var isRecording = false
    @State private var monitor: Any?
    @State private var heldModifiers: Modifiers = []
    @State private var keyWasPressed = false

    var body: some View {
        HStack(spacing: 6) {
            Button(action: toggle) {
                content
                    .frame(minWidth: 130, minHeight: 22)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 5)
                    .background(
                        RoundedRectangle(cornerRadius: 7)
                            .stroke(isRecording ? Color.accentColor : Color.secondary.opacity(0.4),
                                    lineWidth: isRecording ? 2 : 1))
            }
            .buttonStyle(.plain)

            if !isRecording, hotkey != defaultValue {
                Button {
                    hotkey = defaultValue
                    onError(nil)
                } label: {
                    Image(systemName: "arrow.uturn.backward.circle.fill").foregroundStyle(.secondary)
                }
                .buttonStyle(.plain)
                .help(L("Вернуть сочетание по умолчанию"))
            }
        }
        .onDisappear(perform: stop)
    }

    @ViewBuilder private var content: some View {
        if isRecording {
            if heldModifiers.symbols.isEmpty {
                Text(L("Введите сочетание…")).foregroundStyle(.secondary)
            } else {
                keycaps(heldModifiers.symbols)
            }
        } else if let hotkey {
            keycaps(hotkey.keycaps)
        } else {
            Text(L("Не задано")).foregroundStyle(.secondary)
        }
    }

    private func keycaps(_ labels: [String]) -> some View {
        HStack(spacing: 3) {
            ForEach(Array(labels.enumerated()), id: \.offset) { _, label in
                Text(label)
                    .font(.system(size: 13, design: .monospaced))
                    .padding(.horizontal, 5).padding(.vertical, 1)
                    .background(RoundedRectangle(cornerRadius: 5).fill(Color.secondary.opacity(0.15)))
            }
        }
    }

    private func toggle() { isRecording ? stop() : start() }

    private func start() {
        isRecording = true
        heldModifiers = []
        keyWasPressed = false
        store.isRecordingHotkey = true
        onError(nil)
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .flagsChanged]) { event in
            handle(event)
            return nil  // swallow, so the shortcut does not reach the rest of the app
        }
    }

    private func stop() {
        if let monitor { NSEvent.removeMonitor(monitor) }
        monitor = nil
        isRecording = false
        heldModifiers = []
        store.isRecordingHotkey = false
    }

    private func handle(_ event: NSEvent) {
        switch event.type {
        case .keyDown:
            keyWasPressed = true
            switch event.keyCode {
            case 53: stop()                              // Esc cancels
            case 51, 117: hotkey = nil; onError(nil); stop()  // Delete clears
            default:
                let modifiers = Modifiers(event.modifierFlags)
                guard !modifiers.isEmpty else { return }  // a bare key is too easy to trigger
                commit(Hotkey(keyCode: event.keyCode, modifiers: modifiers))
            }
        case .flagsChanged:
            let modifiers = Modifiers(event.modifierFlags)
            if modifiers.isEmpty {
                // Everything released. A chord needs at least two modifiers and no key in between.
                if !keyWasPressed, heldModifiers.symbols.count >= 2 {
                    commit(Hotkey(modifiers: heldModifiers))
                }
                heldModifiers = []
                keyWasPressed = false
            } else {
                heldModifiers.formUnion(modifiers)
            }
        default:
            break
        }
    }

    private func commit(_ candidate: Hotkey) {
        if let message = validate(candidate) {
            onError(message)
        } else {
            hotkey = candidate
            onError(nil)
        }
        stop()
    }
}
