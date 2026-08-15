#include "App.h"
#include "Version.h"
#include "resource.h"
#include <cstdio>
#include <string>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

struct UpdateDownloadResult {
    UpdateInfo info;
    std::wstring zipPath;
    std::wstring error;
    bool ok = false;
};

App& App::Instance() {
    static App app;
    return app;
}

int App::Run(HINSTANCE hInstance, int nCmdShow) {
    hInst_ = hInstance;

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    HICON hIcon = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    HICON hIconSm = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    if (!hIcon) hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    if (!hIconSm) hIconSm = hIcon;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = hIcon;
    wc.hIconSm = hIconSm;

    if (!RegisterClassExW(&wc)) return 1;
    if (!CreateMainWindow(nCmdShow)) return 1;

    RefreshAll();
    SetTimer(hwndMain_, kTimerRefresh, kRefreshIntervalMs, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

bool App::CreateMainWindow(int nCmdShow) {
    wchar_t title[128];
    swprintf_s(title, L"窗口布局管理器  v%ls", UpdateManager::CurrentVersion().c_str());

    hwndMain_ = CreateWindowExW(
        0, kClassName, title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 640,
        nullptr, nullptr, hInst_, this);

    if (!hwndMain_) return false;

    ShowWindow(hwndMain_, nCmdShow);
    UpdateWindow(hwndMain_);
    return true;
}

void App::CreateControls() {
    const DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
    hwndList_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                listStyle, 0, 0, 0, 0, hwndMain_,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)), hInst_, nullptr);

    ListView_SetExtendedListViewStyle(hwndList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = const_cast<LPWSTR>(L"标题");
    col.cx = 280;
    col.iSubItem = 0;
    ListView_InsertColumn(hwndList_, 0, &col);

    col.pszText = const_cast<LPWSTR>(L"进程");
    col.cx = 140;
    col.iSubItem = 1;
    ListView_InsertColumn(hwndList_, 1, &col);

    col.pszText = const_cast<LPWSTR>(L"位置");
    col.cx = 160;
    col.iSubItem = 2;
    ListView_InsertColumn(hwndList_, 2, &col);

    col.pszText = const_cast<LPWSTR>(L"大小");
    col.cx = 100;
    col.iSubItem = 3;
    ListView_InsertColumn(hwndList_, 3, &col);

    col.pszText = const_cast<LPWSTR>(L"状态");
    col.cx = 80;
    col.iSubItem = 4;
    ListView_InsertColumn(hwndList_, 4, &col);

    hwndLabelMon_ = CreateWindowW(L"STATIC", L"目标屏幕:",
                                  WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndMonitors_ = CreateWindowW(L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  0, 0, 0, 0, hwndMain_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdMonitors)), hInst_, nullptr);

    hwndLabelPreset_ = CreateWindowW(L"STATIC", L"预设布局:",
                                     WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndPresets_ = CreateWindowW(L"COMBOBOX", L"",
                                 WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 0, 0, 0, 0, hwndMain_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPresets)), hInst_, nullptr);

    hwndLabelX_ = CreateWindowW(L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndEditX_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                                 WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                 0, 0, 0, 0, hwndMain_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEditX)), hInst_, nullptr);

    hwndLabelY_ = CreateWindowW(L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndEditY_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                                 WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                 0, 0, 0, 0, hwndMain_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEditY)), hInst_, nullptr);

    hwndLabelW_ = CreateWindowW(L"STATIC", L"宽:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndEditW_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"800",
                                 WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                 0, 0, 0, 0, hwndMain_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEditW)), hInst_, nullptr);

    hwndLabelH_ = CreateWindowW(L"STATIC", L"高:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndMain_, nullptr, hInst_, nullptr);
    hwndEditH_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"600",
                                 WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                 0, 0, 0, 0, hwndMain_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEditH)), hInst_, nullptr);

    hwndBtnApply_ = CreateWindowW(L"BUTTON", L"应用",
                                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  0, 0, 0, 0, hwndMain_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnApply)), hInst_, nullptr);

    hwndBtnUpdate_ = CreateWindowW(L"BUTTON", L"检查更新",
                                   WS_CHILD | WS_VISIBLE,
                                   0, 0, 0, 0, hwndMain_,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnUpdate)), hInst_, nullptr);

    hwndStatus_ = CreateWindowW(STATUSCLASSNAMEW, L"就绪",
                                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                0, 0, 0, 0, hwndMain_,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)), hInst_, nullptr);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HWND controls[] = {
        hwndList_, hwndMonitors_, hwndPresets_,
        hwndEditX_, hwndEditY_, hwndEditW_, hwndEditH_,
        hwndBtnApply_, hwndBtnUpdate_,
        hwndLabelMon_, hwndLabelPreset_,
        hwndLabelX_, hwndLabelY_, hwndLabelW_, hwndLabelH_
    };
    for (HWND h : controls) {
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void App::LayoutControls(int width, int height) {
    SendMessageW(hwndStatus_, WM_SIZE, 0, 0);

    RECT statusRect{};
    GetWindowRect(hwndStatus_, &statusRect);
    const int statusH = statusRect.bottom - statusRect.top;
    const int margin = 10;
    const int rightPanelW = 280;
    const int topBarH = 0;

    const int listW = width - rightPanelW - margin * 3;
    const int listH = height - statusH - margin * 2 - topBarH;

    SetWindowPos(hwndList_, nullptr, margin, margin, listW, listH, SWP_NOZORDER);

    int rx = margin * 2 + listW;
    int ry = margin;
    const int labelH = 20;
    const int editH = 24;
    const int comboH = 200;
    const int btnH = 32;
    const int gap = 8;
    const int fieldW = rightPanelW - 20;

    SetWindowPos(hwndLabelMon_, nullptr, rx, ry, fieldW, labelH, SWP_NOZORDER);
    ry += labelH + 2;
    SetWindowPos(hwndMonitors_, nullptr, rx, ry, fieldW, comboH, SWP_NOZORDER);
    ry += editH + gap + 4;

    SetWindowPos(hwndLabelPreset_, nullptr, rx, ry, fieldW, labelH, SWP_NOZORDER);
    ry += labelH + 2;
    SetWindowPos(hwndPresets_, nullptr, rx, ry, fieldW, comboH, SWP_NOZORDER);
    ry += editH + gap + 4;

    // coordinate row
    const int colW = (fieldW - gap) / 2;
    SetWindowPos(hwndLabelX_, nullptr, rx, ry, 30, labelH, SWP_NOZORDER);
    SetWindowPos(hwndEditX_, nullptr, rx + 30, ry, colW - 30, editH, SWP_NOZORDER);
    SetWindowPos(hwndLabelY_, nullptr, rx + colW + gap, ry, 30, labelH, SWP_NOZORDER);
    SetWindowPos(hwndEditY_, nullptr, rx + colW + gap + 30, ry, colW - 30, editH, SWP_NOZORDER);
    ry += editH + gap;

    SetWindowPos(hwndLabelW_, nullptr, rx, ry, 30, labelH, SWP_NOZORDER);
    SetWindowPos(hwndEditW_, nullptr, rx + 30, ry, colW - 30, editH, SWP_NOZORDER);
    SetWindowPos(hwndLabelH_, nullptr, rx + colW + gap, ry, 30, labelH, SWP_NOZORDER);
    SetWindowPos(hwndEditH_, nullptr, rx + colW + gap + 30, ry, colW - 30, editH, SWP_NOZORDER);
    ry += editH + gap * 2;

    SetWindowPos(hwndBtnApply_, nullptr, rx, ry, fieldW, btnH, SWP_NOZORDER);
    ry += btnH + gap;
    SetWindowPos(hwndBtnUpdate_, nullptr, rx, ry, fieldW, btnH, SWP_NOZORDER);
}

void App::RefreshAll() {
    const int prevMon = static_cast<int>(SendMessageW(hwndMonitors_, CB_GETCURSEL, 0, 0));
    const int prevPreset = static_cast<int>(SendMessageW(hwndPresets_, CB_GETCURSEL, 0, 0));
    const bool presetsEmpty = SendMessageW(hwndPresets_, CB_GETCOUNT, 0, 0) == 0;

    monitorMgr_.Refresh();
    windowMgr_.Refresh();
    PopulateMonitorCombo();
    if (presetsEmpty) {
        PopulatePresetCombo();
    } else if (prevPreset >= 0) {
        SendMessageW(hwndPresets_, CB_SETCURSEL, prevPreset, 0);
    }
    if (prevMon >= 0 && prevMon < monitorMgr_.Count()) {
        SendMessageW(hwndMonitors_, CB_SETCURSEL, prevMon, 0);
    }
    PopulateWindowList();
    RestoreSelection();

    wchar_t buf[128];
    swprintf_s(buf, L"%d 个窗口 · %d 块屏幕 · 自动刷新中",
               static_cast<int>(windowMgr_.Windows().size()),
               monitorMgr_.Count());
    SetStatus(buf);
}

void App::RefreshListKeepSelection() {
    windowMgr_.Refresh();
    PopulateWindowList();
    RestoreSelection();
}

void App::RestoreSelection() {
    if (selectedHwnd_ && !IsWindow(selectedHwnd_)) {
        selectedHwnd_ = nullptr;
    }
    if (!selectedHwnd_) return;

    quietSelect_ = true;
    SelectWindowByHwnd(selectedHwnd_);
    quietSelect_ = false;
    InvalidateRect(hwndList_, nullptr, FALSE);
}

void App::PopulateWindowList() {
    ListView_DeleteAllItems(hwndList_);

    int i = 0;
    for (const auto& win : windowMgr_.Windows()) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.pszText = const_cast<LPWSTR>(win.title.c_str());
        item.lParam = reinterpret_cast<LPARAM>(win.hwnd);
        ListView_InsertItem(hwndList_, &item);

        ListView_SetItemText(hwndList_, i, 1, const_cast<LPWSTR>(win.processName.c_str()));

        wchar_t posBuf[64];
        swprintf_s(posBuf, L"%d, %d", win.rect.left, win.rect.top);
        ListView_SetItemText(hwndList_, i, 2, posBuf);

        wchar_t sizeBuf[64];
        swprintf_s(sizeBuf, L"%dx%d",
                   win.rect.right - win.rect.left,
                   win.rect.bottom - win.rect.top);
        ListView_SetItemText(hwndList_, i, 3, sizeBuf);

        const wchar_t* state = L"正常";
        if (win.minimized) state = L"最小化";
        else if (win.maximized) state = L"最大化";
        ListView_SetItemText(hwndList_, i, 4, const_cast<LPWSTR>(state));

        ++i;
    }
}

