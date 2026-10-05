#include "strings.h"

#include <windows.h>

#include <string_view>

namespace klats::app {

namespace {

struct Translation {
    std::wstring_view russian;
    const wchar_t* english;
};

const Translation kEnglish[] = {
    // Menu
    {L"Сменить раскладку выделенного", L"Convert selection layout"},
    {L"Сменить регистр выделенного", L"Change selection case"},
    {L"Приостановить", L"Pause"},
    {L"Параметры…", L"Settings…"},
    {L"О программе", L"About"},
    {L"Открыть журнал", L"Open log"},
    {L"Выход", L"Exit"},
    {L"Окно администратора: здесь Клац не работает", L"Administrator window: Klats does not work here"},
    {L"Клац не может следить за клавиатурой", L"Klats cannot watch the keyboard"},
    {L"Клац приостановлен", L"Klats is paused"},
    // Tooltip and messages
    {L"Клац", L"Klats"},
    {L"Клац — приостановлен", L"Klats — paused"},
    {L"Клац уже работает", L"Klats is already running"},
    {L"Его значок — в области уведомлений, рядом с часами. Если его не видно, нажмите стрелку ^.",
     L"Its icon is in the notification area next to the clock. If you cannot see it, click the ^ arrow."},
    {L"Исправляет раскладку выделенного текста одним нажатием.", L"Fixes the layout of selected text with one press."},
    {L"Версия", L"Version"},
    {L"Окно параметров появится в следующей сборке.", L"The settings window comes in the next build."},
    {L"Пробел", L"Space"},
};

}  // namespace

bool interfaceIsRussian() {
    static const bool russian = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    return russian;
}

const wchar_t* tr(const wchar_t* russian) {
    if (interfaceIsRussian()) return russian;
    for (const Translation& t : kEnglish) {
        if (t.russian == russian) return t.english;
    }
    return russian;
}

}  // namespace klats::app
