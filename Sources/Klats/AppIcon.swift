import AppKit

/// The app icon and the menu bar glyph, drawn in code so the project needs no image assets.
/// A keycap bearing a Latin «K» and a red Cyrillic «Л»: the real face of the K key on a Russian
/// keyboard, and together they read «Кл». The icon generator script draws the very same thing.
@MainActor
enum AppIcon {
    /// The full colour icon, used in the first-run window and by the icon generator.
    static func image(size: CGFloat) -> NSImage {
        NSImage(size: NSSize(width: size, height: size), flipped: false) { _ in
            draw(scale: size / 256)
            return true
        }
    }

    /// A monochrome template of the keycap outline with a «K», tinted by the menu bar itself.
    static func menuBarGlyph() -> NSImage {
        let image = NSImage(size: NSSize(width: 18, height: 18), flipped: false) { bounds in
            NSColor.black.setStroke()
            let cap = NSBezierPath(roundedRect: bounds.insetBy(dx: 1.6, dy: 1.6), xRadius: 4.2, yRadius: 4.2)
            cap.lineWidth = 1.5
            cap.stroke()
            // The letter keeps clear of the frame, like the legends on the app icon.
            // The K's outline runs from 18% into its box to the box's right edge, so the box sits
            // a little left of centre for the letter itself to land in the middle of the frame.
            letterK(centeredIn: NSRect(x: 5.1, y: 5.0, width: 6.6, height: 8.0), lineWidth: 1.6).stroke()
            return true
        }
        image.isTemplate = true
        return image
    }

    // Everything is laid out on a 256-point canvas with AppKit's bottom-left origin.
    private static let tile = NSRect(x: 0, y: 0, width: 256, height: 256)
    private static let rim = NSRect(x: 32, y: 28, width: 192, height: 190)
    private static let face = NSRect(x: 45, y: 43, width: 166, height: 160)

    private static func draw(scale: CGFloat) {
        func scaled(_ rect: NSRect) -> NSRect {
            NSRect(x: rect.minX * scale, y: rect.minY * scale, width: rect.width * scale, height: rect.height * scale)
        }
        NSColor(calibratedRed: 0.165, green: 0.176, blue: 0.204, alpha: 1).setFill()
        NSBezierPath(roundedRect: scaled(tile), xRadius: 58 * scale, yRadius: 58 * scale).fill()
        NSColor(calibratedRed: 0.525, green: 0.549, blue: 0.592, alpha: 1).setFill()
        NSBezierPath(roundedRect: scaled(rim), xRadius: 36 * scale, yRadius: 36 * scale).fill()
        NSColor(calibratedRed: 0.953, green: 0.957, blue: 0.965, alpha: 1).setFill()
        NSBezierPath(roundedRect: scaled(face), xRadius: 26 * scale, yRadius: 26 * scale).fill()

        // Legends sit in opposite corners of the face, like on a real bilingual keycap.
        let inset: CGFloat = 16
        let glyph = NSSize(width: 70, height: 66)
        let kBox = NSRect(x: face.minX + inset, y: face.maxY - inset - glyph.height, width: glyph.width, height: glyph.height)
        let lBox = NSRect(x: face.maxX - inset - glyph.width, y: face.minY + inset, width: glyph.width, height: glyph.height)
        letter("K", in: scaled(kBox), color: NSColor(calibratedRed: 0.106, green: 0.118, blue: 0.141, alpha: 1))
        letter("Л", in: scaled(lBox), color: NSColor(calibratedRed: 0.847, green: 0.275, blue: 0.184, alpha: 1))
    }

    /// Draws one letter so that its visible outline, not its font box, is centred in `box`.
    private static func letter(_ character: String, in box: NSRect, color: NSColor) {
        guard let context = NSGraphicsContext.current?.cgContext else { return }
        let font = NSFont(name: "Unbounded-ExtraBold", size: box.height)
            ?? NSFont.systemFont(ofSize: box.height, weight: .heavy)
        let line = CTLineCreateWithAttributedString(
            NSAttributedString(string: character, attributes: [.font: font, .foregroundColor: color]))
        let bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
        let fit = min(box.width / bounds.width, box.height / bounds.height)
        context.saveGState()
        context.translateBy(x: box.midX, y: box.midY)
        context.scaleBy(x: fit, y: fit)
        context.textPosition = CGPoint(x: -bounds.midX, y: -bounds.midY)
        CTLineDraw(line, context)
        context.restoreGState()
    }

    private static func letterK(centeredIn box: NSRect, lineWidth: CGFloat) -> NSBezierPath {
        let path = NSBezierPath()
        path.lineWidth = lineWidth
        path.lineCapStyle = .round
        path.lineJoinStyle = .round
        let spine = box.minX + box.width * 0.18
        path.move(to: NSPoint(x: spine, y: box.maxY))
        path.line(to: NSPoint(x: spine, y: box.minY))
        path.move(to: NSPoint(x: spine, y: box.midY - box.height * 0.04))
        path.line(to: NSPoint(x: box.maxX, y: box.maxY))
        path.move(to: NSPoint(x: spine + box.width * 0.2, y: box.midY + box.height * 0.06))
        path.line(to: NSPoint(x: box.maxX, y: box.minY))
        return path
    }
}
