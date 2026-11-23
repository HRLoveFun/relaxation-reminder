#include "win32/update_check.h"

#include <winhttp.h>

#include <cstring>

#include "core/version.h"

namespace rr {
namespace {

constexpr wchar_t kUserAgent[] = L"RelaxationReminder-UpdateCheck/1.0";
constexpr wchar_t kHost[] = L"api.github.com";
constexpr wchar_t kLatestPath[] =
    L"/repos/HRLoveFun/relaxation-reminder/releases/latest";
constexpr wchar_t kHeaders[] =
    L"Accept: application/vnd.github+json\r\n"
    L"User-Agent: RelaxationReminder\r\n";
constexpr size_t kMaxResponseBytes = 512 * 1024;

struct InternetHandle {
    HINTERNET handle = nullptr;

    ~InternetHandle() {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
    }
};

std::wstring FormatVersion(const VersionNumber& version) {
    wchar_t text[32] = {};
    swprintf_s(text, L"%u.%u.%u.%u", version.parts[0], version.parts[1],
               version.parts[2], version.parts[3]);
    return text;
}

bool ReadCurrentVersion(VersionNumber* version) {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, path, _countof(path))) {
        return false;
    }
    const DWORD size = GetFileVersionInfoSizeW(path, nullptr);
    if (size == 0) {
        return false;
    }

    BYTE data[4096] = {};
    if (size > sizeof(data) || !GetFileVersionInfoW(path, 0, size, data)) {
        return false;
    }

    VS_FIXEDFILEINFO* fixed = nullptr;
    UINT fixed_size = 0;
    if (!VerQueryValueW(data, L"\\", reinterpret_cast<void**>(&fixed),
                        &fixed_size) ||
        fixed == nullptr || fixed_size < sizeof(VS_FIXEDFILEINFO)) {
        return false;
    }

    version->parts[0] = HIWORD(fixed->dwFileVersionMS);
    version->parts[1] = LOWORD(fixed->dwFileVersionMS);
    version->parts[2] = HIWORD(fixed->dwFileVersionLS);
    version->parts[3] = LOWORD(fixed->dwFileVersionLS);
    return true;
}

bool ReadResponse(HINTERNET request, std::string* body) {
    char buffer[4096];
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            return false;
        }
        if (available == 0) {
            return true;
        }
        if (body->size() + available > kMaxResponseBytes) {
            return false;
        }

        const DWORD chunk = available < sizeof(buffer)
                                ? available
                                : static_cast<DWORD>(sizeof(buffer));
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer, chunk, &read) || read == 0) {
            return false;
        }
        body->append(buffer, read);
    }
}

bool FetchLatestTag(std::string* tag) {
    InternetHandle session;
    session.handle = WinHttpOpen(
        kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (session.handle == nullptr) {
        session.handle = WinHttpOpen(
            kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (session.handle == nullptr) {
        return false;
    }
    WinHttpSetTimeouts(session.handle, 4000, 4000, 5000, 5000);

    InternetHandle connection;
    connection.handle =
        WinHttpConnect(session.handle, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection.handle == nullptr) {
        return false;
    }

    InternetHandle request;
    request.handle = WinHttpOpenRequest(
        connection.handle, L"GET", kLatestPath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (request.handle == nullptr) {
        return false;
    }
    if (!WinHttpSendRequest(request.handle, kHeaders,
                            static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0u,
                            0u, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) {
        return false;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(
            request.handle,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
            WINHTTP_NO_HEADER_INDEX) ||
        status != 200) {
        return false;
    }

    std::string body;
    if (!ReadResponse(request.handle, &body)) {
        return false;
    }

    const size_t key = body.find("\"tag_name\"");
    if (key == std::string::npos) {
        return false;
    }
    size_t colon = body.find(':', key);
    if (colon == std::string::npos) {
        return false;
    }
    const size_t start = body.find('"', colon);
    if (start == std::string::npos) {
        return false;
    }
    const size_t end = body.find('"', start + 1);
    if (end == std::string::npos) {
        return false;
    }
    *tag = body.substr(start + 1, end - start - 1);
    return true;
}

}  // namespace

UpdateInfo CheckForUpdate() {
    UpdateInfo info;

    VersionNumber current;
    if (!ReadCurrentVersion(&current)) {
        return info;
    }
    info.current_version = FormatVersion(current);

    std::string tag;
    VersionNumber latest;
    if (!FetchLatestTag(&tag) || !ParseVersion(tag, &latest)) {
        return info;
    }

    info.latest_version = FormatVersion(latest);
    info.status = CompareVersions(latest, current) > 0 ? UpdateStatus::NewVersion
                                                       : UpdateStatus::UpToDate;
    return info;
}

std::wstring BuildUpdatePrompt(const std::wstring& current_version,
                               const std::wstring& latest_version) {
    return L"请在这台电脑上把「休息提醒（Relaxation Reminder）」更新到最新版本"
           L"（当前 " +
           current_version + L"，最新 " + latest_version +
           L"）。先读取安装说明并严格照做：\r\n"
           L"https://raw.githubusercontent.com/HRLoveFun/relaxation-reminder/"
           L"main/INSTALL.md\r\n"
           L"只从官方仓库下载，不关闭安全软件，不请求管理员权限；完成后按说明报告"
           L"安装版本、路径和 SHA-256。";
}

bool CopyTextToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) {
        return false;
    }
    EmptyClipboard();

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    void* target = GlobalLock(memory);
    if (target == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(target, text.c_str(), bytes);
    GlobalUnlock(memory);

    const bool copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
    if (!copied) {
        GlobalFree(memory);
    }
    CloseClipboard();
    return copied;
}

}  // namespace rr
