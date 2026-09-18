import Testing
import KlatsCore

@Suite("Конвертация раскладки")
struct LayoutConverterTests {
    let abc = Fixtures.abc
    let ruPC = Fixtures.russianPC
    let ruMac = Fixtures.russian

    @Test("Латиница вместо кириллицы", arguments: [
        ("ghbdtn", "привет"),
        ("Ghbdtn? rfr ltkf&", "Привет, как дела?"),
        ("GHBDTN", "ПРИВЕТ"),
        ("z nt,z k.,k.", "я тебя люблю"),
        // On an ISO keyboard «ё» lives on the top-left key, which types «§» in ABC.
        ("§=]", "ё=ъ"),
    ])
    func latinToCyrillic(input: String, expected: String) {
        let result = LayoutConverter.convert(input, a: abc, b: ruPC)
        #expect(result.text == expected)
        #expect(result.direction == .aToB)
    }

    @Test("Кириллица вместо латиницы", arguments: [
        ("руддщ", "hello"),
        ("Привет, как дела?", "Ghbdtn? rfr ltkf&"),
        ("ЦЩКВ", "WORD"),
    ])
    func cyrillicToLatin(input: String, expected: String) {
        let result = LayoutConverter.convert(input, a: abc, b: ruPC)
        #expect(result.text == expected)
        #expect(result.direction == .bToA)
    }

    @Test("Туда и обратно даёт исходный текст", arguments: ["ghbdtn", "Ghbdtn? rfr ltkf&", "руддщ цщкдв", "Ntcn 123!"])
    func roundTrip(input: String) {
        let once = LayoutConverter.convert(input, a: abc, b: ruPC).text
        let twice = LayoutConverter.convert(once, a: abc, b: ruPC).text
        #expect(twice == input)
    }

    @Test("Незнакомые символы проходят как есть")
    func unknownCharactersPassThrough() {
        let result = LayoutConverter.convert("Ntcn 123 — ok 🙂\n\tend", a: abc, b: ruPC)
        #expect(result.text == "Тест 123 — щл 🙂\n\tутв")
    }

    @Test("Порядок раскладок в паре не влияет на результат")
    func argumentOrderDoesNotMatter() {
        #expect(LayoutConverter.convert("ghbdtn", a: ruPC, b: abc).text == "привет")
        #expect(LayoutConverter.convert("руддщ", a: ruPC, b: abc).text == "hello")
    }

    @Test("При ничьей решает текущая раскладка")
    func tieBreakDecides() {
        // Digits live on the same keys in both layouts: nothing to vote with, nothing to change.
        #expect(LayoutConverter.convert("123", a: abc, b: ruPC, tieBreak: .aToB).text == "123")
        #expect(LayoutConverter.convert("123", a: abc, b: ruPC, tieBreak: .bToA).text == "123")
        // A lone full stop exists in both layouts on different keys.
        #expect(LayoutConverter.convert(".", a: abc, b: ruPC, tieBreak: .aToB).text == "ю")
        #expect(LayoutConverter.convert(".", a: abc, b: ruPC, tieBreak: .bToA).text == "/")
    }

    @Test("Разложенные символы Юникода распознаются")
    func decomposedCharacters() {
        let decomposed = "и\u{0306}"  // «й» as a base letter plus a combining breve
        #expect(LayoutConverter.convert(decomposed, a: abc, b: ruPC).text == "q")
    }

    @Test("Работает и с маковской русской раскладкой")
    func macRussianLayout() {
        #expect(LayoutConverter.convert("ghbdtn", a: abc, b: ruMac).text == "привет")
        #expect(LayoutConverter.convert("руддщ", a: abc, b: ruMac).text == "hello")
    }

    @Test("Пустая строка остаётся пустой")
    func emptyString() {
        #expect(LayoutConverter.convert("", a: abc, b: ruPC).text == "")
    }
}
