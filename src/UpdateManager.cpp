#include "UpdateManager.h"
#include "Version.h"

#include <winhttp.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace {

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string JsonGetString(const std::string& json, const std::string& key, size_t from = 0) {
    const std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat, from);
    if (k == std::string::npos) return {};
    size_t colon = json.find(':', k + pat.size());
    if (colon == std::string::npos) return {};
    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return {};
    size_t q2 = q1 + 1;
    while (q2 < json.size()) {
        if (json[q2] == '\\' && q2 + 1 < json.size()) {
            q2 += 2;
            continue;
        }
        if (json[q2] == '"') break;
        ++q2;
    }
    if (q2 >= json.size()) return {};
    std::string raw = json.substr(q1 + 1, q2 - q1 - 1);
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char c = raw[i + 1];
            if (c == '"' || c == '\\' || c == '/') out.push_back(c);
            else if (c == 'n') out.push_back('\n');
            else if (c == 't') out.push_back('\t');
            else out.push_back(c);
            ++i;
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

bool JsonGetBool(const std::string& json, const std::string& key, size_t from = 0) {
    const std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat, from);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t p = colon + 1;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) ++p;
    return json.compare(p, 4, "true") == 0;
}

std::vector<int> ParseVersionParts(std::wstring v) {
    if (!v.empty() && (v[0] == L'v' || v[0] == L'V')) v.erase(v.begin());
    for (auto& c : v) {
        if (c == L'-' || c == L'+' || c == L'_') c = L'.';
    }
    std::vector<int> parts;
    size_t i = 0;
    while (i < v.size() && parts.size() < 4) {
        while (i < v.size() && (v[i] < L'0' || v[i] > L'9')) ++i;
        if (i >= v.size()) break;
        int n = 0;
        while (i < v.size() && v[i] >= L'0' && v[i] <= L'9') {
            n = n * 10 + (v[i] - L'0');
            ++i;
        }
        parts.push_back(n);
    }
    while (parts.size() < 3) parts.push_back(0);
    return parts;
}

int CompareVersions(const std::wstring& a, const std::wstring& b) {
    auto pa = ParseVersionParts(a);
    auto pb = ParseVersionParts(b);
    const size_t n = (std::max)(pa.size(), pb.size());
    pa.resize(n, 0);
    pb.resize(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return 1;
    }
    return 0;
}

bool IsFormalVersionTag(const std::wstring& tag) {
    if (tag.size() < 2) return false;
    if (tag[0] != L'v' && tag[0] != L'V') return false;
    return tag[1] >= L'0' && tag[1] <= L'9';
}

bool HttpGet(const std::wstring& host, const std::wstring& path, bool https,
             const std::wstring& userAgent, std::string& body, std::wstring& error,
             const std::wstring& accept = L"application/vnd.github+json") {
    body.clear();
    HINTERNET hSession = WinHttpOpen(userAgent.c_str(),
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        error = L"无法初始化网络组件";
        return false;
    }

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    // Follow redirects for GitHub asset CDN.
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        error = L"无法连接服务器";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        error = L"无法创建请求";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring headers = L"Accept: " + accept + L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(-1L),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        error = L"网络请求失败";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

    std::string data;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
            error = L"读取响应失败";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        if (avail == 0) break;
        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf.data(), avail, &read)) {
            error = L"下载数据失败";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        data.append(buf.data(), read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (status < 200 || status >= 300) {
        wchar_t msg[128];
        swprintf_s(msg, L"服务器返回错误 %lu", status);
        error = msg;
        return false;
    }

    body = std::move(data);
    return true;
}

bool HttpDownloadFile(const std::wstring& url, const fs::path& dest,
                      UpdateManager::ProgressFn progress, std::wstring& error) {
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[1024]{};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 1024;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        error = L"下载地址无效";
        return false;
    }

    std::wstring fullPath = std::wstring(path) + extra;
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    if (progress) progress(L"正在下载更新...");

    std::string body;
    if (!HttpGet(host, fullPath, https, L"WindowLayout-Updater", body, error, L"*/*")) {
        return false;
    }
    if (body.size() < 64) {
        error = L"下载内容异常（文件过小）";
        return false;
    }

    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream ofs(dest, std::ios::binary);
    if (!ofs) {
        error = L"无法写入临时文件";
        return false;
    }
    ofs.write(body.data(), static_cast<std::streamsize>(body.size()));
    ofs.close();
    if (!ofs) {
        error = L"保存下载文件失败";
        return false;
    }

    if (progress) {
        wchar_t buf[64];
        swprintf_s(buf, L"已下载 %.1f MB", body.size() / (1024.0 * 1024.0));
        progress(buf);
    }
    return true;
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

fs::path TempUpdateDir() {
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    return fs::path(tmp) / L"WindowLayoutUpdate";
}

bool ExpandZip(const fs::path& zip, const fs::path& dest, std::wstring& error) {
    std::error_code ec;
    fs::create_directories(dest, ec);

    std::wstring cmd = L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '"
        + zip.wstring() + L"' -DestinationPath '" + dest.wstring() + L"' -Force\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        error = L"无法解压更新包（PowerShell）";
        return false;
    }
    WaitForSingleObject(pi.hProcess, 120000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0) {
        error = L"解压更新包失败";
        return false;
    }
    return true;
}

