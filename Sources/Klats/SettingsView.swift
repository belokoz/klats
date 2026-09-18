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
        Form {
            Section {
                hotkeyRow(L("Сменить раскладку"), hotkey: $store.layoutHotkey, defaultValue: SettingsStore.defaultLayoutHotkey,
                          other: store.caseHotkey, error: $layoutError)
                hotkeyRow(L("Сменить регистр"), hotkey: $store.caseHotkey, defaultValue: SettingsStore.defaultCaseHotkey,
                          other: store.layoutHotkey, error: $caseError)
            } header: {
                Text(L("Горячие клавиши"))
            } footer: {
                Text(L("Нажмите на поле и введите новое сочетание. Сочетание из одних модификаторов срабатывает, когда вы отпускаете клавиши. Esc отменяет запись, ⌫ убирает сочетание совсем."))
                    .font(.caption).foregroundStyle(.secondary)
            }

            Section {
                layoutPairControl
                Toggle(L("Переключать раскладку после конвертации"), isOn: $store.switchLayoutAfterConversion)
            } header: {
                Text(L("Раскладка"))
            }

            Section {
                Picker("", selection: $store.caseMode) {
                    caseOption(L("Инвертировать"), example: "пРИВЕТ → Привет", tag: .invert)
                    caseOption(L("Все строчные"), example: "ПРИВЕТ → привет", tag: .lowercase)
                }
                .pickerStyle(.radioGroup)
                .labelsHidden()
            } header: {
                Text(L("Регистр"))
            }

            Section {
                Toggle(L("Запускать при входе в систему"), isOn: loginItemBinding)
                LabeledContent(L("Универсальный доступ")) { accessibilityStatus }
            } header: {
                Text(L("Общие"))
            } footer: {
                footer
            }
        }
        .formStyle(.grouped)
        .frame(width: 480, height: 656)
        .onAppear(perform: loadLayouts)
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

    private func hotkeyRow(_ title: String, hotkey: Binding<Hotkey?>, defaultValue: Hotkey, other: Hotkey?, error: Binding<String?>) -> some View {
        LabeledContent(title) {
            VStack(alignment: .trailing, spacing: 4) {
                HotkeyRecorder(hotkey: hotkey, defaultValue: defaultValue, store: store,
                               validate: { validate($0, against: other) },
                               onError: { error.wrappedValue = $0 })
                if let message = error.wrappedValue {
                    Text(message).font(.caption).foregroundStyle(.red)
                }
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

    @ViewBuilder private var layoutPairControl: some View {
        if layouts.count > 2 {
            Picker(L("Первая раскладка"), selection: firstLayoutBinding) {
                ForEach(layouts) { Text($0.name).tag($0.id) }
            }
            Picker(L("Вторая раскладка"), selection: secondLayoutBinding) {
                ForEach(layouts) { Text($0.name).tag($0.id) }
            }
        } else {
            LabeledContent(L("Пара раскладок")) {
                Text(layouts.count == 2 ? "\(layouts[0].name) ⇄ \(layouts[1].name)" : L("Нужны две раскладки"))
                    .foregroundStyle(.secondary)
            }
        }
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

extension View {
    /// SwiftUI links on macOS keep the arrow cursor; a link should show the pointing hand.
    func pointingHandOnHover() -> some View {
        onHover { hovering in
            if hovering { NSCursor.pointingHand.push() } else { NSCursor.pop() }
        }
    }
}
