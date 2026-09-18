// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Klats",
    platforms: [.macOS(.v13)],
    targets: [
        // Pure logic: no AppKit, no system calls. Everything here is covered by tests.
        .target(name: "KlatsCore"),
        // The menu bar app: everything that touches the system.
        .executableTarget(name: "Klats", dependencies: ["KlatsCore"]),
        .testTarget(name: "KlatsCoreTests", dependencies: ["KlatsCore"]),
    ]
)
