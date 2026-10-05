#include "update_checker.h"

#include "common.h"
#include "log.h"
#include "strings.h"
#include "version.h"

#include <commctrl.h>
#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace klats::app::updates {

namespace {

// belokoz/klats by its numeric id: a feed entry only counts when it carries this number, so a
// stranger who later registers the name cannot answer here.
constexpr const char* kRepositoryId = "1375942669";
constexpr const wchar_t* kHost = L"github.com";
constexpr const wchar_t* kFeedPath = L"/belokoz/klats/releases.atom";
constexpr size_t kMaxFeed = 1 << 20;
constexpr size_t kFullPage = 10;  // GitHub's feed never holds more entries
constexpr int kMaxPages = 5;
constexpr int kMaxCandidates = 3;

struct Job {
    HWND owner;
    AppVersion current;
};

class Handle {
public:
    explicit Handle(HINTERNET handle) : handle_(handle) {}
    ~Handle() {
        if (handle_) WinHttpCloseHandle(handle_);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HINTERNET get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_;
};

struct Response {
    DWORD status = 0;
    std::string body;       // GET only
    std::wstring location;  // where a redirect leads
};

// One request to github.com, redirects not followed. Nothing comes back without an answer;
// `networkDown` then tells a missing network from any other failure.
std::optional<Response> ask(const wchar_t* verb, const std::wstring& path, bool* networkDown = nullptr) {
    if (networkDown) *networkDown = false;
    Handle session(WinHttpOpen(L"Klats/" KLATS_VERSION_WSTRING L" (Windows)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return std::nullopt;
    WinHttpSetTimeouts(session.get(), 10000, 10000, 15000, 30000);
    Handle connection(WinHttpConnect(session.get(), kHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) return std::nullopt;
    Handle request(WinHttpOpenRequest(connection.get(), verb, path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      WINHTTP_FLAG_SECURE));
    if (!request) return std::nullopt;
    DWORD noRedirects = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &noRedirects, sizeof noRedirects);
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        DWORD error = GetLastError();
        if (networkDown) {
            *networkDown = error == ERROR_WINHTTP_NAME_NOT_RESOLVED || error == ERROR_WINHTTP_CANNOT_CONNECT ||
                           error == ERROR_WINHTTP_TIMEOUT || error == ERROR_WINHTTP_CONNECTION_ERROR;
        }
        log::write(L"update check: no answer from GitHub, error " + std::to_wstring(error));
        return std::nullopt;
    }
    Response response;
    DWORD size = sizeof response.status;
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &response.status, &size, WINHTTP_NO_HEADER_INDEX);
    wchar_t location[2048];
    size = sizeof location;
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        response.location.assign(location, size / sizeof(wchar_t));
    }
    if (std::wstring_view(verb) != L"GET" || response.status != 200) return response;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available) || available == 0) break;
        if (response.body.size() + available > kMaxFeed) {
            log::write(L"update check: the feed is too large");
            return std::nullopt;
        }
        size_t offset = response.body.size();
        response.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.body.data() + offset, available, &read)) return std::nullopt;
        response.body.resize(offset + read);
    }
    return response;
}

std::wstring percentEncoded(std::string_view text) {
    const char* hex = "0123456789ABCDEF";
    std::wstring result;
    for (char c : text) {
        auto byte = static_cast<unsigned char>(c);
        bool plain = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
                     std::string_view("-._~").find(c) != std::string_view::npos;
        if (plain) {
            result += static_cast<wchar_t>(byte);
        } else {
            result += L'%';
            result += static_cast<wchar_t>(hex[byte >> 4]);
            result += static_cast<wchar_t>(hex[byte & 15]);
        }
    }
    return result;
}

