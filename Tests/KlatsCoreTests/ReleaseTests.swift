import Foundation
import Testing
import KlatsCore

@Suite("Номер версии")
struct AppVersionTests {
    @Test("Читается из номера и из тега", arguments: [
        ("0.1.1", "0.1.1"),
        ("v0.2.0", "0.2.0"),
        (" V1.10 ", "1.10"),
        ("2", "2"),
    ])
    func parses(text: String, expected: String) {
        #expect(AppVersion(text)?.description == expected)
    }

    @Test("Не номер версии", arguments: ["", "v", "0.2.0-beta", "1..2", "1.2.", "один", "1.+2", "１.２"])
    func rejects(text: String) {
        #expect(AppVersion(text) == nil)
    }

    @Test("Сравнение по частям, а не как строки", arguments: [
        ("0.1.1", "0.2.0"),
        ("0.1.9", "0.1.10"),
        ("0.9.9", "1.0"),
        ("0.1", "0.1.1"),
    ])
    func older(lower: String, higher: String) throws {
        let a = try #require(AppVersion(lower)), b = try #require(AppVersion(higher))
        #expect(a < b)
        #expect(!(b < a))
    }

    @Test("Недостающие части равны нулю")
    func trailingZeros() throws {
        #expect(try #require(AppVersion("0.2")) == #require(AppVersion("v0.2.0")))
    }
}

@Suite("Последний релиз на GitHub")
struct LatestReleaseTests {
    private func json(tag: String = "v0.2.0", draft: Bool = false, prerelease: Bool = false,
                      assets: String = #"[{"name": "Klats-0.2.0.dmg", "browser_download_url": "https://github.com/belokoz/klats/releases/download/v0.2.0/Klats-0.2.0.dmg"}]"#,
                      page: String = "https://github.com/belokoz/klats/releases/tag/v0.2.0") -> Data {
        Data("""
        {"tag_name": "\(tag)", "name": "Клац", "html_url": "\(page)", "draft": \(draft), "prerelease": \(prerelease),
         "published_at": "2026-10-01T10:00:00Z", "assets": \(assets)}
        """.utf8)
    }

    @Test("Версия и ссылка на DMG")
    func dmgLink() throws {
        let release = try #require(LatestRelease(githubJSON: json()))
        #expect(release.version.description == "0.2.0")
        #expect(release.downloadURL.absoluteString == "https://github.com/belokoz/klats/releases/download/v0.2.0/Klats-0.2.0.dmg")
    }

    @Test("Без DMG ведёт на страницу релиза")
    func pageWhenNoDMG() throws {
        let release = try #require(LatestRelease(githubJSON: json(assets: "[]")))
        #expect(release.downloadURL.absoluteString == "https://github.com/belokoz/klats/releases/tag/v0.2.0")
    }

    @Test("Ссылки не на GitHub не открываются")
    func foreignLinks() throws {
        let foreign = #"[{"name": "Klats-0.2.0.dmg", "browser_download_url": "https://example.com/Klats-0.2.0.dmg"}]"#
        let release = try #require(LatestRelease(githubJSON: json(assets: foreign)))
        #expect(release.downloadURL.host == "github.com")
        #expect(LatestRelease(githubJSON: json(assets: foreign, page: "http://github.com/belokoz/klats")) == nil)
    }

    @Test("Черновики, предварительные выпуски и мусор пропускаются")
    func skipped() {
        #expect(LatestRelease(githubJSON: json(draft: true)) == nil)
        #expect(LatestRelease(githubJSON: json(prerelease: true)) == nil)
        #expect(LatestRelease(githubJSON: json(tag: "nightly")) == nil)
        #expect(LatestRelease(githubJSON: Data(#"{"message": "API rate limit exceeded"}"#.utf8)) == nil)
        #expect(LatestRelease(githubJSON: Data("not json".utf8)) == nil)
    }
}