void App::SelectWindowByHwnd(HWND hwnd) {
    if (!hwnd) return;
    selectedHwnd_ = hwnd;

    ListView_SetItemState(hwndList_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

    const int count = ListView_GetItemCount(hwndList_);
    for (int i = 0; i < count; ++i) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (ListView_GetItem(hwndList_, &item) &&
            reinterpret_cast<HWND>(item.lParam) == hwnd) {
            ListView_SetItemState(hwndList_, i, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(hwndList_, i, FALSE);
            return;
        }
    }
}

void App::PopulateMonitorCombo() {
    const int prev = static_cast<int>(SendMessageW(hwndMonitors_, CB_GETCURSEL, 0, 0));
    SendMessageW(hwndMonitors_, CB_RESETCONTENT, 0, 0);
    for (const auto& mon : monitorMgr_.Monitors()) {
        SendMessageW(hwndMonitors_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mon.displayName.c_str()));
    }
    if (monitorMgr_.Count() > 0) {
        const int sel = (prev >= 0 && prev < monitorMgr_.Count()) ? prev : 0;
        SendMessageW(hwndMonitors_, CB_SETCURSEL, sel, 0);
    }
}

void App::PopulatePresetCombo() {
    SendMessageW(hwndPresets_, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < MonitorManager::PresetCount; ++i) {
        SendMessageW(hwndPresets_, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(MonitorManager::PresetName(i)));
    }
    SendMessageW(hwndPresets_, CB_SETCURSEL, MonitorManager::LeftHalf, 0);
}

