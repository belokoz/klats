#include "clipboard.h"

#include "common.h"
#include "log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

namespace klats::app {

namespace {

constexpr size_t kMaxFormatBytes = 64 * 1024 * 1024;
constexpr ULONGLONG kSnapshotBudgetMs = 300;
constexpr DWORD kOpenBudgetMs = 500;
// About 0.1 s of conversion. Larger selections are left alone: they would take seconds with nothing
// on the screen, while the user goes on working in the same window.
constexpr size_t kMaxCopiedChars = 512 * 1024;

// Formats whose presence is the data: editors paste a whole line or a column because of them.
// Reading them makes the source render nothing, so they come back as one zero byte.
constexpr std::wstring_view kPresenceFormats[] = {
    L"MSDEVLineSelect",
    L"MSDEVColumnSelect",
    L"VisualStudioEditorOperationsLineCutCopyClipboardTag",
    L"WebKit Smart Paste Format",
};

// OLE formats that misbehave when read and written back by a third party (Word bookmarks, Outlook
// «outgoing call cannot be made»), the same list AutoHotkey skips, plus virtual file contents.
bool isOleFormat(std::wstring_view name) {
    return name.substr(0, 11) == L"Link Source" || name == L"ObjectLink" || name == L"OwnerLink" || name == L"Native" ||
           name == L"Embed Source" || name == L"FileContents";
}

// Handles that are not plain memory, or that Windows rebuilds by itself from another format.
bool isSkippedStandardFormat(UINT format) {
    switch (format) {
    case CF_BITMAP:          // rebuilt from CF_DIB
    case CF_PALETTE:         // rebuilt from CF_DIB
    case CF_OWNERDISPLAY:    // no data: the owner paints it
    case CF_DSPTEXT:
    case CF_DSPBITMAP:
    case CF_DSPMETAFILEPICT:
    case CF_DSPENHMETAFILE:
        return true;
    default:
        return (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST) || (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST);
    }
}

std::wstring formatName(UINT format) {
    if (format < 0xC000) return {};
    wchar_t name[256];
    int length = GetClipboardFormatNameW(format, name, static_cast<int>(std::size(name)));
    return std::wstring(name, static_cast<size_t>(length > 0 ? length : 0));
}

enum class Open { Opened, Busy, Changed, Cancelled };

// Retries while another program holds the clipboard. With `expected`, gives up as soon as the
// clipboard changes, and checks once more after opening, where nobody else can change it.
Open openClipboard(HWND owner, HANDLE cancel, DWORD budgetMs, std::optional<DWORD> expected = std::nullopt) {
    ULONGLONG start = nowMs();
    while (true) {
        if (OpenClipboard(owner)) {
            if (!expected || GetClipboardSequenceNumber() == *expected) return Open::Opened;
            CloseClipboard();
            return Open::Changed;
        }
        if (expected && GetClipboardSequenceNumber() != *expected) return Open::Changed;
        ULONGLONG elapsed = nowMs() - start;
        if (elapsed >= budgetMs) break;
        // Often at first: another program usually holds it for a millisecond or two.
        if (!pumpingWait(elapsed < 20 ? 1 : 10, cancel)) return Open::Cancelled;
    }
    DWORD holderProcess = 0;
    if (HWND holder = GetOpenClipboardWindow()) GetWindowThreadProcessId(holder, &holderProcess);
    log::write(L"clipboard: could not open it for " + std::to_wstring(budgetMs) + L" ms; held by " +
               (holderProcess ? processName(holderProcess) : std::wstring(L"a program without a window")));
    return Open::Busy;
}

WriteResult writeResult(Open open) {
    switch (open) {
    case Open::Changed: return WriteResult::Changed;
    case Open::Cancelled: return WriteResult::Cancelled;
    default: return WriteResult::Busy;
    }
}

HGLOBAL globalCopy(const void* data, size_t size) {
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, std::max<size_t>(size, 1));
    if (!memory) return nullptr;
    void* target = GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        return nullptr;
    }
    if (size) std::memcpy(target, data, size);
    else std::memset(target, 0, 1);
    GlobalUnlock(memory);
    return memory;
}

bool setData(UINT format, HANDLE data) {
    if (!data) return false;
    if (SetClipboardData(format, data)) return true;
    GlobalFree(data);
    return false;
}

