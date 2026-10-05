#include "settings_window.h"

#include "common.h"
#include "hook_thread.h"
#include "hotkey_display.h"
#include "log.h"
#include "login_item.h"
#include "strings.h"
#include "version.h"

#include "../../res/resource.h"

#include <commctrl.h>
#include <shellapi.h>

#include <array>

namespace klats::app {

namespace {

constexpr UINT_PTR kRecordingTimer = 1;
constexpr UINT kRecordingTimeoutMs = 10000;

const wchar_t* kRepositoryLink = L"https://github.com/belokoz/klats";
const wchar_t* kDiktuyLink = L"https://diktuy.ru/?utm_source=klats&utm_medium=app&utm_campaign=settings";

SettingsWindow* g_window = nullptr;

// Shortcuts Windows almost always owns, and the keys Klats itself sends. Not exhaustive: the point
// is to catch the obvious mistakes, not to mirror every system binding.
bool isReserved(const Hotkey& hotkey) {
    using M = Modifier;
    struct Entry {
        uint8_t vk;
        Modifiers modifiers;
    };
    static const Entry kReserved[] = {
        {'L', {M::Win}}, {VK_SPACE, {M::Win}}, {VK_SPACE, {M::Win, M::Shift}}, {'V', {M::Win}}, {'D', {M::Win}},
        {'E', {M::Win}}, {'R', {M::Win}}, {VK_TAB, {M::Win}}, {'X', {M::Win}}, {'I', {M::Win}},
        {VK_DELETE, {M::Ctrl, M::Alt}}, {VK_ESCAPE, {M::Ctrl, M::Shift}}, {VK_ESCAPE, {M::Ctrl}}, {VK_ESCAPE, {M::Alt}},
        {VK_TAB, {M::Alt}}, {VK_TAB, {M::Alt, M::Shift}}, {VK_TAB, {M::Ctrl, M::Alt}}, {VK_F4, {M::Alt}}, {VK_SPACE, {M::Alt}},
        // What Klats sends to copy and paste, and the editing keys everybody uses.
        {VK_INSERT, {M::Ctrl}}, {VK_INSERT, {M::Shift}}, {VK_DELETE, {M::Shift}}, {'C', {M::Ctrl}}, {'V', {M::Ctrl}},
        {'X', {M::Ctrl}}, {'Z', {M::Ctrl}}, {'A', {M::Ctrl}},
    };
    if (hotkey.isChord()) {
        // Alt+Shift and Ctrl+Shift switch the input language and the layout in Windows.
        Modifiers modifiers = hotkey.modifiers;
        return modifiers == Modifiers{M::Alt, M::Shift} || modifiers == Modifiers{M::Ctrl, M::Shift};
    }
    for (const Entry& entry : kReserved) {
        if (entry.vk == hotkey.vk && entry.modifiers == hotkey.modifiers) return true;
    }
    return false;
}

// A program that registered the combination with RegisterHotKey makes the registration fail. It
// does not see programs that catch keys with a hook, so the answer is a hint, never a promise.
bool takenByAnotherProgram(const Hotkey& hotkey) {
    if (hotkey.isChord()) return false;
    UINT modifiers = MOD_NOREPEAT;
    if (hotkey.modifiers.contains(Modifier::Alt)) modifiers |= MOD_ALT;
    if (hotkey.modifiers.contains(Modifier::Ctrl)) modifiers |= MOD_CONTROL;
    if (hotkey.modifiers.contains(Modifier::Shift)) modifiers |= MOD_SHIFT;
    if (hotkey.modifiers.contains(Modifier::Win)) modifiers |= MOD_WIN;
    constexpr int kProbeId = 0x4B4C;
    if (RegisterHotKey(nullptr, kProbeId, modifiers, hotkey.vk)) {
        UnregisterHotKey(nullptr, kProbeId);
        return false;
    }
    return GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED;
}

void setText(HWND dialog, int id, const std::wstring& text) { SetDlgItemTextW(dialog, id, text.c_str()); }

}  // namespace

void SettingsWindow::show() {
    if (!window_) {
        g_window = this;
        window_ = CreateDialogParamW(instance_, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, dialogProc, 0);
        if (!window_) {
            log::write(L"settings window could not be created, error " + std::to_wstring(GetLastError()));
            return;
        }
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
}

INT_PTR CALLBACK SettingsWindow::dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_window) return FALSE;
    if (message == WM_INITDIALOG) g_window->window_ = window;
    if (g_window->window_ != window) return FALSE;
    return g_window->handle(message, wParam, lParam);
}

