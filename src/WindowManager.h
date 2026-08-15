#pragma once

#include <windows.h>
#include <vector>
#include <string>

struct AppWindow {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    std::wstring title;
    std::wstring className;
    std::wstring processName;
    RECT rect{};
    bool visible = false;
    bool minimized = false;
    bool maximized = false;
};

class WindowManager {
public:
    void Refresh();
    const std::vector<AppWindow>& Windows() const { return windows_; }
    const AppWindow* FindByHwnd(HWND hwnd) const;

    static bool MoveResize(HWND hwnd, int x, int y, int w, int h);
    static bool Maximize(HWND hwnd);
    static bool Restore(HWND hwnd);
    static bool BringToFront(HWND hwnd);
    static bool GetRect(HWND hwnd, RECT& out);

private:
    static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam);
    static bool IsAltTabWindow(HWND hwnd);
    static std::wstring GetProcessName(DWORD pid);
    static std::wstring GetWindowTitle(HWND hwnd);
    static std::wstring GetClassNameStr(HWND hwnd);

    std::vector<AppWindow> windows_;
};