// Searches raw clipboard bytes for UTF-16 text.
bool containsUtf16(const std::vector<BYTE>& bytes, std::wstring_view needle) {
    auto* begin = reinterpret_cast<const BYTE*>(needle.data());
    size_t size = needle.size() * sizeof(wchar_t);
    return std::search(bytes.begin(), bytes.end(), begin, begin + size) != bytes.end();
}

bool containsBytes(const std::vector<BYTE>& bytes, std::string_view needle) {
    return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end(),
                       [](BYTE a, char b) { return a == static_cast<BYTE>(b); }) != bytes.end();
}

std::optional<std::vector<BYTE>> readBytes(UINT format) {
    HANDLE handle = GetClipboardData(format);
    if (!handle) return std::nullopt;
    SIZE_T size = GlobalSize(handle);
    if (size > kMaxFormatBytes) return std::nullopt;
    const void* data = GlobalLock(handle);
    if (!data) return std::nullopt;
    std::vector<BYTE> bytes(static_cast<const BYTE*>(data), static_cast<const BYTE*>(data) + size);
    GlobalUnlock(handle);
    return bytes;
}

}  // namespace

bool pumpingWait(DWORD milliseconds, HANDLE cancel) {
    ULONGLONG deadline = nowMs() + milliseconds;
    while (true) {
        ULONGLONG now = nowMs();
        DWORD remaining = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
        DWORD result = MsgWaitForMultipleObjectsEx(cancel ? 1 : 0, cancel ? &cancel : nullptr, remaining, QS_SENDMESSAGE, 0);
        if (cancel && result == WAIT_OBJECT_0) return false;
        if (result == WAIT_TIMEOUT || remaining == 0) return true;
        // A message was sent to one of this thread's windows: answer it and keep waiting.
        MSG message;
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    }
}

std::optional<ClipboardSnapshot> ClipboardSnapshot::take(HWND owner, HANDLE cancel, Failure* failure) {
    *failure = Failure::None;
    // Reading a delayed format waits for its owner to render it, up to 30 seconds, while the
    // clipboard is locked for everyone. An owner that does not answer a ping will not render either.
    HWND clipboardOwner = GetClipboardOwner();
    if (clipboardOwner && clipboardOwner != owner) {
        DWORD_PTR ignored = 0;
        if (!SendMessageTimeoutW(clipboardOwner, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 150, &ignored)) {
            *failure = Failure::OwnerNotResponding;
            return std::nullopt;
        }
    }
    if (openClipboard(owner, cancel, kOpenBudgetMs) != Open::Opened) {
        *failure = Failure::Busy;
        return std::nullopt;
    }

    ClipboardSnapshot snapshot;
    ULONGLONG start = nowMs();
    bool haveBitmap = false;
    bool haveMetafile = false;
    for (UINT format = EnumClipboardFormats(0); format; format = EnumClipboardFormats(format)) {
        if (nowMs() - start > kSnapshotBudgetMs) {
            *failure = Failure::TooSlow;
            break;
        }
        if (isSkippedStandardFormat(format)) {
            ++snapshot.skipped_;
            continue;
        }
        std::wstring name = formatName(format);
        if (std::find(std::begin(kPresenceFormats), std::end(kPresenceFormats), name) != std::end(kPresenceFormats)) {
            snapshot.items_.push_back({format, Kind::Presence});
            continue;
        }
        if (isOleFormat(name)) {
            ++snapshot.skipped_;
            continue;
        }
        // Windows converts between the members of a pair by itself; keep the first one only.
        if (format == CF_DIB || format == CF_DIBV5) {
            if (haveBitmap) {
                ++snapshot.skipped_;
                continue;
            }
            haveBitmap = true;
        }
        if (format == CF_ENHMETAFILE || format == CF_METAFILEPICT) {
            if (haveMetafile) {
                ++snapshot.skipped_;
                continue;
            }
            haveMetafile = true;
        }

        Item item{format};
        if (format == CF_ENHMETAFILE) {
            auto metafile = static_cast<HENHMETAFILE>(GetClipboardData(format));
            UINT size = metafile ? GetEnhMetaFileBits(metafile, 0, nullptr) : 0;
            if (!size) {
                ++snapshot.skipped_;
                continue;
            }
            if (size > kMaxFormatBytes) {
                *failure = Failure::TooLarge;
                break;
            }
            item.kind = Kind::EnhancedMetafile;
            item.data.resize(size);
            GetEnhMetaFileBits(metafile, size, item.data.data());
        } else if (format == CF_METAFILEPICT) {
            HANDLE handle = GetClipboardData(format);
            auto* picture = handle ? static_cast<METAFILEPICT*>(GlobalLock(handle)) : nullptr;
            if (!picture) {
                ++snapshot.skipped_;
                continue;
            }
            UINT size = GetMetaFileBitsEx(picture->hMF, 0, nullptr);
            if (size && size <= kMaxFormatBytes) {
                item.kind = Kind::MetafilePicture;
                item.mapMode = picture->mm;
                item.width = picture->xExt;
                item.height = picture->yExt;
                item.data.resize(size);
                GetMetaFileBitsEx(picture->hMF, size, item.data.data());
            }
            GlobalUnlock(handle);
            if (item.data.empty()) {
                ++snapshot.skipped_;
                continue;
            }
        } else {
            HANDLE handle = GetClipboardData(format);
            if (!handle) {
                ++snapshot.skipped_;
                continue;
            }
            if (GlobalSize(handle) > kMaxFormatBytes) {
                *failure = Failure::TooLarge;
                break;
            }
            auto bytes = readBytes(format);
            if (!bytes) {
                ++snapshot.skipped_;
                continue;
            }
            item.data = std::move(*bytes);
        }
        snapshot.items_.push_back(std::move(item));
    }
    CloseClipboard();
    if (*failure != Failure::None) return std::nullopt;
    return snapshot;
}