void SettingsWindow::initialize() {
    HWND dialog = window_;
    SetWindowTextW(dialog, tr(L"Параметры Клаца"));
    HICON big = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                              GetSystemMetrics(SM_CYICON), 0));
    HICON small = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON), 0));
    SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    SendMessageW(dialog, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));

    // Group headings: the dialog font, semibold.
    LOGFONTW font{};
    GetObjectW(reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0)), sizeof font, &font);
    font.lfWeight = FW_SEMIBOLD;
    headerFont_ = CreateFontIndirectW(&font);
    for (int id : {IDC_HOTKEYS_HEADER, IDC_LAYOUT_HEADER, IDC_CASE_HEADER, IDC_GENERAL_HEADER}) {
        SendDlgItemMessageW(dialog, id, WM_SETFONT, reinterpret_cast<WPARAM>(headerFont_), FALSE);
    }

    setText(dialog, IDC_HOTKEYS_HEADER, tr(L"Сочетания клавиш"));
    setText(dialog, IDC_LAYOUT_LABEL, tr(L"Сменить раскладку"));
    setText(dialog, IDC_CASE_LABEL, tr(L"Сменить регистр"));
    setText(dialog, IDC_HOTKEYS_NOTE,
            tr(L"Нажмите на поле и введите новое сочетание. Сочетание из одних модификаторов срабатывает, когда вы "
               L"отпускаете клавиши. Esc отменяет запись, Backspace убирает сочетание совсем."));
    setText(dialog, IDC_HOTKEY_ERROR, L"");
    for (int id : {IDC_LAYOUT_RESET, IDC_CASE_RESET}) setText(dialog, id, L"↺");
    setText(dialog, IDC_LAYOUT_HEADER, tr(L"Раскладка"));
    setText(dialog, IDC_SWITCH_LAYOUT, tr(L"Переключать раскладку после конвертации"));
    setText(dialog, IDC_CASE_HEADER, tr(L"Регистр"));
    setText(dialog, IDC_CASE_INVERT, tr(L"Инвертировать"));
    setText(dialog, IDC_CASE_INVERT_EXAMPLE, L"пРИВЕТ → Привет");
    setText(dialog, IDC_CASE_LOWER, tr(L"Все строчные"));
    setText(dialog, IDC_CASE_LOWER_EXAMPLE, L"ПРИВЕТ → привет");
    setText(dialog, IDC_GENERAL_HEADER, tr(L"Общие"));
    setText(dialog, IDC_AUTOSTART, tr(L"Запускать при входе в Windows"));
    setText(dialog, IDC_AUTOSTART_NOTE, tr(L"Выключено в Диспетчере задач на вкладке «Автозагрузка»"));
    setText(dialog, IDC_CHECK_UPDATES, tr(L"Проверять обновления при запуске"));
    setText(dialog, IDC_FOOTER_REPO, std::wstring(L"<a>Клац ") + KLATS_VERSION_WSTRING + L" · MIT · github.com/belokoz/klats</a>");
    setText(dialog, IDC_FOOTER_DIKTUY, std::wstring(L"<a>") + tr(L"Диктуй: голос в текст") + L"</a>");

    Settings settings = settings_.snapshot();
    layout_.attach(GetDlgItem(dialog, IDC_LAYOUT_HOTKEY), tr(L"Сменить раскладку"), settings.layoutHotkey);
    case_.attach(GetDlgItem(dialog, IDC_CASE_HOTKEY), tr(L"Сменить регистр"), settings.caseHotkey);
    CheckDlgButton(dialog, IDC_SWITCH_LAYOUT, settings.switchLayoutAfterConversion ? BST_CHECKED : BST_UNCHECKED);
    CheckRadioButton(dialog, IDC_CASE_INVERT, IDC_CASE_LOWER, settings.caseMode == CaseMode::Lowercase ? IDC_CASE_LOWER : IDC_CASE_INVERT);
    CheckDlgButton(dialog, IDC_CHECK_UPDATES, settings.checkForUpdates ? BST_CHECKED : BST_UNCHECKED);
    loadLayouts();
    refreshAutostart();
    refreshResetButtons();
}

