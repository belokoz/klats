import Foundation
import Testing
import KlatsCore

@Suite("Модель хоткея")
struct HotkeyTests {
    @Test("Сохраняется и читается без потерь", arguments: [
        Hotkey(modifiers: [.option, .command]),
        Hotkey(keyCode: 6, modifiers: [.option, .command]),
        Hotkey(keyCode: 49, modifiers: [.control]),
    ])
    func roundTrip(hotkey: Hotkey) throws {
        let data = try JSONEncoder().encode(hotkey)
        #expect(try JSONDecoder().decode(Hotkey.self, from: data) == hotkey)
    }

    @Test("Без клавиши — это аккорд из модификаторов")
    func chord() {
        #expect(Hotkey(modifiers: [.option, .command]).isChord)
        #expect(!Hotkey(keyCode: 6, modifiers: [.option, .command]).isChord)
    }
}
