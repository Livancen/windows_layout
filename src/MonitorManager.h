#pragma once

#include <windows.h>
#include <vector>
#include <string>

struct MonitorInfo {
    HMONITOR handle = nullptr;
    int index = 0;
    RECT monitorRect{};
    RECT workRect{};
    bool primary = false;
    std::wstring name;
    std::wstring displayName;
};

class MonitorManager {
public:
    void Refresh();
    const std::vector<MonitorInfo>& Monitors() const { return monitors_; }
    const MonitorInfo* FindByHandle(HMONITOR h) const;
    const MonitorInfo* FindByIndex(int index) const;
    int Count() const { return static_cast<int>(monitors_.size()); }

    // Place window on monitor with relative offset inside work area
    static bool PlaceWindow(HWND hwnd, const MonitorInfo& mon, int x, int y, int w, int h, bool maximize = false);
    static bool PlaceWindowPreset(HWND hwnd, const MonitorInfo& mon, int preset);
    static bool CalcPresetLayout(const MonitorInfo& mon, int preset, int& x, int& y, int& w, int& h, bool& maximize);

    // preset: 0=maximize, 1=left half, 2=right half, 3=top half, 4=bottom half,
    //         5=center, 6=top-left, 7=top-right, 8=bottom-left, 9=bottom-right
    enum Preset {
        Maximize = 0,
        LeftHalf,
        RightHalf,
        TopHalf,
        BottomHalf,
        Center,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Custom,
        PresetCount
    };

    static const wchar_t* PresetName(int preset);

private:
    static BOOL CALLBACK EnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam);
    std::vector<MonitorInfo> monitors_;
};
