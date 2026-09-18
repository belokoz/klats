import AppKit

/// The menu bar icon and its menu.
@MainActor
final class StatusItemController: NSObject, NSMenuDelegate {
    var onConvertLayout: (() -> Void)?
    var onChangeCase: (() -> Void)?
    var onTogglePause: (() -> Void)?
    var onOpenSettings: (() -> Void)?
    var onShowAbout: (() -> Void)?
    var onOpenLog: (() -> Void)?

    private let store: SettingsStore
    private let appState: AppState
    private let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
    private let menu = NSMenu()

    init(store: SettingsStore, appState: AppState) {
        self.store = store
        self.appState = appState
        super.init()
        item.button?.image = AppIcon.menuBarGlyph()
        item.button?.toolTip = "Клац"
        menu.delegate = self
        menu.autoenablesItems = false
        item.menu = menu
        refresh()
    }

    /// Opens the menu as if the icon had been clicked. Only used for screenshots.
    func popUpMenu() {
        Task { @MainActor in
            try? await Task.sleep(for: .seconds(1))
            item.button?.performClick(nil)
        }
    }

    func refresh() {
        guard let button = item.button else { return }
        button.appearsDisabled = !appState.isTrusted || store.isPaused
    }

    func menuNeedsUpdate(_ menu: NSMenu) {
        appState.refresh()
        refresh()
        menu.removeAllItems()
        let trusted = appState.isTrusted
        let active = trusted && !store.isPaused

        if !trusted {
            addDisabledLabel(L("Клацу нужно разрешение «Универсальный доступ»"))
            menu.addItem(makeItem(L("Выдать разрешение…"), #selector(grantPermission)))
            menu.addItem(.separator())
        } else if store.isPaused {
            addDisabledLabel(L("Клац приостановлен"))
            menu.addItem(.separator())
        } else if appState.secureInputIsOn {
            addDisabledLabel(L("Защищённый ввод включён: хоткеи временно недоступны"))
            menu.addItem(.separator())
        }

        let layout = makeItem("", #selector(convertLayout))
        layout.attributedTitle = title(L("Сменить раскладку выделенного"), shortcut: store.layoutHotkey?.displayString ?? "")
        layout.isEnabled = active
        menu.addItem(layout)

        let letterCase = makeItem("", #selector(changeCase))
        letterCase.attributedTitle = title(L("Сменить регистр выделенного"), shortcut: store.caseHotkey?.displayString ?? "")
        letterCase.isEnabled = active
        menu.addItem(letterCase)

        if trusted {
            menu.addItem(.separator())
            let pause = makeItem(store.isPaused ? L("Возобновить") : L("Приостановить"), #selector(togglePause))
            menu.addItem(pause)
        }

        menu.addItem(.separator())
        menu.addItem(makeItem(L("Настройки…"), #selector(openSettings), key: ",", modifiers: [.command]))
        menu.addItem(makeItem(L("О программе Клац"), #selector(showAbout)))
        let log = makeItem(L("Открыть журнал"), #selector(openLog), modifiers: [.option])
        log.isAlternate = true
        menu.addItem(log)
        menu.addItem(.separator())
        menu.addItem(makeItem(L("Завершить Клац"), #selector(quit), key: "q", modifiers: [.command]))
    }

    private func addDisabledLabel(_ text: String) {
        let label = NSMenuItem(title: text, action: nil, keyEquivalent: "")
        label.isEnabled = false
        menu.addItem(label)
    }

    private func makeItem(_ title: String, _ action: Selector, key: String = "", modifiers: NSEvent.ModifierFlags = []) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key)
        item.keyEquivalentModifierMask = modifiers
        item.target = self
        return item
    }

    // The menu has to finish closing before ⌘C is sent, or the key press lands in the menu.
    @objc private func convertLayout() { afterMenuCloses { [weak self] in self?.onConvertLayout?() } }
    @objc private func changeCase() { afterMenuCloses { [weak self] in self?.onChangeCase?() } }
    @objc private func togglePause() { onTogglePause?() }
    @objc private func openSettings() { onOpenSettings?() }
    @objc private func showAbout() { onShowAbout?() }
    @objc private func openLog() { onOpenLog?() }
    @objc private func grantPermission() {
        Permissions.requestWithSystemPrompt()
        Permissions.openSystemSettings()
    }
    @objc private func quit() { NSApp.terminate(nil) }

    private func afterMenuCloses(_ work: @escaping @MainActor () -> Void) {
        Task { @MainActor in
            try? await Task.sleep(for: .milliseconds(150))
            work()
        }
    }

    /// A menu cannot show a shortcut made of modifiers only, so it is drawn as text.
    private func title(_ text: String, shortcut: String) -> NSAttributedString {
        let paragraph = NSMutableParagraphStyle()
        paragraph.tabStops = [NSTextTab(textAlignment: .right, location: 320)]
        let string = shortcut.isEmpty ? text : "\(text)\t\(shortcut)"
        return NSAttributedString(string: string, attributes: [
            .paragraphStyle: paragraph, .font: NSFont.menuFont(ofSize: 0),
        ])
    }
}
