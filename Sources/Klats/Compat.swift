import Foundation

/// Klats runs on macOS 12 and newer. The few places where macOS 12 needs code of its own ask here.
enum Compat {
    /// `--debug-macos12` sends a newer system down the macOS 12 paths too: the settings layout,
    /// the texts, «launch at login». That way they can be seen and checked on the development Mac.
    static let simulatesMacOS12 = CommandLine.arguments.contains("--debug-macos12")

    static var isMacOS12: Bool {
        if #available(macOS 13, *) { return simulatesMacOS12 }
        return true
    }
}

// `Duration`, `ContinuousClock` and `Task.sleep(for:)` need macOS 13, so time is counted in
// plain milliseconds.

/// Milliseconds since it was created, on the monotonic clock.
struct Stopwatch {
    private let start = DispatchTime.now().uptimeNanoseconds

    var milliseconds: Int { Int((DispatchTime.now().uptimeNanoseconds - start) / 1_000_000) }
}

extension Task where Success == Never, Failure == Never {
    static func sleep(milliseconds: Int) async {
        try? await sleep(nanoseconds: UInt64(milliseconds) * 1_000_000)
    }
}
