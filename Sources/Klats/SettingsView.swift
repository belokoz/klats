import SwiftUI
import KlatsCore

struct SettingsView: View {
    @ObservedObject var store: SettingsStore
    @ObservedObject var appState: AppState
    var onGrantPermission: () -> Void

    @State private var layouts: [LayoutOption] = []
    @State private var layoutError: String?
    @State private var caseError: String?

    var body: some View {
        Group {
            if #available(macOS 13, *), !Compat.simulatesMacOS12 {
                groupedForm
            } else {
                macOS12Form
            }
        }
        .onAppear(perform: loadLayouts)
    }

    @available(macOS 13, *)
    private var groupedForm: some View {
        Form {
            Section {
                LabeledContent(L("Сменить раскладку")) { layoutHotkeyControl }
                LabeledContent(L("Сменить регистр")) { caseHotkeyControl }
            } header: {
                Text(L("Горячие клавиши"))
            } footer: {
                Text(hotkeysNote)
                    .font(.caption).foregroundStyle(.secondary)
            }

            Section {
                if layouts.count > 2 {
                    Picker(L("Первая раскладка"), selection: firstLayoutBinding) { layoutChoices }
                    Picker(L("Вторая раскладка"), selection: secondLayoutBinding) { layoutChoices }
                } else {
                    LabeledContent(L("Пара раскладок")) { layoutPairText }
                }
                Toggle(L("Переключать раскладку после конвертации"), isOn: $store.switchLayoutAfterConversion)
            } header: {
                Text(L("Раскладка"))
            }

            Section {
                caseModePicker
            } header: {
                Text(L("Регистр"))
            }

            Section {
                Toggle(L("Запускать при входе в систему"), isOn: loginItemBinding)
                Toggle(L("Проверять обновления при запуске"), isOn: $store.checkForUpdates)
                LabeledContent(L("Универсальный доступ")) { accessibilityStatus }
            } header: {
                Text(L("Общие"))
            } footer: {
                footer
            }
        }
        .formStyle(.grouped)
        .frame(width: 480, height: 694)
    }

    /// macOS 12 has neither the grouped form style nor LabeledContent, so the same sections are
    /// laid out by hand to look like the grouped form.
    private var macOS12Form: some View {
        VStack(alignment: .leading, spacing: 20) {
            GroupBox12(title: L("Горячие клавиши"), note: hotkeysNote) {
                Row12(L("Сменить раскладку"), alignment: .firstTextBaseline) { layoutHotkeyControl }
                Divider12()
                Row12(L("Сменить регистр"), alignment: .firstTextBaseline) { caseHotkeyControl }
            }

            GroupBox12(title: L("Раскладка")) {
                if layouts.count > 2 {
                    Row12(L("Первая раскладка")) { layoutMenu(L("Первая раскладка"), firstLayoutBinding) }
                    Divider12()
                    Row12(L("Вторая раскладка")) { layoutMenu(L("Вторая раскладка"), secondLayoutBinding) }
                } else {
                    Row12(L("Пара раскладок")) { layoutPairText }
                }
                Divider12()
                Row12(L("Переключать раскладку после конвертации")) {
                    switch12(L("Переключать раскладку после конвертации"), $store.switchLayoutAfterConversion)
                }
            }

            GroupBox12(title: L("Регистр")) {
                caseModePicker
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 8)
            }

            GroupBox12(title: L("Общие")) {
                Row12(L("Запускать при входе в систему")) { switch12(L("Запускать при входе в систему"), loginItemBinding) }
                Divider12()
                Row12(L("Проверять обновления при запуске")) {
                    switch12(L("Проверять обновления при запуске"), $store.checkForUpdates)
                }
                Divider12()
                Row12(L("Универсальный доступ")) { accessibilityStatus }
            }

            footer
        }
        .padding(20)
        .frame(width: 480)
        .fixedSize(horizontal: false, vertical: true)
    }

    private var hotkeysNote: String {
        L("Нажмите на поле и введите новое сочетание. Сочетание из одних модификаторов срабатывает, когда вы отпускаете клавиши. Esc отменяет запись, ⌫ убирает сочетание совсем.")
    }

    private var layoutHotkeyControl: some View {
        hotkeyControl(hotkey: $store.layoutHotkey, defaultValue: SettingsStore.defaultLayoutHotkey,
                      other: store.caseHotkey, error: $layoutError)
    }

    private var caseHotkeyControl: some View {
        hotkeyControl(hotkey: $store.caseHotkey, defaultValue: SettingsStore.defaultCaseHotkey,
                      other: store.layoutHotkey, error: $caseError)
    }

    private var caseModePicker: some View {
        Picker("", selection: $store.caseMode) {
            caseOption(L("Инвертировать"), example: "пРИВЕТ → Привет", tag: .invert)
            caseOption(L("Все строчные"), example: "ПРИВЕТ → привет", tag: .lowercase)
        }
        .pickerStyle(.radioGroup)
        .labelsHidden()
    }

    private var layoutChoices: some View {
        ForEach(layouts) { Text($0.name).tag($0.id) }
    }

    // The title is hidden on screen (the row shows it) but still names the control for VoiceOver.
    private func layoutMenu(_ title: String, _ selection: Binding<String>) -> some View {
        Picker(title, selection: selection) { layoutChoices }
            .labelsHidden()
            .fixedSize()
    }

    private var layoutPairText: some View {
        Text(layouts.count == 2 ? "\(layouts[0].name) ⇄ \(layouts[1].name)" : L("Нужны две раскладки"))
            .foregroundStyle(.secondary)
    }

    private func switch12(_ title: String, _ isOn: Binding<Bool>) -> some View {
        Toggle(title, isOn: isOn)
            .toggleStyle(.switch)
            .controlSize(.mini)
            .labelsHidden()
    }

    private var footer: some View {
        VStack(spacing: 7) {
            Link("Клац \(Links.version) · MIT · github.com/belokoz/klats", destination: Links.repo)
                .font(.caption).foregroundStyle(.secondary)
                .pointingHandOnHover()
            Link(destination: Links.diktuy(from: "settings")) {
                HStack(spacing: 5) {
                    Image(systemName: "mic.fill")
                    Text(L("Диктуй")).fontWeight(.semibold) + Text(L(": голос в текст"))
                }
            }
            .font(.caption)
            .pointingHandOnHover()
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 12)
    }

    private func hotkeyControl(hotkey: Binding<Hotkey?>, defaultValue: Hotkey, other: Hotkey?, error: Binding<String?>) -> some View {
        VStack(alignment: .trailing, spacing: 4) {
            HotkeyRecorder(hotkey: hotkey, defaultValue: defaultValue, store: store,
                           validate: { validate($0, against: other) },
                           onError: { error.wrappedValue = $0 })
            if let message = error.wrappedValue {
                Text(message).font(.caption).foregroundStyle(.red)
            }
        }
    }

    private func caseOption(_ title: String, example: String, tag: CaseConverter.Mode) -> some View {
        HStack {
            Text(title)
            Spacer()
            Text(example).font(.system(size: 12, design: .monospaced)).foregroundStyle(.secondary)
        }
        .tag(tag)
    }

    @ViewBuilder private var accessibilityStatus: some View {
        if appState.isTrusted {
            HStack(spacing: 6) {
                Circle().fill(.green).frame(width: 8, height: 8)
                Text(L("Разрешение выдано")).foregroundStyle(.secondary)
            }
        } else {
            Button(L("Выдать разрешение…"), action: onGrantPermission)
        }
    }

    // The user's pair, defaulting to the first two enabled layouts.
    private var effectivePair: [String] {
        store.layoutPair.count == 2 ? store.layoutPair : Array(layouts.prefix(2).map(\.id))
    }

    private var firstLayoutBinding: Binding<String> {
        Binding(get: { effectivePair.first ?? "" },
                set: { store.layoutPair = [$0, effectivePair.count > 1 ? effectivePair[1] : ""] })
    }

    private var secondLayoutBinding: Binding<String> {
        Binding(get: { effectivePair.count > 1 ? effectivePair[1] : "" },
                set: { store.layoutPair = [effectivePair.first ?? "", $0] })
    }

    private var loginItemBinding: Binding<Bool> {
        Binding(get: { LoginItem.isEnabled }, set: { LoginItem.setEnabled($0) })
    }

    private func loadLayouts() {
        layouts = InputSources.enabledLayouts().map { LayoutOption(id: $0.id, name: $0.name) }
    }

    private func validate(_ candidate: Hotkey, against other: Hotkey?) -> String? {
        if candidate == other { return L("Уже назначено на другое действие") }
        if ReservedHotkeys.isReserved(candidate) { return L("Занято системой. Выберите другое.") }
        return nil
    }
}

