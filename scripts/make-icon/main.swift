// Renders build/AppIcon.iconset from the same drawing code the app uses (Sources/Klats/AppIcon.swift).
// Compiled and run by scripts/build-app.sh; needs only the Command Line Tools.
import AppKit

@MainActor
func png(pixels: Int) -> Data {
    let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels, bitsPerSample: 8, samplesPerPixel: 4,
        hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    AppIcon.image(size: CGFloat(pixels)).draw(in: NSRect(x: 0, y: 0, width: pixels, height: pixels))
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

@MainActor
func render() throws {
    let iconset = URL(fileURLWithPath: "build/AppIcon.iconset")
    try? FileManager.default.removeItem(at: iconset)
    try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
    for base in [16, 32, 128, 256, 512] {
        try png(pixels: base).write(to: iconset.appendingPathComponent("icon_\(base)x\(base).png"))
        try png(pixels: base * 2).write(to: iconset.appendingPathComponent("icon_\(base)x\(base)@2x.png"))
    }
    print("iconset ready")
}

// A command line tool starts on the main thread, which is all the main actor needs here.
try MainActor.assumeIsolated { try render() }