// The feed page with the most recently published Windows releases. The feed lists the 10 newest
// entries of the shared repository, Mac releases and test builds included, so a page without
// Windows releases sends the search on to the next one: releases.atom?after=<its last tag>. Klats
// often starts at login, before the network is up: the first page is tried for about two minutes.
std::optional<FeedPage> windowsPage() {
    std::wstring path = kFeedPath;
    std::vector<std::string> seen;
    for (int pageNumber = 0; pageNumber < kMaxPages; ++pageNumber) {
        std::optional<Response> response;
        const DWORD pauses[] = {5000, 10000, 15000, 30000, 60000};
        for (size_t attempt = 0;; ++attempt) {
            bool networkDown = false;
            response = ask(L"GET", path, &networkDown);
            if (response || !networkDown || pageNumber > 0 || attempt >= std::size(pauses)) break;
            Sleep(pauses[attempt]);
        }
        if (!response) return std::nullopt;
        if (response->status != 200) {
            log::write(L"update check: GitHub answered HTTP " + std::to_wstring(response->status));
            return std::nullopt;
        }
        auto page = readReleaseFeed(response->body, kRepositoryId, "x64");
        if (!page) {
            log::write(L"update check: the feed could not be read");
            return std::nullopt;
        }
        if (!page->releases.empty()) return page;
        // A page that repeats means GitHub ignored the cursor.
        bool repeated = std::find(seen.begin(), seen.end(), page->firstTag) != seen.end();
        if (page->entries < kFullPage || page->lastTag.empty() || repeated) break;
        seen.push_back(page->firstTag);
        path = std::wstring(kFeedPath) + L"?after=" + percentEncoded(page->lastTag);
    }
    log::write(L"update check: no Windows release in the feed");
    return std::nullopt;
}

DWORD WINAPI checkThread(void* parameter) {
    std::unique_ptr<Job> job(static_cast<Job*>(parameter));
    auto page = windowsPage();
    if (!page) return 0;
    const WindowsRelease& newest = page->releases.front();
    if (!(job->current < newest.version)) {
        log::write(L"update check: " + job->current.toString() + L" is up to date (latest " + newest.version.toString() + L")");
        return 0;
    }

    // A tag whose release was deleted, or whose installer is not uploaded yet, looks like a release
    // in the feed. For a real installer GitHub answers with a redirect to its file storage.
    std::optional<WindowsRelease> offered;
    int checked = 0;
    for (const WindowsRelease& release : page->releases) {
        if (!(job->current < release.version) || checked++ >= kMaxCandidates) break;
        auto response = ask(L"HEAD", release.downloadURL.substr(std::wstring_view(L"https://github.com").size()));
        if (!response) return 0;  // the network is gone again: the next launch asks anew
        if (response->status >= 300 && response->status < 400 && isGitHubLink(response->location)) {
            offered = release;
            break;
        }
        log::write(L"update check: no installer for " + release.version.toString() + L" (HTTP " +
                   std::to_wstring(response->status) + L")");
    }
    if (!offered) return 0;
    log::write(L"update check: " + offered->version.toString() + L" is out, offering it to " + job->current.toString());

    // The offer must not land in the middle of typing, where the next Enter would press «Скачать».
    while (true) {
        LASTINPUTINFO input{sizeof input};
        if (GetLastInputInfo(&input) && GetTickCount() - input.dwTime >= 3000) break;
        Sleep(1000);
    }
    auto* message = new WindowsRelease(std::move(*offered));
    if (!PostMessageW(job->owner, WM_KLATS_UPDATE, 0, reinterpret_cast<LPARAM>(message))) delete message;
    return 0;
}

}  // namespace

void check(HWND owner, const AppVersion& current) {
    auto* job = new Job{owner, current};
    HANDLE thread = CreateThread(nullptr, 0, checkThread, job, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        delete job;
    }
}

void offer(HWND owner, HINSTANCE instance, const WindowsRelease& release, const AppVersion& current) {
    constexpr int kDownload = 100;
    std::wstring title = tr(L"Вышел Клац {}", release.version.toString());
    std::wstring content = tr(L"У вас версия {}. Скачайте установщик и запустите его: он закроет Клац, поставит новую версию поверх "
                              L"старой и запустит её. Windows снова попросит подтвердить запуск, как при установке. Настройки сохранятся.",
                              current.toString());
    TASKDIALOG_BUTTON buttons[] = {{kDownload, tr(L"Скачать")}, {IDCANCEL, tr(L"Не сейчас")}};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof config;
    config.hInstance = instance;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = tr(L"Клац");
    config.pszMainIcon = MAKEINTRESOURCEW(1);
    config.pszMainInstruction = title.c_str();
    config.pszContent = content.c_str();
    config.cButtons = static_cast<UINT>(std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = kDownload;
    SetForegroundWindow(owner);
    int chosen = 0;
    TaskDialogIndirect(&config, &chosen, nullptr, nullptr);
    if (chosen == kDownload && isGitHubLink(release.downloadURL)) {
        log::write(L"update offer: download opened");
        ShellExecuteW(nullptr, L"open", release.downloadURL.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        log::write(L"update offer: postponed");
    }
}

}  // namespace klats::app::updates
