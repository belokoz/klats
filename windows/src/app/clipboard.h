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

// How a write to the clipboard ended. Once the copy keys have gone, Klats writes only while the
// clipboard still holds what Klats last saw there: the sequence number is compared inside the
// clipboard session, where nobody else can change it.
enum class WriteResult {
    Written,
    Partial,    // some formats could not be written
    Busy,       // another program held the clipboard all along
    Changed,    // something else was copied meanwhile, and it stays
    Cancelled,  // Klats is quitting
};
const wchar_t* describe(WriteResult result);

// Everything that was on the clipboard, so it can be put back after a conversion. Formats that
// cannot be copied safely (owner-drawn, private handles, OLE links, virtual files) are left out.
class ClipboardSnapshot {
public:
    enum class Failure { None, Busy, OwnerNotResponding, TooLarge, TooSlow };

    static std::optional<ClipboardSnapshot> take(HWND owner, HANDLE cancel, Failure* failure);
    // Puts everything back, unless the clipboard no longer holds `expected` (nullopt: whatever it
    // holds). Waits up to `waitMs` for a busy clipboard, and gives up as soon as it changes.
    WriteResult restore(HWND owner, std::optional<DWORD> expected, DWORD waitMs = 500) const;

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
    DWORD sequence = 0;           // the clipboard's sequence number while Klats read it
    bool tooLarge = false;        // longer than Klats converts; `text` stays empty
    bool lineCopy = false;        // an editor copied the whole line because nothing was selected
    bool columnCopy = false;      // a column (box) selection, to be pasted back as a column
    bool fromCodeEditor = false;  // VS Code's editor made the copy (its terminal does not mark copies)
};
// `layout` is the keyboard layout of the program that copied.
std::optional<CopiedText> readCopiedText(HWND owner, HANDLE cancel, HKL layout);

// The CF_LOCALE to publish `text` with. Windows makes CF_TEXT for ANSI programs in the code page of
// CF_LOCALE, and they read it in the system code page: the system locale keeps the two equal
// whenever the text fits it. Otherwise the language of `layout`.
LCID clipboardLocale(const std::wstring& text, HKL layout);

// Puts the converted text on the clipboard in place of the copy that holds `expected`, with the
// marks that keep it out of the clipboard history, the cloud clipboard and clipboard managers.
// `sequence` receives the sequence number afterwards.
WriteResult publishText(HWND owner, const std::wstring& text, LCID locale, bool columnCopy, DWORD expected, HANDLE cancel,
                        DWORD* sequence);

// Some programs read every clipboard change (clipboard managers, some VPN clients): for dozens of
// milliseconds after a write they keep opening the clipboard, and a program that pastes in that
// moment finds it locked. Notepad's paste then silently does nothing. Waits until nobody has held
// the clipboard for `quietMs`, at most `limitMs`, and returns the programs that held it.
std::vector<std::wstring> waitForQuietClipboard(HANDLE cancel, ULONGLONG quietMs = 25, ULONGLONG limitMs = 200);

}  // namespace klats::app
