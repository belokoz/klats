import Testing
import KlatsCore

@Suite("Хоткей из одних модификаторов")
struct ChordDetectorTests {
    let chord: Modifiers = [.option, .command]

    /// Feeds a script of events and returns how many times the chord fired.
    private enum Step {
        case flags(Modifiers, Double)
        case input
    }

    private func fires(_ steps: [Step], maxHold: Double = 1.0) -> Int {
        var detector = ChordDetector(chord: chord, maxHold: maxHold)
        var count = 0
        for step in steps {
            switch step {
            case .flags(let modifiers, let time):
                if detector.flagsChanged(modifiers, at: time) { count += 1 }
            case .input:
                detector.otherInput()
            }
        }
        return count
    }

    @Test("Нажал и отпустил: срабатывает один раз")
    func pressAndRelease() {
        #expect(fires([.flags([.option], 0), .flags(chord, 0.05), .flags([.command], 0.2), .flags([], 0.25)]) == 1)
    }

    @Test("Порядок нажатия клавиш не важен")
    func eitherOrder() {
        #expect(fires([.flags([.command], 0), .flags(chord, 0.05), .flags([.option], 0.2), .flags([], 0.25)]) == 1)
    }

    @Test("Обе клавиши отпущены одновременно")
    func releasedTogether() {
        #expect(fires([.flags([.option], 0), .flags(chord, 0.05), .flags([], 0.2)]) == 1)
    }

    @Test("Клавиша между нажатием и отпусканием отменяет: ⌥⌘Esc остаётся ⌥⌘Esc")
    func keyWhileHeldCancels() {
        #expect(fires([.flags([.option], 0), .flags(chord, 0.05), .input, .flags([.command], 0.3), .flags([], 0.35)]) == 0)
    }

    @Test("Клавиша до второго модификатора тоже отменяет: ⌘C, потом ⌥")
    func keyBeforeSecondModifierCancels() {
        #expect(fires([.flags([.command], 0), .input, .flags(chord, 0.3), .flags([.command], 0.4), .flags([], 0.5)]) == 0)
    }

    @Test("Лишний модификатор отменяет")
    func extraModifierCancels() {
        let steps: [Step] = [.flags([.option], 0), .flags(chord, 0.05), .flags([.option, .command, .shift], 0.1),
                             .flags(chord, 0.2), .flags([.option], 0.3), .flags([], 0.35)]
        #expect(fires(steps) == 0)
    }

    @Test("Приход из большего сочетания не считается")
    func arrivingFromSupersetDoesNotArm() {
        let steps: [Step] = [.flags([.shift], 0), .flags([.shift, .option], 0.05), .flags([.shift, .option, .command], 0.1),
                             .flags(chord, 0.2), .flags([], 0.3)]
        #expect(fires(steps) == 0)
    }

    @Test("Слишком долгое удержание не считается")
    func heldTooLong() {
        #expect(fires([.flags([.option], 0), .flags(chord, 0.05), .flags([], 1.5)]) == 0)
    }

    @Test("Обычный набор текста ничего не ломает")
    func typingDoesNotBlock() {
        #expect(fires([.input, .input, .flags([.option], 1), .flags(chord, 1.05), .flags([], 1.2)]) == 1)
    }

    @Test("После отменённого нажатия следующее чистое срабатывает")
    func recoversAfterBlocked() {
        let steps: [Step] = [.flags([.option], 0), .flags(chord, 0.05), .input, .flags([], 0.3),
                             .flags([.command], 1), .flags(chord, 1.05), .flags([], 1.2)]
        #expect(fires(steps) == 1)
    }

    @Test("Два нажатия подряд: два срабатывания")
    func twoPresses() {
        let steps: [Step] = [.flags([.option], 0), .flags(chord, 0.05), .flags([], 0.2),
                             .flags([.option], 1), .flags(chord, 1.05), .flags([], 1.2)]
        #expect(fires(steps) == 2)
    }

    @Test("Отпустил одну, снова нажал, не отпуская вторую: второй раз не срабатывает")
    func noRetriggerWithoutFullRelease() {
        let steps: [Step] = [.flags([.command], 0), .flags(chord, 0.05), .flags([.command], 0.2),
                             .flags(chord, 0.3), .flags([.command], 0.4), .flags([], 0.5)]
        #expect(fires(steps) == 1)
    }
}