fs::path FindUpdatedExe(const fs::path& dir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); ++it) {
        if (it->is_regular_file(ec) && it->path().filename() == L"WindowLayout.exe") {
            return it->path();
        }
    }
    return {};
}

bool WriteUpdaterScript(const fs::path& script, DWORD pid,
                        const fs::path& newExe, const fs::path& targetExe, std::wstring& error) {
    std::ofstream ofs(script, std::ios::binary);
    if (!ofs) {
        error = L"无法创建更新脚本";
        return false;
    }
    std::ostringstream ps;
    ps << "$pidToWait = " << pid << "\r\n"
       << "$newExe = '" << WideToUtf8(newExe.wstring()) << "'\r\n"
       << "$target = '" << WideToUtf8(targetExe.wstring()) << "'\r\n"
       << "while (Get-Process -Id $pidToWait -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 400 }\r\n"
       << "Start-Sleep -Milliseconds 300\r\n"
       << "Copy-Item -LiteralPath $newExe -Destination $target -Force\r\n"
       << "Start-Process -FilePath $target\r\n"
       << "Remove-Item -LiteralPath $newExe -Force -ErrorAction SilentlyContinue\r\n"
       << "Remove-Item -LiteralPath $MyInvocation.MyCommand.Path -Force -ErrorAction SilentlyContinue\r\n";
    const std::string content = ps.str();
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return true;
}

struct ReleasePick {
    std::wstring tag;
    std::wstring name;
    std::wstring downloadUrl;
    std::wstring assetName;
    bool prerelease = false;
};

// Pick the newest formal (v*) non-prerelease with the expected zip asset.
// Fallback: newest release (including prerelease) that has the zip.
bool PickRelease(const std::string& json, ReleasePick& out) {
    const std::string assetWanted = APP_RELEASE_ASSET;
    ReleasePick formal;
    ReleasePick any;
    bool hasFormal = false;
    bool hasAny = false;

    size_t pos = 0;
    while ((pos = json.find("\"tag_name\"", pos)) != std::string::npos) {
        const size_t releaseStart = pos;
        size_t nextTag = json.find("\"tag_name\"", pos + 10);
        const size_t releaseEnd = (nextTag == std::string::npos) ? json.size() : nextTag;

        std::string tag = JsonGetString(json, "tag_name", releaseStart);
        std::string name = JsonGetString(json, "name", releaseStart);
        bool prerelease = JsonGetBool(json, "prerelease", releaseStart);

        size_t assets = json.find("\"assets\"", releaseStart);
        if (assets == std::string::npos || assets >= releaseEnd || tag.empty()) {
            pos = releaseEnd;
            continue;
        }

        std::string bestUrl;
        std::string bestName;
        size_t apos = assets;
        while ((apos = json.find("\"browser_download_url\"", apos)) != std::string::npos && apos < releaseEnd) {
            std::string url = JsonGetString(json, "browser_download_url", apos);
            size_t nameKey = json.rfind("\"name\"", apos);
            std::string aname;
            if (nameKey != std::string::npos && nameKey > assets) {
                aname = JsonGetString(json, "name", nameKey);
            }
            if (aname == assetWanted || url.find(assetWanted) != std::string::npos) {
                bestUrl = url;
                bestName = aname.empty() ? assetWanted : aname;
                break;
            }
            if (bestUrl.empty() && aname.size() > 4 && aname.substr(aname.size() - 4) == ".zip") {
                bestUrl = url;
                bestName = aname;
            }
            apos += 22;
        }

        if (!bestUrl.empty()) {
            ReleasePick pick;
            pick.tag = Utf8ToWide(tag);
            pick.name = Utf8ToWide(name);
            pick.downloadUrl = Utf8ToWide(bestUrl);
            pick.assetName = Utf8ToWide(bestName);
            pick.prerelease = prerelease;

            if (!hasAny) {
                any = pick;
                hasAny = true;
            }
            if (!hasFormal && !prerelease && IsFormalVersionTag(pick.tag)) {
                formal = pick;
                hasFormal = true;
            }
        }
        pos = releaseEnd;
    }

    if (hasFormal) {
        out = formal;
        return true;
    }
    if (hasAny) {
        out = any;
        return true;
    }
    return false;
}

} // namespace

