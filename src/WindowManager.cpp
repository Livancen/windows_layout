#include "WindowManager.h"
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

std::wstring WindowManager::GetWindowTitle(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return L"";
    std::wstring title(static_cast<size_t>(len) + 1, L'\0');
    int written = GetWindowTextW(hwnd, title.data(), len + 1);
    title.resize(written > 0 ? written : 0);
    return title;
}

std::wstring WindowManager::GetClassNameStr(HWND hwnd) {
    wchar_t buf[256]{};
    int n = GetClassNameW(hwnd, buf, 256);
    return n > 0 ? std::wstring(buf, n) : L"";
}

std::wstring WindowManager::GetProcessName(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        const wchar_t* base = path;
        for (const wchar_t* p = path; *p; ++p) {
            if (*p == L'\\' || *p == L'/') base = p + 1;
        }
        name = base;
    }
    CloseHandle(hProcess);
    return name;
}

bool WindowManager::IsAltTabWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;

    // Skip tool windows
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;

    // Skip windows owned by other windows (unless app window style)
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (owner && !(exStyle & WS_EX_APPWINDOW)) return false;

    // Skip cloaked UWP windows (invisible shells)
    BOOL cloaked = FALSE;
    // DwmGetWindowAttribute optional - skip if not linked, simple title check instead

    std::wstring title = GetWindowTitle(hwnd);
    if (title.empty()) return false;

    // Skip our own app later by class name if needed
    std::wstring cls = GetClassNameStr(hwnd);
    if (cls == L"WindowLayoutMainClass") return false;
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") return false;

    return true;
}

BOOL CALLBACK WindowManager::EnumProc(HWND hwnd, LPARAM lParam) {
    auto* self = reinterpret_cast<WindowManager*>(lParam);
    if (!IsAltTabWindow(hwnd)) return TRUE;

    AppWindow win;
    win.hwnd = hwnd;
    win.title = GetWindowTitle(hwnd);
    win.className = GetClassNameStr(hwnd);
    GetWindowThreadProcessId(hwnd, &win.pid);
    win.processName = GetProcessName(win.pid);
    GetWindowRect(hwnd, &win.rect);
    win.visible = IsWindowVisible(hwnd) != FALSE;
    win.minimized = IsIconic(hwnd) != FALSE;
    win.maximized = IsZoomed(hwnd) != FALSE;

    self->windows_.push_back(win);
    return TRUE;
}

void WindowManager::Refresh() {
    windows_.clear();
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(this));
}

const AppWindow* WindowManager::FindByHwnd(HWND hwnd) const {
    for (const auto& w : windows_) {
        if (w.hwnd == hwnd) return &w;
    }
    return nullptr;
}

bool WindowManager::MoveResize(HWND hwnd, int x, int y, int w, int h) {
    if (!IsWindow(hwnd)) return false;
    if (IsIconic(hwnd) || IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    if (w < 50) w = 50;
    if (h < 50) h = 50;
    return SetWindowPos(hwnd, nullptr, x, y, w, h,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE;
}

bool WindowManager::Maximize(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;
    return ShowWindow(hwnd, SW_MAXIMIZE) != FALSE;
}

bool WindowManager::Restore(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;
    return ShowWindow(hwnd, SW_RESTORE) != FALSE;
}

bool WindowManager::BringToFront(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    return SetForegroundWindow(hwnd) != FALSE;
}

bool WindowManager::GetRect(HWND hwnd, RECT& out) {
    if (!IsWindow(hwnd)) return false;
    return GetWindowRect(hwnd, &out) != FALSE;
}
