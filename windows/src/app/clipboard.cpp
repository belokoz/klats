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

bool openClipboard(HWND owner, HANDLE cancel) {
    ULONGLONG start = nowMs();
    for (int attempt = 0;; ++attempt) {
        if (OpenClipboard(owner)) return true;
        ULONGLONG elapsed = nowMs() - start;
        if (elapsed >= kOpenBudgetMs) break;
        // Often at first: another program usually holds it for a millisecond or two.
        if (!pumpingWait(elapsed < 20 ? 1 : 10, cancel)) return false;
    }
    DWORD holderProcess = 0;
    if (HWND holder = GetOpenClipboardWindow()) GetWindowThreadProcessId(holder, &holderProcess);
    log::write(L"clipboard: could not open it for 0.5 s; held by " +
               (holderProcess ? processName(holderProcess) : std::wstring(L"a program without a window")));
    return false;
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
    if (!openClipboard(owner, cancel)) {
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

bool ClipboardSnapshot::restore(HWND owner) const {
    if (!openClipboard(owner, nullptr)) return false;
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
    return complete;
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

std::optional<CopiedText> readCopiedText(HWND owner, HANDLE cancel) {
    if (!openClipboard(owner, cancel)) return std::nullopt;
    CopiedText copied;
    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* text = static_cast<const wchar_t*>(GlobalLock(handle))) {
            size_t capacity = GlobalSize(handle) / sizeof(wchar_t);
            copied.text.assign(text, wcsnlen(text, capacity));  // up to the first NUL, as Chromium reads it
            GlobalUnlock(handle);
        }
    }
    static const UINT lineSelect = RegisterClipboardFormatW(L"MSDEVLineSelect");
    static const UINT lineCopyTag = RegisterClipboardFormatW(L"VisualStudioEditorOperationsLineCutCopyClipboardTag");
    static const UINT customData = RegisterClipboardFormatW(L"Chromium Web Custom MIME Data Format");
    copied.lineCopy = IsClipboardFormatAvailable(lineSelect) || IsClipboardFormatAvailable(lineCopyTag);
    if (IsClipboardFormatAvailable(customData)) {
        // VS Code's editor stores «vscode-editor-data» (with isFromEmptySelection) in Chromium's
        // custom data; its terminal does not.
        if (auto bytes = readBytes(customData)) {
            copied.fromCodeEditor = containsUtf16(*bytes, L"vscode-editor-data");
            if (containsUtf16(*bytes, L"\"isFromEmptySelection\":true")) copied.lineCopy = true;
            SecureZeroMemory(bytes->data(), bytes->size());
        }
    }
    CloseClipboard();
    return copied;
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

std::optional<DWORD> publishText(HWND owner, const std::wstring& text, LCID locale, HANDLE cancel) {
    if (!openClipboard(owner, cancel)) return std::nullopt;
    EmptyClipboard();
    bool ok = setData(CF_UNICODETEXT, globalCopy(text.c_str(), (text.size() + 1) * sizeof(wchar_t)));
    if (ok) {
        // Windows makes CF_TEXT from CF_UNICODETEXT through CF_LOCALE; without the right locale
        // Cyrillic would turn into question marks for programs that read CF_TEXT.
        DWORD lcid = locale;
        setData(CF_LOCALE, globalCopy(&lcid, sizeof lcid));
        const DWORD zero = 0;
        setData(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"), globalCopy(&zero, 1));
        setData(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"), globalCopy(&zero, sizeof zero));
        setData(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"), globalCopy(&zero, sizeof zero));
        setData(RegisterClipboardFormatW(L"Clipboard Viewer Ignore"), globalCopy(&zero, 1));
    }
    CloseClipboard();
    if (!ok) return std::nullopt;
    return GetClipboardSequenceNumber();
}

}  // namespace klats::app
