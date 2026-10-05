#include "test.h"

#include "core/release.h"

#include <string>
#include <vector>

using klats::AppVersion;

TEST("Номер версии: читается из номера и из тега") {
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"0.1.1", L"0.1.1"},
        {L"v0.2.0", L"0.2.0"},
        {L" V1.10 ", L"1.10"},
        {L"2", L"2"},
    };
    for (auto [text, expected] : cases) {
        auto version = AppVersion::parse(text);
        CHECK(version.has_value());
        if (version) CHECK_EQ(version->toString(), std::wstring(expected));
    }
}

TEST("Номер версии: не номер версии") {
    for (const wchar_t* text : {L"", L"v", L"0.2.0-beta", L"1..2", L"1.2.", L"один", L"1.+2", L"１.２",
                                L"99999999999999999999", L"1.2\n"}) {
        CHECK(!AppVersion::parse(text).has_value());
    }
}

TEST("Номер версии: сравнение по частям, а не как строки") {
    const std::pair<const wchar_t*, const wchar_t*> cases[] = {
        {L"0.1.1", L"0.2.0"},
        {L"0.1.9", L"0.1.10"},
        {L"0.9.9", L"1.0"},
        {L"0.1", L"0.1.1"},
    };
    for (auto [lower, higher] : cases) {
        auto a = AppVersion::parse(lower);
        auto b = AppVersion::parse(higher);
        CHECK(a && b);
        if (a && b) {
            CHECK(*a < *b);
            CHECK(!(*b < *a));
        }
    }
}

TEST("Номер версии: недостающие части равны нулю") {
    auto a = AppVersion::parse(L"0.2");
    auto b = AppVersion::parse(L"v0.2.0");
    CHECK(a && b && *a == *b);
}

namespace {

const char* kRepository = "1375942669";

std::string entry(const std::string& tag, const std::string& link = {}, const std::string& repository = kRepository) {
    std::string href = link.empty() ? "https://github.com/belokoz/klats/releases/tag/" + tag : link;
    return "  <entry>\n"
           "    <id>tag:github.com,2008:Repository/" + repository + "/" + tag + "</id>\n"
           "    <updated>2026-10-05T10:00:00Z</updated>\n"
           "    <link rel=\"alternate\" type=\"text/html\" href=\"" + href + "\"/>\n"
           "    <title>Клац</title>\n"
           "    <content type=\"html\">&lt;p&gt;Что нового&lt;/p&gt;</content>\n"
           "  </entry>\n";
}

std::string feed(const std::vector<std::string>& entries) {
    std::string text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\" xml:lang=\"en-US\">\n"
        "  <id>tag:github.com,2008:https://github.com/belokoz/klats/releases</id>\n"
        "  <link type=\"text/html\" rel=\"alternate\" href=\"https://github.com/belokoz/klats/releases\"/>\n"
        "  <title>Release notes from klats</title>\n";
    for (const auto& e : entries) text += e;
    return text + "</feed>\n";
}

std::optional<klats::WindowsRelease> newest(const std::string& atom, const char* arch = "x64") {
    return klats::newestWindowsRelease(atom, kRepository, arch);
}

}  // namespace

TEST("Лента релизов: свежий Windows-релиз и ссылка на установщик") {
    auto release = newest(feed({entry("v0.3.0"), entry("windows-v0.1.1"), entry("windows-v0.1.0"), entry("v0.2.0")}));
    CHECK(release.has_value());
    if (release) {
        CHECK_EQ(release->version.toString(), std::wstring(L"0.1.1"));
        CHECK_EQ(release->downloadURL,
                 std::wstring(L"https://github.com/belokoz/klats/releases/download/windows-v0.1.1/Klats-0.1.1-windows-x64.exe"));
    }
}

TEST("Лента релизов: самая новая версия, даже если в ленте она не первая") {
    auto release = newest(feed({entry("windows-v0.1.9"), entry("windows-v0.1.10"), entry("windows-v0.1.2")}));
    CHECK(release && release->version.toString() == L"0.1.10");
}

TEST("Лента релизов: только мак-релизы — предлагать нечего") {
    CHECK(!newest(feed({entry("v0.2.0"), entry("v0.1.0")})).has_value());
}

TEST("Лента релизов: пробные сборки и чужие теги пропускаются") {
    CHECK(!newest(feed({entry("windows-v0.2.0-beta"), entry("windows-v"), entry("windows-v1..2"), entry("windows-v 1.0"),
                        entry("windows-0.1.0"), entry("nightly")}))
               .has_value());
}

TEST("Лента релизов: записи другого репозитория не считаются") {
    CHECK(!newest(feed({entry("windows-v9.0.0", "", "1")})).has_value());
}

