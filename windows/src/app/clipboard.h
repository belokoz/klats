#pragma once
#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace klats::app {

// Waits up to `milliseconds` while answering messages sent to this thread's windows. While Klats
// owns the clipboard, every other program's copy waits for its window to answer
// WM_DESTROYCLIPBOARD, so the clipboard thread never simply sleeps. Returns false early when
// `cancel` (may be null) is signalled.
bool pumpingWait(DWORD milliseconds, HANDLE cancel);

// Everything that was on the clipboard, so it can be put back after a conversion. Formats that
// cannot be copied safely (owner-drawn, private handles, OLE links, virtual files) are left out.
class ClipboardSnapshot {
public:
    enum class Failure { None, Busy, OwnerNotResponding, TooLarge, TooSlow };

    static std::optional<ClipboardSnapshot> take(HWND owner, HANDLE cancel, Failure* failure);
    bool restore(HWND owner) const;

    size_t byteCount() const;
    size_t formatCount() const { return items_.size(); }
    size_t skippedCount() const { return skipped_; }

    ClipboardSnapshot() = default;
    ClipboardSnapshot(ClipboardSnapshot&&) noexcept = default;
    ClipboardSnapshot& operator=(ClipboardSnapshot&&) noexcept = default;
    ClipboardSnapshot(const ClipboardSnapshot&) = delete;
    ClipboardSnapshot& operator=(const ClipboardSnapshot&) = delete;
    // The copy may hold a password from a password manager, so it is wiped, not just freed.
    ~ClipboardSnapshot();

private:
    enum class Kind { Bytes, Presence, EnhancedMetafile, MetafilePicture };
    struct Item {
        UINT format = 0;
        Kind kind = Kind::Bytes;
        std::vector<BYTE> data;
        LONG mapMode = 0, width = 0, height = 0;  // METAFILEPICT
    };

    std::vector<Item> items_;
    size_t skipped_ = 0;
};

const wchar_t* describe(ClipboardSnapshot::Failure failure);

// What the copy keys put on the clipboard.
struct CopiedText {
    std::wstring text;
    bool lineCopy = false;        // an editor copied the whole line because nothing was selected
    bool fromCodeEditor = false;  // VS Code's editor made the copy (its terminal does not mark copies)
};
std::optional<CopiedText> readCopiedText(HWND owner, HANDLE cancel);

// Puts the converted text on the clipboard, with the marks that keep it out of the clipboard
// history, the cloud clipboard and clipboard managers. Returns the sequence number afterwards.
std::optional<DWORD> publishText(HWND owner, const std::wstring& text, LCID locale, HANDLE cancel);

// Some programs read every clipboard change (clipboard managers, some VPN clients): for dozens of
// milliseconds after a write they keep opening the clipboard, and a program that pastes in that
// moment finds it locked. Notepad's paste then silently does nothing. Waits until nobody has held
// the clipboard for `quietMs`, at most `limitMs`, and returns the programs that held it.
std::vector<std::wstring> waitForQuietClipboard(HANDLE cancel, ULONGLONG quietMs = 25, ULONGLONG limitMs = 200);

}  // namespace klats::app