WriteResult ClipboardSnapshot::restore(HWND owner, std::optional<DWORD> expected, DWORD waitMs) const {
    // Never cancelled: a quitting Klats puts the clipboard back before anything else.
    Open open = openClipboard(owner, nullptr, waitMs, expected);
    if (open != Open::Opened) return writeResult(open);
    EmptyClipboard();
    bool complete = true;
    for (const Item& item : items_) {
        bool ok = false;
        switch (item.kind) {
        case Kind::Bytes:
            ok = setData(item.format, globalCopy(item.data.data(), item.data.size()));
            break;
        case Kind::Presence:
            // SetClipboardData refuses an empty block, so presence formats get one zero byte.
            ok = setData(item.format, globalCopy(nullptr, 0));
            break;
        case Kind::EnhancedMetafile:
            if (HENHMETAFILE metafile = SetEnhMetaFileBits(static_cast<UINT>(item.data.size()), item.data.data())) {
                ok = SetClipboardData(item.format, metafile) != nullptr;
                if (!ok) DeleteEnhMetaFile(metafile);
            }
            break;
        case Kind::MetafilePicture:
            if (HMETAFILE metafile = SetMetaFileBitsEx(static_cast<UINT>(item.data.size()), item.data.data())) {
                METAFILEPICT picture{item.mapMode, item.width, item.height, metafile};
                ok = setData(item.format, globalCopy(&picture, sizeof picture));
                if (!ok) DeleteMetaFile(metafile);
            }
            break;
        }
        complete = complete && ok;
    }
    CloseClipboard();
    return complete ? WriteResult::Written : WriteResult::Partial;
}

size_t ClipboardSnapshot::byteCount() const {
    size_t total = 0;
    for (const Item& item : items_) total += item.data.size();
    return total;
}

ClipboardSnapshot::~ClipboardSnapshot() {
    for (Item& item : items_) {
        if (!item.data.empty()) SecureZeroMemory(item.data.data(), item.data.size());
    }
}

const wchar_t* describe(ClipboardSnapshot::Failure failure) {
    switch (failure) {
    case ClipboardSnapshot::Failure::Busy: return L"the clipboard is busy";
    case ClipboardSnapshot::Failure::OwnerNotResponding: return L"the program that owns the clipboard is not responding";
    case ClipboardSnapshot::Failure::TooLarge: return L"the clipboard is too large to keep";
    case ClipboardSnapshot::Failure::TooSlow: return L"the clipboard takes too long to read";
    case ClipboardSnapshot::Failure::None: break;
    }
    return L"no failure";
}

const wchar_t* describe(WriteResult result) {
    switch (result) {
    case WriteResult::Written: return L"written";
    case WriteResult::Partial: return L"written only partly";
    case WriteResult::Busy: return L"busy, not written";
    case WriteResult::Changed: return L"changed meanwhile, not written";
    case WriteResult::Cancelled: return L"not written: Klats is quitting";
    }
    return L"unknown";
}

