import Foundation

/// The two places Klats can send the user. Nothing else in the app touches the network.
enum Links {
    static let repo = URL(string: "https://github.com/belokoz/klats")!

    /// Диктуй is the author's other app: voice to text. The campaign tag names the screen the
    /// visit came from, so the site can tell them apart.
    static func diktuy(from campaign: String) -> URL {
        URL(string: "https://diktuy.ru/?utm_source=klats&utm_medium=app&utm_campaign=\(campaign)")!
    }

    static var version: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "dev"
    }
}
