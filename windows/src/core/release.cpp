#include "release.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace klats {

namespace {

// Swift's CharacterSet.whitespaces: tab plus the space separators (Unicode category Zs).
bool isSwiftWhitespace(wchar_t c) {
    return c == L'\t' || c == L' ' || c == 0x00A0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x202F || c == 0x205F || c == 0x3000;
}

bool isDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

std::wstring widen(std::string_view ascii) { return std::wstring(ascii.begin(), ascii.end()); }

bool isNameCharacter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
}

bool isName(std::string_view text) {
    if (text.empty() || text == "." || text == "..") return false;
    for (char c : text) {
        if (!isNameCharacter(c)) return false;
    }
    return true;
}

// The text between `open` and `close` inside `text`, if both are there.
std::optional<std::string_view> between(std::string_view text, std::string_view open, std::string_view close) {
    size_t start = text.find(open);
    if (start == std::string_view::npos) return std::nullopt;
    start += open.size();
    size_t end = text.find(close, start);
    if (end == std::string_view::npos) return std::nullopt;
    return text.substr(start, end - start);
}

// The href of the entry's <link rel="alternate" …>, whatever the attribute order.
std::optional<std::string_view> alternateLink(std::string_view entry) {
    for (size_t at = entry.find("<link"); at != std::string_view::npos; at = entry.find("<link", at + 5)) {
        size_t end = entry.find('>', at);
        if (end == std::string_view::npos) return std::nullopt;
        std::string_view tag = entry.substr(at, end - at);
        if (tag.find(" rel=\"alternate\"") == std::string_view::npos) continue;
        return between(tag, " href=\"", "\"");
    }
    return std::nullopt;
}

struct Candidate {
    AppVersion version;
    std::wstring url;
};

// The tag of an entry, when the entry belongs to this repository.
std::optional<std::string_view> entryTag(std::string_view entry, std::string_view repositoryID) {
    auto id = between(entry, "<id>", "</id>");
    if (!id) return std::nullopt;
    std::string prefix = "tag:github.com,2008:Repository/" + std::string(repositoryID) + "/";
    if (id->substr(0, prefix.size()) != prefix) return std::nullopt;
    std::string_view tag = id->substr(prefix.size());
    if (tag.empty()) return std::nullopt;
    return tag;
}

std::optional<Candidate> readEntry(std::string_view entry, std::string_view tag, std::string_view arch) {
    constexpr std::string_view tagPrefix = "windows-v";
    if (tag.substr(0, tagPrefix.size()) != tagPrefix) return std::nullopt;
    std::string_view number = tag.substr(tagPrefix.size());
    for (char c : number) {
        if (!(c >= '0' && c <= '9') && c != '.') return std::nullopt;
    }
    auto version = AppVersion::parse(widen(number));
    if (!version) return std::nullopt;

    // The release page link names the repository as it is called now, after any rename.
    auto link = alternateLink(entry);
    constexpr std::string_view site = "https://github.com/";
    if (!link || link->substr(0, site.size()) != site) return std::nullopt;
    std::string_view path = link->substr(site.size());
    size_t slash = path.find('/');
    if (slash == std::string_view::npos) return std::nullopt;
    std::string_view owner = path.substr(0, slash);
    path.remove_prefix(slash + 1);
    slash = path.find('/');
    if (slash == std::string_view::npos) return std::nullopt;
    std::string_view repository = path.substr(0, slash);
    if (!isName(owner) || !isName(repository)) return std::nullopt;
    if (path.substr(slash) != "/releases/tag/" + std::string(tag)) return std::nullopt;

    std::wstring url = widen(site) + widen(owner) + L"/" + widen(repository) + L"/releases/download/" + widen(tag) +
                       L"/Klats-" + widen(number) + L"-windows-" + widen(arch) + L".exe";
    if (!isGitHubLink(url)) return std::nullopt;
    return Candidate{*version, std::move(url)};
}

}  // namespace