std::optional<CopiedText> readCopiedText(HWND owner, HANDLE cancel, HKL layout) {
    if (openClipboard(owner, cancel, kOpenBudgetMs) != Open::Opened) return std::nullopt;
    CopiedText copied;
    copied.sequence = GetClipboardSequenceNumber();  // rendering delayed formats does not move it

    static const UINT lineSelect = RegisterClipboardFormatW(L"MSDEVLineSelect");
    static const UINT lineCopyTag = RegisterClipboardFormatW(L"VisualStudioEditorOperationsLineCutCopyClipboardTag");
    static const UINT columnSelect = RegisterClipboardFormatW(L"MSDEVColumnSelect");
    static const UINT blockType = RegisterClipboardFormatW(L"Borland IDE Block Type");
    static const UINT customData = RegisterClipboardFormatW(L"Chromium Web Custom MIME Data Format");

    // Where each text format sits in the list, and JetBrains' copy options, named after a Java class.
    int position = 0, textAt = -1, localeAt = -1, unicodeAt = -1;
    UINT jetBrainsOptions = 0;
    for (UINT format = EnumClipboardFormats(0); format; format = EnumClipboardFormats(format), ++position) {
        if (format == CF_TEXT) {
            textAt = position;
        } else if (format == CF_LOCALE) {
            localeAt = position;
        } else if (format == CF_UNICODETEXT) {
            unicodeAt = position;
        } else if (format >= 0xC000 && !jetBrainsOptions) {
            std::wstring name = formatName(format);
            if (name.starts_with(L"JAVA_DATAFLAVOR:") && name.find(L"CopyPasteOptionsTransferableData") != std::wstring::npos) {
                jetBrainsOptions = format;
            }
        }
    }

    // An ANSI program puts CF_TEXT alone. Windows adds CF_LOCALE from the layout on at the copy and
    // makes the Unicode text in that layout's code page, so with English on, a Russian program's
    // «пРИВЕТ» arrives as «ïÐÈÂÅÒ». The program wrote the bytes in the system code page: read them so.
    // A CF_LOCALE other than the layout's was set by the program on purpose and is trusted.
    bool ansiOnly = false;
    if (textAt >= 0 && unicodeAt > textAt && localeAt >= 0 && localeAt < unicodeAt) {
        HANDLE handle = GetClipboardData(CF_LOCALE);
        if (handle && GlobalSize(handle) >= sizeof(DWORD)) {
            if (auto* locale = static_cast<const DWORD*>(GlobalLock(handle))) {
                ansiOnly = LANGIDFROMLCID(*locale) == LOWORD(reinterpret_cast<ULONG_PTR>(layout));
                GlobalUnlock(handle);
            }
        }
    }
    if (ansiOnly) {
        if (HANDLE handle = GetClipboardData(CF_TEXT)) {
            if (auto* text = static_cast<const char*>(GlobalLock(handle))) {
                size_t length = strnlen(text, GlobalSize(handle));
                if (length > kMaxCopiedChars) {
                    copied.tooLarge = true;
                } else if (length) {
                    int size = MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(length), nullptr, 0);
                    if (size > 0) {
                        copied.text.resize(static_cast<size_t>(size));
                        MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(length), copied.text.data(), size);
                    }
                }
                GlobalUnlock(handle);
            }
        }
    } else if (HANDLE handle = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* text = static_cast<const wchar_t*>(GlobalLock(handle))) {
            size_t length = wcsnlen(text, GlobalSize(handle) / sizeof(wchar_t));  // up to the first NUL, as Chromium reads it
            if (length > kMaxCopiedChars) copied.tooLarge = true;
            else copied.text.assign(text, length);
            GlobalUnlock(handle);
        }
    }

    copied.lineCopy = IsClipboardFormatAvailable(lineSelect) || IsClipboardFormatAvailable(lineCopyTag);
    // Scintilla (Notepad++) and Visual Studio mark a column selection, and Scintilla a multiple
    // selection too, with one of these.
    copied.columnCopy = IsClipboardFormatAvailable(columnSelect);
    if (!copied.columnCopy && IsClipboardFormatAvailable(blockType)) {
        if (auto bytes = readBytes(blockType)) copied.columnCopy = !bytes->empty() && (*bytes)[0] == 0x02;
    }
    if (IsClipboardFormatAvailable(customData)) {
        // VS Code's editor stores «vscode-editor-data» (with isFromEmptySelection) in Chromium's
        // custom data; its terminal does not.
        if (auto bytes = readBytes(customData)) {
            copied.fromCodeEditor = containsUtf16(*bytes, L"vscode-editor-data");
            if (containsUtf16(*bytes, L"\"isFromEmptySelection\":true")) copied.lineCopy = true;
            SecureZeroMemory(bytes->data(), bytes->size());
        }
    }
    if (jetBrainsOptions) {
        // IntelliJ-based IDEs (2022.3 and later) serialise their copy options with Java. A boolean
        // field's value follows the end of the class description: the field name, TC_ENDBLOCKDATA
        // (0x78), TC_NULL (0x70), then 1 for true. The field is isCopiedFromEmptySelection, in early
        // builds isEntireLineFromEmptySelection.
        if (auto bytes = readBytes(jetBrainsOptions)) {
            if (containsBytes(*bytes, std::string_view("FromEmptySelection\x78\x70\x01"))) copied.lineCopy = true;
            SecureZeroMemory(bytes->data(), bytes->size());
        }
    }
    CloseClipboard();
    return copied;
}

