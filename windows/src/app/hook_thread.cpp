#include "hook_thread.h"

#include "log.h"
#include "synthetic_keys.h"

#include "core/chord_detector.h"

#include <bitset>
#include <memory>
#include <optional>

namespace klats::app {

namespace {

enum : UINT {
    kConfigure = WM_APP + 100,  // lParam: std::vector<Binding>*, owned by the hook thread from now on
    kPaused,                    // wParam: 1 or 0
    kReinstall,
    kReset,
    kMouseHook,                 // wParam: 1 to install, 0 to remove
};

struct ChordBinding {
    Action action;
    Hotkey hotkey;
    ChordDetector detector;
};

// Lives on the hook thread and is touched by nothing else, so the callbacks need no locks.
struct HookState {
    HWND actionTarget = nullptr;
    HWND statusTarget = nullptr;
    HHOOK keyboard = nullptr;
    HHOOK mouse = nullptr;
    std::vector<Binding> combos;
    std::vector<ChordBinding> chords;
    bool paused = false;
    std::bitset<256> down;       // keys held now, left and right modifiers apart
    std::bitset<256> swallowed;  // keys whose press Klats swallowed: their repeats and release go too
    Modifiers modifiers;
    LARGE_INTEGER frequency{};
};

HookState* g_state = nullptr;

// Klats's own clock: the time field of some events (AltGr) is sometimes zero.
double seconds() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) / static_cast<double>(g_state->frequency.QuadPart);
}

// Old systems and some injectors send the neutral codes; tell left and right apart by scan code.
DWORD normalize(DWORD vk, DWORD scanCode, DWORD flags) {
    switch (vk) {
    case VK_SHIFT: return scanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
    case VK_CONTROL: return (flags & LLKHF_EXTENDED) ? VK_RCONTROL : VK_LCONTROL;
    case VK_MENU: return (flags & LLKHF_EXTENDED) ? VK_RMENU : VK_LMENU;
    default: return vk;
    }
}

std::optional<Modifier> modifierOf(DWORD vk) {
    switch (vk) {
    case VK_LSHIFT: case VK_RSHIFT: return Modifier::Shift;
    case VK_LCONTROL: case VK_RCONTROL: return Modifier::Ctrl;
    case VK_LMENU: case VK_RMENU: return Modifier::Alt;
    case VK_LWIN: case VK_RWIN: return Modifier::Win;
    default: return std::nullopt;
    }
}

Modifiers heldModifiers() {
    Modifiers held;
    for (DWORD vk : {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN}) {
        if (g_state->down[vk]) held.insert(*modifierOf(vk));
    }
    return held;
}

bool mouseButtonDown() {
    for (int vk : {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2}) {
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    }
    return false;
}

void fire(Action action, Modifiers modifiers) {
    // The program never sees a key between the press and the release of Win or Alt (there was none,
    // or Klats swallowed it), so Windows would open Start or the menu bar. A masking key in between,
    // sent before the release goes on, prevents that.
    if (modifiers.contains(Modifier::Alt) || modifiers.contains(Modifier::Win)) synthetic::mask();
    PostMessageW(g_state->actionTarget, WM_KLATS_ACTION, static_cast<WPARAM>(action), 0);
}

void otherInput() {
    for (auto& chord : g_state->chords) chord.detector.otherInput();
}

void modifiersChanged(Modifiers now) {
    HookState& s = *g_state;
    bool wasEmpty = s.modifiers.empty();
    s.modifiers = now;
    // Clicks and scrolling cancel a chord, so the mouse is watched while any modifier is down,
    // and only then: a permanent mouse hook would cost time on every mouse move.
    if (!s.chords.empty() && wasEmpty != now.empty()) PostThreadMessageW(GetCurrentThreadId(), kMouseHook, now.empty() ? 0 : 1, 0);
    double time = seconds();
    for (auto& chord : s.chords) {
        if (!chord.detector.flagsChanged(now, time) || s.paused) continue;
        // Win+Alt pressed in the middle of a drag is not ours.
        if (mouseButtonDown()) continue;
        fire(chord.action, chord.hotkey.modifiers);
    }
}

