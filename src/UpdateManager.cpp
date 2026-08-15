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

struct HttpSession {
    HINTERNET session = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;

    ~HttpSession() {
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        if (session) WinHttpCloseHandle(session);
    }
};

bool HttpOpenGet(const std::wstring& host, const std::wstring& path, bool https,
                 const std::wstring& userAgent, const std::wstring& accept,
                 HttpSession& s, DWORD& status, std::wstring& error) {
    s.session = WinHttpOpen(userAgent.c_str(),
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s.session) {
        error = L"无法初始化网络组件";
        return false;
    }

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(s.session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(s.session, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    s.connect = WinHttpConnect(s.session, host.c_str(),
                               https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!s.connect) {
        error = L"无法连接服务器";
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    s.request = WinHttpOpenRequest(s.connect, L"GET", path.c_str(),
                                   nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!s.request) {
        error = L"无法创建请求";
        return false;
    }

    std::wstring headers = L"Accept: " + accept + L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    BOOL ok = WinHttpSendRequest(s.request, headers.c_str(), static_cast<DWORD>(-1L),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(s.request, nullptr);
    if (!ok) {
        error = L"网络请求失败";
        return false;
    }

    status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(s.request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status < 200 || status >= 300) {
        wchar_t msg[128];
        swprintf_s(msg, L"服务器返回错误 %lu", status);
        error = msg;
        return false;
    }
    return true;
}

std::uint64_t QueryContentLength(HINTERNET request) {
    wchar_t buf[64]{};
    DWORD size = sizeof(buf);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                             WINHTTP_HEADER_NAME_BY_INDEX, buf, &size, WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return static_cast<std::uint64_t>(_wcstoui64(buf, nullptr, 10));
}

bool HttpGet(const std::wstring& host, const std::wstring& path, bool https,
             const std::wstring& userAgent, std::string& body, std::wstring& error,
             const std::wstring& accept = L"application/vnd.github+json") {
    body.clear();
    HttpSession s;
    DWORD status = 0;
    if (!HttpOpenGet(host, path, https, userAgent, accept, s, status, error)) {
        return false;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(s.request, &avail)) {
            error = L"读取响应失败";
            return false;
        }
        if (avail == 0) break;
        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(s.request, buf.data(), avail, &read)) {
            error = L"下载数据失败";
            return false;
        }
        body.append(buf.data(), read);
    }
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

    if (progress) progress(0, 0, L"正在连接服务器...");

    HttpSession s;
    DWORD status = 0;
    if (!HttpOpenGet(host, fullPath, https, L"WindowLayout-Updater", L"*/*", s, status, error)) {
        return false;
    }

    const std::uint64_t total = QueryContentLength(s.request);
    if (progress) progress(0, total, L"正在下载更新...");

    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream ofs(dest, std::ios::binary);
    if (!ofs) {
        error = L"无法写入临时文件";
        return false;
    }

    std::uint64_t received = 0;
    std::uint64_t lastReport = 0;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(s.request, &avail)) {
            error = L"读取响应失败";
            return false;
        }
        if (avail == 0) break;

        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(s.request, buf.data(), avail, &read) || read == 0) {
            error = L"下载数据失败";
            return false;
        }
        ofs.write(buf.data(), static_cast<std::streamsize>(read));
        if (!ofs) {
            error = L"保存下载文件失败";
            return false;
        }
        received += read;

        // Throttle UI updates roughly every 32 KB or at completion.
        if (progress && (received - lastReport >= 32 * 1024 || (total > 0 && received >= total))) {
            lastReport = received;
            progress(received, total, L"正在下载更新...");
        }
    }
    ofs.close();
    if (!ofs) {
        error = L"保存下载文件失败";
        return false;
    }
    if (received < 64) {
        error = L"下载内容异常（文件过小）";
        return false;
    }
    if (progress) progress(received, total > 0 ? total : received, L"下载完成");
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

std::wstring PowerShellExePath() {
    wchar_t sys[MAX_PATH]{};
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    }
    return std::wstring(sys) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
}

