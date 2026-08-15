#include "MonitorManager.h"
#include <cstdio>

BOOL CALLBACK MonitorManager::EnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam) {
    auto* self = reinterpret_cast<MonitorManager*>(lParam);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        return TRUE;
    }

    MonitorInfo info;
    info.handle = hMon;
    info.index = static_cast<int>(self->monitors_.size());
    info.monitorRect = mi.rcMonitor;
    info.workRect = mi.rcWork;
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.name = mi.szDevice;

    wchar_t buf[256];
    const int w = mi.rcMonitor.right - mi.rcMonitor.left;
    const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    swprintf_s(buf, L"屏幕 %d%s  %dx%d  (%d,%d)",
               info.index + 1,
               info.primary ? L" [主]" : L"",
               w, h,
               mi.rcMonitor.left, mi.rcMonitor.top);
    info.displayName = buf;

    self->monitors_.push_back(info);
    return TRUE;
}

void MonitorManager::Refresh() {
    monitors_.clear();
    EnumDisplayMonitors(nullptr, nullptr, EnumProc, reinterpret_cast<LPARAM>(this));
}

const MonitorInfo* MonitorManager::FindByHandle(HMONITOR h) const {
    for (const auto& m : monitors_) {
        if (m.handle == h) return &m;
    }
    return nullptr;
}

const MonitorInfo* MonitorManager::FindByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(monitors_.size())) return nullptr;
    return &monitors_[index];
}

bool MonitorManager::PlaceWindow(HWND hwnd, const MonitorInfo& mon, int x, int y, int w, int h, bool maximize) {
    if (!IsWindow(hwnd)) return false;

    ShowWindow(hwnd, SW_RESTORE);

    const int absX = mon.workRect.left + x;
    const int absY = mon.workRect.top + y;

    if (w < 50) w = 50;
    if (h < 50) h = 50;

    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    if (!SetWindowPos(hwnd, nullptr, absX, absY, w, h, flags)) {
        return false;
    }

    if (maximize) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    return true;
}

bool MonitorManager::CalcPresetLayout(const MonitorInfo& mon, int preset,
                                      int& x, int& y, int& w, int& h, bool& maximize) {
    const RECT& work = mon.workRect;
    const int fullW = work.right - work.left;
    const int fullH = work.bottom - work.top;
    const int halfW = fullW / 2;
    const int halfH = fullH / 2;

    x = 0;
    y = 0;
    w = fullW;
    h = fullH;
    maximize = false;

    switch (preset) {
    case Maximize:
        maximize = true;
        break;
    case LeftHalf:
        w = halfW;
        break;
    case RightHalf:
        x = halfW;
        w = fullW - halfW;
        break;
    case TopHalf:
        h = halfH;
        break;
    case BottomHalf:
        y = halfH;
        h = fullH - halfH;
        break;
    case Center:
        w = fullW * 2 / 3;
        h = fullH * 2 / 3;
        x = (fullW - w) / 2;
        y = (fullH - h) / 2;
        break;
    case TopLeft:
        w = halfW;
        h = halfH;
        break;
    case TopRight:
        x = halfW;
        w = fullW - halfW;
        h = halfH;
        break;
    case BottomLeft:
        y = halfH;
        w = halfW;
        h = fullH - halfH;
        break;
    case BottomRight:
        x = halfW;
        y = halfH;
        w = fullW - halfW;
        h = fullH - halfH;
        break;
    case Custom:
        return false;
    default:
        return false;
    }
    return true;
}

bool MonitorManager::PlaceWindowPreset(HWND hwnd, const MonitorInfo& mon, int preset) {
    if (!IsWindow(hwnd)) return false;
    int x = 0, y = 0, w = 0, h = 0;
    bool maximize = false;
    if (!CalcPresetLayout(mon, preset, x, y, w, h, maximize)) return false;
    return PlaceWindow(hwnd, mon, x, y, w, h, maximize);
}

const wchar_t* MonitorManager::PresetName(int preset) {
    switch (preset) {
    case Maximize:     return L"最大化";
    case LeftHalf:     return L"左半屏";
    case RightHalf:    return L"右半屏";
    case TopHalf:      return L"上半屏";
    case BottomHalf:   return L"下半屏";
    case Center:       return L"居中";
    case TopLeft:      return L"左上角";
    case TopRight:     return L"右上角";
    case BottomLeft:   return L"左下角";
    case BottomRight:  return L"右下角";
    case Custom:       return L"自定义坐标";
    default:           return L"未知";
    }
}