// The ↺ button restores the default and only shows when there is something to restore.
void SettingsWindow::refreshResetButtons() {
    ShowWindow(GetDlgItem(window_, IDC_LAYOUT_RESET), layout_.value() == defaults::layoutHotkey ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(window_, IDC_CASE_RESET), case_.value() == defaults::caseHotkey ? SW_HIDE : SW_SHOW);
}

void SettingsWindow::loadLayouts() {
    LayoutSet set = readLayouts(settings_.snapshot().layoutPair);
    layouts_ = std::move(set.layouts);
    bool paired = set.pair.problem == PairChoice::Problem::None;
    setText(window_, IDC_PAIR_LABEL, layouts_.size() > 2 ? tr(L"Первая раскладка") : tr(L"Пара раскладок"));
    setText(window_, IDC_SECOND_LABEL, tr(L"Вторая раскладка"));
    if (layouts_.size() > 2) {
        ShowWindow(GetDlgItem(window_, IDC_PAIR_TEXT), SW_HIDE);
        // The pair in use. While it is still to be chosen nothing is picked: Windows lists layouts
        // by language, so the first two would be a guess.
        for (int id : {IDC_FIRST_LAYOUT, IDC_SECOND_LAYOUT}) {
            HWND combo = GetDlgItem(window_, id);
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            for (const auto& layout : layouts_) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(layout.name.c_str()));
            if (paired) SendMessageW(combo, CB_SETCURSEL, id == IDC_FIRST_LAYOUT ? set.pair.first : set.pair.second, 0);
        }
        return;
    }
    setText(window_, IDC_PAIR_TEXT,
            paired ? layouts_[set.pair.first].name + L" ⇄ " + layouts_[set.pair.second].name : std::wstring(tr(L"Нужны две раскладки")));
    // Exactly two layouts: a line of text instead of two pickers, and the window gets shorter.
    collapse(150, 18, {IDC_FIRST_LAYOUT, IDC_SECOND_LABEL, IDC_SECOND_LAYOUT});
}

