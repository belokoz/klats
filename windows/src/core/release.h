#pragma once
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace klats {

// A version number such as 0.1.1. A leading «v» is dropped. Missing parts count as zero, so 0.2
// and 0.2.0 are the same version.
class AppVersion {
public:
    // Only digits and dots: «0.2.0-beta» or «1.+2» is not a release number.
    static std::optional<AppVersion> parse(std::wstring_view text);

    const std::vector<uint64_t>& parts() const { return parts_; }
    std::wstring toString() const;

    friend std::strong_ordering operator<=>(const AppVersion& a, const AppVersion& b);
    friend bool operator==(const AppVersion& a, const AppVersion& b) { return (a <=> b) == 0; }

private:
    std::vector<uint64_t> parts_;
};

// The newest Windows release, read from GitHub's releases.atom feed of the repository.
struct WindowsRelease {
    AppVersion version;
    std::wstring downloadURL;  // the installer itself
};

// One page of GitHub's releases.atom: at most the 10 newest entries of the repository, Mac and
// Windows releases and test builds alike.
struct FeedPage {
    std::vector<WindowsRelease> releases;  // stable Windows releases on the page, newest version first
    size_t entries = 0;                    // every entry on the page
    std::string firstTag;                  // to notice a page that repeats
    std::string lastTag;                   // for the next page: releases.atom?after=<lastTag>
};

// Releases of this repository have Atom entry ids «tag:github.com,2008:Repository/<id>/<tag>».
// Windows releases are tagged «windows-vX.Y.Z» with digits and dots only; test builds carry a
// suffix and are skipped. A malformed or oversized feed gives nothing. `arch` names the installer:
// Klats-X.Y.Z-windows-<arch>.exe.
//
// A tag without a published release (deleted, or pushed before its files) looks exactly like a
// release in the feed, so the caller checks that the installer is really there before offering it.
std::optional<FeedPage> readReleaseFeed(std::string_view atom, std::string_view repositoryID, std::string_view arch);

// The first candidate of the page, for the simple cases.
std::optional<WindowsRelease> newestWindowsRelease(std::string_view atom, std::string_view repositoryID,
                                                   std::string_view arch);

// The link is opened in the browser, so it has to lead to GitHub over HTTPS and nowhere else.
bool isGitHubLink(std::wstring_view url);

}  // namespace klats