// Returns true when the event must not reach the program.
bool keyEvent(const KBDLLHOOKSTRUCT& event) {
    HookState& s = *g_state;
    if ((event.flags & LLKHF_INJECTED) && event.dwExtraInfo == kInjectedMarker) return false;
    // Windows adds a fake left Ctrl to AltGr (scan code 0x21D) and fake Shifts around the numeric
    // keypad (0x22A, 0x236). Nobody pressed them; without this, a lone right Alt in the Russian
    // layout would look like Ctrl+Alt.
    if (event.scanCode & 0x200) return false;
    bool pressed = !(event.flags & LLKHF_UP);
    DWORD vk = normalize(event.vkCode, event.scanCode, event.flags);
    if (vk >= 256) return false;

    if (modifierOf(vk)) {
        bool wasDown = s.down[vk];
        s.down[vk] = pressed;
        if (wasDown == pressed) return false;  // key repeat of a held modifier changes nothing
        Modifiers now = heldModifiers();
        if (now != s.modifiers) modifiersChanged(now);
        return false;
    }

    if (!pressed) {
        s.down[vk] = false;
        if (s.swallowed[vk]) {
            s.swallowed[vk] = false;
            return true;
        }
        return false;
    }
    bool repeat = s.down[vk];
    s.down[vk] = true;
    if (s.swallowed[vk]) return true;
    otherInput();
    if (s.paused) return false;
    for (const Binding& binding : s.combos) {
        if (binding.hotkey.vk != vk || binding.hotkey.modifiers != s.modifiers) continue;
        // Swallow the press, its repeats and its release, even if the modifiers go up first.
        s.swallowed[vk] = true;
        if (!repeat) fire(binding.action, binding.hotkey.modifiers);
        return true;
    }
    return false;
}

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_state && keyEvent(*reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam))) return 1;
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_state) {
        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            otherInput();
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void setMouseHook(HookState& s, bool on) {
    if (on && !s.mouse) {
        s.mouse = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
    } else if (!on && s.mouse) {
        UnhookWindowsHookEx(s.mouse);
        s.mouse = nullptr;
    }
}

void resetState(HookState& s) {
    s.down.reset();
    s.swallowed.reset();
    s.modifiers = {};
    for (auto& chord : s.chords) chord.detector.reset();
    setMouseHook(s, false);
}

void applyBindings(HookState& s, const std::vector<Binding>& bindings) {
    s.combos.clear();
    s.chords.clear();
    for (const Binding& binding : bindings) {
        if (binding.hotkey.isChord()) {
            s.chords.push_back({binding.action, binding.hotkey, ChordDetector(binding.hotkey.modifiers)});
        } else {
            s.combos.push_back(binding);
        }
    }
    if (s.chords.empty()) setMouseHook(s, false);
}

bool installKeyboardHook(HookState& s) {
    s.keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
    if (!s.keyboard) {
        log::write(L"keyboard hook could not be installed, error " + std::to_wstring(GetLastError()));
        PostMessageW(s.statusTarget, WM_KLATS_HOOK_FAILED, 0, 0);
        return false;
    }
    return true;
}

}  // namespace

bool HookThread::start(HWND actionTarget, HWND statusTarget, bool installHook) {
    if (thread_) return true;
    actionTarget_ = actionTarget;
    statusTarget_ = statusTarget;
    installHook_ = installHook;
    ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready_) return false;
    thread_ = CreateThread(nullptr, 64 * 1024, &HookThread::threadMain, this, 0, &threadId_);
    if (!thread_) return false;
    WaitForSingleObject(ready_, INFINITE);
    return !installHook || hookInstalled_.load();
}

void HookThread::stop() {
    if (thread_) {
        PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        WaitForSingleObject(thread_, 2000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (ready_) {
        CloseHandle(ready_);
        ready_ = nullptr;
    }
}

void HookThread::configure(const std::vector<Binding>& bindings) {
    if (!thread_) return;
    auto* copy = new std::vector<Binding>(bindings);
    if (!PostThreadMessageW(threadId_, kConfigure, 0, reinterpret_cast<LPARAM>(copy))) delete copy;
}

void HookThread::setPaused(bool paused) {
    if (thread_) PostThreadMessageW(threadId_, kPaused, paused ? 1 : 0, 0);
}

void HookThread::reinstall() {
    if (thread_) PostThreadMessageW(threadId_, kReinstall, 0, 0);
}

void HookThread::reset() {
    if (thread_) PostThreadMessageW(threadId_, kReset, 0, 0);
}

DWORD WINAPI HookThread::threadMain(void* parameter) {
    auto* self = static_cast<HookThread*>(parameter);
    HookState state;
    state.actionTarget = self->actionTarget_;
    state.statusTarget = self->statusTarget_;
    QueryPerformanceFrequency(&state.frequency);
    g_state = &state;

    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);  // creates the queue before anyone posts to it
    // Like AutoHotkey: even when a busy program runs at high priority, keys must not lag.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    if (self->installHook_) self->hookInstalled_ = installKeyboardHook(state);
    SetEvent(self->ready_);

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        switch (message.message) {
        case kConfigure: {
            std::unique_ptr<std::vector<Binding>> bindings(reinterpret_cast<std::vector<Binding>*>(message.lParam));
            applyBindings(state, *bindings);
            break;
        }
        case kPaused:
            state.paused = message.wParam != 0;
            break;
        case kReset:
            resetState(state);
            break;
        case kReinstall:
            if (!self->installHook_) break;
            if (state.keyboard && !UnhookWindowsHookEx(state.keyboard) && GetLastError() == ERROR_INVALID_HOOK_HANDLE) {
                log::write(L"Windows had removed the keyboard hook");
            }
            state.keyboard = nullptr;
            resetState(state);
            self->hookInstalled_ = installKeyboardHook(state);
            log::write(self->hookInstalled_ ? L"keyboard hook reinstalled" : L"keyboard hook could not be reinstalled");
            break;
        case kMouseHook:
            setMouseHook(state, message.wParam != 0);
            break;
        default:
            break;
        }
    }

    setMouseHook(state, false);
    if (state.keyboard) UnhookWindowsHookEx(state.keyboard);
    g_state = nullptr;
    return 0;
}

}  // namespace klats::app
