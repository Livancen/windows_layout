#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <cstdint>

struct UpdateInfo {
    bool available = false;
    std::wstring currentVersion;
    std::wstring latestVersion;
    std::wstring tagName;
    std::wstring assetName;
    std::wstring downloadUrl;
    std::wstring releasePageUrl;
    std::wstring releaseName;
    std::wstring error;
};

class UpdateManager {
public:
    // received / total (total may be 0 if unknown)
    using ProgressFn = std::function<void(std::uint64_t received, std::uint64_t total, const wchar_t* status)>;

    static std::wstring CurrentVersion();
    static std::wstring ReleasesPageUrl();

    // Query GitHub Releases for the newest package (prefers formal v* tags).
    static UpdateInfo CheckForUpdate();

    // Download release zip to a temp file. Returns empty path on failure.
    static std::wstring DownloadUpdate(const UpdateInfo& info, ProgressFn progress, std::wstring& error);

    // Extract exe, stage replace-on-exit script, then quit the app to finish update.
    static bool ApplyAndRestart(const std::wstring& zipPath, std::wstring& error);

    static void OpenInBrowser(const std::wstring& url);
};