LCID clipboardLocale(const std::wstring& text, HKL layout) {
    LCID system = GetSystemDefaultLCID();
    DWORD codePage = 0;
    bool fits = GetLocaleInfoW(system, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&codePage),
                               sizeof codePage / sizeof(wchar_t)) &&
                codePage == GetACP() && codePage != CP_UTF8;
    if (fits && !text.empty()) {
        BOOL usedDefault = FALSE;
        fits = WideCharToMultiByte(codePage, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                   &usedDefault) > 0 &&
               !usedDefault;
    }
    return fits ? system : MAKELCID(LOWORD(reinterpret_cast<ULONG_PTR>(layout)), SORT_DEFAULT);
}

std::vector<std::wstring> waitForQuietClipboard(HANDLE cancel, ULONGLONG quietMs, ULONGLONG limitMs) {
    std::vector<std::wstring> holders;
    ULONGLONG start = nowMs();
    ULONGLONG quietSince = start;
    while (nowMs() - start < limitMs) {
        if (HWND holder = GetOpenClipboardWindow()) {
            quietSince = nowMs();
            DWORD process = 0;
            GetWindowThreadProcessId(holder, &process);
            if (process != GetCurrentProcessId()) {
                std::wstring name = processName(process);
                if (std::find(holders.begin(), holders.end(), name) == holders.end()) holders.push_back(name);
            }
        } else if (nowMs() - quietSince >= quietMs) {
            break;
        }
        if (!pumpingWait(1, cancel)) break;
    }
    return holders;
}

WriteResult publishText(HWND owner, const std::wstring& text, LCID locale, bool columnCopy, DWORD expected, HANDLE cancel,
                        DWORD* sequence) {
    Open open = openClipboard(owner, cancel, kOpenBudgetMs, expected);
    if (open != Open::Opened) return writeResult(open);
    EmptyClipboard();
    bool ok = setData(CF_UNICODETEXT, globalCopy(text.c_str(), (text.size() + 1) * sizeof(wchar_t)));
    if (ok) {
        DWORD lcid = locale;
        setData(CF_LOCALE, globalCopy(&lcid, sizeof lcid));
        if (columnCopy) {
            // Without these marks the column comes back as running text, and the lines shift.
            static const UINT columnSelect = RegisterClipboardFormatW(L"MSDEVColumnSelect");
            static const UINT blockType = RegisterClipboardFormatW(L"Borland IDE Block Type");
            const BYTE column = 0x02;
            setData(columnSelect, globalCopy(nullptr, 0));
            setData(blockType, globalCopy(&column, 1));
        }
        const DWORD zero = 0;
        setData(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"), globalCopy(&zero, 1));
        setData(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"), globalCopy(&zero, sizeof zero));
        setData(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"), globalCopy(&zero, sizeof zero));
        setData(RegisterClipboardFormatW(L"Clipboard Viewer Ignore"), globalCopy(&zero, 1));
    }
    CloseClipboard();
    *sequence = GetClipboardSequenceNumber();
    return ok ? WriteResult::Written : WriteResult::Partial;
}

}  // namespace klats::app