HWND App::SelectedWindow() const {
    if (selectedHwnd_ && IsWindow(selectedHwnd_)) {
        return selectedHwnd_;
    }

    int sel = ListView_GetNextItem(hwndList_, -1, LVNI_SELECTED);
    if (sel < 0) return nullptr;
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    if (!ListView_GetItem(hwndList_, &item)) return nullptr;
    HWND hwnd = reinterpret_cast<HWND>(item.lParam);
    return IsWindow(hwnd) ? hwnd : nullptr;
}

const MonitorInfo* App::SelectedMonitor() const {
    int sel = static_cast<int>(SendMessageW(hwndMonitors_, CB_GETCURSEL, 0, 0));
    return monitorMgr_.FindByIndex(sel);
}

int App::SelectedPreset() const {
    return static_cast<int>(SendMessageW(hwndPresets_, CB_GETCURSEL, 0, 0));
}

void App::SetStatus(const wchar_t* text) {
    SendMessageW(hwndStatus_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text));
}

void App::GetCoordInputs(int& x, int& y, int& w, int& h) const {
    wchar_t buf[32]{};
    GetWindowTextW(hwndEditX_, buf, 32); x = _wtoi(buf);
    GetWindowTextW(hwndEditY_, buf, 32); y = _wtoi(buf);
    GetWindowTextW(hwndEditW_, buf, 32); w = _wtoi(buf);
    GetWindowTextW(hwndEditH_, buf, 32); h = _wtoi(buf);
}