std::wstring UpdateManager::CurrentVersion() {
    return Utf8ToWide(APP_VERSION);
}

std::wstring UpdateManager::ReleasesPageUrl() {
    return std::wstring(L"https://github.com/") + APP_REPO_OWNER_W + L"/" + APP_REPO_NAME_W + L"/releases";
}

UpdateInfo UpdateManager::CheckForUpdate() {
    UpdateInfo info;
    info.currentVersion = CurrentVersion();
    info.releasePageUrl = ReleasesPageUrl();

    const std::wstring host = L"api.github.com";
    const std::wstring path = std::wstring(L"/repos/") + APP_REPO_OWNER_W + L"/" + APP_REPO_NAME_W + L"/releases?per_page=20";
    std::string body;
    std::wstring err;
    if (!HttpGet(host, path, true, L"WindowLayout-Updater", body, err)) {
        info.error = err.empty() ? L"检查更新失败" : err;
        return info;
    }

    ReleasePick pick;
    if (!PickRelease(body, pick)) {
        info.error = L"未找到可用的发布包";
        return info;
    }

    info.tagName = pick.tag;
    info.latestVersion = pick.tag;
    if (!info.latestVersion.empty() &&
        (info.latestVersion[0] == L'v' || info.latestVersion[0] == L'V')) {
        info.latestVersion.erase(info.latestVersion.begin());
    }
    info.assetName = pick.assetName;
    info.downloadUrl = pick.downloadUrl;
    info.releaseName = pick.name.empty() ? pick.tag : pick.name;

    // Only formal releases (vX.Y.Z) participate in update comparison.
    // Continuous pre-releases like "latest" are ignored by the updater.
    if (IsFormalVersionTag(pick.tag) && !pick.prerelease) {
        info.available = CompareVersions(info.currentVersion, info.latestVersion) < 0;
    } else {
        info.available = false;
        info.latestVersion = info.currentVersion;
    }
    return info;
}

std::wstring UpdateManager::DownloadUpdate(const UpdateInfo& info, ProgressFn progress, std::wstring& error) {
    if (info.downloadUrl.empty()) {
        error = L"没有下载地址";
        return {};
    }
    if (progress) progress(L"正在下载更新...");

    fs::path dir = TempUpdateDir();
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    fs::path zip = dir / (info.assetName.empty() ? L"update.zip" : info.assetName);

    if (!HttpDownloadFile(info.downloadUrl, zip, progress, error)) {
        return {};
    }
    return zip.wstring();
}

bool UpdateManager::ApplyAndRestart(const std::wstring& zipPath, std::wstring& error) {
    fs::path zip(zipPath);
    fs::path extractDir = TempUpdateDir() / L"extracted";
    std::error_code ec;
    fs::remove_all(extractDir, ec);

    if (!ExpandZip(zip, extractDir, error)) return false;

    fs::path newExe = FindUpdatedExe(extractDir);
    if (newExe.empty()) {
        error = L"更新包中未找到 WindowLayout.exe";
        return false;
    }

    // Keep a stable staged copy so extraction cleanup is safer.
    fs::path staged = TempUpdateDir() / L"WindowLayout.exe.new";
    fs::copy_file(newExe, staged, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = L"无法准备更新文件";
        return false;
    }

    fs::path target = ExePath();
    fs::path script = TempUpdateDir() / L"apply_update.ps1";
    DWORD pid = GetCurrentProcessId();
    if (!WriteUpdaterScript(script, pid, staged, target, error)) return false;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \""
        + script.wstring() + L"\"";
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        error = L"无法启动更新程序";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void UpdateManager::OpenInBrowser(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