struct LayoutOption: Identifiable, Hashable {
    let id: String
    let name: String
}

/// A small set of shortcuts macOS almost always owns. Not exhaustive: the point is to catch the
/// obvious mistakes, not to mirror every system binding.
@MainActor
enum ReservedHotkeys {
    private static let reserved: Set<Hotkey> = [
        Hotkey(keyCode: 12, modifiers: [.command]),   // ⌘Q
        Hotkey(keyCode: 13, modifiers: [.command]),   // ⌘W
        Hotkey(keyCode: 48, modifiers: [.command]),   // ⌘Tab
        Hotkey(keyCode: 49, modifiers: [.command]),   // ⌘Space
        Hotkey(keyCode: 49, modifiers: [.control]),   // ⌃Space
    ]

    static func isReserved(_ hotkey: Hotkey) -> Bool { reserved.contains(hotkey) }
}

// MARK: - macOS 12 stand-ins for the grouped form

/// A titled rounded box of rows, like a section of a grouped form.
private struct GroupBox12<Content: View>: View {
    let title: String
    var note: String?
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.headline).padding(.horizontal, 10)
            VStack(spacing: 0) { content }
                .background(RoundedRectangle(cornerRadius: 8).fill(Color.groupFill12))
                .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(Color.primary.opacity(0.08)))
            if let note {
                Text(note)
                    .font(.caption).foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 10)
            }
        }
    }
}

/// A label on the left, its control on the right.
private struct Row12<Content: View>: View {
    let title: String
    let alignment: VerticalAlignment
    let content: Content

    init(_ title: String, alignment: VerticalAlignment = .center, @ViewBuilder content: () -> Content) {
        self.title = title
        self.alignment = alignment
        self.content = content()
    }

    var body: some View {
        HStack(alignment: alignment, spacing: 12) {
            Text(title)
            Spacer(minLength: 0)
            content
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .frame(minHeight: 38)
    }
}

private struct Divider12: View {
    var body: some View { Divider().padding(.horizontal, 10) }
}

private extension Color {
    /// A grouped form's box: a touch lighter than the window in either appearance.
    static let groupFill12 = Color(nsColor: NSColor(name: nil) { appearance in
        appearance.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua
            ? NSColor(white: 1, alpha: 0.05)
            : NSColor(white: 1, alpha: 0.7)
    })
}

extension View {
    /// SwiftUI links on macOS keep the arrow cursor; a link should show the pointing hand.
    func pointingHandOnHover() -> some View {
        onHover { hovering in
            if hovering { NSCursor.pointingHand.push() } else { NSCursor.pop() }
        }
    }
}