void App::FillCoordsFromSelection() {
    HWND hwnd = SelectedWindow();
    if (!hwnd) return;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return;

    // auto-select monitor containing window
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (const auto& m : monitorMgr_.Monitors()) {
        if (m.handle == hMon) {
            SendMessageW(hwndMonitors_, CB_SETCURSEL, m.index, 0);
            break;
        }
    }

    const MonitorInfo* mon = SelectedMonitor();
    int x = rc.left;
    int y = rc.top;
    if (mon) {
        x = rc.left - mon->workRect.left;
        y = rc.top - mon->workRect.top;
    }

    SetCoordInputs(x, y, rc.right - rc.left, rc.bottom - rc.top);
    SendMessageW(hwndPresets_, CB_SETCURSEL, MonitorManager::Custom, 0);
}

void App::OnWindowSelect() {
    if (quietSelect_) return;

    int sel = ListView_GetNextItem(hwndList_, -1, LVNI_SELECTED);
    if (sel < 0) {
        return;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    if (!ListView_GetItem(hwndList_, &item)) return;

    selectedHwnd_ = reinterpret_cast<HWND>(item.lParam);
    FillCoordsFromSelection();

    if (!selectedHwnd_ || !IsWindow(selectedHwnd_)) {
        selectedHwnd_ = nullptr;
        SetStatus(L"未选择窗口");
        return;
    }

    const AppWindow* info = windowMgr_.FindByHwnd(selectedHwnd_);
    if (info) {
        wchar_t buf[256];
        swprintf_s(buf, L"已选择: %ls (%ls)", info->title.c_str(), info->processName.c_str());
        SetStatus(buf);
    }
    InvalidateRect(hwndList_, nullptr, FALSE);
}

void App::SetCoordInputs(int x, int y, int w, int h) {
    quietEdit_ = true;
    wchar_t buf[32];
    swprintf_s(buf, L"%d", x); SetWindowTextW(hwndEditX_, buf);
    swprintf_s(buf, L"%d", y); SetWindowTextW(hwndEditY_, buf);
    swprintf_s(buf, L"%d", w); SetWindowTextW(hwndEditW_, buf);
    swprintf_s(buf, L"%d", h); SetWindowTextW(hwndEditH_, buf);
    quietEdit_ = false;
}

void App::FillCoordsFromPreset() {
    const MonitorInfo* mon = SelectedMonitor();
    int preset = SelectedPreset();
    if (!mon || preset < 0 || preset == MonitorManager::Custom) return;

    int x = 0, y = 0, w = 0, h = 0;
    bool maximize = false;
    if (!MonitorManager::CalcPresetLayout(*mon, preset, x, y, w, h, maximize)) return;
    SetCoordInputs(x, y, w, h);
}

void App::OnPresetChanged() {
    FillCoordsFromPreset();
}

void App::OnApply() {
    HWND hwnd = SelectedWindow();
    const MonitorInfo* mon = SelectedMonitor();
    int preset = SelectedPreset();
    if (!hwnd) {
        SetStatus(L"请先选择一个窗口");
        return;
    }
    if (!mon) {
        SetStatus(L"请选择目标屏幕");
        return;
    }
    if (preset < 0) {
        SetStatus(L"请选择布局方式");
        return;
    }

    bool ok = false;
    if (preset == MonitorManager::Custom) {
        int x, y, w, h;
        GetCoordInputs(x, y, w, h);
        ok = MonitorManager::PlaceWindow(hwnd, *mon, x, y, w, h, false);
    } else {
        ok = MonitorManager::PlaceWindowPreset(hwnd, *mon, preset);
    }

    if (ok) {
        selectedHwnd_ = hwnd;
        wchar_t buf[160];
        swprintf_s(buf, L"已应用: %ls → %ls",
                   MonitorManager::PresetName(preset), mon->displayName.c_str());
        SetStatus(buf);
        RefreshListKeepSelection();
        if (preset != MonitorManager::Custom) {
            FillCoordsFromPreset();
        }
        SetFocus(hwndBtnApply_);
    } else {
        SetStatus(L"应用失败（窗口可能已关闭或无权操作）");
    }
}

void App::SetUpdateBusy(bool busy) {
    updateBusy_ = busy;
    EnableWindow(hwndBtnUpdate_, busy ? FALSE : TRUE);
    EnableWindow(hwndBtnApply_, busy ? FALSE : TRUE);
}

void App::OnCheckUpdate() {
    if (updateBusy_) return;
    SetUpdateBusy(true);
    SetStatus(L"正在检查更新...");

    HWND hwnd = hwndMain_;
    std::thread([hwnd]() {
        auto* info = new UpdateInfo(UpdateManager::CheckForUpdate());
        PostMessageW(hwnd, WM_APP_UPDATE_CHECK, 0, reinterpret_cast<LPARAM>(info));
    }).detach();
}

void App::OnUpdateCheckDone(UpdateInfo* info) {
    if (!info) {
        SetUpdateBusy(false);
        return;
    }

    if (!info->error.empty() && info->downloadUrl.empty()) {
        wchar_t msg[512];
        swprintf_s(msg,
                   L"检查更新失败：%ls\n\n是否在浏览器中打开发布页手动下载？\n%ls",
                   info->error.c_str(), info->releasePageUrl.c_str());
        SetStatus(L"检查更新失败");
        if (MessageBoxW(hwndMain_, msg, L"检查更新", MB_YESNO | MB_ICONWARNING) == IDYES) {
            UpdateManager::OpenInBrowser(info->releasePageUrl);
        }
        delete info;
        SetUpdateBusy(false);
        return;
    }

    if (!info->available) {
        wchar_t msg[256];
        swprintf_s(msg, L"当前已是最新版本。\n\n当前版本：%ls\n最新版本：%ls",
                   info->currentVersion.c_str(),
                   info->latestVersion.empty() ? info->currentVersion.c_str() : info->latestVersion.c_str());
        SetStatus(L"已是最新版本");
        MessageBoxW(hwndMain_, msg, L"检查更新", MB_OK | MB_ICONINFORMATION);
        delete info;
        SetUpdateBusy(false);
        return;
    }

    wchar_t msg[512];
    swprintf_s(msg,
               L"发现新版本！\n\n当前版本：%ls\n最新版本：%ls\n发布：%ls\n\n是否立即下载并安装？",
               info->currentVersion.c_str(),
               info->latestVersion.c_str(),
               info->releaseName.c_str());
    SetStatus(L"发现新版本");
    if (MessageBoxW(hwndMain_, msg, L"检查更新", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        delete info;
        SetUpdateBusy(false);
        return;
    }

    SetStatus(L"正在下载更新...");
    HWND hwnd = hwndMain_;
    std::thread([hwnd, info]() {
        auto* result = new UpdateDownloadResult();
        result->info = *info;
        delete info;
        std::wstring err;
        result->zipPath = UpdateManager::DownloadUpdate(result->info, nullptr, err);
        result->error = err;
        result->ok = !result->zipPath.empty();
        PostMessageW(hwnd, WM_APP_UPDATE_DOWNLOAD, 0, reinterpret_cast<LPARAM>(result));
    }).detach();
}

void App::OnUpdateDownloadDone(UpdateDownloadResult* result) {
    if (!result) {
        SetUpdateBusy(false);
        return;
    }

    if (!result->ok) {
        const std::wstring page = result->info.releasePageUrl.empty()
            ? UpdateManager::ReleasesPageUrl()
            : result->info.releasePageUrl;
        const std::wstring direct = result->info.downloadUrl;

        wchar_t msg[768];
        swprintf_s(msg,
                   L"下载更新失败：%ls\n\n你可以在浏览器中手动下载安装包。\n是否打开下载页面？",
                   result->error.empty() ? L"未知错误" : result->error.c_str());
        SetStatus(L"下载更新失败");
        if (MessageBoxW(hwndMain_, msg, L"更新失败", MB_YESNO | MB_ICONWARNING) == IDYES) {
            if (!direct.empty()) {
                UpdateManager::OpenInBrowser(direct);
            } else {
                UpdateManager::OpenInBrowser(page);
            }
        }
        delete result;
        SetUpdateBusy(false);
        return;
    }

    std::wstring err;
    if (!UpdateManager::ApplyAndRestart(result->zipPath, err)) {
        wchar_t msg[768];
        swprintf_s(msg,
                   L"安装更新失败：%ls\n\n是否在浏览器中打开下载页手动安装？",
                   err.empty() ? L"未知错误" : err.c_str());
        SetStatus(L"安装更新失败");
        if (MessageBoxW(hwndMain_, msg, L"更新失败", MB_YESNO | MB_ICONWARNING) == IDYES) {
            if (!result->info.downloadUrl.empty()) {
                UpdateManager::OpenInBrowser(result->info.downloadUrl);
            } else {
                UpdateManager::OpenInBrowser(result->info.releasePageUrl);
            }
        }
        delete result;
        SetUpdateBusy(false);
        return;
    }

    SetStatus(L"即将重启以完成更新...");
    delete result;
    DestroyWindow(hwndMain_);
}

LRESULT App::OnListCustomDraw(LPNMLVCUSTOMDRAW cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: {
        const int index = static_cast<int>(cd->nmcd.dwItemSpec);
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = index;
        ListView_GetItem(hwndList_, &item);
        const HWND rowHwnd = reinterpret_cast<HWND>(item.lParam);
        const bool selected = (selectedHwnd_ && rowHwnd == selectedHwnd_) ||
            (ListView_GetItemState(hwndList_, index, LVIS_SELECTED) & LVIS_SELECTED);

        if (selected) {
            cd->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
            cd->clrTextBk = GetSysColor(COLOR_HIGHLIGHT);
            return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
        }
        return CDRF_DODEFAULT;
    }

    case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
        const int index = static_cast<int>(cd->nmcd.dwItemSpec);
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = index;
        ListView_GetItem(hwndList_, &item);
        const HWND rowHwnd = reinterpret_cast<HWND>(item.lParam);
        const bool selected = (selectedHwnd_ && rowHwnd == selectedHwnd_) ||
            (ListView_GetItemState(hwndList_, index, LVIS_SELECTED) & LVIS_SELECTED);

        if (selected) {
            cd->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
            cd->clrTextBk = GetSysColor(COLOR_HIGHLIGHT);
            return CDRF_NEWFONT;
        }
        return CDRF_DODEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwndMain_ = hwnd;
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        LayoutControls(w, h);
        return 0;
    }

    case WM_TIMER:
        if (wParam == kTimerRefresh) {
            RefreshAll();
        }
        return 0;

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == kIdBtnApply && code == BN_CLICKED) {
            OnApply();
        } else if (id == kIdBtnUpdate && code == BN_CLICKED) {
            OnCheckUpdate();
        } else if (id == kIdPresets && code == CBN_SELCHANGE) {
            OnPresetChanged();
        } else if (id == kIdMonitors && code == CBN_SELCHANGE) {
            OnPresetChanged();
        } else if ((id == kIdEditX || id == kIdEditY || id == kIdEditW || id == kIdEditH) &&
                   code == EN_CHANGE && !quietEdit_) {
            if (SelectedPreset() != MonitorManager::Custom) {
                SendMessageW(hwndPresets_, CB_SETCURSEL, MonitorManager::Custom, 0);
            }
        }
        return 0;
    }

    case WM_APP_UPDATE_CHECK:
        OnUpdateCheckDone(reinterpret_cast<UpdateInfo*>(lParam));
        return 0;

    case WM_APP_UPDATE_DOWNLOAD:
        OnUpdateDownloadDone(reinterpret_cast<UpdateDownloadResult*>(lParam));
        return 0;

    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<LPNMHDR>(lParam);
        if (nm->idFrom == kIdList) {
            if (nm->code == LVN_ITEMCHANGED) {
                auto* nmlv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                if ((nmlv->uChanged & LVIF_STATE) &&
                    (nmlv->uNewState & LVIS_SELECTED)) {
                    OnWindowSelect();
                }
            } else if (nm->code == NM_CUSTOMDRAW) {
                return OnListCustomDraw(reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam));
            }
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, kTimerRefresh);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
