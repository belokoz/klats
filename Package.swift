// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Klats",
    platforms: [.macOS(.v12)],
    targets: [
        // Pure logic: no AppKit, no system calls. Everything here is covered by tests.
        .target(name: "KlatsCore"),
        // A Swift runtime function that macOS 12–14 lack, built into the app. See the file for why.
        .target(name: "KlatsRuntimeShims"),
        // The menu bar app: everything that touches the system.
        .executableTarget(name: "Klats", dependencies: ["KlatsCore", "KlatsRuntimeShims"]),
        .testTarget(name: "KlatsCoreTests", dependencies: ["KlatsCore"]),
    ]
)
