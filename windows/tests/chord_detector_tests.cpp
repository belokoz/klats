#include "test.h"

#include "core/chord_detector.h"

#include <variant>
#include <vector>

using klats::ChordDetector;
using klats::Modifier;
using klats::Modifiers;

namespace {

const Modifiers chord{Modifier::Win, Modifier::Alt};
const Modifiers win{Modifier::Win};
const Modifiers alt{Modifier::Alt};
const Modifiers none{};

struct Flags {
    Modifiers modifiers;
    double time;
};
struct Input {};
using Step = std::variant<Flags, Input>;

// Feeds a script of events and returns how many times the chord fired.
int fires(const std::vector<Step>& steps, double maxHold = 1.0) {
    ChordDetector detector(chord, maxHold);
    int count = 0;
    for (const Step& step : steps) {
        if (auto* flags = std::get_if<Flags>(&step)) {
            if (detector.flagsChanged(flags->modifiers, flags->time)) ++count;
        } else {
            detector.otherInput();
        }
    }
    return count;
}

}  // namespace

TEST("Сочетание из модификаторов: нажал и отпустил, срабатывает один раз") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Flags{win, 0.2}, Flags{none, 0.25}}), 1);
}

TEST("Сочетание из модификаторов: порядок нажатия клавиш не важен") {
    CHECK_EQ(fires({Flags{win, 0}, Flags{chord, 0.05}, Flags{alt, 0.2}, Flags{none, 0.25}}), 1);
}

TEST("Сочетание из модификаторов: обе клавиши отпущены одновременно") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Flags{none, 0.2}}), 1);
}

TEST("Сочетание из модификаторов: клавиша между нажатием и отпусканием отменяет, Win+Alt+R остаётся Win+Alt+R") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Input{}, Flags{win, 0.3}, Flags{none, 0.35}}), 0);
}

TEST("Сочетание из модификаторов: клавиша до второго модификатора тоже отменяет") {
    CHECK_EQ(fires({Flags{win, 0}, Input{}, Flags{chord, 0.3}, Flags{win, 0.4}, Flags{none, 0.5}}), 0);
}

TEST("Сочетание из модификаторов: лишний модификатор отменяет") {
    Modifiers withShift{Modifier::Win, Modifier::Alt, Modifier::Shift};
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Flags{withShift, 0.1}, Flags{chord, 0.2}, Flags{alt, 0.3},
                    Flags{none, 0.35}}),
             0);
}

TEST("Сочетание из модификаторов: приход из большего сочетания не считается") {
    Modifiers shift{Modifier::Shift};
    Modifiers shiftAlt{Modifier::Shift, Modifier::Alt};
    Modifiers all{Modifier::Shift, Modifier::Alt, Modifier::Win};
    CHECK_EQ(fires({Flags{shift, 0}, Flags{shiftAlt, 0.05}, Flags{all, 0.1}, Flags{chord, 0.2}, Flags{none, 0.3}}), 0);
}

TEST("Сочетание из модификаторов: слишком долгое удержание не считается") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Flags{none, 1.5}}), 0);
}

TEST("Сочетание из модификаторов: обычный набор текста ничего не ломает") {
    CHECK_EQ(fires({Input{}, Input{}, Flags{alt, 1}, Flags{chord, 1.05}, Flags{none, 1.2}}), 1);
}

TEST("Сочетание из модификаторов: после отменённого нажатия следующее чистое срабатывает") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Input{}, Flags{none, 0.3}, Flags{win, 1}, Flags{chord, 1.05},
                    Flags{none, 1.2}}),
             1);
}

TEST("Сочетание из модификаторов: два нажатия подряд, два срабатывания") {
    CHECK_EQ(fires({Flags{alt, 0}, Flags{chord, 0.05}, Flags{none, 0.2}, Flags{alt, 1}, Flags{chord, 1.05},
                    Flags{none, 1.2}}),
             2);
}

TEST("Сочетание из модификаторов: отпустил одну, снова нажал, не отпуская вторую, второй раз не срабатывает") {
    CHECK_EQ(fires({Flags{win, 0}, Flags{chord, 0.05}, Flags{win, 0.2}, Flags{chord, 0.3}, Flags{win, 0.4},
                    Flags{none, 0.5}}),
             1);
}

TEST("Сочетание из модификаторов: после сброса прежнее нажатие забыто") {
    ChordDetector detector(chord);
    CHECK(!detector.flagsChanged(alt, 0));
    CHECK(!detector.flagsChanged(chord, 0.05));
    detector.reset();
    CHECK(!detector.flagsChanged(none, 0.1));
    CHECK(!detector.flagsChanged(win, 1));
    CHECK(!detector.flagsChanged(chord, 1.05));
    CHECK(detector.flagsChanged(none, 1.1));
}