void SettingsWindow::collapse(int fromDialogUnitsY, int byDialogUnits, std::initializer_list<int> hidden) {
    RECT offset{0, fromDialogUnitsY, 0, byDialogUnits};
    MapDialogRect(window_, &offset);
    for (int id : hidden) ShowWindow(GetDlgItem(window_, id), SW_HIDE);
    for (HWND child = GetWindow(window_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        RECT bounds;
        GetWindowRect(child, &bounds);
        MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&bounds), 2);
        if (bounds.top >= offset.top) SetWindowPos(child, nullptr, bounds.left, bounds.top - offset.bottom, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    RECT window;
    GetWindowRect(window_, &window);
    SetWindowPos(window_, nullptr, 0, 0, window.right - window.left, window.bottom - window.top - offset.bottom, SWP_NOMOVE | SWP_NOZORDER);
}

void SettingsWindow::refreshAutostart() {
    login_item::State state = login_item::state();
    CheckDlgButton(window_, IDC_AUTOSTART, state == login_item::State::On ? BST_CHECKED : BST_UNCHECKED);
    bool disabled = state == login_item::State::DisabledInTaskManager;
    ShowWindow(GetDlgItem(window_, IDC_AUTOSTART_NOTE), disabled ? SW_SHOW : SW_HIDE);
    // The note takes a line under the box only while it shows; the next box moves up into it.
    RECT step{0, 0, 0, disabled ? 26 : 16};
    MapDialogRect(window_, &step);
    RECT autostart;
    GetWindowRect(GetDlgItem(window_, IDC_AUTOSTART), &autostart);
    MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&autostart), 2);
    SetWindowPos(GetDlgItem(window_, IDC_CHECK_UPDATES), nullptr, autostart.left, autostart.top + step.bottom, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::optional<Hotkey>& SettingsWindow::stored(HotkeyRecorder& recorder, Settings& settings) {
    return &recorder == &layout_ ? settings.layoutHotkey : settings.caseHotkey;
}

void SettingsWindow::startRecording(HotkeyRecorder& recorder) {
    if (recording_) stopRecording();
    recording_ = &recorder;
    recorder.startRecording();
    showError(L"");
    hooks_.setRecorder(window_);
    SetTimer(window_, kRecordingTimer, kRecordingTimeoutMs, nullptr);
}

void SettingsWindow::stopRecording() {
    if (!recording_) return;
    KillTimer(window_, kRecordingTimer);
    hooks_.setRecorder(nullptr);
    recording_->stopRecording();
    recording_ = nullptr;
}

void SettingsWindow::recordKey(UINT vk, bool pressed, Modifiers held) {
    if (!recording_) return;
    HotkeyRecorder& recorder = *recording_;
    Hotkey candidate;
    switch (recorder.key(vk, pressed, held, &candidate)) {
    case HotkeyRecorder::Outcome::None:
        return;
    case HotkeyRecorder::Outcome::Cancelled:
        stopRecording();
        return;
    case HotkeyRecorder::Outcome::Cleared:
        stopRecording();
        commit(recorder, std::nullopt);
        return;
    case HotkeyRecorder::Outcome::Candidate:
        stopRecording();
        if (other(recorder).value() && candidate == *other(recorder).value()) {
            showError(tr(L"Уже назначено на другое действие"));
        } else if (isReserved(candidate)) {
            showError(tr(L"Занято системой. Выберите другое."));
        } else {
            commit(recorder, candidate);
            if (takenByAnotherProgram(candidate)) {
                showError(tr(L"Это сочетание уже занято другой программой: Клац перехватит его первым."), true);
            }
        }
        return;
    }
}

void SettingsWindow::commit(HotkeyRecorder& recorder, std::optional<Hotkey> hotkey) {
    recorder.setValue(hotkey);
    settings_.update([&](Settings& settings) { stored(recorder, settings) = hotkey; });
    log::write(std::wstring(&recorder == &layout_ ? L"layout" : L"case") + L" hotkey set to " +
               (hotkey ? displayString(*hotkey) : std::wstring(L"none")));
    refreshResetButtons();
    if (hotkeysChanged_) hotkeysChanged_();
}

void SettingsWindow::showError(const std::wstring& message, bool warning) {
    errorIsWarning_ = warning;
    setText(window_, IDC_HOTKEY_ERROR, message);
    InvalidateRect(GetDlgItem(window_, IDC_HOTKEY_ERROR), nullptr, TRUE);
}

INT_PTR SettingsWindow::handle(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        initialize();
        return TRUE;

    case WM_KLATS_RECORD_KEY:
        recordKey(static_cast<UINT>(wParam), (lParam & 1) != 0, Modifiers::fromBits(static_cast<uint32_t>(lParam >> 8)));
        return TRUE;

    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item->CtlID == IDC_LAYOUT_HOTKEY) layout_.draw(*item);
        else if (item->CtlID == IDC_CASE_HOTKEY) case_.draw(*item);
        else return FALSE;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_LAYOUT_HOTKEY:
        case IDC_CASE_HOTKEY: {
            HotkeyRecorder& recorder = LOWORD(wParam) == IDC_LAYOUT_HOTKEY ? layout_ : case_;
            if (HIWORD(wParam) == BN_CLICKED) {
                if (recording_ == &recorder) stopRecording();
                else startRecording(recorder);
            }
            return TRUE;
        }
        case IDC_LAYOUT_RESET:
            stopRecording();
            showError(L"");
            commit(layout_, defaults::layoutHotkey);
            return TRUE;
        case IDC_CASE_RESET:
            stopRecording();
            showError(L"");
            commit(case_, defaults::caseHotkey);
            return TRUE;
        case IDC_SWITCH_LAYOUT: {
            bool on = IsDlgButtonChecked(window_, IDC_SWITCH_LAYOUT) == BST_CHECKED;
            settings_.update([&](Settings& settings) { settings.switchLayoutAfterConversion = on; });
            return TRUE;
        }
        case IDC_CASE_INVERT:
        case IDC_CASE_LOWER: {
            CaseMode mode = IsDlgButtonChecked(window_, IDC_CASE_LOWER) == BST_CHECKED ? CaseMode::Lowercase : CaseMode::Invert;
            settings_.update([&](Settings& settings) { settings.caseMode = mode; });
            return TRUE;
        }
        case IDC_CHECK_UPDATES: {
            bool on = IsDlgButtonChecked(window_, IDC_CHECK_UPDATES) == BST_CHECKED;
            settings_.update([&](Settings& settings) { settings.checkForUpdates = on; });
            return TRUE;
        }
        case IDC_AUTOSTART: {
            // The box follows what really happened, not the click.
            login_item::setEnabled(IsDlgButtonChecked(window_, IDC_AUTOSTART) == BST_CHECKED);
            refreshAutostart();
            return TRUE;
        }
        case IDC_FIRST_LAYOUT:
        case IDC_SECOND_LAYOUT:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                auto first = SendDlgItemMessageW(window_, IDC_FIRST_LAYOUT, CB_GETCURSEL, 0, 0);
                auto second = SendDlgItemMessageW(window_, IDC_SECOND_LAYOUT, CB_GETCURSEL, 0, 0);
                if (first >= 0 && second >= 0 && first != second && static_cast<size_t>(first) < layouts_.size() &&
                    static_cast<size_t>(second) < layouts_.size()) {
                    std::vector<std::wstring> pair = {layouts_[static_cast<size_t>(first)].id, layouts_[static_cast<size_t>(second)].id};
                    settings_.update([&](Settings& settings) { settings.layoutPair = pair; });
                }
            }
            return TRUE;
        case IDCANCEL:
            stopRecording();
            DestroyWindow(window_);
            return TRUE;
        default:
            return FALSE;
        }

    case WM_NOTIFY: {
        auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if ((header->code == NM_CLICK || header->code == NM_RETURN) &&
            (header->idFrom == IDC_FOOTER_REPO || header->idFrom == IDC_FOOTER_DIKTUY)) {
            ShellExecuteW(nullptr, L"open", header->idFrom == IDC_FOOTER_REPO ? kRepositoryLink : kDiktuyLink, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        return FALSE;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        if (id == IDC_HOTKEY_ERROR) {
            SetTextColor(dc, errorIsWarning_ ? GetSysColor(COLOR_GRAYTEXT) : errorColor_);
        } else if (id == IDC_HOTKEYS_NOTE || id == IDC_CASE_INVERT_EXAMPLE || id == IDC_CASE_LOWER_EXAMPLE ||
                   id == IDC_AUTOSTART_NOTE || id == IDC_PAIR_TEXT) {
            SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
        } else {
            return FALSE;
        }
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_3DFACE));
    }

    case WM_ACTIVATE:
        // Recording ends when the window loses the focus: the keys go back to the user.
        if (LOWORD(wParam) == WA_INACTIVE) stopRecording();
        if (LOWORD(wParam) != WA_INACTIVE) refreshAutostart();  // Task Manager may have changed it meanwhile
        return FALSE;

    case WM_TIMER:
        if (wParam == kRecordingTimer) stopRecording();
        return TRUE;

    case WM_DESTROY:
        stopRecording();
        if (headerFont_) DeleteObject(headerFont_);
        headerFont_ = nullptr;
        window_ = nullptr;
        return FALSE;

    default:
        return FALSE;
    }
}

}  // namespace klats::app
