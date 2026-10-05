#include "test.h"

#include "core/case_converter.h"
#include "core/layout_converter.h"
#include "core/layout_pair.h"
#include "fixtures/layouts.h"

#include <chrono>

using klats::PairChoice;

namespace {

std::vector<const klats::LayoutTable*> tables(std::initializer_list<const klats::LayoutTable*> list) { return list; }

}  // namespace

TEST("Пара раскладок: две разные раскладки берутся сами") {
    auto choice = klats::choosePair({}, tables({&fixtures::us(), &fixtures::russian()}));
    CHECK(choice.problem == PairChoice::Problem::None);
    CHECK_EQ(choice.first, 0u);
    CHECK_EQ(choice.second, 1u);
}

TEST("Пара раскладок: из трёх автоматически не выбирается") {
    auto choice = klats::choosePair({}, tables({&fixtures::us(), &fixtures::german(), &fixtures::russian()}));
    CHECK(choice.problem == PairChoice::Problem::Ambiguous);
}

TEST("Пара раскладок: выбранная пользователем пара важнее") {
    std::vector<std::wstring> chosen = {fixtures::russian().id(), fixtures::us().id()};
    auto choice = klats::choosePair(chosen, tables({&fixtures::us(), &fixtures::german(), &fixtures::russian()}));
    CHECK(choice.problem == PairChoice::Problem::None);
    CHECK_EQ(choice.first, 2u);
    CHECK_EQ(choice.second, 0u);
}

TEST("Пара раскладок: выбранная пара с выключенной раскладкой не годится") {
    std::vector<std::wstring> chosen = {fixtures::german().id(), fixtures::us().id()};
    auto choice = klats::choosePair(chosen, tables({&fixtures::us(), &fixtures::russian()}));
    CHECK(choice.problem == PairChoice::Problem::None);
    CHECK_EQ(choice.first, 0u);
    CHECK_EQ(choice.second, 1u);
}

TEST("Пара раскладок: одна и та же клавиатура под двумя языками считается одной") {
    klats::LayoutTable usUnderRussian(L"0419:00000409", {fixtures::us().forward().begin(), fixtures::us().forward().end()});
    auto choice = klats::choosePair({}, tables({&fixtures::us(), &usUnderRussian, &fixtures::russian()}));
    CHECK(choice.problem == PairChoice::Problem::None);
    CHECK_EQ(choice.first, 0u);
    CHECK_EQ(choice.second, 2u);
    CHECK(klats::choosePair({}, tables({&fixtures::us(), &usUnderRussian})).problem == PairChoice::Problem::NeedTwo);
}

TEST("Пара раскладок: одной раскладки мало") {
    CHECK(klats::choosePair({}, tables({&fixtures::us()})).problem == PairChoice::Problem::NeedTwo);
    CHECK(klats::choosePair({}, {}).problem == PairChoice::Problem::NeedTwo);
}

TEST("Конвертация: огромный кластер комбинирующих знаков не подвешивает Клац") {
    // One base letter with 40 000 combining marks of alternating classes: normalising it costs
    // quadratic time, so such clusters are passed through without a lookup.
    std::wstring bomb = L"g";
    for (int i = 0; i < 40000; ++i) bomb += (i % 2) ? L'̖' : L'́';
    auto start = std::chrono::steady_clock::now();
    auto result = klats::convertLayout(bomb + L" ghbdtn", fixtures::us(), fixtures::russian());
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    CHECK(result.text.size() == bomb.size() + 7);
    CHECK(result.text.substr(bomb.size()) == L" привет");
    CHECK(elapsed.count() < 500);

    start = std::chrono::steady_clock::now();
    std::wstring inverted = klats::convertCase(bomb, klats::CaseMode::Invert);
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    CHECK(inverted.size() == bomb.size());
    CHECK(inverted[0] == L'G');
    CHECK(elapsed.count() < 500);
}
