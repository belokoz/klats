import SwiftUI
import KlatsCore

struct OnboardingView: View {
    @ObservedObject var store: SettingsStore
    @ObservedObject var appState: AppState
    var onGrant: () -> Void
    var onFinish: () -> Void

    @State private var launchAtLogin = LoginItem.isEnabled
    @State private var probe = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            header
            permissionStep
            tryStep
            finishStep
            HStack {
                Spacer()
                Button(L("Готово"), action: finish)
                    .keyboardShortcut(.defaultAction)
                    .disabled(!appState.isTrusted)
            }
        }
        .padding(20)
        .frame(width: 480)
        .fixedSize(horizontal: false, vertical: true)
    }

    private var header: some View {
        HStack(spacing: 16) {
            Image(nsImage: AppIcon.image(size: 64))
                .resizable().frame(width: 64, height: 64)
            VStack(alignment: .leading, spacing: 3) {
                Text("Клац").font(.system(size: 22, weight: .bold))
                Text(L("Исправляет раскладку выделенного текста одним нажатием."))
                    .font(.callout).foregroundStyle(.secondary)
            }
        }
    }

    private var permissionStep: some View {
        StepCard(number: 1, title: L("Разрешите Клацу работать с текстом"), done: appState.isTrusted) {
            if appState.isTrusted {
                Text(L("Универсальный доступ включён. Отключить его можно в любой момент в системных настройках."))
                    .font(.callout).foregroundStyle(.secondary)
            } else {
                Text(L("Чтобы скопировать выделенный текст и вставить исправленный, нужно разрешение «Универсальный доступ». Клац ничего никуда не отправляет: сетевого кода в нём нет."))
                    .font(.callout).foregroundStyle(.secondary)
                Text("Системные настройки → Конфиденциальность и безопасность → Универсальный доступ → Клац")
                    .font(.system(size: 12, design: .monospaced)).foregroundStyle(.secondary)
                HStack(spacing: 14) {
                    Button(L("Открыть системные настройки"), action: onGrant)
                    HStack(spacing: 7) {
                        Circle().fill(.orange).frame(width: 8, height: 8)
                        Text(L("Ждём разрешения…")).font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
        }
    }

    private var tryStep: some View {
        StepCard(number: 2, title: L("Попробуйте"), dimmed: !appState.isTrusted) {
            if appState.isTrusted {
                Text(L("Выделите текст ниже, зажмите ⌥⌘ и отпустите."))
                    .font(.callout).foregroundStyle(.secondary)
                TextField("", text: $probe)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(size: 15))
                    .onAppear { if probe.isEmpty { probe = "ghbdtn vbh" } }
                Text(L("Должно получиться «привет мир», а раскладка переключится на русскую."))
                    .font(.callout).foregroundStyle(.secondary)
            } else {
                Text(L("Станет доступно, как только появится разрешение."))
                    .font(.callout).foregroundStyle(.secondary)
            }
        }
    }

    private var finishStep: some View {
        StepCard(number: 3, title: L("Последний штрих"), dimmed: !appState.isTrusted) {
            Toggle(L("Запускать Клац при входе в систему"), isOn: $launchAtLogin)
                .onChange(of: launchAtLogin) { LoginItem.setEnabled($0) }
            Text(L("Клац живёт в строке меню. Сочетания клавиш меняются в настройках: ⌥⌘ для раскладки, ⌥⌘Z для регистра."))
                .font(.callout).foregroundStyle(.secondary)
        }
    }

    private func finish() {
        store.onboardingCompleted = true
        onFinish()
    }
}

private struct StepCard<Content: View>: View {
    let number: Int
    let title: String
    var done = false
    var dimmed = false
    @ViewBuilder var content: Content

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            badge
            VStack(alignment: .leading, spacing: 8) {
                Text(title).font(.headline)
                content
            }
            Spacer(minLength: 0)
        }
        .padding(16)
        .background(RoundedRectangle(cornerRadius: 12).fill(Color.secondary.opacity(0.08)))
        .opacity(dimmed ? 0.5 : 1)
    }

    @ViewBuilder private var badge: some View {
        if done {
            Image(systemName: "checkmark.circle.fill")
                .font(.system(size: 20)).foregroundStyle(.green)
        } else {
            Text("\(number)")
                .font(.system(size: 12, weight: .semibold))
                .frame(width: 22, height: 22)
                .overlay(Circle().stroke(Color.secondary, lineWidth: 1.5))
        }
    }
}
