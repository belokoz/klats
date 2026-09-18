import Testing
import KlatsCore

@Suite("Смена регистра")
struct CaseConverterTests {
    @Test("Инвертирование", arguments: [
        ("пРИВЕТ", "Привет"),
        ("Hello World", "hELLO wORLD"),
        ("ПРИВЕТ", "привет"),
        ("123 — 🙂!", "123 — 🙂!"),
        ("", ""),
    ])
    func invert(input: String, expected: String) {
        #expect(CaseConverter.convert(input, mode: .invert) == expected)
    }

    @Test("Все строчные", arguments: [
        ("ПРИВЕТ Мир", "привет мир"),
        ("пРИВЕТ", "привет"),
        ("already lower", "already lower"),
        ("123 — 🙂!", "123 — 🙂!"),
    ])
    func lowercase(input: String, expected: String) {
        #expect(CaseConverter.convert(input, mode: .lowercase) == expected)
    }

    @Test("Двойное инвертирование возвращает исходный текст")
    func invertTwice() {
        let text = "Съешь ЕЩЁ этих Мягких french Булок"
        #expect(CaseConverter.convert(CaseConverter.convert(text, mode: .invert), mode: .invert) == text)
    }

    @Test("Буква, которая при смене регистра становится двумя")
    func expandingCharacter() {
        #expect(CaseConverter.convert("straße", mode: .invert) == "STRASSE")
    }
}
