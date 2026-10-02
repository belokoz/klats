import Foundation

/// A version number such as 0.1.1. Release tags carry a leading «v», which is dropped.
/// Missing parts count as zero, so 0.2 and 0.2.0 are the same version.
public struct AppVersion: Comparable, Sendable, CustomStringConvertible {
    public let parts: [Int]

    public init?(_ text: String) {
        var text = Substring(text.trimmingCharacters(in: .whitespaces))
        if text.first == "v" || text.first == "V" { text = text.dropFirst() }
        var parts: [Int] = []
        for piece in text.split(separator: ".", omittingEmptySubsequences: false) {
            // Only plain digits: «0.2.0-beta» or «1.+2» is not a release number.
            guard !piece.isEmpty, piece.allSatisfy(\.isASCII), piece.allSatisfy(\.isNumber), let number = Int(piece) else {
                return nil
            }
            parts.append(number)
        }
        self.parts = parts
    }

    public var description: String { parts.map(String.init).joined(separator: ".") }

    public static func == (a: AppVersion, b: AppVersion) -> Bool { compare(a, b) == 0 }
    public static func < (a: AppVersion, b: AppVersion) -> Bool { compare(a, b) < 0 }

    private static func compare(_ a: AppVersion, _ b: AppVersion) -> Int {
        for index in 0..<max(a.parts.count, b.parts.count) {
            let x = index < a.parts.count ? a.parts[index] : 0
            let y = index < b.parts.count ? b.parts[index] : 0
            if x != y { return x < y ? -1 : 1 }
        }
        return 0
    }
}

/// The newest published release, read from GitHub's «latest release» answer.
public struct LatestRelease: Sendable {
    public let version: AppVersion
    /// The DMG itself when the release has one, otherwise the release page.
    public let downloadURL: URL

    /// Nil for anything that is not a published release with a version number and a GitHub link.
    public init?(githubJSON data: Data) {
        struct Asset: Decodable {
            let name: String
            let browser_download_url: URL
        }
        struct Release: Decodable {
            let tag_name: String
            let html_url: URL
            let draft: Bool?
            let prerelease: Bool?
            let assets: [Asset]?
        }
        guard let release = try? JSONDecoder().decode(Release.self, from: data),
              release.draft != true, release.prerelease != true,
              let version = AppVersion(release.tag_name)
        else { return nil }

        let dmg = release.assets?.first { $0.name.lowercased().hasSuffix(".dmg") }?.browser_download_url
        guard let url = [dmg, release.html_url].compactMap({ $0 }).first(where: Self.isGitHubLink) else { return nil }
        self.version = version
        downloadURL = url
    }

    public init(version: AppVersion, downloadURL: URL) {
        self.version = version
        self.downloadURL = downloadURL
    }

    /// The link is opened in the browser, so it has to lead to GitHub over HTTPS and nowhere else.
    private static func isGitHubLink(_ url: URL) -> Bool {
        guard url.scheme == "https", let host = url.host?.lowercased() else { return false }
        return host == "github.com" || host.hasSuffix(".github.com") || host.hasSuffix(".githubusercontent.com")
    }
}
