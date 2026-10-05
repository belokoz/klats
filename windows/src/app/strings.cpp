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
    {L"Выберите пару раскладок…", L"Choose the layout pair…"},
    {L"Приостановить", L"Pause"},
    {L"Параметры…", L"Settings…"},
    {L"О программе", L"About"},
    {L"Открыть журнал", L"Open log"},
    {L"Выход", L"Exit"},
    {L"Окно администратора: здесь Клац не работает", L"Administrator window: Klats does not work here"},
    {L"Клац не может следить за клавиатурой", L"Klats cannot watch the keyboard"},
    // Tooltip and About
    {L"Клац", L"Klats"},
    {L"Клац — приостановлен", L"Klats — paused"},
    {L"Исправляет раскладку выделенного текста одним нажатием.", L"Fixes the layout of selected text with one press."},
    {L"Версия", L"Version"},
    {L"Диктуй: голос в текст", L"Diktuy: voice to text"},
    // Settings
    {L"Параметры Клаца", L"Klats settings"},
    {L"Сочетания клавиш", L"Keyboard shortcuts"},
    {L"Сменить раскладку", L"Convert layout"},
    {L"Сменить регистр", L"Change case"},
    {L"Нажмите на поле и введите новое сочетание. Сочетание из одних модификаторов срабатывает, когда вы отпускаете клавиши. "
     L"Esc отменяет запись, Backspace убирает сочетание совсем.",
     L"Click a field and type a new combination. A modifier-only combination fires when you let the keys go. Esc cancels, "
     L"Backspace removes the combination entirely."},
    {L"Уже назначено на другое действие", L"Already assigned to another action"},
    {L"Занято системой. Выберите другое.", L"Reserved by the system. Pick another."},
    {L"Это сочетание уже занято другой программой: Клац перехватит его первым.",
     L"Another program already uses this combination: Klats will catch it first."},
    {L"Раскладка", L"Layout"},
    {L"Пара раскладок", L"Layout pair"},
    {L"Первая раскладка", L"First layout"},
    {L"Вторая раскладка", L"Second layout"},
    {L"Нужны две раскладки", L"Two layouts required"},
    {L"Переключать раскладку после конвертации", L"Switch layout after converting"},
    {L"Регистр", L"Case"},
    {L"Инвертировать", L"Invert"},
    {L"Все строчные", L"All lowercase"},
    {L"Общие", L"General"},
    {L"Запускать при входе в Windows", L"Launch when you sign in to Windows"},
    {L"Выключено в Диспетчере задач на вкладке «Автозагрузка»", L"Turned off on the Startup tab of Task Manager"},
    {L"Проверять обновления при запуске", L"Check for updates at launch"},
    // Shortcut field
    {L"Не задано", L"Not set"},
    {L"не задано", L"not set"},
    {L"Введите сочетание…", L"Type a combination…"},
    {L"запись сочетания", L"recording a combination"},
    {L"Пробел", L"Space"},
    // First run
    {L"Добро пожаловать в Клац", L"Welcome to Klats"},
    {L"Попробуйте", L"Try it"},
    {L"Выделите текст ниже, нажмите {} и отпустите.", L"Select the text below, press {} and let go."},
    {L"Должно получиться «привет мир», а раскладка переключится на русскую.",
     L"You should get «привет мир», and the layout switches to Russian."},
    {L"Где живёт Клац", L"Where Klats lives"},
    {L"Значок Клаца — в области уведомлений, рядом с часами. Если его не видно, нажмите стрелку ^ и перетащите значок на "
     L"панель задач: так он всегда будет под рукой.",
     L"The Klats icon is in the notification area, next to the clock. If you cannot see it, click the ^ arrow and drag the "
     L"icon onto the taskbar: that way it is always at hand."},
    {L"Параметры панели задач", L"Taskbar settings"},
    {L"Запускать Клац при входе в Windows", L"Launch Klats when you sign in to Windows"},
    {L"В окнах программ, запущенных от имени администратора, Клац не работает: так Windows их защищает.",
     L"Klats does not work in windows of programs run as administrator: that is how Windows protects them."},
    {L"Готово", L"Done"},
    // Update offer
    {L"Вышел Клац {}", L"Klats {} is out"},
    {L"У вас версия {}. Скачайте установщик и запустите его: он закроет Клац, поставит новую версию поверх старой и запустит "
     L"её. Windows снова попросит подтвердить запуск, как при установке. Настройки сохранятся.",
     L"You have version {}. Download the installer and run it: it closes Klats, installs the new version over the old one and "
     L"starts it. Windows asks you to confirm the launch again, as during installation. Your settings stay."},
    {L"Скачать", L"Download"},
    {L"Не сейчас", L"Not now"},
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

std::wstring tr(const wchar_t* russian, const std::wstring& argument) {
    std::wstring text = tr(russian);
    size_t at = text.find(L"{}");
    if (at != std::wstring::npos) text.replace(at, 2, argument);
    return text;
}

}  // namespace klats::app
