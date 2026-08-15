#pragma once

#include <windows.h>
#include <commctrl.h>
#include "WindowManager.h"
#include "MonitorManager.h"

class App {
public:
    static App& Instance();
    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    App() = default;

    bool CreateMainWindow(int nCmdShow);
    void CreateControls();
    void LayoutControls(int width, int height);
    void RefreshAll();
    void PopulateWindowList();
    void PopulateMonitorCombo();
    void PopulatePresetCombo();
    void OnWindowSelect();
    void OnApply();
    void OnPresetChanged();
    void FillCoordsFromSelection();
    void FillCoordsFromPreset();
    void SetCoordInputs(int x, int y, int w, int h);
    void SelectWindowByHwnd(HWND hwnd);
    void RestoreSelection();
    void RefreshListKeepSelection();
    HWND SelectedWindow() const;
    const MonitorInfo* SelectedMonitor() const;
    int SelectedPreset() const;
    void SetStatus(const wchar_t* text);
    void GetCoordInputs(int& x, int& y, int& w, int& h) const;
    LRESULT OnListCustomDraw(LPNMLVCUSTOMDRAW cd);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInst_ = nullptr;
    HWND hwndMain_ = nullptr;
    HWND hwndList_ = nullptr;
    HWND hwndMonitors_ = nullptr;
    HWND hwndPresets_ = nullptr;
    HWND hwndEditX_ = nullptr;
    HWND hwndEditY_ = nullptr;
    HWND hwndEditW_ = nullptr;
    HWND hwndEditH_ = nullptr;
    HWND hwndBtnApply_ = nullptr;
    HWND hwndStatus_ = nullptr;
    HWND hwndLabelMon_ = nullptr;
    HWND hwndLabelPreset_ = nullptr;
    HWND hwndLabelX_ = nullptr;
    HWND hwndLabelY_ = nullptr;
    HWND hwndLabelW_ = nullptr;
    HWND hwndLabelH_ = nullptr;

    WindowManager windowMgr_;
    MonitorManager monitorMgr_;
    HWND selectedHwnd_ = nullptr;
    bool quietSelect_ = false;
    bool quietEdit_ = false;

    static constexpr wchar_t kClassName[] = L"WindowLayoutMainClass";
    static constexpr int kIdList = 1001;
    static constexpr int kIdMonitors = 1002;
    static constexpr int kIdPresets = 1003;
    static constexpr int kIdEditX = 1004;
    static constexpr int kIdEditY = 1005;
    static constexpr int kIdEditW = 1006;
    static constexpr int kIdEditH = 1007;
    static constexpr int kIdBtnApply = 1009;
    static constexpr int kIdStatus = 1011;
    static constexpr UINT_PTR kTimerRefresh = 1;
    static constexpr UINT kRefreshIntervalMs = 2000;
};