std::optional<AppVersion> AppVersion::parse(std::wstring_view text) {
    while (!text.empty() && isSwiftWhitespace(text.front())) text.remove_prefix(1);
    while (!text.empty() && isSwiftWhitespace(text.back())) text.remove_suffix(1);
    if (!text.empty() && (text.front() == L'v' || text.front() == L'V')) text.remove_prefix(1);

    AppVersion version;
    while (true) {
        size_t dot = text.find(L'.');
        std::wstring_view piece = text.substr(0, dot);
        if (piece.empty()) return std::nullopt;
        uint64_t number = 0;
        for (wchar_t c : piece) {
            if (!isDigit(c)) return std::nullopt;
            // Swift's Int(_:) refuses anything past Int.max.
            uint64_t digit = static_cast<uint64_t>(c - L'0');
            if (number > (static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) - digit) / 10) return std::nullopt;
            number = number * 10 + digit;
        }
        version.parts_.push_back(number);
        if (dot == std::wstring_view::npos) break;
        text.remove_prefix(dot + 1);
    }
    return version;
}

std::wstring AppVersion::toString() const {
    std::wstring text;
    for (size_t i = 0; i < parts_.size(); ++i) {
        if (i) text += L'.';
        text += std::to_wstring(parts_[i]);
    }
    return text;
}

std::strong_ordering operator<=>(const AppVersion& a, const AppVersion& b) {
    size_t count = std::max(a.parts_.size(), b.parts_.size());
    for (size_t i = 0; i < count; ++i) {
        uint64_t x = i < a.parts_.size() ? a.parts_[i] : 0;
        uint64_t y = i < b.parts_.size() ? b.parts_[i] : 0;
        if (x != y) return x <=> y;
    }
    return std::strong_ordering::equal;
}

std::optional<FeedPage> readReleaseFeed(std::string_view atom, std::string_view repositoryID, std::string_view arch) {
    constexpr size_t maxFeed = 1 << 20;
    if (atom.size() > maxFeed || repositoryID.empty() || arch.empty()) return std::nullopt;
    if (atom.find("<feed") == std::string_view::npos) return std::nullopt;
    for (char c : repositoryID) {
        if (c < '0' || c > '9') return std::nullopt;
    }
    for (char c : arch) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return std::nullopt;
    }

    FeedPage page;
    std::vector<Candidate> candidates;
    for (size_t start = atom.find("<entry>"); start != std::string_view::npos; start = atom.find("<entry>", start)) {
        size_t end = atom.find("</entry>", start);
        if (end == std::string_view::npos) break;
        std::string_view entry = atom.substr(start, end - start);
        ++page.entries;
        if (auto tag = entryTag(entry, repositoryID)) {
            if (page.firstTag.empty()) page.firstTag = std::string(*tag);
            page.lastTag = std::string(*tag);
            if (auto candidate = readEntry(entry, *tag, arch)) candidates.push_back(std::move(*candidate));
        }
        start = end;
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return b.version < a.version; });
    for (auto& candidate : candidates) {
        if (!page.releases.empty() && page.releases.back().version == candidate.version) continue;
        page.releases.push_back({std::move(candidate.version), std::move(candidate.url)});
    }
    return page;
}

std::optional<WindowsRelease> newestWindowsRelease(std::string_view atom, std::string_view repositoryID,
                                                   std::string_view arch) {
    auto page = readReleaseFeed(atom, repositoryID, arch);
    if (!page || page->releases.empty()) return std::nullopt;
    return page->releases.front();
}

bool isGitHubLink(std::wstring_view url) {
    constexpr std::wstring_view scheme = L"https://";
    if (url.substr(0, scheme.size()) != scheme) return false;
    url.remove_prefix(scheme.size());

    size_t hostEnd = url.find_first_of(L"/?#");
    std::wstring_view host = url.substr(0, hostEnd);
    if (host.empty() || host.front() == L'.' || host.back() == L'.') return false;
    for (wchar_t c : host) {
        // No user info, no port, no backslash, no uppercase: one spelling of the host only.
        if (!((c >= L'a' && c <= L'z') || isDigit(c) || c == L'.' || c == L'-')) return false;
    }
    if (host.find(L"..") != std::wstring_view::npos) return false;
    auto endsWith = [&](std::wstring_view suffix) {
        return host.size() > suffix.size() && host.substr(host.size() - suffix.size()) == suffix;
    };
    if (host != L"github.com" && !endsWith(L".github.com") && !endsWith(L".githubusercontent.com")) return false;

    if (hostEnd == std::wstring_view::npos) return true;
    constexpr std::wstring_view allowed = L"-._~:/?#[]@!$&'()*+,;=%";
    for (wchar_t c : url.substr(hostEnd)) {
        bool plain = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || isDigit(c);
        if (!plain && allowed.find(c) == std::wstring_view::npos) return false;
    }
    return true;
}

}  // namespace klats