bool LaunchHiddenDetached(const std::wstring& exe, const std::wstring& args, std::wstring& error) {
    // Do NOT combine DETACHED_PROCESS with CREATE_NO_WINDOW (NO_WINDOW is ignored).
    // Avoid cmd.exe / start — both flash a console. Launch powershell.exe directly.
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    DWORD flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
    if (!CreateProcessW(exe.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE,
                        flags, nullptr, nullptr, &si, &pi)) {
        // Retry without BREAKAWAY_FROM_JOB (not always allowed).
        flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP;
        if (!CreateProcessW(exe.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE,
                            flags, nullptr, nullptr, &si, &pi)) {
            error = L"无法启动后台进程";
            return false;
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool ExpandZip(const fs::path& zip, const fs::path& dest, std::wstring& error) {
    std::error_code ec;
    fs::create_directories(dest, ec);

    const std::wstring ps = PowerShellExePath();
    std::wstring args = L"-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -Command "
        L"\"Expand-Archive -LiteralPath '" + zip.wstring() + L"' -DestinationPath '"
        + dest.wstring() + L"' -Force\"";

    std::wstring cmd = L"\"" + ps + L"\" " + args;
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(ps.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE,
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

bool IsWindowLayoutExeName(const std::wstring& name) {
    // Accept WindowLayout.exe and versioned names like WindowLayout-v0.1.9.exe
    if (name.size() < 4) return false;
    if (name.compare(name.size() - 4, 4, L".exe") != 0) return false;
    if (name == L"WindowLayout.exe") return true;
    return name.rfind(L"WindowLayout", 0) == 0;
}

fs::path FindUpdatedExe(const fs::path& dir) {
    std::error_code ec;
    fs::path preferred;
    fs::path any;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file(ec)) continue;
        const std::wstring name = it->path().filename().wstring();
        if (!IsWindowLayoutExeName(name)) continue;
        if (name == L"WindowLayout.exe") {
            preferred = it->path();
            break;
        }
        if (any.empty()) any = it->path();
    }
    return preferred.empty() ? any : preferred;
}

std::string EscapePowerShellSingleQuoted(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    return out;
}

bool WriteUtf16LeBomFile(const fs::path& path, const std::wstring& content, std::wstring& error) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        error = L"无法创建更新脚本";
        return false;
    }
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    if (!WriteFile(h, &bom, sizeof(bom), &written, nullptr)) {
        CloseHandle(h);
        error = L"无法写入更新脚本";
        return false;
    }
    const DWORD bytes = static_cast<DWORD>(content.size() * sizeof(wchar_t));
    if (!WriteFile(h, content.data(), bytes, &written, nullptr) || written != bytes) {
        CloseHandle(h);
        error = L"无法写入更新脚本";
        return false;
    }
    CloseHandle(h);
    return true;
}

bool WriteUpdaterScript(const fs::path& script, DWORD pid,
                        const fs::path& newExe, const fs::path& targetExe, std::wstring& error) {
    auto escape = [](const std::wstring& s) {
        std::wstring o;
        o.reserve(s.size() + 8);
        for (wchar_t c : s) {
            if (c == L'\'') o += L"''";
            else o.push_back(c);
        }
        return o;
    };

    // Robust Windows self-replace (UTF-16 LE script for PowerShell 5.1):
    // 1) wait until old process fully exits and file lock drops
    // 2) move old image to .old, copy new file, verify size
    // 3) relaunch with multiple fallbacks; never relaunch if replace failed
    std::wostringstream ps;
    ps
        << L"$ErrorActionPreference = 'Continue'\r\n"
        << L"$pidToWait = " << pid << L"\r\n"
        << L"$newExe = '" << escape(newExe.wstring()) << L"'\r\n"
        << L"$target = '" << escape(targetExe.wstring()) << L"'\r\n"
        << L"$bak = $target + '.old'\r\n"
        << L"$workDir = Split-Path -Parent $target\r\n"
        << L"$logDir = Join-Path $env:TEMP 'WindowLayoutUpdate'\r\n"
        << L"New-Item -ItemType Directory -Force -Path $logDir | Out-Null\r\n"
        << L"$log = Join-Path $logDir 'update.log'\r\n"
        << L"function Log([string]$m) { try { Add-Content -LiteralPath $log -Value ((Get-Date -Format o) + ' ' + $m) } catch {} }\r\n"
        << L"Log ('updater start pid=' + $pidToWait)\r\n"
        << L"Log ('new=' + $newExe)\r\n"
        << L"Log ('target=' + $target)\r\n"
        << L"for ($i = 0; $i -lt 200; $i++) {\r\n"
        << L"  if (-not (Get-Process -Id $pidToWait -ErrorAction SilentlyContinue)) { break }\r\n"
        << L"  Start-Sleep -Milliseconds 100\r\n"
        << L"}\r\n"
        << L"Start-Sleep -Milliseconds 800\r\n"
        << L"$ok = $false\r\n"
        << L"$lastErr = ''\r\n"
        << L"for ($i = 0; $i -lt 50; $i++) {\r\n"
        << L"  try {\r\n"
        << L"    if (-not (Test-Path -LiteralPath $newExe)) { throw 'new exe missing' }\r\n"
        << L"    $srcLen = [int64](Get-Item -LiteralPath $newExe).Length\r\n"
        << L"    if ($srcLen -lt 1024) { throw 'new exe too small' }\r\n"
        << L"    if (Test-Path -LiteralPath $bak) { Remove-Item -LiteralPath $bak -Force -ErrorAction Stop }\r\n"
        << L"    if (Test-Path -LiteralPath $target) { Move-Item -LiteralPath $target -Destination $bak -Force -ErrorAction Stop }\r\n"
        << L"    Copy-Item -LiteralPath $newExe -Destination $target -Force -ErrorAction Stop\r\n"
        << L"    $dstLen = [int64](Get-Item -LiteralPath $target).Length\r\n"
        << L"    if ($dstLen -ne $srcLen) { throw ('size mismatch src=' + $srcLen + ' dst=' + $dstLen) }\r\n"
        << L"    $ok = $true\r\n"
        << L"    Log ('replace ok size=' + $dstLen + ' try=' + $i)\r\n"
        << L"    break\r\n"
        << L"  } catch {\r\n"
        << L"    $lastErr = $_.Exception.Message\r\n"
        << L"    Log ('retry ' + $i + ' ' + $lastErr)\r\n"
        << L"    if ((-not (Test-Path -LiteralPath $target)) -and (Test-Path -LiteralPath $bak)) {\r\n"
        << L"      try { Move-Item -LiteralPath $bak -Destination $target -Force } catch { Log ('restore failed ' + $_.Exception.Message) }\r\n"
        << L"    }\r\n"
        << L"    Start-Sleep -Milliseconds 300\r\n"
        << L"  }\r\n"
        << L"}\r\n"
        << L"if ($ok) {\r\n"
        << L"  Start-Sleep -Milliseconds 400\r\n"
        << L"  $launched = $false\r\n"
        << L"  try {\r\n"
        << L"    $p = Start-Process -FilePath $target -WorkingDirectory $workDir -PassThru\r\n"
        << L"    if ($p) { $launched = $true; Log ('start Start-Process ok pid=' + $p.Id) }\r\n"
        << L"  } catch { Log ('Start-Process failed ' + $_.Exception.Message) }\r\n"
        << L"  if (-not $launched) {\r\n"
        << L"    try {\r\n"
        << L"      $args = '/c start \"\" \"' + $target + '\"'\r\n"
        << L"      Start-Process -FilePath $env:ComSpec -ArgumentList $args -WorkingDirectory $workDir -WindowStyle Hidden\r\n"
        << L"      $launched = $true\r\n"
        << L"      Log 'start cmd ok'\r\n"
        << L"    } catch { Log ('cmd start failed ' + $_.Exception.Message) }\r\n"
        << L"  }\r\n"
        << L"  if (-not $launched) {\r\n"
        << L"    try {\r\n"
        << L"      Start-Process -FilePath 'explorer.exe' -ArgumentList $target\r\n"
        << L"      $launched = $true\r\n"
        << L"      Log 'start explorer ok'\r\n"
        << L"    } catch { Log ('explorer start failed ' + $_.Exception.Message) }\r\n"
        << L"  }\r\n"
        << L"  if (-not $launched) {\r\n"
        << L"    try {\r\n"
        << L"      Add-Type -AssemblyName System.Windows.Forms\r\n"
        << L"      [System.Windows.Forms.MessageBox]::Show(\r\n"
        << L"        ('更新文件已替换，但自动启动失败。`r`n请手动打开：`r`n' + $target),\r\n"
        << L"        'WindowLayout 更新','OK','Warning') | Out-Null\r\n"
        << L"    } catch {}\r\n"
        << L"  }\r\n"
        << L"  Remove-Item -LiteralPath $bak -Force -ErrorAction SilentlyContinue\r\n"
        << L"  Remove-Item -LiteralPath $newExe -Force -ErrorAction SilentlyContinue\r\n"
        << L"} else {\r\n"
        << L"  Log ('FAILED ' + $lastErr)\r\n"
        << L"  try {\r\n"
        << L"    Add-Type -AssemblyName System.Windows.Forms\r\n"
        << L"    [System.Windows.Forms.MessageBox]::Show(\r\n"
        << L"      ('自动更新失败，未能替换程序文件。`r`n' + $lastErr + '`r`n`r`n请手动下载安装新版本。`r`n日志: ' + $log),\r\n"
        << L"      'WindowLayout 更新失败','OK','Error') | Out-Null\r\n"
        << L"  } catch {}\r\n"
        << L"}\r\n"
        << L"Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n";

    return WriteUtf16LeBomFile(script, ps.str(), error);
}

struct ReleasePick {
    std::wstring tag;
    std::wstring name;
    std::wstring downloadUrl;
    std::wstring assetName;
    bool prerelease = false;
};

bool IsPreferredUpdateZip(const std::string& aname) {
    // Prefer versioned or legacy zip packages used by the updater.
    if (aname.size() < 5) return false;
    if (aname.substr(aname.size() - 4) != ".zip") return false;
    if (aname == APP_RELEASE_ASSET) return true;
    if (aname.find("WindowLayout") != std::string::npos &&
        aname.find("windows-x64") != std::string::npos) {
        return true;
    }
    return aname.rfind("WindowLayout", 0) == 0;
}

// Pick the newest formal (v*) non-prerelease with a WindowLayout zip asset.
// Fallback: newest release (including prerelease) that has a zip.
bool PickRelease(const std::string& json, ReleasePick& out) {
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
            if (IsPreferredUpdateZip(aname) || url.find("windows-x64") != std::string::npos) {
                bestUrl = url;
                bestName = aname.empty() ? APP_RELEASE_ASSET : aname;
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
    // APP_VERSION must be a string literal from generated Version.h
    // (never a numeric compile definition — MSVC turns 0.1.x into 0).
    static_assert(sizeof(APP_VERSION) > 1, "APP_VERSION must be a non-empty string literal");
    const char* v = APP_VERSION;
    std::wstring out;
    out.reserve(16);
    for (; *v; ++v) {
        out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*v)));
    }
    return out;
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
    if (progress) progress(0, 0, L"正在下载更新...");

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

    // Stage outside extract dir so cleanup/extraction leftovers cannot interfere.
    fs::path staged = TempUpdateDir() / L"WindowLayout.exe.new";
    fs::remove(staged, ec);
    fs::copy_file(newExe, staged, fs::copy_options::overwrite_existing, ec);
    if (ec || !fs::exists(staged)) {
        error = L"无法准备更新文件";
        return false;
    }

    const auto stagedSize = fs::file_size(staged, ec);
    if (ec || stagedSize < 64) {
        error = L"更新文件无效";
        return false;
    }

    fs::path target = ExePath();
    // Refuse no-op if target path is missing (should not happen).
    if (target.empty()) {
        error = L"无法定位当前程序路径";
        return false;
    }

    fs::path script = TempUpdateDir() / L"apply_update.ps1";
    DWORD pid = GetCurrentProcessId();
    if (!WriteUpdaterScript(script, pid, staged, target, error)) return false;

    // Launch PowerShell directly (no cmd/start) so no console window flashes.
    const std::wstring ps = PowerShellExePath();
    const std::wstring args = L"-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File \""
        + script.wstring() + L"\"";
    std::wstring launchErr;
    if (!LaunchHiddenDetached(ps, args, launchErr)) {
        error = L"无法启动更新程序";
        return false;
    }
    // Brief head start before this process exits.
    Sleep(300);
    return true;
}

void UpdateManager::OpenInBrowser(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