TEST("Лента релизов: ссылка ведёт только на GitHub и только на этот тег") {
    CHECK(!newest(feed({entry("windows-v1.0.0", "https://example.com/belokoz/klats/releases/tag/windows-v1.0.0")})).has_value());
    CHECK(!newest(feed({entry("windows-v1.0.0", "http://github.com/belokoz/klats/releases/tag/windows-v1.0.0")})).has_value());
    CHECK(!newest(feed({entry("windows-v1.0.0", "https://github.com/belokoz/klats/releases/tag/windows-v2.0.0")})).has_value());
    CHECK(!newest(feed({entry("windows-v1.0.0", "https://github.com/a&b/klats/releases/tag/windows-v1.0.0")})).has_value());
}

TEST("Лента релизов: репозиторий переименовали — ссылка идёт по новому имени") {
    auto release = newest(feed({entry("windows-v1.0.0", "https://github.com/someone/klats-app/releases/tag/windows-v1.0.0")}));
    CHECK(release && release->downloadURL ==
                         L"https://github.com/someone/klats-app/releases/download/windows-v1.0.0/Klats-1.0.0-windows-x64.exe");
}

TEST("Лента релизов: порядок атрибутов ссылки не важен") {
    std::string e = entry("windows-v1.0.0");
    std::string from = "<link rel=\"alternate\" type=\"text/html\" href=\"https://github.com/belokoz/klats/releases/tag/windows-v1.0.0\"/>";
    std::string to = "<link href=\"https://github.com/belokoz/klats/releases/tag/windows-v1.0.0\" type=\"text/html\" rel=\"alternate\"/>";
    e.replace(e.find(from), from.size(), to);
    CHECK(newest(feed({e})).has_value());
}

TEST("Лента релизов: установщик для своей архитектуры") {
    auto release = newest(feed({entry("windows-v1.0.0")}), "arm64");
    CHECK(release && release->downloadURL ==
                         L"https://github.com/belokoz/klats/releases/download/windows-v1.0.0/Klats-1.0.0-windows-arm64.exe");
    CHECK(!newest(feed({entry("windows-v1.0.0")}), "x64/../evil").has_value());
}

TEST("Лента релизов: все Windows-релизы страницы, от новых к старым, и метки для следующей страницы") {
    auto page = klats::readReleaseFeed(feed({entry("v0.4.0"), entry("windows-v0.2.0"), entry("windows-v0.3.0-beta"),
                                              entry("windows-v0.1.0"), entry("windows-v0.2.1")}),
                                         kRepository, "x64");
    CHECK(page.has_value());
    if (page) {
        CHECK_EQ(page->entries, 5u);
        CHECK_EQ(page->firstTag, std::string("v0.4.0"));
        CHECK_EQ(page->lastTag, std::string("windows-v0.2.1"));
        CHECK_EQ(page->releases.size(), 3u);
        if (page->releases.size() == 3) {
            CHECK_EQ(page->releases[0].version.toString(), std::wstring(L"0.2.1"));
            CHECK_EQ(page->releases[1].version.toString(), std::wstring(L"0.2.0"));
            CHECK_EQ(page->releases[2].version.toString(), std::wstring(L"0.1.0"));
        }
    }
}

TEST("Лента релизов: страница без Windows-релизов — повод заглянуть дальше") {
    auto page = klats::readReleaseFeed(feed({entry("v0.4.0"), entry("windows-v0.3.0-beta")}), kRepository, "x64");
    CHECK(page && page->releases.empty() && page->entries == 2 && page->lastTag == "windows-v0.3.0-beta");
    auto empty = klats::readReleaseFeed(feed({}), kRepository, "x64");
    CHECK(empty && empty->entries == 0 && empty->releases.empty());
}

TEST("Лента релизов: мусор и слишком большой ответ") {
    CHECK(!newest("").has_value());
    CHECK(!newest("not a feed").has_value());
    CHECK(!newest("<entry><id>tag:github.com,2008:Repository/1375942669/windows-v1.0.0</id>").has_value());
    std::string huge = feed({entry("windows-v1.0.0")}) + std::string(2 << 20, ' ');
    CHECK(!newest(huge).has_value());
}

TEST("Ссылки: открываются только GitHub по HTTPS") {
    for (const wchar_t* url : {L"https://github.com/belokoz/klats/releases/download/windows-v1.0.0/Klats-1.0.0-windows-x64.exe",
                               L"https://objects.githubusercontent.com/x", L"https://release-assets.githubusercontent.com/a?b=c&d=%20",
                               L"https://github.com"}) {
        CHECK(klats::isGitHubLink(url));
    }
    for (const wchar_t* url : {L"http://github.com/x", L"HTTPS://github.com/x", L"https://GitHub.com/x", L"https://github.com.evil.com/x",
                               L"https://github.com@evil.com/x", L"https://evil.com\\@github.com/x", L"https://evil.com#@github.com",
                               L"https://github.com:443/x", L"https://github.com./x", L"https://evilgithub.com/x",
                               L"https://ｇithub.com/x", L"https://github.com/a b", L"https://github.com/a\nb", L"https://.github.com/x",
                               L"https://", L"github.com/x"}) {
        CHECK(!klats::isGitHubLink(url));
    }
}
