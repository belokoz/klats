import AppKit

/// Everything that was on the pasteboard, so it can be put back after a conversion.
@MainActor
struct PasteboardSnapshot {
    private let items: [[(NSPasteboard.PasteboardType, Data)]]
    let byteCount: Int

    init(_ pasteboard: NSPasteboard) {
        var total = 0
        items = (pasteboard.pasteboardItems ?? []).map { item in
            item.types.compactMap { type in
                guard let data = item.data(forType: type) else { return nil }
                total += data.count
                return (type, data)
            }
        }
        byteCount = total
    }

    func restore(to pasteboard: NSPasteboard) {
        pasteboard.clearContents()
        let restored = items.map { pairs -> NSPasteboardItem in
            let item = NSPasteboardItem()
            for (type, data) in pairs { item.setData(data, forType: type) }
            return item
        }
        if !restored.isEmpty { pasteboard.writeObjects(restored) }
    }
}
